// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OTREditorTools.generated.h"

class UNiagaraSystem;
class UMaterialInterface;

/** Editor-only helpers for the content scripts (P24): Python can't reach Niagara renderers or build brushes. */
UCLASS()
class ONTHEROPES_API UOTREditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Point every renderer of the system at Material. bSpritesOnly disables non-sprite renderers. Returns renderers changed. */
	UFUNCTION(BlueprintCallable, Category = "OTR|Editor")
	static int32 SetNiagaraRendererMaterial(UNiagaraSystem* System, UMaterialInterface* Material, bool bSpritesOnly);

	/** Spawn a BlockingVolume with a real cube brush. Editor only. */
	UFUNCTION(BlueprintCallable, Category = "OTR|Editor", meta = (WorldContext = "WorldContextObject"))
	static AActor* SpawnBlockingVolume(UObject* WorldContextObject, FVector Location, FVector HalfExtent, FString Label);

	/** Point every brush polygon / BSP surface in the world (volumes, builder brush, level model) at a project material. Returns surfaces changed. */
	UFUNCTION(BlueprintCallable, Category = "OTR|Editor", meta = (WorldContext = "WorldContextObject"))
	static int32 ScrubBrushMaterials(UObject* WorldContextObject, UMaterialInterface* Material);

	/** Bone names of a skeletal mesh (to pick attachment sockets). */
	UFUNCTION(BlueprintCallable, Category = "OTR|Editor")
	static TArray<FString> ListBones(class USkeletalMesh* Mesh);

	/** Component-space location of a bone in the reference pose (to place props on the rig). */
	UFUNCTION(BlueprintCallable, Category = "OTR|Editor")
	static FVector RefPoseBoneLocation(class USkeletalMesh* Mesh, FName Bone);

	/** Component-space path of a bone through an animation (Samples evenly spaced over the clip), to measure foot speeds. */
	UFUNCTION(BlueprintCallable, Category = "OTR|Editor")
	static TArray<FVector> SampleBonePath(class USkeletalMesh* Mesh, class UAnimSequence* Anim, FName Bone, int32 Samples);
};
