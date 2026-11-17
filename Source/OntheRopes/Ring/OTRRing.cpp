// Copyright On the Ropes. All Rights Reserved.

#include "Ring/OTRRing.h"
#include "Ring/OTRRopeMath.h"
#include "Data/OTRTuning.h"
#include "Feedback/OTRFeedback.h"
#include "Framework/OTREventSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OntheRopes.h"

#define LOCTEXT_NAMESPACE "OntheRopes"

AOTRRing::AOTRRing()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostPhysics;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

void AOTRRing::BeginPlay()
{
	Super::BeginPlay();
	Build();
}

FVector AOTRRing::PostLocation(int32 Corner) const
{
	static const FVector2D Signs[4] = { FVector2D(1, 1), FVector2D(-1, 1), FVector2D(-1, -1), FVector2D(1, -1) };
	const FVector2D S = Signs[(Corner % 4 + 4) % 4];
	return GetActorLocation() + FVector(S.X * RingHalf, S.Y * RingHalf, Canvas);
}

float AOTRRing::OTR_CanvasZ() const { return GetActorLocation().Z + Canvas; }
FVector AOTRRing::OTR_Centre() const { return GetActorLocation() + FVector(0.0f, 0.0f, Canvas); }

FVector AOTRRing::OTR_SideNormal(int32 Side) const
{
	switch ((Side % 4 + 4) % 4)
	{
	case 0: return FVector(1, 0, 0);
	case 1: return FVector(0, 1, 0);
	case 2: return FVector(-1, 0, 0);
	default: return FVector(0, -1, 0);
	}
}

float AOTRRing::OTR_SideDistance(int32 Side, const FVector& Location) const
{
	const FVector D = Location - GetActorLocation();
	return RingHalf - FVector::DotProduct(FVector(D.X, D.Y, 0.0f), OTR_SideNormal(Side));
}

FVector AOTRRing::OTR_SidePoint(int32 Side, float T) const
{
	return FMath::Lerp(SideStart(Side), SideEnd(Side), FMath::Clamp(T, 0.0f, 1.0f));
}

float AOTRRing::OTR_SideParam(int32 Side, const FVector& Location) const
{
	const FVector A = SideStart(Side);
	const FVector B = SideEnd(Side);
	const FVector AB(B.X - A.X, B.Y - A.Y, 0.0f);
	const FVector AL(Location.X - A.X, Location.Y - A.Y, 0.0f);
	return FMath::Clamp(FVector::DotProduct(AL, AB) / FMath::Max(AB.SizeSquared(), 1.0f), 0.0f, 1.0f);
}

UStaticMeshComponent* AOTRRing::AddStatic(FName Mesh, const FTransform& At, bool bCollide, const TCHAR* Name)
{
	UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, Name);
	C->SetStaticMesh(OTRFeedback::FindMesh(this, Mesh));
	C->SetupAttachment(GetRootComponent());
	C->SetRelativeTransform(At);
	C->SetMobility(EComponentMobility::Static);
	if (bCollide)
	{
		C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		C->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		C->SetCanEverAffectNavigation(true);
	}
	else
	{
		C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		C->SetCanEverAffectNavigation(false);
		C->ComponentTags.Add(OTR::NoCollideTag);
	}
	C->RegisterComponent();
	Statics.Add(C);
	return C;
}

void AOTRRing::Build()
{
	if (bBuilt)
	{
		return;
	}
	bBuilt = true;
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	RingHalf = T.RingHalf;
	Canvas = T.PlatformHeight;

	// Platform (authored for a 620 cm ring with a 150 cm apron and a 120 cm deck).
	const float AuthoredHalf = 770.0f;
	const float Scale = (RingHalf + T.ApronWidth) / AuthoredHalf;
	AddStatic(TEXT("RingPlatform"), FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(Scale, Scale, Canvas / 120.0f)), true, TEXT("Platform"));
	for (int32 C = 0; C < 4; ++C)
	{
		const FVector Local = PostLocation(C) - GetActorLocation();
		// Posts face the ring centre (their pads point inward).
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(-Local.Y, -Local.X));
		AddStatic(TEXT("RingPost"), FTransform(FRotator(0.0f, Yaw, 0.0f), Local), true, *FString::Printf(TEXT("Post%d"), C));
	}
	// Steps up to the apron at the blue-corner side (outside the ropes, decorative but solid).
	const float Edge = RingHalf + T.ApronWidth;
	AddStatic(TEXT("RingSteps"), FTransform(FRotator(0.0f, 180.0f, 0.0f), FVector(-Edge, -RingHalf * 0.55f, 0.0f)), true, TEXT("Steps"));

	Sides.SetNum(4);
	for (int32 S = 0; S < 4; ++S)
	{
		BuildSide(S);
	}

	// QA: -OTRFray=a,b,c,d starts the sides frayed (snap demos and ring-out tests).
	FString Fray;
	if (FParse::Value(FCommandLine::Get(), TEXT("OTRFray="), Fray, false))
	{
		TArray<FString> Parts;
		Fray.ParseIntoArray(Parts, TEXT(","));
		for (int32 S = 0; S < FMath::Min(Parts.Num(), 4); ++S)
		{
			Sides[S].Fray = FMath::Clamp(FCString::Atof(*Parts[S]), 0.0f, 0.99f);
			Sides[S].bDirty = true;
			if (FCString::Atof(*Parts[S]) >= 1.0f)
			{
				Snap(S);
			}
		}
	}
	for (int32 S = 0; S < 4; ++S)
	{
		LayoutSide(S);
	}
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Ring built: half %.0f, canvas %.0f, %d rope segments per side"), RingHalf, Canvas, Sides[0].Segments.Num());
}

void AOTRRing::BuildSide(int32 Index)
{
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	FOTRRopeSide& Side = Sides[Index];
	UStaticMesh* Cable = OTRFeedback::FindMesh(this, TEXT("Cable"));
	if (UMaterialInterface* Base = OTRFeedback::FindMaterial(this, TEXT("Rope")))
	{
		Side.MID = UMaterialInstanceDynamic::Create(Base, this);
	}
	for (int32 Strand = 0; Strand < T.StrandHeights.Num(); ++Strand)
	{
		for (int32 K = 0; K < T.Segments; ++K)
		{
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Rope%d_%d_%d"), Index, Strand, K));
			C->SetStaticMesh(Cable);
			C->SetupAttachment(GetRootComponent());
			C->SetUsingAbsoluteLocation(true);
			C->SetUsingAbsoluteRotation(true);
			C->SetUsingAbsoluteScale(true);
			C->SetMobility(EComponentMobility::Movable);
			C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			C->SetCanEverAffectNavigation(false);
			C->ComponentTags.Add(OTR::NoCollideTag);
			if (Side.MID)
			{
				C->SetMaterial(0, Side.MID);
			}
			C->RegisterComponent();
			Side.Segments.Add(C);
		}
	}
	// The rope line itself is solid while the rope holds (blocks pawns, never the camera).
	const FVector A = SideStart(Index);
	const FVector B = SideEnd(Index);
	const FVector N = OTR_SideNormal(Index);
	UBoxComponent* Box = NewObject<UBoxComponent>(this, *FString::Printf(TEXT("RopeBlocker%d"), Index));
	Box->SetupAttachment(GetRootComponent());
	Box->SetWorldLocation((A + B) * 0.5f + N * BlockerThickness * 0.5f + FVector(0.0f, 0.0f, BlockerHeight * 0.5f - 20.0f));
	Box->SetWorldRotation(FRotator(0.0f, FMath::RadiansToDegrees(FMath::Atan2(N.Y, N.X)), 0.0f));
	Box->SetBoxExtent(FVector(BlockerThickness * 0.5f, RingHalf + 20.0f, BlockerHeight * 0.5f));
	Box->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Box->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Box->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	Box->SetCanEverAffectNavigation(true);
	Box->SetHiddenInGame(true);
	Box->RegisterComponent();
	Side.Blocker = Box;
}

void AOTRRing::PlaceSegment(UStaticMeshComponent* C, const FVector& A, const FVector& B) const
{
	if (!C)
	{
		return;
	}
	const FVector D = B - A;
	const float Len = D.Size();
	if (Len < 0.5f)
	{
		C->SetVisibility(false);
		return;
	}
	C->SetVisibility(true);
	const float R = OTR::Tuning(this)->Rope.CableRadius;
	// SM_OTR_Cable: 100 cm along X, radius 1, centred on its pivot.
	C->SetWorldTransform(FTransform(FRotationMatrix::MakeFromX(D / Len).ToQuat(), (A + B) * 0.5f, FVector(Len / 100.0f * 1.01f, R, R)));
}

void AOTRRing::LayoutSide(int32 Index)
{
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	FOTRRopeSide& Side = Sides[Index];
	const FVector A = SideStart(Index);
	const FVector B = SideEnd(Index);
	const FVector N = OTR_SideNormal(Index);
	const int32 Segs = T.Segments;
	TArray<FVector> Pts;
	Pts.SetNum(Segs + 1);
	for (int32 Strand = 0; Strand < T.StrandHeights.Num(); ++Strand)
	{
		const FVector Up(0.0f, 0.0f, T.StrandHeights[Strand]);
		const float W = StrandWeights.IsValidIndex(Strand) ? StrandWeights[Strand] : 1.0f;
		if (!Side.bSnapped)
		{
			for (int32 K = 0; K <= Segs; ++K)
			{
				const float U = (float)K / Segs;
				Pts[K] = FMath::Lerp(A, B, U) + Up + N * Side.Deflect * W * OTRRope::Shape(U, Side.Contact);
			}
			for (int32 K = 0; K < Segs; ++K)
			{
				PlaceSegment(Side.Segments[Strand * Segs + K], Pts[K], Pts[K + 1]);
			}
			continue;
		}
		// Snapped: two loose halves whip outward, then hang from their posts down to the apron.
		const float Whip = FMath::Clamp(Side.SnapAge / FMath::Max(T.SnapWhipSeconds, 0.05f), 0.0f, 1.0f);
		const float Settle = Whip * Whip * (3.0f - 2.0f * Whip);
		const FVector Break = FMath::Lerp(A, B, Side.SnapAt) + Up;
		const int32 Half = Segs / 2;
		for (int32 End = 0; End < 2; ++End)
		{
			const FVector Post = (End == 0 ? A : B) + Up;
			const FVector Hang = Post + (Break - Post) * 0.45f + N * 35.0f - FVector(0.0f, 0.0f, FMath::Max(T.StrandHeights[Strand] - 12.0f, 0.0f));
			const FVector WhipPos = Break + N * 140.0f * FMath::Sin(PI * Whip) + FVector(0.0f, 0.0f, 40.0f * (1.0f - Whip));
			const FVector Free = FMath::Lerp(WhipPos, Hang, Settle);
			const int32 Count = End == 0 ? Half : Segs - Half;
			const FVector Mid = (Post + Free) * 0.5f - FVector(0.0f, 0.0f, 25.0f * Settle);
			for (int32 K = 0; K <= Count; ++K)
			{
				const float U = (float)K / Count;
				// Quadratic Bezier post -> sag -> free end.
				Pts[K] = (1 - U) * (1 - U) * Post + 2 * (1 - U) * U * Mid + U * U * Free;
			}
			for (int32 K = 0; K < Count; ++K)
			{
				const int32 Seg = End == 0 ? K : Half + K;
				PlaceSegment(Side.Segments[Strand * Segs + Seg], Pts[K], Pts[K + 1]);
			}
		}
	}
	if (Side.MID)
	{
		const FOTRRopeTuning& RT = OTR::Tuning(this)->Rope;
		Side.MID->SetScalarParameterValue(TEXT("Glow"), Side.bSnapped ? 0.0f : FMath::Min(RT.GlowAtFull * (Side.bPressed ? Side.Load : 0.0f), 3.5f));
		Side.MID->SetScalarParameterValue(TEXT("Fray"), Side.Fray);
	}
	Side.bDirty = false;
}

void AOTRRing::UpdateSide(int32 Index, float Dt)
{
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	FOTRRopeSide& Side = Sides[Index];
	if (Side.bSnapped)
	{
		if (Side.SnapAge < T.SnapWhipSeconds + 0.1f)
		{
			Side.SnapAge += Dt;
			Side.bDirty = true;
		}
	}
	else
	{
		const float Goal = OTRRope::TargetDeflect(Side.bPressed, Side.Load, T.MaxDeflect, T.RestDeflect);
		OTRRope::Spring(Side.Deflect, Side.DeflectVel, Goal, T.SpringStiffness, T.SpringDamping, Dt);
		if (FMath::Abs(Side.DeflectVel) > 0.2f || FMath::Abs(Side.Deflect - Goal) > 0.1f || Side.bPressed)
		{
			Side.bDirty = true;
		}
	}
	if (Side.bDirty)
	{
		LayoutSide(Index);
	}
}

void AOTRRing::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float Dt = FMath::Min(DeltaSeconds, 0.05f);
	for (int32 S = 0; S < Sides.Num(); ++S)
	{
		UpdateSide(S, Dt);
	}
}

void AOTRRing::OTR_SetLean(const TArray<int32>& InSides, const FVector& Contact, float Load)
{
	for (int32 S = 0; S < Sides.Num(); ++S)
	{
		const bool bOn = InSides.Contains(S) && !Sides[S].bSnapped;
		Sides[S].bPressed = bOn;
		if (bOn)
		{
			Sides[S].Contact = OTR_SideParam(S, Contact);
			Sides[S].Load = Load;
		}
	}
}

int32 AOTRRing::OTR_AbsorbHit(const TArray<int32>& InSides, float FrayGain)
{
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	int32 Snapped = INDEX_NONE;
	for (int32 S : InSides)
	{
		if (!Sides.IsValidIndex(S) || Sides[S].bSnapped)
		{
			continue;
		}
		Sides[S].DeflectVel += T.HitImpulse;
		if (OTRRope::AddFray(Sides[S].Fray, Sides[S].bSnapped, FrayGain))
		{
			Snap(S);
			Snapped = S;
		}
		else if (!Sides[S].bWarned && Sides[S].Fray >= T.FrayWarn)
		{
			Sides[S].bWarned = true;
			if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
			{
				Events->OnRopeFrayWarning.Broadcast(S, OTR_SidePoint(S, Sides[S].Contact) + FVector(0.0f, 0.0f, T.StrandHeights.Num() > 1 ? T.StrandHeights[1] : 120.0f));
			}
		}
		Sides[S].bDirty = true;
	}
	return Snapped;
}

int32 AOTRRing::OTR_FrayOverTime(const TArray<int32>& InSides, float FrayPerSecond, float DeltaSeconds)
{
	int32 Snapped = INDEX_NONE;
	for (int32 S : InSides)
	{
		if (!Sides.IsValidIndex(S) || Sides[S].bSnapped)
		{
			continue;
		}
		if (OTRRope::AddFray(Sides[S].Fray, Sides[S].bSnapped, FrayPerSecond * DeltaSeconds))
		{
			Snap(S);
			Snapped = S;
		}
		Sides[S].bDirty = true;
	}
	return Snapped;
}

void AOTRRing::OTR_Release(const TArray<int32>& InSides, float Load)
{
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	for (int32 S : InSides)
	{
		if (Sides.IsValidIndex(S) && !Sides[S].bSnapped)
		{
			Sides[S].bPressed = false;
			// The ropes fling back past straight and ring for a moment.
			Sides[S].DeflectVel += T.ReleaseImpulse * (0.4f + 0.6f * Load);
			Sides[S].Load = 0.0f;
			Sides[S].bDirty = true;
		}
	}
}

void AOTRRing::Snap(int32 Index)
{
	FOTRRopeSide& Side = Sides[Index];
	Side.bSnapped = true;
	Side.bPressed = false;
	Side.SnapAge = 0.0f;
	Side.SnapAt = FMath::Clamp(Side.Contact, 0.2f, 0.8f);
	Side.Fray = 1.0f;
	Side.bDirty = true;
	if (Side.Blocker)
	{
		Side.Blocker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	const FOTRRopeTuning& T = OTR::Tuning(this)->Rope;
	const FVector At = OTR_SidePoint(Index, Side.SnapAt) + FVector(0.0f, 0.0f, T.StrandHeights.Num() > 1 ? T.StrandHeights[1] : 120.0f);
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Rope side %d snapped"), Index);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		++Events->RopesSnapped;
		Events->OnRopeSnapped.Broadcast(Index, At);
	}
}

#undef LOCTEXT_NAMESPACE
