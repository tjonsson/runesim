#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "LivingFootstepNotify.generated.h"

/** Gait contact sound routed through the ambient agent's spatial audio/volume. */
UCLASS(const, meta=(DisplayName="Living World Footstep"))
class RUNESIM_API ULivingFootstepNotify : public UAnimNotify
{
    GENERATED_BODY()
public:
    virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
        const FAnimNotifyEventReference& EventReference) override;
};
