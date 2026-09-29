"""Opt the reviewed four-wheel jeep into procedural steering and contact suspension."""
import json
from pathlib import Path
import unreal

assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
root = Path(unreal.Paths.project_dir())
profile = unreal.load_asset('/Game/LivingWorld/Profiles/DA_Jeep')
manifest = json.loads((root/'Art/LivingWorld/Prepared/Ground/Jeep.json').read_text())
wheels = []
for name in ('LF', 'RF', 'LR', 'RR'):
    wheel = unreal.LivingWheelDefinition()
    wheel.bone = 'wheel_' + name
    wheel.radius_cm = manifest['wheel_radius_m'] * 100
    wheel.steers = name.endswith('F')
    wheels.append(wheel)
profile.modify()
profile.wheels = wheels
profile.suspension_travel_cm = 20
profile.max_wheel_steering_degrees = 35
assert unreal.EditorAssetLibrary.save_loaded_asset(profile, False)
print('VEHICLE_ARTICULATION_SAVED', len(wheels), wheels[0].radius_cm)
