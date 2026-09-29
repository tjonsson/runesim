#include "LivingFootstepNotify.h"
#include "LivingAgent.h"
#include "Components/SkeletalMeshComponent.h"

void ULivingFootstepNotify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
    const FAnimNotifyEventReference& EventReference)
{
    if (MeshComp)
        if (ALivingAgent* Agent = Cast<ALivingAgent>(MeshComp->GetOwner())) Agent->PlayFootstep();
}
