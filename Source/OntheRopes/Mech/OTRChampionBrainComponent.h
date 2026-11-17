// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OTRTypes.h"
#include "OTRChampionBrainComponent.generated.h"

class ACharacter;
class UOTRMechRigComponent;
class UOTRHeatComponent;
class UNiagaraComponent;

/**
 * The champion's decisions (P9 state machine, direct steering in an open ring, no navmesh):
 * approach -> telegraphed punch (hook high / body low / jab high) -> recover, heat on every punch, overheat vent
 * (locked, steaming), clean hits from launches during a vent, blocks and sidesteps otherwise. It learns: when the
 * player's rope is half loaded it switches to fast jabs that drag the player back to the centre.
 * Owner-specific logic created by its owner (Begin), like Screw Loose D7; all cross-actor talk goes through interfaces.
 */
UCLASS(ClassGroup = (OntheRopes))
class ONTHEROPES_API UOTRChampionBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOTRChampionBrainComponent();

	void Begin(ACharacter* InBody, UOTRMechRigComponent* InRig, UOTRHeatComponent* InHeat);

	EOTRChampState GetState() const { return State; }
	EOTRPunch GetTelegraph(float& OutSecondsToHit) const;
	const FOTRPunchRow* GetCurrentPunch() const { return State == EOTRChampState::Windup || State == EOTRChampState::Strike ? &Punch : nullptr; }
	bool IsJabMode() const { return bJabMode; }
	FVector GetFistLocation() const;
	EOTRLaunchResult ReceiveLaunch(float Load, AActor* From);
	EOTRStrikeResult ReceiveStrike(EOTRPlayerStrike Strike, AActor* From);
	bool IsGuarding() const { return State == EOTRChampState::Guarding; }
	/** 0..2 from the clean hits it has taken. */
	int32 GetPhase() const;

	/** Turn rate toward the player, degrees/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TurnRate = 260.0f;
	/** Steam puffs while venting, seconds apart. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SteamInterval = 0.13f;
	/** Jab mode turns off below this fraction of JabModeLoad (hysteresis). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float JabModeExit = 0.5f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	void SetState(EOTRChampState NewState);
	void ChoosePunch();
	void StartPunch(EOTRPunch Type, float TelegraphScale);
	void StartGuard();
	void StaggerFor(float Seconds);
	float PhaseValue(const TArray<float>& Values, float Fallback) const;
	void Resolve();
	void Steer(float Dt, bool bAllowMove);
	void Face(float Dt);
	void UpdatePose(float Dt);
	void UpdateGlow(float Dt);
	void UpdateJabMode();
	void Shove(const FVector& Direction, float Distance);
	AActor* Player() const;
	EOTRMatchState MatchState() const;
	UFUNCTION() void HandleOverheat();
	UFUNCTION() void HandleVentEnd();

	UPROPERTY() TObjectPtr<ACharacter> Body;
	UPROPERTY() TObjectPtr<UOTRMechRigComponent> Rig;
	UPROPERTY() TObjectPtr<UOTRHeatComponent> Heat;

	EOTRChampState State = EOTRChampState::Idle;
	FOTRPunchRow Punch;
	TMap<EOTRPunch, FOTRPunchRow> Rows;
	float StateClock = 0.0f;
	float ActiveClock = 0.0f;
	float NextPunchAt = 0.0f;
	float SteamClock = 0.0f;
	float KOClock = 0.0f;
	float Glow = 0.0f;
	bool bJabMode = false;
	bool bPunchRight = true;
	int32 PowerCount = 0;
	float NextHaymakerAt = 0.0f;
	bool bFeint = false;
	bool bComboPending = false;
	float NextGuardAt = 0.0f;
	TArray<float> JabHitTimes;
};
