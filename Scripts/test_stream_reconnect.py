"""Interrupt only the demo PTZ stream for eight seconds, then resume it."""
import unreal
import time
def run_stream_restart_test():
    worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
    assert len(worlds)==1 and 'LivingWorldDemo' in worlds[0].get_name()
    ptz=unreal.GameplayStatics.get_all_actors_of_class(worlds[0],unreal.SimPTZ)[0]
    stream=ptz.get_editor_property('stream')
    stream.stop_stream()
    started=time.monotonic()
    def resume(dt):
        if time.monotonic()-started>=8:
            unreal.unregister_slate_post_tick_callback(handle)
            print('PTZ_RECONNECT_STREAM_RESUMED',stream.start_stream())
    handle=unreal.register_slate_post_tick_callback(resume)
    print('PTZ_RECONNECT_TEST: eight-second stream interruption')
run_stream_restart_test()
