import unreal
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
    if actor.get_actor_label().startswith('LivingLODReview_'):actor.destroy_actor()
print('Temporary LOD review placements removed')
