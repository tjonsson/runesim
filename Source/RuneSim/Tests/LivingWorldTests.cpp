#include "Misc/AutomationTest.h"
#include "../LivingWorldTypes.h"
#include <limits>
#include "../SimPTZ.h"
#include "../SimScenario.h"
#include "../SimReplay.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Components/MeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "../LivingAgent.h"
#include "../LivingRoute.h"
#include "Components/BoxComponent.h"
#include "Components/SplineComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "../LivingGeography.h"
#include "../LivingTerrainBudgetComponent.h"
#include "../LivingHumanAnimation.h"
#include "CesiumGeoreference.h"
#include "CesiumGlobeAnchorComponent.h"
#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingHumanFeetTest, "RuneSim.LivingWorld.HumanFeet", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingHumanFeetTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Planted foot keeps full terrain correction"), LivingWorld::FootContactWeight(9,9), 1.f);
    TestEqual(TEXT("Lifted foot preserves swing"), LivingWorld::FootContactWeight(27,9), 0.f);
    TestEqual(TEXT("Invalid pose cannot acquire contact"), LivingWorld::FootContactWeight(std::numeric_limits<float>::quiet_NaN(),9), 0.f);
    FLivingFootPlant Plant;
    const FVector FootPoint(8,0,10);
    LivingWorld::UpdateFootPlant(Plant,FTransform::Identity,FootPoint,1,true,1.f/30);
    const FTransform Moved(FQuat::Identity,FVector(10,0,0));
    const FVector Locked = LivingWorld::UpdateFootPlant(Plant,Moved,FootPoint,1,true,1.f/30);
    TestTrue(TEXT("Planted foot stays at its world point as the body translates"),Moved.TransformPosition(FootPoint+Locked).Equals(FootPoint,.001));
    TestTrue(TEXT("Swing releases the planted foot"),LivingWorld::UpdateFootPlant(Plant,Moved,FootPoint,.5f,true,1.f/30).IsZero());
    TestTrue(TEXT("Next footfall acquires a fresh anchor"),LivingWorld::UpdateFootPlant(Plant,Moved,FootPoint,1,true,1.f/30).IsZero() && Plant.bPlanted);
    const FTransform Turned(FRotator(0,30,0),FVector(10,0,0));
    TestTrue(TEXT("Sharp turn releases before tethering the leg"),LivingWorld::UpdateFootPlant(Plant,Turned,FootPoint,1,true,1.f/30).IsZero() && !Plant.bPlanted);
    TestFalse(TEXT("Released stance cannot reacquire before swing"),Plant.bCanPlant);
    LivingWorld::UpdateFootPlant(Plant,Moved,FootPoint,1,false,1.f/30);
    TestTrue(TEXT("Lost terrain clears plant history"),!Plant.bPlanted && Plant.bCanPlant);
    LivingWorld::UpdateFootPlant(Plant,FTransform::Identity,FootPoint,1,true,1.f/30);
    for (int32 I=1; I<=35; ++I)
    {
        const FVector Offset = LivingWorld::UpdateFootPlant(Plant,FTransform(FQuat::Identity,FVector(I,0,0)),FootPoint,1,true,.01f);
        TestTrue(TEXT("Reach fade remains within eighteen cm"),Offset.Size()<=18.001);
    }
    TestFalse(TEXT("Excess reach releases anchor"),Plant.bPlanted);
    Plant = {};
    for (int32 I=0; I<30; ++I) LivingWorld::UpdateFootPlant(Plant,FTransform::Identity,FootPoint,1,true,1.f/30);
    TestFalse(TEXT("Idle plant expires without repeated reacquisition"),Plant.bPlanted || Plant.bCanPlant);
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto MakeBox = [World](FVector Position, FVector Extent)
    {
        AActor* Actor = World->SpawnActor<AActor>(); auto* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box); Actor->AddInstanceComponent(Box); Box->SetBoxExtent(Extent);
        Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Actor->SetActorLocation(Position); return Actor;
    };
    AActor* Floor = MakeBox(FVector(0,0,-10), FVector(20000,20000,10));
    auto* Route = World->SpawnActor<ALivingRoute>();
    Route->Path->SetSplinePoints({FVector(0,0,0),FVector(10000,0,0)}, ESplineCoordinateSpace::World);
    Route->bValidated = true; Route->ReviewedHalfWidthCm = 200;
    for (const FString Kind : {FString(TEXT("Civilian")),FString(TEXT("Soldier"))})
    {
        auto* Profile = LoadObject<ULivingAssetProfile>(nullptr, *(TEXT("/Game/LivingWorld/Profiles/DA_")+Kind+TEXT(".DA_")+Kind));
        if (!TestNotNull(TEXT("Human profile available"), Profile)) continue;
        auto* Agent = World->SpawnActor<ALivingAgent>(); Agent->SetActorLocation(FVector(1000,0,90));
        Agent->Activate(Profile,Route,1000,FVector::ZeroVector,1,0);
        auto* Human = Cast<ULivingHumanAnimation>(Agent->AnimatedVisual->GetAnimInstance());
        if (!TestNotNull(TEXT("Human uses terrain animation proxy"), Human)) { Agent->Destroy(); continue; }
        if (!TestEqual(TEXT("Both verified foot chains resolved"), Human->FootSupports.Num(), 2)) { Agent->Destroy(); continue; }
        Human->bReplaySupports = true; Human->SetAnimationAsset(Profile->IdleAnimation.LoadSynchronous(),true);
        Human->SetPlaying(false); Human->SetPosition(0,false);
        auto Pose = [&]() { Human->SetPlaying(false); Human->SetPosition(0,false); Agent->AnimatedVisual->TickAnimation(1.f/30,false); Agent->AnimatedVisual->RefreshBoneTransforms(); };
        const FName Foot = Human->FootSupports[0].Bone;
        const FName Calf = Agent->AnimatedVisual->GetParentBone(Foot), Thigh = Agent->AnimatedVisual->GetParentBone(Calf);
        auto Bone = [&](FName Name) { return Agent->AnimatedVisual->GetSocketTransform(Name,RTS_Component); };
        Pose(); Pose();
        const FTransform Before = Bone(Foot), ActorBefore = Agent->GetActorTransform();
        const double Upper = FVector::Distance(Bone(Thigh).GetLocation(),Bone(Calf).GetLocation());
        const double Lower = FVector::Distance(Bone(Calf).GetLocation(),Bone(Foot).GetLocation());
        Human->FootSupports[0].HeightCm = 10; Pose();
        TestTrue(TEXT("Raised surface actually lifts weighted foot bone"), Bone(Foot).GetLocation().Z > Before.GetLocation().Z+7);
        TestTrue(TEXT("Upper leg does not stretch"), FMath::IsNearlyEqual(Upper,FVector::Distance(Bone(Thigh).GetLocation(),Bone(Calf).GetLocation()),.05));
        TestTrue(TEXT("Lower leg does not stretch"), FMath::IsNearlyEqual(Lower,FVector::Distance(Bone(Calf).GetLocation(),Bone(Foot).GetLocation()),.05));
        for (auto& Support : Human->FootSupports) Support.HeightCm = -10;
        Pose();
        TestTrue(TEXT("Pelvis allows the foot to reach lower ground"), Bone(Foot).GetLocation().Z < Before.GetLocation().Z-7);
        TestTrue(TEXT("Visual IK never moves the collision actor"), Agent->GetActorTransform().Equals(ActorBefore));
        for (auto& Support : Human->FootSupports) Support.HeightCm = 0;
        Pose();
        // Compare against the unmodified pose from the very same evaluation.
        TestTrue(TEXT("Removing terrain correction restores the current authored foot position"),
            Human->BaseFeet.Num()==2 && Bone(Foot).GetLocation().Equals(Human->BaseFeet[0],.02));
        Human->bReplaySupports = false; Pose(); Pose();
        TestEqual(TEXT("Flat reviewed terrain supports both feet"), Human->SupportedFeet,2);
        // The real imported meshes must consume the recorded planar correction too.
        Human->bReplaySupports = true; Human->FootSupports[0].StanceOffsetCm = FVector(8,0,0); Pose();
        TestTrue(TEXT("Replay stance offset changes the actual foot bone"),
            FMath::Abs((Bone(Foot).GetLocation()-Human->BaseFeet[0]).X-8)<.1);
        Human->FootSupports[0].StanceOffsetCm = FVector::ZeroVector;
        Human->bReplaySupports = false; Human->ResetGrounding(); Pose(); Pose();
        const FVector PlantedWorld = Agent->AnimatedVisual->GetSocketLocation(Foot);
        Agent->AddActorWorldOffset(Agent->AnimatedVisual->GetComponentTransform().TransformVectorNoScale(FVector(8,0,0)));
        Pose();
        TestTrue(TEXT("Actual imported foot stays planted under body movement"),
            FVector::Dist2D(PlantedWorld,Agent->AnimatedVisual->GetSocketLocation(Foot)) < .75);
        Agent->SetActorTransform(ActorBefore); Human->ResetGrounding(); Pose(); Pose();
        const FVector Pivot = Agent->AnimatedVisual->GetSocketLocation(Foot);
        AActor* Bump = MakeBox(FVector(Pivot.X,Pivot.Y,4),FVector(9,9,4));
        for(int32 I=0;I<15;++I) Pose();
        TestTrue(TEXT("Live terrain trace supplies an eight cm step"), Human->FootSupports[0].HeightCm > 7);
        Bump->Tags.Add(TEXT("Water")); Pose();
        TestEqual(TEXT("Water contact releases correction immediately"), Human->FootSupports[0].HeightCm,0.f);
        Bump->Destroy(); Floor->SetActorEnableCollision(false); Pose();
        TestEqual(TEXT("Missing terrain cannot retain support"),Human->SupportedFeet,0);
        Floor->SetActorEnableCollision(true); Route->bValidated=false; Pose();
        TestEqual(TEXT("Withdrawn route cannot retain support"),Human->SupportedFeet,0);
        Route->bValidated=true;
        FVector Contact;
        TestFalse(TEXT("Foot keeps its reviewed corridor margin"),Agent->SampleFootGround(FVector(1000,198,0),Contact));
        TestTrue(TEXT("Foot within reviewed margin remains supported"),Agent->SampleFootGround(FVector(1000,190,0),Contact));
        Human->FootSupports[0].HeightCm=12; Agent->Deactivate(); Agent->Activate(Profile,Route,1000,FVector::ZeroVector,2,0);
        TestEqual(TEXT("Pooled humans reset terrain offsets"),Human->FootSupports[0].HeightCm,0.f);
        TestTrue(TEXT("Pooled humans reset stance offsets"),Human->FootSupports[0].StanceOffsetCm.IsZero());
        Agent->Destroy();
    }
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingViewCadenceTest, "RuneSim.LivingWorld.ViewCadence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingViewCadenceTest::RunTest(const FString& Parameters)
{
    FLivingViewCadence Policy;
    TArray<FCesiumCamera> Views = {FCesiumCamera(FVector2D(1280,720),FVector::ZeroVector,FRotator::ZeroRotator,85)};
    TestFalse(TEXT("New camera requests full-rate selection"),Policy.Update(Views,.1f));
    for (int32 I=0; I<6; ++I) Policy.Update(Views,.1f);
    TestTrue(TEXT("Stationary view settles"),Policy.Update(Views,.01f));
    // Small per-frame motion must accumulate against the last accepted view.
    for (int32 I=1; I<=5; ++I) { Views[0].Location.X = I*.25; Policy.Update(Views,.01f); }
    TestTrue(TEXT("Slow accumulated movement wakes selection"),Policy.StillSeconds < .02f);
    for (int32 I=0; I<6; ++I) Policy.Update(Views,.1f);
    Views[0].Rotation.Yaw = 2;
    TestFalse(TEXT("Pan wakes selection"),Policy.Update(Views,.01f));
    for (int32 I=0; I<6; ++I) Policy.Update(Views,.1f);
    Views[0].FieldOfViewDegrees = 5;
    TestFalse(TEXT("Zoom wakes selection"),Policy.Update(Views,.01f));
    for (int32 I=0; I<6; ++I) Policy.Update(Views,.1f);
    Views[0].ViewportSize.X = 1920;
    TestFalse(TEXT("Resolution change wakes selection"),Policy.Update(Views,.01f));
    for (int32 I=0; I<6; ++I) Policy.Update(Views,.1f);
    const FCesiumCamera SecondSensor = Views[0]; Views.Add(SecondSensor);
    TestFalse(TEXT("Additional sensor wakes selection"),Policy.Update(Views,.01f));
    Views.Reset();
    TestFalse(TEXT("Unknown view never enters idle cadence"),Policy.Update(Views,1.f));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingSlopeLaneTest, "RuneSim.LivingWorld.SlopedCorridor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingSlopeLaneTest::RunTest(const FString& Parameters)
{
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    AActor* Floor = World->SpawnActor<AActor>(); UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(Box); Floor->AddInstanceComponent(Box); Box->SetBoxExtent(FVector(20000,20000,10));
    Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent();
    Floor->SetActorRotation(FRotator(0,0,15)); Floor->SetActorLocation(-Floor->GetActorUpVector()*10);
    ALivingRoute* Route = World->SpawnActor<ALivingRoute>();
    Route->Path->SetSplinePoints({FVector(0,0,0),FVector(10000,0,0)}, ESplineCoordinateSpace::World);
    Route->bValidated = true; Route->ReviewedHalfWidthCm = 65;
    auto* Profile = NewObject<ULivingAssetProfile>(World); Profile->Kind = ELivingKind::Civilian;
    Profile->CollisionRadiusCm = 30; Profile->GroundClearanceCm = 90; Profile->SpeedMetersPerSecond = 1.4f;
    FVector Position, Up; TestTrue(TEXT("Sloped narrow corridor supports footprint"), Route->SampleGround(0, Position, Up, 0, 30));
    ALivingAgent* Agent = World->SpawnActor<ALivingAgent>(Position + Up*90, FRotator::ZeroRotator);
    Agent->Activate(Profile, Route, 0, FVector::ZeroVector, 1, 0);
    FLivingWorldOptions Options; const TArray<ALivingAgent*> Agents = {Agent};
    for (int32 I=0; I<300; ++I) Agent->Step(1.f/30, Options, Agents, nullptr);
    TestTrue(TEXT("Normal clearance does not accumulate as lateral lane drift"), Agent->GetActorLocation().X > 1000 && Agent->Behavior != ELivingBehavior::Blocked);
    TestTrue(TEXT("Foot placement remains on the centreline despite raised root"), FMath::Abs((Agent->GetActorLocation() - Up*90).Y) < 1);
    Route->bRetireAtEnds = true;
    Route->SampleGround(9980, Position, Up);
    Agent->SetActorLocation(Position + Up*90); Agent->Activate(Profile, Route, 9980, FVector::ZeroVector, 2, 0);
    Agent->Step(1.f/30, Options, Agents, nullptr);
    TestFalse(TEXT("Open exit recycles before the footprint leaves reviewed terrain"), Agent->bActive);
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingCesiumFlightTest, "RuneSim.LivingWorld.CesiumFlight", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingCesiumFlightTest::RunTest(const FString& Parameters)
{
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ACesiumGeoreference* Geo = ACesiumGeoreference::GetDefaultGeoreference(World);
    AActor* Floor = World->SpawnActor<AActor>(); UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(Box); Floor->AddInstanceComponent(Box); Box->SetBoxExtent(FVector(100000,100000,10));
    Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-10));
    auto* Anchor = NewObject<UCesiumGlobeAnchorComponent>(Floor); Floor->AddInstanceComponent(Anchor); Anchor->RegisterComponent();
    auto* Profile = NewObject<ULivingAssetProfile>(World);
    Profile->Kind = ELivingKind::Drone; Profile->SpeedMetersPerSecond = 8; Profile->TurnRateDegrees = 90;
    FLivingWorldOptions Options; Options.ActivityRadiusMeters = 100;
    const float Radius = LivingWorld::FlightRadius(*Profile, 100);
    const FVector Home(0,0,3000);
    const FVector HomeEcef = Geo->TransformUnrealPositionToEarthCenteredEarthFixed(Home);
    ALivingAgent* Agent = World->SpawnActor<ALivingAgent>(Home + FVector(Radius,0,0), FRotator(0,90,0));
    Agent->Activate(Profile, nullptr, 0, Home, 1, 0, true);
    const TArray<ALivingAgent*> Agents = {Agent};
    for (int32 I=0; I<90; ++I) Agent->Step(1.f/30, Options, Agents, nullptr);
    TestTrue(TEXT("Cesium flight moves above loaded collision"), Agent->Velocity.Size() > 700 && Agent->GetActorLocation().Z > 2000);
    Floor->SetActorEnableCollision(false);
    const FVector Held = Agent->GetActorLocation();
    Agent->Step(1.f/30, Options, Agents, nullptr);
    TestTrue(TEXT("Unavailable terrain holds flight"), Agent->Behavior == ELivingBehavior::Blocked && Agent->GetActorLocation().Equals(Held, .01));
    Floor->SetActorEnableCollision(true);
    Geo->SetOriginLongitudeLatitudeHeight(FVector(45,36,3330));
    for (int32 I=0; I<90; ++I) Agent->Step(1.f/30, Options, Agents, nullptr);
    const FVector NewHome = Geo->TransformEarthCenteredEarthFixedPositionToUnreal(HomeEcef);
    const FQuat Frame = LivingGeography::Frame(Geo, NewHome);
    const FVector Relative = Frame.UnrotateVector(Agent->GetActorLocation() - NewHome);
    TestTrue(TEXT("Origin change preserves geographic flight home and altitude"), FMath::Abs(Relative.Z) < 500 && Relative.Size2D() < Radius * 1.5);
    TestTrue(TEXT("Flight resumes after terrain returns in rotated world frame"), Agent->Behavior != ELivingBehavior::Blocked && Agent->Velocity.Size() > 700);
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingSteeringTest, "RuneSim.LivingWorld.VehicleSteering", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingSteeringTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("Straight wheels"), LivingWorld::WheelSteering(0, 250, 80, 35), 0.f);
    const float Inner = LivingWorld::WheelSteering(.002f, 250, 80, 35);
    const float Outer = LivingWorld::WheelSteering(.002f, 250, -80, 35);
    TestTrue(TEXT("Inside wheel steers farther for the same turning centre"), Inner > Outer && Outer > 0);
    TestTrue(TEXT("Mirrored turn swaps inner wheels and sign"), FMath::IsNearlyEqual(LivingWorld::WheelSteering(-.002f, 250, -80, 35), -Inner));
    TestEqual(TEXT("Steering bounded at a very sharp corner"), LivingWorld::WheelSteering(1, 250, 80, 35), 35.f);
    TestEqual(TEXT("Invalid curvature cannot enter the pose"), LivingWorld::WheelSteering(std::numeric_limits<float>::quiet_NaN(), 250, 80, 35), 0.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingVehicleContactTest, "RuneSim.LivingWorld.VehicleContacts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingVehicleContactTest::RunTest(const FString& Parameters)
{
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    auto MakeBox = [World](FVector Position, FVector Extent)
    {
        AActor* Actor = World->SpawnActor<AActor>(); UBoxComponent* Box = NewObject<UBoxComponent>(Actor);
        Actor->SetRootComponent(Box); Actor->AddInstanceComponent(Box); Box->SetBoxExtent(Extent);
        Box->SetCollisionProfileName(TEXT("BlockAll")); Box->RegisterComponent(); Actor->SetActorLocation(Position);
        return Actor;
    };
    AActor* Floor = MakeBox(FVector(0,0,-10), FVector(20000,20000,10));
    ALivingRoute* Route = World->SpawnActor<ALivingRoute>();
    Route->Path->SetSplinePoints({FVector(0,0,0),FVector(10000,0,0)}, ESplineCoordinateSpace::World);
    Route->bValidated = Route->bVehicles = true; Route->ReviewedHalfWidthCm = 400;
    auto* Source = LoadObject<ULivingAssetProfile>(nullptr, TEXT("/Game/LivingWorld/Profiles/DA_Jeep.DA_Jeep"));
    if (!TestNotNull(TEXT("Reviewed jeep available"), Source)) { GEngine->DestroyWorldContext(World); World->DestroyWorld(false); return false; }
    auto* Profile = DuplicateObject<ULivingAssetProfile>(Source, World);
    Profile->SpeedMetersPerSecond = 0; Profile->Wheels.Empty();
    for (const FName Bone : {FName(TEXT("wheel_LF")), FName(TEXT("wheel_RF")), FName(TEXT("wheel_LR")), FName(TEXT("wheel_RR"))})
    {
        FLivingWheelDefinition Wheel; Wheel.Bone = Bone; Wheel.RadiusCm = 50.937142f;
        Wheel.bSteers = Bone.ToString().EndsWith(TEXT("F")); Profile->Wheels.Add(Wheel);
    }
    ALivingAgent* Agent = World->SpawnActor<ALivingAgent>(); Agent->SetActorLocation(FVector(0,0,110));
    Agent->Activate(Profile, Route, 0, FVector::ZeroVector, 1, 0);
    FLivingWorldOptions Options; const TArray<ALivingAgent*> Agents = {Agent};
    Agent->Step(1.f/30, Options, Agents, nullptr);
    TestTrue(TEXT("Flat ground supports all four wheels"), Agent->Behavior != ELivingBehavior::Blocked && Agent->WheelPoses.Num() == 4);
    auto* Animation = Cast<ULivingVehicleAnimation>(Agent->AnimatedVisual->GetAnimInstance());
    if (TestNotNull(TEXT("Vehicle uses procedural animation proxy"), Animation) && Agent->WheelPoses.Num() == 4)
    {
        auto Pose = [&]() { Agent->AnimatedVisual->TickAnimation(.01f, false); Agent->AnimatedVisual->RefreshBoneTransforms(); };
        Animation->SetPlaying(false); Animation->SetPosition(0, false); Pose();
        const FTransform Before = Agent->AnimatedVisual->GetSocketTransform(TEXT("wheel_LF"), RTS_Component);
        const FTransform RearBefore = Agent->AnimatedVisual->GetSocketTransform(TEXT("wheel_LR"), RTS_Component);
        Animation->WheelPoses[0].SteeringDegrees = 20; Animation->WheelPoses[0].Offset = FVector(0,0,10); Pose();
        const FTransform After = Agent->AnimatedVisual->GetSocketTransform(TEXT("wheel_LF"), RTS_Component);
        TestTrue(TEXT("Suspension changes actual wheel bone position"), FMath::IsNearlyEqual(After.GetLocation().Z-Before.GetLocation().Z, 10., .15));
        TestTrue(TEXT("Steering changes actual wheel bone orientation"), FMath::IsNearlyEqual(FMath::RadiansToDegrees(After.GetRotation().AngularDistance(Before.GetRotation())), 20., .15));
        TestTrue(TEXT("Front wheel adjustment does not deform rear wheel"), Agent->AnimatedVisual->GetSocketTransform(TEXT("wheel_LR"), RTS_Component).Equals(RearBefore, .01));
        Animation->WheelPoses = Agent->WheelPoses; Pose();
        const FVector Pivot = Agent->AnimatedVisual->GetSocketLocation(TEXT("wheel_LF"));
        AActor* Bump = MakeBox(FVector(Pivot.X, Pivot.Y, 4), FVector(20,20,4));
        Agent->Step(1.f/30, Options, Agents, nullptr);
        TestTrue(TEXT("Raised supported contact compresses suspension after chassis fit"), Agent->Behavior != ELivingBehavior::Blocked && Agent->WheelPoses[0].Offset.Z > 1);
        Pose();
        const FVector RaisedContact = Agent->AnimatedVisual->GetSocketLocation(TEXT("wheel_LF")) - Agent->GetActorUpVector()*Profile->Wheels[0].RadiusCm;
        TestTrue(TEXT("Fitted chassis still places the raised wheel on its surface"), FMath::IsNearlyEqual(RaisedContact.Z, 8., .5));
        Bump->Tags.Add(TEXT("Water")); Agent->Step(1.f/30, Options, Agents, nullptr);
        TestTrue(TEXT("Water under a wheel blocks vehicle"), Agent->Behavior == ELivingBehavior::Blocked && Agent->Velocity.IsNearlyZero());
        Bump->Destroy();
        Route->ReviewedHalfWidthCm = 0; Agent->Step(1.f/30, Options, Agents, nullptr);
        TestTrue(TEXT("Wheel contacts require reviewed road width"), Agent->Behavior == ELivingBehavior::Blocked);
        Route->ReviewedHalfWidthCm = 400;
        Floor->SetActorEnableCollision(false); Agent->Step(1.f/30, Options, Agents, nullptr);
        TestTrue(TEXT("Missing support blocks vehicle"), Agent->Behavior == ELivingBehavior::Blocked);
        Floor->SetActorEnableCollision(true); Profile->SpeedMetersPerSecond = 3;
        for (int32 I=0; I<60; ++I) Agent->Step(1.f/30, Options, Agents, nullptr);
        TestTrue(TEXT("Restored contacts allow movement"), Agent->Velocity.Size() > 100);
        const float BarrierX = Agent->GetActorLocation().X + 500;
        AActor* Barrier = MakeBox(FVector(BarrierX,0,200), FVector(30,200,200));
        for (int32 I=0; I<90; ++I) Agent->Step(1.f/30, Options, Agents, nullptr);
        TestTrue(TEXT("Fitted chassis sweep still stops before a road obstacle"), Agent->Behavior == ELivingBehavior::Blocked && Agent->GetActorLocation().X < BarrierX-100);
        Barrier->Destroy();
        Agent->Deactivate(); Agent->Activate(Profile, Route, 0, FVector::ZeroVector, 2, 0);
        TestTrue(TEXT("Pooled vehicle clears previous suspension and steering"), Agent->WheelPoses[0].Offset.IsNearlyZero() && Agent->WheelPoses[0].SteeringDegrees == 0);
    }
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingOptionsTest, "RuneSim.LivingWorld.Settings", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingOptionsTest::RunTest(const FString& Parameters)
{
    FLivingWorldOptions Options;
    TestEqual(TEXT("Disabled spawns no birds"), Options.DesiredCount(ELivingKind::Bird), 0);
    Options.bEnabled = true;
    Options.ApplyPreset(ELivingPreset::Busy);
    Options.Population = ELivingPopulation::Mixed;
    TestEqual(TEXT("Mixed population retains total density"), Options.DesiredCount(ELivingKind::Civilian) + Options.DesiredCount(ELivingKind::Soldier), 64);
    Options.Population = ELivingPopulation::Civilians;
    TestEqual(TEXT("Civilian scenario excludes soldiers"), Options.DesiredCount(ELivingKind::Soldier), 0);
    Options.FlockSize = 10000; Options.MaxActors = -2; Options.CrowdDensity = -1;
    Options.ActivityRadiusMeters = std::numeric_limits<float>::infinity();
    Options.Sanitize();
    TestEqual(TEXT("Flock bounded"), Options.FlockSize, 24);
    TestEqual(TEXT("Actor budget bounded"), Options.MaxActors, 1);
    TestEqual(TEXT("Invalid density clamped"), Options.DesiredCount(ELivingKind::Civilian), 0);
    TestEqual(TEXT("Nonfinite radius repaired"), Options.ActivityRadiusMeters, 500.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingFlightTest, "RuneSim.LivingWorld.FlightEnvelope", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingFlightTest::RunTest(const FString& Parameters)
{
    ULivingAssetProfile* Jet = NewObject<ULivingAssetProfile>();
    Jet->Kind = ELivingKind::Plane; Jet->SpeedMetersPerSecond = 80.f; Jet->TurnRateDegrees = 20.f;
    Jet->FlightRadiusFraction = .8f;
    const float Radius = LivingWorld::FlightRadius(*Jet, 100.f);
    TestTrue(TEXT("Fast aircraft radius respects turn limit even in a small activity area"),
        FMath::RadiansToDegrees(Jet->SpeedMetersPerSecond * 100.f / Radius) < Jet->TurnRateDegrees);
    Jet->Kind = ELivingKind::Drone; Jet->SpeedMetersPerSecond = 8.f; Jet->TurnRateDegrees = 60.f;
    Jet->FlightRadiusFraction = .2f;
    TestEqual(TEXT("Drone uses closer flight lane"), LivingWorld::FlightRadius(*Jet, 500.f), 10000.f);
    Jet->Kind = ELivingKind::Bird;
    TestEqual(TEXT("Bird lane matches flock spawn radius"), LivingWorld::FlightRadius(*Jet, 500.f), 12500.f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingRoutesTest, "RuneSim.LivingWorld.RouteIntegration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingRoutesTest::RunTest(const FString& Parameters)
{
    int32 Direction = 1;
    TestEqual(TEXT("Closed wrap"), LivingWorld::AdvanceRoute(95, 10, 100, true, Direction), 5.f);
    Direction = -1;
    TestEqual(TEXT("Closed reverse wrap"), LivingWorld::AdvanceRoute(5, 10, 100, true, Direction), 95.f);
    Direction = 1;
    TestEqual(TEXT("Open end reflects overshoot"), LivingWorld::AdvanceRoute(95, 10, 100, false, Direction), 95.f);
    TestEqual(TEXT("Open end reverses heading"), Direction, -1);
    TestEqual(TEXT("Continue backwards"), LivingWorld::AdvanceRoute(95, 10, 100, false, Direction), 85.f);
    Direction = 1;
    TestEqual(TEXT("Large timestep bounded"), LivingWorld::AdvanceRoute(0, 450, 100, false, Direction), 50.f);
    TestEqual(TEXT("Degenerate route"), LivingWorld::AdvanceRoute(95, 10, 0, false, Direction), 0.f);
    FRandomStream Random(4242);
    for (int32 I = 0; I < 1000; ++I)
    {
        const float Distance = LivingWorld::AdvanceRoute(Random.FRandRange(0, 100), Random.FRandRange(0, 500), 100, false, Direction);
        if (Distance < 0 || Distance > 100) { AddError(TEXT("Route integration escaped bounds")); return false; }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLivingGroundMotionTest, "RuneSim.LivingWorld.GroundMotion", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLivingGroundMotionTest::RunTest(const FString& Parameters)
{
    TestEqual(TEXT("No stopping room means stop"), LivingWorld::StoppingSpeed(-1, 400, .033f), 0.f);
    TestEqual(TEXT("Invalid braking fails closed"), LivingWorld::StoppingSpeed(100, 0, .033f), 0.f);
    for (float Distance : {10.f, 100.f, 1000.f})
    {
        const float V = LivingWorld::StoppingSpeed(Distance, 400, .033f);
        TestTrue(TEXT("Cap retains both braking and reaction distance"), V * V / 800.f + V * .033f <= Distance + .01f);
    }
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    AActor* Floor = World->SpawnActor<AActor>();
    UBoxComponent* Box = NewObject<UBoxComponent>(Floor);
    Floor->SetRootComponent(Box); Floor->AddInstanceComponent(Box);
    Box->SetBoxExtent(FVector(20000, 2000, 10)); Box->SetCollisionProfileName(TEXT("BlockAll"));
    Box->RegisterComponent(); Floor->SetActorLocation(FVector(0,0,-10));
    ALivingRoute* Route = World->SpawnActor<ALivingRoute>();
    Route->Path->SetSplinePoints({FVector(0,0,0), FVector(12000,0,0)}, ESplineCoordinateSpace::World);
    Route->bValidated = true; Route->bVehicles = true; Route->bOneWay = true;
    ULivingAssetProfile* Profile = NewObject<ULivingAssetProfile>();
    Profile->Kind = ELivingKind::Car; Profile->SpeedMetersPerSecond = 10;
    Profile->CollisionRadiusCm = 50; Profile->GroundClearanceCm = 100;
    ALivingAgent* Follower = World->SpawnActor<ALivingAgent>();
    ALivingAgent* Leader = World->SpawnActor<ALivingAgent>();
    Follower->SetActorLocation(FVector(0,0,100)); Leader->SetActorLocation(FVector(2000,0,100));
    Follower->Activate(Profile, Route, 0, FVector::ZeroVector, 1, 0);
    Leader->Activate(Profile, Route, 2000, FVector::ZeroVector, 2, 0);
    const TArray<ALivingAgent*> Agents = {Follower, Leader};
    FLivingWorldOptions Options;
    Follower->Step(1.f/30, Options, Agents, nullptr);
    TestTrue(TEXT("Launch accelerates instead of jumping to cruise speed"), Follower->Velocity.Size() > 0 && Follower->Velocity.Size() < 10);
    for (int32 I=0; I<900; ++I) Follower->Step(1.f/30, Options, Agents, nullptr);
    TestTrue(TEXT("Stops before stationary leader with stand-off"), Follower->GetActorLocation().X <= 1751 && Follower->GetActorLocation().X > 1600);
    TestTrue(TEXT("Settles at rest"), Follower->Velocity.Size() < 1);
    Leader->Deactivate();
    for (int32 I=0; I<60; ++I) Follower->Step(1.f/30, Options, Agents, nullptr);
    TestTrue(TEXT("Queue resumes when obstruction leaves"), Follower->Velocity.Size() > 300);
    Route->ReviewedHalfWidthCm = 100;
    FVector Ground, Normal;
    TestTrue(TEXT("Reviewed footprint supported"), Route->SampleGround(3000, Ground, Normal, 0, 50));
    TestFalse(TEXT("Footprint cannot escape reviewed corridor"), Route->SampleGround(3000, Ground, Normal, 80, 50));
    AActor* Water = World->SpawnActor<AActor>(); Water->Tags.Add(TEXT("Water"));
    UBoxComponent* WaterBox = NewObject<UBoxComponent>(Water); Water->SetRootComponent(WaterBox); Water->AddInstanceComponent(WaterBox);
    WaterBox->SetBoxExtent(FVector(100,20,10)); WaterBox->SetCollisionProfileName(TEXT("BlockAll")); WaterBox->RegisterComponent();
    Water->SetActorLocation(FVector(3000,55,10));
    TestFalse(TEXT("Water under footprint edge rejects centre-supported step"), Route->SampleGround(3000, Ground, Normal, 0, 50));
    Water->Destroy();
    Route->bValidated = false;
    Follower->Step(1.f/30, Options, Agents, nullptr);
    TestTrue(TEXT("Withdrawn route stops immediately"), Follower->Velocity.IsNearlyZero() && Follower->GroundSpeed == 0);
    Follower->Deactivate();
    Route->bValidated = true; Route->bOneWay = false; Route->ReviewedHalfWidthCm = 200;
    Profile->Kind = ELivingKind::Civilian; Profile->SpeedMetersPerSecond = 1.4f; Profile->CollisionRadiusCm = 30;
    Follower->SetActorLocation(FVector(3000,0,100)); Leader->SetActorLocation(FVector(5000,0,100));
    Follower->Activate(Profile, Route, 3000, FVector::ZeroVector, 3, 0);
    Leader->Activate(Profile, Route, 5000, FVector::ZeroVector, 4, 0);
    const FVector Threat(5100,0,100);
    Options.bReactive = true;
    float Closest = 2000;
    for (int32 I=0; I<330; ++I)
    {
        Follower->Step(1.f/30, Options, Agents, nullptr);
        Leader->Step(1.f/30, Options, Agents, I<30 ? &Threat : nullptr);
        Closest = FMath::Min(Closest, float(FVector::Distance(Follower->GetActorLocation(),Leader->GetActorLocation())));
    }
    TestTrue(TEXT("Opposing pedestrians pass in reviewed lanes"), Follower->GetActorLocation().X > Leader->GetActorLocation().X);
    TestTrue(TEXT("Passing pedestrians retain clearance"), Closest >= 60);

    // A nearby pedestrian turns back after both have joined the same lane.
    // Rejecting a blocked diagonal move must not prevent safe sideways escape.
    Follower->SetActorLocation(FVector(3000,0,100)); Leader->SetActorLocation(FVector(3140,0,100));
    Follower->Activate(Profile, Route, 3000, FVector::ZeroVector, 3, 0);
    Leader->Activate(Profile, Route, 3140, FVector::ZeroVector, 4, 0);
    for (int32 I=0; I<60; ++I)
    {
        Follower->Step(1.f/30, Options, Agents, nullptr);
        Leader->Step(1.f/30, Options, Agents, nullptr);
    }
    const FVector CloseThreat = Leader->GetActorLocation() + FVector(100,0,0);
    Closest = 10000;
    for (int32 I=0; I<240; ++I)
    {
        Follower->Step(1.f/30, Options, Agents, nullptr);
        Leader->Step(1.f/30, Options, Agents, I<30 ? &CloseThreat : nullptr);
        Closest = FMath::Min(Closest, float(FVector::Distance(Follower->GetActorLocation(),Leader->GetActorLocation())));
    }
    TestTrue(TEXT("Close reversal clears the following pedestrian"), Follower->GetActorLocation().X > Leader->GetActorLocation().X + 100);
    TestTrue(TEXT("Close reversal does not overlap pedestrians"), Closest >= 59.9f);

    Follower->SetActorLocation(FVector(3000,0,100)); Leader->SetActorLocation(FVector(3000,62,100));
    Follower->Activate(Profile, Route, 3000, FVector::ZeroVector, 3, 0);
    Leader->Activate(Profile, nullptr, 0, FVector::ZeroVector, 4, 0);
    Closest = 10000;
    for (int32 I=0; I<120; ++I)
    {
        Follower->Step(1.f/30, Options, Agents, nullptr);
        Closest = FMath::Min(Closest, float(FVector::Distance(Follower->GetActorLocation(),Leader->GetActorLocation())));
    }
    TestTrue(TEXT("Blocked lane merge retains safe forward progress"), Follower->GetActorLocation().X > 3300);
    TestTrue(TEXT("Lane merge escape keeps collision clearance"), Closest >= 59.9f);

    Leader->Deactivate();
    AActor* Wall = World->SpawnActor<AActor>();
    UBoxComponent* WallBox = NewObject<UBoxComponent>(Wall);
    Wall->SetRootComponent(WallBox); Wall->AddInstanceComponent(WallBox);
    WallBox->SetBoxExtent(FVector(20,300,200)); WallBox->SetCollisionProfileName(TEXT("BlockAll")); WallBox->RegisterComponent();
    Wall->SetActorLocation(FVector(3400,0,100));
    Follower->SetActorLocation(FVector(3000,0,100));
    Follower->Activate(Profile, Route, 3000, FVector::ZeroVector, 3, 0);
    for (int32 I=0; I<180; ++I) Follower->Step(1.f/30, Options, Agents, nullptr);
    TestTrue(TEXT("Sidestep cannot cross an obstruction spanning the corridor"), Follower->GetActorLocation().X < 3351);
    TestTrue(TEXT("Sidestep remains inside reviewed footprint bounds"), FMath::Abs(Follower->GetActorLocation().Y) + 30 <= 200);
    TestTrue(TEXT("Full obstruction settles to idle"), Follower->Velocity.Size() < 1);

    GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimScenarioTest, "RuneSim.LivingWorld.SimulatedInterfaces", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimScenarioTest::RunTest(const FString& Parameters)
{
    const UWorld::InitializationValues Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Settings);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ASimPTZ* PTZ = World->SpawnActor<ASimPTZ>();
    TestEqual(TEXT("Default stream ID becomes a valid ROS namespace"), PTZ->GetROSTopicPrefix(), FString(TEXT("/runesim/ptz/ptz_1")));
    PTZ->CameraId = TEXT("7 west/roof");
    TestEqual(TEXT("ROS identifier handles digits spaces and slashes"), PTZ->GetROSTopicPrefix(), FString(TEXT("/runesim/ptz/camera_7_west_roof")));
    PTZ->CameraId.Empty();
    TestEqual(TEXT("Empty camera ID has valid ROS fallback"), PTZ->GetROSTopicPrefix(), FString(TEXT("/runesim/ptz/camera")));
    PTZ->CameraId = TEXT("ptz-1");
    TestTrue(TEXT("Finite PTZ command accepted"), PTZ->SetPTZ(999.f,-999.f,1.f));
    TestEqual(TEXT("Pan clamped"), PTZ->Command.X, 170.0);
    TestEqual(TEXT("Tilt clamped"), PTZ->Command.Y, -80.0);
    TestEqual(TEXT("FOV clamped"), PTZ->Command.Z, 5.0);
    TestFalse(TEXT("Nonfinite command rejected"), PTZ->SetPTZ(std::numeric_limits<float>::quiet_NaN(),0,60));
    ASimProjectile* Projectile = World->SpawnActor<ASimProjectile>();
    TestFalse(TEXT("Homing rejects non-opted-in actors"), Projectile->SetSimulatedTarget(PTZ));
    USimTargetComponent* Target = NewObject<USimTargetComponent>(PTZ);
    Target->ImpactEffect = nullptr; // Visual effects have their own in-engine render/lifetime checks.
    PTZ->AddInstanceComponent(Target); Target->RegisterComponent();
    TestTrue(TEXT("Homing accepts opted-in simulated target"), Projectile->SetSimulatedTarget(PTZ));
    Target->ApplyVirtualDamage(25.f);
    TestEqual(TEXT("Damage applied"), Target->Health, 75.f);
    Target->ApplyVirtualDamage(-100.f);
    TestEqual(TEXT("Negative damage ignored"), Target->Health, 75.f);
    Target->ApplyVirtualDamage(100.f);
    TestTrue(TEXT("Destruction recorded"), Target->bDestroyed);
    TestFalse(TEXT("Destroyed target rejected"), Projectile->SetSimulatedTarget(PTZ));
    GEngine->DestroyWorldContext(World);
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSimReplayTest, "RuneSim.LivingWorld.Replay", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FSimReplayTest::RunTest(const FString& Parameters)
{
    const FString Name = TEXT("Automation_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LivingWorld/Recordings"));
    IFileManager::Get().MakeDirectory(*Directory, true);
    const FString Path = FPaths::Combine(Directory, Name + TEXT(".jsonl"));
    const FString Data = TEXT("{\"simulation_time\":10,\"actors\":[{\"id\":\"subject\",\"position_cm\":\"X=0 Y=0 Z=100\",\"rotation_deg\":\"P=0 Y=0 R=0\"}]}\n")
        TEXT("{\"simulation_time\":12,\"actors\":[{\"id\":\"subject\",\"position_cm\":\"X=200 Y=0 Z=100\",\"rotation_deg\":\"P=0 Y=90 R=0\"}]}\n");
    TestTrue(TEXT("Fixture saved"), FFileHelper::SaveStringToFile(Data, *Path));
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true)
        .RequiresHitProxies(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false).SetTransactional(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ASimReplay* Replay = World->SpawnActor<ASimReplay>();
    TestTrue(TEXT("Legacy recording loads"), Replay->LoadRecording(Name));
    TestEqual(TEXT("Relative duration"), Replay->Duration, 2.f);
    TestTrue(TEXT("Seek midpoint"), Replay->Seek(1));
    if (TestEqual(TEXT("One inert visual"), Replay->Visuals.Num(), 1))
    {
        TestTrue(TEXT("Interpolated position"), Replay->Visuals[0]->GetComponentLocation().Equals(FVector(100, 0, 100), .01));
        TestTrue(TEXT("Interpolated rotation"), FMath::IsNearlyEqual(Replay->Visuals[0]->GetComponentRotation().Yaw, 45., .01));
        TestEqual(TEXT("Replay cannot collide with live subjects"), Replay->Visuals[0]->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
    }
    TestFalse(TEXT("Reject NaN seek"), Replay->Seek(std::numeric_limits<float>::quiet_NaN()));
    Replay->PlayReplay(); Replay->Tick(4);
    TestFalse(TEXT("Stops at end"), Replay->bPlaying);
    Replay->bLoop = true; Replay->Seek(1.5); Replay->PlayReplay(); Replay->Tick(1);
    TestEqual(TEXT("Loop preserves overshoot"), Replay->PlaybackTime, .5f);
    const FString InvalidWheels = Data.Replace(TEXT("\"position_cm\""), TEXT("\"wheel_poses\":[{\"bone\":\"wheel_LF\",\"steering_deg\":90,\"offset_cm\":\"X=0 Y=0 Z=0\"}],\"position_cm\""));
    FFileHelper::SaveStringToFile(InvalidWheels, *Path);
    TestFalse(TEXT("Out-of-range wheel pose rejected"), Replay->LoadRecording(Name));
    TestEqual(TEXT("Rejected wheel pose preserves replay"), Replay->Visuals.Num(), 1);
    const FString Feet = TEXT("\"foot_supports\":[{\"bone\":\"mixamorig_LeftFoot\",\"height_cm\":8},{\"bone\":\"mixamorig_RightFoot\",\"height_cm\":-4}],\"position_cm\"");
    const FString FeetData = Data.Replace(TEXT("\"position_cm\""), *Feet);
    FFileHelper::SaveStringToFile(FeetData, *Path);
    TestTrue(TEXT("Bounded recorded foot contacts load"), Replay->LoadRecording(Name));
    const FString StanceData = FeetData.Replace(TEXT("\"height_cm\":8"),TEXT("\"height_cm\":8,\"stance_offset_cm\":\"X=10 Y=0 Z=0\""));
    FFileHelper::SaveStringToFile(StanceData, *Path);
    TestTrue(TEXT("Recorded stance offsets load"),Replay->LoadRecording(Name));
    FFileHelper::SaveStringToFile(StanceData.Replace(TEXT("X=10 Y=0 Z=0"),TEXT("X=19 Y=0 Z=0")), *Path);
    TestFalse(TEXT("Excessive stance offset rejected"),Replay->LoadRecording(Name));
    FFileHelper::SaveStringToFile(StanceData.Replace(TEXT("X=10 Y=0 Z=0"),TEXT("X=0 Y=0 Z=2")), *Path);
    TestFalse(TEXT("Vertical stance offset rejected"),Replay->LoadRecording(Name));
    FFileHelper::SaveStringToFile(FeetData.Replace(TEXT("\"height_cm\":8"),TEXT("\"height_cm\":19")), *Path);
    TestFalse(TEXT("Excessive foot height rejected"), Replay->LoadRecording(Name));
    FFileHelper::SaveStringToFile(FeetData.Replace(TEXT("mixamorig_RightFoot"),TEXT("mixamorig_LeftFoot")), *Path);
    TestFalse(TEXT("Duplicate foot contacts rejected"), Replay->LoadRecording(Name));
    FFileHelper::SaveStringToFile(TEXT("{\"simulation_time\":0,\"actors\":[{}]}"), *Path);
    TestFalse(TEXT("Malformed actor rejected"), Replay->LoadRecording(Name));
    TestEqual(TEXT("Failed load preserves current replay"), Replay->Visuals.Num(), 1);
    Replay->ClearReplay(); TestEqual(TEXT("Clear releases visuals"), Replay->Visuals.Num(), 0);
    GEngine->DestroyWorldContext(World); World->DestroyWorld(false);
    IFileManager::Get().Delete(*Path);
    return true;
}
#endif
