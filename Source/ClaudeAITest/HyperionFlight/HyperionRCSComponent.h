// Poussee reelle des propulseurs de manoeuvre de l'Hyperion.
// A chaque image, chaque propulseur allume (AHyperionShip::IsThrusterFiring) pousse le vaisseau depuis sa
// position, a l'oppose de sa tuyere (axe +Z local de la piece) : force + couple transmis au
// USolarGravityComponent du pion, qui integre translation et rotation.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HyperionShipTypes.h"
#include "InputCoreTypes.h"
#include "HyperionRCSComponent.generated.h"

class AHyperionShip;
class USolarGravityComponent;

UCLASS(ClassGroup = (Hyperion), meta = (BlueprintSpawnableComponent, DisplayName = "Hyperion RCS"))
class CLAUDEAITEST_API UHyperionRCSComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UHyperionRCSComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Poussee d'un propulseur de manoeuvre (N). 2 MN : ordre de grandeur d'un gros moteur-fusee. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS", meta = (ClampMin = "0"))
	double ThrustPerThrusterNewtons = 2.0e6;

	/** Les propulseurs de manoeuvre (touches, autopilote, KILL ROT) ramenent le temps a x1 pendant la poussee. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS")
	bool bManeuverThrustSlowsTime = false;

	/** Les moteurs principaux et auxiliaires ramenent le temps a x1 pendant la poussee. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs")
	bool bEngineThrustSlowsTime = true;

	/** Desactive la poussee (les propulseurs restent seulement visuels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS")
	bool bApplyThrust = true;

	/** Nombre de propulseurs allumes a l'image courante. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|RCS")
	int32 ActiveThrusters = 0;

	// ------------------------------------------------------------------ Pilotage clavier
	/** Lacet au clavier : tant qu'une touche est enfoncee, les propulseurs de lacet sont allumes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier")
	bool bKeyboardYaw = true;

	/** Action (Project Settings > Input > Action Mappings) de lacet a gauche. Defaut : Q. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier")
	FName YawLeftAction = TEXT("Hyperion_YawLeft");

	/** Action de lacet a droite. Defaut : D. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier")
	FName YawRightAction = TEXT("Hyperion_YawRight");

	/** Tangage au clavier : tant qu'une touche est enfoncee, les propulseurs de tangage sont allumes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier")
	bool bKeyboardPitch = true;

	/** Action pour piquer (nez vers le bas), comme pousser le manche. Defaut : Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier")
	FName PitchDownAction = TEXT("Hyperion_PitchDown");

	/** Action pour cabrer (nez vers le haut), comme tirer le manche. Defaut : S. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier")
	FName PitchUpAction = TEXT("Hyperion_PitchUp");

	/** Roulis au clavier : tant qu'une touche est enfoncee, les propulseurs arriere de roulis sont allumes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier")
	bool bKeyboardRoll = true;

	/** Action de roulis a gauche (cote gauche vers le bas). Defaut : A. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier")
	FName RollLeftAction = TEXT("Hyperion_RollLeft");

	/** Action de roulis a droite (cote droit vers le bas). Defaut : E. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier")
	FName RollRightAction = TEXT("Hyperion_RollRight");

	/**
	 * Action qui allume ensemble les 8 propulseurs de la section 3 (faces avant, tuyeres vers l'avant) :
	 * poussee vers l'arriere, sans rotation (4 a gauche, 4 a droite). Defaut : touche 9 du pave numerique.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier")
	FName S3ThrustersAction = TEXT("Hyperion_S3Thrusters");

	// ------------------------------------------------------------------ Moteurs principaux et auxiliaires
	/** Poussee reelle des moteurs (a la place d'un simple effet visuel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs")
	bool bApplyEngineThrust = true;

	/** Poussee d'un moteur auxiliaire a 100 % (N). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs", meta = (ClampMin = "0"))
	double AuxEngineMaxThrustNewtons = 50.0e6;

	/** Poussee d'un moteur principal a 100 % (N). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs", meta = (ClampMin = "0"))
	double MainEngineMaxThrustNewtons = 200.0e6;

	/** Action (Action Mappings) qui allume les auxiliaires AuxLowThrustEngines a AuxLowThrustLevel. Defaut : pave 3. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs")
	FName AuxLowThrustAction = TEXT("Hyperion_AuxLowThrust");

	/** Moteurs auxiliaires concernes (numeros 1 a 6). Defaut : A1, A3, A4, A6 (les quatre coins, poussee sans couple). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs")
	TArray<int32> AuxLowThrustEngines = { 1, 3, 4, 6 };

	/** Regime (0..1) applique tant que l'action est maintenue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs", meta = (ClampMin = "0", ClampMax = "1"))
	float AuxLowThrustLevel = 0.03f;

	/** Actions (Action Mappings) qui montent / baissent le regime des 6 auxiliaires. Defaut : + et - du pave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs")
	FName AuxThrottleUpAction = TEXT("Hyperion_AuxThrottleUp");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs")
	FName AuxThrottleDownAction = TEXT("Hyperion_AuxThrottleDown");

	/** Actions qui mettent directement les 6 auxiliaires a 100 % / 0 %. Defaut : Ctrl + et Ctrl - du pave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs")
	FName AuxThrottleMaxAction = TEXT("Hyperion_AuxThrottleMax");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs")
	FName AuxThrottleZeroAction = TEXT("Hyperion_AuxThrottleZero");

	/** Pas de regime par pression (0.01 = 1 %). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs", meta = (ClampMin = "0.001", ClampMax = "1"))
	float AuxThrottleStep = 0.01f;

	/** Touche maintenue : delai avant la repetition, puis intervalle entre deux pas (s, temps reel). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs", meta = (ClampMin = "0"))
	float AuxRepeatDelay = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|Moteurs", meta = (ClampMin = "0.01"))
	float AuxRepeatInterval = 0.08f;

	/** Regime commande des 6 auxiliaires (0..1). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|Moteurs")
	float AuxThrottleSetting = 0.f;

	/** Change le regime commande des 6 auxiliaires. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|Moteurs")
	void SetAuxThrottleSetting(float Throttle);

	/** Allume les 8 propulseurs de la section 3 pendant Duration secondes. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|RCS")
	void FireSection3(float Duration);

	/**
	 * Roulis : le plugin n'a pas de manoeuvre de roulis, on allume 4 propulseurs arriere formant un couple pur
	 * (forces opposees de part et d'autre de l'axe, sans poussee nette).
	 */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|RCS")
	void FireRoll(bool bRight, float Duration);

	// ------------------------------------------------------------------ KILL ROT
	/**
	 * Annule toutes les rotations avec les propulseurs de manoeuvre (tangage, lacet, roulis en meme temps).
	 * Reste actif jusqu'a l'arret complet, puis se coupe seul. Un nouvel appel pendant l'annulation l'interrompt.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|RCS|Kill Rot")
	void ToggleKillRotation();

	UFUNCTION(BlueprintCallable, Category = "Hyperion|RCS|Kill Rot")
	void SetKillRotation(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Hyperion|RCS|Kill Rot")
	bool IsKillRotationActive() const { return bKillRotation; }

	/**
	 * Fin de l'annulation : quand chaque axe tourne moins vite que ce que les propulseurs corrigent en une image,
	 * le reliquat (imperceptible) est annule et KILL ROT se coupe. Multiplicateur de cette tolerance.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Kill Rot", meta = (ClampMin = "1"))
	float KillRotDeadbandFrames = 1.5f;

	// ------------------------------------------------------------------ PRO (autopilote)
	/**
	 * Oriente le nez de l'Hyperion dans le sens de sa vitesse orbitale (prograde, par rapport a
	 * ProgradeReferenceBody) avec les propulseurs de manoeuvre, puis suit cette direction tant qu'aucune
	 * autre action n'est faite (touche de rotation, autre bouton). Le roulis est seulement amorti.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|RCS|Autopilote")
	void SetPrograde(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Hyperion|RCS|Autopilote")
	bool IsProgradeActive() const { return bPrograde && OrientTarget == 0; }

	/**
	 * Vrai quand la manoeuvre d'orientation en cours (PRO, RET, ORBIT+, ORBIT-) est accomplie : roulis annule et
	 * nez aligne sur la cible (maintien, ecart sous la moitie de AutopilotHoldToleranceDeg, hysteresis jusqu'a la
	 * tolerance entiere).
	 */
	UFUNCTION(BlueprintPure, Category = "Hyperion|RCS|Autopilote")
	bool IsOrientationAligned() const { return bPrograde && bProgradeHolding && bRollAligned && !bRollDampPhase; }

	/** PRO / RET : ecart de roulis (deg) entre le dessus du vaisseau (passerelle) et la direction de l'astre. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|RCS|Autopilote")
	float RollErrorDeg = 0.f;

	/**
	 * RET : meme pilotage que PRO, nez a l'oppose de la vitesse orbitale (retrograde), par exemple pour
	 * freiner avec les moteurs principaux. PRO et RET s'excluent ; toute autre action l'annule.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|RCS|Autopilote")
	void SetRetrograde(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Hyperion|RCS|Autopilote")
	bool IsRetrogradeActive() const { return bPrograde && OrientTarget == 1; }

	/**
	 * ORBIT+ : meme pilotage que PRO, nez selon la normale au plan de l'orbite (sens du moment cinetique,
	 * r x v par rapport a ProgradeReferenceBody). Sert a changer l'inclinaison de l'orbite.
	 */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|RCS|Autopilote")
	void SetOrbitNormal(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Hyperion|RCS|Autopilote")
	bool IsOrbitNormalActive() const { return bPrograde && OrientTarget == 2; }

	/** ORBIT- : meme pilotage, nez a l'oppose de la normale a l'orbite (anti-normale). */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|RCS|Autopilote")
	void SetOrbitAntiNormal(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Hyperion|RCS|Autopilote")
	bool IsOrbitAntiNormalActive() const { return bPrograde && OrientTarget == 3; }

	/** Astre de reference de la vitesse orbitale. Vide : celui du SolarGravityComponent (Terre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Autopilote")
	FName ProgradeReferenceBody = NAME_None;

	/**
	 * Vitesse de rotation maximale de l'autopilote (deg/s, temps simule). Le vaisseau accelere a x1, file a cette
	 * vitesse en temps accelere puis freine juste ce qu'il faut pour s'arreter sur la cible.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Autopilote", meta = (ClampMin = "0.001"))
	float AutopilotMaxRateDegPerSec = 0.05f;

	/**
	 * Duree reelle visee pour parcourir l'ecart en rotation libre (s). En temps accelere, la vitesse visee est
	 * reduite d'autant : l'essentiel de la manoeuvre se fait sans poussee, donc sans repasser a x1.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Autopilote", meta = (ClampMin = "0.1"))
	float AutopilotCoastSeconds = 4.f;

	/** Marge sur la capacite de freinage (0..1) : plus petit = arrivee plus douce, moins de depassement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Autopilote", meta = (ClampMin = "0.1", ClampMax = "1"))
	float AutopilotBrakingMargin = 0.6f;

	/** Ecart (deg) en dessous duquel le vaisseau est considere aligne : les corrections se font plus rares. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Autopilote", meta = (ClampMin = "0"))
	float AutopilotHoldToleranceDeg = 1.0f;

	/** Une fois aligne, ecart de vitesse de rotation tolere avant correction (deg/s, temps simule). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Autopilote", meta = (ClampMin = "0"))
	float AutopilotHoldRateToleranceDegPerSec = 0.001f;

	/** Nombre d'images ou l'autopilote a allume chaque paire (X tangage, Y lacet, Z roulis), pour le reglage. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|RCS|Autopilote")
	FIntVector AutopilotPairFrames = FIntVector::ZeroValue;

	/** Ecart actuel entre le nez et la direction prograde (deg). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Hyperion|RCS|Autopilote")
	float ProgradeErrorDeg = 0.f;

	/** Annule l'autopilote (PRO) : toute autre action du pilote. */
	UFUNCTION(BlueprintCallable, Category = "Hyperion|RCS|Autopilote")
	void CancelAutopilot();

	/** Duree d'allumage entretenue a chaque image tant que la touche est tenue (s) ; extinction juste apres le relachement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Clavier", meta = (ClampMin = "0.01"))
	float KeyPulseSeconds = 0.1f;

	/** Vaisseau pilote (trouve automatiquement dans un ChildActorComponent du pion). */
	UFUNCTION(BlueprintPure, Category = "Hyperion|RCS")
	AHyperionShip* GetShip() const;

	// ------------------------------------------------------------------ Verification
	/** Lance une manoeuvre au demarrage, pour verifier le sens des poussees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	bool bDebugManeuverAtStart = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	EHyperionManeuver DebugManeuver = EHyperionManeuver::PitchUp;

	/**
	 * Verification de KILL ROT : si non nul, au lieu de la manoeuvre, donne au vaisseau cette rotation
	 * (deg/s, axes locaux X roulis, Y tangage, Z lacet) puis active KILL ROT DebugKillRotDelay s plus tard.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	FVector DebugSpinDegPerSec = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	float DebugKillRotDelay = 3.f;

	/** Verification de PRO : l'active DebugDelay s apres le depart (sans manoeuvre). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	bool bDebugProgradeAtStart = false;

	/** Verification de RET : l'active DebugDelay s apres le depart (prioritaire sur PRO). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	bool bDebugRetrogradeAtStart = false;

	/** Verification de ORBIT+ : l'active DebugDelay s apres le depart (prioritaire sur RET et PRO). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	bool bDebugOrbitNormalAtStart = false;

	/** Verification de ORBIT- : l'active DebugDelay s apres le depart (prioritaire sur les autres). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	bool bDebugOrbitAntiNormalAtStart = false;

	/** Verification : regime des 6 auxiliaires a DebugAuxLevel de DebugDelay a DebugDelay + DebugDuration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	bool bDebugAuxBurn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug", meta = (ClampMin = "0", ClampMax = "1"))
	float DebugAuxLevel = 1.f;

	/** Verification du roulis : 0 = manoeuvre DebugManeuver, -1 = roulis a gauche, +1 = roulis a droite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug", meta = (ClampMin = "-1", ClampMax = "1"))
	int32 DebugRoll = 0;

	/** Duree de la manoeuvre de verification (s de jeu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	float DebugDuration = 5.f;

	/** Delai avant la manoeuvre de verification (s de jeu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hyperion|RCS|Debug")
	float DebugDelay = 2.f;

private:
	mutable TWeakObjectPtr<AHyperionShip> CachedShip;
	TWeakObjectPtr<USolarGravityComponent> Gravity;
	float Elapsed = 0.f;
	bool bDebugFired = false;
	bool bKillRotation = false;
	bool bPrograde = false;          // pilotage d'orientation (PRO ou RET) actif
	uint8 OrientTarget = 0;          // cible : 0 PRO, 1 RET, 2 ORBIT+ (normale), 3 ORBIT- (anti-normale)
	void SetOrientation(bool bActive, uint8 Target);
	bool bDebugKillRotPending = false;
	bool bDebugProgradeFired = false;
	FVector PrevProgradeDir = FVector::ZeroVector;
	FVector ProgradeDirRate = FVector::ZeroVector;  // rotation filtree de la direction prograde (rad/s)
	bool bProgradeBigCorrection = true;
	bool bProgradeHolding = false;
	bool bRollHolding = false;      // PRO / RET : roulis aligne (dessus vers l'astre), avec hysteresis
	bool bRollAligned = false;      // condition de roulis remplie pour la manoeuvre en cours
	bool bRollDampPhase = false;  // PRO / RET / ORBIT+/- : annulation du roulis avant la manoeuvre
	double PrevProgradeSimTime = 0.0;
	float SmoothedDeltaTime = 1.f / 60.f;
	double AutopilotAlpha = 1.0e-5;  // acceleration angulaire tangage / lacet disponible (rad/s2), mise a jour en vol
	bool bShipTickOrdered = false;
	bool bAuxLowThrustHeld = false;
	bool bDebugAuxBurnDone = false;
	int32 AuxRepeatDir = 0;        // +1 / -1 : touche de regime maintenue
	bool bAuxMaxHeld = false;
	bool bAuxZeroHeld = false;
	float AuxRepeatTimer = 0.f;
	void ApplyAuxThrottles();      // consignes des 6 auxiliaires (regime commande + pave 3)

	/** Couple (N.m, repere monde) produit par un groupe de propulseurs. */
	FVector ThrusterTorque(const AHyperionShip* Ship, TArrayView<const TCHAR* const> Names) const;
	/**
	 * Une image de pilotage d'attitude : allume les paires tangage / lacet / roulis qui rapprochent la vitesse de
	 * rotation de TargetW (rad/s, repere monde). Renvoie false si aucun axe n'avait besoin de correction.
	 * MinAlpha recoit l'acceleration angulaire la plus faible des paires tangage / lacet (rad/s2).
	 */
	bool TickAttitude(AHyperionShip* Ship, float DeltaTime, const FVector& TargetW, double* MinAlpha = nullptr, double ExtraDeadband = 0.0,
		bool bRollOnlyWhenIdle = false);
	/** Une image d'annulation ; true si tout est arrete. */
	bool TickKillRotation(AHyperionShip* Ship, float DeltaTime);
	/** Une image de PRO. */
	void TickPrograde(AHyperionShip* Ship, float DeltaTime);
	/** Direction du nez (repere monde). */
	FVector GetNoseDirection(const AHyperionShip* Ship) const;
};
