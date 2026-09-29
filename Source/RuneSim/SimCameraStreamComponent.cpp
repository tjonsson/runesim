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
        Capture->TextureTarget = RenderTarget;
        Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
        Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
        // Scheduled captures still need exposure and temporal history between frames.
        Capture->bAlwaysPersistRenderingState = true;
        ApplyCameraAppearance();
    }
    if (bAutoStart) StartStream();
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
    const float SelectionScale = FMath::Min(1.f, FMath::Tan(FMath::DegreesToRadians(Capture->FOVAngle*.5f)) /
        FMath::Tan(FMath::DegreesToRadians(DetailFloor*.5f)));
    FCesiumCamera Camera(FVector2D(RenderTarget->SizeX, RenderTarget->SizeY)*SelectionScale, Capture->GetComponentLocation(), Capture->GetComponentRotation(), Capture->FOVAngle);
    if (CesiumManager.IsValid())
    {
        if (CesiumCameraId < 0) CesiumCameraId = CesiumManager->AddCamera(Camera);
        else CesiumManager->UpdateCamera(CesiumCameraId, Camera);
    }
    Capture->CaptureScene();
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
