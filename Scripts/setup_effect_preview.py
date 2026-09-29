"""Prepare an editable engine-native Niagara burst while Fab acquisition is pending.

This is a starter effect for look development, not the requested realism pack.
The preview placement is transient and never saved into the simulation map.
"""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
source='/Niagara/DefaultAssets/Templates/Systems/SimpleExplosion'
destination='/Game/LivingWorld/Effects/NS_ImpactBurst'
system=unreal.load_asset(destination)
if not system:system=unreal.EditorAssetLibrary.duplicate_asset(source,destination)
assert isinstance(system,unreal.NiagaraSystem)
unreal.EditorAssetLibrary.set_metadata_tag(system,'Source',source)
unreal.EditorAssetLibrary.set_metadata_tag(system,'QualityStatus','Engine template starter; production smoke/electrical effects pending')
unreal.EditorAssetLibrary.save_loaded_asset(system)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actor=next((a for a in actors.get_all_level_actors() if a.get_actor_label()=='LivingFXReview'),None)
if not actor:actor=actors.spawn_actor_from_class(unreal.NiagaraActor,unreal.Vector(0,0,100),transient=True)
actor.set_actor_label('LivingFXReview')
component=actor.get_component_by_class(unreal.NiagaraComponent)
component.set_asset(system)
component.set_auto_activate(True)
component.activate(True)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(700,-700,420),unreal.Rotator(pitch=-20,yaw=135))
print('NIAGARA_STARTER_PREVIEW_READY',destination)
