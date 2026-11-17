// Copyright On the Ropes. All Rights Reserved.

#include "Components/OTRGameFeelComponent.h"
#include "Data/OTRTuning.h"
#include "Interfaces/OTRInterfaces.h"
#include "Feedback/OTRFeedback.h"
#include "Feedback/OTRCameraShakes.h"
#include "Framework/OTREventSubsystem.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UOTRGameFeelComponent::UOTRGameFeelComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Every frame: the FOV / fringe punches are animated here.
	PrimaryComponentTick.TickInterval = 0.0f;
}

void UOTRGameFeelComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnImpact.AddDynamic(this, &UOTRGameFeelComponent::HandleImpact);
		Events->OnPlayerHit.AddDynamic(this, &UOTRGameFeelComponent::HandlePlayerHit);
		Events->OnCleanHit.AddDynamic(this, &UOTRGameFeelComponent::HandleCleanHit);
		Events->OnLaunch.AddDynamic(this, &UOTRGameFeelComponent::HandleLaunch);
		Events->OnRopeSnapped.AddDynamic(this, &UOTRGameFeelComponent::HandleSnap);
		Events->OnRopeFrayWarning.AddDynamic(this, &UOTRGameFeelComponent::HandleFray);
		Events->OnMatchState.AddDynamic(this, &UOTRGameFeelComponent::HandleState);
		Events->OnCountdown.AddDynamic(this, &UOTRGameFeelComponent::HandleCountdown);
		Events->OnOverheat.AddDynamic(this, &UOTRGameFeelComponent::HandleOverheat);
	}
	const FOTRFeelTuning& T = OTR::Tuning(this)->Feel;
	Crowd = OTRFeedback::Loop2D(this, TEXT("HallAmbience"), T.AmbienceVolume);
	if (Crowd)
	{
		Crowd->Play();
	}
}

void UOTRGameFeelComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (WarpHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WarpHandle);
		WarpHandle.Reset();
	}
	if (GetWorld())
	{
		UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	}
	Super::EndPlay(Reason);
}

void UOTRGameFeelComponent::Shake(bool bHeavy, float Scale)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (PC && Scale > 0.03f)
	{
		PC->ClientStartCameraShake(bHeavy ? UOTRImpactShake::StaticClass() : UOTRMicroShake::StaticClass(), Scale * OTR::Tuning(this)->Feel.ShakeScale);
	}
}

void UOTRGameFeelComponent::TimeWarp(float Dilation, float RealSeconds)
{
	UGameplayStatics::SetGlobalTimeDilation(this, Dilation);
	if (WarpHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(WarpHandle);
	}
	// Real-time restore: the world clock is (nearly) frozen during the stop.
	TWeakObjectPtr<UOTRGameFeelComponent> Weak(this);
	WarpHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float)
	{
		if (UOTRGameFeelComponent* Self = Weak.Get())
		{
			UGameplayStatics::SetGlobalTimeDilation(Self, 1.0f);
			Self->WarpHandle.Reset();
		}
		return false;
	}), RealSeconds);
}

void UOTRGameFeelComponent::Punch(float FOV, float InFringe)
{
	// The strongest punch wins, so overlapping events never stack into a fisheye.
	if (FMath::Abs(FOV) >= FMath::Abs(FOVPunch))
	{
		FOVPunch = FOV;
	}
	Fringe = FMath::Max(Fringe, InFringe);
}

void UOTRGameFeelComponent::TickPunch(float RealDt)
{
	const FOTRFeelTuning& T = OTR::Tuning(this)->Feel;
	FOVPunch = FMath::FInterpTo(FOVPunch, 0.0f, RealDt, T.FOVPunchDecay);
	Fringe = FMath::FInterpTo(Fringe, 0.0f, RealDt, T.FringeDecay);
	if (UCameraComponent* Cam = GetOwner()->FindComponentByClass<UCameraComponent>())
	{
		Cam->PostProcessBlendWeight = 1.0f;
		Cam->PostProcessSettings.bOverride_SceneFringeIntensity = true;
		Cam->PostProcessSettings.SceneFringeIntensity = Fringe;
	}
}

void UOTRGameFeelComponent::HandleOverheat(FVector Location)
{
	Punch(OTR::Tuning(this)->Feel.FOVPunchVent, 0.0f);
	Shake(false, 0.8f);
}

void UOTRGameFeelComponent::HandleImpact(FVector Location, float Strength, bool bHeavy)
{
	const float Falloff = OTR::Tuning(this)->Feel.ShakeFalloff;
	const float D = FVector::Dist(GetOwner()->GetActorLocation(), Location);
	Shake(bHeavy, FMath::Clamp(1.0f - D / Falloff, 0.0f, 1.0f) * Strength);
}

void UOTRGameFeelComponent::HandlePlayerHit(float Damage, bool bGuarded, EOTRPunch Punch, FVector Location)
{
	const FOTRFeelTuning& T = OTR::Tuning(this)->Feel;
	if (bGuarded)
	{
		Shake(false, Punch == EOTRPunch::Jab ? 0.5f : 1.0f);
		return;
	}
	Shake(true, Punch == EOTRPunch::Jab ? 0.7f : 1.3f);
	TimeWarp(T.HitStopDilation, T.HitStopSeconds);
	this->Punch(T.FOVPunchHit * (Punch == EOTRPunch::Jab ? 0.5f : 1.0f), T.FringeHit);
	OTRFeedback::Sound2D(this, TEXT("HitSting"), 0.7f);
	CrowdBoost = FMath::Max(CrowdBoost, 0.4f);
}

void UOTRGameFeelComponent::HandleCleanHit(float Load, FVector Location)
{
	const FOTRFeelTuning& T = OTR::Tuning(this)->Feel;
	Shake(true, 1.6f + 1.2f * Load);
	TimeWarp(T.CleanHitSlowMo, T.CleanHitSlowMoSeconds);
	Punch(T.FOVPunchClean * (0.5f + 0.5f * Load), T.FringeClean * (0.5f + 0.5f * Load));
	OTRFeedback::Vfx(this, TEXT("Sparks"), Location, FRotator(90.0f, 0.0f, 0.0f), T.CleanSparkScale * (0.6f + 0.4f * Load));
	OTRFeedback::Vfx(this, TEXT("Dust"), FVector(Location.X, Location.Y, Location.Z - 200.0f), FRotator::ZeroRotator, T.CleanDustScale);
	OTRFeedback::Sound2D(this, TEXT("Bell"), 0.35f, 1.4f);
	OTRFeedback::Sound2D(this, TEXT("CrowdRoar"), 0.6f + 0.4f * Load);
	CrowdBoost = 1.0f;
}

void UOTRGameFeelComponent::HandleLaunch(EOTRLaunchResult Result, float Load, FVector Location)
{
	if (Result == EOTRLaunchResult::Blocked)
	{
		Shake(false, 1.2f);
	}
	else if (Result == EOTRLaunchResult::Sidestepped || Result == EOTRLaunchResult::Whiff)
	{
		OTRFeedback::Sound2D(this, TEXT("CrowdGroan"), 0.5f);
	}
}

void UOTRGameFeelComponent::HandleSnap(int32 Side, FVector Location)
{
	const FOTRFeelTuning& T = OTR::Tuning(this)->Feel;
	Shake(true, 1.6f);
	TimeWarp(T.SnapSlowMo, T.SnapSlowMoSeconds);
	Punch(T.FOVPunchSnap, T.FringeHit);
	OTRFeedback::Sound(this, TEXT("Snap"), Location, 1.0f);
	OTRFeedback::Vfx(this, TEXT("Sparks"), Location, FRotator(90.0f, 0.0f, 0.0f), 2.2f);
	OTRFeedback::Vfx(this, TEXT("Dust"), Location - FVector(0.0f, 0.0f, 120.0f), FRotator::ZeroRotator, 1.2f);
	OTRFeedback::Sound2D(this, TEXT("CrowdGroan"), 0.6f);
	CrowdBoost = 0.8f;
}

void UOTRGameFeelComponent::HandleFray(int32 Side, FVector Location)
{
	OTRFeedback::Sound(this, TEXT("FrayPing"), Location, 0.8f);
	OTRFeedback::Vfx(this, TEXT("Sparks"), Location, FRotator(90.0f, 0.0f, 0.0f), 0.5f);
}

void UOTRGameFeelComponent::HandleCountdown(int32 SecondsLeft)
{
	if (SecondsLeft > 0)
	{
		OTRFeedback::Sound2D(this, TEXT("Tick"), 0.8f);
	}
	else
	{
		OTRFeedback::Sound2D(this, TEXT("Bell"), 1.0f);
	}
}

void UOTRGameFeelComponent::HandleState(EOTRMatchState State, EOTRDefeat Reason)
{
	const FOTRFeelTuning& T = OTR::Tuning(this)->Feel;
	if (State == EOTRMatchState::Active)
	{
		TempoStep = 0;
		Music = OTRFeedback::Loop2D(this, TEXT("Music"), T.MusicVolume);
		if (Music)
		{
			Music->Play();
		}
	}
	else if (State == EOTRMatchState::Victory || State == EOTRMatchState::Defeat)
	{
		if (Music)
		{
			Music->FadeOut(0.6f, 0.0f);
		}
		OTRFeedback::Sound2D(this, TEXT("Bell"), 0.9f, 0.95f);
		OTRFeedback::Sound2D(this, State == EOTRMatchState::Victory ? FName(TEXT("Victory")) : FName(TEXT("Defeat")), 0.9f);
		// The fight-ending blow hits with a big FOV punch and fringe; a defeat also drops into slow motion.
		const bool bWon = State == EOTRMatchState::Victory;
		if (!bWon)
		{
			TimeWarp(T.DefeatSlowMo, T.DefeatSlowMoSeconds);
		}
		Punch(T.FOVPunchClean * (bWon ? 1.4f : 0.8f), T.FringeClean * (bWon ? 1.5f : 1.0f));
		Shake(true, bWon ? 2.4f : 1.6f);
		if (State == EOTRMatchState::Victory)
		{
			OTRFeedback::Sound2D(this, TEXT("CrowdVictory"), 1.0f);
			CrowdBoost = 1.0f;
			// Knockout: the champion goes down in slow motion while the camera punches in.
			TimeWarp(T.KOSlowMo, T.KOSlowMoSeconds);
			Punch(T.FOVPunchKO, T.FringeClean);
			Shake(true, 2.0f);
		}
	}
}

void UOTRGameFeelComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// Punches animate in real time so slow motion and hit-stop never stretch them.
	TickPunch(FMath::Min((float)FApp::GetDeltaTime(), 0.1f));
	const IOTRMatchInfo* Match = GetWorld() ? Cast<IOTRMatchInfo>(GetWorld()->GetAuthGameMode()) : nullptr;
	const UOTRTuning* T = OTR::Tuning(this);
	// The crowd swells after big moments and with the score.
	CrowdBoost = FMath::Max(0.0f, CrowdBoost - DeltaTime * 0.35f);
	if (Crowd && Match)
	{
		const float Progress = Match->OTR_Progress() / FMath::Max(T->Match.KnockdownTarget, 0.1f);
		Crowd->SetVolumeMultiplier(T->Feel.AmbienceVolume * (1.0f + 0.5f * Progress + 0.8f * CrowdBoost));
	}
	if (!Match || Match->OTR_State() != EOTRMatchState::Active)
	{
		return;
	}
	int32 Step = 0;
	for (float S : T->Match.TempoSteps)
	{
		Step += Match->OTR_Elapsed() >= S ? 1 : 0;
	}
	if (Music && Step != TempoStep)
	{
		TempoStep = Step;
		Music->SetPitchMultiplier(1.0f + 0.07f * Step);
	}
	const int32 Left = FMath::CeilToInt(Match->OTR_TimeLeft());
	if (Match->OTR_TimeLeft() <= T->Match.LastStretchSeconds && Left != LastTick && Left > 0)
	{
		LastTick = Left;
		OTRFeedback::Sound2D(this, TEXT("Tick"), 0.45f, 1.3f);
	}
}
