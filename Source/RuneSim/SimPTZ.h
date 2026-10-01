#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IWebSocket.h"
#include "SimScenario.h"
#include "SimPTZ.generated.h"

/** What the tripod's own stream shows while an interceptor flies. */
UENUM(BlueprintType)
enum class ESimPTZView : uint8 { Tripod, Missile, Chase };
class UCameraComponent;
class USceneCaptureComponent2D;
class USimCameraStreamComponent;
class UCesiumGlobeAnchorComponent;
class ASimProjectile;
class ASimFollowCamera;


/** A simulated camera tripod. ROS commands affect only pan/tilt/zoom in this world. */
UCLASS()
class RUNESIM_API ASimPTZ : public AActor
{
    GENERATED_BODY()
public:
    ASimPTZ();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneComponent> Pan;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USimCameraStreamComponent> Stream;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString CameraId = TEXT("ptz-1");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnableROS = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString RosbridgeUrl = TEXT("ws://127.0.0.1:9090");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PanLimit = 170.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MinTilt = -80.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxTilt = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SlewDegreesPerSecond = 60.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Command = FVector(0, 0, 60);
    UFUNCTION(BlueprintCallable) bool SetPTZ(float PanDegrees, float TiltDegrees, float HorizontalFovDegrees);

    /**
     * Virtual engagement: designate SimTargetComponent actors and launch simulated interceptors.
     * Opt-in per tripod; acts only on Unreal actors and never on the external FC or hardware.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engagement") bool bAllowSimulatedEngagement = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engagement") float MaxEngagementRangeCm = 400000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engagement", meta=(ClampMin="1", ClampMax="4")) int32 MaxInterceptorsInFlight = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engagement") float LaunchCooldownSeconds = 2.f;
    /** Slew and zoom to keep the designated target framed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engagement") bool bTrackTarget = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engagement") bool bAutoZoom = true;
    /** Tripod keeps the operator view; Missile shows the seeker; Chase trails the interceptor. Sticky across launches. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engagement") ESimPTZView ViewMode = ESimPTZView::Tripod;
    /** Launching starts tracking the target so the tripod view frames the hit. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engagement") bool bAutoTrackOnLaunch = true;
    /** Seconds a view stays on the burst after the interceptor resolves. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Engagement") float ViewHoldSeconds = 3.f;
    UFUNCTION(BlueprintCallable, Category="Engagement") void SetViewMode(ESimPTZView Mode);
    UFUNCTION(BlueprintCallable, Category="Engagement") void CycleView();
    UFUNCTION(BlueprintPure, Category="Engagement") FString GetViewName() const;

    /**
     * ROS images: JPEG sensor_msgs/CompressedImage on <prefix>/image/compressed, showing what this
     * camera's stream shows (tripod, missile or chase view). Published only while a demand arrives
     * on <prefix>/image_demand (std_msgs/Float32 = requested Hz, 0 = off), e.g. from the Pi's
     * ptz_image_bridge node when the topic has subscribers. Demand expires after five seconds.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ROS|Images", meta=(ClampMin="0", ClampMax="30")) float MaxImageRate = 15.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ROS|Images", meta=(ClampMin="10", ClampMax="100")) int32 ImageJpegQuality = 75;
    /** ROS images are downscaled to this width (aspect kept); JPEG over rosbridge shares the Wi-Fi with the video. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ROS|Images", meta=(ClampMin="160", ClampMax="3840")) int32 ImageWidth = 640;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ROS|Images") float ImageRate = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="ROS|Images") int32 ImagesPublished = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Engagement") TWeakObjectPtr<AActor> DesignatedTarget;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Engagement") FString EngagementStatus;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Engagement") int32 Launches = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Engagement") int32 Hits = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Engagement") int32 Misses = 0;
    /** Separate stable feed (<CameraId>-seeker) following the newest interceptor. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Engagement") TObjectPtr<ASimFollowCamera> SeekerCamera;
    /** Picks the most central visible target; bNext cycles through visible targets by angle. */
    UFUNCTION(BlueprintCallable, Category="Engagement") bool DesignateTarget(bool bNext = false);
    UFUNCTION(BlueprintCallable, Category="Engagement") void ClearDesignation();
    UFUNCTION(BlueprintCallable, Category="Engagement") ASimProjectile* LaunchInterceptor();
    UFUNCTION(BlueprintCallable, Category="Engagement") int32 AbortInterceptors();
    /** Tracking remembers the operator's view and restores it when tracking stops. */
    UFUNCTION(BlueprintCallable, Category="Engagement") void SetTracking(bool bEnabled);
    UFUNCTION(BlueprintPure, Category="Engagement") int32 GetInterceptorsInFlight() const;
    /** Executes a text engagement command (designate, next, fire, abort, track_on, track_off, clear). */
    UFUNCTION(BlueprintCallable, Category="Engagement") bool ExecuteEngagementCommand(const FString& Order);
    /** Where the tripod's crosshair meets the ground or a structure, within MaxEngagementRangeCm. */
    UFUNCTION(BlueprintCallable, Category="Engagement") bool AimPoint(FVector& Location) const;
    /** Pan/tilt (deg) that points the camera at a world location, in this tripod's frame. */
    FVector2D AnglesTo(const FVector& WorldLocation) const;
    UFUNCTION(BlueprintCallable) bool SavePlacement();
    UFUNCTION(BlueprintCallable) bool LoadPlacement();
    /** ROS identifiers cannot contain hyphens; video stream IDs remain unchanged. */
    UFUNCTION(BlueprintPure) FString GetROSTopicPrefix() const;
    virtual void Tick(float DeltaTime) override;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void ConnectROS();
    void ReceiveROS(const FString& Message);
    void SendState();
    void UpdateTracking();
    void ApplyView();
    void UpdateImagePublishing(float DeltaTime);
    void SendImage(const FString& Base64Jpeg, double StampSeconds, FIntPoint Size);
    double LastImageDemand = -1e9;
    float ImageTimer = 0.f;
    bool bImageInFlight = false;
    TSharedPtr<class FRHIGPUTextureReadback, ESPMode::ThreadSafe> ImageReadback;
    FIntPoint ImageSize = FIntPoint::ZeroValue;
    double ImageStamp = 0;
    TWeakObjectPtr<ASimProjectile> ViewSubject;
    void OnInterceptorResolved(ASimProjectile* Projectile, ESimInterceptorResult Result);
    bool IsTargetVisible(const AActor* Target) const;
    TArray<TWeakObjectPtr<ASimProjectile>> Interceptors;
    float Cooldown = 0.f;
    float SeekerHold = 0.f;
    FVector PreTrackCommand = FVector::ZeroVector;
    bool bHasPreTrackCommand = false;
    TSharedPtr<IWebSocket> Socket;
    float NetworkTimer = 0;
    float RetryTimer = 0;
};
