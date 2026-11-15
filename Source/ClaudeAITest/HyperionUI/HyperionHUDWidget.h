// HUD futuriste du vaisseau Hyperion.
//  - en haut : 6 boutons (libelles reglables, clic -> OnHudButtonClicked(Index)) ;
//  - en bas a droite : puissance des 2 moteurs principaux et des 6 auxiliaires ;
//  - en bas a gauche : schema du vaisseau, propulseurs de manoeuvre allumes quand ils sont actifs.
// L'arbre de widgets est construit en C++ : aucun asset UMG a maintenir.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "HyperionHUDWidget.generated.h"

class UButton;
class UTextBlock;
class UProgressBar;
class UBorder;
class UHorizontalBox;
class UVerticalBox;
class AHyperionShip;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FHyperionHudButtonSignature, int32, ButtonIndex);

UCLASS(meta = (DisplayName = "Hyperion HUD Widget"))
class CLAUDEAITEST_API UHyperionHUDWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UHyperionHUDWidget(const FObjectInitializer& ObjectInitializer);

	static constexpr int32 NumButtons = 6;
	static constexpr int32 NumMainEngines = 2;
	static constexpr int32 NumAuxEngines = 6;

	/** Libelles des 6 boutons du haut (de gauche a droite). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD")
	TArray<FText> ButtonLabels;

	/** Couleur d'accent (lignes, textes, jauges). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Style")
	FLinearColor AccentColor = FLinearColor(0.25f, 0.88f, 1.0f, 1.0f);

	/** Fond des panneaux (translucide). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Style")
	FLinearColor PanelColor = FLinearColor(0.01f, 0.035f, 0.06f, 0.72f);

	/** Couleur des jauges au-dela de HighThrustThreshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Style")
	FLinearColor HighThrustColor = FLinearColor(1.0f, 0.55f, 0.15f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Style", meta = (ClampMin = "0", ClampMax = "1"))
	float HighThrustThreshold = 0.8f;

	/** Couleur d'un propulseur de manoeuvre allume sur le schema. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Style")
	FLinearColor ThrusterActiveColor = FLinearColor(1.0f, 0.62f, 0.2f, 1.0f);

	/** Allume tous les propulseurs du schema (verification de l'affichage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Debug")
	bool bPreviewAllThrusters = false;

	/** Clic sur un bouton du haut (Index de 0 a 5). */
	UPROPERTY(BlueprintAssignable, Category = "Hyperion HUD")
	FHyperionHudButtonSignature OnHudButtonClicked;

	/** Change le libelle d'un bouton en cours de jeu. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion HUD")
	void SetButtonLabel(int32 Index, FText Label);

	/** Allume un bouton (fonction en cours, ex. KILL ROT) : fond et libelle a la couleur ActiveButtonColor. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion HUD")
	void SetButtonActive(int32 Index, bool bActive, bool bDone = false);

	/** Couleur d'un bouton dont la fonction est en cours. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Style")
	FLinearColor ActiveButtonColor = FLinearColor(1.0f, 0.62f, 0.2f, 1.0f);

	/** Couleur d'un bouton dont la manoeuvre est accomplie (PRO, RET, ORBIT+, ORBIT- alignes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Style")
	FLinearColor DoneButtonColor = FLinearColor(0.25f, 0.95f, 0.45f, 1.0f);

	// ------------------------------------------------------------------ Carte (F1)
	/**
	 * Vue carte : les 2/3 gauches montrent le systeme solaire vu du dessus de l'ecliptique (astres, orbites,
	 * Hyperion, encart Terre-Lune), le tiers droit la liste des astres. Les autres panneaux sont masques.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hyperion HUD|Carte")
	void ToggleMapView();

	UFUNCTION(BlueprintCallable, Category = "Hyperion HUD|Carte")
	void SetMapViewVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "Hyperion HUD|Carte")
	bool IsMapViewVisible() const { return bMapVisible; }

	/** Intervalle (s reelles) entre deux predictions des apsides du panneau ORBITE (propagation complete). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Orbite", meta = (ClampMin = "0.05"))
	float ApsisPredictionInterval = 0.5f;

	/** Valeurs affichees T-AP / T-PE (s simulees, -1 si inconnues), pour verification. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion HUD|Orbite")
	double ShownTimeToAp = -1.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion HUD|Orbite")
	double ShownTimeToPe = -1.0;

	/** Memes temps selon les seuls elements kepleriens (attraction de l'astre de reference seule). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion HUD|Orbite")
	double KeplerTimeToAp = -1.0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion HUD|Orbite")
	double KeplerTimeToPe = -1.0;

	/** Distance au Soleil (UA) du bord de la carte. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Carte", meta = (ClampMin = "1"))
	double MapOuterAU = 32.0;

	/** Echelle radiale logarithmique : distance (UA) en dessous de laquelle l'echelle devient presque lineaire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Carte", meta = (ClampMin = "0.01"))
	double MapLogKneeAU = 0.25;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void BuildTree();
	UButton* MakeButton(int32 Index, UHorizontalBox* Bar);
	UButton* MakeTimeButton(int32 Index, UHorizontalBox* Bar);
	void SetTimeWarp(int32 Index);
	void UpdateTimeButtons();
	void MakeEngineRow(UVerticalBox* Box, const FString& Label, bool bMain);
	void MakeOrbitRow(UVerticalBox* Box, const FString& Label, TArray<TObjectPtr<UTextBlock>>* Into = nullptr);
	void UpdateOrbitPanel();
	/** Propage la trajectoire (gravite de tous les astres) pour trouver les prochains apoapside / periapside. */
	void UpdateApsisPrediction(const class ASolarSystemSimulator* S, const class USolarGravityComponent* G, int32 RefIdx, double OscPeriod);
	struct FApsisPrediction
	{
		bool bApFound = false;
		bool bPeFound = false;
		double ApTime = 0.0;      // temps simule absolu
		double PeTime = 0.0;
		double ApDistance = 0.0;  // distance au centre de l'astre (m)
		double PeDistance = 0.0;
		double ComputedRealTime = -1.0;
		int32 RefIdx = INDEX_NONE;
	};
	FApsisPrediction ApsisPrediction;
	void UpdateRotationPanel();
	UTextBlock* MakeText(const FString& Text, int32 Size, const FLinearColor& Color, bool bBold = true) const;
	AHyperionShip* FindShip() const;
	void PaintBrackets(const FGeometry& Allotted, const UWidget* Target, FSlateWindowElementList& Out, int32 Layer) const;
	void PaintSchematic(const FGeometry& Allotted, FSlateWindowElementList& Out, int32 Layer) const;
	bool IsThrusterLit(const AHyperionShip* Ship, const TCHAR* Name) const;
	void BuildMapView(class UCanvasPanel* Root);
	void PaintMap(const FGeometry& Allotted, FSlateWindowElementList& Out, int32 Layer) const;
	void PaintMapSide(const FGeometry& Allotted, FSlateWindowElementList& Out, int32 Layer) const;
	void RefreshOrbitCache(const class ASolarSystemSimulator* S) const;

	UFUNCTION() void HandleButton0();
	UFUNCTION() void HandleButton1();
	UFUNCTION() void HandleButton2();
	UFUNCTION() void HandleButton3();
	UFUNCTION() void HandleButton4();
	UFUNCTION() void HandleButton5();
	UFUNCTION() void HandleTime0();
	UFUNCTION() void HandleTime1();
	UFUNCTION() void HandleTime2();
	UFUNCTION() void HandleTime3();

	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> TopBar = nullptr;
	UPROPERTY(Transient) TObjectPtr<UHorizontalBox> TimeBar = nullptr;
	UPROPERTY(Transient) TObjectPtr<UWidget> TopArea = nullptr;  // boutons de pilotage + acceleration du temps
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> TimeButtons;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> TimeButtonTexts;
	TArray<bool> TimeButtonActive;
	UPROPERTY(Transient) TObjectPtr<UBorder> EnginePanel = nullptr;
	UPROPERTY(Transient) TObjectPtr<UBorder> ManeuverPanel = nullptr;
	UPROPERTY(Transient) TObjectPtr<UBorder> OrbitPanel = nullptr;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> OrbitRefText = nullptr;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> OrbitValues;  // AP, PE, ECC, T-AP, T-PE, RInc
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> OrbitAltValues;  // Alt (premiere ligne)
	UPROPERTY(Transient) TObjectPtr<UBorder> RotationPanel = nullptr;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> RotationValues;  // tangage, lacet, roulis
	UPROPERTY(Transient) TObjectPtr<UWidget> SchematicArea = nullptr;  // zone reservee au dessin du vaisseau

	FSlateBrush MarkerFillBrush;
	FSlateBrush MarkerOutlineBrush;
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> ButtonTexts;
	UPROPERTY(Transient) TArray<TObjectPtr<UButton>> Buttons;
	TArray<uint8> ButtonActive;  // 0 eteint, 1 actif, 2 manoeuvre accomplie
	FButtonStyle MakeButtonStyle(bool bActive, bool bDone = false) const;
	UPROPERTY(Transient) TArray<TObjectPtr<UProgressBar>> EngineBars;     // 2 principaux puis 6 auxiliaires
	UPROPERTY(Transient) TArray<TObjectPtr<UTextBlock>> EngineValues;
	TArray<bool> EngineIsMain;

	mutable TWeakObjectPtr<AHyperionShip> CachedShip;

	// Carte
	bool bMapVisible = false;
	UPROPERTY(Transient) TObjectPtr<UWidget> MapLayer = nullptr;
	UPROPERTY(Transient) TObjectPtr<UBorder> MapPanel = nullptr;
	UPROPERTY(Transient) TObjectPtr<UBorder> MapSidePanel = nullptr;
	UPROPERTY(Transient) TObjectPtr<UWidget> MapArea = nullptr;      // zone de dessin de la carte
	UPROPERTY(Transient) TObjectPtr<UWidget> MapSideArea = nullptr;  // zone de dessin de la liste
	UPROPERTY(Transient) TObjectPtr<UTextBlock> MapDateText = nullptr;
	mutable TArray<TArray<FVector2D>> OrbitCache;  // trajectoires heliocentriques (m, plan XY) par astre
	mutable TArray<FVector2D> MoonOrbitCache;      // orbite de la Lune autour de la Terre (m)
	mutable double OrbitCacheTime = -1.0e30;
};
