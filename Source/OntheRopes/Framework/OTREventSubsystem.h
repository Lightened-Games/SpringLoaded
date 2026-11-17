// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Data/OTRTypes.h"
#include "OTREventSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOTROnMatchState, EOTRMatchState, State, EOTRDefeat, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOTROnCountdown, int32, SecondsLeft);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOTROnNotice, FText, Title, FText, Sub, bool, bDanger);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOTROnPrompt, FText, Text, AActor*, Anchor, FVector, Offset, bool, bDanger);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOTROnInput, FName, KeyId, bool, bSimulated);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOTROnPlayerHit, float, Damage, bool, bGuarded, EOTRPunch, Punch, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOTROnPopup, FText, Text, FVector, WorldLocation, FLinearColor, Colour);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOTROnImpact, FVector, Location, float, Strength, bool, bHeavy);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOTROnCleanHit, float, Load, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOTROnLaunch, EOTRLaunchResult, Result, float, Load, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOTROnRopeEvent, int32, Side, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOTROnChampionEvent, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOTROnTelegraph, EOTRPunch, Punch, FVector, FistLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOTROnToggle, bool, bOn);

/**
 * The match event bus. Gameplay broadcasts here; the HUD, game feel and audio only listen (architecture directive 2).
 * First-time prompts are de-duplicated per bout by id.
 */
UCLASS()
class ONTHEROPES_API UOTREventSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UOTREventSubsystem* Get(const UObject* WorldContext);

	UPROPERTY(BlueprintAssignable) FOTROnMatchState OnMatchState;
	UPROPERTY(BlueprintAssignable) FOTROnCountdown OnCountdown;
	UPROPERTY(BlueprintAssignable) FOTROnNotice OnNotice;
	UPROPERTY(BlueprintAssignable) FOTROnPrompt OnPrompt;
	UPROPERTY(BlueprintAssignable) FOTROnInput OnInput;
	UPROPERTY(BlueprintAssignable) FOTROnPlayerHit OnPlayerHit;
	UPROPERTY(BlueprintAssignable) FOTROnPopup OnPopup;
	/** Any heavy contact (footfall, block, clang): game feel turns it into shake + dust. */
	UPROPERTY(BlueprintAssignable) FOTROnImpact OnImpact;
	UPROPERTY(BlueprintAssignable) FOTROnCleanHit OnCleanHit;
	UPROPERTY(BlueprintAssignable) FOTROnLaunch OnLaunch;
	UPROPERTY(BlueprintAssignable) FOTROnRopeEvent OnRopeSnapped;
	UPROPERTY(BlueprintAssignable) FOTROnRopeEvent OnRopeFrayWarning;
	UPROPERTY(BlueprintAssignable) FOTROnChampionEvent OnOverheat;
	UPROPERTY(BlueprintAssignable) FOTROnChampionEvent OnVentEnd;
	UPROPERTY(BlueprintAssignable) FOTROnTelegraph OnTelegraph;
	UPROPERTY(BlueprintAssignable) FOTROnToggle OnJabMode;
	UPROPERTY(BlueprintAssignable) FOTROnToggle OnLean;

	/** Fires a first-time prompt once per bout. Returns true if it was shown. */
	bool PromptOnce(FName Id, const FText& Text, AActor* Anchor, const FVector& Offset = FVector::ZeroVector, bool bDanger = false);
	bool HasPrompted(FName Id) const { return Prompted.Contains(Id); }

	/** Running counters for the end screen and telemetry. */
	int32 CleanHits = 0;
	int32 BlockedLaunches = 0;
	int32 RopesSnapped = 0;
	int32 HitsTaken = 0;
	int32 HitsGuarded = 0;
	int32 Vents = 0;
	int32 CornerLoads = 0;
	int32 Dodges = 0;
	int32 Jabs = 0;
	int32 JabsLanded = 0;
	int32 Interrupts = 0;
	int32 GuardBreaks = 0;
	int32 Parries = 0;
	int32 Ducks = 0;
	float LowestArmor = 100.0f;

private:
	TSet<FName> Prompted;
};
