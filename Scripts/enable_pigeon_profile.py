"""Apply the reviewed distant-flock pigeon to production and demo bird profiles."""
import unreal

profile = unreal.load_asset('/Game/LivingWorld/Profiles/DA_Pigeon')
assert profile and profile.get_editor_property('skeletal_mesh')
profile.set_editor_property('visual_rotation', unreal.Rotator(yaw=90))
profile.set_editor_property('approved', True)
unreal.EditorAssetLibrary.save_loaded_asset(profile)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
for asset in registry.get_assets_by_path('/Game/LivingWorld/Demo', recursive=True):
    demo = asset.get_asset()
    if isinstance(demo, unreal.LivingAssetProfile) and demo.get_editor_property('kind') == unreal.LivingKind.BIRD:
        for key in ['skeletal_mesh', 'cruise_animation', 'source', 'license', 'scale_evidence', 'visual_rotation', 'speed_meters_per_second', 'collision_radius_cm']:
            demo.set_editor_property(key, profile.get_editor_property(key))
        demo.set_editor_property('static_mesh', None)
        demo.set_editor_property('visual_scale', unreal.Vector(1, 1, 1))
        unreal.EditorAssetLibrary.save_loaded_asset(demo)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() == 'Pigeon_AssetReview':
        actors.destroy_actor(actor)
print('PIGEON_PROFILE_APPROVED: distant flock use; five clips imported')
