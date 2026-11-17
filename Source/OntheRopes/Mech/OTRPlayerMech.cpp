// Copyright On the Ropes. All Rights Reserved.

#include "Mech/OTRPlayerMech.h"
#include "Mech/OTRMechRigComponent.h"
#include "Components/OTRArmorComponent.h"
#include "Components/OTRRopeLeanComponent.h"
#include "Components/OTRGameFeelComponent.h"
#include "Framework/OTREventSubsystem.h"
#include "Feedback/OTRFeedback.h"
#include "Data/OTRTuning.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/World.h"
#include "OntheRopes.h"

#define LOCTEXT_NAMESPACE "OntheRopes"

AOTRPlayerMech::AOTRPlayerMech(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const FOTRPlayerTuning& T = GetDefault<UOTRTuning>()->Player;
	GetCapsuleComponent()->InitCapsuleSize(T.CapsuleRadius, T.CapsuleHalfHeight);
	Tags.Add(OTR::PlayerTag);

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(GetCapsuleComponent());
	Boom->bUsePawnControlRotation = true;
	Boom->bInheritRoll = false;
	Boom->bDoCollisionTest = true;
	Boom->ProbeSize = 14.0f;
	Boom->bEnableCameraLag = true;
	Boom->CameraLagSpeed = 9.0f;
	Boom->TargetArmLength = 520.0f;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;

	Armor = CreateDefaultSubobject<UOTRArmorComponent>(TEXT("Armor"));
	Lean = CreateDefaultSubobject<UOTRRopeLeanComponent>(TEXT("Lean"));
	Feel = CreateDefaultSubobject<UOTRGameFeelComponent>(TEXT("Feel"));

	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = T.WalkSpeed;
	Move->MaxAcceleration = T.Acceleration;
	Move->BrakingDecelerationWalking = T.BrakingDeceleration;
	Move->bUseSeparateBrakingFriction = true;
	Move->BrakingFriction = 0.0f;
	FootstepVolume = 0.5f;
}

const FOTRMechLook& AOTRPlayerMech::ChooseLook() const
{
	return OTR::Tuning(this)->PlayerLook;
}

void AOTRPlayerMech::ApplyBodyTuning()
{
	const UOTRTuning* T = OTR::Tuning(this);
	const float OldHalf = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	GetCapsuleComponent()->SetCapsuleSize(T->Player.CapsuleRadius, T->Player.CapsuleHalfHeight);
	AddActorWorldOffset(FVector(0.0f, 0.0f, T->Player.CapsuleHalfHeight - OldHalf));
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = T->Player.WalkSpeed;
	Move->MaxAcceleration = T->Player.Acceleration;
	Move->BrakingDecelerationWalking = T->Player.BrakingDeceleration;
	Armor->Init(T->Player.Armor);
	Boom->TargetOffset = FVector(0.0f, 0.0f, T->Camera.PivotHeight);
	Boom->SocketOffset = FVector(0.0f, T->Camera.ShoulderOffset, 0.0f);
	Boom->TargetArmLength = T->Camera.ReadyArmLength;
	Camera->SetFieldOfView(T->Camera.FOV);
}

void AOTRPlayerMech::BeginPlay()
{
	Super::BeginPlay();
	Armor->OnDepleted.AddDynamic(this, &AOTRPlayerMech::HandleArmorDepleted);
	if (UMaterialInstanceDynamic* Glow = Rig->GetGlowMID())
	{
		Glow->SetVectorParameterValue(TEXT("Color"), ChooseLook().Glow);
		Glow->SetScalarParameterValue(TEXT("Emissive"), ChooseLook().GlowEmissive);
	}
}

EOTRMatchState AOTRPlayerMech::MatchState() const
{
	const IOTRMatchInfo* Match = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	return Match ? Match->OTR_State() : EOTRMatchState::Ready;
}

AActor* AOTRPlayerMech::ChampionActor() const
{
	const IOTRMatchInfo* Match = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	return Match ? Match->OTR_Champion() : nullptr;
}

float AOTRPlayerMech::OTR_StoredLoad() const { return Lean ? Lean->GetLoad() : 0.0f; }
bool AOTRPlayerMech::OTR_IsLeaning() const { return Lean && Lean->IsLeaning(); }
bool AOTRPlayerMech::OTR_IsInCorner() const { return Lean && Lean->IsLeaning() && Lean->IsCorner(); }
bool AOTRPlayerMech::OTR_IsLaunching() const { return Lean && Lean->IsLaunching(); }
float AOTRPlayerMech::OTR_Armor01() const { return Armor ? Armor->GetArmor01() : 0.0f; }
const TArray<int32>& AOTRPlayerMech::OTR_LeanSides() const { return Lean ? Lean->GetSides() : NoSides; }

bool AOTRPlayerMech::OTR_CanLeanHere() const
{
	return Lean && Lean->CanLeanHere();
}

bool AOTRPlayerMech::CanMove() const
{
	return MatchState() == EOTRMatchState::Active && Stun <= 0.0f && DodgeClock <= 0.0f && !OTR_IsLeaning() && !OTR_IsLaunching() && Action != 2;
}

void AOTRPlayerMech::SetGuard(EOTRGuard InGuard)
{
	if (InGuard != Guard && InGuard != EOTRGuard::None && GetWorld())
	{
		GuardRaisedAt = GetWorld()->GetTimeSeconds();
	}
	Guard = InGuard;
}

bool AOTRPlayerMech::CanAct() const
{
	return MatchState() == EOTRMatchState::Active && Stun <= 0.0f && DodgeClock <= 0.0f && Action == 0 && DuckClock <= 0.0f
		&& !OTR_IsLeaning() && !OTR_IsLaunching();
}

bool AOTRPlayerMech::OTR_Jab()
{
	if (!CanAct() || JabCool > 0.0f)
	{
		return false;
	}
	const FOTRPlayerTuning& T = OTR::Tuning(this)->Player;
	Action = 1;
	ActionClock = 0.0f;
	bActionResolved = false;
	JabCool = T.JabWindup + T.JabStrike + T.JabCooldown;
	bJabRight = !bJabRight;
	OTRFeedback::Sound(this, TEXT("Swing"), GetActorLocation(), 0.45f, 1.45f);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		++Events->Jabs;
	}
	return true;
}

static IOTRMatchInfo* OTRMatchOf(const UObject* Ctx)
{
	UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
	return World ? Cast<IOTRMatchInfo>(World->GetAuthGameMode()) : nullptr;
}

bool AOTRPlayerMech::OTR_Break()
{
	// The finishing blow (review 2): with the champion dazed, Q is always ready and lunges in for the knockout.
	const IOTRMatchInfo* Match = OTRMatchOf(this);
	const bool bFinisher = Match && Match->OTR_IsFinisher();
	if (!CanAct() || (BreakCool > 0.0f && !bFinisher))
	{
		return false;
	}
	const FOTRPlayerTuning& T = OTR::Tuning(this)->Player;
	Action = 2;
	ActionClock = 0.0f;
	bActionResolved = false;
	BreakCool = T.BreakCooldown;
	OTRFeedback::Sound(this, TEXT("Windup"), GetActorLocation(), 0.7f, 0.8f);
	return true;
}

bool AOTRPlayerMech::OTR_Duck()
{
	const FOTRPlayerTuning& T = OTR::Tuning(this)->Player;
	if (MatchState() != EOTRMatchState::Active || DuckCool > 0.0f || DuckClock > 0.0f || OTR_IsLaunching() || Stun > 0.0f || Action != 0)
	{
		return false;
	}
	// Ducking on the ropes keeps the lean (a slipped punch stores nothing, but nothing is lost either).
	DuckClock = T.DuckSeconds;
	DuckCool = T.DuckSeconds + T.DuckCooldown;
	Rig->AddJolt(FVector(0.0f, 0.0f, -260.0f));
	OTRFeedback::Sound(this, TEXT("Hydraulic"), GetActorLocation(), 0.7f, FMath::FRandRange(0.95f, 1.1f));
	return true;
}

void AOTRPlayerMech::ResolveStrike(EOTRPlayerStrike Kind)
{
	const FOTRPlayerTuning& T = OTR::Tuning(this)->Player;
	AActor* Champ = ChampionActor();
	IOTRChampion* C = Cast<IOTRChampion>(Champ);
	UOTREventSubsystem* Events = UOTREventSubsystem::Get(this);
	const FVector Fist = Rig->GetHandWorld(Kind == EOTRPlayerStrike::Jab ? bJabRight : true);
	IOTRMatchInfo* Match = OTRMatchOf(this);
	if (Champ && Kind == EOTRPlayerStrike::Break && Match && Match->OTR_IsFinisher())
	{
		const FVector At = Champ->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f);
		OTRFeedback::Sound(this, TEXT("GuardBreak"), At, 1.0f, 0.75f);
		OTRFeedback::Sound(this, TEXT("Clang"), At, 1.0f, 0.6f);
		OTRFeedback::Vfx(this, TEXT("Sparks"), At, (GetActorLocation() - Champ->GetActorLocation()).Rotation(), 2.0f);
		if (Events)
		{
			Events->OnImpact.Broadcast(At, 1.6f, true);
		}
		Match->OTR_FinishingBlow(At);
		return;
	}
	if (!C || FVector::Dist2D(Champ->GetActorLocation(), GetActorLocation()) > T.JabReach)
	{
		OTRFeedback::Sound(this, TEXT("Whiff"), Fist, 0.45f, 1.2f);
		return;
	}
	const EOTRStrikeResult R = C->OTR_ReceiveStrike(Kind, this);
	const FVector At = Champ->GetActorLocation() + FVector(0.0f, 0.0f, Kind == EOTRPlayerStrike::Jab ? 90.0f : 60.0f);
	const FRotator Facing = (GetActorLocation() - Champ->GetActorLocation()).Rotation();
	switch (R)
	{
	case EOTRStrikeResult::Blocked:
		OTRFeedback::Sound(this, TEXT("Block"), At, 0.7f, 1.2f);
		OTRFeedback::Vfx(this, TEXT("Sparks"), At, Facing, 0.4f);
		Rig->AddJolt(FVector(-260.0f, 0.0f, 0.0f));
		Rig->AddTwist(0.0f, -60.0f, 0.0f);
		break;
	case EOTRStrikeResult::Interrupted:
	case EOTRStrikeResult::GuardBroken:
		OTRFeedback::Sound(this, R == EOTRStrikeResult::GuardBroken ? FName(TEXT("GuardBreak")) : FName(TEXT("JabHit")), At, 1.0f, 0.9f);
		OTRFeedback::Sound(this, TEXT("Clang"), At, 0.6f, 0.85f);
		OTRFeedback::Vfx(this, TEXT("Sparks"), At, Facing, 1.1f);
		if (Events)
		{
			Events->OnImpact.Broadcast(At, 0.8f, true);
		}
		break;
	case EOTRStrikeResult::Hit:
		OTRFeedback::Sound(this, TEXT("JabHit"), At, Kind == EOTRPlayerStrike::Jab ? 0.75f : 1.0f, FMath::FRandRange(0.94f, 1.08f) * (Kind == EOTRPlayerStrike::Jab ? 1.0f : 0.8f));
		OTRFeedback::Vfx(this, TEXT("Sparks"), At, Facing, Kind == EOTRPlayerStrike::Jab ? 0.45f : 0.8f);
		if (Events)
		{
			Events->OnImpact.Broadcast(At, Kind == EOTRPlayerStrike::Jab ? 0.3f : 0.6f, false);
		}
		break;
	default:
		break;
	}
	if (Events && Kind == EOTRPlayerStrike::Jab && R != EOTRStrikeResult::Blocked && R != EOTRStrikeResult::Missed)
	{
		++Events->JabsLanded;
	}
}

void AOTRPlayerMech::TickAction(float Dt)
{
	JabCool = FMath::Max(0.0f, JabCool - Dt);
	BreakCool = FMath::Max(0.0f, BreakCool - Dt);
	DuckClock = FMath::Max(0.0f, DuckClock - Dt);
	DuckCool = FMath::Max(0.0f, DuckCool - Dt);
	if (Action == 0)
	{
		return;
	}
	const FOTRPlayerTuning& T = OTR::Tuning(this)->Player;
	const bool bJab = Action == 1;
	const float Wind = bJab ? T.JabWindup : T.BreakWindup;
	const float Strike = bJab ? T.JabStrike : T.BreakStrike;
	const float Recover = bJab ? T.JabRecover : T.BreakRecover;
	ActionClock += Dt;
	// Finishing blow: lunge in to punching range during the wind-up.
	if (!bJab && !bActionResolved)
	{
		const IOTRMatchInfo* Match = OTRMatchOf(this);
		AActor* Champ = ChampionActor();
		if (Match && Match->OTR_IsFinisher() && Champ)
		{
			FVector To = Champ->GetActorLocation() - GetActorLocation();
			To.Z = 0.0f;
			const float Gap = To.Size() - T.JabReach * 0.6f;
			if (Gap > 0.0f)
			{
				const float Step = FMath::Min(Gap, Gap * FMath::Clamp(Dt / FMath::Max(Wind + Strike - ActionClock, Dt), 0.0f, 1.0f));
				AddActorWorldOffset(To.GetSafeNormal() * Step, true);
			}
			SetActorRotation(FRotator(0.0f, To.Rotation().Yaw, 0.0f));
		}
	}
	if (!bActionResolved && ActionClock >= Wind + Strike)
	{
		bActionResolved = true;
		ResolveStrike(bJab ? EOTRPlayerStrike::Jab : EOTRPlayerStrike::Break);
	}
	// A hit taken mid-move, a dodge or the end of the bout cancels it.
	if (ActionClock >= Wind + Strike + Recover || Stun > 0.0f || MatchState() != EOTRMatchState::Active)
	{
		Action = 0;
	}
}

bool AOTRPlayerMech::OTR_Dodge(float Side)
{
	const FOTRPlayerTuning& T = OTR::Tuning(this)->Player;
	if (MatchState() != EOTRMatchState::Active || DodgeCool > 0.0f || OTR_IsLaunching() || Stun > 0.0f)
	{
		return false;
	}
	// Stepping off the ropes lets them go: the stored load is lost (the dodge-or-absorb decision).
	if (OTR_IsLeaning())
	{
		Lean->BreakLean(true);
	}
	// Re-press lean after a dodge (no instant re-lean, no accidental launch).
	Lean->SetLeanHeld(false);
	Action = 0;
	DodgeSide = Side < 0.0f ? -1.0f : 1.0f;
	DodgeDir = GetActorRightVector() * DodgeSide;
	DodgeClock = T.DodgeSeconds;
	DodgeInvuln = T.DodgeInvulnerableSeconds;
	DodgeCool = T.DodgeCooldown;
	Rig->AddJolt(FVector(0.0f, DodgeSide * 420.0f, -80.0f));
	OTRFeedback::Sound(this, TEXT("Whoosh"), GetActorLocation(), 0.75f, 1.25f);
	OTRFeedback::Sound(this, TEXT("Hydraulic"), GetActorLocation(), 0.6f, 1.15f);
	OTRFeedback::Vfx(this, TEXT("Dust"), GetActorLocation() - FVector(0.0f, 0.0f, GetCapsuleComponent()->GetScaledCapsuleHalfHeight()), FRotator::ZeroRotator, 0.9f);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnImpact.Broadcast(GetActorLocation(), 0.35f, false);
	}
	return true;
}

void AOTRPlayerMech::SetLeanHeld(bool bHeld)
{
	if (Lean)
	{
		Lean->SetLeanHeld(bHeld);
	}
}

void AOTRPlayerMech::MoveInput(const FVector& WorldDirection, float Scale)
{
	if (CanMove() && Scale > 0.01f)
	{
		AddMovementInput(FVector(WorldDirection.X, WorldDirection.Y, 0.0f).GetSafeNormal(), Scale);
	}
}

bool AOTRPlayerMech::OTR_ReceivePunch(const FOTRPunchRow& Punch, AActor* Source)
{
	if (MatchState() != EOTRMatchState::Active || OTR_IsLaunching())
	{
		return false;
	}
	const UOTRTuning* T = OTR::Tuning(this);
	if (DodgeInvuln > 0.0f)
	{
		return false;
	}
	UOTREventSubsystem* Ev = UOTREventSubsystem::Get(this);
	IOTRChampion* Champ = Cast<IOTRChampion>(Source);
	const FVector Hit = Rig->GetHandWorld(true) * 0.5f + Rig->GetHandWorld(false) * 0.5f;
	// Ducked under a high punch: it sails over, the swing over-extends.
	if (DuckClock > 0.0f && Punch.bHigh)
	{
		if (Champ)
		{
			Champ->OTR_ReceiveStrike(EOTRPlayerStrike::Duck, this);
		}
		OTRFeedback::Sound(this, TEXT("Whiff"), Hit + FVector(0.0f, 0.0f, 60.0f), 0.9f, 0.8f);
		if (Ev)
		{
			++Ev->Ducks;
			Ev->OnPopup.Broadcast(LOCTEXT("Slipped", "SLIPPED"), GetActorLocation() + FVector(0.0f, 0.0f, 150.0f), FLinearColor::White);
		}
		return false;
	}
	const bool bGuarded = !Punch.bUnblockable && DuckClock <= 0.0f && ((Guard == EOTRGuard::High && Punch.bHigh) || (Guard == EOTRGuard::Low && !Punch.bHigh));
	const bool bParry = bGuarded && GetWorld()->GetTimeSeconds() - GuardRaisedAt <= T->Player.ParryWindow;
	const float DamageScale = bParry ? 0.0f : (bGuarded ? T->Player.GuardedDamageScale : (DuckClock > 0.0f ? T->Player.DuckLowDamageScale : 1.0f));
	const float Damage = Punch.Damage * DamageScale;
	const bool bWasLeaning = OTR_IsLeaning();
	// Every hit absorbed on the ropes stores energy, guarded or not; a parry stores more.
	if (bWasLeaning)
	{
		FOTRPunchRow Stored = Punch;
		Stored.LoadGain *= bParry ? T->Player.ParryLoadMultiplier : 1.0f;
		Lean->OnAbsorb(Stored);
	}
	if (bParry)
	{
		if (Champ)
		{
			Champ->OTR_ReceiveStrike(EOTRPlayerStrike::Parry, this);
		}
		OTRFeedback::Sound(this, TEXT("Parry"), Hit, 1.0f, FMath::FRandRange(0.96f, 1.04f));
		OTRFeedback::Vfx(this, TEXT("Sparks"), Hit, GetActorForwardVector().Rotation(), 1.3f);
		if (Ev)
		{
			++Ev->Parries;
			Ev->OnPopup.Broadcast(LOCTEXT("Parry", "PARRY"), GetActorLocation() + FVector(0.0f, 0.0f, 170.0f), FLinearColor(1.0f, 0.82f, 0.4f));
			Ev->OnImpact.Broadcast(Hit, 0.9f, true);
		}
	}
	const FVector Away = Source ? (GetActorLocation() - Source->GetActorLocation()).GetSafeNormal2D() : -GetActorForwardVector();
	if (!bGuarded)
	{
		Stun = T->Player.StunSeconds;
		const IOTRMatchInfo* Match = Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode());
		const IOTRRingInfo* Ring = Match ? Cast<IOTRRingInfo>(Match->OTR_Ring()) : nullptr;
		if (Punch.Drag > 0.0f && Ring)
		{
			// Jabs pull you off the ropes toward the centre and bleed stored energy.
			const FVector ToCentre = (Ring->OTR_Centre() - GetActorLocation()).GetSafeNormal2D();
			GetCharacterMovement()->Velocity = ToCentre * FMath::Sqrt(2.0f * T->Player.BrakingDeceleration * Punch.Drag);
			Lean->Bleed(Punch.LoadBleed);
		}
		else if (!bWasLeaning && Punch.Knockback > 0.0f)
		{
			GetCharacterMovement()->Velocity = Away * FMath::Sqrt(2.0f * T->Player.BrakingDeceleration * Punch.Knockback);
		}
		Rig->AddJolt(FVector(-520.0f, FMath::FRandRange(-120.0f, 120.0f), -60.0f));
		// Directional flinch: a hook twists the chest and snaps the head, a body shot folds it forward.
		const float Side = FMath::FRand() < 0.5f ? -1.0f : 1.0f;
		if (Punch.bHigh)
		{
			Rig->AddTwist(Side * 260.0f, -140.0f, Side * 120.0f, -260.0f);
		}
		else
		{
			Rig->AddTwist(Side * 120.0f, 220.0f, 0.0f, 120.0f);
		}
		Action = 0;
	}
	else
	{
		Rig->AddJolt(FVector(bParry ? -80.0f : -200.0f, 0.0f, -30.0f));
		Rig->AddTwist(0.0f, bParry ? -40.0f : -90.0f, 0.0f, -60.0f);
	}
	Armor->ApplyDamage(Damage, Source);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		++Events->HitsTaken;
		Events->HitsGuarded += bGuarded ? 1 : 0;
		Events->LowestArmor = FMath::Min(Events->LowestArmor, Armor->GetArmor());
		Events->OnPlayerHit.Broadcast(Damage, bGuarded, Punch.Type, Hit);
	}
	if (!bGuarded)
	{
		// Dutch-angle kick away from the side the punch came from.
		const float FromRight = FVector::DotProduct(Hit - GetActorLocation(), GetActorRightVector()) >= 0.0f ? 1.0f : -1.0f;
		CamRollVel += -FromRight * OTR::Tuning(this)->Camera.HitRollKick * (Punch.Type == EOTRPunch::Jab ? 0.5f : 1.0f);
	}
	OTRFeedback::Sound(this, bGuarded ? FName(TEXT("Clang")) : FName(TEXT("Crunch")), Hit, bGuarded ? 0.8f : 1.0f, FMath::FRandRange(0.92f, 1.06f));
	OTRFeedback::Vfx(this, TEXT("Sparks"), Hit, Away.Rotation(), bGuarded ? 0.6f : 1.0f);
	return bGuarded;
}

void AOTRPlayerMech::HandleArmorDepleted(AActor* LastSource)
{
	if (Lean)
	{
		Lean->BreakLean(true);
	}
}

void AOTRPlayerMech::FaceChampion(float Dt)
{
	FVector Want = FVector::ZeroVector;
	if (OTR_IsLaunching())
	{
		Want = Lean->GetLaunchDirection();
	}
	else if (const AActor* Champ = ChampionActor())
	{
		Want = (Champ->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
	}
	if (Want.IsNearlyZero())
	{
		return;
	}
	const float Cur = GetActorRotation().Yaw;
	const float Delta = FRotator::NormalizeAxis(Want.Rotation().Yaw - Cur);
	const float Step = FMath::Clamp(Delta, -FaceTurnRate * Dt, FaceTurnRate * Dt);
	SetActorRotation(FRotator(0.0f, Cur + Step, 0.0f));
}

void AOTRPlayerMech::UpdatePose(float Dt)
{
	const EOTRMatchState State = MatchState();
	FOTRMechPose Pose;
	float Sharp = 14.0f;
	Pose.TorsoPitch = 4.0f;
	Pose.Crouch = 6.0f;
	switch (Guard)
	{
	case EOTRGuard::High:
		Pose.HandL = FVector(50.0f, -20.0f, 246.0f);
		Pose.HandR = FVector(50.0f, 20.0f, 246.0f);
		Pose.TorsoPitch = 6.0f;
		Pose.HeadPitch = 8.0f;
		Sharp = 22.0f;
		break;
	case EOTRGuard::Low:
		Pose.HandL = FVector(62.0f, -28.0f, 140.0f);
		Pose.HandR = FVector(62.0f, 28.0f, 140.0f);
		Pose.TorsoPitch = 14.0f;
		Pose.Crouch = 14.0f;
		Sharp = 22.0f;
		break;
	default:
		Pose.HandL = FVector(60.0f, -30.0f, 182.0f);
		Pose.HandR = FVector(60.0f, 30.0f, 182.0f);
		break;
	}
	if (OTR_IsLeaning())
	{
		// Weight back into the ropes; more stretch, more lean.
		const float L = Lean->GetLoad();
		Pose.PelvisOffset = FVector(-14.0f - 10.0f * L, 0.0f, 0.0f);
		Pose.TorsoPitch -= 14.0f + 6.0f * L;
		Pose.Crouch += 4.0f;
		Pose.HeadPitch -= 4.0f;
	}
	if (OTR_IsLaunching())
	{
		Pose.HandL = FVector(118.0f, -24.0f, 192.0f);
		Pose.HandR = FVector(118.0f, 24.0f, 192.0f);
		Pose.TorsoPitch = 22.0f;
		Pose.PelvisOffset = FVector(12.0f, 0.0f, 0.0f);
		Sharp = 30.0f;
	}
	if (DodgeClock > 0.0f)
	{
		Pose.TorsoRoll = -DodgeSide * 16.0f;
		Pose.TorsoPitch += 8.0f;
		Pose.Crouch += 18.0f;
		Sharp = 26.0f;
	}
	if (DuckClock > 0.0f)
	{
		// Bob under: knees deep, chest folded, gloves tight to the head.
		Pose.HandL = FVector(58.0f, -22.0f, 168.0f);
		Pose.HandR = FVector(58.0f, 22.0f, 168.0f);
		Pose.TorsoPitch += 26.0f;
		Pose.Crouch += 34.0f;
		Pose.HeadPitch = 6.0f;
		Sharp = 24.0f;
	}
	if (Action != 0)
	{
		const FOTRPlayerTuning& PT = OTR::Tuning(this)->Player;
		const bool bJab = Action == 1;
		const float Wind = bJab ? PT.JabWindup : PT.BreakWindup;
		const float Strike = bJab ? PT.JabStrike : PT.BreakStrike;
		const float Reach = 165.0f;
		if (bJab)
		{
			// Shoulder turns in on the wind-up, fist snaps out, rotates back.
			const float S = bJabRight ? 1.0f : -1.0f;
			FVector& Hand = bJabRight ? Pose.HandR : Pose.HandL;
			if (ActionClock < Wind)
			{
				Hand = FVector(40.0f, S * 34.0f, 214.0f);
				Pose.TorsoYaw = -S * 8.0f;
				Sharp = 30.0f;
			}
			else if (ActionClock < Wind + Strike + 0.06f)
			{
				Hand = FVector(Reach, S * 8.0f, 224.0f);
				Pose.TorsoYaw = S * 16.0f;
				Pose.TorsoPitch += 6.0f;
				Sharp = 46.0f;
			}
		}
		else
		{
			// Guard-break: both fists drop and coil, then a rising double smash.
			if (ActionClock < Wind)
			{
				Pose.HandL = FVector(10.0f, -46.0f, 132.0f);
				Pose.HandR = FVector(10.0f, 46.0f, 132.0f);
				Pose.TorsoPitch = 18.0f;
				Pose.Crouch += 22.0f;
				Pose.TorsoYaw = -10.0f;
				Sharp = 12.0f;
			}
			else if (ActionClock < Wind + Strike + 0.12f)
			{
				Pose.HandL = FVector(Reach, -18.0f, 236.0f);
				Pose.HandR = FVector(Reach, 18.0f, 236.0f);
				Pose.TorsoPitch = -4.0f;
				Pose.PelvisOffset += FVector(14.0f, 0.0f, 0.0f);
				Pose.TorsoYaw = 8.0f;
				Sharp = 40.0f;
			}
		}
	}
	if (State == EOTRMatchState::Victory)
	{
		Pose.HandL = FVector(40.0f, -88.0f, 300.0f);
		Pose.HandR = FVector(40.0f, 88.0f, 300.0f);
		Pose.TorsoPitch = -6.0f;
		Pose.HeadPitch = -10.0f;
		Sharp = 6.0f;
	}
	else if (State == EOTRMatchState::Defeat)
	{
		Pose.HandL = FVector(28.0f, -52.0f, 112.0f);
		Pose.HandR = FVector(28.0f, 52.0f, 112.0f);
		Pose.TorsoPitch = 24.0f;
		Pose.HeadPitch = 22.0f;
		Pose.Crouch = 22.0f;
		Sharp = 4.0f;
	}
	Rig->SetPose(Pose, Sharp);
}

void AOTRPlayerMech::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Stun = FMath::Max(0.0f, Stun - DeltaSeconds);
	DodgeCool = FMath::Max(0.0f, DodgeCool - DeltaSeconds);
	DodgeInvuln = FMath::Max(0.0f, DodgeInvuln - DeltaSeconds);
	TickAction(DeltaSeconds);
	if (DodgeClock > 0.0f)
	{
		const FOTRPlayerTuning& DT = OTR::Tuning(this)->Player;
		const float A = 1.0f - DodgeClock / FMath::Max(DT.DodgeSeconds, 0.05f);
		GetCharacterMovement()->Velocity = DodgeDir * DT.DodgeSpeed * FMath::Lerp(1.0f, 0.35f, A * A);
		DodgeClock -= DeltaSeconds;
	}
	const EOTRMatchState State = MatchState();
	if (State != EOTRMatchState::Active)
	{
		Guard = State == EOTRMatchState::Ready ? Guard : EOTRGuard::None;
	}
	const UOTRTuning* T = OTR::Tuning(this);
	GetCharacterMovement()->MaxWalkSpeed = Guard != EOTRGuard::None ? T->Player.GuardWalkSpeed : T->Player.WalkSpeed;
	FaceChampion(DeltaSeconds);
	UpdatePose(DeltaSeconds);

	// Lock-on camera: yaw toward the look point between us and the champion, fixed pitch; wide during the countdown.
	if (AController* C = GetController())
	{
		const FOTRCameraTuning& Cam = T->Camera;
		static const bool bPortrait = FParse::Param(FCommandLine::Get(), TEXT("OTRPortrait"));
		const bool bReady = State == EOTRMatchState::Ready;
		FVector Look = GetActorLocation() + GetActorForwardVector() * 300.0f;
		if (const AActor* Champ = ChampionActor())
		{
			Look = FMath::Lerp(GetActorLocation(), Champ->GetActorLocation(), Cam.LookBias + (OTR_IsLaunching() ? 0.3f : 0.0f));
		}
		FVector ToLook = Look - GetActorLocation();
		float WantYaw = ToLook.IsNearlyZero(1.0f) ? GetActorRotation().Yaw : ToLook.Rotation().Yaw;
		float WantPitch = bReady ? Cam.ReadyPitch : Cam.Pitch;
		float WantArm = bReady ? Cam.ReadyArmLength : Cam.ArmLength;
		float WantRoll = 0.0f;
		// Camera phases (review 3): fight card orbit, countdown sweep, lock-on, launch dolly, finisher side view, end orbit.
		const IOTRMatchInfo* MI = OTRMatchOf(this);
		const bool bTitle = MI && MI->OTR_IsTitle();
		const bool bFinisher = MI && MI->OTR_IsFinisher();
		const bool bEnded = State == EOTRMatchState::Victory || State == EOTRMatchState::Defeat;
		const int32 Phase = bTitle ? 0 : bFinisher ? 2 : bEnded ? 3 : 1;
		if (Phase != CamPhase)
		{
			CamPhase = Phase;
			OrbitYaw = 0.0f;
		}
		if (bTitle)
		{
			OrbitYaw += DeltaSeconds * Cam.TitleOrbitSpeed;
			WantYaw += 150.0f + OrbitYaw;
			WantPitch = Cam.TitlePitch;
			WantArm = Cam.TitleArmLength;
		}
		else if (bFinisher)
		{
			WantYaw += Cam.FinisherYawOffset;
			WantPitch = Cam.FinisherPitch;
			WantArm = Cam.FinisherArmLength;
		}
		else if (bEnded)
		{
			OrbitYaw += DeltaSeconds * Cam.EndOrbitSpeed;
			const bool bWon = State == EOTRMatchState::Victory;
			WantYaw += (bWon ? 35.0f : -20.0f) + OrbitYaw;
			WantPitch = bWon ? Cam.EndPitch : Cam.DefeatPitch;
			WantArm = Cam.EndArmLength;
			WantRoll = bWon ? 0.0f : Cam.DefeatRoll;
		}
		else if (OTR_IsLaunching())
		{
			WantArm -= Cam.LaunchDolly;
			const float Side = FVector::DotProduct(GetVelocity(), GetActorRightVector()) >= 0.0f ? 1.0f : -1.0f;
			WantRoll = -Side * Cam.LaunchRoll;
		}
		if (bPortrait)
		{
			WantYaw += 150.0f;
			WantPitch = -4.0f;
			WantArm = 420.0f;
		}
		const FRotator R = C->GetControlRotation();
		const float Follow = bTitle || bEnded ? 1.5f : (bFinisher ? 2.5f : Cam.YawFollowSpeed);
		const float Yaw = R.Yaw + FRotator::NormalizeAxis(WantYaw - R.Yaw) * FMath::Min(1.0f, DeltaSeconds * Follow);
		const float Pitch = FMath::FInterpTo(FRotator::NormalizeAxis(R.Pitch), WantPitch, DeltaSeconds, Cam.BlendSpeed);
		C->SetControlRotation(FRotator(Pitch, Yaw, 0.0f));
		Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, WantArm, DeltaSeconds, OTR_IsLaunching() ? 6.0f : Cam.BlendSpeed);
		// Roll: a spring toward the phase's tilt, kicked by clean hits; plus a slow broadcast handheld drift.
		CamRollVel += (Cam.RollStiffness * (WantRoll - CamRoll) - Cam.RollDamping * CamRollVel) * DeltaSeconds;
		CamRoll += CamRollVel * DeltaSeconds;
		CamClock += DeltaSeconds;
		const float W = 2.0f * PI * Cam.HandheldHz;
		Camera->SetRelativeRotation(FRotator(Cam.Handheld * FMath::Sin(CamClock * W), Cam.Handheld * 0.8f * FMath::Sin(CamClock * W * 0.73f + 1.3f), CamRoll));
		const float WantFOV = Cam.FOV + (OTR_IsLaunching() ? Cam.LaunchFOVKick : 0.0f);
		BaseFOV = FMath::FInterpTo(BaseFOV > 1.0f ? BaseFOV : Cam.FOV, WantFOV, DeltaSeconds, 8.0f);
		Camera->SetFieldOfView(BaseFOV + (Feel ? Feel->GetFOVPunch() : 0.0f));
	}
}

#undef LOCTEXT_NAMESPACE
