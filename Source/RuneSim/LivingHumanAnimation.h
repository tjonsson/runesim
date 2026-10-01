#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "LivingHumanAnimation.generated.h"

USTRUCT(BlueprintType)
struct RUNESIM_API FLivingFootSupport
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FName Bone;
    /** Ground height relative to the mesh's authored ground plane, in mesh cm. */
    UPROPERTY(BlueprintReadWrite) float HeightCm = 0;
    /** Evaluated mesh-space stance correction; recorded verbatim for visual replay. */
    UPROPERTY(BlueprintReadWrite) FVector StanceOffsetCm = FVector::ZeroVector;
    /** Surface normal under the foot in mesh space; the planted foot pitches/rolls to match. */
    UPROPERTY(BlueprintReadWrite) FVector GroundNormal = FVector::UpVector;
    bool bGrounded = false;
};

struct FLivingFootPlant
{
    FVector Anchor = FVector::ZeroVector;
    FQuat Rotation = FQuat::Identity;
    float Age = 0;
    bool bPlanted = false;
    bool bCanPlant = true;
};

/** Visual terrain adaptation layered over the existing locomotion Blend Space. */
UCLASS(Transient)
class RUNESIM_API ULivingHumanAnimation : public UAnimSingleNodeInstance
{
    GENERATED_BODY()
public:
    UPROPERTY(Transient, BlueprintReadWrite) TArray<FLivingFootSupport> FootSupports;
    /** Inert replay supplies recorded supports and never queries today's terrain. */
    UPROPERTY(Transient) bool bReplaySupports = false;
    UPROPERTY(Transient, BlueprintReadOnly) int32 SupportedFeet = 0;
    void ResetGrounding();
    void UpdateGrounding(float DeltaSeconds);
    TArray<FVector> BaseFeet;
    FTransform LastMeshTransform;
    bool bHasBasePose = false;
    uint32 GroundingRevision = 0;
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
};

namespace LivingWorld
{
    RUNESIM_API float FootContactWeight(float AnimatedHeight, float ReferenceHeight);
    RUNESIM_API FVector UpdateFootPlant(FLivingFootPlant& Plant, const FTransform& Mesh,
        const FVector& AnimatedFoot, float ContactWeight, bool bSupported, float Dt);
    RUNESIM_API bool SolveGroundedLeg(FTransform& Thigh, FTransform& Calf, FTransform& Foot, const FVector& Target);
    /** Weighted rotation taking mesh up onto the ground normal, limited to a plausible ankle range. */
    RUNESIM_API FQuat FootTilt(const FVector& GroundNormal, float Weight, float MaxDegrees = 25.f);
}
