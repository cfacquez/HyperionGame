#include "HyperionRCSComponent.h"

#include "Components/ChildActorComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "HyperionShipActor.h"
#include "SolarGravityComponent.h"
#include "SolarSystemSimulator.h"

DEFINE_LOG_CATEGORY_STATIC(LogHyperionRCS, Log, All);

namespace
{
	// Noms des pieces dans la maquette (voir AHyperionShip).
	const TCHAR* const RCSThrusters[] = {
		TEXT("SM_Propulseur_Manoeuvre_H"), TEXT("SM_Propulseur_Manoeuvre_B"),
		TEXT("SM_Propulseur_Manoeuvre_G"), TEXT("SM_Propulseur_Manoeuvre_D"),
		TEXT("SM_Propulseur_Manoeuvre_AR_H1"), TEXT("SM_Propulseur_Manoeuvre_AR_H2"),
		TEXT("SM_Propulseur_Manoeuvre_AR_B1"), TEXT("SM_Propulseur_Manoeuvre_AR_B2"),
		TEXT("SM_Propulseur_Manoeuvre_AR_G1"), TEXT("SM_Propulseur_Manoeuvre_AR_G2"),
		TEXT("SM_Propulseur_Manoeuvre_AR_D1"), TEXT("SM_Propulseur_Manoeuvre_AR_D2"),
		// Section 3 : faces avant, tuyeres vers l'avant (poussee vers l'arriere).
		TEXT("SM_Propulseur_Manoeuvre_S3_G1"), TEXT("SM_Propulseur_Manoeuvre_S3_G2"),
		TEXT("SM_Propulseur_Manoeuvre_S3_G3"), TEXT("SM_Propulseur_Manoeuvre_S3_G4"),
		TEXT("SM_Propulseur_Manoeuvre_S3_D1"), TEXT("SM_Propulseur_Manoeuvre_S3_D2"),
		TEXT("SM_Propulseur_Manoeuvre_S3_D3"), TEXT("SM_Propulseur_Manoeuvre_S3_D4") };

	// Groupes de propulseurs par manoeuvre (memes que AHyperionShip::PerformManeuver ; roulis : voir FireRoll).
	const TCHAR* const PitchUpSet[]   = { TEXT("SM_Propulseur_Manoeuvre_B"), TEXT("SM_Propulseur_Manoeuvre_AR_H1"), TEXT("SM_Propulseur_Manoeuvre_AR_H2") };
	const TCHAR* const PitchDownSet[] = { TEXT("SM_Propulseur_Manoeuvre_H"), TEXT("SM_Propulseur_Manoeuvre_AR_B1"), TEXT("SM_Propulseur_Manoeuvre_AR_B2") };
	const TCHAR* const YawLeftSet[]   = { TEXT("SM_Propulseur_Manoeuvre_D"), TEXT("SM_Propulseur_Manoeuvre_AR_G1"), TEXT("SM_Propulseur_Manoeuvre_AR_G2") };
	const TCHAR* const YawRightSet[]  = { TEXT("SM_Propulseur_Manoeuvre_G"), TEXT("SM_Propulseur_Manoeuvre_AR_D1"), TEXT("SM_Propulseur_Manoeuvre_AR_D2") };
	// Repere de la maquette : +X a gauche, Z vers le haut. AR_x1 a gauche (ou en haut pour G/D), AR_x2 a droite (ou en bas).
	// Gauche : le flanc gauche descend (H1 pousse vers le bas a gauche, B2 vers le haut a droite,
	// D1 pousse le haut vers la gauche, G2 le bas vers la droite). Couple pur, sans poussee nette.
	const TCHAR* const RollLeftSet[]  = { TEXT("SM_Propulseur_Manoeuvre_AR_H1"), TEXT("SM_Propulseur_Manoeuvre_AR_B2"), TEXT("SM_Propulseur_Manoeuvre_AR_D1"), TEXT("SM_Propulseur_Manoeuvre_AR_G2") };
	const TCHAR* const RollRightSet[] = { TEXT("SM_Propulseur_Manoeuvre_AR_H2"), TEXT("SM_Propulseur_Manoeuvre_AR_B1"), TEXT("SM_Propulseur_Manoeuvre_AR_G1"), TEXT("SM_Propulseur_Manoeuvre_AR_D2") };
}

UHyperionRCSComponent::UHyperionRCSComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

AHyperionShip* UHyperionRCSComponent::GetShip() const
{
	if (!CachedShip.IsValid() && GetOwner())
	{
		TInlineComponentArray<UChildActorComponent*> Children(GetOwner());
		for (UChildActorComponent* C : Children)
		{
			if (AHyperionShip* Ship = Cast<AHyperionShip>(C->GetChildActor()))
			{
				CachedShip = Ship;
				break;
			}
		}
	}
	return CachedShip.Get();
}

void UHyperionRCSComponent::FireRoll(bool bRight, float Duration)
{
	AHyperionShip* Ship = GetShip();
	if (!Ship)
	{
		return;
	}
	for (const TCHAR* Name : (bRight ? RollRightSet : RollLeftSet))
	{
		Ship->FireRCS(FName(Name), Duration);
	}
}

void UHyperionRCSComponent::SetAuxThrottleSetting(float Throttle)
{
	// Arrondi au pas pour eviter l'accumulation d'erreurs (1 %, 2 %, ... exactement).
	const float Step = FMath::Max(AuxThrottleStep, 0.001f);
	AuxThrottleSetting = FMath::Clamp(FMath::RoundToFloat(Throttle / Step) * Step, 0.f, 1.f);
	ApplyAuxThrottles();
}

void UHyperionRCSComponent::ApplyAuxThrottles()
{
	AHyperionShip* Ship = GetShip();
	if (!Ship)
	{
		return;
	}
	// Moteurs du vaisseau : 0-1 principaux, puis A1..A6 aux index 2..7.
	for (int32 AuxNumber = 1; AuxNumber <= 6; ++AuxNumber)
	{
		float Throttle = AuxThrottleSetting;
		if (bAuxLowThrustHeld && AuxLowThrustEngines.Contains(AuxNumber))
		{
			Throttle = FMath::Max(Throttle, AuxLowThrustLevel);
		}
		Ship->SetEngineThrottle(AuxNumber + 1, Throttle);
	}
}

void UHyperionRCSComponent::FireSection3(float Duration)
{
	AHyperionShip* Ship = GetShip();
	if (!Ship)
	{
		return;
	}
	static const TCHAR* const Section3[] = {
		TEXT("SM_Propulseur_Manoeuvre_S3_G1"), TEXT("SM_Propulseur_Manoeuvre_S3_G2"),
		TEXT("SM_Propulseur_Manoeuvre_S3_G3"), TEXT("SM_Propulseur_Manoeuvre_S3_G4"),
		TEXT("SM_Propulseur_Manoeuvre_S3_D1"), TEXT("SM_Propulseur_Manoeuvre_S3_D2"),
		TEXT("SM_Propulseur_Manoeuvre_S3_D3"), TEXT("SM_Propulseur_Manoeuvre_S3_D4") };
	for (const TCHAR* Name : Section3)
	{
		Ship->FireRCS(FName(Name), Duration);
	}
}

// ===================================================================== KILL ROT

FVector UHyperionRCSComponent::ThrusterTorque(const AHyperionShip* Ship, TArrayView<const TCHAR* const> Names) const
{
	const ASolarSystemSimulator* S = ASolarSystemSimulator::Get(this);
	const AActor* Owner = GetOwner();
	if (!Ship || !S || !Owner)
	{
		return FVector::ZeroVector;
	}
	FVector Torque = FVector::ZeroVector;
	for (const TCHAR* Name : Names)
	{
		if (const USceneComponent* Part = Ship->FindPart(FName(Name)))
		{
			// Meme calcul que USolarGravityComponent::AddForceAtLocation.
			const FVector Arm = (Part->GetComponentLocation() - Owner->GetActorLocation()) * S->RealMetersPerUnrealCm;
			Torque += Arm ^ (-Part->GetUpVector() * ThrustPerThrusterNewtons);
		}
	}
	return Torque;
}

void UHyperionRCSComponent::ToggleKillRotation()
{
	SetKillRotation(!bKillRotation);
}

void UHyperionRCSComponent::SetKillRotation(bool bActive)
{
	if (bActive)
	{
		CancelAutopilot();  // KILL ROT est une autre action : fin de PRO / RET
	}
	if (bKillRotation != bActive)
	{
		bKillRotation = bActive;
		UE_LOG(LogHyperionRCS, Log, TEXT("KILL ROT %s."), bActive ? TEXT("active") : TEXT("interrompu"));
	}
}

namespace
{
	const TCHAR* OrientName(uint8 Target)
	{
		switch (Target)
		{
		case 1:  return TEXT("RET");
		case 2:  return TEXT("ORBIT+");
		case 3:  return TEXT("ORBIT-");
		default: return TEXT("PRO");
		}
	}
}

void UHyperionRCSComponent::SetOrientation(bool bActive, uint8 Target)
{
	if (bActive && bKillRotation)
	{
		bKillRotation = false;
		UE_LOG(LogHyperionRCS, Log, TEXT("KILL ROT interrompu par %s."), OrientName(Target));
	}
	if (bPrograde != bActive || (bActive && OrientTarget != Target))
	{
		if (bPrograde && !bActive)
		{
			UE_LOG(LogHyperionRCS, Log, TEXT("%s desactive."), OrientName(OrientTarget));
		}
		bPrograde = bActive;
		OrientTarget = bActive ? Target : OrientTarget;
		// Nouvelle cible : on repart de zero (filtre, maintien, phase de pilotage).
		PrevProgradeDir = FVector::ZeroVector;
		ProgradeDirRate = FVector::ZeroVector;
		bProgradeBigCorrection = true;
		bProgradeHolding = false;
		bRollDampPhase = bActive;  // d'abord annuler le roulis, ensuite seulement orienter le nez
		bRollHolding = false;
		bRollAligned = false;
		if (bActive)
		{
			UE_LOG(LogHyperionRCS, Log, TEXT("%s active."), OrientName(Target));
		}
	}
}

void UHyperionRCSComponent::SetPrograde(bool bActive)
{
	if (bActive || IsProgradeActive())
	{
		SetOrientation(bActive, 0);
	}
}

void UHyperionRCSComponent::SetRetrograde(bool bActive)
{
	if (bActive || IsRetrogradeActive())
	{
		SetOrientation(bActive, 1);
	}
}

void UHyperionRCSComponent::SetOrbitNormal(bool bActive)
{
	if (bActive || IsOrbitNormalActive())
	{
		SetOrientation(bActive, 2);
	}
}

void UHyperionRCSComponent::SetOrbitAntiNormal(bool bActive)
{
	if (bActive || IsOrbitAntiNormalActive())
	{
		SetOrientation(bActive, 3);
	}
}

void UHyperionRCSComponent::CancelAutopilot()
{
	SetOrientation(false, OrientTarget);
}

FVector UHyperionRCSComponent::GetNoseDirection(const AHyperionShip* Ship) const
{
	// Maquette : proue selon +Y local du vaisseau (propulseurs avant a Y = +59 500).
	return Ship ? Ship->GetActorQuat().RotateVector(FVector(0.0, 1.0, 0.0)).GetSafeNormal() : FVector::ForwardVector;
}

bool UHyperionRCSComponent::TickKillRotation(AHyperionShip* Ship, float DeltaTime)
{
	USolarGravityComponent* G = Gravity.Get();
	if (!G || !TickAttitude(Ship, DeltaTime, FVector::ZeroVector))
	{
		// Reliquat plus petit que la correction d'une seule image de poussee : arret.
		if (G)
		{
			G->ResetAngularVelocity();
		}
		return true;
	}
	return false;
}

void UHyperionRCSComponent::TickPrograde(AHyperionShip* Ship, float DeltaTime)
{
	USolarGravityComponent* G = Gravity.Get();
	if (!G || !Ship)
	{
		return;
	}
	// Phase 1 : annuler la vitesse de roulis (autour du nez) avant la manoeuvre. Tangage et lacet sont
	// laisses tels quels (la cible reprend leur valeur actuelle) ; seule la paire de roulis travaille.
	if (bRollDampPhase)
	{
		const FVector Nose = GetNoseDirection(Ship);
		const FVector W = G->AngularVelocity;
		const FVector TargetNoRoll = W - Nose * (W | Nose);
		bProgradeBigCorrection = true;  // poussee a x1
		if (!TickAttitude(Ship, DeltaTime, TargetNoRoll))
		{
			bRollDampPhase = false;
			UE_LOG(LogHyperionRCS, Log, TEXT("%s : roulis annule (%.5f deg/s restant), debut de la manoeuvre."),
				OrientTarget == 1 ? TEXT("RET") : OrientTarget == 2 ? TEXT("ORBIT+") : OrientTarget == 3 ? TEXT("ORBIT-") : TEXT("PRO"),
				FMath::RadiansToDegrees(G->AngularVelocity | Nose));
		}
		return;
	}

	const FName Body = ProgradeReferenceBody.IsNone() ? G->ReferenceBody : ProgradeReferenceBody;
	const FVector V = G->GetVelocityRelativeTo(Body);  // m/s, direction monde
	// Direction visee (repere monde) :
	//  PRO : sens de la vitesse ; RET : a l'oppose ;
	//  ORBIT+ : normale au plan de l'orbite, sens du moment cinetique r x v (calcule dans le repere reel,
	//  direct, puis converti : le passage au repere Unreal inverse Y, un produit vectoriel fait en monde
	//  donnerait le sens oppose) ; ORBIT- : a l'oppose de cette normale.
	FVector T = FVector::ZeroVector;
	if (OrientTarget == 2 || OrientTarget == 3)
	{
		if (const ASolarSystemSimulator* SimNorm = ASolarSystemSimulator::Get(this))
		{
			const int32 Idx = SimNorm->FindBodyIndex(Body);
			const double Now = SimNorm->GetSimTimeSeconds();
			const FVector R = G->RealPosition - (Idx != INDEX_NONE ? SimNorm->BodyRealPosition(Idx, Now) : FVector::ZeroVector);
			const FVector Vr = G->RealVelocity - (Idx != INDEX_NONE ? SimNorm->BodyRealVelocity(Idx, Now) : FVector::ZeroVector);
			T = SimNorm->RealDirToWorld(R ^ Vr).GetSafeNormal() * (OrientTarget == 3 ? -1.0 : 1.0);
		}
	}
	else if (V.SizeSquared() > 1.0)
	{
		T = V.GetSafeNormal() * (OrientTarget == 1 ? -1.0 : 1.0);
	}
	FVector TargetW = FVector::ZeroVector;
	if (!T.IsNearlyZero())
	{
		const FVector F = GetNoseDirection(Ship);
		FVector Axis = F ^ T;
		const double SinA = Axis.Size();
		const double CosA = F | T;
		const double Angle = FMath::Atan2(SinA, CosA);
		ProgradeErrorDeg = static_cast<float>(FMath::RadiansToDegrees(Angle));
		if (SinA < 1e-6)
		{
			// Aligne (rien a faire) ou exactement a l'oppose : demi-tour autour de l'axe haut du vaisseau.
			Axis = CosA > 0.0 ? FVector::ZeroVector : Ship->GetActorUpVector();
		}
		else
		{
			Axis /= SinA;
		}
		// Profil "accelerer - filer - freiner" : la vitesse de rotation visee est celle qui permet encore de
		// s'arreter sur la cible avec l'acceleration angulaire disponible (avec une marge).
		const double MaxRate = FMath::DegreesToRadians(static_cast<double>(AutopilotMaxRateDegPerSec));
		// Sans ralentissement du temps, une image de poussee ne dure qu'une image reelle alors que le temps simule
		// avance de TimeScale fois plus : l'acceleration angulaire efficace (par seconde simulee) est reduite d'autant.
		double EffAlpha = AutopilotAlpha;
		if (!bManeuverThrustSlowsTime)
		{
			if (const ASolarSystemSimulator* SimA = ASolarSystemSimulator::Get(this))
			{
				EffAlpha *= FMath::Clamp(SimA->ThrustTimeScale / FMath::Max(SimA->TimeScale, 1e-3), 0.0, 1.0);
			}
		}
		const double BrakeRate = FMath::Sqrt(2.0 * EffAlpha * AutopilotBrakingMargin * Angle);
		// Sans poussee le temps repasse en accelere : une seule image fait alors tourner le vaisseau de
		// |W| * DeltaTime * WarpTimeScale. On limite la vitesse pour qu'une image ne parcoure qu'une fraction
		// de l'ecart restant : approche progressive, sans depassement, quelle que soit la cadence d'affichage.
		const ASolarSystemSimulator* S = ASolarSystemSimulator::Get(this);
		const double WarpScale = S ? FMath::Max(S->WarpTimeScale, 1.0) : 1.0;
		SmoothedDeltaTime = FMath::Lerp(SmoothedDeltaTime, DeltaTime, 0.1f);
		const double StepRate = Angle / (FMath::Max<double>(SmoothedDeltaTime, 1e-3) * WarpScale * 2.0);

		// Maintien (avec hysteresis) : une fois a moins de la moitie de la tolerance, on ne vise plus que la
		// rotation de la direction prograde, avec une tres douce correction de l'ecart (constante de 10 min
		// simulees). On ne repasse en pilotage complet que si l'ecart depasse la tolerance.
		const double HoldTol = FMath::DegreesToRadians(static_cast<double>(AutopilotHoldToleranceDeg));
		if (bProgradeHolding && Angle > HoldTol)
		{
			bProgradeHolding = false;
		}
		else if (!bProgradeHolding && Angle < HoldTol * 0.5)
		{
			bProgradeHolding = true;
		}
		// En temps accelere, inutile de tourner vite : une vitesse qui parcourt l'ecart en AutopilotCoastSeconds
		// reelles suffit, et elle s'atteint en tres peu de poussee (donc peu de temps passe a x1).
		// Plancher : nettement au-dessus de la rotation propre de la direction visee (elle tourne avec l'orbite),
		// sinon les deux se compensent et le vaisseau ne rattrape jamais la cible.
		const double CoastRate = FMath::Max(Angle / (FMath::Max(static_cast<double>(AutopilotCoastSeconds), 0.1) * WarpScale),
			3.0 * ProgradeDirRate.Size());
		const double Rate = bProgradeHolding ? Angle / 600.0
			: FMath::Min(FMath::Min3(MaxRate, BrakeRate, StepRate), FMath::Max(CoastRate, 1e-6));
		TargetW = Axis * Rate;

		// La direction prograde tourne elle-meme (le vaisseau orbite) : on ajoute sa vitesse de rotation
		// pour la suivre sans retard une fois aligne.
		// Filtree (constante de temps ~1 min simulee) : la mesure image par image est bruitee.
		const double SimT = S ? S->GetSimTimeSeconds() : 0.0;
		if (!PrevProgradeDir.IsNearlyZero() && SimT > PrevProgradeSimTime)
		{
			const double SimDt = SimT - PrevProgradeSimTime;
			const FVector Raw = (PrevProgradeDir ^ T) / SimDt;
			const double K = 1.0 - FMath::Exp(-SimDt / 60.0);
			ProgradeDirRate = ProgradeDirRate + (Raw - ProgradeDirRate) * K;
		}
		PrevProgradeDir = T;
		PrevProgradeSimTime = SimT;
		TargetW += ProgradeDirRate;

		// PRO / RET : une fois le nez (presque) aligne, roulis autour du nez pour tourner le dessus du vaisseau
		// (cote passerelle, +Z local) vers l'astre de reference.
		bool bRollDone = true;
		if ((OrientTarget == 0 || OrientTarget == 1) && S && (bProgradeHolding || Angle < FMath::DegreesToRadians(5.0)))
		{
			const int32 BodyIdx = S->FindBodyIndex(Body);
			const FVector BodyPos = BodyIdx != INDEX_NONE ? S->BodyRealPosition(BodyIdx, S->GetSimTimeSeconds()) : FVector::ZeroVector;
			const FVector Toward = S->RealDirToWorld(BodyPos - G->RealPosition).GetSafeNormal();
			const FVector UpTarget = (Toward - F * (Toward | F)).GetSafeNormal();
			if (!UpTarget.IsNearlyZero())
			{
				const FVector Up = Ship->GetActorUpVector();
				// Angle signe autour du nez : > 0, une rotation positive autour de F amene le dessus vers la cible.
				const double RollErr = FMath::Atan2((Up ^ UpTarget) | F, Up | UpTarget);
				const double AbsErr = FMath::Abs(RollErr);
				RollErrorDeg = static_cast<float>(FMath::RadiansToDegrees(RollErr));

				// Acceleration de roulis disponible (couple de la paire de roulis, inertie du vaisseau).
				double RollAlpha = 0.0;
				if (const USolarGravityComponent* GI = Gravity.Get())
				{
					const FVector TR = ThrusterTorque(Ship, MakeArrayView(RollLeftSet));
					const double TSize = TR.Size();
					if (TSize > 1.0)
					{
						const FQuat Q = GetOwner()->GetActorQuat();
						const FVector I(FMath::Max(GI->InertiaKgM2.X, 1.0), FMath::Max(GI->InertiaKgM2.Y, 1.0), FMath::Max(GI->InertiaKgM2.Z, 1.0));
						RollAlpha = FMath::Abs(Q.RotateVector(Q.UnrotateVector(TR) / I) | (TR / TSize));
					}
				}
				const double EffScale = EffAlpha / FMath::Max(AutopilotAlpha, 1e-12);  // meme reduction en temps accelere
				const double RollBrake = FMath::Sqrt(2.0 * RollAlpha * EffScale * AutopilotBrakingMargin * AbsErr);
				const double RollStep = AbsErr / (FMath::Max<double>(SmoothedDeltaTime, 1e-3) * WarpScale * 2.0);
				const double RollCoast = AbsErr / (FMath::Max(static_cast<double>(AutopilotCoastSeconds), 0.1) * WarpScale);

				// Hysteresis comme pour le nez : maintien doux sous la demi-tolerance.
				if (bRollHolding && AbsErr > HoldTol)
				{
					bRollHolding = false;
				}
				else if (!bRollHolding && AbsErr < HoldTol * 0.5)
				{
					bRollHolding = true;
				}
				const double RollRate = bRollHolding ? AbsErr / 600.0
					: FMath::Min(FMath::Min3(MaxRate, RollBrake, RollStep), FMath::Max(RollCoast, 1e-6));
				TargetW += F * (RollErr >= 0.0 ? RollRate : -RollRate);
				bRollDone = bRollHolding;
			}
		}
		else
		{
			bRollHolding = false;
			bRollDone = !(OrientTarget == 0 || OrientTarget == 1);
		}
		bRollAligned = bRollDone;
	}
	// Grosse correction (accelerer, freiner) : temps a x1. Simple entretien de la vitesse de rotation :
	// les impulsions agissent sans ralentir le temps.
	const double RateErr = (G->AngularVelocity - TargetW).Size();
	bProgradeBigCorrection = RateErr > 0.25 * FMath::Max(TargetW.Size(),
		FMath::DegreesToRadians(static_cast<double>(AutopilotHoldRateToleranceDegPerSec)) * 4.0);
	// Une fois aligne, on tolere un petit ecart de vitesse de rotation : chaque correction ramene le temps a x1,
	// il ne faut donc pas corriger en permanence des derives imperceptibles.
	// En pleine rotation, 5 % d'ecart sur la vitesse visee suffisent : le vaisseau file alors sans poussee
	// et le temps peut repasser en accelere.
	double ExtraDeadband = TargetW.Size() * 0.05;
	if (bProgradeHolding)
	{
		ExtraDeadband = FMath::Max(ExtraDeadband, FMath::DegreesToRadians(static_cast<double>(AutopilotHoldRateToleranceDegPerSec)));
	}
	double MinAlpha = 0.0;
	// Le roulis n'est corrige que lorsque tangage et lacet n'ont rien a faire : les propulseurs arriere sont
	// partages entre lacet et roulis, les allumer ensemble affaiblirait le lacet.
	TickAttitude(Ship, DeltaTime, TargetW, &MinAlpha, ExtraDeadband, /*bRollOnlyWhenIdle*/ true);
	if (MinAlpha > 0.0)
	{
		AutopilotAlpha = MinAlpha;
	}
}

bool UHyperionRCSComponent::TickAttitude(AHyperionShip* Ship, float DeltaTime, const FVector& TargetW, double* MinAlpha, double ExtraDeadband, bool bRollOnlyWhenIdle)
{
	USolarGravityComponent* G = Gravity.Get();
	const ASolarSystemSimulator* S = ASolarSystemSimulator::Get(this);
	if (!G || !S || !GetOwner())
	{
		return false;
	}

	struct FAxisPair { TArrayView<const TCHAR* const> A; TArrayView<const TCHAR* const> B; };
	const FAxisPair Pairs[] = {
		{ MakeArrayView(PitchUpSet), MakeArrayView(PitchDownSet) },
		{ MakeArrayView(YawLeftSet), MakeArrayView(YawRightSet) },
		{ MakeArrayView(RollLeftSet), MakeArrayView(RollRightSet) } };

	const FVector W = G->AngularVelocity;  // rad/s, repere monde
	const FQuat Q = GetOwner()->GetActorQuat();
	const FVector I(FMath::Max(G->InertiaKgM2.X, 1.0), FMath::Max(G->InertiaKgM2.Y, 1.0), FMath::Max(G->InertiaKgM2.Z, 1.0));
	// Pendant une poussee le temps passe a ThrustTimeScale : une image de poussee dure DeltaTime * ThrustTimeScale.
	const double BurnDt = static_cast<double>(DeltaTime) * FMath::Max(S->ThrustTimeScale, 0.0001);
	// Entretenu a chaque image ; le vaisseau (qui tique apres nous) l'eteint une image apres la derniere commande.
	const float Pulse = DeltaTime * 1.5f;

	bool bFired = false;
	double LowestAlpha = 0.0;
	for (int32 PairIdx = 0; PairIdx < UE_ARRAY_COUNT(Pairs); ++PairIdx)
	{
		const FAxisPair& P = Pairs[PairIdx];
		if (PairIdx == 2 && bRollOnlyWhenIdle && bFired)
		{
			continue;
		}
		const FVector TA = ThrusterTorque(Ship, P.A);
		const double TSize = TA.Size();
		if (TSize < 1.0)
		{
			continue;
		}
		const FVector N = TA / TSize;
		// Acceleration angulaire produite le long de N (inertie dans le repere du vaisseau).
		const FVector AlphaWorld = Q.RotateVector(Q.UnrotateVector(TA) / I);
		const double AlphaN = FMath::Abs(AlphaWorld | N);
		if (PairIdx < 2)  // tangage et lacet : ce sont eux qui orientent le nez
		{
			LowestAlpha = LowestAlpha > 0.0 ? FMath::Min(LowestAlpha, AlphaN) : AlphaN;
		}
		const double Deadband = AlphaN * BurnDt * KillRotDeadbandFrames + ExtraDeadband;
		const double C = (W - TargetW) | N;  // ecart de rotation le long de l'axe de cette paire
		if (FMath::Abs(C) <= Deadband)
		{
			continue;
		}
		// C < 0 : le couple de A (selon +N) ramene vers la cible ; C > 0 : la manoeuvre opposee.
		for (const TCHAR* Name : (C < 0.0 ? P.A : P.B))
		{
			Ship->FireRCS(FName(Name), Pulse);
		}
		bFired = true;
		AutopilotPairFrames[PairIdx] += 1;
	}
	if (MinAlpha)
	{
		*MinAlpha = LowestAlpha;
	}
	return bFired;
}

// ===================================================================== Cycle de vie

void UHyperionRCSComponent::BeginPlay()
{
	Super::BeginPlay();
	if (AActor* Owner = GetOwner())
	{
		if (USolarGravityComponent* G = Owner->FindComponentByClass<USolarGravityComponent>())
		{
			Gravity = G;
			// Les poussees de l'image sont posees avant que la gravite n'integre le mouvement.
			G->AddTickPrerequisiteComponent(this);
		}
		else
		{
			UE_LOG(LogHyperionRCS, Warning, TEXT("%s : pas de SolarGravityComponent, les propulseurs ne poussent pas."), *Owner->GetName());
		}
	}
}

void UHyperionRCSComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AHyperionShip* Ship = GetShip();
	if (!Ship)
	{
		return;
	}
	if (!bShipTickOrdered)
	{
		// Le vaisseau decompte ses allumages apres nos commandes : une impulsion entretenue ne clignote pas.
		Ship->AddTickPrerequisiteComponent(this);
		bShipTickOrdered = true;
	}

	Elapsed += DeltaTime;
	if (bDebugManeuverAtStart && !bDebugFired && Elapsed >= DebugDelay)
	{
		bDebugFired = true;
		if (!DebugSpinDegPerSec.IsNearlyZero())
		{
			if (USolarGravityComponent* G = Gravity.Get())
			{
				G->AngularVelocity = GetOwner()->GetActorQuat().RotateVector(DebugSpinDegPerSec * (PI / 180.0));
			}
			bDebugKillRotPending = true;
			UE_LOG(LogHyperionRCS, Log, TEXT("Verification : rotation %s deg/s, KILL ROT dans %.1f s."),
				*DebugSpinDegPerSec.ToString(), DebugKillRotDelay);
		}
		else if (DebugRoll != 0)
		{
			FireRoll(DebugRoll > 0, DebugDuration);
			UE_LOG(LogHyperionRCS, Log, TEXT("Manoeuvre de verification : roulis a %s pendant %.1f s."),
				DebugRoll > 0 ? TEXT("droite") : TEXT("gauche"), DebugDuration);
		}
		else
		{
			Ship->PerformManeuver(DebugManeuver, DebugDuration);
			UE_LOG(LogHyperionRCS, Log, TEXT("Manoeuvre de verification : %s pendant %.1f s."),
				*UEnum::GetValueAsString(DebugManeuver), DebugDuration);
		}
	}
	if ((bDebugProgradeAtStart || bDebugRetrogradeAtStart || bDebugOrbitNormalAtStart || bDebugOrbitAntiNormalAtStart)
		&& !bDebugProgradeFired && Elapsed >= DebugDelay)
	{
		bDebugProgradeFired = true;
		SetOrientation(true, bDebugOrbitAntiNormalAtStart ? 3 : bDebugOrbitNormalAtStart ? 2 : (bDebugRetrogradeAtStart ? 1 : 0));
	}
	if (bDebugAuxBurn)
	{
		const bool bBurn = Elapsed >= DebugDelay && Elapsed < DebugDelay + DebugDuration;
		const float Wanted = bBurn ? DebugAuxLevel : 0.f;
		if (Elapsed >= DebugDelay && !FMath::IsNearlyEqual(AuxThrottleSetting, Wanted) && (bBurn || !bDebugAuxBurnDone))
		{
			SetAuxThrottleSetting(Wanted);
			bDebugAuxBurnDone = !bBurn;
		}
	}
	if (bDebugKillRotPending && Elapsed >= DebugDelay + DebugKillRotDelay)
	{
		bDebugKillRotPending = false;
		SetKillRotation(true);
	}

	// Pilotage clavier (lacet Q / D, tangage Z / S, roulis A / E) : propulseurs allumes tant que la touche est tenue.
	bool bManual = false;
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		// Action mappings (Project Settings > Input) : une action est active si l'une de ses touches est enfoncee.
		// Les modificateurs du mapping (Ctrl, Maj, Alt, Cmd) doivent correspondre exactement : "+" seul et
		// "Ctrl +" sont deux actions differentes.
		const UPlayerInput* Input = PC->PlayerInput;
		const bool bCtrl = PC->IsInputKeyDown(EKeys::LeftControl) || PC->IsInputKeyDown(EKeys::RightControl);
		const bool bShift = PC->IsInputKeyDown(EKeys::LeftShift) || PC->IsInputKeyDown(EKeys::RightShift);
		const bool bAlt = PC->IsInputKeyDown(EKeys::LeftAlt) || PC->IsInputKeyDown(EKeys::RightAlt);
		const bool bCmd = PC->IsInputKeyDown(EKeys::LeftCommand) || PC->IsInputKeyDown(EKeys::RightCommand);
		auto IsActionDown = [&](FName Action)
		{
			if (!Input || Action.IsNone())
			{
				return false;
			}
			for (const FInputActionKeyMapping& Mapping : Input->GetKeysForAction(Action))
			{
				if (PC->IsInputKeyDown(Mapping.Key) && Mapping.bCtrl == bCtrl && Mapping.bShift == bShift
					&& Mapping.bAlt == bAlt && Mapping.bCmd == bCmd)
				{
					return true;
				}
			}
			return false;
		};
		auto KeyPair = [&](bool bEnabled, FName ActionA, FName ActionB, TFunctionRef<void(bool)> Fire)
		{
			if (!bEnabled)
			{
				return;
			}
			const bool bA = IsActionDown(ActionA);
			const bool bB = IsActionDown(ActionB);
			if (bA != bB)  // les deux a la fois : rien
			{
				Fire(bA);
				bManual = true;
			}
		};
		KeyPair(bKeyboardYaw, YawLeftAction, YawRightAction, [&](bool bA)
			{ Ship->PerformManeuver(bA ? EHyperionManeuver::YawLeft : EHyperionManeuver::YawRight, KeyPulseSeconds); });
		KeyPair(bKeyboardPitch, PitchUpAction, PitchDownAction, [&](bool bA)
			{ Ship->PerformManeuver(bA ? EHyperionManeuver::PitchUp : EHyperionManeuver::PitchDown, KeyPulseSeconds); });
		KeyPair(bKeyboardRoll, RollLeftAction, RollRightAction, [&](bool bA) { FireRoll(!bA, KeyPulseSeconds); });

		// Section 3 : poussee vers l'arriere, sans couple ; ce n'est pas une commande de rotation (PRO etc. restent actifs).
		if (IsActionDown(S3ThrustersAction))
		{
			FireSection3(KeyPulseSeconds);
		}

		// Auxiliaires a bas regime (pave 3) tant que la touche est maintenue.
		const bool bAuxHeld = IsActionDown(AuxLowThrustAction);
		if (bAuxHeld != bAuxLowThrustHeld)
		{
			bAuxLowThrustHeld = bAuxHeld;
			ApplyAuxThrottles();
		}

		// Ctrl + / Ctrl - : directement 100 % / 0 % (a l'appui).
		const bool bMax = IsActionDown(AuxThrottleMaxAction);
		const bool bZero = IsActionDown(AuxThrottleZeroAction);
		if (bMax && !bAuxMaxHeld)
		{
			SetAuxThrottleSetting(1.f);
		}
		if (bZero && !bAuxZeroHeld)
		{
			SetAuxThrottleSetting(0.f);
		}
		bAuxMaxHeld = bMax;
		bAuxZeroHeld = bZero;

		// Regime des 6 auxiliaires : +/- un pas par pression ; touche maintenue = repetition.
		const bool bUp = IsActionDown(AuxThrottleUpAction);
		const bool bDown = IsActionDown(AuxThrottleDownAction);
		const int32 Dir = (bUp != bDown) ? (bUp ? 1 : -1) : 0;
		if (Dir != AuxRepeatDir)
		{
			AuxRepeatDir = Dir;
			AuxRepeatTimer = AuxRepeatDelay;
			if (Dir != 0)
			{
				SetAuxThrottleSetting(AuxThrottleSetting + Dir * AuxThrottleStep);  // premiere pression
			}
		}
		else if (Dir != 0)
		{
			AuxRepeatTimer -= DeltaTime;
			while (AuxRepeatTimer <= 0.f)
			{
				SetAuxThrottleSetting(AuxThrottleSetting + Dir * AuxThrottleStep);
				AuxRepeatTimer += AuxRepeatInterval;
			}
		}
	}

	// PRO : une commande manuelle de rotation est une autre action, l'autopilote rend la main.
	if (bManual && bPrograde)
	{
		CancelAutopilot();
	}

	// KILL ROT : suspendu tant que le pilote tient une touche de rotation, puis reprend jusqu'a l'arret.
	bool bKillRotJustEnded = false;
	if (bKillRotation && !bManual && TickKillRotation(Ship, DeltaTime))
	{
		bKillRotation = false;
		bKillRotJustEnded = true;
		UE_LOG(LogHyperionRCS, Log, TEXT("KILL ROT termine : rotation annulee."));
	}
	else if (bPrograde)
	{
		TickPrograde(Ship, DeltaTime);
	}

	ActiveThrusters = 0;
	USolarGravityComponent* G = Gravity.Get();
	if (G)
	{
		// Propulseurs de manoeuvre : le temps garde son acceleration (sauf si bManeuverThrustSlowsTime).
		// Chaque image de poussee agit alors pendant une image reelle (impulsion d'une seconde reelle par seconde).
		G->bThrustSlowsTime = bManeuverThrustSlowsTime;
	}
	for (const TCHAR* Name : RCSThrusters)
	{
		if (!Ship->IsThrusterFiring(FName(Name)))
		{
			continue;
		}
		++ActiveThrusters;
		const USceneComponent* Part = Ship->FindPart(FName(Name));
		// Fin de KILL ROT : la rotation vient d'etre annulee, la fin d'impulsion de l'image precedente
		// (qui s'eteint a cette image) ne doit pas la relancer.
		if (!bApplyThrust || !G || !Part || bKillRotJustEnded)
		{
			continue;
		}
		// La tuyere souffle selon +Z local : la poussee s'exerce en sens inverse, au point de montage.
		const FVector Force = -Part->GetUpVector() * ThrustPerThrusterNewtons;
		G->AddForceAtLocation(Force, Part->GetComponentLocation());
	}

	// Moteurs principaux et auxiliaires : les tuyeres soufflent vers l'arriere, la poussee pousse vers la proue,
	// appliquee a la tuyere (un moteur seul ou une combinaison asymetrique fait aussi tourner le vaisseau).
	if (bApplyEngineThrust && G)
	{
		G->bThrustSlowsTime = bEngineThrustSlowsTime;  // moteurs : temps a x1 pendant la poussee (par defaut)
		const FVector Forward = Ship->GetShipForward();
		for (int32 i = 0; i < Ship->GetEngineCount(); ++i)
		{
			const float Throttle = Ship->GetEngineThrottle(i);
			const USceneComponent* Nozzle = Ship->GetEngineNozzle(i);
			if (Throttle <= 0.f || !Nozzle)
			{
				continue;
			}
			const double MaxThrust = Ship->IsMainEngine(i) ? MainEngineMaxThrustNewtons : AuxEngineMaxThrustNewtons;
			G->AddForceAtLocation(Forward * (MaxThrust * Throttle), Nozzle->GetComponentLocation());
		}
	}
}
