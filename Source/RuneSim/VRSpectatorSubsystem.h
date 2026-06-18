#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "VRSpectatorSubsystem.generated.h"

class AVRSpectator;

UCLASS()
class RUNESIM_API UVRSpectatorSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:
    UPROPERTY()
    AVRSpectator* SpectatorInstance = nullptr;
};
