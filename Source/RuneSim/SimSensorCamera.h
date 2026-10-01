#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SimSensorCamera.generated.h"
class USceneCaptureComponent2D;
class USimCameraStreamComponent;

/**
 * A camera feed with a stable WebRTC stream ID that can be re-attached to whichever actor
 * currently carries the sensor (a drone gimbal, an interceptor seeker). Viewers keep their
 * connection when the carrier changes. Rendering only happens while someone is watching.
 */
UCLASS()
class RUNESIM_API ASimFollowCamera : public AActor
{
    GENERATED_BODY()
public:
    ASimFollowCamera();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USimCameraStreamComponent> Stream;
    /** Mount offset in the carrier's frame (cm). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector MountOffset = FVector(0, 0, -30);
    /** Stabilized gimbal: keeps the horizon level and a fixed depression instead of the carrier's roll/pitch. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGimbal = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="-90", ClampMax="30")) float GimbalPitch = -20.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<AActor> Carrier;
    /** Mirror an existing view when no carrier is attached (e.g. the PTZ that launched the interceptor). */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<USceneComponent> IdleView;
    UFUNCTION(BlueprintCallable) void SetCarrier(AActor* NewCarrier, bool bInGimbal, float Pitch);
    UFUNCTION(BlueprintCallable) void SetIdleView(USceneComponent* View);
    UFUNCTION(BlueprintCallable) bool Configure(const FString& StreamId, float FieldOfView);
    /** Updates the transform now; ticking calls this after carriers move. */
    void Follow();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
};
