"""Enable the seven inspected airborne profiles and retire duplicate demo birds."""
import unreal
import json
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
entries=json.loads((Path(unreal.Paths.project_saved_dir())/'LivingWorld/flight-import.json').read_text())
for entry in entries:
    profile=unreal.load_asset(entry['profile'])
    assert profile.skeletal_mesh and profile.cruise_animation,entry['name']
    assert len(entry['vertices_per_lod'])==4
    profile.set_editor_property('approved',True)
    unreal.EditorAssetLibrary.save_loaded_asset(profile)
duplicate=unreal.load_asset('/Game/LivingWorld/Demo/DA_TestBird')
if duplicate:
    duplicate.set_editor_property('approved',False)
    unreal.EditorAssetLibrary.save_loaded_asset(duplicate)
print('FLIGHT_PROFILES_ENABLED', [e['name'] for e in entries])
