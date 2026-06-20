#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "RuneSimModeBootSubsystem.generated.h"

/**
 * Selects the runtime scene in packaged/standalone builds.
 *
 * Boots into MainLevel (Cesium) by default. If "-scene=<Name>" is on the command line,
 * travels to /Game/<Name> via OpenLevel.
 *
 *   run.bat                    -> MainLevel (Cesium)
 *   run.bat SecondaryLevel     -> travels to /Game/SecondaryLevel
 */
UCLASS()
class RUNESIM_API URuneSimModeBootSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;

protected:
    virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
