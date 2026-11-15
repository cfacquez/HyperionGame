// Mecanique orbitale du systeme solaire.
//
// Les planetes suivent leurs orbites kepleriennes exactes (elements moyens du JPL, E.M. Standish,
// valables 1800-2050), la Lune ses elements geocentriques moyens (P. Schlyter). Les positions sont
// calculees analytiquement a partir de la date simulee : pas de derive dans le temps.
// Les masses servent au calcul de la gravite subie par les vaisseaux (USolarGravityComponent).
//
// Reperes :
//  - "reel" : ecliptique J2000 heliocentrique, en metres, droitier (x vers le point vernal, z vers le nord).
//  - "monde" : coordonnees Unreal (cm). Monde = (x, -y, z) / RealMetersPerUnrealCm.
//    (Unreal est gaucher : on inverse Y, ce qui garde le nord de l'ecliptique vers +Z.)
//
// Fichier volontairement en ASCII (pas d'accents).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SolarSystemSimulator.generated.h"

UENUM(BlueprintType)
enum class ESolarOrbitModel : uint8
{
	Fixed      UMETA(DisplayName = "Fixe (Soleil)"),
	Planet     UMETA(DisplayName = "Planete (elements JPL)"),
	EarthMoon  UMETA(DisplayName = "Terre (barycentre Terre-Lune)"),
	Moon       UMETA(DisplayName = "Lune (geocentrique)")
};

/** Elements kepleriens moyens a J2000 et leurs derives par siecle julien (JPL, Standish). */
USTRUCT(BlueprintType)
struct FSolarKeplerElements
{
	GENERATED_BODY()

	/** Demi-grand axe (UA). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite") double SemiMajorAxisAU = 1.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite") double Eccentricity = 0.0;
	/** Inclinaison sur l'ecliptique (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite") double InclinationDeg = 0.0;
	/** Longitude moyenne (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite") double MeanLongitudeDeg = 0.0;
	/** Longitude du perihelie (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite") double LongPerihelionDeg = 0.0;
	/** Longitude du noeud ascendant (deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite") double LongAscNodeDeg = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite|Derives par siecle") double SemiMajorAxisRate = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite|Derives par siecle") double EccentricityRate = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite|Derives par siecle") double InclinationRate = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite|Derives par siecle") double MeanLongitudeRate = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite|Derives par siecle") double LongPerihelionRate = 0.0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Orbite|Derives par siecle") double LongAscNodeRate = 0.0;
};

/** Un astre du systeme : masse, modele d'orbite et acteur qui le represente dans le niveau. */
USTRUCT(BlueprintType)
struct FSolarBody
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astre")
	FName Name;

	/** Masse (kg). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astre")
	double MassKg = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astre")
	ESolarOrbitModel Model = ESolarOrbitModel::Planet;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astre")
	FSolarKeplerElements Elements;

	/**
	 * Acteur place a la position de l'astre. Pour une planete, c'est l'acteur qui porte la sphere
	 * (et ses nuages, anneaux...). Laisser vide pour un astre purement gravitationnel.
	 */
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Astre")
	TObjectPtr<AActor> DrivenActor = nullptr;

	/** La Lune garde la meme face tournee vers la Terre (axe +X local vers la Terre). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Astre")
	bool bFaceParentBody = false;
};

UCLASS(meta = (DisplayName = "Solar System Simulator"))
class SOLARMECHANICS_API ASolarSystemSimulator : public AActor
{
	GENERATED_BODY()

public:
	ASolarSystemSimulator();

	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------ Reglages
	/** Date simulee au lancement du jeu (UTC). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Temps")
	FDateTime StartDateUTC = FDateTime(2026, 11, 12, 12, 0, 0);

	/** Secondes simulees par seconde de jeu, en cours (pilote par WarpTimeScale / ThrustTimeScale). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Temps", meta = (ClampMin = "0"))
	double TimeScale = 1000.0;

	/** Acceleration du temps hors poussee (reglable en jeu par les boutons x1 a x1000 du HUD). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Temps", meta = (ClampMin = "0"))
	double WarpTimeScale = 1000.0;

	/**
	 * Acceleration pour laquelle les rotations a vitesse fixe du niveau (Terre, nuages...) ont ete reglees :
	 * x1440, un jour = 60 s. Leur vitesse est mise a l'echelle TimeScale / RotationReferenceTimeScale.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar|Temps", meta = (ClampMin = "0.001"))
	double RotationReferenceTimeScale = 1440.0;

	/** Acceleration du temps pendant une poussee (1 = temps reel, physique coherente comme dans KSP). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Temps", meta = (ClampMin = "0"))
	double ThrustTimeScale = 1.0;

	/** Delai (s de jeu) apres la derniere poussee avant de revenir a WarpTimeScale. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Temps", meta = (ClampMin = "0"))
	float ThrustHoldSeconds = 1.0f;

	/** A appeler a chaque image ou un moteur / propulseur pousse : le temps passe a ThrustTimeScale. */
	UFUNCTION(BlueprintCallable, Category = "Solar|Temps")
	void NotifyThrust();

	/** Vrai si le temps est actuellement ralenti par une poussee. */
	UFUNCTION(BlueprintPure, Category = "Solar|Temps")
	bool IsThrustTimeActive() const;

	/** Echelle du niveau : metres reels par centimetre Unreal (2,5 = echelle 1:250, 1 cm Unreal = 250 cm reels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Solar|Echelle", meta = (ClampMin = "0.000001"))
	double RealMetersPerUnrealCm = 2.5;

	/** Constante de gravitation (m3 kg-1 s-2). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar|Physique")
	double GravitationalConstant = 6.67430e-11;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Solar|Astres")
	TArray<FSolarBody> Bodies;

	// ------------------------------------------------------------------ API
	/** Temps simule en secondes depuis J2000 (1er janvier 2000, 12 h TT). */
	UFUNCTION(BlueprintPure, Category = "Solar")
	double GetSimTimeSeconds() const { return SimTime; }

	/** Date simulee courante (UTC). */
	UFUNCTION(BlueprintPure, Category = "Solar")
	FDateTime GetSimDateUTC() const;

	/** Masse d'un astre (kg), 0 si inconnu. */
	UFUNCTION(BlueprintPure, Category = "Solar")
	double GetBodyMass(FName BodyName) const;

	/** Position d'un astre en coordonnees monde (cm) a la date simulee courante. */
	UFUNCTION(BlueprintPure, Category = "Solar")
	FVector GetBodyWorldLocation(FName BodyName) const;

	/** Acceleration gravitationnelle (m/s2, repere reel) au point donne (repere reel, m) a l'instant Time. */
	FVector GravityAt(const FVector& RealPos, double Time) const;

	/** Position (repere reel, m) de l'astre d'indice Index a l'instant Time. */
	FVector BodyRealPosition(int32 Index, double Time) const;

	/** Vitesse (repere reel, m/s) de l'astre d'indice Index a l'instant Time (derivee numerique). */
	FVector BodyRealVelocity(int32 Index, double Time) const;

	int32 FindBodyIndex(FName BodyName) const;
	double BodyGM(int32 Index) const { return GravitationalConstant * Bodies[Index].MassKg; }

	FVector RealToWorld(const FVector& Real) const;
	FVector WorldToReal(const FVector& World) const;
	FVector RealDirToWorld(const FVector& Dir) const { return FVector(Dir.X, -Dir.Y, Dir.Z); }

	/** Premier simulateur du monde (ou nullptr). */
	static ASolarSystemSimulator* Get(const UObject* WorldContext);

	/** Place les astres a la date de depart et prend la main sur leurs acteurs (appele une seule fois). */
	void EnsureInitialized();

private:
	void FreeDrivenActors();
	void ApplyPositions();
	void ComputeAllPositions(double Time, TArray<FVector>& Out) const;

	FVector PlanetHelio(const FSolarKeplerElements& E, double Time) const;
	FVector MoonGeocentric(double Time) const;

	void CaptureScaledMovers();
	void UpdateTimeScale();
	void ApplyMoverScale();

	/** Rotations a vitesse fixe encore actives (Terre, nuages...) : leur vitesse suit TimeScale. */
	struct FScaledMover
	{
		TWeakObjectPtr<class URotatingMovementComponent> Mover;
		FRotator BaseRate;
	};
	TArray<FScaledMover> ScaledMovers;
	double MoverReferenceScale = 1440.0;
	double LastThrustTime = -1.0e9;

	double SimTime = 0.0;
	bool bInitialized = false;
	int32 EarthIndex = INDEX_NONE;
	int32 MoonIndex = INDEX_NONE;
};
