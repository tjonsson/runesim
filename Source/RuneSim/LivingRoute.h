#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LivingWorldTypes.h"
#include "LivingRoute.generated.h"
class USplineComponent;
class UCesiumGlobeAnchorComponent;
class ALivingRoute;

/** A place where an agent leaving one end of a route can continue on another reviewed route. */
struct FLivingRouteLink
{
    TWeakObjectPtr<ALivingRoute> Route;
    float Distance = 0.f;
    int32 Direction = 1;
};

/** Authored road/footpath corridor. Unreviewed GIS imports cannot spawn agents. */
UCLASS()
class RUNESIM_API ALivingRoute : public AActor
{
    GENERATED_BODY()
public:
    ALivingRoute();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TObjectPtr<USplineComponent> Path;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bVehicles = false;
    /** Open one-way routes retire agents at the exit instead of reversing into traffic. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOneWay = false;
    /** Open corridor exits: recycle agents before their footprint leaves reviewed geometry. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRetireAtEnds = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bValidated = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Provenance;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ELivingSurface Surface = ELivingSurface::Paved;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxGroundDeviationCm = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxSlopeDegrees = 30.f;
    /** Explicitly reviewed clearance on either side; zero permits only the centreline. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float ReviewedHalfWidthCm = 0.f;
    /**
     * Vehicles may roll one wheel over an isolated unsupported photogrammetry facet, hanging it at
     * neutral suspension while the other wheels carry the chassis. Water/no-walk tags still fail.
     * Set only after a full-length placement scan; zero keeps every wheel strictly supported.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="1")) int32 MaxBridgedWheels = 0;
    /** Filled by the population from reviewed routes (LivingWorld::LinkRoutes); not saved. */
    TArray<FLivingRouteLink> StartLinks, EndLinks;
    UFUNCTION(BlueprintCallable, Category="Living World")
    bool SampleGround(float Distance, FVector& Location, FVector& Up, float LateralOffset = 0.f, float FootprintRadius = 0.f) const;
};
