#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LivingAnimationAuthoring.generated.h"
class ULivingAssetProfile;
class UBlendSpace;

/** Editor-only construction of the reviewed humanoid locomotion assets. */
UCLASS()
class RUNESIM_API ULivingAnimationAuthoring : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) static UBlendSpace* BuildLocomotion(ULivingAssetProfile* Profile, const FString& AssetName);
};
