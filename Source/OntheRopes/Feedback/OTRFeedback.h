// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USoundBase;
class UNiagaraSystem;
class UNiagaraComponent;
class UAudioComponent;
class UStaticMesh;
class UMaterialInterface;
class USceneComponent;

/**
 * Finds the game's content by name through the tuning roots and plays it: sounds S_OTR_<Cue>, Niagara NS_OTR_<Name>,
 * meshes SM_OTR_<Name>, materials MI_OTR_<Name>. Missing assets are logged once and skipped (the game still runs).
 */
namespace OTRFeedback
{
	ONTHEROPES_API USoundBase* FindSound(const UObject* Ctx, FName Cue);
	ONTHEROPES_API UNiagaraSystem* FindVfx(const UObject* Ctx, FName Name);
	ONTHEROPES_API UStaticMesh* FindMesh(const UObject* Ctx, FName Name);
	ONTHEROPES_API UMaterialInterface* FindMaterial(const UObject* Ctx, FName Name);

	ONTHEROPES_API void Sound(const UObject* Ctx, FName Cue, const FVector& Location, float Volume = 1.0f, float Pitch = 1.0f);
	ONTHEROPES_API void Sound2D(const UObject* Ctx, FName Cue, float Volume = 1.0f, float Pitch = 1.0f);
	ONTHEROPES_API UAudioComponent* Loop2D(const UObject* Ctx, FName Cue, float Volume = 1.0f);
	ONTHEROPES_API UAudioComponent* LoopAttached(const UObject* Ctx, FName Cue, USceneComponent* Parent, float Volume = 1.0f, float Pitch = 1.0f);
	/**
	 * Mech part meshes name their material slots (Slot<i>_Paint / Trim / Metal / Glow / Rubber / Hazard / Chrome).
	 * Paint and Trim get MIDs of MI_OTR_Paint tinted with the mech's colours (shared per mech through OutPaint /
	 * OutTrim), Glow a shared MID of MI_OTR_Glow (OutGlow), the rest shared instances.
	 */
	ONTHEROPES_API void ApplyMechMaterials(UObject* Outer, class UStaticMeshComponent* Mesh, const FLinearColor& Paint, const FLinearColor& Trim,
		TObjectPtr<class UMaterialInstanceDynamic>& OutPaint, TObjectPtr<class UMaterialInstanceDynamic>& OutTrim, TObjectPtr<class UMaterialInstanceDynamic>& OutGlow, const TMap<FName, float>& PaintScalars, TObjectPtr<class UMaterialInstanceDynamic>& OutMetal, float MetalBrightness);
	ONTHEROPES_API UNiagaraComponent* Vfx(const UObject* Ctx, FName Name, const FVector& Location, const FRotator& Rotation = FRotator::ZeroRotator, float Scale = 1.0f, const FLinearColor& Colour = FLinearColor::White);
}
