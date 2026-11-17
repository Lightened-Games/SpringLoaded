// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OTRArena.generated.h"

class UDataTable;
class USpotLightComponent;
class UPointLightComponent;
class UStaticMeshComponent;

/**
 * Builds the hall around the ring from DT_OTR_Arena rows at runtime (P40 data-table level): floor, walls, bleachers,
 * scoreboard, light truss, crane, pit-crew props and the practical lights. Re-runnable: the layout lives in
 * Tools/Editor/build_content.py. Dynamic layer: tungsten flicker, swaying hanging lamps, a light pulse on clean hits.
 */
UCLASS()
class ONTHEROPES_API AOTRArena : public AActor
{
	GENERATED_BODY()

public:
	AOTRArena();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") TSoftObjectPtr<UDataTable> ArenaTable;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FlickerSpeed = 9.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FlickerDepth = 0.12f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SwayHz = 0.18f;
	/** Screen-space contact shadow length on hall lights. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float ContactShadowLength = 0.04f;
	/** Ring lights jump by this factor for a moment on a clean hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitPulse = 0.9f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float HitPulseSeconds = 0.7f;
	/** Beam strength of the spots in the hall haze (volumetric scattering). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float SpotVolumetric = 1.6f;
	/** Rows whose mesh is this get a crowd. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Content") FName CrowdMesh = TEXT("Bleacher");

	class UOTRCrowdComponent* GetCrowd() const { return Crowd; }

private:
	UPROPERTY(VisibleAnywhere, Category = "Crowd") TObjectPtr<class UOTRCrowdComponent> Crowd;
	void Build();
	UFUNCTION() void HandleCleanHit(float Load, FVector Location);

	UPROPERTY() TArray<TObjectPtr<UActorComponent>> Built;
	UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> Spots;
	TArray<float> SpotBase;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Flickers;
	TArray<float> FlickerBase;
	UPROPERTY() TArray<TObjectPtr<USceneComponent>> Swaying;
	TArray<FRotator> SwayBase;
	TArray<float> SwayAmount;
	float Clock = 0.0f;
	float Pulse = 0.0f;
};
