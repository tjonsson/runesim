import unreal,json,collections
world=unreal.EditorLevelLibrary.get_pie_worlds(False)[0]
assert 'MainLevel' in world.get_name()
system=next(o for o in unreal.ObjectIterator(unreal.LivingWorldSubsystem) if o.get_outer()==world)
agents=[a for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.LivingAgent) if a.active]
print('MAIN_RUNTIME',system.status,dict(collections.Counter(str(a.profile.kind) for a in agents)))
print('MAIN_BLOCKED',dict(collections.Counter(str(a.profile.kind) for a in agents if a.behavior==unreal.LivingBehavior.BLOCKED)))
print('MAIN_GROUND',[(a.get_name(),a.profile.get_name(),round(a.velocity.length(),1),str(a.get_actor_location()),str(a.behavior),a.blocked_reason) for a in agents if a.profile.kind in (unreal.LivingKind.CAR,unreal.LivingKind.CIVILIAN,unreal.LivingKind.SOLDIER)])
pc=unreal.GameplayStatics.get_player_controller(world,0)
print('MAIN_PAWN',unreal.GameplayStatics.get_player_pawn(world,0))
