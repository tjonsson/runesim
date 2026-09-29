#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "IWebSocket.h"
#include "SimPTZ.generated.h"
class UCameraComponent;
class USceneCaptureComponent2D;
class USimCameraStreamComponent;
class UCesiumGlobeAnchorComponent;

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
    TSharedPtr<IWebSocket> Socket;
    float NetworkTimer = 0;
    float RetryTimer = 0;
};
