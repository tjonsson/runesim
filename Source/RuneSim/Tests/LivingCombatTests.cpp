#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "HAL/IConsoleManager.h"
#include <limits>
#include "../LivingWorldTypes.h"
#include "../LivingAgent.h"
#include "../LivingRoute.h"
#include "../SimPTZ.h"
#include "../SimScenario.h"
#include "../SimReplay.h"
#include "../SimSensorCamera.h"
#include "../SimCameraStreamComponent.h"
#include "../SimWarEffects.h"
#include "Camera/CameraComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SphereComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#if WITH_DEV_AUTOMATION_TESTS

namespace
{
struct FTestWorld
{
    UWorld* World = nullptr;
    FTestWorld()
    {
        SimEvents::SetLiveOutput(false);
        const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
            .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
        World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
        GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    }
    ~FTestWorld()
    {
        GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
        SimEvents::SetLiveOutput(true);
    }
    AActor* Box(const FVector& Location, const FVector& Extent)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        UBoxComponent* Shape = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Shape); Actor->AddInstanceComponent(Shape);
        Shape->SetBoxExtent(Extent); Shape->SetCollisionProfileName(TEXT("BlockAll")); Shape->RegisterComponent();
        Actor->SetActorLocation(Location);
        return Actor;
    }
    AActor* Target(const FVector& Location, float Health = 35.f)
    {
        AActor* Actor = World->SpawnActor<AActor>();
        USphereComponent* Shape = NewObject<USphereComponent>(Actor);
        Actor->SetRootComponent(Shape); Actor->AddInstanceComponent(Shape);
        Shape->SetSphereRadius(40.f); Shape->SetCollisionProfileName(TEXT("Pawn")); Shape->RegisterComponent();
        Actor->SetActorLocation(Location);
        USimTargetComponent* Opt = NewObject<USimTargetComponent>(Actor);
        Opt->ImpactEffect = nullptr; Opt->ResetTarget(Health);
        Actor->AddInstanceComponent(Opt); Opt->RegisterComponent();
        return Actor;
    }
    ALivingRoute* Route(const FVector& A, const FVector& B, bool bVehicles, float HalfWidth)
    {
        ALivingRoute* Route = World->SpawnActor<ALivingRoute>();
        Route->Path->SetSplinePoints({A, B}, ESplineCoordinateSpace::World);
        Route->bValidated = true; Route->bVehicles = bVehicles; Route->ReviewedHalfWidthCm = HalfWidth;
        return Route;
    }
};

/** Flies an interceptor against a constant-velocity target without collision; returns the result. */
ESimInterceptorResult Fly(ASimProjectile* Missile, AActor* Target, const FVector& TargetVelocity, float& Closest)
{
    const float Dt = 1.f / 120.f;
    USimTargetComponent* Opt = Target->FindComponentByClass<USimTargetComponent>();
    Missile->Movement->Velocity = Missile->GetActorForwardVector() * Missile->LaunchSpeed;
    for (int32 I = 0; I < 120 * 30 && Missile->Result == ESimInterceptorResult::InFlight; ++I)
    {
        Target->SetActorLocation(Target->GetActorLocation() + TargetVelocity * Dt);
        Opt->TickComponent(Dt, LEVELTICK_All, nullptr);
        Missile->Guide(Dt);
        if (Missile->Result != ESimInterceptorResult::InFlight) break;
        Missile->SetActorLocation(Missile->GetActorLocation() + Missile->Movement->Velocity * Dt);
    }
    Closest = Missile->ClosestApproachCm;
    return Missile->Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimGuidanceTest, "RuneSim.LivingWorld.Guidance", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimGuidanceTest::RunTest(const FString& Parameters)
{
    const FVector MissileVelocity(20000, 0, 0);
    const FVector Command = SimGuidance::Acceleration(FVector::ZeroVector, MissileVelocity, FVector(100000, 0, 5000), FVector(0, 5000, 0), 4, 34300);
    TestTrue(TEXT("PN command is perpendicular to missile velocity"), FMath::Abs(FVector::DotProduct(Command.GetSafeNormal(), MissileVelocity.GetSafeNormal())) < 1e-3);
    TestTrue(TEXT("PN command leads a target moving in +Y"), Command.Y > 0);
    TestTrue(TEXT("PN command respects the lateral limit"), Command.Size() <= 34300.1f);
    TestTrue(TEXT("Collision course needs no correction"), SimGuidance::Acceleration(FVector::ZeroVector, MissileVelocity, FVector(100000, 0, 0), FVector::ZeroVector, 4, 34300).Size() < 1);
    TestTrue(TEXT("Degenerate inputs fail safe"), SimGuidance::Acceleration(FVector::ZeroVector, FVector::ZeroVector, FVector(1, 0, 0), FVector::ZeroVector, 4, 100).IsZero());

    FTestWorld Test;
    struct FCase { const TCHAR* Name; FVector Start; FVector Velocity; float Health; bool bHit; };
    const FCase Cases[] = {
        {TEXT("crossing drone"), FVector(25000, -6000, 6000), FVector(0, 1500, 0), LivingWorld::DefaultTargetHealth(ELivingKind::Drone), true},
        {TEXT("crossing aircraft"), FVector(60000, -30000, 15000), FVector(0, 15000, 0), LivingWorld::DefaultTargetHealth(ELivingKind::Plane), true},
        {TEXT("diving helicopter"), FVector(30000, 10000, 12000), FVector(-2000, -3000, -800), LivingWorld::DefaultTargetHealth(ELivingKind::Helicopter), true},
        {TEXT("receding supersonic target"), FVector(20000, 0, 3000), FVector(45000, 0, 0), LivingWorld::DefaultTargetHealth(ELivingKind::Plane), false},
    };
    for (const FCase& Case : Cases)
    {
        AActor* Target = Test.Target(Case.Start, Case.Health);
        const FRotator Aim = (Case.Start - FVector::ZeroVector).Rotation() + FRotator(12, 0, 0);
        ASimProjectile* Missile = Test.World->SpawnActor<ASimProjectile>(FVector::ZeroVector, Aim);
        TestTrue(FString(TEXT("Target accepted: ")) + Case.Name, Missile->SetSimulatedTarget(Target));
        float Closest = 0;
        const ESimInterceptorResult Result = Fly(Missile, Target, Case.Velocity, Closest);
        const USimTargetComponent* Opt = Target->FindComponentByClass<USimTargetComponent>();
        if (Case.bHit)
        {
            TestEqual(FString(TEXT("Intercept: ")) + Case.Name, Result, ESimInterceptorResult::Hit);
            TestTrue(FString(TEXT("Proximity fuse inside radius: ")) + Case.Name, Closest <= Missile->ProximityFuseCm);
            TestTrue(FString(TEXT("Blast destroys target: ")) + Case.Name, Opt->bDestroyed);
        }
        else
        {
            TestEqual(FString(TEXT("Self-destructs after max flight: ")) + Case.Name, Result, ESimInterceptorResult::Missed);
            TestFalse(FString(TEXT("Out-of-reach target intact: ")) + Case.Name, Opt->bDestroyed);
        }
        TestFalse(FString(TEXT("Resolved interceptor stops guiding: ")) + Case.Name, (Missile->Guide(.1f), Missile->Result == ESimInterceptorResult::InFlight));
    }
    ASimProjectile* Aborted = Test.World->SpawnActor<ASimProjectile>();
    Aborted->Abort();
    TestEqual(TEXT("Abort resolves the interceptor"), Aborted->Result, ESimInterceptorResult::Aborted);
    AActor* Nearby = Test.Target(FVector(-5000, 0, 0), LivingWorld::DefaultTargetHealth(ELivingKind::Drone));
    AActor* Far = Test.Target(FVector(-5000, 3000, 0), LivingWorld::DefaultTargetHealth(ELivingKind::Drone));
    ASimProjectile* Burst = Test.World->SpawnActor<ASimProjectile>(FVector(-5000, 900, 0), FRotator::ZeroRotator);
    Burst->Abort();
    TestTrue(TEXT("Blast beyond the fuse radius still damages a nearby target"), Nearby->FindComponentByClass<USimTargetComponent>()->bDestroyed);
    TestEqual(TEXT("Blast does not reach distant targets"), Far->FindComponentByClass<USimTargetComponent>()->Health, LivingWorld::DefaultTargetHealth(ELivingKind::Drone));
    AActor* Destroyed = Test.Target(FVector(5000, 0, 0));
    Destroyed->FindComponentByClass<USimTargetComponent>()->ApplyVirtualDamage(1000);
    TestFalse(TEXT("Destroyed targets cannot be engaged"), Test.World->SpawnActor<ASimProjectile>()->SetSimulatedTarget(Destroyed));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimEngagementTest, "RuneSim.LivingWorld.Engagement", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimEngagementTest::RunTest(const FString& Parameters)
{
    FTestWorld Test;
    ASimPTZ* PTZ = Test.World->SpawnActor<ASimPTZ>();
    AActor* Ahead = Test.Target(FVector(10000, 0, 2000));
    AActor* Side = Test.Target(FVector(0, 8000, 3000));
    TestFalse(TEXT("Launch refused while engagement is disabled"), PTZ->LaunchInterceptor() != nullptr);
    PTZ->bAllowSimulatedEngagement = true;
    TestTrue(TEXT("Designates a visible target"), PTZ->DesignateTarget(false));
    TestTrue(TEXT("Most central target designated first"), PTZ->DesignatedTarget.Get() == Ahead);
    TestTrue(TEXT("Next cycles to another visible target"), PTZ->DesignateTarget(true) && PTZ->DesignatedTarget.Get() == Side);
    TestTrue(TEXT("Cycling wraps around"), PTZ->DesignateTarget(true) && PTZ->DesignatedTarget.Get() == Ahead);
    const FVector2D Angles = PTZ->AnglesTo(Side->GetActorLocation());
    TestTrue(TEXT("Pan angle to side target"), FMath::IsNearlyEqual(Angles.X, 90.f, .01f));
    TestTrue(TEXT("Tilt angle to side target"), FMath::IsNearlyEqual(Angles.Y, FMath::RadiansToDegrees(FMath::Atan2(3000.f - 160.f, 8000.f)), .01f));

    AActor* Wall = Test.Box(FVector(0, 4000, 1500), FVector(2000, 20, 3000));
    TestTrue(TEXT("Designation still finds the unobstructed target"), PTZ->DesignateTarget(true) && PTZ->DesignatedTarget.Get() == Ahead);
    TestTrue(TEXT("Terrain-occluded target is skipped"), PTZ->DesignateTarget(true) && PTZ->DesignatedTarget.Get() == Ahead);
    Wall->Destroy();

    AActor* Behind = Test.Target(FVector(-10000, 0, 1000));
    for (int32 I = 0; I < 4; ++I)
    {
        PTZ->DesignateTarget(true);
        TestTrue(TEXT("Targets beyond the pan limit are never designated"), PTZ->DesignatedTarget.Get() != Behind);
    }
    AActor* Distant = Test.Target(FVector(PTZ->MaxEngagementRangeCm + 5000.f, 0, 0));
    for (int32 I = 0; I < 4; ++I) { PTZ->DesignateTarget(true); TestTrue(TEXT("Out-of-range targets ignored"), PTZ->DesignatedTarget.Get() != Distant); }

    PTZ->DesignateTarget(false);
    PTZ->SetPTZ(-40.f, 5.f, 70.f);
    const FVector OperatorView = PTZ->Command;
    TestTrue(TEXT("Track command"), PTZ->ExecuteEngagementCommand(TEXT(" TRACK_ON ")));
    for (int32 I = 0; I < 120; ++I) PTZ->Tick(1.f / 30.f);
    const FVector2D AheadAngles = PTZ->AnglesTo(Ahead->GetActorLocation());
    TestTrue(TEXT("Tracking pans to the target"), FMath::IsNearlyEqual(PTZ->Command.X, AheadAngles.X, .01f));
    TestTrue(TEXT("Tracking tilts to the target"), FMath::IsNearlyEqual(PTZ->Command.Y, AheadAngles.Y, .01f));
    TestTrue(TEXT("Auto-zoom frames a small target at the narrowest optics"), FMath::IsNearlyEqual(PTZ->Command.Z, 5.f, .01f));
    TestTrue(TEXT("Slew reaches commanded pan"), FMath::IsNearlyEqual(PTZ->Pan->GetRelativeRotation().Yaw, AheadAngles.X, .1f));

    ASimProjectile* First = PTZ->LaunchInterceptor();
    TestNotNull(TEXT("Interceptor launched at designated target"), First);
    if (First) TestTrue(TEXT("Interceptor guides to the designated target"), First->GetSimulatedTarget() == Ahead);
    TestEqual(TEXT("Launch counted"), PTZ->Launches, 1);
    TestEqual(TEXT("One interceptor in flight"), PTZ->GetInterceptorsInFlight(), 1);
    TestTrue(TEXT("Tripod view is the default while the missile flies"), PTZ->Stream->GetActiveCapture() == PTZ->Capture.Get());
    TestTrue(TEXT("Launch keeps the target tracked"), PTZ->bTrackTarget);
    TestTrue(TEXT("View command selects the missile seeker"), PTZ->ExecuteEngagementCommand(TEXT("view")) &&
        PTZ->Stream->GetActiveCapture() == (First ? First->Capture.Get() : nullptr));
    TestTrue(TEXT("View command selects the chase camera"), PTZ->ExecuteEngagementCommand(TEXT("view")) &&
        PTZ->Stream->GetActiveCapture() == (First ? First->ChaseCapture.Get() : nullptr));
    TestTrue(TEXT("View command returns to the tripod"), PTZ->ExecuteEngagementCommand(TEXT("view")) && PTZ->Stream->GetActiveCapture() == PTZ->Capture.Get());
    TestTrue(TEXT("Missile view selected"), PTZ->ExecuteEngagementCommand(TEXT("view_missile")) && PTZ->GetViewName() == TEXT("missile"));
    TestNull(TEXT("Launcher cooldown prevents a salvo"), PTZ->LaunchInterceptor());
    TestTrue(TEXT("Cooldown reported"), PTZ->EngagementStatus.Contains(TEXT("reloading")));
    for (int32 I = 0; I < 90; ++I) PTZ->Tick(1.f / 30.f);
    TestNotNull(TEXT("Second interceptor after reload"), PTZ->LaunchInterceptor());
    for (int32 I = 0; I < 90; ++I) PTZ->Tick(1.f / 30.f);
    TestNull(TEXT("In-flight limit enforced"), PTZ->LaunchInterceptor());
    TestTrue(TEXT("Abort command"), PTZ->ExecuteEngagementCommand(TEXT("abort")));
    TestEqual(TEXT("Nothing left in flight"), PTZ->GetInterceptorsInFlight(), 0);
    PTZ->Tick(1.f / 30.f);
    TestTrue(TEXT("View holds on the burst"), PTZ->Stream->GetActiveCapture() != PTZ->Capture.Get());
    for (int32 I = 0; I < 100; ++I) PTZ->Tick(1.f / 30.f);
    TestTrue(TEXT("View returns to the tripod after the burst"), PTZ->Stream->GetActiveCapture() == PTZ->Capture.Get());
    TestEqual(TEXT("Chosen view is kept for the next launch"), PTZ->GetViewName(), FString(TEXT("missile")));
    PTZ->ExecuteEngagementCommand(TEXT("view_tripod"));
    TestFalse(TEXT("Unknown commands rejected"), PTZ->ExecuteEngagementCommand(TEXT("self_destruct_all")));

    Ahead->FindComponentByClass<USimTargetComponent>()->ApplyVirtualDamage(1000);
    PTZ->Tick(1.f / 30.f);
    TestFalse(TEXT("Destroyed target designation cleared"), PTZ->DesignatedTarget.IsValid());
    TestTrue(TEXT("Clear command stops tracking"), PTZ->ExecuteEngagementCommand(TEXT("clear")) && !PTZ->bTrackTarget);
    TestTrue(TEXT("Stopping tracking restores the operator's view"), PTZ->Command.Equals(OperatorView, .01));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingShotDownTest, "RuneSim.LivingWorld.ShotDown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingShotDownTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("People are never targets"), !LivingWorld::IsEngageable(ELivingKind::Civilian) && !LivingWorld::IsEngageable(ELivingKind::Soldier));
    TestFalse(TEXT("Birds are never targets"), LivingWorld::IsEngageable(ELivingKind::Bird));
    TestTrue(TEXT("Drones are lighter than aircraft"), LivingWorld::DefaultTargetHealth(ELivingKind::Drone) < LivingWorld::DefaultTargetHealth(ELivingKind::Plane));
    FTestWorld Test;
    Test.Box(FVector(0, 0, -10), FVector(50000, 50000, 10));
    ULivingAssetProfile* Drone = NewObject<ULivingAssetProfile>();
    Drone->Kind = ELivingKind::Drone; Drone->SpeedMetersPerSecond = 12; Drone->CollisionRadiusCm = 30; Drone->AltitudeMeters = 30;
    ALivingAgent* Agent = Test.World->SpawnActor<ALivingAgent>(FVector(0, 0, 3000), FRotator::ZeroRotator);
    Agent->Activate(Drone, nullptr, 0, FVector(0, 0, 3000), 7, 0);
    Agent->EnableCombatTarget(true);
    if (!TestNotNull(TEXT("Drone opts into virtual damage"), Agent->TargetComponent.Get())) return false;
    TestTrue(TEXT("Drone is engageable"), Agent->TargetComponent->CanBeEngaged());
    TestEqual(TEXT("Category default health"), Agent->TargetComponent->Health, 35.f);
    FLivingWorldOptions Options;
    for (int32 I = 0; I < 30; ++I) Agent->Step(1.f / 30.f, Options, {Agent}, nullptr);
    Agent->TargetComponent->ApplyVirtualDamage(100);
    TestEqual(TEXT("Destroyed drone is downed, not hidden"), Agent->Behavior, ELivingBehavior::Downed);
    TestFalse(TEXT("Wreck is still visible while falling"), Agent->IsHidden());
    TestFalse(TEXT("Wreck cannot be engaged again"), Agent->TargetComponent->CanBeEngaged());
    const float StartZ = Agent->GetActorLocation().Z;
    for (int32 I = 0; I < 15; ++I) Agent->Step(1.f / 30.f, Options, {Agent}, nullptr);
    TestTrue(TEXT("Wreck falls"), Agent->GetActorLocation().Z < StartZ);
    float Seconds = 0;
    while (Agent->bActive && Seconds < 30) { Agent->Step(1.f / 30.f, Options, {Agent}, nullptr); Seconds += 1.f / 30.f; }
    TestFalse(TEXT("Wreck is recycled after ground impact"), Agent->bActive);
    TestTrue(TEXT("Impact occurs by free fall time"), Seconds < 5.f);
    TestTrue(TEXT("Wreck stopped at the ground"), Agent->GetActorLocation().Z > -100.f);

    Agent->SetActorLocation(FVector(0, 0, 3000));
    Agent->Activate(Drone, nullptr, 0, FVector(0, 0, 3000), 8, 0);
    TestFalse(TEXT("Reused agent is not engageable until enabled"), Agent->TargetComponent->CanBeEngaged());
    Agent->EnableCombatTarget(true);
    TestTrue(TEXT("Reused agent restored"), Agent->TargetComponent->CanBeEngaged() && Agent->Behavior == ELivingBehavior::Cruising && Agent->TargetComponent->Health == 35.f);
    Agent->EnableCombatTarget(false);
    TestFalse(TEXT("Combat targets can be switched off"), Agent->TargetComponent->CanBeEngaged());

    ULivingAssetProfile* Person = NewObject<ULivingAssetProfile>();
    Person->Kind = ELivingKind::Civilian; Person->SpeedMetersPerSecond = 1.4f; Person->CollisionRadiusCm = 30;
    ALivingAgent* Civilian = Test.World->SpawnActor<ALivingAgent>(FVector(0, 500, 3000), FRotator::ZeroRotator);
    Civilian->Activate(Person, nullptr, 0, FVector::ZeroVector, 9, 0);
    Civilian->EnableCombatTarget(true);
    TestTrue(TEXT("Civilians never receive a target component"), Civilian->TargetComponent == nullptr);

    ULivingAssetProfile* Car = NewObject<ULivingAssetProfile>();
    Car->Kind = ELivingKind::Car; Car->SpeedMetersPerSecond = 8; Car->CollisionRadiusCm = 50; Car->GroundClearanceCm = 100;
    ALivingRoute* Road = Test.Route(FVector(0, 0, 0), FVector(10000, 0, 0), true, 200);
    ALivingAgent* Vehicle = Test.World->SpawnActor<ALivingAgent>(FVector(1000, 0, 100), FRotator::ZeroRotator);
    Vehicle->Activate(Car, Road, 1000, FVector::ZeroVector, 10, 0);
    Vehicle->EnableCombatTarget(true);
    for (int32 I = 0; I < 30; ++I) Vehicle->Step(1.f / 30.f, Options, {Vehicle}, nullptr);
    USimWarEffects* War = Test.World->GetSubsystem<USimWarEffects>();
    if (War) War->StopAllFires(); // Fires from the drone crash above are not part of this check.
    Vehicle->TargetComponent->ApplyVirtualDamage(500);
    const FVector Stopped = Vehicle->GetActorLocation();
    for (int32 I = 0; I < 60; ++I) Vehicle->Step(1.f / 30.f, Options, {Vehicle}, nullptr);
    TestTrue(TEXT("Disabled vehicle stays in place"), Vehicle->GetActorLocation().Equals(Stopped, .01));
    const int32 Burning = War ? War->ActiveFireCount() : 0;
    TestTrue(TEXT("A disabled vehicle becomes a burning wreck"), Burning >= 1);
    const int32 BurnSteps = FMath::CeilToInt32((ALivingAgent::WreckBurnSeconds + 1.f) * 30.f);
    for (int32 I = 0; I < BurnSteps && Vehicle->bActive; ++I) Vehicle->Step(1.f / 30.f, Options, {Vehicle}, nullptr);
    TestFalse(TEXT("Burning vehicle is recycled"), Vehicle->bActive);
    TestTrue(TEXT("The wreck fire goes out with the recycled vehicle"), War && War->ActiveFireCount() < Burning);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingCrowdReactionTest, "RuneSim.LivingWorld.CrowdReactions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingCrowdReactionTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Yield away from a threat on the right"), LivingWorld::YieldOffset(500, 120), -120.f);
    TestEqual(TEXT("Yield away from a threat on the left"), LivingWorld::YieldOffset(-500, 120), 120.f);
    TestEqual(TEXT("No room means no lateral yield"), LivingWorld::YieldOffset(500, 0), 0.f);
    TestTrue(TEXT("Close threat is urgent"), LivingWorld::IsUrgentThreat(800, 0));
    TestTrue(TEXT("Fast approaching threat is urgent"), LivingWorld::IsUrgentThreat(1500, 600));
    TestFalse(TEXT("Distant passing threat is not urgent"), LivingWorld::IsUrgentThreat(1500, 0));
    TestTrue(TEXT("Approaching engine pitches up"), LivingWorld::DopplerFactor(FVector(10000, 0, 0), FVector(8000, 0, 0)) > 1.2f);
    TestTrue(TEXT("Receding engine pitches down"), LivingWorld::DopplerFactor(FVector(10000, 0, 0), FVector(-8000, 0, 0)) < .85f);
    TestTrue(TEXT("Crossing engine has no shift"), FMath::IsNearlyEqual(LivingWorld::DopplerFactor(FVector(10000, 0, 0), FVector(0, 8000, 0)), 1.f));
    ULivingAssetProfile* Tank = NewObject<ULivingAssetProfile>(); Tank->Kind = ELivingKind::Car; Tank->bMilitary = true;
    ULivingAssetProfile* Sedan = NewObject<ULivingAssetProfile>(); Sedan->Kind = ELivingKind::Car;
    ULivingAssetProfile* Walker = NewObject<ULivingAssetProfile>(); Walker->Kind = ELivingKind::Civilian;
    TestFalse(TEXT("Civilian traffic excludes military vehicles"), LivingWorld::MatchesPopulation(*Tank, ELivingPopulation::Civilians));
    TestFalse(TEXT("Military traffic excludes civilian cars"), LivingWorld::MatchesPopulation(*Sedan, ELivingPopulation::Military));
    TestTrue(TEXT("Mixed traffic takes both"), LivingWorld::MatchesPopulation(*Tank, ELivingPopulation::Mixed) && LivingWorld::MatchesPopulation(*Sedan, ELivingPopulation::Mixed));
    TestTrue(TEXT("Affiliation only filters vehicles"), LivingWorld::MatchesPopulation(*Walker, ELivingPopulation::Military));
    TestTrue(TEXT("Flat ground needs no ankle tilt"), LivingWorld::FootTilt(FVector::UpVector, 1.f).Equals(FQuat::Identity, 1e-5));
    const FVector Slope = FRotator(20, 0, 0).RotateVector(FVector::UpVector);
    TestTrue(TEXT("Planted foot matches a 20 degree slope"), (LivingWorld::FootTilt(Slope, 1.f).RotateVector(FVector::UpVector) - Slope).Size() < 1e-4);
    TestTrue(TEXT("Ankle tilt is limited"), FMath::RadiansToDegrees(LivingWorld::FootTilt(FRotator(60, 0, 0).RotateVector(FVector::UpVector), 1.f).GetAngle()) <= 25.01f);
    TestTrue(TEXT("Lifted foot keeps its authored roll"), LivingWorld::FootTilt(Slope, 0.f).Equals(FQuat::Identity, 1e-5));
    TestTrue(TEXT("Invalid normal is ignored"), LivingWorld::FootTilt(FVector(NAN, 0, 0), 1.f).Equals(FQuat::Identity, 1e-5));

    FTestWorld Test;
    Test.Box(FVector(0, 0, -10), FVector(20000, 2000, 10));
    ALivingRoute* Walk = Test.Route(FVector(0, 0, 0), FVector(12000, 0, 0), false, 200);
    ULivingAssetProfile* Person = NewObject<ULivingAssetProfile>();
    Person->Kind = ELivingKind::Civilian; Person->SpeedMetersPerSecond = 1.4f; Person->CollisionRadiusCm = 30; Person->GroundClearanceCm = 100;
    FLivingWorldOptions Options; Options.bReactive = true;
    ALivingAgent* Civilian = Test.World->SpawnActor<ALivingAgent>(FVector(3000, 0, 100), FRotator::ZeroRotator);
    Civilian->Activate(Person, Walk, 3000, FVector::ZeroVector, 11, 0);
    const FVector Distant(3000, 1500, 400);
    Civilian->ThreatVelocity = FVector(0, 0, 0);
    for (int32 I = 0; I < 60; ++I) Civilian->Step(1.f / 30.f, Options, {Civilian}, &Distant);
    TestEqual(TEXT("Distant hovering threat: civilian steps aside"), Civilian->Behavior, ELivingBehavior::Yielding);
    TestTrue(TEXT("Steps to the side away from the threat"), Civilian->LateralOffsetCm < -50.f);
    TestTrue(TEXT("Stepping aside is slower than walking"), Civilian->Velocity.Size() <= 1.4f * 100.f * .5f + 20.f);
    TestTrue(TEXT("Stays inside the reviewed corridor"), FMath::Abs(Civilian->LateralOffsetCm) + 30.f <= 200.01f);
    Civilian->ThreatVelocity = FVector(0, -800, 0);
    for (int32 I = 0; I < 15; ++I) Civilian->Step(1.f / 30.f, Options, {Civilian}, &Distant);
    TestEqual(TEXT("Approaching threat: civilian flees"), Civilian->Behavior, ELivingBehavior::Fleeing);
    for (int32 I = 0; I < 300; ++I) Civilian->Step(1.f / 30.f, Options, {Civilian}, nullptr);
    TestEqual(TEXT("Civilian recovers after the threat leaves"), Civilian->Behavior, ELivingBehavior::Cruising);

    ULivingAssetProfile* Trooper = NewObject<ULivingAssetProfile>();
    Trooper->Kind = ELivingKind::Soldier; Trooper->SpeedMetersPerSecond = 1.3f; Trooper->CollisionRadiusCm = 30; Trooper->GroundClearanceCm = 100;
    ALivingAgent* Soldier = Test.World->SpawnActor<ALivingAgent>(FVector(6000, 0, 100), FRotator::ZeroRotator);
    Soldier->Activate(Trooper, Walk, 6000, FVector::ZeroVector, 12, 0);
    const FVector Watch(6000, -1400, 600);
    Soldier->ThreatVelocity = FVector::ZeroVector;
    for (int32 I = 0; I < 90; ++I) Soldier->Step(1.f / 30.f, Options, {Soldier}, &Watch);
    TestEqual(TEXT("Soldier halts to observe a threat"), Soldier->Behavior, ELivingBehavior::Halted);
    TestTrue(TEXT("Halted soldier stands still"), Soldier->Velocity.Size() < 1.f);
    const FVector Look = FVector::VectorPlaneProject(Watch - Soldier->GetActorLocation(), FVector::UpVector).GetSafeNormal();
    TestTrue(TEXT("Soldier turns to face the threat"), FVector::DotProduct(Soldier->GetActorForwardVector(), Look) > .9f);
    for (int32 I = 0; I < 300; ++I) Soldier->Step(1.f / 30.f, Options, {Soldier}, nullptr);
    Civilian->Deactivate();
    Soldier->SetActorLocation(FVector(1000, 0, 100));
    Soldier->Activate(Trooper, Walk, 1000, FVector::ZeroVector, 13, 0);
    bool bPaused = false; float Travelled = 0; FString Blocked;
    for (int32 I = 0; I < 90 * 30; ++I)
    {
        const FVector Before = Soldier->GetActorLocation();
        Soldier->Step(1.f / 30.f, Options, {Soldier}, nullptr);
        Travelled += FVector::Dist(Before, Soldier->GetActorLocation());
        bPaused |= Soldier->Behavior == ELivingBehavior::Halted;
        if (Blocked.IsEmpty()) Blocked = Soldier->BlockedReason;
    }
    TestTrue(FString::Printf(TEXT("Patrols pause to observe between legs (travelled %.0f cm, blocked '%s', behavior %d)"),
        Travelled, *Blocked, int32(Soldier->Behavior)), bPaused);
    TestTrue(TEXT("Patrol still makes progress"), Travelled > 5000.f);

    // Two people blocking each other in single file: one turns back instead of waiting forever.
    ALivingRoute* Narrow = Test.Route(FVector(0, 1500, 0), FVector(8000, 1500, 0), false, 0);
    ALivingAgent* A = Test.World->SpawnActor<ALivingAgent>(FVector(3000, 1500, 100), FRotator::ZeroRotator);
    ALivingAgent* B = Test.World->SpawnActor<ALivingAgent>(FVector(3200, 1500, 100), FRotator::ZeroRotator);
    A->Activate(Person, Narrow, 3000, FVector::ZeroVector, 40, 0);
    B->Activate(Person, Narrow, 3200, FVector::ZeroVector, 41, 0);
    for (int32 I = 0; I < 30; ++I) { A->Step(1.f / 30.f, Options, {A, B}, nullptr); B->Step(1.f / 30.f, Options, {A, B}, nullptr); }
    // Point them at each other.
    // A close threat just ahead of B turns it back toward A.
    const FVector Toward = B->GetActorLocation() + FVector(300, 0, 0);
    for (int32 I = 0; I < 15; ++I) B->Step(1.f / 30.f, Options, {A, B}, &Toward);
    float Moved = 0; FVector Last = A->GetActorLocation();
    for (int32 I = 0; I < 30 * 12; ++I)
    {
        A->Step(1.f / 30.f, Options, {A, B}, nullptr); B->Step(1.f / 30.f, Options, {A, B}, nullptr);
        Moved += FVector::Dist(Last, A->GetActorLocation()); Last = A->GetActorLocation();
    }
    TestTrue(TEXT("A mutual block resolves by turning back"), Moved > 300.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingCrossingTest, "RuneSim.LivingWorld.Crossings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingCrossingTest::RunTest(const FString& Parameters)
{
    FTestWorld Test;
    Test.Box(FVector(6000, 0, -10), FVector(20000, 20000, 10));
    ALivingRoute* Road = Test.Route(FVector(0, 0, 0), FVector(12000, 0, 0), true, 150);
    ALivingRoute* Crossing = Test.Route(FVector(6000, -3000, 0), FVector(6000, 3000, 0), false, 100);
    ALivingRoute* Shoulder = Test.Route(FVector(0, 250, 0), FVector(12000, 250, 0), false, 100);
    ALivingRoute* SideRoad = Test.Route(FVector(9000, -5000, 0), FVector(9000, 5000, 0), true, 150);
    TArray<FLivingConflictZone> Zones = LivingWorld::FindConflictZones({Road, Crossing, Shoulder, SideRoad});
    int32 Pedestrian = 0, Junction = 0;
    for (const FLivingConflictZone& Zone : Zones)
    {
        Pedestrian += Zone.bPedestrianCrossing; Junction += !Zone.bPedestrianCrossing;
        const bool bShoulder = Zone.Entries.ContainsByPredicate([&](const TPair<TWeakObjectPtr<ALivingRoute>, float>& E) { return E.Key.Get() == Shoulder && E.Value > 500 && E.Value < 11500; });
        TestFalse(TEXT("A parallel shoulder is not a crossing"), bShoulder && Zone.Entries.ContainsByPredicate([&](const TPair<TWeakObjectPtr<ALivingRoute>, float>& E) { return E.Key.Get() == Road; }));
    }
    const FLivingConflictZone* Zebra = Zones.FindByPredicate([&](const FLivingConflictZone& Z)
        { return Z.bPedestrianCrossing && Z.Entries.ContainsByPredicate([&](const TPair<TWeakObjectPtr<ALivingRoute>, float>& E) { return E.Key.Get() == Crossing; }) &&
                 Z.Entries.ContainsByPredicate([&](const TPair<TWeakObjectPtr<ALivingRoute>, float>& E) { return E.Key.Get() == Road; }); });
    if (!TestNotNull(TEXT("Road/walkway crossing found"), Zebra)) return false;
    TestTrue(TEXT("Crossing located at the intersection"), FVector::Dist2D(Zebra->Location, FVector(6000, 0, 0)) < 150.f);
    TestTrue(TEXT("Vehicle junction found"), Junction >= 1);
    TestTrue(TEXT("Shoulder meets the side road as a crossing"), Pedestrian >= 2);

    ULivingAssetProfile* Car = NewObject<ULivingAssetProfile>();
    Car->Kind = ELivingKind::Car; Car->SpeedMetersPerSecond = 8; Car->CollisionRadiusCm = 60; Car->GroundClearanceCm = 100;
    ULivingAssetProfile* Person = NewObject<ULivingAssetProfile>();
    Person->Kind = ELivingKind::Civilian; Person->SpeedMetersPerSecond = .01f; Person->CollisionRadiusCm = 30; Person->GroundClearanceCm = 100;
    TArray<FLivingConflictZone> Only = {*Zebra};
    ALivingAgent* Vehicle = Test.World->SpawnActor<ALivingAgent>(FVector(1000, 0, 100), FRotator::ZeroRotator);
    ALivingAgent* Walker = Test.World->SpawnActor<ALivingAgent>(FVector(6000, 0, 100), FRotator::ZeroRotator);
    Vehicle->ConflictZones = &Only; Walker->ConflictZones = &Only;
    Vehicle->Activate(Car, Road, 1000, FVector::ZeroVector, 20, 0);
    Walker->Activate(Person, Crossing, 3000, FVector::ZeroVector, 21, 0);
    const TArray<ALivingAgent*> Agents = {Vehicle, Walker};
    FLivingWorldOptions Options;
    for (int32 I = 0; I < 30 * 20; ++I) { Vehicle->Step(1.f / 30.f, Options, Agents, nullptr); Walker->Step(1.f / 30.f, Options, Agents, nullptr); }
    TestTrue(TEXT("Vehicle stops before a pedestrian on the crossing"), Vehicle->GetActorLocation().X < 6000.f - Zebra->RadiusCm - 60.f);
    TestTrue(TEXT("Vehicle waits at rest"), Vehicle->Velocity.Size() < 1.f);
    Walker->Deactivate();
    for (int32 I = 0; I < 30 * 6; ++I) Vehicle->Step(1.f / 30.f, Options, Agents, nullptr);
    TestTrue(TEXT("Vehicle proceeds once the crossing is clear"), Vehicle->GetActorLocation().X > 6500.f);

    // A pedestrian waits for a vehicle already inside the crossing.
    Vehicle->SetActorLocation(FVector(6000, 0, 100));
    Vehicle->Activate(Car, Road, 6000, FVector::ZeroVector, 22, 0);
    Person->SpeedMetersPerSecond = 1.4f;
    Walker->SetActorLocation(FVector(6000, -1500, 100));
    Walker->Activate(Person, Crossing, 1500, FVector::ZeroVector, 23, 0);
    Car->SpeedMetersPerSecond = .01f;
    for (int32 I = 0; I < 30 * 8; ++I) { Vehicle->Step(1.f / 30.f, Options, Agents, nullptr); Walker->Step(1.f / 30.f, Options, Agents, nullptr); }
    TestTrue(TEXT("Pedestrian waits outside an occupied crossing"), Walker->GetActorLocation().Y < -Zebra->RadiusCm - 100.f + 1.f);

    // Don't block the box: with a stopped car just beyond the crossing, the next car waits before it.
    Walker->Deactivate();
    Car->SpeedMetersPerSecond = 8;
    ALivingAgent* Stalled = Test.World->SpawnActor<ALivingAgent>(FVector(6450, 0, 100), FRotator::ZeroRotator);
    Stalled->ConflictZones = &Only; Vehicle->ConflictZones = &Only;
    ULivingAssetProfile* Parked = DuplicateObject<ULivingAssetProfile>(Car, Car->GetOuter()); Parked->SpeedMetersPerSecond = .01f;
    Stalled->Activate(Parked, Road, 6450, FVector::ZeroVector, 26, 0);
    Vehicle->SetActorLocation(FVector(3000, 0, 100)); Vehicle->Activate(Car, Road, 3000, FVector::ZeroVector, 27, 0);
    const TArray<ALivingAgent*> Queue = {Vehicle, Stalled};
    for (int32 I = 0; I < 30 * 12; ++I) { Vehicle->Step(1.f / 30.f, Options, Queue, nullptr); Stalled->Step(1.f / 30.f, Options, Queue, nullptr); }
    TestTrue(TEXT("Queued car stops before the crossing instead of inside it"), Vehicle->GetActorLocation().X < 6000.f - Zebra->RadiusCm);
    Stalled->Deactivate();

    // Vehicle junction: first come, first served, without overlap.
    Car->SpeedMetersPerSecond = 8;
    const FLivingConflictZone* Cross = Zones.FindByPredicate([&](const FLivingConflictZone& Z) { return !Z.bPedestrianCrossing; });
    TArray<FLivingConflictZone> JunctionOnly = {*Cross};
    ALivingAgent* Other = Test.World->SpawnActor<ALivingAgent>(FVector(9000, -3000, 100), FRotator::ZeroRotator);
    Vehicle->ConflictZones = &JunctionOnly; Other->ConflictZones = &JunctionOnly;
    Walker->Deactivate();
    Vehicle->SetActorLocation(FVector(6500, 0, 100));
    Vehicle->Activate(Car, Road, 6500, FVector::ZeroVector, 24, 0);
    Other->Activate(Car, SideRoad, 2000, FVector::ZeroVector, 25, 0);
    const TArray<ALivingAgent*> Cars = {Vehicle, Other};
    Road->bOneWay = true; SideRoad->bOneWay = true;
    float Closest = BIG_NUMBER, MaxX = 0, MaxY = -BIG_NUMBER;
    for (int32 I = 0; I < 30 * 20; ++I)
    {
        Vehicle->Step(1.f / 30.f, Options, Cars, nullptr); Other->Step(1.f / 30.f, Options, Cars, nullptr);
        if (Vehicle->bActive && Other->bActive) Closest = FMath::Min(Closest, float(FVector::Dist(Vehicle->GetActorLocation(), Other->GetActorLocation())));
        MaxX = FMath::Max(MaxX, float(Vehicle->GetActorLocation().X)); MaxY = FMath::Max(MaxY, float(Other->GetActorLocation().Y));
    }
    TestTrue(TEXT("Both vehicles clear the junction"), MaxX > 9500.f && MaxY > 500.f);
    TestTrue(TEXT("Junction order prevents overlap"), Closest > 2.f * 60.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingRouteNetworkTest, "RuneSim.LivingWorld.RouteNetwork", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingRouteNetworkTest::RunTest(const FString& Parameters)
{
    FTestWorld Test;
    Test.Box(FVector(0, 0, -10), FVector(20000, 20000, 10));
    ALivingRoute* Lane = Test.Route(FVector(0, 0, 0), FVector(5000, 0, 0), false, 150);
    ALivingRoute* Branch = Test.Route(FVector(5000, 0, 0), FVector(5000, 6000, 0), false, 150);
    ALivingRoute* Cross = Test.Route(FVector(-2000, 3000, 0), FVector(8000, 3000, 0), false, 150);
    ALivingRoute* Road = Test.Route(FVector(0, 100, 0), FVector(5000, 100, 0), true, 150);
    Lane->bRetireAtEnds = true;
    const int32 Links = LivingWorld::LinkRoutes({Lane, Branch, Cross, Road});
    TestTrue(TEXT("End-to-end link found"), Lane->EndLinks.ContainsByPredicate([&](const FLivingRouteLink& L) { return L.Route.Get() == Branch && L.Direction == 1; }));
    TestTrue(TEXT("T-junction links both ways"), Branch->EndLinks.Num() == 0 &&
        Cross->StartLinks.Num() == 0 && LivingWorld::LinkRoutes({Lane, Branch, Cross}) >= 2);
    TestFalse(TEXT("Walkways never link onto roads"), Lane->EndLinks.ContainsByPredicate([&](const FLivingRouteLink& L) { return L.Route.Get() == Road; }));
    TestTrue(TEXT("Links counted"), Links >= 2);
    ULivingAssetProfile* Person = NewObject<ULivingAssetProfile>();
    Person->Kind = ELivingKind::Civilian; Person->SpeedMetersPerSecond = 2.f; Person->CollisionRadiusCm = 30; Person->GroundClearanceCm = 100;
    ALivingAgent* Walker = Test.World->SpawnActor<ALivingAgent>(FVector(4000, 0, 100), FRotator::ZeroRotator);
    Walker->Activate(Person, Lane, 4000, FVector::ZeroVector, 30, 0);
    FLivingWorldOptions Options;
    for (int32 I = 0; I < 30 * 12; ++I) Walker->Step(1.f / 30.f, Options, {Walker}, nullptr);
    TestTrue(TEXT("Walker continues onto the linked route instead of retiring"), Walker->bActive && Walker->GetRoute() == Branch);
    TestTrue(TEXT("Walker progresses along the new route"), Walker->GetActorLocation().Y > 1000.f);
    Branch->bValidated = false;
    Walker->SetActorLocation(FVector(4000, 0, 100)); Walker->Activate(Person, Lane, 4000, FVector::ZeroVector, 31, 0);
    for (int32 I = 0; I < 30 * 12 && Walker->bActive; ++I) Walker->Step(1.f / 30.f, Options, {Walker}, nullptr);
    TestFalse(TEXT("Without a validated link the walker retires at the exit"), Walker->bActive);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimEventReplayTest, "RuneSim.LivingWorld.EventReplay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimEventReplayTest::RunTest(const FString& Parameters)
{
    FTestWorld Test;
    const FString Name = TEXT("Automation_Events_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LivingWorld/Recordings"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory, Name + TEXT(".jsonl"));
    const FString Data =
        TEXT("{\"simulation_time\":10,\"actors\":[{\"id\":\"a\",\"position_cm\":\"X=0 Y=0 Z=0\",\"rotation_deg\":\"P=0 Y=0 R=0\"}]}\n")
        TEXT("{\"simulation_time\":11,\"actors\":[{\"id\":\"a\",\"position_cm\":\"X=0 Y=0 Z=0\",\"rotation_deg\":\"P=0 Y=0 R=0\"}],\"events\":[{\"time\":10.5,\"type\":\"launch\",\"location_cm\":\"X=1 Y=2 Z=3\",\"subject\":\"m\"},{\"time\":10.6,\"type\":\"designate\",\"location_cm\":\"X=0 Y=0 Z=0\"}]}\n")
        TEXT("{\"simulation_time\":12,\"actors\":[{\"id\":\"a\",\"position_cm\":\"X=0 Y=0 Z=0\",\"rotation_deg\":\"P=0 Y=0 R=0\"}],\"events\":[{\"time\":11.5,\"type\":\"detonation\",\"location_cm\":\"X=9 Y=9 Z=9\"}]}\n");
    FFileHelper::SaveStringToFile(Data, *Path);
    ASimReplay* Replay = Test.World->SpawnActor<ASimReplay>();
    Replay->bReplayEffects = false;
    TestTrue(TEXT("Recording with events loads"), Replay->LoadRecording(Name));
    TestEqual(TEXT("All events parsed"), Replay->EventCount, 3);
    Replay->PlayReplay(); Replay->Tick(.75f);
    TestEqual(TEXT("Launch effect reproduced at its time"), Replay->EffectsPlayed, 1);
    Replay->Tick(1.f);
    TestEqual(TEXT("Detonation reproduced; command events are not"), Replay->EffectsPlayed, 2);
    Replay->Seek(0.f);
    TestEqual(TEXT("Seeking does not replay effects"), Replay->EffectsPlayed, 2);
    Replay->bLoop = true; Replay->PlayReplay(); Replay->Tick(1.9f); Replay->Tick(.2f);
    TestEqual(TEXT("Looping replays each effect once per pass"), Replay->EffectsPlayed, 4);
    FFileHelper::SaveStringToFile(Data.Replace(TEXT("X=9 Y=9 Z=9"), TEXT("X=nan Y=0 Z=0")), *Path);
    TestFalse(TEXT("Invalid event location rejected"), Replay->LoadRecording(Name));
    TestEqual(TEXT("Rejected load keeps the current events"), Replay->EventCount, 3);
    IFileManager::Get().Delete(*Path);

    // Recorder captures events and interceptors launched after recording started.
    AActor* Subject = Test.Target(FVector(100, 0, 0));
    ASimScenarioRecorder* Recorder = Test.World->SpawnActor<ASimScenarioRecorder>();
    Recorder->Subjects.Add(Subject);
    Recorder->StartRecording();
    Recorder->Tick(.2f);
    Test.World->SpawnActor<ASimProjectile>(FVector(0, 0, 500), FRotator::ZeroRotator);
    SimEvents::Record(Test.World, TEXT("launch"), FVector(0, 0, 500), TEXT("m"));
    Recorder->Tick(.2f);
    SimEvents::Record(Test.World, TEXT("detonation"), FVector(5, 0, 500), TEXT("m"));
    const FString Saved = TEXT("Automation_Recorder_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    TestTrue(TEXT("Recording saved"), Recorder->StopAndSave(Saved));
    FString Text;
    FFileHelper::LoadFileToString(Text, *FPaths::Combine(Directory, Saved + TEXT(".jsonl")));
    TestTrue(TEXT("Events written"), Text.Contains(TEXT("\"type\":\"launch\"")) && Text.Contains(TEXT("\"type\":\"detonation\"")));
    TestTrue(TEXT("Interceptor sampled"), Text.Contains(TEXT("SimProjectile")));
    TestTrue(TEXT("Recorder output replays"), Replay->LoadRecording(Saved) && Replay->EventCount == 2);
    IFileManager::Get().Delete(*FPaths::Combine(Directory, Saved + TEXT(".jsonl")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimSensorCameraTest, "RuneSim.LivingWorld.SensorCameras", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimSensorCameraTest::RunTest(const FString& Parameters)
{
    FTestWorld Test;
    const FTransform Start;
    ASimFollowCamera* Camera = Test.World->SpawnActorDeferred<ASimFollowCamera>(ASimFollowCamera::StaticClass(), Start);
    TestTrue(TEXT("Stable stream ID configured"), Camera->Configure(TEXT("air-9"), 70.f));
    TestFalse(TEXT("Empty stream ID rejected"), Camera->Configure(TEXT(""), 70.f));
    Camera->FinishSpawning(Start);
    TestEqual(TEXT("Stream ID applied"), Camera->Stream->StreamId, FString(TEXT("air-9")));
    TestTrue(TEXT("Secondary feed renders only when watched"), Camera->Stream->bCaptureOnlyWhenViewed);
    AActor* Carrier = Test.Target(FVector(100, 0, 500));
    Carrier->SetActorRotation(FRotator(15, 90, 30));
    Camera->MountOffset = FVector(50, 0, -20);
    Camera->SetCarrier(Carrier, true, -25.f);
    TestTrue(TEXT("Mounted in the carrier frame"), Camera->GetActorLocation().Equals(Carrier->GetActorTransform().TransformPosition(FVector(50, 0, -20)), .01));
    TestTrue(TEXT("Gimbal levels roll and holds depression"), FMath::IsNearlyEqual(Camera->GetActorRotation().Roll, 0.f, .01f) &&
        FMath::IsNearlyEqual(Camera->GetActorRotation().Pitch, -25.f, .01f) && FMath::IsNearlyEqual(Camera->GetActorRotation().Yaw, 90.f, .01f));
    TestTrue(TEXT("Carrier hidden from its own camera"), Camera->Capture->HiddenActors.Contains(Carrier));
    Carrier->SetActorLocation(FVector(1000, 0, 500));
    Camera->Tick(1.f / 30.f);
    TestTrue(TEXT("Follows the moving carrier"), FVector::Dist(Camera->GetActorLocation(), Carrier->GetActorLocation()) < 100.f);
    Camera->SetCarrier(Carrier, false, 0.f);
    TestTrue(TEXT("Seeker mount keeps the carrier attitude"), Camera->GetActorRotation().Equals(Carrier->GetActorRotation(), .01f));
    ASimPTZ* PTZ = Test.World->SpawnActor<ASimPTZ>();
    Camera->SetCarrier(nullptr, true, -20.f);
    Camera->SetIdleView(PTZ->Camera);
    TestTrue(TEXT("Idle feed mirrors the launching tripod"), Camera->GetActorLocation().Equals(PTZ->Camera->GetComponentLocation(), .01));
    USceneCaptureComponent2D* Seeker = Camera->Capture;
    PTZ->Stream->SetCaptureOverride(Seeker);
    TestTrue(TEXT("Stream renders the override"), PTZ->Stream->GetActiveCapture() == Seeker);
    PTZ->Stream->SetCaptureOverride(nullptr);
    TestTrue(TEXT("Override cleared"), PTZ->Stream->GetActiveCapture() == PTZ->Capture.Get());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingWarEffectsTest, "RuneSim.LivingWorld.WarEffects", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingWarEffectsTest::RunTest(const FString& Parameters)
{
    // Apparent size: a 6 m fire 100 m ahead fills a tenth of a 33-degree view; behind the camera it is invisible.
    const FSimEffectViewer Wide{FVector::ZeroVector, FVector::ForwardVector, 90.f};
    const FSimEffectViewer Zoomed{FVector::ZeroVector, FVector::ForwardVector, 2.f};
    TestTrue(TEXT("Nearby fire is large in a wide view"), SimWarEffects::ApparentFraction(Wide, FVector(5000, 0, 0), 600) > .05f);
    TestTrue(TEXT("Distant fire is small in a wide view"), SimWarEffects::ApparentFraction(Wide, FVector(200000, 0, 0), 600) < .01f);
    TestTrue(TEXT("A zoomed tripod sees a 2 km fire large"), SimWarEffects::ApparentFraction(Zoomed, FVector(200000, 0, 0), 600) > .06f);
    TestEqual(TEXT("A fire behind the camera is not seen"), SimWarEffects::ApparentFraction(Wide, FVector(-5000, 0, 0), 600), 0.f);
    TestEqual(TEXT("A fire outside a zoomed view is not seen"), SimWarEffects::ApparentFraction(Zoomed, FVector(5000, 2000, 0), 600), 0.f);
    // Selection: budget, threshold, largest first, hysteresis for fires already simulated.
    TestTrue(TEXT("Budget limits fluid fires"), SimWarEffects::SelectFluid({.5f, .3f, .2f}, {false, false, false}, 2, .06f) == TArray<int32>({0, 1}));
    TestTrue(TEXT("Small fires stay sprites"), SimWarEffects::SelectFluid({.01f, .5f}, {false, false}, 2, .06f) == TArray<int32>({1}));
    TestTrue(TEXT("A simulated fire keeps its fluid just below the threshold"), SimWarEffects::SelectFluid({.05f}, {true}, 2, .06f) == TArray<int32>({0}));
    TestEqual(TEXT("A new fire needs the full threshold"), SimWarEffects::SelectFluid({.05f}, {false}, 2, .06f).Num(), 0);
    TestEqual(TEXT("Zero budget disables fluids"), SimWarEffects::SelectFluid({.9f}, {false}, 0, .06f).Num(), 0);

    FTestWorld Test;
    USimWarEffects* War = Test.World->GetSubsystem<USimWarEffects>();
    if (!TestNotNull(TEXT("War effects exist in game worlds"), War)) return false;
    // The project config ships with fluids off (sim.fx.MaxFluidFires=0); this test exercises the selection.
    IConsoleVariable* Budget = IConsoleManager::Get().FindConsoleVariable(TEXT("sim.fx.MaxFluidFires"));
    const int32 PreviousBudget = Budget ? Budget->GetInt() : 0;
    if (Budget) Budget->Set(2, ECVF_SetByCode);
    ON_SCOPE_EXIT { if (Budget) Budget->Set(PreviousBudget, ECVF_SetByCode); };
    War->ViewerOverride = {FSimEffectViewer{FVector::ZeroVector, FVector::ForwardVector, 60.f}};
    const int32 Near = War->StartFire(FVector(3000, 0, 0), 1.f, 10.f);
    const int32 Far = War->StartFire(FVector(300000, 0, 0), 1.f, 10.f);
    const int32 Behind = War->StartFire(FVector(-3000, 0, 0), 1.f, 0.f);
    War->Tick(.1f);
    TestTrue(TEXT("The close, visible fire is simulated as a fluid"), War->IsFireFluid(Near));
    TestFalse(TEXT("The distant fire stays sprites"), War->IsFireFluid(Far));
    TestFalse(TEXT("A fire behind every camera stays sprites"), War->IsFireFluid(Behind));
    // Turning the camera around moves the fluid detail to the other fire.
    War->ViewerOverride = {FSimEffectViewer{FVector::ZeroVector, -FVector::ForwardVector, 60.f}};
    War->Tick(.6f);
    TestFalse(TEXT("Fluid detail leaves the fire the camera turned away from"), War->IsFireFluid(Near));
    TestTrue(TEXT("Fluid detail follows the view"), War->IsFireFluid(Behind));
    War->Tick(10.f);
    TestEqual(TEXT("Timed fires expire; an open-ended fire keeps burning"), War->ActiveFireCount(), 1);
    War->SmokeScreen(FVector::ZeroVector, 1.f, 5.f);
    War->Tick(.6f);
    TestEqual(TEXT("A smoke screen never takes the fluid budget"), War->ActiveFluidFireCount(), 1);
    War->StopAllFires();
    TestEqual(TEXT("All fires stop"), War->ActiveFireCount(), 0);
    // Burning wreck: a disabled ground vehicle carries a fire that ends when it is recycled.
    const int32 Attached = War->StartFire(FVector(100, 0, 0), 1.f, 0.f, nullptr);
    TestTrue(TEXT("Fire ids are unique"), Attached > Behind);
    War->StopFire(Attached);
    TestTrue(TEXT("Crashes and strikes are replayable"), SimEvents::IsReplayable(TEXT("strike")) && SimEvents::IsReplayable(TEXT("smoke_screen")) && SimEvents::IsReplayable(TEXT("fire")));
    return true;
}

#endif
