// FactoryQA (design guide C3 gait gate): measures leg quality on a walking skeletal character.
// Drop-in: no game code needed; UFactoryQASubsystem attaches it when -GaitLog / -GaitShots / -FactoryQA is on the command line.
//
// Per frame, for each leg, it measures:
//   toe-in  : signed yaw of the foot->toe vector (positive = toe points to the midline), against the direction of travel
//             when moving (the clinical foot progression angle) and against the body's own forward (hip line) when standing
//   step width: sideways distance between the ankles across the travel line (scissor / catwalk steps read as "feet inward")
//   knee    : offset of the knee out of the leg's own plane (hip, ankle, foot direction); positive = bowed outward.
//             A bent knee over a turned-out foot is NOT counted (that is toe-out, measured separately); only knees that
//             leave the line the foot points along are. The plain front-view offset is reported too (kneeLat).
//   slide   : horizontal drift of a planted foot during one ground contact
//   contact : toe height above the floor while planted, relative to the standing baseline (float > 0, sink < 0)
// Samples are grouped into idle / walk / run bands; p50/p95 per band are judged against the thresholds.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FactoryGaitProbeComponent.generated.h"

class USkeletalMeshComponent;
class UFactoryQASubsystem;

UENUM()
enum class EFactoryGaitBand : uint8 { None, Idle, Walk, Run };

struct FFactoryGaitLegStats
{
	TArray<float> ToeIn;
	TArray<float> Knee;
	TArray<float> KneeLat;
	TArray<float> ToeRaw;
	TArray<float> StepWidth; // per character (stored on the left leg's stats)
	TArray<float> Slide;
	TArray<float> Contact;
};

UCLASS(ClassGroup = (FactoryQA), meta = (BlueprintSpawnableComponent))
class UFactoryGaitProbeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UFactoryGaitProbeComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** JSON object describing this character's gait (bands, percentiles, verdicts). */
	FString BuildSummaryJson() const;
	/** One log line per band with PASS/FAIL. */
	void LogSummary() const;
	bool IsValidProbe() const { return bResolved; }
	bool Passed() const;

	// ---------------- Tuning (defaults match design guide C3) ----------------
	UPROPERTY(EditAnywhere, Category = "Tuning|Bands") float IdleMaxSpeed = 10.0f;
	UPROPERTY(EditAnywhere, Category = "Tuning|Bands") float WalkMinSpeed = 60.0f;
	UPROPERTY(EditAnywhere, Category = "Tuning|Bands") float RunMinSpeed = 330.0f;
	/** Ignore samples while turning faster than this (deg/s): foot angles are only meaningful going straight. */
	UPROPERTY(EditAnywhere, Category = "Tuning|Bands") float MaxYawRate = 45.0f;
	/** Seconds a band must hold before samples count. */
	UPROPERTY(EditAnywhere, Category = "Tuning|Bands") float BandSettleTime = 0.4f;

	UPROPERTY(EditAnywhere, Category = "Tuning|Thresholds") float ToeInMaxDeg = 8.0f;
	UPROPERTY(EditAnywhere, Category = "Tuning|Thresholds") float ToeOutMaxDeg = 15.0f;
	/** Standing toe-out limit (natural stances turn out more than walking feet). Default = ToeOutMaxDeg; confirm per project. */
	UPROPERTY(EditAnywhere, Category = "Tuning|Thresholds") float IdleToeOutMaxDeg = 15.0f;
	UPROPERTY(EditAnywhere, Category = "Tuning|Thresholds") float KneeOutMaxCm = 3.0f;
	UPROPERTY(EditAnywhere, Category = "Tuning|Thresholds") float KneeInMaxCm = 3.0f;
	UPROPERTY(EditAnywhere, Category = "Tuning|Thresholds") float SlideMaxCm = 2.0f;
	UPROPERTY(EditAnywhere, Category = "Tuning|Thresholds") float ContactToleranceCm = 1.5f;
	/** Narrowest allowed step width while walking/running, p05 of the ankle-to-ankle distance across the travel line (cm).
	 *  0 = feet may not cross the midline. Calibrate per project; human walking is roughly 5-15 cm. */
	UPROPERTY(EditAnywhere, Category = "Tuning|Thresholds") float MinStepWidthCm = 0.0f;

	/** A foot counts as planted when it moves slower than this horizontally (cm/s)... */
	UPROPERTY(EditAnywhere, Category = "Tuning|Contact") float PlantSpeed = 20.0f;
	/** ...and its toe is within this height of the standing baseline (cm). */
	UPROPERTY(EditAnywhere, Category = "Tuning|Contact") float PlantHeightMargin = 5.0f;
	/** Toe angles are only measured while the foot is on the ground (stance), where in/out-toeing is visible and the angle is stable. */
	UPROPERTY(EditAnywhere, Category = "Tuning|Contact") bool bToeStanceOnly = true;
	/** ...and only when the foot->toe vector is at least this flat (horizontal length / 3D length); a foot pointing down at push-off has no meaningful yaw. */
	UPROPERTY(EditAnywhere, Category = "Tuning|Contact") float ToeMinFlatRatio = 0.5f;
	/** Measure toe angles relative to the skeleton's bind pose (true) or raw (false, default). Calibration 2026-11-07: Rocketbox
	 *  bind poses stand with the feet turned out 14-20 deg (varies per avatar) while the same idle clip gives identical raw angles
	 *  on every avatar, so the raw foot->toe direction is the true one. The bind-pose yaw is logged either way. */
	UPROPERTY(EditAnywhere, Category = "Tuning|Contact") bool bToeRelativeToBindPose = false;
	/** Standing (idle band): knees only count while the leg is at least this extended (hip-ankle / thigh+shin), so kneels and crouches are skipped.
	 *  Walking and running: knees count through the whole stride, because retarget knee flare shows in the swing. */
	UPROPERTY(EditAnywhere, Category = "Tuning|Contact") float KneeMinExtension = 0.8f;
	/** Toe height above the floor when standing, used until idle samples calibrate it. */
	UPROPERTY(EditAnywhere, Category = "Tuning|Contact") float DefaultSoleOffsetCm = 4.0f;

	/** Optional explicit bone names (empty = auto-detect UE mannequin, UAL, Rocketbox, Mixamo and common names). */
	UPROPERTY(EditAnywhere, Category = "Tuning|Bones") TArray<FName> LeftLegBones;  // thigh, calf, foot, toe
	UPROPERTY(EditAnywhere, Category = "Tuning|Bones") TArray<FName> RightLegBones; // thigh, calf, foot, toe

	// ---------------- Shots ----------------
	UPROPERTY(EditAnywhere, Category = "Tuning|Shots") bool bTakeShots = false;
	UPROPERTY(EditAnywhere, Category = "Tuning|Shots") int32 ShotPhasesPerBand = 4;
	/** If no foot contact is detected after this long in a band, shots fall back to a timed schedule. */
	UPROPERTY(EditAnywhere, Category = "Tuning|Shots") float ShotFallbackSeconds = 1.5f;
	/** Stride period assumed by the timed fallback when none was measured (s). */
	UPROPERTY(EditAnywhere, Category = "Tuning|Shots") float FallbackStridePeriod = 0.9f;

private:
	struct FLeg
	{
		FName Thigh, Calf, Foot, Toe;
		bool bPlanted = false;
		FVector PlantStart = FVector::ZeroVector;
		float PlantMaxDrift = 0.0f;
		FVector PrevToe = FVector::ZeroVector;
		float LastContactTime = -1.0f;
		float Period = 0.0f;
		float BindYaw = 0.0f;
		FVector LastFootDir = FVector::ZeroVector; // foot->toe yaw vs actor forward in the bind pose (outward-positive like toe-out)
	};

	bool ResolveBones();
	FName FindBone(const TArray<FName>& Exact, const TArray<FString>& PartKeys, bool bLeft) const;
	void SampleLeg(FLeg& Leg, bool bLeft, float Dt, const FVector& Fwd, const FVector& Right, EFactoryGaitBand Band);
	float FloorHeightBelow(const FVector& P) const;
	void HandleShots(float Now, EFactoryGaitBand Band, bool bLeftContactNow);

	UPROPERTY(Transient) TObjectPtr<USkeletalMeshComponent> Mesh;

	FLeg Legs[2];
	TMap<EFactoryGaitBand, FFactoryGaitLegStats> Stats[2];
	TArray<float> IdleContact;
	EFactoryGaitBand CurrentBand = EFactoryGaitBand::None;
	float BandTime = 0.0f;
	FVector PrevLocation = FVector::ZeroVector;
	float PrevYaw = 0.0f;
	bool bHavePrev = false;
	bool bResolved = false;
	float SoleBaseline = 0.0f;

	// Shot scheduling per band: phase times relative to the last left contact.
	TSet<EFactoryGaitBand> ShotBandsDone;
	TArray<float> PendingShotTimes;
	EFactoryGaitBand PendingShotBand = EFactoryGaitBand::None;
	int32 PendingShotIndex = 0;
	float IdleHeldTime = 0.0f;
	float BandCountedTime = 0.0f;
	FVector PrevTravelLoc = FVector::ZeroVector;
	FVector TravelDir = FVector::ZeroVector;
};
