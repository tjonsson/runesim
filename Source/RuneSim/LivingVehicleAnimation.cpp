#include "LivingVehicleAnimation.h"
#include "Animation/AnimSingleNodeInstanceProxy.h"
#include "BonePose.h"

namespace
{
struct FLivingVehicleAnimationProxy final : FAnimSingleNodeInstanceProxy
{
    explicit FLivingVehicleAnimationProxy(UAnimInstance* Instance) : FAnimSingleNodeInstanceProxy(Instance) {}
    TArray<FLivingWheelPose> WheelPoses;
    virtual void PreUpdate(UAnimInstance* Instance, float DeltaSeconds) override
    {
        FAnimSingleNodeInstanceProxy::PreUpdate(Instance, DeltaSeconds);
        // Copy game-thread values; evaluation never reads live actors or performs traces.
        WheelPoses = CastChecked<ULivingVehicleAnimation>(Instance)->WheelPoses;
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        FAnimSingleNodeInstanceProxy::Evaluate(Output);
        FCSPose<FCompactPose> Pose; Pose.InitPose(Output.Pose);
        const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
        for (const FLivingWheelPose& Wheel : WheelPoses)
        {
            const int32 MeshIndex = Bones.GetReferenceSkeleton().FindBoneIndex(Wheel.Bone);
            if (MeshIndex == INDEX_NONE || !FMath::IsFinite(Wheel.SteeringDegrees) || Wheel.Offset.ContainsNaN()) continue;
            const FCompactPoseBoneIndex Index = Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
            if (Index == INDEX_NONE) continue; // A reduced LOD may omit a wheel bone.
            FTransform Transform = Pose.GetComponentSpaceTransform(Index);
            Transform.SetRotation(FRotator(0, FMath::Clamp(Wheel.SteeringDegrees, -60.f, 60.f), 0).Quaternion() * Transform.GetRotation());
            Transform.AddToTranslation(Wheel.Offset.GetClampedToMaxSize(100.f));
            Transform.NormalizeRotation(); Pose.SetComponentSpaceTransform(Index, Transform);
        }
        FCSPose<FCompactPose>::ConvertComponentPosesToLocalPoses(Pose, Output.Pose);
        return true;
    }
};
}

FAnimInstanceProxy* ULivingVehicleAnimation::CreateAnimInstanceProxy()
{
    return new FLivingVehicleAnimationProxy(this);
}

float LivingWorld::WheelSteering(float Curvature, float WheelbaseCm, float SideOffsetCm, float LimitDegrees)
{
    if (!FMath::IsFinite(Curvature) || !FMath::IsFinite(WheelbaseCm) || !FMath::IsFinite(SideOffsetCm) ||
        !FMath::IsFinite(LimitDegrees) || WheelbaseCm <= 0) return 0;
    const float Angle = FMath::RadiansToDegrees(FMath::Atan2(WheelbaseCm * Curvature,
        FMath::Max(.1f, 1.f - SideOffsetCm * Curvature)));
    const float Limit = FMath::Clamp(LimitDegrees, 0.f, 60.f);
    return FMath::Clamp(Angle, -Limit, Limit);
}
