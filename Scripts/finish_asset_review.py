"""Remove only this task's temporary review placements and save owned assets."""
import unreal
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
for actor in actors.get_all_level_actors():
    if actor.get_actor_label() in ('Civilian_AssetReview','Soldier_AssetReview','pigeon 3d model'):
        actors.destroy_actor(actor)
for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()+unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
    if package.get_name().startswith(('/Game/LivingWorld/','/Game/TripoModels/')):
        assert unreal.EditorLoadingAndSavingUtils.save_packages([package],False)
print('OWNED_ASSETS_SAVED')
