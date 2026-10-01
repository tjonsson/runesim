#include "SimCameraStreamComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "IPixelStreaming2Module.h"
#include "IPixelStreaming2Streamer.h"
#include "IPixelStreaming2VideoProducer.h"
#include "PixelCaptureInputFrameRHI.h"
#include "RenderingThread.h"
#include "TextureResource.h"
#include "CesiumCameraManager.h"
#include "CesiumCamera.h"
#include "CesiumGeoreference.h"

namespace
{
    // A full-size feed view near the horizon made Cesium traverse tiles out to the horizon at feed
    // resolution: about 116 ms per frame (4 fps) with the tripod at 3 degrees down and 30 degrees FOV. A
    // quarter-size view costs nothing measurable, and the MainLevel feed looks the same because the
    // dataset's finest tiles are already selected at that size (compared at the near, default and zoomed poses).
    TAutoConsoleVariable<float> CVarFeedCesiumDetail(TEXT("sim.camera.CesiumDetail"), .25f,
        TEXT("Scale of the Cesium tile-selection view registered by camera feeds (tripods, sensors); 0 registers none."));
}

class FSimVideoProducer final : public IPixelStreaming2VideoProducer
{
public:
    virtual EVideoProducerCapabilities GetCapabilities() override { return EVideoProducerCapabilities::Default; }
    virtual FString ToString() override { return TEXT("RuneSim sensor camera"); }
};

USimCameraStreamComponent::USimCameraStreamComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

void USimCameraStreamComponent::BeginPlay()
{
    Super::BeginPlay();
    if (!Capture) Capture = GetOwner()->FindComponentByClass<USceneCaptureComponent2D>();
    if (Capture)
    {
        RenderTarget = NewObject<UTextureRenderTarget2D>(this);
        // FinalColorLDR supplies display-encoded video. Store those bytes in a
        // non-sRGB target so the render-target write does not encode them again.
        RenderTarget->InitCustomFormat(FMath::Clamp(Width, 160, 3840), FMath::Clamp(Height, 90, 2160), PF_B8G8R8A8, true);
        RenderTarget->TargetGamma = 2.2f;
        ConfigureCapture(Capture);
        ApplyCameraAppearance();
    }
    if (bAutoStart) StartStream();
}

void USimCameraStreamComponent::ConfigureCapture(USceneCaptureComponent2D* Target)
{
    if (!Target || !RenderTarget) return;
    Target->TextureTarget = RenderTarget;
    Target->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
    Target->bCaptureEveryFrame = false; Target->bCaptureOnMovement = false;
    // Scheduled captures still need exposure and temporal history between frames.
    Target->bAlwaysPersistRenderingState = true;
}

USceneCaptureComponent2D* USimCameraStreamComponent::GetActiveCapture() const
{
    return Override.IsValid() ? Override.Get() : Capture.Get();
}

void USimCameraStreamComponent::SetCaptureOverride(USceneCaptureComponent2D* Source)
{
    if (Override.Get() == Source) return;
    if (Override.IsValid() && Override->TextureTarget == RenderTarget) Override->TextureTarget = nullptr;
    Override = Source != Capture ? Source : nullptr;
    if (Override.IsValid())
    {
        ConfigureCapture(Override.Get());
        // The seeker inherits the sensor's calibrated look; only its optics differ.
        if (Capture) Override->PostProcessSettings = Capture->PostProcessSettings;
    }
    else if (Capture) ConfigureCapture(Capture);
    // Viewers see the switch immediately instead of waiting for the next GOP.
    if (Streamer) Streamer->ForceKeyFrame();
}

void USimCameraStreamComponent::ApplyCameraAppearance()
{
    if (!Capture) return;
    FPostProcessSettings& Look = Capture->PostProcessSettings;
    Look.bOverride_AutoExposureBias = true;
    Look.AutoExposureBias = FMath::IsFinite(ExposureCompensation) ? FMath::Clamp(ExposureCompensation, -4.f, 4.f) : 0.f;
    // Retain scene metering and white balance, without the default +1 EV boost
    // or cinematic blur/glare obscuring moving subjects in the sensor output.
    Look.bOverride_BloomIntensity = true; Look.BloomIntensity = .1f;
    Look.bOverride_MotionBlurAmount = true; Look.MotionBlurAmount = 0.f;
    Look.bOverride_LensFlareIntensity = true; Look.LensFlareIntensity = 0.f;
    Look.bOverride_SceneFringeIntensity = true; Look.SceneFringeIntensity = 0.f;
    Look.bOverride_FilmGrainIntensity = true; Look.FilmGrainIntensity = 0.f;
}

bool USimCameraStreamComponent::StartStream()
{
    if (Streamer) return true;
    if (!Capture || !RenderTarget || StreamId.IsEmpty()) return false;
    IPixelStreaming2Module& Module = IPixelStreaming2Module::Get();
    if (!Module.IsReady() || Module.FindStreamer(StreamId)) return false;
    Producer = MakeShared<FSimVideoProducer>();
    Streamer = Module.CreateStreamer(StreamId);
    if (!Streamer) { Producer.Reset(); return false; }
    Streamer->SetVideoProducer(Producer);
    Streamer->SetConnectionURL(SignallingUrl);
    // Camera streams are outputs. They do not inject browser input into FC control.
    Streamer->SetInputHandler(nullptr);
    Streamer->StartStreaming();
    return true;
}

bool USimCameraStreamComponent::IsConnected() const { return Streamer && Streamer->IsConnected(); }

void USimCameraStreamComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaTime, TickType, Function);
    if (bAutoStart && !Streamer) StartStream();
    if (!Capture || !RenderTarget || !Streamer) return;
    if (Override.IsValid() && !IsValid(Override->GetOwner())) SetCaptureOverride(nullptr);
    ViewerPollTimer -= DeltaTime;
    if (ViewerPollTimer <= 0.f) { ViewerPollTimer = .5f; ConnectedViewers = Streamer->GetConnectedPlayers().Num(); }
    if (bCaptureOnlyWhenViewed && ConnectedViewers == 0) return;
    USceneCaptureComponent2D* Active = GetActiveCapture();
    CaptureTimer += DeltaTime;
    const float Interval = 1.f / FMath::Clamp(FramesPerSecond, 1, 60);
    if (CaptureTimer < Interval) return;
    CaptureTimer = FMath::Fmod(CaptureTimer, Interval);
    if (!CesiumManager.IsValid()) CesiumManager = ACesiumCameraManager::GetDefaultCameraManager(this);
    // A tightly zoomed ground view can otherwise spend hundreds of milliseconds
    // traversing Cesium's finest tiles. Limit requested angular terrain detail,
    // retaining the real frustum, rendered resolution and optical zoom. Authored
    // route cameras independently retain the collision detail needed by agents.
    const float DetailFloor = FMath::IsFinite(TerrainDetailFovFloorDegrees) ? FMath::Clamp(TerrainDetailFovFloorDegrees, 1.f, 100.f) : 30.f;
    const float SelectionScale = FMath::Min(1.f, FMath::Tan(FMath::DegreesToRadians(Active->FOVAngle*.5f)) /
        FMath::Tan(FMath::DegreesToRadians(DetailFloor*.5f)));
    float ViewScale = FMath::IsFinite(CesiumViewScale) ? FMath::Clamp(CesiumViewScale, 0.f, 1.f) : 1.f;
    // Global feed detail (sim.camera.CesiumDetail): every registered feed view adds Cesium selection work on the game thread.
    ViewScale *= FMath::Clamp(CVarFeedCesiumDetail.GetValueOnGameThread(), 0.f, 1.f);
    if (bSkipCesiumViewAboveHorizon && ViewScale > 0.f)
    {
        FVector Up = FVector::UpVector;
        if (const ACesiumGeoreference* Georeference = ACesiumGeoreference::GetDefaultGeoreference(this))
        {
            FMatrix Frame = Georeference->ComputeEastSouthUpToUnrealTransformation(Active->GetComponentLocation());
            Up = Frame.GetUnitAxis(EAxis::Z);
        }
        const float Elevation = FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(float(FVector::DotProduct(Active->GetForwardVector(), Up)), -1.f, 1.f)));
        const float HalfVertical = FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Active->FOVAngle * .5f)) *
            float(RenderTarget->SizeY) / FMath::Max(1.f, float(RenderTarget->SizeX))));
        if (Elevation - HalfVertical > 1.f) ViewScale = 0.f;
    }
    FCesiumCamera Camera(FVector2D(RenderTarget->SizeX, RenderTarget->SizeY)*SelectionScale*ViewScale, Active->GetComponentLocation(), Active->GetComponentRotation(), Active->FOVAngle);
    if (CesiumManager.IsValid() && ViewScale <= 0.f && CesiumCameraId >= 0) { CesiumManager->RemoveCamera(CesiumCameraId); CesiumCameraId = -1; }
    else if (CesiumManager.IsValid() && ViewScale > 0.f)
    {
        if (CesiumCameraId < 0) CesiumCameraId = CesiumManager->AddCamera(Camera);
        else CesiumManager->UpdateCamera(CesiumCameraId, Camera);
    }
    Active->CaptureScene();
    LastCaptureSimulationTime = GetWorld()->GetTimeSeconds(); ++FrameNumber;
    FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
    TSharedPtr<IPixelStreaming2VideoProducer> Output = Producer;
    ENQUEUE_RENDER_COMMAND(RuneSimCameraFrame)([Resource, Output](FRHICommandListImmediate& RHICmdList)
    {
        if (Resource && Output)
        {
            FTextureRHIRef Texture = Resource->GetRenderTargetTexture();
            if (Texture) Output->PushFrame(FPixelCaptureInputFrameRHI(Texture));
        }
    });
}

void USimCameraStreamComponent::StopStream()
{
    bAutoStart = false;
    SetCaptureOverride(nullptr);
    if (Streamer)
    {
        Streamer->StopStreaming();
        if (IPixelStreaming2Module::IsAvailable()) IPixelStreaming2Module::Get().DeleteStreamer(Streamer);
        Streamer.Reset(); Producer.Reset();
        FlushRenderingCommands();
    }
    if (CesiumManager.IsValid() && CesiumCameraId >= 0) CesiumManager->RemoveCamera(CesiumCameraId);
    CesiumCameraId = -1;
}

void USimCameraStreamComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopStream();
    Super::EndPlay(Reason);
}
