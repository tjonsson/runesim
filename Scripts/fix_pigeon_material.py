import unreal
folder = '/Game/LivingWorld/Models/Pigeon'
assets = unreal.AssetToolsHelpers.get_asset_tools()
material = unreal.load_asset(folder + '/M_Pigeon')
if material is None:
    material = assets.create_asset('M_Pigeon', folder, unreal.Material, unreal.MaterialFactoryNew())
    sample = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSample, -300, 0)
    sample.texture = unreal.load_asset(folder + '/Pigeon_BaseColor')
    unreal.MaterialEditingLibrary.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    roughness = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionConstant, -300, 180)
    roughness.r = .8
    unreal.MaterialEditingLibrary.connect_material_property(roughness, '', unreal.MaterialProperty.MP_ROUGHNESS)
    material.set_editor_property('two_sided', True)
    unreal.MaterialEditingLibrary.recompile_material(material)
mesh = unreal.load_asset(folder + '/SK_Pigeon')
materials = mesh.get_editor_property('materials')
for slot in materials:
    slot.set_editor_property('material_interface', material)
mesh.set_editor_property('materials', materials)
unreal.EditorAssetLibrary.save_loaded_asset(material)
unreal.EditorAssetLibrary.save_loaded_asset(mesh)
# The failed PIE import left an empty dirty package, with no asset to save.
empty = unreal.find_object(None, folder + '/Pigeon')
if empty and not unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_package_name(folder + '/Pigeon'):
    print('EMPTY_IMPORT_PACKAGE_UNLOAD', unreal.EditorLoadingAndSavingUtils.unload_packages([empty]))
