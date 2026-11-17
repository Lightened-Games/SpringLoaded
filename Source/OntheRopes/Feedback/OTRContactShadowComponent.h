// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/DecalComponent.h"
#include "OTRContactShadowComponent.generated.h"

class UMaterialInstanceDynamic;

/**
 * Soft contact shadow under any actor (player, robot, prop). Lumen's sky occlusion skips skinned and moving meshes,
 * so without this, characters read as floating. A downward-projected radial decal pinned at the owner's feet,
 * which follows the owner without rotating with it and fades as the owner leaves the ground. The owner's own
 * primitives stop receiving decals so the shadow never paints the character.
 */
UCLASS(ClassGroup = (OntheRopes), meta = (BlueprintSpawnableComponent))
class ONTHEROPES_API UOTRContactShadowComponent : public UDecalComponent
{
	GENERATED_BODY()

public:
	UOTRContactShadowComponent();

	/** Shadow radius on the ground, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Radius = 60.0f;
	/** Darkness at the centre, 0..1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Strength = 0.7f;
	/** Elongation along the owner's forward axis (treads are longer than wide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ForwardStretch = 1.0f;
	/** Height above the ground at which the shadow has faded out (jumps, knock-ups), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FadeHeight = 150.0f;
	/** Projection depth above / below the foot point, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ProjectionDepth = 60.0f;
	/** Material looked up as MI_OTR_<Name>. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FName MaterialName = TEXT("BlobShadow");

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float FootOffset() const;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> MID;
	float LastStrength = -1.0f;
};
