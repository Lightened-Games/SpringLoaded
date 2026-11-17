// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Data/OTRTypes.h"
#include "OTRInterfaces.generated.h"

class UOTRTuning;

/** Read-only view of the match (the GameMode implements it). HUD, bot and components use it instead of casts. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UOTRMatchInfo : public UInterface { GENERATED_BODY() };

class ONTHEROPES_API IOTRMatchInfo
{
	GENERATED_BODY()

public:
	virtual const UOTRTuning* OTR_Tuning() const = 0;
	virtual EOTRMatchState OTR_State() const = 0;
	virtual EOTRDefeat OTR_DefeatReason() const = 0;
	virtual float OTR_TimeLeft() const = 0;
	virtual float OTR_Elapsed() const = 0;
	/** Clean-hit progress toward the knockout (0 .. KnockdownTarget). */
	virtual float OTR_Progress() const = 0;
	virtual bool OTR_IsAutoplay() const = 0;
	/** The last launch hit has landed: the champion is dazed and waits for the finishing blow (Q). */
	virtual bool OTR_IsFinisher() const = 0;
	/** The fight card (start screen) is up: the world idles behind it until ENTER (or autoplay) steps into the ring. */
	virtual bool OTR_IsTitle() const = 0;
	virtual float OTR_TitleAge() const = 0;
	virtual void OTR_StartFight() = 0;
	/** The player's finishing blow connected: ends the bout. */
	virtual void OTR_FinishingBlow(const FVector& Location) = 0;
	virtual AActor* OTR_Ring() const = 0;
	virtual AActor* OTR_Champion() const = 0;
	/** End screen stats: clean hits, ropes snapped, armor left (0..100). */
	virtual void OTR_Stats(int32& OutCleanHits, int32& OutSnapped, float& OutArmor) const = 0;
	/** R on an end screen. Returns false when a restart isn't allowed now. */
	virtual bool OTR_RequestRestart() = 0;
};

/** The ring: four rope sides that stretch, store, fray and snap. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UOTRRingInfo : public UInterface { GENERATED_BODY() };

class ONTHEROPES_API IOTRRingInfo
{
	GENERATED_BODY()

public:
	virtual int32 OTR_SideCount() const = 0;
	/** Distance from a location to a side's rope line, positive inside the ring (cm, horizontal). */
	virtual float OTR_SideDistance(int32 Side, const FVector& Location) const = 0;
	/** Outward unit normal of a side (horizontal). */
	virtual FVector OTR_SideNormal(int32 Side) const = 0;
	/** World point on a side's rope line at parameter T (0..1 between its posts), at canvas height. */
	virtual FVector OTR_SidePoint(int32 Side, float T) const = 0;
	/** Parameter (0..1) of a location projected on a side. */
	virtual float OTR_SideParam(int32 Side, const FVector& Location) const = 0;
	virtual bool OTR_IsSnapped(int32 Side) const = 0;
	virtual float OTR_Fray(int32 Side) const = 0;
	/** Canvas height (world Z of the mat). */
	virtual float OTR_CanvasZ() const = 0;
	virtual FVector OTR_Centre() const = 0;
	/** Visual + fray state of a lean: sides being pressed, contact location, stored load (0..1). */
	virtual void OTR_SetLean(const TArray<int32>& Sides, const FVector& Contact, float Load) = 0;
	/** An absorbed hit on the leaned sides: frays them (returns any side that snapped, or INDEX_NONE). */
	virtual int32 OTR_AbsorbHit(const TArray<int32>& Sides, float FrayGain) = 0;
	/** Continuous fray while held at full load. Returns a snapped side or INDEX_NONE. */
	virtual int32 OTR_FrayOverTime(const TArray<int32>& Sides, float FrayPerSecond, float DeltaSeconds) = 0;
	/** The player let go: ropes recoil. */
	virtual void OTR_Release(const TArray<int32>& Sides, float Load) = 0;
};

/** The player's mech as the champion sees it. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UOTRBoxer : public UInterface { GENERATED_BODY() };

class ONTHEROPES_API IOTRBoxer
{
	GENERATED_BODY()

public:
	virtual EOTRGuard OTR_Guard() const = 0;
	/** A punch resolves on the player. Returns true if it was guarded correctly. */
	virtual bool OTR_ReceivePunch(const FOTRPunchRow& Punch, AActor* Source) = 0;
	virtual float OTR_StoredLoad() const = 0;
	virtual bool OTR_IsLeaning() const = 0;
	virtual bool OTR_IsInCorner() const = 0;
	virtual bool OTR_IsLaunching() const = 0;
	virtual float OTR_Armor01() const = 0;
	/** Inside a dodge's invulnerable window. */
	virtual bool OTR_IsDodging() const = 0;
	/** Sides the player is leaning on (empty when not leaning). */
	virtual const TArray<int32>& OTR_LeanSides() const = 0;
	/** Ducking under high punches. */
	virtual bool OTR_IsDucking() const = 0;
};

/** The champion as the player, HUD and bot see it. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UOTRChampion : public UInterface { GENERATED_BODY() };

class ONTHEROPES_API IOTRChampion
{
	GENERATED_BODY()

public:
	virtual EOTRChampState OTR_ChampState() const = 0;
	/** The punch being wound up or thrown (None otherwise) and seconds until it lands. */
	virtual EOTRPunch OTR_Telegraph(float& OutSecondsToHit) const = 0;
	virtual const FOTRPunchRow* OTR_CurrentPunch() const = 0;
	virtual float OTR_Heat01() const = 0;
	virtual bool OTR_IsVenting() const = 0;
	virtual float OTR_VentTimeLeft() const = 0;
	virtual bool OTR_IsJabMode() const = 0;
	/** World location of the punching fist (for world tags). */
	virtual FVector OTR_FistLocation() const = 0;
	/** A spring launch arrives with this much stored load. */
	virtual EOTRLaunchResult OTR_ReceiveLaunch(float Load, AActor* From) = 0;
	/** A jab, guard-break, parry or duck from the player. */
	virtual EOTRStrikeResult OTR_ReceiveStrike(EOTRPlayerStrike Strike, AActor* From) = 0;
	/** Its guard is up (jabs and launches bounce off; break it with Q). */
	virtual bool OTR_IsGuarding() const = 0;
	/** 0, 1 or 2: rises with each clean hit taken (faster, more combos). */
	virtual int32 OTR_Phase() const = 0;
};

/** A pawn the controller (or the bot) can pilot: the same calls for keys and autoplay. */
UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UOTRPilotable : public UInterface { GENERATED_BODY() };

class ONTHEROPES_API IOTRPilotable
{
	GENERATED_BODY()

public:
	virtual void OTR_SetGuard(EOTRGuard Guard) = 0;
	virtual void OTR_SetLeanHeld(bool bHeld) = 0;
	virtual void OTR_MoveInput(const FVector& WorldDirection, float Scale) = 0;
	virtual bool OTR_CanMove() const = 0;
	/** Sidestep: Side -1 = left, +1 = right (relative to facing). Returns true if it started. */
	virtual bool OTR_Dodge(float Side) = 0;
	/** Can a lean start where the pawn stands now? */
	virtual bool OTR_CanLeanHere() const = 0;
	/** Weak jab (E), guard-break (Q), duck (C). Return true if the move started. */
	virtual bool OTR_Jab() = 0;
	virtual bool OTR_Break() = 0;
	virtual bool OTR_Duck() = 0;
	/** Seconds until the guard-break is ready again (0 = ready). */
	virtual float OTR_BreakCooldown() const = 0;
};
