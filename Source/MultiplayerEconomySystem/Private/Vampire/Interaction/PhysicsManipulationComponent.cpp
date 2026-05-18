#include "Vampire/Interaction/PhysicsManipulationComponent.h"

#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "UObject/UObjectIterator.h"
#include "Vampire/Interaction/InteractionPlacementTargetComponent.h"
#include "Vampire/Interaction/ManipulatableObjectComponent.h"

namespace
{
	struct FPlacementTargetSearchResult
	{
		TArray<UInteractionPlacementTargetComponent*> CandidatesWithinTolerance;
		UInteractionPlacementTargetComponent* BestTarget = nullptr;
	};

	FPlacementTargetSearchResult FindPlacementTargets(
		UWorld* World,
		const UManipulatableObjectComponent* Manipulatable,
		const UPrimitiveComponent* HeldPrimitive)
	{
		FPlacementTargetSearchResult Result;
		if (!World || !Manipulatable || !HeldPrimitive)
		{
			return Result;
		}

		float BestDistanceSq = TNumericLimits<float>::Max();

		for (TObjectIterator<UInteractionPlacementTargetComponent> It; It; ++It)
		{
			UInteractionPlacementTargetComponent* Candidate = *It;
			if (!Candidate || Candidate->GetWorld() != World)
			{
				continue;
			}

			if (!Candidate->UsesSnapPlacement())
			{
				continue;
			}

			if (!Candidate->IsObjectWithinPlacementTolerance(Manipulatable))
			{
				continue;
			}

			Result.CandidatesWithinTolerance.Add(Candidate);

			const float DistanceSq = FVector::DistSquared(
				HeldPrimitive->GetComponentLocation(),
				Candidate->GetEffectiveSnapLocation());

			if (DistanceSq < BestDistanceSq)
			{
				BestDistanceSq = DistanceSq;
				Result.BestTarget = Candidate;
			}
		}

		return Result;
	}

	UInteractionPlacementTargetComponent* FindBestPlacementTarget(
		UWorld* World,
		const UManipulatableObjectComponent* Manipulatable,
		const UPrimitiveComponent* HeldPrimitive)
	{
		return FindPlacementTargets(World, Manipulatable, HeldPrimitive).BestTarget;
	}

	FPlacementTargetSearchResult DebugDrawPlacementTargets(
		UWorld* World,
		const UManipulatableObjectComponent* Manipulatable,
		const UPrimitiveComponent* HeldPrimitive,
		const float Duration)
	{
		FPlacementTargetSearchResult SearchResult;
		if (!World || !Manipulatable || !HeldPrimitive)
		{
			return SearchResult;
		}

		SearchResult = FindPlacementTargets(World, Manipulatable, HeldPrimitive);

		for (TObjectIterator<UInteractionPlacementTargetComponent> It; It; ++It)
		{
			UInteractionPlacementTargetComponent* Candidate = *It;
			if (!Candidate || Candidate->GetWorld() != World)
			{
				continue;
			}

			if (!Candidate->UsesSnapPlacement())
			{
				continue;
			}

			Candidate->DebugDraw(Manipulatable, Candidate == SearchResult.BestTarget, Duration);
		}

		const FString SummaryText = SearchResult.BestTarget
			? FString::Printf(
				TEXT("Best target: %s\nValid targets: %d"),
				*GetNameSafe(SearchResult.BestTarget),
				SearchResult.CandidatesWithinTolerance.Num())
			: FString::Printf(
				TEXT("Best target: none\nValid targets: %d"),
				SearchResult.CandidatesWithinTolerance.Num());
		DrawDebugString(
			World,
			HeldPrimitive->GetComponentLocation() + FVector(0.0f, 0.0f, 24.0f),
			SummaryText,
			nullptr,
			SearchResult.BestTarget ? FColor::Cyan : FColor::Red,
			Duration,
			false,
			1.0f);

		return SearchResult;
	}
}

UPhysicsManipulationComponent::UPhysicsManipulationComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
}

void UPhysicsManipulationComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsurePhysicsHandle();
	HeldDistance = DefaultHoldDistance;
}

void UPhysicsManipulationComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (HasHeldObject())
	{
		if (!bServerDrivenRemoteHold && IsOwnerLocallyDrivingManipulation())
		{
			ApplyHeldTarget();

			if (bEnablePlacementDebugDraw)
			{
				const FPlacementTargetSearchResult SearchResult = DebugDrawPlacementTargets(
					GetWorld(),
					HeldManipulatable.Get(),
					HeldPrimitive.Get(),
					PlacementDebugDrawDuration);
				SetCurrentPlacementTarget(SearchResult.BestTarget);
			}
			else
			{
				SetCurrentPlacementTarget(FindBestPlacementTarget(GetWorld(), HeldManipulatable.Get(), HeldPrimitive.Get()));
			}
		}
	}
	else
	{
		SetCurrentPlacementTarget(nullptr);
	}
}

bool UPhysicsManipulationComponent::TryGrabFromView(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		UE_LOG(LogTemp, Warning, TEXT("PhysicsManipulation: TryGrabFromView failed - PlayerController is null"));
		return false;
	}

	EnsurePhysicsHandle();
	if (!PhysicsHandle || HasHeldObject())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PhysicsManipulation: TryGrabFromView failed - PhysicsHandle=%s HasHeldObject=%s"),
			PhysicsHandle ? TEXT("true") : TEXT("false"),
			HasHeldObject() ? TEXT("true") : TEXT("false"));
		return false;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceDirection = FVector::ZeroVector;
	const bool bUsingMouseRay = PlayerController->DeprojectMousePositionToWorld(TraceStart, TraceDirection);

	if (!bUsingMouseRay)
	{
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		TraceStart = ViewLocation;
		TraceDirection = ViewRotation.Vector();
	}

	FHitResult Hit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PhysicsManipulationGrab), false);
	if (APawn* Pawn = PlayerController->GetPawn())
	{
		QueryParams.AddIgnoredActor(Pawn);
	}

	const FVector TraceEnd = TraceStart + (TraceDirection * GrabTraceDistance);
	if (!GetWorld() || !GetWorld()->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, GrabTraceChannel, QueryParams))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PhysicsManipulation: Trace miss on channel %d from %s to %s (usingMouseRay=%s)"),
			static_cast<int32>(GrabTraceChannel.GetValue()),
			*TraceStart.ToString(),
			*TraceEnd.ToString(),
			bUsingMouseRay ? TEXT("true") : TEXT("false"));
		return false;
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("PhysicsManipulation: Trace hit actor=%s component=%s simPhysics=%s usingMouseRay=%s"),
		*GetNameSafe(Hit.GetActor()),
		*GetNameSafe(Hit.GetComponent()),
		(Hit.GetComponent() && Hit.GetComponent()->IsSimulatingPhysics()) ? TEXT("true") : TEXT("false"),
		bUsingMouseRay ? TEXT("true") : TEXT("false"));

	return TryGrabComponent(Hit.GetComponent(), Hit);
}

bool UPhysicsManipulationComponent::TryGrabComponent(UPrimitiveComponent* PrimitiveComponent, const FHitResult& HitResult)
{
	EnsurePhysicsHandle();
	if (!PhysicsHandle || !PrimitiveComponent || HasHeldObject())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PhysicsManipulation: TryGrabComponent failed - PhysicsHandle=%s Primitive=%s HasHeldObject=%s"),
			PhysicsHandle ? TEXT("true") : TEXT("false"),
			*GetNameSafe(PrimitiveComponent),
			HasHeldObject() ? TEXT("true") : TEXT("false"));
		return false;
	}

	UManipulatableObjectComponent* Manipulatable = ResolveManipulatableFromComponent(PrimitiveComponent);
	if (!Manipulatable || !Manipulatable->CanBeGrabbed())
	{
		const UPrimitiveComponent* PrimaryPhysicsComponent = Manipulatable ? Manipulatable->GetPrimaryPhysicsComponent() : nullptr;
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PhysicsManipulation: TryGrabComponent failed - Manipulatable=%s CanBeGrabbed=%s PrimaryPhysics=%s SimPhysics=%s bCanBeGrabbed=%s"),
			*GetNameSafe(Manipulatable),
			(Manipulatable && Manipulatable->CanBeGrabbed()) ? TEXT("true") : TEXT("false"),
			*GetNameSafe(PrimaryPhysicsComponent),
			(PrimaryPhysicsComponent && PrimaryPhysicsComponent->IsSimulatingPhysics()) ? TEXT("true") : TEXT("false"),
			(Manipulatable && Manipulatable->bCanBeGrabbed) ? TEXT("true") : TEXT("false"));
		return false;
	}

	const float InitialHeldDistance = FMath::Clamp(FVector::Dist(HitResult.ImpactPoint, HitResult.TraceStart), MinHoldDistance, MaxHoldDistance);
	const FRotator InitialTargetRotation = PrimitiveComponent->GetComponentRotation() + Manipulatable->HeldRotationOffset;
	if (!AcquireHeldObject(PrimitiveComponent, Manipulatable, HitResult.ImpactPoint, InitialHeldDistance, InitialTargetRotation))
	{
		return false;
	}

	BeginRemoteManipulationIfNeeded(PrimitiveComponent, HitResult);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("PhysicsManipulation: Grab success actor=%s component=%s distance=%.2f"),
		*GetNameSafe(PrimitiveComponent->GetOwner()),
		*GetNameSafe(PrimitiveComponent),
		HeldDistance);

	OnObjectGrabbed.Broadcast(this, Manipulatable);
	return true;
}

void UPhysicsManipulationComponent::ReleaseHeldObject()
{
	if (!HasHeldObject())
	{
		return;
	}

	UManipulatableObjectComponent* ReleasedObject = HeldManipulatable.Get();
	UPrimitiveComponent* ReleasedPrimitive = HeldPrimitive.Get();

	if (UInteractionPlacementTargetComponent* AutoPlacementTarget = FindBestPlacementTarget(GetWorld(), HeldManipulatable.Get(), HeldPrimitive.Get()))
	{
		if (TryPlaceHeldObjectAtTarget(AutoPlacementTarget))
		{
			EndRemoteManipulationIfNeeded(ReleasedPrimitive);
			return;
		}
	}

	EndRemoteManipulationIfNeeded(ReleasedPrimitive);
	ClearHeldObjectState(true);
}

void UPhysicsManipulationComponent::SetManipulationMode(const EPhysicsManipulationMode NewMode)
{
	CurrentMode = NewMode;
}

void UPhysicsManipulationComponent::AddPlanarInput(const FVector2D InputAxis)
{
	if (!HasHeldObject() || !HeldManipulatable || !HeldManipulatable->bAllowTranslation || CurrentMode != EPhysicsManipulationMode::Translate)
	{
		return;
	}

	HeldLocalPlanarOffset.X += InputAxis.X * TranslationSpeed;
	HeldLocalPlanarOffset.Z += InputAxis.Y * TranslationSpeed;
}

void UPhysicsManipulationComponent::AddDepthInput(const float InputAxis)
{
	if (!HasHeldObject())
	{
		return;
	}

	HeldDistance = FMath::Clamp(HeldDistance + (InputAxis * DepthSpeed), MinHoldDistance, MaxHoldDistance);
}

void UPhysicsManipulationComponent::AddRotationInput(const FVector2D InputAxis)
{
	if (!HasHeldObject() || !HeldManipulatable || !HeldManipulatable->bAllowRotation || CurrentMode != EPhysicsManipulationMode::Rotate)
	{
		return;
	}

	if (HeldManipulatable->AllowedRotationAxes == EManipulatableRotationAxes::YawPitch || HeldManipulatable->AllowedRotationAxes == EManipulatableRotationAxes::All)
	{
		HeldTargetRotation.Yaw += InputAxis.X * RotationSpeedDegrees;
		HeldTargetRotation.Pitch += InputAxis.Y * RotationSpeedDegrees;
	}
}

void UPhysicsManipulationComponent::AddRollInput(const float InputAxis)
{
	if (!HasHeldObject() || !HeldManipulatable || !HeldManipulatable->bAllowRotation || !HeldManipulatable->bAllowRoll || CurrentMode != EPhysicsManipulationMode::Rotate)
	{
		return;
	}

	if (HeldManipulatable->AllowedRotationAxes == EManipulatableRotationAxes::All)
	{
		HeldTargetRotation.Roll += InputAxis * RotationSpeedDegrees;
	}
}

bool UPhysicsManipulationComponent::TryPlaceHeldObjectAtTarget(UInteractionPlacementTargetComponent* PlacementTarget)
{
	if (!HasHeldObject() || !PlacementTarget || !HeldManipulatable)
	{
		return false;
	}

	const bool bPlaced = PlacementTarget->TryPlaceObject(HeldManipulatable);
	if (bPlaced && HeldManipulatable->bSnapOnValidPlacement)
	{
		if (PhysicsHandle)
		{
			PhysicsHandle->ReleaseComponent();
		}

		UPrimitiveComponent* PhysicsComponent = HeldManipulatable->GetPrimaryPhysicsComponent();
		if (PhysicsComponent)
		{
			PhysicsComponent->SetSimulatePhysics(false);
			PhysicsComponent->SetWorldTransform(PlacementTarget->BuildSnapTransform(HeldManipulatable));
		}

		UManipulatableObjectComponent* ReleasedObject = HeldManipulatable.Get();
		HeldPrimitive = nullptr;
		HeldManipulatable = nullptr;
		HeldLocalPlanarOffset = FVector::ZeroVector;
		HeldDistance = DefaultHoldDistance;
		bServerDrivenRemoteHold = false;
		LastServerTargetSendTimeSeconds = -1.0;
		SetCurrentPlacementTarget(PlacementTarget);
		OnObjectReleased.Broadcast(this, ReleasedObject);
	}

	return bPlaced;
}

bool UPhysicsManipulationComponent::HasHeldObject() const
{
	return HeldPrimitive != nullptr;
}

UManipulatableObjectComponent* UPhysicsManipulationComponent::GetHeldManipulatable() const
{
	return HeldManipulatable.Get();
}

UPrimitiveComponent* UPhysicsManipulationComponent::GetHeldPrimitive() const
{
	return HeldPrimitive.Get();
}

void UPhysicsManipulationComponent::EnsurePhysicsHandle()
{
	if (PhysicsHandle || !GetOwner())
	{
		return;
	}

	PhysicsHandle = GetOwner()->FindComponentByClass<UPhysicsHandleComponent>();
	if (!PhysicsHandle)
	{
		PhysicsHandle = NewObject<UPhysicsHandleComponent>(GetOwner(), TEXT("PhysicsManipulationHandle"));
		if (PhysicsHandle)
		{
			PhysicsHandle->RegisterComponent();
		}
	}

	if (PhysicsHandle)
	{
		PhysicsHandle->LinearStiffness = 2500.0f;
		PhysicsHandle->LinearDamping = 200.0f;
		PhysicsHandle->AngularStiffness = 3500.0f;
		PhysicsHandle->AngularDamping = 250.0f;
		PhysicsHandle->InterpolationSpeed = 18.0f;
	}
}

void UPhysicsManipulationComponent::ApplyHeldTarget()
{
	if (!PhysicsHandle || !HeldPrimitive)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController)
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			PlayerController = Cast<APlayerController>(OwnerPawn->GetController());
		}
	}

	if (!PlayerController)
	{
		return;
	}

	FVector TraceStart = FVector::ZeroVector;
	FVector TraceDirection = FVector::ZeroVector;
	const bool bUsingMouseRay = PlayerController->DeprojectMousePositionToWorld(TraceStart, TraceDirection);

	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!bUsingMouseRay)
	{
		FVector ViewLocation = FVector::ZeroVector;
		PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		TraceStart = ViewLocation;
		TraceDirection = ViewRotation.Vector();
	}
	else
	{
		ViewRotation = TraceDirection.Rotation();
	}

	const FVector ForwardVector = TraceDirection.GetSafeNormal();
	const FVector RightVector = FRotationMatrix(ViewRotation).GetScaledAxis(EAxis::Y);
	const FVector UpVector = FRotationMatrix(ViewRotation).GetScaledAxis(EAxis::Z);

	FVector TargetLocation = TraceStart + (ForwardVector * HeldDistance);
	TargetLocation += RightVector * HeldLocalPlanarOffset.X;
	TargetLocation += UpVector * HeldLocalPlanarOffset.Z;

	PhysicsHandle->SetTargetLocationAndRotation(TargetLocation, HeldTargetRotation);
	SendRemoteManipulationTargetIfNeeded(TargetLocation, HeldTargetRotation);
}

void UPhysicsManipulationComponent::SetCurrentPlacementTarget(UInteractionPlacementTargetComponent* NewTarget)
{
	if (CurrentPlacementTarget == NewTarget)
	{
		return;
	}

	if (CurrentPlacementTarget)
	{
		OnPlacementTargetLeft.Broadcast(this, CurrentPlacementTarget);
	}

	CurrentPlacementTarget = NewTarget;

	if (CurrentPlacementTarget)
	{
		OnPlacementTargetEntered.Broadcast(this, CurrentPlacementTarget);
	}
}

UManipulatableObjectComponent* UPhysicsManipulationComponent::ResolveManipulatableFromComponent(UPrimitiveComponent* PrimitiveComponent) const
{
	if (!PrimitiveComponent)
	{
		return nullptr;
	}

	if (UManipulatableObjectComponent* OnOwner = PrimitiveComponent->GetOwner() ? PrimitiveComponent->GetOwner()->FindComponentByClass<UManipulatableObjectComponent>() : nullptr)
	{
		return OnOwner;
	}

	return nullptr;
}

bool UPhysicsManipulationComponent::IsOwnerLocallyDrivingManipulation() const
{
	if (const APlayerController* PlayerControllerOwner = Cast<APlayerController>(GetOwner()))
	{
		return PlayerControllerOwner->IsLocalController();
	}

	if (const APawn* PawnOwner = Cast<APawn>(GetOwner()))
	{
		if (const AController* Controller = PawnOwner->GetController())
		{
			return Controller->IsLocalController();
		}
	}

	return false;
}

void UPhysicsManipulationComponent::BeginRemoteManipulationIfNeeded(UPrimitiveComponent* PrimitiveComponent, const FHitResult& HitResult)
{
	if (GetOwnerRole() >= ROLE_Authority || !PrimitiveComponent)
	{
		return;
	}

	AActor* ManipulatedActor = PrimitiveComponent->GetOwner();
	if (!ManipulatedActor)
	{
		return;
	}

	LastServerTargetSendTimeSeconds = -1.0;
	LastSentServerTargetLocation = FVector::ZeroVector;
	LastSentServerTargetRotation = FRotator::ZeroRotator;
	ServerBeginRemoteManipulation(ManipulatedActor, HitResult.ImpactPoint, HeldDistance, HeldTargetRotation);
}

void UPhysicsManipulationComponent::EndRemoteManipulationIfNeeded(UPrimitiveComponent* PrimitiveComponent)
{
	if (GetOwnerRole() >= ROLE_Authority || !PrimitiveComponent)
	{
		return;
	}

	if (AActor* ManipulatedActor = PrimitiveComponent->GetOwner())
	{
		ServerEndRemoteManipulation(ManipulatedActor);
	}
}

void UPhysicsManipulationComponent::SendRemoteManipulationTargetIfNeeded(const FVector& TargetLocation, const FRotator& TargetRotation)
{
	if (GetOwnerRole() >= ROLE_Authority || !HeldPrimitive)
	{
		return;
	}

	AActor* ManipulatedActor = HeldPrimitive->GetOwner();
	if (!ManipulatedActor || !GetWorld())
	{
		return;
	}

	const double CurrentTimeSeconds = GetWorld()->GetTimeSeconds();
	const bool bReachedSendInterval = LastServerTargetSendTimeSeconds < 0.0
		|| (CurrentTimeSeconds - LastServerTargetSendTimeSeconds) >= ServerTargetSendInterval;
	const bool bLocationChangedEnough = LastServerTargetSendTimeSeconds < 0.0
		|| FVector::DistSquared(LastSentServerTargetLocation, TargetLocation) >= FMath::Square(ServerTargetLocationThreshold);
	const bool bRotationChangedEnough = LastServerTargetSendTimeSeconds < 0.0
		|| LastSentServerTargetRotation.Equals(TargetRotation, ServerTargetRotationThresholdDegrees) == false;

	if (!bReachedSendInterval && !bLocationChangedEnough && !bRotationChangedEnough)
	{
		return;
	}

	ServerUpdateRemoteManipulationTarget(ManipulatedActor, TargetLocation, TargetRotation);
	LastServerTargetSendTimeSeconds = CurrentTimeSeconds;
	LastSentServerTargetLocation = TargetLocation;
	LastSentServerTargetRotation = TargetRotation;
}

bool UPhysicsManipulationComponent::AcquireHeldObject(
	UPrimitiveComponent* PrimitiveComponent,
	UManipulatableObjectComponent* Manipulatable,
	const FVector& GrabLocation,
	const float InHeldDistance,
	const FRotator& InitialTargetRotation)
{
	if (!PhysicsHandle || !PrimitiveComponent || !Manipulatable)
	{
		return false;
	}

	HeldPrimitive = PrimitiveComponent;
	HeldManipulatable = Manipulatable;
	HeldDistance = InHeldDistance;
	HeldLocalPlanarOffset = FVector::ZeroVector;
	HeldTargetRotation = InitialTargetRotation;
	bServerDrivenRemoteHold = !IsOwnerLocallyDrivingManipulation();

	PrimitiveComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
	PrimitiveComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	PhysicsHandle->GrabComponentAtLocationWithRotation(
		PrimitiveComponent,
		NAME_None,
		GrabLocation,
		HeldTargetRotation);

	return true;
}

void UPhysicsManipulationComponent::ClearHeldObjectState(const bool bBroadcastRelease)
{
	UManipulatableObjectComponent* ReleasedObject = HeldManipulatable.Get();

	if (PhysicsHandle)
	{
		PhysicsHandle->ReleaseComponent();
	}

	HeldPrimitive = nullptr;
	HeldManipulatable = nullptr;
	HeldLocalPlanarOffset = FVector::ZeroVector;
	HeldDistance = DefaultHoldDistance;
	bServerDrivenRemoteHold = false;
	LastServerTargetSendTimeSeconds = -1.0;
	SetCurrentPlacementTarget(nullptr);

	if (bBroadcastRelease)
	{
		OnObjectReleased.Broadcast(this, ReleasedObject);
	}
}

void UPhysicsManipulationComponent::ServerBeginRemoteManipulation_Implementation(
	AActor* ManipulatedActor,
	const FVector GrabLocation,
	const float InHeldDistance,
	const FRotator InitialTargetRotation)
{
	if (!ManipulatedActor || !GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	UPrimitiveComponent* PrimitiveComponent = nullptr;
	UManipulatableObjectComponent* Manipulatable = ManipulatedActor->FindComponentByClass<UManipulatableObjectComponent>();
	if (Manipulatable)
	{
		PrimitiveComponent = Manipulatable->GetPrimaryPhysicsComponent();
	}

	if (!PrimitiveComponent)
	{
		PrimitiveComponent = Cast<UPrimitiveComponent>(ManipulatedActor->GetRootComponent());
	}

	if (!PrimitiveComponent || !PrimitiveComponent->IsSimulatingPhysics())
	{
		return;
	}

	EnsurePhysicsHandle();
	if (!PhysicsHandle)
	{
		return;
	}

	ClearHeldObjectState(false);
	AcquireHeldObject(PrimitiveComponent, Manipulatable ? Manipulatable : ResolveManipulatableFromComponent(PrimitiveComponent), GrabLocation, InHeldDistance, InitialTargetRotation);
}

void UPhysicsManipulationComponent::ServerUpdateRemoteManipulationTarget_Implementation(
	AActor* ManipulatedActor,
	const FVector TargetLocation,
	const FRotator TargetRotation)
{
	if (!ManipulatedActor || !PhysicsHandle || !HeldPrimitive || HeldPrimitive->GetOwner() != ManipulatedActor)
	{
		return;
	}

	HeldTargetRotation = TargetRotation;
	PhysicsHandle->SetTargetLocationAndRotation(TargetLocation, TargetRotation);
}

void UPhysicsManipulationComponent::ServerEndRemoteManipulation_Implementation(AActor* ManipulatedActor)
{
	if (!ManipulatedActor || !HeldPrimitive || HeldPrimitive->GetOwner() != ManipulatedActor)
	{
		return;
	}

	ClearHeldObjectState(false);
}
