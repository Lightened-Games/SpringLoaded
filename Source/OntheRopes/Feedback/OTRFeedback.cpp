// Copyright On the Ropes. All Rights Reserved.

#include "Feedback/OTRFeedback.h"
#include "Data/OTRTuning.h"
#include "Sound/SoundBase.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/StaticMeshComponent.h"
#include "OntheRopes.h"

namespace OTRFeedback
{
	static TSet<FName> Missing;

	template <typename T>
	static T* Find(const FString& Root, const FString& Prefix, FName Name)
	{
		const FString Asset = Prefix + Name.ToString();
		const FString Path = FString::Printf(TEXT("%s/%s.%s"), *Root, *Asset, *Asset);
		T* Obj = FindObject<T>(nullptr, *Path);
		if (!Obj && !Missing.Contains(FName(*Path)))
		{
			Obj = LoadObject<T>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
			if (!Obj)
			{
				Missing.Add(FName(*Path));
				UE_LOG(LogOntheRopes, Warning, TEXT("[OntheRopes] Missing content asset %s"), *Path);
			}
		}
		return Obj;
	}

	USoundBase* FindSound(const UObject* Ctx, FName Cue) { return Find<USoundBase>(OTR::Tuning(Ctx)->SoundRoot, TEXT("S_OTR_"), Cue); }
	UNiagaraSystem* FindVfx(const UObject* Ctx, FName Name) { return Find<UNiagaraSystem>(OTR::Tuning(Ctx)->VfxRoot, TEXT("NS_OTR_"), Name); }
	UStaticMesh* FindMesh(const UObject* Ctx, FName Name) { return Find<UStaticMesh>(OTR::Tuning(Ctx)->MeshRoot, TEXT("SM_OTR_"), Name); }
	UMaterialInterface* FindMaterial(const UObject* Ctx, FName Name) { return Find<UMaterialInterface>(OTR::Tuning(Ctx)->MaterialRoot, TEXT("MI_OTR_"), Name); }

	void Sound(const UObject* Ctx, FName Cue, const FVector& Location, float Volume, float Pitch)
	{
		if (USoundBase* S = FindSound(Ctx, Cue))
		{
			UGameplayStatics::PlaySoundAtLocation(Ctx, S, Location, Volume, Pitch);
		}
	}

	void Sound2D(const UObject* Ctx, FName Cue, float Volume, float Pitch)
	{
		if (USoundBase* S = FindSound(Ctx, Cue))
		{
			UGameplayStatics::PlaySound2D(Ctx, S, Volume, Pitch);
		}
	}

	UAudioComponent* Loop2D(const UObject* Ctx, FName Cue, float Volume)
	{
		if (USoundBase* S = FindSound(Ctx, Cue))
		{
			return UGameplayStatics::SpawnSound2D(Ctx, S, Volume, 1.0f, 0.0f, nullptr, false, false);
		}
		return nullptr;
	}

	UAudioComponent* LoopAttached(const UObject* Ctx, FName Cue, USceneComponent* Parent, float Volume, float Pitch)
	{
		USoundBase* S = FindSound(Ctx, Cue);
		if (!S || !Parent)
		{
			return nullptr;
		}
		return UGameplayStatics::SpawnSoundAttached(S, Parent, NAME_None, FVector::ZeroVector, EAttachLocation::KeepRelativeOffset, false, Volume, Pitch);
	}

	void ApplyMechMaterials(UObject* Outer, UStaticMeshComponent* Mesh, const FLinearColor& Paint, const FLinearColor& Trim,
		TObjectPtr<UMaterialInstanceDynamic>& OutPaint, TObjectPtr<UMaterialInstanceDynamic>& OutTrim, TObjectPtr<UMaterialInstanceDynamic>& OutGlow, const TMap<FName, float>& PaintScalars, TObjectPtr<UMaterialInstanceDynamic>& OutMetal, float MetalBrightness)
	{
		if (!Mesh)
		{
			return;
		}
		auto Tinted = [Outer, &PaintScalars](TObjectPtr<UMaterialInstanceDynamic>& Slot, const FLinearColor& Colour) -> UMaterialInterface*
		{
			if (!Slot)
			{
				if (UMaterialInterface* Base = FindMaterial(Outer, TEXT("Paint")))
				{
					Slot = UMaterialInstanceDynamic::Create(Base, Outer);
					Slot->SetVectorParameterValue(TEXT("Tint"), Colour);
					for (const TPair<FName, float>& P : PaintScalars)
					{
						Slot->SetScalarParameterValue(P.Key, P.Value);
					}
				}
			}
			return Slot;
		};
		const TArray<FName> SlotNames = Mesh->GetMaterialSlotNames();
		for (int32 I = 0; I < SlotNames.Num(); ++I)
		{
			const FString Slot = SlotNames[I].ToString();
			UMaterialInterface* Mat = nullptr;
			if (Slot.Contains(TEXT("Paint"))) { Mat = Tinted(OutPaint, Paint); }
			else if (Slot.Contains(TEXT("Trim"))) { Mat = Tinted(OutTrim, Trim); }
			else if (Slot.Contains(TEXT("Glow")))
			{
				if (!OutGlow)
				{
					if (UMaterialInterface* Base = FindMaterial(Outer, TEXT("Glow")))
					{
						OutGlow = UMaterialInstanceDynamic::Create(Base, Outer);
					}
				}
				Mat = OutGlow;
			}
			else if (Slot.Contains(TEXT("Rubber"))) { Mat = FindMaterial(Outer, TEXT("Rubber")); }
			else if (Slot.Contains(TEXT("Hazard"))) { Mat = FindMaterial(Outer, TEXT("Hazard")); }
			else if (Slot.Contains(TEXT("Chrome"))) { Mat = FindMaterial(Outer, TEXT("Chrome")); }
			else if (MetalBrightness >= 0.0f)
			{
				if (!OutMetal)
				{
					if (UMaterialInterface* Base = FindMaterial(Outer, TEXT("MechMetal")))
					{
						OutMetal = UMaterialInstanceDynamic::Create(Base, Outer);
						OutMetal->SetScalarParameterValue(TEXT("Brightness"), MetalBrightness);
					}
				}
				Mat = OutMetal;
			}
			else { Mat = FindMaterial(Outer, TEXT("MechMetal")); }
			if (Mat)
			{
				Mesh->SetMaterial(I, Mat);
			}
		}
	}

	UNiagaraComponent* Vfx(const UObject* Ctx, FName Name, const FVector& Location, const FRotator& Rotation, float Scale, const FLinearColor& Colour)
	{
		if (UNiagaraSystem* N = FindVfx(Ctx, Name))
		{
			UNiagaraComponent* C = UNiagaraFunctionLibrary::SpawnSystemAtLocation(Ctx, N, Location, Rotation, FVector(Scale), true, true, ENCPoolMethod::AutoRelease);
			if (C)
			{
				C->SetColorParameter(TEXT("User.Tint"), Colour);
			}
			return C;
		}
		return nullptr;
	}
}
