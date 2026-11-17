// Copyright On the Ropes. All Rights Reserved.

#include "Framework/OTRAutoplayComponent.h"
#include "Framework/OTRPlayerController.h"
#include "Interfaces/OTRInterfaces.h"
#include "Data/OTRTuning.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OntheRopes.h"

namespace
{
	const IOTRMatchInfo* MatchOf(const UObject* Ctx)
	{
		const UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
		return World ? Cast<IOTRMatchInfo>(World->GetAuthGameMode()) : nullptr;
	}
}

UOTRAutoplayComponent::UOTRAutoplayComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

AOTRPlayerController* UOTRAutoplayComponent::PC() const
{
	// The bot lives on (and only on) the controller that created it.
	return Cast<AOTRPlayerController>(GetOwner());
}

APawn* UOTRAutoplayComponent::Pawn() const
{
	const AOTRPlayerController* C = PC();
	return C ? C->GetPawn() : nullptr;
}

void UOTRAutoplayComponent::ChooseTarget()
{
	const IOTRMatchInfo* Match = MatchOf(this);
	const IOTRRingInfo* Ring = Match ? Cast<IOTRRingInfo>(Match->OTR_Ring()) : nullptr;
	const IOTRBoxer* Boxer = Cast<IOTRBoxer>(Pawn());
	APawn* P = Pawn();
	if (!Ring || !Boxer || !P)
	{
		return;
	}
	const UOTRTuning* T = Match->OTR_Tuning();
	const float Radius = T->Player.CapsuleRadius;
	const FVector Loc = P->GetActorLocation();
	TargetCorner = INDEX_NONE;
	// One corner gamble when it is safe: both ropes fresh, armor healthy, a hit already on the board.
	static const bool bCornerTest = FParse::Param(FCommandLine::Get(), TEXT("OTRCornerTest"));
	if (!bCornerUsed && (bCornerTest || (Boxer->OTR_Armor01() > CornerMinArmor && Match->OTR_Progress() >= CornerAfterProgress)))
	{
		float Best = 1e9f;
		for (int32 C = 0; C < 4; ++C)
		{
			const int32 A = C;
			const int32 B = (C + 1) % 4;
			if (Ring->OTR_IsSnapped(A) || Ring->OTR_IsSnapped(B) || Ring->OTR_Fray(A) > CornerMaxFray || Ring->OTR_Fray(B) > CornerMaxFray)
			{
				continue;
			}
			const FVector Post = Ring->OTR_SidePoint(A, 1.0f);
			const float D = FVector::Dist2D(Post, Loc);
			if (D < Best)
			{
				Best = D;
				TargetCorner = C;
			}
		}
		if (TargetCorner != INDEX_NONE)
		{
			const FVector Post = Ring->OTR_SidePoint(TargetCorner, 1.0f);
			const FVector In = (Ring->OTR_Centre() - Post).GetSafeNormal2D();
			TargetPoint = Post + In * (Radius + 8.0f) * 1.414f;
			TargetSide = INDEX_NONE;
			UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Bot heads for corner %d"), TargetCorner);
			return;
		}
	}
	// Otherwise the freshest intact side, away from open sides, not too far.
	float BestScore = 1e9f;
	int32 Best = INDEX_NONE;
	for (int32 S = 0; S < Ring->OTR_SideCount(); ++S)
	{
		if (Ring->OTR_IsSnapped(S))
		{
			continue;
		}
		const float Fray = Ring->OTR_Fray(S);
		float Score = Fray * FrayWeight + FMath::Max(0.0f, Ring->OTR_SideDistance(S, Loc)) / 1200.0f;
		if (Fray >= AvoidFray)
		{
			Score += 5.0f;
		}
		// An open neighbour makes a knock-back along the rope dangerous.
		if (Ring->OTR_IsSnapped((S + 1) % 4) || Ring->OTR_IsSnapped((S + 3) % 4))
		{
			Score += 0.4f;
		}
		// Prefer the side behind us (the champion is in front).
		if (const AActor* Champ = Match->OTR_Champion())
		{
			const FVector Away = (Loc - Champ->GetActorLocation()).GetSafeNormal2D();
			Score -= 0.35f * FVector::DotProduct(Away, Ring->OTR_SideNormal(S));
		}
		if (Score < BestScore)
		{
			BestScore = Score;
			Best = S;
		}
	}
	TargetSide = Best;
	if (Best != INDEX_NONE)
	{
		const float U = FMath::Clamp(Ring->OTR_SideParam(Best, Loc), 0.3f, 0.7f);
		TargetPoint = Ring->OTR_SidePoint(Best, U) - Ring->OTR_SideNormal(Best) * (Radius + 18.0f);
	}
}

void UOTRAutoplayComponent::TickGuard(float Dt)
{
	const IOTRMatchInfo* Match = MatchOf(this);
	const IOTRChampion* Champ = Match ? Cast<IOTRChampion>(Match->OTR_Champion()) : nullptr;
	AOTRPlayerController* C = PC();
	if (!Champ || !C)
	{
		return;
	}
	if (bThrowMatch)
	{
		C->BotGuard(EOTRGuard::None);
		return;
	}
	float ToHit = 0.0f;
	const EOTRPunch Punch = Champ->OTR_Telegraph(ToHit);
	if (Punch != SeenPunch)
	{
		SeenPunch = Punch;
		if (Punch != EOTRPunch::None)
		{
			ReactClock = FMath::Lerp(ReactionRange.X, ReactionRange.Y, Skill) * FMath::FRandRange(0.8f, 1.2f);
			const bool bHigh = Punch != EOTRPunch::Body;
			const bool bMistake = FMath::FRand() < MistakeAtZero * (1.0f - Skill);
			PlannedGuard = (bHigh != bMistake) ? EOTRGuard::High : EOTRGuard::Low;
			// Haymaker: absorb it on a well-loaded rope when armor allows, otherwise dodge.
			// In the open, the bot sometimes dodges power punches too (over-extends the champion).
			const IOTRBoxer* B = Cast<IOTRBoxer>(Pawn());
			const bool bLeaning = B && B->OTR_IsLeaning();
			const bool bLoaded = bLeaning && B->OTR_Armor01() > 0.3f;
			// Haymaker on a loaded rope with armor: absorb. On the ropes with low armor: duck it (keeps the lean).
			bPlannedDuck = (Punch == EOTRPunch::Haymaker && bLeaning && (!bLoaded || B->OTR_Armor01() < 0.6f))
				|| (Punch == EOTRPunch::Hook && FMath::FRand() < (bLeaning ? RopeDuckChance : DuckChance) * Skill);
			bPlannedDodge = !bPlannedDuck && ((Punch == EOTRPunch::Haymaker && !bLeaning)
				|| (Punch != EOTRPunch::Jab && B && !bLeaning && FMath::FRand() < 0.2f * Skill));
			bPlannedParry = !bPlannedDuck && !bPlannedDodge && Punch != EOTRPunch::Haymaker && FMath::FRand() < ParryChance * Skill;
			bPlannedJab = !bLeaning && Punch != EOTRPunch::Haymaker && Punch != EOTRPunch::Jab && FMath::FRand() < InterruptChance * Skill;
			bDodged = false;
			bActed = false;
		}
	}
	if (Punch != EOTRPunch::None)
	{
		ReactClock -= Dt;
		const float Window = Match->OTR_Tuning()->Player.ParryWindow;
		if (bPlannedParry)
		{
			// Hands down (or the other guard), then snap the right guard up inside the parry window.
			Guard = ToHit <= Window * 0.6f ? PlannedGuard : EOTRGuard::None;
		}
		else if (ReactClock <= 0.0f)
		{
			Guard = PlannedGuard;
		}
		if (bPlannedDuck && !bActed && ReactClock <= 0.0f && ToHit <= Match->OTR_Tuning()->Player.DuckSeconds * 0.6f)
		{
			bActed = true;
			C->BotDuck();
		}
		if (bPlannedJab && !bActed && ReactClock <= 0.0f && ToHit > 0.2f)
		{
			APawn* P = Pawn();
			const AActor* ChampActor = Match->OTR_Champion();
			if (P && ChampActor && FVector::Dist2D(P->GetActorLocation(), ChampActor->GetActorLocation()) <= Match->OTR_Tuning()->Player.JabReach - 20.0f)
			{
				bActed = true;
				C->BotJab();
			}
		}
		// Dodge late (inside the invulnerable window) so the swing over-extends.
		if (bPlannedDodge && !bDodged && ReactClock <= 0.0f && ToHit <= 0.22f)
		{
			bDodged = true;
			if (APawn* P = Pawn())
			{
				const FVector ToCentre = (Match->OTR_Ring() ? Match->OTR_Ring()->GetActorLocation() : FVector::ZeroVector) - P->GetActorLocation();
				const float Side = FVector::DotProduct(P->GetActorRightVector(), ToCentre) >= 0.0f ? 1.0f : -1.0f;
				// Dodge first (it breaks the lean without launching), then let go of lean.
				C->BotDodge(Side);
				if (bLeanHeld)
				{
					bLeanHeld = false;
					bSawLean = false;
					C->BotLean(false);
				}
			}
		}
	}
	else
	{
		// Between punches: keep the high guard up on the ropes (jabs and hooks come high), hands loose when walking.
		const IOTRBoxer* Boxer = Cast<IOTRBoxer>(Pawn());
		Guard = Boxer && Boxer->OTR_IsLeaning() ? EOTRGuard::High : EOTRGuard::None;
	}
	C->BotGuard(Guard);
}

void UOTRAutoplayComponent::TickMove(float Dt)
{
	AOTRPlayerController* C = PC();
	APawn* P = Pawn();
	const IOTRMatchInfo* Match = MatchOf(this);
	const IOTRPilotable* Pilot = Cast<IOTRPilotable>(P);
	if (!C || !P || !Match || !Pilot)
	{
		return;
	}
	static const bool bRingOutTest = FParse::Param(FCommandLine::Get(), TEXT("OTRRingOutTest"));
	if (bRingOutTest)
	{
		return; // QA: stand still with the guard down beside the open side
	}
	if (bThrowMatch)
	{
		const IOTRRingInfo* Ring = Cast<IOTRRingInfo>(Match->OTR_Ring());
		if (Ring)
		{
			const FVector To = Ring->OTR_Centre() - P->GetActorLocation();
			if (To.Size2D() > 80.0f)
			{
				C->BotMove(To.GetSafeNormal2D(), 1.0f);
			}
		}
		return;
	}
	RetargetClock -= Dt;
	if (RetargetClock <= 0.0f || (TargetSide == INDEX_NONE && TargetCorner == INDEX_NONE))
	{
		ChooseTarget();
	}
	// Arrived (or against a usable rope with the plan): lean.
	const float Dist = FVector::Dist2D(P->GetActorLocation(), TargetPoint);
	if (Pilot->OTR_CanLeanHere() && Dist < (TargetCorner == INDEX_NONE ? 160.0f : 220.0f))
	{
		if (TargetCorner != INDEX_NONE)
		{
			bCornerUsed = true;
		}
		if (!bLeanHeld)
		{
			LeanWaitClock = 0.0f;
		}
		bLeanHeld = true;
		C->BotLean(true);
		return;
	}
	// Stuck against the champion or a post: sidestep for a moment.
	StuckClock += Dt;
	if (StuckClock >= 0.6f)
	{
		if (FVector::Dist2D(StuckFrom, P->GetActorLocation()) < 25.0f && Pilot->OTR_CanMove())
		{
			SidestepClock = 0.45f;
			const FVector To = (TargetPoint - P->GetActorLocation()).GetSafeNormal2D();
			SidestepDir = FVector::CrossProduct(To, FVector::UpVector) * (FMath::FRand() < 0.5f ? 1.0f : -1.0f);
		}
		StuckClock = 0.0f;
		StuckFrom = P->GetActorLocation();
	}
	FVector Dir = (TargetPoint - P->GetActorLocation()).GetSafeNormal2D();
	// The champion is solid: walk around it when it stands between us and the target.
	if (const AActor* Champ = Match->OTR_Champion())
	{
		const FVector ToChamp = Champ->GetActorLocation() - P->GetActorLocation();
		const float Ahead = FVector::DotProduct(FVector(ToChamp.X, ToChamp.Y, 0.0f), Dir);
		const float Lateral = FVector::CrossProduct(Dir, FVector(ToChamp.X, ToChamp.Y, 0.0f)).Z;
		if (Ahead > 0.0f && Ahead < Dist && FMath::Abs(Lateral) < AvoidRadius)
		{
			const FVector Perp = FVector::CrossProduct(FVector::UpVector, Dir) * (Lateral >= 0.0f ? -1.0f : 1.0f);
			const float Push = 1.0f - FMath::Abs(Lateral) / AvoidRadius;
			Dir = (Dir + Perp * (0.6f + 1.4f * Push)).GetSafeNormal2D();
		}
	}
	if (SidestepClock > 0.0f)
	{
		SidestepClock -= Dt;
		Dir = (SidestepDir + Dir * 0.3f).GetSafeNormal2D();
	}
	C->BotMove(Dir, FMath::Clamp(Dist / 80.0f, 0.4f, 1.0f));
}

void UOTRAutoplayComponent::TickLean(float Dt)
{
	AOTRPlayerController* C = PC();
	const IOTRMatchInfo* Match = MatchOf(this);
	const IOTRBoxer* Boxer = Cast<IOTRBoxer>(Pawn());
	const IOTRChampion* Champ = Match ? Cast<IOTRChampion>(Match->OTR_Champion()) : nullptr;
	const IOTRRingInfo* Ring = Match ? Cast<IOTRRingInfo>(Match->OTR_Ring()) : nullptr;
	if (!C || !Boxer || !Champ || !Ring)
	{
		return;
	}
	if (!Boxer->OTR_IsLeaning())
	{
		LeanWaitClock += Dt;
		// The lean broke (snap or drag), or never started: let go of the key and pick a new rope.
		if (bLeanHeld && (bSawLean || LeanWaitClock > 0.6f))
		{
			bLeanHeld = false;
			bSawLean = false;
			C->BotLean(false);
			RetargetClock = 0.0f;
		}
		return;
	}
	bSawLean = true;
	LeanWaitClock = 0.0f;
	const UOTRTuning* T = Match->OTR_Tuning();
	const float Load = Boxer->OTR_StoredLoad();
	const bool bVent = Champ->OTR_IsVenting();
	const float VentLeft = Champ->OTR_VentTimeLeft();
	const float Need = T->Match.KnockdownTarget - Match->OTR_Progress();
	bool bRelease = false;
	if (bVent)
	{
		// Wait for more load while the window stays open, but never let it close on us.
		const bool bEnough = Load >= T->Rope.MaxLoad - 0.02f || Load >= Need - 0.02f;
		bRelease = (Load >= 0.5f && (bEnough || VentLeft < 1.4f)) || (Load >= T->Rope.MinLaunchLoad && VentLeft < 0.9f);
	}
	else
	{
		float MaxFray = 0.0f;
		for (int32 S : Boxer->OTR_LeanSides())
		{
			MaxFray = FMath::Max(MaxFray, Ring->OTR_Fray(S));
		}
		// Save a rope about to snap, unless the vent is close.
		if (MaxFray >= BailFray && Champ->OTR_Heat01() < 0.8f)
		{
			bRelease = true;
		}
		// Human-like impatience at low skill.
		if (Load >= 0.6f)
		{
			ImpatienceClock += Dt;
			if (ImpatienceClock >= 1.0f)
			{
				ImpatienceClock = 0.0f;
				bRelease |= FMath::FRand() < 0.12f * (1.0f - Skill);
			}
		}
	}
	if (bRelease)
	{
		bLeanHeld = false;
		bSawLean = false;
		ImpatienceClock = 0.0f;
		C->BotLean(false);
		RetargetClock = 0.0f;
	}
}

void UOTRAutoplayComponent::TickStrikes(float Dt, bool bPressure)
{
	AOTRPlayerController* C = PC();
	APawn* P = Pawn();
	const IOTRMatchInfo* Match = MatchOf(this);
	const IOTRChampion* Champ = Match ? Cast<IOTRChampion>(Match->OTR_Champion()) : nullptr;
	const IOTRPilotable* Pilot = Cast<IOTRPilotable>(P);
	if (!C || !P || !Champ || !Pilot || bThrowMatch)
	{
		return;
	}
	const float Dist = FVector::Dist2D(P->GetActorLocation(), Match->OTR_Champion()->GetActorLocation());
	if (Dist > Match->OTR_Tuning()->Player.JabReach - 20.0f)
	{
		return;
	}
	// A raised guard: smash it open.
	if (Champ->OTR_IsGuarding() && Pilot->OTR_BreakCooldown() <= 0.0f && FMath::FRand() < Dt * 4.0f * FMath::Max(Skill, 0.3f))
	{
		C->BotBreak();
		return;
	}
	float ToHit = 0.0f;
	const bool bIncoming = Champ->OTR_Telegraph(ToHit) != EOTRPunch::None;
	if (!bIncoming && !Champ->OTR_IsGuarding() && !Champ->OTR_IsVenting())
	{
		JabClock += Dt;
		if (JabClock >= 1.0f / FMath::Max(bPressure ? PressureJabRate : FreeJabRate, 0.05f))
		{
			JabClock = FMath::FRandRange(-0.3f, 0.2f);
			C->BotJab();
		}
	}
}

void UOTRAutoplayComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const IOTRMatchInfo* Match = MatchOf(this);
	AOTRPlayerController* C = PC();
	if (!Match || !C)
	{
		return;
	}
	if (Match->OTR_State() != EOTRMatchState::Active)
	{
		if (bLeanHeld)
		{
			bLeanHeld = false;
			C->BotLean(false);
		}
		return;
	}
	// Finishing blow: let the prompt read for a beat, then deal it (retry until it lands).
	if (Match->OTR_IsFinisher())
	{
		if (bLeanHeld)
		{
			bLeanHeld = false;
			C->BotLean(false);
		}
		FinishClock += DeltaTime;
		if (FinishClock >= FinishDelay)
		{
			FinishClock = FinishDelay - 0.6f;
			C->BotBreak();
		}
		return;
	}
	TickGuard(DeltaTime);
	const IOTRBoxer* Boxer = Cast<IOTRBoxer>(Pawn());
	const bool bLaunching = Boxer && Boxer->OTR_IsLaunching();
	if (bWasLaunching && !bLaunching && !bThrowMatch)
	{
		// Just landed a spring: work the champion over up close for a moment.
		PressureClock = PressureSeconds * FMath::Lerp(0.5f, 1.0f, Skill);
		JabClock = 1.0f;
	}
	bWasLaunching = bLaunching;
	if (Boxer && (Boxer->OTR_IsLeaning() || bLaunching))
	{
		TickLean(DeltaTime);
		return;
	}
	if (PressureClock > 0.0f)
	{
		PressureClock -= DeltaTime;
		TickStrikes(DeltaTime, true);
		if (APawn* P = Pawn())
		{
			if (const AActor* ChampActor = Match->OTR_Champion())
			{
				const FVector To = ChampActor->GetActorLocation() - P->GetActorLocation();
				if (To.Size2D() > 230.0f)
				{
					C->BotMove(To.GetSafeNormal2D(), 0.8f);
				}
			}
		}
		if (bLeanHeld)
		{
			bLeanHeld = false;
			C->BotLean(false);
		}
		return;
	}
	TickStrikes(DeltaTime, false);
	if (bLeanHeld)
	{
		TickLean(DeltaTime);
		if (bLeanHeld)
		{
			// Holding lean while not yet on the rope: keep walking onto it.
			TickMove(DeltaTime);
		}
		return;
	}
	TickMove(DeltaTime);
}
