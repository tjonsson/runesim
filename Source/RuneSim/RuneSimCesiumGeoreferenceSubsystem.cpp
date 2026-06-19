#include "RuneSimCesiumGeoreferenceSubsystem.h"

#include "CesiumGeoreference.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Json.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "OriginPlacement.h"

DEFINE_LOG_CATEGORY_STATIC(LogRuneSimCesium, Log, All);

namespace
{
struct FAirSimOriginGeopoint
{
    double Latitude = 0.0;
    double Longitude = 0.0;
    double Altitude = 0.0;
};

bool TryReadSettingsFile(const FString& SettingsPath, FString& OutSettingsText, FString& OutSource)
{
    if (!FPaths::FileExists(SettingsPath))
    {
        return false;
    }

    if (!FFileHelper::LoadFileToString(OutSettingsText, *SettingsPath))
    {
        UE_LOG(LogRuneSimCesium, Warning, TEXT("Found AirSim settings file but could not read it: %s"), *SettingsPath);
        return false;
    }

    OutSource = SettingsPath;
    return true;
}

TArray<FString> GetAirSimSettingsPathCandidates()
{
    TArray<FString> Candidates;

    Candidates.Add(FPaths::Combine(FPlatformProcess::BaseDir(), TEXT("settings.json")));
    Candidates.Add(FPaths::Combine(FPaths::ConvertRelativePathToFull(FPaths::LaunchDir()), TEXT("settings.json")));
    Candidates.Add(FPaths::Combine(FPlatformProcess::UserDir(), TEXT("AirSim"), TEXT("settings.json")));

    const FString UserProfile = FPlatformMisc::GetEnvironmentVariable(TEXT("USERPROFILE"));
    if (!UserProfile.IsEmpty())
    {
        Candidates.Add(FPaths::Combine(UserProfile, TEXT("Documents"), TEXT("AirSim"), TEXT("settings.json")));
    }

    const FString Home = FPlatformMisc::GetEnvironmentVariable(TEXT("HOME"));
    if (!Home.IsEmpty())
    {
        Candidates.Add(FPaths::Combine(Home, TEXT("Documents"), TEXT("AirSim"), TEXT("settings.json")));
    }

    return Candidates;
}

bool TryLoadAirSimSettingsText(FString& OutSettingsText, FString& OutSource)
{
    FString CommandLineSettings;
    if (FParse::Value(FCommandLine::Get(), TEXT("-settings="), CommandLineSettings, false))
    {
        if (TryReadSettingsFile(CommandLineSettings, OutSettingsText, OutSource))
        {
            return true;
        }

        OutSettingsText = CommandLineSettings;
        OutSource = TEXT("command line -settings JSON");
        return true;
    }

    for (const FString& SettingsPath : GetAirSimSettingsPathCandidates())
    {
        if (TryReadSettingsFile(SettingsPath, OutSettingsText, OutSource))
        {
            return true;
        }
    }

    return false;
}

bool TryReadAirSimOriginGeopoint(FAirSimOriginGeopoint& OutOrigin, FString& OutSource)
{
    FString SettingsText;
    if (!TryLoadAirSimSettingsText(SettingsText, OutSource))
    {
        UE_LOG(LogRuneSimCesium, Warning, TEXT("No AirSim settings.json found; leaving Cesium georeference unchanged."));
        return false;
    }

    TSharedPtr<FJsonObject> SettingsObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(SettingsText);
    if (!FJsonSerializer::Deserialize(Reader, SettingsObject) || !SettingsObject.IsValid())
    {
        UE_LOG(LogRuneSimCesium, Warning, TEXT("Could not parse AirSim settings from %s; leaving Cesium georeference unchanged."), *OutSource);
        return false;
    }

    const TSharedPtr<FJsonObject>* OriginObject = nullptr;
    if (!SettingsObject->TryGetObjectField(TEXT("OriginGeopoint"), OriginObject) || OriginObject == nullptr || !OriginObject->IsValid())
    {
        UE_LOG(LogRuneSimCesium, Warning, TEXT("AirSim settings from %s do not contain OriginGeopoint; leaving Cesium georeference unchanged."), *OutSource);
        return false;
    }

    if (!(*OriginObject)->TryGetNumberField(TEXT("Latitude"), OutOrigin.Latitude) ||
        !(*OriginObject)->TryGetNumberField(TEXT("Longitude"), OutOrigin.Longitude) ||
        !(*OriginObject)->TryGetNumberField(TEXT("Altitude"), OutOrigin.Altitude))
    {
        UE_LOG(LogRuneSimCesium, Warning, TEXT("OriginGeopoint in %s must contain numeric Latitude, Longitude, and Altitude fields."), *OutSource);
        return false;
    }

    if (!FMath::IsFinite(OutOrigin.Latitude) ||
        !FMath::IsFinite(OutOrigin.Longitude) ||
        !FMath::IsFinite(OutOrigin.Altitude) ||
        OutOrigin.Latitude < -90.0 ||
        OutOrigin.Latitude > 90.0 ||
        OutOrigin.Longitude < -180.0 ||
        OutOrigin.Longitude > 180.0)
    {
        UE_LOG(
            LogRuneSimCesium,
            Warning,
            TEXT("OriginGeopoint in %s is invalid: Latitude=%f Longitude=%f Altitude=%f"),
            *OutSource,
            OutOrigin.Latitude,
            OutOrigin.Longitude,
            OutOrigin.Altitude);
        return false;
    }

    return true;
}
}

void URuneSimCesiumGeoreferenceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    FAirSimOriginGeopoint Origin;
    FString SettingsSource;
    if (!TryReadAirSimOriginGeopoint(Origin, SettingsSource))
    {
        return;
    }

    ACesiumGeoreference* Georeference = ACesiumGeoreference::GetDefaultGeoreference(&InWorld);
    if (Georeference == nullptr)
    {
        UE_LOG(LogRuneSimCesium, Warning, TEXT("Could not find or create the default Cesium georeference."));
        return;
    }

    Georeference->SetOriginPlacement(EOriginPlacement::CartographicOrigin);
    Georeference->SetOriginLongitudeLatitudeHeight(FVector(Origin.Longitude, Origin.Latitude, Origin.Altitude));

    UE_LOG(
        LogRuneSimCesium,
        Log,
        TEXT("Updated Cesium georeference from AirSim settings (%s): Latitude=%f Longitude=%f Altitude=%f"),
        *SettingsSource,
        Origin.Latitude,
        Origin.Longitude,
        Origin.Altitude);
}

bool URuneSimCesiumGeoreferenceSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}
