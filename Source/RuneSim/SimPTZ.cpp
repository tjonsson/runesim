#include "SimPTZ.h"
#include "SimCameraStreamComponent.h"
#include "SimScenario.h"
#include "SimSensorCamera.h"
#include "UObject/UObjectIterator.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "TextureResource.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/Base64.h"
#include "ImageUtils.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "CesiumGlobeAnchorComponent.h"
#include "WebSocketsModule.h"
#include "Json.h"
#include "Async/Async.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/GameplayStatics.h"

namespace
{
FString JsonText(const TSharedRef<FJsonObject>& Object)
{
    FString Text; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text)); return Text;
}
FString PTZPath(const UObject* Context, const FString& Id)
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LivingWorld/PTZ"),
        FPaths::MakeValidFileName(UGameplayStatics::GetCurrentLevelName(Context, true)), FPaths::MakeValidFileName(Id) + TEXT(".json"));
}
}

ASimPTZ::ASimPTZ()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("TripodBase")));
    Pan = CreateDefaultSubobject<USceneComponent>(TEXT("PanJoint")); Pan->SetupAttachment(RootComponent); Pan->SetRelativeLocation(FVector(0,0,160));
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("PTZCamera")); Camera->SetupAttachment(Pan);
    Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SensorCapture")); Capture->SetupAttachment(Camera);
    Stream = CreateDefaultSubobject<USimCameraStreamComponent>(TEXT("WebRTC"));
    Stream->Capture = Capture;
    GlobeAnchor = CreateDefaultSubobject<UCesiumGlobeAnchorComponent>(TEXT("GlobeAnchor"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    for (int32 I=0; I<3; ++I)
    {
        UStaticMeshComponent* Leg = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("TripodLeg%d"),I));
        Leg->SetupAttachment(RootComponent); Leg->SetStaticMesh(Cylinder.Object);
        const float Angle=I*120.f;
        Leg->SetRelativeLocation(FVector(FMath::Cos(FMath::DegreesToRadians(Angle))*30, FMath::Sin(FMath::DegreesToRadians(Angle))*30,75));
        Leg->SetRelativeRotation(FRotator(20,Angle,0)); Leg->SetRelativeScale3D(FVector(.035,.035,1.6));
        Leg->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void ASimPTZ::BeginPlay()
{
    Stream->StreamId = CameraId;
    Super::BeginPlay();
    LoadPlacement();
    if (bAllowSimulatedEngagement && GetWorld()->IsGameWorld())
    {
        const FTransform View = Camera->GetComponentTransform();
        SeekerCamera = GetWorld()->SpawnActorDeferred<ASimFollowCamera>(ASimFollowCamera::StaticClass(), View, this);
        if (SeekerCamera)
        {
            SeekerCamera->Configure(CameraId + TEXT("-seeker"), 40.f);
            SeekerCamera->MountOffset = FVector(50, 0, 0);
            SeekerCamera->Stream->SignallingUrl = Stream->SignallingUrl;
            SeekerCamera->FinishSpawning(View);
            SeekerCamera->SetIdleView(Camera);
        }
    }
    // Loaded on the game thread; JPEG encoding later runs on a worker thread.
    FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    if (bEnableROS) ConnectROS();
}

void ASimPTZ::UpdateImagePublishing(float DeltaTime)
{
    const bool bDemanded = FPlatformTime::Seconds() - LastImageDemand < 5.0 && ImageRate > 0.f;
    if (!bDemanded || !Socket || !Socket->IsConnected() || !Stream->RenderTarget) { ImageTimer = 0.f; return; }
    // The interval runs while a frame is being read back or encoded, so the pipeline delay does not lower the rate.
    ImageTimer += DeltaTime;
    if (ImageReadback)
    {
        // A copy is queued: lock and compress once the GPU has finished it.
        if (!ImageReadback->IsReady()) return;
        TSharedPtr<FRHIGPUTextureReadback, ESPMode::ThreadSafe> Readback = MoveTemp(ImageReadback);
        const FIntPoint Size = ImageSize; const double Stamp = ImageStamp;
        const int32 Quality = FMath::Clamp(ImageJpegQuality, 10, 100);
        const int32 Width = FMath::Clamp(ImageWidth, 160, ImageSize.X);
        TWeakObjectPtr<ASimPTZ> Weak(this);
        ENQUEUE_RENDER_COMMAND(RuneSimImageLock)([Readback, Size, Stamp, Quality, Width, Weak](FRHICommandListImmediate&)
        {
            TArray<FColor> Pixels; Pixels.SetNumUninitialized(Size.X * Size.Y);
            int32 RowPitch = 0;
            const uint8* Data = static_cast<const uint8*>(Readback->Lock(RowPitch));
            if (Data && RowPitch >= Size.X)
                for (int32 Y = 0; Y < Size.Y; ++Y) FMemory::Memcpy(&Pixels[Y * Size.X], Data + int64(Y) * RowPitch * 4, Size.X * 4);
            Readback->Unlock();
            if (!Data) { AsyncTask(ENamedThreads::GameThread, [Weak]() { if (Weak.IsValid()) Weak->bImageInFlight = false; }); return; }
            AsyncTask(ENamedThreads::AnyBackgroundThreadNormalTask, [Pixels = MoveTemp(Pixels), Size, Stamp, Quality, Width, Weak]() mutable
            {
                FIntPoint OutSize = Size;
                if (Width < Size.X)
                {
                    OutSize = FIntPoint(Width, FMath::Max(1, FMath::RoundToInt32(float(Size.Y) * Width / Size.X)));
                    TArray<FColor> Scaled; Scaled.SetNumUninitialized(OutSize.X * OutSize.Y);
                    FImageUtils::ImageResize(Size.X, Size.Y, Pixels, OutSize.X, OutSize.Y, Scaled, false, true);
                    Pixels = MoveTemp(Scaled);
                }
                for (FColor& Pixel : Pixels) Pixel.A = 255;
                TSharedPtr<IImageWrapper> Jpeg = FModuleManager::GetModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper")).CreateImageWrapper(EImageFormat::JPEG);
                FString Encoded;
                if (Jpeg && Jpeg->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), OutSize.X, OutSize.Y, ERGBFormat::BGRA, 8))
                {
                    const TArray64<uint8>& Bytes = Jpeg->GetCompressed(Quality);
                    Encoded = FBase64::Encode(Bytes.GetData(), Bytes.Num());
                }
                AsyncTask(ENamedThreads::GameThread, [Weak, Encoded = MoveTemp(Encoded), Stamp, OutSize]()
                {
                    if (!Weak.IsValid()) return;
                    Weak->bImageInFlight = false;
                    if (!Encoded.IsEmpty()) Weak->SendImage(Encoded, Stamp, OutSize);
                });
            });
        });
        return;
    }
    const float Interval = 1.f / FMath::Clamp(ImageRate, .1f, FMath::Max(.1f, MaxImageRate));
    if (bImageInFlight || ImageTimer < Interval) return;
    ImageTimer = FMath::Min(ImageTimer - Interval, Interval);
    FTextureRenderTargetResource* Resource = Stream->RenderTarget->GameThread_GetRenderTargetResource();
    if (!Resource) return;
    // A fresh readback per frame: its fence cannot report a previous copy as ready.
    ImageReadback = MakeShared<FRHIGPUTextureReadback, ESPMode::ThreadSafe>(TEXT("RuneSimPTZImage"));
    ImageSize = FIntPoint(Stream->RenderTarget->SizeX, Stream->RenderTarget->SizeY);
    ImageStamp = (FDateTime::UtcNow() - FDateTime(1970, 1, 1)).GetTotalSeconds();
    bImageInFlight = true;
    ENQUEUE_RENDER_COMMAND(RuneSimImageCopy)([Readback = ImageReadback, Resource](FRHICommandListImmediate& RHICmdList)
    {
        if (FRHITexture* Texture = Resource->GetRenderTargetTexture()) Readback->EnqueueCopy(RHICmdList, Texture);
    });
}

void ASimPTZ::SendImage(const FString& Base64Jpeg, double StampSeconds, FIntPoint Size)
{
    if (!Socket || !Socket->IsConnected()) return;
    const int64 Seconds = FMath::FloorToInt64(StampSeconds);
    const int32 Nanoseconds = FMath::Clamp(int32((StampSeconds - Seconds) * 1e9), 0, 999999999);
    FString Frame = GetROSTopicPrefix().RightChop(FString(TEXT("/runesim/ptz/")).Len());
    // Built directly: the payload is a large base64 string that a JSON object would copy twice.
    Socket->Send(FString::Printf(TEXT("{\"op\":\"publish\",\"topic\":\"%s/image/compressed\",\"msg\":{\"header\":{\"stamp\":{\"sec\":%lld,\"nanosec\":%d},\"frame_id\":\"%s\"},\"format\":\"jpeg\",\"data\":\"%s\"}}"),
        *GetROSTopicPrefix(), Seconds, Nanoseconds, *Frame, *Base64Jpeg));
    ++ImagesPublished;
}

FVector2D ASimPTZ::AnglesTo(const FVector& WorldLocation) const
{
    const FVector Local = GetActorTransform().InverseTransformPositionNoScale(WorldLocation) - Pan->GetRelativeLocation();
    return FVector2D(FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)),
        FMath::RadiansToDegrees(FMath::Atan2(Local.Z, FVector2D(Local.X, Local.Y).Size())));
}

int32 ASimPTZ::GetInterceptorsInFlight() const
{
    int32 Count = 0;
    for (const TWeakObjectPtr<ASimProjectile>& Interceptor : Interceptors)
        if (Interceptor.IsValid() && Interceptor->Result == ESimInterceptorResult::InFlight) ++Count;
    return Count;
}

bool ASimPTZ::IsTargetVisible(const AActor* Target) const
{
    if (!Target || !GetWorld()) return false;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SimPTZSight), false, this);
    Params.AddIgnoredActor(Target);
    if (SeekerCamera) Params.AddIgnoredActor(SeekerCamera);
    for (const TWeakObjectPtr<ASimProjectile>& Interceptor : Interceptors) if (Interceptor.IsValid()) Params.AddIgnoredActor(Interceptor.Get());
    FHitResult Hit;
    // Terrain, buildings and other static geometry block the sensor; moving agents do not hide each other.
    return !GetWorld()->LineTraceSingleByObjectType(Hit, Camera->GetComponentLocation(), Target->GetActorLocation(),
        FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

bool ASimPTZ::DesignateTarget(bool bNext)
{
    struct FCandidate { AActor* Actor; float Angle; float Distance; };
    TArray<FCandidate> Visible;
    const FVector Eye = Camera->GetComponentLocation();
    const FVector Forward = Camera->GetForwardVector();
    for (TObjectIterator<USimTargetComponent> It; It; ++It)
    {
        if (It->GetWorld() != GetWorld() || !It->CanBeEngaged() || It->GetOwner() == this) continue;
        AActor* Actor = It->GetOwner();
        const FVector To = Actor->GetActorLocation() - Eye;
        const float Distance = To.Size();
        if (Distance < 100.f || Distance > MaxEngagementRangeCm) continue;
        const FVector2D Angles = AnglesTo(Actor->GetActorLocation());
        if (FMath::Abs(Angles.X) > FMath::Abs(PanLimit) + 1.f || Angles.Y < FMath::Min(MinTilt, MaxTilt) - 1.f ||
            Angles.Y > FMath::Max(MinTilt, MaxTilt) + 1.f || !IsTargetVisible(Actor)) continue;
        Visible.Add({Actor, float(FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(To / Distance, Forward), -1., 1.)))), Distance});
    }
    if (Visible.IsEmpty())
    {
        DesignatedTarget.Reset(); EngagementStatus = TEXT("No visible target in range");
        return false;
    }
    Visible.Sort([](const FCandidate& A, const FCandidate& B) { return A.Angle < B.Angle; });
    const FCandidate* Choice = &Visible[0];
    if (bNext && DesignatedTarget.IsValid())
    {
        const int32 Current = Visible.IndexOfByPredicate([this](const FCandidate& C) { return C.Actor == DesignatedTarget.Get(); });
        Choice = &Visible[Current == INDEX_NONE ? 0 : (Current + 1) % Visible.Num()];
    }
    else if (Visible[0].Angle > Camera->FieldOfView * .6f)
    {
        // Nothing in view: take the nearest visible target so tracking can bring it into frame.
        for (const FCandidate& Candidate : Visible) if (Candidate.Distance < Choice->Distance) Choice = &Candidate;
    }
    DesignatedTarget = Choice->Actor;
    const USimTargetComponent* Target = Choice->Actor->FindComponentByClass<USimTargetComponent>();
    EngagementStatus = FString::Printf(TEXT("Designated %s (%s) at %.0f m"), *Choice->Actor->GetName(),
        Target ? *Target->Category : TEXT("target"), Choice->Distance / 100.f);
    SimEvents::Record(GetWorld(), TEXT("designate"), Choice->Actor->GetActorLocation(), GetName(), Choice->Actor->GetName());
    return true;
}

void ASimPTZ::SetTracking(bool bEnabled)
{
    if (bEnabled && !bTrackTarget) { PreTrackCommand = Command; bHasPreTrackCommand = true; }
    if (!bEnabled && bTrackTarget && bHasPreTrackCommand)
    {
        SetPTZ(PreTrackCommand.X, PreTrackCommand.Y, PreTrackCommand.Z);
        bHasPreTrackCommand = false;
    }
    bTrackTarget = bEnabled;
}

void ASimPTZ::ClearDesignation()
{
    DesignatedTarget.Reset(); SetTracking(false); EngagementStatus = TEXT("Designation cleared");
}

ASimProjectile* ASimPTZ::LaunchInterceptor()
{
    if (!bAllowSimulatedEngagement) { EngagementStatus = TEXT("Simulated engagement is disabled on this camera"); return nullptr; }
    if (Cooldown > 0.f) { EngagementStatus = FString::Printf(TEXT("Launcher reloading (%.1f s)"), Cooldown); return nullptr; }
    if (GetInterceptorsInFlight() >= FMath::Clamp(MaxInterceptorsInFlight, 1, 4)) { EngagementStatus = TEXT("Maximum interceptors in flight"); return nullptr; }
    AActor* Target = DesignatedTarget.Get();
    const USimTargetComponent* Opt = Target ? Target->FindComponentByClass<USimTargetComponent>() : nullptr;
    if (!Opt || !Opt->CanBeEngaged()) { EngagementStatus = TEXT("No designated target"); return nullptr; }
    // Launch rail beside the pan head, lofted above the line of sight.
    const FVector Muzzle = Pan->GetComponentTransform().TransformPosition(FVector(20, 35, 25));
    FRotator Aim = (Target->GetActorLocation() - Muzzle).Rotation();
    Aim.Pitch = FMath::Min(Aim.Pitch + 12.f, 85.f); Aim.Roll = 0;
    const FTransform Launch(Aim, Muzzle);
    ASimProjectile* Interceptor = GetWorld()->SpawnActorDeferred<ASimProjectile>(ASimProjectile::StaticClass(), Launch, this, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!Interceptor) { EngagementStatus = TEXT("Launch failed"); return nullptr; }
    Interceptor->SetSimulatedTarget(Target);
    Interceptor->OnResolvedNative.AddUObject(this, &ASimPTZ::OnInterceptorResolved);
    Interceptor->FinishSpawning(Launch);
    Interceptors.RemoveAll([](const TWeakObjectPtr<ASimProjectile>& P) { return !P.IsValid(); });
    Interceptors.Add(Interceptor);
    ++Launches; Cooldown = FMath::Max(0.f, LaunchCooldownSeconds); SeekerHold = 0.f;
    if (bAutoTrackOnLaunch) SetTracking(true);
    ViewSubject = Interceptor;
    ApplyView();
    if (SeekerCamera) SeekerCamera->SetCarrier(Interceptor, false, 0.f);
    EngagementStatus = FString::Printf(TEXT("Interceptor %d away at %s"), Launches, *Target->GetName());
    return Interceptor;
}

int32 ASimPTZ::AbortInterceptors()
{
    int32 Count = 0;
    for (const TWeakObjectPtr<ASimProjectile>& Interceptor : Interceptors)
        if (Interceptor.IsValid() && Interceptor->Result == ESimInterceptorResult::InFlight) { Interceptor->Abort(); ++Count; }
    if (Count) EngagementStatus = FString::Printf(TEXT("Aborted %d interceptor(s)"), Count);
    return Count;
}

void ASimPTZ::OnInterceptorResolved(ASimProjectile* Projectile, ESimInterceptorResult Result)
{
    if (Result == ESimInterceptorResult::Hit) ++Hits; else if (Result == ESimInterceptorResult::Missed) ++Misses;
    const USimTargetComponent* Target = DesignatedTarget.IsValid() ? DesignatedTarget->FindComponentByClass<USimTargetComponent>() : nullptr;
    const TCHAR* Outcome = Result == ESimInterceptorResult::Hit ? (Target && Target->bDestroyed ? TEXT("Target destroyed") : TEXT("Hit, target damaged")) :
        Result == ESimInterceptorResult::Missed ? TEXT("Miss") : TEXT("Aborted");
    EngagementStatus = FString::Printf(TEXT("%s · closest %.1f m after %.1f s · %d/%d hits"), Outcome,
        Projectile && Projectile->ClosestApproachCm < BIG_NUMBER ? Projectile->ClosestApproachCm / 100.f : -1.f,
        Projectile ? Projectile->FlightTime : 0.f, Hits, Launches);
    // Hold the chosen view on the burst, then return to the tripod view.
    SeekerHold = FMath::Clamp(ViewHoldSeconds, .5f, 8.f);
}

bool ASimPTZ::ExecuteEngagementCommand(const FString& Order)
{
    const FString Text = Order.TrimStartAndEnd().ToLower();
    if (Text == TEXT("designate")) return DesignateTarget(false);
    if (Text == TEXT("next")) return DesignateTarget(true);
    if (Text == TEXT("fire") || Text == TEXT("launch")) return LaunchInterceptor() != nullptr;
    if (Text == TEXT("abort")) return AbortInterceptors() > 0;
    if (Text == TEXT("track_on")) { SetTracking(true); return true; }
    if (Text == TEXT("track_off")) { SetTracking(false); return true; }
    if (Text == TEXT("clear")) { ClearDesignation(); return true; }
    if (Text == TEXT("view")) { CycleView(); return true; }
    if (Text == TEXT("view_tripod")) { SetViewMode(ESimPTZView::Tripod); return true; }
    if (Text == TEXT("view_missile")) { SetViewMode(ESimPTZView::Missile); return true; }
    if (Text == TEXT("view_chase")) { SetViewMode(ESimPTZView::Chase); return true; }
    if (Text == TEXT("strike") || Text == TEXT("smoke_screen") || Text == TEXT("burn"))
    {
        // Simulated battlefield effects at the crosshair: artillery strike, obscurant screen or a fire.
        FVector Aim;
        if (!bAllowSimulatedEngagement || !AimPoint(Aim)) { EngagementStatus = TEXT("No ground under the crosshair in range"); return false; }
        const FName Type = Text == TEXT("burn") ? FName(TEXT("fire")) : FName(*Text);
        const float Scale = Text == TEXT("strike") ? 2.f : 1.f;
        SimEvents::Record(GetWorld(), Type, Aim, CameraId, FString::SanitizeFloat(Scale));
        SimEvents::PlayEffect(GetWorld(), Type, Aim, Scale);
        EngagementStatus = FString::Printf(TEXT("%s at %.0f m"), *Text, FVector::Distance(Aim, Camera->GetComponentLocation()) / 100.f);
        return true;
    }
    EngagementStatus = TEXT("Unknown engagement command");
    return false;
}

bool ASimPTZ::AimPoint(FVector& Location) const
{
    if (!GetWorld() || !Camera) return false;
    const FVector Start = Camera->GetComponentLocation();
    FHitResult Hit;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(PTZAim), true, this);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Camera->GetForwardVector() * MaxEngagementRangeCm, ECC_Visibility, Params)) return false;
    Location = Hit.ImpactPoint;
    return true;
}

void ASimPTZ::SetViewMode(ESimPTZView Mode) { ViewMode = Mode; ApplyView(); }

void ASimPTZ::CycleView()
{
    SetViewMode(ViewMode == ESimPTZView::Tripod ? ESimPTZView::Missile : ViewMode == ESimPTZView::Missile ? ESimPTZView::Chase : ESimPTZView::Tripod);
}

FString ASimPTZ::GetViewName() const
{
    return ViewMode == ESimPTZView::Missile ? TEXT("missile") : ViewMode == ESimPTZView::Chase ? TEXT("chase") : TEXT("tripod");
}

void ASimPTZ::ApplyView()
{
    ASimProjectile* Subject = ViewSubject.Get();
    USceneCaptureComponent2D* Source = !Subject ? nullptr : ViewMode == ESimPTZView::Missile ? Subject->Capture.Get() :
        ViewMode == ESimPTZView::Chase ? Subject->ChaseCapture.Get() : nullptr;
    Stream->SetCaptureOverride(Source);
}

void ASimPTZ::UpdateTracking()
{
    const USimTargetComponent* Target = DesignatedTarget.IsValid() ? DesignatedTarget->FindComponentByClass<USimTargetComponent>() : nullptr;
    // A destroyed target stays framed while its wreck falls or burns; it cannot be engaged again.
    const bool bWreck = Target && Target->bDestroyed && Target->bEngageable && !DesignatedTarget->IsHidden();
    if (DesignatedTarget.IsValid() && (!Target || (!Target->CanBeEngaged() && !bWreck)))
    {
        DesignatedTarget.Reset();
        if (!EngagementStatus.StartsWith(TEXT("Target destroyed"))) EngagementStatus = TEXT("Target lost");
        if (bTrackTarget) SetTracking(false);
    }
    if (!bTrackTarget || !DesignatedTarget.IsValid()) return;
    const FVector Location = DesignatedTarget->GetActorLocation();
    const FVector2D Angles = AnglesTo(Location);
    float Fov = Command.Z;
    if (bAutoZoom)
    {
        // Frame the target at roughly a quarter of the image width.
        const float Size = FMath::Max(50.f, DesignatedTarget->GetSimpleCollisionRadius() * 2.f);
        const float Distance = FMath::Max(100.f, float(FVector::Distance(Location, Camera->GetComponentLocation())));
        Fov = FMath::Clamp(FMath::RadiansToDegrees(2.f * FMath::Atan(Size * .5f / Distance)) / .25f, 5.f, 60.f);
    }
    SetPTZ(Angles.X, Angles.Y, Fov);
}

bool ASimPTZ::SetPTZ(float PanDegrees, float TiltDegrees, float HorizontalFovDegrees)
{
    if (!FMath::IsFinite(PanDegrees) || !FMath::IsFinite(TiltDegrees) || !FMath::IsFinite(HorizontalFovDegrees)) return false;
    Command = FVector(FMath::Clamp(PanDegrees,-FMath::Abs(PanLimit),FMath::Abs(PanLimit)),
        FMath::Clamp(TiltDegrees,FMath::Min(MinTilt,MaxTilt),FMath::Max(MinTilt,MaxTilt)),FMath::Clamp(HorizontalFovDegrees,5.f,100.f));
    return true;
}

FString ASimPTZ::GetROSTopicPrefix() const
{
    FString Token;
    for (TCHAR C : CameraId)
    {
        const bool bAsciiLetter = (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('A') && C <= TEXT('Z'));
        const bool bDigit = C >= TEXT('0') && C <= TEXT('9');
        Token.AppendChar(bAsciiLetter || bDigit || C == TEXT('_') ? C : TEXT('_'));
    }
    if (Token.IsEmpty()) Token = TEXT("camera");
    if (Token[0] >= TEXT('0') && Token[0] <= TEXT('9')) Token = TEXT("camera_") + Token;
    return TEXT("/runesim/ptz/") + Token;
}

void ASimPTZ::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    Cooldown = FMath::Max(0.f, Cooldown - DeltaTime);
    Interceptors.RemoveAll([](const TWeakObjectPtr<ASimProjectile>& P) { return !P.IsValid(); });
    if (SeekerHold > 0.f && (SeekerHold -= DeltaTime) <= 0.f)
    {
        // Continue with another interceptor still in flight, otherwise restore the tripod view.
        ASimProjectile* Next = nullptr;
        for (const TWeakObjectPtr<ASimProjectile>& P : Interceptors) if (P->Result == ESimInterceptorResult::InFlight) Next = P.Get();
        ViewSubject = Next;
        ApplyView();
        if (SeekerCamera) SeekerCamera->SetCarrier(Next, false, 0.f);
    }
    UpdateTracking();
    const float Rate=FMath::Clamp(bTrackTarget ? FMath::Max(SlewDegreesPerSecond, 120.f) : SlewDegreesPerSecond,1.f,180.f);
    const float Yaw=FMath::FInterpConstantTo(Pan->GetRelativeRotation().Yaw,Command.X,DeltaTime,Rate);
    const float Pitch=FMath::FInterpConstantTo(Camera->GetRelativeRotation().Pitch,Command.Y,DeltaTime,Rate);
    Pan->SetRelativeRotation(FRotator(0,Yaw,0)); Camera->SetRelativeRotation(FRotator(Pitch,0,0));
    Camera->SetFieldOfView(FMath::FInterpConstantTo(Camera->FieldOfView,Command.Z,DeltaTime,40.f));
    Capture->FOVAngle=Camera->FieldOfView;
    if (!bEnableROS && Socket)
    {
        Socket->OnMessage().Clear(); Socket->OnConnected().Clear(); Socket->Close(); Socket.Reset();
    }
    if (bEnableROS)
    {
        RetryTimer+=DeltaTime; NetworkTimer+=DeltaTime;
        if ((!Socket || !Socket->IsConnected()) && RetryTimer>3.f) { RetryTimer=0; ConnectROS(); }
        if (Socket && Socket->IsConnected() && NetworkTimer>.1f) { NetworkTimer=0; SendState(); }
        UpdateImagePublishing(DeltaTime);
    }
}

void ASimPTZ::ConnectROS()
{
    if (Socket) { Socket->OnMessage().Clear(); Socket->OnConnected().Clear(); Socket->Close(); }
    Socket = FModuleManager::LoadModuleChecked<FWebSocketsModule>(TEXT("WebSockets")).CreateWebSocket(RosbridgeUrl);
    Socket->OnConnected().AddWeakLambda(this,[this]()
    {
        auto Request=MakeShared<FJsonObject>(); Request->SetStringField(TEXT("op"),TEXT("subscribe"));
        Request->SetStringField(TEXT("topic"),GetROSTopicPrefix()+TEXT("/command"));
        Request->SetStringField(TEXT("type"),TEXT("geometry_msgs/msg/Vector3"));
        Request->SetNumberField(TEXT("queue_length"),1); Socket->Send(JsonText(Request));
        auto Advertise=MakeShared<FJsonObject>(); Advertise->SetStringField(TEXT("op"),TEXT("advertise"));
        Advertise->SetStringField(TEXT("topic"),GetROSTopicPrefix()+TEXT("/state"));
        Advertise->SetStringField(TEXT("type"),TEXT("std_msgs/msg/String")); Socket->Send(JsonText(Advertise));
        auto Images=MakeShared<FJsonObject>(); Images->SetStringField(TEXT("op"),TEXT("advertise"));
        Images->SetStringField(TEXT("topic"),GetROSTopicPrefix()+TEXT("/image/compressed"));
        Images->SetStringField(TEXT("type"),TEXT("sensor_msgs/msg/CompressedImage")); Socket->Send(JsonText(Images));
        auto Demand=MakeShared<FJsonObject>(); Demand->SetStringField(TEXT("op"),TEXT("subscribe"));
        Demand->SetStringField(TEXT("topic"),GetROSTopicPrefix()+TEXT("/image_demand"));
        Demand->SetStringField(TEXT("type"),TEXT("std_msgs/msg/Float32"));
        Demand->SetNumberField(TEXT("queue_length"),1); Socket->Send(JsonText(Demand));
        if (bAllowSimulatedEngagement)
        {
            auto Engage=MakeShared<FJsonObject>(); Engage->SetStringField(TEXT("op"),TEXT("subscribe"));
            Engage->SetStringField(TEXT("topic"),GetROSTopicPrefix()+TEXT("/engage"));
            Engage->SetStringField(TEXT("type"),TEXT("std_msgs/msg/String"));
            Engage->SetNumberField(TEXT("queue_length"),4); Socket->Send(JsonText(Engage));
        }
    });
    Socket->OnMessage().AddUObject(this,&ASimPTZ::ReceiveROS);
    Socket->Connect();
}

void ASimPTZ::ReceiveROS(const FString& Message)
{
    if (!bEnableROS || Message.Len()>8192) return;
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Message),Root) || !Root) return;
    FString Topic; const TSharedPtr<FJsonObject>* Msg = nullptr;
    if (!Root->TryGetStringField(TEXT("topic"),Topic) || !Root->TryGetObjectField(TEXT("msg"),Msg) || !Msg || !Msg->IsValid()) return;
    if (Topic==GetROSTopicPrefix()+TEXT("/image_demand"))
    {
        double Rate = 0;
        if (!(*Msg)->TryGetNumberField(TEXT("data"),Rate) || !FMath::IsFinite(Rate)) return;
        TWeakObjectPtr<ASimPTZ> Weak(this);
        AsyncTask(ENamedThreads::GameThread,[Weak,Rate]()
        {
            if (!Weak.IsValid()) return;
            Weak->ImageRate = FMath::Clamp(float(Rate), 0.f, FMath::Max(0.f, Weak->MaxImageRate));
            Weak->LastImageDemand = FPlatformTime::Seconds();
        });
        return;
    }
    if (Topic==GetROSTopicPrefix()+TEXT("/engage"))
    {
        FString Data;
        if (!bAllowSimulatedEngagement || !(*Msg)->TryGetStringField(TEXT("data"),Data) || Data.Len()>32) return;
        TWeakObjectPtr<ASimPTZ> Weak(this);
        AsyncTask(ENamedThreads::GameThread,[Weak,Data]() { if (Weak.IsValid() && Weak->bEnableROS && Weak->bAllowSimulatedEngagement) Weak->ExecuteEngagementCommand(Data); });
        return;
    }
    if (Topic!=GetROSTopicPrefix()+TEXT("/command")) return;
    double X,Y,Z;
    if (!(*Msg)->TryGetNumberField(TEXT("x"),X) || !(*Msg)->TryGetNumberField(TEXT("y"),Y) || !(*Msg)->TryGetNumberField(TEXT("z"),Z)) return;
    TWeakObjectPtr<ASimPTZ> Weak(this);
    AsyncTask(ENamedThreads::GameThread,[Weak,X,Y,Z]() { if (Weak.IsValid() && Weak->bEnableROS) Weak->SetPTZ(X,Y,Z); });
}

void ASimPTZ::SendState()
{
    auto State=MakeShared<FJsonObject>();
    State->SetNumberField(TEXT("pan_deg"),Pan->GetRelativeRotation().Yaw);
    State->SetNumberField(TEXT("tilt_deg"),Camera->GetRelativeRotation().Pitch);
    State->SetNumberField(TEXT("horizontal_fov_deg"),Camera->FieldOfView);
    State->SetNumberField(TEXT("simulation_time"),GetWorld()->GetTimeSeconds());
    State->SetNumberField(TEXT("capture_time"),Stream->LastCaptureSimulationTime);
    State->SetNumberField(TEXT("frame"),Stream->FrameNumber);
    State->SetNumberField(TEXT("image_rate_hz"),FPlatformTime::Seconds() - LastImageDemand < 5.0 ? ImageRate : 0.f);
    State->SetNumberField(TEXT("images_published"),ImagesPublished);
    if (bAllowSimulatedEngagement)
    {
        auto Engagement=MakeShared<FJsonObject>();
        Engagement->SetStringField(TEXT("status"),EngagementStatus);
        Engagement->SetStringField(TEXT("target"),DesignatedTarget.IsValid() ? DesignatedTarget->GetName() : FString());
        if (DesignatedTarget.IsValid())
        {
            const FVector2D Angles=AnglesTo(DesignatedTarget->GetActorLocation());
            Engagement->SetNumberField(TEXT("target_range_m"),FVector::Distance(DesignatedTarget->GetActorLocation(),Camera->GetComponentLocation())/100.);
            Engagement->SetNumberField(TEXT("target_pan_deg"),Angles.X); Engagement->SetNumberField(TEXT("target_tilt_deg"),Angles.Y);
        }
        Engagement->SetBoolField(TEXT("tracking"),bTrackTarget);
        Engagement->SetStringField(TEXT("view"),GetViewName());
        Engagement->SetNumberField(TEXT("in_flight"),GetInterceptorsInFlight());
        Engagement->SetNumberField(TEXT("launches"),Launches); Engagement->SetNumberField(TEXT("hits"),Hits); Engagement->SetNumberField(TEXT("misses"),Misses);
        State->SetObjectField(TEXT("engagement"),Engagement);
    }
    auto Data=MakeShared<FJsonObject>(); Data->SetStringField(TEXT("data"),JsonText(State));
    auto Envelope=MakeShared<FJsonObject>(); Envelope->SetStringField(TEXT("op"),TEXT("publish"));
    Envelope->SetStringField(TEXT("topic"),GetROSTopicPrefix()+TEXT("/state"));
    Envelope->SetObjectField(TEXT("msg"),Data); Socket->Send(JsonText(Envelope));
}

bool ASimPTZ::SavePlacement()
{
    const FVector LLH=GlobeAnchor->GetLongitudeLatitudeHeight(); auto Data=MakeShared<FJsonObject>();
    Data->SetNumberField(TEXT("longitude"),LLH.X); Data->SetNumberField(TEXT("latitude"),LLH.Y); Data->SetNumberField(TEXT("height"),LLH.Z);
    Data->SetNumberField(TEXT("pan"),Command.X); Data->SetNumberField(TEXT("tilt"),Command.Y); Data->SetNumberField(TEXT("fov"),Command.Z);
    const FString Path=PTZPath(this,CameraId); IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    return FFileHelper::SaveStringToFile(JsonText(Data),*Path);
}

bool ASimPTZ::LoadPlacement()
{
    FString Text; TSharedPtr<FJsonObject> Data;
    if (!FFileHelper::LoadFileToString(Text,*PTZPath(this,CameraId)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Data) || !Data) return false;
    double Lon,Lat,H,P,T,F;
    if (!Data->TryGetNumberField(TEXT("longitude"),Lon)||!Data->TryGetNumberField(TEXT("latitude"),Lat)||!Data->TryGetNumberField(TEXT("height"),H)||
        !Data->TryGetNumberField(TEXT("pan"),P)||!Data->TryGetNumberField(TEXT("tilt"),T)||!Data->TryGetNumberField(TEXT("fov"),F)) return false;
    if (!FMath::IsFinite(Lon)||!FMath::IsFinite(Lat)||!FMath::IsFinite(H)||FMath::Abs(Lon)>180||FMath::Abs(Lat)>90||!SetPTZ(P,T,F)) return false;
    GlobeAnchor->MoveToLongitudeLatitudeHeight(FVector(Lon,Lat,H)); return true;
}

void ASimPTZ::EndPlay(const EEndPlayReason::Type Reason)
{
    if (IsValid(SeekerCamera)) SeekerCamera->Destroy();
    SeekerCamera = nullptr;
    if (Socket) { Socket->OnMessage().Clear(); Socket->OnConnected().Clear(); Socket->Close(); Socket.Reset(); }
    Super::EndPlay(Reason);
}
