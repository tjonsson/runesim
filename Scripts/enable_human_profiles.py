"""Enable visually reviewed humanoids and replace the demo's human proxies."""
import unreal
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
registry = unreal.AssetRegistryHelpers.get_asset_registry()
keys = ['skeletal_mesh','cruise_animation','flee_animation','idle_animation','source','license','scale_evidence',
        'visual_rotation','visual_offset','speed_meters_per_second','collision_radius_cm','ground_clearance_cm']
for kind, category in [('Civilian',unreal.LivingKind.CIVILIAN),('Soldier',unreal.LivingKind.SOLDIER)]:
    profile = unreal.load_asset('/Game/LivingWorld/Profiles/DA_'+kind)
    profile.set_editor_property('visual_rotation',unreal.Rotator(yaw=-90))
    profile.set_editor_property('idle_animation',unreal.load_asset('/Game/LivingWorld/Models/'+kind+'/'+kind+'_Idle'))
    profile.set_editor_property('approved',True)
    unreal.EditorAssetLibrary.save_loaded_asset(profile)
    for entry in registry.get_assets_by_path('/Game/LivingWorld/Demo',recursive=True):
        demo = entry.get_asset()
        if isinstance(demo,unreal.LivingAssetProfile) and demo.get_editor_property('kind') == category:
            for key in keys:
                demo.set_editor_property(key,profile.get_editor_property(key))
            demo.set_editor_property('static_mesh',None)
            demo.set_editor_property('visual_scale',unreal.Vector(1,1,1))
            unreal.EditorAssetLibrary.save_loaded_asset(demo)
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() in ('Civilian_AssetReview','Soldier_AssetReview'):
        actors.destroy_actor(actor)
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(340,-280,190),unreal.Rotator(pitch=-13,yaw=135))
