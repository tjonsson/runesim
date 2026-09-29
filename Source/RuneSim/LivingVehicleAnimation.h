#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "LivingVehicleAnimation.generated.h"

USTRUCT(BlueprintType)
struct RUNESIM_API FLivingWheelPose
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Bone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float SteeringDegrees = 0;
    /** Translation in mesh component space, before the actor transform. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Offset = FVector::ZeroVector;
};

/** Adds visual wheel steering/suspension to the existing distance-matched wheel clip. */
UCLASS(Transient)
class RUNESIM_API ULivingVehicleAnimation : public UAnimSingleNodeInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient, BlueprintReadWrite) TArray<FLivingWheelPose> WheelPoses;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};

namespace LivingWorld
{
    /** Signed curvature 1/cm; positive is a right turn in Unreal's actor axes. */
    RUNESIM_API float WheelSteering(float Curvature, float WheelbaseCm, float SideOffsetCm, float LimitDegrees);
}
