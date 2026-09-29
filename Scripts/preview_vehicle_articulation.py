"""Temporary engine preview: neutral wheels beside a right-turn wheel pose."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()=='LivingWorldDemo'
assert not globals().get('vehicle_review_visuals'), 'Finish the earlier preview first'
profile=unreal.load_asset('/Game/LivingWorld/Profiles/DA_Jeep')
globals()['vehicle_review_visuals']=[]
for index,steering in enumerate(((0,0),(30,30))):
    actor=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(0,index*520,0),profile.visual_rotation)
    actor.set_actor_label('LivingVehicleReview_'+('Neutral' if index==0 else 'SteeringRange'))
    vehicle_review_visuals.append(actor)
    mesh=actor.get_component_by_class(unreal.SkeletalMeshComponent)
    mesh.set_skeletal_mesh_asset(profile.skeletal_mesh)
    mesh.set_anim_instance_class(unreal.LivingVehicleAnimation)
    animation=mesh.get_anim_instance()
    animation.set_animation_asset(profile.cruise_animation,True,0)
    poses=[]
    for i,wheel in enumerate(profile.wheels):
        pose=unreal.LivingWheelPose()
        pose.bone=wheel.bone
        pose.steering_degrees=steering[i] if i<2 else 0
        poses.append(pose)
    animation.wheel_poses=poses
    animation.set_playing(False)
    animation.set_position(0,False)
    mesh.set_update_animation_in_editor(True)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(720,850,350),unreal.Rotator(pitch=-18,yaw=-140))
print('VEHICLE_PREVIEW_READY')
