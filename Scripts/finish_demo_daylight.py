"""Enable dynamic daylight in the standalone airborne acceptance map."""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
assert 'LivingWorldDemo' in world.get_name()
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
# The acceptance map also needs explicit sky geometry for sensor captures.
sky=next((a for a in actors.get_all_level_actors() if a.get_actor_label()=='LivingDemo_SkyDome'),None)
if not sky:
    sky=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector())
    sky.set_actor_label('LivingDemo_SkyDome')
    sky.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/EngineSky/SM_SkySphere'))
    sky.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    sky.static_mesh_component.set_editor_property('cast_shadow',False)
    sky.set_actor_scale3d(unreal.Vector(400,400,400))
sky.static_mesh_component.set_material(0,unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap_Mat'))
for actor in actors.get_all_level_actors():
    if isinstance(actor,unreal.DirectionalLight):
        light=actor.get_component_by_class(unreal.DirectionalLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_editor_property('atmosphere_sun_light',True)
        light.set_intensity(8.)
    elif isinstance(actor,unreal.SkyLight):
        light=actor.get_component_by_class(unreal.SkyLightComponent)
        light.set_mobility(unreal.ComponentMobility.MOVABLE)
        light.set_editor_property('real_time_capture',False)
        light.set_editor_property('source_type',unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
        light.set_editor_property('cubemap',unreal.load_asset('/Engine/MapTemplates/Sky/DaylightAmbientCubemap'))
        light.set_intensity(3.)
        light.recapture_sky()
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
print('DEMO_DYNAMIC_DAYLIGHT_READY')
