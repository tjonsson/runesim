#include "RuneSimModeBootSubsystem.h"

#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuneSimBoot, Log, All);

void URuneSimModeBootSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);

    const FString MapName = InWorld.GetMapName();
    if (!MapName.Contains(TEXT("MainLevel")))
    {
        return;
    }

    FString SceneName;
    if (FParse::Value(FCommandLine::Get(), TEXT("-scene="), SceneName))
    {
        FString MapPath = FString::Printf(TEXT("/Game/%s"), *SceneName);
        UE_LOG(LogRuneSimBoot, Log, TEXT("-scene=%s detected; traveling to %s."), *SceneName, *MapPath);
        UGameplayStatics::OpenLevel(&InWorld, FName(*MapPath));
    }
}

bool URuneSimModeBootSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    // Packaged/standalone game only. In PIE/Editor you open the map you want directly.
    return WorldType == EWorldType::Game;
}
