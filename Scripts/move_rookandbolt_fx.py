"""Move the copied Rook & Bolt effects from /Game/SW/FX to /Game/LivingWorld/Effects/RookAndBolt (editor).

The .uasset files were copied unchanged from ../RookAndBolt/Content/SW/FX/{Coastal,VehicleFluids}
(BP_CoastalBurst omitted: it depends on Rook & Bolt's game instance). Renaming through AssetTools
rewrites internal references; redirectors are then fixed up and the empty /Game/SW tree removed.
"""
import unreal
assert not unreal.EditorLevelLibrary.get_pie_worlds(False)
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
lib = unreal.EditorAssetLibrary
registry.scan_paths_synchronous(['/Game/SW'], True)
moves = []
for folder in ('Coastal', 'VehicleFluids'):
    for data in registry.get_assets_by_path('/Game/SW/FX/' + folder, False):
        moves.append(unreal.AssetRenameData(data.get_asset(), '/Game/LivingWorld/Effects/RookAndBolt/' + folder, str(data.asset_name)))
assert not moves or tools.rename_assets(moves), 'rename failed'
# Only the moved assets referenced each other (rewritten by the rename), so the redirectors can go.
lib.delete_directory('/Game/SW')
moved = [str(d.asset_name) for d in registry.get_assets_by_path('/Game/LivingWorld/Effects/RookAndBolt', True)]
print('RAB_MOVED', len(moves), len(moved), sorted(moved))
