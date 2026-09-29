"""Render each profile in Unreal with its real imported materials and animation."""
import unreal
import json
from pathlib import Path

assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert 'LivingWorldDemo' in world.get_name()
entries=json.loads((Path(unreal.Paths.project_saved_dir())/'LivingWorld/flight-import.json').read_text())
if unreal.load_asset('/Game/LivingWorld/Profiles/DA_Jeep'):
    entries.append({'name':'Jeep','profile':'/Game/LivingWorld/Profiles/DA_Jeep'})
review={'index':0,'time':0.,'actor':None,'report':[]}
capture_actor=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(1000,-1100,1000),transient=True)
capture=capture_actor.get_component_by_class(unreal.SceneCaptureComponent2D)
capture.fov_angle=45.
target=unreal.RenderingLibrary.create_render_target2d(world,960,720,unreal.TextureRenderTargetFormat.RTF_RGBA8)
capture.texture_target=target
capture.capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
capture_actor.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(capture_actor.get_actor_location(),unreal.Vector(0,0,500)),False)

def review_tick(dt):
    review['time']+=dt
    if review['time']<1.:return
    review['time']=0.
    if review['actor']:
        name=entries[review['index']-1]['name']
        capture.capture_scene()
        unreal.RenderingLibrary.export_render_target(world,target,str(Path(unreal.Paths.project_saved_dir())/'LivingWorld'),name+'-unreal.png')
        actors.destroy_actor(review['actor']); review['actor']=None
    if review['index']==len(entries):
        actors.destroy_actor(capture_actor)
        unreal.unregister_slate_post_tick_callback(review['handle'])
        (Path(unreal.Paths.project_saved_dir())/'LivingWorld/flight-review.json').write_text(json.dumps(review['report'],indent=2))
        print('FLIGHT_RENDER_COMPLETE');return
    entry=entries[review['index']]; profile=unreal.load_asset(entry['profile'])
    mesh=profile.skeletal_mesh
    actor=actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(0,0,500),transient=True)
    actor.set_actor_label('FlightReview_'+entry['name'])
    component=actor.get_component_by_class(unreal.SkeletalMeshComponent)
    component.set_skeletal_mesh_asset(mesh)
    component.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    component.set_animation(profile.cruise_animation); component.set_position(.05,False)
    component.set_update_animation_in_editor(True); component.set_forced_lod(1)
    size=mesh.get_bounds().box_extent
    scale=650./(max(size.x,size.y,size.z)*2)
    actor.set_actor_scale3d(unreal.Vector(scale,scale,scale))
    actor.set_actor_rotation(profile.visual_rotation,False)
    review['actor']=actor; review['index']+=1
    review['report'].append({'name':entry['name'],'bones':component.get_num_bones(),'material_count':component.get_num_materials(),
        'socket_positions':{str(component.get_bone_name(i)):str(component.get_socket_location(component.get_bone_name(i))) for i in range(component.get_num_bones())}})

review['handle']=unreal.register_slate_post_tick_callback(review_tick)
print('FLIGHT_RENDER_STARTED')
