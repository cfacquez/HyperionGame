// Hyperion (classe Atlas) - types partages.
// Fichier volontairement en ASCII (pas d'accents) pour eviter les avertissements de page de code du compilateur.

#pragma once

#include "CoreMinimal.h"
#include "HyperionShipTypes.generated.h"

class USceneComponent;
class UStaticMeshComponent;
class UNiagaraComponent;
class UMaterialInstanceDynamic;

/** Groupes d'armes du vaisseau (voir planche 04 de la fiche de conception). */
UENUM(BlueprintType)
enum class EHyperionWeapon : uint8
{
	Spinal UMETA(DisplayName = "Canon axial"),
	Main   UMETA(DisplayName = "Arme principale 5 canons"),
	Heavy  UMETA(DisplayName = "Tourelles lourdes"),
	Laser  UMETA(DisplayName = "Lasers de poupe"),
	VLS    UMETA(DisplayName = "Missiles VLS"),
	CIWS   UMETA(DisplayName = "Defense rapprochee")
};

/** Manoeuvres pre-cablees sur les propulseurs de manoeuvre avant et arriere. */
UENUM(BlueprintType)
enum class EHyperionManeuver : uint8
{
	PitchUp         UMETA(DisplayName = "Cabrer"),
	PitchDown       UMETA(DisplayName = "Piquer"),
	YawLeft         UMETA(DisplayName = "Lacet a gauche"),
	YawRight        UMETA(DisplayName = "Lacet a droite"),
	TranslateUp     UMETA(DisplayName = "Translation haut"),
	TranslateDown   UMETA(DisplayName = "Translation bas"),
	TranslateLeft   UMETA(DisplayName = "Translation gauche"),
	TranslateRight  UMETA(DisplayName = "Translation droite")
};

/** Debattements et vitesse d'une famille de tourelles (degres, degres/s), dans le repere de leur embase. */
USTRUCT(BlueprintType)
struct FHyperionTurretLimits
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle") float YawMin = -135.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle") float YawMax = 135.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle") float PitchMin = -5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle") float PitchMax = 85.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tourelle") float TurnRate = 20.f;
};

/** Etat d'execution d'une tourelle (embase / lacet / site). */
USTRUCT()
struct FHyperionTurret
{
	GENERATED_BODY()

	UPROPERTY(Transient) TObjectPtr<USceneComponent> Base = nullptr;   // WPN_xxx
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Yaw = nullptr;    // WPN_xxx_Lacet (absent sur les lasers)
	UPROPERTY(Transient) TObjectPtr<USceneComponent> Pitch = nullptr;  // WPN_xxx_Site

	EHyperionWeapon Type = EHyperionWeapon::Heavy;
	FQuat YawBase = FQuat::Identity;
	FQuat PitchBase = FQuat::Identity;
	float CurrentYaw = 0.f;
	float CurrentPitch = 0.f;
	float TargetYaw = 0.f;
	float TargetPitch = 0.f;
	bool bTargetInArc = false;
};

/** Moteur principal ou auxiliaire. */
USTRUCT()
struct FHyperionEngine
{
	GENERATED_BODY()

	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> Nozzle = nullptr;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Fx = nullptr;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMaterials;

	bool bMain = false;
	float ExitOffsetCm = 0.f;  // distance col -> sortie de tuyere
	float Throttle = 0.f;        // poussee courante (0..1), lissee
	float TargetThrottle = 0.f;  // consigne propre a ce moteur
};

/** Propulseur de manoeuvre. */
USTRUCT()
struct FHyperionThruster
{
	GENERATED_BODY()

	UPROPERTY(Transient) TObjectPtr<USceneComponent> Body = nullptr;
	UPROPERTY(Transient) TObjectPtr<UNiagaraComponent> Fx = nullptr;
	UPROPERTY(Transient) TArray<TObjectPtr<UMaterialInstanceDynamic>> GlowMaterials;

	float TimeLeft = 0.f;
};
