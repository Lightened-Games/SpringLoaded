// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OTRTypes.h"
#include "OTRAutoplayComponent.generated.h"

class AOTRPlayerController;

/**
 * The -autoplay bot (P22). Plays through the same controller calls as a human (so the legend lights up): reads the
 * champion's telegraph and guards high or low after a reaction delay, walks to the freshest intact rope (a corner
 * once, when it is safe), leans, waits for the vent and launches. Skill 0..1 sets reaction time, guard mistakes and
 * impatience. -OTRDefeat makes it drop its guard and stand in the open (Defeat path QA).
 */
UCLASS(ClassGroup = (OntheRopes))
class ONTHEROPES_API UOTRAutoplayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOTRAutoplayComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float Skill = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") bool bThrowMatch = false;
	/** Reaction time to a telegraph at skill 0 and 1, seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") FVector2D ReactionRange = FVector2D(0.36f, 0.12f);
	/** Chance to pick the wrong guard at skill 0. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float MistakeAtZero = 0.35f;
	/** Ropes frayed past this are avoided. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float AvoidFray = 0.8f;
	/** Release a rope that frays past this even outside a vent (saves the rope). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float BailFray = 0.9f;
	/** Use a corner once when armor is above this and progress at least CornerAfterProgress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float CornerMinArmor = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float CornerAfterProgress = 0.5f;
	/** Both corner ropes must be frayed less than this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float CornerMaxFray = 0.55f;
	/** How much the bot prefers fresh ropes over the nearest one (low = reuses ropes, so some snap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float FrayWeight = 0.6f;
	/** Walk around the champion when it is within this distance of the straight line to the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float AvoidRadius = 260.0f;
	/** At skill 1: chance to parry (raise the guard late) instead of guarding early. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float ParryChance = 0.3f;
	/** At skill 1: chance to jab a power punch's wind-up when in reach off the ropes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float InterruptChance = 0.5f;
	/** At skill 1: chance to duck a hook in the open (on the ropes it ducks haymakers when armor is low). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float DuckChance = 0.3f;
	/** Free jabs per second while closing in (keeps heat building). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float FreeJabRate = 0.6f;
	/** After a launch the bot stays in close this long: jabs, and breaks a raised guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float PressureSeconds = 1.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float PressureJabRate = 2.2f;
	/** At skill 1: chance to duck a hook while on the ropes (keeps the lean, stores nothing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float RopeDuckChance = 0.18f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void ChooseTarget();
	void TickGuard(float Dt);
	void TickMove(float Dt);
	void TickLean(float Dt);
	void TickStrikes(float Dt, bool bPressure);

	AOTRPlayerController* PC() const;
	APawn* Pawn() const;

	EOTRPunch SeenPunch = EOTRPunch::None;
	float ReactClock = 0.0f;
	EOTRGuard PlannedGuard = EOTRGuard::None;
	EOTRGuard Guard = EOTRGuard::None;
	int32 TargetSide = INDEX_NONE;
	int32 TargetCorner = INDEX_NONE;
	bool bCornerUsed = false;
	FVector TargetPoint = FVector::ZeroVector;
	float RetargetClock = 0.0f;
	float StuckClock = 0.0f;
	FVector StuckFrom = FVector::ZeroVector;
	float SidestepClock = 0.0f;
	FVector SidestepDir = FVector::ZeroVector;
	float ImpatienceClock = 0.0f;
	bool bLeanHeld = false;
	bool bPlannedDodge = false;
	bool bPlannedParry = false;
	bool bPlannedDuck = false;
	bool bPlannedJab = false;
	bool bActed = false;
	float JabClock = 0.0f;
	float FinishClock = 0.0f;
	/** Seconds the bot lets the "finishing blow" prompt read before pressing Q. */
	UPROPERTY(EditAnywhere, Category = "Autoplay") float FinishDelay = 1.0f;
	float PressureClock = 0.0f;
	bool bWasLaunching = false;
	bool bDodged = false;
	bool bSawLean = false;
	float LeanWaitClock = 0.0f;
};
