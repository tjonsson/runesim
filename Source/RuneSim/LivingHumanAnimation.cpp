#include "LivingHumanAnimation.h"
#include "LivingAgent.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "BonePose.h"
#include "TwoBoneIK.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
struct FLivingHumanAnimationProxy final : FAnimSingleNodeInstanceProxy
{
    explicit FLivingHumanAnimationProxy(UAnimInstance* Instance) : FAnimSingleNodeInstanceProxy(Instance) {}
    TArray<FLivingFootSupport> Supports;
    TArray<FVector> BaseFeet;
    FLivingFootPlant Plants[2];
    FTransform MeshTransform;
    float DeltaSeconds = 0;
    uint32 Revision = 0;
    bool bReplay = false;
    virtual void PreUpdate(UAnimInstance* Instance, float Dt) override
    {
        FAnimSingleNodeInstanceProxy::PreUpdate(Instance, Dt);
        auto* Human = CastChecked<ULivingHumanAnimation>(Instance);
        Human->UpdateGrounding(Dt); // Game thread only; the worker receives value copies.
        Supports = Human->FootSupports;
        bReplay = Human->bReplaySupports;
        DeltaSeconds = Dt;
        MeshTransform = Human->GetSkelMeshComponent()->GetComponentTransform();
        if (Revision != Human->GroundingRevision || bReplay)
        { Plants[0] = {}; Plants[1] = {}; Revision = Human->GroundingRevision; }
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        FAnimSingleNodeInstanceProxy::Evaluate(Output);
        BaseFeet.Reset();
        FCSPose<FCompactPose> Pose; Pose.InitPose(Output.Pose);
        const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
        const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
        struct FLeg { FCompactPoseBoneIndex Thigh, Calf, Foot; FTransform T, C, F; float Offset; FVector Stance = FVector::ZeroVector; FQuat Tilt = FQuat::Identity; };
        TArray<FLeg, TInlineAllocator<2>> Legs;
        float PelvisDrop = 0;
        FCompactPoseBoneIndex Pelvis(INDEX_NONE);
        for (int32 I = 0; I < Supports.Num(); ++I)
        {
            auto& Support = Supports[I];
            const int32 MeshFoot = Ref.FindBoneIndex(Support.Bone);
            const auto Foot = MeshFoot != INDEX_NONE ? Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshFoot)) : FCompactPoseBoneIndex(INDEX_NONE);
            if (Foot == INDEX_NONE) { if (I < 2) Plants[I] = {}; Support.StanceOffsetCm = FVector::ZeroVector; continue; }
            const auto Calf = Bones.GetParentBoneIndex(Foot), Thigh = Calf != INDEX_NONE ? Bones.GetParentBoneIndex(Calf) : FCompactPoseBoneIndex(INDEX_NONE);
            if (Thigh == INDEX_NONE) continue;
            FLeg Leg{Thigh, Calf, Foot, Pose.GetComponentSpaceTransform(Thigh), Pose.GetComponentSpaceTransform(Calf), Pose.GetComponentSpaceTransform(Foot), 0};
            BaseFeet.Add(Leg.F.GetLocation()); // Always the unmodified pose, avoiding IK feedback.
            if (!FMath::IsFinite(Support.HeightCm)) continue;
            FTransform Reference = Ref.GetRefBonePose()[MeshFoot];
            for (int32 Parent = Ref.GetParentIndex(MeshFoot); Parent != INDEX_NONE; Parent = Ref.GetParentIndex(Parent)) Reference *= Ref.GetRefBonePose()[Parent];
            const float Weight = LivingWorld::FootContactWeight(Leg.F.GetLocation().Z, Reference.GetLocation().Z);
            Leg.Offset = FMath::Clamp(Support.HeightCm, -18.f, 18.f) * Weight;
            // Ankle follows the slope only while the foot carries weight; swing keeps the authored roll.
            Leg.Tilt = Support.bGrounded || bReplay ? LivingWorld::FootTilt(Support.GroundNormal, Weight) : FQuat::Identity;
            if (!bReplay && I < 2) Support.StanceOffsetCm = LivingWorld::UpdateFootPlant(Plants[I], MeshTransform,
                Leg.F.GetLocation(), Weight, Support.bGrounded, DeltaSeconds);
            if (!Support.StanceOffsetCm.ContainsNaN() && Support.StanceOffsetCm.SizeSquared() <= FMath::Square(18.01f))
                Leg.Stance = Support.StanceOffsetCm;
            PelvisDrop = FMath::Min(PelvisDrop, Leg.Offset);
            Pelvis = Bones.GetParentBoneIndex(Thigh);
            Legs.Add(Leg);
        }
        PelvisDrop = FMath::Clamp(PelvisDrop, -12.f, 0.f);
        if (Pelvis != INDEX_NONE && !FMath::IsNearlyZero(PelvisDrop))
        {
            FTransform Transform = Pose.GetComponentSpaceTransform(Pelvis); Transform.AddToTranslation(FVector(0,0,PelvisDrop));
            const FBoneTransform Change(Pelvis, Transform);
            Pose.SafeSetCSBoneTransforms(MakeArrayView(&Change, 1));
        }
        TArray<FBoneTransform> Changes;
        for (const FLeg& Leg : Legs)
        {
            if (FMath::IsNearlyZero(Leg.Offset) && FMath::IsNearlyZero(PelvisDrop) && Leg.Stance.IsNearlyZero() && Leg.Tilt.Equals(FQuat::Identity, 1e-4)) continue;
            FTransform Thigh = Pose.GetComponentSpaceTransform(Leg.Thigh), Calf = Pose.GetComponentSpaceTransform(Leg.Calf), Foot = Pose.GetComponentSpaceTransform(Leg.Foot);
            const FVector Target = Leg.F.GetLocation() + FVector(0,0,Leg.Offset) + Leg.Stance;
            if (!LivingWorld::SolveGroundedLeg(Thigh, Calf, Foot, Target)) continue;
            Foot.SetRotation((Leg.Tilt * Foot.GetRotation()).GetNormalized());
            Changes.Emplace(Leg.Thigh, Thigh); Changes.Emplace(Leg.Calf, Calf); Changes.Emplace(Leg.Foot, Foot);
        }
        Changes.Sort([](const FBoneTransform& A, const FBoneTransform& B) { return A.BoneIndex < B.BoneIndex; });
        if (!Changes.IsEmpty()) Pose.SafeSetCSBoneTransforms(Changes); // Descendants, including toes, retain their local animation.
        FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(Pose, Output.Pose);
        return true;
    }
    virtual void PostEvaluate(UAnimInstance* Instance) override
    {
        FAnimSingleNodeInstanceProxy::PostEvaluate(Instance);
        auto* Human = CastChecked<ULivingHumanAnimation>(Instance);
        Human->BaseFeet = BaseFeet;
        Human->bHasBasePose = BaseFeet.Num() == 2;
        if (!bReplay) Human->FootSupports = Supports;
    }
};
}

FAnimInstanceProxy* ULivingHumanAnimation::CreateAnimInstanceProxy() { return new FLivingHumanAnimationProxy(this); }

void ULivingHumanAnimation::ResetGrounding()
{
    FootSupports.Reset(); BaseFeet.Reset(); bHasBasePose = false; SupportedFeet = 0;
    ++GroundingRevision;
    const auto* Mesh = GetSkelMeshComponent() ? GetSkelMeshComponent()->GetSkeletalMeshAsset() : nullptr;
    if (!Mesh) return;
    const auto& Ref = Mesh->GetRefSkeleton();
    for (const FString Side : {FString(TEXT("Left")), FString(TEXT("Right"))})
        for (int32 Index = 0; Index < Ref.GetNum(); ++Index)
            if (Ref.GetBoneName(Index).ToString().EndsWith(Side + TEXT("Foot")))
            {
                const int32 Calf = Ref.GetParentIndex(Index), Thigh = Calf != INDEX_NONE ? Ref.GetParentIndex(Calf) : INDEX_NONE;
                if (Thigh != INDEX_NONE && Ref.GetBoneName(Calf).ToString().EndsWith(Side + TEXT("Leg")) && Ref.GetBoneName(Thigh).ToString().EndsWith(Side + TEXT("UpLeg")))
                { FLivingFootSupport Support; Support.Bone = Ref.GetBoneName(Index); FootSupports.Add(Support); }
                break;
            }
    LastMeshTransform = GetSkelMeshComponent()->GetComponentTransform();
}

void ULivingHumanAnimation::UpdateGrounding(float Dt)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(RuneSimHumanGrounding);
    if (bReplaySupports) return;
    auto* Mesh = GetSkelMeshComponent();
    const auto* Agent = Mesh ? Cast<ALivingAgent>(Mesh->GetOwner()) : nullptr;
    SupportedFeet = 0;
    if (!Agent || !Agent->bActive || Mesh->GetPredictedLODLevel() > 1 || FootSupports.Num() != 2 || !bHasBasePose || BaseFeet.Num() != 2)
    { for (auto& Support : FootSupports) { Support.HeightCm = 0; Support.StanceOffsetCm = FVector::ZeroVector; Support.bGrounded = false; } ++GroundingRevision; return; }
    const FTransform Transform = Mesh->GetComponentTransform();
    const bool bDiscontinuity = !FMath::IsFinite(Dt) || Dt <= 0 || Dt > .2f ||
        FVector::DistSquared(Transform.GetLocation(), LastMeshTransform.GetLocation()) > FMath::Square(100.f) ||
        Transform.GetRotation().AngularDistance(LastMeshTransform.GetRotation()) > FMath::DegreesToRadians(45.f);
    LastMeshTransform = Transform;
    if (bDiscontinuity) ++GroundingRevision;
    for (int32 I = 0; I < 2; ++I)
    {
        const FVector SupportedFoot = BaseFeet[I] + FootSupports[I].StanceOffsetCm;
        FVector Contact, Normal = FVector::UpVector; const FVector Probe = Transform.TransformPosition(FVector(SupportedFoot.X, SupportedFoot.Y, 0));
        const bool bHit = !bDiscontinuity && Agent->SampleFootGround(Probe, Contact, &Normal);
        const float Height = bHit ? Transform.InverseTransformPosition(Contact).Z : 0;
        const bool bSupported = bHit && FMath::IsFinite(Height) && FMath::Abs(Height) <= 18.f;
        if (bSupported) ++SupportedFeet;
        FootSupports[I].bGrounded = bSupported;
        // Drop invalid support immediately; never retain a phantom contact on an unloaded tile.
        FootSupports[I].HeightCm = bSupported ? FMath::FInterpTo(FootSupports[I].HeightCm, Height, Dt, 15.f) : 0;
        const FVector MeshNormal = bSupported ? Transform.InverseTransformVectorNoScale(Normal).GetSafeNormal(SMALL_NUMBER, FVector::UpVector) : FVector::UpVector;
        FootSupports[I].GroundNormal = FMath::VInterpTo(FootSupports[I].GroundNormal, MeshNormal, Dt, 15.f).GetSafeNormal(SMALL_NUMBER, FVector::UpVector);
    }
}

FVector LivingWorld::UpdateFootPlant(FLivingFootPlant& Plant, const FTransform& Mesh,
    const FVector& AnimatedFoot, float ContactWeight, bool bSupported, float Dt)
{
    if (!bSupported || !FMath::IsFinite(Dt) || Dt <= 0 || Dt > .2f || Mesh.ContainsNaN() ||
        AnimatedFoot.ContainsNaN() || !FMath::IsFinite(ContactWeight))
    { Plant = {}; return FVector::ZeroVector; }
    // Hysteresis prevents repeated acquisition within the same stance. Lift rearms the next step.
    if (ContactWeight < .8f) { Plant = {}; return FVector::ZeroVector; }
    if (!Plant.bPlanted && Plant.bCanPlant && ContactWeight >= .98f)
    {
        Plant.Anchor = Mesh.TransformPosition(AnimatedFoot); Plant.Rotation = Mesh.GetRotation();
        Plant.Age = 0; Plant.bPlanted = true; Plant.bCanPlant = false;
    }
    if (!Plant.bPlanted) return FVector::ZeroVector;
    Plant.Age += Dt;
    FVector Offset = Mesh.InverseTransformPosition(Plant.Anchor) - AnimatedFoot; Offset.Z = 0;
    const float Distance = Offset.Size();
    const float Turn = FMath::RadiansToDegrees(Plant.Rotation.AngularDistance(Mesh.GetRotation()));
    // Fade before releasing at reach/turn/time limits, so a planted leg cannot tether the body.
    const float Weight = FMath::Min(FMath::Min3(FMath::Clamp((30.f-Distance)/12.f,0.f,1.f),
        FMath::Clamp((25.f-Turn)/15.f,0.f,1.f), FMath::Clamp((.7f-Plant.Age)/.15f,0.f,1.f)),
        FMath::Clamp((ContactWeight-.8f)/.18f,0.f,1.f));
    if (Weight <= 0) { Plant.bPlanted = false; return FVector::ZeroVector; }
    return (Offset * Weight).GetClampedToMaxSize(18.f);
}

float LivingWorld::FootContactWeight(float AnimatedHeight, float ReferenceHeight)
{
    if (!FMath::IsFinite(AnimatedHeight) || !FMath::IsFinite(ReferenceHeight)) return 0;
    const float T = FMath::Clamp((AnimatedHeight - ReferenceHeight - 3.f) / 12.f, 0.f, 1.f);
    return 1.f - T*T*(3.f - 2.f*T);
}

FQuat LivingWorld::FootTilt(const FVector& GroundNormal, float Weight, float MaxDegrees)
{
    if (GroundNormal.ContainsNaN() || !FMath::IsFinite(Weight) || !FMath::IsFinite(MaxDegrees) || GroundNormal.IsNearlyZero()) return FQuat::Identity;
    FQuat Tilt = FQuat::FindBetweenNormals(FVector::UpVector, GroundNormal.GetSafeNormal());
    FVector Axis; float Angle;
    Tilt.ToAxisAndAngle(Axis, Angle);
    Angle = FMath::Min(Angle, FMath::DegreesToRadians(FMath::Clamp(MaxDegrees, 0.f, 45.f)));
    return FQuat::Slerp(FQuat::Identity, FQuat(Axis, Angle), FMath::Clamp(Weight, 0.f, 1.f)).GetNormalized();
}

bool LivingWorld::SolveGroundedLeg(FTransform& Thigh, FTransform& Calf, FTransform& Foot, const FVector& Target)
{
    if (Thigh.ContainsNaN() || Calf.ContainsNaN() || Foot.ContainsNaN() || Target.ContainsNaN() ||
        FVector::DistSquared(Foot.GetLocation(), Target) > FMath::Square(36.f) ||
        FVector::DistSquared(Thigh.GetLocation(), Calf.GetLocation()) < 1.f || FVector::DistSquared(Calf.GetLocation(), Foot.GetLocation()) < 1.f) return false;
    AnimationCore::SolveTwoBoneIK(Thigh, Calf, Foot, Calf.GetLocation(), Target, false, 1., 1.);
    Thigh.NormalizeRotation(); Calf.NormalizeRotation(); Foot.NormalizeRotation();
    return true;
}
