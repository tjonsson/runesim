"""Read-only inventory of the main Cesium scene for Living World integration."""
import json
from pathlib import Path
import unreal

assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert world.get_name()=='MainLevel'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
def xyz(v):return [v.x,v.y,v.z]
report={'map':world.get_path_name(),'actors':[]}
for actor in actors:
    row={'name':actor.get_actor_label(),'class':actor.get_class().get_path_name(),'location':xyz(actor.get_actor_location())}
    if isinstance(actor,unreal.CesiumGeoreference):
        row['origin']=[actor.get_editor_property(key) for key in ('origin_longitude','origin_latitude','origin_height')]
    if isinstance(actor,unreal.Cesium3DTileset):
        row['tiles']={key:str(actor.get_editor_property(key)) for key in ('ion_asset_id','create_physics_meshes','maximum_screen_space_error','tileset_source')}
    if isinstance(actor,unreal.LivingRoute):
        row['route']={'validated':actor.validated,'vehicles':actor.vehicles,'points':actor.path.get_number_of_spline_points(),'length_cm':actor.path.get_spline_length(),'reviewed_half_width_cm':actor.reviewed_half_width_cm}
    if isinstance(actor,unreal.SimPTZ):row['camera']={'id':actor.camera_id,'auto_start':actor.stream.auto_start,'ros':actor.enable_ros}
    report['actors'].append(row)
output=Path(unreal.Paths.project_saved_dir())/'LivingWorld/main-inventory.json'
output.write_text(json.dumps(report,indent=2))
print('MAIN_INVENTORY',report)
