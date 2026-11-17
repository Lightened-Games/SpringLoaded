// Copyright On the Ropes. All Rights Reserved.

#include "Editor/OTREditorTools.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraRibbonRendererProperties.h"
#include "Materials/MaterialInterface.h"
#include "Engine/BlockingVolume.h"
#include "Engine/Polys.h"
#include "Engine/Brush.h"
#include "Engine/Level.h"
#include "Model.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "ReferenceSkeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimCurveTypes.h"
#include "Animation/AttributesRuntime.h"
#include "Animation/AnimData/AnimDataModel.h"
#include "BonePose.h"
#if WITH_EDITOR
#include "Builders/CubeBuilder.h"
#include "ActorFactories/ActorFactory.h"
#endif

AActor* UOTREditorTools::SpawnBlockingVolume(UObject* WorldContextObject, FVector Location, FVector HalfExtent, FString Label)
{
#if WITH_EDITOR
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World)
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	ABlockingVolume* Volume = World->SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(), FTransform(Location), Params);
	if (!Volume)
	{
		return nullptr;
	}
	UCubeBuilder* Builder = NewObject<UCubeBuilder>();
	Builder->X = HalfExtent.X * 2.0f;
	Builder->Y = HalfExtent.Y * 2.0f;
	Builder->Z = HalfExtent.Z * 2.0f;
	UActorFactory::CreateBrushForVolumeActor(Volume, Builder);
	// Brush faces default to WorldGridMaterial; give them a project material so no placeholder is referenced.
	if (UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/OntheRopes/Materials/MI_OTR_MetalDark.MI_OTR_MetalDark")))
	{
		if (Volume->Brush && Volume->Brush->Polys)
		{
			for (FPoly& Poly : Volume->Brush->Polys->Element)
			{
				Poly.Material = Mat;
			}
		}
		if (Volume->Brush)
		{
			for (FBspSurf& Surf : Volume->Brush->Surfs)
			{
				Surf.Material = Mat;
			}
		}
	}
	Volume->SetActorLabel(Label);
	return Volume;
#else
	return nullptr;
#endif
}

TArray<FString> UOTREditorTools::ListBones(USkeletalMesh* Mesh)
{
	TArray<FString> Out;
	if (Mesh)
	{
		const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
		for (int32 I = 0; I < Ref.GetNum(); ++I)
		{
			Out.Add(Ref.GetBoneName(I).ToString());
		}
	}
	return Out;
}

int32 UOTREditorTools::SetNiagaraRendererMaterial(UNiagaraSystem* System, UMaterialInterface* Material, bool bSpritesOnly)
{
	int32 Changed = 0;
#if WITH_EDITOR
	if (!System || !Material)
	{
		return 0;
	}
	System->Modify();
	for (FNiagaraEmitterHandle& Handle : System->GetEmitterHandles())
	{
		FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
		if (!Data)
		{
			continue;
		}
		for (UNiagaraRendererProperties* Renderer : Data->GetRenderers())
		{
			if (!Renderer)
			{
				continue;
			}
			Renderer->Modify();
			if (bSpritesOnly && !Renderer->IsA<UNiagaraSpriteRendererProperties>())
			{
				Renderer->SetIsEnabled(false);
				continue;
			}
			if (UNiagaraSpriteRendererProperties* Sprite = Cast<UNiagaraSpriteRendererProperties>(Renderer))
			{
				Sprite->Material = Material;
			}
			else if (UNiagaraRibbonRendererProperties* Ribbon = Cast<UNiagaraRibbonRendererProperties>(Renderer))
			{
				Ribbon->Material = Material;
			}
			else if (UNiagaraMeshRendererProperties* MeshRenderer = Cast<UNiagaraMeshRendererProperties>(Renderer))
			{
				MeshRenderer->bOverrideMaterials = true;
				MeshRenderer->OverrideMaterials.SetNum(FMath::Max(1, MeshRenderer->OverrideMaterials.Num()));
				for (FNiagaraMeshMaterialOverride& Override : MeshRenderer->OverrideMaterials)
				{
					Override.ExplicitMat = Material;
				}
			}
			else
			{
				continue;
			}
			FPropertyChangedEvent Event(nullptr);
			Renderer->PostEditChangeProperty(Event);
			++Changed;
		}
	}
	System->MarkPackageDirty();
#endif
	return Changed;
}

FVector UOTREditorTools::RefPoseBoneLocation(USkeletalMesh* Mesh, FName Bone)
{
	if (!Mesh)
	{
		return FVector::ZeroVector;
	}
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	int32 Index = Ref.FindBoneIndex(Bone);
	FTransform T = FTransform::Identity;
	while (Index != INDEX_NONE)
	{
		T = T * Ref.GetRefBonePose()[Index];
		Index = Ref.GetParentIndex(Index);
	}
	return T.GetLocation();
}

TArray<FVector> UOTREditorTools::SampleBonePath(USkeletalMesh* Mesh, UAnimSequence* Anim, FName Bone, int32 Samples)
{
	TArray<FVector> Out;
	if (!Mesh || !Anim || Samples < 2)
	{
		return Out;
	}
	const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
	const int32 MeshIndex = Ref.FindBoneIndex(Bone);
	if (MeshIndex == INDEX_NONE)
	{
		return Out;
	}
	FMemMark Mark(FMemStack::Get());
	TArray<FBoneIndexType> Required;
	for (int32 I = 0; I < Ref.GetNum(); ++I)
	{
		Required.Add((FBoneIndexType)I);
	}
	FBoneContainer Container(Required, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *Mesh);
	const FCompactPoseBoneIndex Compact = Container.MakeCompactPoseIndex(FMeshPoseBoneIndex(MeshIndex));
	const double Length = Anim->GetPlayLength();
	for (int32 S = 0; S < Samples; ++S)
	{
		FCompactPose Pose;
		Pose.SetBoneContainer(&Container);
		FBlendedCurve Curve;
		Curve.InitFrom(Container);
		UE::Anim::FStackAttributeContainer Attributes;
		FAnimationPoseData PoseData(Pose, Curve, Attributes);
		Anim->GetAnimationPose(PoseData, FAnimExtractContext(Length * S / (Samples - 1), false));
		FCSPose<FCompactPose> CS;
		CS.InitPose(Pose);
		Out.Add(CS.GetComponentSpaceTransform(Compact).GetLocation());
	}
	return Out;
}

int32 UOTREditorTools::ScrubBrushMaterials(UObject* WorldContextObject, UMaterialInterface* Material)
{
	int32 Changed = 0;
#if WITH_EDITOR
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) : nullptr;
	if (!World || !Material)
	{
		return 0;
	}
	auto ScrubModel = [&Changed, Material](UModel* Model)
	{
		if (!Model)
		{
			return;
		}
		Model->Modify();
		if (Model->Polys)
		{
			for (FPoly& Poly : Model->Polys->Element)
			{
				Poly.Material = Material;
				++Changed;
			}
		}
		for (FBspSurf& Surf : Model->Surfs)
		{
			Surf.Material = Material;
			++Changed;
		}
	};
	for (ULevel* Level : World->GetLevels())
	{
		if (!Level)
		{
			continue;
		}
		ScrubModel(Level->Model);
		for (AActor* Actor : Level->Actors)
		{
			if (ABrush* Brush = Cast<ABrush>(Actor))
			{
				ScrubModel(Brush->Brush);
			}
		}
	}
#endif
	return Changed;
}
