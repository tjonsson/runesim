#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LivingWorldTypes.h"
#include "LivingVehicleAnimation.h"
#include "LivingAgent.generated.h"
class USphereComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class UAudioComponent;
class UCesiumGlobeAnchorComponent;
class ALivingRoute;
class ALivingPerch;
class ACesiumGeoreference;

UCLASS()
class RUNESIM_API ALivingAgent : public AActor
{
    GENERATED_BODY()
public:
    ALivingAgent();
    void Activate(ULivingAssetProfile* Asset, ALivingRoute* Route, float Distance, const FVector& Home, int32 Seed, int32 InFlock, bool bTerrainAware = false);
    void Deactivate();
    /** Authoring probe using the same footprint, chassis and wheel checks as runtime. */
    UFUNCTION(BlueprintCallable, Category="Living World") bool PreviewRoutePlacement(ULivingAssetProfile* Asset, ALivingRoute* InRoute, float Distance);
    float GroundSpacingRadius() const { return Profile ? Profile->CollisionRadiusCm + WheelbaseCm*.5f : 0.f; }
    void PlayFootstep();
    bool SampleFootGround(const FVector& Probe, FVector& Contact) const;
    UFUNCTION(BlueprintCallable) bool RequestPerch(ALivingPerch* Site);
    UFUNCTION(BlueprintCallable) void TakeOff();
    void Step(float Dt, const FLivingWorldOptions& Options, const TArray<ALivingAgent*>& Neighbors, const FVector* Threat);
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UStaticMeshComponent> Visual;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USkeletalMeshComponent> AnimatedVisual;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent> Audio;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<ULivingAssetProfile> Profile;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) ELivingBehavior Behavior = ELivingBehavior::Cruising;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FString BlockedReason;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FVector Velocity = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float GroundSpeed = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float LocomotionSpeedRatio = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TArray<FLivingWheelPose> WheelPoses;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bActive = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 Flock = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 FootstepsPlayed = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) ELivingFlightState FlightState = ELivingFlightState::Flapping;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<ALivingPerch> Perch;
    virtual void ApplyWorldOffset(const FVector& Offset, bool bWorldShift) override;
private:
    UPROPERTY() TObjectPtr<ALivingRoute> Route;
    FVector Home = FVector::ZeroVector;
    FVector HomeEcef = FVector::ZeroVector;
    TWeakObjectPtr<ACesiumGeoreference> FlightGeoreference;
    bool bRequireAirTerrain = false;
    float CurrentBank = 0.f;
    float RouteDistance = 0.f;
    float LateralOffset = 0.f;
    float Time = 0.f;
    float CalmTime = 0.f;
    float Phase = 0.f;
    int32 Direction = 1;
    int32 LocomotionAnimation = -1;
    FRandomStream SoundRandom;
    FRandomStream BirdRandom;
    float FlightStateTime = 0.f;
    float FlightStateDuration = 0.f;
    float PerchSearchTime = 0.f;
    float ApproachTime = 0.f;
    FVector DepartureUp = FVector::UpVector;
    void SetFlightState(ELivingFlightState State);
    void UpdateBirdIntent(float Dt, bool bAlarm, float Speed, FVector& Desired);
    void ReleasePerch();
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void UpdateLocomotionAnimation(bool bMoving, bool bRunning);
    TArray<FVector> WheelReferenceLocations;
    float WheelbaseCm = 0;
    void InitializeWheels();
    bool SampleWheelContacts(const FTransform& ActorTransform, TArray<FLivingWheelPose>& Poses);
    bool FitVehicleGround(FVector& Position, FVector& Up, const FVector& Tangent) const;
    void UpdateWheels(float Dt, float Curvature, const TArray<FLivingWheelPose>& Contacts);
};
