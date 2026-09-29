#include "LivingTerrainBudgetComponent.h"
#include "Cesium3DTileset.h"
#include "CesiumCameraManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/Engine.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"

bool FLivingViewCadence::Update(const TArray<FCesiumCamera>& Views, float DeltaTime)
{
    bool Changed = Views.Num() != ReferenceViews.Num() || Views.IsEmpty();
    for (int32 I = 0; !Changed && I < Views.Num(); ++I)
    {
        const FCesiumCamera& A = Views[I]; const FCesiumCamera& B = ReferenceViews[I];
        Changed = !A.Location.Equals(B.Location, 1.) || !A.Rotation.Equals(B.Rotation, .01) ||
            !A.ViewportSize.Equals(B.ViewportSize, .01) ||
            !FMath::IsNearlyEqual(A.FieldOfViewDegrees, B.FieldOfViewDegrees, .01) ||
            !FMath::IsNearlyEqual(A.OverrideAspectRatio, B.OverrideAspectRatio, .0001);
    }
    if (Changed) { ReferenceViews = Views; StillSeconds = 0; }
    else StillSeconds += FMath::Clamp(DeltaTime, 0.f, .1f);
    return StillSeconds >= .5f;
}

ULivingTerrainBudgetComponent::ULivingTerrainBudgetComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

void ULivingTerrainBudgetComponent::BeginPlay()
{
    Super::BeginPlay();
    AActor* Owner = GetOwner();
    OriginalInterval = AppliedInterval = Owner->GetActorTickInterval();
    // Respect an authored cadence; only manage the normal every-frame case.
    bOwnsInterval = Cast<ACesium3DTileset>(Owner) && OriginalInterval == 0.f;
    LastTilesetTransform = Owner->GetActorTransform();
    if (bOwnsInterval) Owner->AddTickPrerequisiteComponent(this);
}

void ULivingTerrainBudgetComponent::ApplyInterval(float Interval)
{
    if (!FMath::IsNearlyEqual(AppliedInterval, Interval))
    {
        // Reset the pending cooldown too, so a moving camera does not wait out
        // the old idle interval. UE may apply this on the following tick frame.
        GetOwner()->PrimaryActorTick.UpdateTickIntervalAndCoolDown(Interval);
        AppliedInterval = Interval;
    }
    bUsingIdleCadence = Interval > OriginalInterval;
}

bool ULivingTerrainBudgetComponent::CollectViews(TArray<FCesiumCamera>& Views) const
{
    // Stereo eye poses are added inside Cesium; this observer cannot compare them.
    if (GEngine && GEngine->IsStereoscopic3D()) return false;
    ACesium3DTileset* Tileset = Cast<ACesium3DTileset>(GetOwner());
    ACesiumCameraManager* Manager = Tileset ? Tileset->ResolveCameraManager() : nullptr;
    if (!Manager) return false;
    for (FCesiumCamera View : Manager->GetAllCameras())
    {
        if (View.ParameterSource == ECameraParameterSource::CameraComponent)
        {
            UCameraComponent* Camera = View.CameraComponent.Get();
            if (!Camera) return false;
            View.Location = Camera->GetComponentLocation(); View.Rotation = Camera->GetComponentRotation();
            View.FieldOfViewDegrees = Camera->FieldOfView;
        }
        Views.Add(View);
    }
    if (Manager->UsePlayerCameras)
    {
        for (auto It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (!PC || !PC->PlayerCameraManager) return false;
            FVector Location; FRotator Rotation; int32 X, Y;
            PC->GetPlayerViewPoint(Location, Rotation); PC->GetViewportSize(X,Y);
            if (X > 0 && Y > 0) Views.Emplace(FVector2D(X,Y),Location,Rotation,PC->PlayerCameraManager->GetFOVAngle());
        }
    }
    auto AddCapture = [&Views](ASceneCapture2D* Actor)
    {
        USceneCaptureComponent2D* Capture = IsValid(Actor) ? Actor->GetCaptureComponent2D() : nullptr;
        if (Capture && Capture->TextureTarget && Capture->ProjectionType == ECameraProjectionMode::Perspective)
            Views.Emplace(FVector2D(Capture->TextureTarget->SizeX,Capture->TextureTarget->SizeY),
                Capture->GetComponentLocation(),Capture->GetComponentRotation(),Capture->FOVAngle);
    };
    if (Manager->UseSceneCapturesInLevel)
        for (TActorIterator<ASceneCapture2D> It(GetWorld()); It; ++It) AddCapture(*It);
    else
        for (ASceneCapture2D* Capture : Manager->SceneCaptures) AddCapture(Capture);
    for (const FCesiumCamera& View : Views)
        if (View.Location.ContainsNaN() || View.Rotation.ContainsNaN() || !FMath::IsFinite(View.FieldOfViewDegrees)) return false;
    return !Views.IsEmpty();
}

void ULivingTerrainBudgetComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaTime,TickType,Function);
    if (!bOwnsInterval) return;
    if (!FMath::IsNearlyEqual(GetOwner()->GetActorTickInterval(), AppliedInterval))
    { bOwnsInterval = false; bUsingIdleCadence = false; return; } // Another system took ownership.
    TArray<FCesiumCamera> Views;
    if (!bEnabled || !CollectViews(Views))
    { Cadence.Reset(); ApplyInterval(OriginalInterval); return; }
    ObservedViews = Views.Num();
    const FTransform Transform = GetOwner()->GetActorTransform();
    if (!Transform.Equals(LastTilesetTransform)) { Cadence.Reset(); LastTilesetTransform = Transform; }
    ApplyInterval(Cadence.Update(Views,DeltaTime) ? .1f : OriginalInterval);
}

void ULivingTerrainBudgetComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (bOwnsInterval)
    {
        if (FMath::IsNearlyEqual(GetOwner()->GetActorTickInterval(),AppliedInterval)) ApplyInterval(OriginalInterval);
        GetOwner()->RemoveTickPrerequisiteComponent(this);
    }
    Super::EndPlay(Reason);
}
