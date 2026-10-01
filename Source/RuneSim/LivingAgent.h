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
class USimTargetComponent;
class ALivingAgent;

/** Where two reviewed corridors cross: a pedestrian crossing or a vehicle junction. */
struct RUNESIM_API FLivingConflictZone
{
    FVector Location = FVector::ZeroVector;
    float RadiusCm = 300.f;
    /** Route and distance along it at which that route passes the zone centre. */
    TArray<TPair<TWeakObjectPtr<ALivingRoute>, float>> Entries;
    /** Vehicle currently holding the junction (first come, first served). */
    TWeakObjectPtr<ALivingAgent> Holder;
    bool bPedestrianCrossing = false;
};

namespace LivingWorld
{
    /** Crossings between a vehicle corridor and any other corridor. Parallel corridors do not conflict. */
    RUNESIM_API TArray<FLivingConflictZone> FindConflictZones(const TArray<ALivingRoute*>& Routes);
    /** Links open route ends that meet another reviewed route of the same mode (within a small gap). */
    /** Audio listener used for engine Doppler; set by the population each frame. */
    RUNESIM_API void SetListener(const FVector& Location, bool bValid);
    RUNESIM_API int32 LinkRoutes(const TArray<ALivingRoute*>& Routes, float MaxGapCm = 200.f);
}

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
    bool SampleFootGround(const FVector& Probe, FVector& Contact, FVector* Normal = nullptr) const;
    UFUNCTION(BlueprintCallable) bool RequestPerch(ALivingPerch* Site);
    UFUNCTION(BlueprintCallable) void TakeOff();
    void Step(float Dt, const FLivingWorldOptions& Options, const TArray<ALivingAgent*>& Neighbors, const FVector* Threat);
    /** Opt this agent into virtual engagement (aircraft, drones and vehicles only). */
    void EnableCombatTarget(bool bEnable);
    /** Velocity of the threat passed to Step; used to tell an approaching threat from a passing one. */
    FVector ThreatVelocity = FVector::ZeroVector;
    /** Crossings and junctions owned by the population subsystem. */
    TArray<FLivingConflictZone>* ConflictZones = nullptr;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USimTargetComponent> TargetComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float DownedTime = 0.f;
    /** How long a disabled ground vehicle burns before it is recycled. */
    static constexpr float WreckBurnSeconds = 30.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float LateralOffsetCm = 0.f;
    float GetRouteDistance() const { return RouteDistance; }
    const ALivingRoute* GetRoute() const { return Route; }
    UFUNCTION(BlueprintPure, Category="Living World") ALivingRoute* GetCurrentRoute() const { return Route; }
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
    /** People stand along local gravity, not the terrain normal; null means world +Z. */
    TWeakObjectPtr<ACesiumGeoreference> GroundGeoreference;
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
    void OnShotDown();
    void StepDowned(float Dt);
    /** Speed cap for crossings/junctions ahead; also claims or releases junctions for vehicles. */
    float ConflictSpeedLimit(float DesiredSpeed, float Braking, float Dt, const TArray<ALivingAgent*>& Neighbors);
    FVector DownedUp = FVector::UpVector;
    float CallTimer = 0.f;
    float EnginePitch = 1.f;
    /** Seconds without progress; people turn back after a while instead of waiting forever. */
    float StuckTime = 0.f;
    bool bWasAlarmed = false;
    void UpdateCalls(float Dt, bool bAlarm, float VolumeDb);
    FRotator DownedSpin = FRotator::ZeroRotator;
    float DownedSmokeTimer = 0.f;
    /** Burning-wreck fire in USimWarEffects (0 = none). */
    int32 WreckFireId = 0;
    float PatrolRemainingCm = 0.f;
    float HaltTime = 0.f;
    bool bFacingThreat = false;
    FVector ThreatLocation = FVector::ZeroVector;
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
