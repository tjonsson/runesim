#include "LivingWorldTypes.h"

void FLivingWorldOptions::Sanitize()
{
    CrowdDensity = FMath::Clamp(CrowdDensity, 0, 3);
    TrafficDensity = FMath::Clamp(TrafficDensity, 0, 3);
    Planes = FMath::Clamp(Planes, 0, 12);
    Helicopters = FMath::Clamp(Helicopters, 0, 12);
    Drones = FMath::Clamp(Drones, 0, 24);
    BirdFlocks = FMath::Clamp(BirdFlocks, 0, 8);
    FlockSize = FMath::Clamp(FlockSize, 1, 24);
    MaxActors = FMath::Clamp(MaxActors, 1, 300);
    ActivityRadiusMeters = FMath::IsFinite(ActivityRadiusMeters) ? FMath::Clamp(ActivityRadiusMeters, 100.f, 3000.f) : 500.f;
    AmbientVolumeDb = FMath::IsFinite(AmbientVolumeDb) ? FMath::Clamp(AmbientVolumeDb, -60.f, 0.f) : -12.f;
    if (static_cast<uint8>(Population) > 2) Population = ELivingPopulation::Mixed;
    if (static_cast<uint8>(Preset) > 3) Preset = ELivingPreset::Custom;
}

void FLivingWorldOptions::ApplyPreset(ELivingPreset Value)
{
    Preset = Value;
    if (Value == ELivingPreset::Custom) return;
    const int32 Level = static_cast<int32>(Value) + 1;
    CrowdDensity = Level;
    TrafficDensity = Level;
    Planes = Level;
    Helicopters = Level == 1 ? 1 : Level;
    Drones = Level * 3;
    BirdFlocks = Level;
    FlockSize = Level * 4;
    Sanitize();
}

float LivingWorld::FlightRadius(const ULivingAssetProfile& Profile, float ActivityRadiusMeters)
{
    const float Fraction = Profile.Kind == ELivingKind::Bird ? 0.25f : FMath::Clamp(Profile.FlightRadiusFraction, 0.1f, 1.f);
    const float TurnRadius = FMath::Max(0.f, Profile.SpeedMetersPerSecond) * 100.f /
        FMath::DegreesToRadians(FMath::Max(5.f, Profile.TurnRateDegrees));
    return FMath::Max3(3000.f, ActivityRadiusMeters * 100.f * Fraction, TurnRadius * 1.25f);
}

int32 FLivingWorldOptions::DesiredCount(ELivingKind Kind) const
{
    if (!bEnabled) return 0;
    const int32 Crowd[] = {0, 8, 24, 64};
    const int32 Cars[] = {0, 3, 8, 20};
    const int32 People = Crowd[FMath::Clamp(CrowdDensity, 0, 3)];
    switch (Kind)
    {
    case ELivingKind::Civilian: return Population == ELivingPopulation::Military ? 0 : Population == ELivingPopulation::Mixed ? People / 2 : People;
    case ELivingKind::Soldier: return Population == ELivingPopulation::Civilians ? 0 : Population == ELivingPopulation::Mixed ? People - People / 2 : People;
    case ELivingKind::Car: return Cars[FMath::Clamp(TrafficDensity, 0, 3)];
    case ELivingKind::Plane: return Planes;
    case ELivingKind::Helicopter: return Helicopters;
    case ELivingKind::Drone: return Drones;
    case ELivingKind::Bird: return BirdFlocks * FlockSize;
    default: return 0;
    }
}

float LivingWorld::StoppingSpeed(float FreeDistance, float Braking, float ReactionTime)
{
    if (!FMath::IsFinite(FreeDistance) || !FMath::IsFinite(Braking) || !FMath::IsFinite(ReactionTime) || FreeDistance <= 0 || Braking <= 0) return 0;
    const float Reaction = Braking * FMath::Max(0.f, ReactionTime);
    return FMath::Max(0.f, FMath::Sqrt(Reaction * Reaction + 2.f * Braking * FreeDistance) - Reaction);
}

float LivingWorld::AdvanceRoute(float Distance, float Travel, float Length, bool bClosed, int32& Direction)
{
    if (Length <= KINDA_SMALL_NUMBER || !FMath::IsFinite(Travel)) return 0.f;
    Direction = Direction < 0 ? -1 : 1;
    if (bClosed) return FMath::Fmod(FMath::Fmod(Distance + Travel * Direction, Length) + Length, Length);
    // Unfold the ping-pong path before integrating, including arbitrarily large steps.
    float Phase = Direction > 0 ? Distance : 2.f * Length - Distance;
    Phase = FMath::Fmod(FMath::Fmod(Phase + Travel, 2.f * Length) + 2.f * Length, 2.f * Length);
    Direction = Phase < Length ? 1 : -1;
    return Phase <= Length ? Phase : 2.f * Length - Phase;
}
