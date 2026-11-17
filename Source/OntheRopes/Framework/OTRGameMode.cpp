// Copyright On the Ropes. All Rights Reserved.

#include "Framework/OTRGameMode.h"
#include "Framework/OTRPlayerController.h"
#include "Framework/OTREventSubsystem.h"
#include "Framework/OTRAutoplayComponent.h"
#include "Mech/OTRPlayerMech.h"
#include "Mech/OTRChampionMech.h"
#include "Components/OTRArmorComponent.h"
#include "Data/OTRTuning.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformMisc.h"
#include "UnrealClient.h"
#include "OntheRopes.h"

#define LOCTEXT_NAMESPACE "OntheRopes"

int32 AOTRGameMode::RunsDone = 0;
int32 AOTRGameMode::Wins = 0;

AOTRGameMode::AOTRGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AOTRPlayerMech::StaticClass();
	PlayerControllerClass = AOTRPlayerController::StaticClass();
	TuningAsset = TSoftObjectPtr<UOTRTuning>(FSoftObjectPath(TEXT("/Game/OntheRopes/Data/DA_OTR_Tuning.DA_OTR_Tuning")));
}

void AOTRGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	// Loaded here, not in BeginPlay: level actors (ring, arena) can BeginPlay before the GameMode (Ripe Run lesson).
	Tuning = TuningAsset.LoadSynchronous();
	if (!Tuning)
	{
		UE_LOG(LogOntheRopes, Warning, TEXT("[OTR] DA_OTR_Tuning missing, using class defaults"));
		Tuning = NewObject<UOTRTuning>(this, TEXT("DefaultTuning"));
	}
	Super::InitGame(MapName, Options, ErrorMessage);
	CheckCommandLine();
}

void AOTRGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (bHideEngineScreenMessages)
	{
		if (GEngine)
		{
			GEngine->bEnableOnScreenDebugMessages = false;
		}
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			PC->ConsoleCommand(TEXT("DisableAllScreenMessages"));
		}
	}
	// Never-fall-through: KillZ here as well as in the WorldSettings (the GameMode wins at runtime).
	if (AWorldSettings* WS = GetWorldSettings())
	{
		WS->KillZ = Tuning->Match.KillZ;
		WS->bEnableWorldBoundsChecks = true;
	}
	FindRing();
	SpawnChampion();
	if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		PlayerStartLocation = P->GetActorLocation();
		PlayerStartRotation = P->GetActorRotation();
	}
	// The fight card holds first; a rematch (R) skips straight to the countdown.
	bTitle = !UGameplayStatics::HasOption(OptionsString, TEXT("rematch"));
	SetState(EOTRMatchState::Ready);
	if (bAutoplayActive)
	{
		ActivateAutoplayMode();
	}
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnCleanHit.AddDynamic(this, &AOTRGameMode::HandleCleanHit);
		if (bEventShots)
		{
			Events->OnRopeSnapped.AddDynamic(this, &AOTRGameMode::HandleSnapShot);
			Events->OnOverheat.AddDynamic(this, &AOTRGameMode::HandleVentShot);
		}
	}
}

void AOTRGameMode::CheckCommandLine()
{
	const TCHAR* Cmd = FCommandLine::Get();
	if (FParse::Param(Cmd, TEXT("autoplay")))
	{
		UE_LOG(LogOntheRopes, Warning, TEXT("[OTR] Autoplay mode requested via command line."));
		bAutoplayActive = true;
	}
	bExitAfterMatch = FParse::Param(Cmd, TEXT("autoplayexit")) || FParse::Param(Cmd, TEXT("OTRExit"));
	bEventShots = FParse::Param(Cmd, TEXT("OTREventShots"));
	bBotThrows = FParse::Param(Cmd, TEXT("OTRDefeat"));
	FParse::Value(Cmd, TEXT("OTRSkill="), BotSkill);
	FParse::Value(Cmd, TEXT("OTRRuns="), RunsWanted);
	FParse::Value(Cmd, TEXT("OTRShotTag="), ShotTag);
	FParse::Value(Cmd, TEXT("OTRQuitAfter="), QuitAfter);
	if (FParse::Value(Cmd, TEXT("OTRSeed="), Seed) && Seed >= 0)
	{
		FMath::RandInit(Seed + RunsDone * 7919);
		FMath::SRandInit(Seed + RunsDone * 7919);
	}
	FString Shots;
	if (FParse::Value(Cmd, TEXT("OTRShots="), Shots, false))
	{
		TArray<FString> Parts;
		Shots.ParseIntoArray(Parts, TEXT(","));
		ShotTimes.Reset();
		for (const FString& P : Parts)
		{
			ShotTimes.Add(FCString::Atof(*P));
		}
		ShotTimes.Sort();
	}
}

void AOTRGameMode::ActivateAutoplayMode()
{
	// The bot drives the mech through the same controller calls a human uses (and lights the same legend rows).
	APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!PC)
	{
		return;
	}
	UOTRAutoplayComponent* Bot = NewObject<UOTRAutoplayComponent>(PC, TEXT("AutoplayBot"));
	Bot->Skill = FMath::Clamp(BotSkill, 0.0f, 1.0f);
	Bot->bThrowMatch = bBotThrows;
	Bot->RegisterComponent();
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Autoplay bot active (skill %.2f%s)"), Bot->Skill, bBotThrows ? TEXT(", throwing") : TEXT(""));
}

void AOTRGameMode::FindRing()
{
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (Cast<IOTRRingInfo>(*It))
		{
			Ring = *It;
			break;
		}
	}
	if (!Ring)
	{
		UE_LOG(LogOntheRopes, Error, TEXT("[OTR] No ring in the level"));
	}
}

void AOTRGameMode::SpawnChampion()
{
	const IOTRRingInfo* R = Cast<IOTRRingInfo>(Ring);
	const FVector Centre = R ? R->OTR_Centre() : FVector(0.0f, 0.0f, 120.0f);
	const FOTRChampionTuning& T = Tuning->Champion;
	const FVector At = Centre + FVector(T.StartLocation.X, T.StartLocation.Y, T.CapsuleHalfHeight + 5.0f);
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	Champion = GetWorld()->SpawnActor<AOTRChampionMech>(AOTRChampionMech::StaticClass(), At, FRotator(0.0f, 180.0f, 0.0f), Params);
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Champion spawned: %s"), Champion ? TEXT("yes") : TEXT("FAILED"));
}

float AOTRGameMode::OTR_TimeLeft() const
{
	if (State == EOTRMatchState::Ready)
	{
		return Tuning ? Tuning->Match.RoundSeconds : 0.0f;
	}
	if (State == EOTRMatchState::Active)
	{
		return FMath::Max(0.0f, Tuning->Match.RoundSeconds - StateClock);
	}
	return TimeAtEnd;
}

float AOTRGameMode::OTR_Elapsed() const
{
	if (State == EOTRMatchState::Ready)
	{
		return -FMath::Max(0.0f, (Tuning ? Tuning->Match.CountdownSeconds : 3) - StateClock);
	}
	return State == EOTRMatchState::Active ? StateClock : Tuning->Match.RoundSeconds - TimeAtEnd;
}

void AOTRGameMode::OTR_Stats(int32& OutCleanHits, int32& OutSnapped, float& OutArmor) const
{
	const UOTREventSubsystem* Events = UOTREventSubsystem::Get(this);
	OutCleanHits = Events ? Events->CleanHits : 0;
	OutSnapped = Events ? Events->RopesSnapped : 0;
	const APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	const UOTRArmorComponent* A = P ? P->FindComponentByClass<UOTRArmorComponent>() : nullptr;
	OutArmor = A ? A->GetArmor() : 0.0f;
}

void AOTRGameMode::SetState(EOTRMatchState NewState, EOTRDefeat Reason)
{
	State = NewState;
	DefeatReason = Reason;
	StateClock = 0.0f;
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] State -> %s"), *UEnum::GetValueAsString(NewState));
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnMatchState.Broadcast(NewState, Reason);
		if (NewState == EOTRMatchState::Active)
		{
			Events->OnCountdown.Broadcast(0);
		}
	}
}

void AOTRGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Tuning)
	{
		return;
	}
	StateClock += DeltaSeconds;
	RealClock += FApp::GetDeltaTime();
	TickShots();
	if (QuitAfter > 0.0f && RealClock >= QuitAfter)
	{
		QuitAfter = 0.0f;
		FPlatformMisc::RequestExit(false);
	}
	if (!bArmorBound)
	{
		if (APawn* P = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			if (UOTRArmorComponent* A = P->FindComponentByClass<UOTRArmorComponent>())
			{
				A->OnDepleted.AddDynamic(this, &AOTRGameMode::HandleDepleted);
				bArmorBound = true;
			}
		}
	}

	switch (State)
	{
	case EOTRMatchState::Ready:
	{
		if (bTitle)
		{
			// Fight card: the hall idles; autoplay lets it read, then visibly presses ENTER.
			StateClock = 0.0f;
			TitleClock += FApp::GetDeltaTime();
			if (bAutoplayActive && TitleClock >= Tuning->Match.TitleHoldSeconds)
			{
				if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
				{
					Events->OnInput.Broadcast(TEXT("Start"), true);
				}
				OTR_StartFight();
			}
			break;
		}
		const int32 Left = FMath::CeilToInt(Tuning->Match.CountdownSeconds - StateClock);
		if (Left != LastCountdown && Left > 0)
		{
			LastCountdown = Left;
			if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
			{
				Events->OnCountdown.Broadcast(Left);
			}
		}
		if (StateClock >= Tuning->Match.CountdownSeconds)
		{
			SetState(EOTRMatchState::Active);
			// QA: -OTRCornerTest starts the player beside corner 0 (+X +Y) to prove the two-rope lean.
			static const bool bCornerTest = FParse::Param(FCommandLine::Get(), TEXT("OTRCornerTest"));
			const IOTRRingInfo* R = Cast<IOTRRingInfo>(Ring);
			APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
			static const bool bRingOutTest = FParse::Param(FCommandLine::Get(), TEXT("OTRRingOutTest"));
			if (bRingOutTest && R && P)
			{
				// QA: -OTRRingOutTest -OTRDefeat -OTRFray=0,0,1,0 puts the player beside the open -X side.
				P->TeleportTo(R->OTR_SidePoint(2, 0.5f) + FVector(110.0f, 0.0f, Tuning->Player.CapsuleHalfHeight + 5.0f), FRotator::ZeroRotator, false, true);
			}
			if (bCornerTest && R && P)
			{
				const FVector Post = R->OTR_SidePoint(0, 1.0f);
				const FVector In = (R->OTR_Centre() - Post).GetSafeNormal2D();
				P->TeleportTo(Post + In * 110.0f + FVector(0.0f, 0.0f, Tuning->Player.CapsuleHalfHeight + 5.0f), In.Rotation(), false, true);
			}
		}
		break;
	}
	case EOTRMatchState::Active:
		TickBounds();
		if (State == EOTRMatchState::Active && StateClock >= Tuning->Match.RoundSeconds)
		{
			// A dazed champion at the bell still counts as your knockout.
			EndMatch(bFinisher, bFinisher ? EOTRDefeat::None : EOTRDefeat::Bell);
		}
		break;
	default:
		EndClock += FApp::GetDeltaTime();
		if (!PendingEndShot.IsEmpty() && EndClock >= 1.6f)
		{
			Shot(PendingEndShot);
			PendingEndShot.Reset();
		}
		if (bAutoplayActive && !bEndHandled && EndClock >= Tuning->Match.AutoplayRestartDelay)
		{
			bEndHandled = true;
			// -autoplay -autoplayexit plays until a Victory (at most AutoplayMaxAttempts bouts); -OTRRuns=N plays N bouts.
			const bool bMoreRuns = RunsWanted > 0 ? RunsDone < RunsWanted : (State == EOTRMatchState::Defeat && !bBotThrows && RunsDone < AutoplayMaxAttempts);
			if (bExitAfterMatch && !bMoreRuns)
			{
				UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Autoplay finished (%d/%d wins), exiting."), Wins, RunsDone);
				FPlatformMisc::RequestExit(false);
			}
			else
			{
				RestartRound();
			}
		}
		break;
	}
}

void AOTRGameMode::TickBounds()
{
	APawn* P = UGameplayStatics::GetPlayerPawn(this, 0);
	const IOTRRingInfo* R = Cast<IOTRRingInfo>(Ring);
	if (!P || !R)
	{
		return;
	}
	const FVector L = P->GetActorLocation();
	// Through an open side: out of the ring.
	for (int32 S = 0; S < R->OTR_SideCount(); ++S)
	{
		if (R->OTR_IsSnapped(S) && R->OTR_SideDistance(S, L) < -Tuning->Match.RingOutMargin)
		{
			UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Ring-out through side %d"), S);
			EndMatch(false, EOTRDefeat::RingOut);
			return;
		}
	}
	// Safety net (should never fire): anything far below the canvas goes back to its start.
	if (L.Z < Tuning->Match.RespawnBelowZ)
	{
		P->TeleportTo(PlayerStartLocation, PlayerStartRotation, false, true);
		if (ACharacter* C = Cast<ACharacter>(P))
		{
			C->GetCharacterMovement()->StopMovementImmediately();
		}
		UE_LOG(LogOntheRopes, Warning, TEXT("[OTR] Player fell below the hall; put back at the start"));
	}
	if (Champion && Champion->GetActorLocation().Z < Tuning->Match.RespawnBelowZ)
	{
		Champion->TeleportTo(R->OTR_Centre() + FVector(0.0f, 0.0f, Tuning->Champion.CapsuleHalfHeight + 10.0f), Champion->GetActorRotation(), false, true);
	}
}

void AOTRGameMode::HandleDepleted(AActor* LastSource)
{
	if (State == EOTRMatchState::Active)
	{
		EndMatch(false, EOTRDefeat::Armor);
	}
}

void AOTRGameMode::HandleCleanHit(float Load, FVector Location)
{
	if (State != EOTRMatchState::Active)
	{
		return;
	}
	Progress = FMath::Min(Tuning->Match.KnockdownTarget, Progress + Load);
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Progress %.2f / %.1f at %.1fs"), Progress, Tuning->Match.KnockdownTarget, StateClock);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnPopup.Broadcast(FText::FromString(FString::Printf(TEXT("+%.1f"), Load)), Location + FVector(0.0f, 0.0f, 140.0f), FLinearColor::White);
		if (Progress < Tuning->Match.KnockdownTarget - 0.01f)
		{
			Events->OnNotice.Broadcast(Load >= 0.95f ? LOCTEXT("FullHit", "FULL POWER") : LOCTEXT("PartHit", "CLEAN HIT"),
				FText::Format(LOCTEXT("ProgressSub", "{0} of {1} to the knockout"), FText::AsNumber(FMath::FloorToInt(Progress + 0.01f)), FText::AsNumber(FMath::RoundToInt(Tuning->Match.KnockdownTarget))), false);
		}
	}
	if (bEventShots)
	{
		Shot(FString::Printf(TEXT("Clean%d"), FMath::RoundToInt(Progress * 10.0f)));
	}
	if (Progress >= Tuning->Match.KnockdownTarget - 0.01f && !bFinisher)
	{
		// Review 2: the last launch leaves the champion dazed; the player deals the finishing blow (Q).
		bFinisher = true;
		if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
		{
			Events->OnNotice.Broadcast(LOCTEXT("FinishTitle", "IT'S DAZED"), LOCTEXT("FinishSub", "Press Q for the finishing blow"), false);
		}
	}
}

void AOTRGameMode::OTR_FinishingBlow(const FVector& Location)
{
	if (OTR_IsFinisher())
	{
		UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Finishing blow at %.1fs"), StateClock);
		EndMatch(true, EOTRDefeat::None);
	}
}

void AOTRGameMode::EndMatch(bool bWon, EOTRDefeat Reason)
{
	if (State != EOTRMatchState::Active)
	{
		return;
	}
	TimeAtEnd = FMath::Max(0.0f, Tuning->Match.RoundSeconds - StateClock);
	++RunsDone;
	Wins += bWon ? 1 : 0;
	DefeatReason = Reason;
	WriteTelemetry(bWon);
	SetState(bWon ? EOTRMatchState::Victory : EOTRMatchState::Defeat, Reason);
	EndClock = 0.0f;
	bEndHandled = false;
	if (bEventShots)
	{
		PendingEndShot = bWon ? TEXT("Victory") : TEXT("Defeat");
	}
}

void AOTRGameMode::RestartRound()
{
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Restart"));
	UGameplayStatics::SetGlobalTimeDilation(this, 1.0f);
	UGameplayStatics::OpenLevel(this, FName(*UGameplayStatics::GetCurrentLevelName(this, true)), true, TEXT("rematch"));
}

void AOTRGameMode::OTR_StartFight()
{
	if (!bTitle)
	{
		return;
	}
	bTitle = false;
	LastCountdown = 0;
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Fight card closed after %.1fs"), TitleClock);
	SetState(EOTRMatchState::Ready);
}

void AOTRGameMode::Shot(const FString& Label)
{
	const FString Name = FString::Printf(TEXT("OTR%s_%s_%04d.png"), *ShotTag, *Label, FMath::RoundToInt(RealClock * 10.0f));
	FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir() / TEXT("Screenshots") / TEXT("OTR") / Name, true, false);
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Screenshot %s"), *Name);
}

void AOTRGameMode::TickShots()
{
	if (NextShot < ShotTimes.Num() && RealClock >= ShotTimes[NextShot])
	{
		Shot(FString::Printf(TEXT("t%04d"), FMath::RoundToInt(ShotTimes[NextShot] * 10.0f)));
		++NextShot;
	}
}

void AOTRGameMode::HandleSnapShot(int32 Side, FVector Location)
{
	Shot(FString::Printf(TEXT("Snap%d"), Side));
}

void AOTRGameMode::HandleVentShot(FVector Location)
{
	if (VentShots++ < 2)
	{
		Shot(FString::Printf(TEXT("Vent%d"), VentShots));
	}
}

void AOTRGameMode::WriteTelemetry(bool bWon)
{
	const UOTREventSubsystem* E = UOTREventSubsystem::Get(this);
	int32 Clean = 0, Snapped = 0;
	float Armor = 0.0f;
	OTR_Stats(Clean, Snapped, Armor);
	const FString Json = FString::Printf(
		TEXT("{\"result\":\"%s\",\"reason\":\"%s\",\"duration\":%.1f,\"timeLeft\":%.1f,\"progress\":%.2f,\"armor\":%.0f,\"lowestArmor\":%.0f,\"cleanHits\":%d,\"blocked\":%d,\"snapped\":%d,\"hitsTaken\":%d,\"guarded\":%d,\"vents\":%d,\"cornerLoads\":%d,\"dodges\":%d,\"jabs\":%d,\"jabsLanded\":%d,\"interrupts\":%d,\"guardBreaks\":%d,\"parries\":%d,\"ducks\":%d,\"skill\":%.2f,\"seed\":%d}"),
		bWon ? TEXT("VICTORY") : TEXT("DEFEAT"), *UEnum::GetValueAsString(DefeatReason),
		StateClock, FMath::Max(0.0f, Tuning->Match.RoundSeconds - StateClock), Progress, Armor, E ? E->LowestArmor : 0.0f, Clean,
		E ? E->BlockedLaunches : 0, Snapped, E ? E->HitsTaken : 0, E ? E->HitsGuarded : 0, E ? E->Vents : 0, E ? E->CornerLoads : 0, E ? E->Dodges : 0,
		E ? E->Jabs : 0, E ? E->JabsLanded : 0, E ? E->Interrupts : 0, E ? E->GuardBreaks : 0, E ? E->Parries : 0, E ? E->Ducks : 0, BotSkill, Seed);
	const FString Path = FPaths::ProjectSavedDir() / TEXT("Telemetry") / FString::Printf(TEXT("match_%d.json"), RunsDone);
	FFileHelper::SaveStringToFile(Json, *Path);
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Match ended: %s %s"), bWon ? TEXT("VICTORY") : TEXT("DEFEAT"), *Json);
}

#undef LOCTEXT_NAMESPACE
