// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OTRHeatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOTROnHeatEvent);

/**
 * Accumulating heat with an overheat lock-up (P5). Work adds heat, idle time cools it, reaching the threshold vents
 * for VentSeconds (the owner can't work), then heat drops to HeatAfterVent. Attachable to any actor.
 */
UCLASS(ClassGroup = (OntheRopes), meta = (BlueprintSpawnableComponent))
class ONTHEROPES_API UOTRHeatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOTRHeatComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CoolRate = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Threshold = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float VentSeconds = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HeatAfterVent = 30.0f;

	UPROPERTY(BlueprintAssignable) FOTROnHeatEvent OnOverheat;
	UPROPERTY(BlueprintAssignable) FOTROnHeatEvent OnVentEnd;

	void SetHeat(float Value) { Heat = FMath::Clamp(Value, 0.0f, Threshold); }
	/** Adds heat; returns true if this overheated. */
	bool AddHeat(float Amount);
	/** Pause cooling (e.g. while the arms are working). */
	void SetCooling(bool bOn) { bCooling = bOn; }
	/** Cut a vent short (clean hit knocks the champion out of it). */
	void EndVent();
	float GetHeat01() const { return Threshold > 0.0f ? Heat / Threshold : 0.0f; }
	bool IsVenting() const { return VentLeft > 0.0f; }
	float GetVentLeft() const { return VentLeft; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	float Heat = 0.0f;
	float VentLeft = 0.0f;
	bool bCooling = true;
};
