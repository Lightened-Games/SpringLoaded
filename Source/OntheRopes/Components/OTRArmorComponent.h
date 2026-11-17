// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OTRArmorComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOTROnArmorChanged, float, Armor, float, MaxArmor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOTROnArmorDepleted, AActor*, LastSource);

/** Armor / health pool (P4). Attach to anything that can be hit. */
UCLASS(ClassGroup = (OntheRopes), meta = (BlueprintSpawnableComponent))
class ONTHEROPES_API UOTRArmorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MaxArmor = 100.0f;

	UPROPERTY(BlueprintAssignable) FOTROnArmorChanged OnArmorChanged;
	UPROPERTY(BlueprintAssignable) FOTROnArmorDepleted OnDepleted;

	void Init(float InMax) { MaxArmor = InMax; Armor = InMax; bDepleted = false; }
	/** Returns the damage actually applied. */
	float ApplyDamage(float Amount, AActor* Source);
	float GetArmor() const { return Armor; }
	float GetArmor01() const { return MaxArmor > 0.0f ? Armor / MaxArmor : 0.0f; }
	bool IsDepleted() const { return bDepleted; }

private:
	float Armor = 100.0f;
	bool bDepleted = false;
};
