#include "LivingWorldSubsystem.h"
#include "SimScenario.h"
#include "SimReplay.h"
#include "LivingAgent.h"
#include "LivingRoute.h"
#include "LivingGeography.h"
#include "LivingTerrainBudgetComponent.h"
#include "CesiumGeoreference.h"
#include "Cesium3DTileset.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/SplineComponent.h"
#include "Engine/GameViewportClient.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "CesiumCameraManager.h"
#include "CesiumCamera.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"

bool ULivingWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    return World && World->IsGameWorld() && World->GetNetMode() != NM_DedicatedServer;
}

void ULivingWorldSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    Options = GetDefault<ULivingWorldPreferences>()->Options;
    Options.Sanitize();
    Random.Initialize(Options.Seed);
    for (TActorIterator<ACesium3DTileset> It(&InWorld); It; ++It)
    {
        Georeference = ACesiumGeoreference::GetDefaultGeoreference(&InWorld);
        if (!It->FindComponentByClass<ULivingTerrainBudgetComponent>())
        {
            auto* Budget = NewObject<ULivingTerrainBudgetComponent>(*It);
            It->AddInstanceComponent(Budget); Budget->RegisterComponent();
        }
    }
    RefreshPopulation();
}

void ULivingWorldSubsystem::RefreshPopulation()
{
    Profiles.Reset(); Routes.Reset();
    FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> Assets;
    Registry.Get().GetAssetsByPath(TEXT("/Game/LivingWorld"), Assets, true);
    Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });
    for (const FAssetData& Asset : Assets)
    {
        if (Asset.PackageName.ToString().Contains(TEXT("/Demo/")) && !GetWorld()->GetMapName().Contains(TEXT("LivingWorldDemo"))) continue;
        if (Asset.AssetClassPath != ULivingAssetProfile::StaticClass()->GetClassPathName()) continue;
        ULivingAssetProfile* Profile = Cast<ULivingAssetProfile>(Asset.GetAsset());
        if (Profile && Profile->bApproved && (!Profile->StaticMesh.IsNull() || !Profile->SkeletalMesh.IsNull())) Profiles.Add(Profile);
    }
    for (TActorIterator<ALivingRoute> It(GetWorld()); It; ++It) if (It->bValidated) Routes.Add(*It);
    Routes.Sort([](const ALivingRoute& A, const ALivingRoute& B) { return A.GetName() < B.GetName(); });
    for (ALivingAgent* Agent : Pool) if (IsValid(Agent)) Agent->Deactivate();
    Random.Initialize(Options.Seed);
    MaintenanceTimer = 1.f;
}

void ULivingWorldSubsystem::ApplyOptions(FLivingWorldOptions Value)
{
    Value.Sanitize();
    Options = Value;
    ULivingWorldPreferences* Saved = GetMutableDefault<ULivingWorldPreferences>();
    Saved->Options = Options;
    Saved->SaveConfig();
    RefreshPopulation();
    Reconcile();
    OnSettingsChanged.Broadcast(Options);
}

void ULivingWorldSubsystem::Reconcile()
{
    Pool.RemoveAll([](const ALivingAgent* Agent) { return !IsValid(Agent); });
    ActiveCount = 0;
    for (ALivingAgent* Agent : Pool) if (Agent->bActive) ++ActiveCount;
    if (!Options.bEnabled)
    {
        for (ALivingAgent* Agent : Pool) Agent->Deactivate();
        ActiveCount = 0; Status = TEXT("Living World is off"); return;
    }
    int32 Missing = 0;
    for (int32 K = 0; K <= static_cast<int32>(ELivingKind::Bird); ++K)
    {
        const ELivingKind Kind = static_cast<ELivingKind>(K);
        const int32 Desired = Options.DesiredCount(Kind);
        TArray<ULivingAssetProfile*> Choices;
        for (ULivingAssetProfile* Profile : Profiles) if (Profile->Kind == Kind) Choices.Add(Profile);
        int32 Existing = 0;
        for (ALivingAgent* Agent : Pool) if (Agent->bActive && Agent->Profile->Kind == Kind) ++Existing;
        if (Choices.IsEmpty()) { Missing += Desired; continue; }
        TArray<ALivingRoute*> ValidRoutes;
        if (LivingWorld::IsGround(Kind))
        {
            for (ALivingRoute* Route : Routes)
                if (IsValid(Route) && Route->bValidated && Route->bVehicles == (Kind == ELivingKind::Car) &&
                    FVector::DistSquared(Route->Path->FindLocationClosestToWorldLocation(Center, ESplineCoordinateSpace::World), Center) < FMath::Square(Options.ActivityRadiusMeters * 100.f)) ValidRoutes.Add(Route);
            if (ValidRoutes.IsEmpty()) { Missing += Desired; continue; }
        }
        // A bounded number of spawn attempts per maintenance cycle avoids hitches.
        const int32 Attempts = FMath::Min(Desired - Existing, 8);
        for (int32 I = 0; I < Attempts && ActiveCount < Options.MaxActors; ++I)
        {
            const int32 Flock = (Existing + I) / Options.FlockSize;
            // One species/profile per flock; adjacent flocks cycle through the available birds.
            ULivingAssetProfile* Profile = Choices[Kind == ELivingKind::Bird ? Flock % Choices.Num() : Random.RandRange(0, Choices.Num() - 1)];
            ALivingRoute* Route = ValidRoutes.IsEmpty() ? nullptr : ValidRoutes[Random.RandRange(0, ValidRoutes.Num() - 1)];
            FVector Location, Up;
            const float Distance = Route ? Random.FRandRange(0.f, Route->Path->GetSplineLength()) : 0.f;
            const float AltitudeOffset = LivingWorld::IsGround(Kind) ? 0.f : Kind == ELivingKind::Bird ? Flock * 250.f : Random.FRandRange(-500.f, 500.f);
            const FQuat Frame = LivingGeography::Frame(Georeference.Get(), Center);
            FVector Home = Center + Frame.GetAxisZ() * (Profile->AltitudeMeters * 100.f + AltitudeOffset);
            FRotator SpawnRotation = Frame.Rotator();
            if (Route)
            {
                if (!Route->SampleGround(Distance, Location, Up)) { ++Missing; continue; }
                Location += Up * Profile->GroundClearanceCm;
                const FVector Tangent = Route->Path->GetDirectionAtDistanceAlongSpline(Distance, ESplineCoordinateSpace::World);
                SpawnRotation = FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(Tangent, Up).GetSafeNormal(), Up).Rotator();
            }
            else
            {
                FRandomStream FlockRandom(Options.Seed + Flock * 7919);
                const float Angle = Kind == ELivingKind::Bird ? FlockRandom.FRandRange(0.f, 2.f * PI) : Random.FRandRange(0.f, 2.f * PI);
                const float Radius = LivingWorld::FlightRadius(*Profile, Options.ActivityRadiusMeters);
                Location = Home + Frame.RotateVector(FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius);
                if (Kind == ELivingKind::Bird) Location += Frame.RotateVector(FVector(Random.FRandRange(-200.f,200.f),Random.FRandRange(-200.f,200.f),Random.FRandRange(-100.f,100.f)));
                SpawnRotation = (Frame * FRotator(0, FMath::RadiansToDegrees(Angle) + (Flock % 2 ? -90.f : 90.f), 0).Quaternion()).Rotator();
                if (Georeference.IsValid())
                {
                    FVector Ground;
                    if (!LivingGeography::Surface(GetWorld(), Location, Frame.GetAxisZ(), Ground)) { ++Missing; continue; }
                    const float Rise = FMath::Max(0.f, LivingGeography::AirClearance(*Profile) + 500.f - float(FVector::DotProduct(Location - Ground, Frame.GetAxisZ())));
                    Location += Frame.GetAxisZ() * Rise; Home += Frame.GetAxisZ() * Rise;
                }
            }
            if (GetWorld()->OverlapBlockingTestByChannel(Location, FQuat::Identity, ECC_Pawn,
                FCollisionShape::MakeSphere(Profile->CollisionRadiusCm))) continue;
            ALivingAgent* Agent = nullptr;
            for (ALivingAgent* Candidate : Pool) if (!Candidate->bActive) { Agent = Candidate; break; }
            if (!Agent)
            {
                Agent = GetWorld()->SpawnActor<ALivingAgent>(Location, FRotator::ZeroRotator);
                if (Agent) Pool.Add(Agent);
            }
            if (Agent)
            {
                Agent->SetActorLocation(Location);
                Agent->SetActorRotation(SpawnRotation);
                Agent->Activate(Profile, Route, Distance, Home, Kind == ELivingKind::Bird ? Options.Seed + Flock * 7919 : Random.RandHelper(MAX_int32), Flock, Georeference.IsValid());
                if (LivingWorld::IsGround(Kind))
                {
                    bool bSpacing = true;
                    // Leave the same stopping gap used by following behavior.
                    // A new pedestrian must not start beside an existing queue
                    // where its first lane change would be blocked by that queue.
                    for (const ALivingAgent* Other : Pool)
                        if (Other != Agent && Other->bActive && LivingWorld::IsGround(Other->Profile->Kind))
                        {
                            const float Gap = Kind == ELivingKind::Car || Other->Profile->Kind == ELivingKind::Car ? 150.f : 30.f;
                            if (FVector::DistSquared(Location, Other->GetActorLocation()) < FMath::Square(Agent->GroundSpacingRadius() + Other->GroundSpacingRadius() + Gap))
                            { bSpacing = false; break; }
                        }
                    // A centre/sphere test cannot validate suspension or a whole
                    // chassis. Check the actual profile before accepting a spawn.
                    if (bSpacing) Agent->Step(0.f, Options, {}, nullptr);
                    if (!bSpacing || !Agent->bActive || Agent->Behavior == ELivingBehavior::Blocked)
                    { Agent->Deactivate(); ++Missing; continue; }
                    Agent->Velocity = FVector::ZeroVector;
                }
                ++ActiveCount;
            }
        }
    }
    Status = FString::Printf(TEXT("%d active · %d profiles · %d validated routes%s"), ActiveCount, Profiles.Num(), Routes.Num(),
        Missing ? TEXT(" · some activity awaits assets / safe routes") : TEXT(""));
}

void ULivingWorldSubsystem::UpdateStreamingCamera()
{
    if (!Options.bEnabled)
    {
        if (CameraManager.IsValid() && StreamingCameraId >= 0) CameraManager->RemoveCamera(StreamingCameraId);
        if (CameraManager.IsValid()) for (int32 Id : GroundStreamingCameraIds) CameraManager->RemoveCamera(Id);
        GroundStreamingCameraIds.Reset();
        StreamingCameraId = -1; return;
    }
    if (!CameraManager.IsValid()) CameraManager = ACesiumCameraManager::GetDefaultCameraManager(this);
    if (!CameraManager.IsValid()) return;
    // Keep a bounded overhead view of the active area in tile selection even when the player looks away.
    float Radius = Options.ActivityRadiusMeters * 100.f;
    for (const ULivingAssetProfile* Profile : Profiles)
        if (!LivingWorld::IsGround(Profile->Kind) && Options.DesiredCount(Profile->Kind) > 0)
            Radius = FMath::Max(Radius, LivingWorld::FlightRadius(*Profile, Options.ActivityRadiusMeters) * 1.5f);
    const FQuat Frame = LivingGeography::Frame(Georeference.Get(), Center);
    FCesiumCamera Camera(FVector2D(1024, 1024), Center + Frame.GetAxisZ() * Radius * 1.5f,
        (Frame * FRotator(-90, 0, 0).Quaternion()).Rotator(), 90.f);
    if (StreamingCameraId < 0) StreamingCameraId = CameraManager->AddCamera(Camera);
    else CameraManager->UpdateCamera(StreamingCameraId, Camera);
    // The air envelope's distant camera retains coverage, but not enough tile
    // detail for wheels. Keep a closer view over each active reviewed corridor.
    int32 GroundViews = 0;
    for (const ALivingRoute* Route : Routes)
    {
        if (GroundViews >= 8) break;
        if (!IsValid(Route) || !Route->bValidated || (Route->bVehicles ? Options.TrafficDensity == 0 : Options.CrowdDensity == 0) ||
            FVector::DistSquared(Route->Path->FindLocationClosestToWorldLocation(Center, ESplineCoordinateSpace::World), Center) > FMath::Square(Options.ActivityRadiusMeters*100.f)) continue;
        const FBoxSphereBounds Bounds = Route->Path->CalcBounds(Route->Path->GetComponentTransform());
        const FQuat RouteFrame = LivingGeography::Frame(Georeference.Get(), Bounds.Origin);
        const FCesiumCamera GroundCamera(FVector2D(1280,1280), Bounds.Origin + RouteFrame.GetAxisZ()*FMath::Max(3000., Bounds.SphereRadius*1.6),
            (RouteFrame*FRotator(-90,0,0).Quaternion()).Rotator(),90.f);
        if (GroundViews == GroundStreamingCameraIds.Num()) GroundStreamingCameraIds.Add(CameraManager->AddCamera(GroundCamera));
        else CameraManager->UpdateCamera(GroundStreamingCameraIds[GroundViews],GroundCamera);
        ++GroundViews;
    }
    while (GroundStreamingCameraIds.Num() > GroundViews) CameraManager->RemoveCamera(GroundStreamingCameraIds.Pop());
}

void ULivingWorldSubsystem::Tick(float DeltaTime)
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC)
    {
        if (!MenuRoot.IsValid()) InstallMenu();
        // AirSim's weather MenuActor already owns F10 in MainLevel.
        if (PC->WasInputKeyJustPressed(EKeys::F9)) ToggleMenu();
        if (bMenuOpen && PC->WasInputKeyJustPressed(EKeys::Escape)) CloseMenu();
    }
    MaintenanceTimer += DeltaTime;
    if (MaintenanceTimer >= 1.f)
    {
        MaintenanceTimer = 0.f;
        if (Georeference.IsValid() && bCenterAnchored) Center = Georeference->TransformEarthCenteredEarthFixedPositionToUnreal(CenterEcef);
        const FVector NewCenter = PC && PC->GetPawn() ? PC->GetPawn()->GetActorLocation() : Center;
        if (FVector::DistSquared(NewCenter, Center) > FMath::Square(Options.ActivityRadiusMeters * 100.f))
        {
            for (ALivingAgent* Agent : Pool) if (IsValid(Agent)) Agent->Deactivate();
        }
        Center = NewCenter;
        if (Georeference.IsValid()) { CenterEcef = Georeference->TransformUnrealPositionToEarthCenteredEarthFixed(Center); bCenterAnchored = true; }
        const FVector CenterUp = LivingGeography::Frame(Georeference.Get(), Center).GetAxisZ();
        for (ALivingAgent* Agent : Pool)
            if (IsValid(Agent) && Agent->bActive)
            {
                const float Radius = LivingWorld::IsGround(Agent->Profile->Kind) ? Options.ActivityRadiusMeters * 100.f :
                    FMath::Max(Options.ActivityRadiusMeters * 100.f, LivingWorld::FlightRadius(*Agent->Profile, Options.ActivityRadiusMeters) * 1.35f);
                if (FVector::VectorPlaneProject(Agent->GetActorLocation() - Center, CenterUp).SizeSquared() > FMath::Square(Radius)) Agent->Deactivate();
            }
        UpdateStreamingCamera();
        Reconcile();
    }
    if (!Options.bEnabled) return;
    Accumulator = FMath::Min(Accumulator + DeltaTime, 0.15f);
    TArray<ALivingAgent*> Active;
    for (ALivingAgent* Agent : Pool) if (IsValid(Agent) && Agent->bActive) Active.Add(Agent);
    TArray<FVector> Threats;
    if (Options.bReactive)
        for (TActorIterator<APawn> It(GetWorld()); It; ++It)
            if (It->GetVelocity().SizeSquared() > FMath::Square(30.f)) Threats.Add(It->GetActorLocation());
    while (Accumulator >= 1.f / 30.f)
    {
        Accumulator -= 1.f / 30.f;
        for (ALivingAgent* Agent : Active)
        {
            const FVector* Closest = nullptr;
            float Best = FMath::Square(2500.f);
            for (const FVector& Threat : Threats)
            {
                const float D = FVector::DistSquared(Agent->GetActorLocation(), Threat);
                if (D < Best)
                {
                    FHitResult SightHit;
                    FCollisionQueryParams SightParams(SCENE_QUERY_STAT(LivingPerception),false,Agent);
                    const bool Hit = GetWorld()->LineTraceSingleByChannel(SightHit, Agent->GetActorLocation(),Threat,ECC_Visibility,SightParams);
                    if (!Hit || FVector::DistSquared(SightHit.ImpactPoint,Threat) < FMath::Square(200.f)) { Best = D; Closest = &Threat; }
                }
            }
            Agent->Step(1.f / 30.f, Options, Active, Closest);
        }
    }
}

TStatId ULivingWorldSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(ULivingWorldSubsystem, STATGROUP_Tickables);
}

void ULivingWorldSubsystem::InstallMenu()
{
    if (!GetWorld()->GetGameViewport()) return;
    MenuRoot = SNew(SOverlay)
        + SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24.f, 60.f)
        [SNew(SButton).Text(FText::FromString(TEXT("Environment · Living World [F9]")))
            .OnClicked_Lambda([this]() { ToggleMenu(); return FReply::Handled(); })]
        + SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(24.f)
        [SAssignNew(MenuPanel, SBorder).Padding(24.f).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .ForegroundColor(FLinearColor::White).BorderBackgroundColor(FLinearColor(0.015f, 0.025f, 0.04f, 1.f))];
    MenuPanel->SetVisibility(EVisibility::Collapsed);
    GetWorld()->GetGameViewport()->AddViewportWidgetContent(MenuRoot.ToSharedRef(), 30);
}

void ULivingWorldSubsystem::ToggleMenu()
{
    if (bMenuOpen) { CloseMenu(); return; }
    if (!MenuPanel.IsValid()) InstallMenu();
    if (!MenuPanel.IsValid()) return;
    TSharedRef<FLivingWorldOptions> Draft = MakeShared<FLivingWorldOptions>(Options);
    TSharedRef<SVerticalBox> Rows = SNew(SVerticalBox);
    Rows->AddSlot().AutoHeight().Padding(0, 0, 0, 12)[SNew(STextBlock).Text(FText::FromString(TEXT("Settings / Environment / Living World"))).Font(FCoreStyle::GetDefaultFontStyle("Bold", 21))];
    Rows->AddSlot().AutoHeight().Padding(0, 4)[SNew(SCheckBox)
        .IsChecked_Lambda([Draft]() { return Draft->bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
        .OnCheckStateChanged_Lambda([Draft](ECheckBoxState S) { Draft->bEnabled = S == ECheckBoxState::Checked; })
        [SNew(STextBlock).Text(FText::FromString(TEXT("Enable Living World")))]];
    TSharedRef<SHorizontalBox> Presets = SNew(SHorizontalBox);
    const TCHAR* PresetNames[] = {TEXT("Quiet"), TEXT("Balanced"), TEXT("Busy")};
    for (int32 I = 0; I < 3; ++I)
        Presets->AddSlot().FillWidth(1).Padding(2)[SNew(SButton).Text(FText::FromString(PresetNames[I]))
            .OnClicked_Lambda([Draft, I]() { Draft->ApplyPreset(static_cast<ELivingPreset>(I)); return FReply::Handled(); })];
    Rows->AddSlot().AutoHeight().Padding(0, 8)[Presets];
    Rows->AddSlot().AutoHeight().Padding(0, 5)[SNew(SButton)
        .Text_Lambda([Draft]() { const TCHAR* Names[] = {TEXT("Civilians"), TEXT("Military"), TEXT("Mixed")}; return FText::FromString(FString(TEXT("Population: ")) + Names[static_cast<int32>(Draft->Population)]); })
        .OnClicked_Lambda([Draft]() { Draft->Population = static_cast<ELivingPopulation>((static_cast<int32>(Draft->Population) + 1) % 3); return FReply::Handled(); })];
    auto Integer = [Rows, Draft](const TCHAR* Label, int32 FLivingWorldOptions::*Field, int32 Min, int32 Max)
    {
        Rows->AddSlot().AutoHeight().Padding(0, 4)[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(Label))]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(130)[SNew(SSpinBox<int32>).MinValue(Min).MaxValue(Max)
                .Value_Lambda([Draft, Field]() { return Draft.Get().*Field; })
                .OnValueChanged_Lambda([Draft, Field](int32 Value) { Draft.Get().*Field = Value; Draft->Preset = ELivingPreset::Custom; })]]];
    };
    Integer(TEXT("Crowds (0 off · 1 sparse · 2 normal · 3 dense)"), &FLivingWorldOptions::CrowdDensity, 0, 3);
    Integer(TEXT("Ground traffic (0 off · 1 light · 2 normal · 3 heavy)"), &FLivingWorldOptions::TrafficDensity, 0, 3);
    Integer(TEXT("Planes"), &FLivingWorldOptions::Planes, 0, 12);
    Integer(TEXT("Helicopters"), &FLivingWorldOptions::Helicopters, 0, 12);
    Integer(TEXT("Drones"), &FLivingWorldOptions::Drones, 0, 24);
    Integer(TEXT("Bird flocks"), &FLivingWorldOptions::BirdFlocks, 0, 8);
    Integer(TEXT("Birds per flock"), &FLivingWorldOptions::FlockSize, 1, 24);
    Integer(TEXT("Maximum ambient actors"), &FLivingWorldOptions::MaxActors, 1, 300);
    Integer(TEXT("Scenario seed"), &FLivingWorldOptions::Seed, 0, MAX_int32);
    auto Decimal = [Rows, Draft](const TCHAR* Label, float FLivingWorldOptions::*Field, float Min, float Max)
    {
        Rows->AddSlot().AutoHeight().Padding(0, 4)[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(Label))]
            + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(130)[SNew(SSpinBox<float>).MinValue(Min).MaxValue(Max)
                .Value_Lambda([Draft, Field]() { return Draft.Get().*Field; })
                .OnValueChanged_Lambda([Draft, Field](float Value) { Draft.Get().*Field = Value; })]]];
    };
    Decimal(TEXT("Activity radius (m)"), &FLivingWorldOptions::ActivityRadiusMeters, 100.f, 3000.f);
    Decimal(TEXT("Ambient volume (dB)"), &FLivingWorldOptions::AmbientVolumeDb, -60.f, 0.f);
    Rows->AddSlot().AutoHeight().Padding(0, 8)[SNew(SCheckBox)
        .IsChecked_Lambda([Draft]() { return Draft->bReactive ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
        .OnCheckStateChanged_Lambda([Draft](ECheckBoxState S) { Draft->bReactive = S == ECheckBoxState::Checked; })
        [SNew(STextBlock).Text(FText::FromString(TEXT("Reactive avoidance and fleeing")))]];
    Rows->AddSlot().AutoHeight().Padding(0, 8)[SNew(STextBlock).AutoWrapText(true).Text(FText::FromString(Status))];
    Rows->AddSlot().AutoHeight().Padding(0, 8)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).Padding(2)[SNew(SButton)
            .Text_Lambda([this]() { return FText::FromString(IsValid(Recorder) ? TEXT("Stop and save recording") : TEXT("Record population")); })
            .OnClicked_Lambda([this]() { ToggleRecording(); return FReply::Handled(); })]
        + SHorizontalBox::Slot().FillWidth(1).Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("Overlay last recording")))
            .OnClicked_Lambda([this]() { PlayLastRecording(); return FReply::Handled(); })]
        + SHorizontalBox::Slot().AutoWidth().Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("Clear replay")))
            .OnClicked_Lambda([this]() { ClearReplay(); return FReply::Handled(); })]];
    Rows->AddSlot().AutoHeight()[SNew(STextBlock).AutoWrapText(true)
        .Text_Lambda([this]() { return FText::FromString(RecordingStatus); })];
    TSharedRef<SButton> Apply = SNew(SButton).Text(FText::FromString(TEXT("Apply and save")))
        .OnClicked_Lambda([this, Draft]() { ApplyOptions(Draft.Get()); CloseMenu(); return FReply::Handled(); });
    Rows->AddSlot().AutoHeight().Padding(0, 8)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).Padding(2)[Apply]
        + SHorizontalBox::Slot().FillWidth(1).Padding(2)[SNew(SButton).Text(FText::FromString(TEXT("Cancel")))
            .OnClicked_Lambda([this]() { CloseMenu(); return FReply::Handled(); })]];
    MenuPanel->SetContent(SNew(SBox).WidthOverride(650.f).MaxDesiredHeight(720.f)[SNew(SScrollBox) + SScrollBox::Slot()[Rows]]);
    MenuPanel->SetVisibility(EVisibility::Visible);
    bMenuOpen = true;
    MenuController = GetWorld()->GetFirstPlayerController();
    if (MenuController.IsValid())
    {
        bPreviousCursor = MenuController->bShowMouseCursor;
        MenuController->bShowMouseCursor = true;
        MenuController->SetIgnoreMoveInput(true); MenuController->SetIgnoreLookInput(true);
        FInputModeGameAndUI Mode; Mode.SetWidgetToFocus(Apply); Mode.SetHideCursorDuringCapture(false);
        MenuController->SetInputMode(Mode);
    }
    FSlateApplication::Get().SetKeyboardFocus(Apply);
}

void ULivingWorldSubsystem::ToggleRecording()
{
    if (IsValid(Recorder))
    {
        const FString Name = TEXT("Population_") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S"));
        if (!Recorder->StopAndSave(Name)) { RecordingStatus = TEXT("Recording could not be saved; try again."); return; }
        LastRecording = Name; RecordingStatus = TEXT("Saved ") + Name + TEXT(". Replay overlays inert copies; turn ambient activity off for an isolated view.");
        Recorder->Destroy(); Recorder = nullptr; return;
    }
    Recorder = GetWorld()->SpawnActor<ASimScenarioRecorder>();
    if (!Recorder) return;
    for (ALivingAgent* Agent : Pool) if (IsValid(Agent) && Agent->bActive) Recorder->Subjects.Add(Agent);
    if (Recorder->Subjects.IsEmpty()) { Recorder->Destroy(); Recorder = nullptr; RecordingStatus = TEXT("Enable a population before recording."); return; }
    Recorder->StartRecording(); RecordingStatus = TEXT("Recording current population; buffer is limited to 16 MiB. Stop to save.");
}
bool ULivingWorldSubsystem::PlayLastRecording()
{
    if (LastRecording.IsEmpty()) { RecordingStatus = TEXT("Record and save a population first."); return false; }
    if (!IsValid(Replay)) Replay = GetWorld()->SpawnActor<ASimReplay>();
    if (!Replay || !Replay->LoadRecording(LastRecording)) { RecordingStatus = Replay ? Replay->LastError : TEXT("Could not create replay"); return false; }
    Replay->PlayReplay(); RecordingStatus = TEXT("Playing ") + LastRecording + TEXT(" as an inert visual overlay."); return true;
}
void ULivingWorldSubsystem::ClearReplay()
{
    if (IsValid(Replay)) Replay->Destroy(); Replay = nullptr;
    RecordingStatus = TEXT("Replay cleared.");
}

void ULivingWorldSubsystem::CloseMenu()
{
    if (!bMenuOpen) return;
    bMenuOpen = false;
    if (MenuPanel.IsValid()) MenuPanel->SetVisibility(EVisibility::Collapsed);
    if (MenuController.IsValid())
    {
        MenuController->bShowMouseCursor = bPreviousCursor;
        MenuController->SetIgnoreMoveInput(false); MenuController->SetIgnoreLookInput(false);
        MenuController->SetInputMode(FInputModeGameOnly());
    }
}

void ULivingWorldSubsystem::Deinitialize()
{
    CloseMenu();
    if (MenuRoot.IsValid() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(MenuRoot.ToSharedRef());
    MenuRoot.Reset(); MenuPanel.Reset();
    if (CameraManager.IsValid() && StreamingCameraId >= 0) CameraManager->RemoveCamera(StreamingCameraId);
    if (CameraManager.IsValid()) for (int32 Id : GroundStreamingCameraIds) CameraManager->RemoveCamera(Id);
    GroundStreamingCameraIds.Reset();
    for (ALivingAgent* Agent : Pool) if (IsValid(Agent)) Agent->Destroy();
    Pool.Reset();
    Super::Deinitialize();
}
