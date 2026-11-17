// Copyright On the Ropes. All Rights Reserved.

#include "World/OTRCrowdComponent.h"
#include "Framework/OTREventSubsystem.h"
#include "Feedback/OTRFeedback.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "GameFramework/Actor.h"
#include "OntheRopes.h"

UOTRCrowdComponent::UOTRCrowdComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f;
}

UAnimSequence* UOTRCrowdComponent::LoadClip(const FString& Name) const
{
	const FString Path = FString::Printf(TEXT("%s/Anims/A_%s.A_%s"), *CrowdRoot, *Name, *Name);
	return LoadObject<UAnimSequence>(nullptr, *Path);
}

void UOTRCrowdComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnCleanHit.AddDynamic(this, &UOTRCrowdComponent::HandleCleanHit);
		Events->OnRopeSnapped.AddDynamic(this, &UOTRCrowdComponent::HandleSnap);
		Events->OnPopup.AddDynamic(this, &UOTRCrowdComponent::HandlePopup);
		Events->OnPlayerHit.AddDynamic(this, &UOTRCrowdComponent::HandlePlayerHit);
		Events->OnMatchState.AddDynamic(this, &UOTRCrowdComponent::HandleMatchState);
	}
}

void UOTRCrowdComponent::Populate(const TArray<FTransform>& Bleachers)
{
	Rng.Initialize(Seed);
	for (const FString& C : IdleClips) { if (UAnimSequence* A = LoadClip(C)) { Idles.Add(A); } }
	for (const FString& C : CheerClips) { if (UAnimSequence* A = LoadClip(C)) { Cheers.Add(A); } }
	TArray<USkeletalMesh*> Bodies;
	for (const FString& Av : Avatars)
	{
		const FString Path = FString::Printf(TEXT("%s/%s/SK_%s.SK_%s"), *CrowdRoot, *Av, *Av, *Av);
		if (USkeletalMesh* M = LoadObject<USkeletalMesh>(nullptr, *Path))
		{
			Bodies.Add(M);
		}
	}
	if (Bodies.Num() == 0 || Idles.Num() == 0)
	{
		UE_LOG(LogOntheRopes, Warning, TEXT("[OTR] Crowd assets missing (%d bodies, %d idles): empty stands"), Bodies.Num(), Idles.Num());
		return;
	}
	AActor* Owner = GetOwner();
	int32 Placed = 0;
	for (int32 B = 0; B < Bleachers.Num() && Placed < MaxSpectators; ++B)
	{
		for (int32 R = 0; R < Rows && Placed < MaxSpectators; ++R)
		{
			const float Density = FMath::Lerp(FrontDensity, BackDensity, Rows > 1 ? float(R) / (Rows - 1) : 0.0f);
			for (float Y = -HalfWidth; Y <= HalfWidth && Placed < MaxSpectators; Y += Spacing)
			{
				if (Rng.FRand() > Density)
				{
					continue;
				}
				const FVector Local(-R * RowDepth + StandForward + Rng.FRandRange(-6.0f, 6.0f), Y + Rng.FRandRange(-12.0f, 12.0f), R * RowRise + DeckTop);
				const FVector World = Bleachers[B].TransformPosition(Local);
				// Face the ring centre (the arena origin), with a little scatter.
				const FVector ToRing = (Owner->GetActorLocation() - World).GetSafeNormal2D();
				const float Yaw = ToRing.Rotation().Yaw + Rng.FRandRange(-YawJitter, YawJitter);
				USkeletalMeshComponent* M = NewObject<USkeletalMeshComponent>(Owner, *FString::Printf(TEXT("Fan_%d"), Placed));
				M->SetSkeletalMesh(Bodies[Rng.RandRange(0, Bodies.Num() - 1)]);
				M->SetupAttachment(this);
				M->SetUsingAbsoluteLocation(true);
				M->SetUsingAbsoluteRotation(true);
				M->SetUsingAbsoluteScale(true);
				M->SetWorldLocationAndRotation(World, FRotator(0.0f, Yaw + MeshYaw, 0.0f));
				M->SetWorldScale3D(FVector(Rng.FRandRange(ScaleRange.X, ScaleRange.Y)));
				M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				M->SetCanEverAffectNavigation(false);
				M->SetGenerateOverlapEvents(false);
				M->ComponentTags.Add(OTR::NoCollideTag);
				M->SetCastShadow(R < ShadowRows);
				M->bEnableUpdateRateOptimizations = true;
				M->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
				M->RegisterComponent();
				FFan Fan;
				Fan.Mesh = M;
				Fan.Idle = Rng.RandRange(0, Idles.Num() - 1);
				M->PlayAnimation(Idles[Fan.Idle], true);
				M->SetPosition(Rng.FRandRange(0.0f, Idles[Fan.Idle]->GetPlayLength()), false);
				M->SetPlayRate(Rng.FRandRange(0.85f, 1.1f));
				Meshes.Add(M);
				Fans.Add(Fan);
				++Placed;
			}
		}
	}
	for (int32 I = 0; I < FlashPool && Placed > 0; ++I)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(Owner, *FString::Printf(TEXT("Flash_%d"), I));
		L->SetupAttachment(this);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetIntensity(0.0f);
		L->SetLightColor(FLinearColor(0.85f, 0.92f, 1.0f));
		L->SetAttenuationRadius(FlashRadius);
		L->SetCastShadows(false);
		L->SetVisibility(false);
		L->RegisterComponent();
		Flashes.Add(L);
		FlashLeft.Add(0.0f);
	}
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Crowd: %d spectators on %d bleachers (%d bodies, %d idles, %d cheers)"), Placed, Bleachers.Num(), Bodies.Num(), Idles.Num(), Cheers.Num());
}

void UOTRCrowdComponent::Cheer(float Share, float Seconds)
{
	if (Cheers.Num() == 0)
	{
		return;
	}
	for (FFan& F : Fans)
	{
		if (FMath::FRand() < Share)
		{
			const float Start = Clock + FMath::FRandRange(0.0f, ReactSpread);
			F.CheerStart = F.bCheering ? Clock : Start;
			F.CheerEnd = FMath::Max(F.CheerEnd, Start + Seconds * FMath::FRandRange(0.8f, 1.2f));
		}
	}
}

void UOTRCrowdComponent::QueueFlashes(int32 Count, float OverSeconds)
{
	for (int32 I = 0; I < Count; ++I)
	{
		PendingFlashAt.Add(Clock + FMath::FRandRange(0.0f, OverSeconds));
	}
}

void UOTRCrowdComponent::TickFlashes(float Dt)
{
	if (Flashes.Num() == 0 || Fans.Num() == 0)
	{
		return;
	}
	if (FMath::FRand() < FlashAmbientRate * Dt)
	{
		PendingFlashAt.Add(Clock);
	}
	for (int32 P = PendingFlashAt.Num() - 1; P >= 0; --P)
	{
		if (PendingFlashAt[P] > Clock)
		{
			continue;
		}
		PendingFlashAt.RemoveAtSwap(P);
		for (int32 I = 0; I < Flashes.Num(); ++I)
		{
			if (FlashLeft[I] <= 0.0f)
			{
				const FFan& Fan = Fans[FMath::RandRange(0, Fans.Num() - 1)];
				if (Fan.Mesh)
				{
					Flashes[I]->SetWorldLocation(Fan.Mesh->GetComponentLocation() + FVector(0.0f, 0.0f, 165.0f));
					Flashes[I]->SetIntensity(FlashCandela);
					Flashes[I]->SetVisibility(true);
					FlashLeft[I] = FlashSeconds;
				}
				break;
			}
		}
	}
	for (int32 I = 0; I < Flashes.Num(); ++I)
	{
		if (FlashLeft[I] > 0.0f)
		{
			FlashLeft[I] -= Dt;
			if (FlashLeft[I] <= 0.0f)
			{
				Flashes[I]->SetVisibility(false);
			}
		}
	}
}

void UOTRCrowdComponent::HandleCleanHit(float Load, FVector Location) { Cheer(CleanHitShare, CheerSeconds); QueueFlashes(FlashBurst, 1.2f); }
void UOTRCrowdComponent::HandleSnap(int32 Side, FVector Location) { Cheer(SnapShare, CheerSeconds * 0.8f); QueueFlashes(FlashBurst / 2, 0.8f); }
void UOTRCrowdComponent::HandlePopup(FText Text, FVector WorldLocation, FLinearColor Colour)
{
	Cheer(BigMoveShare, CheerSeconds * 0.6f);
	OTRFeedback::Sound2D(this, TEXT("CrowdCheer"), BigMoveCheerVolume, FMath::FRandRange(0.95f, 1.05f));
}

void UOTRCrowdComponent::HandlePlayerHit(float Damage, bool bGuarded, EOTRPunch Punch, FVector Location)
{
	if (!bGuarded && Damage >= PlayerHurtMinDamage)
	{
		Cheer(PlayerHurtShare, CheerSeconds * 0.5f);
	}
}

void UOTRCrowdComponent::HandleMatchState(EOTRMatchState State, EOTRDefeat Reason)
{
	if (State == EOTRMatchState::Victory)
	{
		Cheer(1.0f, VictoryCheerSeconds);
		QueueFlashes(FlashBurst * 3, 4.0f);
	}
	else if (State == EOTRMatchState::Defeat)
	{
		Cheer(0.45f, VictoryCheerSeconds * 0.5f);
	}
}

void UOTRCrowdComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Clock += DeltaTime;
	TickFlashes(DeltaTime);
	for (FFan& F : Fans)
	{
		if (!F.Mesh)
		{
			continue;
		}
		const bool bWant = F.CheerStart >= 0.0f && Clock >= F.CheerStart && Clock < F.CheerEnd;
		if (bWant && !F.bCheering)
		{
			F.bCheering = true;
			UAnimSequence* A = Cheers[FMath::RandRange(0, Cheers.Num() - 1)];
			F.Mesh->PlayAnimation(A, true);
			// Cheer clips start calm: skip into the jump and arm pump.
			F.Mesh->SetPosition(FMath::FRandRange(0.15f, 0.45f) * A->GetPlayLength(), false);
			F.Mesh->SetPlayRate(FMath::FRandRange(0.95f, 1.15f));
		}
		else if (!bWant && F.bCheering)
		{
			F.bCheering = false;
			F.CheerStart = -1.0f;
			UAnimSequence* A = Idles[F.Idle];
			F.Mesh->PlayAnimation(A, true);
			F.Mesh->SetPosition(FMath::FRandRange(0.0f, A->GetPlayLength()), false);
			F.Mesh->SetPlayRate(FMath::FRandRange(0.85f, 1.1f));
		}
	}
}
