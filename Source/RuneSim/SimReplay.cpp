#include "SimReplay.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Json.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "SimScenario.h"

ASimReplay::ASimReplay()
{
    PrimaryActorTick.bCanEverTick = true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ReplayOrigin")));
    SetActorEnableCollision(false);
}

void ASimReplay::ClearReplay()
{
    bPlaying = false;
    for (UMeshComponent* Visual : Visuals) if (Visual) Visual->DestroyComponent();
    Visuals.Empty(); Tracks.Empty(); Events.Empty(); EventCount = EffectsPlayed = 0; Duration = PlaybackTime = 0;
}

bool ASimReplay::LoadRecording(const FString& Name)
{
    // Only project recordings, bounded before reading or allocating visual components.
    const FString Path = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("LivingWorld/Recordings"), FPaths::MakeValidFileName(Name) + TEXT(".jsonl"));
    const int64 Size = IFileManager::Get().FileSize(*Path);
    LastError.Empty();
    auto Fail = [this](const TCHAR* Error) { LastError = Error; return false; };
    if (Size <= 0 || Size > 17 * 1024 * 1024) return Fail(TEXT("Recording missing, empty or larger than 17 MiB"));
    FString Text;
    if (!FFileHelper::LoadFileToString(Text, *Path)) return Fail(TEXT("Cannot read recording"));
    TArray<FString> Lines; Text.ParseIntoArrayLines(Lines, true);
    TArray<FSimReplayTrack> Parsed;
    TArray<FSimReplayEvent> ParsedEvents;
    TMap<FString, int32> Indices;
    double Start = -1, Previous = -1, End = 0;
    int32 PoseCount = 0;
    for (const FString& Line : Lines)
    {
        TSharedPtr<FJsonObject> Frame;
        double Time = 0;
        const TArray<TSharedPtr<FJsonValue>>* Actors = nullptr;
        if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Frame) || !Frame.IsValid() ||
            !Frame->TryGetNumberField(TEXT("simulation_time"), Time) || !FMath::IsFinite(Time) || Time < 0 || Time < Previous ||
            !Frame->TryGetArrayField(TEXT("actors"), Actors)) return Fail(TEXT("Invalid or out-of-order recording frame"));
        if (Start < 0) Start = Time;
        Previous = Time; End = Time - Start;
        const TArray<TSharedPtr<FJsonValue>>* FrameEvents = nullptr;
        if (Frame->HasField(TEXT("events")))
        {
            if (!Frame->TryGetArrayField(TEXT("events"), FrameEvents)) return Fail(TEXT("Invalid event array"));
            for (const auto& Value : *FrameEvents)
            {
                const TSharedPtr<FJsonObject>* Object = nullptr; FSimReplayEvent Event; FString Type, Location; double EventTime = 0;
                if (!Value->TryGetObject(Object) || !(*Object)->TryGetNumberField(TEXT("time"), EventTime) || !FMath::IsFinite(EventTime) ||
                    !(*Object)->TryGetStringField(TEXT("type"), Type) || Type.IsEmpty() || Type.Len() > 32 ||
                    !(*Object)->TryGetStringField(TEXT("location_cm"), Location) || !Event.Location.InitFromString(Location) || Event.Location.ContainsNaN())
                    return Fail(TEXT("Invalid recorded event"));
                if (ParsedEvents.Num() >= 4096) return Fail(TEXT("Recording exceeds event budget"));
                // Events occur between samples; clamp into the recording's span.
                Event.Time = FMath::Clamp(EventTime - Start, 0.0, End);
                Event.Type = FName(*Type);
                (*Object)->TryGetStringField(TEXT("subject"), Event.Subject);
                ParsedEvents.Add(MoveTemp(Event));
            }
        }
        TSet<FString> Seen;
        for (const auto& Value : *Actors)
        {
            const TSharedPtr<FJsonObject>* ItemPtr = nullptr;
            if (!Value->TryGetObject(ItemPtr)) return Fail(TEXT("Invalid actor entry"));
            const auto& Item = *ItemPtr;
            FString Id, Position, Rotation, Scale;
            FVector P, S = FVector::OneVector; FRotator R;
            if (!Item->TryGetStringField(TEXT("id"), Id) || Id.IsEmpty() || Seen.Contains(Id) ||
                !Item->TryGetStringField(TEXT("position_cm"), Position) || !P.InitFromString(Position) || P.ContainsNaN() ||
                !Item->TryGetStringField(TEXT("rotation_deg"), Rotation) || !R.InitFromString(Rotation) || R.ContainsNaN()) return Fail(TEXT("Invalid actor pose or duplicate id"));
            Seen.Add(Id);
            if (Item->TryGetStringField(TEXT("scale"), Scale) && (!S.InitFromString(Scale) || S.ContainsNaN())) return Fail(TEXT("Invalid actor scale"));
            if (++PoseCount > 100000 || (Indices.Num() >= 512 && !Indices.Contains(Id))) return Fail(TEXT("Recording exceeds replay budget"));
            if (!Indices.Contains(Id))
            {
                FSimReplayTrack Track; Track.Id = Id;
                Item->TryGetStringField(TEXT("static_mesh"), Track.StaticMesh);
                Item->TryGetStringField(TEXT("skeletal_mesh"), Track.SkeletalMesh);
                FString Transform;
                if (Item->TryGetStringField(TEXT("visual_transform"), Transform) &&
                    (!Track.VisualTransform.InitFromString(Transform) || Track.VisualTransform.ContainsNaN())) return Fail(TEXT("Invalid visual transform"));
                for (const FString& Asset : {Track.StaticMesh, Track.SkeletalMesh})
                    if (!Asset.IsEmpty() && !Asset.StartsWith(TEXT("/Game/")) && !Asset.StartsWith(TEXT("/Engine/"))) return Fail(TEXT("Unsupported replay asset path"));
                Indices.Add(Id, Parsed.Add(MoveTemp(Track)));
            }
            FSimReplayPose Pose; Pose.Time = End; Pose.Transform = FTransform(R, P, S);
            Item->TryGetBoolField(TEXT("hidden"), Pose.bHidden);
            Item->TryGetStringField(TEXT("animation"), Pose.Animation);
            double ClipTime = 0;
            if (Item->TryGetNumberField(TEXT("animation_time"), ClipTime) && (!FMath::IsFinite(ClipTime) || ClipTime < 0)) return Fail(TEXT("Invalid animation time"));
            if (!Pose.Animation.IsEmpty() && !Pose.Animation.StartsWith(TEXT("/Game/"))) return Fail(TEXT("Unsupported animation path"));
            Pose.AnimationTime = ClipTime;
            FString BlendPosition;
            if (Item->TryGetStringField(TEXT("blend_position"), BlendPosition) &&
                (!Pose.BlendPosition.InitFromString(BlendPosition) || Pose.BlendPosition.ContainsNaN())) return Fail(TEXT("Invalid blend position"));
            if (Item->HasField(TEXT("foot_supports")))
            {
                const TArray<TSharedPtr<FJsonValue>>* Feet = nullptr;
                if (!Item->TryGetArrayField(TEXT("foot_supports"), Feet) || Feet->Num() != 2) return Fail(TEXT("Invalid foot support array"));
                TSet<FName> Names;
                for (const auto& FootValue : *Feet)
                {
                    const TSharedPtr<FJsonObject>* Foot = nullptr; FString Bone; double Height = 0;
                    if (!FootValue->TryGetObject(Foot) || !(*Foot)->TryGetStringField(TEXT("bone"), Bone) || Bone.Len() > 128 ||
                        !(Bone.EndsWith(TEXT("LeftFoot")) || Bone.EndsWith(TEXT("RightFoot"))) || Names.Contains(FName(*Bone)) ||
                        !(*Foot)->TryGetNumberField(TEXT("height_cm"), Height) || !FMath::IsFinite(Height) || FMath::Abs(Height) > 18)
                        return Fail(TEXT("Invalid foot support"));
                    FLivingFootSupport Support; Support.Bone = FName(*Bone); Support.HeightCm = Height;
                    if ((*Foot)->HasField(TEXT("stance_offset_cm")))
                    {
                        FString Offset;
                        if (!(*Foot)->TryGetStringField(TEXT("stance_offset_cm"), Offset) ||
                            !Support.StanceOffsetCm.InitFromString(Offset) || Support.StanceOffsetCm.ContainsNaN() ||
                            Support.StanceOffsetCm.SizeSquared() > FMath::Square(18.01f) || !FMath::IsNearlyZero(Support.StanceOffsetCm.Z))
                            return Fail(TEXT("Invalid foot stance offset"));
                    }
                    Names.Add(Support.Bone); Pose.FootSupports.Add(Support);
                }
            }
            if (Item->HasField(TEXT("wheel_poses")))
            {
                const TArray<TSharedPtr<FJsonValue>>* Wheels = nullptr;
                if (!Item->TryGetArrayField(TEXT("wheel_poses"), Wheels) || Wheels->Num() > 16) return Fail(TEXT("Invalid wheel pose array"));
                TSet<FName> BoneNames;
                for (const auto& WheelValue : *Wheels)
                {
                    const TSharedPtr<FJsonObject>* Wheel = nullptr;
                    FString Bone, Offset; double Steering = 0; FLivingWheelPose WheelPose;
                    if (!WheelValue->TryGetObject(Wheel) || !(*Wheel)->TryGetStringField(TEXT("bone"), Bone) || Bone.IsEmpty() || Bone.Len() > 128 ||
                        !(*Wheel)->TryGetNumberField(TEXT("steering_deg"), Steering) || !FMath::IsFinite(Steering) || FMath::Abs(Steering) > 60 ||
                        !(*Wheel)->TryGetStringField(TEXT("offset_cm"), Offset) || !WheelPose.Offset.InitFromString(Offset) ||
                        WheelPose.Offset.ContainsNaN() || WheelPose.Offset.Size() > 100) return Fail(TEXT("Invalid wheel pose"));
                    WheelPose.Bone = FName(*Bone); WheelPose.SteeringDegrees = Steering;
                    if (BoneNames.Contains(WheelPose.Bone)) return Fail(TEXT("Duplicate wheel bone"));
                    BoneNames.Add(WheelPose.Bone); Pose.WheelPoses.Add(WheelPose);
                }
            }
            Parsed[Indices[Id]].Poses.Add(MoveTemp(Pose));
        }
    }
    if (Parsed.IsEmpty()) return Fail(TEXT("Recording has no subjects"));
    // Commit only after full validation: failed loads leave the current replay intact.
    ClearReplay(); Tracks = MoveTemp(Parsed); Duration = End;
    ParsedEvents.StableSort([](const FSimReplayEvent& A, const FSimReplayEvent& B) { return A.Time < B.Time; });
    Events = MoveTemp(ParsedEvents); EventCount = Events.Num();
    for (const FSimReplayTrack& Track : Tracks)
    {
        UMeshComponent* Visual = nullptr;
        if (!Track.SkeletalMesh.IsEmpty())
        {
            auto* Mesh = NewObject<USkeletalMeshComponent>(this);
            Mesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, *Track.SkeletalMesh)); Visual = Mesh;
        }
        else
        {
            auto* Mesh = NewObject<UStaticMeshComponent>(this);
            Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, Track.StaticMesh.IsEmpty() ? TEXT("/Engine/BasicShapes/Sphere.Sphere") : *Track.StaticMesh)); Visual = Mesh;
        }
        Visual->SetMobility(EComponentMobility::Movable); Visual->SetupAttachment(RootComponent);
        Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision); Visual->SetGenerateOverlapEvents(false);
        AddInstanceComponent(Visual); Visual->RegisterComponent(); Visuals.Add(Visual);
    }
    UpdateVisuals(); return true;
}

void ASimReplay::PlayReplay() { if (!Tracks.IsEmpty()) bPlaying = true; }
void ASimReplay::PauseReplay() { bPlaying = false; }
bool ASimReplay::Seek(float Seconds)
{
    if (!FMath::IsFinite(Seconds) || Tracks.IsEmpty()) return false;
    PlaybackTime = FMath::Clamp(Seconds, 0.f, Duration); UpdateVisuals(); return true;
}
void ASimReplay::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if (!bPlaying) return;
    const float Rate = FMath::IsFinite(PlaybackRate) ? FMath::Clamp(PlaybackRate, .05f, 8.f) : 1.f;
    const float From = PlaybackTime;
    float Next = PlaybackTime + FMath::Max(DeltaTime, 0.f) * Rate;
    if (Next >= Duration)
    {
        if (bLoop && Duration > 0) { Next = FMath::Fmod(Next, Duration); PlayEventsBetween(From, Duration); PlayEventsBetween(-1., Next); }
        else { Next = Duration; bPlaying = false; PlayEventsBetween(From, Duration); }
    }
    else PlayEventsBetween(From, Next);
    Seek(Next);
}
void ASimReplay::PlayEventsBetween(double From, double To)
{
    // Half-open interval (From, To]; an event at exactly zero plays on the first tick.
    if (From == 0. && To > 0.) From = -1.;
    for (const FSimReplayEvent& Event : Events)
    {
        if (Event.Time <= From) continue;
        if (Event.Time > To) break;
        if (!SimEvents::IsReplayable(Event.Type)) continue;
        ++EffectsPlayed;
        if (bReplayEffects) SimEvents::PlayEffect(GetWorld(), Event.Type, Event.Location);
    }
}
void ASimReplay::UpdateVisuals()
{
    for (int32 I = 0; I < Tracks.Num(); ++I)
    {
        const auto& Track = Tracks[I]; UMeshComponent* Visual = Visuals[I];
        int32 Low = 0, High = Track.Poses.Num();
        while (Low < High) { const int32 Mid = (Low + High) / 2; if (Track.Poses[Mid].Time <= PlaybackTime) Low = Mid + 1; else High = Mid; }
        const int32 A = FMath::Max(0, Low - 1), B = FMath::Min(A + 1, Track.Poses.Num() - 1);
        const auto& Left = Track.Poses[A]; const auto& Right = Track.Poses[B];
        const float Alpha = Right.Time > Left.Time ? FMath::Clamp(float((PlaybackTime - Left.Time) / (Right.Time - Left.Time)), 0.f, 1.f) : 0.f;
        FTransform Pose; Pose.Blend(Left.Transform, Right.Transform, Alpha);
        Visual->SetWorldTransform(Track.VisualTransform * Pose);
        Visual->SetVisibility(!Left.bHidden && PlaybackTime >= Track.Poses[0].Time && PlaybackTime <= Track.Poses.Last().Time);
        if (auto* Mesh = Cast<USkeletalMeshComponent>(Visual); Mesh && !Left.Animation.IsEmpty())
        {
            UAnimationAsset* Animation = LoadObject<UAnimationAsset>(nullptr, *Left.Animation);
            if (!Left.FootSupports.IsEmpty())
            {
                if (!Cast<ULivingHumanAnimation>(Mesh->GetAnimInstance())) Mesh->SetAnimInstanceClass(ULivingHumanAnimation::StaticClass());
                if (auto* Human = Cast<ULivingHumanAnimation>(Mesh->GetAnimInstance()))
                {
                    Human->bReplaySupports = true;
                    if (Human->GetCurrentAsset() != Animation) Human->SetAnimationAsset(Animation, false);
                    Human->FootSupports = Left.FootSupports;
                    for (FLivingFootSupport& Foot : Human->FootSupports)
                        if (const auto* Next = Right.FootSupports.FindByPredicate([&](const FLivingFootSupport& Other) { return Other.Bone == Foot.Bone; }))
                        {
                            Foot.HeightCm = FMath::Lerp(Foot.HeightCm, Next->HeightCm, Alpha);
                            Foot.StanceOffsetCm = FMath::Lerp(Foot.StanceOffsetCm, Next->StanceOffsetCm, Alpha);
                        }
                }
            }
            else if (!Left.WheelPoses.IsEmpty())
            {
                if (!Cast<ULivingVehicleAnimation>(Mesh->GetAnimInstance())) Mesh->SetAnimInstanceClass(ULivingVehicleAnimation::StaticClass());
                if (auto* Vehicle = Cast<ULivingVehicleAnimation>(Mesh->GetAnimInstance()))
                {
                    if (Vehicle->GetCurrentAsset() != Animation) Vehicle->SetAnimationAsset(Animation, false);
                    Vehicle->WheelPoses = Left.WheelPoses;
                    for (FLivingWheelPose& Wheel : Vehicle->WheelPoses)
                    {
                        const FLivingWheelPose* Next = Right.WheelPoses.FindByPredicate([&](const FLivingWheelPose& Other) { return Other.Bone == Wheel.Bone; });
                        if (Next)
                        {
                            Wheel.Offset = FMath::Lerp(Wheel.Offset, Next->Offset, Alpha);
                            Wheel.SteeringDegrees = FMath::Lerp(Wheel.SteeringDegrees, Next->SteeringDegrees, Alpha);
                        }
                    }
                }
            }
            else if (!Mesh->GetSingleNodeInstance() || Cast<ULivingVehicleAnimation>(Mesh->GetAnimInstance()) || Cast<ULivingHumanAnimation>(Mesh->GetAnimInstance()) || Mesh->GetSingleNodeInstance()->GetCurrentAsset() != Animation)
                Mesh->PlayAnimation(Animation, false);
            const float Position = Left.Animation == Right.Animation && Right.AnimationTime >= Left.AnimationTime ? FMath::Lerp(Left.AnimationTime, Right.AnimationTime, Alpha) : Left.AnimationTime;
            if (UAnimSingleNodeInstance* Instance = Mesh->GetSingleNodeInstance())
            {
                const FVector BlendPosition = Left.Animation == Right.Animation ? FMath::Lerp(Left.BlendPosition, Right.BlendPosition, Alpha) : Left.BlendPosition;
                Instance->SetBlendSpacePosition(BlendPosition);
                Instance->UpdateBlendspaceSamples(BlendPosition);
            }
            Mesh->SetPosition(Position, false); Mesh->SetPlayRate(0);
        }
    }
}
