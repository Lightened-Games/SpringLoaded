// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Interfaces/OTRInterfaces.h"
#include "Data/OTRTypes.h"
#include "OTRGameMode.generated.h"

class UOTRTuning;

/**
 * On the Ropes bout flow (P1): Ready (3-2-1-FIGHT, input locked) -> Active (90 s bell, 3 power hits to win)
 * -> Victory (knockout) / Defeat (armor gone, through the ropes, or the bell) -> Restart [R]. Spawns the champion,
 * enforces bounds and ring-outs, starts the autoplay bot with -autoplay, writes telemetry and handles the QA flags:
 *   -autoplay -autoplayexit -OTRShots=3,8,14 -OTREventShots -OTRExit -OTRDefeat -OTRSkill=0..1 -OTRSeed=N -OTRRuns=N
 *   -OTRQuitAfter=N -OTRFray=a,b,c,d
 */
UCLASS()
class ONTHEROPES_API AOTRGameMode : public AGameModeBase, public IOTRMatchInfo
{
	GENERATED_BODY()

public:
	AOTRGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// IOTRMatchInfo
	virtual const UOTRTuning* OTR_Tuning() const override { return Tuning; }
	virtual EOTRMatchState OTR_State() const override { return State; }
	virtual EOTRDefeat OTR_DefeatReason() const override { return DefeatReason; }
	virtual float OTR_TimeLeft() const override;
	virtual float OTR_Elapsed() const override;
	virtual float OTR_Progress() const override { return Progress; }
	virtual bool OTR_IsAutoplay() const override { return bAutoplayActive; }
	virtual bool OTR_IsFinisher() const override { return bFinisher && State == EOTRMatchState::Active; }
	virtual void OTR_FinishingBlow(const FVector& Location) override;
	virtual bool OTR_IsTitle() const override { return bTitle; }
	virtual float OTR_TitleAge() const override { return TitleClock; }
	virtual void OTR_StartFight() override;
	virtual AActor* OTR_Ring() const override { return Ring; }
	virtual AActor* OTR_Champion() const override { return Champion; }
	virtual void OTR_Stats(int32& OutCleanHits, int32& OutSnapped, float& OutArmor) const override;
	virtual bool OTR_RequestRestart() override { if (!CanRestart()) { return false; } RestartRound(); return true; }

	UFUNCTION(BlueprintCallable, Category = "Match") void RestartRound();
	bool CanRestart() const { return State == EOTRMatchState::Victory || State == EOTRMatchState::Defeat; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") TSoftObjectPtr<UOTRTuning> TuningAsset;
	/** Hide engine on-screen notices (VRAM budget, shader warm-up) so they never appear in the gameplay video. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Presentation") bool bHideEngineScreenMessages = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") bool bAutoplayActive = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") bool bExitAfterMatch = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") float BotSkill = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") bool bBotThrows = false;
	/** Unattended runs replay a lost bout until a Victory, up to this many bouts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Autoplay") int32 AutoplayMaxAttempts = 5;

private:
	void CheckCommandLine();
	void ActivateAutoplayMode();
	void FindRing();
	void SpawnChampion();
	void SetState(EOTRMatchState NewState, EOTRDefeat Reason = EOTRDefeat::None);
	void EndMatch(bool bWon, EOTRDefeat Reason);
	void TickBounds();
	void TickShots();
	void WriteTelemetry(bool bWon);
	void Shot(const FString& Label);
	UFUNCTION() void HandleDepleted(AActor* LastSource);
	UFUNCTION() void HandleCleanHit(float Load, FVector Location);
	UFUNCTION() void HandleSnapShot(int32 Side, FVector Location);
	UFUNCTION() void HandleVentShot(FVector Location);

	UPROPERTY() TObjectPtr<UOTRTuning> Tuning;
	UPROPERTY() TObjectPtr<AActor> Ring;
	UPROPERTY() TObjectPtr<AActor> Champion;
	EOTRMatchState State = EOTRMatchState::Ready;
	EOTRDefeat DefeatReason = EOTRDefeat::None;
	float Progress = 0.0f;
	bool bFinisher = false;
	bool bTitle = false;
	float TitleClock = 0.0f;
	float StateClock = 0.0f;
	float RealClock = 0.0f;
	float EndClock = 0.0f;
	float TimeAtEnd = 0.0f;
	int32 LastCountdown = -1;
	bool bArmorBound = false;
	bool bEventShots = false;
	bool bEndHandled = false;
	int32 VentShots = 0;
	FString ShotTag;
	FString PendingEndShot;
	TArray<float> ShotTimes;
	int32 NextShot = 0;
	int32 Seed = -1;
	int32 RunsWanted = 0;
	float QuitAfter = 0.0f;
	FVector PlayerStartLocation = FVector::ZeroVector;
	FRotator PlayerStartRotation = FRotator::ZeroRotator;

	static int32 RunsDone;
	static int32 Wins;
};
