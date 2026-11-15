// Gravite appliquee a un acteur libre (vaisseau) : somme des attractions de tous les astres du
// ASolarSystemSimulator, integree en "leapfrog" (symplectique, stable sur de longues orbites).
// Position et vitesse sont tenues dans le repere reel (metres, ecliptique J2000) puis converties en
// coordonnees monde a chaque image ; l'acteur ne doit donc etre attache a rien.
//
// Fichier volontairement en ASCII (pas d'accents).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SolarGravityComponent.generated.h"

class ASolarSystemSimulator;

UCLASS(ClassGroup = (Solar), meta = (BlueprintSpawnableComponent, DisplayName = "Solar Gravity"))
class SOLARMECHANICS_API USolarGravityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USolarGravityComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------ Reglages
	/** Masse du vaisseau (kg). Sans effet sur la chute libre ; sert a convertir les forces de poussee en acceleration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Gravite", meta = (ClampMin = "1"))
	double MassKg = 2.0e9;

	/** Astre de reference pour le placement de depart (nom dans ASolarSystemSimulator::Bodies). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Depart")
	FName ReferenceBody = TEXT("Earth");

	/** Place l'acteur au depart a StartDistanceKm du centre de l'astre de reference, cote Soleil. Sinon garde sa position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Depart")
	bool bPlaceAtStart = true;

	/** Distance reelle au centre de l'astre de reference au depart (km). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Depart", meta = (ClampMin = "0"))
	double StartDistanceKm = 25000.0;

	/** Donne au depart la vitesse d'une orbite circulaire autour de l'astre de reference (sinon immobile par rapport a lui). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Depart")
	bool bStartInCircularOrbit = true;

	/** Oriente l'acteur vers l'astre de reference au depart (axe +X). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Depart")
	bool bFaceReferenceBodyAtStart = true;

	/** Pas d'integration maximal en temps simule (s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Gravite", meta = (ClampMin = "0.01"))
	double MaxSubstepSeconds = 5.0;

	// ------------------------------------------------------------------ Rotation (corps rigide)
	/** Integre la rotation du vaisseau (couples des propulseurs). Sinon l'orientation n'est pas touchee. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Rotation")
	bool bSimulateRotation = true;

	/**
	 * Moments d'inertie principaux (kg.m2) autour des axes locaux X (roulis), Y (tangage), Z (lacet).
	 * Defaut : Hyperion, 2e9 kg, coque de 1 254 m (rayon ~60 m, 80 % de la masse) et anneau de 300 m (20 %).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Rotation")
	FVector InertiaKgM2 = FVector(3.9e13, 2.3e14, 2.3e14);

	/** Vitesse de rotation (rad/s), axes en coordonnees monde Unreal. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solar|Rotation")
	FVector AngularVelocity = FVector::ZeroVector;

	// ------------------------------------------------------------------ Etat (repere reel)
	/** Vitesse heliocentrique (m/s, repere reel). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solar|Etat")
	FVector RealVelocity = FVector::ZeroVector;

	/** Position heliocentrique (m, repere reel). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Solar|Etat")
	FVector RealPosition = FVector::ZeroVector;

	/**
	 * Une poussee ramene le temps a ThrustTimeScale (comportement par defaut). A false, la poussee agit sans
	 * ralentir le temps (petites corrections d'un autopilote) : elle dure alors une image de jeu en temps simule.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Gravite")
	bool bThrustSlowsTime = true;

	// ------------------------------------------------------------------ API
	/** Ajoute une force (N) pour l'image en cours ; direction en coordonnees monde Unreal. */
	UFUNCTION(BlueprintCallable, Category = "Solar|Gravite")
	void AddForce(FVector ForceNewtonsWorld);

	/** Ajoute une acceleration (m/s2) pour l'image en cours ; direction en coordonnees monde Unreal. */
	UFUNCTION(BlueprintCallable, Category = "Solar|Gravite")
	void AddAcceleration(FVector AccelerationWorld);

	/**
	 * Ajoute une force (N) appliquee en un point du monde pour l'image en cours :
	 * fait avancer le vaisseau et, si le point est decale du centre, le fait tourner.
	 */
	UFUNCTION(BlueprintCallable, Category = "Solar|Rotation")
	void AddForceAtLocation(FVector ForceNewtonsWorld, FVector WorldLocation);

	/** Ajoute un couple (N.m), axe en coordonnees monde Unreal, pour l'image en cours. */
	UFUNCTION(BlueprintCallable, Category = "Solar|Rotation")
	void AddTorque(FVector TorqueWorld);

	/** Vitesse de rotation en degres par seconde (temps reel simule), axes monde Unreal. */
	UFUNCTION(BlueprintPure, Category = "Solar|Rotation")
	FVector GetAngularVelocityDeg() const { return AngularVelocity * (180.0 / PI); }

	/** Annule immediatement la rotation (pour les tests ; l'autopilote utilisera les propulseurs). */
	UFUNCTION(BlueprintCallable, Category = "Solar|Rotation")
	void ResetAngularVelocity() { AngularVelocity = FVector::ZeroVector; }

	/** Vitesse relative a un astre (m/s), direction en coordonnees monde Unreal. */
	UFUNCTION(BlueprintPure, Category = "Solar|Gravite")
	FVector GetVelocityRelativeTo(FName BodyName) const;

	/** Distance reelle au centre d'un astre (km). */
	UFUNCTION(BlueprintPure, Category = "Solar|Gravite")
	double GetDistanceToBodyKm(FName BodyName) const;

	/** Acceleration gravitationnelle actuelle (m/s2), direction en coordonnees monde Unreal. */
	UFUNCTION(BlueprintPure, Category = "Solar|Gravite")
	FVector GetGravityAcceleration() const;

private:
	ASolarSystemSimulator* Sim() const;
	void InitializeState();

	mutable TWeakObjectPtr<ASolarSystemSimulator> CachedSim;
	bool bStateInitialized = false;
	void MarkThrust();
	void IntegrateRotation(double SimDt, const FVector& Torque);

	FVector PendingAccelReal = FVector::ZeroVector;  // poussee accumulee pendant l'image (m/s2, repere reel)
	FVector PendingTorque = FVector::ZeroVector;     // couple accumule pendant l'image (N.m, repere monde)
	bool bThrustThisFrame = false;
	FQuat Orientation = FQuat::Identity;
};
