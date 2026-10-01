#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SimCameraStreamComponent.generated.h"
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class IPixelStreaming2Streamer;
class IPixelStreaming2VideoProducer;

/** Reusable WebRTC output for simulated PTZ, vehicle, and projectile cameras. */
UCLASS(ClassGroup=(Simulation), meta=(BlueprintSpawnableComponent))
class RUNESIM_API USimCameraStreamComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USimCameraStreamComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UTextureRenderTarget2D> RenderTarget;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString StreamId = TEXT("ptz-1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString SignallingUrl = TEXT("ws://127.0.0.1:8888");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAutoStart = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1",ClampMax="60")) int32 FramesPerSecond = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Width = 1280;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Height = 720;
    /** Sensor exposure relative to scene metering, in stops; zero is neutral. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Streaming|Image", meta=(ClampMin="-4",ClampMax="4"))
    float ExposureCompensation = 0.f;
    UFUNCTION(BlueprintCallable, Category="Streaming|Image") void ApplyCameraAppearance();
    /** Bound Cesium terrain refinement under extreme optical zoom; rendered FOV is unchanged. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Streaming|Cesium", meta=(ClampMin="1",ClampMax="100"))
    float TerrainDetailFovFloorDegrees = 30.f;
    /**
     * Resolution scale of this feed's Cesium tile-selection view. Every registered view costs
     * selection time each frame it moves; carried feeds (drone gimbals, seekers) use a coarse
     * view inside airspace the population and main cameras already cover. Zero registers none.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Streaming|Cesium", meta=(ClampMin="0",ClampMax="1"))
    float CesiumViewScale = 1.f;
    /**
     * A view entirely above the local horizon (e.g. tracking an aircraft) requests no terrain.
     * A narrow sky view otherwise kept refining distant horizon tiles and slowed the whole simulator.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Streaming|Cesium") bool bSkipCesiumViewAboveHorizon = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 FrameNumber = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) double LastCaptureSimulationTime = 0;
    /** Secondary feeds skip scene rendering while no player is connected. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCaptureOnlyWhenViewed = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ConnectedViewers = 0;
    /** Temporarily render another capture (e.g. an interceptor seeker) into this feed; null restores it. */
    UFUNCTION(BlueprintCallable) void SetCaptureOverride(USceneCaptureComponent2D* Source);
    UFUNCTION(BlueprintPure) USceneCaptureComponent2D* GetActiveCapture() const;
    UFUNCTION(BlueprintCallable) bool StartStream();
    UFUNCTION(BlueprintCallable) void StopStream();
    UFUNCTION(BlueprintPure) bool IsConnected() const;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
private:
    TSharedPtr<IPixelStreaming2Streamer> Streamer;
    TSharedPtr<IPixelStreaming2VideoProducer> Producer;
    float CaptureTimer = 0;
    float ViewerPollTimer = 0;
    TWeakObjectPtr<USceneCaptureComponent2D> Override;
    void ConfigureCapture(USceneCaptureComponent2D* Target);
    int32 CesiumCameraId = -1;
    TWeakObjectPtr<class ACesiumCameraManager> CesiumManager;
};
