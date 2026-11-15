// Hyperion (classe Atlas) - acteur du vaisseau.
//
// Principe : la maquette Hyperion_Blockout.glb est importee dans le niveau (Interchange), puis
// convertie en Blueprint (Harvest Components) dont la classe parente est AHyperionShip.
// Au BeginPlay, l'acteur retrouve les pieces par leur nom (SM_Anneau, WPN_Heavy_01_Lacet, ...)
// et les anime. Aucune piece n'est creee en C++ : la geometrie reste celle du fichier importe.
//
// Fichier volontairement en ASCII (pas d'accents).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "HyperionShipTypes.h"
#include "HyperionShipActor.generated.h"

class UNiagaraSystem;
class UPointLightComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FHyperionWeaponFiredSignature, EHyperionWeapon, Weapon, FTransform, Muzzle);

UCLASS(Blueprintable, meta = (DisplayName = "Hyperion Ship"))
class HYPERIONSHIP_API AHyperionShip : public AActor
{
	GENERATED_BODY()

public:
	AHyperionShip();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------ Anneau gravitationnel
	/** Vitesse reelle : 1 g a 300 m de rayon -> 0,1808 rad/s = 10,36 deg/s (1,7 tr/min). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Anneau")
	float RingSpeedDegPerSec = 10.36f;

	/** Multiplicateur pour la mise en scene (1 = vitesse reelle, 0 = arret). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Anneau", meta = (ClampMin = "0"))
	float RingTimeScale = 1.f;

	// ------------------------------------------------------------------ Propulsion
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hyperion|Propulsion")
	TObjectPtr<UNiagaraSystem> MainExhaustFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hyperion|Propulsion")
	TObjectPtr<UNiagaraSystem> AuxExhaustFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hyperion|Propulsion")
	TObjectPtr<UNiagaraSystem> RCSFX = nullptr;

	/** Parametre float envoye aux systemes Niagara (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hyperion|Propulsion")
	FName NiagaraThrustParam = TEXT("User.Thrust");

	/** Parametre vectoriel d'emissif des materiaux "M_Propulsion" importes du glTF. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hyperion|Propulsion")
	FName GlowMaterialParam = TEXT("EmissiveFactor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Propulsion")
	FLinearColor EngineGlowColor = FLinearColor(0.37f, 0.83f, 0.93f);

	/** Intensite emissive a plein regime. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Propulsion", meta = (ClampMin = "0"))
	float GlowAtFullThrottle = 20.f;

	/** Poussee courante (0..1), lissee dans Tick. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|Propulsion")
	float MainThrottle = 0.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|Propulsion")
	float AuxThrottle = 0.f;

	/** Vitesse de variation de la poussee (unites/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Propulsion", meta = (ClampMin = "0.01"))
	float ThrottleResponse = 0.5f;

	// ------------------------------------------------------------------ Armement
	/** Projectile a faire apparaitre pour chaque groupe (laisser vide = evenement OnWeaponFired seulement). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Armement")
	TMap<EHyperionWeapon, TSubclassOf<AActor>> ProjectileClasses;

	/** Cadence : intervalle minimal entre deux salves d'un meme groupe (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Armement")
	TMap<EHyperionWeapon, float> FireIntervals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Armement")
	FHyperionTurretLimits HeavyLimits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Armement")
	FHyperionTurretLimits MainLimits;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Armement")
	FHyperionTurretLimits LaserLimits;

	/** Ecart angulaire maximal (deg) pour qu'une tourelle soit consideree "en joue". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Armement")
	float AimTolerance = 2.f;

	UPROPERTY(BlueprintAssignable, Category = "Hyperion|Armement")
	FHyperionWeaponFiredSignature OnWeaponFired;

	// ------------------------------------------------------------------ Antenne parabolique
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Antenne")
	bool bDishSweep = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Antenne")
	float DishTurnRate = 6.f;

	// ------------------------------------------------------------------ Hangars
	/** Course d'ouverture des portes de hangar (glissement vers le haut), en cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Hangars")
	float DoorSlideCm = 2600.f;

	/** Vitesse d'ouverture (fraction de course par seconde). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Hangars")
	float DoorSpeed = 0.2f;

	// ------------------------------------------------------------------ Balise
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Balise")
	bool bBeacon = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Balise")
	FLinearColor BeaconColor = FLinearColor(1.f, 0.15f, 0.1f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Balise")
	float BeaconPeriod = 1.5f;

	// ------------------------------------------------------------------ API Blueprint
	/** Consigne de poussee (0..1) des moteurs principaux et auxiliaires. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Propulsion")
	void SetThrottle(float Main, float Aux);

	/**
	 * Consigne de poussee (0..1) d'un seul moteur. Index : 0-1 principaux (P1 gauche, P2 droit),
	 * 2-7 auxiliaires A1 a A6 (SM_Tuyere_Aux_01 a 06).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Propulsion")
	void SetEngineThrottle(int32 EngineIndex, float Throttle);

	/** Poussee courante (0..1, lissee) d'un moteur. */
	UFUNCTION(BlueprintPure, Category = "Hyperion|Propulsion")
	float GetEngineThrottle(int32 EngineIndex) const;

	UFUNCTION(BlueprintPure, Category = "Hyperion|Propulsion")
	int32 GetEngineCount() const { return Engines.Num(); }

	UFUNCTION(BlueprintPure, Category = "Hyperion|Propulsion")
	bool IsMainEngine(int32 EngineIndex) const { return Engines.IsValidIndex(EngineIndex) && Engines[EngineIndex].bMain; }

	/** Tuyere d'un moteur (point d'application de sa poussee). */
	UFUNCTION(BlueprintPure, Category = "Hyperion|Propulsion")
	USceneComponent* GetEngineNozzle(int32 EngineIndex) const;

	/** Allume un propulseur de manoeuvre (nom de la piece, ex. "SM_Propulseur_Manoeuvre_H"). */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Propulsion")
	bool FireRCS(FName ThrusterName, float Duration = 0.5f);

	/** Vrai si le propulseur de manoeuvre (nom de la piece) est allume en ce moment. */
	UFUNCTION(BlueprintPure, Category = "Hyperion|Propulsion")
	bool IsThrusterFiring(FName ThrusterName) const;

	/** Allume la combinaison de propulseurs avant/arriere correspondant a la manoeuvre. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Propulsion")
	void PerformManeuver(EHyperionManeuver Maneuver, float Duration = 0.5f);

	/** Oriente toutes les tourelles (lourdes, principale, lasers) vers un point du monde. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Armement")
	void AimAllTurrets(FVector WorldTarget);

	/** Oriente un seul groupe de tourelles. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Armement")
	void AimGroup(EHyperionWeapon Group, FVector WorldTarget);

	/** Remet toutes les tourelles en position de repos. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Armement")
	void StowTurrets();

	/**
	 * Tire avec un groupe d'armes. Seules les tourelles en joue et dans leur secteur tirent.
	 * Retourne le nombre de departs.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Armement")
	int32 FireWeapon(EHyperionWeapon Group, FVector WorldTarget);

	UFUNCTION(BlueprintCallable, Category = "Hyperion|Hangars")
	void SetHangarDoorsOpen(bool bOpen);

	/** L'antenne suit un point du monde (desactive le balayage). */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Antenne")
	void SetDishTarget(FVector WorldTarget);

	/** Reprend le balayage automatique de l'antenne. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Antenne")
	void ClearDishTarget();

	/** Axe longitudinal du vaisseau (vers la proue), en coordonnees monde. */
	UFUNCTION(BlueprintPure, Category = "Hyperion")
	FVector GetShipForward() const;

	/** Retrouve une piece de la maquette par son nom (ex. "SM_S3_Commandement"). */
	UFUNCTION(BlueprintPure, Category = "Hyperion")
	USceneComponent* FindPart(FName PartName) const;

	/** Nombre de pieces attendues qui n'ont pas ete trouvees au demarrage (0 = assemblage complet). */
	UFUNCTION(BlueprintPure, Category = "Hyperion")
	int32 GetMissingPartCount() const { return MissingParts.Num(); }

private:
	void CollectParts();
	USceneComponent* Require(const FString& Name);
	void InitAxis();
	void InitRing();
	void InitTurrets();
	void InitEngines();
	void InitThrusters();
	void InitDishAndDoors();
	void InitBeacon();

	void TickRing(float Dt);
	void TickTurrets(float Dt);
	void TickEngines(float Dt);
	void TickThrusters(float Dt);
	void TickDish(float Dt);
	void TickDoors(float Dt);
	void TickBeacon(float Dt);

	void AimTurret(FHyperionTurret& T, const FVector& WorldTarget) const;
	const FHyperionTurretLimits& LimitsFor(EHyperionWeapon Type) const;
	void GetMuzzleTransforms(const FHyperionTurret& T, TArray<FTransform>& Out) const;
	void SpawnShot(EHyperionWeapon Group, const FTransform& Muzzle);
	void CollectGlowMaterials(UStaticMeshComponent* Mesh, TArray<TObjectPtr<UMaterialInstanceDynamic>>& Out) const;
	void SetGlow(const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Mats, float Alpha) const;

	UPROPERTY(Transient) TMap<FString, TObjectPtr<USceneComponent>> PartsByName;
	UPROPERTY(Transient) TArray<FString> MissingParts;

	UPROPERTY(Transient) TObjectPtr<USceneComponent> RingPivot = nullptr;
	FQuat RingBase = FQuat::Identity;
	FVector RingAxisInParent = FVector::ForwardVector;
	float RingAngleDeg = 0.f;

	FVector ShipForwardLocal = FVector::ForwardVector;

	UPROPERTY(Transient) TArray<FHyperionTurret> Turrets;
	UPROPERTY(Transient) TArray<FHyperionEngine> Engines;
	UPROPERTY(Transient) TMap<FName, FHyperionThruster> Thrusters;

	float TargetMainThrottle = 0.f;
	float TargetAuxThrottle = 0.f;

	TMap<EHyperionWeapon, float> NextFireTime;

	UPROPERTY(Transient) TObjectPtr<USceneComponent> Dish = nullptr;
	FQuat DishBase = FQuat::Identity;
	float DishYaw = 0.f;
	float DishRestYaw = 0.f;
	bool bDishHasTarget = false;
	FVector DishTarget = FVector::ZeroVector;

	UPROPERTY(Transient) TArray<TObjectPtr<USceneComponent>> Doors;
	TArray<FVector> DoorBase;
	float DoorAlpha = 0.f;
	float DoorTarget = 0.f;

	UPROPERTY(Transient) TObjectPtr<UPointLightComponent> BeaconLight = nullptr;
	float BeaconClock = 0.f;
};
