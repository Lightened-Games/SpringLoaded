// Copyright On the Ropes. All Rights Reserved.

#include "Mech/OTRMech.h"
#include "Mech/OTRMechRigComponent.h"
#include "Feedback/OTRContactShadowComponent.h"
#include "Feedback/OTRFeedback.h"
#include "Framework/OTREventSubsystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"

AOTRMech::AOTRMech(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.DoNotCreateDefaultSubobject(ACharacter::MeshComponentName))
{
	PrimaryActorTick.bCanEverTick = true;
	Rig = CreateDefaultSubobject<UOTRMechRigComponent>(TEXT("Rig"));
	Rig->SetupAttachment(GetCapsuleComponent());
	ContactShadow = CreateDefaultSubobject<UOTRContactShadowComponent>(TEXT("ContactShadow"));
	ContactShadow->SetupAttachment(GetCapsuleComponent());
	ContactShadow->Radius = 95.0f;
	ContactShadow->Strength = 0.75f;
	ContactShadow->ForwardStretch = 0.9f;
	// Mechs block each other and the world; the camera passes through them.
	UCapsuleComponent* Cap = GetCapsuleComponent();
	Cap->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Cap->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Cap->SetCanEverAffectNavigation(false);
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->bOrientRotationToMovement = false;
	Move->bUseControllerDesiredRotation = false;
	Move->GravityScale = 2.0f;
	Move->MaxStepHeight = 30.0f;
	Move->bRunPhysicsWithNoController = true;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
}

const FOTRMechLook& AOTRMech::ChooseLook() const
{
	return OTR::Tuning(this)->PlayerLook;
}

void AOTRMech::BeginPlay()
{
	Super::BeginPlay();
	ApplyBodyTuning();
	const FOTRMechLook& Look = ChooseLook();
	Rig->FeetBelowRoot = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	Rig->Build(Look);
	Rig->OnFootDown.AddUObject(this, &AOTRMech::HandleFootDown);
	ContactShadow->FadeHeight = 150.0f;
	Hum = OTRFeedback::LoopAttached(this, TEXT("ServoHum"), GetCapsuleComponent(), 0.18f, Look.VoicePitch);
}

FVector AOTRMech::GetVelocity() const
{
	return KinematicVelocity.IsNearlyZero() ? Super::GetVelocity() : KinematicVelocity;
}

void AOTRMech::HandleFootDown(const FVector& Location, float Strength)
{
	OTRFeedback::Sound(this, TEXT("Footstep"), Location, FootstepVolume * Strength, ChooseLook().VoicePitch * FMath::FRandRange(0.93f, 1.07f));
	OTRFeedback::Vfx(this, TEXT("Dust"), Location + FVector(0.0f, 0.0f, 4.0f), FRotator::ZeroRotator, 0.45f * Strength);
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnImpact.Broadcast(Location, FootstepShake * Strength, false);
	}
}
