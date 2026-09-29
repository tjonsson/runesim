"""Build explicit PBR materials and pose both humanoids for editor review."""
import unreal
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
assert 'LivingWorldDemo' in unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world().get_name()
for index, kind in enumerate(('Civilian','Soldier')):
    folder = '/Game/LivingWorld/Models/' + kind
    material = unreal.load_asset(folder + '/M_' + kind)
    textures = {str(a.asset_name): a.get_asset() for a in registry.get_assets_by_path(folder) if str(a.asset_class_path.asset_name) == 'Texture2D'}
    if not material:
        material = tools.create_asset('M_' + kind, folder, unreal.Material, unreal.MaterialFactoryNew())
        for row, (suffix, prop) in enumerate([
            ('basecolor',unreal.MaterialProperty.MP_BASE_COLOR),
            ('normal',unreal.MaterialProperty.MP_NORMAL),
            ('roughness',unreal.MaterialProperty.MP_ROUGHNESS),
            ('metallic',unreal.MaterialProperty.MP_METALLIC),
        ]):
            texture = next(t for name,t in textures.items() if name.endswith(suffix))
            texture.set_editor_property('srgb', suffix == 'basecolor')
            if suffix == 'normal':
                texture.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_NORMALMAP)
            sample = unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionTextureSample,-400,row*220)
            sample.texture = texture
            if suffix == 'normal':
                sample.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL
            elif suffix != 'basecolor':
                sample.sampler_type = unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR
            unreal.MaterialEditingLibrary.connect_material_property(sample,'RGB' if suffix in ('normal','basecolor') else 'R',prop)
            unreal.EditorAssetLibrary.save_loaded_asset(texture)
        unreal.MaterialEditingLibrary.recompile_material(material)
    mesh = unreal.load_asset(folder + '/SK_' + kind)
    slots = mesh.get_editor_property('materials')
    for slot in slots:
        slot.set_editor_property('material_interface',material)
    mesh.set_editor_property('materials',slots)
    unreal.EditorAssetLibrary.save_loaded_asset(material)
    unreal.EditorAssetLibrary.save_loaded_asset(mesh)
    label = kind + '_AssetReview'
    actor = next((a for a in actors.get_all_level_actors() if a.get_actor_label()==label),None)
    if not actor:
        actor = actors.spawn_actor_from_class(unreal.SkeletalMeshActor,unreal.Vector(0,index*160,20))
        actor.set_actor_label(label)
    component = actor.get_component_by_class(unreal.SkeletalMeshComponent)
    component.set_skeletal_mesh_asset(mesh)
    component.set_animation_mode(unreal.AnimationMode.ANIMATION_SINGLE_NODE)
    component.set_animation(unreal.load_asset(folder+'/'+kind+'_Walk'))
    component.set_position(.6,False)
    component.set_update_animation_in_editor(True)
    actor.set_actor_rotation(unreal.Rotator(yaw=90),False)
    print('REVIEW',kind,'bones',component.get_num_bones(),'bounds',mesh.get_bounds())
unreal.EditorLevelLibrary.set_level_viewport_camera_info(unreal.Vector(430,-360,230),unreal.Rotator(pitch=-12,yaw=135))
