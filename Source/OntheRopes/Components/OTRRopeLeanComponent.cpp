// Copyright On the Ropes. All Rights Reserved.

#include "Components/OTRRopeLeanComponent.h"
#include "Ring/OTRRopeMath.h"
#include "Interfaces/OTRInterfaces.h"
#include "Data/OTRTuning.h"
#include "Framework/OTREventSubsystem.h"
#include "Feedback/OTRFeedback.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "OntheRopes.h"

#define LOCTEXT_NAMESPACE "OntheRopes"

UOTRRopeLeanComponent::UOTRRopeLeanComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

IOTRRingInfo* UOTRRopeLeanComponent::Ring() const
{
	const IOTRMatchInfo* Match = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	return Match ? Cast<IOTRRingInfo>(Match->OTR_Ring()) : nullptr;
}

IOTRChampion* UOTRRopeLeanComponent::Champion(AActor** OutActor) const
{
	const IOTRMatchInfo* Match = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	AActor* A = Match ? Match->OTR_Champion() : nullptr;
	if (OutActor)
	{
		*OutActor = A;
	}
	return Cast<IOTRChampion>(A);
}

float UOTRRopeLeanComponent::OwnerRadius() const
{
	const ACharacter* C = Cast<ACharacter>(GetOwner());
	return C ? C->GetCapsuleComponent()->GetScaledCapsuleRadius() : 70.0f;
}

bool UOTRRopeLeanComponent::CanAct() const
{
	const IOTRMatchInfo* Match = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	return Match && Match->OTR_State() == EOTRMatchState::Active;
}

void UOTRRopeLeanComponent::FindSides(TArray<int32>& Out, float ExtraReach) const
{
	Out.Reset();
	const IOTRRingInfo* R = Ring();
	if (!R)
	{
		return;
	}
	const float Reach = OwnerRadius() + OTR::Tuning(this)->Rope.LeanReach + ExtraReach;
	const FVector Loc = GetOwner()->GetActorLocation();
	for (int32 S = 0; S < R->OTR_SideCount(); ++S)
	{
		if (!R->OTR_IsSnapped(S) && R->OTR_SideDistance(S, Loc) <= Reach)
		{
			Out.Add(S);
		}
	}
}

bool UOTRRopeLeanComponent::CanLeanHere() const
{
	TArray<int32> Found;
	FindSides(Found, 0.0f);
	return Found.Num() > 0;
}

void UOTRRopeLeanComponent::SetLeanHeld(bool bInHeld)
{
	const bool bWas = bHeld;
	bHeld = bInHeld;
	if (bWas && !bHeld && bLeaning)
	{
		Release();
	}
}

bool UOTRRopeLeanComponent::TryStartLean()
{
	if (bLeaning || bLaunching || !CanAct())
	{
		return false;
	}
	TArray<int32> Found;
	FindSides(Found, 0.0f);
	if (Found.Num() == 0)
	{
		return false;
	}
	LeanSides = Found;
	bLeaning = true;
	Load = 0.0f;
	FullClock = 0.0f;
	bCornerCounted = false;
	if (ACharacter* C = Cast<ACharacter>(GetOwner()))
	{
		C->GetCharacterMovement()->StopMovementImmediately();
	}
	OTRFeedback::Sound(this, TEXT("RopeCreak"), GetOwner()->GetActorLocation(), 0.6f, 0.8f);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnLean.Broadcast(true);
		if (IsCorner())
		{
			Events->PromptOnce(TEXT("Corner"), LOCTEXT("PromptCorner", "CORNER  DOUBLE LOAD"), GetOwner(), FVector(0.0f, 0.0f, 150.0f));
		}
	}
	if (IOTRRingInfo* R = Ring())
	{
		R->OTR_SetLean(LeanSides, GetOwner()->GetActorLocation(), Load);
	}
	UE_LOG(LogOntheRopes, Verbose, TEXT("[OTR] Lean on %d side(s)"), LeanSides.Num());
	return true;
}

void UOTRRopeLeanComponent::OnAbsorb(const FOTRPunchRow& Punch)
{
	if (!bLeaning)
	{
		return;
	}
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	const float Before = Load;
	Load = OTRRope::AddLoad(Load, Punch.LoadGain, IsCorner(), T.CornerLoadMultiplier, T.MaxLoad);
	UOTREventSubsystem* Events = UOTREventSubsystem::Get(this);
	if (IsCorner() && !bCornerCounted && Events)
	{
		bCornerCounted = true;
		++Events->CornerLoads;
	}
	OTRFeedback::Sound(this, TEXT("RopeCreak"), GetOwner()->GetActorLocation(), 0.7f, 0.75f + 0.6f * Load);
	if (Events && Before < OTR::Tuning(this)->Champion.JabModeLoad && Load >= OTR::Tuning(this)->Champion.JabModeLoad)
	{
		AActor* ChampActor = nullptr;
		Champion(&ChampActor);
		Events->PromptOnce(TEXT("Wait"), LOCTEXT("PromptWait", "WAIT FOR THE VENT"), ChampActor, FVector(0.0f, 0.0f, 265.0f));
	}
	IOTRRingInfo* R = Ring();
	if (!R)
	{
		return;
	}
	const int32 Snapped = R->OTR_AbsorbHit(LeanSides, Punch.FrayGain);
	if (Snapped != INDEX_NONE)
	{
		BreakLean(true);
	}
	else
	{
		R->OTR_SetLean(LeanSides, GetOwner()->GetActorLocation(), Load);
	}
}

void UOTRRopeLeanComponent::Bleed(float Fraction)
{
	Load = FMath::Max(0.0f, Load * (1.0f - FMath::Clamp(Fraction, 0.0f, 1.0f)));
}

void UOTRRopeLeanComponent::BreakLean(bool bLoseLoad)
{
	if (!bLeaning)
	{
		return;
	}
	if (IOTRRingInfo* R = Ring())
	{
		R->OTR_Release(LeanSides, bLoseLoad ? 0.0f : Load);
	}
	bLeaning = false;
	LeanSides.Reset();
	if (bLoseLoad)
	{
		Load = 0.0f;
	}
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnLean.Broadcast(false);
	}
}

void UOTRRopeLeanComponent::Release()
{
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	if (Load >= T.MinLaunchLoad && CanAct())
	{
		StartLaunch();
	}
	else
	{
		BreakLean(true);
	}
}

void UOTRRopeLeanComponent::StartLaunch()
{
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	AActor* Target = nullptr;
	Champion(&Target);
	const FVector From = GetOwner()->GetActorLocation();
	FVector To = Target ? Target->GetActorLocation() : From + GetOwner()->GetActorForwardVector() * 500.0f;
	FVector Dir = To - From;
	Dir.Z = 0.0f;
	const float Dist = FMath::Min(Dir.Size(), T.LaunchMaxDistance);
	LaunchDir = Dir.GetSafeNormal();
	if (LaunchDir.IsNearlyZero())
	{
		LaunchDir = GetOwner()->GetActorForwardVector();
	}
	LaunchLoad = Load;
	LaunchDuration = T.LaunchDashTime;
	// Cover the gap (plus a little overshoot) in the dash time.
	LaunchSpeed = FMath::Max(900.0f, (Dist + 120.0f) / FMath::Max(LaunchDuration, 0.05f));
	LaunchClock = 0.0f;
	bResolved = false;
	TArray<int32> Sides = LeanSides;
	if (IOTRRingInfo* R = Ring())
	{
		R->OTR_Release(Sides, LaunchLoad);
	}
	bLeaning = false;
	LeanSides.Reset();
	Load = 0.0f;
	bLaunching = true;
	OTRFeedback::Sound(this, TEXT("Twang"), From, 0.9f, 0.85f + 0.3f * LaunchLoad);
	OTRFeedback::Sound(this, TEXT("Whoosh"), From, 0.9f, 1.0f);
	OTRFeedback::Vfx(this, TEXT("Dust"), From - FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator, 1.0f + LaunchLoad);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnLean.Broadcast(false);
		Events->OnImpact.Broadcast(From, 0.6f + 0.6f * LaunchLoad, true);
	}
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Launch with load %.2f"), LaunchLoad);
}

void UOTRRopeLeanComponent::TickLaunch(float Dt)
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (!C)
	{
		return;
	}
	LaunchClock += Dt;
	UCharacterMovementComponent* Move = C->GetCharacterMovement();
	AActor* Target = nullptr;
	IOTRChampion* Champ = Champion(&Target);
	if (!bResolved && Target && Champ)
	{
		const UCapsuleComponent* TC = Cast<UCapsuleComponent>(Target->GetRootComponent());
		const float Gap = FVector::Dist2D(C->GetActorLocation(), Target->GetActorLocation()) - OwnerRadius() - (TC ? TC->GetScaledCapsuleRadius() : 80.0f);
		if (Gap <= OTR::Tuning(this)->Rope.LaunchContactGap)
		{
			const EOTRLaunchResult Result = Champ->OTR_ReceiveLaunch(LaunchLoad, C);
			EndLaunch(Result, Target);
			return;
		}
	}
	if (LaunchClock >= LaunchDuration)
	{
		EndLaunch(EOTRLaunchResult::Whiff, Target);
		return;
	}
	// Ease out at the end of the dash so it lands instead of stopping dead.
	const float A = LaunchClock / LaunchDuration;
	const float Speed = LaunchSpeed * (A < 0.75f ? 1.0f : FMath::Lerp(1.0f, 0.4f, (A - 0.75f) / 0.25f));
	Move->Velocity = LaunchDir * Speed;
}

void UOTRRopeLeanComponent::EndLaunch(EOTRLaunchResult Result, AActor* Target)
{
	bLaunching = false;
	bResolved = true;
	ACharacter* C = Cast<ACharacter>(GetOwner());
	if (C)
	{
		UCharacterMovementComponent* Move = C->GetCharacterMovement();
		switch (Result)
		{
		case EOTRLaunchResult::Blocked: Move->Velocity = -LaunchDir * 520.0f; break;
		case EOTRLaunchResult::Clean: Move->Velocity = -LaunchDir * 180.0f; break;
		case EOTRLaunchResult::Sidestepped: Move->Velocity = LaunchDir * 420.0f; break;
		default: Move->Velocity = LaunchDir * 200.0f; break;
		}
	}
	UOTREventSubsystem* Events = UOTREventSubsystem::Get(this);
	const FVector At = Target ? (GetOwner()->GetActorLocation() + Target->GetActorLocation()) * 0.5f + FVector(0.0f, 0.0f, 60.0f) : GetOwner()->GetActorLocation();
	if (Events)
	{
		Events->OnLaunch.Broadcast(Result, LaunchLoad, At);
		if (Result == EOTRLaunchResult::Blocked || Result == EOTRLaunchResult::Sidestepped)
		{
			++Events->BlockedLaunches;
			Events->PromptOnce(TEXT("Blocked"), LOCTEXT("PromptBlocked", "LAUNCH WHILE IT VENTS"), Target, FVector(0.0f, 0.0f, 265.0f), true);
		}
	}
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Launch result %s (load %.2f)"), *UEnum::GetValueAsString(Result), LaunchLoad);
	LaunchLoad = 0.0f;
}

void UOTRRopeLeanComponent::TickLean(float Dt)
{
	ACharacter* C = Cast<ACharacter>(GetOwner());
	IOTRRingInfo* R = Ring();
	if (!C || !R)
	{
		return;
	}
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	// Still on the ropes? Pushed (dragged) too far off them breaks the lean and the rope lets go.
	TArray<int32> Still;
	FindSides(Still, T.LeanBreakDistance);
	LeanSides.RemoveAll([&Still](int32 S) { return !Still.Contains(S); });
	if (LeanSides.Num() == 0)
	{
		BreakLean(true);
		return;
	}
	// The ropes pull a dragged mech back onto them.
	const FVector Loc = C->GetActorLocation();
	const float Reach = OwnerRadius() + T.LeanReach * 0.5f;
	FVector Back = FVector::ZeroVector;
	for (int32 S : LeanSides)
	{
		const float D = R->OTR_SideDistance(S, Loc);
		if (D > Reach)
		{
			Back += R->OTR_SideNormal(S) * (D - Reach);
		}
	}
	if (!Back.IsNearlyZero())
	{
		C->GetCharacterMovement()->Velocity = Back.GetClampedToMaxSize(1.0f) * FMath::Min(Back.Size() * 6.0f, 240.0f);
	}
	// Held at full load: "load it too hard" and it frays.
	if (Load >= T.MaxLoad - KINDA_SMALL_NUMBER)
	{
		FullClock += Dt;
		if (R->OTR_FrayOverTime(LeanSides, T.FrayAtFullPerSecond, Dt) != INDEX_NONE)
		{
			BreakLean(true);
			return;
		}
	}
	R->OTR_SetLean(LeanSides, Loc, Load);
}

void UOTRRopeLeanComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (bLaunching)
	{
		TickLaunch(DeltaTime);
		return;
	}
	if (!CanAct())
	{
		if (bLeaning)
		{
			BreakLean(true);
		}
		return;
	}
	if (bLeaning)
	{
		TickLean(DeltaTime);
		return;
	}
	// Holding lean while walking back into a rope starts the lean on contact.
	if (bHeld)
	{
		TryStartLean();
	}
	else if (CanLeanHere())
	{
		if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
		{
			Events->PromptOnce(TEXT("Lean"), LOCTEXT("PromptLean", "HOLD SPACE  LEAN ON THE ROPES"), GetOwner(), FVector(0.0f, 0.0f, 150.0f));
		}
	}
}

#undef LOCTEXT_NAMESPACE
