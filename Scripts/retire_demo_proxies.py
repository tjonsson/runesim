"""Canonical profiles now provide the demo's people, cars and birds."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
for name in ('Civilian','Soldier','Car','Bird'):
    profile=unreal.load_asset('/Game/LivingWorld/Demo/DA_Test'+name)
    if profile:
        profile.set_editor_property('approved',False)
        unreal.EditorAssetLibrary.save_loaded_asset(profile,False)
print('LEGACY_DEMO_PROFILES_RETIRED')
