import unreal,json
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
for route in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingRoute):
    if 'LivingWorld.MainCorridor' not in [str(t) for t in route.tags]:continue
    route.modify();route.retire_at_ends=True
    data=json.loads(route.provenance);data['open_exits']=True;route.provenance=json.dumps(data)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('MAIN_EXITS_ENABLED')
