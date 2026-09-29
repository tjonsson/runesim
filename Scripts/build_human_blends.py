"""Create reviewed idle/walk/run blend assets; leaves existing clips and source rigs intact."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
for kind in ('Civilian', 'Soldier'):
    profile=unreal.load_asset('/Game/LivingWorld/Profiles/DA_'+kind)
    blend=unreal.LivingAnimationAuthoring.build_locomotion(profile,'BS_'+kind)
    assert blend,kind
    assert unreal.EditorAssetLibrary.save_loaded_asset(blend,False)
    assert unreal.EditorAssetLibrary.save_loaded_asset(profile,False)
    print('LOCOMOTION_BLEND',kind,blend.get_path_name())
