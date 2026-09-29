#include "LivingRoute.h"
#include "Components/SplineComponent.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Engine/World.h"

ALivingRoute::ALivingRoute()
{
    Path = CreateDefaultSubobject<USplineComponent>(TEXT("ValidatedPath"));
    SetRootComponent(Path);
    Path->SetSplinePointType(0, ESplinePointType::Linear);
    Path->SetSplinePointType(1, ESplinePointType::Linear);
    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
}

bool ALivingRoute::SampleGround(float Distance, FVector& Location, FVector& Up, float LateralOffset, float FootprintRadius) const
{
    if (!bValidated || Path->GetSplineLength() < 100.f || !GetWorld()) return false;
    if (!FMath::IsFinite(LateralOffset) || !FMath::IsFinite(FootprintRadius) || FootprintRadius < 0 ||
        (ReviewedHalfWidthCm <= 0 && !FMath::IsNearlyZero(LateralOffset)) ||
        (ReviewedHalfWidthCm > 0 && FMath::Abs(LateralOffset) + FootprintRadius > ReviewedHalfWidthCm)) return false;
    const FVector Right = Path->GetRightVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
    const FVector Expected = Path->GetLocationAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World) + Right * LateralOffset;
    Up = Path->GetUpVectorAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World).GetSafeNormal();
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LivingGround), true, this);
    // A missing tile, roof above the corridor, water tag, or steep normal fails closed.
    if (!GetWorld()->LineTraceSingleByObjectType(Hit, Expected + Up * MaxGroundDeviationCm,
        Expected - Up * MaxGroundDeviationCm, FCollisionObjectQueryParams(ECC_WorldStatic), Params)) return false;
    if (!Hit.GetActor() || Hit.GetActor()->ActorHasTag(TEXT("LivingWorld.NoWalk")) ||
        Hit.GetActor()->ActorHasTag(TEXT("Water")) ||
        FVector::DotProduct(Hit.ImpactNormal, Up) < FMath::Cos(FMath::DegreesToRadians(MaxSlopeDegrees))) return false;
    Location = Hit.ImpactPoint;
    Up = Hit.ImpactNormal;
    // A centre hit alone cannot authorize an agent straddling water or a ledge.
    // Check both sides of the footprint without recursively checking a footprint.
    if (FootprintRadius > 0 && ReviewedHalfWidthCm > 0)
    {
        for (float Side : {-1.f, 1.f})
        {
            FVector Edge, Normal;
            if (!SampleGround(Distance, Edge, Normal, LateralOffset + Side * FootprintRadius, 0) ||
                FMath::Abs(FVector::DotProduct(Edge - Location, Up)) > 30.f) return false;
        }
    }
    return true;
}
