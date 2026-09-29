#include "LivingGeography.h"
#include "CesiumGeoreference.h"
#include "Engine/World.h"

FQuat LivingGeography::Frame(const ACesiumGeoreference* Georeference, const FVector& Position)
{
    if (!Georeference) return FQuat::Identity;
    FMatrix Matrix = Georeference->ComputeEastSouthUpToUnrealTransformation(Position);
    Matrix.RemoveScaling();
    return FQuat(Matrix).GetNormalized();
}

float LivingGeography::AirClearance(const ULivingAssetProfile& Profile)
{
    const float Base = Profile.Kind == ELivingKind::Plane ? 4000.f :
        Profile.Kind == ELivingKind::Helicopter ? 2000.f : Profile.Kind == ELivingKind::Drone ? 1000.f : 800.f;
    return FMath::Max(Base, Profile.CollisionRadiusCm * 2.f);
}

bool LivingGeography::Surface(UWorld* World, const FVector& Position, const FVector& Up, FVector& Ground, const AActor* Ignore)
{
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LivingAirTerrain), true, Ignore);
    if (!World || !World->LineTraceSingleByObjectType(Hit, Position + Up * 300000.f, Position - Up * 500000.f,
        FCollisionObjectQueryParams(ECC_WorldStatic), Params)) return false;
    Ground = Hit.ImpactPoint;
    return true;
}
