// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "OTRTypes.generated.h"

/** The four explicit match states (Pillar 5). */
UENUM(BlueprintType)
enum class EOTRMatchState : uint8
{
	Ready,
	Active,
	Victory,
	Defeat
};

/** Why the bout was lost (shown on the Defeat screen). */
UENUM(BlueprintType)
enum class EOTRDefeat : uint8
{
	None,
	Armor,
	RingOut,
	Bell
};

/** The champion's three punches. Hooks and jabs come high, body shots low. */
UENUM(BlueprintType)
enum class EOTRPunch : uint8
{
	None,
	Hook,
	Body,
	Jab,
	/** Slow overhead the champion throws when it runs hot: the guard doesn't stop it, dodge it. */
	Haymaker
};

/** The player's guard. */
UENUM(BlueprintType)
enum class EOTRGuard : uint8
{
	None,
	High,
	Low
};

/** What the champion is doing. */
UENUM(BlueprintType)
enum class EOTRChampState : uint8
{
	Idle,
	Approach,
	Windup,
	Strike,
	Recover,
	Venting,
	Stagger,
	Block,
	Sidestep,
	KO,
	/** Guard up: blocks jabs and launches until a guard-break (Q) or it lowers it. */
	Guarding
};

/** What the player threw or did at the champion. */
UENUM(BlueprintType)
enum class EOTRPlayerStrike : uint8
{
	/** Weak jab (E): chips heat, interrupts a wind-up, bounces off a raised guard. */
	Jab,
	/** Guard-break (Q): slow, smashes a raised guard. */
	Break,
	/** A guard raised in the last instant before a punch lands. */
	Parry,
	/** A high punch slipped under (C): the swing over-extends. */
	Duck
};

/** How a player strike landed. */
UENUM(BlueprintType)
enum class EOTRStrikeResult : uint8
{
	Hit,
	Interrupted,
	Blocked,
	GuardBroken,
	Missed
};

/** How a spring launch ended. */
UENUM(BlueprintType)
enum class EOTRLaunchResult : uint8
{
	Clean,
	Blocked,
	Sidestepped,
	Whiff
};

/** One champion punch (DT_OTR_Punches). Every number of a punch lives here. */
USTRUCT(BlueprintType)
struct FOTRPunchRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") EOTRPunch Type = EOTRPunch::Hook;
	/** World tag shown during the wind-up ("HIGH" / "LOW"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") FText TagText;
	/** High punches need the high guard, low punches the low guard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") bool bHigh = true;
	/** The guard does not reduce it (only a dodge avoids it). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") bool bUnblockable = false;
	/** Wind-up (telegraph) seconds before the fist moves. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float Telegraph = 0.45f;
	/** Seconds the fist travels; the hit resolves at the end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float StrikeTime = 0.12f;
	/** Seconds the arm takes to come back. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float Recover = 0.3f;
	/** Armor damage when not guarded correctly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float Damage = 16.0f;
	/** Rope energy stored when absorbed while leaning (fraction of a full rope). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float LoadGain = 0.22f;
	/** Rope fray added per loaded side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float FrayGain = 0.09f;
	/** Heat the champion's arms gain from throwing it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float Heat = 18.0f;
	/** Knock-back of an unguarded hit, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float Knockback = 60.0f;
	/** Pull toward the ring centre of an unguarded hit (jabs drag), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float Drag = 0.0f;
	/** Fraction of stored rope energy lost to an unguarded hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float LoadBleed = 0.0f;
	/** Camera shake scale when it lands. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Punch") float Shake = 1.0f;
	/** Combo: the punch thrown straight after this one (None = no combo), and the chance it follows. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo") EOTRPunch FollowUp = EOTRPunch::None;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo") float FollowChance = 0.0f;
	/** Chance the wind-up is a feint: it stops half way and a quick jab comes instead. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo") float FeintChance = 0.0f;
};

/**
 * One placed piece of the hall (DT_OTR_Arena, written by Tools/Editor/build_content.py). Meshes are found as
 * SM_OTR_<Mesh>, materials as MI_OTR_<Material>. A row with LightCandela > 0 adds a light at its location.
 */
USTRUCT(BlueprintType)
struct FOTRPropRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FName Mesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FName Material;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FName Material2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FName Material3;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FVector Location = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FRotator Rotation = FRotator::ZeroRotator;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FVector Scale = FVector::OneVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") bool bCollide = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") bool bCastShadow = true;
	/** Tint passed to the material's "Tint" parameter (alpha 0 = leave the instance's own tint). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") FLinearColor Tint = FLinearColor(1.0f, 1.0f, 1.0f, 0.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") float LightCandela = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") FLinearColor LightColour = FLinearColor(1.0f, 0.85f, 0.65f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") FRotator LightRotation = FRotator(-90.0f, 0.0f, 0.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") float LightRadius = 3200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") float LightCone = 50.0f;
	/** Point light instead of a spot. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") bool bPointLight = false;
	/** Tungsten flicker (practicals). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") bool bFlicker = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Light") bool bLightShadows = true;
	/** Slow swing of hanging pieces (chains, lamp cans), degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Prop") float Sway = 0.0f;
};

namespace OTR
{
	static const FName PlayerTag(TEXT("OTR.Player"));
	static const FName ChampionTag(TEXT("OTR.Champion"));
	static const FName NoCollideTag(TEXT("NoCollide"));
}
