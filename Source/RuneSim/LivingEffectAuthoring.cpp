#include "LivingEffectAuthoring.h"
#if WITH_EDITOR
#include "NiagaraExternalSystemEditorUtilities.h"
#include "NiagaraSystem.h"
#include "UObject/UObjectHash.h"
#include "JsonObjectConverter.h"

namespace
{
FString Errors(const FNiagaraExternalEditContext& Context)
{
    FString Result;
    for (const FText& Error : Context.Errors) Result += Error.ToString() + TEXT("\n");
    return Result;
}
bool Editable(UNiagaraSystem* System) { return System && System->GetPathName().StartsWith(TEXT("/Game/LivingWorld/Effects/")); }
}
#endif

FString ULivingEffectAuthoring::Inspect(UNiagaraSystem* System)
{
#if WITH_EDITOR
    if (!System) return TEXT("{}");
    FNiagaraExternalEditContext Context(System);
    FNiagaraExt_SystemSummary Summary;
    UNiagaraExternalEditUtilities::GetSystemSummary(System, Summary, Context);
    TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetObjectField(TEXT("summary"), FJsonObjectConverter::UStructToJsonObject(Summary));
    TArray<TSharedPtr<FJsonValue>> Emitters;
    for (const auto& Emitter : Summary.Emitters)
    {
        FNiagaraExt_EmitterTopology Topology;
        UNiagaraExternalEditUtilities::GetEmitterTopology(FNiagaraExt_StackItemReference(System, Emitter.EmitterName), Topology, Context);
        Emitters.Add(MakeShared<FJsonValueObject>(FJsonObjectConverter::UStructToJsonObject(Topology)));
    }
    Json->SetArrayField(TEXT("topology"), Emitters);
    Json->SetStringField(TEXT("errors"), Errors(Context));
    FString Result; FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Result)); return Result;
#else
    return TEXT("Editor only");
#endif
}
FString ULivingEffectAuthoring::SetInput(UNiagaraSystem* System, FName Emitter, FName Stack, FName Module, FName Input, const FString& Type, FVector4 Value)
{
#if WITH_EDITOR
    if (!Editable(System)) return TEXT("Only project Living World effects can be edited");
    FNiagaraExt_StackItemReference Ref(System, Emitter, Stack, Module); Ref.InputNameStack.Add(Input);
    FNiagaraExternalEditContext Context(System); FNiagaraExt_StackInputValue Data;
    if (Type == TEXT("float")) Data.InitializeAs<FNiagaraFloat>().Value = Value.X;
    else if (Type == TEXT("int")) Data.InitializeAs<FNiagaraInt32>().Value = Value.X;
    else if (Type == TEXT("vector"))
    {
        const FVector3f Vector(Value.X, Value.Y, Value.Z);
        Data.InitializeAs(FindObjectChecked<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Vector3f")), reinterpret_cast<const uint8*>(&Vector));
    }
    else if (Type == TEXT("vector2"))
    {
        const FVector2f Vector(Value.X, Value.Y);
        Data.InitializeAs(FindObjectChecked<UScriptStruct>(nullptr, TEXT("/Script/CoreUObject.Vector2f")), reinterpret_cast<const uint8*>(&Vector));
    }
    else if (Type == TEXT("color")) Data.InitializeAs<FLinearColor>() = FLinearColor(Value.X, Value.Y, Value.Z, Value.W);
    else return TEXT("Unsupported literal type");
    System->Modify(); UNiagaraExternalEditUtilities::SetStackInputData(Ref, Data, Context);
    System->MarkPackageDirty(); return Errors(Context);
#else
    return TEXT("Editor only");
#endif
}
FString ULivingEffectAuthoring::SetRenderer(UNiagaraSystem* System, FName Emitter, int32 Index, const FString& PropertiesJson)
{
#if WITH_EDITOR
    if (!Editable(System)) return TEXT("Only project Living World effects can be edited");
    FNiagaraExt_StackItemReference Ref(System, Emitter); Ref.RendererIndex = Index;
    FNiagaraExternalEditContext Context(System); FNiagaraExt_RendererData Data; Data.PropertyValues = PropertiesJson;
    System->Modify(); UNiagaraExternalEditUtilities::SetRendererData(Ref, Data, Context);
    System->MarkPackageDirty(); return Errors(Context);
#else
    return TEXT("Editor only");
#endif
}

int32 ULivingEffectAuthoring::SetDataInterfaceProperty(UNiagaraSystem* System, const FString& ClassName, FName Property, const FString& ValueText)
{
#if WITH_EDITOR
    if (!Editable(System)) return -1;
    TArray<UObject*> Inner;
    GetObjectsWithOuter(System, Inner, true);
    int32 Changed = 0;
    System->Modify();
    for (UObject* Object : Inner)
    {
        if (!Object || Object->GetClass()->GetName() != ClassName) continue;
        FProperty* Field = Object->GetClass()->FindPropertyByName(Property);
        if (!Field) return -2;
        Object->Modify();
        if (!Field->ImportText_Direct(*ValueText, Field->ContainerPtrToValuePtr<void>(Object), Object, PPF_None)) return -3;
        ++Changed;
    }
    if (Changed)
    {
        System->RequestCompile(false);
        System->WaitForCompilationComplete();
        System->MarkPackageDirty();
    }
    return Changed;
#else
    return -1;
#endif
}
