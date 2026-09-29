#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LivingRoute.generated.h"
class USplineComponent;
class UCesiumGlobeAnchorComponent;

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
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxGroundDeviationCm = 150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxSlopeDegrees = 30.f;
    /** Explicitly reviewed clearance on either side; zero permits only the centreline. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float ReviewedHalfWidthCm = 0.f;
    UFUNCTION(BlueprintCallable, Category="Living World")
    bool SampleGround(float Distance, FVector& Location, FVector& Up, float LateralOffset = 0.f, float FootprintRadius = 0.f) const;
};
