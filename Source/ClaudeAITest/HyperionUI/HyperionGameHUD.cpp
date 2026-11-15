#include "HyperionGameHUD.h"

#include "Blueprint/UserWidget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HyperionHUDWidget.h"
#include "HyperionRCSComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerInput.h"

AHyperionGameHUD::AHyperionGameHUD()
{
	PrimaryActorTick.bCanEverTick = true;
	WidgetClass = UHyperionHUDWidget::StaticClass();
	for (int32 i = 0; i < UHyperionHUDWidget::NumButtons; ++i)
	{
		ButtonLabels.Add(FText::FromString(FString::Printf(TEXT("BOUTON %d"), i + 1)));
	}
}

void AHyperionGameHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (!PC || !WidgetClass)
	{
		return;
	}

	HudWidget = CreateWidget<UHyperionHUDWidget>(PC, WidgetClass);
	if (!HudWidget)
	{
		return;
	}
	HudWidget->ButtonLabels = ButtonLabels;
	HudWidget->bPreviewAllThrusters = bPreviewAllThrusters;
	HudWidget->OnHudButtonClicked.AddDynamic(this, &AHyperionGameHUD::HandleButtonClicked);
	HudWidget->AddToViewport(10);

	if (bShowCursor)
	{
		// Curseur visible pour les boutons ; un clic dans la vue capture la souris (clic droit = camera).
		PC->SetShowMouseCursor(true);
		FInputModeGameAndUI Mode;
		Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		Mode.SetHideCursorDuringCapture(true);
		PC->SetInputMode(Mode);

		// "Jeu + interface" ne capture la souris que pendant un clic gauche : sans capture, aucun
		// deplacement de souris n'est transmis au jeu et la camera orbitale (clic droit) ne bouge plus.
		// On capture donc pendant le clic droit ; le curseur reapparait a sa place au relachement.
		if (UGameViewportClient* Viewport = GetWorld() ? GetWorld()->GetGameViewport() : nullptr)
		{
			Viewport->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringRightMouseDown);
		}
	}
}

UHyperionRCSComponent* AHyperionGameHUD::FindRCS() const
{
	const APlayerController* PC = GetOwningPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	return Pawn ? Pawn->FindComponentByClass<UHyperionRCSComponent>() : nullptr;
}

void AHyperionGameHUD::DrawHUD()
{
	if (HudWidget && HudWidget->IsMapViewVisible())
	{
		return;
	}
	Super::DrawHUD();
}

void AHyperionGameHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (HudWidget)
	{
		// F1 : ouvre / ferme la carte.
		if (const APlayerController* PC = GetOwningPlayerController())
		{
			// Action mapping : l'une de ses touches vient d'etre enfoncee.
			bool bToggle = false;
			if (PC->PlayerInput && !MapToggleAction.IsNone())
			{
				for (const FInputActionKeyMapping& Mapping : PC->PlayerInput->GetKeysForAction(MapToggleAction))
				{
					bToggle |= PC->WasInputKeyJustPressed(Mapping.Key);
				}
			}
			if (bToggle)
			{
				HudWidget->ToggleMapView();
			}
		}
		// Le bouton reste allume tant que l'annulation de rotation est en cours.
		const UHyperionRCSComponent* RCS = FindRCS();
		HudWidget->SetButtonActive(KillRotButtonIndex, RCS && RCS->IsKillRotationActive());
		// Orange pendant la manoeuvre, vert une fois le vaisseau aligne.
		const bool bAligned = RCS && RCS->IsOrientationAligned();
		HudWidget->SetButtonActive(ProButtonIndex, RCS && RCS->IsProgradeActive(), bAligned);
		HudWidget->SetButtonActive(RetButtonIndex, RCS && RCS->IsRetrogradeActive(), bAligned);
		HudWidget->SetButtonActive(OrbitNormalButtonIndex, RCS && RCS->IsOrbitNormalActive(), bAligned);
		HudWidget->SetButtonActive(OrbitAntiNormalButtonIndex, RCS && RCS->IsOrbitAntiNormalActive(), bAligned);
	}
}

void AHyperionGameHUD::HandleButtonClicked(int32 ButtonIndex)
{
	if (UHyperionRCSComponent* RCS = FindRCS())
	{
		if (ButtonIndex == KillRotButtonIndex)
		{
			RCS->ToggleKillRotation();  // coupe aussi PRO
		}
		// PRO / RET / ORBIT+ / ORBIT- : un clic active la fonction (et remplace celle qui etait active) ;
		// un clic sur le bouton deja actif la desactive.
		else if (ButtonIndex == ProButtonIndex)
		{
			RCS->SetPrograde(!RCS->IsProgradeActive());
		}
		else if (ButtonIndex == RetButtonIndex)
		{
			RCS->SetRetrograde(!RCS->IsRetrogradeActive());
		}
		else if (ButtonIndex == OrbitNormalButtonIndex)
		{
			RCS->SetOrbitNormal(!RCS->IsOrbitNormalActive());
		}
		else if (ButtonIndex == OrbitAntiNormalButtonIndex)
		{
			RCS->SetOrbitAntiNormal(!RCS->IsOrbitAntiNormalActive());
		}
		else
		{
			RCS->CancelAutopilot();  // tout autre bouton est une autre action
		}
	}
	OnHudButtonClicked(ButtonIndex);
}
