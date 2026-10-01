#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "LivingEffectAuthoring.generated.h"
class UNiagaraSystem;

/** Narrow editor-only authoring adapter. Packaged builds contain no Niagara editor dependency. */
UCLASS()
class RUNESIM_API ULivingEffectAuthoring : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) static FString Inspect(UNiagaraSystem* System);
    UFUNCTION(BlueprintCallable) static FString SetInput(UNiagaraSystem* System, FName Emitter, FName Stack, FName Module, FName Input, const FString& Type, FVector4 Value);
    UFUNCTION(BlueprintCallable) static FString SetRenderer(UNiagaraSystem* System, FName Emitter, int32 Index, const FString& PropertiesJson);
    /** Sets a reflected property (text form, e.g. "(RuneSimNone)") on every data interface of ClassName inside the system,
     *  including compiled copies, then recompiles. Returns the number changed, or a negative value on error. */
    UFUNCTION(BlueprintCallable) static int32 SetDataInterfaceProperty(UNiagaraSystem* System, const FString& ClassName, FName Property, const FString& ValueText);
};
