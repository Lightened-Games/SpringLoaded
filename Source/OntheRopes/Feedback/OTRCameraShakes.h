// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "OTRCameraShakes.generated.h"

/** Heavy shake: a slam or a hit on the scrapper. */
UCLASS()
class ONTHEROPES_API UOTRImpactShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UOTRImpactShake(const FObjectInitializer& ObjectInitializer);
};

/** Micro shake: crashes, knocks, bolts. */
UCLASS()
class ONTHEROPES_API UOTRMicroShake : public UCameraShakeBase
{
	GENERATED_BODY()
public:
	UOTRMicroShake(const FObjectInitializer& ObjectInitializer);
};
