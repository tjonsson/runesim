#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "CameraQualitySubsystem.generated.h"

class APIPCamera;

UENUM(BlueprintType)
enum class ECameraQualityMode : uint8
{
    Cinematic,
    Sensor
};

UCLASS()
class RUNESIM_API UCameraQualitySubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;

    UFUNCTION(BlueprintCallable, Category = "Camera")
    void SetQualityMode(ECameraQualityMode NewMode);

    UFUNCTION(BlueprintCallable, Category = "Camera")
    ECameraQualityMode GetQualityMode() const { return CurrentMode; }

    void ToggleQualityMode();

private:
    void DiscoverCameras();
    void ApplyCinematicSettings(APIPCamera* Camera);
    void ApplySensorSettings(APIPCamera* Camera);
    void ApplyVRQualitySettings(ECameraQualityMode Mode);
    void ShowModeToast();

    ECameraQualityMode CurrentMode = ECameraQualityMode::Cinematic;

    UPROPERTY()
    TArray<TWeakObjectPtr<APIPCamera>> TrackedCameras;

    TMap<APIPCamera*, FPostProcessSettings> OriginalSettings;

    TWeakObjectPtr<AActor> LastViewTarget;
    bool bCamerasDiscovered = false;
    float SettleTimer = 0.0f;
};
