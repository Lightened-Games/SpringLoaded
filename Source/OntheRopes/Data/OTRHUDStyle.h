// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OTRHUDStyle.generated.h"

class UFontFace;

/**
 * Every HUD style value (design guide 8.7.4). "Racing / sports" theme row: Barlow Condensed (OFL), Medium + SemiBold,
 * amber accent for stored energy and your progress, red danger for incoming punches, armor loss, open ropes, defeat.
 */
UCLASS(BlueprintType)
class ONTHEROPES_API UOTRHUDStyle : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") TSoftObjectPtr<UFontFace> RegularFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/OntheRopes/UI/Fonts/FF_OTR_Medium.FF_OTR_Medium")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") TSoftObjectPtr<UFontFace> BoldFace = TSoftObjectPtr<UFontFace>(FSoftObjectPath(TEXT("/Game/OntheRopes/UI/Fonts/FF_OTR_SemiBold.FF_OTR_SemiBold")));
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 LabelSize = 15;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 BodySize = 21;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 HeadlineSize = 50;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 LabelTracking = 300;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Type") int32 HeadlineTracking = 240;

	/** Off-white #EDEAE4 at 88 %. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor Text = FLinearColor(0.846f, 0.822f, 0.776f, 0.88f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float SecondaryOpacity = 0.58f;
	/** Amber #FFB23E: stored energy, launch, your power hits, lit legend row. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor Accent = FLinearColor(1.0f, 0.445f, 0.048f, 1.0f);
	/** Red #FF4057: incoming punches, armor loss, frayed and open ropes, defeat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor Danger = FLinearColor(1.0f, 0.051f, 0.093f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor Scrim = FLinearColor(0.004f, 0.004f, 0.005f, 0.6f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") FLinearColor Track = FLinearColor(1.0f, 1.0f, 1.0f, 0.16f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float EndDimOpacity = 0.62f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float Margin = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float TopScrimHeight = 160.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float BottomScrimHeight = 200.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float LineThickness = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float HitLineWidth = 58.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float ArmorLineWidth = 180.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float ChipShear = -12.0f;
	/** Review 2: the two health bars (champion top left, Old Iron bottom right) are thick broadcast bars. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float BarThickness = 16.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float BarSegmentWidth = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float BarLabelSize = 28.0f;
	/** In-fight plates and banners are drawn at this scale (user, 2026-11-16: "the ui takes too much space"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float HudScale = 0.78f;
	/** Steel plate / slot background opacity; text and bars stay solid (user, 2026-11-16: "decrease opacity of the boxes a little"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Colour") float PlateOpacity = 0.7f;
	/** Speed-streak strength while launching off the ropes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float LaunchStreaks = 0.55f;
	/** Result-card bolts: one for the win, one for this much armor left, one for a knockout this fast (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copy") float RatingArmor = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copy") float RatingSeconds = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float LegendKeyWidth = 76.0f;
	/** World tag heights above the actors' centres, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float ChampionTagHeight = 235.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float PlayerTagHeight = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float RopeTagHeight = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float FadeSeconds = 0.2f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float BannerSeconds = 3.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float EventBannerSeconds = 1.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float PromptSeconds = 2.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float LegendDimAfter = 10.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float LegendDimOpacity = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float KeyLightSeconds = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float PopupSeconds = 1.1f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float PopupRise = 60.0f;
	/** Popup pop-in scale (1 = none). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float PopupPop = 1.8f;
	/** Full-screen flash strength on a clean hit / snap / knockout, and how fast it fades (per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float CleanFlash = 0.4f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float SnapFlash = 0.3f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float KOFlash = 0.55f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float FlashFade = 3.5f;
	/** Amber edge glow while the champion vents: strength and pulse rate (Hz). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float VentGlowStrength = 0.6f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion") float VentGlowHz = 2.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copy") FText BannerTitle = NSLOCTEXT("OTR", "BannerTitle", "KNOCK OUT THE CHAMPION");
	/** Minimum gap between world labels (punch tag, prompts, popups, heat, load, rope tags), px. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") float LabelGap = 8.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Copy") FText BannerSub = NSLOCTEXT("OTR", "BannerSub", "Your jab barely scratches it. Soak hits on the ropes, launch when it vents.");
};
