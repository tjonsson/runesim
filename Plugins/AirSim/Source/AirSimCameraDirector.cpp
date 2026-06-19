#include "AirSimCameraDirector.h"
#include "GameFramework/PlayerController.h"
#include "AirBlueprintLib.h"

AAirSimCameraDirector::AAirSimCameraDirector()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create a spring arm component for our chase camera
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    this->SetRootComponent(SpringArm);
    SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 34.0f));
    SpringArm->SetWorldRotation(FRotator(-20.0f, 0.0f, 0.0f));
    SpringArm->TargetArmLength = 125.0f;
    SpringArm->bEnableCameraLag = false;
    SpringArm->bEnableCameraRotationLag = false;
    SpringArm->CameraRotationLagSpeed = 10.0f;
    SpringArm->bInheritPitch = true;
    SpringArm->bInheritYaw = true;
    SpringArm->bInheritRoll = true;
}

void AAirSimCameraDirector::BeginPlay()
{
    Super::BeginPlay();
}

void AAirSimCameraDirector::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (mode_ == ECameraDirectorMode::CAMERA_DIRECTOR_MODE_MANUAL) {
        manual_pose_controller_->updateActorPose(DeltaTime);
    }
    else if (mode_ == ECameraDirectorMode::CAMERA_DIRECTOR_MODE_SPRINGARM_CHASE) {
        //do nothing, spring arm is pulling the camera with it
    }
    else if (mode_ == ECameraDirectorMode::CAMERA_DIRECTOR_MODE_NODISPLAY) {
        //do nothing, we have camera turned off
    }
    else { //make camera move in desired way
        UAirBlueprintLib::FollowActor(ExternalCamera, follow_actor_, initial_ground_obs_offset_, ext_obs_fixed_z_);
    }
}

ECameraDirectorMode AAirSimCameraDirector::getMode()
{
    return mode_;
}

void AAirSimCameraDirector::initializeForBeginPlay(ECameraDirectorMode view_mode,
                                             AActor* follow_actor, APIPCamera* fpv_camera, APIPCamera* front_camera, APIPCamera* back_camera, const TArray<APIPCamera*>& cycle_cameras)
{
    manual_pose_controller_ = NewObject<UManualPoseController>(this, "CameraDirector_ManualPoseController");
    manual_pose_controller_->initializeForPlay();

    setupInputBindings();

    mode_ = view_mode;

    follow_actor_ = follow_actor;
    fpv_camera_ = fpv_camera;
    front_camera_ = front_camera;
    backup_camera_ = back_camera;
    setCycleCameras(cycle_cameras);
    camera_start_location_ = ExternalCamera->GetActorLocation();
    camera_start_rotation_ = ExternalCamera->GetActorRotation();
    initial_ground_obs_offset_ = camera_start_location_ -
                                 (follow_actor_ ? follow_actor_->GetActorLocation() : FVector::ZeroVector);

    //set initial view mode
    switch (mode_) {
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_FLY_WITH_ME:
        inputEventFlyWithView();
        break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_FPV:
        inputEventFpvView();
        break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_GROUND_OBSERVER:
        inputEventGroundView();
        break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_MANUAL:
        inputEventManualView();
        break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_SPRINGARM_CHASE:
        inputEventSpringArmChaseView();
        break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_BACKUP:
        inputEventBackupView();
        break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_NODISPLAY:
        inputEventNoDisplayView();
        break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_FRONT:
        inputEventFrontView();
        break;
    case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_DOWN:
        inputEventCycleView();
        break;
    default:
        throw std::out_of_range("Unsupported view mode specified in CameraDirector::initializeForBeginPlay");
    }
}

void AAirSimCameraDirector::attachSpringArm(bool attach)
{
    if (attach) {
        //If we do have actor to follow AND don't have sprint arm attached to that actor, we will attach it
        if (follow_actor_ && ExternalCamera->GetRootComponent()->GetAttachParent() != SpringArm) {
            //For car, we want a bit of camera lag, as that is customary of racing video games
            //If the lag is missing, the camera will also occasionally shake.
            //But, lag is not desired when piloting a drone
            SpringArm->bEnableCameraRotationLag = camera_rotation_lag_enabled_;

            //attach spring arm to actor
            SpringArm->AttachToComponent(follow_actor_->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
            SpringArm->SetRelativeLocation(FVector(0.0f, 0.0f, 34.0f));

            //remember current parent for external camera. Later when we remove external
            //camera from spring arm, we will attach it back to its last parent
            last_parent_ = ExternalCamera->GetRootComponent()->GetAttachParent();
            ExternalCamera->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
            //now attach camera to spring arm
            ExternalCamera->AttachToComponent(SpringArm, FAttachmentTransformRules::KeepRelativeTransform);
        }

        //For car, we need to move the camera back a little more than for a drone.
        //Otherwise, the camera will be stuck inside the car
        ExternalCamera->SetActorRelativeLocation(FVector(follow_distance_ * 100.0f, 0.0f, 0.0f));
        ExternalCamera->SetActorRelativeRotation(FRotator(10.0f, 0.0f, 0.0f));
        //ExternalCamera->bUsePawnControlRotation = false;
    }
    else { //detach
        if (last_parent_ && ExternalCamera->GetRootComponent()->GetAttachParent() == SpringArm) {
            ExternalCamera->DetachFromActor(FDetachmentTransformRules::KeepRelativeTransform);
            ExternalCamera->AttachToComponent(last_parent_, FAttachmentTransformRules::KeepRelativeTransform);
        }
    }
}

void AAirSimCameraDirector::setMode(ECameraDirectorMode mode)
{
    { //first remove any settings done by previous mode

        //detach spring arm
        if (mode_ == ECameraDirectorMode::CAMERA_DIRECTOR_MODE_SPRINGARM_CHASE &&
            mode != ECameraDirectorMode::CAMERA_DIRECTOR_MODE_SPRINGARM_CHASE) {
            attachSpringArm(false);
        }

        // Re-enable rendering
        if (mode_ == ECameraDirectorMode::CAMERA_DIRECTOR_MODE_NODISPLAY &&
            mode != ECameraDirectorMode::CAMERA_DIRECTOR_MODE_NODISPLAY) {
            UAirBlueprintLib::enableViewportRendering(this, true);
        }

        //Remove any existing key bindings for manual mode
        if (mode != ECameraDirectorMode::CAMERA_DIRECTOR_MODE_MANUAL) {
            if (ExternalCamera != nullptr && manual_pose_controller_->getActor() == ExternalCamera) {

                manual_pose_controller_->setActor(nullptr);
            }
            //else someone else is bound to manual pose controller, leave it alone
        }
    }

    { //perform any settings to enter in to this mode

        switch (mode) {
        case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_MANUAL:
            //if new mode is manual mode then add key bindings
            manual_pose_controller_->setActor(ExternalCamera);
            break;
        case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_SPRINGARM_CHASE:
            //if we switched to spring arm mode then attach to spring arm (detachment was done earlier in method)
            attachSpringArm(true);
            break;
        case ECameraDirectorMode::CAMERA_DIRECTOR_MODE_NODISPLAY:
            UAirBlueprintLib::enableViewportRendering(this, false);
            break;
        default:
            //other modes don't need special setup
            break;
        }
    }

    //make switch official
    mode_ = mode;
}

void AAirSimCameraDirector::setupInputBindings()
{
    UAirBlueprintLib::EnableInput(this);

    UAirBlueprintLib::BindActionToKey("inputEventFpvView", EKeys::F, this, &AAirSimCameraDirector::inputEventFpvView);
    UAirBlueprintLib::BindActionToKey("inputEventFlyWithView", EKeys::B, this, &AAirSimCameraDirector::inputEventFlyWithView);
    UAirBlueprintLib::BindActionToKey("inputEventGroundView", EKeys::G, this, &AAirSimCameraDirector::inputEventGroundView);
    UAirBlueprintLib::BindActionToKey("inputEventManualView", EKeys::M, this, &AAirSimCameraDirector::inputEventManualView);
    UAirBlueprintLib::BindActionToKey("inputEventSpringArmChaseView", EKeys::O, this, &AAirSimCameraDirector::inputEventSpringArmChaseView);
    UAirBlueprintLib::BindActionToKey("inputEventBackupView", EKeys::K, this, &AAirSimCameraDirector::inputEventBackupView);
    UAirBlueprintLib::BindActionToKey("inputEventNoDisplayView", EKeys::Hyphen, this, &AAirSimCameraDirector::inputEventNoDisplayView);
    UAirBlueprintLib::BindActionToKey("inputEventFrontView", EKeys::I, this, &AAirSimCameraDirector::inputEventFrontView);
    // C key is handled by VRSpectator to use the unified camera cycle list
}

void AAirSimCameraDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    manual_pose_controller_ = nullptr;
    SpringArm = nullptr;
    ExternalCamera = nullptr;
    fpv_camera_ = nullptr;
    backup_camera_ = nullptr;
    front_camera_ = nullptr;
    follow_actor_ = nullptr;
    cycle_cameras_.Empty();
    cycle_camera_index_ = -1;
}

APIPCamera* AAirSimCameraDirector::getFpvCamera() const
{
    return fpv_camera_;
}

APIPCamera* AAirSimCameraDirector::getExternalCamera() const
{
    return ExternalCamera;
}

APIPCamera* AAirSimCameraDirector::getBackupCamera() const
{
    return backup_camera_;
}

APIPCamera* AAirSimCameraDirector::getFrontCamera() const
{
    return front_camera_;
}

void AAirSimCameraDirector::inputEventSpringArmChaseView()
{
    if (ExternalCamera) {
        setMode(ECameraDirectorMode::CAMERA_DIRECTOR_MODE_SPRINGARM_CHASE);
        ExternalCamera->showToScreen();
        disableAllManagedCameras(ExternalCamera);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "ExternalCamera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void AAirSimCameraDirector::inputEventGroundView()
{
    if (ExternalCamera) {
        setMode(ECameraDirectorMode::CAMERA_DIRECTOR_MODE_GROUND_OBSERVER);
        ExternalCamera->showToScreen();
        disableAllManagedCameras(ExternalCamera);
        ext_obs_fixed_z_ = true;
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "ExternalCamera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void AAirSimCameraDirector::inputEventManualView()
{
    if (ExternalCamera) {
        setMode(ECameraDirectorMode::CAMERA_DIRECTOR_MODE_MANUAL);
        ExternalCamera->showToScreen();
        disableAllManagedCameras(ExternalCamera);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "ExternalCamera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void AAirSimCameraDirector::inputEventNoDisplayView()
{
    if (ExternalCamera) {
        setMode(ECameraDirectorMode::CAMERA_DIRECTOR_MODE_NODISPLAY);
        disableAllManagedCameras(nullptr);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "ExternalCamera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void AAirSimCameraDirector::inputEventBackupView()
{
    if (backup_camera_) {
        setMode(ECameraDirectorMode::CAMERA_DIRECTOR_MODE_BACKUP);
        backup_camera_->showToScreen();
        disableAllManagedCameras(backup_camera_);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "backup_camera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void AAirSimCameraDirector::inputEventFrontView()
{
    if (front_camera_) {
        setMode(ECameraDirectorMode::CAMERA_DIRECTOR_MODE_FRONT);
        front_camera_->showToScreen();
        disableAllManagedCameras(front_camera_);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "backup_camera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void AAirSimCameraDirector::inputEventFlyWithView()
{
    if (ExternalCamera) {
        setMode(ECameraDirectorMode::CAMERA_DIRECTOR_MODE_FLY_WITH_ME);
        ExternalCamera->showToScreen();

        if (follow_actor_)
            ExternalCamera->SetActorLocationAndRotation(
                follow_actor_->GetActorLocation() + initial_ground_obs_offset_, camera_start_rotation_);
        disableAllManagedCameras(ExternalCamera);
        ext_obs_fixed_z_ = false;
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "ExternalCamera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void AAirSimCameraDirector::inputEventFpvView()
{
    if (fpv_camera_) {
        setMode(ECameraDirectorMode::CAMERA_DIRECTOR_MODE_FPV);
        fpv_camera_->showToScreen();
        disableAllManagedCameras(fpv_camera_);
    }
    else
        UAirBlueprintLib::LogMessageString("Camera is not available: ", "fpv_camera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void AAirSimCameraDirector::inputEventCycleView()
{
    if (cycle_cameras_.Num() > 0) {
        int start_index = cycle_camera_index_;
        APIPCamera* selected_camera = nullptr;
        for (int offset = 1; offset <= cycle_cameras_.Num(); ++offset) {
            int idx = (start_index + offset) % cycle_cameras_.Num();
            if (cycle_cameras_[idx]) {
                cycle_camera_index_ = idx;
                selected_camera = cycle_cameras_[idx];
                break;
            }
        }

        if (selected_camera) {
            setMode(ECameraDirectorMode::CAMERA_DIRECTOR_MODE_DOWN);
            selected_camera->showToScreen();
            disableAllManagedCameras(selected_camera);
        }
        else {
            UAirBlueprintLib::LogMessageString("Camera is not available: ", "cycle_camera", LogDebugLevel::Failure);
        }
    }
    else
        UAirBlueprintLib::LogMessageString("Camera list is empty: ", "cycle_camera", LogDebugLevel::Failure);

    notifyViewModeChanged();
}

void AAirSimCameraDirector::disableAllManagedCameras(APIPCamera* keep)
{
    TSet<APIPCamera*> cameras;
    if (fpv_camera_)
        cameras.Add(fpv_camera_);
    if (backup_camera_)
        cameras.Add(backup_camera_);
    if (front_camera_)
        cameras.Add(front_camera_);
    for (APIPCamera* camera : cycle_cameras_) {
        if (camera)
            cameras.Add(camera);
    }

    for (APIPCamera* camera : cameras) {
        if (camera && camera != keep)
            camera->disableMain();
    }
}

void AAirSimCameraDirector::setCycleCameras(const TArray<APIPCamera*>& cameras)
{
    cycle_cameras_.Reset();
    TSet<APIPCamera*> unique_cameras;
    for (APIPCamera* camera : cameras) {
        if (camera && !unique_cameras.Contains(camera)) {
            unique_cameras.Add(camera);
            cycle_cameras_.Add(camera);
        }
    }
    cycle_camera_index_ = -1;
}

void AAirSimCameraDirector::notifyViewModeChanged()
{
    bool nodisplay = ECameraDirectorMode::CAMERA_DIRECTOR_MODE_NODISPLAY == mode_;

    UWorld* world = GetWorld();
    UGameViewportClient* gameViewport = world->GetGameViewport();
    gameViewport->bDisableWorldRendering = nodisplay;
}
