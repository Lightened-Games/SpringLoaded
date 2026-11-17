// Copyright On the Ropes. All Rights Reserved.

#include "Feedback/OTRContactShadowComponent.h"
#include "Feedback/OTRFeedback.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"

UOTRContactShadowComponent::UOTRContactShadowComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	SetUsingAbsoluteRotation(true);
	SetUsingAbsoluteScale(true);
	SortOrder = -10;
}

float UOTRContactShadowComponent::FootOffset() const
{
	const AActor* Owner = GetOwner();
	if (const UCapsuleComponent* Cap = Owner ? Cast<UCapsuleComponent>(Owner->GetRootComponent()) : nullptr)
	{
		return Cap->GetScaledCapsuleHalfHeight();
	}
	return 0.0f;
}

void UOTRContactShadowComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UMaterialInterface* Base = OTRFeedback::FindMaterial(this, MaterialName))
	{
		MID = UMaterialInstanceDynamic::Create(Base, this);
		SetDecalMaterial(MID);
	}
	DecalSize = FVector(ProjectionDepth, Radius * ForwardStretch, Radius);
	if (AActor* Owner = GetOwner())
	{
		TInlineComponentArray<UPrimitiveComponent*> Prims(Owner);
		for (UPrimitiveComponent* P : Prims)
		{
			P->SetReceivesDecals(false);
		}
	}
	MarkRenderStateDirty();
}

void UOTRContactShadowComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	// Pin to the floor under the owner: trace down so jumps and knock-ups leave the shadow on the ground.
	const FVector Foot = Owner->GetActorLocation() - FVector(0.0f, 0.0f, FootOffset());
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(OTRContactShadow), false, Owner);
	const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Foot + FVector(0.0f, 0.0f, 20.0f), Foot - FVector(0.0f, 0.0f, FadeHeight + 20.0f), ECC_Visibility, Params);
	const FVector Ground = bHit ? Hit.ImpactPoint : Foot;
	const float Height = bHit ? FMath::Max(0.0f, Foot.Z - Ground.Z) : FadeHeight;
	SetWorldLocationAndRotation(Ground, FRotator(-90.0f, Owner->GetActorRotation().Yaw, 0.0f));
	const float Want = Strength * FMath::Clamp(1.0f - Height / FMath::Max(FadeHeight, 1.0f), 0.0f, 1.0f);
	if (MID && !FMath::IsNearlyEqual(Want, LastStrength, 0.01f))
	{
		MID->SetScalarParameterValue(TEXT("Strength"), Want);
		LastStrength = Want;
	}
}
