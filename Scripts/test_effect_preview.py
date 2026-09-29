"""Check that the engine-native starter burst reaches completion, then remove preview."""
import unreal
import json
from pathlib import Path
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
actor=next(a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.NiagaraActor) if a.get_actor_label()=='LivingFXReview')
component=actor.get_component_by_class(unreal.NiagaraComponent)
component.set_paused(False)
component.reinitialize_system()
component.advance_simulation(300,1/30)
result={'system':component.get_asset().get_path_name(),'inactive_after_10_seconds':not component.is_active()}
(Path(unreal.Paths.project_saved_dir())/'LivingWorld/effect-preview.json').write_text(json.dumps(result,indent=2))
actor.destroy_actor()
assert result['inactive_after_10_seconds'],result
print('EFFECT_LIFETIME_RESULT',result)
