// Copyright On the Ropes. All Rights Reserved.

#include "Components/OTRHeatComponent.h"

UOTRHeatComponent::UOTRHeatComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

bool UOTRHeatComponent::AddHeat(float Amount)
{
	if (IsVenting())
	{
		return false;
	}
	Heat = FMath::Clamp(Heat + Amount, 0.0f, Threshold);
	if (Heat >= Threshold)
	{
		VentLeft = VentSeconds;
		OnOverheat.Broadcast();
		return true;
	}
	return false;
}

void UOTRHeatComponent::EndVent()
{
	if (VentLeft > 0.0f)
	{
		VentLeft = 0.0f;
		Heat = HeatAfterVent;
		OnVentEnd.Broadcast();
	}
}

void UOTRHeatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (VentLeft > 0.0f)
	{
		VentLeft -= DeltaTime;
		// Venting dumps heat visibly over the lock-up.
		Heat = FMath::Max(HeatAfterVent, Heat - (Threshold - HeatAfterVent) * DeltaTime / FMath::Max(VentSeconds, 0.1f));
		if (VentLeft <= 0.0f)
		{
			VentLeft = 0.0f;
			Heat = HeatAfterVent;
			OnVentEnd.Broadcast();
		}
		return;
	}
	if (bCooling)
	{
		Heat = FMath::Max(0.0f, Heat - CoolRate * DeltaTime);
	}
}
