// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Data/OTRTuning.h"
#include "OTRMechRigComponent.generated.h"

class UStaticMeshComponent;
class UMaterialInstanceDynamic;

/**
 * Pose request for a mech (rig space: origin between the feet, X forward, Y right, Z up, cm).
 * The owner sets it every tick; the rig eases toward it (Sharpness) and solves the limbs.
 */
USTRUCT(BlueprintType)
struct FOTRMechPose
{
	GENERATED_BODY()

	/** Wrist targets in rig space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose") FVector HandL = FVector(55.0f, -35.0f, 175.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose") FVector HandR = FVector(55.0f, 35.0f, 175.0f);
	/** Torso rotation over the hips, degrees (pitch > 0 leans forward). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose") float TorsoPitch = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose") float TorsoYaw = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose") float TorsoRoll = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose") float HeadPitch = 0.0f;
	/** Pelvis offset in rig space (lean back into the ropes = negative X), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose") FVector PelvisOffset = FVector::ZeroVector;
	/** Knee bend: how far the pelvis sits below its rest height, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose") float Crouch = 0.0f;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FOTROnFootDown, const FVector& /*Location*/, float /*Strength*/);

/**
 * A code-built mech: rigid generated parts (pelvis, torso, head, upper arms, forearms, fists, thighs, shins, feet)
 * placed every tick in world space. Arms and legs use two-bone IK with fixed poles (elbows out and down, knees
 * forward), and the feet are world-locked between steps: a foot only moves during its own step arc, one foot at a
 * time, and always points along the body's facing. That is the C3 gait gate by construction (no slide, no toe-in,
 * no crossing steps, no bowed knees). A KO collapse rotates the whole assembly back around the heels.
 */
UCLASS(ClassGroup = (OntheRopes), meta = (BlueprintSpawnableComponent))
class ONTHEROPES_API UOTRMechRigComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UOTRMechRigComponent();

	/** Build the parts for this look (meshes SM_OTR_<Prefix><Part>). Call once, after the owner is placed. */
	void Build(const FOTRMechLook& InLook);

	void SetPose(const FOTRMechPose& InPose, float InSharpness);
	/** A jolt (hit reaction / impact) in rig space, cm. Springs back. */
	void AddJolt(const FVector& RigSpaceKick);
	/** Rotational hit reaction (degrees/s kick on torso yaw, pitch back, roll, head snap). Springs back. */
	void AddTwist(float YawKick, float PitchKick, float RollKick, float HeadKick = 0.0f);
	/** KO collapse 0..1 (falls back around the heels). */
	void SetCollapse(float Alpha) { Collapse = FMath::Clamp(Alpha, 0.0f, 1.0f); }
	/** Snap feet under the body (spawn, teleport). */
	void ResetFeet();

	FVector GetHandWorld(bool bRight) const { return bRight ? HandWorld[1] : HandWorld[0]; }
	FVector GetHeadWorld() const { return HeadWorld; }
	FTransform GetTorsoWorld() const { return TorsoWorld; }
	/** Shoulder pivots in world space (vents, steam). */
	FVector GetShoulderWorld(bool bRight) const { return bRight ? ShoulderWorld[1] : ShoulderWorld[0]; }
	bool IsStepping() const { return bStepping[0] || bStepping[1]; }
	UMaterialInstanceDynamic* GetGlowMID() const { return GlowMID; }
	UMaterialInstanceDynamic* GetPaintMID() const { return PaintMID; }
	const FOTRMechLook& GetLook() const { return Look; }
	/** Feet world positions (planted or in flight). */
	FVector GetFootWorld(int32 Index) const { return FootWorld[Index]; }

	FOTROnFootDown OnFootDown;

	/** Height of the rig origin below the owner's root (capsule half height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FeetBelowRoot = 130.0f;
	/** A foot farther than this from its target is moved at once (teleports, knock-backs), cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SnapFootDistance = 260.0f;
	/** Settle the feet when standing and off target by more than this, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IdleSettleDistance = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IdleSettleDelay = 0.35f;
	/** Jolt spring (stiffness, damping). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float JoltStiffness = 180.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float JoltDamping = 14.0f;
	/** Idle breathing sway amplitude (cm) and rate (Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IdleSway = 1.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float IdleSwayHz = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float CollapseDegrees = 82.0f;
	/** Hands and torso follow their targets on a spring (natural frequency = pose sharpness x this); damping ratio
	 *  under 1 gives the punch overshoot and follow-through. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float SpringFrequencyScale = 1.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float HandDampingRatio = 0.62f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float TorsoDampingRatio = 0.7f;
	/** Gait weight shift: hip sway toward the planted foot (cm), pelvis yaw with the swinging leg, torso counter-yaw (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float StepHipSway = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float StepPelvisYaw = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float StepTorsoCounterYaw = 5.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float StepTorsoRoll = 3.5f;
	/** Footfall compression: downward kick on the body when a foot lands (cm/s per strength). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float FootfallKick = 160.0f;
	/** Boxer bounce while standing: crouch bob (cm) at this rate (Hz); the guard hands bob a little after it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float BounceAmplitude = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float BounceHz = 1.7f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float BounceHandLag = 0.6f;
	/** The head stays level: share of the torso pitch it cancels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float HeadStabilise = 0.45f;
	/** Rotational hit-reaction spring (stiffness, damping). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float TwistStiffness = 160.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Motion") float TwistDamping = 11.0f;

	// Per-joint motion (review 1, "work on the movement of each joint"). Lengths in cm, scaled by the mech's size.
	/** Head yaw and roll held toward the opponent: share of the chest's yaw / roll it cancels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float HeadYawStabilise = 0.55f;
	/** Tightest elbow angle (between upper arm and forearm), degrees: the fist never folds into the biceps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float MinElbowDegrees = 62.0f;
	/** Anti-clipping (review 2): a fist is a sphere this big (cm, scaled by mech size); fists stay out of other fists,
	 *  the other mech's chest and head, and this mech's own chest. Hand targets are pushed out before the IK. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clipping") float FistRadius = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clipping") float ChestRadius = 48.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clipping") float HeadRadius = 26.0f;
	/** Forearm and upper-arm plating as capsules (radius, cm, scaled by mech size). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clipping") float ForearmRadius = 16.0f;
	/** Gap kept between this mech's two fists (cm, scaled): each hand stays in its own lane either side of the centre line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clipping") float HandLaneGap = 20.0f;
	/** How far in front of an opponent's centre its guard reaches (cm, scaled by its size): hands stop there. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clipping") float OpponentFrontDepth = 135.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clipping") float UpperArmRadius = 14.0f;
	/** Chest sphere centre above the waist (share of the neck height). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clipping") float ChestHeightShare = 0.55f;
	/** How fast the clip correction comes in (fast, so contact stops a fist) and lets go (slower, no flicker), 1/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clipping") float ClipEaseIn = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Clipping") float ClipEaseOut = 8.0f;
	/** Shoulders: protract toward a reaching hand, shrug when the hand rises above them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float ShoulderReach = 9.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float ShoulderShrug = 5.0f;
	/** Elbows lift and flare (hook shape) as the hand sweeps sideways or crosses the body's midline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float ElbowFlare = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float ElbowFlareSpeed = 450.0f;
	/** Wrists: the fist corkscrews near full extension and trails the forearm slightly (follow rate, 1/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float WristCorkscrew = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float WristFollow = 28.0f;
	/** Knees spread with the crouch (extra pole sideways per 40 cm of crouch). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float KneeSpreadPerCrouch = 0.25f;
	/** Feet: heel-toe roll through a step (toe-off then a heel strike), pivoting on the toe / heel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float ToeOffDegrees = 22.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float HeelStrikeDegrees = 14.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float ToeOffSplit = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float ToeLength = 24.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning|Joints") float HeelLength = 12.0f;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	UStaticMeshComponent* MakePart(const TCHAR* Part, const TCHAR* Suffix);
	void Place(UStaticMeshComponent* C, const FVector& Location, const FQuat& Rotation) const;
	void TickFeet(float Dt, const FTransform& Frame);
	static bool SolveTwoBone(const FVector& Root, const FVector& Target, float L1, float L2, const FVector& Pole, FVector& OutJoint, FVector& OutEnd, float MinJointDegrees = 0.0f);
	static FQuat LimbRotation(const FVector& From, const FVector& To, const FVector& Forward);

	FOTRMechLook Look;
	bool bBuilt = false;

	UPROPERTY() TObjectPtr<UStaticMeshComponent> PelvisC;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> TorsoC;
	UPROPERTY() TObjectPtr<UStaticMeshComponent> HeadC;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> UpperArmC;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ForearmC;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FistC;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ThighC;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> ShinC;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> FootC;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PaintMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> TrimMID;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlowMID;
	/** Per-mech bare-metal instance, only when Look.MetalBrightness >= 0. */
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MetalMID;

	FOTRMechPose Target;
	FOTRMechPose Current;
	float Sharpness = 12.0f;
	bool bPoseInit = false;

	FVector Plant[2];
	float PlantYaw[2] = { 0.0f, 0.0f };
	bool bStepping[2] = { false, false };
	float StepT[2] = { 0.0f, 0.0f };
	float StepDuration[2] = { 0.3f, 0.3f };
	FVector StepFrom[2];
	FVector StepTo[2];
	float StepYawFrom[2] = { 0.0f, 0.0f };
	int32 LastStepped = 1;
	float IdleClock = 0.0f;
	bool bFeetInit = false;

	FVector HandVel[2] = { FVector::ZeroVector, FVector::ZeroVector };
	FVector TorsoVel = FVector::ZeroVector;   // pitch, yaw, roll rates
	FVector4 Twist = FVector4(0, 0, 0, 0);    // yaw, pitch, roll, head
	FVector4 TwistVel = FVector4(0, 0, 0, 0);
	float BounceClock = 0.0f;
	float BounceWeight = 0.0f;
	FVector Jolt = FVector::ZeroVector;
	FVector JoltVel = FVector::ZeroVector;
	float Collapse = 0.0f;
	float Clock = 0.0f;
	FQuat FistQ[2] = { FQuat::Identity, FQuat::Identity };
	bool bFistInit = false;
	/** Collision spheres published each frame for other rigs: 0-1 fists, 2 chest, 3 head (centre xyz, radius w). */
	FVector4 Spheres[4];
	bool bSpheresValid = false;
	FVector ClipOffset[2] = { FVector::ZeroVector, FVector::ZeroVector };
	/** Arm capsules published for the other rig: 0-1 forearms (elbow to wrist), 2-3 upper arms (shoulder to elbow). */
	FVector4 ArmCaps[4][2];
	/** This frame's solved left arm (capsule ends, radius in W), seen only by the right arm's solve. */
	TArray<TPair<FVector4, FVector4>> SelfCaps;
	static TArray<TWeakObjectPtr<UOTRMechRigComponent>> AllRigs;
	FVector PushOut(const FVector& Point, float Radius, int32 SelfHand, const FVector& OwnChest, float OwnChestRadius) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	FVector HandWorld[2];
	FVector ShoulderWorld[2];
	FVector FootWorld[2];
	FVector HeadWorld = FVector::ZeroVector;
	FTransform TorsoWorld = FTransform::Identity;
};
