// FactoryQA gait probe (design guide C3). See header.

#include "FactoryGaitProbeComponent.h"
#include "FactoryQASubsystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

namespace FactoryGait
{
	static float Percentile(TArray<float> V, float P)
	{
		if (V.Num() == 0) { return 0.0f; }
		V.Sort();
		const float Idx = FMath::Clamp(P, 0.0f, 1.0f) * (V.Num() - 1);
		const int32 Lo = FMath::FloorToInt(Idx);
		const int32 Hi = FMath::Min(Lo + 1, V.Num() - 1);
		return FMath::Lerp(V[Lo], V[Hi], Idx - Lo);
	}

	static const TCHAR* BandName(EFactoryGaitBand B)
	{
		switch (B)
		{
		case EFactoryGaitBand::Idle: return TEXT("idle");
		case EFactoryGaitBand::Walk: return TEXT("walk");
		case EFactoryGaitBand::Run:  return TEXT("run");
		default:                     return TEXT("none");
		}
	}

	static bool IsSide(const FString& LowerName, bool bLeft)
	{
		const TCHAR* Word = bLeft ? TEXT("left") : TEXT("right");
		const TCHAR C = bLeft ? TEXT('l') : TEXT('r');
		if (LowerName.Contains(Word)) { return true; }
		const int32 N = LowerName.Len();
		// Suffixes: _l .l -l " l"; prefixes: l_ l. "l "; infix: " l " _l_
		if (N >= 2 && LowerName[N - 1] == C && (LowerName[N - 2] == TEXT('_') || LowerName[N - 2] == TEXT('.') || LowerName[N - 2] == TEXT('-') || LowerName[N - 2] == TEXT(' '))) { return true; }
		if (N >= 2 && LowerName[0] == C && (LowerName[1] == TEXT('_') || LowerName[1] == TEXT('.') || LowerName[1] == TEXT(' '))) { return true; }
		const FString Infix1 = FString::Printf(TEXT(" %c "), C);
		const FString Infix2 = FString::Printf(TEXT("_%c_"), C);
		const FString Infix3 = FString::Printf(TEXT("-%c-"), C);
		return LowerName.Contains(Infix1) || LowerName.Contains(Infix2) || LowerName.Contains(Infix3);
	}
}

UFactoryGaitProbeComponent::UFactoryGaitProbeComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork; // after animation has posed the mesh
}

void UFactoryGaitProbeComponent::BeginPlay()
{
	Super::BeginPlay();
	SoleBaseline = DefaultSoleOffsetCm;
	if (const ACharacter* Char = Cast<ACharacter>(GetOwner()))
	{
		Mesh = Char->GetMesh();
	}
	if (!Mesh)
	{
		Mesh = GetOwner() ? GetOwner()->FindComponentByClass<USkeletalMeshComponent>() : nullptr;
	}
	bResolved = Mesh && ResolveBones();
	if (!bResolved)
	{
		SetComponentTickEnabled(false);
	}
}

FName UFactoryGaitProbeComponent::FindBone(const TArray<FName>& Exact, const TArray<FString>& PartKeys, bool bLeft) const
{
	for (const FName& N : Exact)
	{
		if (Mesh->GetBoneIndex(N) != INDEX_NONE) { return N; }
	}
	// Fuzzy: first bone whose lower-case name contains a part key and a side marker, skipping twist / roll / IK helper bones.
	const int32 Num = Mesh->GetNumBones();
	for (const FString& Key : PartKeys)
	{
		for (int32 i = 0; i < Num; ++i)
		{
			const FName Bone = Mesh->GetBoneName(i);
			const FString L = Bone.ToString().ToLower();
			if (!L.Contains(Key) || L.Contains(TEXT("twist")) || L.Contains(TEXT("roll")) || L.Contains(TEXT("ik_")) || L.Contains(TEXT("_ik")) || L.Contains(TEXT("vb ")))
			{
				continue;
			}
			if (FactoryGait::IsSide(L, bLeft)) { return Bone; }
		}
	}
	return NAME_None;
}

bool UFactoryGaitProbeComponent::ResolveBones()
{
	for (int32 Side = 0; Side < 2; ++Side)
	{
		const bool bLeft = Side == 0;
		const TArray<FName>& Override = bLeft ? LeftLegBones : RightLegBones;
		FLeg& Leg = Legs[Side];
		if (Override.Num() == 4)
		{
			Leg.Thigh = Override[0]; Leg.Calf = Override[1]; Leg.Foot = Override[2]; Leg.Toe = Override[3];
		}
		else
		{
			const TCHAR* S = bLeft ? TEXT("l") : TEXT("r");
			const TCHAR* SS = bLeft ? TEXT("L") : TEXT("R");
			const TCHAR* W = bLeft ? TEXT("Left") : TEXT("Right");
			Leg.Thigh = FindBone({ FName(*FString::Printf(TEXT("thigh_%s"), S)), FName(*FString::Printf(TEXT("Bip01 %s Thigh"), SS)), FName(*FString::Printf(TEXT("Bip01-%s-Thigh"), SS)), FName(*FString::Printf(TEXT("Bip001 %s Thigh"), SS)), FName(*FString::Printf(TEXT("%sUpLeg"), W)), FName(*FString::Printf(TEXT("mixamorig:%sUpLeg"), W)), FName(*FString::Printf(TEXT("UpperLeg.%s"), SS)), FName(*FString::Printf(TEXT("upperleg.%s"), S)) },
				{ TEXT("thigh"), TEXT("upleg"), TEXT("upperleg"), TEXT("upper_leg"), TEXT("femur") }, bLeft);
			Leg.Calf = FindBone({ FName(*FString::Printf(TEXT("calf_%s"), S)), FName(*FString::Printf(TEXT("Bip01 %s Calf"), SS)), FName(*FString::Printf(TEXT("Bip01-%s-Calf"), SS)), FName(*FString::Printf(TEXT("Bip001 %s Calf"), SS)), FName(*FString::Printf(TEXT("%sLeg"), W)), FName(*FString::Printf(TEXT("mixamorig:%sLeg"), W)), FName(*FString::Printf(TEXT("LowerLeg.%s"), SS)), FName(*FString::Printf(TEXT("lowerleg.%s"), S)) },
				{ TEXT("calf"), TEXT("shin"), TEXT("lowerleg"), TEXT("lower_leg"), TEXT("knee") }, bLeft);
			Leg.Foot = FindBone({ FName(*FString::Printf(TEXT("foot_%s"), S)), FName(*FString::Printf(TEXT("Bip01 %s Foot"), SS)), FName(*FString::Printf(TEXT("Bip01-%s-Foot"), SS)), FName(*FString::Printf(TEXT("Bip001 %s Foot"), SS)), FName(*FString::Printf(TEXT("%sFoot"), W)), FName(*FString::Printf(TEXT("mixamorig:%sFoot"), W)), FName(*FString::Printf(TEXT("Foot.%s"), SS)), FName(*FString::Printf(TEXT("foot.%s"), S)) },
				{ TEXT("foot"), TEXT("ankle") }, bLeft);
			Leg.Toe = FindBone({ FName(*FString::Printf(TEXT("ball_%s"), S)), FName(*FString::Printf(TEXT("Bip01 %s Toe0"), SS)), FName(*FString::Printf(TEXT("Bip01-%s-Toe0"), SS)), FName(*FString::Printf(TEXT("Bip001 %s Toe0"), SS)), FName(*FString::Printf(TEXT("%sToeBase"), W)), FName(*FString::Printf(TEXT("mixamorig:%sToeBase"), W)), FName(*FString::Printf(TEXT("Toes.%s"), SS)), FName(*FString::Printf(TEXT("toes.%s"), S)) },
				{ TEXT("ball"), TEXT("toebase"), TEXT("toe") }, bLeft);
		}
		if (Leg.Thigh.IsNone() || Leg.Calf.IsNone() || Leg.Foot.IsNone() || Leg.Toe.IsNone())
		{
			FString All;
			for (int32 i = 0; i < Mesh->GetNumBones(); ++i) { All += Mesh->GetBoneName(i).ToString() + TEXT(", "); }
			UE_LOG(LogTemp, Warning, TEXT("[FactoryQA] Gait: could not resolve %s leg bones on %s (thigh=%s calf=%s foot=%s toe=%s). Set LeftLegBones/RightLegBones. Bones: %s"),
				bLeft ? TEXT("left") : TEXT("right"), *GetNameSafe(GetOwner()), *Leg.Thigh.ToString(), *Leg.Calf.ToString(), *Leg.Foot.ToString(), *Leg.Toe.ToString(), *All);
			return false;
		}
	}
	// Bind-pose foot->toe yaw: the rig's own toe offset with the feet straight.
	if (const USkinnedAsset* Asset = Mesh->GetSkinnedAsset())
	{
		const FReferenceSkeleton& Ref = Asset->GetRefSkeleton();
		auto RefCS = [&Ref](FName B)
		{
			FTransform T = FTransform::Identity;
			for (int32 i = Ref.FindBoneIndex(B); i != INDEX_NONE; i = Ref.GetParentIndex(i)) { T = T * Ref.GetRefBonePose()[i]; }
			return T.GetLocation();
		};
		const AActor* Owner = GetOwner();
		const FVector Fwd = FVector(Owner->GetActorForwardVector().X, Owner->GetActorForwardVector().Y, 0).GetSafeNormal();
		const FVector Right = FVector(Owner->GetActorRightVector().X, Owner->GetActorRightVector().Y, 0).GetSafeNormal();
		for (int32 Side = 0; Side < 2; ++Side)
		{
			FVector Dir = Mesh->GetComponentQuat().RotateVector(RefCS(Legs[Side].Toe) - RefCS(Legs[Side].Foot));
			Dir.Z = 0.0f;
			if (Dir.Normalize())
			{
				const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Dir, Right), FVector::DotProduct(Dir, Fwd)));
				Legs[Side].BindYaw = Side == 0 ? -Yaw : Yaw; // outward-positive
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("[FactoryQA] Gait probe on %s: L[%s %s %s %s] R[%s %s %s %s] bind-pose toe-out L %.1f R %.1f deg (%s)"), *GetNameSafe(GetOwner()),
		*Legs[0].Thigh.ToString(), *Legs[0].Calf.ToString(), *Legs[0].Foot.ToString(), *Legs[0].Toe.ToString(),
		*Legs[1].Thigh.ToString(), *Legs[1].Calf.ToString(), *Legs[1].Foot.ToString(), *Legs[1].Toe.ToString(),
		Legs[0].BindYaw, Legs[1].BindYaw, bToeRelativeToBindPose ? TEXT("toe angles relative to it") : TEXT("raw toe angles"));
	return true;
}

float UFactoryGaitProbeComponent::FloorHeightBelow(const FVector& P) const
{
	FHitResult Hit;
	FCollisionQueryParams Q(SCENE_QUERY_STAT(FactoryGaitFloor), false, GetOwner());
	TArray<AActor*> Attached;
	GetOwner()->GetAttachedActors(Attached, true, true);
	Q.AddIgnoredActors(Attached);
	if (GetWorld()->LineTraceSingleByChannel(Hit, P + FVector(0, 0, 60), P - FVector(0, 0, 200), ECC_Visibility, Q))
	{
		return P.Z - Hit.ImpactPoint.Z;
	}
	return TNumericLimits<float>::Max();
}

void UFactoryGaitProbeComponent::SampleLeg(FLeg& Leg, bool bLeft, float Dt, const FVector& Fwd, const FVector& Right, EFactoryGaitBand Band)
{
	const FVector H = Mesh->GetSocketLocation(Leg.Thigh);
	const FVector K = Mesh->GetSocketLocation(Leg.Calf);
	const FVector A = Mesh->GetSocketLocation(Leg.Foot);
	const FVector T = Mesh->GetSocketLocation(Leg.Toe);

	// Toe yaw vs facing; positive = toward the midline (toe-in).
	const FVector Toe3D = T - A;
	FVector ToeDir = Toe3D;
	ToeDir.Z = 0.0f;
	float ToeIn = 0.0f;
	float ToeInRaw = 0.0f;
	bool bToeValid = ToeDir.SizeSquared() > 1.0f && ToeDir.Size() >= ToeMinFlatRatio * Toe3D.Size();
	if (bToeValid)
	{
		ToeDir.Normalize();
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(ToeDir, Right), FVector::DotProduct(ToeDir, Fwd)));
		ToeIn = bLeft ? Yaw : -Yaw;
		ToeInRaw = ToeIn;
		if (bToeRelativeToBindPose)
		{
			ToeIn += Leg.BindYaw; // remove the rig's built-in toe-out
			ToeDir = FRotator(0.0f, bLeft ? -Leg.BindYaw : Leg.BindYaw, 0.0f).RotateVector(ToeDir); // true foot direction for the knee plane
		}
	}

	// Knee: (1) front-view lateral offset from the hip->ankle line (kneeLat, reference only);
	// (2) offset out of the leg plane spanned by hip->ankle and the foot's direction (the verdict metric). Positive = outward.
	const float Hr = FVector::DotProduct(H, Right), Kr = FVector::DotProduct(K, Right), Ar = FVector::DotProduct(A, Right);
	const float SpanZ = A.Z - H.Z;
	float KneeLat = 0.0f;
	bool bKneeValid = FMath::Abs(SpanZ) > 5.0f;
	if (bKneeValid)
	{
		const float Tz = FMath::Clamp((K.Z - H.Z) / SpanZ, 0.0f, 1.0f);
		const float Dev = Kr - FMath::Lerp(Hr, Ar, Tz);
		KneeLat = bLeft ? -Dev : Dev;
	}
	float KneeOut = 0.0f;
	{
		const FVector HipAnkle = A - H;
		// Foot direction: this frame's if the foot is flat enough to have one, else the last good one (toe-off, swing).
		const FVector Flat = FVector(Toe3D.X, Toe3D.Y, 0.0f);
		if (Flat.Size() >= ToeMinFlatRatio * Toe3D.Size() && Flat.SizeSquared() > 1.0f) { Leg.LastFootDir = FRotator(0.0f, bToeRelativeToBindPose ? (bLeft ? -Leg.BindYaw : Leg.BindYaw) : 0.0f, 0.0f).RotateVector(Flat.GetSafeNormal()); }
		const FVector FootDir = Leg.LastFootDir.IsNearlyZero() ? Fwd : Leg.LastFootDir;
		FVector N = FVector::CrossProduct(HipAnkle, FootDir);
		const FVector OutDir = bLeft ? -Right : Right;
		if (N.Normalize())
		{
			if (FVector::DotProduct(N, OutDir) < 0.0f) { N = -N; }
			KneeOut = FVector::DotProduct(K - H, N);
		}
		else { bKneeValid = false; }
		const float LegLen = FVector::Dist(H, K) + FVector::Dist(K, A);
		if (LegLen < 1.0f || (Band == EFactoryGaitBand::Idle && HipAnkle.Size() < KneeMinExtension * LegLen)) { bKneeValid = false; }
	}

	// Contact: planted = toe nearly still and near the floor.
	const float ToeSpeed = Dt > KINDA_SMALL_NUMBER ? FVector::Dist2D(T, Leg.PrevToe) / Dt : 0.0f;
	Leg.PrevToe = T;
	const float Height = FloorHeightBelow(T);
	const bool bNearFloor = Height < SoleBaseline + PlantHeightMargin;
	const bool bPlantedNow = bNearFloor && ToeSpeed < PlantSpeed;
	if (bToeStanceOnly && !bNearFloor) { bToeValid = false; }
	FFactoryGaitLegStats& S = Stats[bLeft ? 0 : 1].FindOrAdd(Band);

	if (bPlantedNow && !Leg.bPlanted)
	{
		Leg.bPlanted = true;
		Leg.PlantStart = T;
		Leg.PlantMaxDrift = 0.0f;
		if (bLeft && Band != EFactoryGaitBand::Idle)
		{
			const float Now = GetWorld()->GetTimeSeconds();
			if (Leg.LastContactTime > 0.0f) { Leg.Period = Now - Leg.LastContactTime; }
			Leg.LastContactTime = Now;
			HandleShots(Now, Band, true);
		}
	}
	else if (bPlantedNow)
	{
		Leg.PlantMaxDrift = FMath::Max(Leg.PlantMaxDrift, FVector::Dist2D(T, Leg.PlantStart));
	}
	else if (Leg.bPlanted)
	{
		Leg.bPlanted = false;
		if (Band != EFactoryGaitBand::Idle) { S.Slide.Add(Leg.PlantMaxDrift); }
	}

	if (bPlantedNow && Height < 1000.0f)
	{
		if (Band == EFactoryGaitBand::Idle) { IdleContact.Add(Height); }
		else { S.Contact.Add(Height); }
	}
	if (bToeValid) { S.ToeIn.Add(ToeIn); S.ToeRaw.Add(ToeInRaw); }
	if (bKneeValid) { S.Knee.Add(KneeOut); S.KneeLat.Add(KneeLat); }
	for (TArray<float>* Arr : { &S.ToeIn, &S.ToeRaw, &S.Knee, &S.KneeLat }) { if (Arr->Num() > 20000) { Arr->RemoveAt(0, 5000); } }
}

void UFactoryGaitProbeComponent::HandleShots(float Now, EFactoryGaitBand Band, bool bLeftContactNow)
{
	if (!bTakeShots || ShotBandsDone.Contains(Band) || PendingShotBand != EFactoryGaitBand::None)
	{
		return;
	}
	UFactoryQASubsystem* QA = GetWorld()->GetSubsystem<UFactoryQASubsystem>();
	if (!QA || !QA->ClaimShotSlot(GetOwner()))
	{
		return;
	}
	const bool bMeasured = Legs[0].Period > 0.2f && Legs[0].Period < 3.0f;
	const float Period = bMeasured ? Legs[0].Period : FallbackStridePeriod;
	if ((bLeftContactNow && bMeasured) || !bLeftContactNow)
	{
		PendingShotBand = Band;
		PendingShotIndex = 0;
		PendingShotTimes.Reset();
		for (int32 i = 0; i < ShotPhasesPerBand; ++i)
		{
			PendingShotTimes.Add(Now + Period * i / FMath::Max(1, ShotPhasesPerBand));
		}
	}
}

void UFactoryGaitProbeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	AActor* Owner = GetOwner();
	if (!bResolved || !Owner || DeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}
	const FVector Loc = Owner->GetActorLocation();
	const float Yaw = Owner->GetActorRotation().Yaw;
	if (!bHavePrev)
	{
		PrevLocation = Loc; PrevYaw = Yaw; bHavePrev = true;
		Legs[0].PrevToe = Mesh->GetSocketLocation(Legs[0].Toe);
		Legs[1].PrevToe = Mesh->GetSocketLocation(Legs[1].Toe);
		return;
	}
	// Speed from real displacement (works for kinematic pawns too).
	const float Speed = FVector::Dist2D(Loc, PrevLocation) / DeltaTime;
	const float YawRate = FMath::Abs(FMath::FindDeltaAngleDegrees(PrevYaw, Yaw)) / DeltaTime;
	PrevLocation = Loc; PrevYaw = Yaw;

	EFactoryGaitBand Band = EFactoryGaitBand::None;
	if (Speed <= IdleMaxSpeed) { Band = EFactoryGaitBand::Idle; }
	else if (Speed >= RunMinSpeed) { Band = EFactoryGaitBand::Run; }
	else if (Speed >= WalkMinSpeed) { Band = EFactoryGaitBand::Walk; }
	if (Band != CurrentBand) { CurrentBand = Band; BandTime = 0.0f; }
	BandTime += DeltaTime;

	const bool bCounts = Band != EFactoryGaitBand::None && BandTime >= BandSettleTime && YawRate <= MaxYawRate;
	// Reference frame: direction of travel when moving; the body's own forward (perpendicular to the hip line) when standing.
	FVector Fwd = FVector(Owner->GetActorForwardVector().X, Owner->GetActorForwardVector().Y, 0).GetSafeNormal();
	const FVector Travel = FVector(Loc.X - PrevTravelLoc.X, Loc.Y - PrevTravelLoc.Y, 0.0f);
	if (Band == EFactoryGaitBand::Walk || Band == EFactoryGaitBand::Run)
	{
		if (Travel.Size() > 1.0f) { TravelDir = Travel.GetSafeNormal(); PrevTravelLoc = Loc; }
		if (!TravelDir.IsNearlyZero()) { Fwd = TravelDir; }
	}
	else
	{
		PrevTravelLoc = Loc;
		FVector HipLine = Mesh->GetSocketLocation(Legs[1].Thigh) - Mesh->GetSocketLocation(Legs[0].Thigh);
		HipLine.Z = 0.0f;
		FVector BodyFwd = FVector::CrossProduct(HipLine, FVector::UpVector).GetSafeNormal();
		if (!BodyFwd.IsNearlyZero()) { Fwd = FVector::DotProduct(BodyFwd, Fwd) < 0.0f ? -BodyFwd : BodyFwd; }
	}
	const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
	if (bCounts)
	{
		SampleLeg(Legs[0], true, DeltaTime, Fwd, Right, Band);
		SampleLeg(Legs[1], false, DeltaTime, Fwd, Right, Band);
		if (Band != EFactoryGaitBand::Idle)
		{
			const float W = FVector::DotProduct(Mesh->GetSocketLocation(Legs[1].Foot) - Mesh->GetSocketLocation(Legs[0].Foot), Right);
			TArray<float>& SW = Stats[0].FindOrAdd(Band).StepWidth;
			SW.Add(W);
			if (SW.Num() > 20000) { SW.RemoveAt(0, 5000); }
		}
		if (IdleContact.Num() >= 30)
		{
			SoleBaseline = FactoryGait::Percentile(IdleContact, 0.5f);
		}
	}
	else
	{
		// Not sampling (turning, band change, unsettled): drop any open plant so its drift is not carried across the gap.
		for (FLeg& L : Legs) { L.bPlanted = false; L.PlantMaxDrift = 0.0f; L.PrevToe = Mesh->GetSocketLocation(L.Toe); }
	}
	BandCountedTime = (bCounts && Band != EFactoryGaitBand::Idle) ? BandCountedTime + DeltaTime : 0.0f;

	// Shots: idle once after holding still, walk/run at quarter phases of one stride.
	UFactoryQASubsystem* QA = GetWorld()->GetSubsystem<UFactoryQASubsystem>();
	if (!bTakeShots || !QA)
	{
		return;
	}
	if (Band == EFactoryGaitBand::Idle && bCounts) { IdleHeldTime += DeltaTime; } else { IdleHeldTime = 0.0f; }
	if (BandCountedTime > ShotFallbackSeconds && Band != EFactoryGaitBand::Idle && !ShotBandsDone.Contains(Band) && PendingShotBand == EFactoryGaitBand::None)
	{
		HandleShots(GetWorld()->GetTimeSeconds(), Band, false); // no contact detected: timed schedule
	}
	if (IdleHeldTime > 1.0f && !ShotBandsDone.Contains(EFactoryGaitBand::Idle) && QA->ClaimShotSlot(Owner))
	{
		ShotBandsDone.Add(EFactoryGaitBand::Idle);
		QA->CaptureLegViews(Owner, TEXT("idle"));
	}
	if (PendingShotBand != EFactoryGaitBand::None)
	{
		if (Band != PendingShotBand)
		{
			PendingShotBand = EFactoryGaitBand::None; // band changed mid-stride: retry on the next contact
		}
		else if (PendingShotIndex < PendingShotTimes.Num() && GetWorld()->GetTimeSeconds() >= PendingShotTimes[PendingShotIndex])
		{
			QA->CaptureLegViews(Owner, FString::Printf(TEXT("%s_phase%d"), FactoryGait::BandName(PendingShotBand), PendingShotIndex));
			if (++PendingShotIndex >= PendingShotTimes.Num())
			{
				ShotBandsDone.Add(PendingShotBand);
				PendingShotBand = EFactoryGaitBand::None;
			}
		}
	}
}

bool UFactoryGaitProbeComponent::Passed() const
{
	bool bPass = bResolved;
	for (int32 Side = 0; Side < 2; ++Side)
	{
		for (const TPair<EFactoryGaitBand, FFactoryGaitLegStats>& P : Stats[Side])
		{
			const FFactoryGaitLegStats& S = P.Value;
			if (S.ToeIn.Num() < 10) { continue; }
			bPass &= FactoryGait::Percentile(S.ToeIn, 0.95f) <= ToeInMaxDeg;
			bPass &= -FactoryGait::Percentile(S.ToeIn, 0.05f) <= (P.Key == EFactoryGaitBand::Idle ? IdleToeOutMaxDeg : ToeOutMaxDeg);
			if (S.Knee.Num() >= 10)
			{
				bPass &= FactoryGait::Percentile(S.Knee, 0.95f) <= KneeOutMaxCm;
				bPass &= -FactoryGait::Percentile(S.Knee, 0.05f) <= KneeInMaxCm;
			}
			if (Side == 0 && S.StepWidth.Num() >= 30) { bPass &= FactoryGait::Percentile(S.StepWidth, 0.05f) >= MinStepWidthCm; }
			if (S.Slide.Num() >= 3) { bPass &= FactoryGait::Percentile(S.Slide, 0.95f) <= SlideMaxCm; }
			if (S.Contact.Num() >= 10) { bPass &= FMath::Abs(FactoryGait::Percentile(S.Contact, 0.5f) - SoleBaseline) <= ContactToleranceCm; }
		}
	}
	return bPass;
}

FString UFactoryGaitProbeComponent::BuildSummaryJson() const
{
	FString J = FString::Printf(TEXT("{\"actor\":\"%s\",\"resolved\":%s,\"soleBaselineCm\":%.2f,\"pass\":%s,\"bands\":["),
		*GetNameSafe(GetOwner()), bResolved ? TEXT("true") : TEXT("false"), SoleBaseline, Passed() ? TEXT("true") : TEXT("false"));
	bool bFirst = true;
	for (const EFactoryGaitBand B : { EFactoryGaitBand::Idle, EFactoryGaitBand::Walk, EFactoryGaitBand::Run })
	{
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FFactoryGaitLegStats* S = Stats[Side].Find(B);
			if (!S || S->ToeIn.Num() == 0) { continue; }
			using namespace FactoryGait;
			J += FString::Printf(TEXT("%s{\"band\":\"%s\",\"leg\":\"%s\",\"samples\":%d,\"toeInP50\":%.2f,\"toeInP95\":%.2f,\"toeOutP95\":%.2f,\"toeInRawP50\":%.2f,\"bindToeOut\":%.2f,\"kneeOutP50\":%.2f,\"kneeOutP95\":%.2f,\"kneeInP95\":%.2f,\"kneeLatP95\":%.2f,\"kneeSamples\":%d,\"slideP95\":%.2f,\"plants\":%d,\"contactP50\":%.2f,\"stepWidthP05\":%.2f,\"stepWidthP50\":%.2f}"),
				bFirst ? TEXT("") : TEXT(","), BandName(B), Side == 0 ? TEXT("L") : TEXT("R"), S->ToeIn.Num(),
				Percentile(S->ToeIn, 0.5f), Percentile(S->ToeIn, 0.95f), -Percentile(S->ToeIn, 0.05f), Percentile(S->ToeRaw, 0.5f), Legs[Side].BindYaw,
				Percentile(S->Knee, 0.5f), Percentile(S->Knee, 0.95f), -Percentile(S->Knee, 0.05f), Percentile(S->KneeLat, 0.95f), S->Knee.Num(),
				Percentile(S->Slide, 0.95f), S->Slide.Num(), Percentile(S->Contact, 0.5f), Percentile(S->StepWidth, 0.05f), Percentile(S->StepWidth, 0.5f));
			bFirst = false;
		}
	}
	J += TEXT("]}");
	return J;
}

void UFactoryGaitProbeComponent::LogSummary() const
{
	using namespace FactoryGait;
	UE_LOG(LogTemp, Display, TEXT("[FactoryQA] Gait %s: %s (baseline toe height %.1f cm; limits toe-in %.0f / toe-out %.0f deg, knee %.0f cm, slide %.0f cm, contact +-%.1f cm)"),
		*GetNameSafe(GetOwner()), Passed() ? TEXT("PASS") : TEXT("FAIL"), SoleBaseline, ToeInMaxDeg, ToeOutMaxDeg, KneeOutMaxCm, SlideMaxCm, ContactToleranceCm);
	for (const EFactoryGaitBand B : { EFactoryGaitBand::Idle, EFactoryGaitBand::Walk, EFactoryGaitBand::Run })
	{
		for (int32 Side = 0; Side < 2; ++Side)
		{
			const FFactoryGaitLegStats* S = Stats[Side].Find(B);
			if (!S || S->ToeIn.Num() == 0) { continue; }
			UE_LOG(LogTemp, Display, TEXT("[FactoryQA]   %-4s %s n=%5d toe-in p50 %6.1f p95 %6.1f | toe-out p95 %6.1f | knee-out p50 %5.1f p95 %5.1f | knee-in p95 %5.1f (front-view %5.1f) | slide p95 %5.1f (%d plants) | contact p50 %5.1f | step width p05 %5.1f p50 %5.1f"),
				BandName(B), Side == 0 ? TEXT("L") : TEXT("R"), S->ToeIn.Num(),
				Percentile(S->ToeIn, 0.5f), Percentile(S->ToeIn, 0.95f), -Percentile(S->ToeIn, 0.05f),
				Percentile(S->Knee, 0.5f), Percentile(S->Knee, 0.95f), -Percentile(S->Knee, 0.05f), Percentile(S->KneeLat, 0.95f),
				Percentile(S->Slide, 0.95f), S->Slide.Num(), Percentile(S->Contact, 0.5f), Percentile(S->StepWidth, 0.05f), Percentile(S->StepWidth, 0.5f));
		}
	}
}
