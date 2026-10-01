#include "SimScenario.h"
#include "LivingVehicleAnimation.h"
#include "LivingHumanAnimation.h"
#include "LivingAgent.h"
#include "SimCameraStreamComponent.h"
#include "SimWarEffects.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Animation/BlendSpace.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

namespace
{
template <typename T> T* OptionalAsset(const TCHAR* Path)
{
    // Effects fire many times a second (smoke trails, bursts). Keep each asset loaded after first use and
    // remember missing ones: a reload during play flushes async loading and hitches Cesium-heavy scenes.
    static TMap<FString, TWeakObjectPtr<UObject>> Known;
    static TSet<FString> Missing;
    if (Missing.Contains(Path)) return nullptr;
    if (const TWeakObjectPtr<UObject>* Found = Known.Find(Path)) if (Found->IsValid()) return Cast<T>(Found->Get());
    T* Loaded = LoadObject<T>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
    // Only in the packaged simulator: in the editor, rooted assets could not be re-authored or deleted.
    if (!Loaded) { if (!GIsEditor) Missing.Add(Path); return nullptr; }
    if (!GIsEditor) Loaded->AddToRoot(); // A handful of small effect and sound assets, held for the session.
    Known.Add(Path, Loaded);
    return Loaded;
}
TSharedRef<FJsonObject> EventObject(const FSimEvent& Event)
{
    auto Object = MakeShared<FJsonObject>();
    Object->SetNumberField(TEXT("time"), Event.Time);
    Object->SetStringField(TEXT("type"), Event.Type.ToString());
    Object->SetStringField(TEXT("location_cm"), Event.Location.ToString());
    Object->SetStringField(TEXT("subject"), Event.Subject);
    if (!Event.Detail.IsEmpty()) Object->SetStringField(TEXT("detail"), Event.Detail);
    return Object;
}
FString EventJson(const FSimEvent& Event)
{
    FString Line; FJsonSerializer::Serialize(EventObject(Event), TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line));
    return Line;
}
}

namespace { bool GLiveOutput = true; }
void SimEvents::SetLiveOutput(bool bEnabled) { GLiveOutput = bEnabled; }

bool SimEvents::IsReplayable(FName Type)
{
    static const FName Types[] = { TEXT("launch"), TEXT("detonation"), TEXT("impact"), TEXT("shot_down"), TEXT("crash"), TEXT("miss"),
        TEXT("strike"), TEXT("fire"), TEXT("smoke_screen"), TEXT("impact_metal"), TEXT("impact_rock"), TEXT("impact_sand"), TEXT("impact_water") };
    for (const FName& Known : Types) if (Known == Type) return true;
    return false;
}

void SimEvents::PlayEffect(UWorld* World, FName Type, const FVector& Location, float Scale)
{
    Scale = FMath::IsFinite(Scale) ? FMath::Clamp(Scale, .25f, 8.f) : 1.f;
    if (!GLiveOutput || !World || World->IsNetMode(NM_DedicatedServer) || Location.ContainsNaN()) return;
    // Ground battle effects: the war layer owns sustained fires, smoke and fluid level of detail.
    if (USimWarEffects* War = World->GetSubsystem<USimWarEffects>())
    {
        if (Type == TEXT("strike")) { War->Strike(Location, Scale); return; }
        if (Type == TEXT("crash")) { War->Strike(Location, Scale, true); return; }
        if (Type == TEXT("fire")) { War->StartFire(Location, Scale, 60.f); return; }
        if (Type == TEXT("smoke_screen")) { War->SmokeScreen(Location, Scale, 45.f); return; }
        const FString Name = Type.ToString();
        if (Name.StartsWith(TEXT("impact_"))) { War->Impact(Location, FName(*Name.RightChop(7)), Scale); return; }
    }
    const TCHAR* Effect = nullptr; const TCHAR* Sound = nullptr; float Volume = 1.f;
    if (Type == TEXT("launch")) { Effect = TEXT("/Game/LivingWorld/Effects/NS_SmokePuff.NS_SmokePuff"); Sound = TEXT("/Game/LivingWorld/Audio/Combat/S_MissileLaunch.S_MissileLaunch"); }
    else if (Type == TEXT("detonation") || Type == TEXT("shot_down"))
    {
        // Air bursts: long, large flash (visible on 10 Hz image streams), debris and dark smoke.
        Effect = OptionalAsset<UNiagaraSystem>(TEXT("/Game/LivingWorld/Effects/NS_AirBurst.NS_AirBurst")) ?
            TEXT("/Game/LivingWorld/Effects/NS_AirBurst.NS_AirBurst") : TEXT("/Game/LivingWorld/Effects/NS_Explosion.NS_Explosion");
        Sound = TEXT("/Game/LivingWorld/Audio/Combat/S_Explosion.S_Explosion");
    }
    else if (Type == TEXT("crash"))
    { Effect = TEXT("/Game/LivingWorld/Effects/NS_Explosion.NS_Explosion"); Sound = TEXT("/Game/LivingWorld/Audio/Combat/S_Explosion.S_Explosion"); }
    else if (Type == TEXT("trail")) Effect = TEXT("/Game/LivingWorld/Effects/NS_MissileTrail.NS_MissileTrail");
    else if (Type == TEXT("impact") || Type == TEXT("miss"))
    { Effect = TEXT("/Game/LivingWorld/Effects/NS_ImpactBurst.NS_ImpactBurst"); Sound = TEXT("/Game/LivingWorld/Audio/Combat/S_Explosion.S_Explosion"); Volume = .45f; }
    else if (Type == TEXT("sparks")) Effect = TEXT("/Game/LivingWorld/Effects/NS_ElectricalSparks.NS_ElectricalSparks");
    else if (Type == TEXT("smoke")) Effect = TEXT("/Game/LivingWorld/Effects/NS_SmokePuff.NS_SmokePuff");
    if (Effect) if (UNiagaraSystem* System = OptionalAsset<UNiagaraSystem>(Effect))
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location, FRotator::ZeroRotator, FVector(Scale));
        // Large targets break up in several bursts spread over their size.
        FRandomStream Random(GetTypeHash(Location));
        for (int32 Extra = 0; Extra < FMath::Min(4, FMath::FloorToInt32(Scale) - 1); ++Extra)
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, System, Location + Random.VRand() * 150.f * Scale,
                FRotator::ZeroRotator, FVector(Scale * .7f));
    }
    if (Sound) if (USoundBase* Asset = OptionalAsset<USoundBase>(Sound))
        UGameplayStatics::PlaySoundAtLocation(World, Asset, Location, Volume);
    // Electrical damage accompanies explosions on machinery.
    if (Type == TEXT("shot_down"))
        if (UNiagaraSystem* Sparks = OptionalAsset<UNiagaraSystem>(TEXT("/Game/LivingWorld/Effects/NS_ElectricalSparks.NS_ElectricalSparks")))
            UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, Sparks, Location);
}

void SimEvents::Record(UWorld* World, FName Type, const FVector& Location, const FString& Subject, const FString& Detail)
{
    if (!World) return;
    FSimEvent Event; Event.Time = World->GetTimeSeconds(); Event.Type = Type; Event.Location = Location;
    Event.Subject = Subject.Left(128); Event.Detail = Detail.Left(512);
    for (TActorIterator<ASimScenarioRecorder> It(World); It; ++It) if (It->IsRecording()) It->AddEvent(Event);
    // Only real sessions (PIE or packaged, which own a game instance) write the log.
    if (!GLiveOutput || !World->IsGameWorld() || !World->GetGameInstance()) return;
    // Engagement log: bounded append-only JSONL for after-action review.
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LivingWorld/Combat"));
    const FString Path = FPaths::Combine(Directory, TEXT("Engagements_") + FDateTime::Now().ToString(TEXT("%Y%m%d")) + TEXT(".jsonl"));
    if (IFileManager::Get().FileSize(*Path) > 32 * 1024 * 1024) return;
    IFileManager::Get().MakeDirectory(*Directory, true);
    FFileHelper::SaveStringToFile(EventJson(Event) + TEXT("\n"), *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
        &IFileManager::Get(), FILEWRITE_Append);
}

USimTargetComponent::USimTargetComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.f;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Effect(TEXT("/Game/LivingWorld/Effects/NS_Explosion.NS_Explosion"));
    ImpactEffect = Effect.Object;
}

bool USimTargetComponent::CanBeEngaged() const
{
    const AActor* Owner = GetOwner();
    return bEngageable && !bDestroyed && IsValid(Owner) && !Owner->IsHidden() && Owner->GetRootComponent();
}

void USimTargetComponent::ResetTarget(float InHealth)
{
    MaxHealth = Health = FMath::IsFinite(InHealth) && InHealth > 0.f ? InHealth : 100.f;
    bDestroyed = false; bEngageable = true; bHasLastLocation = false; EstimatedVelocity = FVector::ZeroVector;
}

FVector USimTargetComponent::GetTargetVelocity() const
{
    if (const ALivingAgent* Agent = Cast<ALivingAgent>(GetOwner())) return Agent->Velocity;
    return EstimatedVelocity;
}

void USimTargetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaTime, TickType, Function);
    if (!GetOwner() || DeltaTime <= 0.f) return;
    const FVector Location = GetOwner()->GetActorLocation();
    if (bHasLastLocation) EstimatedVelocity = FMath::VInterpTo(EstimatedVelocity, (Location - LastLocation) / DeltaTime, DeltaTime, 8.f);
    LastLocation = Location; bHasLastLocation = true;
}

void USimTargetComponent::ApplyVirtualDamage(float Amount)
{
    if (bDestroyed || !FMath::IsFinite(Amount) || Amount <= 0.f) return;
    Health = FMath::Max(0.f, Health - Amount);
    if (ImpactEffect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactEffect, GetOwner()->GetActorLocation());
    if (ImpactSound) UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetOwner()->GetActorLocation(), .7f);
    SimEvents::Record(GetWorld(), TEXT("hit"), GetOwner()->GetActorLocation(), GetOwner()->GetName(),
        FString::Printf(TEXT("damage=%.1f health=%.1f"), Amount, Health));
    if (Health <= 0.f)
    {
        bDestroyed = true;
        if (bHideOnDestroyed)
        {
            GetOwner()->SetActorEnableCollision(false);
            GetOwner()->SetActorHiddenInGame(true);
        }
        SimEvents::Record(GetWorld(), TEXT("destroyed"), GetOwner()->GetActorLocation(), GetOwner()->GetName(), Category);
        OnDestroyedNative.Broadcast(this);
        OnDestroyed.Broadcast();
    }
}

FVector SimGuidance::Acceleration(const FVector& MissilePosition, const FVector& MissileVelocity,
    const FVector& TargetPosition, const FVector& TargetVelocity, float N, float MaxAcceleration)
{
    const FVector Range = TargetPosition - MissilePosition;
    const double Distance2 = Range.SizeSquared();
    if (Distance2 < 1.0 || MissileVelocity.IsNearlyZero() || !FMath::IsFinite(N)) return FVector::ZeroVector;
    const FVector Relative = TargetVelocity - MissileVelocity;
    // Line-of-sight rotation rate; true PN commands N * Vc * LOS rate, perpendicular to the LOS.
    const FVector Omega = FVector::CrossProduct(Range, Relative) / Distance2;
    const float Closing = -FVector::DotProduct(Relative, Range.GetUnsafeNormal());
    FVector Command = FVector::CrossProduct(Omega, Range.GetUnsafeNormal()) * (N * FMath::Max(Closing, 0.f));
    // Receding geometry has no useful PN solution: steer the velocity toward the target instead.
    if (Closing <= 0.f)
        Command = (Range.GetUnsafeNormal() * MissileVelocity.Size() - MissileVelocity) * 2.f;
    Command = FVector::VectorPlaneProject(Command, MissileVelocity.GetUnsafeNormal());
    return Command.GetClampedToMaxSize(FMath::Max(0.f, MaxAcceleration));
}

ASimProjectile::ASimProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("ProjectileCollision")); SetRootComponent(Collision);
    Collision->InitSphereRadius(8.f); Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Collision->OnComponentHit.AddDynamic(this,&ASimProjectile::OnImpact);
    Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("GameProjectileMovement"));
    Movement->SetUpdatedComponent(Collision); Movement->InitialSpeed=LaunchSpeed; Movement->MaxSpeed=0.f;
    Movement->ProjectileGravityScale=0.f; Movement->bRotationFollowsVelocity=true;
    Movement->bForceSubStepping=true; Movement->MaxSimulationTimeStep=1.f/120.f;
    Movement->bIsHomingProjectile=false;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("ProjectileCamera")); Camera->SetupAttachment(Collision);
    Camera->SetFieldOfView(40.f); Camera->SetRelativeLocation(FVector(40,0,0));
    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ProjectileCapture")); Capture->SetupAttachment(Camera);
    Capture->FOVAngle=40.f; Capture->bCaptureEveryFrame=false; Capture->bCaptureOnMovement=false;
    Stream=CreateDefaultSubobject<USimCameraStreamComponent>(TEXT("ProjectileWebRTC"));
    ChaseArm=CreateDefaultSubobject<USpringArmComponent>(TEXT("ChaseArm")); ChaseArm->SetupAttachment(Collision);
    // Behind, above and to the side: the trail streams below the view instead of between camera and missile.
    ChaseArm->TargetArmLength=1800.f; ChaseArm->SocketOffset=FVector(0,300,450);
    ChaseArm->bDoCollisionTest=false; ChaseArm->bEnableCameraRotationLag=true; ChaseArm->CameraRotationLagSpeed=5.f;
    ChaseCapture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ChaseCapture"));
    ChaseCapture->SetupAttachment(ChaseArm, USpringArmComponent::SocketName);
    ChaseCapture->FOVAngle=65.f; ChaseCapture->bCaptureEveryFrame=false; ChaseCapture->bCaptureOnMovement=false;
    UStaticMeshComponent* Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Body->SetupAttachment(Collision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    // 1.6 m x 12 cm airframe, long axis along +X.
    Body->SetStaticMesh(Cylinder.Object); Body->SetRelativeRotation(FRotator(-90,0,0)); Body->SetRelativeScale3D(FVector(.12,.12,1.6));
    Body->SetRelativeLocation(FVector(-80,0,0)); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MotorAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("MotorAudio")); MotorAudio->SetupAttachment(Collision);
    MotorAudio->bAutoActivate=false; MotorAudio->bOverrideAttenuation=true;
    MotorAudio->AttenuationOverrides.bAttenuate=true; MotorAudio->AttenuationOverrides.bSpatialize=true;
    MotorAudio->AttenuationOverrides.FalloffDistance=20000.f;
    InitialLifeSpan=0.f;
}

void ASimProjectile::BeginPlay()
{
    Stream->StreamId = TEXT("projectile-")+GetName();
    Super::BeginPlay();
    if (GetOwner()) Collision->IgnoreActorWhenMoving(GetOwner(),true);
    if (USoundBase* Motor = OptionalAsset<USoundBase>(TEXT("/Game/LivingWorld/Audio/Combat/S_MissileMotor.S_MissileMotor")))
    { MotorAudio->SetSound(Motor); MotorAudio->Play(); }
    SimEvents::Record(GetWorld(), TEXT("launch"), GetActorLocation(), GetName(), GetNameSafe(Target.Get()));
    SimEvents::PlayEffect(GetWorld(), TEXT("launch"), GetActorLocation());
}

bool ASimProjectile::SetSimulatedTarget(AActor* InTarget)
{
    USimTargetComponent* OptIn = IsValid(InTarget) ? InTarget->FindComponentByClass<USimTargetComponent>() : nullptr;
    if (!OptIn || !OptIn->CanBeEngaged()) return false;
    Target = InTarget;
    return true;
}

void ASimProjectile::Guide(float Dt)
{
    if (Result != ESimInterceptorResult::InFlight || Dt <= 0.f) return;
    FlightTime += Dt;
    FVector Velocity = Movement->Velocity;
    if (Velocity.IsNearlyZero()) Velocity = GetActorForwardVector() * LaunchSpeed;
    const float Burn = FMath::Max(.1f, BurnTime);
    const float Speed = FlightTime < Burn ? FMath::Lerp(LaunchSpeed, BurnoutSpeed, FlightTime / Burn) :
        FMath::Max(LaunchSpeed, BurnoutSpeed * FMath::Exp(-(FlightTime - Burn) * .04f)); // Coasting drag.
    USimTargetComponent* Opt = Target.IsValid() ? Target->FindComponentByClass<USimTargetComponent>() : nullptr;
    if (Opt && Opt->CanBeEngaged())
    {
        const FVector TargetPosition = Target->GetActorLocation();
        const float Distance = FVector::Distance(TargetPosition, GetActorLocation());
        ClosestApproachCm = FMath::Min(ClosestApproachCm, Distance);
        // Fuse on the target's extent, not its centre: a 35 m-span aircraft is hit well off-centre.
        const float Fuse = FMath::Max(ProximityFuseCm, 100.f) + Target->GetSimpleCollisionRadius();
        if (Distance <= Fuse)
        {
            Detonate(ESimInterceptorResult::Hit, nullptr);
            return;
        }
        const FVector Command = SimGuidance::Acceleration(GetActorLocation(), Velocity, TargetPosition,
            Opt->GetTargetVelocity(), NavigationConstant, MaxLateralG * 980.f);
        Velocity += Command * Dt;
    }
    Movement->Velocity = Velocity.GetSafeNormal() * Speed;
    if (FlightTime >= MaxFlightTime) Detonate(ESimInterceptorResult::Missed, nullptr);
}

void ASimProjectile::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    Guide(DeltaTime);
    if (Result != ESimInterceptorResult::InFlight) return;
    // Motor smoke trail during burn, then a thinning trail; bounded to ~10 puffs/s.
    TrailTimer += DeltaTime;
    const float Interval = FlightTime < BurnTime ? .1f : .35f;
    if (TrailTimer >= Interval && FlightTime < BurnTime + 6.f)
    {
        TrailTimer = 0.f;
        SimEvents::PlayEffect(GetWorld(), TEXT("trail"), GetActorLocation() - GetActorForwardVector() * 120.f);
    }
    if (MotorAudio->IsPlaying() && FlightTime > BurnTime) MotorAudio->FadeOut(.5f, 0.f);
}

void ASimProjectile::Abort()
{
    if (Result == ESimInterceptorResult::InFlight) Detonate(ESimInterceptorResult::Aborted, nullptr);
}

void ASimProjectile::Detonate(ESimInterceptorResult Outcome, AActor* HitActor)
{
    if (Result != ESimInterceptorResult::InFlight) return;
    const FVector Location = GetActorLocation();
    // Blast: full damage on a direct hit, 60% at the fuse radius, fading to nothing at 1.5x.
    // A ground burst near a vehicle therefore still damages it.
    const float Fuse = FMath::Max(ProximityFuseCm, 100.f), Reach = Fuse * 1.5f;
    bool bDamaged = false;
    TArray<USimTargetComponent*> InBlast;
    for (TObjectIterator<USimTargetComponent> It; It; ++It)
        if (It->GetWorld() == GetWorld() && It->CanBeEngaged() && It->GetOwner() != GetOwner()) InBlast.Add(*It);
    for (USimTargetComponent* Victim : InBlast)
    {
        const float Distance = Victim->GetOwner() == HitActor ? 0.f : FMath::Max(0.f,
            float(FVector::Distance(Victim->GetOwner()->GetActorLocation(), Location)) - Victim->GetOwner()->GetSimpleCollisionRadius());
        if (Distance > Reach) continue;
        const float Scale = Distance <= Fuse ? FMath::Lerp(1.f, .6f, Distance / Fuse) : FMath::Lerp(.6f, 0.f, (Distance - Fuse) / (Reach - Fuse));
        Victim->ApplyVirtualDamage(Damage * Scale);
        bDamaged = true;
    }
    if (Outcome == ESimInterceptorResult::Missed && bDamaged) Outcome = ESimInterceptorResult::Hit;
    Result = Outcome;
    const TCHAR* Type = Outcome == ESimInterceptorResult::Hit ? TEXT("detonation") : Outcome == ESimInterceptorResult::Aborted ? TEXT("abort") : TEXT("miss");
    SimEvents::Record(GetWorld(), Type, Location, GetName(), FString::Printf(TEXT("target=%s closest_m=%.1f flight_s=%.2f"),
        *GetNameSafe(HitActor ? HitActor : Target.Get()), ClosestApproachCm < BIG_NUMBER ? ClosestApproachCm / 100.f : -1.f, FlightTime));
    SimEvents::PlayEffect(GetWorld(), Outcome == ESimInterceptorResult::Hit || Outcome == ESimInterceptorResult::Aborted ? FName(TEXT("detonation")) : FName(TEXT("miss")), Location, 1.5f);
    Movement->StopMovementImmediately();
    SetActorHiddenInGame(true); SetActorEnableCollision(false);
    MotorAudio->Stop();
    OnResolvedNative.Broadcast(this, Outcome);
    OnResolved.Broadcast(this, Outcome);
    // Keep the actor briefly so a switched camera feed can show the burst before returning.
    SetLifeSpan(FMath::Clamp(LingerSeconds, .5f, 10.f));
}

void ASimProjectile::OnImpact(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,FVector NormalImpulse,const FHitResult& Hit)
{
    if (Result != ESimInterceptorResult::InFlight) return;
    USimTargetComponent* HitTarget = IsValid(OtherActor) ? OtherActor->FindComponentByClass<USimTargetComponent>() : nullptr;
    // Contact fuse. Terrain or unrelated objects detonate too; the blast may still reach a target.
    Detonate(HitTarget && HitTarget->CanBeEngaged() ? ESimInterceptorResult::Hit : ESimInterceptorResult::Missed, OtherActor);
}

void ASimProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Result == ESimInterceptorResult::InFlight && Reason == EEndPlayReason::Destroyed)
    { Result = ESimInterceptorResult::Aborted; OnResolvedNative.Broadcast(this, Result); OnResolved.Broadcast(this, Result); }
    Super::EndPlay(Reason);
}

ASimScenarioRecorder::ASimScenarioRecorder() { PrimaryActorTick.bCanEverTick=true; }
void ASimScenarioRecorder::StartRecording() { Buffer.Empty(); PendingEvents.Empty(); Timer=0.f; bRecording=true; }
void ASimScenarioRecorder::AddEvent(const FSimEvent& Event)
{
    if (bRecording && PendingEvents.Num() < 4096) PendingEvents.Add(Event);
}
void ASimScenarioRecorder::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bRecording) return;
    Timer+=DeltaTime;
    if (Timer<FMath::Max(.02f,SampleInterval)) return;
    Timer=0.f;
    // Bound memory; save in chunks for longer sessions.
    if (Buffer.Len()>16*1024*1024) { bRecording=false; return; }
    WriteFrame(false);
}
void ASimScenarioRecorder::WriteFrame(bool bFinal)
{
    auto Frame=MakeShared<FJsonObject>(); Frame->SetNumberField(TEXT("simulation_time"),GetWorld()->GetTimeSeconds());
    TArray<TSharedPtr<FJsonValue>> Actors;
    TArray<AActor*> Sampled;
    for (AActor* Actor:Subjects) if (IsValid(Actor)) Sampled.Add(Actor);
    if (bTrackProjectiles)
        for (TActorIterator<ASimProjectile> It(GetWorld()); It; ++It)
            if (Sampled.Num() < 512 && !Sampled.Contains(*It)) Sampled.Add(*It);
    if (bFinal) Sampled.Reset();
    for (AActor* Actor:Sampled)
    {
        auto Item=MakeShared<FJsonObject>(); Item->SetStringField(TEXT("id"),Actor->GetName());
        const FVector P=Actor->GetActorLocation(); const FRotator R=Actor->GetActorRotation();
        Item->SetStringField(TEXT("position_cm"),P.ToString()); Item->SetStringField(TEXT("rotation_deg"),R.ToString());
        Item->SetStringField(TEXT("scale"),Actor->GetActorScale3D().ToString());
        Item->SetBoolField(TEXT("hidden"),Actor->IsHidden());
        // Replay instantiates inert visual components; it never reuses the live actor.
        UMeshComponent* Visual = nullptr;
        if (USkeletalMeshComponent* Skeletal = Actor->FindComponentByClass<USkeletalMeshComponent>(); Skeletal && Skeletal->GetSkeletalMeshAsset())
        {
            Visual = Skeletal;
            Item->SetStringField(TEXT("skeletal_mesh"),Skeletal->GetSkeletalMeshAsset()->GetPathName());
            if (UAnimSingleNodeInstance* Animation = Skeletal->GetSingleNodeInstance(); Animation && Animation->GetCurrentAsset())
            {
                Item->SetStringField(TEXT("animation"),Animation->GetCurrentAsset()->GetPathName());
                Item->SetNumberField(TEXT("animation_time"),Animation->GetCurrentTime());
                if (const auto* Human = Cast<ULivingHumanAnimation>(Animation); Human && Human->FootSupports.Num() == 2)
                {
                    TArray<TSharedPtr<FJsonValue>> Feet;
                    for (const FLivingFootSupport& Support : Human->FootSupports)
                    {
                        auto Foot = MakeShared<FJsonObject>();
                        Foot->SetStringField(TEXT("bone"), Support.Bone.ToString());
                        Foot->SetNumberField(TEXT("height_cm"), Support.HeightCm);
                        Foot->SetStringField(TEXT("stance_offset_cm"), Support.StanceOffsetCm.ToString());
                        Feet.Add(MakeShared<FJsonValueObject>(Foot));
                    }
                    Item->SetArrayField(TEXT("foot_supports"), Feet);
                }
                if (const auto* Vehicle = Cast<ULivingVehicleAnimation>(Animation))
                {
                    TArray<TSharedPtr<FJsonValue>> Wheels;
                    for (const FLivingWheelPose& Pose : Vehicle->WheelPoses)
                    {
                        auto Wheel = MakeShared<FJsonObject>();
                        Wheel->SetStringField(TEXT("bone"), Pose.Bone.ToString());
                        Wheel->SetNumberField(TEXT("steering_deg"), Pose.SteeringDegrees);
                        Wheel->SetStringField(TEXT("offset_cm"), Pose.Offset.ToString());
                        Wheels.Add(MakeShared<FJsonValueObject>(Wheel));
                    }
                    Item->SetArrayField(TEXT("wheel_poses"), Wheels);
                }
                if (Cast<UBlendSpace>(Animation->GetCurrentAsset()))
                {
                    FVector Input, Filtered;
                    Animation->GetBlendSpaceState(Input, Filtered);
                    Item->SetStringField(TEXT("blend_position"), Filtered.ToString());
                }
            }
        }
        else if (UStaticMeshComponent* Static = Actor->FindComponentByClass<UStaticMeshComponent>(); Static && Static->GetStaticMesh())
        {
            Visual = Static;
            Item->SetStringField(TEXT("static_mesh"),Static->GetStaticMesh()->GetPathName());
        }
        if (Visual) Item->SetStringField(TEXT("visual_transform"),Visual->GetComponentTransform().GetRelativeTransform(Actor->GetActorTransform()).ToString());
        if (USimTargetComponent* Target=Actor->FindComponentByClass<USimTargetComponent>()) Item->SetNumberField(TEXT("health"),Target->Health);
        Actors.Add(MakeShared<FJsonValueObject>(Item));
    }
    Frame->SetArrayField(TEXT("actors"),Actors);
    if (!PendingEvents.IsEmpty())
    {
        TArray<TSharedPtr<FJsonValue>> Events;
        for (const FSimEvent& Event : PendingEvents) Events.Add(MakeShared<FJsonValueObject>(EventObject(Event)));
        Frame->SetArrayField(TEXT("events"),Events);
        PendingEvents.Reset();
    }
    else if (bFinal) return;
    FString Line; FJsonSerializer::Serialize(Frame,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line));
    Buffer+=Line+TEXT("\n");
}
bool ASimScenarioRecorder::StopAndSave(const FString& Name)
{
    // Events after the last sample still belong to the recording.
    if (bRecording && !Buffer.IsEmpty()) WriteFrame(true);
    bRecording=false;
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("LivingWorld/Recordings"));
    IFileManager::Get().MakeDirectory(*Directory,true);
    return FFileHelper::SaveStringToFile(Buffer,*FPaths::Combine(Directory,FPaths::MakeValidFileName(Name)+TEXT(".jsonl")));
}
