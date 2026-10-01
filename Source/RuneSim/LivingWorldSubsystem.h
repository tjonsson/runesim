#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "LivingWorldTypes.h"
#include "LivingAgent.h"
#include "LivingWorldSubsystem.generated.h"
class ALivingAgent;
class ALivingRoute;
class ACesiumCameraManager;
class ACesiumGeoreference;
class SWidget;
class SBorder;
class ASimScenarioRecorder;
class ASimReplay;
class ASimFollowCamera;
class ALivingPerch;
class ASimPTZ;

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
    /** First tripod in the world that opts into simulated engagement, if any. */
    UFUNCTION(BlueprintCallable, Category="Living World") ASimPTZ* GetEngagementCamera();
    UFUNCTION(BlueprintPure, Category="Living World") int32 GetConflictZoneCount() const { return ConflictZones.Num(); }
    UFUNCTION(BlueprintPure, Category="Living World") int32 GetRuntimePerchCount() const { return RuntimePerches.Num(); }
    UFUNCTION(BlueprintPure, Category="Living World") TArray<ASimFollowCamera*> GetSensorCameras() const;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString RecordingStatus;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString LastRecording;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ASimReplay> Replay;
    UPROPERTY(BlueprintAssignable) FLivingSettingsChanged OnSettingsChanged;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ActiveCount = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString Status;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<ULivingAssetProfile>> Profiles;
private:
    void Reconcile();
    // Settings panel (LivingWorldMenu.cpp).
    void InstallMenu();
    void CloseMenu();
    TSharedRef<SWidget> BuildMenuContent();
    void ApplyMenuDraft();
    void DiscardMenuDraft();
    void UpdateStreamingCamera();
    void BuildConflictZones();
    void BuildRuntimePerches();
    void UpdateSensorCameras();
    FText EngagementText() const;
    TArray<FLivingConflictZone> ConflictZones;
    UPROPERTY() TArray<TObjectPtr<ASimFollowCamera>> SensorCameras;
    UPROPERTY() TArray<TObjectPtr<ALivingPerch>> RuntimePerches;
    TWeakObjectPtr<ASimPTZ> EngagementCamera;
    FLivingWorldOptions Options;
    UPROPERTY() TArray<TObjectPtr<ALivingAgent>> Pool;
    UPROPERTY() TArray<TObjectPtr<ALivingRoute>> Routes;
    UPROPERTY() TObjectPtr<ASimScenarioRecorder> Recorder;
    TSharedPtr<SWidget> MenuRoot;
    TSharedPtr<SBorder> MenuPanel;
    TSharedPtr<SWidget> MenuFocus;
    TSharedPtr<FLivingWorldOptions> MenuDraft;
    int32 MenuTab = 0;
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
