// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mech/OTRMech.h"
#include "Interfaces/OTRInterfaces.h"
#include "OTRPlayerMech.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UOTRArmorComponent;
class UOTRRopeLeanComponent;
class UOTRGameFeelComponent;

/**
 * "Old Iron", the player's underpowered mech. Its jab is too weak to hurt the champion (it chips heat, interrupts a
 * wind-up and bounces off a raised guard; Q smashes a guard open). The real damage is stored: it guards high or low
 * (a guard raised at the last instant parries), ducks high punches, leans into the ropes to store the champion's
 * hits and springs off them (UOTRRopeLeanComponent). Always faces the champion (lock-on); WASD
 * steps around it. Over-the-shoulder camera (C1).
 */
UCLASS()
class ONTHEROPES_API AOTRPlayerMech : public AOTRMech, public IOTRBoxer, public IOTRPilotable
{
	GENERATED_BODY()

public:
	AOTRPlayerMech(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// IOTRBoxer
	virtual EOTRGuard OTR_Guard() const override { return Guard; }
	virtual bool OTR_ReceivePunch(const FOTRPunchRow& Punch, AActor* Source) override;
	virtual float OTR_StoredLoad() const override;
	virtual bool OTR_IsLeaning() const override;
	virtual bool OTR_IsInCorner() const override;
	virtual bool OTR_IsLaunching() const override;
	virtual float OTR_Armor01() const override;
	virtual const TArray<int32>& OTR_LeanSides() const override;
	virtual bool OTR_IsDodging() const override { return DodgeInvuln > 0.0f; }
	virtual bool OTR_IsDucking() const override { return DuckClock > 0.0f; }

	// IOTRPilotable (human keys and the bot)
	virtual void OTR_SetGuard(EOTRGuard InGuard) override { SetGuard(InGuard); }
	virtual void OTR_SetLeanHeld(bool bHeld) override { SetLeanHeld(bHeld); }
	virtual void OTR_MoveInput(const FVector& WorldDirection, float Scale) override { MoveInput(WorldDirection, Scale); }
	virtual bool OTR_CanMove() const override { return CanMove(); }
	virtual bool OTR_CanLeanHere() const override;
	virtual bool OTR_Dodge(float Side) override;
	virtual bool OTR_Jab() override;
	virtual bool OTR_Break() override;
	virtual bool OTR_Duck() override;
	virtual float OTR_BreakCooldown() const override { return BreakCool; }

	/** Controller entry points (human keys and the bot). */
	void SetGuard(EOTRGuard InGuard);
	void SetLeanHeld(bool bHeld);
	void MoveInput(const FVector& WorldDirection, float Scale);
	/** True when WASD may move the mech now. */
	bool CanMove() const;

	UOTRArmorComponent* GetArmor() const { return Armor; }
	UOTRRopeLeanComponent* GetLean() const { return Lean; }
	USpringArmComponent* GetBoom() const { return Boom; }
	UCameraComponent* GetCamera() const { return Camera; }

	/** Lock-on turn rate toward the champion, degrees/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FaceTurnRate = 420.0f;

protected:
	virtual const FOTRMechLook& ChooseLook() const override;
	virtual void ApplyBodyTuning() override;

private:
	void UpdatePose(float Dt);
	void TickAction(float Dt);
	bool CanAct() const;
	void ResolveStrike(EOTRPlayerStrike Kind);
	void FaceChampion(float Dt);
	AActor* ChampionActor() const;
	EOTRMatchState MatchState() const;
	UFUNCTION() void HandleArmorDepleted(AActor* LastSource);

	UPROPERTY(VisibleAnywhere, Category = "Camera") TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere, Category = "Camera") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, Category = "Gameplay") TObjectPtr<UOTRArmorComponent> Armor;
	UPROPERTY(VisibleAnywhere, Category = "Gameplay") TObjectPtr<UOTRRopeLeanComponent> Lean;
	UPROPERTY(VisibleAnywhere, Category = "Feel") TObjectPtr<UOTRGameFeelComponent> Feel;

	EOTRGuard Guard = EOTRGuard::None;
	float Stun = 0.0f;
	float HitFlash = 0.0f;
	float BaseFOV = 0.0f;
	// Camera dynamics (review 3).
	float CamRoll = 0.0f;
	float CamRollVel = 0.0f;
	float OrbitYaw = 0.0f;
	float CamClock = 0.0f;
	int32 CamPhase = -1;
	float DodgeClock = 0.0f;
	float DodgeInvuln = 0.0f;
	float DodgeCool = 0.0f;
	float DodgeSide = 1.0f;
	FVector DodgeDir = FVector::ZeroVector;
	/** 0 none, 1 jab, 2 guard-break. */
	uint8 Action = 0;
	float ActionClock = 0.0f;
	bool bActionResolved = false;
	float JabCool = 0.0f;
	float BreakCool = 0.0f;
	float DuckClock = 0.0f;
	float DuckCool = 0.0f;
	float GuardRaisedAt = -100.0f;
	bool bJabRight = false;
	TArray<int32> NoSides;
};
