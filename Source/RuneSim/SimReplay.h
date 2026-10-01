#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LivingVehicleAnimation.h"
#include "LivingHumanAnimation.h"
#include "SimReplay.generated.h"

class UMeshComponent;
struct FSimReplayPose
{
    double Time = 0;
    FTransform Transform;
    bool bHidden = false;
    FString Animation;
    float AnimationTime = 0;
    FVector BlendPosition = FVector::ZeroVector;
    TArray<FLivingWheelPose> WheelPoses;
    TArray<FLivingFootSupport> FootSupports;
};
struct FSimReplayEvent
{
    double Time = 0;
    FName Type;
    FVector Location = FVector::ZeroVector;
    FString Subject;
};
struct FSimReplayTrack
{
    FString Id, StaticMesh, SkeletalMesh;
    FTransform VisualTransform;
    TArray<FSimReplayPose> Poses;
};

/** Inert visual playback of saved simulation recordings. Never controls live subjects. */
UCLASS()
class RUNESIM_API ASimReplay : public AActor
{
    GENERATED_BODY()
public:
    ASimReplay();
    UPROPERTY(BlueprintReadOnly) float Duration = 0;
    UPROPERTY(BlueprintReadOnly) float PlaybackTime = 0;
    UPROPERTY(BlueprintReadOnly) bool bPlaying = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float PlaybackRate = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bLoop = false;
    UPROPERTY(BlueprintReadOnly) FString LastError;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<TObjectPtr<UMeshComponent>> Visuals;
    /** Recorded events and how many effects playback has reproduced. Seeking never replays effects. */
    UPROPERTY(BlueprintReadOnly) int32 EventCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 EffectsPlayed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bReplayEffects = true;
    UFUNCTION(BlueprintCallable) bool LoadRecording(const FString& Name);
    UFUNCTION(BlueprintCallable) void PlayReplay();
    UFUNCTION(BlueprintCallable) void PauseReplay();
    UFUNCTION(BlueprintCallable) bool Seek(float Seconds);
    UFUNCTION(BlueprintCallable) void ClearReplay();
    virtual void Tick(float DeltaTime) override;
private:
    TArray<FSimReplayTrack> Tracks;
    TArray<FSimReplayEvent> Events;
    void PlayEventsBetween(double From, double To);
    void UpdateVisuals();
};
