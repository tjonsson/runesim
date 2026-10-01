#include "LivingWorldSubsystem.h"
#include "SimScenario.h"
#include "SimReplay.h"
#include "LivingAgent.h"
#include "LivingRoute.h"
#include "LivingGeography.h"
#include "LivingTerrainBudgetComponent.h"
#include "LivingPerch.h"
#include "SimPTZ.h"
#include "SimSensorCamera.h"
#include "SimCameraStreamComponent.h"
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
    BuildConflictZones();
    BuildRuntimePerches();
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
        for (ULivingAssetProfile* Profile : Profiles) if (Profile->Kind == Kind && LivingWorld::MatchesPopulation(*Profile, Options.Population)) Choices.Add(Profile);
        // A population with no matching vehicle keeps traffic from whatever vehicles exist.
        if (Choices.IsEmpty()) for (ULivingAssetProfile* Profile : Profiles) if (Profile->Kind == Kind) Choices.Add(Profile);
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
                Agent->ConflictZones = &ConflictZones;
                Agent->Activate(Profile, Route, Distance, Home, Kind == ELivingKind::Bird ? Options.Seed + Flock * 7919 : Random.RandHelper(MAX_int32), Flock, Georeference.IsValid());
                Agent->EnableCombatTarget(Options.bCombatTargets);
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

void ULivingWorldSubsystem::BuildConflictZones()
{
    TArray<ALivingRoute*> Valid;
    for (ALivingRoute* Route : Routes) if (IsValid(Route)) Valid.Add(Route);
    ConflictZones = LivingWorld::FindConflictZones(Valid);
    LivingWorld::LinkRoutes(Valid);
}

void ULivingWorldSubsystem::BuildRuntimePerches()
{
    for (ALivingPerch* Perch : RuntimePerches) if (IsValid(Perch)) Perch->Destroy();
    RuntimePerches.Reset();
    if (!Options.bRuntimePerches) return;
    bool bAnyPercher = false;
    for (const ULivingAssetProfile* Profile : Profiles) bAnyPercher |= Profile->Kind == ELivingKind::Bird && Profile->bAllowPerching;
    if (!bAnyPercher) return;
    // Ground-feeding spots along reviewed walkways, alternating sides. The perch's own
    // landing probe still rejects water, steep ground and occupied positions at runtime.
    int32 Side = 1;
    for (ALivingRoute* Route : Routes)
    {
        if (!IsValid(Route) || Route->bVehicles || Route->ReviewedHalfWidthCm < 60.f) continue;
        const float Length = Route->Path->GetSplineLength();
        for (float Distance = 600.f; Distance < Length - 600.f && RuntimePerches.Num() < 16; Distance += 1200.f, Side = -Side)
        {
            FVector Location, Up;
            const float Lateral = Side * Route->ReviewedHalfWidthCm * .6f;
            if (!Route->SampleGround(Distance, Location, Up, Lateral, 20.f)) continue;
            FActorSpawnParameters Params; Params.ObjectFlags |= RF_Transient;
            ALivingPerch* Perch = GetWorld()->SpawnActor<ALivingPerch>(Location + Up * 20.f,
                FRotationMatrix::MakeFromZ(Up).Rotator(), Params);
            if (!Perch) continue;
            Perch->bValidated = true;
            Perch->Tags.Add(TEXT("LivingWorld.RuntimePerch"));
            RuntimePerches.Add(Perch);
        }
    }
}

TArray<ASimFollowCamera*> ULivingWorldSubsystem::GetSensorCameras() const
{
    TArray<ASimFollowCamera*> Result;
    for (ASimFollowCamera* Camera : SensorCameras) if (IsValid(Camera)) Result.Add(Camera);
    return Result;
}

void ULivingWorldSubsystem::UpdateSensorCameras()
{
    const int32 Wanted = Options.bEnabled ? FMath::Clamp(Options.SensorStreams, 0, 4) : 0;
    SensorCameras.RemoveAll([](const ASimFollowCamera* Camera) { return !IsValid(Camera); });
    while (SensorCameras.Num() > Wanted) SensorCameras.Pop()->Destroy();
    if (!Wanted) return;
    // Drones carry the gimbal feeds first, then helicopters and aircraft. Names give a stable order.
    TArray<ALivingAgent*> Carriers;
    for (ALivingAgent* Agent : Pool)
        if (IsValid(Agent) && Agent->bActive && Agent->Profile && Agent->Behavior != ELivingBehavior::Downed &&
            (Agent->Profile->Kind == ELivingKind::Drone || Agent->Profile->Kind == ELivingKind::Helicopter || Agent->Profile->Kind == ELivingKind::Plane))
            Carriers.Add(Agent);
    auto Rank = [](ELivingKind Kind) { return Kind == ELivingKind::Drone ? 0 : Kind == ELivingKind::Helicopter ? 1 : 2; };
    Carriers.Sort([&](const ALivingAgent& A, const ALivingAgent& B)
        { return Rank(A.Profile->Kind) != Rank(B.Profile->Kind) ? Rank(A.Profile->Kind) < Rank(B.Profile->Kind) : A.GetName() < B.GetName(); });
    for (int32 I = 0; I < Wanted; ++I)
    {
        if (I >= SensorCameras.Num())
        {
            const FTransform Start(Center);
            ASimFollowCamera* Camera = GetWorld()->SpawnActorDeferred<ASimFollowCamera>(ASimFollowCamera::StaticClass(), Start);
            if (!Camera) return;
            Camera->Configure(FString::Printf(TEXT("air-%d"), I + 1), 70.f);
            Camera->FinishSpawning(Start);
            SensorCameras.Add(Camera);
        }
        ASimFollowCamera* Camera = SensorCameras[I];
        ALivingAgent* Current = Cast<ALivingAgent>(Camera->Carrier.Get());
        if (Current && Carriers.Contains(Current)) { Carriers.Remove(Current); continue; }
        if (Carriers.IsEmpty()) { Camera->SetCarrier(nullptr, true, -20.f); continue; }
        ALivingAgent* Next = Carriers[0]; Carriers.RemoveAt(0);
        const float Radius = FMath::Max(20.f, Next->Profile->CollisionRadiusCm);
        Camera->MountOffset = FVector(Radius * .6f, 0.f, -Radius * .4f);
        Camera->SetCarrier(Next, true, Next->Profile->Kind == ELivingKind::Drone ? -25.f : -10.f);
    }
}

ASimPTZ* ULivingWorldSubsystem::GetEngagementCamera()
{
    if (EngagementCamera.IsValid() && EngagementCamera->bAllowSimulatedEngagement) return EngagementCamera.Get();
    EngagementCamera.Reset();
    for (TActorIterator<ASimPTZ> It(GetWorld()); It; ++It)
        if (It->bAllowSimulatedEngagement) { EngagementCamera = *It; break; }
    return EngagementCamera.Get();
}

FText ULivingWorldSubsystem::EngagementText() const
{
    const ASimPTZ* Camera = EngagementCamera.Get();
    if (!Camera) return FText::GetEmpty();
    FString Text = FString::Printf(TEXT("%s [%s view, F6] · %s%s"), *Camera->CameraId, *Camera->GetViewName(),
        Camera->EngagementStatus.IsEmpty() ? TEXT("F7 designate · F8 launch") : *Camera->EngagementStatus,
        Camera->bTrackTarget ? TEXT(" · tracking") : TEXT(""));
    if (Camera->GetInterceptorsInFlight()) Text += FString::Printf(TEXT(" · %d in flight"), Camera->GetInterceptorsInFlight());
    return FText::FromString(Text);
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
        if (!IsValid(Route) || !Route->bValidated || (Route->bVehicles ? Options.TrafficDensity == 0 : Options.CrowdDensity == 0) ||
            FVector::DistSquared(Route->Path->FindLocationClosestToWorldLocation(Center, ESplineCoordinateSpace::World), Center) > FMath::Square(Options.ActivityRadiusMeters*100.f)) continue;
        // Long corridors get one close view per ~120 m segment, so wheel and foot contacts keep
        // refined tiles along the whole route. Stationary views settle to the idle cadence.
        const float Length = Route->Path->GetSplineLength();
        const int32 Segments = FMath::Clamp(FMath::CeilToInt(Length / 12000.f), 1, 4);
        for (int32 Segment = 0; Segment < Segments && GroundViews < 8; ++Segment)
        {
            const float From = Length * Segment / Segments, To = Length * (Segment + 1) / Segments;
            const FVector A = Route->Path->GetLocationAtDistanceAlongSpline(From, ESplineCoordinateSpace::World);
            const FVector B = Route->Path->GetLocationAtDistanceAlongSpline(To, ESplineCoordinateSpace::World);
            const FVector Middle = Route->Path->GetLocationAtDistanceAlongSpline((From + To) * .5f, ESplineCoordinateSpace::World);
            const float SegmentRadius = FMath::Max3(float(FVector::Dist(A, Middle)), float(FVector::Dist(B, Middle)), 500.f);
            const FQuat RouteFrame = LivingGeography::Frame(Georeference.Get(), Middle);
            const FCesiumCamera GroundCamera(FVector2D(1024,1024), Middle + RouteFrame.GetAxisZ()*FMath::Max(3000.f, SegmentRadius*1.6f),
                (RouteFrame*FRotator(-90,0,0).Quaternion()).Rotator(),90.f);
            if (GroundViews == GroundStreamingCameraIds.Num()) GroundStreamingCameraIds.Add(CameraManager->AddCamera(GroundCamera));
            else CameraManager->UpdateCamera(GroundStreamingCameraIds[GroundViews],GroundCamera);
            ++GroundViews;
        }
    }
    while (GroundStreamingCameraIds.Num() > GroundViews) CameraManager->RemoveCamera(GroundStreamingCameraIds.Pop());
}

void ULivingWorldSubsystem::Tick(float DeltaTime)
{
    APlayerController* PC = GetWorld()->GetFirstPlayerController();
    if (PC)
    {
        if (!MenuRoot.IsValid())
        {
            InstallMenu();
            // -LivingWorldMenu[=tab] opens the panel at start (0 Population, 1 Behavior, 2 Simulation), for screenshots and kiosks.
            int32 Tab = 0;
            if (MenuRoot.IsValid() && (FParse::Param(FCommandLine::Get(), TEXT("LivingWorldMenu")) || FParse::Value(FCommandLine::Get(), TEXT("LivingWorldMenu="), Tab)))
            { MenuTab = FMath::Clamp(Tab, 0, 2); ToggleMenu(); }
        }
        // AirSim's weather MenuActor already owns F10 in MainLevel.
        if (PC->WasInputKeyJustPressed(EKeys::F9)) ToggleMenu();
        if (bMenuOpen && PC->WasInputKeyJustPressed(EKeys::Escape)) CloseMenu();
        if (bMenuOpen && (PC->WasInputKeyJustPressed(EKeys::F) || PC->WasInputKeyJustPressed(EKeys::Enter))) ApplyMenuDraft();
        // Simulated engagement from the first opted-in tripod.
        const bool bShift = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
        const bool bControl = PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl);
        if (PC->WasInputKeyJustPressed(EKeys::F6))
            if (ASimPTZ* Camera = GetEngagementCamera()) Camera->CycleView();
        if (PC->WasInputKeyJustPressed(EKeys::F7) || PC->WasInputKeyJustPressed(EKeys::F8))
            if (ASimPTZ* Camera = GetEngagementCamera())
            {
                if (PC->WasInputKeyJustPressed(EKeys::F7))
                {
                    if (bControl) Camera->SetTracking(!Camera->bTrackTarget);
                    else Camera->DesignateTarget(Camera->DesignatedTarget.IsValid() || bShift);
                }
                else if (bShift) Camera->AbortInterceptors();
                else Camera->LaunchInterceptor();
            }
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
        UpdateSensorCameras();
        GetEngagementCamera();
    }
    if (!Options.bEnabled) return;
    {
        FVector Listener, Front, Right;
        const bool bListener = PC && (PC->GetAudioListenerPosition(Listener, Front, Right), true);
        LivingWorld::SetListener(bListener ? Listener : FVector::ZeroVector, bListener);
    }
    Accumulator = FMath::Min(Accumulator + DeltaTime, 0.15f);
    TArray<ALivingAgent*> Active;
    for (ALivingAgent* Agent : Pool) if (IsValid(Agent) && Agent->bActive) Active.Add(Agent);
    TArray<FVector> Threats;
    TArray<FVector> ThreatVelocities;
    if (Options.bReactive)
        for (TActorIterator<APawn> It(GetWorld()); It; ++It)
            if (It->GetVelocity().SizeSquared() > FMath::Square(30.f)) { Threats.Add(It->GetActorLocation()); ThreatVelocities.Add(It->GetVelocity()); }
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
            Agent->ThreatVelocity = Closest ? ThreatVelocities[static_cast<int32>(Closest - Threats.GetData())] : FVector::ZeroVector;
            Agent->Step(1.f / 30.f, Options, Active, Closest);
        }
    }
}

TStatId ULivingWorldSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(ULivingWorldSubsystem, STATGROUP_Tickables);
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

void ULivingWorldSubsystem::Deinitialize()
{
    CloseMenu();
    if (MenuRoot.IsValid() && GetWorld()->GetGameViewport()) GetWorld()->GetGameViewport()->RemoveViewportWidgetContent(MenuRoot.ToSharedRef());
    MenuRoot.Reset(); MenuPanel.Reset(); MenuFocus.Reset(); MenuDraft.Reset();
    if (CameraManager.IsValid() && StreamingCameraId >= 0) CameraManager->RemoveCamera(StreamingCameraId);
    if (CameraManager.IsValid()) for (int32 Id : GroundStreamingCameraIds) CameraManager->RemoveCamera(Id);
    GroundStreamingCameraIds.Reset();
    for (ALivingAgent* Agent : Pool) if (IsValid(Agent)) Agent->Destroy();
    Pool.Reset();
    for (ASimFollowCamera* Camera : SensorCameras) if (IsValid(Camera)) Camera->Destroy();
    SensorCameras.Reset();
    for (ALivingPerch* Perch : RuntimePerches) if (IsValid(Perch)) Perch->Destroy();
    RuntimePerches.Reset();
    ConflictZones.Reset();
    Super::Deinitialize();
}
