#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LivingWorldTypes.h"
#include "LivingWorldSubsystem.generated.h"
class ALivingAgent;
class ALivingRoute;
class ACesiumCameraManager;
class ACesiumGeoreference;
class SWidget;
class SBorder;
class ASimScenarioRecorder;
class ASimReplay;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FLivingSettingsChanged, FLivingWorldOptions, Options);

UCLASS()
class RUNESIM_API ULivingWorldSubsystem : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override;
    UFUNCTION(BlueprintCallable, Category="Living World") void ApplyOptions(FLivingWorldOptions Value);
    UFUNCTION(BlueprintPure, Category="Living World") FLivingWorldOptions GetOptions() const { return Options; }
    UFUNCTION(BlueprintCallable, Category="Living World") void ToggleMenu();
    UFUNCTION(BlueprintCallable, Category="Living World") void RefreshPopulation();
    UFUNCTION(BlueprintCallable, Category="Living World") void ToggleRecording();
    UFUNCTION(BlueprintCallable, Category="Living World") bool PlayLastRecording();
    UFUNCTION(BlueprintCallable, Category="Living World") void ClearReplay();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString RecordingStatus;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString LastRecording;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ASimReplay> Replay;
    UPROPERTY(BlueprintAssignable) FLivingSettingsChanged OnSettingsChanged;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ActiveCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Status;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<ULivingAssetProfile>> Profiles;
private:
    void Reconcile();
    void InstallMenu();
    void CloseMenu();
    void UpdateStreamingCamera();
    FLivingWorldOptions Options;
    UPROPERTY() TArray<TObjectPtr<ALivingAgent>> Pool;
    UPROPERTY() TArray<TObjectPtr<ALivingRoute>> Routes;
    UPROPERTY() TObjectPtr<ASimScenarioRecorder> Recorder;
    TSharedPtr<SWidget> MenuRoot;
    TSharedPtr<SBorder> MenuPanel;
    TWeakObjectPtr<ACesiumCameraManager> CameraManager;
    TWeakObjectPtr<APlayerController> MenuController;
    int32 StreamingCameraId = -1;
    TArray<int32> GroundStreamingCameraIds;
    FRandomStream Random;
    FVector Center = FVector::ZeroVector;
    FVector CenterEcef = FVector::ZeroVector;
    TWeakObjectPtr<ACesiumGeoreference> Georeference;
    bool bCenterAnchored = false;
    float Accumulator = 0.f;
    float MaintenanceTimer = 0.f;
    bool bMenuOpen = false;
    bool bPreviousCursor = false;
};
