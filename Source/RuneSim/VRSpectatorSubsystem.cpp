#include "VRSpectatorSubsystem.h"
#include "VRSpectator.h"
#include "Engine/World.h"

bool UVRSpectatorSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld();
}

void UVRSpectatorSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    SpectatorInstance = InWorld.SpawnActor<AVRSpectator>();
}
