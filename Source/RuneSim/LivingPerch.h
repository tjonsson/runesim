#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LivingPerch.generated.h"
class ALivingAgent;
class UCesiumGlobeAnchorComponent;

/** An explicitly reviewed landing site. One bird may reserve it at a time. */
UCLASS()
class RUNESIM_API ALivingPerch : public AActor
{
    GENERATED_BODY()
public:
    ALivingPerch();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UCesiumGlobeAnchorComponent> GlobeAnchor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bValidated = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="5", ClampMax="100")) float ClearanceCm = 25.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin="10", ClampMax="500")) float ProbeDepthCm = 150.f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) TWeakObjectPtr<ALivingAgent> Occupant;
    bool SampleLanding(FVector& Position, FVector& Up, const ALivingAgent* Bird) const;
    bool Reserve(ALivingAgent* Bird);
    void Release(const ALivingAgent* Bird);
};
