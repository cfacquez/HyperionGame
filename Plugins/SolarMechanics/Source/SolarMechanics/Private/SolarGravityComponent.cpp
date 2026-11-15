// Gravite appliquee a un acteur libre - voir SolarGravityComponent.h.
// Fichier volontairement en ASCII (pas d'accents).

#include "SolarGravityComponent.h"

#include "SolarSystemSimulator.h"
#include "GameFramework/Actor.h"
#include "Math/RotationMatrix.h"

DEFINE_LOG_CATEGORY_STATIC(LogSolarGravity, Log, All);

USolarGravityComponent::USolarGravityComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

ASolarSystemSimulator* USolarGravityComponent::Sim() const
{
	if (!CachedSim.IsValid())
	{
		CachedSim = ASolarSystemSimulator::Get(this);
	}
	return CachedSim.Get();
}

void USolarGravityComponent::BeginPlay()
{
	Super::BeginPlay();
	if (ASolarSystemSimulator* S = Sim())
	{
		// Les astres doivent avoir avance avant que l'on integre la trajectoire du vaisseau.
		AddTickPrerequisiteActor(S);
	}
	else
	{
		UE_LOG(LogSolarGravity, Warning, TEXT("%s : aucun SolarSystemSimulator dans le niveau, gravite inactive."), *GetNameSafe(GetOwner()));
	}
}

void USolarGravityComponent::InitializeState()
{
	ASolarSystemSimulator* S = Sim();
	AActor* Owner = GetOwner();
	S->EnsureInitialized();

	// Trajectoire calculee ici : l'acteur ne doit suivre aucun parent.
	Owner->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	const double T = S->GetSimTimeSeconds();
	const int32 Ref = S->FindBodyIndex(ReferenceBody);
	const FVector RefPos = Ref != INDEX_NONE ? S->BodyRealPosition(Ref, T) : FVector::ZeroVector;
	const FVector RefVel = Ref != INDEX_NONE ? S->BodyRealVelocity(Ref, T) : FVector::ZeroVector;

	if (bPlaceAtStart && Ref != INDEX_NONE)
	{
		// Cote jour : entre l'astre et le Soleil (place a l'origine).
		const FVector ToSun = (-RefPos).GetSafeNormal();
		RealPosition = RefPos + ToSun * (StartDistanceKm * 1000.0);
	}
	else
	{
		RealPosition = S->WorldToReal(Owner->GetActorLocation());
	}

	RealVelocity = RefVel;
	if (bStartInCircularOrbit && Ref != INDEX_NONE)
	{
		const FVector R = RealPosition - RefPos;
		const double Dist = R.Size();
		// Sens direct : meme sens que l'orbite de l'astre autour du Soleil.
		FVector Normal = (RefPos ^ RefVel).GetSafeNormal();
		if (Normal.IsNearlyZero())
		{
			Normal = FVector::UpVector;
		}
		const FVector Tangent = (Normal ^ R).GetSafeNormal();
		if (Dist > 1.0)
		{
			RealVelocity += Tangent * FMath::Sqrt(S->BodyGM(Ref) / Dist);
		}
	}

	Owner->SetActorLocation(S->RealToWorld(RealPosition), false, nullptr, ETeleportType::TeleportPhysics);
	if (bFaceReferenceBodyAtStart && Ref != INDEX_NONE)
	{
		const FVector Dir = S->RealDirToWorld(RefPos - RealPosition).GetSafeNormal();
		Owner->SetActorRotation(FRotationMatrix::MakeFromX(Dir).Rotator(), ETeleportType::TeleportPhysics);
	}

	Orientation = Owner->GetActorQuat();
	AngularVelocity = FVector::ZeroVector;

	UE_LOG(LogSolarGravity, Log, TEXT("%s : depart a %.0f km de %s, vitesse relative %.3f km/s."),
		*GetNameSafe(Owner), StartDistanceKm, *ReferenceBody.ToString(), (RealVelocity - RefVel).Size() / 1000.0);
	bStateInitialized = true;
}

void USolarGravityComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ASolarSystemSimulator* S = Sim();
	if (!S || !GetOwner())
	{
		return;
	}
	if (!bStateInitialized)
	{
		InitializeState();
		PendingAccelReal = FVector::ZeroVector;
		PendingTorque = FVector::ZeroVector;
		bThrustThisFrame = false;
		return;
	}

	// Le simulateur a deja avance a T1 ; on integre de T0 a T1 en temps simule.
	const double SimDt = static_cast<double>(DeltaTime) * S->TimeScale;

	// Une poussee dure le temps reel de l'image a ThrustTimeScale. Si le simulateur n'est pas encore
	// redescendu (premiere image de la poussee), on n'applique que cette duree, pas toute l'image acceleree.
	double ThrustFactor = 1.0;
	if (bThrustThisFrame && SimDt > 0.0)
	{
		const double Burn = static_cast<double>(DeltaTime) * S->ThrustTimeScale;
		ThrustFactor = FMath::Min(1.0, Burn / SimDt);
	}
	const FVector ThrustAccel = PendingAccelReal * ThrustFactor;
	const FVector Torque = PendingTorque * ThrustFactor;

	const double T1 = S->GetSimTimeSeconds();
	double T = T1 - SimDt;
	if (SimDt > 0.0)
	{
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(SimDt / MaxSubstepSeconds));
		const double H = SimDt / Steps;
		FVector Acc = S->GravityAt(RealPosition, T) + ThrustAccel;
		for (int32 i = 0; i < Steps; ++i)
		{
			RealVelocity += Acc * (0.5 * H);
			RealPosition += RealVelocity * H;
			T += H;
			Acc = S->GravityAt(RealPosition, T) + ThrustAccel;
			RealVelocity += Acc * (0.5 * H);
		}
		if (bSimulateRotation)
		{
			IntegrateRotation(SimDt, Torque);
		}
	}
	PendingAccelReal = FVector::ZeroVector;
	PendingTorque = FVector::ZeroVector;
	bThrustThisFrame = false;

	GetOwner()->SetActorLocation(S->RealToWorld(RealPosition), false, nullptr, ETeleportType::TeleportPhysics);
	if (bSimulateRotation)
	{
		GetOwner()->SetActorRotation(Orientation, ETeleportType::TeleportPhysics);
	}
}

void USolarGravityComponent::IntegrateRotation(double SimDt, const FVector& TorqueWorld)
{
	// Equations d'Euler dans le repere du vaisseau (axes principaux) : I dw/dt = tau - w x (I w).
	const FVector I(FMath::Max(InertiaKgM2.X, 1.0), FMath::Max(InertiaKgM2.Y, 1.0), FMath::Max(InertiaKgM2.Z, 1.0));
	const int32 Steps = FMath::Max(1, FMath::CeilToInt(SimDt / MaxSubstepSeconds));
	const double H = SimDt / Steps;
	FVector W = Orientation.UnrotateVector(AngularVelocity);  // repere corps
	for (int32 i = 0; i < Steps; ++i)
	{
		const FVector Tb = Orientation.UnrotateVector(TorqueWorld);
		const FVector L = W * I;
		W += ((Tb - (W ^ L)) / I) * H;
		const double Rate = W.Size();
		if (Rate * H > 1e-12)
		{
			Orientation = Orientation * FQuat(W / Rate, Rate * H);
			Orientation.Normalize();
		}
	}
	AngularVelocity = Orientation.RotateVector(W);
}

void USolarGravityComponent::MarkThrust()
{
	bThrustThisFrame = true;
	ASolarSystemSimulator* S = bThrustSlowsTime ? Sim() : nullptr;
	if (S)
	{
		S->NotifyThrust();
	}
}

void USolarGravityComponent::AddForceAtLocation(FVector ForceNewtonsWorld, FVector WorldLocation)
{
	ASolarSystemSimulator* S = Sim();
	if (!S || !GetOwner())
	{
		return;
	}
	// Bras de levier en metres reels (l'echelle du niveau ne change pas la physique du vaisseau).
	const FVector Arm = (WorldLocation - GetOwner()->GetActorLocation()) * S->RealMetersPerUnrealCm;
	PendingTorque += Arm ^ ForceNewtonsWorld;
	AddForce(ForceNewtonsWorld);
}

void USolarGravityComponent::AddTorque(FVector TorqueWorld)
{
	PendingTorque += TorqueWorld;
	MarkThrust();
}

void USolarGravityComponent::AddForce(FVector ForceNewtonsWorld)
{
	if (ASolarSystemSimulator* S = Sim())
	{
		PendingAccelReal += S->RealDirToWorld(ForceNewtonsWorld) / FMath::Max(MassKg, 1.0);  // inversion Y : symetrique
		MarkThrust();
	}
}

void USolarGravityComponent::AddAcceleration(FVector AccelerationWorld)
{
	if (ASolarSystemSimulator* S = Sim())
	{
		PendingAccelReal += S->RealDirToWorld(AccelerationWorld);
		MarkThrust();
	}
}

FVector USolarGravityComponent::GetVelocityRelativeTo(FName BodyName) const
{
	ASolarSystemSimulator* S = Sim();
	if (!S)
	{
		return FVector::ZeroVector;
	}
	const int32 i = S->FindBodyIndex(BodyName);
	const FVector BodyVel = i != INDEX_NONE ? S->BodyRealVelocity(i, S->GetSimTimeSeconds()) : FVector::ZeroVector;
	return S->RealDirToWorld(RealVelocity - BodyVel);
}

double USolarGravityComponent::GetDistanceToBodyKm(FName BodyName) const
{
	ASolarSystemSimulator* S = Sim();
	if (!S)
	{
		return 0.0;
	}
	const int32 i = S->FindBodyIndex(BodyName);
	return i == INDEX_NONE ? 0.0 : (S->BodyRealPosition(i, S->GetSimTimeSeconds()) - RealPosition).Size() / 1000.0;
}

FVector USolarGravityComponent::GetGravityAcceleration() const
{
	ASolarSystemSimulator* S = Sim();
	return S ? S->RealDirToWorld(S->GravityAt(RealPosition, S->GetSimTimeSeconds())) : FVector::ZeroVector;
}
