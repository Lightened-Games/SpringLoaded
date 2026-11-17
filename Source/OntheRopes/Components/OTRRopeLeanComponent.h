// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OTRTypes.h"
#include "OTRRopeLeanComponent.generated.h"

class IOTRRingInfo;
class IOTRChampion;

/**
 * Lean + spring launch (new system #2, Docs/PLAN.md section 10): the player's only offensive verb. Holding lean with
 * the back to an intact rope (or a corner = two ropes) makes every absorbed hit store energy in it; releasing flings
 * the owner at the champion with that energy. Works on any ACharacter owner (movement through its CMC).
 */
UCLASS(ClassGroup = (OntheRopes), meta = (BlueprintSpawnableComponent))
class ONTHEROPES_API UOTRRopeLeanComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOTRRopeLeanComponent();

	/** Lean input held / released (human or bot). */
	void SetLeanHeld(bool bHeld);
	/** An absorbed punch (called by the owner when a punch lands while leaning). */
	void OnAbsorb(const FOTRPunchRow& Punch);
	/** Lose part of the stored energy (unguarded jab drags). */
	void Bleed(float Fraction);
	/** Leave the rope without launching (snaps, defeats, end of match). */
	void BreakLean(bool bLoseLoad);

	bool IsLeaning() const { return bLeaning; }
	bool IsLaunching() const { return bLaunching; }
	bool IsCorner() const { return LeanSides.Num() > 1; }
	float GetLoad() const { return Load; }
	const TArray<int32>& GetSides() const { return LeanSides; }
	/** 0..1 through the current launch dash. */
	float GetLaunchAlpha() const { return bLaunching ? FMath::Clamp(LaunchClock / FMath::Max(LaunchDuration, 0.01f), 0.0f, 1.0f) : 0.0f; }
	FVector GetLaunchDirection() const { return LaunchDir; }
	/** Can a lean start here now (an intact side within reach)? */
	bool CanLeanHere() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	IOTRRingInfo* Ring() const;
	IOTRChampion* Champion(AActor** OutActor = nullptr) const;
	void FindSides(TArray<int32>& Out, float ExtraReach) const;
	bool TryStartLean();
	void Release();
	void StartLaunch();
	void TickLean(float Dt);
	void TickLaunch(float Dt);
	void EndLaunch(EOTRLaunchResult Result, AActor* Target);
	float OwnerRadius() const;
	bool CanAct() const;

	bool bHeld = false;
	bool bLeaning = false;
	bool bLaunching = false;
	bool bResolved = false;
	float Load = 0.0f;
	float LaunchLoad = 0.0f;
	float LaunchClock = 0.0f;
	float LaunchDuration = 0.35f;
	float LaunchSpeed = 0.0f;
	float FullClock = 0.0f;
	FVector LaunchDir = FVector::ForwardVector;
	TArray<int32> LeanSides;
	bool bCornerCounted = false;
};
