#include "VRSpectator.h"

#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "IMotionController.h"
#include "Features/IModularFeatures.h"
#include "Kismet/GameplayStatics.h"
#include "Components/WidgetComponent.h"
#include "AirSimCameraDirector.h"
#include "SimHUD/SimHUD.h"
#include "SimHUD/SimHUDWidget.h"

typedef void (AAirSimCameraDirector::*CameraModeFunc)();
static const CameraModeFunc CameraModes[] = {
    &AAirSimCameraDirector::inputEventFpvView,
    &AAirSimCameraDirector::inputEventSpringArmChaseView,
    &AAirSimCameraDirector::inputEventFlyWithView,
    &AAirSimCameraDirector::inputEventCycleView,
    &AAirSimCameraDirector::inputEventManualView,
};
static const TCHAR* CameraModeNames[] = {
    TEXT("FPV"),
    TEXT("Chase"),
    TEXT("Fly-With-Me"),
    TEXT("Vehicle RGB"),
    TEXT("Manual"),
};
static constexpr int32 NumCameraModes = UE_ARRAY_COUNT(CameraModes);
static constexpr int32 ManualModeIndex = 4;

AVRSpectator::AVRSpectator()
{
    PrimaryActorTick.bCanEverTick = true;

    VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VRRoot"));
    RootComponent = VRRoot;

    VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
    VRCamera->SetupAttachment(VRRoot);

    // Controller poses are queried directly from IMotionController in Tick
    LeftController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftController"));
    LeftController->SetupAttachment(VRRoot);

    RightController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightController"));
    RightController->SetupAttachment(VRRoot);

    VRHUDWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("VRHUDWidget"));
    VRHUDWidget->SetupAttachment(VRCamera);
    VRHUDWidget->SetRelativeLocation(FVector(350.0f, 0.0f, -30.0f));
    VRHUDWidget->SetRelativeScale3D(FVector(0.08f));
    VRHUDWidget->SetDrawSize(FVector2D(1920, 1080));
    VRHUDWidget->SetWidgetSpace(EWidgetSpace::World);
    VRHUDWidget->SetVisibility(false);
    VRHUDWidget->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    VRHUDWidget->SetBlendMode(EWidgetBlendMode::Transparent);
}

void AVRSpectator::BeginPlay()
{
    Super::BeginPlay();
    OriginLocation = GetActorLocation();
    OriginRotation = GetActorRotation();
    SetupInputBindings();
}

void AVRSpectator::SetupInputBindings()
{
    // All input is polled in Tick.
}

// ── Helpers ────────────────────────────────────────────────────────────────

static AAirSimCameraDirector* FindCameraDirector(UWorld* World)
{
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(World, AAirSimCameraDirector::StaticClass(), Found);
    return Found.Num() > 0 ? Cast<AAirSimCameraDirector>(Found[0]) : nullptr;
}

// ── VR Toggle ──────────────────────────────────────────────────────────────

void AVRSpectator::ToggleVR()
{
    if (bVRActive) DisableVR(); else EnableVR();
}

void AVRSpectator::EnableVR()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    PreviousViewTarget = PC->GetViewTarget();

    // Snap to the current camera position before becoming the view target
    FVector CamLoc;
    FRotator CamRot;
    PC->GetPlayerViewPoint(CamLoc, CamRot);
    SetActorLocationAndRotation(CamLoc, CamRot);
    OriginLocation = CamLoc;
    OriginRotation = CamRot;

    // Reduce rendering cost for VR — stereo at 90Hz on a 3060 needs lighter settings
    GEngine->Exec(GetWorld(), TEXT("r.AntiAliasingMethod 1"));           // FXAA instead of TSR
    GEngine->Exec(GetWorld(), TEXT("r.ScreenPercentage 100"));           // no upscaling
    GEngine->Exec(GetWorld(), TEXT("r.Lumen.HardwareRayTracing 0"));     // software Lumen
    GEngine->Exec(GetWorld(), TEXT("r.Lumen.Reflections.HardwareRayTracing 0"));
    GEngine->Exec(GetWorld(), TEXT("r.Shadow.Virtual.MaxPhysicalPages 2048"));
    GEngine->Exec(GetWorld(), TEXT("vr.PixelDensity 0.8"));              // lower VR resolution
    GEngine->Exec(GetWorld(), TEXT("r.Bloom.Quality 1"));                // simpler bloom

    PC->ConsoleCommand(TEXT("vr.bEnableStereo True"));

    // Make THIS actor the view target so moving it moves the VR camera
    PC->SetViewTarget(this);

    if (GEngine && GEngine->XRSystem.IsValid())
        GEngine->XRSystem->ResetOrientationAndPosition();

    VRHUDWidget->SetVisibility(false);  // start hidden, toggle with H
    bVRActive = true;
    UE_LOG(LogTemp, Warning, TEXT("VRSpectator: VR enabled."));
}

void AVRSpectator::DisableVR()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    PC->ConsoleCommand(TEXT("vr.bEnableStereo False"));

    // Restore rendering settings for desktop
    GEngine->Exec(GetWorld(), TEXT("r.AntiAliasingMethod 4"));           // TSR
    GEngine->Exec(GetWorld(), TEXT("r.ScreenPercentage 85"));
    GEngine->Exec(GetWorld(), TEXT("r.Lumen.HardwareRayTracing 1"));
    GEngine->Exec(GetWorld(), TEXT("r.Lumen.Reflections.HardwareRayTracing 1"));
    GEngine->Exec(GetWorld(), TEXT("r.Shadow.Virtual.MaxPhysicalPages 4096"));
    GEngine->Exec(GetWorld(), TEXT("vr.PixelDensity 1.0"));
    GEngine->Exec(GetWorld(), TEXT("r.Bloom.Quality 5"));

    // Restore original view target
    if (PreviousViewTarget)
        PC->SetViewTarget(PreviousViewTarget);

    VRHUDWidget->SetVisibility(false);
    bVRActive = false;
    UE_LOG(LogTemp, Warning, TEXT("VRSpectator: VR disabled."));
}

// ── Actions ────────────────────────────────────────────────────────────────

// Vehicle RGB index (3) uses inputEventCycleView — show the actual camera number
static constexpr int32 VehicleRGBIndex = 3;

void AVRSpectator::ShowCameraToast(AAirSimCameraDirector* Dir)
{
    FString Name = CameraModeNames[CameraModeIndex];

    if (CameraModeIndex == VehicleRGBIndex && Dir)
        Name = FString::Printf(TEXT("%d"), Dir->getCycleCameraIndex());

    if (APlayerController* HPC = GetWorld()->GetFirstPlayerController())
        if (ASimHUD* HUD = Cast<ASimHUD>(HPC->GetHUD()))
            if (USimHUDWidget* W = HUD->getWidget())
                W->showCameraName(Name);
}

void AVRSpectator::OnNextCamera()
{
    AAirSimCameraDirector* Dir = FindCameraDirector(GetWorld());
    if (!Dir) return;

    CameraModeIndex = (CameraModeIndex + 1) % NumCameraModes;
    (Dir->*CameraModes[CameraModeIndex])();

    if (bVRActive)
    {
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            PreviousViewTarget = PC->GetViewTarget();
            PC->SetViewTarget(this);
        }
    }

    ShowCameraToast(Dir);
}

void AVRSpectator::OnPrevCamera()
{
    AAirSimCameraDirector* Dir = FindCameraDirector(GetWorld());
    if (!Dir) return;

    CameraModeIndex = (CameraModeIndex - 1 + NumCameraModes) % NumCameraModes;
    (Dir->*CameraModes[CameraModeIndex])();

    if (bVRActive)
    {
        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            PreviousViewTarget = PC->GetViewTarget();
            PC->SetViewTarget(this);
        }
    }

    ShowCameraToast(Dir);
}

void AVRSpectator::OnReturnToOrigin()
{
    SetActorLocationAndRotation(OriginLocation, OriginRotation);
    if (GEngine && GEngine->XRSystem.IsValid())
        GEngine->XRSystem->ResetOrientationAndPosition();
    UE_LOG(LogTemp, Warning, TEXT("VRSpectator: Returned to origin."));
}

void AVRSpectator::OnToggleHelp()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;
    ASimHUD* HUD = Cast<ASimHUD>(PC->GetHUD());
    if (HUD) HUD->inputEventToggleHelp();
}

static bool GetControllerDirection(const FName& MotionSource, FVector& OutForward, float WorldToMeters)
{
    TArray<IMotionController*> Controllers = IModularFeatures::Get().GetModularFeatureImplementations<IMotionController>(IMotionController::GetModularFeatureName());
    for (IMotionController* MC : Controllers)
    {
        FRotator Orientation;
        FVector Position;
        if (MC->GetControllerOrientationAndPosition(0, MotionSource, Orientation, Position, WorldToMeters))
        {
            OutForward = Orientation.Vector();
            return true;
        }
    }
    return false;
}

bool AVRSpectator::IsDroneAttachedCamera() const
{
    // FPV=0, Vehicle RGB=3 are fixed on the drone (no positional tracking, no movement)
    // Chase=1, Fly-With-Me=2 get 6DOF head tracking but no trigger movement
    // Manual=4 gets full 6DOF + trigger movement
    return CameraModeIndex == 0 || CameraModeIndex == 3;
}

bool AVRSpectator::IsManualCamera() const
{
    return CameraModeIndex == ManualModeIndex;
}

// ── Tick ────────────────────────────────────────────────────────────────────

void AVRSpectator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    // ── V key: toggle VR (always) ────────────────────────────────────────
    if (PC->WasInputKeyJustPressed(EKeys::V))
        ToggleVR();

    // ── Camera switching (works on desktop AND VR) ───────────────────────
    if (PC->WasInputKeyJustPressed(EKeys::N) || PC->WasInputKeyJustPressed(EKeys::C))
        OnNextCamera();
    if (PC->WasInputKeyJustPressed(EKeys::J))
        OnPrevCamera();


    if (!bVRActive)
        return;

    // ── Keyboard actions (VR only) ───────────────────────────────────────
    if (PC->WasInputKeyJustPressed(EKeys::Home))  OnReturnToOrigin();
    if (PC->WasInputKeyJustPressed(EKeys::H))     OnToggleHelp();

    // ── Vive: Grips ──────────────────────────────────────────────────────
    if (PC->WasInputKeyJustPressed(EKeys::Vive_Left_Grip_Click))
        OnReturnToOrigin();
    bFastMove = PC->IsInputKeyDown(EKeys::Vive_Right_Grip_Click);

    // ── Vive: Menu → toggle help ─────────────────────────────────────────
    if (PC->WasInputKeyJustPressed(EKeys::Vive_Left_Menu_Click))
        OnToggleHelp();

    // ── Vive: Trackpad Left/Right buttons → switch cameras ───────────────
    if (PC->WasInputKeyJustPressed(EKeys::Vive_Left_Trackpad_Left) ||
        PC->WasInputKeyJustPressed(EKeys::Vive_Right_Trackpad_Left))
        OnPrevCamera();

    if (PC->WasInputKeyJustPressed(EKeys::Vive_Left_Trackpad_Right) ||
        PC->WasInputKeyJustPressed(EKeys::Vive_Right_Trackpad_Right))
        OnNextCamera();

    // ── Read trigger input ──────────────────────────────────────────────────
    const float LeftTriggerAxis  = PC->GetInputAnalogKeyState(EKeys::Vive_Left_Trigger_Axis);
    const float RightTriggerAxis = PC->GetInputAnalogKeyState(EKeys::Vive_Right_Trigger_Axis);
    const bool  LeftTriggerBtn   = PC->IsInputKeyDown(EKeys::Vive_Left_Trigger_Click);
    const bool  RightTriggerBtn  = PC->IsInputKeyDown(EKeys::Vive_Right_Trigger_Click);

    const float LeftTrigger  = (LeftTriggerAxis  > TriggerDeadzone) ? LeftTriggerAxis  : (LeftTriggerBtn  ? 1.0f : 0.0f);
    const float RightTrigger = (RightTriggerAxis > TriggerDeadzone) ? RightTriggerAxis : (RightTriggerBtn ? 1.0f : 0.0f);

    const float Speed = MoveSpeed * (bFastMove ? FastMoveMultiplier : 1.0f) * DeltaTime;

    // Query controller aim direction directly from XR system
    const float W2M = GetWorld()->GetWorldSettings()->WorldToMeters;
    FVector RightFwd, LeftFwd;
    const bool bRTracked = GetControllerDirection(FName("RightAim"), RightFwd, W2M)
                        || GetControllerDirection(FName("Right"),    RightFwd, W2M);
    const bool bLTracked = GetControllerDirection(FName("LeftAim"),  LeftFwd, W2M)
                        || GetControllerDirection(FName("Left"),     LeftFwd, W2M);
    if (!bRTracked) RightFwd = VRCamera->GetForwardVector();
    if (!bLTracked) LeftFwd  = VRCamera->GetForwardVector();

    // Keyboard
    float KBFwd   = (PC->IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::S) ? 1.f : 0.f);
    float KBRight = (PC->IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::A) ? 1.f : 0.f);
    float KBUp    = (PC->IsInputKeyDown(EKeys::E) ? 1.f : 0.f) - (PC->IsInputKeyDown(EKeys::Q) ? 1.f : 0.f);
    bool bKBMoving = !FMath::IsNearlyZero(KBFwd) || !FMath::IsNearlyZero(KBRight) || !FMath::IsNearlyZero(KBUp);

    const bool bDroneCamera = IsDroneAttachedCamera();
    const bool bTriggerMoving = (RightTrigger > TriggerDeadzone) || (LeftTrigger > TriggerDeadzone);
    const bool bWantsToMove = bTriggerMoving || bKBMoving;

    // Debug: log state every 2 seconds
    static int32 DbgCnt = 0;
    if (++DbgCnt % 120 == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("VR: cam=%d(%s) drone=%d Rtrig=%.2f Ltrig=%.2f Ltrack=%d Rtrack=%d"),
            CameraModeIndex, CameraModeNames[CameraModeIndex],
            bDroneCamera ? 1 : 0, RightTrigger, LeftTrigger,
            bLTracked ? 1 : 0, bRTracked ? 1 : 0);
    }

    // ── Manual camera: trigger + keyboard movement ─────────────────────────
    const bool bManual = IsManualCamera();
    if (bManual && bWantsToMove)
    {
        if (RightTrigger > TriggerDeadzone)
            AddActorWorldOffset(RightFwd * RightTrigger * Speed);

        if (LeftTrigger > TriggerDeadzone)
            AddActorWorldOffset(-LeftFwd * LeftTrigger * Speed);

        if (bKBMoving)
            AddActorWorldOffset(
                VRCamera->GetForwardVector() * KBFwd   * Speed +
                VRCamera->GetRightVector()   * KBRight * Speed +
                FVector::UpVector             * KBUp    * Speed);
    }

    // ── Follow drone (non-manual cameras) ──────────────────────────────────
    if (!bManual && PreviousViewTarget && PreviousViewTarget != this)
    {
        FVector TargetLoc;
        FRotator TargetRot;

        if (APawn* Pawn = Cast<APawn>(PreviousViewTarget))
        {
            FMinimalViewInfo ViewInfo;
            Pawn->CalcCamera(DeltaTime, ViewInfo);
            TargetLoc = ViewInfo.Location;
            TargetRot = ViewInfo.Rotation;
        }
        else
        {
            TargetLoc = PreviousViewTarget->GetActorLocation();
            TargetRot = PreviousViewTarget->GetActorRotation();
        }

        if (bDroneCamera)
        {
            // FPV / Vehicle RGB: cancel HMD positional tracking (rotation only)
            if (GEngine && GEngine->XRSystem.IsValid())
            {
                FQuat HMDOri;
                FVector HMDPos;
                GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, HMDOri, HMDPos);
                TargetLoc -= HMDPos;
            }
        }
        // Chase / Fly-With-Me: follow drone with full 6DOF head tracking (no cancellation)

        VRRoot->SetWorldLocationAndRotation(TargetLoc, TargetRot);
    }
}
