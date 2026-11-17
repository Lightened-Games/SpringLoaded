// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OTRTuning.generated.h"

class UDataTable;
class UOTRHUDStyle;

USTRUCT(BlueprintType)
struct FOTRMatchTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RoundSeconds = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 CountdownSeconds = 3;
	/** Autoplay lets the fight card read this long before it presses ENTER (2.5.2). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float TitleHoldSeconds = 2.5f;
	/** Clean-hit progress (sum of launch loads) that knocks the champion out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KnockdownTarget = 3.0f;
	/** The bell timer turns to the danger colour and ticks below this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LastStretchSeconds = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KillZ = -2000.0f;
	/** Below this height anything is put back at its start (should never fire). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RespawnBelowZ = -600.0f;
	/** A player whose centre is this far past an open side's rope line is out of the ring. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RingOutMargin = 40.0f;
	/** Seconds after Victory/Defeat before autoplay restarts (or quits with -autoplayexit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AutoplayRestartDelay = 5.0f;
	/** Music tempo steps up at these elapsed times. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") TArray<float> TempoSteps = { 40.0f, 70.0f };
};

USTRUCT(BlueprintType)
struct FOTRPlayerTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Armor = 100.0f;
	/** Share of a punch's damage that gets through the right guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GuardedDamageScale = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WalkSpeed = 260.0f;
	/** Walk speed while a guard is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GuardWalkSpeed = 190.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Acceleration = 1300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BrakingDeceleration = 1800.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CapsuleRadius = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CapsuleHalfHeight = 130.0f;
	/** Knock-backs are applied as a velocity: horizontal speed per cm of knock-back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KnockbackSpeedPerCm = 9.0f;
	/** Seconds input is locked after an unguarded hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StunSeconds = 0.25f;
	/** Dodge: a quick sidestep (Shift) with a short invulnerable window. Dodging off the ropes loses the stored load. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DodgeSpeed = 900.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DodgeSeconds = 0.28f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DodgeInvulnerableSeconds = 0.34f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DodgeCooldown = 0.9f;
	/** Weak jab (E): wind-up, travel, recovery, cooldown (s) and reach (centre to centre, cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Jab") float JabWindup = 0.08f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Jab") float JabStrike = 0.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Jab") float JabRecover = 0.22f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Jab") float JabCooldown = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Jab") float JabReach = 370.0f;
	/** Guard-break (Q): a slow two-handed smash that opens a raised guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Break") float BreakWindup = 0.38f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Break") float BreakStrike = 0.14f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Break") float BreakRecover = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Break") float BreakCooldown = 3.0f;
	/** Parry: the right guard raised within this many seconds of the hit: no damage, champion staggers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Parry") float ParryWindow = 0.14f;
	/** A parry on the ropes stores this much more energy than a normal guarded hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Parry") float ParryLoadMultiplier = 1.25f;
	/** Duck (C): high punches miss; a body shot caught ducking hurts more. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Duck") float DuckSeconds = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Duck") float DuckCooldown = 0.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Duck") float DuckLowDamageScale = 1.3f;
};

USTRUCT(BlueprintType)
struct FOTRCameraTuning
{
	GENERATED_BODY()

	/** Over-the-shoulder lock-on (provisional, C8: the user may pick a wider arm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ArmLength = 520.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Pitch = -14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ShoulderOffset = 90.0f;
	/** Pivot height above the capsule centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PivotHeight = 120.0f;
	/** Establishing framing during the countdown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ReadyArmLength = 950.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ReadyPitch = -20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BlendSpeed = 2.4f;
	/** How fast the lock-on yaw follows the champion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float YawFollowSpeed = 5.0f;
	/** The look point sits this fraction of the way from the player to the champion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LookBias = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FOV = 75.0f;
	/** Extra arm length and FOV kick during a launch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LaunchFOVKick = 16.0f;

	// Review 3 ("more dynamics"): a camera that reacts to the fight (all provisional, C8).
	/** Fight card: a slow orbit around Old Iron with the hall behind. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float TitleArmLength = 1150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float TitlePitch = -12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float TitleOrbitSpeed = 7.0f;
	/** Launch off the ropes: the camera dollies in behind the charge and tilts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float LaunchDolly = 130.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float LaunchRoll = 5.0f;
	/** A clean (unguarded) hit on you kicks the camera into a dutch angle that springs back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float HitRollKick = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float RollStiffness = 110.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float RollDamping = 11.0f;
	/** The dazed champion: a three-quarter side view that frames both fighters for the finishing blow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float FinisherYawOffset = 55.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float FinisherArmLength = 640.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float FinisherPitch = -7.0f;
	/** After the bell: a slow orbit (victory) or a low, tilted look down (defeat). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float EndOrbitSpeed = 9.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float EndArmLength = 780.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float EndPitch = -10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float DefeatPitch = -34.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float DefeatRoll = 7.0f;
	/** Broadcast handheld: a slow drift on the camera (degrees, Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float Handheld = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dynamics") float HandheldHz = 0.55f;
};

USTRUCT(BlueprintType)
struct FOTRRopeTuning
{
	GENERATED_BODY()

	/** Inner half size of the ring (post centres at +-RingHalf), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RingHalf = 620.0f;
	/** Height of the canvas above the hall floor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PlatformHeight = 120.0f;
	/** Apron beyond the ropes, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ApronWidth = 150.0f;
	/** Strand heights above the canvas (mech hip, waist, chest). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") TArray<float> StrandHeights = { 75.0f, 130.0f, 185.0f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") int32 Segments = 14;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CableRadius = 4.0f;
	/** Gap between the player's capsule and an intact rope line that still counts as "against the ropes", cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LeanReach = 45.0f;
	/** Below this load a release just steps off the rope. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MinLaunchLoad = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LaunchDashTime = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LaunchMaxDistance = 1100.0f;
	/** A launch hits when the capsules come this close (gap), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LaunchContactGap = 25.0f;
	/** Corner = two ropes: energy per absorbed hit is multiplied (provisional, C8). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CornerLoadMultiplier = 2.0f;
	/** Cap of one launch's power (1 = one full-power hit; provisional, C8). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MaxLoad = 1.0f;
	/** Fray per second while a rope is held at full load ("load it too hard"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FrayAtFullPerSecond = 0.25f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FrayWarn = 0.7f;
	/** Max rope deflection at full load, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MaxDeflect = 48.0f;
	/** Deflection with no load (the mech's weight on the rope), fraction of MaxDeflect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float RestDeflect = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SpringStiffness = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SpringDamping = 7.0f;
	/** Spring kick of an absorbed hit and of a release, cm/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitImpulse = 220.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ReleaseImpulse = -900.0f;
	/** Rope glow (accent) at full load. Emissive stays <= 3.5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float GlowAtFull = 1.8f;
	/** Leaning breaks when the player is pushed this far off the rope, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float LeanBreakDistance = 60.0f;
	/** A snapped strand whips out for this long, s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SnapWhipSeconds = 0.5f;
};

USTRUCT(BlueprintType)
struct FOTRChampionTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float WalkSpeed = 300.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float Acceleration = 1100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CapsuleRadius = 80.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CapsuleHalfHeight = 160.0f;
	/** Champion start (ring-local), player start is mirrored. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FVector2D StartLocation = FVector2D(330.0f, 0.0f);
	/** Seconds after GO before it engages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float EngageDelay = 0.8f;
	/** Centre-to-centre distance it keeps while punching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PreferredDistance = 290.0f;
	/** Review 2 (anti-clipping): the champion never stands closer than this, centre to centre, except while the player launches or deals the finishing blow. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MinSeparation = 275.0f;
	/** A punch lands if the centres are this close when it resolves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PunchReach = 340.0f;
	/** Seconds between punch starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PunchInterval = 1.15f;
	/** Interval while the player is pinned in a corner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float PinnedInterval = 0.75f;
	/** It switches to jabs when the player's stored rope load reaches this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float JabModeLoad = 0.5f;
	/** Share of jabs in jab mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float JabShare = 0.6f;
	/** Share of hooks among power punches (the rest are body shots). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HookShare = 0.5f;
	/** A launch that is not clean is blocked with this chance, otherwise sidestepped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BlockChance = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SidestepDistance = 170.0f;
	/** Clean hit: pushed back this far per unit of launch load (plus a base). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StaggerDistance = 360.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float StaggerSeconds = 1.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BlockSeconds = 0.6f;
	/** Heat model (UOTRHeatComponent). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float InitialHeat = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HeatCoolRate = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float OverheatThreshold = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float VentSeconds = 2.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HeatAfterVent = 30.0f;
	/** Arm glow (heat colour) at full heat. Emissive stays <= 3.5. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HeatGlowMax = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KOSeconds = 1.4f;
	/** Haymaker: thrown when heat is above this fraction, with this chance per punch, at most once per cooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HaymakerHeat = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HaymakerChance = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HaymakerCooldown = 5.0f;
	/** A dodged punch over-extends the champion: extra heat and a short stagger. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float OverextendHeat = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float OverextendSeconds = 0.6f;
	/** Player strikes on the champion: heat added, stagger seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Strikes") float JabHeat = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Strikes") float JabInterruptStagger = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Strikes") float BreakHeat = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Strikes") float GuardBreakStagger = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Strikes") float ParryHeat = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Strikes") float ParryStagger = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Strikes") float DuckOverextendHeat = 10.0f;
	/** Guard: raised after GuardAfterJabs jabs land inside JabMemorySeconds, or by choice (GuardChance per punch
	 *  decision while the player is off the ropes), held GuardSeconds, at most once per GuardCooldown. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guard") int32 GuardAfterJabs = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guard") float JabMemorySeconds = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guard") float GuardChance = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guard") float GuardSeconds = 1.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Guard") float GuardCooldown = 3.0f;
	/** Combos: a follow-up punch's wind-up is this fraction of its own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Combo") float ComboTelegraphScale = 0.6f;
	/** Feint: the fake stops at this fraction of the wind-up; the jab that follows uses this telegraph scale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Combo") float FeintAt = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Combo") float FeintJabTelegraphScale = 0.55f;
	/** Phases (0, 1, 2 clean hits taken): punch interval scale, extra combo and feint chance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Phases") TArray<float> PhaseIntervalScale = { 1.0f, 0.88f, 0.76f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Phases") TArray<float> PhaseFollowBonus = { 0.0f, 0.15f, 0.3f };
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Phases") TArray<float> PhaseFeintBonus = { 0.0f, 0.1f, 0.2f };
};

USTRUCT(BlueprintType)
struct FOTRFeelTuning
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitStopSeconds = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitStopDilation = 0.05f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ShakeFalloff = 2200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float MusicVolume = 0.38f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float AmbienceVolume = 0.45f;
	/** Clean-hit slow motion (dilation, real seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CleanHitSlowMo = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CleanHitSlowMoSeconds = 0.55f;
	/** A defeat lands in a short slow motion (the knockout has its own, KOSlowMo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DefeatSlowMo = 0.45f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float DefeatSlowMoSeconds = 0.6f;
	/** Rope snap: brief slow motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SnapSlowMo = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SnapSlowMoSeconds = 0.35f;
	/** Knockout: the champion falls in slow motion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KOSlowMo = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float KOSlowMoSeconds = 1.6f;
	/** Camera FOV punches in degrees (negative = zoom in), decaying at FOVPunchDecay per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FOVPunchClean = -12.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FOVPunchHit = -5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FOVPunchSnap = 7.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FOVPunchVent = -4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FOVPunchKO = -16.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FOVPunchDecay = 4.0f;
	/** Chromatic fringe pulses (post process scene fringe), decaying per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FringeClean = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FringeHit = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FringeDecay = 5.0f;
	/** Multiplier on every camera shake. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ShakeScale = 1.35f;
	/** Spark / dust burst sizes on clean hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CleanSparkScale = 2.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CleanDustScale = 2.2f;
};

/** Look and proportions of one mech (rigid code-built parts, procedural animation). */
USTRUCT(BlueprintType)
struct FOTRMechLook
{
	GENERATED_BODY()

	/** Meshes are SM_OTR_<Prefix>Pelvis / Torso / Head / UpperArm / Forearm / Fist / Thigh / Shin / Foot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") FName Prefix = TEXT("Iron");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") FLinearColor Paint = FLinearColor(0.55f, 0.4f, 0.08f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") FLinearColor Trim = FLinearColor(0.08f, 0.08f, 0.09f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") FLinearColor Glow = FLinearColor(1.0f, 0.7f, 0.3f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float GlowEmissive = 2.0f;
	/** Overrides for this mech's paint and trim MIDs (MI_OTR_Paint scalars such as EdgeWear, RustBias, Specular). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") TMap<FName, float> PaintScalars;
	/** Bare-metal parts (joints, frames) brightness (MI_OTR_MechMetal "Brightness", default 0.85); below 0 keeps the shared instance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float MetalBrightness = -1.0f;
	/** Hip pivot height above the feet at rest. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float HipHeight = 112.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float HipHalfWidth = 34.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float ThighLength = 62.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float ShinLength = 60.0f;
	/** Ankle height above the sole. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float AnkleHeight = 16.0f;
	/** Waist pivot above the hip pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float WaistHeight = 22.0f;
	/** Shoulder pivots relative to the waist pivot (X fwd, Y right half width, Z up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") FVector Shoulder = FVector(0.0f, 66.0f, 82.0f);
	/** Neck pivot above the waist pivot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float NeckHeight = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float UpperArmLength = 70.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float ForearmLength = 66.0f;
	/** Feet stand this far apart (half), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float StanceHalfWidth = 36.0f;
	/** A foot steps when its target is this far from where it is planted, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float StepThreshold = 34.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float StepSeconds = 0.34f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float StepHeight = 14.0f;
	/** The step target leads the body by this many seconds of velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float StepLead = 0.22f;
	/** Pelvis drop per step (heavy gait). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float StepBob = 5.0f;
	/** Servo sound pitch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") float VoicePitch = 1.0f;
};

/**
 * Every balance and feel number of On the Ropes (DA_OTR_Tuning). The GameMode loads it; components read it through
 * OTR::Tuning(). Editing data only, never code, changes the game (architecture directive 3).
 */
UCLASS(BlueprintType)
class ONTHEROPES_API UOTRTuning : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UOTRTuning();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FOTRMatchTuning Match;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FOTRPlayerTuning Player;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FOTRCameraTuning Camera;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FOTRRopeTuning Rope;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FOTRChampionTuning Champion;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") FOTRFeelTuning Feel;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") FOTRMechLook PlayerLook;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Look") FOTRMechLook ChampionLook;

	/** DT_OTR_Punches (FOTRPunchRow), rows Hook / Body / Jab. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") TSoftObjectPtr<UDataTable> PunchTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/OntheRopes/Data/DT_OTR_Punches.DT_OTR_Punches")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") TSoftObjectPtr<UOTRHUDStyle> HUDStyle = TSoftObjectPtr<UOTRHUDStyle>(FSoftObjectPath(TEXT("/Game/OntheRopes/Data/DA_OTR_HUDStyle.DA_OTR_HUDStyle")));
	/** Sounds are found as <SoundRoot>/S_OTR_<Cue>, VFX as <VfxRoot>/NS_OTR_<Name>, meshes/materials by name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") FString SoundRoot = TEXT("/Game/OntheRopes/Audio");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") FString VfxRoot = TEXT("/Game/OntheRopes/VFX");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") FString MeshRoot = TEXT("/Game/OntheRopes/Meshes");
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") FString MaterialRoot = TEXT("/Game/OntheRopes/Materials");
};

namespace OTR
{
	/** The match's tuning asset, or the class defaults when none is loaded (never null). */
	ONTHEROPES_API const UOTRTuning* Tuning(const UObject* WorldContext);
}
