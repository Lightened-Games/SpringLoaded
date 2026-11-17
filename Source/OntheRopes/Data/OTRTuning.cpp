// Copyright On the Ropes. All Rights Reserved.

#include "Data/OTRTuning.h"
#include "Interfaces/OTRInterfaces.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "GameFramework/GameModeBase.h"

UOTRTuning::UOTRTuning()
{
	// The champion: a taller, newer machine, graphite with ivory trim; its glow is arm heat.
	ChampionLook.Prefix = TEXT("Titan");
	ChampionLook.Paint = FLinearColor(0.013f, 0.014f, 0.017f, 1.0f);
	// Review 1 (user, 2026-11-11 "Yes, darken it"): the champion read light grey under the key spot. Cause: near-black
	// paint showing grazing-angle specular sheen. Low specular, little chipping, no rust bleed, dark bronze trim.
	ChampionLook.Trim = FLinearColor(0.075f, 0.065f, 0.05f, 1.0f);
	ChampionLook.PaintScalars.Add(TEXT("EdgeWear"), 0.3f);
	ChampionLook.PaintScalars.Add(TEXT("RustBias"), 2.0f);
	ChampionLook.PaintScalars.Add(TEXT("Specular"), 0.08f);
	ChampionLook.MetalBrightness = 0.25f;
	ChampionLook.Glow = FLinearColor(1.0f, 0.32f, 0.06f, 1.0f);
	ChampionLook.GlowEmissive = 0.0f;
	ChampionLook.HipHeight = 138.0f;
	ChampionLook.HipHalfWidth = 40.0f;
	ChampionLook.ThighLength = 76.0f;
	ChampionLook.ShinLength = 74.0f;
	ChampionLook.AnkleHeight = 18.0f;
	ChampionLook.WaistHeight = 26.0f;
	ChampionLook.Shoulder = FVector(0.0f, 80.0f, 104.0f);
	ChampionLook.NeckHeight = 124.0f;
	ChampionLook.UpperArmLength = 92.0f;
	ChampionLook.ForearmLength = 88.0f;
	ChampionLook.StanceHalfWidth = 42.0f;
	ChampionLook.StepThreshold = 40.0f;
	ChampionLook.StepSeconds = 0.36f;
	ChampionLook.StepHeight = 16.0f;
	ChampionLook.StepBob = 6.0f;
	ChampionLook.VoicePitch = 0.8f;
}

namespace OTR
{
	const UOTRTuning* Tuning(const UObject* WorldContext)
	{
		const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
		const IOTRMatchInfo* Match = World ? Cast<IOTRMatchInfo>(World->GetAuthGameMode()) : nullptr;
		const UOTRTuning* T = Match ? Match->OTR_Tuning() : nullptr;
		return T ? T : GetDefault<UOTRTuning>();
	}
}
