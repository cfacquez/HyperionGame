// Camera exterieure de l'Hyperion (toutes les commandes passent par des Action Mappings) :
//  - orbite : clic droit maintenu + souris, fait tourner le pivot de la camera autour du vaisseau ;
//  - molette : rapproche / eloigne la camera (le long de son decalage par rapport au pivot, meme angle de vue) ;
//  - clic du bouton du milieu : retour au zoom d'origine.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HyperionCameraZoomComponent.generated.h"

class USceneComponent;

UCLASS(ClassGroup = (Hyperion), meta = (BlueprintSpawnableComponent, DisplayName = "Hyperion Camera Zoom"))
class CLAUDEAITEST_API UHyperionCameraZoomComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHyperionCameraZoomComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Nom du composant camera a deplacer (dans le pion). Vide : la premiere camera trouvee. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera")
	FName CameraComponentName = TEXT("RearCamera");

	/** Actions (Project Settings > Input > Action Mappings). Defauts : molette avant / arriere, bouton du milieu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera")
	FName ZoomInAction = TEXT("Hyperion_ZoomIn");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera")
	FName ZoomOutAction = TEXT("Hyperion_ZoomOut");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera")
	FName ZoomResetAction = TEXT("Hyperion_ZoomReset");

	/** Facteur de distance par cran de molette (1.15 = 15 % plus loin / plus pres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera", meta = (ClampMin = "1.01"))
	float ZoomStepFactor = 1.15f;

	/** Distance minimale, en fraction de la distance d'origine (plus pres = zoom avant). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera", meta = (ClampMin = "0.05"))
	float MinZoom = 0.4f;

	/** Distance maximale, en multiple de la distance d'origine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera", meta = (ClampMin = "1"))
	float MaxZoom = 20.f;

	/** Vitesse de lissage du zoom (plus grand = plus vif). 0 = instantane. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera", meta = (ClampMin = "0"))
	float ZoomInterpSpeed = 10.f;

	/** Zoom vise (1 = distance d'origine). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|Camera")
	float TargetZoom = 1.f;

	/** Zoom applique (lisse). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|Camera")
	float CurrentZoom = 1.f;

	// ------------------------------------------------------------------ Orbite autour du vaisseau
	/** Action (Action Mappings) a maintenir pour faire tourner la camera avec la souris. Defaut : clic droit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera|Orbite")
	FName OrbitAction = TEXT("Hyperion_CameraOrbit");

	/** Composant pivot que l'on fait tourner (la camera lui est attachee). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera|Orbite")
	FName PivotComponentName = TEXT("CameraPivot");

	/**
	 * Faux : la camera garde son orientation dans l'espace (seule la souris la fait tourner) et le vaisseau
	 * tourne devant elle, on voit ses rotations sur les 3 axes. Vrai : camera accrochee a l'arriere du vaisseau.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera|Orbite")
	bool bCameraFollowsShipRotation = false;

	/** Degres par unite de deplacement de la souris. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera|Orbite", meta = (ClampMin = "0"))
	float OrbitSensitivity = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera|Orbite")
	float MinCameraPitch = -80.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera|Orbite")
	float MaxCameraPitch = 80.f;

	/** Inverse le sens vertical de l'orbite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Camera|Orbite")
	bool bInvertOrbitPitch = false;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|Camera|Orbite")
	float CameraYaw = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|Camera|Orbite")
	float CameraPitch = 0.f;

	/** Zoom par programme : crans > 0 eloignent, < 0 rapprochent. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Camera")
	void ZoomSteps(float Steps);

	/** Retour au zoom d'origine. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Camera")
	void ResetZoom();

private:
	TWeakObjectPtr<USceneComponent> Camera;
	TWeakObjectPtr<USceneComponent> Pivot;
	bool bInitialFrameDone = false;
	float InitElapsed = 0.f;
	FVector DefaultOffset = FVector::ZeroVector;
};
