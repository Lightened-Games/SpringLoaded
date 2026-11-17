// Copyright On the Ropes. All Rights Reserved.

#include "UI/OTRHUDWidget.h"
#include "Data/OTRHUDStyle.h"
#include "Data/OTRTuning.h"
#include "Framework/OTREventSubsystem.h"
#include "Framework/OTRPlayerController.h"
#include "Interfaces/OTRInterfaces.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Engine/Texture2D.h"
#include "Framework/OTREventSubsystem.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "Engine/World.h"
#include "Fonts/CompositeFont.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "OntheRopes"

namespace
{
	void SetImageSize(UImage* Img, const FVector2D& Size)
	{
		if (!Img) { return; }
		FSlateBrush Brush = Img->GetBrush();
		Brush.ImageSize = Size;
		Img->SetBrush(Brush);
	}

	template <typename SlotT>
	SlotT* Pad(SlotT* Slot, const FMargin& Margin)
	{
		if (Slot) { Slot->SetPadding(Margin); }
		return Slot;
	}

	void ShearWidget(UWidget* W, float ShearDegrees)
	{
		if (!W) { return; }
		FWidgetTransform T = W->GetRenderTransform();
		T.Shear = FVector2D(ShearDegrees, 0.0f);
		W->SetRenderTransform(T);
	}

	void ScaleWidget(UWidget* W, float Scale)
	{
		if (!W) { return; }
		FWidgetTransform T = W->GetRenderTransform();
		T.Scale = FVector2D(Scale);
		W->SetRenderTransform(T);
	}

	void OffsetWidget(UWidget* W, const FVector2D& Offset)
	{
		if (!W) { return; }
		FWidgetTransform T = W->GetRenderTransform();
		T.Translation = Offset;
		W->SetRenderTransform(T);
	}

	float EaseOut(float T) { T = FMath::Clamp(T, 0.0f, 1.0f); return 1.0f - (1.0f - T) * (1.0f - T); }

	FText Clock(float Seconds)
	{
		const int32 S = FMath::Max(0, FMath::CeilToInt(Seconds));
		return FText::FromString(FString::Printf(TEXT("%02d:%02d"), S / 60, S % 60));
	}

	template <typename T>
	T* InterfaceOf(AActor* A) { return A ? Cast<T>(A) : nullptr; }
}

IOTRMatchInfo* UOTRHUDWidget::Match() const
{
	const UWorld* World = GetWorld();
	return World ? Cast<IOTRMatchInfo>(World->GetAuthGameMode()) : nullptr;
}

FLinearColor UOTRHUDWidget::TextC(float Opacity) const
{
	return FLinearColor(Style->Text.R, Style->Text.G, Style->Text.B, Style->Text.A * Opacity);
}

// ---------------------------------------------------------------------------------------------- factories

void UOTRHUDWidget::EnsureFonts(const FSlateFontInfo& EngineDefault)
{
	if (bFontsBuilt)
	{
		return;
	}
	bFontsBuilt = true;
	const UFont* Roboto = Cast<UFont>(EngineDefault.FontObject);
	const FCompositeFont* RobotoComposite = Roboto ? Roboto->GetCompositeFont() : nullptr;
	auto Build = [this, RobotoComposite](const TSoftObjectPtr<UFontFace>& FaceRef) -> TSharedPtr<FCompositeFont>
	{
		UFontFace* Face = FaceRef.LoadSynchronous();
		if (!Face)
		{
			return nullptr;
		}
		Faces.Add(Face);
		TSharedPtr<FCompositeFont> Composite = MakeShared<FCompositeFont>();
		FTypefaceEntry& Entry = Composite->DefaultTypeface.Fonts.Add_GetRef(FTypefaceEntry(TEXT("Regular")));
		Entry.Font = FFontData(Face);
		if (RobotoComposite)
		{
			Composite->FallbackTypeface.Typeface = RobotoComposite->DefaultTypeface;
		}
		return Composite;
	};
	RegularFont = Build(Style->RegularFace);
	BoldFont = Build(Style->BoldFace);
}

UTextBlock* UOTRHUDWidget::Txt(const FText& Text, int32 Size, const FLinearColor& Colour, EWeight Weight, int32 Tracking)
{
	UTextBlock* Block = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	Block->SetText(Text);
	FSlateFontInfo Font = Block->GetFont();
	EnsureFonts(Font);
	const TSharedPtr<FCompositeFont>& Composite = Weight == EWeight::Bold ? BoldFont : RegularFont;
	if (Composite.IsValid())
	{
		Font = FSlateFontInfo(Composite, Size, TEXT("Regular"));
	}
	else
	{
		Font.TypefaceFontName = Weight == EWeight::Bold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
	}
	Font.Size = Size;
	Font.LetterSpacing = Tracking;
	Font.OutlineSettings.OutlineSize = 0;
	Block->SetFont(Font);
	Block->SetColorAndOpacity(FSlateColor(Colour));
	Block->SetShadowOffset(FVector2D(0.0f, 1.0f));
	Block->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.45f));
	Block->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Block;
}

UImage* UOTRHUDWidget::Blk(const FLinearColor& Colour, const FVector2D& Size)
{
	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush Brush;
	Brush.DrawAs = ESlateBrushDrawType::Box;
	Brush.ImageSize = Size;
	Img->SetBrush(Brush);
	Img->SetColorAndOpacity(Colour);
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Img;
}

UHorizontalBox* UOTRHUDWidget::MakeLine(FLine& Out, float Width, float Thickness, const FLinearColor& FillColour)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	Out.Width = Width;
	Out.Thickness = Thickness;
	Out.Fill = Blk(FillColour, FVector2D(0.0f, Thickness));
	Out.Rest = Blk(Style->Track, FVector2D(Width, Thickness));
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Out.Fill)) { S->SetVerticalAlignment(VAlign_Center); }
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Out.Rest)) { S->SetVerticalAlignment(VAlign_Center); }
	return Row;
}

void UOTRHUDWidget::SetLine(const FLine& Line, float Alpha, const FLinearColor& FillColour) const
{
	const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
	SetImageSize(Line.Fill, FVector2D(FMath::Max(0.0f, Line.Width * A), Line.Thickness));
	SetImageSize(Line.Rest, FVector2D(FMath::Max(0.0f, Line.Width * (1.0f - A)), Line.Thickness));
	if (Line.Fill) { Line.Fill->SetColorAndOpacity(FillColour); }
}

UCanvasPanelSlot* UOTRHUDWidget::Place(UWidget* Widget, const FAnchors& Anchors, const FVector2D& Alignment, const FVector2D& Position)
{
	UCanvasPanelSlot* S = Root->AddChildToCanvas(Widget);
	if (S)
	{
		S->SetAnchors(Anchors);
		S->SetAlignment(Alignment);
		S->SetPosition(Position);
		S->SetAutoSize(true);
	}
	return S;
}

void UOTRHUDWidget::AddFade(int32 Edge, float Extent, const FLinearColor& Colour, int32 Slices, TArray<TObjectPtr<UImage>>* Out, float BandAnchorY)
{
	// Nested slices accumulate into a seamless soft scrim (Shift Supervisor AddFade): never a box.
	const int32 N = FMath::Max(1, Slices);
	const float SliceAlpha = 1.0f - FMath::Pow(1.0f - FMath::Clamp(Colour.A, 0.0f, 0.99f), 1.0f / N);
	for (int32 I = 0; I < N; ++I)
	{
		UImage* Slice = Blk(FLinearColor(Colour.R, Colour.G, Colour.B, SliceAlpha), FVector2D(4.0f));
		UCanvasPanelSlot* S = Root->AddChildToCanvas(Slice);
		if (!S)
		{
			continue;
		}
		const float Depth = Extent * (I + 1) / N;
		switch (Edge)
		{
		case 0: S->SetAnchors(FAnchors(0, 0, 1, 0)); S->SetOffsets(FMargin(0, 0, 0, Depth)); break;
		case 1: S->SetAnchors(FAnchors(0, 1, 1, 1)); S->SetOffsets(FMargin(0, -Depth, 0, Depth)); break;
		case 2: S->SetAnchors(FAnchors(0, 0, 0, 1)); S->SetOffsets(FMargin(0, 0, Depth, 0)); break;
		case 3: S->SetAnchors(FAnchors(1, 0, 1, 1)); S->SetOffsets(FMargin(-Depth, 0, Depth, 0)); break;
		default: S->SetAnchors(FAnchors(0, BandAnchorY, 1, BandAnchorY)); S->SetOffsets(FMargin(0, -Depth * 0.5f, 0, Depth)); break;
		}
		if (Out)
		{
			Out->Add(Slice);
		}
	}
}

FVector2D UOTRHUDWidget::ViewSize() const
{
	return UWidgetLayoutLibrary::GetViewportSize(this) / FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
}

bool UOTRHUDWidget::Project(const FVector& World, FVector2D& Out) const
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC)
	{
		return false;
	}
	FVector2D Screen = FVector2D::ZeroVector;
	if (!UGameplayStatics::ProjectWorldToScreen(PC, World, Screen, true))
	{
		return false;
	}
	Out = Screen / FMath::Max(UWidgetLayoutLibrary::GetViewportScale(this), 0.01f);
	const FVector2D V = ViewSize();
	return Out.X > -50.0f && Out.Y > -50.0f && Out.X < V.X + 50.0f && Out.Y < V.Y + 50.0f;
}

// ---------------------------------------------------------------------------------------------- fight-night steel

UTexture2D* UOTRHUDWidget::UITex(const TCHAR* Name)
{
	const FName Key(Name);
	if (const TObjectPtr<UTexture2D>* Found = UITextures.Find(Key))
	{
		return *Found;
	}
	UTexture2D* Tex = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/OntheRopes/Textures/T_OTR_UI_%s.T_OTR_UI_%s"), Name, Name));
	UITextures.Add(Key, Tex);
	return Tex;
}

UImage* UOTRHUDWidget::TexImg(const TCHAR* Name, const FVector2D& Size, float Margin, const FLinearColor& Tint, bool bTileX)
{
	UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
	FSlateBrush Brush;
	Brush.SetResourceObject(UITex(Name));
	Brush.ImageSize = Size;
	Brush.DrawAs = Margin > 0.0f ? ESlateBrushDrawType::Box : ESlateBrushDrawType::Image;
	Brush.Margin = FMargin(Margin);
	Brush.Tiling = bTileX ? ESlateBrushTileType::Horizontal : ESlateBrushTileType::NoTile;
	Img->SetBrush(Brush);
	Img->SetColorAndOpacity(Tint);
	Img->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Img;
}

UWidget* UOTRHUDWidget::Plated(const TCHAR* TexName, UWidget* Content, const FMargin& Pads, float Margin, float Width, bool bCenter, const FVector2D& BrushSize)
{
	UOverlay* O = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
	// The plate stretches to its content and never sets the size itself (its brush size only scales the bevelled corners).
	USizeBox* Bg = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Bg->SetMaxDesiredWidth(1.0f);
	Bg->SetMaxDesiredHeight(1.0f);
	// Plates and slots are see-through so the fight shows behind them; text, bars and key caps stay solid.
	const bool bSurface = FCString::Strcmp(TexName, TEXT("Plate")) == 0 || FCString::Strcmp(TexName, TEXT("Slot")) == 0;
	Bg->SetContent(TexImg(TexName, BrushSize, Margin, FLinearColor(1.0f, 1.0f, 1.0f, bSurface ? Style->PlateOpacity : 1.0f)));
	if (UOverlaySlot* S = O->AddChildToOverlay(Bg))
	{
		S->SetHorizontalAlignment(HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Fill);
	}
	if (UOverlaySlot* S = O->AddChildToOverlay(Content))
	{
		S->SetPadding(Pads);
		S->SetHorizontalAlignment(bCenter ? HAlign_Center : HAlign_Fill);
		S->SetVerticalAlignment(VAlign_Center);
	}
	O->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (Width <= 0.0f)
	{
		return O;
	}
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Width);
	Box->SetContent(O);
	Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Box;
}

UWidget* UOTRHUDWidget::Hazard(float Width, float Height)
{
	USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Box->SetWidthOverride(Width);
	Box->SetHeightOverride(Height);
	Box->SetContent(TexImg(TEXT("Hazard"), FVector2D(256.0f * Height / 32.0f, Height), 0.0f, FLinearColor::White, true));
	Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Box;
}

UWidget* UOTRHUDWidget::KeyCap(const FText& Glyph, UTextBlock** OutText, int32 Size)
{
	UTextBlock* T = Txt(Glyph, Size, FLinearColor(0.035f, 0.035f, 0.04f, 1.0f), EWeight::Bold, 60);
	T->SetShadowColorAndOpacity(FLinearColor(1.0f, 1.0f, 1.0f, 0.25f));
	T->SetJustification(ETextJustify::Center);
	if (OutText)
	{
		*OutText = T;
	}
	USizeBox* Min = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Min->SetMinDesiredWidth(Size * 2.6f);
	Min->SetContent(Plated(TEXT("Key"), T, FMargin(12, 3, 12, 6), 0.3f, 0.0f, true, FVector2D(40.0f)));
	Min->SetVisibility(ESlateVisibility::HitTestInvisible);
	return Min;
}

void UOTRHUDWidget::BuildTitle()
{
	// The fight card (start screen): a riveted league plate on the left, the live hall orbiting behind it.
	const float W = 880.0f;
	const int32 Big = FMath::RoundToInt(Style->BarLabelSize);
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(Hazard(W - 56.0f, 22.0f)), FMargin(0, 0, 0, 10))) { S->SetHorizontalAlignment(HAlign_Center); }
	UHorizontalBox* Bulbs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 I = 0; I < 16; ++I)
	{
		UImage* B = TexImg(TEXT("Bulb"), FVector2D(26.0f));
		Pad(Bulbs->AddChildToHorizontalBox(B), FMargin(8, 0));
		TitleBulbs.Add(B);
	}
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(Bulbs), FMargin(0, 0, 0, 8))) { S->SetHorizontalAlignment(HAlign_Center); }
	UTextBlock* League = Txt(LOCTEXT("TitleLeague", "IRON LEAGUE   \u00B7   HALL 9   \u00B7   TONIGHT"), Big, WithA(Style->Accent, 1.0f), EWeight::Bold, 300);
	Pad(Col->AddChildToVerticalBox(League), FMargin(6, 0, 0, 0));
	UTextBlock* Name = Txt(LOCTEXT("TitleName", "ON THE ROPES"), FMath::RoundToInt(Style->HeadlineSize * 1.8f), TextC(), EWeight::Bold, Style->HeadlineTracking);
	Pad(Col->AddChildToVerticalBox(Name), FMargin(2, -6, 0, -4));
	UTextBlock* Tag = Txt(LOCTEXT("TitleTag", "Your old mech can barely punch. Soak the champion's hits on the ropes, launch when it vents."), Big, TextC(0.88f), EWeight::Regular, 20);
	Tag->SetAutoWrapText(true);
	Tag->SetWrapTextAt(W - 70.0f);
	Pad(Col->AddChildToVerticalBox(Tag), FMargin(6, 0, 0, 10));
	UTextBlock* Bout = Txt(LOCTEXT("TitleBout", "TITAN, THE CHAMPION    vs    OLD IRON, YOU"), Big, TextC(Style->SecondaryOpacity), EWeight::Bold, 160);
	Pad(Col->AddChildToVerticalBox(Bout), FMargin(6, 0, 0, 12));
	Pad(Col->AddChildToVerticalBox(Plated(TEXT("Slot"), LegendBox, FMargin(18, 12, 18, 12), 0.25f, 0.0f, false, FVector2D(48.0f))), FMargin(0, 0, 0, 16));
	// Buttons: steel plates with the key on a cap.
	auto Button = [this, Big](const FText& Key, const FText& Label, bool bPrimary, UImage** OutBar)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		UImage* Bar = Blk(bPrimary ? Style->Accent : FLinearColor(0, 0, 0, 0), FVector2D(10.0f, 58.0f));
		if (OutBar) { *OutBar = Bar; }
		if (UHorizontalBoxSlot* S = Pad(Row->AddChildToHorizontalBox(Bar), FMargin(0, 0, 10, 0))) { S->SetVerticalAlignment(VAlign_Center); }
		UHorizontalBox* Inner = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Pad(Inner->AddChildToHorizontalBox(KeyCap(Key, nullptr, 24)), FMargin(0, 0, 16, 0))) { S->SetVerticalAlignment(VAlign_Center); }
		UTextBlock* T = Txt(Label, bPrimary ? Big + 6 : Big, bPrimary ? TextC() : TextC(Style->SecondaryOpacity), EWeight::Bold, 200);
		if (UHorizontalBoxSlot* S = Inner->AddChildToHorizontalBox(T)) { S->SetVerticalAlignment(VAlign_Center); }
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Plated(TEXT("Button"), Inner, FMargin(16, 8, 26, 10), 0.2f, 0.0f, false, FVector2D(64.0f)))) { S->SetVerticalAlignment(VAlign_Center); }
		return Row;
	};
	UImage* Bar = nullptr;
	Pad(Col->AddChildToVerticalBox(Button(LOCTEXT("KeyEnter", "ENTER"), LOCTEXT("TitleStart", "STEP INTO THE RING"), true, &Bar)), FMargin(0, 0, 0, 10));
	StartBar = Bar;
	Col->AddChildToVerticalBox(Button(LOCTEXT("KeyEsc", "ESC"), LOCTEXT("TitleQuit", "THROW IN THE TOWEL"), false, nullptr));
	TitleCard = Plated(TEXT("Plate"), Col, FMargin(28, 24, 28, 28), 0.08f, W);
	Place(TitleCard, FAnchors(0.0f, 0.5f), FVector2D(0.0f, 0.5f), FVector2D(Style->Margin + 20.0f, 0.0f));
}

void UOTRHUDWidget::BuildKeyFlash()
{
	// In play the controls live on the fight card only; each key you press flashes here (cap + the move's name).
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UTextBlock* Glyph = nullptr;
	if (UHorizontalBoxSlot* S = Pad(Row->AddChildToHorizontalBox(KeyCap(FText::GetEmpty(), &Glyph, 26)), FMargin(0, 0, 14, 0))) { S->SetVerticalAlignment(VAlign_Center); }
	KeyFlashGlyph = Glyph;
	KeyFlashLabel = Txt(FText::GetEmpty(), FMath::RoundToInt(Style->BarLabelSize), TextC(), EWeight::Bold, 160);
	if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(KeyFlashLabel)) { S->SetVerticalAlignment(VAlign_Center); }
	KeyFlashBox = Plated(TEXT("Plate"), Row, FMargin(28, 16, 30, 16), 0.08f);
	KeyFlashBox->SetRenderOpacity(0.0f);
	KeyFlashBox->SetRenderTransformPivot(FVector2D(0.0f, 1.0f));
	Place(KeyFlashBox, FAnchors(0, 1), FVector2D(0, 1), FVector2D(Style->Margin, -Style->Margin));
}

void UOTRHUDWidget::UpdateTitle(float Dt)
{
	const IOTRMatchInfo* M = Match();
	const bool bTitle = M && M->OTR_IsTitle();
	TitleAlpha = FMath::FInterpTo(TitleAlpha, bTitle ? 1.0f : 0.0f, Dt, bTitle ? 8.0f : 5.0f);
	if (TitleCard)
	{
		TitleCard->SetRenderOpacity(TitleAlpha);
		OffsetWidget(TitleCard, FVector2D(-(1.0f - TitleAlpha) * 80.0f, 0.0f));
	}
	const float Hud = 1.0f - TitleAlpha;
	for (UWidget* W : { ChampPlate.Get(), ClockPlate.Get(), ArmorPlate.Get() })
	{
		if (W) { W->SetRenderOpacity(Hud); }
	}
	// Marquee bulbs chase along the card's top edge.
	BulbClock += Dt;
	const int32 Step = FMath::FloorToInt(BulbClock * 9.0f);
	for (int32 I = 0; I < TitleBulbs.Num(); ++I)
	{
		const bool bOn = (I + Step) % 3 == 0;
		TitleBulbs[I]->SetColorAndOpacity(bOn ? FLinearColor::White : FLinearColor(0.35f, 0.22f, 0.12f, 0.8f));
	}
	StartPress = FMath::Max(0.0f, StartPress - Dt * 2.5f);
	if (StartBar)
	{
		const float Pulse = 0.5f + 0.5f * FMath::Sin(BulbClock * 4.0f);
		StartBar->SetColorAndOpacity(FMath::Lerp(WithA(Style->Accent, 0.65f + 0.35f * Pulse), FLinearColor::White, StartPress));
		SetImageSize(StartBar, FVector2D(10.0f + 14.0f * StartPress, 58.0f));
	}
	// Key flash: pops in, holds, fades.
	KeyFlashAge += Dt;
	if (KeyFlashBox)
	{
		const float A = KeyFlashAge < 0.95f ? 1.0f - EaseOut((KeyFlashAge - 0.65f) / 0.3f) * (KeyFlashAge > 0.65f ? 1.0f : 0.0f) : 0.0f;
		KeyFlashBox->SetRenderOpacity(A * Hud);
		ScaleWidget(KeyFlashBox, Style->HudScale * (1.0f + 0.12f * (1.0f - EaseOut(KeyFlashAge / 0.14f))));
	}
	// Launch: speed streaks rush in from the screen edges.
	const IOTRBoxer* Boxer = InterfaceOf<IOTRBoxer>(GetOwningPlayerPawn());
	const bool bLaunch = Boxer && Boxer->OTR_IsLaunching();
	SpeedAlpha = FMath::FInterpTo(SpeedAlpha, bLaunch ? Style->LaunchStreaks : 0.0f, Dt, bLaunch ? 14.0f : 5.0f);
	if (SpeedImage)
	{
		SpeedImage->SetRenderOpacity(SpeedAlpha);
		SpeedImage->SetRenderTransformPivot(FVector2D(0.5f));
		ScaleWidget(SpeedImage, 1.05f + 0.04f * FMath::Sin(BulbClock * 47.0f));
	}
}

// ---------------------------------------------------------------------------------------------- layout

TSharedRef<SWidget> UOTRHUDWidget::RebuildWidget()
{
	if (WidgetTree && !WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

void UOTRHUDWidget::BuildTree()
{
	const IOTRMatchInfo* M = Match();
	const UOTRTuning* Tuning = M ? M->OTR_Tuning() : nullptr;
	Style = Tuning ? Tuning->HUDStyle.LoadSynchronous() : nullptr;
	if (!Style)
	{
		Style = NewObject<UOTRHUDStyle>(this, TEXT("DefaultHUDStyle"));
	}
	Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	AddFade(0, Style->TopScrimHeight, Style->Scrim, 28, nullptr);
	AddFade(1, Style->BottomScrimHeight, Style->Scrim, 28, nullptr);
	const FLinearColor V = WithA(Style->Danger, 0.5f);
	AddFade(2, 220.0f, V, 18, &VignetteImages);
	AddFade(3, 220.0f, V, 18, &VignetteImages);
	AddFade(0, 150.0f, V, 14, &VignetteImages);
	AddFade(1, 150.0f, V, 14, &VignetteImages);
	for (UImage* Img : VignetteImages)
	{
		Img->SetRenderOpacity(0.0f);
	}
	const FLinearColor VA = WithA(Style->Accent, 0.45f);
	AddFade(2, 260.0f, VA, 18, &VentImages);
	AddFade(3, 260.0f, VA, 18, &VentImages);
	AddFade(0, 170.0f, VA, 14, &VentImages);
	AddFade(1, 170.0f, VA, 14, &VentImages);
	for (UImage* Img : VentImages)
	{
		Img->SetRenderOpacity(0.0f);
	}
	FlashImage = Blk(FLinearColor::White, FVector2D(8.0f));
	FlashImage->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(FlashImage))
	{
		S->SetAnchors(FAnchors(0, 0, 1, 1));
		S->SetOffsets(FMargin(0));
	}
	SpeedImage = TexImg(TEXT("Speed"), FVector2D(1024.0f));
	SpeedImage->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(SpeedImage))
	{
		S->SetAnchors(FAnchors(0, 0, 1, 1));
		S->SetOffsets(FMargin(0));
	}
	// World tags sit under everything else.
	BuildTag(HeatTag, 64.0f);
	BuildTag(PunchTag, 40.0f);
	BuildTag(LoadTag, 56.0f);
	RopeTags.SetNum(4);
	for (FTag& T : RopeTags)
	{
		BuildTag(T, 48.0f);
	}
	BuildPrompts();
	BuildHits();
	BuildClock();
	BuildArmor();
	BuildLegend();
	BuildBanner();
	BuildEnd();
	BuildKeyFlash();
	BuildTitle();
}

void UOTRHUDWidget::BuildTag(FTag& Tag, float LineWidth)
{
	Tag.Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Tag.Text = Txt(FText::GetEmpty(), Style->LabelSize, TextC(), EWeight::Bold, Style->LabelTracking);
	if (UVerticalBoxSlot* S = Tag.Box->AddChildToVerticalBox(Tag.Text)) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = Pad(Tag.Box->AddChildToVerticalBox(MakeLine(Tag.Line, LineWidth, 2.0f, Style->Accent)), FMargin(0, 4, 0, 0))) { S->SetHorizontalAlignment(HAlign_Center); }
	Tag.Box->SetVisibility(ESlateVisibility::HitTestInvisible);
	Tag.Box->SetRenderOpacity(0.0f);
	Tag.Box->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Tag.Box))
	{
		S->SetAlignment(FVector2D(0.5f, 1.0f));
		S->SetAutoSize(true);
		S->SetZOrder(-1);
	}
}

void UOTRHUDWidget::BuildHits()
{
	// Champion plate: hazard header, the name, a recessed slot with one red segment per launch hit still needed.
	const int32 Big = FMath::RoundToInt(Style->BarLabelSize);
	const float W = Style->BarSegmentWidth * 3.0f + 12.0f;
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Pad(Col->AddChildToVerticalBox(Hazard(W + 16.0f, 12.0f)), FMargin(0, 0, 0, 8));
	UHorizontalBox* Name = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	HitsLabel = Txt(LOCTEXT("HitsLabel", "TITAN"), Big, TextC(), EWeight::Bold, Style->LabelTracking);
	Name->AddChildToHorizontalBox(HitsLabel);
	UTextBlock* Sub = Txt(LOCTEXT("HitsSub", "   CHAMPION"), Big, WithA(Style->Danger, 0.9f), EWeight::Bold, 200);
	Name->AddChildToHorizontalBox(Sub);
	Pad(Col->AddChildToVerticalBox(Name), FMargin(0, 0, 0, 6));
	UHorizontalBox* Lines = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const int32 Count = Match() ? FMath::Max(1, FMath::RoundToInt(Match()->OTR_Tuning()->Match.KnockdownTarget)) : 3;
	HitLines.SetNum(Count);
	for (int32 I = 0; I < Count; ++I)
	{
		Pad(Lines->AddChildToHorizontalBox(MakeLine(HitLines[I], Style->BarSegmentWidth, Style->BarThickness, Style->Danger)), FMargin(0, 0, I + 1 < Count ? 6 : 0, 0));
	}
	Pad(Col->AddChildToVerticalBox(Plated(TEXT("Slot"), Lines, FMargin(8, 7, 8, 7), 0.25f, 0.0f, false, FVector2D(48.0f))), FMargin(0, 0, 0, 6));
	HitsValue = Txt(FText::FromString(TEXT("3 HITS TO KNOCK OUT")), Big, TextC(Style->SecondaryOpacity), EWeight::Bold, 100);
	HitsValue->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
	Col->AddChildToVerticalBox(HitsValue);
	ChampPlate = Plated(TEXT("Plate"), Col, FMargin(32, 20, 32, 22), 0.08f);
	Place(ChampPlate, FAnchors(0, 0), FVector2D(0, 0), FVector2D(Style->Margin, Style->Margin - 6.0f));
	ChampPlate->SetRenderTransformPivot(FVector2D(0.0f, 0.0f));
	ScaleWidget(ChampPlate, Style->HudScale);
}

void UOTRHUDWidget::BuildClock()
{
	// Scoreboard plate: ROUND, the clock, and a row of bulbs that go out as the round runs down.
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	ClockLabel = Txt(LOCTEXT("TimeLabel", "ROUND"), FMath::RoundToInt(Style->BarLabelSize), TextC(Style->SecondaryOpacity), EWeight::Bold, Style->LabelTracking);
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(ClockLabel)) { S->SetHorizontalAlignment(HAlign_Center); }
	ClockText = Txt(FText::FromString(TEXT("01:30")), FMath::RoundToInt(Style->HeadlineSize * 1.15f), TextC(), EWeight::Bold, 60);
	if (UVerticalBoxSlot* S = Pad(Col->AddChildToVerticalBox(ClockText), FMargin(0, -8, 0, 2))) { S->SetHorizontalAlignment(HAlign_Center); }
	UHorizontalBox* Bulbs = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 I = 0; I < 6; ++I)
	{
		UImage* B = TexImg(TEXT("Bulb"), FVector2D(22.0f));
		Pad(Bulbs->AddChildToHorizontalBox(B), FMargin(4, 0));
		ClockBulbs.Add(B);
	}
	if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Bulbs)) { S->SetHorizontalAlignment(HAlign_Center); }
	ClockPlate = Plated(TEXT("Plate"), Col, FMargin(34, 18, 34, 22), 0.08f, 0.0f, true);
	Place(ClockPlate, FAnchors(1, 0), FVector2D(1, 0), FVector2D(-Style->Margin, Style->Margin - 6.0f));
	ClockPlate->SetRenderTransformPivot(FVector2D(1.0f, 0.0f));
	ScaleWidget(ClockPlate, Style->HudScale);
}

void UOTRHUDWidget::BuildArmor()
{
	// Old Iron plate: hazard header, name and the armor number, an amber bar in a recessed slot.
	const int32 Big = FMath::RoundToInt(Style->BarLabelSize);
	const float W = Style->BarSegmentWidth * 3.0f + 12.0f;
	UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	Pad(Col->AddChildToVerticalBox(Hazard(W + 16.0f, 12.0f)), FMargin(0, 0, 0, 6));
	UHorizontalBox* Name = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	ArmorLabel = Txt(LOCTEXT("ArmorLabel", "OLD IRON"), Big, TextC(), EWeight::Bold, Style->LabelTracking);
	if (UHorizontalBoxSlot* S = Name->AddChildToHorizontalBox(ArmorLabel)) { S->SetVerticalAlignment(VAlign_Center); }
	UTextBlock* Sub = Txt(LOCTEXT("ArmorSub", "   ARMOR"), Big, WithA(Style->Accent, 0.9f), EWeight::Bold, 200);
	if (UHorizontalBoxSlot* S = Name->AddChildToHorizontalBox(Sub)) { S->SetVerticalAlignment(VAlign_Center); S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); }
	ArmorValue = Txt(FText::AsNumber(100), Style->HeadlineSize, TextC(), EWeight::Bold);
	ArmorValue->SetRenderTransformPivot(FVector2D(1.0f, 0.5f));
	if (UHorizontalBoxSlot* S = Pad(Name->AddChildToHorizontalBox(ArmorValue), FMargin(28, 0, 0, 0))) { S->SetVerticalAlignment(VAlign_Center); }
	Pad(Col->AddChildToVerticalBox(Name), FMargin(0, -4, 0, 2));
	Col->AddChildToVerticalBox(Plated(TEXT("Slot"), MakeLine(ArmorLine, W, Style->BarThickness, Style->Accent), FMargin(8, 7, 8, 7), 0.25f, 0.0f, false, FVector2D(48.0f)));
	ArmorPlate = Plated(TEXT("Plate"), Col, FMargin(32, 20, 32, 22), 0.08f);
	Place(ArmorPlate, FAnchors(1, 1), FVector2D(1, 1), FVector2D(-Style->Margin, -Style->Margin));
	ArmorPlate->SetRenderTransformPivot(FVector2D(1.0f, 1.0f));
	ScaleWidget(ArmorPlate, Style->HudScale);
}

void UOTRHUDWidget::BuildLegend()
{
	// The controls: two columns of key caps on the fight card (never on screen during the bout, 2.10).
	struct FRowDef { FName Key; FText Glyph; FText Label; };
	const TArray<FRowDef> Rows = {
		{ AOTRPlayerController::KeyMove, LOCTEXT("GlyphMove", "WASD"), LOCTEXT("RowMove", "Move") },
		{ AOTRPlayerController::KeyGuardHigh, LOCTEXT("GlyphHigh", "LMB"), LOCTEXT("RowHigh", "Guard high") },
		{ AOTRPlayerController::KeyGuardLow, LOCTEXT("GlyphLow", "RMB"), LOCTEXT("RowLow", "Guard low") },
		{ AOTRPlayerController::KeyDodge, LOCTEXT("GlyphDodge", "SHIFT"), LOCTEXT("RowDodge", "Dodge") },
		{ AOTRPlayerController::KeyDuck, LOCTEXT("GlyphDuck", "C"), LOCTEXT("RowDuck", "Duck") },
		{ AOTRPlayerController::KeyJab, LOCTEXT("GlyphJab", "E"), LOCTEXT("RowJab", "Jab") },
		{ AOTRPlayerController::KeyBreak, LOCTEXT("GlyphBreak", "Q"), LOCTEXT("RowBreak", "Guard break / finish") },
		{ AOTRPlayerController::KeyLean, LOCTEXT("GlyphLean", "SPACE"), LOCTEXT("RowLean", "Ropes: hold, release") },
	};
	KeyInfo.Add(AOTRPlayerController::KeyStart, TPair<FText, FText>(LOCTEXT("GlyphStart", "ENTER"), LOCTEXT("RowStart", "Step into the ring")));
	KeyInfo.Add(AOTRPlayerController::KeyRestart, TPair<FText, FText>(LOCTEXT("GlyphRestart", "R"), LOCTEXT("RowRestart", "Rematch")));
	const int32 Big = FMath::RoundToInt(Style->BarLabelSize);
	LegendBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UHorizontalBox* Cols = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	UVerticalBox* ColBoxes[2] = { WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()), WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass()) };
	for (int32 I = 0; I < Rows.Num(); ++I)
	{
		const FRowDef& Def = Rows[I];
		KeyInfo.Add(Def.Key, TPair<FText, FText>(Def.Glyph, Def.Label));
		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		FLegendRow Row;
		Row.Key = Def.Key;
		USizeBox* KeyCol = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		KeyCol->SetWidthOverride(112.0f);
		UTextBlock* GlyphText = nullptr;
		KeyCol->SetContent(KeyCap(Def.Glyph, &GlyphText, 20));
		Row.Glyph = GlyphText;
		if (UHorizontalBoxSlot* S = Pad(Line->AddChildToHorizontalBox(KeyCol), FMargin(0, 0, 12, 0))) { S->SetVerticalAlignment(VAlign_Center); }
		Row.Label = Txt(Def.Label, Big, TextC(0.9f), EWeight::Regular, 40);
		if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Row.Label)) { S->SetVerticalAlignment(VAlign_Center); }
		Pad(ColBoxes[I / 4]->AddChildToVerticalBox(Line), FMargin(0, 3));
		Legend.Add(Row);
	}
	Pad(Cols->AddChildToHorizontalBox(ColBoxes[0]), FMargin(0, 0, 30, 0));
	Cols->AddChildToHorizontalBox(ColBoxes[1]);
	LegendBox->AddChildToVerticalBox(Cols);
}

void UOTRHUDWidget::BuildBanner()
{
	// Event banners ride on a steel plate with a hazard header (danger turns the rule and title red).
	BannerBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = Pad(Inner->AddChildToVerticalBox(Hazard(560.0f, 14.0f)), FMargin(0, 0, 0, 8))) { S->SetHorizontalAlignment(HAlign_Center); }
	BannerTitle = Txt(FText::GetEmpty(), Style->HeadlineSize, TextC(), EWeight::Bold, Style->HeadlineTracking);
	BannerTitle->SetJustification(ETextJustify::Center);
	BannerRule = Blk(Style->Accent, FVector2D(84.0f, 4.0f));
	ShearWidget(BannerRule, Style->ChipShear * 2.0f);
	BannerSub = Txt(FText::GetEmpty(), FMath::RoundToInt(Style->BarLabelSize), TextC(0.9f), EWeight::Regular, 20);
	BannerSub->SetJustification(ETextJustify::Center);
	BannerSub->SetWrapTextAt(620.0f);
	if (UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(BannerTitle)) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = Pad(Inner->AddChildToVerticalBox(BannerRule), FMargin(0, 4, 0, 8))) { S->SetHorizontalAlignment(HAlign_Center); }
	if (UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(BannerSub)) { S->SetHorizontalAlignment(HAlign_Center); }
	BannerBox->AddChildToVerticalBox(Plated(TEXT("Plate"), Inner, FMargin(30, 18, 30, 22), 0.08f, 0.0f, true));
	BannerBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	BannerBox->SetRenderOpacity(0.0f);
	Place(BannerBox, FAnchors(0.5f, 0.2f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
	BannerBox->SetRenderTransformPivot(FVector2D(0.5f, 0.0f));
	ScaleWidget(BannerBox, Style->HudScale);

	CountdownText = Txt(FText::GetEmpty(), Style->HeadlineSize, TextC(), EWeight::Bold);
	CountdownText->SetJustification(ETextJustify::Center);
	CountdownText->SetRenderOpacity(0.0f);
	CountdownText->SetRenderTransformPivot(FVector2D(0.5f));
	CountdownText->SetShadowOffset(FVector2D(0.0f, 4.0f));
	CountdownText->SetShadowColorAndOpacity(FLinearColor(0.0f, 0.0f, 0.0f, 0.7f));
	Place(CountdownText, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D(0.0f, 30.0f));
}

void UOTRHUDWidget::BuildPrompts()
{
	for (int32 I = 0; I < 2; ++I)
	{
		FPrompt P;
		P.Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		P.Text = Txt(FText::GetEmpty(), Style->LabelSize, TextC(), EWeight::Bold, Style->LabelTracking);
		if (UVerticalBoxSlot* S = P.Box->AddChildToVerticalBox(P.Text)) { S->SetHorizontalAlignment(HAlign_Center); }
		P.Rule = Blk(Style->Accent, FVector2D(40.0f, 2.0f));
		if (UVerticalBoxSlot* S = Pad(P.Box->AddChildToVerticalBox(P.Rule), FMargin(0, 4, 0, 0))) { S->SetHorizontalAlignment(HAlign_Center); }
		P.Box->SetVisibility(ESlateVisibility::HitTestInvisible);
		P.Box->SetRenderOpacity(0.0f);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(P.Box))
		{
			S->SetAlignment(FVector2D(0.5f, 1.0f));
			S->SetAutoSize(true);
		}
		Prompts.Add(P);
	}
	for (int32 I = 0; I < 4; ++I)
	{
		FPopup P;
		P.Text = Txt(FText::GetEmpty(), Style->BodySize, Style->Accent, EWeight::Bold);
		P.Text->SetRenderOpacity(0.0f);
		if (UCanvasPanelSlot* S = Root->AddChildToCanvas(P.Text))
		{
			S->SetAlignment(FVector2D(0.5f, 1.0f));
			S->SetAutoSize(true);
		}
		Popups.Add(P);
	}
}

void UOTRHUDWidget::BuildEnd()
{
	// The result card (2.5.16): league header, a headline in the hall's voice, the story line, a bolt rating,
	// four stats in recessed slots, steel buttons with their keys.
	Dim = Blk(FLinearColor(0.0f, 0.0f, 0.0f, Style->EndDimOpacity), FVector2D(8.0f));
	Dim->SetRenderOpacity(0.0f);
	if (UCanvasPanelSlot* S = Root->AddChildToCanvas(Dim))
	{
		S->SetAnchors(FAnchors(0, 0, 1, 1));
		S->SetOffsets(FMargin(0));
	}
	const float W = 1120.0f;
	const int32 Big = FMath::RoundToInt(Style->BarLabelSize);
	EndBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UVerticalBox* Inner = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	if (UVerticalBoxSlot* S = Pad(Inner->AddChildToVerticalBox(Hazard(W - 72.0f, 22.0f)), FMargin(0, 0, 0, 12))) { S->SetHorizontalAlignment(HAlign_Center); }
	EndHeader = Txt(LOCTEXT("EndHeader", "IRON LEAGUE   \u00B7   HALL 9   \u00B7   RESULT"), Big, WithA(Style->Accent, 1.0f), EWeight::Bold, 300);
	if (UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(EndHeader)) { S->SetHorizontalAlignment(HAlign_Center); }
	EndTitle = Txt(FText::GetEmpty(), FMath::RoundToInt(Style->HeadlineSize * 1.9f), TextC(), EWeight::Bold, Style->HeadlineTracking + 80);
	EndTitle->SetJustification(ETextJustify::Center);
	EndTitle->SetRenderTransformPivot(FVector2D(0.5f));
	if (UVerticalBoxSlot* S = Pad(Inner->AddChildToVerticalBox(EndTitle), FMargin(0, -4, 0, 0))) { S->SetHorizontalAlignment(HAlign_Center); }
	EndRule = Blk(Style->Accent, FVector2D(110.0f, 4.0f));
	ShearWidget(EndRule, Style->ChipShear * 2.0f);
	if (UVerticalBoxSlot* S = Pad(Inner->AddChildToVerticalBox(EndRule), FMargin(0, 2, 0, 12))) { S->SetHorizontalAlignment(HAlign_Center); }
	EndReason = Txt(FText::GetEmpty(), Big, TextC(0.9f), EWeight::Regular, 20);
	EndReason->SetJustification(ETextJustify::Center);
	EndReason->SetAutoWrapText(true);
	EndReason->SetWrapTextAt(W - 140.0f);
	if (UVerticalBoxSlot* S = Pad(Inner->AddChildToVerticalBox(EndReason), FMargin(0, 0, 0, 16))) { S->SetHorizontalAlignment(HAlign_Center); }
	// Rating: three lightning bolts.
	UHorizontalBox* Bolts = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	for (int32 I = 0; I < 3; ++I)
	{
		UImage* B = TexImg(TEXT("Bolt"), FVector2D(64.0f));
		B->SetRenderTransformPivot(FVector2D(0.5f));
		Pad(Bolts->AddChildToHorizontalBox(B), FMargin(10, 0));
		EndBolts.Add(B);
	}
	if (UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(Bolts)) { S->SetHorizontalAlignment(HAlign_Center); }
	UTextBlock* RateLabel = Txt(LOCTEXT("EndRating", "LEAGUE RATING"), Big, TextC(Style->SecondaryOpacity), EWeight::Bold, Style->LabelTracking);
	if (UVerticalBoxSlot* S = Pad(Inner->AddChildToVerticalBox(RateLabel), FMargin(0, 2, 0, 16))) { S->SetHorizontalAlignment(HAlign_Center); }
	UHorizontalBox* Stats = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	const TArray<FText> Labels = { LOCTEXT("StatTime", "ROUND TIME"), LOCTEXT("StatHits", "LAUNCH HITS"), LOCTEXT("StatParries", "PARRIES"), LOCTEXT("StatArmor", "ARMOR LEFT") };
	for (int32 I = 0; I < Labels.Num(); ++I)
	{
		UVerticalBox* Stat = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
		UTextBlock* Value = Txt(FText::GetEmpty(), FMath::RoundToInt(Style->HeadlineSize * 0.9f), TextC(), EWeight::Bold);
		UTextBlock* Label = Txt(Labels[I], Big, TextC(Style->SecondaryOpacity), EWeight::Bold, 40);
		if (UVerticalBoxSlot* S = Pad(Stat->AddChildToVerticalBox(Value), FMargin(0, 0, 0, -6))) { S->SetHorizontalAlignment(HAlign_Center); }
		if (UVerticalBoxSlot* S = Stat->AddChildToVerticalBox(Label)) { S->SetHorizontalAlignment(HAlign_Center); }
		Pad(Stats->AddChildToHorizontalBox(Plated(TEXT("Slot"), Stat, FMargin(24, 12, 24, 14), 0.25f, 240.0f, true, FVector2D(48.0f))), FMargin(I == 0 ? 0 : 16, 0, 0, 0));
		EndStatValues.Add(Value);
		EndStatLabels.Add(Label);
	}
	if (UVerticalBoxSlot* S = Pad(Inner->AddChildToVerticalBox(Stats), FMargin(0, 0, 0, 28))) { S->SetHorizontalAlignment(HAlign_Center); }
	UHorizontalBox* Buttons = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
	auto Button = [this, Big](const FText& Key, UTextBlock*& OutLabel, const FText& Label, bool bPrimary)
	{
		UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		if (UHorizontalBoxSlot* S = Pad(Row->AddChildToHorizontalBox(KeyCap(Key, nullptr, 24)), FMargin(0, 0, 16, 0))) { S->SetVerticalAlignment(VAlign_Center); }
		OutLabel = Txt(Label, bPrimary ? Big + 6 : Big, bPrimary ? TextC() : TextC(Style->SecondaryOpacity), EWeight::Bold, 200);
		if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(OutLabel)) { S->SetVerticalAlignment(VAlign_Center); }
		return Plated(TEXT("Button"), Row, FMargin(22, 12, 30, 14), 0.2f, 0.0f, false, FVector2D(64.0f));
	};
	UTextBlock* Retry = nullptr;
	UTextBlock* Leave = nullptr;
	Pad(Buttons->AddChildToHorizontalBox(Button(LOCTEXT("KeyR", "R"), Retry, LOCTEXT("EndRestart", "REMATCH"), true)), FMargin(0, 0, 32, 0));
	Buttons->AddChildToHorizontalBox(Button(LOCTEXT("KeyEsc2", "ESC"), Leave, LOCTEXT("EndLeave", "LEAVE THE HALL"), false));
	EndPrompt = Retry;
	EndRetry = Retry;
	if (UVerticalBoxSlot* S = Inner->AddChildToVerticalBox(Buttons)) { S->SetHorizontalAlignment(HAlign_Center); }
	EndBox->AddChildToVerticalBox(Plated(TEXT("Plate"), Inner, FMargin(36, 26, 36, 32), 0.08f, W, true));
	EndBox->SetVisibility(ESlateVisibility::HitTestInvisible);
	EndBox->SetRenderOpacity(0.0f);
	Place(EndBox, FAnchors(0.5f, 0.5f), FVector2D(0.5f, 0.5f), FVector2D::ZeroVector);
}

// ---------------------------------------------------------------------------------------------- events

void UOTRHUDWidget::NativeConstruct()
{
	Super::NativeConstruct();
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->OnMatchState.AddDynamic(this, &UOTRHUDWidget::HandleState);
		Events->OnCountdown.AddDynamic(this, &UOTRHUDWidget::HandleCountdown);
		Events->OnNotice.AddDynamic(this, &UOTRHUDWidget::HandleNotice);
		Events->OnPrompt.AddDynamic(this, &UOTRHUDWidget::HandlePrompt);
		Events->OnInput.AddDynamic(this, &UOTRHUDWidget::HandleInput);
		Events->OnPlayerHit.AddDynamic(this, &UOTRHUDWidget::HandlePlayerHit);
		Events->OnPopup.AddDynamic(this, &UOTRHUDWidget::HandlePopup);
		Events->OnRopeSnapped.AddDynamic(this, &UOTRHUDWidget::HandleSnap);
		Events->OnRopeFrayWarning.AddDynamic(this, &UOTRHUDWidget::HandleFray);
		Events->OnCleanHit.AddDynamic(this, &UOTRHUDWidget::HandleCleanHit);
	}
}

void UOTRHUDWidget::HandleState(EOTRMatchState State, EOTRDefeat Reason)
{
	if (State == EOTRMatchState::Active)
	{
		ActiveAge = 0.0f;
	}
	if (State != EOTRMatchState::Victory && State != EOTRMatchState::Defeat)
	{
		return;
	}
	EndAge = 0.0f;
	if (State == EOTRMatchState::Victory)
	{
		Flash(FLinearColor(1.0f, 0.95f, 0.85f, 1.0f), Style->KOFlash);
	}
	BannerQueue.Reset();
	PromptQueue.Reset();
	BannerAge = -1.0f;
	const IOTRMatchInfo* M = Match();
	const bool bWon = State == EOTRMatchState::Victory;
	FText Title;
	FText Why;
	switch (Reason)
	{
	case EOTRDefeat::Armor:
		Title = LOCTEXT("EndArmor", "SCRAPPED");
		Why = LOCTEXT("WhyArmor", "Titan took Old Iron apart plate by plate. Raise the guard at the height it telegraphs.");
		break;
	case EOTRDefeat::RingOut:
		Title = LOCTEXT("EndRingOut", "RING OUT");
		Why = LOCTEXT("WhyRingOut", "Old Iron went through the broken ropes and into the front row. Stay off the open side.");
		break;
	case EOTRDefeat::Bell:
		Title = LOCTEXT("EndBell", "THE BELL");
		Why = LOCTEXT("WhyBell", "The bell rang with the champion still standing. Launch the moment it vents.");
		break;
	default:
		Title = LOCTEXT("EndWin", "KNOCKOUT");
		Why = LOCTEXT("WhyWin", "The old mech that can barely punch just put the champion on the canvas. The hall is on its feet.");
		break;
	}
	const FLinearColor Tone = bWon ? WithA(Style->Accent, 1.0f) : WithA(Style->Danger, 1.0f);
	EndTitle->SetText(Title);
	EndTitle->SetColorAndOpacity(FSlateColor(Tone));
	EndHeader->SetColorAndOpacity(FSlateColor(Tone));
	EndRule->SetColorAndOpacity(Tone);
	EndReason->SetText(Why);
	int32 Lit = 0;
	if (M)
	{
		int32 Clean = 0, Snapped = 0;
		float Armor = 0.0f;
		M->OTR_Stats(Clean, Snapped, Armor);
		const UOTRTuning* T = M->OTR_Tuning();
		const float Elapsed = T->Match.RoundSeconds - M->OTR_TimeLeft();
		const UOTREventSubsystem* Events = UOTREventSubsystem::Get(this);
		EndStatValues[0]->SetText(Clock(Elapsed));
		EndStatValues[1]->SetText(FText::AsNumber(FMath::FloorToInt(M->OTR_Progress() + 0.01f)));
		EndStatValues[2]->SetText(FText::AsNumber(Events ? Events->Parries : 0));
		EndStatValues[3]->SetText(FText::AsNumber(FMath::RoundToInt(Armor)));
		Lit = bWon ? 1 + (Armor >= Style->RatingArmor ? 1 : 0) + (Elapsed <= Style->RatingSeconds ? 1 : 0) : 0;
	}
	for (int32 I = 0; I < EndBolts.Num(); ++I)
	{
		EndBolts[I]->SetColorAndOpacity(I < Lit ? WithA(Style->Accent, 1.0f) : FLinearColor(0.16f, 0.16f, 0.17f, 1.0f));
	}
	EndPrompt->SetText(bWon ? LOCTEXT("EndRestart", "REMATCH") : LOCTEXT("EndRetry", "TRY AGAIN"));
}

void UOTRHUDWidget::HandleCountdown(int32 SecondsLeft)
{
	CountdownAge = 0.0f;
	CountdownText->SetText(SecondsLeft > 0 ? FText::AsNumber(SecondsLeft) : LOCTEXT("Go", "FIGHT"));
	CountdownText->SetColorAndOpacity(FSlateColor(SecondsLeft > 0 ? TextC() : WithA(Style->Accent, 1.0f)));
	if (!bStartBannerShown)
	{
		bStartBannerShown = true;
		ShowBanner(Style->BannerTitle, Style->BannerSub, false, Style->BannerSeconds + 1.5f);
	}
}

void UOTRHUDWidget::HandleNotice(FText Title, FText Sub, bool bDanger)
{
	ShowBanner(Title, Sub, bDanger, Style->EventBannerSeconds);
}

void UOTRHUDWidget::HandlePrompt(FText Text, AActor* Anchor, FVector Offset, bool bDanger)
{
	// One first-time prompt at a time (8.7.6 rule 7): later ones wait their turn.
	FQueuedPrompt Q;
	Q.Text = Text;
	Q.Anchor = Anchor;
	Q.Offset = Offset;
	Q.bDanger = bDanger;
	Q.QueuedAt = ActiveAge;
	PromptQueue.Add(Q);
}

void UOTRHUDWidget::HandleInput(FName KeyId, bool bSimulated)
{
	if (KeyId == AOTRPlayerController::KeyStart)
	{
		StartPress = 1.0f;
	}
	const IOTRMatchInfo* M = Match();
	if (const TPair<FText, FText>* Info = KeyInfo.Find(KeyId))
	{
		if (M && M->OTR_State() == EOTRMatchState::Active && KeyFlashGlyph && KeyFlashLabel)
		{
			KeyFlashGlyph->SetText(Info->Key);
			KeyFlashLabel->SetText(Info->Value.ToUpper());
			KeyFlashAge = 0.0f;
		}
	}
	for (FLegendRow& Row : Legend)
	{
		if (Row.Key == KeyId)
		{
			Row.Lit = Style->KeyLightSeconds;
		}
	}
}

void UOTRHUDWidget::HandlePlayerHit(float Damage, bool bGuarded, EOTRPunch Punch, FVector Location)
{
	ArmorPop = 1.0f;
	if (!bGuarded)
	{
		VignetteStrength = 1.0f;
	}
	else
	{
		VignetteStrength = FMath::Max(VignetteStrength, 0.25f);
	}
}

void UOTRHUDWidget::HandlePopup(FText Text, FVector WorldLocation, FLinearColor Colour)
{
	int32 Pick = 0;
	for (int32 I = 1; I < Popups.Num(); ++I)
	{
		if (Popups[I].Age > Popups[Pick].Age) { Pick = I; }
	}
	FPopup& P = Popups[Pick];
	P.Text->SetText(Text);
	P.Text->SetColorAndOpacity(FSlateColor(Colour.Equals(FLinearColor::White) ? WithA(Style->Accent, 1.0f) : Colour));
	P.World = WorldLocation;
	P.Age = 0.0f;
	HitsPop = 1.0f;
}

void UOTRHUDWidget::Flash(const FLinearColor& Colour, float Alpha)
{
	if (FlashImage)
	{
		FlashImage->SetColorAndOpacity(FLinearColor(Colour.R, Colour.G, Colour.B, 1.0f));
	}
	FlashAlpha = FMath::Max(FlashAlpha, Alpha);
}

void UOTRHUDWidget::HandleCleanHit(float Load, FVector Location)
{
	// Warm white flash, stronger for a full-power hit.
	const FLinearColor Warm = FMath::Lerp(FLinearColor::White, Style->Accent, 0.35f);
	Flash(Warm, Style->CleanFlash * (0.5f + 0.5f * Load));
}

void UOTRHUDWidget::HandleSnap(int32 Side, FVector Location)
{
	Flash(Style->Danger, Style->SnapFlash);
	ShowBanner(LOCTEXT("SnapTitle", "ROPE SNAPPED"), LOCTEXT("SnapSub", "That side is open now. Get pushed through it and you're out."), true, Style->EventBannerSeconds + 0.6f);
	if (RopeTags.IsValidIndex(Side))
	{
		RopeTags[Side].Pop = 1.0f;
	}
}

void UOTRHUDWidget::HandleFray(int32 Side, FVector Location)
{
	if (UOTREventSubsystem* Events = UOTREventSubsystem::Get(this))
	{
		Events->PromptOnce(TEXT("Fray"), LOCTEXT("PromptFray", "FRAYING  USE A FRESH ROPE"), GetOwningPlayerPawn(), FVector(0.0f, 0.0f, 150.0f), true);
	}
	if (RopeTags.IsValidIndex(Side))
	{
		RopeTags[Side].Pop = 1.0f;
	}
}

void UOTRHUDWidget::ShowBanner(const FText& Title, const FText& Sub, bool bDanger, float Duration)
{
	FBannerItem Item;
	Item.Title = Title;
	Item.Sub = Sub;
	Item.bDanger = bDanger;
	Item.Duration = Duration;
	// Event banners replace a waiting one instead of stacking up (no stale news).
	if (BannerQueue.Num() > 1)
	{
		BannerQueue.SetNum(1);
	}
	BannerQueue.Add(Item);
}

// ---------------------------------------------------------------------------------------------- tick

void UOTRHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	// HUD motion runs on real time so hit-stop and slow motion never freeze it.
	const float Dt = FMath::Min((float)FApp::GetDeltaTime(), 0.1f);
	UpdateReadouts(Dt);
	UpdateTitle(Dt);
	UpdateBanner(Dt);
	UpdateTags(Dt);
	UpdatePrompts(Dt);
	UpdateFeedback(Dt);
	DeclutterWorldLabels(Dt);
	UpdateEnd(Dt);
}

void UOTRHUDWidget::UpdateReadouts(float Dt)
{
	const IOTRMatchInfo* M = Match();
	if (!M)
	{
		return;
	}
	const UOTRTuning* T = M->OTR_Tuning();
	const bool bActive = M->OTR_State() == EOTRMatchState::Active;
	if (bActive)
	{
		ActiveAge += Dt;
	}
	// Bell timer: danger colour only in the last stretch.
	const float Left = M->OTR_TimeLeft();
	ClockText->SetText(Clock(Left));
	const bool bLate = bActive && Left <= T->Match.LastStretchSeconds;
	ClockText->SetColorAndOpacity(FSlateColor(bLate ? WithA(Style->Danger, 1.0f) : TextC()));

	// Champion health: drains from the right with each launch hit (partial hits drain partway).
	ShownProgress = FMath::FInterpTo(ShownProgress, M->OTR_Progress(), Dt, 6.0f);
	const int32 HitsLeft = FMath::Max(0, HitLines.Num() - FMath::FloorToInt(M->OTR_Progress() + 0.01f));
	HitsValue->SetText(M->OTR_IsFinisher() ? LOCTEXT("HitsFinish", "DAZED: FINISH IT")
		: FText::Format(LOCTEXT("HitsLeft", "{0} {0}|plural(one=HIT,other=HITS) TO KNOCK OUT"), FText::AsNumber(HitsLeft)));
	HitsValue->SetColorAndOpacity(FSlateColor(M->OTR_IsFinisher() ? WithA(Style->Accent, 1.0f) : TextC(Style->SecondaryOpacity)));
	for (int32 I = 0; I < HitLines.Num(); ++I)
	{
		SetLine(HitLines[I], HitLines.Num() - ShownProgress - I, Style->Danger);
	}
	HitsPop = FMath::Max(0.0f, HitsPop - Dt * 3.0f);
	ScaleWidget(HitsValue, 1.0f + 0.18f * EaseOut(HitsPop));

	// Armor: numeral + hairline, danger below 30 %.
	const IOTRBoxer* Boxer = InterfaceOf<IOTRBoxer>(GetOwningPlayerPawn());
	const float A01 = Boxer ? Boxer->OTR_Armor01() : 1.0f;
	ShownArmor = FMath::FInterpTo(ShownArmor, A01 * 100.0f, Dt, 10.0f);
	ArmorValue->SetText(FText::AsNumber(FMath::RoundToInt(ShownArmor)));
	const bool bLow = A01 < 0.3f;
	ArmorPop = FMath::Max(0.0f, ArmorPop - Dt * 3.0f);
	ArmorValue->SetColorAndOpacity(FSlateColor(bLow || ArmorPop > 0.6f ? WithA(Style->Danger, 1.0f) : TextC()));
	ScaleWidget(ArmorValue, 1.0f + 0.12f * EaseOut(ArmorPop));
	SetLine(ArmorLine, ShownArmor / 100.0f, bLow ? WithA(Style->Danger, 1.0f) : Style->Accent);

	// Legend: rows light on input, the whole list dims after a while.
	for (FLegendRow& Row : Legend)
	{
		Row.Lit = FMath::Max(0.0f, Row.Lit - Dt);
		const float L = FMath::Clamp(Row.Lit / FMath::Max(Style->KeyLightSeconds, 0.01f), 0.0f, 1.0f);
		Row.Glyph->SetColorAndOpacity(FSlateColor(FMath::Lerp(FLinearColor(0.035f, 0.035f, 0.04f, 1.0f), WithA(Style->Accent, 1.0f), L)));
		Row.Label->SetColorAndOpacity(FSlateColor(FMath::Lerp(TextC(0.9f), TextC(1.0f), L)));
	}
	// Scoreboard bulbs go out as the round runs down; the last ones blink in the final stretch.
	const float Share = T->Match.RoundSeconds > 0.0f ? Left / T->Match.RoundSeconds : 0.0f;
	const int32 On = FMath::CeilToInt(Share * ClockBulbs.Num());
	const bool bBlink = bLate && FMath::Fmod(GetWorld()->GetRealTimeSeconds(), 0.5f) < 0.25f;
	for (int32 I = 0; I < ClockBulbs.Num(); ++I)
	{
		const bool bOn = I < On && !(bBlink && I == On - 1);
		ClockBulbs[I]->SetColorAndOpacity(bOn ? (bLate ? FLinearColor(1.0f, 0.35f, 0.3f, 1.0f) : FLinearColor::White) : FLinearColor(0.18f, 0.14f, 0.1f, 0.7f));
	}
}

void UOTRHUDWidget::UpdateBanner(float Dt)
{
	if (BannerAge < 0.0f && BannerQueue.Num() > 0 && EndAge < 0.0f)
	{
		CurrentBanner = BannerQueue[0];
		BannerQueue.RemoveAt(0);
		BannerAge = 0.0f;
		BannerTitle->SetText(CurrentBanner.Title);
		BannerSub->SetText(CurrentBanner.Sub);
		BannerRule->SetColorAndOpacity(CurrentBanner.bDanger ? Style->Danger : Style->Accent);
	}
	float Alpha = 0.0f;
	if (BannerAge >= 0.0f)
	{
		BannerAge += Dt;
		const float In = EaseOut(BannerAge / Style->FadeSeconds);
		const float Out = 1.0f - EaseOut((BannerAge - CurrentBanner.Duration) / Style->FadeSeconds);
		Alpha = FMath::Clamp(FMath::Min(In, Out), 0.0f, 1.0f);
		OffsetWidget(BannerBox, FVector2D(0.0f, (1.0f - In) * 12.0f));
		if (BannerAge > CurrentBanner.Duration + Style->FadeSeconds)
		{
			BannerAge = -1.0f;
		}
	}
	BannerBox->SetRenderOpacity(Alpha);
	for (UImage* Img : BannerBand)
	{
		Img->SetRenderOpacity(Alpha);
	}
	// Countdown numerals: pop in, fade out within the second (headline face drawn large).
	CountdownAge += Dt;
	const float C = CountdownAge < 0.9f ? 1.0f - EaseOut((CountdownAge - 0.55f) / 0.35f) * (CountdownAge > 0.55f ? 1.0f : 0.0f) : 0.0f;
	CountdownText->SetRenderOpacity(C);
	ScaleWidget(CountdownText, 1.9f + 0.4f * (1.0f - EaseOut(CountdownAge / 0.2f)));
}

void UOTRHUDWidget::UpdateTag(FTag& Tag, bool bShow, const FVector& World, float Dt)
{
	FVector2D Screen = FVector2D::ZeroVector;
	const bool bVisible = bShow && Project(World, Screen);
	Tag.Alpha = FMath::FInterpTo(Tag.Alpha, bVisible ? 1.0f : 0.0f, Dt, 12.0f);
	Tag.Box->SetRenderOpacity(Tag.Alpha);
	Tag.Pop = FMath::Max(0.0f, Tag.Pop - Dt * 3.0f);
	ScaleWidget(Tag.Box, 1.0f + 0.2f * EaseOut(Tag.Pop));
	if (bVisible)
	{
		if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Tag.Box->Slot))
		{
			S->SetPosition(Screen);
		}
	}
}

void UOTRHUDWidget::DeclutterWorldLabels(float Dt)
{
	// Priority: incoming punch tag (never moves), prompts, popups, champion heat, stored load, rope tags.
	TArray<UWidget*> Order;
	Order.Add(PunchTag.Box);
	for (const FPrompt& P : Prompts) { Order.Add(P.Box); }
	for (const FPopup& P : Popups) { Order.Add(P.Text); }
	Order.Add(HeatTag.Box);
	Order.Add(LoadTag.Box);
	for (const FTag& T : RopeTags) { Order.Add(T.Box); }

	const float Pad = Style ? Style->LabelGap : 8.0f;
	TArray<FBox2D> Placed;
	for (UWidget* W : Order)
	{
		UCanvasPanelSlot* S = W ? Cast<UCanvasPanelSlot>(W->Slot) : nullptr;
		if (!S)
		{
			continue;
		}
		const FVector2D Base = S->GetPosition();
		FVector2D& Nudge = LabelNudge.FindOrAdd(W);
		if (W->GetRenderOpacity() < 0.05f || !W->IsVisible())
		{
			Nudge = FVector2D::ZeroVector;
			continue;
		}
		const FVector2D Size = W->GetDesiredSize() * W->GetRenderTransform().Scale;
		const FVector2D Align = S->GetAlignment();
		auto RectAt = [&](const FVector2D& Pos) { const FVector2D Min = Pos - Align * Size; return FBox2D(Min, Min + Size); };
		// Find the smallest vertical shift that clears every label already placed.
		FVector2D Want = FVector2D::ZeroVector;
		for (int32 Pass = 0; Pass < 6; ++Pass)
		{
			const FBox2D R = RectAt(Base + Want);
			bool bHit = false;
			for (const FBox2D& O : Placed)
			{
				const bool bOverlapX = R.Min.X < O.Max.X + Pad && R.Max.X > O.Min.X - Pad;
				const bool bOverlapY = R.Min.Y < O.Max.Y + Pad && R.Max.Y > O.Min.Y - Pad;
				if (bOverlapX && bOverlapY)
				{
					const float Down = O.Max.Y + Pad - R.Min.Y;
					const float Up = R.Max.Y - (O.Min.Y - Pad);
					Want.Y += Down <= Up ? Down : -Up;
					bHit = true;
					break;
				}
			}
			if (!bHit)
			{
				break;
			}
		}
		Nudge = FMath::Vector2DInterpTo(Nudge, Want, Dt, 16.0f);
		// Snap when far off so a label never sits on another for more than a frame.
		if ((Nudge - Want).Size() > 40.0f)
		{
			Nudge = Want;
		}
		S->SetPosition(Base + Nudge);
		Placed.Add(RectAt(Base + Want));
	}
}

void UOTRHUDWidget::UpdateTags(float Dt)
{
	const IOTRMatchInfo* M = Match();
	const bool bPlay = M && M->OTR_State() == EOTRMatchState::Active;
	AActor* ChampActor = M ? M->OTR_Champion() : nullptr;
	const IOTRChampion* Champ = InterfaceOf<IOTRChampion>(ChampActor);
	const IOTRBoxer* Boxer = InterfaceOf<IOTRBoxer>(GetOwningPlayerPawn());
	const IOTRRingInfo* Ring = M ? InterfaceOf<IOTRRingInfo>(M->OTR_Ring()) : nullptr;

	// Incoming punch: HIGH / LOW / JAB at the winding fist (danger), the line drains to the hit.
	float ToHit = 0.0f;
	const EOTRPunch Punch = Champ ? Champ->OTR_Telegraph(ToHit) : EOTRPunch::None;
	if (Punch != EOTRPunch::None)
	{
		const FOTRPunchRow* Row = Champ->OTR_CurrentPunch();
		PunchTag.Text->SetText(Row ? Row->TagText : FText::GetEmpty());
		PunchTag.Text->SetColorAndOpacity(FSlateColor(WithA(Style->Danger, 1.0f)));
		const float Total = Row ? FMath::Max(Row->Telegraph + Row->StrikeTime, 0.05f) : 0.5f;
		SetLine(PunchTag.Line, ToHit / Total, WithA(Style->Danger, 1.0f));
	}
	UpdateTag(PunchTag, bPlay && Punch != EOTRPunch::None, Champ ? Champ->OTR_FistLocation() + FVector(0.0f, 0.0f, 55.0f) : FVector::ZeroVector, Dt);

	// Champion heat: a quiet hairline over its head; VENTING in the accent while the window is open.
	if (Champ && ChampActor)
	{
		const bool bVent = Champ->OTR_IsVenting();
		HeatTag.Text->SetText(bVent ? LOCTEXT("TagVent", "VENTING") : LOCTEXT("TagHeat", "HEAT"));
		HeatTag.Text->SetColorAndOpacity(FSlateColor(bVent ? WithA(Style->Accent, 1.0f) : TextC(Style->SecondaryOpacity)));
		const float VentTotal = FMath::Max(M->OTR_Tuning()->Champion.VentSeconds, 0.1f);
		SetLine(HeatTag.Line, bVent ? Champ->OTR_VentTimeLeft() / VentTotal : Champ->OTR_Heat01(), bVent ? WithA(Style->Accent, 1.0f) : TextC(0.75f));
		UpdateTag(HeatTag, bPlay, ChampActor->GetActorLocation() + FVector(0.0f, 0.0f, Style->ChampionTagHeight), Dt);
	}

	// Stored load at your shoulder while on the ropes.
	if (Boxer)
	{
		const float Load = Boxer->OTR_StoredLoad();
		const bool bShow = bPlay && Boxer->OTR_IsLeaning();
		LoadTag.Text->SetText(FText::Format(LOCTEXT("TagLoad", "LOAD  {0}"), FText::AsNumber(FMath::RoundToInt(Load * 100.0f))));
		LoadTag.Text->SetColorAndOpacity(FSlateColor(Load > 0.01f ? WithA(Style->Accent, 1.0f) : TextC(Style->SecondaryOpacity)));
		SetLine(LoadTag.Line, Load, WithA(Style->Accent, 1.0f));
		const APawn* P = GetOwningPlayerPawn();
		UpdateTag(LoadTag, bShow, P ? P->GetActorLocation() + FVector(0.0f, 0.0f, Style->PlayerTagHeight) : FVector::ZeroVector, Dt);
	}

	// Ropes: FRAYED (danger line = fray) and OPEN tags at the middle of a side.
	for (int32 S = 0; S < RopeTags.Num(); ++S)
	{
		FTag& Tag = RopeTags[S];
		if (!Ring || S >= Ring->OTR_SideCount())
		{
			UpdateTag(Tag, false, FVector::ZeroVector, Dt);
			continue;
		}
		const bool bOpen = Ring->OTR_IsSnapped(S);
		const float Fray = Ring->OTR_Fray(S);
		const bool bFrayed = !bOpen && Fray >= M->OTR_Tuning()->Rope.FrayWarn;
		Tag.Text->SetText(bOpen ? LOCTEXT("TagOpen", "OPEN") : LOCTEXT("TagFrayed", "FRAYED"));
		Tag.Text->SetColorAndOpacity(FSlateColor(WithA(Style->Danger, 1.0f)));
		SetLine(Tag.Line, bOpen ? 1.0f : Fray, WithA(Style->Danger, bOpen ? 0.0f : 1.0f));
		Tag.Line.Rest->SetColorAndOpacity(bOpen ? FLinearColor::Transparent : Style->Track);
		UpdateTag(Tag, bPlay && (bOpen || bFrayed), Ring->OTR_SidePoint(S, 0.5f) + FVector(0.0f, 0.0f, Style->RopeTagHeight), Dt);
	}
}

void UOTRHUDWidget::UpdatePrompts(float Dt)
{
	// Start the next queued prompt once the current one has finished (stale ones are dropped).
	FPrompt& Cur = Prompts[0];
	if (Cur.Age > Style->PromptSeconds + Style->FadeSeconds && PromptQueue.Num() > 0)
	{
		while (PromptQueue.Num() > 0 && ActiveAge - PromptQueue[0].QueuedAt > 6.0f)
		{
			PromptQueue.RemoveAt(0);
		}
		if (PromptQueue.Num() > 0)
		{
			const FQueuedPrompt Q = PromptQueue[0];
			PromptQueue.RemoveAt(0);
			Cur.Text->SetText(Q.Text);
			Cur.Text->SetColorAndOpacity(FSlateColor(Q.bDanger ? WithA(Style->Danger, 1.0f) : TextC()));
			Cur.Rule->SetColorAndOpacity(Q.bDanger ? Style->Danger : Style->Accent);
			Cur.Anchor = Q.Anchor;
			Cur.Offset = Q.Offset;
			Cur.World = Q.Anchor.IsValid() ? Q.Anchor->GetActorLocation() : FVector::ZeroVector;
			Cur.Age = 0.0f;
		}
	}
	for (FPrompt& P : Prompts)
	{
		P.Age += Dt;
		if (P.Anchor.IsValid())
		{
			P.World = P.Anchor->GetActorLocation();
		}
		const float In = EaseOut(P.Age / Style->FadeSeconds);
		const float Out = 1.0f - EaseOut((P.Age - Style->PromptSeconds) / Style->FadeSeconds);
		// Prompts yield to banners (no overlap, 8.7.6 rule 7).
		const float Alpha = BannerAge >= 0.0f ? 0.0f : FMath::Clamp(FMath::Min(In, Out), 0.0f, 1.0f);
		FVector2D Screen = FVector2D::ZeroVector;
		const bool bOn = Alpha > 0.01f && Project(P.World + P.Offset, Screen);
		P.Box->SetRenderOpacity(bOn ? Alpha : 0.0f);
		if (bOn)
		{
			if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(P.Box->Slot))
			{
				S->SetPosition(Screen);
			}
		}
	}
	for (FPopup& P : Popups)
	{
		P.Age += Dt;
		const float T = P.Age / Style->PopupSeconds;
		FVector2D Screen = FVector2D::ZeroVector;
		const bool bOn = T < 1.0f && Project(P.World, Screen);
		P.Text->SetRenderOpacity(bOn ? 1.0f - EaseOut(FMath::Max(0.0f, T - 0.5f) / 0.5f) : 0.0f);
		P.Text->SetRenderTransformPivot(FVector2D(0.5f, 1.0f));
		ScaleWidget(P.Text, 1.0f + (Style->PopupPop - 1.0f) * (1.0f - EaseOut(P.Age / 0.25f)));
		if (bOn)
		{
			if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(P.Text->Slot))
			{
				S->SetPosition(Screen - FVector2D(0.0f, Style->PopupRise * EaseOut(T)));
			}
		}
	}
}

void UOTRHUDWidget::UpdateFeedback(float Dt)
{
	VignetteStrength = FMath::Max(0.0f, VignetteStrength - Dt * 2.2f);
	const float V = EaseOut(VignetteStrength);
	for (UImage* Img : VignetteImages)
	{
		Img->SetRenderOpacity(V);
	}
	FlashAlpha = FMath::FInterpTo(FlashAlpha, 0.0f, Dt, Style->FlashFade);
	if (FlashImage)
	{
		FlashImage->SetRenderOpacity(FlashAlpha);
	}
	// The vent window glows amber at the screen edges, pulsing: the moment to release.
	const IOTRMatchInfo* M = Match();
	const IOTRChampion* Champ = M ? InterfaceOf<IOTRChampion>(M->OTR_Champion()) : nullptr;
	const bool bVent = Champ && Champ->OTR_IsVenting() && M->OTR_State() == EOTRMatchState::Active;
	const float Pulse = 0.75f + 0.25f * FMath::Sin(GetWorld()->GetRealTimeSeconds() * Style->VentGlowHz * 2.0f * PI);
	VentGlow = FMath::FInterpTo(VentGlow, bVent ? Style->VentGlowStrength * Pulse : 0.0f, Dt, 10.0f);
	for (UImage* Img : VentImages)
	{
		Img->SetRenderOpacity(VentGlow);
	}
}

void UOTRHUDWidget::UpdateEnd(float Dt)
{
	if (EndAge < 0.0f)
	{
		Dim->SetRenderOpacity(0.0f);
		EndBox->SetRenderOpacity(0.0f);
		return;
	}
	EndAge += Dt;
	// Let the knockout / ring-out land before the card comes in.
	const float A = EaseOut((EndAge - 1.0f) / 0.25f);
	Dim->SetRenderOpacity(A);
	for (UImage* Img : EndBand)
	{
		Img->SetRenderOpacity(A);
	}
	EndBox->SetRenderOpacity(A);
	OffsetWidget(EndBox, FVector2D(0.0f, (1.0f - A) * 14.0f));
	ScaleWidget(EndTitle, 1.0f + 0.35f * (1.0f - EaseOut((EndAge - 1.0f) / 0.3f)));
	for (int32 I = 0; I < EndBolts.Num(); ++I)
	{
		const float B = EaseOut((EndAge - 1.15f - I * 0.12f) / 0.14f);
		EndBolts[I]->SetRenderOpacity(B);
		ScaleWidget(EndBolts[I], 1.0f + 0.6f * (1.0f - B));
	}
	for (UWidget* W : { ChampPlate.Get(), ClockPlate.Get(), ArmorPlate.Get() })
	{
		if (W) { W->SetRenderOpacity(1.0f - A); }
	}
	LegendBox->SetRenderOpacity(1.0f - A);
	for (FPopup& P : Popups)
	{
		P.Age = 100.0f;
	}
}

#undef LOCTEXT_NAMESPACE
