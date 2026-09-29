#include "SimScenario.h"
#include "LivingVehicleAnimation.h"
#include "LivingHumanAnimation.h"
#include "SimCameraStreamComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimationAsset.h"
#include "Animation/BlendSpace.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/ConstructorHelpers.h"

USimTargetComponent::USimTargetComponent()
{
    static ConstructorHelpers::FObjectFinder<UNiagaraSystem> Effect(TEXT("/Game/LivingWorld/Effects/NS_Explosion.NS_Explosion"));
    ImpactEffect = Effect.Object;
}

void USimTargetComponent::ApplyVirtualDamage(float Amount)
{
    if (bDestroyed || !FMath::IsFinite(Amount) || Amount <= 0.f) return;
    Health = FMath::Max(0.f, Health - Amount);
    if (ImpactEffect) UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ImpactEffect, GetOwner()->GetActorLocation());
    if (ImpactSound) UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetOwner()->GetActorLocation(), .7f);
    if (Health <= 0.f)
    {
        bDestroyed = true;
        GetOwner()->SetActorEnableCollision(false);
        GetOwner()->SetActorHiddenInGame(true);
        OnDestroyed.Broadcast();
    }
}

ASimProjectile::ASimProjectile()
{
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("ProjectileCollision")); SetRootComponent(Collision);
    Collision->InitSphereRadius(8.f); Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Collision->OnComponentHit.AddDynamic(this,&ASimProjectile::OnImpact);
    Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("GameProjectileMovement"));
    Movement->SetUpdatedComponent(Collision); Movement->InitialSpeed=6000.f; Movement->MaxSpeed=6000.f;
    Movement->ProjectileGravityScale=0.f; Movement->bRotationFollowsVelocity=true;
    Movement->bForceSubStepping=true; Movement->HomingAccelerationMagnitude=4000.f;
    Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("ProjectileCamera")); Camera->SetupAttachment(Collision);
    Capture=CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("ProjectileCapture")); Capture->SetupAttachment(Camera);
    Stream=CreateDefaultSubobject<USimCameraStreamComponent>(TEXT("ProjectileWebRTC"));
    UStaticMeshComponent* Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Visual")); Body->SetupAttachment(Collision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Body->SetStaticMesh(Sphere.Object); Body->SetRelativeScale3D(FVector(.6,.12,.12)); Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    InitialLifeSpan=20.f;
}

void ASimProjectile::BeginPlay()
{
    Stream->StreamId = TEXT("projectile-")+GetName();
    Super::BeginPlay();
    if (GetOwner()) Collision->IgnoreActorWhenMoving(GetOwner(),true);
}

bool ASimProjectile::SetSimulatedTarget(AActor* Target)
{
    USimTargetComponent* OptIn = IsValid(Target) ? Target->FindComponentByClass<USimTargetComponent>() : nullptr;
    if (!OptIn || OptIn->bDestroyed || !Target->GetRootComponent()) return false;
    Movement->HomingTargetComponent=Target->GetRootComponent(); Movement->bIsHomingProjectile=true;
    return true;
}

void ASimProjectile::OnImpact(UPrimitiveComponent* HitComponent,AActor* OtherActor,UPrimitiveComponent* OtherComponent,FVector NormalImpulse,const FHitResult& Hit)
{
    if (IsValid(OtherActor))
        if (USimTargetComponent* Target=OtherActor->FindComponentByClass<USimTargetComponent>()) Target->ApplyVirtualDamage(Damage);
    Destroy();
}

ASimScenarioRecorder::ASimScenarioRecorder() { PrimaryActorTick.bCanEverTick=true; }
void ASimScenarioRecorder::StartRecording() { Buffer.Empty(); Timer=0.f; bRecording=true; }
void ASimScenarioRecorder::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bRecording) return;
    Timer+=DeltaTime;
    if (Timer<FMath::Max(.02f,SampleInterval)) return;
    Timer=0.f;
    // Bound memory; save in chunks for longer sessions.
    if (Buffer.Len()>16*1024*1024) { bRecording=false; return; }
    auto Frame=MakeShared<FJsonObject>(); Frame->SetNumberField(TEXT("simulation_time"),GetWorld()->GetTimeSeconds());
    TArray<TSharedPtr<FJsonValue>> Actors;
    for (AActor* Actor:Subjects)
    {
        if (!IsValid(Actor)) continue;
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
    FString Line; FJsonSerializer::Serialize(Frame,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line));
    Buffer+=Line+TEXT("\n");
}
bool ASimScenarioRecorder::StopAndSave(const FString& Name)
{
    bRecording=false;
    const FString Directory=FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("LivingWorld/Recordings"));
    IFileManager::Get().MakeDirectory(*Directory,true);
    return FFileHelper::SaveStringToFile(Buffer,*FPaths::Combine(Directory,FPaths::MakeValidFileName(Name)+TEXT(".jsonl")));
}
