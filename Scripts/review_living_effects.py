"""Capture saved effects at useful ages and verify every one-shot terminates."""
import unreal
import json
from pathlib import Path
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
root=Path(unreal.Paths.project_saved_dir())/'LivingWorld'
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
entries=[('NS_SmokePuff',2.),('NS_Explosion',.06),('NS_Explosion',2.),('NS_ElectricalSparks',.15)]
state={'index':0,'elapsed':0.,'actor':None,'report':[]}
camera=actors.spawn_actor_from_class(unreal.SceneCapture2D,unreal.Vector(950,-950,550),transient=True)
camera.set_actor_rotation(unreal.MathLibrary.find_look_at_rotation(camera.get_actor_location(),unreal.Vector(0,0,180)),False)
capture=camera.get_component_by_class(unreal.SceneCaptureComponent2D);capture.fov_angle=50
capture.capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
target=unreal.RenderingLibrary.create_render_target2d(world,960,720,unreal.TextureRenderTargetFormat.RTF_RGBA8);capture.texture_target=target
def tick(dt):
    state['elapsed']+=dt
    if state['elapsed']<1:return
    state['elapsed']=0
    if state['actor']:
        name,age=entries[state['index']-1]
        unreal.RenderingLibrary.export_render_target(world,target,str(root),f'{name}-{age}.png')
        component=state['actor'].get_component_by_class(unreal.NiagaraComponent)
        component.set_paused(False);component.advance_simulation(300,1/30)
        state['report'].append({'name':name,'age':age,'stopped_after_10s':not component.is_active()})
        actors.destroy_actor(state['actor']);state['actor']=None
    if state['index']==len(entries):
        actors.destroy_actor(camera);unreal.unregister_slate_post_tick_callback(state['handle'])
        (root/'effects-review.json').write_text(json.dumps(state['report'],indent=2));print('EFFECTS_REVIEWED',state['report']);return
    name,age=entries[state['index']];state['index']+=1
    actor=actors.spawn_actor_from_class(unreal.NiagaraActor,unreal.Vector(0,0,100),transient=True)
    component=actor.get_component_by_class(unreal.NiagaraComponent)
    component.set_asset(unreal.load_asset('/Game/LivingWorld/Effects/'+name))
    component.reinitialize_system();component.advance_simulation(round(age*60),1/60);component.set_paused(True)
    state['actor']=actor
state['handle']=unreal.register_slate_post_tick_callback(tick)
