// Copyright On the Ropes. All Rights Reserved.

#include "Mech/OTRMechRigComponent.h"
#include "Feedback/OTRFeedback.h"
#include "Data/OTRTypes.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "OntheRopes.h"

namespace
{
	float Smooth01(float T)
	{
		T = FMath::Clamp(T, 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	FVector LerpV(const FVector& A, const FVector& B, float K) { return A + (B - A) * K; }
	float LerpF(float A, float B, float K) { return A + (B - A) * K; }
}

UOTRMechRigComponent::UOTRMechRigComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
	UpperArmC.SetNum(2);
	ForearmC.SetNum(2);
	FistC.SetNum(2);
	ThighC.SetNum(2);
	ShinC.SetNum(2);
	FootC.SetNum(2);
	for (int32 I = 0; I < 2; ++I)
	{
		Plant[I] = StepFrom[I] = StepTo[I] = HandWorld[I] = ShoulderWorld[I] = FootWorld[I] = FVector::ZeroVector;
	}
}

UStaticMeshComponent* UOTRMechRigComponent::MakePart(const TCHAR* Part, const TCHAR* Suffix)
{
	AActor* Owner = GetOwner();
	const FName MeshName(*FString::Printf(TEXT("%s%s"), *Look.Prefix.ToString(), Part));
	UStaticMesh* Mesh = OTRFeedback::FindMesh(this, MeshName);
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(Owner, *FString::Printf(TEXT("Rig_%s%s"), Part, Suffix));
	C->SetStaticMesh(Mesh);
	C->SetupAttachment(this);
	C->SetUsingAbsoluteLocation(true);
	C->SetUsingAbsoluteRotation(true);
	C->SetUsingAbsoluteScale(true);
	C->SetMobility(EComponentMobility::Movable);
	// The capsule is the body's collision; parts are visuals riding on it.
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCanEverAffectNavigation(false);
	C->SetGenerateOverlapEvents(false);
	C->ComponentTags.Add(OTR::NoCollideTag);
	C->SetCastShadow(true);
	C->bCastDynamicShadow = true;
	C->RegisterComponent();
	OTRFeedback::ApplyMechMaterials(Owner, C, Look.Paint, Look.Trim, PaintMID, TrimMID, GlowMID, Look.PaintScalars, MetalMID, Look.MetalBrightness);
	return C;
}

void UOTRMechRigComponent::Build(const FOTRMechLook& InLook)
{
	if (bBuilt)
	{
		return;
	}
	Look = InLook;
	bBuilt = true;
	PelvisC = MakePart(TEXT("Pelvis"), TEXT(""));
	TorsoC = MakePart(TEXT("Torso"), TEXT(""));
	HeadC = MakePart(TEXT("Head"), TEXT(""));
	const TCHAR* Sides[2] = { TEXT("_L"), TEXT("_R") };
	for (int32 I = 0; I < 2; ++I)
	{
		UpperArmC[I] = MakePart(TEXT("UpperArm"), Sides[I]);
		ForearmC[I] = MakePart(TEXT("Forearm"), Sides[I]);
		FistC[I] = MakePart(TEXT("Fist"), Sides[I]);
		ThighC[I] = MakePart(TEXT("Thigh"), Sides[I]);
		ShinC[I] = MakePart(TEXT("Shin"), Sides[I]);
		FootC[I] = MakePart(TEXT("Foot"), Sides[I]);
	}
	if (GlowMID)
	{
		GlowMID->SetVectorParameterValue(TEXT("Color"), Look.Glow);
		GlowMID->SetScalarParameterValue(TEXT("Emissive"), FMath::Min(Look.GlowEmissive, 3.5f));
	}
	ResetFeet();
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Mech rig %s built (meshes %s) paint %s trim %s"), *Look.Prefix.ToString(), PelvisC && PelvisC->GetStaticMesh() ? TEXT("ok") : TEXT("MISSING"), *Look.Paint.ToString(), *Look.Trim.ToString());
}

TArray<TWeakObjectPtr<UOTRMechRigComponent>> UOTRMechRigComponent::AllRigs;

void UOTRMechRigComponent::BeginPlay()
{
	Super::BeginPlay();
	AllRigs.Add(this);
}

void UOTRMechRigComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	AllRigs.RemoveAll([this](const TWeakObjectPtr<UOTRMechRigComponent>& R) { return !R.IsValid() || R.Get() == this; });
	Super::EndPlay(Reason);
}

FVector UOTRMechRigComponent::PushOut(const FVector& Point, float Radius, int32 SelfHand, const FVector& OwnChest, float OwnChestRadius) const
{
	// Gather the shapes to stay out of: the other rigs' chests and heads (spheres), and, for the rig that gives way,
	// their fists (spheres) and forearms / upper arms (capsules A-B with radius).
	struct FCap { FVector A; FVector B; float R; };
	TArray<FCap, TInlineAllocator<16>> Caps;
	TArray<FVector4, TInlineAllocator<12>> Blockers;
	for (const TWeakObjectPtr<UOTRMechRigComponent>& W : AllRigs)
	{
		const UOTRMechRigComponent* Other = W.Get();
		if (!Other || !Other->bSpheresValid || Other->GetWorld() != GetWorld())
		{
			continue;
		}
		if (Other == this)
		{
			if (SelfHand >= 0)
			{
				Blockers.Add(Spheres[1 - SelfHand]);
			}
			continue;
		}
		// Fist against fist: only one rig gives way (the later-registered one), so the two never chase each other.
		const bool bYieldFists = AllRigs.IndexOfByPredicate([this](const TWeakObjectPtr<UOTRMechRigComponent>& R) { return R.Get() == this; }) > AllRigs.IndexOfByPredicate([Other](const TWeakObjectPtr<UOTRMechRigComponent>& R) { return R.Get() == Other; });
		for (int32 K = 0; K < 4; ++K)
		{
			if (K >= 2 || bYieldFists)
			{
				Blockers.Add(Other->Spheres[K]);
			}
		}
		if (bYieldFists)
		{
			for (int32 K = 0; K < 4; ++K)
			{
				Caps.Add({ FVector(Other->ArmCaps[K][0]), FVector(Other->ArmCaps[K][1]), (float)Other->ArmCaps[K][0].W });
			}
		}
	}
	if (OwnChestRadius > 0.0f)
	{
		Blockers.Add(FVector4(OwnChest, OwnChestRadius));
	}
	// This frame's already-solved left arm (only the right arm sees it, so the two never chase each other).
	for (const TPair<FVector4, FVector4>& SC : SelfCaps)
	{
		Caps.Add({ FVector(SC.Key), FVector(SC.Value), (float)SC.Key.W });
	}
	for (const FVector4& S : Blockers)
	{
		Caps.Add({ FVector(S), FVector(S), (float)S.W });
	}
	FVector Out = Point;
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		bool bMoved = false;
		for (const FCap& Cap : Caps)
		{
			const FVector C = FMath::ClosestPointOnSegment(Out, Cap.A, Cap.B);
			const float MinD = Radius + Cap.R;
			FVector D = Out - C;
			const float Len = D.Size();
			if (Len < MinD)
			{
				D = Len > KINDA_SMALL_NUMBER ? D / Len : FVector::UpVector;
				Out = C + D * MinD;
				bMoved = true;
			}
		}
		if (!bMoved)
		{
			break;
		}
	}
	return Out;
}

void UOTRMechRigComponent::SetPose(const FOTRMechPose& InPose, float InSharpness)
{
	Target = InPose;
	Sharpness = InSharpness;
	if (!bPoseInit)
	{
		Current = InPose;
		bPoseInit = true;
	}
}

void UOTRMechRigComponent::AddJolt(const FVector& RigSpaceKick)
{
	JoltVel += RigSpaceKick;
}

void UOTRMechRigComponent::AddTwist(float YawKick, float PitchKick, float RollKick, float HeadKick)
{
	TwistVel += FVector4(YawKick, PitchKick, RollKick, HeadKick);
}

void UOTRMechRigComponent::ResetFeet()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	const FVector Feet = Owner->GetActorLocation() - FVector(0.0f, 0.0f, FeetBelowRoot);
	const FRotator Yaw(0.0f, Owner->GetActorRotation().Yaw, 0.0f);
	const FVector Right = FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y);
	for (int32 I = 0; I < 2; ++I)
	{
		const float Side = I == 0 ? -1.0f : 1.0f;
		Plant[I] = Feet + Right * Side * Look.StanceHalfWidth;
		PlantYaw[I] = Yaw.Yaw;
		bStepping[I] = false;
		FootWorld[I] = Plant[I];
	}
	bFeetInit = true;
}

bool UOTRMechRigComponent::SolveTwoBone(const FVector& Root, const FVector& Goal, float L1, float L2, const FVector& Pole, FVector& OutJoint, FVector& OutEnd, float MinJointDegrees)
{
	FVector ToGoal = Goal - Root;
	float D = ToGoal.Size();
	const FVector Dir = D > KINDA_SMALL_NUMBER ? ToGoal / D : FVector(0.0f, 0.0f, -1.0f);
	const float MaxD = L1 + L2 - 0.5f;
	// The joint never closes tighter than MinJointDegrees (law of cosines), so a limb can't fold into itself.
	const float MinJoint = FMath::DegreesToRadians(FMath::Clamp(MinJointDegrees, 0.0f, 170.0f));
	const float MinD = FMath::Max(FMath::Abs(L1 - L2) + 1.0f, FMath::Sqrt(FMath::Max(L1 * L1 + L2 * L2 - 2.0f * L1 * L2 * FMath::Cos(MinJoint), 0.0f)));
	const bool bReached = D <= MaxD;
	D = FMath::Clamp(D, MinD, MaxD);
	OutEnd = Root + Dir * D;
	const float A = (L1 * L1 - L2 * L2 + D * D) / (2.0f * D);
	const float H = FMath::Sqrt(FMath::Max(L1 * L1 - A * A, 0.0f));
	FVector Bend = Pole - Dir * FVector::DotProduct(Pole, Dir);
	if (!Bend.Normalize())
	{
		Bend = FVector::CrossProduct(Dir, FVector::RightVector).GetSafeNormal();
	}
	OutJoint = Root + Dir * A + Bend * H;
	return bReached;
}

FQuat UOTRMechRigComponent::LimbRotation(const FVector& From, const FVector& To, const FVector& Forward)
{
	// Limb meshes hang along their local -Z from the pivot; local +X faces Forward (kneecap / front of the arm).
	const FVector Down = (To - From).GetSafeNormal();
	FVector Fwd = Forward - Down * FVector::DotProduct(Forward, Down);
	if (!Fwd.Normalize())
	{
		Fwd = FVector::CrossProduct(Down, FVector::RightVector).GetSafeNormal();
	}
	return FRotationMatrix::MakeFromZX(-Down, Fwd).ToQuat();
}

void UOTRMechRigComponent::Place(UStaticMeshComponent* C, const FVector& Location, const FQuat& Rotation) const
{
	if (C)
	{
		C->SetWorldLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
	}
}

void UOTRMechRigComponent::TickFeet(float Dt, const FTransform& Frame)
{
	const AActor* Owner = GetOwner();
	const FVector Feet = Frame.GetLocation();
	const FVector Right = Frame.GetUnitAxis(EAxis::Y);
	FVector Vel = Owner->GetVelocity();
	Vel.Z = 0.0f;
	const float Speed = Vel.Size();
	const float BodyYaw = Frame.Rotator().Yaw;
	FVector Goal[2];
	float Err[2];
	for (int32 I = 0; I < 2; ++I)
	{
		const float Side = I == 0 ? -1.0f : 1.0f;
		Goal[I] = Feet + Right * Side * Look.StanceHalfWidth + Vel * Look.StepLead;
		Goal[I].Z = Feet.Z;
		Err[I] = FVector::Dist2D(Plant[I], Goal[I]);
		// Far off (teleport, launch, knock-back): put the foot down under the body at once.
		if (Err[I] > SnapFootDistance)
		{
			Plant[I] = Goal[I];
			PlantYaw[I] = BodyYaw;
			bStepping[I] = false;
			Err[I] = 0.0f;
		}
		Plant[I].Z = Feet.Z;
	}

	// Advance a step in flight.
	for (int32 I = 0; I < 2; ++I)
	{
		if (!bStepping[I])
		{
			continue;
		}
		StepT[I] += Dt / FMath::Max(StepDuration[I], 0.05f);
		// Keep re-aiming the landing point while the body keeps moving.
		StepTo[I] = Goal[I] + Vel * StepDuration[I] * (1.0f - FMath::Min(StepT[I], 1.0f)) * 0.5f;
		if (StepT[I] >= 1.0f)
		{
			bStepping[I] = false;
			Plant[I] = StepTo[I];
			Plant[I].Z = Feet.Z;
			PlantYaw[I] = BodyYaw;
			const float Strength = FMath::Clamp(Look.HipHeight / 112.0f, 0.5f, 2.0f);
			JoltVel.Z -= FootfallKick * Strength;
			OnFootDown.Broadcast(Plant[I], Strength);
		}
	}

	// Start a new step: one foot at a time, the one farthest off target (alternating when both are off).
	if (!bStepping[0] && !bStepping[1])
	{
		const bool bMoving = Speed > 25.0f;
		IdleClock = bMoving ? 0.0f : IdleClock + Dt;
		const float Threshold = bMoving ? Look.StepThreshold : (IdleClock > IdleSettleDelay ? IdleSettleDistance : Look.StepThreshold);
		int32 Pick = INDEX_NONE;
		if (Err[0] > Threshold && Err[1] > Threshold)
		{
			Pick = LastStepped == 0 ? 1 : 0;
		}
		else if (Err[0] > Threshold)
		{
			Pick = 0;
		}
		else if (Err[1] > Threshold)
		{
			Pick = 1;
		}
		// Turning in place also needs the feet to follow the facing.
		if (Pick == INDEX_NONE)
		{
			for (int32 I = 0; I < 2; ++I)
			{
				if (FMath::Abs(FRotator::NormalizeAxis(PlantYaw[I] - BodyYaw)) > 28.0f)
				{
					Pick = I;
					break;
				}
			}
		}
		if (Pick != INDEX_NONE)
		{
			bStepping[Pick] = true;
			StepT[Pick] = 0.0f;
			StepFrom[Pick] = Plant[Pick];
			StepYawFrom[Pick] = PlantYaw[Pick];
			// Faster movement: quicker steps (never below 60 % of the authored time).
			StepDuration[Pick] = Look.StepSeconds * FMath::Clamp(1.0f - Speed / 900.0f, 0.6f, 1.0f);
			StepTo[Pick] = Goal[Pick];
			LastStepped = Pick;
			IdleClock = 0.0f;
		}
	}

	for (int32 I = 0; I < 2; ++I)
	{
		if (bStepping[I])
		{
			const float T = FMath::Clamp(StepT[I], 0.0f, 1.0f);
			FootWorld[I] = LerpV(StepFrom[I], StepTo[I], Smooth01(T)) + FVector(0.0f, 0.0f, Look.StepHeight * FMath::Sin(PI * T));
		}
		else
		{
			FootWorld[I] = Plant[I];
		}
	}
}

void UOTRMechRigComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	const AActor* Owner = GetOwner();
	if (!bBuilt || !Owner)
	{
		return;
	}
	const float Dt = FMath::Min(DeltaTime, 0.05f);
	Clock += Dt;
	if (!bFeetInit)
	{
		ResetFeet();
	}

	// Ease the pose: hands and torso on under-damped springs (anticipation reads from the authored wind-up, the
	// overshoot and settle come from here); pelvis, crouch and head on a plain exponential ease.
	const float K = 1.0f - FMath::Exp(-Sharpness * Dt);
	{
		const float W = FMath::Max(Sharpness * SpringFrequencyScale, 1.0f);
		const int32 Sub = FMath::Clamp(FMath::CeilToInt(W * Dt / 0.5f), 1, 8);
		const float H = Dt / Sub;
		FVector* Hands[2] = { &Current.HandL, &Current.HandR };
		const FVector Goals[2] = { Target.HandL, Target.HandR };
		FVector Ang(Current.TorsoPitch, Current.TorsoYaw, Current.TorsoRoll);
		const FVector AngGoal(Target.TorsoPitch, Target.TorsoYaw, Target.TorsoRoll);
		for (int32 S = 0; S < Sub; ++S)
		{
			for (int32 I = 0; I < 2; ++I)
			{
				HandVel[I] += (W * W * (Goals[I] - *Hands[I]) - 2.0f * HandDampingRatio * W * HandVel[I]) * H;
				*Hands[I] += HandVel[I] * H;
			}
			TorsoVel += (W * W * (AngGoal - Ang) - 2.0f * TorsoDampingRatio * W * TorsoVel) * H;
			Ang += TorsoVel * H;
		}
		Current.TorsoPitch = Ang.X;
		Current.TorsoYaw = Ang.Y;
		Current.TorsoRoll = Ang.Z;
	}
	Current.HeadPitch = LerpF(Current.HeadPitch, Target.HeadPitch, K);
	Current.PelvisOffset = LerpV(Current.PelvisOffset, Target.PelvisOffset, K);
	Current.Crouch = LerpF(Current.Crouch, Target.Crouch, K);

	// Jolt spring (linear) and twist spring (rotational hit reactions).
	JoltVel += (-JoltStiffness * Jolt - JoltDamping * JoltVel) * Dt;
	Jolt += JoltVel * Dt;
	TwistVel += (Twist * -TwistStiffness - TwistVel * TwistDamping) * Dt;
	Twist += TwistVel * Dt;

	// Boxer bounce: only while planted and standing still.
	{
		FVector V = Owner->GetVelocity();
		V.Z = 0.0f;
		const bool bStill = V.Size() < 25.0f && !bStepping[0] && !bStepping[1] && Collapse <= 0.0f;
		BounceWeight = LerpF(BounceWeight, bStill ? 1.0f : 0.0f, 1.0f - FMath::Exp(-4.0f * Dt));
		BounceClock += Dt * BounceHz * 2.0f * PI;
	}
	const float BounceBody = BounceAmplitude * BounceWeight * (0.5f + 0.5f * FMath::Sin(BounceClock));
	const float BounceHands = BounceAmplitude * 0.8f * BounceWeight * FMath::Sin(BounceClock - BounceHandLag);

	const FVector FeetLoc = Owner->GetActorLocation() - FVector(0.0f, 0.0f, FeetBelowRoot);
	const FRotator BodyRot(0.0f, Owner->GetActorRotation().Yaw, 0.0f);
	const FTransform Frame(BodyRot, FeetLoc);
	const FVector Fwd = Frame.GetUnitAxis(EAxis::X);
	const FVector Right = Frame.GetUnitAxis(EAxis::Y);
	const FVector Up = FVector::UpVector;

	if (Collapse <= 0.0f)
	{
		TickFeet(Dt, Frame);
	}

	// Pelvis: rises mid-step and shifts its weight over the planted foot.
	float StepPhase = 0.0f;
	float StanceSide = 0.0f;
	for (int32 I = 0; I < 2; ++I)
	{
		if (bStepping[I])
		{
			StepPhase = FMath::Sin(PI * FMath::Clamp(StepT[I], 0.0f, 1.0f));
			StanceSide = I == 0 ? 1.0f : -1.0f;
		}
	}
	const float Breath = FMath::Sin(Clock * IdleSwayHz * 2.0f * PI);
	// The swinging leg drags its hip forward (pelvis yaw); the chest counter-rotates; weight rides over the planted foot.
	const float SwingSide = -StanceSide;
	const FVector PelvisRig = FVector(0.0f, StanceSide * StepHipSway * StepPhase, Look.HipHeight - Current.Crouch - BounceBody + Look.StepBob * 0.6f * StepPhase + Breath * IdleSway * 0.4f)
		+ Current.PelvisOffset + Jolt * 0.35f;
	const FVector PelvisLoc = Frame.TransformPosition(PelvisRig);
	const FQuat PelvisRot = (BodyRot + FRotator(0.0f, SwingSide * StepPelvisYaw * StepPhase, StanceSide * 2.5f * StepPhase)).Quaternion();

	// Torso over the waist, then the head.
	const FVector WaistLoc = PelvisLoc + PelvisRot.RotateVector(FVector(0.0f, 0.0f, Look.WaistHeight));
	const float TorsoPitchNow = Current.TorsoPitch + Breath * 0.8f + BounceBody * 0.4f;
	const FRotator TorsoLocal(-TorsoPitchNow - Jolt.X * 0.25f + Twist.Y,
		Current.TorsoYaw - SwingSide * StepTorsoCounterYaw * StepPhase + Twist.X,
		Current.TorsoRoll + Jolt.Y * 0.2f - StanceSide * StepTorsoRoll * StepPhase + Twist.Z);
	const FQuat TorsoRot = BodyRot.Quaternion() * TorsoLocal.Quaternion();
	TorsoWorld = FTransform(TorsoRot, WaistLoc + Frame.TransformVector(Jolt * 0.65f));
	const FVector NeckLoc = TorsoWorld.TransformPosition(FVector(0.0f, 0.0f, Look.NeckHeight));
	// Head: keeps its eyes level and on the opponent while the chest yaws and rolls under it.
	const float ChestYaw = Current.TorsoYaw - SwingSide * StepTorsoCounterYaw * StepPhase;
	const float ChestRoll = Current.TorsoRoll - StanceSide * StepTorsoRoll * StepPhase;
	const FQuat HeadRot = TorsoRot * FRotator(-Current.HeadPitch + TorsoPitchNow * HeadStabilise + Twist.W,
		-Twist.X * 0.4f - ChestYaw * HeadYawStabilise, -ChestRoll * HeadYawStabilise).Quaternion();
	HeadWorld = NeckLoc;
	const float Size = FMath::Clamp(Look.HipHeight / 112.0f, 0.8f, 1.5f);

	// Arms: two-bone IK from the shoulders to the wrist targets, elbows out, down and back.
	SelfCaps.Reset();
	FVector HandRig[2] = { Current.HandL - FVector(0.0f, 0.0f, BounceHands + BounceBody), Current.HandR - FVector(0.0f, 0.0f, BounceHands * 0.85f + BounceBody) };
	// Hard hand limits (review 2), in rig space and from this frame's data, so they are exact and cannot jitter:
	// 1) each hand keeps to its own lane, a fist-width either side of the centre line (own fists never meet or cross);
	// 2) no hand reaches past the opponent's front (its guard), so a punch stops on it instead of passing through.
	float MaxReachX = BIG_NUMBER;
	for (const TWeakObjectPtr<UOTRMechRigComponent>& W : AllRigs)
	{
		const UOTRMechRigComponent* Other = W.Get();
		if (Other && Other != this && Other->GetOwner() && Other->GetWorld() == GetWorld())
		{
			const float OtherSize = FMath::Clamp(Other->Look.HipHeight / 112.0f, 0.8f, 1.5f);
			const FVector ToOther = Other->GetOwner()->GetActorLocation() - Owner->GetActorLocation();
			const float Ahead = FVector::DotProduct(ToOther, Fwd);
			if (Ahead > 0.0f && FMath::Abs(FVector::DotProduct(ToOther, Right)) < 150.0f)
			{
				MaxReachX = FMath::Min(MaxReachX, Ahead - OpponentFrontDepth * OtherSize - FistRadius * Size);
			}
		}
	}
	for (int32 I = 0; I < 2; ++I)
	{
		const float Side = I == 0 ? -1.0f : 1.0f;
		const float Lane = (FistRadius * 1.6f + HandLaneGap * 0.5f) * Size;
		if (Side * HandRig[I].Y < Lane)
		{
			HandRig[I].Y = Side * Lane;
		}
		HandRig[I].X = FMath::Min(HandRig[I].X, FMath::Max(MaxReachX, 45.0f));
	}
	FVector Elbow[2];
	FVector Wrist[2];
	float Reach[2] = { 0.0f, 0.0f };
	const float ArmLength = Look.UpperArmLength + Look.ForearmLength;
	for (int32 I = 0; I < 2; ++I)
	{
		const float Side = I == 0 ? -1.0f : 1.0f;
		const FVector ShoulderBase = TorsoWorld.TransformPosition(FVector(Look.Shoulder.X, Side * Look.Shoulder.Y, Look.Shoulder.Z));
		const FVector HandGoal = Frame.TransformPosition(HandRig[I]) + Frame.TransformVector(Jolt * 0.5f);
		// Shoulder: protracts (forward and in) as the arm extends, shrugs as the hand rises above it.
		Reach[I] = FMath::Clamp((FVector::Dist(ShoulderBase, HandGoal) - ArmLength * 0.55f) / (ArmLength * 0.45f), 0.0f, 1.0f);
		const float Raise = FMath::Clamp((HandGoal.Z - ShoulderBase.Z) / (40.0f * Size), 0.0f, 1.0f);
		ShoulderWorld[I] = ShoulderBase + TorsoRot.RotateVector(FVector(ShoulderReach * Smooth01(Reach[I]), -Side * ShoulderReach * 0.3f * Reach[I], ShoulderShrug * Raise) * Size);
		// Elbow: lifts and flares when the hand sweeps sideways (hooks) or crosses the midline.
		const float Sweep = FMath::Clamp(FMath::Abs(HandVel[I].Y) / FMath::Max(ElbowFlareSpeed, 1.0f), 0.0f, 1.0f);
		const float Cross = FMath::Clamp((15.0f - Side * HandRig[I].Y) / 40.0f, 0.0f, 1.0f);
		const float Flare = FMath::Clamp(FMath::Max(Sweep, Cross) * ElbowFlare, 0.0f, 1.0f);
		const FVector Pole = TorsoRot.RotateVector(FVector(-0.35f, Side * 0.85f, LerpF(-0.55f, 0.45f, Flare)));
		// Anti-clipping: find where the hand must go to keep the fist out of the other mech's fists, chest and head and
		// out of this mech's other fist (target push, then a check on the solved fist, which sits ahead of the wrist).
		FVector Goal = PushOut(HandGoal, FistRadius * Size, INDEX_NONE, FVector::ZeroVector, 0.0f);
		SolveTwoBone(ShoulderWorld[I], Goal, Look.UpperArmLength, Look.ForearmLength, Pole, Elbow[I], Wrist[I], MinElbowDegrees);
		for (int32 Pass = 0; Pass < 3; ++Pass)
		{
			// Check the solved fist and two points along the forearm; shift the hand by the deepest overlap and re-solve.
			const FVector Along = (Wrist[I] - Elbow[I]).GetSafeNormal();
			const FVector Samples[3] = { Wrist[I] + Along * FistRadius * Size * 0.6f, LerpV(Elbow[I], Wrist[I], 0.4f), LerpV(Elbow[I], Wrist[I], 0.75f) };
			const float Radii[3] = { FistRadius * Size + 4.0f, ForearmRadius * Size + 3.0f, ForearmRadius * Size + 3.0f };
			FVector Deepest = FVector::ZeroVector;
			for (int32 Smp = 0; Smp < 3; ++Smp)
			{
				const FVector Delta = PushOut(Samples[Smp], Radii[Smp], INDEX_NONE, FVector::ZeroVector, 0.0f) - Samples[Smp];
				if (Delta.SizeSquared() > Deepest.SizeSquared())
				{
					Deepest = Delta;
				}
			}
			if (Deepest.SizeSquared() < 1.0f)
			{
				break;
			}
			Goal = Wrist[I] + Deepest * 1.15f;
			SolveTwoBone(ShoulderWorld[I], Goal, Look.UpperArmLength, Look.ForearmLength, Pole, Elbow[I], Wrist[I], MinElbowDegrees);
		}
		// The correction eases in and out (the other rig's spheres are a frame old, so snapping to it vibrates).
		const FVector Want = Goal - HandGoal;
		const float Ease = 1.0f - FMath::Exp(-(Want.SizeSquared() > ClipOffset[I].SizeSquared() ? ClipEaseIn : ClipEaseOut) * Dt);
		ClipOffset[I] = bFistInit ? LerpV(ClipOffset[I], Want, Ease) : Want;
		// The lane holds for the final target too (the clip push must never move a hand across the centre line).
		FVector FinalRig = Frame.InverseTransformPosition(HandGoal + ClipOffset[I]);
		const float FinalLane = (FistRadius * 1.6f + HandLaneGap * 0.5f) * Size;
		if (Side * FinalRig.Y < FinalLane)
		{
			FinalRig.Y = Side * FinalLane;
		}
		// A hand tucked close to the shoulder moves straight forward to the tightest elbow angle (the IK's own clamp would
		// slide it along the shoulder line, inward, across its lane).
		FVector FinalGoal = Frame.TransformPosition(FinalRig);
		{
			const float L1 = Look.UpperArmLength, L2 = Look.ForearmLength;
			const float MinD = FMath::Sqrt(FMath::Max(L1 * L1 + L2 * L2 - 2.0f * L1 * L2 * FMath::Cos(FMath::DegreesToRadians(MinElbowDegrees)), 0.0f)) + 1.0f;
			const FVector D = FinalGoal - ShoulderWorld[I];
			if (D.Size() < MinD)
			{
				const float B = FVector::DotProduct(D, Fwd);
				FinalGoal += Fwd * (-B + FMath::Sqrt(FMath::Max(B * B - (D.SizeSquared() - MinD * MinD), 0.0f)));
			}
		}
		SolveTwoBone(ShoulderWorld[I], FinalGoal, Look.UpperArmLength, Look.ForearmLength, Pole, Elbow[I], Wrist[I], MinElbowDegrees);
		HandWorld[I] = Wrist[I];
		// Self-collision: the right arm keeps clear of the left arm as just solved (fist, forearm, upper arm).
		if (I == 0 && Collapse <= 0.0f)
		{
			const FVector Along = (Wrist[0] - Elbow[0]).GetSafeNormal();
			const FVector4 LeftFist(Wrist[0] + Along * FistRadius * Size * 0.6f, FistRadius * Size);
			SelfCaps.Add(TPair<FVector4, FVector4>(LeftFist, LeftFist));
			SelfCaps.Add(TPair<FVector4, FVector4>(FVector4(Elbow[0], ForearmRadius * Size), FVector4(Wrist[0], ForearmRadius * Size)));
			SelfCaps.Add(TPair<FVector4, FVector4>(FVector4(ShoulderWorld[0], UpperArmRadius * Size), FVector4(Elbow[0], UpperArmRadius * Size)));
		}
	}

	// Feet: heel-toe roll through a step. The ankle rises by the lever so the toe (or heel) stays on the ground line.
	float FootPitch[2] = { 0.0f, 0.0f };
	FVector Ankle[2] = { FootWorld[0] + Up * Look.AnkleHeight, FootWorld[1] + Up * Look.AnkleHeight };
	for (int32 I = 0; I < 2; ++I)
	{
		if (!bStepping[I] || Collapse > 0.0f)
		{
			continue;
		}
		const float T = FMath::Clamp(StepT[I], 0.0f, 1.0f);
		const float Split = FMath::Clamp(ToeOffSplit, 0.1f, 0.9f);
		FootPitch[I] = T < Split ? -ToeOffDegrees * FMath::Sin(PI * T / Split) : HeelStrikeDegrees * FMath::Sin(PI * (T - Split) / (1.0f - Split));
		const float Lever = (FootPitch[I] < 0.0f ? ToeLength : HeelLength) * Size;
		Ankle[I].Z += Lever * FMath::Sin(FMath::DegreesToRadians(FMath::Abs(FootPitch[I])));
	}

	// Legs: hips to the (world-locked) ankles, knees forward and a touch out, spreading as the body crouches.
	FVector Hip[2];
	FVector Knee[2];
	FVector AnkleSolved[2];
	FVector KneeDir[2];
	const float Spread = 0.12f + KneeSpreadPerCrouch * FMath::Clamp(Current.Crouch / 40.0f, 0.0f, 1.5f);
	for (int32 I = 0; I < 2; ++I)
	{
		const float Side = I == 0 ? -1.0f : 1.0f;
		Hip[I] = PelvisLoc + PelvisRot.RotateVector(FVector(0.0f, Side * Look.HipHalfWidth, 0.0f));
		KneeDir[I] = Fwd + Right * Side * Spread;
		SolveTwoBone(Hip[I], Ankle[I], Look.ThighLength, Look.ShinLength, KneeDir[I], Knee[I], AnkleSolved[I]);
	}

	// Optional KO collapse: everything rotates back around the heels.
	FQuat Fall = FQuat::Identity;
	FVector Pivot = FeetLoc - Fwd * 30.0f;
	if (Collapse > 0.0f)
	{
		const float Angle = FMath::DegreesToRadians(CollapseDegrees * Smooth01(Collapse));
		Fall = FQuat(Right, Angle);
		if (FVector::DotProduct(Fall.RotateVector(Up), Fwd) > 0.0f)
		{
			Fall = FQuat(Right, -Angle);
		}
	}
	auto P = [&Fall, &Pivot](const FVector& V) { return Pivot + Fall.RotateVector(V - Pivot); };
	auto R = [&Fall](const FQuat& Q) { return Fall * Q; };

	Place(PelvisC, P(PelvisLoc), R(PelvisRot));
	Place(TorsoC, P(TorsoWorld.GetLocation()), R(TorsoRot));
	Place(HeadC, P(NeckLoc), R(HeadRot));
	for (int32 I = 0; I < 2; ++I)
	{
		const float Side = I == 0 ? -1.0f : 1.0f;
		const FVector ElbowDir = (Elbow[I] - (ShoulderWorld[I] + Wrist[I]) * 0.5f).GetSafeNormal();
		Place(UpperArmC[I], P(ShoulderWorld[I]), R(LimbRotation(ShoulderWorld[I], Elbow[I], -ElbowDir)));
		Place(ForearmC[I], P(Elbow[I]), R(LimbRotation(Elbow[I], Wrist[I], -ElbowDir)));
		// Wrist: corkscrews (thumb turns in) near full extension, and trails the forearm a little.
		const FVector Along = (Wrist[I] - Elbow[I]).GetSafeNormal();
		const FVector UpHint = FMath::Abs(FVector::DotProduct(Along, Up)) > 0.92f ? Fwd : Up;
		const float Screw = -Side * WristCorkscrew * Smooth01(FMath::Clamp((Reach[I] - 0.5f) / 0.5f, 0.0f, 1.0f));
		const FQuat FistGoal = FRotationMatrix::MakeFromXZ(Along, UpHint).ToQuat() * FQuat(FVector::ForwardVector, FMath::DegreesToRadians(Screw));
		FistQ[I] = bFistInit ? FQuat::Slerp(FistQ[I], FistGoal, 1.0f - FMath::Exp(-WristFollow * Dt)).GetNormalized() : FistGoal;
		Place(FistC[I], P(Wrist[I]), R(FistQ[I]));
		Place(ThighC[I], P(Hip[I]), R(LimbRotation(Hip[I], Knee[I], KneeDir[I])));
		Place(ShinC[I], P(Knee[I]), R(LimbRotation(Knee[I], AnkleSolved[I], KneeDir[I])));
		float FootYaw = PlantYaw[I];
		if (bStepping[I])
		{
			FootYaw = StepYawFrom[I] + FRotator::NormalizeAxis(BodyRot.Yaw - StepYawFrom[I]) * Smooth01(StepT[I]);
		}
		Place(FootC[I], P(AnkleSolved[I]), R(FRotator(FootPitch[I], FootYaw, 0.0f).Quaternion()));
		HandWorld[I] = P(Wrist[I]);
		ShoulderWorld[I] = P(ShoulderWorld[I]);
	}
	HeadWorld = P(NeckLoc);
	bFistInit = true;

	// Publish this frame's collision spheres for the other rig (fist centres sit just past the wrist).
	for (int32 I = 0; I < 2; ++I)
	{
		const FVector Along = (Wrist[I] - Elbow[I]).GetSafeNormal();
		Spheres[I] = FVector4(P(Wrist[I] + Along * FistRadius * Size * 0.6f), FistRadius * Size);
		ArmCaps[I][0] = FVector4(P(Elbow[I]), ForearmRadius * Size);
		ArmCaps[I][1] = FVector4(P(Wrist[I]), ForearmRadius * Size);
		ArmCaps[2 + I][0] = FVector4(ShoulderWorld[I], UpperArmRadius * Size);
		ArmCaps[2 + I][1] = FVector4(P(Elbow[I]), UpperArmRadius * Size);
	}
	Spheres[2] = FVector4(P(TorsoWorld.TransformPosition(FVector(0.0f, 0.0f, Look.NeckHeight * ChestHeightShare))), ChestRadius * Size);
	Spheres[3] = FVector4(P(NeckLoc + Up * HeadRadius * Size), HeadRadius * Size);
	bSpheresValid = Collapse <= 0.0f;




}
