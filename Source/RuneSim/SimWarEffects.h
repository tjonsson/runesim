#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SimWarEffects.generated.h"

class UNiagaraComponent;
class UNiagaraSystem;

/** A camera that can see battlefield effects: the local view, a tripod feed or an airborne sensor. */
struct FSimEffectViewer
{
    FVector Location = FVector::ZeroVector;
    FVector Forward = FVector::ForwardVector;
    float FovDegrees = 90.f;
};

namespace SimWarEffects
{
    /** Fraction of the view width a feature of SizeCm covers; zero when it is outside the (slightly widened) view cone. */
    RUNESIM_API float ApparentFraction(const FSimEffectViewer& Viewer, const FVector& Target, float SizeCm);
    /** Picks at most Max indices to simulate as fluids, largest apparent size first; hysteresis keeps current ones down to 60 % of Threshold. */
    RUNESIM_API TArray<int32> SelectFluid(const TArray<float>& Apparent, const TArray<bool>& Current, int32 Max, float Threshold);
}

/**
 * Battlefield effects: ground strikes, sustained fires, smoke columns and screens, impacts and scorch marks.
 * Fires always burn with sprite flames and smoke; the closest-looking few also run a NiagaraFluids gas
 * simulation, chosen every half second from what the local view and every sensor/tripod camera sees, using a
 * pool of fluid components created at world start (never created or destroyed during play).
 * Assets are optional: missing systems are skipped, so bookkeeping works in tests without content.
 */
UCLASS()
class RUNESIM_API USimWarEffects : public UTickableWorldSubsystem
{
    GENERATED_BODY()
public:
    /** Starts a fire; Seconds <= 0 burns until StopFire. Attach follows a moving or falling actor. Returns an id. */
    UFUNCTION(BlueprintCallable, Category="War effects") int32 StartFire(FVector Location, float Scale = 1.f, float Seconds = 45.f, AActor* Attach = nullptr);
    UFUNCTION(BlueprintCallable, Category="War effects") void StopFire(int32 Id);
    UFUNCTION(BlueprintCallable, Category="War effects") void StopAllFires();
    /** Artillery/bomb/crash blast on the ground: flash, grit, dust, smoke column and a scorch mark; Scale >= 1 leaves a fire. */
    UFUNCTION(BlueprintCallable, Category="War effects") void Strike(FVector Location, float Scale = 1.f, bool bLeaveFire = true);
    /** A white obscurant cloud that persists for Seconds. */
    UFUNCTION(BlueprintCallable, Category="War effects") void SmokeScreen(FVector Location, float Scale = 1.f, float Seconds = 45.f);
    /** Small arms/fragment impact: "metal", "rock", "sand" or "water". */
    UFUNCTION(BlueprintCallable, Category="War effects") void Impact(FVector Location, FName Surface, float Scale = 1.f);

    UFUNCTION(BlueprintPure, Category="War effects") int32 ActiveFireCount() const { return Fires.Num(); }
    UFUNCTION(BlueprintPure, Category="War effects") int32 ActiveFluidFireCount() const;
    UFUNCTION(BlueprintPure, Category="War effects") bool IsFireFluid(int32 Id) const;

    /** Scorch decals under strikes (off by default: barely visible on Cesium photogrammetry, cost unmeasured there). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="War effects") bool bScorchDecals = false;

    /** Cameras that currently matter for level of detail (local view, tripod and sensor captures). */
    TArray<FSimEffectViewer> GatherViewers() const;
    /** Viewers override for tests and tools; empty means gather from the world. */
    TArray<FSimEffectViewer> ViewerOverride;

    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(USimWarEffects, STATGROUP_Tickables); }
    virtual bool DoesSupportWorldType(const EWorldType::Type Type) const override;
    /** Loads and holds every effect asset up front: a synchronous load during play flushes the async
     *  loading queue (Cesium-heavy scenes hitch for frames), and unreferenced assets would be collected. */
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
    struct FFire
    {
        FVector Location = FVector::ZeroVector;
        TWeakObjectPtr<AActor> Attach;
        FVector AttachOffset = FVector::ZeroVector;
        float Scale = 1.f;
        float Remaining = 0.f;
        bool bForever = false;
        float SmokeTimer = 0.f;
        float FlameTimer = 0.f;
        bool bFluid = false;
        TWeakObjectPtr<UNiagaraComponent> Flames;
        TWeakObjectPtr<UNiagaraComponent> Fluid;
        FVector Current() const;
    };
    TMap<int32, FFire> Fires;
    int32 NextId = 1;
    float SelectTimer = 0.f;
    void SetFluid(FFire& Fire, bool bOn);
    UNiagaraComponent* Spawn(const TCHAR* Path, const FVector& Location, float Scale, bool bAutoDestroy = true);
    void Scorch(const FVector& Location, float Scale);
    /** Cached asset lookup; a missing asset is remembered as null so it is never retried during play. */
    UObject* Asset(const TCHAR* Path);
    UPROPERTY(Transient) TMap<FString, TObjectPtr<UObject>> Assets;
    /** Effects spawned once out of sight at start so their GPU pipelines compile during load, not at the first strike. */
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> Prewarm;
    /** Fluid fires are never created or destroyed during play: tearing a NiagaraFluids component down stalled the
     *  stream (~5 fps for 10+ s). A small pool lives from the start, parked far below the scene, and is moved onto
     *  whichever fires need fluid detail. */
    UPROPERTY(Transient) TArray<TObjectPtr<UNiagaraComponent>> FluidPool;
    UNiagaraComponent* BorrowFluid(const FFire& Fire);
    void ReturnFluid(UNiagaraComponent* Fluid) const;
    float PrewarmRemaining = 0.f;
};
