#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
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

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    void EnableVR();
    void DisableVR();
    void SetupInputBindings();

    UPROPERTY(VisibleAnywhere)
    USceneComponent* VRRoot;

    UPROPERTY(VisibleAnywhere)
    UCameraComponent* VRCamera;

    UPROPERTY()
    AActor* PreviousViewTarget = nullptr;

    bool bVRActive = false;
};
