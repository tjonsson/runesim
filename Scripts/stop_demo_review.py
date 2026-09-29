"""Restore the integration test's previous settings and stop its demo PIE session.

Run through ue_remote.py in the same editor session as test_living_world_pie.py.
"""
import unreal

previous = globals().pop('living_demo_previous_options', globals().get('original'))
if previous is not None:
    for subsystem in unreal.ObjectIterator(unreal.LivingWorldSubsystem):
        if 'UEDPIE' in subsystem.get_path_name():
            subsystem.apply_options(previous)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
