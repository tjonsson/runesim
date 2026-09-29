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

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSimTargetDestroyed);

/** Explicitly opt an Unreal actor into virtual scenario damage. No external actuator interface. */
UCLASS(ClassGroup=(Simulation), meta=(BlueprintSpawnableComponent))
class RUNESIM_API USimTargetComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    USimTargetComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Health = 100.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bDestroyed = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<UNiagaraSystem> ImpactEffect;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TObjectPtr<USoundBase> ImpactSound;
    UPROPERTY(BlueprintAssignable) FSimTargetDestroyed OnDestroyed;
    UFUNCTION(BlueprintCallable) void ApplyVirtualDamage(float Amount);
};

/** Game-style projectile with a camera. Homing accepts only actors with SimTargetComponent. */
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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Damage = 100.f;
    UFUNCTION(BlueprintCallable) bool SetSimulatedTarget(AActor* Target);
    virtual void BeginPlay() override;
private:
    UFUNCTION() void OnImpact(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);
};

/** Bounded transform and health recording for explicitly selected simulated actors. */
UCLASS()
class RUNESIM_API ASimScenarioRecorder : public AActor
{
    GENERATED_BODY()
public:
    ASimScenarioRecorder();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TObjectPtr<AActor>> Subjects;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SampleInterval = .1f;
    UFUNCTION(BlueprintCallable) void StartRecording();
    UFUNCTION(BlueprintCallable) bool StopAndSave(const FString& Name);
    virtual void Tick(float DeltaTime) override;
private:
    bool bRecording = false;
    float Timer = 0.f;
    FString Buffer;
};
