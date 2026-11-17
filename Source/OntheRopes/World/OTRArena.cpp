// Copyright On the Ropes. All Rights Reserved.

#include "World/OTRArena.h"
#include "World/OTRCrowdComponent.h"
#include "Data/OTRTypes.h"
#include "Feedback/OTRFeedback.h"
#include "Framework/OTREventSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/DataTable.h"
#include "Engine/StaticMesh.h"
#include "OntheRopes.h"

AOTRArena::AOTRArena()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.03f;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	ArenaTable = TSoftObjectPtr<UDataTable>(FSoftObjectPath(TEXT("/Game/OntheRopes/Data/DT_OTR_Arena.DT_OTR_Arena")));
	Crowd = CreateDefaultSubobject<UOTRCrowdComponent>(TEXT("Crowd"));
	Crowd->SetupAttachment(GetRootComponent());
}

void AOTRArena::BeginPlay()
{
	Super::BeginPlay();
	Build();
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnCleanHit.AddDynamic(this, &AOTRArena::HandleCleanHit);
	}
}

void AOTRArena::Build()
{
	UDataTable* Table = ArenaTable.LoadSynchronous();
	if (!Table)
	{
		UE_LOG(LogOntheRopes, Error, TEXT("[OTR] DT_OTR_Arena missing: empty hall"));
		return;
	}
	int32 Count = 0;
	TArray<FTransform> Stands;
	for (const FName& RowName : Table->GetRowNames())
	{
		const FOTRPropRow* Row = Table->FindRow<FOTRPropRow>(RowName, TEXT("Arena"));
		if (!Row)
		{
			continue;
		}
		const FTransform At(Row->Rotation, Row->Location, Row->Scale);
		if (!Row->Mesh.IsNone())
		{
			UStaticMesh* Mesh = OTRFeedback::FindMesh(this, Row->Mesh);
			if (!Mesh)
			{
				continue;
			}
			UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("Prop_%s"), *RowName.ToString()));
			C->SetStaticMesh(Mesh);
			C->SetupAttachment(GetRootComponent());
			C->SetRelativeTransform(At);
			C->SetMobility(Row->Sway > 0.0f ? EComponentMobility::Movable : EComponentMobility::Static);
			C->SetCastShadow(Row->bCastShadow);
			if (Row->bCollide)
			{
				// Solid by default (C2): blocks pawns, affects navigation; the camera passes through.
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
			const FName Mats[] = { Row->Material, Row->Material2, Row->Material3 };
			for (int32 I = 0; I < 3; ++I)
			{
				if (Mats[I].IsNone())
				{
					continue;
				}
				UMaterialInterface* M = OTRFeedback::FindMaterial(this, Mats[I]);
				if (I == 0 && M && Row->Tint.A > 0.0f)
				{
					UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(M, this);
					MID->SetVectorParameterValue(TEXT("Tint"), FLinearColor(Row->Tint.R, Row->Tint.G, Row->Tint.B, 1.0f));
					M = MID;
				}
				if (M)
				{
					C->SetMaterial(I, M);
				}
			}
			C->RegisterComponent();
			Built.Add(C);
			++Count;
			if (Row->Mesh == CrowdMesh)
			{
				Stands.Add(C->GetComponentTransform());
			}
			if (Row->Sway > 0.0f)
			{
				Swaying.Add(C);
				SwayBase.Add(Row->Rotation);
				SwayAmount.Add(Row->Sway);
			}
		}
		if (Row->LightCandela > 0.0f && Row->bPointLight)
		{
			UPointLightComponent* L = NewObject<UPointLightComponent>(this, *FString::Printf(TEXT("Point_%s"), *RowName.ToString()));
			L->SetupAttachment(GetRootComponent());
			L->SetMobility(EComponentMobility::Movable);
			L->SetRelativeLocation(Row->Location);
			L->SetIntensityUnits(ELightUnits::Candelas);
			L->SetIntensity(Row->LightCandela);
			L->SetLightColor(Row->LightColour);
			L->SetAttenuationRadius(Row->LightRadius);
			L->SetCastShadows(Row->bLightShadows);
			L->SetSourceRadius(8.0f);
			L->ContactShadowLength = ContactShadowLength;
			L->RegisterComponent();
			Built.Add(L);
			if (Row->bFlicker)
			{
				Flickers.Add(L);
				FlickerBase.Add(Row->LightCandela);
			}
		}
		else if (Row->LightCandela > 0.0f)
		{
			USpotLightComponent* L = NewObject<USpotLightComponent>(this, *FString::Printf(TEXT("Spot_%s"), *RowName.ToString()));
			L->SetupAttachment(GetRootComponent());
			L->SetMobility(EComponentMobility::Movable);
			L->SetRelativeLocationAndRotation(Row->Location, Row->LightRotation);
			L->SetIntensityUnits(ELightUnits::Candelas);
			L->SetIntensity(Row->LightCandela);
			L->SetLightColor(Row->LightColour);
			L->SetAttenuationRadius(Row->LightRadius);
			L->SetOuterConeAngle(Row->LightCone);
			L->SetInnerConeAngle(Row->LightCone * 0.6f);
			L->SetCastShadows(Row->bLightShadows);
			L->SetSourceRadius(10.0f);
			L->SetVolumetricScatteringIntensity(SpotVolumetric);
			L->ContactShadowLength = ContactShadowLength;
			L->RegisterComponent();
			Built.Add(L);
			Spots.Add(L);
			SpotBase.Add(Row->LightCandela);
		}
	}
	UE_LOG(LogOntheRopes, Display, TEXT("[OTR] Arena built: %d props, %d spots, %d practicals"), Count, Spots.Num(), Flickers.Num());
	if (Crowd)
	{
		Crowd->Populate(Stands);
	}
}

void AOTRArena::HandleCleanHit(float Load, FVector Location)
{
	Pulse = HitPulseSeconds;
}

void AOTRArena::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Clock += DeltaSeconds;
	for (int32 I = 0; I < Flickers.Num(); ++I)
	{
		// Layered sines read as old tungsten without random popping.
		const float T = Clock * FlickerSpeed + I * 1.7f;
		const float N = 0.5f * FMath::Sin(T) + 0.3f * FMath::Sin(T * 2.3f + 1.1f) + 0.2f * FMath::Sin(T * 5.1f + 2.3f);
		Flickers[I]->SetIntensity(FlickerBase[I] * (1.0f + FlickerDepth * N));
	}
	for (int32 I = 0; I < Swaying.Num(); ++I)
	{
		const float S = FMath::Sin(Clock * SwayHz * 2.0f * PI + I * 1.3f);
		Swaying[I]->SetRelativeRotation(SwayBase[I] + FRotator(S * SwayAmount[I], 0.0f, FMath::Cos(Clock * SwayHz * 1.7f * PI + I) * SwayAmount[I] * 0.6f));
	}
	Pulse = FMath::Max(0.0f, Pulse - DeltaSeconds);
	const float K = 1.0f + HitPulse * (Pulse / FMath::Max(HitPulseSeconds, 0.01f));
	for (int32 I = 0; I < Spots.Num(); ++I)
	{
		Spots[I]->SetIntensity(SpotBase[I] * K);
	}
}
