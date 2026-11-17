// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/OTRInterfaces.h"
#include "OTRRing.generated.h"

class UStaticMeshComponent;
class UBoxComponent;
class UMaterialInstanceDynamic;

/** Runtime state of one rope side (three strands between two posts). */
USTRUCT()
struct FOTRRopeSide
{
	GENERATED_BODY()

	float Fray = 0.0f;
	bool bSnapped = false;
	bool bWarned = false;
	bool bPressed = false;
	float Load = 0.0f;
	float Contact = 0.5f;
	float Deflect = 0.0f;
	float DeflectVel = 0.0f;
	float SnapAge = 0.0f;
	float SnapAt = 0.5f;
	bool bDirty = true;

	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Segments;
	UPROPERTY() TObjectPtr<UBoxComponent> Blocker;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> MID;
};

/**
 * The raised ring: canvas platform, four corner posts, steps, and four rope sides that stretch, store energy, fray
 * and snap (new system #1, analytic springs, Docs/PLAN.md section 10). Each intact side has an invisible blocker on
 * its rope line; a snapped side loses it, so that side becomes a ring-out edge. Built from tuning at BeginPlay.
 * Sides: 0 = +X, 1 = +Y, 2 = -X, 3 = -Y (counter-clockwise; corner i joins side i and side i+1).
 */
UCLASS()
class ONTHEROPES_API AOTRRing : public AActor, public IOTRRingInfo
{
	GENERATED_BODY()

public:
	AOTRRing();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// IOTRRingInfo
	virtual int32 OTR_SideCount() const override { return Sides.Num(); }
	virtual float OTR_SideDistance(int32 Side, const FVector& Location) const override;
	virtual FVector OTR_SideNormal(int32 Side) const override;
	virtual FVector OTR_SidePoint(int32 Side, float T) const override;
	virtual float OTR_SideParam(int32 Side, const FVector& Location) const override;
	virtual bool OTR_IsSnapped(int32 Side) const override { return Sides.IsValidIndex(Side) && Sides[Side].bSnapped; }
	virtual float OTR_Fray(int32 Side) const override { return Sides.IsValidIndex(Side) ? Sides[Side].Fray : 0.0f; }
	virtual float OTR_CanvasZ() const override;
	virtual FVector OTR_Centre() const override;
	virtual void OTR_SetLean(const TArray<int32>& InSides, const FVector& Contact, float Load) override;
	virtual int32 OTR_AbsorbHit(const TArray<int32>& InSides, float FrayGain) override;
	virtual int32 OTR_FrayOverTime(const TArray<int32>& InSides, float FrayPerSecond, float DeltaSeconds) override;
	virtual void OTR_Release(const TArray<int32>& InSides, float Load) override;

	/** QA: start with these fray values (-OTRFray=0.6,0,0,0). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BlockerThickness = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float BlockerHeight = 420.0f;
	/** Strand weights (bottom to top): the middle strand takes most of the push. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") TArray<float> StrandWeights = { 0.7f, 1.0f, 0.8f };

private:
	void Build();
	void BuildSide(int32 Index);
	void UpdateSide(int32 Index, float Dt);
	void LayoutSide(int32 Index);
	void Snap(int32 Index);
	void PlaceSegment(UStaticMeshComponent* C, const FVector& A, const FVector& B) const;
	FVector PostLocation(int32 Corner) const;
	FVector SideStart(int32 Side) const { return PostLocation((Side + 3) % 4); }
	FVector SideEnd(int32 Side) const { return PostLocation(Side); }
	UStaticMeshComponent* AddStatic(FName Mesh, const FTransform& At, bool bCollide, const TCHAR* Name);

	UPROPERTY() TArray<FOTRRopeSide> Sides;
	UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Statics;
	float RingHalf = 620.0f;
	float Canvas = 120.0f;
	bool bBuilt = false;
};
