// Copyright On the Ropes. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Data/OTRTypes.h"
#include "OTRHUDWidget.generated.h"

class UCanvasPanel;
class UCanvasPanelSlot;
class UTextBlock;
class UImage;
class UHorizontalBox;
class UVerticalBox;
class UWidget;
class UFontFace;
class UOTRHUDStyle;
class UOverlay;
class UTexture2D;
struct FCompositeFont;

/**
 * The boxless broadcast-style HUD (P21, design guide 8.7.0), built in C++ from UOTRHUDStyle.
 * Normal play (5 elements): power hits (top left), bell timer (top right), armor (bottom right), quiet key legend
 * (bottom left), one contextual banner or prompt. Diegetic world tags: the incoming punch (HIGH / LOW / JAB, danger),
 * the champion's heat line (VENTING in the accent), the stored load at your shoulder, frayed / open ropes.
 * It only listens to UOTREventSubsystem and reads the match / ring / champion / boxer interfaces.
 */
UCLASS()
class ONTHEROPES_API UOTRHUDWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	enum class EWeight : uint8 { Regular, Bold };

	struct FLine
	{
		TObjectPtr<UImage> Fill;
		TObjectPtr<UImage> Rest;
		float Width = 100.0f;
		float Thickness = 2.0f;
	};

	struct FLegendRow
	{
		FName Key;
		TObjectPtr<UTextBlock> Glyph;
		TObjectPtr<UTextBlock> Label;
		float Lit = 0.0f;
	};

	struct FBannerItem
	{
		FText Title;
		FText Sub;
		bool bDanger = false;
		float Duration = 1.6f;
	};

	struct FPrompt
	{
		TObjectPtr<UVerticalBox> Box;
		TObjectPtr<UTextBlock> Text;
		TObjectPtr<UImage> Rule;
		TWeakObjectPtr<AActor> Anchor;
		FVector Offset = FVector::ZeroVector;
		FVector World = FVector::ZeroVector;
		float Age = 100.0f;
	};

	struct FQueuedPrompt
	{
		FText Text;
		TWeakObjectPtr<AActor> Anchor;
		FVector Offset = FVector::ZeroVector;
		bool bDanger = false;
		float QueuedAt = 0.0f;
	};

	struct FPopup
	{
		TObjectPtr<UTextBlock> Text;
		FVector World = FVector::ZeroVector;
		float Age = 100.0f;
	};

	/** A small world tag: caps text over a hairline. */
	struct FTag
	{
		TObjectPtr<UVerticalBox> Box;
		TObjectPtr<UTextBlock> Text;
		FLine Line;
		float Alpha = 0.0f;
		float Pop = 0.0f;
	};

	UPROPERTY() TObjectPtr<UOTRHUDStyle> Style;
	UPROPERTY() TArray<TObjectPtr<UFontFace>> Faces;
	TSharedPtr<FCompositeFont> RegularFont;
	TSharedPtr<FCompositeFont> BoldFont;
	bool bFontsBuilt = false;

	UPROPERTY() TObjectPtr<UCanvasPanel> Root;
	UPROPERTY() TArray<TObjectPtr<UImage>> VignetteImages;
	/** Amber edge glow while the champion vents (your window). */
	UPROPERTY() TArray<TObjectPtr<UImage>> VentImages;
	/** Brief full-screen flash on clean hits, snaps and the knockout. */
	UPROPERTY() TObjectPtr<UImage> FlashImage;
	UPROPERTY() TObjectPtr<UImage> Dim;
	UPROPERTY() TObjectPtr<UTextBlock> HitsLabel;
	UPROPERTY() TObjectPtr<UTextBlock> HitsValue;
	UPROPERTY() TObjectPtr<UTextBlock> ClockText;
	UPROPERTY() TObjectPtr<UTextBlock> ClockLabel;
	UPROPERTY() TObjectPtr<UTextBlock> ArmorValue;
	UPROPERTY() TObjectPtr<UTextBlock> ArmorLabel;
	UPROPERTY() TObjectPtr<UVerticalBox> LegendBox;
	UPROPERTY() TObjectPtr<UTextBlock> CountdownText;
	UPROPERTY() TObjectPtr<UVerticalBox> BannerBox;
	UPROPERTY() TObjectPtr<UTextBlock> BannerTitle;
	UPROPERTY() TObjectPtr<UImage> BannerRule;
	UPROPERTY() TObjectPtr<UTextBlock> BannerSub;
	UPROPERTY() TArray<TObjectPtr<UImage>> BannerBand;
	UPROPERTY() TArray<TObjectPtr<UImage>> EndBand;
	UPROPERTY() TObjectPtr<UVerticalBox> EndBox;
	UPROPERTY() TObjectPtr<UTextBlock> EndTitle;
	UPROPERTY() TObjectPtr<UImage> EndRule;
	UPROPERTY() TObjectPtr<UTextBlock> EndReason;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> EndStatValues;
	UPROPERTY() TObjectPtr<UTextBlock> EndPrompt;

	// Fight-night steel (design guide 2.5.16): plates, fight card, key flash, launch streaks, result card.
	UPROPERTY() TMap<FName, TObjectPtr<UTexture2D>> UITextures;
	UPROPERTY() TObjectPtr<UWidget> TitleCard;
	UPROPERTY() TArray<TObjectPtr<UImage>> TitleBulbs;
	UPROPERTY() TObjectPtr<UImage> StartBar;
	UPROPERTY() TObjectPtr<UWidget> ChampPlate;
	UPROPERTY() TObjectPtr<UWidget> ClockPlate;
	UPROPERTY() TObjectPtr<UWidget> ArmorPlate;
	UPROPERTY() TArray<TObjectPtr<UImage>> ClockBulbs;
	UPROPERTY() TObjectPtr<UWidget> KeyFlashBox;
	UPROPERTY() TObjectPtr<UTextBlock> KeyFlashGlyph;
	UPROPERTY() TObjectPtr<UTextBlock> KeyFlashLabel;
	UPROPERTY() TObjectPtr<UImage> SpeedImage;
	UPROPERTY() TArray<TObjectPtr<UImage>> EndBolts;
	UPROPERTY() TObjectPtr<UTextBlock> EndHeader;
	UPROPERTY() TObjectPtr<UTextBlock> EndRetry;
	UPROPERTY() TArray<TObjectPtr<UTextBlock>> EndStatLabels;
	TMap<FName, TPair<FText, FText>> KeyInfo;
	float TitleAlpha = 1.0f;
	float StartPress = 0.0f;
	float KeyFlashAge = 100.0f;
	float SpeedAlpha = 0.0f;
	float BulbClock = 0.0f;

	TArray<FLine> HitLines;
	FLine ArmorLine;
	TArray<FLegendRow> Legend;
	TArray<FPrompt> Prompts;
	TArray<FQueuedPrompt> PromptQueue;
	TArray<FPopup> Popups;
	FTag PunchTag;
	FTag HeatTag;
	FTag LoadTag;
	TArray<FTag> RopeTags;
	TArray<FBannerItem> BannerQueue;
	FBannerItem CurrentBanner;
	float BannerAge = -1.0f;
	float CountdownAge = 100.0f;
	float VignetteStrength = 0.0f;
	float FlashAlpha = 0.0f;
	float VentGlow = 0.0f;
	float EndAge = -1.0f;
	float ActiveAge = 0.0f;
	float HitsPop = 0.0f;
	float ArmorPop = 0.0f;
	float ShownProgress = 0.0f;
	float ShownArmor = 100.0f;
	bool bStartBannerShown = false;

	UTextBlock* Txt(const FText& Text, int32 Size, const FLinearColor& Colour, EWeight Weight, int32 Tracking = 0);
	UImage* Blk(const FLinearColor& Colour, const FVector2D& Size);
	UHorizontalBox* MakeLine(FLine& Out, float Width, float Thickness, const FLinearColor& FillColour);
	void SetLine(const FLine& Line, float Alpha, const FLinearColor& FillColour) const;
	UCanvasPanelSlot* Place(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position);
	void AddFade(int32 Edge, float Extent, const FLinearColor& Colour, int32 Slices, TArray<TObjectPtr<UImage>>* Out, float BandAnchorY = 0.3f);
	void EnsureFonts(const FSlateFontInfo& EngineDefault);
	FLinearColor TextC(float Opacity = 1.0f) const;
	static FLinearColor WithA(const FLinearColor& C, float Opacity) { return FLinearColor(C.R, C.G, C.B, Opacity); }

	void BuildTree();
	void BuildHits();
	void BuildClock();
	void BuildArmor();
	void BuildLegend();
	void BuildBanner();
	void BuildPrompts();
	void BuildTag(FTag& Tag, float LineWidth);
	void BuildEnd();
	void BuildTitle();
	void BuildKeyFlash();
	void UpdateTitle(float Dt);
	UTexture2D* UITex(const TCHAR* Name);
	UImage* TexImg(const TCHAR* Name, const FVector2D& Size, float Margin = 0.0f, const FLinearColor& Tint = FLinearColor::White, bool bTileX = false);
	UWidget* Plated(const TCHAR* TexName, UWidget* Content, const FMargin& Pads, float Margin, float Width = 0.0f, bool bCenter = false, const FVector2D& BrushSize = FVector2D(320.0f));
	UWidget* Hazard(float Width, float Height);
	UWidget* KeyCap(const FText& Glyph, UTextBlock** OutText = nullptr, int32 Size = 24);
	FVector2D ViewSize() const;
	bool Project(const FVector& World, FVector2D& Out) const;
	class IOTRMatchInfo* Match() const;

	void ShowBanner(const FText& Title, const FText& Sub, bool bDanger, float Duration);
	void UpdateReadouts(float Dt);
	void UpdateBanner(float Dt);
	void UpdatePrompts(float Dt);
	void UpdateTags(float Dt);
	void UpdateTag(FTag& Tag, bool bShow, const FVector& World, float Dt);
	void UpdateFeedback(float Dt);
	void UpdateEnd(float Dt);
	/** World labels never overlap: lower-priority labels slide clear of higher ones (eased, per label). */
	void DeclutterWorldLabels(float Dt);
	TMap<TObjectPtr<UWidget>, FVector2D> LabelNudge;

	UFUNCTION() void HandleState(EOTRMatchState State, EOTRDefeat Reason);
	UFUNCTION() void HandleCountdown(int32 SecondsLeft);
	UFUNCTION() void HandleNotice(FText Title, FText Sub, bool bDanger);
	UFUNCTION() void HandlePrompt(FText Text, AActor* Anchor, FVector Offset, bool bDanger);
	UFUNCTION() void HandleInput(FName KeyId, bool bSimulated);
	UFUNCTION() void HandlePlayerHit(float Damage, bool bGuarded, EOTRPunch Punch, FVector Location);
	UFUNCTION() void HandlePopup(FText Text, FVector WorldLocation, FLinearColor Colour);
	UFUNCTION() void HandleSnap(int32 Side, FVector Location);
	UFUNCTION() void HandleFray(int32 Side, FVector Location);
	UFUNCTION() void HandleCleanHit(float Load, FVector Location);
	void Flash(const FLinearColor& Colour, float Alpha);
};
