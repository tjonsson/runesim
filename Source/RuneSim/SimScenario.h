#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "SimScenario.generated.h"
class USphereComponent;
class UProjectileMovementComponent;
class UCameraComponent;
class USceneCaptureComponent2D;
class USimCameraStreamComponent;
class UNiagaraSystem;
class USoundBase;
class UAudioComponent;
class USpringArmComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSimTargetDestroyed);
DECLARE_MULTICAST_DELEGATE_OneParam(FSimTargetDestroyedNative, class USimTargetComponent*);

/** A timestamped simulation event. Replay reproduces its visual effect; it never re-issues commands. */
USTRUCT(BlueprintType)
struct RUNESIM_API FSimEvent
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) double Time = 0;
    /** launch, detonation, impact, shot_down, crash, miss, designate, abort. */
    UPROPERTY(BlueprintReadOnly) FName Type;
    UPROPERTY(BlueprintReadOnly) FVector Location = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FString Subject;
    UPROPERTY(BlueprintReadOnly) FString Detail;
};

namespace SimEvents
{
    /** Forwards the event to every recording in the world and the persistent engagement log. */
    RUNESIM_API void Record(UWorld* World, FName Type, const FVector& Location, const FString& Subject, const FString& Detail = FString());
    /** Visual/audio reproduction shared by live play and replay. Unknown types are ignored. */
    /** Scale > 1 enlarges bursts for large targets (extra offset bursts beyond 2x). */
    RUNESIM_API void PlayEffect(UWorld* World, FName Type, const FVector& Location, float Scale = 1.f);
    RUNESIM_API bool IsReplayable(FName Type);
    /** Automation disables effect spawning and the engagement log; recordings still receive events. */
    RUNESIM_API void SetLiveOutput(bool bEnabled);
}

/** Explicitly opt an Unreal actor into virtual scenario damage. No external actuator interface. */
UCLASS(ClassGroup=(Simulation), meta=(BlueprintSpawnableComponent))
class RUNESIM_API USimTargetComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USimTargetComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Health = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxHealth = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDestroyed = false;
    /** Designation ignores inactive targets, e.g. pooled agents that are not in the world. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEngageable = true;
    /** Owners with their own destruction behavior (falling aircraft) keep their actor visible. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHideOnDestroyed = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Category = TEXT("target");
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UNiagaraSystem> ImpactEffect;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USoundBase> ImpactSound;
    UPROPERTY(BlueprintAssignable) FSimTargetDestroyed OnDestroyed;
    /** Native listeners run even before actors are initialized for play (e.g. automation worlds). */
    FSimTargetDestroyedNative OnDestroyedNative;
    UFUNCTION(BlueprintCallable) void ApplyVirtualDamage(float Amount);
    UFUNCTION(BlueprintCallable) void ResetTarget(float InHealth);
    UFUNCTION(BlueprintPure) bool CanBeEngaged() const;
    /** Best available velocity for guidance: owner-reported or finite-differenced. */
    FVector GetTargetVelocity() const;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
private:
    FVector LastLocation = FVector::ZeroVector;
    FVector EstimatedVelocity = FVector::ZeroVector;
    bool bHasLastLocation = false;
};

UENUM(BlueprintType)
enum class ESimInterceptorResult : uint8 { InFlight, Hit, Missed, Aborted };

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSimInterceptorResolved, class ASimProjectile*, Projectile, ESimInterceptorResult, Result);
DECLARE_MULTICAST_DELEGATE_TwoParams(FSimInterceptorResolvedNative, class ASimProjectile*, ESimInterceptorResult);

/**
 * Game-style guided interceptor with a seeker camera. Guidance is proportional navigation
 * with a boost profile and lateral-acceleration limit; a proximity fuse avoids tunneling past
 * fast targets. Homing accepts only actors with SimTargetComponent.
 */
UCLASS()
class RUNESIM_API ASimProjectile : public AActor
{
    GENERATED_BODY()
public:
    ASimProjectile();
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UProjectileMovementComponent> Movement;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UCameraComponent> Camera;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneCaptureComponent2D> Capture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USimCameraStreamComponent> Stream;
    /** Trailing third-person view: the missile, its smoke trail and the target ahead of it. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USpringArmComponent> ChaseArm;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USceneCaptureComponent2D> ChaseCapture;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<UAudioComponent> MotorAudio;
    /** Seconds the resolved interceptor (and its cameras) stay after the burst. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LingerSeconds = 4.f;
    /** Warhead damage at the burst point; a burst at the fuse radius deals 60% of it. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 150.f;
    /** Launch speed, burnout speed (cm/s) and motor burn time (s). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float LaunchSpeed = 4000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BurnoutSpeed = 28000.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float BurnTime = 3.5f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float NavigationConstant = 4.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxLateralG = 35.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ProximityFuseCm = 700.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxFlightTime = 22.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float FlightTime = 0.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) float ClosestApproachCm = BIG_NUMBER;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) ESimInterceptorResult Result = ESimInterceptorResult::InFlight;
    UPROPERTY(BlueprintAssignable) FSimInterceptorResolved OnResolved;
    FSimInterceptorResolvedNative OnResolvedNative;
    UFUNCTION(BlueprintCallable) bool SetSimulatedTarget(AActor* Target);
    UFUNCTION(BlueprintPure) AActor* GetSimulatedTarget() const { return Target.Get(); }
    UFUNCTION(BlueprintCallable) void Abort();
    /** One guidance step; exposed so automation can integrate without a render loop. */
    void Guide(float Dt);
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void OnImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
    void Detonate(ESimInterceptorResult Outcome, AActor* HitActor);
    TWeakObjectPtr<AActor> Target;
    float TrailTimer = 0.f;
};

namespace SimGuidance
{
    /** Proportional-navigation acceleration (cm/s^2), perpendicular to missile velocity, limited in magnitude. */
    RUNESIM_API FVector Acceleration(const FVector& MissilePosition, const FVector& MissileVelocity,
        const FVector& TargetPosition, const FVector& TargetVelocity, float N, float MaxAcceleration);
}

/** Bounded transform, health and event recording for explicitly selected simulated actors. */
UCLASS()
class RUNESIM_API ASimScenarioRecorder : public AActor
{
    GENERATED_BODY()
public:
    ASimScenarioRecorder();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<AActor>> Subjects;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SampleInterval = .1f;
    /** Also samples interceptors launched after recording started. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTrackProjectiles = true;
    UFUNCTION(BlueprintCallable) void StartRecording();
    UFUNCTION(BlueprintCallable) bool StopAndSave(const FString& Name);
    UFUNCTION(BlueprintPure) bool IsRecording() const { return bRecording; }
    void AddEvent(const FSimEvent& Event);
    virtual void Tick(float DeltaTime) override;
private:
    void WriteFrame(bool bFinal);
    bool bRecording = false;
    float Timer = 0.f;
    FString Buffer;
    TArray<FSimEvent> PendingEvents;
};
