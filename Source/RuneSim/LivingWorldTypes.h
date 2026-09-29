#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LivingWorldTypes.generated.h"

class UStaticMesh;
class USkeletalMesh;
class UAnimationAsset;
class USoundBase;
class UBlendSpace;

USTRUCT(BlueprintType)
struct RUNESIM_API FLivingWheelDefinition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Bone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="1")) float RadiusCm = 50.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSteers = false;
};

UENUM(BlueprintType)
enum class ELivingKind : uint8 { Civilian, Soldier, Car, Plane, Helicopter, Drone, Bird };
UENUM(BlueprintType)
enum class ELivingPreset : uint8 { Quiet, Balanced, Busy, Custom };
UENUM(BlueprintType)
enum class ELivingPopulation : uint8 { Civilians, Military, Mixed };
UENUM(BlueprintType)
enum class ELivingBehavior : uint8 { Cruising, Startled, Fleeing, Recovering, Blocked };
UENUM(BlueprintType)
enum class ELivingFlightState : uint8 { Flapping, Gliding, Approaching, Landing, Perched, TakingOff };

USTRUCT(BlueprintType)
struct RUNESIM_API FLivingWorldOptions
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bEnabled = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ELivingPreset Preset = ELivingPreset::Balanced;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ELivingPopulation Population = ELivingPopulation::Mixed;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CrowdDensity = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 TrafficDensity = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Planes = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Helicopters = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Drones = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BirdFlocks = 2;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FlockSize = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bReactive = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float ActivityRadiusMeters = 500.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxActors = 120;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 4242;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AmbientVolumeDb = -12.f;

    void Sanitize();
    void ApplyPreset(ELivingPreset Value);
    int32 DesiredCount(ELivingKind Kind) const;
};

/** Saved in Saved/Config/<platform>/GameUserSettings.ini; independent of graphics quality. */
UCLASS(Config=GameUserSettings)
class RUNESIM_API ULivingWorldPreferences : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(Config) FLivingWorldOptions Options;
};

/** Only approved, dimensioned profiles enter the runtime population. Unreal distances are cm. */
UCLASS(BlueprintType)
class RUNESIM_API ULivingAssetProfile : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite) ELivingKind Kind = ELivingKind::Bird;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bApproved = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Source;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString License;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ScaleEvidence;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UStaticMesh> StaticMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USkeletalMesh> SkeletalMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimationAsset> CruiseAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimationAsset> FleeAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimationAsset> IdleAnimation;
    /** Optional idle/walk/run blend: X is actual speed divided by cruise speed (0..2.5). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UBlendSpace> LocomotionBlend;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimationAsset> GlideAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimationAsset> LandingAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<UAnimationAsset> TakeoffAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAllowPerching = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TSoftObjectPtr<USoundBase> LoopSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<TSoftObjectPtr<USoundBase>> FootstepSounds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector VisualScale = FVector::OneVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector VisualOffset = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator VisualRotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SpeedMetersPerSecond = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1")) float GroundAcceleration = 2.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1")) float GroundBraking = 4.f;
    /** Distance covered by one cruise cycle; zero leaves authored animation speed. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0")) float CruiseCycleMeters = 0.f;
    /** Opt-in visual articulation; empty keeps the ordinary animation path. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FLivingWheelDefinition> Wheels;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="60")) float MaxWheelSteeringDegrees = 35.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="50")) float SuspensionTravelCm = 20.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float CollisionRadiusCm = 40.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float GroundClearanceCm = 90.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float AltitudeMeters = 80.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float TurnRateDegrees = 45.f;
    /** Orbit fraction of the activity radius, with a minimum based on turn rate. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.1", ClampMax="1.0")) float FlightRadiusFraction = 0.65f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", ClampMax="60")) float MaxBankDegrees = 25.f;
};

namespace LivingWorld
{
    inline bool IsGround(ELivingKind Kind) { return Kind <= ELivingKind::Car; }
    /** Constant-speed route integration without overshoot, including reverse/open routes. */
    RUNESIM_API float AdvanceRoute(float Distance, float Travel, float Length, bool bClosed, int32& Direction);
    RUNESIM_API float FlightRadius(const ULivingAssetProfile& Profile, float ActivityRadiusMeters);
    /** Speed cap that retains a stopping distance plus one integration step of reaction time. Units cm/s. */
    RUNESIM_API float StoppingSpeed(float FreeDistance, float Braking, float ReactionTime);
}
