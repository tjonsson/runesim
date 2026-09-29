"""Temporary authoring cameras with the same corridor coverage as runtime."""
import unreal
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel' and not unreal.EditorLevelLibrary.get_pie_worlds(False)
manager=unreal.CesiumCameraManager.get_default_camera_manager(world)
for old in globals().get('main_authoring_cameras',[]):manager.remove_camera(old)
globals()['main_authoring_cameras']=[]
for route in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingRoute):
    center,extent=route.get_actor_bounds(False)
    camera=unreal.CesiumCamera()
    camera.viewport_size=unreal.Vector2D(1280,1280)
    camera.location=center+unreal.Vector(0,0,max(3000,extent.length()*1.6))
    camera.rotation=unreal.Rotator(pitch=-90,yaw=0)
    camera.field_of_view_degrees=90
    main_authoring_cameras.append(manager.add_camera(camera))
print('AUTHORING_CAMERAS',main_authoring_cameras)
