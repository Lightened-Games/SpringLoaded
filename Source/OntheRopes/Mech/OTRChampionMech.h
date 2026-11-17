// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Mech/OTRMech.h"
#include "Interfaces/OTRInterfaces.h"
#include "OTRChampionMech.generated.h"

class UOTRHeatComponent;
class UOTRChampionBrainComponent;

/**
 * "Titan", the champion: a newer, taller machine that punches twice as hard and runs hot. Its decisions live in
 * UOTRChampionBrainComponent, its overheat in UOTRHeatComponent; this actor only exposes them through IOTRChampion.
 */
UCLASS()
class ONTHEROPES_API AOTRChampionMech : public AOTRMech, public IOTRChampion
{
	GENERATED_BODY()

public:
	AOTRChampionMech(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;

	// IOTRChampion
	virtual EOTRChampState OTR_ChampState() const override;
	virtual EOTRPunch OTR_Telegraph(float& OutSecondsToHit) const override;
	virtual const FOTRPunchRow* OTR_CurrentPunch() const override;
	virtual float OTR_Heat01() const override;
	virtual bool OTR_IsVenting() const override;
	virtual float OTR_VentTimeLeft() const override;
	virtual bool OTR_IsJabMode() const override;
	virtual FVector OTR_FistLocation() const override;
	virtual EOTRLaunchResult OTR_ReceiveLaunch(float Load, AActor* From) override;
	virtual EOTRStrikeResult OTR_ReceiveStrike(EOTRPlayerStrike Strike, AActor* From) override;
	virtual bool OTR_IsGuarding() const override;
	virtual int32 OTR_Phase() const override;

	UOTRHeatComponent* GetHeat() const { return Heat; }

protected:
	virtual const FOTRMechLook& ChooseLook() const override;
	virtual void ApplyBodyTuning() override;

private:
	UPROPERTY(VisibleAnywhere, Category = "Gameplay") TObjectPtr<UOTRHeatComponent> Heat;
	UPROPERTY(VisibleAnywhere, Category = "Gameplay") TObjectPtr<UOTRChampionBrainComponent> Brain;
};
