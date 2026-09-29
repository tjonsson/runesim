"""Read actual MainLevel corridor support; do not persist validation or change geometry."""
import unreal, json, math
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel' and not unreal.EditorLevelLibrary.get_pie_worlds(False)
routes=[a for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors() if isinstance(a,unreal.LivingRoute) and 'LivingWorld.MainCorridor' in [str(t) for t in a.tags]]
reports=[]
for route in routes:
    old=(route.validated,route.reviewed_half_width_cm)
    samples=[]
    try:
        route.validated=True;route.reviewed_half_width_cm=200 if route.vehicles else 85
        length=route.path.get_spline_length()
        for i in range(math.ceil(length/50)+1):
            distance=min(length,i*50)
            sides=(0,) if route.vehicles else (-42.5,0,42.5)
            passed=all(route.sample_ground(distance,side,105 if route.vehicles else 30) is not None for side in sides)
            samples.append({'distance_cm':distance,'passed':passed})
        reports.append({'actor':route.get_actor_label(),'vehicles':route.vehicles,'length_cm':length,'width_cm':route.reviewed_half_width_cm*2,'passed':sum(s['passed'] for s in samples),'total':len(samples),'samples':samples})
    finally:route.validated,route.reviewed_half_width_cm=old
folder=Path(unreal.Paths.project_saved_dir())/'LivingWorld/MainRoutes'
(folder/'corridor-support.json').write_text(json.dumps(reports,indent=2))
print('MAIN_SUPPORT', [{k:v for k,v in r.items() if k!='samples'} for r in reports])
