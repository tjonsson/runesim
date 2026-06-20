#include "CameraQualitySubsystem.h"

#include "PIPCamera.h"
#include "CineCameraComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "SimHUD/SimHUD.h"
#include "SimHUD/SimHUDWidget.h"
#include "VRSpectator.h"

DEFINE_LOG_CATEGORY_STATIC(LogCameraQuality, Log, All);

bool UCameraQualitySubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld();
}

void UCameraQualitySubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    CurrentMode = ECameraQualityMode::Cinematic;
    bCamerasDiscovered = false;
}

TStatId UCameraQualitySubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UCameraQualitySubsystem, STATGROUP_Tickables);
}

void UCameraQualitySubsystem::DiscoverCameras()
{
    UWorld* World = GetWorld();
    if (!World) return;

    for (TActorIterator<APIPCamera> It(World); It; ++It)
    {
        APIPCamera* Camera = *It;
        bool bAlreadyTracked = false;
        for (const auto& Weak : TrackedCameras)
        {
            if (Weak.Get() == Camera)
            {
                bAlreadyTracked = true;
                break;
            }
        }
        if (bAlreadyTracked) continue;

        UCineCameraComponent* CineComp = Camera->GetCineCameraComponent();
        if (!CineComp) continue;

        OriginalSettings.Add(Camera, CineComp->PostProcessSettings);
        TrackedCameras.Add(Camera);

        if (CurrentMode == ECameraQualityMode::Cinematic)
            ApplyCinematicSettings(Camera);

        UE_LOG(LogCameraQuality, Log, TEXT("Tracking camera: %s"), *Camera->GetName());
    }

    bCamerasDiscovered = TrackedCameras.Num() > 0;
}

void UCameraQualitySubsystem::ApplyCinematicSettings(APIPCamera* Camera)
{
    UCineCameraComponent* CineComp = Camera->GetCineCameraComponent();
    if (!CineComp) return;

    FPostProcessSettings& PP = CineComp->PostProcessSettings;

    PP.bOverride_DynamicGlobalIlluminationMethod = 1;
    PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;

    PP.bOverride_ReflectionMethod = 1;
    PP.ReflectionMethod = EReflectionMethod::Lumen;

    PP.bOverride_AutoExposureBias = 1;
    PP.AutoExposureBias = 0.0f;

    PP.bOverride_BloomIntensity = 1;
    PP.BloomIntensity = 0.7f;

    PP.bOverride_MotionBlurAmount = 1;
    PP.MotionBlurAmount = 0.0f;

    PP.bOverride_AutoExposureMethod = 0;
    PP.bOverride_AutoExposureSpeedDown = 0;
    PP.bOverride_AutoExposureSpeedUp = 0;
    PP.bOverride_AutoExposureMaxBrightness = 0;
    PP.bOverride_AutoExposureMinBrightness = 0;
    PP.bOverride_AutoExposureLowPercent = 0;
    PP.bOverride_AutoExposureHighPercent = 0;
    PP.bOverride_HistogramLogMin = 0;
    PP.bOverride_HistogramLogMax = 0;
}

void UCameraQualitySubsystem::ApplySensorSettings(APIPCamera* Camera)
{
    UCineCameraComponent* CineComp = Camera->GetCineCameraComponent();
    if (!CineComp) return;

    FPostProcessSettings& PP = CineComp->PostProcessSettings;

    PP.bOverride_DynamicGlobalIlluminationMethod = 1;
    PP.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::None;

    PP.bOverride_ReflectionMethod = 1;
    PP.ReflectionMethod = EReflectionMethod::None;

    PP.bOverride_AutoExposureBias = 1;
    PP.AutoExposureBias = 0.5f;

    PP.bOverride_AutoExposureMethod = 1;
    PP.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;

    PP.bOverride_AutoExposureMinBrightness = 1;
    PP.AutoExposureMinBrightness = 0.03f;

    PP.bOverride_AutoExposureMaxBrightness = 1;
    PP.AutoExposureMaxBrightness = 6.0f;

    PP.bOverride_AutoExposureSpeedDown = 1;
    PP.AutoExposureSpeedDown = 20.0f;
    PP.bOverride_AutoExposureSpeedUp = 1;
    PP.AutoExposureSpeedUp = 20.0f;

    PP.bOverride_BloomIntensity = 0;

    PP.bOverride_MotionBlurAmount = 1;
    PP.MotionBlurAmount = 0.0f;
}

void UCameraQualitySubsystem::ApplyVRQualitySettings(ECameraQualityMode Mode)
{
    UWorld* World = GetWorld();
    if (!World || !GEngine) return;

    if (Mode == ECameraQualityMode::Cinematic)
    {
        GEngine->Exec(World, TEXT("r.AntiAliasingMethod 4"));
        GEngine->Exec(World, TEXT("r.ScreenPercentage 85"));
        GEngine->Exec(World, TEXT("r.Lumen.HardwareRayTracing 1"));
        GEngine->Exec(World, TEXT("r.Lumen.Reflections.HardwareRayTracing 1"));
        GEngine->Exec(World, TEXT("r.Shadow.Virtual.MaxPhysicalPages 4096"));
        GEngine->Exec(World, TEXT("vr.PixelDensity 1.0"));
        GEngine->Exec(World, TEXT("r.Bloom.Quality 5"));
    }
    else
    {
        GEngine->Exec(World, TEXT("r.AntiAliasingMethod 1"));
        GEngine->Exec(World, TEXT("r.ScreenPercentage 100"));
        GEngine->Exec(World, TEXT("r.Lumen.HardwareRayTracing 0"));
        GEngine->Exec(World, TEXT("r.Lumen.Reflections.HardwareRayTracing 0"));
        GEngine->Exec(World, TEXT("r.Shadow.Virtual.MaxPhysicalPages 2048"));
        GEngine->Exec(World, TEXT("vr.PixelDensity 0.8"));
        GEngine->Exec(World, TEXT("r.Bloom.Quality 1"));
    }
}

void UCameraQualitySubsystem::SetQualityMode(ECameraQualityMode NewMode)
{
    if (CurrentMode == NewMode) return;
    CurrentMode = NewMode;

    for (auto It = TrackedCameras.CreateIterator(); It; ++It)
    {
        APIPCamera* Camera = It->Get();
        if (!Camera)
        {
            OriginalSettings.Remove(Camera);
            It.RemoveCurrent();
            continue;
        }

        if (NewMode == ECameraQualityMode::Cinematic)
            ApplyCinematicSettings(Camera);
        else
            ApplySensorSettings(Camera);
    }

    // Apply VR quality settings if VR is active
    TArray<AActor*> VRActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AVRSpectator::StaticClass(), VRActors);
    for (AActor* Actor : VRActors)
    {
        AVRSpectator* VR = Cast<AVRSpectator>(Actor);
        if (VR && VR->IsVRActive())
        {
            ApplyVRQualitySettings(NewMode);
            break;
        }
    }

    ShowModeToast();
    UE_LOG(LogCameraQuality, Log, TEXT("Camera quality: %s"),
        NewMode == ECameraQualityMode::Cinematic ? TEXT("Cinematic") : TEXT("Sensor"));
}

void UCameraQualitySubsystem::ToggleQualityMode()
{
    SetQualityMode(CurrentMode == ECameraQualityMode::Cinematic
        ? ECameraQualityMode::Sensor
        : ECameraQualityMode::Cinematic);
}

void UCameraQualitySubsystem::ShowModeToast()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return;

    ASimHUD* HUD = Cast<ASimHUD>(PC->GetHUD());
    if (!HUD) return;

    USimHUDWidget* Widget = HUD->getWidget();
    if (!Widget) return;

    FString Label = CurrentMode == ECameraQualityMode::Cinematic
        ? TEXT("Cinematic") : TEXT("Sensor");
    Widget->showCameraName(Label);
}

void UCameraQualitySubsystem::Tick(float DeltaTime)
{
    UWorld* World = GetWorld();
    if (!World) return;

    if (!bCamerasDiscovered)
        DiscoverCameras();

    // Re-apply cinematic settings during the first 3 seconds to survive AirSim's late setup
    if (SettleTimer < 3.0f)
    {
        SettleTimer += DeltaTime;
        if (CurrentMode == ECameraQualityMode::Cinematic)
        {
            for (const auto& Weak : TrackedCameras)
            {
                if (APIPCamera* Cam = Weak.Get())
                    ApplyCinematicSettings(Cam);
            }
        }
    }

    APlayerController* PC = World->GetFirstPlayerController();
    if (!PC) return;

    if (PC->WasInputKeyJustPressed(EKeys::P))
        ToggleQualityMode();

    // If view target changed to a tracked camera, apply current mode
    AActor* ViewTarget = PC->GetViewTarget();
    if (ViewTarget != LastViewTarget.Get())
    {
        LastViewTarget = ViewTarget;
        APIPCamera* CamTarget = Cast<APIPCamera>(ViewTarget);
        if (CamTarget)
        {
            if (CurrentMode == ECameraQualityMode::Cinematic)
                ApplyCinematicSettings(CamTarget);
        }
    }
}
