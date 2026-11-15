#include "HyperionHUDWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateColorBrush.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ChildActorComponent.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "GameFramework/Pawn.h"
#include "HyperionShipActor.h"
#include "SolarGravityComponent.h"
#include "SolarSystemSimulator.h"
#include "HAL/PlatformTime.h"
#include "Rendering/DrawElements.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Components/Spacer.h"

namespace
{
	// Accelerations du temps proposees par la rangee de boutons sous la barre de pilotage.
	constexpr int32 TimeWarpValues[] = { 1, 10, 100, 1000 };
}

UHyperionHUDWidget::UHyperionHUDWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	for (int32 i = 0; i < NumButtons; ++i)
	{
		ButtonLabels.Add(FText::FromString(FString::Printf(TEXT("BOUTON %d"), i + 1)));
	}
	// Pastilles des propulseurs : pleine (allume) ou contour seul (eteint), teintees au dessin.
	MarkerFillBrush = FSlateRoundedBoxBrush(FLinearColor::White, 3.f);
	MarkerOutlineBrush = FSlateRoundedBoxBrush(FLinearColor::Transparent, 3.f, FLinearColor::White, 1.f);
}

// ===================================================================== Construction

UTextBlock* UHyperionHUDWidget::MakeText(const FString& Text, int32 Size, const FLinearColor& Color, bool bBold) const
{
	UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
	T->SetText(FText::FromString(Text));
	FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
	Font.LetterSpacing = 180;  // lettres espacees : style "console"
	T->SetFont(Font);
	T->SetColorAndOpacity(FSlateColor(Color));
	return T;
}

FButtonStyle UHyperionHUDWidget::MakeButtonStyle(bool bActive, bool bDone) const
{
	// Cadre fin a coins biseautes (rayons asymetriques), fond translucide qui s'allume au survol.
	// Bouton actif : meme dessin dans la couleur ActiveButtonColor (DoneButtonColor si sa manoeuvre est
	// accomplie), fond allume.
	const FLinearColor C = !bActive ? AccentColor : (bDone ? DoneButtonColor : ActiveButtonColor);
	const FVector4 Corners(0.f, 9.f, 0.f, 9.f);
	FLinearColor NormalFill = PanelColor;
	FLinearColor NormalOutline = C; NormalOutline.A = 0.55f;
	if (bActive)
	{
		NormalFill = C; NormalFill.A = 0.28f;
		NormalOutline.A = 1.f;
	}
	FLinearColor HoverFill = C; HoverFill.A = bActive ? 0.36f : 0.18f;
	FLinearColor PressFill = C; PressFill.A = 0.45f;

	FButtonStyle Style;
	Style.SetNormal(FSlateRoundedBoxBrush(NormalFill, Corners, NormalOutline, bActive ? 2.f : 1.f));
	Style.SetHovered(FSlateRoundedBoxBrush(HoverFill, Corners, C, 1.5f));
	Style.SetPressed(FSlateRoundedBoxBrush(PressFill, Corners, C, 2.f));
	Style.SetDisabled(FSlateRoundedBoxBrush(PanelColor, Corners, FLinearColor(0.3f, 0.3f, 0.3f, 0.5f), 1.f));
	Style.SetNormalPadding(FMargin(8.f, 3.f));
	Style.SetPressedPadding(FMargin(8.f, 4.f, 8.f, 2.f));
	return Style;
}

void UHyperionHUDWidget::SetButtonActive(int32 Index, bool bActive, bool bDone)
{
	// Etat : 0 eteint, 1 actif (orange), 2 actif et manoeuvre accomplie (vert).
	const uint8 State = !bActive ? 0 : (bDone ? 2 : 1);
	if (!Buttons.IsValidIndex(Index) || !ButtonActive.IsValidIndex(Index) || ButtonActive[Index] == State)
	{
		return;
	}
	ButtonActive[Index] = State;
	Buttons[Index]->SetStyle(MakeButtonStyle(bActive, bDone));
	if (ButtonTexts.IsValidIndex(Index))
	{
		ButtonTexts[Index]->SetColorAndOpacity(FSlateColor(!bActive ? AccentColor : (bDone ? DoneButtonColor : ActiveButtonColor)));
	}
}

UButton* UHyperionHUDWidget::MakeButton(int32 Index, UHorizontalBox* Bar)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(MakeButtonStyle(false));
	Buttons.Add(Button);
	ButtonActive.Add(0);

	const FText Label = ButtonLabels.IsValidIndex(Index) ? ButtonLabels[Index] : FText::GetEmpty();
	UTextBlock* Text = MakeText(Label.ToString(), 9, AccentColor);
	Text->SetJustification(ETextJustify::Center);
	ButtonTexts.Add(Text);
	Button->AddChild(Text);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(108.f);
	Size->SetHeightOverride(28.f);
	Size->AddChild(Button);

	UHorizontalBoxSlot* BarSlot = Bar->AddChildToHorizontalBox(Size);
	BarSlot->SetPadding(FMargin(4.f, 0.f));

	switch (Index)
	{
	case 0: Button->OnClicked.AddDynamic(this, &UHyperionHUDWidget::HandleButton0); break;
	case 1: Button->OnClicked.AddDynamic(this, &UHyperionHUDWidget::HandleButton1); break;
	case 2: Button->OnClicked.AddDynamic(this, &UHyperionHUDWidget::HandleButton2); break;
	case 3: Button->OnClicked.AddDynamic(this, &UHyperionHUDWidget::HandleButton3); break;
	case 4: Button->OnClicked.AddDynamic(this, &UHyperionHUDWidget::HandleButton4); break;
	case 5: Button->OnClicked.AddDynamic(this, &UHyperionHUDWidget::HandleButton5); break;
	default: break;
	}
	return Button;
}

void UHyperionHUDWidget::MakeEngineRow(UVerticalBox* Box, const FString& Label, bool bMain)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(44.f);
	LabelBox->AddChild(MakeText(Label, 10, AccentColor));
	UHorizontalBoxSlot* S0 = Row->AddChildToHorizontalBox(LabelBox);
	S0->SetVerticalAlignment(VAlign_Center);

	UProgressBar* Bar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass());
	FLinearColor Track = AccentColor; Track.A = 0.10f;
	FProgressBarStyle BarStyle;
	BarStyle.SetBackgroundImage(FSlateColorBrush(Track));
	BarStyle.SetFillImage(FSlateColorBrush(FLinearColor::White));
	BarStyle.SetMarqueeImage(FSlateColorBrush(FLinearColor::White));
	Bar->SetWidgetStyle(BarStyle);
	Bar->SetFillColorAndOpacity(AccentColor);
	Bar->SetPercent(0.f);
	USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	BarBox->SetWidthOverride(bMain ? 230.f : 200.f);
	BarBox->SetHeightOverride(bMain ? 12.f : 8.f);
	BarBox->AddChild(Bar);
	UHorizontalBoxSlot* S1 = Row->AddChildToHorizontalBox(BarBox);
	S1->SetVerticalAlignment(VAlign_Center);
	S1->SetPadding(FMargin(4.f, 0.f, 10.f, 0.f));

	USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ValueBox->SetWidthOverride(58.f);
	UTextBlock* Value = MakeText(TEXT("0 %"), 10, FLinearColor(0.85f, 0.95f, 1.f, 1.f));
	Value->SetJustification(ETextJustify::Right);
	ValueBox->AddChild(Value);
	UHorizontalBoxSlot* S2 = Row->AddChildToHorizontalBox(ValueBox);
	S2->SetVerticalAlignment(VAlign_Center);

	UVerticalBoxSlot* RowSlot = Box->AddChildToVerticalBox(Row);
	RowSlot->SetPadding(FMargin(0.f, bMain ? 4.f : 2.f));

	EngineBars.Add(Bar);
	EngineValues.Add(Value);
	EngineIsMain.Add(bMain);
}

void UHyperionHUDWidget::BuildTree()
{
	UCanvasPanel* Root = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Root"));
	WidgetTree->RootWidget = Root;

	// ---------------------------------------------------------------- Barre de boutons (haut, centree)
	TopBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TopBar"));
	Buttons.Reset();
	ButtonActive.Reset();
	for (int32 i = 0; i < NumButtons; ++i)
	{
		MakeButton(i, TopBar);
	}

	// Rangee de l'acceleration du temps, centree sous les boutons de pilotage.
	TimeBar = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("TimeBar"));
	TimeButtons.Reset();
	TimeButtonActive.Reset();
	TimeButtonTexts.Reset();
	for (int32 i = 0; i < UE_ARRAY_COUNT(TimeWarpValues); ++i)
	{
		MakeTimeButton(i, TimeBar);
	}

	UVerticalBox* TopColumn = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("TopArea"));
	TopColumn->AddChildToVerticalBox(TopBar);
	UVerticalBoxSlot* TimeSlot = TopColumn->AddChildToVerticalBox(TimeBar);
	TimeSlot->SetHorizontalAlignment(HAlign_Center);
	TimeSlot->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));
	TopArea = TopColumn;

	UCanvasPanelSlot* TopSlot = Root->AddChildToCanvas(TopColumn);
	TopSlot->SetAnchors(FAnchors(0.5f, 0.f));
	TopSlot->SetAlignment(FVector2D(0.5f, 0.f));
	TopSlot->SetPosition(FVector2D(0.f, 26.f));
	TopSlot->SetAutoSize(true);

	// ---------------------------------------------------------------- Panneau propulsion (bas, droite)
	UVerticalBox* Box = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	UTextBlock* Title = MakeText(TEXT("PROPULSION"), 14, AccentColor);
	Box->AddChildToVerticalBox(Title)->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

	FLinearColor Dim = AccentColor; Dim.A = 0.6f;
	Box->AddChildToVerticalBox(MakeText(TEXT("MOTEURS PRINCIPAUX"), 9, Dim, false))->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
	for (int32 i = 0; i < NumMainEngines; ++i)
	{
		MakeEngineRow(Box, FString::Printf(TEXT("P%d"), i + 1), true);
	}

	// Filet de separation
	FLinearColor Rule = AccentColor; Rule.A = 0.35f;
	UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
	Line->SetBrush(FSlateColorBrush(Rule));
	USizeBox* LineBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LineBox->SetHeightOverride(1.f);
	LineBox->AddChild(Line);
	Box->AddChildToVerticalBox(LineBox)->SetPadding(FMargin(0.f, 8.f));

	Box->AddChildToVerticalBox(MakeText(TEXT("MOTEURS AUXILIAIRES"), 9, Dim, false))->SetPadding(FMargin(0.f, 0.f, 0.f, 2.f));
	for (int32 i = 0; i < NumAuxEngines; ++i)
	{
		MakeEngineRow(Box, FString::Printf(TEXT("A%d"), i + 1), false);
	}

	EnginePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("EnginePanel"));
	FLinearColor Outline = AccentColor; Outline.A = 0.45f;
	EnginePanel->SetBrush(FSlateRoundedBoxBrush(PanelColor, FVector4(0.f, 16.f, 0.f, 16.f), Outline, 1.f));
	EnginePanel->SetPadding(FMargin(18.f, 14.f));
	EnginePanel->SetContent(Box);

	UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(EnginePanel);
	PanelSlot->SetAnchors(FAnchors(1.f, 1.f));
	PanelSlot->SetAlignment(FVector2D(1.f, 1.f));
	PanelSlot->SetPosition(FVector2D(-28.f, -28.f));
	PanelSlot->SetAutoSize(true);

	// ---------------------------------------------------------------- Panneau manoeuvre (bas, gauche)
	UVerticalBox* MBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MBox->AddChildToVerticalBox(MakeText(TEXT("MANŒUVRE"), 14, AccentColor))->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	MBox->AddChildToVerticalBox(MakeText(TEXT("PROPULSEURS DE MANŒUVRE"), 9, Dim, false));

	// Zone vide de taille fixe : le vaisseau et les propulseurs y sont dessines dans NativePaint.
	USizeBox* Area = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SchematicArea"));
	Area->SetWidthOverride(340.f);
	Area->SetHeightOverride(200.f);  // place pour 3 groupes de propulseurs et leurs titres
	SchematicArea = Area;
	MBox->AddChildToVerticalBox(Area)->SetPadding(FMargin(0.f, 6.f));
	MBox->AddChildToVerticalBox(MakeText(TEXT("H HAUT   B BAS   G GAUCHE   D DROITE"), 8, Dim, false));

	ManeuverPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("ManeuverPanel"));
	ManeuverPanel->SetBrush(FSlateRoundedBoxBrush(PanelColor, FVector4(16.f, 0.f, 16.f, 0.f), Outline, 1.f));
	ManeuverPanel->SetPadding(FMargin(18.f, 14.f));
	ManeuverPanel->SetContent(MBox);

	UCanvasPanelSlot* MSlot = Root->AddChildToCanvas(ManeuverPanel);
	MSlot->SetAnchors(FAnchors(0.f, 1.f));
	MSlot->SetAlignment(FVector2D(0.f, 1.f));
	MSlot->SetPosition(FVector2D(28.f, -28.f));
	MSlot->SetAutoSize(true);

	// ---------------------------------------------------------------- Panneau orbite (haut, gauche)
	UVerticalBox* OBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	OBox->AddChildToVerticalBox(MakeText(TEXT("ORBITE"), 14, AccentColor))->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	OrbitRefText = MakeText(TEXT("—"), 9, Dim, false);
	OBox->AddChildToVerticalBox(OrbitRefText)->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	OrbitValues.Reset();
	OrbitAltValues.Reset();
	MakeOrbitRow(OBox, TEXT("Alt"), &OrbitAltValues);
	MakeOrbitRow(OBox, TEXT("AP"));
	MakeOrbitRow(OBox, TEXT("PE"));
	MakeOrbitRow(OBox, TEXT("ECC"));
	MakeOrbitRow(OBox, TEXT("T-AP"));
	MakeOrbitRow(OBox, TEXT("T-PE"));
	MakeOrbitRow(OBox, TEXT("RInc"));

	OrbitPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("OrbitPanel"));
	OrbitPanel->SetBrush(FSlateRoundedBoxBrush(PanelColor, FVector4(0.f, 16.f, 0.f, 16.f), Outline, 1.f));
	OrbitPanel->SetPadding(FMargin(18.f, 14.f));
	OrbitPanel->SetContent(OBox);

	UCanvasPanelSlot* OSlot = Root->AddChildToCanvas(OrbitPanel);
	OSlot->SetAnchors(FAnchors(0.f, 0.f));
	OSlot->SetAlignment(FVector2D(0.f, 0.f));
	OSlot->SetPosition(FVector2D(28.f, 28.f));
	OSlot->SetAutoSize(true);

	// ---------------------------------------------------------------- Panneau rotation (haut, droite)
	UVerticalBox* RBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	RBox->AddChildToVerticalBox(MakeText(TEXT("ROTATION"), 14, AccentColor))->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	RBox->AddChildToVerticalBox(MakeText(TEXT("DEG/S (TEMPS SIMULÉ)"), 9, Dim, false))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	RotationValues.Reset();
	MakeOrbitRow(RBox, TEXT("TANG"), &RotationValues);
	MakeOrbitRow(RBox, TEXT("LACET"), &RotationValues);
	MakeOrbitRow(RBox, TEXT("ROUL"), &RotationValues);

	RotationPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("RotationPanel"));
	RotationPanel->SetBrush(FSlateRoundedBoxBrush(PanelColor, FVector4(16.f, 0.f, 16.f, 0.f), Outline, 1.f));
	RotationPanel->SetPadding(FMargin(18.f, 14.f));
	RotationPanel->SetContent(RBox);

	UCanvasPanelSlot* RSlot = Root->AddChildToCanvas(RotationPanel);
	RSlot->SetAnchors(FAnchors(1.f, 0.f));
	RSlot->SetAlignment(FVector2D(1.f, 0.f));
	RSlot->SetPosition(FVector2D(-28.f, 28.f));
	RSlot->SetAutoSize(true);

	BuildMapView(Root);
}

void UHyperionHUDWidget::MakeOrbitRow(UVerticalBox* Box, const FString& Label, TArray<TObjectPtr<UTextBlock>>* Into)
{
	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());

	USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	LabelBox->SetWidthOverride(64.f);
	LabelBox->AddChild(MakeText(Label, 10, AccentColor));
	Row->AddChildToHorizontalBox(LabelBox)->SetVerticalAlignment(VAlign_Center);

	USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	ValueBox->SetWidthOverride(150.f);
	UTextBlock* Value = MakeText(TEXT("—"), 10, FLinearColor(0.85f, 0.95f, 1.f, 1.f));
	Value->SetJustification(ETextJustify::Right);
	ValueBox->AddChild(Value);
	Row->AddChildToHorizontalBox(ValueBox)->SetVerticalAlignment(VAlign_Center);

	Box->AddChildToVerticalBox(Row)->SetPadding(FMargin(0.f, 2.f));
	(Into ? *Into : OrbitValues).Add(Value);
}

TSharedRef<SWidget> UHyperionHUDWidget::RebuildWidget()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
	}
	if (!WidgetTree->RootWidget)
	{
		BuildTree();
	}
	return Super::RebuildWidget();
}

// ===================================================================== Mise a jour

AHyperionShip* UHyperionHUDWidget::FindShip() const
{
	if (CachedShip.IsValid())
	{
		return CachedShip.Get();
	}
	APawn* Pawn = GetOwningPlayerPawn();
	if (!Pawn)
	{
		return nullptr;
	}
	// Le pion BP_HyperionPawn porte le vaisseau dans un ChildActorComponent.
	TInlineComponentArray<UChildActorComponent*> Children(Pawn);
	for (UChildActorComponent* C : Children)
	{
		if (AHyperionShip* Ship = Cast<AHyperionShip>(C->GetChildActor()))
		{
			CachedShip = Ship;
			return Ship;
		}
	}
	return nullptr;
}

void UHyperionHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	const AHyperionShip* Ship = FindShip();
	// Une jauge par moteur : P1, P2 puis A1..A6, dans l'ordre des moteurs du vaisseau.
	for (int32 i = 0; i < EngineBars.Num(); ++i)
	{
		const float V = FMath::Clamp(Ship ? Ship->GetEngineThrottle(i) : 0.f, 0.f, 1.f);
		EngineBars[i]->SetPercent(V);
		EngineBars[i]->SetFillColorAndOpacity(V >= HighThrustThreshold ? HighThrustColor : AccentColor);
		EngineValues[i]->SetText(FText::FromString(FString::Printf(TEXT("%d %%"), FMath::RoundToInt(V * 100.f))));
	}

	UpdateOrbitPanel();
	UpdateRotationPanel();
	UpdateTimeButtons();

	if (bMapVisible && MapDateText)
	{
		if (const ASolarSystemSimulator* S = ASolarSystemSimulator::Get(this))
		{
			MapDateText->SetText(FText::FromString(FString::Printf(TEXT("%s UTC   ·   TEMPS ×%.0f"),
				*S->GetSimDateUTC().ToString(TEXT("%d/%m/%Y %H:%M")), S->TimeScale)));
		}
	}
}

void UHyperionHUDWidget::UpdateRotationPanel()
{
	if (RotationValues.Num() < 3)
	{
		return;
	}
	const APawn* Pawn = GetOwningPlayerPawn();
	const USolarGravityComponent* G = Pawn ? Pawn->FindComponentByClass<USolarGravityComponent>() : nullptr;
	const AHyperionShip* Ship = FindShip();
	if (!G || !Ship)
	{
		for (UTextBlock* T : RotationValues)
		{
			T->SetText(FText::FromString(TEXT("—")));
		}
		return;
	}
	// Vitesse de rotation (rad/s simulees, repere monde) exprimee dans les axes du vaisseau :
	// maquette +Y = proue, +X = flanc gauche, +Z = haut.
	//  tangage > 0 : le nez monte  (rotation autour de +X) ;
	//  lacet   > 0 : le nez va a droite (autour de +Z) ;
	//  roulis  > 0 : le flanc droit descend (autour de -Y).
	const FVector W = Ship->GetActorQuat().UnrotateVector(G->AngularVelocity) * (180.0 / PI);
	const double Values[3] = { W.X, W.Z, -W.Y };
	const FLinearColor Normal(0.85f, 0.95f, 1.f, 1.f);
	FLinearColor Still = AccentColor; Still.A = 0.6f;
	for (int32 i = 0; i < 3; ++i)
	{
		const double V = FMath::Abs(Values[i]) < 5e-5 ? 0.0 : Values[i];
		RotationValues[i]->SetText(FText::FromString(FString::Printf(TEXT("%+.4f °/s"), V)));
		RotationValues[i]->SetColorAndOpacity(FSlateColor(V == 0.0 ? Still : Normal));
	}
}

// ===================================================================== Orbite

namespace
{
	struct FBodyInfo { const TCHAR* Name; const TCHAR* Label; double RadiusKm; FLinearColor Color; };
	// Rayons moyens (km) : les apsides sont affichees en altitude au-dessus de la surface.
	// Couleurs : reperes de la carte (F1).
	const FBodyInfo BodyInfos[] = {
		{ TEXT("Sun"),     TEXT("SOLEIL"),  695700.0, FLinearColor(1.00f, 0.82f, 0.30f) },
		{ TEXT("Mercury"), TEXT("MERCURE"),   2439.7, FLinearColor(0.65f, 0.62f, 0.58f) },
		{ TEXT("Venus"),   TEXT("VÉNUS"),     6051.8, FLinearColor(0.95f, 0.85f, 0.55f) },
		{ TEXT("Earth"),   TEXT("TERRE"),     6371.0, FLinearColor(0.30f, 0.60f, 1.00f) },
		{ TEXT("Moon"),    TEXT("LUNE"),      1737.4, FLinearColor(0.80f, 0.80f, 0.80f) },
		{ TEXT("Mars"),    TEXT("MARS"),      3389.5, FLinearColor(0.95f, 0.40f, 0.25f) },
		{ TEXT("Jupiter"), TEXT("JUPITER"),  69911.0, FLinearColor(0.90f, 0.70f, 0.50f) },
		{ TEXT("Saturn"),  TEXT("SATURNE"),  58232.0, FLinearColor(0.95f, 0.85f, 0.55f) },
		{ TEXT("Uranus"),  TEXT("URANUS"),   25362.0, FLinearColor(0.55f, 0.90f, 0.95f) },
		{ TEXT("Neptune"), TEXT("NEPTUNE"),  24622.0, FLinearColor(0.35f, 0.50f, 1.00f) } };

	const FBodyInfo* FindBodyInfo(FName Name)
	{
		for (const FBodyInfo& B : BodyInfos)
		{
			if (Name == FName(B.Name))
			{
				return &B;
			}
		}
		return nullptr;
	}

	/** 12 345 678 km (separateur de milliers : espace). */
	FString FormatKm(double Km)
	{
		const bool bNeg = Km < 0.0;
		FString Digits = FString::Printf(TEXT("%.0f"), FMath::Abs(Km));
		FString Out;
		for (int32 i = 0; i < Digits.Len(); ++i)
		{
			if (i > 0 && (Digits.Len() - i) % 3 == 0)
			{
				Out.AppendChar(TEXT(' '));
			}
			Out.AppendChar(Digits[i]);
		}
		return (bNeg ? TEXT("-") : TEXT("")) + Out + TEXT(" km");
	}

	/** 2 j 03:25:10, ou 03:25:10 (temps simule). */
	FString FormatDuration(double Seconds)
	{
		if (!FMath::IsFinite(Seconds) || Seconds < 0.0)
		{
			return TEXT("—");
		}
		const int64 S = static_cast<int64>(Seconds + 0.5);
		const int64 Days = S / 86400;
		const int32 H = static_cast<int32>((S / 3600) % 24);
		const int32 M = static_cast<int32>((S / 60) % 60);
		const int32 Sec = static_cast<int32>(S % 60);
		return Days > 0 ? FString::Printf(TEXT("%lld j %02d:%02d:%02d"), Days, H, M, Sec)
		                : FString::Printf(TEXT("%02d:%02d:%02d"), H, M, Sec);
	}
}

void UHyperionHUDWidget::UpdateOrbitPanel()
{
	if (OrbitValues.Num() < 5)
	{
		return;
	}
	const APawn* Pawn = GetOwningPlayerPawn();
	const USolarGravityComponent* G = Pawn ? Pawn->FindComponentByClass<USolarGravityComponent>() : nullptr;
	const ASolarSystemSimulator* S = ASolarSystemSimulator::Get(this);
	const int32 Idx = (G && S) ? S->FindBodyIndex(G->ReferenceBody) : INDEX_NONE;
	if (Idx == INDEX_NONE)
	{
		for (UTextBlock* T : OrbitValues)
		{
			T->SetText(FText::FromString(TEXT("—")));
		}
		for (UTextBlock* T : OrbitAltValues)
		{
			T->SetText(FText::FromString(TEXT("—")));
		}
		return;
	}

	// Elements kepleriens a partir de la position et de la vitesse relatives (repere reel, metres).
	const double Now = S->GetSimTimeSeconds();
	const FVector R = G->RealPosition - S->BodyRealPosition(Idx, Now);
	const FVector V = G->RealVelocity - S->BodyRealVelocity(Idx, Now);
	const double Mu = S->BodyGM(Idx);
	const double r = R.Size();
	const double v2 = V.SizeSquared();
	const FVector H = R ^ V;
	const FVector EVec = (V ^ H) / Mu - R / FMath::Max(r, 1.0);
	const double E = EVec.Size();
	const double Energy = 0.5 * v2 - Mu / FMath::Max(r, 1.0);
	const double A = -Mu / (2.0 * Energy);  // demi-grand axe (negatif si hyperbolique)

	const FBodyInfo* Info = FindBodyInfo(G->ReferenceBody);
	const double RadiusKm = Info ? Info->RadiusKm : 0.0;
	OrbitRefText->SetText(FText::FromString(FString::Printf(TEXT("AUTOUR DE : %s   (ALTITUDES)"),
		Info ? Info->Label : *G->ReferenceBody.ToString().ToUpper())));

	// Alt : altitude actuelle au-dessus de la surface de l'astre de reference (orange sous la surface).
	if (OrbitAltValues.Num() > 0)
	{
		const double AltKm = r / 1000.0 - RadiusKm;
		OrbitAltValues[0]->SetText(FText::FromString(FormatKm(AltKm)));
		OrbitAltValues[0]->SetColorAndOpacity(FSlateColor(AltKm < 0.0 ? HighThrustColor : FLinearColor(0.85f, 0.95f, 1.f, 1.f)));
	}

	const bool bBound = E < 1.0 && Energy < 0.0;
	const double PeKm = (bBound ? A * (1.0 - E) : (H.SizeSquared() / Mu) / (1.0 + E)) / 1000.0 - RadiusKm;
	const double ApKm = bBound ? A * (1.0 + E) / 1000.0 - RadiusKm : 0.0;

	// Anomalie vraie : 0 au periapside ; sens donne par la vitesse radiale.
	const double RV = R | V;
	double Nu = 0.0;
	if (E > 1e-8)
	{
		Nu = FMath::Acos(FMath::Clamp((EVec | R) / (E * r), -1.0, 1.0));
		if (RV < 0.0)
		{
			Nu = 2.0 * PI - Nu;
		}
	}

	double TimeToPe = -1.0;
	double TimeToAp = -1.0;
	if (bBound)
	{
		const double N = FMath::Sqrt(Mu / (A * A * A));  // moyen mouvement (rad/s)
		const double Period = 2.0 * PI / N;
		const double EccAnom = 2.0 * FMath::Atan(FMath::Sqrt((1.0 - E) / (1.0 + E)) * FMath::Tan(Nu * 0.5));
		double M = EccAnom - E * FMath::Sin(EccAnom);
		M = FMath::Fmod(M + 2.0 * PI, 2.0 * PI);
		TimeToPe = (2.0 * PI - M) / N;
		TimeToAp = FMath::Fmod((PI - M) / N + Period, Period);
	}
	else if (E > 1.0 && RV < 0.0)
	{
		// Hyperbole, encore avant le periapside.
		const double N = FMath::Sqrt(Mu / FMath::Pow(-A, 3.0));
		const double CoshF = (E + FMath::Cos(Nu)) / (1.0 + E * FMath::Cos(Nu));
		const double F = FMath::Loge(CoshF + FMath::Sqrt(FMath::Max(CoshF * CoshF - 1.0, 0.0)));
		const double M = E * FMath::Sinh(F) - F;
		TimeToPe = M / N;
	}

	// Prochains apoapside / periapside reels : la trajectoire est propagee avec la gravite de tous les astres.
	// Les elements kepleriens ci-dessus supposent que seul l'astre de reference attire le vaisseau ; des que
	// l'apoapside s'eloigne (vers l'orbite de la Lune), l'attraction lunaire deforme l'orbite et leurs temps derivent.
	double ApShownKm = ApKm;
	double PeShownKm = PeKm;
	bool bApShown = bBound;
	KeplerTimeToAp = TimeToAp;
	KeplerTimeToPe = TimeToPe;
	if (bBound || (E > 1.0 && RV < 0.0))
	{
		const double OscPeriod = bBound ? 2.0 * PI * FMath::Sqrt(A * A * A / Mu) : 0.0;
		UpdateApsisPrediction(S, G, Idx, OscPeriod);
		if (ApsisPrediction.bApFound)
		{
			TimeToAp = ApsisPrediction.ApTime - Now;
			ApShownKm = ApsisPrediction.ApDistance / 1000.0 - RadiusKm;
			bApShown = true;
		}
		if (ApsisPrediction.bPeFound)
		{
			TimeToPe = ApsisPrediction.PeTime - Now;
			PeShownKm = ApsisPrediction.PeDistance / 1000.0 - RadiusKm;
		}
	}

	const FLinearColor Normal(0.85f, 0.95f, 1.f, 1.f);
	OrbitValues[0]->SetText(FText::FromString(bApShown ? FormatKm(ApShownKm) : FString(TEXT("ÉCHAPPEMENT"))));
	OrbitValues[1]->SetText(FText::FromString(FormatKm(PeShownKm)));
	// Periapside sous la surface : trajectoire d'impact, en orange.
	OrbitValues[1]->SetColorAndOpacity(FSlateColor(PeShownKm < 0.0 ? HighThrustColor : Normal));
	OrbitValues[2]->SetText(FText::FromString(FString::Printf(TEXT("%.4f"), E)));
	ShownTimeToAp = TimeToAp;
	ShownTimeToPe = TimeToPe;
	OrbitValues[3]->SetText(FText::FromString(FormatDuration(TimeToAp)));
	OrbitValues[4]->SetText(FText::FromString(FormatDuration(TimeToPe)));

	// RInc : inclinaison relative a l'equateur de l'astre de reference (connu pour la Terre).
	// Pole nord celeste dans le repere ecliptique J2000 : (0, sin e, cos e), e = obliquite 23,439 deg.
	if (OrbitValues.IsValidIndex(5))
	{
		FString Inc = TEXT("—");
		if (G->ReferenceBody == FName(TEXT("Earth")) && !H.IsNearlyZero())
		{
			const double Obliquity = FMath::DegreesToRadians(23.4392911);
			const FVector Pole(0.0, FMath::Sin(Obliquity), FMath::Cos(Obliquity));
			const double IncDeg = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(H.GetSafeNormal() | Pole, -1.0, 1.0)));
			Inc = FString::Printf(TEXT("%.2f°"), IncDeg);
		}
		OrbitValues[5]->SetText(FText::FromString(Inc));
	}
}

void UHyperionHUDWidget::UpdateApsisPrediction(const ASolarSystemSimulator* S, const USolarGravityComponent* G, int32 RefIdx, double OscPeriod)
{
	const double Now = S->GetSimTimeSeconds();
	const double RealNow = FPlatformTime::Seconds();
	FApsisPrediction& P = ApsisPrediction;
	// Recalcul toutes les ApsisPredictionInterval secondes reelles, ou des qu'une apside prevue est passee.
	const bool bPassed = (P.bApFound && P.ApTime <= Now) || (P.bPeFound && P.PeTime <= Now);
	if (P.ComputedRealTime > 0.0 && RealNow - P.ComputedRealTime < ApsisPredictionInterval && !bPassed && P.RefIdx == RefIdx)
	{
		return;
	}
	P = FApsisPrediction();
	P.ComputedRealTime = RealNow;
	P.RefIdx = RefIdx;

	int32 MoonIdx = INDEX_NONE;
	for (int32 i = 0; i < S->Bodies.Num(); ++i)
	{
		if (S->Bodies[i].Model == ESolarOrbitModel::Moon && i != RefIdx)
		{
			MoonIdx = i;
		}
	}

	// Horizon : un peu plus d'une periode osculatrice (bornee), sinon 60 jours.
	const double Horizon = OscPeriod > 0.0 ? FMath::Clamp(OscPeriod * 1.6, 3600.0, 200.0 * 86400.0) : 60.0 * 86400.0;
	FVector Pos = G->RealPosition;
	FVector Vel = G->RealVelocity;
	double T = Now;
	double PrevRV = 0.0;
	double PrevDist = 0.0;
	bool bHavePrev = false;
	double LastDt = 0.0;
	FVector Acc = S->GravityAt(Pos, T);
	for (int32 Step = 0; Step < 20000 && T - Now < Horizon && !(P.bApFound && P.bPeFound); ++Step)
	{
		const FVector Rel = Pos - S->BodyRealPosition(RefIdx, T);
		const FVector RelV = Vel - S->BodyRealVelocity(RefIdx, T);
		const double Dist = Rel.Size();
		const double RV = Rel | RelV;
		if (bHavePrev)
		{
			// Changement de signe de la vitesse radiale : apside entre les deux pas (interpolation lineaire).
			if ((PrevRV > 0.0 && RV <= 0.0) || (PrevRV < 0.0 && RV >= 0.0))
			{
				const double Frac = PrevRV / (PrevRV - RV);
				const double EventTime = T - (1.0 - Frac) * LastDt;
				const double EventDist = PrevDist + (Dist - PrevDist) * Frac;
				if (PrevRV > 0.0 && !P.bApFound)
				{
					P.bApFound = true; P.ApTime = EventTime; P.ApDistance = EventDist;
				}
				else if (PrevRV < 0.0 && !P.bPeFound)
				{
					P.bPeFound = true; P.PeTime = EventTime; P.PeDistance = EventDist;
				}
			}
		}
		PrevRV = RV;
		PrevDist = Dist;
		bHavePrev = true;

		// Pas adaptatif : petite fraction du temps de parcours local (et pres de la Lune, de sa distance).
		const double Speed = FMath::Max(RelV.Size(), 1.0);
		double Dt = 0.004 * Dist / Speed;
		if (MoonIdx != INDEX_NONE)
		{
			const double MoonDist = (Pos - S->BodyRealPosition(MoonIdx, T)).Size();
			Dt = FMath::Min(Dt, 0.004 * MoonDist / Speed);
		}
		Dt = FMath::Clamp(Dt, 1.0, 1800.0);
		LastDt = Dt;

		// Leapfrog (meme integrateur que le vaisseau).
		Vel += Acc * (0.5 * Dt);
		Pos += Vel * Dt;
		T += Dt;
		Acc = S->GravityAt(Pos, T);
		Vel += Acc * (0.5 * Dt);
	}
}

void UHyperionHUDWidget::PaintBrackets(const FGeometry& Allotted, const UWidget* Target, FSlateWindowElementList& Out, int32 Layer) const
{
	if (!Target)
	{
		return;
	}
	const FGeometry& G = Target->GetCachedGeometry();
	const FVector2D TL = Allotted.AbsoluteToLocal(G.GetAbsolutePosition()) - FVector2D(6.f);
	const FVector2D BR = Allotted.AbsoluteToLocal(G.GetAbsolutePosition() + G.GetAbsoluteSize()) + FVector2D(6.f);
	if ((BR - TL).X < 20.f || (BR - TL).Y < 20.f)
	{
		return;
	}
	const float L = 16.f;
	auto Corner = [&](FVector2D P, float Sx, float Sy)
	{
		TArray<FVector2f> Pts;
		Pts.Add(FVector2f(P.X + Sx * L, P.Y));
		Pts.Add(FVector2f(P.X, P.Y));
		Pts.Add(FVector2f(P.X, P.Y + Sy * L));
		FSlateDrawElement::MakeLines(Out, Layer, Allotted.ToPaintGeometry(), Pts, ESlateDrawEffect::None, AccentColor, true, 2.f);
	};
	// Equerres aux quatre coins : accent "viseur" autour du panneau.
	Corner(TL, 1.f, 1.f);
	Corner(FVector2D(BR.X, TL.Y), -1.f, 1.f);
	Corner(FVector2D(TL.X, BR.Y), 1.f, -1.f);
	Corner(BR, -1.f, -1.f);
}

int32 UHyperionHUDWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 MaxLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	if (bMapVisible)
	{
		PaintBrackets(AllottedGeometry, MapPanel, OutDrawElements, MaxLayer + 1);
		PaintBrackets(AllottedGeometry, MapSidePanel, OutDrawElements, MaxLayer + 1);
		PaintMap(AllottedGeometry, OutDrawElements, MaxLayer + 1);
		PaintMapSide(AllottedGeometry, OutDrawElements, MaxLayer + 1);
		return MaxLayer + 4;
	}
	PaintBrackets(AllottedGeometry, TopArea, OutDrawElements, MaxLayer + 1);
	PaintBrackets(AllottedGeometry, EnginePanel, OutDrawElements, MaxLayer + 1);
	PaintBrackets(AllottedGeometry, ManeuverPanel, OutDrawElements, MaxLayer + 1);
	PaintBrackets(AllottedGeometry, OrbitPanel, OutDrawElements, MaxLayer + 1);
	PaintBrackets(AllottedGeometry, RotationPanel, OutDrawElements, MaxLayer + 1);
	PaintSchematic(AllottedGeometry, OutDrawElements, MaxLayer + 1);
	return MaxLayer + 3;
}

// ===================================================================== Schema de manoeuvre

bool UHyperionHUDWidget::IsThrusterLit(const AHyperionShip* Ship, const TCHAR* Name) const
{
	if (bPreviewAllThrusters)
	{
		return true;
	}
	return Ship && Ship->IsThrusterFiring(FName(Name));
}

void UHyperionHUDWidget::PaintSchematic(const FGeometry& Allotted, FSlateWindowElementList& Out, int32 Layer) const
{
	if (!SchematicArea)
	{
		return;
	}
	const FGeometry& G = SchematicArea->GetCachedGeometry();
	const FVector2D TL = Allotted.AbsoluteToLocal(G.GetAbsolutePosition());
	const FVector2D BR = Allotted.AbsoluteToLocal(G.GetAbsolutePosition() + G.GetAbsoluteSize());
	const FVector2D Size = BR - TL;
	if (Size.X < 40.0 || Size.Y < 40.0)
	{
		return;
	}
	// Coordonnees normalisees (0..1) dans la zone -> coordonnees locales du widget.
	auto P = [&](double X, double Y) { return FVector2f(TL.X + X * Size.X, TL.Y + Y * Size.Y); };
	const FPaintGeometry PG = Allotted.ToPaintGeometry();

	FLinearColor Hull = AccentColor; Hull.A = 0.85f;
	FLinearColor Faint = AccentColor; Faint.A = 0.3f;
	auto Poly = [&](const TArray<FVector2f>& Pts, const FLinearColor& C, float W)
	{
		FSlateDrawElement::MakeLines(Out, Layer, PG, Pts, ESlateDrawEffect::None, C, true, W);
	};

	// ---------------------------------------------------------------- Silhouette (vue de profil, proue a droite)
	const double Cy = 0.30;  // axe du vaisseau
	Poly({ P(0.97, Cy), P(0.86, Cy - 0.07), P(0.20, Cy - 0.07), P(0.14, Cy - 0.10), P(0.08, Cy - 0.10),
	       P(0.08, Cy + 0.10), P(0.14, Cy + 0.10), P(0.20, Cy + 0.07), P(0.86, Cy + 0.07), P(0.97, Cy) }, Hull, 1.5f);
	// Passerelle
	Poly({ P(0.68, Cy - 0.07), P(0.70, Cy - 0.13), P(0.77, Cy - 0.13), P(0.79, Cy - 0.07) }, Hull, 1.2f);
	// Tuyeres principales
	Poly({ P(0.08, Cy - 0.08), P(0.03, Cy - 0.12), P(0.03, Cy - 0.01), P(0.08, Cy - 0.02) }, Hull, 1.2f);
	Poly({ P(0.08, Cy + 0.02), P(0.03, Cy + 0.01), P(0.03, Cy + 0.12), P(0.08, Cy + 0.08) }, Hull, 1.2f);
	// Anneau gravitationnel (vu par la tranche : ellipse haute et fine) et ses rayons
	{
		TArray<FVector2f> Ring;
		for (int32 i = 0; i <= 40; ++i)
		{
			const double A = 2.0 * PI * i / 40.0;
			Ring.Add(P(0.46 + 0.035 * FMath::Cos(A), Cy + 0.27 * FMath::Sin(A)));
		}
		Poly(Ring, Hull, 1.5f);
		Poly({ P(0.46, Cy - 0.27), P(0.46, Cy + 0.27) }, Faint, 1.f);
	}

	// ---------------------------------------------------------------- Croix des propulseurs (avant / arriere)
	const ESlateDrawEffect Fx = ESlateDrawEffect::None;
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 8);
	const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 7);

	auto Marker = [&](const FVector2f& C, bool bLit, const TCHAR* Letter, const FVector2f& LabelOffset)
	{
		const float S = 10.f;
		if (bLit)
		{
			// Allume : carre plein, couleur fixe (simple ON / OFF, sans halo ni scintillement).
			const FLinearColor Core = ThrusterActiveColor;
			FSlateDrawElement::MakeBox(Out, Layer + 1, Allotted.ToPaintGeometry(FVector2f(S), FSlateLayoutTransform(C - FVector2f(S * 0.5f))), &MarkerFillBrush, Fx, Core);
		}
		else
		{
			FLinearColor Off = AccentColor; Off.A = 0.55f;
			FSlateDrawElement::MakeBox(Out, Layer, Allotted.ToPaintGeometry(FVector2f(S), FSlateLayoutTransform(C - FVector2f(S * 0.5f))), &MarkerOutlineBrush, Fx, Off);
		}
		FLinearColor TextColor = bLit ? ThrusterActiveColor : Faint;
		FSlateDrawElement::MakeText(Out, Layer + 1, Allotted.ToPaintGeometry(FVector2f(14.f, 12.f), FSlateLayoutTransform(C + LabelOffset)),
			FString(Letter), Small, Fx, TextColor);
	};

	const double ClusterY = 0.74;
	const float Arm = 20.f;
	// Titre centre sous un groupe.
	auto Title = [&](const FVector2f& C, const TCHAR* Text)
	{
		const FVector2D TextSize = FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(FString(Text), Font);
		FSlateDrawElement::MakeText(Out, Layer, Allotted.ToPaintGeometry(FVector2f(TextSize), FSlateLayoutTransform(FVector2f(C.X - TextSize.X * 0.5f, C.Y + Arm + 9.f))),
			FString(Text), Font, Fx, Hull);
	};

	auto Cluster = [&](double X, const TCHAR* TitleText, double HullX, bool bRear, const AHyperionShip* Ship)
	{
		const FVector2f C = P(X, ClusterY);
		// Repere du vaisseau : trait entre le groupe et sa position sur la coque
		Poly({ P(HullX, Cy + 0.08), FVector2f(C.X, C.Y - Arm - 8.f) }, Faint, 1.f);
		Poly({ FVector2f(C.X - Arm, C.Y), FVector2f(C.X + Arm, C.Y) }, Faint, 1.f);
		Poly({ FVector2f(C.X, C.Y - Arm), FVector2f(C.X, C.Y + Arm) }, Faint, 1.f);

		static const TCHAR* Faces[] = { TEXT("H"), TEXT("B"), TEXT("G"), TEXT("D") };
		const FVector2f Dirs[] = { FVector2f(0.f, -1.f), FVector2f(0.f, 1.f), FVector2f(-1.f, 0.f), FVector2f(1.f, 0.f) };
		for (int32 f = 0; f < 4; ++f)
		{
			const FVector2f Dir = Dirs[f];
			const FVector2f Side(-Dir.Y, Dir.X);  // perpendiculaire, pour les paires arriere
			const FVector2f LabelOff = Dir * 9.f + FVector2f(3.f, -6.f);
			if (!bRear)
			{
				const FString Name = FString(TEXT("SM_Propulseur_Manoeuvre_")) + Faces[f];
				Marker(C + Dir * Arm, IsThrusterLit(Ship, *Name), Faces[f], LabelOff);
			}
			else
			{
				for (int32 k = 1; k <= 2; ++k)
				{
					const FString Name = FString::Printf(TEXT("SM_Propulseur_Manoeuvre_AR_%s%d"), Faces[f], k);
					const FVector2f Pos = C + Dir * Arm + Side * (k == 1 ? -6.f : 6.f);
					Marker(Pos, IsThrusterLit(Ship, *Name), k == 1 ? Faces[f] : TEXT(""), LabelOff + Side * -6.f);
				}
			}
		}
		Title(C, TitleText);
	};

	// Section 3 : 4 propulseurs par cote sur les faces avant, tuyeres vers l'avant.
	// Vue de face comme les autres groupes : G a gauche, D a droite ; colonnes interieure (1, 2) et
	// exterieure (3, 4), rangees haute (1, 3) et basse (2, 4).
	auto ClusterS3 = [&](double X, const TCHAR* TitleText, double HullX, const AHyperionShip* Ship)
	{
		const FVector2f C = P(X, ClusterY);
		Poly({ P(HullX, Cy + 0.08), FVector2f(C.X, C.Y - Arm - 8.f) }, Faint, 1.f);
		// Coque de la S3 vue de face (large, avec le moyeu au centre)
		Poly({ FVector2f(C.X - 38.f, C.Y - 13.f), FVector2f(C.X + 38.f, C.Y - 13.f), FVector2f(C.X + 38.f, C.Y + 13.f),
		       FVector2f(C.X - 38.f, C.Y + 13.f), FVector2f(C.X - 38.f, C.Y - 13.f) }, Faint, 1.f);
		Poly({ FVector2f(C.X - 3.f, C.Y), FVector2f(C.X + 3.f, C.Y) }, Faint, 1.f);
		Poly({ FVector2f(C.X, C.Y - 3.f), FVector2f(C.X, C.Y + 3.f) }, Faint, 1.f);

		for (int32 SideIdx = 0; SideIdx < 2; ++SideIdx)
		{
			const TCHAR* Face = SideIdx == 0 ? TEXT("G") : TEXT("D");
			const float Sx = SideIdx == 0 ? -1.f : 1.f;
			for (int32 k = 1; k <= 4; ++k)
			{
				const float Col = (k <= 2) ? 14.f : 28.f;     // interieur / exterieur
				const float Row = (k % 2 == 1) ? -6.5f : 6.5f;  // haut / bas
				const FString Name = FString::Printf(TEXT("SM_Propulseur_Manoeuvre_S3_%s%d"), Face, k);
				const bool bLabel = (k == 3);
				Marker(FVector2f(C.X + Sx * Col, C.Y + Row), IsThrusterLit(Ship, *Name), bLabel ? Face : TEXT(""),
					FVector2f(Sx < 0.f ? -21.f : 9.f, 0.f));
			}
		}
		Title(C, TitleText);
	};

	const AHyperionShip* Ship = FindShip();
	Cluster(0.83, TEXT("AVANT"), 0.84, false, Ship);
	ClusterS3(0.50, TEXT("SECTION 3"), 0.80, Ship);
	Cluster(0.17, TEXT("ARRIÈRE"), 0.16, true, Ship);
}

// ===================================================================== Boutons

void UHyperionHUDWidget::SetButtonLabel(int32 Index, FText Label)
{
	if (ButtonLabels.IsValidIndex(Index))
	{
		ButtonLabels[Index] = Label;
	}
	if (ButtonTexts.IsValidIndex(Index) && ButtonTexts[Index])
	{
		ButtonTexts[Index]->SetText(Label);
	}
}

void UHyperionHUDWidget::HandleButton0() { OnHudButtonClicked.Broadcast(0); }
void UHyperionHUDWidget::HandleButton1() { OnHudButtonClicked.Broadcast(1); }
void UHyperionHUDWidget::HandleButton2() { OnHudButtonClicked.Broadcast(2); }
void UHyperionHUDWidget::HandleButton3() { OnHudButtonClicked.Broadcast(3); }
void UHyperionHUDWidget::HandleButton4() { OnHudButtonClicked.Broadcast(4); }
void UHyperionHUDWidget::HandleButton5() { OnHudButtonClicked.Broadcast(5); }

// ===================================================================== Acceleration du temps

UButton* UHyperionHUDWidget::MakeTimeButton(int32 Index, UHorizontalBox* Bar)
{
	UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
	Button->SetStyle(MakeButtonStyle(false));
	TimeButtons.Add(Button);
	TimeButtonActive.Add(false);

	UTextBlock* Text = MakeText(FString::Printf(TEXT("×%d"), TimeWarpValues[Index]), 9, AccentColor);
	Text->SetJustification(ETextJustify::Center);
	TimeButtonTexts.Add(Text);
	Button->AddChild(Text);

	USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
	Size->SetWidthOverride(76.f);
	Size->SetHeightOverride(24.f);
	Size->AddChild(Button);
	Bar->AddChildToHorizontalBox(Size)->SetPadding(FMargin(4.f, 0.f));

	switch (Index)
	{
	case 0: Button->OnClicked.AddDynamic(this, &UHyperionHUDWidget::HandleTime0); break;
	case 1: Button->OnClicked.AddDynamic(this, &UHyperionHUDWidget::HandleTime1); break;
	case 2: Button->OnClicked.AddDynamic(this, &UHyperionHUDWidget::HandleTime2); break;
	case 3: Button->OnClicked.AddDynamic(this, &UHyperionHUDWidget::HandleTime3); break;
	default: break;
	}
	return Button;
}

void UHyperionHUDWidget::SetTimeWarp(int32 Index)
{
	if (ASolarSystemSimulator* S = ASolarSystemSimulator::Get(this))
	{
		if (Index >= 0 && Index < UE_ARRAY_COUNT(TimeWarpValues))
		{
			// Acceleration hors poussee ; pendant une poussee le temps reste a x1 (ThrustTimeScale).
			S->WarpTimeScale = TimeWarpValues[Index];
		}
	}
}

void UHyperionHUDWidget::UpdateTimeButtons()
{
	const ASolarSystemSimulator* S = ASolarSystemSimulator::Get(this);
	for (int32 i = 0; i < TimeButtons.Num(); ++i)
	{
		// Le bouton allume indique l'acceleration en cours (x1 pendant une poussee).
		const bool bActive = S && FMath::IsNearlyEqual(S->TimeScale, static_cast<double>(TimeWarpValues[i]), 0.5);
		if (TimeButtonActive[i] != bActive)
		{
			TimeButtonActive[i] = bActive;
			TimeButtons[i]->SetStyle(MakeButtonStyle(bActive));
			if (TimeButtonTexts.IsValidIndex(i))
			{
				TimeButtonTexts[i]->SetColorAndOpacity(FSlateColor(bActive ? ActiveButtonColor : AccentColor));
			}
		}
	}
}

void UHyperionHUDWidget::HandleTime0() { SetTimeWarp(0); }
void UHyperionHUDWidget::HandleTime1() { SetTimeWarp(1); }
void UHyperionHUDWidget::HandleTime2() { SetTimeWarp(2); }
void UHyperionHUDWidget::HandleTime3() { SetTimeWarp(3); }

// ===================================================================== Carte (F1)

void UHyperionHUDWidget::BuildMapView(UCanvasPanel* Root)
{
	FLinearColor Dim = AccentColor; Dim.A = 0.6f;
	FLinearColor Outline = AccentColor; Outline.A = 0.45f;
	// Fond plus opaque que les autres panneaux : la carte recouvre la vue 3D.
	const FLinearColor MapFill(0.008f, 0.025f, 0.045f, 0.94f);

	UCanvasPanel* Layer = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MapLayer"));
	UCanvasPanelSlot* LayerSlot = Root->AddChildToCanvas(Layer);
	LayerSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
	LayerSlot->SetOffsets(FMargin(0.f));
	MapLayer = Layer;

	// ---------------------------------------------------------------- Carte (2/3 gauche)
	UVerticalBox* MBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	MBox->AddChildToVerticalBox(MakeText(TEXT("CARTE DU SYSTÈME SOLAIRE"), 14, AccentColor))->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	MBox->AddChildToVerticalBox(MakeText(TEXT("VUE DU DESSUS DE L'ÉCLIPTIQUE   ·   DISTANCES AU SOLEIL EN ÉCHELLE LOGARITHMIQUE"), 9, Dim, false));
	MapDateText = MakeText(TEXT("—"), 9, Dim, false);
	MBox->AddChildToVerticalBox(MapDateText)->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));
	USpacer* Area = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("MapArea"));
	UVerticalBoxSlot* AreaSlot = MBox->AddChildToVerticalBox(Area);
	AreaSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MapArea = Area;

	MapPanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MapPanel"));
	MapPanel->SetBrush(FSlateRoundedBoxBrush(MapFill, FVector4(0.f, 16.f, 0.f, 16.f), Outline, 1.f));
	MapPanel->SetPadding(FMargin(18.f, 14.f));
	MapPanel->SetContent(MBox);
	UCanvasPanelSlot* MSlotMap = Layer->AddChildToCanvas(MapPanel);
	MSlotMap->SetAnchors(FAnchors(0.f, 0.f, 2.f / 3.f, 1.f));
	MSlotMap->SetOffsets(FMargin(28.f, 28.f, 10.f, 28.f));  // marges gauche, haut, droite, bas

	// ---------------------------------------------------------------- Astres (1/3 droit)
	UVerticalBox* SBox = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass());
	SBox->AddChildToVerticalBox(MakeText(TEXT("ASTRES"), 14, AccentColor))->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
	SBox->AddChildToVerticalBox(MakeText(TEXT("DIST. SOLEIL   ·   DIST. HYPÉRION"), 9, Dim, false))->SetPadding(FMargin(0.f, 0.f, 0.f, 6.f));
	USpacer* SideArea = WidgetTree->ConstructWidget<USpacer>(USpacer::StaticClass(), TEXT("MapSideArea"));
	SBox->AddChildToVerticalBox(SideArea)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	MapSideArea = SideArea;
	SBox->AddChildToVerticalBox(MakeText(TEXT("F1 : FERMER LA CARTE"), 9, Dim, false));

	MapSidePanel = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("MapSidePanel"));
	MapSidePanel->SetBrush(FSlateRoundedBoxBrush(MapFill, FVector4(16.f, 0.f, 16.f, 0.f), Outline, 1.f));
	MapSidePanel->SetPadding(FMargin(18.f, 14.f));
	MapSidePanel->SetContent(SBox);
	UCanvasPanelSlot* SSlot = Layer->AddChildToCanvas(MapSidePanel);
	SSlot->SetAnchors(FAnchors(2.f / 3.f, 0.f, 1.f, 1.f));
	SSlot->SetOffsets(FMargin(10.f, 28.f, 28.f, 28.f));

	Layer->SetVisibility(bMapVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UHyperionHUDWidget::ToggleMapView()
{
	SetMapViewVisible(!bMapVisible);
}

void UHyperionHUDWidget::SetMapViewVisible(bool bVisible)
{
	bMapVisible = bVisible;
	const ESlateVisibility Hud = bVisible ? ESlateVisibility::Collapsed : ESlateVisibility::Visible;
	UWidget* const HudPanels[] = { TopArea.Get(), EnginePanel.Get(), ManeuverPanel.Get(), OrbitPanel.Get(), RotationPanel.Get() };
	for (UWidget* W : HudPanels)
	{
		if (W)
		{
			W->SetVisibility(Hud);
		}
	}
	if (MapLayer)
	{
		MapLayer->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void UHyperionHUDWidget::RefreshOrbitCache(const ASolarSystemSimulator* S) const
{
	const double Now = S->GetSimTimeSeconds();
	// Les orbites changent tres lentement : recalcul une fois par jour simule.
	if (OrbitCache.Num() == S->Bodies.Num() && FMath::Abs(Now - OrbitCacheTime) < 86400.0)
	{
		return;
	}
	OrbitCacheTime = Now;
	OrbitCache.SetNum(S->Bodies.Num());
	int32 SunIdx = INDEX_NONE;
	int32 EarthIdx = INDEX_NONE;
	int32 MoonIdx = INDEX_NONE;
	for (int32 i = 0; i < S->Bodies.Num(); ++i)
	{
		switch (S->Bodies[i].Model)
		{
		case ESolarOrbitModel::Fixed:     SunIdx = i; break;
		case ESolarOrbitModel::EarthMoon: EarthIdx = i; break;
		case ESolarOrbitModel::Moon:      MoonIdx = i; break;
		default: break;
		}
	}
	const double MuSun = SunIdx != INDEX_NONE ? S->BodyGM(SunIdx) : 1.32712440018e20;
	const int32 Samples = 180;
	for (int32 i = 0; i < S->Bodies.Num(); ++i)
	{
		TArray<FVector2D>& Path = OrbitCache[i];
		Path.Reset();
		const ESolarOrbitModel Model = S->Bodies[i].Model;
		if (Model != ESolarOrbitModel::Planet && Model != ESolarOrbitModel::EarthMoon)
		{
			continue;
		}
		// Periode a partir du demi-grand axe (vis-viva), puis echantillonnage des positions reelles.
		const FVector R = S->BodyRealPosition(i, Now);
		const FVector V = S->BodyRealVelocity(i, Now);
		const double InvA = 2.0 / FMath::Max(R.Size(), 1.0) - V.SizeSquared() / MuSun;
		if (InvA <= 0.0)
		{
			continue;
		}
		const double A = 1.0 / InvA;
		const double Period = 2.0 * PI * FMath::Sqrt(A * A * A / MuSun);
		for (int32 k = 0; k <= Samples; ++k)
		{
			const FVector P = S->BodyRealPosition(i, Now + Period * k / Samples);
			Path.Add(FVector2D(P.X, P.Y));
		}
	}
	MoonOrbitCache.Reset();
	if (MoonIdx != INDEX_NONE && EarthIdx != INDEX_NONE)
	{
		const double MoonPeriod = 27.321661 * 86400.0;
		for (int32 k = 0; k <= 120; ++k)
		{
			const double T = Now + MoonPeriod * k / 120;
			const FVector D = S->BodyRealPosition(MoonIdx, T) - S->BodyRealPosition(EarthIdx, T);
			MoonOrbitCache.Add(FVector2D(D.X, D.Y));
		}
	}
}

namespace
{
	/** Petit cercle (polyligne) en coordonnees locales du widget. */
	void DrawCircle(FSlateWindowElementList& Out, int32 Layer, const FPaintGeometry& PG, const FVector2f& C, float Radius,
		const FLinearColor& Color, float Thickness, int32 Segments = 64)
	{
		TArray<FVector2f> Pts;
		Pts.Reserve(Segments + 1);
		for (int32 i = 0; i <= Segments; ++i)
		{
			const float A = 2.f * PI * i / Segments;
			Pts.Add(C + FVector2f(FMath::Cos(A), FMath::Sin(A)) * Radius);
		}
		FSlateDrawElement::MakeLines(Out, Layer, PG, Pts, ESlateDrawEffect::None, Color, true, Thickness);
	}

	FLinearColor WithAlpha(FLinearColor C, float A)
	{
		C.A = A;
		return C;
	}

	/** Distance lisible : km sous 0,1 UA, UA au-dela. */
	FString FormatMapDistance(double Meters)
	{
		const double AU = 1.495978707e11;
		const double Km = Meters / 1000.0;
		if (Meters < 0.1 * AU)
		{
			return Km < 1.0e6 ? FString::Printf(TEXT("%.0f km"), Km) : FString::Printf(TEXT("%.2f Mkm"), Km / 1.0e6);
		}
		return FString::Printf(TEXT("%.2f UA"), Meters / AU);
	}
}

void UHyperionHUDWidget::PaintMap(const FGeometry& Allotted, FSlateWindowElementList& Out, int32 Layer) const
{
	const ASolarSystemSimulator* S = ASolarSystemSimulator::Get(this);
	if (!S || !MapArea)
	{
		return;
	}
	const FGeometry& G = MapArea->GetCachedGeometry();
	const FVector2D TL = Allotted.AbsoluteToLocal(G.GetAbsolutePosition());
	const FVector2D BR = Allotted.AbsoluteToLocal(G.GetAbsolutePosition() + G.GetAbsoluteSize());
	const FVector2D Size = BR - TL;
	if (Size.X < 100.0 || Size.Y < 100.0)
	{
		return;
	}
	RefreshOrbitCache(S);

	const FPaintGeometry PG = Allotted.ToPaintGeometry();
	const ESlateDrawEffect Fx = ESlateDrawEffect::None;
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 8);
	const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 7);
	const double AU = 1.495978707e11;
	const double Now = S->GetSimTimeSeconds();

	// Systeme solaire centre a droite de la zone (l'encart Terre-Lune occupe le coin inferieur gauche).
	const float R = static_cast<float>(FMath::Min(Size.X * 0.5, Size.Y * 0.5) - 8.0);
	const FVector2f C(static_cast<float>(BR.X) - R - 8.f, static_cast<float>(TL.Y + Size.Y * 0.5));
	const double LogDen = FMath::Loge(1.0 + MapOuterAU / MapLogKneeAU);
	auto Radial = [&](double DistAU) { return static_cast<float>(R * FMath::Loge(1.0 + DistAU / MapLogKneeAU) / LogDen); };
	// Repere reel (ecliptique, direct) -> ecran : X a droite, Y vers le haut.
	auto MapPt = [&](double X, double Y)
	{
		const double D = FMath::Sqrt(X * X + Y * Y);
		if (D < 1.0)
		{
			return C;
		}
		const float Rho = Radial(D / AU);
		return C + FVector2f(static_cast<float>(X / D) * Rho, static_cast<float>(-Y / D) * Rho);
	};

	// Graduations : cercles de distance.
	const FLinearColor GridColor = WithAlpha(AccentColor, 0.12f);
	for (const double Ring : { 0.5, 1.0, 2.0, 5.0, 10.0, 20.0, 30.0 })
	{
		if (Ring > MapOuterAU)
		{
			continue;
		}
		const float Rr = Radial(Ring);
		DrawCircle(Out, Layer, PG, C, Rr, GridColor, 1.f, 96);
		FSlateDrawElement::MakeText(Out, Layer, Allotted.ToPaintGeometry(FVector2f(60.f, 10.f), FSlateLayoutTransform(C + FVector2f(Rr * 0.7071f + 2.f, -Rr * 0.7071f - 10.f))),
			FString::Printf(TEXT("%g UA"), Ring), Small, Fx, WithAlpha(AccentColor, 0.35f));
	}
	// Direction du point vernal (axe X de l'ecliptique)
	FSlateDrawElement::MakeLines(Out, Layer, PG, { C, C + FVector2f(R, 0.f) }, Fx, WithAlpha(AccentColor, 0.08f), true, 1.f);

	// Orbites puis astres.
	for (int32 i = 0; i < S->Bodies.Num() && i < OrbitCache.Num(); ++i)
	{
		const FBodyInfo* Info = FindBodyInfo(S->Bodies[i].Name);
		const FLinearColor Col = Info ? Info->Color : AccentColor;
		if (OrbitCache[i].Num() > 1)
		{
			TArray<FVector2f> Pts;
			Pts.Reserve(OrbitCache[i].Num());
			for (const FVector2D& P : OrbitCache[i])
			{
				Pts.Add(MapPt(P.X, P.Y));
			}
			FSlateDrawElement::MakeLines(Out, Layer, PG, Pts, Fx, WithAlpha(Col, 0.35f), true, 1.f);
		}
	}
	for (int32 i = 0; i < S->Bodies.Num(); ++i)
	{
		const FSolarBody& B = S->Bodies[i];
		if (B.Model == ESolarOrbitModel::Moon)
		{
			continue;  // trop proche de la Terre a cette echelle : voir l'encart
		}
		const FBodyInfo* Info = FindBodyInfo(B.Name);
		const FLinearColor Col = Info ? Info->Color : AccentColor;
		const FVector P = S->BodyRealPosition(i, Now);
		const FVector2f Pt = MapPt(P.X, P.Y);
		const float Sz = B.Model == ESolarOrbitModel::Fixed ? 14.f : 8.f;
		FSlateDrawElement::MakeBox(Out, Layer + 1, Allotted.ToPaintGeometry(FVector2f(Sz), FSlateLayoutTransform(Pt - FVector2f(Sz * 0.5f))), &MarkerFillBrush, Fx, Col);
		FSlateDrawElement::MakeText(Out, Layer + 1, Allotted.ToPaintGeometry(FVector2f(80.f, 12.f), FSlateLayoutTransform(Pt + FVector2f(Sz * 0.5f + 4.f, -Sz * 0.5f - 8.f))),
			FString(Info ? Info->Label : *B.Name.ToString()), Font, Fx, WithAlpha(Col, 0.95f));
	}

	// Hyperion (heliocentrique) : losange orange.
	const APawn* Pawn = GetOwningPlayerPawn();
	const USolarGravityComponent* Grav = Pawn ? Pawn->FindComponentByClass<USolarGravityComponent>() : nullptr;
	auto Diamond = [&](const FVector2f& P, float Rad, int32 L)
	{
		const TArray<FVector2f> D = { P + FVector2f(0.f, -Rad), P + FVector2f(Rad, 0.f), P + FVector2f(0.f, Rad), P + FVector2f(-Rad, 0.f), P + FVector2f(0.f, -Rad) };
		FSlateDrawElement::MakeLines(Out, L, PG, D, Fx, ThrusterActiveColor, true, 2.f);
	};
	if (Grav)
	{
		const FVector2f HP = MapPt(Grav->RealPosition.X, Grav->RealPosition.Y);
		Diamond(HP, 7.f, Layer + 2);
		FSlateDrawElement::MakeText(Out, Layer + 2, Allotted.ToPaintGeometry(FVector2f(80.f, 12.f), FSlateLayoutTransform(HP + FVector2f(8.f, 4.f))),
			FString(TEXT("HYPÉRION")), Font, Fx, ThrusterActiveColor);
	}

	// ---------------------------------------------------------------- Encart Terre - Lune - Hyperion (echelle lineaire)
	int32 EarthIdx = INDEX_NONE;
	int32 MoonIdx = INDEX_NONE;
	for (int32 i = 0; i < S->Bodies.Num(); ++i)
	{
		EarthIdx = S->Bodies[i].Model == ESolarOrbitModel::EarthMoon ? i : EarthIdx;
		MoonIdx = S->Bodies[i].Model == ESolarOrbitModel::Moon ? i : MoonIdx;
	}
	if (EarthIdx == INDEX_NONE)
	{
		return;
	}
	const float Box = static_cast<float>(FMath::Clamp(FMath::Min(Size.X - 2.0 * R - 24.0, Size.Y * 0.45), 180.0, 320.0));
	const FVector2f BoxTL(static_cast<float>(TL.X), static_cast<float>(BR.Y) - Box);
	const FVector2f IC = BoxTL + FVector2f(Box * 0.5f, Box * 0.5f + 8.f);
	const TArray<FVector2f> Frame = { BoxTL, BoxTL + FVector2f(Box, 0.f), BoxTL + FVector2f(Box, Box), BoxTL + FVector2f(0.f, Box), BoxTL };
	FSlateDrawElement::MakeBox(Out, Layer, Allotted.ToPaintGeometry(FVector2f(Box), FSlateLayoutTransform(BoxTL)), &MarkerFillBrush, Fx, FLinearColor(0.f, 0.f, 0.f, 0.55f));
	FSlateDrawElement::MakeLines(Out, Layer, PG, Frame, Fx, WithAlpha(AccentColor, 0.45f), true, 1.f);
	FSlateDrawElement::MakeText(Out, Layer + 1, Allotted.ToPaintGeometry(FVector2f(Box, 12.f), FSlateLayoutTransform(BoxTL + FVector2f(8.f, 6.f))),
		FString(TEXT("TERRE · LUNE · HYPÉRION")), Font, Fx, AccentColor);

	const FVector EarthPos = S->BodyRealPosition(EarthIdx, Now);
	const FVector MoonRel = MoonIdx != INDEX_NONE ? S->BodyRealPosition(MoonIdx, Now) - EarthPos : FVector::ZeroVector;

	// Orbite de l'Hyperion autour de la Terre (ellipse keplerienne) si elle est liee.
	TArray<FVector2D> ShipOrbit;
	FVector ShipRel = FVector::ZeroVector;
	double ShipExtent = 0.0;  // plus grande distance a la Terre sur l'orbite de l'Hyperion
	if (Grav)
	{
		ShipRel = Grav->RealPosition - EarthPos;
		const FVector V = Grav->RealVelocity - S->BodyRealVelocity(EarthIdx, Now);
		const double Mu = S->BodyGM(EarthIdx);
		const FVector H = ShipRel ^ V;
		const FVector E = (V ^ H) / Mu - ShipRel / FMath::Max(ShipRel.Size(), 1.0);
		const double Ecc = E.Size();
		const double Pp = H.SizeSquared() / Mu;
		if (Ecc < 0.99 && !H.IsNearlyZero())
		{
			const FVector Pu = Ecc > 1e-6 ? E / Ecc : ShipRel.GetSafeNormal();
			const FVector Qu = (H.GetSafeNormal() ^ Pu).GetSafeNormal();
			for (int32 k = 0; k <= 120; ++k)
			{
				const double Nu = 2.0 * PI * k / 120;
				const double Rr = Pp / (1.0 + Ecc * FMath::Cos(Nu));
				const FVector P = (Pu * FMath::Cos(Nu) + Qu * FMath::Sin(Nu)) * Rr;
				ShipOrbit.Add(FVector2D(P.X, P.Y));
				ShipExtent = FMath::Max(ShipExtent, Rr);
			}
		}
		ShipExtent = FMath::Max(ShipExtent, ShipRel.Size());
	}
	// Echelle calee sur l'orbite de l'Hyperion (sinon il se confond avec la Terre) ; la Lune n'est dessinee
	// a sa place que si elle tient dans l'encart, sinon elle est signalee au bord, dans sa direction.
	const double MoonDist = MoonRel.Size();
	double Extent = FMath::Max(ShipExtent * 1.8, 2.0e7);
	if (MoonDist > 0.0 && MoonDist < Extent * 1.6)
	{
		Extent = FMath::Max(Extent, MoonDist);
	}
	const bool bMoonInside = MoonDist > 0.0 && MoonDist <= Extent;
	const double Scale = (Box * 0.5 - 18.0) / (Extent * 1.05);
	auto InsetPt = [&](double X, double Y) { return IC + FVector2f(static_cast<float>(X * Scale), static_cast<float>(-Y * Scale)); };

	if (bMoonInside && MoonOrbitCache.Num() > 1)
	{
		TArray<FVector2f> Pts;
		for (const FVector2D& P : MoonOrbitCache)
		{
			Pts.Add(InsetPt(P.X, P.Y));
		}
		FSlateDrawElement::MakeLines(Out, Layer + 1, PG, Pts, Fx, WithAlpha(FLinearColor(0.8f, 0.8f, 0.8f), 0.35f), true, 1.f);
	}
	if (ShipOrbit.Num() > 1)
	{
		TArray<FVector2f> Pts;
		for (const FVector2D& P : ShipOrbit)
		{
			Pts.Add(InsetPt(P.X, P.Y));
		}
		FSlateDrawElement::MakeLines(Out, Layer + 1, PG, Pts, Fx, WithAlpha(ThrusterActiveColor, 0.55f), true, 1.f);
	}
	const FBodyInfo* EarthInfo = FindBodyInfo(S->Bodies[EarthIdx].Name);
	const FLinearColor EarthCol = EarthInfo ? EarthInfo->Color : AccentColor;
	FSlateDrawElement::MakeBox(Out, Layer + 2, Allotted.ToPaintGeometry(FVector2f(10.f), FSlateLayoutTransform(IC - FVector2f(5.f))), &MarkerFillBrush, Fx, EarthCol);
	FSlateDrawElement::MakeText(Out, Layer + 2, Allotted.ToPaintGeometry(FVector2f(60.f, 12.f), FSlateLayoutTransform(IC + FVector2f(7.f, -14.f))),
		FString(TEXT("TERRE")), Font, Fx, EarthCol);
	if (MoonIdx != INDEX_NONE && MoonDist > 0.0)
	{
		const FLinearColor MoonCol(0.8f, 0.8f, 0.8f);
		if (bMoonInside)
		{
			const FVector2f MP = InsetPt(MoonRel.X, MoonRel.Y);
			FSlateDrawElement::MakeBox(Out, Layer + 2, Allotted.ToPaintGeometry(FVector2f(7.f), FSlateLayoutTransform(MP - FVector2f(3.5f))), &MarkerFillBrush, Fx, MoonCol);
			FSlateDrawElement::MakeText(Out, Layer + 2, Allotted.ToPaintGeometry(FVector2f(60.f, 12.f), FSlateLayoutTransform(MP + FVector2f(6.f, -12.f))),
				FString(TEXT("LUNE")), Font, Fx, MoonCol);
		}
		else
		{
			// Hors champ : fleche au bord de l'encart, dans la direction de la Lune.
			const FVector2f Dir = FVector2f(static_cast<float>(MoonRel.X), static_cast<float>(-MoonRel.Y)).GetSafeNormal();
			const float Edge = Box * 0.5f - 14.f;
			const FVector2f Tip = IC + Dir * Edge;
			const FVector2f Side(-Dir.Y, Dir.X);
			FSlateDrawElement::MakeLines(Out, Layer + 2, PG, { Tip - Dir * 10.f + Side * 5.f, Tip, Tip - Dir * 10.f - Side * 5.f }, Fx, MoonCol, true, 1.5f);
			const FVector2f LabelPos = Tip - Dir * 22.f - FVector2f(30.f, 6.f);
			FSlateDrawElement::MakeText(Out, Layer + 2, Allotted.ToPaintGeometry(FVector2f(110.f, 12.f), FSlateLayoutTransform(LabelPos)),
				FString::Printf(TEXT("LUNE %s"), *FormatMapDistance(MoonDist)), Small, Fx, MoonCol);
		}
	}
	if (Grav)
	{
		const FVector2f SP = InsetPt(ShipRel.X, ShipRel.Y);
		Diamond(SP, 5.f, Layer + 3);
	}
	// Sous le titre (le bas de l'encart peut accueillir la fleche de la Lune).
	FSlateDrawElement::MakeText(Out, Layer + 1, Allotted.ToPaintGeometry(FVector2f(Box, 10.f), FSlateLayoutTransform(BoxTL + FVector2f(8.f, 20.f))),
		FString::Printf(TEXT("ÉCHELLE LINÉAIRE · RAYON %s"), *FormatMapDistance(Extent * 1.05)), Small, Fx, WithAlpha(AccentColor, 0.5f));
}

void UHyperionHUDWidget::PaintMapSide(const FGeometry& Allotted, FSlateWindowElementList& Out, int32 Layer) const
{
	const ASolarSystemSimulator* S = ASolarSystemSimulator::Get(this);
	if (!S || !MapSideArea)
	{
		return;
	}
	const FGeometry& G = MapSideArea->GetCachedGeometry();
	const FVector2D TL = Allotted.AbsoluteToLocal(G.GetAbsolutePosition());
	const FVector2D Size = Allotted.AbsoluteToLocal(G.GetAbsolutePosition() + G.GetAbsoluteSize()) - TL;
	if (Size.X < 60.0)
	{
		return;
	}
	const ESlateDrawEffect Fx = ESlateDrawEffect::None;
	const FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle("Bold", 10);
	const FSlateFontInfo Small = FCoreStyle::GetDefaultFontStyle("Regular", 9);
	const double Now = S->GetSimTimeSeconds();
	const APawn* Pawn = GetOwningPlayerPawn();
	const USolarGravityComponent* Grav = Pawn ? Pawn->FindComponentByClass<USolarGravityComponent>() : nullptr;
	const FLinearColor ValueCol(0.85f, 0.95f, 1.f, 1.f);

	float Y = static_cast<float>(TL.Y) + 4.f;
	const float X0 = static_cast<float>(TL.X);
	const float W = static_cast<float>(Size.X);
	auto Row = [&](const FLinearColor& Col, const FString& Name, const FString& DistSun, const FString& DistShip, bool bDiamond)
	{
		if (bDiamond)
		{
			const FVector2f P(X0 + 6.f, Y + 7.f);
			FSlateDrawElement::MakeLines(Out, Layer, Allotted.ToPaintGeometry(),
				{ P + FVector2f(0.f, -6.f), P + FVector2f(6.f, 0.f), P + FVector2f(0.f, 6.f), P + FVector2f(-6.f, 0.f), P + FVector2f(0.f, -6.f) },
				Fx, Col, true, 2.f);
		}
		else
		{
			FSlateDrawElement::MakeBox(Out, Layer, Allotted.ToPaintGeometry(FVector2f(10.f), FSlateLayoutTransform(FVector2f(X0 + 1.f, Y + 2.f))), &MarkerFillBrush, Fx, Col);
		}
		FSlateDrawElement::MakeText(Out, Layer, Allotted.ToPaintGeometry(FVector2f(W * 0.4f, 14.f), FSlateLayoutTransform(FVector2f(X0 + 20.f, Y))),
			Name, Font, Fx, Col);
		FSlateDrawElement::MakeText(Out, Layer, Allotted.ToPaintGeometry(FVector2f(W * 0.3f, 14.f), FSlateLayoutTransform(FVector2f(X0 + W * 0.42f, Y + 1.f))),
			DistSun, Small, Fx, ValueCol);
		FSlateDrawElement::MakeText(Out, Layer, Allotted.ToPaintGeometry(FVector2f(W * 0.3f, 14.f), FSlateLayoutTransform(FVector2f(X0 + W * 0.70f, Y + 1.f))),
			DistShip, Small, Fx, ValueCol);
		Y += 24.f;
	};

	if (Grav)
	{
		Row(ThrusterActiveColor, TEXT("HYPÉRION"), FormatMapDistance(Grav->RealPosition.Size()), TEXT("—"), true);
		Y += 6.f;
	}
	for (int32 i = 0; i < S->Bodies.Num(); ++i)
	{
		const FSolarBody& B = S->Bodies[i];
		const FBodyInfo* Info = FindBodyInfo(B.Name);
		const FVector P = S->BodyRealPosition(i, Now);
		const FString DistSun = B.Model == ESolarOrbitModel::Fixed ? FString(TEXT("—")) : FormatMapDistance(P.Size());
		const FString DistShip = Grav ? FormatMapDistance((P - Grav->RealPosition).Size()) : FString(TEXT("—"));
		Row(Info ? Info->Color : AccentColor, Info ? Info->Label : B.Name.ToString().ToUpper(), DistSun, DistShip, false);
	}
}
