// Hyperion (classe Atlas) - acteur du vaisseau.
// Fichier volontairement en ASCII (pas d'accents).

#include "HyperionShipActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogHyperion, Log, All);

namespace HyperionNames
{
	// Noms des pieces tels qu'ecrits dans Hyperion_Blockout.glb.
	static const TCHAR* RingPivot   = TEXT("Pivot_Anneau");
	static const TCHAR* Spinal      = TEXT("WPN_Spinal");
	static const TCHAR* Dish        = TEXT("SM_Antenne_Parabole");
	static const TCHAR* Mast        = TEXT("SM_Antenne_Passerelle");
	static const TCHAR* Doors[]     = { TEXT("SM_PorteHangar_G"), TEXT("SM_PorteHangar_D") };
	static const TCHAR* MainEngines[] = { TEXT("SM_Tuyere_Main_L"), TEXT("SM_Tuyere_Main_R") };
	static const TCHAR* AuxEngines[]  = { TEXT("SM_Tuyere_Aux_01"), TEXT("SM_Tuyere_Aux_02"), TEXT("SM_Tuyere_Aux_03"),
	                                      TEXT("SM_Tuyere_Aux_04"), TEXT("SM_Tuyere_Aux_05"), TEXT("SM_Tuyere_Aux_06") };
	static const TCHAR* Thrusters[] = {
		TEXT("SM_Propulseur_Manoeuvre_H"), TEXT("SM_Propulseur_Manoeuvre_B"),
		TEXT("SM_Propulseur_Manoeuvre_G"), TEXT("SM_Propulseur_Manoeuvre_D"),
		TEXT("SM_Propulseur_Manoeuvre_AR_H1"), TEXT("SM_Propulseur_Manoeuvre_AR_H2"),
		TEXT("SM_Propulseur_Manoeuvre_AR_B1"), TEXT("SM_Propulseur_Manoeuvre_AR_B2"),
		TEXT("SM_Propulseur_Manoeuvre_AR_G1"), TEXT("SM_Propulseur_Manoeuvre_AR_G2"),
		TEXT("SM_Propulseur_Manoeuvre_AR_D1"), TEXT("SM_Propulseur_Manoeuvre_AR_D2"),
		// Section 3 (commandement) : faces avant, tuyeres vers l'avant selon l'axe longitudinal.
		// 1 / 2 : colonne interieure (haut / bas), 3 / 4 : colonne exterieure (haut / bas).
		TEXT("SM_Propulseur_Manoeuvre_S3_G1"), TEXT("SM_Propulseur_Manoeuvre_S3_G2"),
		TEXT("SM_Propulseur_Manoeuvre_S3_G3"), TEXT("SM_Propulseur_Manoeuvre_S3_G4"),
		TEXT("SM_Propulseur_Manoeuvre_S3_D1"), TEXT("SM_Propulseur_Manoeuvre_S3_D2"),
		TEXT("SM_Propulseur_Manoeuvre_S3_D3"), TEXT("SM_Propulseur_Manoeuvre_S3_D4") };

	// Cotes utiles de la maquette (cm, repere local des pieces).
	static constexpr float MainNozzleExit = 3200.f;   // col -> sortie, moteurs principaux
	static constexpr float AuxNozzleExit  = 2070.f;   // col -> sortie, moteurs auxiliaires
	static constexpr float RCSNozzleExit  = 360.f;    // socle -> sortie, propulseurs de manoeuvre
	static constexpr float MastHeight     = 7050.f;   // pied -> feu de l'antenne verticale
	static constexpr float VLSHatch       = 300.f;
	static constexpr float CIWSHead       = 400.f;
}

AHyperionShip::AHyperionShip()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	FireIntervals.Add(EHyperionWeapon::Spinal, 20.f);
	FireIntervals.Add(EHyperionWeapon::Main, 6.f);
	FireIntervals.Add(EHyperionWeapon::Heavy, 2.f);
	FireIntervals.Add(EHyperionWeapon::Laser, 0.5f);
	FireIntervals.Add(EHyperionWeapon::VLS, 4.f);
	FireIntervals.Add(EHyperionWeapon::CIWS, 0.1f);

	// Debattements repris de la planche 04 (repere de l'embase de chaque tourelle).
	auto SetLimits = [](FHyperionTurretLimits& L, float YawMin, float YawMax, float PitchMin, float PitchMax, float Rate)
	{
		L.YawMin = YawMin; L.YawMax = YawMax; L.PitchMin = PitchMin; L.PitchMax = PitchMax; L.TurnRate = Rate;
	};
	SetLimits(HeavyLimits, -135.f, 135.f, -5.f, 85.f, 20.f);
	SetLimits(MainLimits, -150.f, 150.f, 0.f, 85.f, 8.f);
	SetLimits(LaserLimits, -90.f, 90.f, 0.f, 80.f, 45.f);
}

// ===================================================================== Initialisation

void AHyperionShip::BeginPlay()
{
	Super::BeginPlay();

	// Le vaisseau se deplace et plusieurs pieces s'animent : tout doit etre mobile.
	if (RootComponent && RootComponent->Mobility != EComponentMobility::Movable)
	{
		RootComponent->SetMobility(EComponentMobility::Movable);
	}

	CollectParts();
	MissingParts.Reset();

	InitAxis();
	InitRing();
	InitTurrets();
	InitEngines();
	InitThrusters();
	InitDishAndDoors();
	InitBeacon();

	if (MissingParts.Num() > 0)
	{
		UE_LOG(LogHyperion, Warning, TEXT("%s : %d piece(s) introuvable(s) : %s"),
			*GetName(), MissingParts.Num(), *FString::Join(MissingParts, TEXT(", ")));
	}
	UE_LOG(LogHyperion, Log, TEXT("%s : %d tourelles, %d moteurs, %d propulseurs de manoeuvre."),
		*GetName(), Turrets.Num(), Engines.Num(), Thrusters.Num());
}

void AHyperionShip::CollectParts()
{
	PartsByName.Reset();
	TInlineComponentArray<USceneComponent*> Components(this);
	for (USceneComponent* Comp : Components)
	{
		if (!Comp)
		{
			continue;
		}
		FString Name = Comp->GetName();
		Name.RemoveFromEnd(TEXT("_GEN_VARIABLE"));
		PartsByName.Add(Name, Comp);
	}
}

USceneComponent* AHyperionShip::FindPart(FName PartName) const
{
	const TObjectPtr<USceneComponent>* Found = PartsByName.Find(PartName.ToString());
	return Found ? Found->Get() : nullptr;
}

USceneComponent* AHyperionShip::Require(const FString& Name)
{
	if (const TObjectPtr<USceneComponent>* Found = PartsByName.Find(Name))
	{
		return Found->Get();
	}
	MissingParts.AddUnique(Name);
	return nullptr;
}

void AHyperionShip::InitAxis()
{
	// L'axe du vaisseau passe par la bouche du canon axial et par le centre de l'anneau :
	// on le calcule a partir des pieces importees, quelle que soit la convention d'axes de l'import.
	USceneComponent* Spinal = Require(HyperionNames::Spinal);
	USceneComponent* Pivot = Require(HyperionNames::RingPivot);
	FVector WorldForward = GetActorForwardVector();
	if (Spinal && Pivot)
	{
		const FVector Dir = Spinal->GetComponentLocation() - Pivot->GetComponentLocation();
		if (!Dir.IsNearlyZero())
		{
			WorldForward = Dir.GetSafeNormal();
		}
	}
	ShipForwardLocal = GetActorTransform().InverseTransformVectorNoScale(WorldForward);
}

FVector AHyperionShip::GetShipForward() const
{
	return GetActorTransform().TransformVectorNoScale(ShipForwardLocal).GetSafeNormal();
}

void AHyperionShip::InitRing()
{
	RingPivot = Require(HyperionNames::RingPivot);
	if (!RingPivot)
	{
		return;
	}
	RingBase = RingPivot->GetRelativeRotation().Quaternion();
	const USceneComponent* Parent = RingPivot->GetAttachParent();
	const FTransform ParentXf = Parent ? Parent->GetComponentTransform() : GetActorTransform();
	RingAxisInParent = ParentXf.InverseTransformVectorNoScale(GetShipForward()).GetSafeNormal();
	RingPivot->SetMobility(EComponentMobility::Movable);
}

void AHyperionShip::InitTurrets()
{
	Turrets.Reset();

	auto AddTurret = [this](const FString& BaseName, EHyperionWeapon Type)
	{
		FHyperionTurret T;
		T.Type = Type;
		T.Base = Require(BaseName);
		T.Yaw = (Type == EHyperionWeapon::Laser) ? nullptr : Require(BaseName + TEXT("_Lacet"));
		T.Pitch = Require(BaseName + TEXT("_Site"));
		if (!T.Base || !T.Pitch || (Type != EHyperionWeapon::Laser && !T.Yaw))
		{
			return;
		}
		T.YawBase = T.Yaw ? T.Yaw->GetRelativeRotation().Quaternion() : FQuat::Identity;
		T.PitchBase = T.Pitch->GetRelativeRotation().Quaternion();
		if (T.Yaw)
		{
			T.Yaw->SetMobility(EComponentMobility::Movable);
		}
		T.Pitch->SetMobility(EComponentMobility::Movable);
		Turrets.Add(T);
	};

	AddTurret(TEXT("WPN_Main"), EHyperionWeapon::Main);
	for (int32 i = 1; i <= 8; ++i)
	{
		AddTurret(FString::Printf(TEXT("WPN_Heavy_%02d"), i), EHyperionWeapon::Heavy);
	}
	for (int32 i = 1; i <= 4; ++i)
	{
		AddTurret(FString::Printf(TEXT("WPN_Laser_%02d"), i), EHyperionWeapon::Laser);
	}
}

void AHyperionShip::CollectGlowMaterials(UStaticMeshComponent* Mesh, TArray<TObjectPtr<UMaterialInstanceDynamic>>& Out) const
{
	if (!Mesh)
	{
		return;
	}
	const TArray<FName> SlotNames = Mesh->GetMaterialSlotNames();
	for (int32 i = 0; i < Mesh->GetNumMaterials(); ++i)
	{
		const UMaterialInterface* Mat = Mesh->GetMaterial(i);
		const bool bSlotMatch = SlotNames.IsValidIndex(i) && SlotNames[i].ToString().Contains(TEXT("Propulsion"));
		const bool bMatMatch = Mat && Mat->GetName().Contains(TEXT("Propulsion"));
		if (bSlotMatch || bMatMatch)
		{
			if (UMaterialInstanceDynamic* MID = Mesh->CreateDynamicMaterialInstance(i))
			{
				Out.Add(MID);
			}
		}
	}
}

void AHyperionShip::SetGlow(const TArray<TObjectPtr<UMaterialInstanceDynamic>>& Mats, float Alpha) const
{
	const FLinearColor Value = EngineGlowColor * FMath::Lerp(0.15f, GlowAtFullThrottle, FMath::Clamp(Alpha, 0.f, 1.f));
	for (UMaterialInstanceDynamic* MID : Mats)
	{
		if (MID)
		{
			MID->SetVectorParameterValue(GlowMaterialParam, Value);
		}
	}
}

void AHyperionShip::InitEngines()
{
	Engines.Reset();
	const FVector Back = -GetShipForward();

	auto AddEngine = [this, &Back](const TCHAR* Name, bool bMain)
	{
		UStaticMeshComponent* Nozzle = Cast<UStaticMeshComponent>(Require(Name));
		if (!Nozzle)
		{
			return;
		}
		FHyperionEngine E;
		E.Nozzle = Nozzle;
		E.bMain = bMain;
		E.ExitOffsetCm = bMain ? HyperionNames::MainNozzleExit : HyperionNames::AuxNozzleExit;
		CollectGlowMaterials(Nozzle, E.GlowMaterials);

		if (UNiagaraSystem* Fx = bMain ? MainExhaustFX.Get() : AuxExhaustFX.Get())
		{
			const FVector Exit = Nozzle->GetComponentLocation() + Back * E.ExitOffsetCm;
			E.Fx = UNiagaraFunctionLibrary::SpawnSystemAttached(Fx, Nozzle, NAME_None, Exit,
				FRotationMatrix::MakeFromX(Back).Rotator(), EAttachLocation::KeepWorldPosition, false, true);
		}
		Engines.Add(E);
	};

	for (const TCHAR* Name : HyperionNames::MainEngines) { AddEngine(Name, true); }
	for (const TCHAR* Name : HyperionNames::AuxEngines)  { AddEngine(Name, false); }

	MainThrottle = AuxThrottle = 0.f;
	TickEngines(0.f);
}

void AHyperionShip::InitThrusters()
{
	Thrusters.Reset();
	for (const TCHAR* Name : HyperionNames::Thrusters)
	{
		USceneComponent* Body = Require(Name);
		if (!Body)
		{
			continue;
		}
		FHyperionThruster T;
		T.Body = Body;
		CollectGlowMaterials(Cast<UStaticMeshComponent>(Body), T.GlowMaterials);
		SetGlow(T.GlowMaterials, 0.f);
		Thrusters.Add(FName(Name), T);
	}
}

void AHyperionShip::InitDishAndDoors()
{
	Dish = Require(HyperionNames::Dish);
	if (Dish)
	{
		Dish->SetMobility(EComponentMobility::Movable);
		DishBase = Dish->GetRelativeRotation().Quaternion();
		// L'axe de la parabole est son axe Z local ; on note son cap de repos dans le repere du mat.
		const FVector Axis = DishBase.RotateVector(FVector::UpVector);
		DishRestYaw = FMath::RadiansToDegrees(FMath::Atan2(Axis.Y, Axis.X));
		DishYaw = 0.f;
	}

	Doors.Reset();
	DoorBase.Reset();
	for (const TCHAR* Name : HyperionNames::Doors)
	{
		if (USceneComponent* Door = Require(Name))
		{
			Door->SetMobility(EComponentMobility::Movable);
			Doors.Add(Door);
			DoorBase.Add(Door->GetRelativeLocation());
		}
	}
}

void AHyperionShip::InitBeacon()
{
	if (!bBeacon)
	{
		return;
	}
	USceneComponent* Mast = Require(HyperionNames::Mast);
	if (!Mast)
	{
		return;
	}
	BeaconLight = NewObject<UPointLightComponent>(this, TEXT("HyperionBeacon"));
	BeaconLight->SetupAttachment(Mast);
	BeaconLight->SetRelativeLocation(FVector(0.f, 0.f, HyperionNames::MastHeight));
	BeaconLight->SetLightColor(BeaconColor);
	BeaconLight->SetAttenuationRadius(30000.f);
	BeaconLight->SetIntensity(0.f);
	BeaconLight->SetCastShadows(false);
	BeaconLight->SetMobility(EComponentMobility::Movable);
	BeaconLight->RegisterComponent();
}

// ===================================================================== Tick

void AHyperionShip::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	TickRing(DeltaSeconds);
	TickTurrets(DeltaSeconds);
	TickEngines(DeltaSeconds);
	TickThrusters(DeltaSeconds);
	TickDish(DeltaSeconds);
	TickDoors(DeltaSeconds);
	TickBeacon(DeltaSeconds);
}

void AHyperionShip::TickRing(float Dt)
{
	if (!RingPivot || RingTimeScale <= 0.f)
	{
		return;
	}
	RingAngleDeg = FMath::Fmod(RingAngleDeg + RingSpeedDegPerSec * RingTimeScale * Dt, 360.f);
	const FQuat Spin(RingAxisInParent, FMath::DegreesToRadians(RingAngleDeg));
	RingPivot->SetRelativeRotation(Spin * RingBase);
}

void AHyperionShip::TickTurrets(float Dt)
{
	for (FHyperionTurret& T : Turrets)
	{
		const float Rate = LimitsFor(T.Type).TurnRate * Dt;
		T.CurrentYaw = FMath::FixedTurn(T.CurrentYaw, T.TargetYaw, Rate);
		T.CurrentPitch = FMath::FixedTurn(T.CurrentPitch, T.TargetPitch, Rate);

		const FQuat YawQ = FRotator(0.f, T.CurrentYaw, 0.f).Quaternion();
		const FQuat PitchQ = FRotator(T.CurrentPitch, 0.f, 0.f).Quaternion();
		if (T.Yaw)
		{
			T.Yaw->SetRelativeRotation(YawQ * T.YawBase);
			T.Pitch->SetRelativeRotation(PitchQ * T.PitchBase);
		}
		else
		{
			// Lasers : pas de piece de lacet, le site porte les deux rotations.
			T.Pitch->SetRelativeRotation(YawQ * PitchQ * T.PitchBase);
		}
	}
}

void AHyperionShip::TickEngines(float Dt)
{
	// Chaque moteur suit sa propre consigne ; MainThrottle / AuxThrottle donnent le plus fort de chaque groupe.
	float MaxMain = 0.f;
	float MaxAux = 0.f;
	for (FHyperionEngine& E : Engines)
	{
		if (Dt > 0.f)
		{
			E.Throttle = FMath::FInterpConstantTo(E.Throttle, E.TargetThrottle, Dt, ThrottleResponse);
		}
		float& GroupMax = E.bMain ? MaxMain : MaxAux;
		GroupMax = FMath::Max(GroupMax, E.Throttle);
	}
	MainThrottle = MaxMain;
	AuxThrottle = MaxAux;
	for (FHyperionEngine& E : Engines)
	{
		const float Thrust = E.Throttle;
		SetGlow(E.GlowMaterials, Thrust);
		if (E.Fx)
		{
			E.Fx->SetVariableFloat(NiagaraThrustParam, Thrust);
		}
	}
}

void AHyperionShip::TickThrusters(float Dt)
{
	for (TPair<FName, FHyperionThruster>& Pair : Thrusters)
	{
		FHyperionThruster& T = Pair.Value;
		if (T.TimeLeft <= 0.f)
		{
			continue;
		}
		T.TimeLeft -= Dt;
		if (T.TimeLeft <= 0.f)
		{
			T.TimeLeft = 0.f;
			SetGlow(T.GlowMaterials, 0.f);
			if (T.Fx)
			{
				T.Fx->Deactivate();
			}
		}
	}
}

void AHyperionShip::TickDish(float Dt)
{
	if (!Dish)
	{
		return;
	}
	if (bDishHasTarget)
	{
		const USceneComponent* Parent = Dish->GetAttachParent();
		const FTransform ParentXf = Parent ? Parent->GetComponentTransform() : GetActorTransform();
		const FVector Local = ParentXf.InverseTransformVectorNoScale(DishTarget - Dish->GetComponentLocation());
		const float Desired = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X)) - DishRestYaw;
		DishYaw = FMath::FixedTurn(DishYaw, Desired, DishTurnRate * Dt);
	}
	else if (bDishSweep)
	{
		DishYaw = FMath::Fmod(DishYaw + DishTurnRate * Dt, 360.f);
	}
	else
	{
		return;
	}
	// Rotation autour de l'axe vertical du mat (repere parent), appliquee a l'orientation de repos.
	Dish->SetRelativeRotation(FQuat(FVector::UpVector, FMath::DegreesToRadians(DishYaw)) * DishBase);
}

void AHyperionShip::TickDoors(float Dt)
{
	if (Doors.Num() == 0 || FMath::IsNearlyEqual(DoorAlpha, DoorTarget))
	{
		return;
	}
	DoorAlpha = FMath::FInterpConstantTo(DoorAlpha, DoorTarget, Dt, DoorSpeed);
	const float Ease = FMath::InterpEaseInOut(0.f, 1.f, DoorAlpha, 2.f);
	for (int32 i = 0; i < Doors.Num(); ++i)
	{
		if (Doors[i])
		{
			// Glissement vers le haut dans le repere de la nacelle.
			Doors[i]->SetRelativeLocation(DoorBase[i] + FVector(0.f, 0.f, Ease * DoorSlideCm));
		}
	}
}

void AHyperionShip::TickBeacon(float Dt)
{
	if (!BeaconLight)
	{
		return;
	}
	BeaconClock = FMath::Fmod(BeaconClock + Dt, FMath::Max(BeaconPeriod, 0.1f));
	const bool bOn = bBeacon && BeaconClock < 0.15f;
	BeaconLight->SetIntensity(bOn ? 2.0e6f : 0.f);
}

// ===================================================================== Propulsion

void AHyperionShip::SetThrottle(float Main, float Aux)
{
	TargetMainThrottle = FMath::Clamp(Main, 0.f, 1.f);
	TargetAuxThrottle = FMath::Clamp(Aux, 0.f, 1.f);
	for (FHyperionEngine& E : Engines)
	{
		E.TargetThrottle = E.bMain ? TargetMainThrottle : TargetAuxThrottle;
	}
}

void AHyperionShip::SetEngineThrottle(int32 EngineIndex, float Throttle)
{
	if (Engines.IsValidIndex(EngineIndex))
	{
		Engines[EngineIndex].TargetThrottle = FMath::Clamp(Throttle, 0.f, 1.f);
	}
}

float AHyperionShip::GetEngineThrottle(int32 EngineIndex) const
{
	return Engines.IsValidIndex(EngineIndex) ? Engines[EngineIndex].Throttle : 0.f;
}

USceneComponent* AHyperionShip::GetEngineNozzle(int32 EngineIndex) const
{
	return Engines.IsValidIndex(EngineIndex) ? Engines[EngineIndex].Nozzle.Get() : nullptr;
}

bool AHyperionShip::FireRCS(FName ThrusterName, float Duration)
{
	FHyperionThruster* T = Thrusters.Find(ThrusterName);
	if (!T || !T->Body)
	{
		return false;
	}
	T->TimeLeft = FMath::Max(T->TimeLeft, Duration);
	SetGlow(T->GlowMaterials, 1.f);

	if (RCSFX)
	{
		if (!T->Fx)
		{
			// La tuyere pointe selon l'axe Z local du propulseur.
			const FVector Up = T->Body->GetUpVector();
			const FVector Exit = T->Body->GetComponentLocation() + Up * HyperionNames::RCSNozzleExit;
			T->Fx = UNiagaraFunctionLibrary::SpawnSystemAttached(RCSFX, T->Body, NAME_None, Exit,
				FRotationMatrix::MakeFromX(Up).Rotator(), EAttachLocation::KeepWorldPosition, false, true);
		}
		else
		{
			T->Fx->Activate(true);
		}
		if (T->Fx)
		{
			T->Fx->SetVariableFloat(NiagaraThrustParam, 1.f);
		}
	}
	return true;
}

bool AHyperionShip::IsThrusterFiring(FName ThrusterName) const
{
	const FHyperionThruster* T = Thrusters.Find(ThrusterName);
	return T && T->TimeLeft > 0.f;
}

void AHyperionShip::PerformManeuver(EHyperionManeuver Maneuver, float Duration)
{
	// Un propulseur pousse le vaisseau a l'oppose de sa tuyere.
	// Avant : H, B, G, D. Arriere : AR_H1/H2, AR_B1/B2, AR_G1/G2, AR_D1/D2.
	static const FString Front = TEXT("SM_Propulseur_Manoeuvre_");
	static const FString Rear = TEXT("SM_Propulseur_Manoeuvre_AR_");

	TArray<FString> Fire;
	auto RearPair = [&Fire](const TCHAR* Face)
	{
		Fire.Add(Rear + Face + TEXT("1"));
		Fire.Add(Rear + Face + TEXT("2"));
	};

	switch (Maneuver)
	{
	case EHyperionManeuver::PitchUp:        Fire.Add(Front + TEXT("B")); RearPair(TEXT("H")); break;
	case EHyperionManeuver::PitchDown:      Fire.Add(Front + TEXT("H")); RearPair(TEXT("B")); break;
	case EHyperionManeuver::YawLeft:        Fire.Add(Front + TEXT("D")); RearPair(TEXT("G")); break;
	case EHyperionManeuver::YawRight:       Fire.Add(Front + TEXT("G")); RearPair(TEXT("D")); break;
	case EHyperionManeuver::TranslateUp:    Fire.Add(Front + TEXT("B")); RearPair(TEXT("B")); break;
	case EHyperionManeuver::TranslateDown:  Fire.Add(Front + TEXT("H")); RearPair(TEXT("H")); break;
	case EHyperionManeuver::TranslateLeft:  Fire.Add(Front + TEXT("D")); RearPair(TEXT("D")); break;
	case EHyperionManeuver::TranslateRight: Fire.Add(Front + TEXT("G")); RearPair(TEXT("G")); break;
	default: break;
	}

	for (const FString& Name : Fire)
	{
		FireRCS(FName(*Name), Duration);
	}
}

// ===================================================================== Armement

const FHyperionTurretLimits& AHyperionShip::LimitsFor(EHyperionWeapon Type) const
{
	switch (Type)
	{
	case EHyperionWeapon::Main:  return MainLimits;
	case EHyperionWeapon::Laser: return LaserLimits;
	default:                     return HeavyLimits;
	}
}

void AHyperionShip::AimTurret(FHyperionTurret& T, const FVector& WorldTarget) const
{
	// Calcul dans le repere de l'embase : X = canons au repos, Z = axe de lacet.
	const FTransform BaseXf = T.Base->GetComponentTransform();
	const FVector Local = BaseXf.InverseTransformVectorNoScale(WorldTarget - T.Pitch->GetComponentLocation());
	const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Local.Y, Local.X));
	const float Pitch = FMath::RadiansToDegrees(FMath::Atan2(Local.Z, FVector2D(Local.X, Local.Y).Size()));

	const FHyperionTurretLimits& L = LimitsFor(T.Type);
	T.bTargetInArc = Yaw >= L.YawMin && Yaw <= L.YawMax && Pitch >= L.PitchMin && Pitch <= L.PitchMax;
	T.TargetYaw = FMath::Clamp(Yaw, L.YawMin, L.YawMax);
	T.TargetPitch = FMath::Clamp(Pitch, L.PitchMin, L.PitchMax);
}

void AHyperionShip::AimAllTurrets(FVector WorldTarget)
{
	for (FHyperionTurret& T : Turrets)
	{
		AimTurret(T, WorldTarget);
	}
}

void AHyperionShip::AimGroup(EHyperionWeapon Group, FVector WorldTarget)
{
	for (FHyperionTurret& T : Turrets)
	{
		if (T.Type == Group)
		{
			AimTurret(T, WorldTarget);
		}
	}
}

void AHyperionShip::StowTurrets()
{
	for (FHyperionTurret& T : Turrets)
	{
		T.TargetYaw = 0.f;
		T.TargetPitch = 0.f;
		T.bTargetInArc = false;
	}
}

void AHyperionShip::GetMuzzleTransforms(const FHyperionTurret& T, TArray<FTransform>& Out) const
{
	// Bouches des canons dans le repere de la piece "Site" (cm), d'apres la maquette.
	TArray<FVector, TInlineAllocator<5>> Offsets;
	switch (T.Type)
	{
	case EHyperionWeapon::Heavy:
		Offsets = { FVector(3400.f, 325.f, 0.f), FVector(3400.f, -325.f, 0.f) };
		break;
	case EHyperionWeapon::Main:
		Offsets = { FVector(9700.f, -2000.f, 0.f), FVector(9700.f, -1000.f, 0.f), FVector(9700.f, 0.f, 0.f),
		            FVector(9700.f, 1000.f, 0.f), FVector(9700.f, 2000.f, 0.f) };
		break;
	case EHyperionWeapon::Laser:
		Offsets = { FVector(2200.f, 0.f, 0.f) };
		break;
	default:
		break;
	}
	const FTransform SiteXf = T.Pitch->GetComponentTransform();
	for (const FVector& O : Offsets)
	{
		Out.Add(FTransform(SiteXf.GetRotation(), SiteXf.TransformPosition(O)));
	}
}

void AHyperionShip::SpawnShot(EHyperionWeapon Group, const FTransform& Muzzle)
{
	if (const TSubclassOf<AActor>* Cls = ProjectileClasses.Find(Group))
	{
		if (*Cls && GetWorld())
		{
			FActorSpawnParameters Params;
			Params.Owner = this;
			Params.Instigator = GetInstigator();
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			GetWorld()->SpawnActor<AActor>(*Cls, Muzzle.GetLocation(), Muzzle.Rotator(), Params);
		}
	}
	OnWeaponFired.Broadcast(Group, Muzzle);
}

int32 AHyperionShip::FireWeapon(EHyperionWeapon Group, FVector WorldTarget)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}
	const float Now = World->GetTimeSeconds();
	if (const float* Next = NextFireTime.Find(Group))
	{
		if (Now < *Next)
		{
			return 0;
		}
	}

	int32 Shots = 0;
	TArray<FTransform> Muzzles;

	switch (Group)
	{
	case EHyperionWeapon::Main:
	case EHyperionWeapon::Heavy:
	case EHyperionWeapon::Laser:
		for (FHyperionTurret& T : Turrets)
		{
			if (T.Type != Group)
			{
				continue;
			}
			AimTurret(T, WorldTarget);
			const bool bOnTarget = T.bTargetInArc
				&& FMath::Abs(FMath::FindDeltaAngleDegrees(T.CurrentYaw, T.TargetYaw)) <= AimTolerance
				&& FMath::Abs(T.CurrentPitch - T.TargetPitch) <= AimTolerance;
			if (bOnTarget)
			{
				GetMuzzleTransforms(T, Muzzles);
			}
		}
		break;

	case EHyperionWeapon::Spinal:
		if (const USceneComponent* Spinal = FindPart(FName(HyperionNames::Spinal)))
		{
			// Arme fixe : tire dans l'axe du vaisseau, on vise avec la coque.
			Muzzles.Add(FTransform(FRotationMatrix::MakeFromX(GetShipForward()).ToQuat(), Spinal->GetComponentLocation()));
		}
		break;

	case EHyperionWeapon::VLS:
		for (int32 i = 1; i <= 4; ++i)
		{
			if (const USceneComponent* Cell = FindPart(FName(*FString::Printf(TEXT("VLS_%02d"), i))))
			{
				const FVector Up = Cell->GetUpVector();
				Muzzles.Add(FTransform(FRotationMatrix::MakeFromX(Up).ToQuat(), Cell->GetComponentLocation() + Up * HyperionNames::VLSHatch));
			}
		}
		break;

	case EHyperionWeapon::CIWS:
		for (int32 i = 1; i <= 23; ++i)
		{
			if (const USceneComponent* Mount = FindPart(FName(*FString::Printf(TEXT("CIWS_%02d"), i))))
			{
				const FVector Up = Mount->GetUpVector();
				const FVector Head = Mount->GetComponentLocation() + Up * HyperionNames::CIWSHead;
				const FVector Dir = (WorldTarget - Head).GetSafeNormal();
				if ((Dir | Up) > 0.f)  // seulement si la cible est dans l'hemisphere de l'affut
				{
					Muzzles.Add(FTransform(FRotationMatrix::MakeFromX(Dir).ToQuat(), Head));
				}
			}
		}
		break;
	}

	for (const FTransform& M : Muzzles)
	{
		SpawnShot(Group, M);
		++Shots;
	}
	if (Shots > 0)
	{
		const float* Interval = FireIntervals.Find(Group);
		NextFireTime.Add(Group, Now + (Interval ? *Interval : 1.f));
	}
	return Shots;
}

// ===================================================================== Hangars / antenne

void AHyperionShip::SetHangarDoorsOpen(bool bOpen)
{
	DoorTarget = bOpen ? 1.f : 0.f;
}

void AHyperionShip::SetDishTarget(FVector WorldTarget)
{
	bDishHasTarget = true;
	DishTarget = WorldTarget;
}

void AHyperionShip::ClearDishTarget()
{
	bDishHasTarget = false;
}
