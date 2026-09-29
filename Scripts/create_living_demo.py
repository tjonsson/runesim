"""Create an isolated functional demo. Colored primitives are explicitly test proxies."""
import unreal

actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
assets=unreal.AssetToolsHelpers.get_asset_tools()
path='/Game/LivingWorld/Maps/LivingWorldDemo'
if unreal.EditorAssetLibrary.does_asset_exist(path):
    raise RuntimeError('Demo already exists; refusing to overwrite editor work')
levels.new_level(path)
world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property('default_game_mode', unreal.GameModeBase)
cube=unreal.load_asset('/Engine/BasicShapes/Cube')
sphere=unreal.load_asset('/Engine/BasicShapes/Sphere')
cylinder=unreal.load_asset('/Engine/BasicShapes/Cylinder')

def block(name,location,scale,tags=[]):
    actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(*location))
    actor.set_actor_label(name)
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector(*scale)); actor.tags=tags
    return actor
block('Validated test surface',(0,0,-50),(300,300,1))
block('No-walk building',(2500,2500,500),(10,10,10),['LivingWorld.NoWalk'])
block('Water exclusion test',(3500,-2500,1),(20,20,.02),['Water','LivingWorld.NoWalk'])
sun=actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,3000),unreal.Rotator(-50,-25,0))
actors.spawn_actor_from_class(unreal.SkyAtmosphere,unreal.Vector())
actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,2000))
actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-2000,0,250))
for name,points,vehicles in [
    ('Walking circuit',[(-1200,-1200,0),(1200,-1200,0),(1200,1200,0),(-1200,1200,0)],False),
    ('Driving circuit',[(-7000,-7000,0),(7000,-7000,0),(7000,7000,0),(-7000,7000,0)],True)]:
    route=actors.spawn_actor_from_class(unreal.LivingRoute,unreal.Vector())
    route.set_actor_label(name)
    route.path.set_spline_points([unreal.Vector(*p) for p in points],unreal.SplineCoordinateSpace.LOCAL)
    for i in range(len(points)): route.path.set_spline_point_type(i,unreal.SplinePointType.LINEAR)
    route.path.set_closed_loop(True)
    route.set_editor_property('vehicles',vehicles)
    route.set_editor_property('validated',True)
    route.set_editor_property('provenance','Hand-authored on the bounded collision test platform; no GIS claim')
for name,kind,mesh,scale,speed,radius,clearance,altitude in [
    ('TestCivilian',unreal.LivingKind.CIVILIAN,cylinder,(.5,.5,1.75),1.4,24.,90.,0.),
    ('TestSoldier',unreal.LivingKind.SOLDIER,cylinder,(.6,.6,1.8),1.6,28.,95.,0.),
    ('TestCar',unreal.LivingKind.CAR,cube,(4.2,1.8,1.5),7.,85.,100.,0.),
    ('TestBird',unreal.LivingKind.BIRD,sphere,(.32,.65,.12),8.,12.,0.,25.)]:
    factory=unreal.DataAssetFactory(); factory.set_editor_property('data_asset_class',unreal.LivingAssetProfile)
    profile=assets.create_asset('DA_'+name,'/Game/LivingWorld/Demo',unreal.LivingAssetProfile,factory)
    for key,value in {'kind':kind,'static_mesh':mesh,'visual_scale':unreal.Vector(*scale),
        'speed_meters_per_second':speed,'collision_radius_cm':radius,'ground_clearance_cm':clearance,
        'altitude_meters':altitude,'approved':True,'source':'RuneSim functional test geometry',
        'license':'Unreal Engine basic shapes; test use','scale_evidence':'Explicit synthetic test dimensions'}.items():
        profile.set_editor_property(key,value)
    unreal.EditorAssetLibrary.save_loaded_asset(profile)
levels.save_current_level()
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(-3000,-4000,3500),unreal.Rotator(-35,50,0))
print('DEMO_READY',path)
