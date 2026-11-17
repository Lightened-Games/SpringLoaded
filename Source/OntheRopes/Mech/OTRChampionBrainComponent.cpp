// Copyright On the Ropes. All Rights Reserved.

#include "Mech/OTRChampionBrainComponent.h"
#include "Mech/OTRMechRigComponent.h"
#include "Components/OTRHeatComponent.h"
#include "Interfaces/OTRInterfaces.h"
#include "Data/OTRTuning.h"
#include "Framework/OTREventSubsystem.h"
#include "Feedback/OTRFeedback.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "OntheRopes.h"

#define LOCTEXT_NAMESPACE "OntheRopes"

namespace
{
	FOTRPunchRow DefaultRow(EOTRPunch Type)
	{
		FOTRPunchRow R;
		R.Type = Type;
		switch (Type)
		{
		case EOTRPunch::Body:
			R.TagText = NSLOCTEXT("OTR", "TagLow", "LOW");
			R.bHigh = false;
			R.Damage = 14.0f;
			R.Heat = 16.0f;
			R.FollowUp = EOTRPunch::Hook;
			R.FollowChance = 0.3f;
			R.FeintChance = 0.12f;
			break;
		case EOTRPunch::Haymaker:
			R.TagText = NSLOCTEXT("OTR", "TagDodge", "DODGE");
			R.bUnblockable = true;
			R.Telegraph = 0.85f;
			R.StrikeTime = 0.16f;
			R.Recover = 0.5f;
			R.Damage = 24.0f;
			R.LoadGain = 0.35f;
			R.FrayGain = 0.14f;
			R.Heat = 20.0f;
			R.Knockback = 130.0f;
			R.Shake = 1.6f;
			break;
		case EOTRPunch::Jab:
			R.TagText = NSLOCTEXT("OTR", "TagJab", "JAB");
			R.Telegraph = 0.28f;
			R.StrikeTime = 0.09f;
			R.Recover = 0.2f;
			R.Damage = 6.0f;
			R.LoadGain = 0.07f;
			R.FrayGain = 0.04f;
			R.Heat = 6.0f;
			R.Knockback = 0.0f;
			R.Drag = 70.0f;
			R.LoadBleed = 0.3f;
			R.Shake = 0.5f;
			break;
		default:
			R.TagText = NSLOCTEXT("OTR", "TagHigh", "HIGH");
			R.FollowUp = EOTRPunch::Body;
			R.FollowChance = 0.25f;
			R.FeintChance = 0.15f;
			break;
		}
		return R;
	}
}

UOTRChampionBrainComponent::UOTRChampionBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UOTRChampionBrainComponent::Begin(ACharacter* InBody, UOTRMechRigComponent* InRig, UOTRHeatComponent* InHeat)
{
	Body = InBody;
	Rig = InRig;
	Heat = InHeat;
	for (EOTRPunch Type : { EOTRPunch::Hook, EOTRPunch::Body, EOTRPunch::Jab, EOTRPunch::Haymaker })
	{
		Rows.Add(Type, DefaultRow(Type));
	}
	if (UDataTable* Table = OTR::Tuning(this)->PunchTable.LoadSynchronous())
	{
		for (const FName& Name : Table->GetRowNames())
		{
			if (const FOTRPunchRow* Row = Table->FindRow<FOTRPunchRow>(Name, TEXT("ChampionBrain")))
			{
				Rows.Add(Row->Type, *Row);
			}
		}
	}
	else
	{
		UE_LOG(LogOntheRopes, Warning, TEXT("[OTR] DT_OTR_Punches missing: using built-in punch defaults"));
	}
	if (Heat)
	{
		Heat->OnOverheat.AddDynamic(this, &UOTRChampionBrainComponent::HandleOverheat);
		Heat->OnVentEnd.AddDynamic(this, &UOTRChampionBrainComponent::HandleVentEnd);
	}
	Punch = Rows[EOTRPunch::Hook];
}

EOTRMatchState UOTRChampionBrainComponent::MatchState() const
{
	const IOTRMatchInfo* Match = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	return Match ? Match->OTR_State() : EOTRMatchState::Ready;
}

AActor* UOTRChampionBrainComponent::Player() const
{
	const UWorld* World = GetWorld();
	const APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	return PC ? PC->GetPawn() : nullptr;
}

void UOTRChampionBrainComponent::SetState(EOTRChampState NewState)
{
	State = NewState;
	StateClock = 0.0f;
}

EOTRPunch UOTRChampionBrainComponent::GetTelegraph(float& OutSecondsToHit) const
{
	OutSecondsToHit = 0.0f;
	if (State == EOTRChampState::Windup)
	{
		OutSecondsToHit = FMath::Max(0.0f, Punch.Telegraph - StateClock) + Punch.StrikeTime;
		return Punch.Type;
	}
	if (State == EOTRChampState::Strike)
	{
		OutSecondsToHit = FMath::Max(0.0f, Punch.StrikeTime - StateClock);
		return Punch.Type;
	}
	return EOTRPunch::None;
}

FVector UOTRChampionBrainComponent::GetFistLocation() const
{
	return Rig ? Rig->GetHandWorld(bPunchRight) : (Body ? Body->GetActorLocation() : FVector::ZeroVector);
}

void UOTRChampionBrainComponent::HandleOverheat()
{
	if (State == EOTRChampState::KO)
	{
		return;
	}
	SetState(EOTRChampState::Venting);
	SteamClock = 0.0f;
	OTRFeedback::Sound(this, TEXT("Alarm"), Body->GetActorLocation(), 0.8f);
	OTRFeedback::Sound(this, TEXT("Steam"), Body->GetActorLocation(), 1.0f);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		++Events->Vents;
		Events->OnOverheat.Broadcast(Body->GetActorLocation());
		Events->PromptOnce(TEXT("Vent"), LOCTEXT("PromptVent", "IT'S VENTING  RELEASE TO LAUNCH"), Body, FVector(0.0f, 0.0f, 265.0f));
	}
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Champion overheated at %.1fs"), ActiveClock);
}

void UOTRChampionBrainComponent::HandleVentEnd()
{
	if (State == EOTRChampState::Venting)
	{
		SetState(EOTRChampState::Recover);
		NextPunchAt = ActiveClock + 0.5f;
	}
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnVentEnd.Broadcast(Body->GetActorLocation());
	}
}

void UOTRChampionBrainComponent::UpdateJabMode()
{
	const IOTRBoxer* Boxer = Cast<IOTRBoxer>(Player());
	if (!Boxer)
	{
		return;
	}
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	const float Load = Boxer->OTR_StoredLoad();
	const bool bWas = bJabMode;
	if (!bJabMode && Boxer->OTR_IsLeaning() && Load >= T.JabModeLoad)
	{
		bJabMode = true;
	}
	else if (bJabMode && (!Boxer->OTR_IsLeaning() || Load < T.JabModeLoad * JabModeExit))
	{
		bJabMode = false;
	}
	if (bJabMode != bWas)
	{
		if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
		{
			Events->OnJabMode.Broadcast(bJabMode);
			if (bJabMode)
			{
				Events->PromptOnce(TEXT("Jabs"), LOCTEXT("PromptJabs", "JABS  GUARD HIGH OR GET DRAGGED"), Body, FVector(0.0f, 0.0f, 265.0f), true);
			}
		}
		if (bJabMode)
		{
			OTRFeedback::Sound(this, TEXT("Chatter"), Body->GetActorLocation(), 0.7f);
		}
	}
}

int32 UOTRChampionBrainComponent::GetPhase() const
{
	const IOTRMatchInfo* Match = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	return Match ? FMath::Clamp(FMath::FloorToInt(Match->OTR_Progress() + 0.001f), 0, 2) : 0;
}

float UOTRChampionBrainComponent::PhaseValue(const TArray<float>& Values, float Fallback) const
{
	return Values.IsValidIndex(GetPhase()) ? Values[GetPhase()] : (Values.Num() > 0 ? Values.Last() : Fallback);
}

void UOTRChampionBrainComponent::StaggerFor(float Seconds)
{
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	SetState(EOTRChampState::Stagger);
	StateClock = FMath::Max(0.0f, T.StaggerSeconds - Seconds);
	bComboPending = false;
	bFeint = false;
}

void UOTRChampionBrainComponent::StartGuard()
{
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	SetState(EOTRChampState::Guarding);
	NextGuardAt = ActiveClock + T.GuardSeconds + T.GuardCooldown;
	JabHitTimes.Reset();
	OTRFeedback::Sound(this, TEXT("Chatter"), Body->GetActorLocation(), 0.6f, 0.8f);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->PromptOnce(TEXT("GuardUp"), LOCTEXT("PromptGuardUp", "GUARD UP  Q TO BREAK IT"), Body, FVector(0.0f, 0.0f, 265.0f), true);
	}
}

EOTRStrikeResult UOTRChampionBrainComponent::ReceiveStrike(EOTRPlayerStrike Strike, AActor* From)
{
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	UOTREventSubsystem* Events = UOTREventSubsystem::Get(this);
	const FVector Pop = Body->GetActorLocation() + FVector(0.0f, 0.0f, 190.0f);
	const bool bGuardUp = State == EOTRChampState::Guarding || State == EOTRChampState::Block;
	auto Flinch = [this](float Scale)
	{
		Rig->AddJolt(FVector(-380.0f * Scale, FMath::FRandRange(-80.0f, 80.0f), 0.0f));
		Rig->AddTwist(FMath::FRandRange(-120.0f, 120.0f) * Scale, -120.0f * Scale, 0.0f, -200.0f * Scale);
	};
	switch (Strike)
	{
	case EOTRPlayerStrike::Jab:
		if (bGuardUp)
		{
			// Counter quickly off a blocked jab.
			NextPunchAt = FMath::Min(NextPunchAt, ActiveClock + 0.25f);
			Rig->AddJolt(FVector(-120.0f, 0.0f, 0.0f));
			return EOTRStrikeResult::Blocked;
		}
		if (State == EOTRChampState::Windup && Punch.Type != EOTRPunch::Haymaker)
		{
			StaggerFor(T.JabInterruptStagger);
			Flinch(1.0f);
			if (Heat) { Heat->AddHeat(T.JabHeat); }
			if (Events)
			{
				++Events->Interrupts;
				Events->OnPopup.Broadcast(LOCTEXT("Interrupted", "INTERRUPTED"), Pop, FLinearColor(1.0f, 0.82f, 0.4f));
			}
			return EOTRStrikeResult::Interrupted;
		}
		if (Heat && State != EOTRChampState::Venting) { Heat->AddHeat(T.JabHeat); }
		Flinch(0.5f);
		JabHitTimes.Add(ActiveClock);
		JabHitTimes.RemoveAll([this, &T](float At) { return ActiveClock - At > T.JabMemorySeconds; });
		if (JabHitTimes.Num() >= T.GuardAfterJabs && ActiveClock >= NextGuardAt && (State == EOTRChampState::Approach || State == EOTRChampState::Recover))
		{
			StartGuard();
		}
		return EOTRStrikeResult::Hit;
	case EOTRPlayerStrike::Break:
		if (bGuardUp)
		{
			StaggerFor(T.GuardBreakStagger);
			Flinch(1.6f);
			if (Heat) { Heat->AddHeat(T.BreakHeat); }
			OTRFeedback::Sound(this, TEXT("Crunch"), Pop, 1.0f, 0.8f);
			if (Events)
			{
				++Events->GuardBreaks;
				Events->OnPopup.Broadcast(LOCTEXT("GuardBroken", "GUARD BROKEN"), Pop, FLinearColor(1.0f, 0.82f, 0.4f));
			}
			return EOTRStrikeResult::GuardBroken;
		}
		if (Heat && State != EOTRChampState::Venting) { Heat->AddHeat(T.BreakHeat * 0.5f); }
		if (State == EOTRChampState::Windup && Punch.Type != EOTRPunch::Haymaker)
		{
			StaggerFor(T.JabInterruptStagger * 1.5f);
			Flinch(1.3f);
			if (Events) { ++Events->Interrupts; }
			return EOTRStrikeResult::Interrupted;
		}
		Flinch(1.0f);
		if (From) { Shove(Body->GetActorLocation() - From->GetActorLocation(), 60.0f); }
		return EOTRStrikeResult::Hit;
	case EOTRPlayerStrike::Parry:
		StaggerFor(T.ParryStagger);
		Flinch(1.2f);
		if (Heat) { Heat->AddHeat(T.ParryHeat); }
		return EOTRStrikeResult::Hit;
	case EOTRPlayerStrike::Duck:
		if (Heat) { Heat->AddHeat(T.DuckOverextendHeat); }
		StaggerFor(T.OverextendSeconds * 0.8f);
		Rig->AddJolt(FVector(450.0f, 0.0f, -60.0f));
		Rig->AddTwist(160.0f, 120.0f, 0.0f);
		return EOTRStrikeResult::Hit;
	default:
		return EOTRStrikeResult::Missed;
	}
}

void UOTRChampionBrainComponent::ChoosePunch()
{
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	const IOTRBoxer* Boxer = Cast<IOTRBoxer>(Player());
	// Sometimes it covers up instead of punching while the player is off the ropes (makes the jab a decision).
	if (Boxer && !Boxer->OTR_IsLeaning() && !bJabMode && ActiveClock >= NextGuardAt && FMath::FRand() < T.GuardChance)
	{
		StartGuard();
		return;
	}
	EOTRPunch Type = EOTRPunch::Hook;
	if (!bJabMode && Heat && Heat->GetHeat01() >= T.HaymakerHeat && ActiveClock >= NextHaymakerAt && FMath::FRand() < T.HaymakerChance)
	{
		Type = EOTRPunch::Haymaker;
		NextHaymakerAt = ActiveClock + T.HaymakerCooldown;
	}
	else if (bJabMode && FMath::FRand() < T.JabShare)
	{
		Type = EOTRPunch::Jab;
	}
	else
	{
		// Mostly random, but never the same power punch three times in a row (keeps the guard decision live).
		Type = FMath::FRand() < T.HookShare ? EOTRPunch::Hook : EOTRPunch::Body;
		if (PowerCount >= 2 && Punch.Type == Type)
		{
			Type = Type == EOTRPunch::Hook ? EOTRPunch::Body : EOTRPunch::Hook;
		}
		PowerCount = Punch.Type == Type ? PowerCount + 1 : 1;
	}
	StartPunch(Type, 1.0f);
}

void UOTRChampionBrainComponent::StartPunch(EOTRPunch Type, float TelegraphScale)
{
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	Punch = Rows.Contains(Type) ? Rows[Type] : DefaultRow(Type);
	Punch.Telegraph *= TelegraphScale;
	bPunchRight = Type != EOTRPunch::Jab;
	bFeint = TelegraphScale >= 1.0f && Type != EOTRPunch::Jab && Type != EOTRPunch::Haymaker
		&& FMath::FRand() < Punch.FeintChance + PhaseValue(T.PhaseFeintBonus, 0.0f);
	bComboPending = Punch.FollowUp != EOTRPunch::None && FMath::FRand() < Punch.FollowChance + PhaseValue(T.PhaseFollowBonus, 0.0f);
	SetState(EOTRChampState::Windup);
	OTRFeedback::Sound(this, TEXT("Windup"), Body->GetActorLocation(), Type == EOTRPunch::Jab ? 0.45f : 0.75f, Type == EOTRPunch::Jab ? 1.35f : 1.0f);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnTelegraph.Broadcast(Type, GetFistLocation());
		if (Type == EOTRPunch::Haymaker)
		{
			Events->PromptOnce(TEXT("Dodge"), LOCTEXT("PromptDodge", "HAYMAKER  SHIFT TO DODGE"), Body, FVector(0.0f, 0.0f, 265.0f), true);
			OTRFeedback::Sound(this, TEXT("Alarm"), Body->GetActorLocation(), 0.5f, 0.7f);
		}
		if (Type == EOTRPunch::Body)
		{
			Events->PromptOnce(TEXT("GuardLow"), LOCTEXT("PromptLow", "LOW  HOLD RMB"), Body, FVector(0.0f, 0.0f, 265.0f), true);
		}
		else if (Type == EOTRPunch::Hook)
		{
			Events->PromptOnce(TEXT("GuardHigh"), LOCTEXT("PromptHigh", "HIGH  HOLD LMB"), Body, FVector(0.0f, 0.0f, 265.0f), true);
		}
	}
}

void UOTRChampionBrainComponent::Resolve()
{
	AActor* P = Player();
	IOTRBoxer* Boxer = Cast<IOTRBoxer>(P);
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	if (Heat)
	{
		Heat->AddHeat(Punch.Heat);
	}
	if (!P || !Boxer)
	{
		return;
	}
	const float Dist = FVector::Dist2D(P->GetActorLocation(), Body->GetActorLocation());
	if (Dist <= T.PunchReach && Boxer->OTR_IsDodging())
	{
		// Dodged at the last moment: the swing over-extends, the arms heat and the champion stumbles.
		if (Heat)
		{
			Heat->AddHeat(T.OverextendHeat);
		}
		OTRFeedback::Sound(this, TEXT("Whiff"), GetFistLocation(), 0.9f, 0.85f);
		if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
		{
			++Events->Dodges;
			Events->OnPopup.Broadcast(NSLOCTEXT("OTR", "Dodged", "DODGED"), P->GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), FLinearColor::White);
		}
		if (State == EOTRChampState::Strike)
		{
			SetState(EOTRChampState::Stagger);
			StateClock = FMath::Max(0.0f, T.StaggerSeconds - T.OverextendSeconds);
			Rig->AddJolt(FVector(500.0f, 0.0f, -60.0f));
		}
	}
	else if (Dist <= T.PunchReach && !Boxer->OTR_IsLaunching())
	{
		Boxer->OTR_ReceivePunch(Punch, Body);
	}
	else
	{
		OTRFeedback::Sound(this, TEXT("Whiff"), GetFistLocation(), 0.5f);
	}
}

void UOTRChampionBrainComponent::Shove(const FVector& Direction, float Distance)
{
	UCharacterMovementComponent* Move = Body ? Body->GetCharacterMovement() : nullptr;
	if (!Move)
	{
		return;
	}
	// Keep the champion inside the ring: never pushed past the ropes.
	const IOTRMatchInfo* Match = Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode());
	const IOTRRingInfo* Ring = Match ? Cast<IOTRRingInfo>(Match->OTR_Ring()) : nullptr;
	float D = Distance;
	if (Ring)
	{
		const FVector Dir = Direction.GetSafeNormal2D();
		const float Radius = Body->GetCapsuleComponent()->GetScaledCapsuleRadius();
		for (int32 S = 0; S < Ring->OTR_SideCount(); ++S)
		{
			const float Toward = FVector::DotProduct(Dir, Ring->OTR_SideNormal(S));
			if (Toward > 0.05f)
			{
				const float Room = Ring->OTR_SideDistance(S, Body->GetActorLocation()) - Radius - 30.0f;
				D = FMath::Min(D, FMath::Max(0.0f, Room) / Toward);
			}
		}
	}
	Move->Velocity = Direction.GetSafeNormal2D() * FMath::Sqrt(2.0f * Move->BrakingDecelerationWalking * FMath::Max(D, 0.0f));
}

EOTRLaunchResult UOTRChampionBrainComponent::ReceiveLaunch(float Load, AActor* From)
{
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	const FVector Push = From ? (Body->GetActorLocation() - From->GetActorLocation()).GetSafeNormal2D() : -Body->GetActorForwardVector();
	UOTREventSubsystem* Events = UOTREventSubsystem::Get(this);
	const FVector At = Body->GetActorLocation() + FVector(0.0f, 0.0f, 60.0f) - Push * 60.0f;
	if (Heat && Heat->IsVenting())
	{
		// Clean: the vent window. The hit knocks it out of the lock-up and back across the ring.
		Heat->EndVent();
		SetState(EOTRChampState::Stagger);
		Shove(Push, T.StaggerDistance * (0.45f + 0.55f * Load));
		Rig->AddJolt(FVector(-1400.0f * (0.5f + Load), FMath::FRandRange(-300.0f, 300.0f), 140.0f));
		OTRFeedback::Sound(this, TEXT("CleanHit"), At, 1.0f, 1.0f - 0.15f * Load);
		OTRFeedback::Vfx(this, TEXT("Sparks"), At, (-Push).Rotation(), 1.6f);
		if (Events)
		{
			++Events->CleanHits;
			Events->OnCleanHit.Broadcast(Load, At);
			Events->OnImpact.Broadcast(At, 1.0f + Load, true);
		}
		UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Clean hit %.2f"), Load);
		return EOTRLaunchResult::Clean;
	}
	if (State == EOTRChampState::Guarding || FMath::FRand() < T.BlockChance)
	{
		SetState(EOTRChampState::Block);
		Shove(Push, 40.0f);
		Rig->AddJolt(FVector(-300.0f, 0.0f, 0.0f));
		OTRFeedback::Sound(this, TEXT("Block"), At, 0.9f);
		OTRFeedback::Vfx(this, TEXT("Sparks"), At, (-Push).Rotation(), 0.7f);
		if (Events)
		{
			Events->OnImpact.Broadcast(At, 0.6f, false);
		}
		return EOTRLaunchResult::Blocked;
	}
	SetState(EOTRChampState::Sidestep);
	const FVector Side = FVector::CrossProduct(Push, FVector::UpVector) * (FMath::FRand() < 0.5f ? 1.0f : -1.0f);
	Shove(Side, T.SidestepDistance);
	OTRFeedback::Sound(this, TEXT("Whiff"), At, 0.8f);
	return EOTRLaunchResult::Sidestepped;
}

void UOTRChampionBrainComponent::Face(float Dt)
{
	const AActor* P = Player();
	if (!P)
	{
		return;
	}
	const FVector To = (P->GetActorLocation() - Body->GetActorLocation()).GetSafeNormal2D();
	if (To.IsNearlyZero())
	{
		return;
	}
	const float Cur = Body->GetActorRotation().Yaw;
	const float Delta = FRotator::NormalizeAxis(To.Rotation().Yaw - Cur);
	Body->SetActorRotation(FRotator(0.0f, Cur + FMath::Clamp(Delta, -TurnRate * Dt, TurnRate * Dt), 0.0f));
}

void UOTRChampionBrainComponent::Steer(float Dt, bool bAllowMove)
{
	const AActor* P = Player();
	if (!P || !bAllowMove)
	{
		return;
	}
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	const FVector Delta = P->GetActorLocation() - Body->GetActorLocation();
	const float Dist = Delta.Size2D();
	const FVector Dir = Delta.GetSafeNormal2D();
	if (Dist > T.PreferredDistance + 15.0f)
	{
		Body->AddMovementInput(Dir, FMath::Clamp((Dist - T.PreferredDistance) / 120.0f, 0.35f, 1.0f));
	}
	else if (Dist < T.PreferredDistance - 35.0f)
	{
		Body->AddMovementInput(-Dir, 0.4f);
	}
}

void UOTRChampionBrainComponent::UpdatePose(float Dt)
{
	if (!Rig)
	{
		return;
	}
	FOTRMechPose Pose;
	float Sharp = 10.0f;
	Pose.HandL = FVector(70.0f, -38.0f, 248.0f);
	Pose.HandR = FVector(70.0f, 38.0f, 248.0f);
	Pose.TorsoPitch = 6.0f;
	Pose.Crouch = 8.0f;
	// Target points on the player, in rig space.
	FVector Head(200.0f, 0.0f, 240.0f);
	FVector Belly(200.0f, 0.0f, 150.0f);
	if (const AActor* P = Player())
	{
		const FTransform Frame(FRotator(0.0f, Body->GetActorRotation().Yaw, 0.0f), Body->GetActorLocation() - FVector(0.0f, 0.0f, Body->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
		const FVector Local = Frame.InverseTransformPosition(P->GetActorLocation());
		const float Reach = FMath::Max(80.0f, Local.X - 70.0f);
		Head = FVector(Reach, Local.Y * 0.5f, 238.0f);
		Belly = FVector(Reach + 5.0f, Local.Y * 0.5f, 150.0f);
	}
	const float WindA = FMath::Clamp(StateClock / FMath::Max(Punch.Telegraph, 0.05f), 0.0f, 1.0f);
	switch (State)
	{
	case EOTRChampState::Windup:
		Sharp = 9.0f;
		if (Punch.Type == EOTRPunch::Haymaker)
		{
			Pose.HandR = FVector(-40.0f, 70.0f, 345.0f);
			Pose.TorsoYaw = -32.0f * WindA;
			Pose.TorsoPitch = -10.0f * WindA;
			Pose.TorsoRoll = -10.0f;
			Pose.Crouch = 14.0f;
		}
		else if (Punch.Type == EOTRPunch::Hook)
		{
			Pose.HandR = FVector(-15.0f, 112.0f, 292.0f);
			Pose.TorsoYaw = -24.0f * WindA;
			Pose.TorsoRoll = -6.0f;
		}
		else if (Punch.Type == EOTRPunch::Body)
		{
			Pose.HandR = FVector(-20.0f, 96.0f, 168.0f);
			Pose.TorsoYaw = -20.0f * WindA;
			Pose.TorsoPitch = 14.0f;
			Pose.Crouch = 22.0f;
		}
		else
		{
			Pose.HandL = FVector(34.0f, -52.0f, 258.0f);
			Pose.TorsoYaw = 10.0f * WindA;
		}
		break;
	case EOTRChampState::Strike:
		Sharp = 42.0f;
		if (Punch.Type == EOTRPunch::Hook || Punch.Type == EOTRPunch::Haymaker)
		{
			Pose.HandR = Head + FVector(0.0f, 14.0f, Punch.Type == EOTRPunch::Haymaker ? 20.0f : 0.0f);
			Pose.TorsoYaw = 22.0f;
			Pose.TorsoPitch = 10.0f;
		}
		else if (Punch.Type == EOTRPunch::Body)
		{
			Pose.HandR = Belly;
			Pose.TorsoYaw = 18.0f;
			Pose.TorsoPitch = 18.0f;
			Pose.Crouch = 22.0f;
		}
		else
		{
			Pose.HandL = Head + FVector(0.0f, -10.0f, 0.0f);
			Pose.TorsoYaw = -12.0f;
		}
		break;
	case EOTRChampState::Venting:
		Sharp = 5.0f;
		Pose.HandL = FVector(30.0f, -98.0f, 128.0f);
		Pose.HandR = FVector(30.0f, 98.0f, 128.0f);
		Pose.TorsoPitch = 18.0f;
		Pose.HeadPitch = 26.0f;
		Pose.Crouch = 18.0f;
		break;
	case EOTRChampState::Guarding:
	case EOTRChampState::Block:
		Sharp = 30.0f;
		Pose.HandL = FVector(62.0f, -24.0f, 286.0f);
		Pose.HandR = FVector(62.0f, 24.0f, 286.0f);
		Pose.TorsoPitch = 10.0f;
		break;
	case EOTRChampState::Stagger:
		Sharp = 12.0f;
		Pose.HandL = FVector(10.0f, -104.0f, 226.0f);
		Pose.HandR = FVector(10.0f, 104.0f, 226.0f);
		Pose.TorsoPitch = -20.0f;
		Pose.HeadPitch = -16.0f;
		break;
	case EOTRChampState::KO:
		Sharp = 4.0f;
		Pose.HandL = FVector(20.0f, -110.0f, 200.0f);
		Pose.HandR = FVector(20.0f, 110.0f, 200.0f);
		Pose.HeadPitch = -20.0f;
		break;
	default:
		if (bJabMode)
		{
			// Tighter, faster stance in jab mode.
			Pose.HandL = FVector(84.0f, -30.0f, 252.0f);
			Pose.HandR = FVector(56.0f, 42.0f, 236.0f);
			Pose.Crouch = 14.0f;
		}
		break;
	}
	Rig->SetPose(Pose, Sharp);
}

void UOTRChampionBrainComponent::UpdateGlow(float Dt)
{
	UMaterialInstanceDynamic* MID = Rig ? Rig->GetGlowMID() : nullptr;
	if (!MID || !Heat)
	{
		return;
	}
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	const float H = Heat->GetHeat01();
	// Dark at rest, orange as it heats, hottest and flickering while it vents.
	float Want = T.HeatGlowMax * FMath::Square(FMath::Clamp((H - 0.25f) / 0.75f, 0.0f, 1.0f));
	if (Heat->IsVenting())
	{
		Want = T.HeatGlowMax * (0.75f + 0.25f * FMath::Sin(GetWorld()->GetTimeSeconds() * 22.0f));
	}
	Glow = FMath::FInterpTo(Glow, Want, Dt, 6.0f);
	MID->SetScalarParameterValue(TEXT("Emissive"), FMath::Min(Glow, 3.5f));
}

void UOTRChampionBrainComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!Body || !Rig)
	{
		return;
	}
	const float Dt = DeltaTime;
	StateClock += Dt;
	UpdateGlow(Dt);
	const EOTRMatchState Match = MatchState();
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	if (Match == EOTRMatchState::Victory)
	{
		if (State != EOTRChampState::KO)
		{
			SetState(EOTRChampState::KO);
			KOClock = 0.0f;
			OTRFeedback::Sound(this, TEXT("KO"), Body->GetActorLocation(), 1.0f);
		}
		KOClock += Dt;
		Rig->SetCollapse(KOClock / FMath::Max(T.KOSeconds, 0.1f));
		if (KOClock > T.KOSeconds * 0.85f && KOClock - Dt <= T.KOSeconds * 0.85f)
		{
			OTRFeedback::Sound(this, TEXT("Crash"), Body->GetActorLocation(), 1.0f, 0.8f);
			OTRFeedback::Vfx(this, TEXT("Dust"), Body->GetActorLocation() - FVector(0.0f, 0.0f, 150.0f) - Body->GetActorForwardVector() * 200.0f, FRotator::ZeroRotator, 2.5f);
			if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
			{
				Events->OnImpact.Broadcast(Body->GetActorLocation(), 1.6f, true);
			}
		}
		UpdatePose(Dt);
		return;
	}
	// Dazed after the last launch hit: it reels in place until the finishing blow.
	if (const IOTRMatchInfo* MI = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr)
	{
		if (MI->OTR_IsFinisher())
		{
			if (State != EOTRChampState::Stagger)
			{
				StaggerFor(0.0f);
			}
			StateClock = 0.0f;
			UpdatePose(Dt);
			return;
		}
	}
	// Spacing (review 2): never close enough for the arms to tangle, except when the player launches into it.
	if (Match == EOTRMatchState::Active)
	{
		AActor* P = Player();
		const IOTRBoxer* Boxer = P ? Cast<IOTRBoxer>(P) : nullptr;
		if (P && !(Boxer && Boxer->OTR_IsLaunching()))
		{
			FVector Away = Body->GetActorLocation() - P->GetActorLocation();
			Away.Z = 0.0f;
			const float Gap = T.MinSeparation - Away.Size();
			if (Gap > 0.0f)
			{
				Body->AddActorWorldOffset(Away.GetSafeNormal() * FMath::Min(Gap, 900.0f * Dt), true);
			}
		}
	}
	if (Match != EOTRMatchState::Active)
	{
		if (Heat)
		{
			Heat->SetCooling(false);
		}
		if (Match == EOTRMatchState::Ready)
		{
			Face(Dt);
		}
		SetState(EOTRChampState::Idle);
		UpdatePose(Dt);
		return;
	}
	if (Heat)
	{
		Heat->SetCooling(true);
	}
	ActiveClock += Dt;
	UpdateJabMode();
	const IOTRBoxer* Boxer = Cast<IOTRBoxer>(Player());
	const float Dist = Player() ? FVector::Dist2D(Player()->GetActorLocation(), Body->GetActorLocation()) : 9999.0f;
	switch (State)
	{
	case EOTRChampState::Idle:
		if (ActiveClock >= T.EngageDelay)
		{
			SetState(EOTRChampState::Approach);
			NextPunchAt = ActiveClock + 0.4f;
		}
		break;
	case EOTRChampState::Approach:
	case EOTRChampState::Recover:
		Face(Dt);
		Steer(Dt, true);
		if (State == EOTRChampState::Recover && StateClock >= Punch.Recover)
		{
			SetState(EOTRChampState::Approach);
		}
		if (State == EOTRChampState::Approach && ActiveClock >= NextPunchAt && Dist <= T.PunchReach + 25.0f)
		{
			const bool bPinned = Boxer && Boxer->OTR_IsInCorner();
			NextPunchAt = ActiveClock + (bPinned ? T.PinnedInterval : T.PunchInterval) * (bJabMode ? 0.8f : 1.0f) * PhaseValue(T.PhaseIntervalScale, 1.0f);
			ChoosePunch();
		}
		break;
	case EOTRChampState::Windup:
		Face(Dt);
		if (bFeint && StateClock >= Punch.Telegraph * T.FeintAt)
		{
			// The feint: the big wind-up stops dead and a quick jab comes instead.
			bFeint = false;
			OTRFeedback::Sound(this, TEXT("Windup"), Body->GetActorLocation(), 0.4f, 1.5f);
			StartPunch(EOTRPunch::Jab, T.FeintJabTelegraphScale);
			bComboPending = false;
			break;
		}
		if (StateClock >= Punch.Telegraph)
		{
			SetState(EOTRChampState::Strike);
			OTRFeedback::Sound(this, TEXT("Swing"), GetFistLocation(), 0.6f, Punch.Type == EOTRPunch::Jab ? 1.3f : 0.9f);
		}
		break;
	case EOTRChampState::Strike:
		if (StateClock >= Punch.StrikeTime)
		{
			Resolve();
			if (State == EOTRChampState::Strike)
			{
				if (bComboPending && Rows.Contains(Punch.FollowUp) && Dist <= T.PunchReach + 40.0f)
				{
					StartPunch(Punch.FollowUp, T.ComboTelegraphScale);
					bComboPending = false;
				}
				else
				{
					SetState(EOTRChampState::Recover);
				}
			}
		}
		break;
	case EOTRChampState::Venting:
		SteamClock -= Dt;
		if (SteamClock <= 0.0f)
		{
			SteamClock = SteamInterval;
			for (int32 Side = 0; Side < 2; ++Side)
			{
				const FVector Vent = Rig->GetShoulderWorld(Side == 1) + FVector(0.0f, 0.0f, 25.0f);
				OTRFeedback::Vfx(this, TEXT("Steam"), Vent, FRotator(90.0f, 0.0f, 0.0f), 1.4f);
			}
		}
		break;
	case EOTRChampState::Stagger:
		if (StateClock >= T.StaggerSeconds)
		{
			SetState(EOTRChampState::Recover);
			NextPunchAt = ActiveClock + 0.6f;
		}
		break;
	case EOTRChampState::Block:
		if (StateClock >= T.BlockSeconds)
		{
			SetState(EOTRChampState::Recover);
			NextPunchAt = ActiveClock + 0.3f;
		}
		break;
	case EOTRChampState::Guarding:
		Face(Dt);
		Steer(Dt, true);
		if (StateClock >= T.GuardSeconds)
		{
			SetState(EOTRChampState::Approach);
			NextPunchAt = ActiveClock + 0.15f;
		}
		break;
	case EOTRChampState::Sidestep:
		Face(Dt);
		if (StateClock >= 0.5f)
		{
			SetState(EOTRChampState::Recover);
			NextPunchAt = ActiveClock + 0.3f;
		}
		break;
	default:
		break;
	}
	// A vent can start mid-recover; it always wins.
	if (Heat && Heat->IsVenting() && State != EOTRChampState::Venting && State != EOTRChampState::Stagger)
	{
		SetState(EOTRChampState::Venting);
	}
	UpdatePose(Dt);
}

#undef LOCTEXT_NAMESPACE
