#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CesiumCamera.h"
#include "LivingTerrainBudgetComponent.generated.h"

/** Tracks cumulative view changes, including slow motion across several frames. */
struct FLivingViewCadence
{
    TArray<FCesiumCamera> ReferenceViews;
    float StillSeconds = 0;
    bool Update(const TArray<FCesiumCamera>& Views, float DeltaTime);
    void Reset() { ReferenceViews.Reset(); StillSeconds = 0; }
};

/** Runtime-only selection cadence; leaves Cesium quality, render and collision settings intact. */
UCLASS(ClassGroup=(Simulation))
class RUNESIM_API ULivingTerrainBudgetComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    ULivingTerrainBudgetComponent();
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = true;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) bool bUsingIdleCadence = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int32 ObservedViews = 0;
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function) override;
private:
    bool CollectViews(TArray<FCesiumCamera>& Views) const;
    void ApplyInterval(float Interval);
    FLivingViewCadence Cadence;
    FTransform LastTilesetTransform;
    float OriginalInterval = 0;
    float AppliedInterval = 0;
    bool bOwnsInterval = false;
};
