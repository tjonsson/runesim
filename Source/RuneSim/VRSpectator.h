#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "MotionControllerComponent.h"
#include "VRSpectator.generated.h"

UCLASS(BlueprintType, Blueprintable)
class RUNESIM_API AVRSpectator : public AActor
{
    GENERATED_BODY()

public:
    AVRSpectator();

    UFUNCTION(BlueprintCallable, Category = "VR")
    void ToggleVR();

    UFUNCTION(BlueprintCallable, Category = "VR")
    bool IsVRActive() const { return bVRActive; }

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Movement")
    float MoveSpeed = 800.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Movement")
    float FastMoveMultiplier = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VR|Input")
    float TriggerDeadzone = 0.15f;

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    void EnableVR();
    void DisableVR();
    void SetupInputBindings();

    void OnNextCamera();
    void OnPrevCamera();
    void OnReturnToOrigin();
    void OnToggleHelp();
    void ShowCameraToast(class AAirSimCameraDirector* Dir);

    UPROPERTY(VisibleAnywhere)
    USceneComponent* VRRoot;

    UPROPERTY(VisibleAnywhere)
    UCameraComponent* VRCamera;

    UPROPERTY(VisibleAnywhere)
    UMotionControllerComponent* LeftController;

    UPROPERTY(VisibleAnywhere)
    UMotionControllerComponent* RightController;

    UPROPERTY(VisibleAnywhere)
    UWidgetComponent* VRHUDWidget;

    UPROPERTY()
    AActor* PreviousViewTarget = nullptr;

    FVector OriginLocation;
    FRotator OriginRotation;

    bool bVRActive = false;
    bool bFastMove = false;
    int32 CameraModeIndex = 0;

    bool IsDroneAttachedCamera() const;
    bool IsManualCamera() const;
};
