#include "VRSpectator.h"

#include "Engine/World.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/PlayerController.h"
#include "IHeadMountedDisplay.h"
#include "IXRTrackingSystem.h"
#include "Kismet/GameplayStatics.h"

AVRSpectator::AVRSpectator()
{
    PrimaryActorTick.bCanEverTick = false;

    VRRoot = CreateDefaultSubobject<USceneComponent>(TEXT("VRRoot"));
    RootComponent = VRRoot;

    VRCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("VRCamera"));
    VRCamera->SetupAttachment(VRRoot);
}

void AVRSpectator::BeginPlay()
{
    Super::BeginPlay();
    SetupInputBindings();
}

void AVRSpectator::SetupInputBindings()
{
    if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
    {
        UInputSettings* Settings = UInputSettings::GetInputSettings();
        if (Settings)
        {
            FInputActionKeyMapping Mapping(FName(TEXT("ToggleVR")), EKeys::V);
            TArray<FInputActionKeyMapping> Existing;
            Settings->GetActionMappingByName(FName(TEXT("ToggleVR")), Existing);
            if (Existing.Num() == 0)
            {
                Settings->AddActionMapping(Mapping, false);
                Settings->SaveKeyMappings();
                Settings->ForceRebuildKeymaps();
            }
        }

        EnableInput(PC);
        if (InputComponent)
        {
            FInputActionBinding& Binding = InputComponent->BindAction(
                TEXT("ToggleVR"), IE_Pressed, this, &AVRSpectator::ToggleVR);
            Binding.bConsumeInput = false;
        }
    }
}

void AVRSpectator::ToggleVR()
{
    if (bVRActive)
        DisableVR();
    else
        EnableVR();
}

void AVRSpectator::EnableVR()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
        return;

    PC->ConsoleCommand(TEXT("stereo on"));

    // Reset tracking so forward aligns with the current camera direction
    if (GEngine && GEngine->XRSystem.IsValid())
    {
        GEngine->XRSystem->ResetOrientationAndPosition();
    }

    bVRActive = true;
    UE_LOG(LogTemp, Log, TEXT("VRSpectator: VR enabled — press V to disable."));
}

void AVRSpectator::DisableVR()
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (!PC)
        return;

    PC->ConsoleCommand(TEXT("stereo off"));

    bVRActive = false;
    UE_LOG(LogTemp, Log, TEXT("VRSpectator: VR disabled."));
}

void AVRSpectator::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}
