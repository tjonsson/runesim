#include "LivingPerch.h"
#include "LivingAgent.h"
#include "CesiumGlobeAnchorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

ALivingPerch::ALivingPerch()
{
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PerchOrigin")));
    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
}

bool ALivingPerch::SampleLanding(FVector& Position, FVector& Up, const ALivingAgent* Bird) const
{
    if (!bValidated || !Bird || !Bird->Profile || !GetWorld()) return false;
    Up = GetActorUpVector();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(LivingPerch), false, Bird);
    Params.AddIgnoredActor(this);
    FHitResult Hit;
    if (!GetWorld()->LineTraceSingleByObjectType(Hit, GetActorLocation() + Up * 20.f,
        GetActorLocation() - Up * FMath::Clamp(ProbeDepthCm, 10.f, 500.f),
        FCollisionObjectQueryParams(ECC_WorldStatic), Params)) return false;
    const AActor* Surface = Hit.GetActor();
    if (!Surface || Surface->ActorHasTag(TEXT("Water")) || Surface->ActorHasTag(TEXT("NoWalk")) || Surface->ActorHasTag(TEXT("LivingWorld.NoWalk")) ||
        Surface->ActorHasTag(TEXT("NoPerch")) || FVector::DotProduct(Hit.ImpactNormal, Up) < 0.85f) return false;
    const float Radius = FMath::Max(5.f, Bird->Profile->CollisionRadiusCm);
    const float Seated = Bird->Profile->PerchHeightCm;
    // A resting bird's body sits on the surface; its bounding sphere would leave it hovering.
    const float Height = Seated > 0.f ? FMath::Clamp(Seated, 3.f, 100.f) : FMath::Max(FMath::Clamp(ClearanceCm, 5.f, 100.f), Radius + 2.f);
    Position = Hit.ImpactPoint + Up * Height;
    // Probe for other pawns above the surface; the ground itself is excluded by the smaller probe.
    const float Probe = Seated > 0.f ? FMath::Max(3.f, FMath::Min(Radius, Height - 2.f)) : Radius;
    return !GetWorld()->OverlapBlockingTestByChannel(Position + Up * (Seated > 0.f ? Radius - Probe : 0.f), FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeSphere(Probe), Params);
}

bool ALivingPerch::Reserve(ALivingAgent* Bird)
{
    if (!Bird || (Occupant.IsValid() && Occupant.Get() != Bird && Occupant->bActive)) return false;
    FVector Position, Up;
    if (!SampleLanding(Position, Up, Bird)) return false;
    Occupant = Bird;
    return true;
}

void ALivingPerch::Release(const ALivingAgent* Bird)
{
    if (Occupant.Get() == Bird) Occupant.Reset();
}
