// Copyright On the Ropes. All Rights Reserved.

#include "Mech/OTRChampionMech.h"
#include "Mech/OTRChampionBrainComponent.h"
#include "Components/OTRHeatComponent.h"
#include "Data/OTRTuning.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

AOTRChampionMech::AOTRChampionMech(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const FOTRChampionTuning& T = GetDefault<UOTRTuning>()->Champion;
	GetCapsuleComponent()->InitCapsuleSize(T.CapsuleRadius, T.CapsuleHalfHeight);
	AutoPossessAI = EAutoPossessAI::Disabled;
	Tags.Add(OTR::ChampionTag);
	Heat = CreateDefaultSubobject<UOTRHeatComponent>(TEXT("Heat"));
	Brain = CreateDefaultSubobject<UOTRChampionBrainComponent>(TEXT("Brain"));
	UCharacterMovementComponent* Move = GetCharacterMovement();
	Move->MaxWalkSpeed = T.WalkSpeed;
	Move->MaxAcceleration = T.Acceleration;
	Move->BrakingDecelerationWalking = 1600.0f;
	Move->bUseSeparateBrakingFriction = true;
	Move->BrakingFriction = 0.0f;
	FootstepVolume = 0.75f;
	FootstepShake = 0.2f;
}

const FOTRMechLook& AOTRChampionMech::ChooseLook() const
{
	return OTR::Tuning(this)->ChampionLook;
}

void AOTRChampionMech::ApplyBodyTuning()
{
	const FOTRChampionTuning& T = OTR::Tuning(this)->Champion;
	const float OldHalf = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	GetCapsuleComponent()->SetCapsuleSize(T.CapsuleRadius, T.CapsuleHalfHeight);
	AddActorWorldOffset(FVector(0.0f, 0.0f, T.CapsuleHalfHeight - OldHalf));
	GetCharacterMovement()->MaxWalkSpeed = T.WalkSpeed;
	GetCharacterMovement()->MaxAcceleration = T.Acceleration;
	Heat->CoolRate = T.HeatCoolRate;
	Heat->Threshold = T.OverheatThreshold;
	Heat->VentSeconds = T.VentSeconds;
	Heat->HeatAfterVent = T.HeatAfterVent;
	Heat->SetHeat(T.InitialHeat);
}

void AOTRChampionMech::BeginPlay()
{
	Super::BeginPlay();
	Brain->Begin(this, Rig, Heat);
}

EOTRChampState AOTRChampionMech::OTR_ChampState() const { return Brain->GetState(); }
EOTRPunch AOTRChampionMech::OTR_Telegraph(float& OutSecondsToHit) const { return Brain->GetTelegraph(OutSecondsToHit); }
const FOTRPunchRow* AOTRChampionMech::OTR_CurrentPunch() const { return Brain->GetCurrentPunch(); }
float AOTRChampionMech::OTR_Heat01() const { return Heat->GetHeat01(); }
bool AOTRChampionMech::OTR_IsVenting() const { return Heat->IsVenting(); }
float AOTRChampionMech::OTR_VentTimeLeft() const { return Heat->GetVentLeft(); }
bool AOTRChampionMech::OTR_IsJabMode() const { return Brain->IsJabMode(); }
FVector AOTRChampionMech::OTR_FistLocation() const { return Brain->GetFistLocation(); }
EOTRLaunchResult AOTRChampionMech::OTR_ReceiveLaunch(float Load, AActor* From) { return Brain->ReceiveLaunch(Load, From); }

EOTRStrikeResult AOTRChampionMech::OTR_ReceiveStrike(EOTRPlayerStrike Strike, AActor* From)
{
	return Brain ? Brain->ReceiveStrike(Strike, From) : EOTRStrikeResult::Missed;
}

bool AOTRChampionMech::OTR_IsGuarding() const
{
	return Brain && Brain->IsGuarding();
}

int32 AOTRChampionMech::OTR_Phase() const
{
	return Brain ? Brain->GetPhase() : 0;
}
