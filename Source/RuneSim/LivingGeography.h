#pragma once
#include "CoreMinimal.h"
#include "LivingWorldTypes.h"
class ACesiumGeoreference;
class UWorld;
class AActor;

namespace LivingGeography
{
    FQuat Frame(const ACesiumGeoreference* Georeference, const FVector& Position);
    float AirClearance(const ULivingAssetProfile& Profile);
    // Query the rendered collision, including authored tileset offsets. Missing tiles fail closed.
    bool Surface(UWorld* World, const FVector& Position, const FVector& Up, FVector& Ground, const AActor* Ignore = nullptr);
}
