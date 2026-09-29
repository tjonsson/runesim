"""Temporary editor comparison: four forced LODs for both imported humanoids."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert 'LivingWorldDemo' in unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()
existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
for row,kind in enumerate(('Civilian','Soldier')):
    folder='/Game/LivingWorld/Models/'+kind
    for lod in range(4):
        label=f'LivingLODReview_{kind}_{lod}'
        actor=existing.get(label) or actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(row*220,lod*145,0),transient=True)
        actor.set_actor_label(label)
        component=actor.get_component_by_class(unreal.SkeletalMeshComponent)
        component.set_skeletal_mesh_asset(unreal.load_asset(folder+'/SK_'+kind))
        component.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
        component.set_animation(unreal.load_asset(folder+'/'+kind+'_Walk'))
        component.set_position(.6,False)
        component.set_update_animation_in_editor(True)
        component.set_forced_lod(lod+1)
        actor.set_actor_rotation(unreal.Rotator(yaw=-90),False)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(1050,170,420),unreal.Rotator(pitch=-19,yaw=180))
print('LOD_REVIEW_READY; temporary placements, no map save')
