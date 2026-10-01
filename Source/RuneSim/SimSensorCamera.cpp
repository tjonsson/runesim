#include "SimSensorCamera.h"
#include "SimCameraStreamComponent.h"
#include "Components/SceneCaptureComponent2D.h"

ASimFollowCamera::ASimFollowCamera()
{
    PrimaryActorTick.bCanEverTick = true;
    // Living World agents move in the subsystem tick, after the physics groups.
    // Follow them in post-update work, before this feed's stream captures.
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Mount")));
    Capture = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SensorCapture"));
    Capture->SetupAttachment(RootComponent);
    Capture->bCaptureEveryFrame = false; Capture->bCaptureOnMovement = false;
    Capture->FOVAngle = 70.f;
    Stream = CreateDefaultSubobject<USimCameraStreamComponent>(TEXT("WebRTC"));
    Stream->Capture = Capture;
    Stream->bCaptureOnlyWhenViewed = true;
    // Moving views force Cesium to re-select tiles every frame (~8 ms each, measured); carried
    // feeds see the tiles already selected for the activity area and the main views.
    Stream->CesiumViewScale = 0.f;
    // Carried feeds are secondary sensors: 540p at 15 fps keeps each extra scene capture
    // affordable next to the tripod's 720p/30 feed.
    Stream->Width = 960; Stream->Height = 540; Stream->FramesPerSecond = 15;
    Stream->bAutoStart = true;
    Stream->StreamId = TEXT("air-1");
    SetActorEnableCollision(false);
}

bool ASimFollowCamera::Configure(const FString& StreamId, float FieldOfView)
{
    if (StreamId.IsEmpty() || StreamId.Len() > 64 || HasActorBegunPlay()) return false;
    Stream->StreamId = StreamId;
    Capture->FOVAngle = FMath::Clamp(FMath::IsFinite(FieldOfView) ? FieldOfView : 70.f, 5.f, 120.f);
    return true;
}

void ASimFollowCamera::SetCarrier(AActor* NewCarrier, bool bInGimbal, float Pitch)
{
    Carrier = NewCarrier;
    bGimbal = bInGimbal;
    GimbalPitch = FMath::Clamp(FMath::IsFinite(Pitch) ? Pitch : -20.f, -90.f, 30.f);
    if (Stream) Stream->SetCaptureOverride(nullptr);
    // The carrier's own body would otherwise fill a nose- or belly-mounted view.
    Capture->HiddenActors.Reset();
    if (NewCarrier) Capture->HiddenActors.Add(NewCarrier);
    Follow();
}

void ASimFollowCamera::BeginPlay()
{
    Super::BeginPlay();
    Stream->PrimaryComponentTick.AddPrerequisite(this, PrimaryActorTick);
}

void ASimFollowCamera::SetIdleView(USceneComponent* View) { IdleView = View; Follow(); }

void ASimFollowCamera::Follow()
{
    if (Carrier.IsValid())
    {
        const FTransform Frame = Carrier->GetActorTransform();
        const FVector Location = Frame.TransformPosition(MountOffset);
        FRotator Rotation = Frame.Rotator();
        if (bGimbal) Rotation = FRotator(GimbalPitch, Rotation.Yaw, 0.f);
        SetActorLocationAndRotation(Location, Rotation);
    }
    else if (IdleView.IsValid())
        SetActorLocationAndRotation(IdleView->GetComponentLocation(), IdleView->GetComponentRotation());
}

void ASimFollowCamera::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    Follow();
}
