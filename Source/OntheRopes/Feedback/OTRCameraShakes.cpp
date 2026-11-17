// Copyright On the Ropes. All Rights Reserved.

#include "Feedback/OTRCameraShakes.h"
#include "Shakes/PerlinNoiseCameraShakePattern.h"

namespace OTRShake
{
	UPerlinNoiseCameraShakePattern* Make(UCameraShakeBase* Owner, float Duration, float BlendOut, float Loc, float Rot, float Freq)
	{
		UPerlinNoiseCameraShakePattern* P = Owner->CreateDefaultSubobject<UPerlinNoiseCameraShakePattern>(TEXT("Pattern"));
		P->Duration = Duration;
		P->BlendInTime = 0.02f;
		P->BlendOutTime = BlendOut;
		P->LocationAmplitudeMultiplier = 1.0f;
		P->LocationFrequencyMultiplier = 1.0f;
		P->X.Amplitude = Loc * 0.5f; P->X.Frequency = Freq;
		P->Y.Amplitude = Loc;        P->Y.Frequency = Freq * 1.1f;
		P->Z.Amplitude = Loc;        P->Z.Frequency = Freq * 1.3f;
		P->RotationAmplitudeMultiplier = 1.0f;
		P->RotationFrequencyMultiplier = 1.0f;
		P->Pitch.Amplitude = Rot;       P->Pitch.Frequency = Freq * 1.2f;
		P->Yaw.Amplitude = Rot * 0.6f;  P->Yaw.Frequency = Freq;
		P->Roll.Amplitude = Rot * 0.5f; P->Roll.Frequency = Freq * 0.9f;
		return P;
	}
}

UOTRImpactShake::UOTRImpactShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(OTRShake::Make(this, 0.5f, 0.28f, 14.0f, 1.4f, 18.0f));
}

UOTRMicroShake::UOTRMicroShake(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bSingleInstance = false;
	SetRootShakePattern(OTRShake::Make(this, 0.2f, 0.1f, 4.5f, 0.5f, 26.0f));
}
