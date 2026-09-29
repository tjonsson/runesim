#include "SimPTZ.h"
#include "SimCameraStreamComponent.h"
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
    if (bEnableROS) ConnectROS();
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
    const float Rate=FMath::Clamp(SlewDegreesPerSecond,1.f,180.f);
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
    if (!Root->TryGetStringField(TEXT("topic"),Topic) || Topic!=GetROSTopicPrefix()+TEXT("/command") || !Root->TryGetObjectField(TEXT("msg"),Msg)) return;
    if (!Msg || !Msg->IsValid()) return;
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
    if (Socket) { Socket->OnMessage().Clear(); Socket->OnConnected().Clear(); Socket->Close(); Socket.Reset(); }
    Super::EndPlay(Reason);
}
