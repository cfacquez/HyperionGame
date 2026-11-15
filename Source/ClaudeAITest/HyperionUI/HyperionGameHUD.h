// HUD du jeu : affiche le widget futuriste de l'Hyperion et transmet les clics des boutons au Blueprint.
// BP_SolarHUD en herite (il garde son dessin des noms d'astres dans Event Receive Draw HUD).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "InputCoreTypes.h"
#include "HyperionGameHUD.generated.h"

class UHyperionHUDWidget;

UCLASS()
class CLAUDEAITEST_API AHyperionGameHUD : public AHUD
{
	GENERATED_BODY()

public:
	AHyperionGameHUD();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	/** Carte ouverte : les noms d'astres dessines sur la vue 3D (Blueprint) sont masques. */
	virtual void DrawHUD() override;

	/** Action (Project Settings > Input > Action Mappings) d'ouverture / fermeture de la carte. Defaut : F1. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Carte")
	FName MapToggleAction = TEXT("Hyperion_ToggleMap");

	/** Index du bouton KILL ROT (annulation des rotations par les propulseurs de manoeuvre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Fonctions")
	int32 KillRotButtonIndex = 4;

	/** Index du bouton PRO (autopilote : nez dans le sens de la vitesse orbitale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Fonctions")
	int32 ProButtonIndex = 0;

	/** Index du bouton RET (autopilote : nez a l'oppose de la vitesse orbitale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Fonctions")
	int32 RetButtonIndex = 1;

	/** Index du bouton ORBIT+ (autopilote : nez selon la normale a l'orbite). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Fonctions")
	int32 OrbitNormalButtonIndex = 2;

	/** Index du bouton ORBIT- (autopilote : nez selon l'anti-normale a l'orbite). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Fonctions")
	int32 OrbitAntiNormalButtonIndex = 3;

	/** Classe du widget (une sous-classe Blueprint est possible pour changer le style). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hyperion HUD")
	TSubclassOf<UHyperionHUDWidget> WidgetClass;

	/** Libelles des 6 boutons du haut, de gauche a droite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD")
	TArray<FText> ButtonLabels;

	/** Affiche le curseur pour pouvoir cliquer les boutons (la camera reste pilotable au clic droit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD")
	bool bShowCursor = true;

	/** Allume tous les propulseurs du schema de manoeuvre (verification de l'affichage). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion HUD|Debug")
	bool bPreviewAllThrusters = false;

	UPROPERTY(BlueprintReadOnly, Transient, Category = "Hyperion HUD")
	TObjectPtr<UHyperionHUDWidget> HudWidget = nullptr;

	/** Appele quand un bouton du haut est clique (Index de 0 a 5). A implementer dans BP_SolarHUD. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Hyperion HUD")
	void OnHudButtonClicked(int32 ButtonIndex);

private:
	class UHyperionRCSComponent* FindRCS() const;

	UFUNCTION()
	void HandleButtonClicked(int32 ButtonIndex);
};
