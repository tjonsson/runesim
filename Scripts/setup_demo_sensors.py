import unreal
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not any(isinstance(a,unreal.SimPTZ) for a in actors.get_all_level_actors()):
    ptz=actors.spawn_actor_from_class(unreal.SimPTZ,unreal.Vector(-1500,-1500,0))
    ptz.set_actor_label('Simulated PTZ tripod')
    ptz.set_editor_property('camera_id','ptz-1')
    ptz.set_ptz(45,10,60)
levels.save_current_level()
print('PTZ_DEMO_READY')
