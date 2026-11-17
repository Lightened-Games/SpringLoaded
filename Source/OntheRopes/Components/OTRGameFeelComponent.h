// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/OTRTypes.h"
#include "Containers/Ticker.h"
#include "OTRGameFeelComponent.generated.h"

class UAudioComponent;

/**
 * Juice (P20): camera shakes scaled by distance, 50 ms hit-stop on unguarded hits, slow motion on clean hits, the
 * bell, crowd, music bed with tempo steps, last-stretch ticks and the victory / defeat stings. Listens to
 * UOTREventSubsystem only.
 */
UCLASS(ClassGroup = (OntheRopes), meta = (BlueprintSpawnableComponent))
class ONTHEROPES_API UOTRGameFeelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UOTRGameFeelComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Current camera FOV punch (degrees, decays in real time); the owner's camera adds it on top of its own FOV. */
	float GetFOVPunch() const { return FOVPunch; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UFUNCTION() void HandleImpact(FVector Location, float Strength, bool bHeavy);
	UFUNCTION() void HandlePlayerHit(float Damage, bool bGuarded, EOTRPunch Punch, FVector Location);
	UFUNCTION() void HandleCleanHit(float Load, FVector Location);
	UFUNCTION() void HandleLaunch(EOTRLaunchResult Result, float Load, FVector Location);
	UFUNCTION() void HandleSnap(int32 Side, FVector Location);
	UFUNCTION() void HandleFray(int32 Side, FVector Location);
	UFUNCTION() void HandleState(EOTRMatchState State, EOTRDefeat Reason);
	UFUNCTION() void HandleCountdown(int32 SecondsLeft);
	UFUNCTION() void HandleOverheat(FVector Location);
	void Punch(float FOV, float InFringe);
	void TickPunch(float RealDt);
	void TimeWarp(float Dilation, float RealSeconds);
	void Shake(bool bHeavy, float Scale);

	UPROPERTY() TObjectPtr<UAudioComponent> Music;
	UPROPERTY() TObjectPtr<UAudioComponent> Crowd;
	FTSTicker::FDelegateHandle WarpHandle;
	int32 TempoStep = 0;
	int32 LastTick = -1;
	float CrowdBoost = 0.0f;
	float FOVPunch = 0.0f;
	float Fringe = 0.0f;
};
