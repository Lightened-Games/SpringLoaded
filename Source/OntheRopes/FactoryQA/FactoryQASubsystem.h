// FactoryQA: drop-in quality gates from design guide Section 2.0 (C2 solidity, C3 gait). Copy this folder into any
// game's Source/<Module>/; no other code changes. It only exists when a flag is on the command line:
//   -FactoryQA        everything below
//   -CollisionAudit   C2 static scan: visible, reachable props that don't block characters / have no simple collision / don't affect navigation
//   -PenetrationLog   C2 runtime: characters overlapping blocking geometry or each other, or standing inside non-solid props
//   -GaitLog          C3 numbers: toe-in, knee bow, foot slide, contact height per idle/walk/run band
//   -GaitShots        C3 pictures: front/side/back leg close-ups (idle + 4 stride phases of walk and run); needs rendering
//                     (use -RenderOffscreen for no window, never -nullrhi)
// Reports: Saved/QA/FactoryQA/<timestamp>/report.json, report.txt and leg_*.png. Summary lines in the log start with "[FactoryQA]".
// Tuning: [/Script/<Module>.FactoryQASubsystem] in DefaultGame.ini (Config properties below).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FactoryQASubsystem.generated.h"

class ACharacter;
class ASceneCapture2D;
class UTextureRenderTarget2D;
class UPrimitiveComponent;
class UFactoryGaitProbeComponent;

UCLASS(Config = Game)
class UFactoryQASubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	/** Front, side and back close-ups of a character's legs, saved as leg_<label>_<view>.png. */
	void CaptureLegViews(AActor* Subject, const FString& Label);

	/** True if this character may take leg shots: the player always, plus up to MaxShotCharacters-1 other characters (first come). */
	bool ClaimShotSlot(AActor* Subject);

	/** Writes report.json / report.txt now (also done periodically and at shutdown). */
	void WriteReport();

	// ---------------- Tuning ----------------
	/** Seconds after the world starts before the static collision scan (lets runtime level builders finish). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float AuditDelaySeconds = 5.0f;
	/** Props whose lowest point is higher than this above the player's feet are out of reach and skipped. */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float ReachHeightCm = 230.0f;
	/** Props smaller than this (largest bounds edge) are skipped. */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float MinPropSizeCm = 12.0f;
	/** Props larger than this in every horizontal edge are treated as level shells for the inside-prop test. */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float ShellSizeCm = 800.0f;
	/** Props thinner than this in their smallest dimension (quads, decal-like rings, rugs) are walk-over by nature and skipped. */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float FlatThicknessCm = 3.0f;
	/** Character capsules are shrunk by this before overlap tests, so resting contact doesn't count. */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float PenetrationShrinkCm = 3.0f;
	/** Gait-probe every character (true) or only player-controlled ones (false). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") bool bGaitAllCharacters = true;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") int32 ShotWidth = 960;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") int32 ShotHeight = 960;
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float ShotFOV = 40.0f;
	/** Characters that get leg shots, the player included (crowd games: raise it so several NPCs are photographed). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") int32 MaxShotCharacters = 4;
	/** Leg-shot lighting: 0 = the game's own lighting (dark games give silhouettes), 1 = unlit base colour (default: shape and texture always readable), 2 = game lighting plus a QA key light at the camera. */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") int32 ShotLightMode = 1;
	/** Key-light intensity for ShotLightMode 2 (candela). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float ShotKeyLightCd = 40.0f;
	/** Exposure bias applied to leg shots (manual exposure, EV). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float ShotExposureBias = 0.0f;
	/** Leg shots show only the character, what it carries and the floor under it (true), or the full scene with occluders hidden (false). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") bool bShotIsolateSubject = true;
	/** Skip non-blocking visual meshes on actors that also own an overlap trigger (pickup bodies, markers, beacons). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") bool bSkipTriggerOwnerVisuals = true;
	/** Seconds between periodic report writes (crash safety). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") float ReportInterval = 15.0f;
	/** Actor or component tag that exempts a primitive from the solidity rules (decals, light shafts, thin cables). */
	UPROPERTY(Config, EditAnywhere, Category = "Tuning") FName NoCollideTag = TEXT("NoCollide");

private:
	struct FPropIssue { FString Owner; FString Component; FString Mesh; FString Problems; FVector Center; FVector Extent; };
	struct FPenEvent { FString Character; FString Other; FString Kind; int32 Frames = 0; float FirstTime = 0.0f; FVector Where = FVector::ZeroVector; };

	void RunCollisionAudit();
	void TickPenetration(float DeltaTime);
	void EnsureProbes();
	bool IsExempt(const UPrimitiveComponent* C) const;
	float PlayerFeetZ() const;
	FString OutDir() const;

	bool bAudit = false, bPen = false, bGait = false, bShots = false;
	bool bAudited = false;
	float Elapsed = 0.0f;
	float SinceReport = 0.0f;
	float SinceProbeScan = 0.0f;
	FString RunStamp;

	int32 PropsChecked = 0;
	int32 TriggersSkipped = 0;
	int32 FlatSkipped = 0;
	int32 VfxSkipped = 0;
	TArray<FString> TriggerVisualsSkipped;
	TSet<TWeakObjectPtr<AActor>> ShotSubjects;
	TArray<FPropIssue> PropIssues;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> NonSolid;
	TMap<FString, FPenEvent> PenEvents;
	int32 ShotCount = 0;

	UPROPERTY(Transient) TArray<TObjectPtr<UFactoryGaitProbeComponent>> Probes;
	UPROPERTY(Transient) TObjectPtr<ASceneCapture2D> Capture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> CaptureRT;
};
