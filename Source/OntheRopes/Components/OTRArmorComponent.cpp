// Copyright On the Ropes. All Rights Reserved.

#include "Components/OTRArmorComponent.h"

float UOTRArmorComponent::ApplyDamage(float Amount, AActor* Source)
{
	if (bDepleted || Amount <= 0.0f)
	{
		return 0.0f;
	}
	const float Before = Armor;
	Armor = FMath::Max(0.0f, Armor - Amount);
	OnArmorChanged.Broadcast(Armor, MaxArmor);
	if (Armor <= 0.0f)
	{
		bDepleted = true;
		OnDepleted.Broadcast(Source);
	}
	return Before - Armor;
}
