// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Data/OTRTuning.h"
#include "OTRMech.generated.h"

class UOTRMechRigComponent;
class UOTRContactShadowComponent;
class UAudioComponent;

/**
 * Shared mech body: capsule + CharacterMovement + the procedural rig (no skeletal mesh). Footfalls make a heavy
 * thud, a dust puff and a small impact event. Subclasses choose their look and drive the pose.
 */
UCLASS(Abstract)
class ONTHEROPES_API AOTRMech : public ACharacter
{
	GENERATED_BODY()

public:
	AOTRMech(const FObjectInitializer& ObjectInitializer);

	virtual void BeginPlay() override;
	virtual FVector GetVelocity() const override;

	UOTRMechRigComponent* GetRig() const { return Rig; }

	/** Footstep loudness (the champion is heavier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FootstepVolume = 0.55f;
	/** Footfalls closer than this to the camera target shake it a little. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tuning") float FootstepShake = 0.12f;

protected:
	/** Look + capsule size for this mech (called in BeginPlay before the rig is built). */
	virtual const FOTRMechLook& ChooseLook() const;
	virtual void ApplyBodyTuning() {}
	void HandleFootDown(const FVector& Location, float Strength);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mech") TObjectPtr<UOTRMechRigComponent> Rig;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mech") TObjectPtr<UOTRContactShadowComponent> ContactShadow;
	UPROPERTY() TObjectPtr<UAudioComponent> Hum;

	/** Kinematic moves (launches, staggers) report their real displacement here (GetVelocity override). */
	FVector KinematicVelocity = FVector::ZeroVector;
};
