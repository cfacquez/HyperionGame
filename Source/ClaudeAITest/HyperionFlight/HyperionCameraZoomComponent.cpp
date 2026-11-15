#include "HyperionCameraZoomComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogHyperionCamera, Log, All);

UHyperionCameraZoomComponent::UHyperionCameraZoomComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

void UHyperionCameraZoomComponent::BeginPlay()
{
	Super::BeginPlay();
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}
	TInlineComponentArray<UCameraComponent*> Cameras(Owner);
	for (UCameraComponent* C : Cameras)
	{
		if (CameraComponentName.IsNone() || C->GetFName() == CameraComponentName)
		{
			Camera = C;
			break;
		}
	}
	if (!Camera.IsValid() && Cameras.Num() > 0)
	{
		Camera = Cameras[0];
	}
	if (Camera.IsValid())
	{
		DefaultOffset = Camera->GetRelativeLocation();
	}
	TInlineComponentArray<USceneComponent*> Scenes(Owner);
	for (USceneComponent* C : Scenes)
	{
		if (C->GetFName() == PivotComponentName)
		{
			Pivot = C;
			const FRotator R = C->GetRelativeRotation();
			CameraYaw = R.Yaw;
			CameraPitch = R.Pitch;
			break;
		}
	}
	if (!Pivot.IsValid())
	{
		UE_LOG(LogHyperionCamera, Warning, TEXT("%s : pivot '%s' introuvable, orbite inactive."), *Owner->GetName(), *PivotComponentName.ToString());
	}
	else if (!bCameraFollowsShipRotation)
	{
		// Le pivot ne suit plus la rotation du vaisseau : la camera garde son orientation dans l'espace et
		// le vaisseau tourne devant elle (tangage, lacet et roulis visibles).
		Pivot->SetUsingAbsoluteRotation(true);
	}
	if (!Camera.IsValid())
	{
		UE_LOG(LogHyperionCamera, Warning, TEXT("%s : aucune camera, zoom inactif."), *Owner->GetName());
	}
}

void UHyperionCameraZoomComponent::ZoomSteps(float Steps)
{
	TargetZoom = FMath::Clamp(TargetZoom * FMath::Pow(ZoomStepFactor, Steps), MinZoom, MaxZoom);
}

void UHyperionCameraZoomComponent::ResetZoom()
{
	TargetZoom = 1.f;
}

void UHyperionCameraZoomComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	USceneComponent* Cam = Camera.Get();
	if (!Cam)
	{
		return;
	}

	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (const APlayerController* PC = Pawn ? Cast<APlayerController>(Pawn->GetController()) : nullptr)
	{
		// Action mappings : une action se declenche quand l'une de ses touches vient d'etre enfoncee.
		auto JustPressed = [PC](FName Action)
		{
			if (!PC->PlayerInput || Action.IsNone())
			{
				return false;
			}
			for (const FInputActionKeyMapping& Mapping : PC->PlayerInput->GetKeysForAction(Action))
			{
				if (PC->WasInputKeyJustPressed(Mapping.Key))
				{
					return true;
				}
			}
			return false;
		};
		// Molette vers l'avant : rapprocher ; vers l'arriere : eloigner ; bouton du milieu : zoom d'origine.
		if (JustPressed(ZoomInAction))
		{
			ZoomSteps(-1.f);
		}
		if (JustPressed(ZoomOutAction))
		{
			ZoomSteps(1.f);
		}
		if (JustPressed(ZoomResetAction))
		{
			ResetZoom();
		}

		// Orbite : tant que l'action est maintenue, la souris fait tourner le pivot (X : lacet, Y : tangage).
		bool bOrbit = false;
		if (PC->PlayerInput && !OrbitAction.IsNone())
		{
			for (const FInputActionKeyMapping& Mapping : PC->PlayerInput->GetKeysForAction(OrbitAction))
			{
				bOrbit |= PC->IsInputKeyDown(Mapping.Key);
			}
		}
		if (bOrbit && Pivot.IsValid())
		{
			float DX = 0.f, DY = 0.f;
			PC->GetInputMouseDelta(DX, DY);
			CameraYaw += DX * OrbitSensitivity;
			CameraPitch = FMath::Clamp(CameraPitch + DY * OrbitSensitivity * (bInvertOrbitPitch ? -1.f : 1.f), MinCameraPitch, MaxCameraPitch);
		}
	}

	if (USceneComponent* P = Pivot.Get())
	{
		// Camera independante : au depart, placee derriere le vaisseau une fois celui-ci oriente par la simulation
		// (la gravite le tourne vers la Terre a sa premiere image).
		if (!bCameraFollowsShipRotation && !bInitialFrameDone)
		{
			InitElapsed += DeltaTime;
			if (InitElapsed >= 0.2f)
			{
				const FRotator ShipRot = GetOwner()->GetActorRotation();
				CameraYaw = ShipRot.Yaw;
				CameraPitch = FMath::Clamp(ShipRot.Pitch, MinCameraPitch, MaxCameraPitch);
				bInitialFrameDone = true;
			}
		}
		// Avec une rotation absolue, la rotation "relative" du pivot est son orientation dans le monde.
		P->SetRelativeRotation(FRotator(CameraPitch, CameraYaw, 0.f));
	}

	CurrentZoom = ZoomInterpSpeed > 0.f
		? FMath::Exp(FMath::FInterpTo(FMath::Loge(CurrentZoom), FMath::Loge(TargetZoom), DeltaTime, ZoomInterpSpeed))
		: TargetZoom;
	Cam->SetRelativeLocation(DefaultOffset * CurrentZoom);
}
