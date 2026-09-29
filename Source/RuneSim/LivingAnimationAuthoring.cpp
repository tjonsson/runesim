#include "LivingAnimationAuthoring.h"
#include "LivingWorldTypes.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"
#include "UObject/Package.h"
#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#endif

UBlendSpace* ULivingAnimationAuthoring::BuildLocomotion(ULivingAssetProfile* Profile, const FString& AssetName)
{
#if WITH_EDITOR
    if (!Profile || Profile->Kind > ELivingKind::Soldier || (AssetName != TEXT("BS_Civilian") && AssetName != TEXT("BS_Soldier"))) return nullptr;
    UAnimSequence* Idle = Cast<UAnimSequence>(Profile->IdleAnimation.LoadSynchronous());
    UAnimSequence* Walk = Cast<UAnimSequence>(Profile->CruiseAnimation.LoadSynchronous());
    UAnimSequence* Run = Cast<UAnimSequence>(Profile->FleeAnimation.LoadSynchronous());
    if (!Idle || !Walk || !Run || Idle->GetSkeleton() != Walk->GetSkeleton() || Idle->GetSkeleton() != Run->GetSkeleton()) return nullptr;
    const FString Path = TEXT("/Game/LivingWorld/Animations/") + AssetName;
    UBlendSpace1D* Blend = LoadObject<UBlendSpace1D>(nullptr, *(Path + TEXT(".") + AssetName));
    if (!Blend)
    {
        Blend = NewObject<UBlendSpace1D>(CreatePackage(*Path), *AssetName, RF_Public | RF_Standalone | RF_Transactional);
        FAssetRegistryModule::AssetCreated(Blend);
    }
    Blend->Modify(); Blend->SetSkeleton(Idle->GetSkeleton());
    // GetBlendParameter exposes a const reference, but this asset is mutable and
    // this is an editor transaction followed by resampling and PostEditChange.
    FBlendParameter& Axis = const_cast<FBlendParameter&>(Blend->GetBlendParameter(0));
    Axis.DisplayName = TEXT("Speed / cruise speed"); Axis.Min = 0; Axis.Max = 2.5f; Axis.GridNum = 10;
    while (Blend->GetNumberOfBlendSamples()) Blend->DeleteSample(0);
    Blend->AddSample(Idle, FVector::ZeroVector);
    Blend->AddSample(Walk, FVector(1, 0, 0));
    Blend->AddSample(Run, FVector(2.5, 0, 0));
    Blend->TargetWeightInterpolationSpeedPerSec = 5.f;
    Blend->NotifyTriggerMode = ENotifyTriggerMode::HighestWeightedAnimation;
    Blend->ValidateSampleData(); Blend->ResampleData(); Blend->PostEditChange(); Blend->MarkPackageDirty();
    Profile->Modify(); Profile->LocomotionBlend = Blend; Profile->MarkPackageDirty();
    return Blend;
#else
    return nullptr;
#endif
}
