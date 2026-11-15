// Mecanique orbitale du systeme solaire - voir SolarSystemSimulator.h.
// Fichier volontairement en ASCII (pas d'accents).

#include "SolarSystemSimulator.h"

#include "EngineUtils.h"
#include "Engine/World.h"
#include "GameFramework/RotatingMovementComponent.h"
#include "Math/RotationMatrix.h"

DEFINE_LOG_CATEGORY_STATIC(LogSolar, Log, All);

namespace SolarConst
{
	static constexpr double AU = 1.495978707e11;            // m
	static constexpr double SecondsPerDay = 86400.0;
	static constexpr double DaysPerCentury = 36525.0;
	static const FDateTime J2000(2000, 1, 1, 12, 0, 0);    // epoque des elements (TT ~ UTC ici)

	static double Wrap180(double Deg)
	{
		Deg = FMath::Fmod(Deg + 180.0, 360.0);
		if (Deg < 0.0) { Deg += 360.0; }
		return Deg - 180.0;
	}

	/** Resout l'equation de Kepler M = E - e sin E (radians). */
	static double SolveKepler(double M, double e)
	{
		double E = (e < 0.8) ? M : PI;
		for (int32 i = 0; i < 12; ++i)
		{
			const double dE = (E - e * FMath::Sin(E) - M) / (1.0 - e * FMath::Cos(E));
			E -= dE;
			if (FMath::Abs(dE) < 1e-12) { break; }
		}
		return E;
	}

	/** Position dans le repere de reference a partir d'elements (a en m, angles en degres). */
	static FVector OrbitToReference(double a, double e, double IDeg, double NodeDeg, double ArgPeriDeg, double MeanAnomDeg)
	{
		const double E = SolveKepler(FMath::DegreesToRadians(Wrap180(MeanAnomDeg)), e);
		const double xp = a * (FMath::Cos(E) - e);
		const double yp = a * FMath::Sqrt(1.0 - e * e) * FMath::Sin(E);

		const double I = FMath::DegreesToRadians(IDeg);
		const double O = FMath::DegreesToRadians(NodeDeg);
		const double w = FMath::DegreesToRadians(ArgPeriDeg);
		const double cw = FMath::Cos(w), sw = FMath::Sin(w);
		const double cO = FMath::Cos(O), sO = FMath::Sin(O);
		const double cI = FMath::Cos(I), sI = FMath::Sin(I);

		return FVector(
			(cw * cO - sw * sO * cI) * xp + (-sw * cO - cw * sO * cI) * yp,
			(cw * sO + sw * cO * cI) * xp + (-sw * sO + cw * cO * cI) * yp,
			(sw * sI) * xp + (cw * sI) * yp);
	}

	static FSolarKeplerElements MakeElements(double a, double e, double I, double L, double W, double N,
		double aR, double eR, double IR, double LR, double WR, double NR)
	{
		FSolarKeplerElements K;
		K.SemiMajorAxisAU = a; K.Eccentricity = e; K.InclinationDeg = I;
		K.MeanLongitudeDeg = L; K.LongPerihelionDeg = W; K.LongAscNodeDeg = N;
		K.SemiMajorAxisRate = aR; K.EccentricityRate = eR; K.InclinationRate = IR;
		K.MeanLongitudeRate = LR; K.LongPerihelionRate = WR; K.LongAscNodeRate = NR;
		return K;
	}
}

ASolarSystemSimulator::ASolarSystemSimulator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	auto Add = [this](const TCHAR* Name, double Mass, ESolarOrbitModel Model, const FSolarKeplerElements& K, bool bFace = false)
	{
		FSolarBody B;
		B.Name = FName(Name);
		B.MassKg = Mass;
		B.Model = Model;
		B.Elements = K;
		B.bFaceParentBody = bFace;
		Bodies.Add(B);
	};
	using namespace SolarConst;

	// Masses : NASA Planetary Fact Sheet. Elements : JPL "Approximate Positions of the Planets", table 1 (1800-2050).
	Add(TEXT("Sun"), 1.98847e30, ESolarOrbitModel::Fixed, FSolarKeplerElements());
	Add(TEXT("Mercury"), 3.3011e23, ESolarOrbitModel::Planet,
		MakeElements(0.38709927, 0.20563593, 7.00497902, 252.25032350, 77.45779628, 48.33076593,
		             0.00000037, 0.00001906, -0.00594749, 149472.67411175, 0.16047689, -0.12534081));
	Add(TEXT("Venus"), 4.8675e24, ESolarOrbitModel::Planet,
		MakeElements(0.72333566, 0.00677672, 3.39467605, 181.97909950, 131.60246718, 76.67984255,
		             0.00000390, -0.00004107, -0.00078890, 58517.81538729, 0.00268329, -0.27769418));
	Add(TEXT("Earth"), 5.9722e24, ESolarOrbitModel::EarthMoon,
		MakeElements(1.00000261, 0.01671123, -0.00001531, 100.46457166, 102.93768193, 0.0,
		             0.00000562, -0.00004392, -0.01294668, 35999.37244981, 0.32327364, 0.0));
	Add(TEXT("Moon"), 7.342e22, ESolarOrbitModel::Moon, FSolarKeplerElements(), true);
	Add(TEXT("Mars"), 6.4171e23, ESolarOrbitModel::Planet,
		MakeElements(1.52371034, 0.09339410, 1.84969142, -4.55343205, -23.94362959, 49.55953891,
		             0.00001847, 0.00007882, -0.00813131, 19140.30268499, 0.44441088, -0.29257343));
	Add(TEXT("Jupiter"), 1.89819e27, ESolarOrbitModel::Planet,
		MakeElements(5.20288700, 0.04838624, 1.30439695, 34.39644051, 14.72847983, 100.47390909,
		             -0.00011607, -0.00013253, -0.00183714, 3034.74612775, 0.21252668, 0.20469106));
	Add(TEXT("Saturn"), 5.6834e26, ESolarOrbitModel::Planet,
		MakeElements(9.53667594, 0.05386179, 2.48599187, 49.95424423, 92.59887831, 113.66242448,
		             -0.00125060, -0.00050991, 0.00193609, 1222.49362201, -0.41897216, -0.28867794));
	Add(TEXT("Uranus"), 8.6810e25, ESolarOrbitModel::Planet,
		MakeElements(19.18916464, 0.04725744, 0.77263783, 313.23810451, 170.95427630, 74.01692503,
		             -0.00196176, -0.00004397, -0.00242939, 428.48202785, 0.40805281, 0.04240589));
	Add(TEXT("Neptune"), 1.02413e26, ESolarOrbitModel::Planet,
		MakeElements(30.06992276, 0.00859048, 1.77004347, -55.12002969, 44.96476227, 131.78422574,
		             0.00026291, 0.00005105, 0.00035372, 218.45945325, -0.32241464, -0.00508664));
}

// ===================================================================== Temps / reperes

FDateTime ASolarSystemSimulator::GetSimDateUTC() const
{
	return SolarConst::J2000 + FTimespan::FromSeconds(SimTime);
}

FVector ASolarSystemSimulator::RealToWorld(const FVector& Real) const
{
	return FVector(Real.X, -Real.Y, Real.Z) / RealMetersPerUnrealCm;
}

FVector ASolarSystemSimulator::WorldToReal(const FVector& World) const
{
	return FVector(World.X, -World.Y, World.Z) * RealMetersPerUnrealCm;
}

ASolarSystemSimulator* ASolarSystemSimulator::Get(const UObject* WorldContext)
{
	UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ASolarSystemSimulator> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

int32 ASolarSystemSimulator::FindBodyIndex(FName BodyName) const
{
	return Bodies.IndexOfByPredicate([BodyName](const FSolarBody& B) { return B.Name == BodyName; });
}

double ASolarSystemSimulator::GetBodyMass(FName BodyName) const
{
	const int32 i = FindBodyIndex(BodyName);
	return i == INDEX_NONE ? 0.0 : Bodies[i].MassKg;
}

FVector ASolarSystemSimulator::GetBodyWorldLocation(FName BodyName) const
{
	const int32 i = FindBodyIndex(BodyName);
	return i == INDEX_NONE ? FVector::ZeroVector : RealToWorld(BodyRealPosition(i, SimTime));
}

// ===================================================================== Orbites

FVector ASolarSystemSimulator::PlanetHelio(const FSolarKeplerElements& K, double Time) const
{
	using namespace SolarConst;
	const double T = Time / (SecondsPerDay * DaysPerCentury);  // siecles juliens depuis J2000
	const double a = (K.SemiMajorAxisAU + K.SemiMajorAxisRate * T) * AU;
	const double e = K.Eccentricity + K.EccentricityRate * T;
	const double I = K.InclinationDeg + K.InclinationRate * T;
	const double L = K.MeanLongitudeDeg + K.MeanLongitudeRate * T;
	const double W = K.LongPerihelionDeg + K.LongPerihelionRate * T;
	const double N = K.LongAscNodeDeg + K.LongAscNodeRate * T;
	return OrbitToReference(a, e, I, N, W - N, L - W);
}

FVector ASolarSystemSimulator::MoonGeocentric(double Time) const
{
	// Elements moyens geocentriques de la Lune (P. Schlyter, "How to compute planetary positions"),
	// d compte depuis le 0 janvier 2000 a 0 h, soit 1,5 jour avant J2000.
	const double d = Time / SolarConst::SecondsPerDay + 1.5;
	const double N = 125.1228 - 0.0529538083 * d;
	const double I = 5.1454;
	const double w = 318.0634 + 0.1643573223 * d;
	const double a = 60.2666 * 6378140.0;  // 60,2666 rayons terrestres equatoriaux, en m
	const double e = 0.054900;
	const double M = 115.3654 + 13.0649929509 * d;
	return SolarConst::OrbitToReference(a, e, I, N, w, M);
}

void ASolarSystemSimulator::ComputeAllPositions(double Time, TArray<FVector>& Out) const
{
	Out.SetNumUninitialized(Bodies.Num());
	FVector Geo = FVector::ZeroVector;
	double MoonFraction = 0.0;
	if (EarthIndex != INDEX_NONE && MoonIndex != INDEX_NONE)
	{
		Geo = MoonGeocentric(Time);
		const double mE = Bodies[EarthIndex].MassKg, mM = Bodies[MoonIndex].MassKg;
		MoonFraction = mM / FMath::Max(mE + mM, 1.0);
	}
	for (int32 i = 0; i < Bodies.Num(); ++i)
	{
		const FSolarBody& B = Bodies[i];
		switch (B.Model)
		{
		case ESolarOrbitModel::Planet:
			Out[i] = PlanetHelio(B.Elements, Time);
			break;
		case ESolarOrbitModel::EarthMoon:
			// Les elements JPL decrivent le barycentre Terre-Lune ; la Terre en est decalee a l'oppose de la Lune.
			Out[i] = PlanetHelio(B.Elements, Time) - Geo * MoonFraction;
			break;
		default:
			Out[i] = FVector::ZeroVector;
			break;
		}
	}
	if (MoonIndex != INDEX_NONE && EarthIndex != INDEX_NONE)
	{
		Out[MoonIndex] = Out[EarthIndex] + Geo;
	}
}

FVector ASolarSystemSimulator::BodyRealPosition(int32 Index, double Time) const
{
	TArray<FVector> P;
	ComputeAllPositions(Time, P);
	return P.IsValidIndex(Index) ? P[Index] : FVector::ZeroVector;
}

FVector ASolarSystemSimulator::BodyRealVelocity(int32 Index, double Time) const
{
	const double h = 30.0;  // s
	return (BodyRealPosition(Index, Time + h) - BodyRealPosition(Index, Time - h)) / (2.0 * h);
}

FVector ASolarSystemSimulator::GravityAt(const FVector& RealPos, double Time) const
{
	TArray<FVector> P;
	ComputeAllPositions(Time, P);
	FVector Acc = FVector::ZeroVector;
	for (int32 i = 0; i < Bodies.Num(); ++i)
	{
		const FVector D = P[i] - RealPos;
		const double R2 = D.SizeSquared();
		if (R2 < 1.0 || Bodies[i].MassKg <= 0.0)
		{
			continue;
		}
		Acc += D * (BodyGM(i) / (R2 * FMath::Sqrt(R2)));
	}
	return Acc;
}

// ===================================================================== Cycle de vie

void ASolarSystemSimulator::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	EarthIndex = FindBodyIndex(TEXT("Earth"));
	MoonIndex = FindBodyIndex(TEXT("Moon"));
	SimTime = (StartDateUTC - SolarConst::J2000).GetTotalSeconds();
}

void ASolarSystemSimulator::BeginPlay()
{
	Super::BeginPlay();
	EnsureInitialized();
}

void ASolarSystemSimulator::EnsureInitialized()
{
	if (bInitialized)
	{
		return;
	}
	bInitialized = true;
	EarthIndex = FindBodyIndex(TEXT("Earth"));
	MoonIndex = FindBodyIndex(TEXT("Moon"));
	SimTime = (StartDateUTC - SolarConst::J2000).GetTotalSeconds();
	FreeDrivenActors();
	TimeScale = WarpTimeScale;
	CaptureScaledMovers();
	// Vitesses du niveau reglees pour x1440 : mise a l'echelle des le depart (sinon, si l'acceleration de depart
	// n'est jamais modifiee, la Terre garderait sa vitesse x1440).
	ApplyMoverScale();
	ApplyPositions();
	UE_LOG(LogSolar, Log, TEXT("Simulation orbitale : %d astres, depart %s UTC, x%.0f."),
		Bodies.Num(), *StartDateUTC.ToString(), TimeScale);
}

void ASolarSystemSimulator::FreeDrivenActors()
{
	TSet<AActor*> Driven;
	for (const FSolarBody& B : Bodies)
	{
		AActor* A = B.DrivenActor;
		if (!A)
		{
			continue;
		}
		Driven.Add(A);
		// Les anciens pivots d'orbite (rotation a vitesse fixe) sont remplaces par le calcul keplerien.
		for (AActor* Up = A; Up; Up = Up->GetAttachParentActor())
		{
			TInlineComponentArray<URotatingMovementComponent*> Movers(Up);
			for (URotatingMovementComponent* M : Movers)
			{
				M->Deactivate();
			}
		}
		A->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		if (USceneComponent* Root = A->GetRootComponent())
		{
			Root->SetMobility(EComponentMobility::Movable);
		}
	}

	// Tout ce qui est porte par un astre (rotation propre, nuages, anneaux...) se met a jour apres lui.
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		AActor* Other = *It;
		if (Other == this)
		{
			continue;
		}
		bool bRides = false;
		for (AActor* Up = Other; Up && !bRides; Up = Up->GetAttachParentActor())
		{
			bRides = Driven.Contains(Up);
		}
		if (!bRides)
		{
			continue;
		}
		Other->AddTickPrerequisiteActor(this);
		for (UActorComponent* C : Other->GetComponents())
		{
			if (C)
			{
				C->AddTickPrerequisiteActor(this);
			}
		}
	}
}

void ASolarSystemSimulator::ApplyPositions()
{
	TArray<FVector> P;
	ComputeAllPositions(SimTime, P);
	for (int32 i = 0; i < Bodies.Num(); ++i)
	{
		AActor* A = Bodies[i].DrivenActor;
		if (!A || Bodies[i].Model == ESolarOrbitModel::Fixed)
		{
			continue;
		}
		const FVector World = RealToWorld(P[i]);
		A->SetActorLocation(World, false, nullptr, ETeleportType::TeleportPhysics);
		if (Bodies[i].bFaceParentBody && EarthIndex != INDEX_NONE)
		{
			const FVector ToEarth = RealToWorld(P[EarthIndex]) - World;
			A->SetActorRotation(FRotationMatrix::MakeFromX(ToEarth.GetSafeNormal()).Rotator(), ETeleportType::TeleportPhysics);
		}
	}
}

void ASolarSystemSimulator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	EnsureInitialized();
	UpdateTimeScale();
	SimTime += static_cast<double>(DeltaSeconds) * TimeScale;
	ApplyPositions();
}

// ===================================================================== Acceleration du temps

void ASolarSystemSimulator::NotifyThrust()
{
	if (UWorld* World = GetWorld())
	{
		LastThrustTime = World->GetTimeSeconds();
	}
}

bool ASolarSystemSimulator::IsThrustTimeActive() const
{
	const UWorld* World = GetWorld();
	return World && (World->GetTimeSeconds() - LastThrustTime) < ThrustHoldSeconds;
}

void ASolarSystemSimulator::UpdateTimeScale()
{
	const double Target = IsThrustTimeActive() ? ThrustTimeScale : WarpTimeScale;
	if (FMath::IsNearlyEqual(Target, TimeScale))
	{
		return;
	}
	TimeScale = Target;
	ApplyMoverScale();
	UE_LOG(LogSolar, Log, TEXT("Temps x%.0f%s"), TimeScale, IsThrustTimeActive() ? TEXT(" (poussee)") : TEXT(""));
}

void ASolarSystemSimulator::ApplyMoverScale()
{
	// Les rotations a vitesse fixe restantes (rotation propre de la Terre, nuages) suivent le temps simule.
	const double Ratio = MoverReferenceScale > 0.0 ? TimeScale / MoverReferenceScale : 1.0;
	for (const FScaledMover& M : ScaledMovers)
	{
		if (URotatingMovementComponent* Mover = M.Mover.Get())
		{
			Mover->RotationRate = M.BaseRate * Ratio;
		}
	}
}

void ASolarSystemSimulator::CaptureScaledMovers()
{
	// Les vitesses de rotation du niveau ont ete reglees pour RotationReferenceTimeScale (x1440 : un jour = 60 s),
	// independamment de l'acceleration choisie ensuite.
	MoverReferenceScale = RotationReferenceTimeScale;
	ScaledMovers.Reset();
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<URotatingMovementComponent*> Movers(*It);
		for (URotatingMovementComponent* M : Movers)
		{
			if (M && M->IsActive())
			{
				ScaledMovers.Add({ M, M->RotationRate });
			}
		}
	}
}
