#include "SimWarEffects.h"
#include "Misc/App.h"
#include "SimCameraStreamComponent.h"
#include "SimPTZ.h"
#include "SimScenario.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Sound/SoundBase.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

namespace
{
    // Authored by Scripts/build_war_effects.py from the Rook & Bolt coastal/vehicle-fluid effects.
    const TCHAR* FlashPath = TEXT("/Game/LivingWorld/Effects/War/NS_War_Flash.NS_War_Flash");
    const TCHAR* BlastPath = TEXT("/Game/LivingWorld/Effects/War/NS_War_Blast.NS_War_Blast");
    const TCHAR* SmokeColumnPath = TEXT("/Game/LivingWorld/Effects/War/NS_War_SmokeColumn.NS_War_SmokeColumn");
    const TCHAR* SmokeScreenPath = TEXT("/Game/LivingWorld/Effects/War/NS_War_SmokeScreen.NS_War_SmokeScreen");
    const TCHAR* FlamesPath = TEXT("/Game/LivingWorld/Effects/War/NS_War_Flames.NS_War_Flames");
    const TCHAR* FluidFirePath = TEXT("/Game/LivingWorld/Effects/War/NS_War_FluidFire.NS_War_FluidFire");
    const TCHAR* ScorchPath = TEXT("/Game/LivingWorld/Effects/War/M_War_Scorch.M_War_Scorch");
    const TCHAR* ExplosionSoundPath = TEXT("/Game/LivingWorld/Audio/Combat/S_Explosion.S_Explosion");
    constexpr float FireSizeCm = 600.f;
    constexpr int32 MaxFluidPool = 4;
    const FVector ParkLocation(0, 0, -1.0e7); // 100 km below the scene: simulated, never seen.

    TAutoConsoleVariable<int32> CVarMaxFluidFires(TEXT("sim.fx.MaxFluidFires"), 2,
        TEXT("Fires simulated with NiagaraFluids at once (the rest burn with sprites), at most 4; 0 disables fluids. ")
        TEXT("The pool of fluid components is created at world start, so raise it in the config, not mid-session."));
    TAutoConsoleVariable<float> CVarFluidScreenFraction(TEXT("sim.fx.FluidScreenFraction"), .06f,
        TEXT("A fire becomes a fluid simulation when it covers at least this fraction of some camera's view width."));

    const TCHAR* ImpactPaths[] = {
        TEXT("/Game/LivingWorld/Effects/War/NS_War_Impact_Metal.NS_War_Impact_Metal"), TEXT("/Game/LivingWorld/Effects/War/NS_War_Impact_Rock.NS_War_Impact_Rock"),
        TEXT("/Game/LivingWorld/Effects/War/NS_War_Impact_Sand.NS_War_Impact_Sand"), TEXT("/Game/LivingWorld/Effects/War/NS_War_Impact_Water.NS_War_Impact_Water")};
}

UObject* USimWarEffects::Asset(const TCHAR* Path)
{
    if (const TObjectPtr<UObject>* Found = Assets.Find(Path)) return Found->Get();
    UObject* Loaded = LoadObject<UObject>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
    Assets.Add(Path, Loaded);
    return Loaded;
}

void USimWarEffects::OnWorldBeginPlay(UWorld& InWorld)
{
    Super::OnWorldBeginPlay(InWorld);
    for (const TCHAR* Path : {FlashPath, BlastPath, SmokeColumnPath, SmokeScreenPath, FlamesPath, FluidFirePath, ScorchPath, ExplosionSoundPath})
        Asset(Path);
    for (const TCHAR* Path : ImpactPaths) Asset(Path);
    // NiagaraFluids has dozens of compute stages; compiling their pipelines on first use stalled the stream
    // for 10-70 s. Spawn every system once, 100 km below the scene, and keep it alive while they compile.
    if (FApp::CanEverRender())
    {
        for (const TCHAR* Path : {FlashPath, BlastPath, SmokeColumnPath, SmokeScreenPath, FlamesPath})
            if (UNiagaraComponent* Warm = Spawn(Path, ParkLocation, 1.f, false)) Prewarm.Add(Warm);
        PrewarmRemaining = 8.f;
        const int32 PoolSize = FMath::Clamp(CVarMaxFluidFires.GetValueOnGameThread(), 0, MaxFluidPool);
        for (int32 I = 0; I < PoolSize; ++I)
            if (UNiagaraComponent* Fluid = Spawn(FluidFirePath, ParkLocation, 1.f, false)) FluidPool.Add(Fluid);
    }
}

float SimWarEffects::ApparentFraction(const FSimEffectViewer& Viewer, const FVector& Target, float SizeCm)
{
    const FVector Delta = Target - Viewer.Location;
    const float Distance = Delta.Size();
    if (!FMath::IsFinite(Distance) || SizeCm <= 0.f) return 0.f;
    if (Distance < 1.f) return 1.f;
    const float Fov = FMath::Clamp(Viewer.FovDegrees, 1.f, 170.f);
    // A fire just outside the frame still matters: tripods and sensors pan onto it.
    const float Cone = FMath::Min(89.f, Fov * .5f * 1.15f);
    if (FVector::DotProduct(Delta / Distance, Viewer.Forward.GetSafeNormal()) < FMath::Cos(FMath::DegreesToRadians(Cone))) return 0.f;
    const float Width = 2.f * Distance * FMath::Tan(FMath::DegreesToRadians(Fov * .5f));
    return SizeCm / FMath::Max(Width, 1.f);
}

TArray<int32> SimWarEffects::SelectFluid(const TArray<float>& Apparent, const TArray<bool>& Current, int32 Max, float Threshold)
{
    TArray<int32> Order;
    for (int32 I = 0; I < Apparent.Num(); ++I) Order.Add(I);
    Order.Sort([&](int32 A, int32 B) { return Apparent[A] > Apparent[B]; });
    TArray<int32> Chosen;
    for (const int32 I : Order)
    {
        if (Chosen.Num() >= Max) break;
        const float Needed = Current.IsValidIndex(I) && Current[I] ? Threshold * .6f : Threshold;
        if (Apparent[I] >= Needed) Chosen.Add(I);
    }
    return Chosen;
}

bool USimWarEffects::DoesSupportWorldType(const EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

FVector USimWarEffects::FFire::Current() const
{
    return Attach.IsValid() ? Attach->GetActorLocation() + AttachOffset : Location;
}

UNiagaraComponent* USimWarEffects::Spawn(const TCHAR* Path, const FVector& Location, float Scale, bool bAutoDestroy)
{
    UNiagaraSystem* System = Cast<UNiagaraSystem>(Asset(Path));
    if (!System || !GetWorld()) return nullptr;
    return UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), System, Location, FRotator::ZeroRotator, FVector(Scale), bAutoDestroy);
}

void USimWarEffects::Scorch(const FVector& Location, float Scale)
{
    UMaterialInterface* Material = Cast<UMaterialInterface>(Asset(ScorchPath));
    if (!Material || !GetWorld() || Material->GetMaterial()->MaterialDomain != MD_DeferredDecal) return;
    const float Size = 350.f * Scale;
    UGameplayStatics::SpawnDecalAtLocation(GetWorld(), Material, FVector(250.f, Size, Size), Location, FRotator(-90.f, 0.f, 0.f), 180.f);
}

int32 USimWarEffects::StartFire(FVector Location, float Scale, float Seconds, AActor* Attach)
{
    FFire Fire;
    Fire.Location = Location;
    Fire.Scale = FMath::Clamp(FMath::IsFinite(Scale) ? Scale : 1.f, .25f, 8.f);
    Fire.bForever = !(Seconds > 0.f);
    Fire.Remaining = Fire.bForever ? 0.f : Seconds;
    if (IsValid(Attach)) { Fire.Attach = Attach; Fire.AttachOffset = Location - Attach->GetActorLocation(); }
    if (UNiagaraSystem* Flames = Cast<UNiagaraSystem>(Asset(FlamesPath)); Flames && Flames->IsLooping())
        Fire.Flames = Spawn(FlamesPath, Location, Fire.Scale, false);
    const int32 Id = NextId++;
    Fires.Add(Id, Fire);
    SelectTimer = 0.f; // Decide fluid detail on the next tick.
    return Id;
}

void USimWarEffects::StopFire(int32 Id)
{
    if (FFire* Fire = Fires.Find(Id))
    {
        SetFluid(*Fire, false);
        if (UNiagaraComponent* Flames = Fire->Flames.Get()) { Flames->Deactivate(); Flames->SetAutoDestroy(true); }
        Fires.Remove(Id);
    }
}

UNiagaraComponent* USimWarEffects::BorrowFluid(const FFire& Fire)
{
    UNiagaraComponent* Free = nullptr;
    for (UNiagaraComponent* Candidate : FluidPool)
    {
        if (!IsValid(Candidate)) continue;
        bool bUsed = false;
        for (const auto& Pair : Fires) bUsed |= Pair.Value.Fluid.Get() == Candidate;
        if (!bUsed) { Free = Candidate; break; }
    }
    // Worlds that never began play (tests, tools) grow the pool on demand.
    if (!Free && FluidPool.Num() < MaxFluidPool)
        if ((Free = Spawn(FluidFirePath, ParkLocation, 1.f, false))) FluidPool.Add(Free);
    if (Free)
    {
        Free->SetWorldScale3D(FVector(Fire.Scale));
        Free->SetWorldLocation(Fire.Current());
    }
    return Free;
}

void USimWarEffects::ReturnFluid(UNiagaraComponent* Fluid) const
{
    if (IsValid(Fluid)) Fluid->SetWorldLocation(ParkLocation);
}

void USimWarEffects::SetFluid(FFire& Fire, bool bOn)
{
    if (Fire.bFluid == bOn) return;
    Fire.bFluid = bOn;
    if (bOn) Fire.Fluid = BorrowFluid(Fire);
    else { ReturnFluid(Fire.Fluid.Get()); Fire.Fluid = nullptr; }
}

void USimWarEffects::StopAllFires()
{
    TArray<int32> Ids;
    Fires.GetKeys(Ids);
    for (const int32 Id : Ids) StopFire(Id);
}

int32 USimWarEffects::ActiveFluidFireCount() const
{
    int32 Count = 0;
    for (const auto& Pair : Fires) Count += Pair.Value.bFluid;
    return Count;
}

bool USimWarEffects::IsFireFluid(int32 Id) const
{
    const FFire* Fire = Fires.Find(Id);
    return Fire && Fire->bFluid;
}

void USimWarEffects::Strike(FVector Location, float Scale, bool bLeaveFire)
{
    Scale = FMath::Clamp(FMath::IsFinite(Scale) ? Scale : 1.f, .25f, 8.f);
    Spawn(FlashPath, Location + FVector(0, 0, 150.f * Scale), Scale);
    Spawn(BlastPath, Location, Scale);
    Spawn(SmokeColumnPath, Location + FVector(0, 0, 200.f * Scale), Scale);
    // Scorch decals are off by default: they barely show on Cesium's photogrammetry and their cost over many
    // tile materials is unmeasured. Scorch() remains available for flat levels (bScorchDecals).
    if (bScorchDecals) Scorch(Location, Scale);
    if (USoundBase* Sound = Cast<USoundBase>(Asset(ExplosionSoundPath)))
        UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, Location, FMath::Clamp(.6f + .2f * Scale, .6f, 1.5f));
    if (bLeaveFire && Scale >= 1.f) StartFire(Location, Scale * .6f, 20.f + 10.f * Scale);
}

void USimWarEffects::SmokeScreen(FVector Location, float Scale, float Seconds)
{
    // A screen is a smoke-only fire: it re-emits the long-lived white cloud until it expires.
    const int32 Id = NextId++;
    FFire Screen;
    Screen.Location = Location;
    Screen.Scale = FMath::Clamp(FMath::IsFinite(Scale) ? Scale : 1.f, .25f, 8.f);
    Screen.Remaining = FMath::Max(1.f, Seconds);
    Screen.FlameTimer = -1.f; // Marks a screen: no flames, no fluid.
    Fires.Add(Id, Screen);
}

void USimWarEffects::Impact(FVector Location, FName Surface, float Scale)
{
    const FString Name = Surface == TEXT("metal") ? TEXT("Metal") : Surface == TEXT("water") ? TEXT("Water") :
        Surface == TEXT("sand") ? TEXT("Sand") : TEXT("Rock");
    const TCHAR* Path = Name == TEXT("Metal") ? ImpactPaths[0] : Name == TEXT("Rock") ? ImpactPaths[1] : Name == TEXT("Sand") ? ImpactPaths[2] : ImpactPaths[3];
    Spawn(Path, Location, FMath::Clamp(Scale, .25f, 8.f));
}

TArray<FSimEffectViewer> USimWarEffects::GatherViewers() const
{
    if (!ViewerOverride.IsEmpty()) return ViewerOverride;
    TArray<FSimEffectViewer> Viewers;
    UWorld* World = GetWorld();
    if (!World) return Viewers;
    if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(World, 0))
        Viewers.Add({Camera->GetCameraLocation(), Camera->GetCameraRotation().Vector(), Camera->GetFOVAngle()});
    for (TObjectIterator<USimCameraStreamComponent> It; It; ++It)
        if (It->GetWorld() == World)
            if (const USceneCaptureComponent2D* Capture = It->GetActiveCapture())
                Viewers.Add({Capture->GetComponentLocation(), Capture->GetForwardVector(), Capture->FOVAngle});
    return Viewers;
}

void USimWarEffects::Tick(float DeltaTime)
{
    if (PrewarmRemaining > 0.f && (PrewarmRemaining -= DeltaTime) <= 0.f)
    {
        for (UNiagaraComponent* Warm : Prewarm) if (IsValid(Warm)) { Warm->DeactivateImmediate(); Warm->DestroyComponent(); }
        Prewarm.Reset();
    }
    if (Fires.IsEmpty()) return;
    TArray<int32> Finished;
    for (auto& Pair : Fires)
    {
        FFire& Fire = Pair.Value;
        if (!Fire.bForever && (Fire.Remaining -= DeltaTime) <= 0.f) { Finished.Add(Pair.Key); continue; }
        if (Fire.Attach.IsStale()) { Finished.Add(Pair.Key); continue; }
        const FVector Here = Fire.Current();
        const bool bScreen = Fire.FlameTimer < 0.f;
        if ((Fire.SmokeTimer -= DeltaTime) <= 0.f)
        {
            // Bursts overlap: each smoke puff lives several seconds, so a burst per interval reads as a column.
            Fire.SmokeTimer = bScreen ? 2.5f : 1.2f;
            // The column starts above the flames so its dark base does not hide them.
            Spawn(bScreen ? SmokeScreenPath : SmokeColumnPath, Here + FVector(0, 0, (bScreen ? 50.f : 550.f) * Fire.Scale), Fire.Scale);
        }
        if (bScreen) continue;
        if (UNiagaraComponent* Flames = Fire.Flames.Get()) Flames->SetWorldLocation(Here);
        else if ((Fire.FlameTimer -= DeltaTime) <= 0.f) { Fire.FlameTimer = .5f; Spawn(FlamesPath, Here, Fire.Scale); }
        if (UNiagaraComponent* Fluid = Fire.Fluid.Get()) Fluid->SetWorldLocation(Here);
    }
    for (const int32 Id : Finished) StopFire(Id);
    if ((SelectTimer -= DeltaTime) > 0.f) return;
    SelectTimer = .5f;
    const TArray<FSimEffectViewer> Viewers = GatherViewers();
    TArray<int32> Ids; TArray<float> Apparent; TArray<bool> Current;
    for (const auto& Pair : Fires)
    {
        if (Pair.Value.FlameTimer < 0.f) continue; // Smoke screens have no fluid detail.
        float Best = 0.f;
        for (const FSimEffectViewer& Viewer : Viewers)
            Best = FMath::Max(Best, SimWarEffects::ApparentFraction(Viewer, Pair.Value.Current(), FireSizeCm * Pair.Value.Scale));
        Ids.Add(Pair.Key); Apparent.Add(Best); Current.Add(Pair.Value.bFluid);
    }
    const int32 Budget = FMath::Clamp(CVarMaxFluidFires.GetValueOnGameThread(), 0, MaxFluidPool);
    const TArray<int32> Chosen = SimWarEffects::SelectFluid(Apparent, Current, Budget, CVarFluidScreenFraction.GetValueOnGameThread());
    // Release before borrowing, so a fire taking over the detail can reuse the component just freed.
    for (int32 I = 0; I < Ids.Num(); ++I) if (!Chosen.Contains(I)) SetFluid(Fires[Ids[I]], false);
    for (int32 I = 0; I < Ids.Num(); ++I) SetFluid(Fires[Ids[I]], Chosen.Contains(I));
}

namespace
{
    // sim.fx <type> [scale]: places an effect where the first tripod (or the local view) looks.
    void RunFx(const TArray<FString>& Args, UWorld* World)
    {
        if (!World || Args.IsEmpty()) { UE_LOG(LogTemp, Display, TEXT("sim.fx <strike|fire|smoke_screen|impact_metal|impact_rock|impact_sand|impact_water|stop> [scale]")); return; }
        const FName Type(*Args[0].ToLower());
        const float Scale = Args.Num() > 1 ? FCString::Atof(*Args[1]) : 1.f;
        if (Type == TEXT("stop")) { if (USimWarEffects* War = World->GetSubsystem<USimWarEffects>()) War->StopAllFires(); return; }
        FVector Aim;
        bool bFound = false;
        for (TActorIterator<ASimPTZ> It(World); It && !bFound; ++It) bFound = It->AimPoint(Aim);
        if (!bFound)
            if (APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(World, 0))
            {
                FHitResult Hit;
                const FVector Start = Camera->GetCameraLocation();
                bFound = World->LineTraceSingleByChannel(Hit, Start, Start + Camera->GetCameraRotation().Vector() * 500000.f, ECC_Visibility);
                Aim = Hit.ImpactPoint;
            }
        if (!bFound) { UE_LOG(LogTemp, Warning, TEXT("sim.fx: nothing under the view")); return; }
        SimEvents::Record(World, Type, Aim, TEXT("console"), FString::SanitizeFloat(Scale));
        SimEvents::PlayEffect(World, Type, Aim, Scale);
    }
    FAutoConsoleCommandWithWorldAndArgs GSimFxCommand(TEXT("sim.fx"),
        TEXT("sim.fx <strike|fire|smoke_screen|impact_metal|impact_rock|impact_sand|impact_water|stop> [scale]: effect where the tripod or view looks"),
        FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&RunFx));
}
