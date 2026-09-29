#include "LivingAgent.h"
#include "LivingHumanAnimation.h"
#include "LivingRoute.h"
#include "LivingPerch.h"
#include "LivingGeography.h"
#include "CesiumGeoreference.h"
#include "EngineUtils.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SplineComponent.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Sound/SoundBase.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"

ALivingAgent::ALivingAgent()
{
    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("AgentBounds"));
    SetRootComponent(Collision);
    Collision->SetCollisionProfileName(TEXT("Pawn"));
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticVisual"));
    Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AnimatedVisual = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("AnimatedVisual"));
    AnimatedVisual->SetupAttachment(Collision);
    AnimatedVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Audio = CreateDefaultSubobject<UAudioComponent>(TEXT("AmbientSound"));
    Audio->SetupAttachment(Collision);
    Audio->bAutoActivate = false;
    Audio->bOverrideAttenuation = true;
    Audio->AttenuationOverrides.bAttenuate = true;
    Audio->AttenuationOverrides.bSpatialize = true;
    Audio->AttenuationOverrides.bEnableOcclusion = true;
    Audio->AttenuationOverrides.FalloffDistance = 15000.f;
    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
}

void ALivingAgent::Activate(ULivingAssetProfile* Asset, ALivingRoute* InRoute, float Distance, const FVector& InHome, int32 Seed, int32 InFlock, bool bTerrainAware)
{
    ReleasePerch();
    Profile = Asset;
    Route = InRoute;
    RouteDistance = Distance;
    LateralOffset = 0.f;
    Home = InHome;
    bRequireAirTerrain = bTerrainAware && !LivingWorld::IsGround(Asset->Kind);
    FlightGeoreference = bRequireAirTerrain ? GlobeAnchor->ResolveGeoreference() : nullptr;
    if (FlightGeoreference.IsValid()) HomeEcef = FlightGeoreference->TransformUnrealPositionToEarthCenteredEarthFixed(Home);
    CurrentBank = 0;
    Flock = InFlock;
    FRandomStream Random(Seed);
    SoundRandom.Initialize(Seed ^ 0x34B12);
    BirdRandom.Initialize(Seed ^ GetTypeHash(GetActorLocation()));
    PerchSearchTime = BirdRandom.FRandRange(8.f, 18.f);
    ApproachTime = 0.f;
    FootstepsPlayed = 0;
    Phase = Random.FRandRange(0.f, 2.f * PI);
    Time = CalmTime = 0.f;
    Direction = LivingWorld::IsGround(Profile->Kind) ? 1 : (InFlock % 2 ? -1 : 1);
    Behavior = ELivingBehavior::Cruising;
    LocomotionAnimation = -1;
    Velocity = FVector::ZeroVector;
    GroundSpeed = LocomotionSpeedRatio = 0.f;
    Collision->SetSphereRadius(FMath::Max(5.f, Profile->CollisionRadiusCm));
    Visual->SetStaticMesh(Profile->StaticMesh.LoadSynchronous());
    AnimatedVisual->SetSkeletalMeshAsset(Profile->SkeletalMesh.LoadSynchronous());
    const FTransform Transform(Profile->VisualRotation, Profile->VisualOffset, Profile->VisualScale);
    Visual->SetRelativeTransform(Transform);
    AnimatedVisual->SetRelativeTransform(Transform);
    AnimatedVisual->Stop();
    InitializeWheels();
    UpdateLocomotionAnimation(true, false);
    if (auto* Human = Cast<ULivingHumanAnimation>(AnimatedVisual->GetAnimInstance())) Human->ResetGrounding();
    if (Profile->Kind == ELivingKind::Bird) SetFlightState(ELivingFlightState::Flapping);
    if (const UAnimSequenceBase* Clip = Cast<UAnimSequenceBase>(Profile->CruiseAnimation.Get()))
        AnimatedVisual->SetPosition(BirdRandom.FRand() * Clip->GetPlayLength(), false);
    Audio->SetSound(Profile->LoopSound.LoadSynchronous());
    Audio->SetPitchMultiplier(1.f);
    Audio->AttenuationOverrides.FalloffDistance = Profile->Kind <= ELivingKind::Soldier ? 1500.f : 15000.f;
    bActive = true;
    SetActorHiddenInGame(false);
    SetActorEnableCollision(true);
    AnimatedVisual->SetComponentTickEnabled(true);
    if (Audio->Sound) Audio->Play();
}

void ALivingAgent::Deactivate()
{
    ReleasePerch();
    bActive = false;
    Velocity = FVector::ZeroVector;
    Audio->Stop();
    AnimatedVisual->SetComponentTickEnabled(false);
    SetActorHiddenInGame(true);
    SetActorEnableCollision(false);
}

bool ALivingAgent::PreviewRoutePlacement(ULivingAssetProfile* Asset, ALivingRoute* InRoute, float Distance)
{
    BlockedReason.Reset();
    if (!Asset || !InRoute || !LivingWorld::IsGround(Asset->Kind))
    { BlockedReason = TEXT("Invalid ground profile or route"); return false; }
    FVector Position, Up;
    if (!InRoute->SampleGround(Distance, Position, Up, 0, Asset->CollisionRadiusCm))
    { BlockedReason = TEXT("Corridor footprint or terrain support"); return false; }
    SetActorLocation(Position + Up*Asset->GroundClearanceCm);
    const FVector Tangent = InRoute->Path->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
    SetActorRotation(FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(Tangent, Up).GetSafeNormal(), Up).Rotator());
    Activate(Asset, InRoute, Distance, FVector::ZeroVector, 1, 0);
    FLivingWorldOptions PreviewOptions;
    Step(0.f, PreviewOptions, {}, nullptr);
    const bool bSupported = bActive && Behavior != ELivingBehavior::Blocked;
    Velocity = FVector::ZeroVector;
    Deactivate();
    return bSupported;
}

void ALivingAgent::ReleasePerch()
{
    if (Perch.IsValid()) Perch->Release(this);
    Perch.Reset();
}

void ALivingAgent::EndPlay(const EEndPlayReason::Type Reason)
{
    ReleasePerch();
    Super::EndPlay(Reason);
}

void ALivingAgent::SetFlightState(ELivingFlightState State)
{
    FlightState = State;
    FlightStateTime = 0.f;
    FlightStateDuration = BirdRandom.FRandRange(6.f, 10.f);
    UAnimationAsset* Clip = Profile->CruiseAnimation.LoadSynchronous();
    bool bLoop = true;
    switch (State)
    {
    case ELivingFlightState::Gliding:
        Clip = Profile->GlideAnimation.LoadSynchronous();
        FlightStateDuration = BirdRandom.FRandRange(2.f, 4.f); break;
    case ELivingFlightState::Perched:
        Clip = Profile->IdleAnimation.LoadSynchronous();
        FlightStateDuration = BirdRandom.FRandRange(12.f, 24.f); break;
    case ELivingFlightState::Landing:
        Clip = Profile->LandingAnimation.LoadSynchronous(); bLoop = false; break;
    case ELivingFlightState::TakingOff:
        Clip = Profile->TakeoffAnimation.LoadSynchronous(); bLoop = false; break;
    default: break;
    }
    if (!Clip) Clip = Profile->CruiseAnimation.LoadSynchronous();
    if (!bLoop)
    {
        const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Clip);
        FlightStateDuration = Sequence ? FMath::Max(0.1f, Sequence->GetPlayLength()) : 1.f;
    }
    if (Clip) AnimatedVisual->PlayAnimation(Clip, bLoop);
}

bool ALivingAgent::RequestPerch(ALivingPerch* Site)
{
    if (!bActive || !Profile || Profile->Kind != ELivingKind::Bird || !Profile->bAllowPerching ||
        !IsValid(Site) || (FlightState != ELivingFlightState::Flapping && FlightState != ELivingFlightState::Gliding) ||
        !Site->Reserve(this)) return false;
    ReleasePerch();
    Perch = Site;
    ApproachTime = 0.f;
    SetFlightState(ELivingFlightState::Approaching);
    return true;
}

void ALivingAgent::TakeOff()
{
    if (!bActive || !Profile || Profile->Kind != ELivingKind::Bird) return;
    const bool bOnPerch = FlightState == ELivingFlightState::Perched || FlightState == ELivingFlightState::Landing;
    DepartureUp = Perch.IsValid() ? Perch->GetActorUpVector() : FVector::UpVector;
    ReleasePerch();
    PerchSearchTime = BirdRandom.FRandRange(20.f, 35.f);
    SetFlightState(bOnPerch ? ELivingFlightState::TakingOff : ELivingFlightState::Flapping);
}

void ALivingAgent::UpdateBirdIntent(float Dt, bool bAlarm, float Speed, FVector& Desired)
{
    FlightStateTime += Dt;
    PerchSearchTime -= Dt;
    if (bAlarm && (Perch.IsValid() || FlightState == ELivingFlightState::Gliding)) TakeOff();
    const bool bLanding = FlightState == ELivingFlightState::Approaching || FlightState == ELivingFlightState::Landing;
    if (bLanding || FlightState == ELivingFlightState::Perched)
    {
        FVector Position, Up;
        ApproachTime += Dt;
        if (!Perch.IsValid() || !Perch->SampleLanding(Position, Up, this) || (bLanding && ApproachTime > 30.f))
        { TakeOff(); return; }
        const FVector Offset = Position - GetActorLocation();
        const float Distance = Offset.Size();
        if (FlightState == ELivingFlightState::Perched)
        {
            Desired = FVector::ZeroVector;
            if (FlightStateTime >= FlightStateDuration) TakeOff();
            return;
        }
        if (FlightState == ELivingFlightState::Approaching && Distance < 350.f) SetFlightState(ELivingFlightState::Landing);
        // Arrive without overshooting. Final approach aligns to the reviewed surface.
        Desired = Offset.GetSafeNormal() * FMath::Min(Speed, Distance * 1.5f);
        if (Distance < 3.f && FlightState == ELivingFlightState::Landing && FlightStateTime >= FlightStateDuration)
        {
            SetFlightState(ELivingFlightState::Perched);
            Desired = FVector::ZeroVector;
            SetActorRotation(FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(GetActorForwardVector(), Up).GetSafeNormal(), Up).Rotator());
        }
        return;
    }
    if (FlightState == ELivingFlightState::TakingOff)
    {
        Desired = DepartureUp * Speed * 0.55f + FVector::VectorPlaneProject(GetActorForwardVector(), DepartureUp).GetSafeNormal() * Speed * 0.25f;
        if (FlightStateTime >= FlightStateDuration) SetFlightState(ELivingFlightState::Flapping);
        return;
    }
    if (FlightStateTime >= FlightStateDuration)
        SetFlightState(FlightState == ELivingFlightState::Flapping && !bAlarm && !Profile->GlideAnimation.IsNull() ?
            ELivingFlightState::Gliding : ELivingFlightState::Flapping);
    if (!bAlarm && Profile->bAllowPerching && PerchSearchTime <= 0.f)
    {
        PerchSearchTime = BirdRandom.FRandRange(8.f, 15.f);
        ALivingPerch* Best = nullptr;
        float BestDistance = FMath::Square(6000.f);
        for (TActorIterator<ALivingPerch> It(GetWorld()); It; ++It)
        {
            const float Distance = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
            if (Distance >= BestDistance || (It->Occupant.IsValid() && It->Occupant.Get() != this)) continue;
            FVector Position, Up;
            if (It->SampleLanding(Position, Up, this)) { Best = *It; BestDistance = Distance; }
        }
        if (Best) RequestPerch(Best);
    }
}

void ALivingAgent::UpdateLocomotionAnimation(bool bMoving, bool bRunning)
{
    const int32 Mode = !bMoving ? 0 : bRunning ? 2 : 1;
    if (Profile->Kind == ELivingKind::Car && !Profile->Wheels.IsEmpty())
    {
        auto* Instance = Cast<ULivingVehicleAnimation>(AnimatedVisual->GetAnimInstance());
        if (!Instance)
        {
            AnimatedVisual->SetAnimInstanceClass(ULivingVehicleAnimation::StaticClass());
            Instance = Cast<ULivingVehicleAnimation>(AnimatedVisual->GetAnimInstance());
        }
        if (Instance)
        {
            UAnimationAsset* Clip = Profile->CruiseAnimation.LoadSynchronous();
            if (Instance->GetCurrentAsset() != Clip) Instance->SetAnimationAsset(Clip, true);
            Instance->SetPlaying(bMoving); Instance->WheelPoses = WheelPoses;
        }
        LocomotionAnimation = Mode;
        return;
    }
    if (Profile->Kind <= ELivingKind::Soldier)
    {
        if (UBlendSpace* Blend = Profile->LocomotionBlend.LoadSynchronous())
        {
            auto* Instance = Cast<ULivingHumanAnimation>(AnimatedVisual->GetAnimInstance());
            if (!Instance)
            {
                AnimatedVisual->SetAnimInstanceClass(ULivingHumanAnimation::StaticClass());
                Instance = Cast<ULivingHumanAnimation>(AnimatedVisual->GetAnimInstance());
                if (Instance) Instance->ResetGrounding();
            }
            if (Instance && (Instance->GetCurrentAsset() != Blend || LocomotionAnimation < 0)) Instance->SetAnimationAsset(Blend, true);
            LocomotionSpeedRatio = bMoving ? FMath::Clamp(float(Velocity.Size()) / FMath::Max(1.f, Profile->SpeedMetersPerSecond * 100.f), 0.f, 2.5f) : 0.f;
            if (Instance) { Instance->SetPlaying(true); Instance->SetBlendSpacePosition(FVector(LocomotionSpeedRatio, 0, 0)); }
            LocomotionAnimation = Mode;
            return;
        }
    }
    if (Mode == LocomotionAnimation) return;
    LocomotionAnimation = Mode;
    UAnimationAsset* Animation = Mode == 0 ? Profile->IdleAnimation.LoadSynchronous() :
        Mode == 2 ? Profile->FleeAnimation.LoadSynchronous() : Profile->CruiseAnimation.LoadSynchronous();
    if (!Animation && Mode == 2) Animation = Profile->CruiseAnimation.LoadSynchronous();
    if (Animation) AnimatedVisual->PlayAnimation(Animation, true);
    else AnimatedVisual->Stop();
}

bool ALivingAgent::SampleFootGround(const FVector& Probe, FVector& Contact) const
{
    if (!Route || !Route->bValidated || Route->ReviewedHalfWidthCm <= 0 || Probe.ContainsNaN()) return false;
    const float Key = Route->Path->FindInputKeyClosestToWorldLocation(Probe);
    const FVector Centre = Route->Path->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
    const FVector Right = Route->Path->GetRightVectorAtSplineInputKey(Key, ESplineCoordinateSpace::World);
    const FVector Tangent = Route->Path->GetDirectionAtSplineInputKey(Key, ESplineCoordinateSpace::World);
    if (FMath::Abs(FVector::DotProduct(Probe - Centre, Tangent)) > 5.f) return false;
    const float Lateral = FVector::DotProduct(Probe - Centre, Right);
    if (FMath::Abs(Lateral) + 5.f > Route->ReviewedHalfWidthCm) return false;
    FVector Normal;
    // The movement step already validates the body's full footprint. Visual IK
    // needs the surface at each foot, not three additional footprint rays per foot.
    return Route->SampleGround(Route->Path->GetDistanceAlongSplineAtSplineInputKey(Key), Contact, Normal,
        Lateral, 0.f);
}

void ALivingAgent::InitializeWheels()
{
    WheelPoses.Empty(); WheelReferenceLocations.Empty(); WheelbaseCm = 0;
    const USkeletalMesh* Mesh = AnimatedVisual->GetSkeletalMeshAsset();
    if (Profile->Kind != ELivingKind::Car || !Mesh || Profile->Wheels.IsEmpty() || Profile->Wheels.Num() > 16) return;
    const FReferenceSkeleton& Skeleton = Mesh->GetRefSkeleton();
    float MinX = BIG_NUMBER, MaxX = -BIG_NUMBER;
    for (const FLivingWheelDefinition& Wheel : Profile->Wheels)
    {
        const int32 Index = Skeleton.FindBoneIndex(Wheel.Bone);
        if (Index == INDEX_NONE || !FMath::IsFinite(Wheel.RadiusCm) || Wheel.RadiusCm <= 0)
        { WheelPoses.Empty(); WheelReferenceLocations.Empty(); return; }
        FTransform Bone = Skeleton.GetRefBonePose()[Index];
        for (int32 Parent = Skeleton.GetParentIndex(Index); Parent != INDEX_NONE; Parent = Skeleton.GetParentIndex(Parent))
            Bone = Bone * Skeleton.GetRefBonePose()[Parent];
        WheelReferenceLocations.Add(Bone.GetLocation());
        FLivingWheelPose Pose; Pose.Bone = Wheel.Bone; WheelPoses.Add(Pose);
        const float X = AnimatedVisual->GetRelativeTransform().TransformPosition(Bone.GetLocation()).X;
        MinX = FMath::Min(MinX, X); MaxX = FMath::Max(MaxX, X);
    }
    WheelbaseCm = MaxX - MinX;
}

bool ALivingAgent::SampleWheelContacts(const FTransform& ActorTransform, TArray<FLivingWheelPose>& Poses)
{
    if (Profile->Wheels.IsEmpty()) return true;
    if (!Route || Route->ReviewedHalfWidthCm <= 0 || WheelReferenceLocations.Num() != Profile->Wheels.Num() || WheelPoses.Num() != Profile->Wheels.Num()) return false;
    if (!FMath::IsFinite(Profile->SuspensionTravelCm)) return false;
    const float Travel = FMath::Clamp(Profile->SuspensionTravelCm, 0.f, 50.f);
    const FTransform MeshTransform = AnimatedVisual->GetRelativeTransform() * ActorTransform;
    const FVector Up = ActorTransform.GetUnitAxis(EAxis::Z);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LivingWheels), true, this);
    Poses = WheelPoses;
    for (int32 I = 0; I < Poses.Num(); ++I)
    {
        const FVector Pivot = MeshTransform.TransformPosition(WheelReferenceLocations[I]);
        const float Radius = Profile->Wheels[I].RadiusCm;
        const FVector Contact = Pivot - Up * Radius;
        const float Key = Route->Path->FindInputKeyClosestToWorldLocation(Contact);
        const FVector Corridor = Route->Path->GetLocationAtSplineInputKey(Key, ESplineCoordinateSpace::World);
        const FVector Right = Route->Path->GetRightVectorAtSplineInputKey(Key, ESplineCoordinateSpace::World);
        // A wheel contact cannot authorize departure from the reviewed road strip.
        if (FMath::Abs(FVector::DotProduct(Contact - Corridor, Right)) + 15.f > Route->ReviewedHalfWidthCm)
        { BlockedReason = TEXT("Wheel outside reviewed width: ") + Poses[I].Bone.ToString(); return false; }
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByObjectType(Hit, Contact + Up * (Travel + 2.f), Contact - Up * (Travel + 2.f),
            FCollisionObjectQueryParams(ECC_WorldStatic), Params) || !Hit.GetActor() ||
            Hit.GetActor()->ActorHasTag(TEXT("Water")) || Hit.GetActor()->ActorHasTag(TEXT("LivingWorld.NoWalk")) ||
            FVector::DotProduct(Hit.ImpactNormal, Up) < FMath::Cos(FMath::DegreesToRadians(Route->MaxSlopeDegrees)))
        { BlockedReason = TEXT("Missing, tagged or steep wheel support: ") + Poses[I].Bone.ToString(); return false; }
        const float Offset = FVector::DotProduct(Hit.ImpactPoint - Contact, Up);
        if (FMath::Abs(Offset) > Travel + .1f)
        { BlockedReason = FString::Printf(TEXT("Suspension limit: %s %.2f cm"), *Poses[I].Bone.ToString(), Offset); return false; }
        Poses[I].Offset = MeshTransform.InverseTransformVector(Up * Offset);
    }
    return true;
}

void ALivingAgent::UpdateWheels(float Dt, float Curvature, const TArray<FLivingWheelPose>& Contacts)
{
    if (Contacts.Num() != WheelPoses.Num()) return;
    for (int32 I = 0; I < WheelPoses.Num(); ++I)
    {
        const FVector LocalPivot = AnimatedVisual->GetRelativeTransform().TransformPosition(WheelReferenceLocations[I]);
        const float Steering = Profile->Wheels[I].bSteers ? LivingWorld::WheelSteering(Curvature, WheelbaseCm, LocalPivot.Y, Profile->MaxWheelSteeringDegrees) : 0;
        WheelPoses[I].SteeringDegrees = FMath::FInterpConstantTo(WheelPoses[I].SteeringDegrees, Steering, Dt, 100.f);
        // Follow raised contact immediately to avoid tire penetration; extension can settle.
        const FVector Smoothed = FMath::VInterpTo(WheelPoses[I].Offset, Contacts[I].Offset, Dt, 12.f);
        WheelPoses[I].Offset = Contacts[I].Offset.Z > Smoothed.Z ? Contacts[I].Offset : Smoothed;
    }
    if (auto* Animation = Cast<ULivingVehicleAnimation>(AnimatedVisual->GetAnimInstance())) Animation->WheelPoses = WheelPoses;
}

bool ALivingAgent::FitVehicleGround(FVector& Position, FVector& Up, const FVector& Tangent) const
{
    if (WheelReferenceLocations.Num() != 4) return true;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LivingChassis), true, this);
    const FRotator Facing = FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(Tangent, Up).GetSafeNormal(), Up).Rotator();
    FTransform ActorTransform(Facing, Position, GetActorScale3D());
    FTransform MeshTransform = AnimatedVisual->GetRelativeTransform() * ActorTransform;
    FVector Front = FVector::ZeroVector, Rear = FVector::ZeroVector, Left = FVector::ZeroVector, Right = FVector::ZeroVector;
    TArray<FVector> Hits;
    int32 FrontCount = 0, RearCount = 0, LeftCount = 0, RightCount = 0;
    const float Reach = FMath::Clamp(Profile->SuspensionTravelCm * 2.f, 2.f, 100.f) + 2.f;
    for (int32 I=0; I<4; ++I)
    {
        const FVector Local = AnimatedVisual->GetRelativeTransform().TransformPosition(WheelReferenceLocations[I]);
        const FVector Contact = MeshTransform.TransformPosition(WheelReferenceLocations[I]) - Up * Profile->Wheels[I].RadiusCm;
        FHitResult Hit;
        if (!GetWorld()->LineTraceSingleByObjectType(Hit, Contact + Up*Reach, Contact - Up*Reach,
            FCollisionObjectQueryParams(ECC_WorldStatic), Params)) return false;
        Hits.Add(Hit.ImpactPoint);
        if (Local.X >= 0) { Front += Hit.ImpactPoint; ++FrontCount; } else { Rear += Hit.ImpactPoint; ++RearCount; }
        if (Local.Y >= 0) { Right += Hit.ImpactPoint; ++RightCount; } else { Left += Hit.ImpactPoint; ++LeftCount; }
    }
    if (!FrontCount || !RearCount || !LeftCount || !RightCount) return false;
    const FVector FittedUp = FVector::CrossProduct(Front/FrontCount - Rear/RearCount, Right/RightCount - Left/LeftCount).GetSafeNormal();
    if (FVector::DotProduct(FittedUp, Route->Path->GetUpVector()) < FMath::Cos(FMath::DegreesToRadians(Route->MaxSlopeDegrees))) return false;
    Up = FittedUp;
    ActorTransform.SetRotation(FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(Tangent, Up).GetSafeNormal(), Up).ToQuat());
    MeshTransform = AnimatedVisual->GetRelativeTransform() * ActorTransform;
    float HeightCorrection = 0;
    for (int32 I=0; I<4; ++I)
    {
        const FVector Contact = MeshTransform.TransformPosition(WheelReferenceLocations[I]) - Up*Profile->Wheels[I].RadiusCm;
        HeightCorrection += FVector::DotProduct(Hits[I] - Contact, Up) * .25f;
    }
    Position += Up*HeightCorrection;
    // The caller still verifies each wheel's reviewed width, slope, tags and
    // original suspension limit. This fit only prevents a single triangle's
    // normal from tilting the whole chassis away from otherwise supported axles.
    return true;
}

void ALivingAgent::PlayFootstep()
{
    if (!bActive || !Profile || Profile->Kind > ELivingKind::Soldier || Velocity.IsNearlyZero() || Profile->FootstepSounds.IsEmpty()) return;
    if (USoundBase* Sound = Profile->FootstepSounds[SoundRandom.RandRange(0, Profile->FootstepSounds.Num() - 1)].LoadSynchronous())
    {
        Audio->SetSound(Sound);
        Audio->SetPitchMultiplier(SoundRandom.FRandRange(0.96f, 1.04f));
        Audio->Play();
        ++FootstepsPlayed;
    }
}

void ALivingAgent::ApplyWorldOffset(const FVector& Offset, bool bWorldShift)
{
    Super::ApplyWorldOffset(Offset, bWorldShift);
    Home += Offset;
}

void ALivingAgent::Step(float Dt, const FLivingWorldOptions& Options, const TArray<ALivingAgent*>& Neighbors, const FVector* Threat)
{
    if (!bActive || !Profile) return;
    BlockedReason.Reset();
    if (FlightGeoreference.IsValid()) Home = FlightGeoreference->TransformEarthCenteredEarthFixedPositionToUnreal(HomeEcef);
    Time += Dt;
    const FVector Old = GetActorLocation();
    const FRotator OldRotation = GetActorRotation();
    const bool bPerson = Profile->Kind <= ELivingKind::Soldier;
    const bool bAlarm = Options.bReactive && Threat && FVector::DistSquared(*Threat, Old) < FMath::Square(bPerson ? 1800.f : 2500.f);
    if (bAlarm)
    {
        if (Behavior == ELivingBehavior::Cruising || Behavior == ELivingBehavior::Recovering)
        {
            Behavior = ELivingBehavior::Startled;
            if (Route && bPerson)
            {
                const FVector Tangent = Route->Path->GetDirectionAtDistanceAlongSpline(RouteDistance, ESplineCoordinateSpace::World);
                Direction = FVector::DotProduct(Tangent, Old - *Threat) >= 0.f ? 1 : -1;
            }
        }
        CalmTime += Dt;
        if (CalmTime > 0.3f) Behavior = ELivingBehavior::Fleeing;
    }
    else
    {
        CalmTime = FMath::Max(0.f, CalmTime - Dt * 0.5f);
        if (Behavior == ELivingBehavior::Fleeing || Behavior == ELivingBehavior::Startled) Behavior = ELivingBehavior::Recovering;
        if (CalmTime <= 0.f && Behavior == ELivingBehavior::Recovering) Behavior = ELivingBehavior::Cruising;
    }
    CalmTime = FMath::Min(CalmTime, 3.f);
    const bool bRunning = bPerson && Behavior == ELivingBehavior::Fleeing;
    Audio->SetVolumeMultiplier(FMath::Pow(10.f, Options.AmbientVolumeDb / 20.f));
    const float Speed = Profile->SpeedMetersPerSecond * 100.f * (bRunning ? 2.5f : 1.f);
    FVector Next = Old;
    FVector Up = GetActorUpVector();
    const FQuat FlightFrame = LivingGeography::Frame(FlightGeoreference.Get(), Home);
    float ProposedDistance = RouteDistance;
    float ProposedLateralOffset = LateralOffset;
    int32 ProposedDirection = Direction;
    if (LivingWorld::IsGround(Profile->Kind))
    {
        if (!Route || !Route->bValidated) { Behavior = ELivingBehavior::Blocked; GroundSpeed = 0; Velocity = FVector::ZeroVector; UpdateLocomotionAnimation(false, false); return; }
        const float Braking = FMath::Max(10.f, Profile->GroundBraking * 100.f);
        float DesiredSpeed = Speed;
        // Two-way pedestrian traffic keeps right only within an explicitly
        // reviewed corridor. GIS imports have no lateral permission by default.
        const float Lane = bPerson && Route->ReviewedHalfWidthCm >= Profile->CollisionRadiusCm * 3.f ?
            FMath::Min(100.f, Route->ReviewedHalfWidthCm * .5f) : 0.f;
        const float ProposedOffset = FMath::FInterpConstantTo(LateralOffset, Lane * Direction, Dt, 100.f);
        ProposedLateralOffset = ProposedOffset;
        // Follow the route ahead, including across a closed spline's seam. Agents on
        // different routes remain protected by the final collision sweep.
        for (const ALivingAgent* Other : Neighbors)
        {
            if (Other == this || !Other->bActive || Other->Route != Route || !Other->Profile) continue;
            if (FMath::Abs(ProposedOffset - Other->LateralOffset) > Profile->CollisionRadiusCm + Other->Profile->CollisionRadiusCm + 10.f) continue;
            float Ahead = (Other->RouteDistance - RouteDistance) * Direction;
            if (Route->Path->IsClosedLoop()) Ahead = FMath::Fmod(Ahead + Route->Path->GetSplineLength(), Route->Path->GetSplineLength());
            if (Ahead <= 0) continue;
            const float Gap = Ahead - Profile->CollisionRadiusCm - WheelbaseCm*.5f -
                Other->Profile->CollisionRadiusCm - Other->WheelbaseCm*.5f - (bPerson ? 30.f : 150.f);
            DesiredSpeed = FMath::Min(DesiredSpeed, LivingWorld::StoppingSpeed(Gap, Braking, Dt));
        }
        // Limit acceleration at launch/recovery. Collision remains authoritative if
        // a new obstacle appears inside the planned stopping distance.
        GroundSpeed = FMath::FInterpConstantTo(GroundSpeed, DesiredSpeed, Dt,
            DesiredSpeed < GroundSpeed ? Braking : FMath::Max(10.f, Profile->GroundAcceleration * 100.f));
        const float Travel = GroundSpeed * Dt;
        if ((Route->bOneWay || Route->bRetireAtEnds) && !Route->Path->IsClosedLoop())
        {
            const float Margin = Profile->CollisionRadiusCm + WheelbaseCm * .5f;
            const int32 ExitDirection = Route->bOneWay ? 1 : Direction;
            const float Remaining = ExitDirection > 0 ? Route->Path->GetSplineLength() - RouteDistance : RouteDistance;
            if (Remaining <= Travel + Margin) { Deactivate(); return; }
        }
        if (Route->bOneWay)
        {
            ProposedDirection = 1;
            if (!Route->Path->IsClosedLoop() && RouteDistance + Travel >= Route->Path->GetSplineLength())
            { Deactivate(); return; }
        }
        ProposedDistance = LivingWorld::AdvanceRoute(RouteDistance, Travel, Route->Path->GetSplineLength(), Route->Path->IsClosedLoop(), ProposedDirection);
        bool bSupported = Route->SampleGround(ProposedDistance, Next, Up, ProposedOffset, Profile->CollisionRadiusCm);
        if (!bSupported && bPerson && Route->ReviewedHalfWidthCm > Profile->CollisionRadiusCm)
        {
            const float Limit = Route->ReviewedHalfWidthCm - Profile->CollisionRadiusCm;
            // Keep a supported lane before considering a bounded sidestep. A
            // fixed 20 cm fallback every frame made people oscillate on slopes.
            const float SideStep = 100.f * Dt;
            for (const float Offset : {LateralOffset,
                FMath::FInterpConstantTo(LateralOffset, 0.f, Dt, 100.f),
                LateralOffset - SideStep, LateralOffset + SideStep})
            {
                const float Candidate = FMath::Clamp(Offset, -Limit, Limit);
                if (Route->SampleGround(ProposedDistance, Next, Up, Candidate, Profile->CollisionRadiusCm))
                { ProposedLateralOffset = Candidate; bSupported = true; break; }
            }
        }
        if (!bSupported) { BlockedReason = TEXT("Corridor footprint or terrain support"); Behavior = ELivingBehavior::Blocked; GroundSpeed = 0; Velocity = FVector::ZeroVector; UpdateLocomotionAnimation(false, false); return; }
        Next += Up * Profile->GroundClearanceCm;
        // Commit the lane only after the collision sweep below succeeds.
    }
    else
    {
        // Tangential flight, bounded turn rate and flock-local cohesion/separation.
        const float Radius = LivingWorld::FlightRadius(*Profile, Options.ActivityRadiusMeters);
        Up = FlightFrame.GetAxisZ();
        const FVector Radial = FVector::VectorPlaneProject(Old - Home, Up);
        const FVector Out = Radial.GetSafeNormal(SMALL_NUMBER, FlightFrame.GetAxisX());
        const FVector Tangent = FVector::CrossProduct(Up, Out) * Direction;
        // Tangent guidance starts along the flight lane instead of chasing a
        // moving point across its centre. Radial correction returns displaced agents.
        FVector Desired = Tangent * Speed + Out * FMath::Clamp((Radius - Radial.Size()) * .8f, -Speed, Speed);
        Desired += Up * FMath::Clamp((FVector::DotProduct(Home - Old, Up) + FMath::Sin(Time*.15f+Phase)*150.f)*.5f, -Speed*.2f, Speed*.2f);
        if (Profile->Kind == ELivingKind::Bird)
        {
            FVector Center = FVector::ZeroVector, Alignment = FVector::ZeroVector, Separation = FVector::ZeroVector;
            int32 Count = 0;
            for (const ALivingAgent* Other : Neighbors)
            {
                if (Other == this || !Other->bActive || Other->Flock != Flock || Other->Profile->Kind != ELivingKind::Bird ||
                    Other->FlightState == ELivingFlightState::Perched || Other->FlightState == ELivingFlightState::Landing) continue;
                const FVector Difference = Old - Other->GetActorLocation();
                const float Distance = Difference.Size();
                if (Distance > 2000.f) continue;
                Center += Other->GetActorLocation(); Alignment += Other->Velocity; ++Count;
                if (Distance < 200.f) Separation += Difference.GetSafeNormal() * (200.f - Distance) * 6.f;
            }
            if (Count) Desired += (Center / Count - Old) * 0.3f + (Alignment / Count - Velocity) * 0.2f + Separation;
        }
        if (bAlarm) Desired += (Old - *Threat).GetSafeNormal() * Speed;
        if (Profile->Kind == ELivingKind::Bird) UpdateBirdIntent(Dt, bAlarm, Speed, Desired);
        const bool bBirdArrival = Profile->Kind == ELivingKind::Bird &&
            (FlightState == ELivingFlightState::Approaching || FlightState == ELivingFlightState::Landing ||
             FlightState == ELivingFlightState::Perched || FlightState == ELivingFlightState::TakingOff);
        if (bRequireAirTerrain && !bBirdArrival)
        {
            FVector Ground;
            const FVector Ahead = Old + FVector::VectorPlaneProject(Desired, Up).GetSafeNormal() * FMath::Max(Speed * 3.f, 1500.f);
            if (!LivingGeography::Surface(GetWorld(), Ahead, Up, Ground, this))
            { Behavior = ELivingBehavior::Blocked; Velocity = FVector::ZeroVector; return; }
            const float Rise = FVector::DotProduct(Ground - Old, Up) + LivingGeography::AirClearance(*Profile) + 500.f;
            if (Rise > 0) Desired += Up * FMath::Max(0.f, FMath::Min(Speed * .65f, Rise) - FVector::DotProduct(Desired, Up));
        }
        // Look ahead before approaching streamed geometry.
        FHitResult Obstacle;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(LivingFlight), false, this);
        float LookAhead = FMath::Max(Speed, 500.f);
        if (Profile->Kind == ELivingKind::Bird && Perch.IsValid())
        {
            FVector Landing, LandingUp;
            if (Perch->SampleLanding(Landing, LandingUp, this)) LookAhead = FMath::Min(LookAhead, float(FVector::Distance(Old, Landing)));
        }
        if (!Desired.IsNearlyZero() && GetWorld()->SweepSingleByChannel(Obstacle, Old, Old + Desired.GetSafeNormal() * LookAhead,
            FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(Profile->CollisionRadiusCm), Params))
        {
            // A reserved site does not authorize flying through intervening geometry.
            if (Profile->Kind == ELivingKind::Bird && FlightState == ELivingFlightState::Approaching) TakeOff();
            Desired += Obstacle.ImpactNormal * Speed * 3.f;
        }
        const FRotator Facing = FMath::RInterpConstantTo((FlightFrame.Inverse() * GetActorQuat()).Rotator(),
            FlightFrame.UnrotateVector(Desired).Rotation(), Dt, Profile->TurnRateDegrees);
        Velocity = bBirdArrival ? Desired.GetClampedToMaxSize(Speed) : FlightFrame.RotateVector(Facing.Vector()) * Speed;
        Next = Old + Velocity * Dt;
        if (bRequireAirTerrain && !bBirdArrival)
        {
            FVector Ground;
            if (!LivingGeography::Surface(GetWorld(), Next, Up, Ground, this))
            { Behavior = ELivingBehavior::Blocked; Velocity = FVector::ZeroVector; return; }
            if (FVector::DotProduct(Next - Ground, Up) < LivingGeography::AirClearance(*Profile))
            {
                // Hold horizontal travel and climb while a newly refined tile raises the surface.
                Next = Old + Up * Speed * .3f * Dt;
            }
        }
    }
    TArray<FLivingWheelPose> Contacts;
    if (Profile->Kind == ELivingKind::Car && !Profile->Wheels.IsEmpty())
    {
        // Surface refinement may move the chassis vertically or sideways without
        // changing its heading. Steering follows the route, not that correction.
        const FVector Tangent = Route->Path->GetDirectionAtDistanceAlongSpline(ProposedDistance, ESplineCoordinateSpace::World) * ProposedDirection;
        if (!FitVehicleGround(Next, Up, Tangent))
        { BlockedReason = TEXT("Chassis contact fit"); Behavior = ELivingBehavior::Blocked; GroundSpeed = 0; Velocity = FVector::ZeroVector; UpdateLocomotionAnimation(false, false); return; }
        const FRotator Facing = FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(Tangent, Up).GetSafeNormal(), Up).Rotator();
        if (!SampleWheelContacts(FTransform(Facing, Next, GetActorScale3D()), Contacts))
        { if (BlockedReason.IsEmpty()) BlockedReason = TEXT("Wheel configuration"); Behavior = ELivingBehavior::Blocked; GroundSpeed = 0; Velocity = FVector::ZeroVector; UpdateLocomotionAnimation(false, false); return; }
    }
    FHitResult Block;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LivingMove), false, this);
    FCollisionShape Body = FCollisionShape::MakeSphere(Profile->CollisionRadiusCm);
    FQuat BodyRotation = FQuat::Identity;
    if (Profile->Kind == ELivingKind::Car && WheelbaseCm > 0)
    {
        // Wheels provide ground contact. A sphere as wide as the vehicle also
        // reaches its road surface and falsely blocks the fitted chassis.
        Body = FCollisionShape::MakeBox(FVector(WheelbaseCm*.5f + Profile->CollisionRadiusCm,
            Profile->CollisionRadiusCm, FMath::Max(10.f, FMath::Min(Profile->CollisionRadiusCm, Profile->GroundClearanceCm*.45f))));
        const FVector Tangent = Route->Path->GetDirectionAtDistanceAlongSpline(ProposedDistance, ESplineCoordinateSpace::World) * ProposedDirection;
        BodyRotation = FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(Tangent, Up).GetSafeNormal(), Up).ToQuat();
    }
    bool bBlocked = GetWorld()->SweepSingleByChannel(Block, Old, Next, BodyRotation, ECC_Pawn, Body, Params);
    if (bBlocked && bPerson && Route && Dt > 0)
    {
        // A blocked lane merge must not freeze both movement axes. Each
        // alternative still needs reviewed ground and a complete body sweep.
        const FVector2D Alternatives[] = {
            FVector2D(ProposedDistance, LateralOffset),
            FVector2D(RouteDistance, ProposedLateralOffset)
        };
        for (const FVector2D& Alternative : Alternatives)
        {
            FVector Candidate, CandidateUp;
            if (!Route->SampleGround(Alternative.X, Candidate, CandidateUp, Alternative.Y, Profile->CollisionRadiusCm)) continue;
            Candidate += CandidateUp * Profile->GroundClearanceCm;
            if (Candidate.Equals(Old, .01)) continue;
            FHitResult AlternativeHit;
            if (GetWorld()->SweepSingleByChannel(AlternativeHit, Old, Candidate, BodyRotation, ECC_Pawn, Body, Params)) continue;
            if (FMath::IsNearlyEqual(Alternative.X, double(RouteDistance)))
            { GroundSpeed = 0; ProposedDirection = Direction; }
            ProposedDistance = Alternative.X; ProposedLateralOffset = Alternative.Y;
            Next = Candidate; Up = CandidateUp; bBlocked = false;
            break;
        }
    }
    if (bBlocked)
    {
        BlockedReason = FString::Printf(TEXT("Collision: %s (overlap=%d)"), *GetNameSafe(Block.GetActor()), Block.bStartPenetrating);
        Velocity = FVector::ZeroVector; GroundSpeed = 0; Behavior = ELivingBehavior::Blocked;
        if (Profile->Kind == ELivingKind::Bird) TakeOff(); else UpdateLocomotionAnimation(false, false);
        return;
    }
    Velocity = (Next - Old) / FMath::Max(Dt, 0.001f);
    RouteDistance = ProposedDistance;
    if (LivingWorld::IsGround(Profile->Kind) && Route)
    {
        // GroundClearance is along the terrain normal, not the spline's up axis.
        // Including it in lane distance creates false lateral drift on a hillside.
        LateralOffset = ProposedLateralOffset;
    }
    Direction = ProposedDirection;
    if (!Velocity.IsNearlyZero())
    {
        FRotator Facing = FRotationMatrix::MakeFromXZ(Velocity.GetSafeNormal(), Up).Rotator();
        if (LivingWorld::IsGround(Profile->Kind) && Route)
        {
            const FVector Tangent = Route->Path->GetDirectionAtDistanceAlongSpline(ProposedDistance, ESplineCoordinateSpace::World) * Direction;
            Facing = FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(Tangent, Up).GetSafeNormal(), Up).Rotator();
        }
        if (!LivingWorld::IsGround(Profile->Kind))
        {
            FRotator LocalFacing = (FlightFrame.Inverse() * Facing.Quaternion()).Rotator();
            const float OldYaw = (FlightFrame.Inverse() * GetActorQuat()).Rotator().Yaw;
            const float YawRate = FMath::FindDeltaAngleDegrees(OldYaw, LocalFacing.Yaw) / FMath::Max(Dt, .001f);
            const float Bank = FMath::Clamp(FMath::RadiansToDegrees(FMath::Atan(Velocity.Size() * FMath::DegreesToRadians(YawRate) / 980.f)),
                -Profile->MaxBankDegrees, Profile->MaxBankDegrees);
            CurrentBank = FMath::FInterpTo(CurrentBank, Bank, Dt, 2.f);
            LocalFacing.Roll = CurrentBank;
            Facing = (FlightFrame * LocalFacing.Quaternion()).Rotator();
        }
        SetActorRotation(Facing);
    }
    SetActorLocation(Next);
    if (Profile->Kind == ELivingKind::Car && !Profile->Wheels.IsEmpty())
    {
        const float Travel = FVector::Distance(Old, Next);
        const float Curvature = Travel > 2.f ? FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(OldRotation.Yaw, GetActorRotation().Yaw)) / Travel : 0;
        UpdateWheels(Dt, Curvature, Contacts);
    }
    if (Profile->Kind != ELivingKind::Bird) UpdateLocomotionAnimation(!Velocity.IsNearlyZero(), bRunning);
    if (Profile->CruiseCycleMeters > KINDA_SMALL_NUMBER && LocomotionAnimation == 1)
    {
        const UAnimSequenceBase* Clip = Cast<UAnimSequenceBase>(Profile->CruiseAnimation.Get());
        if (Clip) AnimatedVisual->SetPlayRate(FMath::Clamp(Velocity.Size() * .01f * Clip->GetPlayLength() / Profile->CruiseCycleMeters, 0.f, 12.f));
    }
    if (Behavior == ELivingBehavior::Blocked) Behavior = ELivingBehavior::Cruising;
}
