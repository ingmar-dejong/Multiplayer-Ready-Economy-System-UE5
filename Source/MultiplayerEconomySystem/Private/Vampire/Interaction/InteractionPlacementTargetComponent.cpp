#include "Vampire/Interaction/InteractionPlacementTargetComponent.h"

#include "DrawDebugHelpers.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "Vampire/Interaction/ManipulatableObjectComponent.h"

UInteractionPlacementTargetComponent::UInteractionPlacementTargetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

#if WITH_EDITORONLY_DATA
	EditorToleranceSphere = CreateEditorOnlyDefaultSubobject<USphereComponent>(TEXT("EditorToleranceSphere"));
	if (EditorToleranceSphere)
	{
		EditorToleranceSphere->SetupAttachment(this);
		EditorToleranceSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		EditorToleranceSphere->SetGenerateOverlapEvents(false);
		EditorToleranceSphere->SetHiddenInGame(true);
		EditorToleranceSphere->SetVisibility(true);
		EditorToleranceSphere->SetIsVisualizationComponent(true);
		EditorToleranceSphere->ShapeColor = FColor::Silver;
	}
#endif
}

void UInteractionPlacementTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bUseTriggerVolume)
	{
		return;
	}

	ResolvedTriggerVolume = ResolveTriggerVolumeComponent();
	if (!ResolvedTriggerVolume)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlacementTarget: BeginPlay target=%s trigger-volume mode enabled but no trigger volume resolved"), *GetNameSafe(this));
		return;
	}

	ConfigureResolvedTriggerVolume();

	ResolvedTriggerVolume->OnComponentBeginOverlap.RemoveDynamic(this, &UInteractionPlacementTargetComponent::HandleTriggerVolumeBeginOverlap);
	ResolvedTriggerVolume->OnComponentBeginOverlap.AddDynamic(this, &UInteractionPlacementTargetComponent::HandleTriggerVolumeBeginOverlap);
	ResolvedTriggerVolume->OnComponentEndOverlap.RemoveDynamic(this, &UInteractionPlacementTargetComponent::HandleTriggerVolumeEndOverlap);
	ResolvedTriggerVolume->OnComponentEndOverlap.AddDynamic(this, &UInteractionPlacementTargetComponent::HandleTriggerVolumeEndOverlap);
	ResolvedTriggerVolume->UpdateOverlaps();
}

void UInteractionPlacementTargetComponent::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	UpdateEditorVisualization();
#endif
}

void UInteractionPlacementTargetComponent::TickComponent(
	const float DeltaTime,
	const ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

bool UInteractionPlacementTargetComponent::CanAcceptObject(const UManipulatableObjectComponent* ManipulatedObject) const
{
	if (!ManipulatedObject)
	{
		UE_LOG(LogTemp, Warning, TEXT("PlacementTarget: CanAcceptObject failed target=%s reason=null object"), *GetNameSafe(this));
		return false;
	}

	if (bAllowOnlyOneObject && PlacedObject && PlacedObject != ManipulatedObject)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementTarget: CanAcceptObject failed target=%s reason=occupied placedObject=%s incomingObject=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PlacedObject.Get()),
			*GetNameSafe(ManipulatedObject));
		return false;
	}

	if (!ManipulatedObject->IsValidForPlacementTag(PlacementTargetTag))
	{
		return false;
	}

	return
		AcceptedObjectTags.IsEmpty()
		|| AcceptedObjectTags.HasTagExact(ManipulatedObject->ObjectRoleTag)
		|| AcceptedObjectTags.HasTagExact(ManipulatedObject->ManipulationTag);
}

bool UInteractionPlacementTargetComponent::IsObjectWithinPlacementTolerance(const UManipulatableObjectComponent* ManipulatedObject) const
{
	if (!CanAcceptObject(ManipulatedObject))
	{
		return false;
	}

	if (bUseTriggerVolume && ResolvedTriggerVolume)
	{
		const UPrimitiveComponent* PhysicsComponent = ManipulatedObject->GetPrimaryPhysicsComponent();
		return PhysicsComponent && ResolvedTriggerVolume->IsOverlappingComponent(PhysicsComponent);
	}

	const UPrimitiveComponent* PhysicsComponent = ManipulatedObject->GetPrimaryPhysicsComponent();
	if (!PhysicsComponent)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementTarget: IsObjectWithinPlacementTolerance failed target=%s reason=no physics component object=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ManipulatedObject));
		return false;
	}

	const USceneComponent* SnapTarget = SnapComponent ? SnapComponent.Get() : this;
	const FVector DeltaLocation = PhysicsComponent->GetComponentLocation() - SnapTarget->GetComponentLocation();
	if (DeltaLocation.Size() > PositionTolerance)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementTarget: IsObjectWithinPlacementTolerance failed target=%s reason=position delta=%.2f tolerance=%.2f object=%s physicsLocation=%s targetLocation=%s"),
			*GetNameSafe(this),
			DeltaLocation.Size(),
			PositionTolerance,
			*GetNameSafe(ManipulatedObject),
			*PhysicsComponent->GetComponentLocation().ToString(),
			*SnapTarget->GetComponentLocation().ToString());
		return false;
	}

	if (!bRequireRotationMatch)
	{
		return true;
	}

	const FRotator DeltaRotation = (PhysicsComponent->GetComponentRotation() - SnapTarget->GetComponentRotation()).GetNormalized();
	return FMath::Abs(DeltaRotation.Roll) <= RotationToleranceDegrees
		&& FMath::Abs(DeltaRotation.Pitch) <= RotationToleranceDegrees
		&& FMath::Abs(DeltaRotation.Yaw) <= RotationToleranceDegrees;
}

FTransform UInteractionPlacementTargetComponent::BuildSnapTransform(const UManipulatableObjectComponent* ManipulatedObject) const
{
	const USceneComponent* SnapTarget = SnapComponent ? SnapComponent.Get() : this;
	FTransform SnapTransform = SnapTarget->GetComponentTransform();

	if (ManipulatedObject)
	{
		SnapTransform.ConcatenateRotation(ManipulatedObject->HeldRotationOffset.Quaternion());
		SnapTransform.AddToTranslation(SnapTransform.TransformVectorNoScale(ManipulatedObject->HeldOffset));

		if (const UPrimitiveComponent* PhysicsComponent = ManipulatedObject->GetPrimaryPhysicsComponent())
		{
			SnapTransform.SetScale3D(PhysicsComponent->GetComponentScale());
		}
	}

	return SnapTransform;
}

bool UInteractionPlacementTargetComponent::TryPlaceObject(UManipulatableObjectComponent* ManipulatedObject)
{
	if (!IsObjectWithinPlacementTolerance(ManipulatedObject))
	{
		return false;
	}

	UPrimitiveComponent* PhysicsComponent = ManipulatedObject ? ManipulatedObject->GetPrimaryPhysicsComponent() : nullptr;
	if (!PhysicsComponent)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementTarget: TryPlaceObject failed target=%s object=%s reason=no physics component"),
			*GetNameSafe(this),
			*GetNameSafe(ManipulatedObject));
		return false;
	}

	if (bLockObjectWhenPlaced)
	{
		if (bUseSnapPlacement)
		{
			PhysicsComponent->SetWorldTransform(BuildSnapTransform(ManipulatedObject));
		}
		PhysicsComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
		PhysicsComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}

	PlacedObject = ManipulatedObject;
	OnValidObjectPlaced.Broadcast(this, ManipulatedObject);
	return true;
}

void UInteractionPlacementTargetComponent::RemovePlacedObject()
{
	if (!PlacedObject)
	{
		return;
	}

	UManipulatableObjectComponent* RemovedObject = PlacedObject.Get();
	PlacedObject = nullptr;
	OnPlacedObjectRemoved.Broadcast(this, RemovedObject);
}

bool UInteractionPlacementTargetComponent::HasPlacedObject() const
{
	return PlacedObject != nullptr;
}

bool UInteractionPlacementTargetComponent::UsesSnapPlacement() const
{
	return bUseSnapPlacement;
}

UPrimitiveComponent* UInteractionPlacementTargetComponent::GetResolvedTriggerVolume() const
{
	return ResolvedTriggerVolume.Get();
}

USceneComponent* UInteractionPlacementTargetComponent::GetEffectiveSnapComponent() const
{
	return SnapComponent ? SnapComponent.Get() : const_cast<UInteractionPlacementTargetComponent*>(this);
}

FVector UInteractionPlacementTargetComponent::GetEffectiveSnapLocation() const
{
	if (const USceneComponent* EffectiveSnap = GetEffectiveSnapComponent())
	{
		return EffectiveSnap->GetComponentLocation();
	}

	return GetComponentLocation();
}

float UInteractionPlacementTargetComponent::GetPlacementDistance(const UManipulatableObjectComponent* ManipulatedObject) const
{
	const UPrimitiveComponent* PhysicsComponent = ManipulatedObject ? ManipulatedObject->GetPrimaryPhysicsComponent() : nullptr;
	if (!PhysicsComponent)
	{
		return TNumericLimits<float>::Max();
	}

	return FVector::Dist(PhysicsComponent->GetComponentLocation(), GetEffectiveSnapLocation());
}

FText UInteractionPlacementTargetComponent::BuildPlacementDebugText(const UManipulatableObjectComponent* ManipulatedObject) const
{
	bool bCanAccept = false;
	bool bWithinTolerance = false;
	bool bRotationMismatch = false;
	float Distance = TNumericLimits<float>::Max();
	FText Reason = FText::GetEmpty();
	EvaluatePlacementDebug(ManipulatedObject, bCanAccept, bWithinTolerance, bRotationMismatch, Distance, Reason);

	if (!ManipulatedObject)
	{
		return FText::Format(
			NSLOCTEXT("InteractionPlacementTarget", "DebugNoObject", "{0}\nTol {1}"),
			FText::FromString(GetName()),
			FText::AsNumber(PositionTolerance));
	}

	const FString StateText = bWithinTolerance
		? TEXT("VALID")
		: bCanAccept
			? TEXT("RANGE")
			: TEXT("BLOCKED");

	return FText::Format(
		NSLOCTEXT("InteractionPlacementTarget", "DebugTextFmt", "{0}\n{1}\nDist {2} / Tol {3}\n{4}"),
		FText::FromString(GetName()),
		FText::FromString(StateText),
		FText::AsNumber(Distance),
		FText::AsNumber(PositionTolerance),
		Reason);
}

void UInteractionPlacementTargetComponent::DebugDraw(
	const UManipulatableObjectComponent* ManipulatedObject,
	const bool bIsBestCandidate,
	const float Duration) const
{
	if (!bEnableDebugDraw || !GetWorld() || !bUseSnapPlacement)
	{
		return;
	}

	const FVector SnapLocation = GetEffectiveSnapLocation();
	bool bCanAccept = false;
	bool bWithinTolerance = false;
	bool bRotationMismatch = false;
	float PlacementDistance = TNumericLimits<float>::Max();
	FText DebugReason = FText::GetEmpty();
	EvaluatePlacementDebug(ManipulatedObject, bCanAccept, bWithinTolerance, bRotationMismatch, PlacementDistance, DebugReason);

	const FColor ToleranceColor = bIsBestCandidate
		? FColor::Cyan
		: bWithinTolerance
			? FColor::Green
			: bCanAccept
				? FColor::Yellow
				: FColor::Red;

	DrawDebugSphere(GetWorld(), SnapLocation, PositionTolerance, 24, ToleranceColor, false, Duration, 0, 1.5f);
	DrawDebugPoint(GetWorld(), SnapLocation, 16.0f, bIsBestCandidate ? FColor::Blue : FColor::White, false, Duration, 0);

	if (ManipulatedObject)
	{
		DrawDebugString(
			GetWorld(),
			SnapLocation + FVector(0.0f, 0.0f, 12.0f),
			BuildPlacementDebugText(ManipulatedObject).ToString(),
			nullptr,
			ToleranceColor,
			Duration,
			false,
			0.85f);

		if (const UPrimitiveComponent* PhysicsComponent = ManipulatedObject->GetPrimaryPhysicsComponent())
		{
			DrawDebugLine(GetWorld(), PhysicsComponent->GetComponentLocation(), SnapLocation, ToleranceColor, false, Duration, 0, 0.75f);
		}
	}
}

#if WITH_EDITOR
void UInteractionPlacementTargetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UpdateEditorVisualization();
}

void UInteractionPlacementTargetComponent::UpdateEditorVisualization()
{
#if WITH_EDITORONLY_DATA
	const bool bShowEditorViz = bEnableDebugDraw && bEnableIdleDebugDraw && bUseSnapPlacement;
	const FVector SnapLocation = GetEffectiveSnapLocation();
	const FRotator SnapRotation = GetComponentRotation();

	if (EditorToleranceSphere)
	{
		EditorToleranceSphere->SetVisibility(bShowEditorViz);
		EditorToleranceSphere->SetWorldLocationAndRotation(SnapLocation, SnapRotation);
		EditorToleranceSphere->SetSphereRadius(PositionTolerance);
		EditorToleranceSphere->ShapeColor = HasPlacedObject() ? FColor(255, 140, 0) : FColor(150, 150, 150);
	}
#endif
}
#endif

void UInteractionPlacementTargetComponent::EvaluatePlacementDebug(
	const UManipulatableObjectComponent* ManipulatedObject,
	bool& bOutCanAccept,
	bool& bOutWithinTolerance,
	bool& bOutRotationMismatch,
	float& OutDistance,
	FText& OutReason) const
{
	bOutCanAccept = false;
	bOutWithinTolerance = false;
	bOutRotationMismatch = false;
	OutDistance = TNumericLimits<float>::Max();
	OutReason = FText::GetEmpty();

	if (!ManipulatedObject)
	{
		OutReason = NSLOCTEXT("InteractionPlacementTarget", "ReasonNoObject", "geen object");
		return;
	}

	if (bAllowOnlyOneObject && PlacedObject && PlacedObject != ManipulatedObject)
	{
		OutReason = NSLOCTEXT("InteractionPlacementTarget", "ReasonOccupied", "slot bezet");
		return;
	}

	if (!ManipulatedObject->IsValidForPlacementTag(PlacementTargetTag))
	{
		OutReason = NSLOCTEXT("InteractionPlacementTarget", "ReasonPlacementTag", "placement tag mismatch");
		return;
	}

	const bool bAcceptedByObjectTags =
		AcceptedObjectTags.IsEmpty()
		|| AcceptedObjectTags.HasTagExact(ManipulatedObject->ObjectRoleTag)
		|| AcceptedObjectTags.HasTagExact(ManipulatedObject->ManipulationTag);
	if (!bAcceptedByObjectTags)
	{
		OutReason = NSLOCTEXT("InteractionPlacementTarget", "ReasonAcceptedTags", "accepted object tags mismatch");
		return;
	}

	bOutCanAccept = true;

	const UPrimitiveComponent* PhysicsComponent = ManipulatedObject->GetPrimaryPhysicsComponent();
	if (!PhysicsComponent)
	{
		OutReason = NSLOCTEXT("InteractionPlacementTarget", "ReasonNoPhysics", "geen physics component");
		return;
	}

	if (bUseTriggerVolume && ResolvedTriggerVolume)
	{
		const bool bOverlapping = ResolvedTriggerVolume->IsOverlappingComponent(PhysicsComponent);
		bOutWithinTolerance = bOverlapping;
		OutReason = bOverlapping
			? NSLOCTEXT("InteractionPlacementTarget", "ReasonTriggerValid", "overlap geldig")
			: NSLOCTEXT("InteractionPlacementTarget", "ReasonTriggerInvalid", "geen overlap met trigger");
		return;
	}

	const USceneComponent* SnapTarget = GetEffectiveSnapComponent();
	OutDistance = FVector::Dist(PhysicsComponent->GetComponentLocation(), SnapTarget->GetComponentLocation());
	if (OutDistance > PositionTolerance)
	{
		OutReason = NSLOCTEXT("InteractionPlacementTarget", "ReasonOutOfRange", "buiten tolerance");
		return;
	}

	if (bRequireRotationMatch)
	{
		const FRotator DeltaRotation = (PhysicsComponent->GetComponentRotation() - SnapTarget->GetComponentRotation()).GetNormalized();
		const bool bRotationMatches =
			FMath::Abs(DeltaRotation.Roll) <= RotationToleranceDegrees
			&& FMath::Abs(DeltaRotation.Pitch) <= RotationToleranceDegrees
			&& FMath::Abs(DeltaRotation.Yaw) <= RotationToleranceDegrees;
		if (!bRotationMatches)
		{
			bOutRotationMismatch = true;
			OutReason = NSLOCTEXT("InteractionPlacementTarget", "ReasonRotationMismatch", "rotation mismatch");
			return;
		}
	}

	bOutWithinTolerance = true;
	OutReason = NSLOCTEXT("InteractionPlacementTarget", "ReasonValid", "geldig");
}

void UInteractionPlacementTargetComponent::HandleTriggerVolumeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	UManipulatableObjectComponent* ManipulatableObject = ResolveManipulatableFromOverlap(OtherActor, OtherComp);

	if (!ManipulatableObject)
	{
		return;
	}

	if (TryPlaceObject(ManipulatableObject))
	{
	}
}

void UInteractionPlacementTargetComponent::HandleTriggerVolumeEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherActor;
	(void)OtherBodyIndex;

	if (!PlacedObject)
	{
		return;
	}

	const UPrimitiveComponent* PlacedPhysics = PlacedObject->GetPrimaryPhysicsComponent();
	if (PlacedPhysics && OtherComp == PlacedPhysics)
	{
		RemovePlacedObject();
	}
}

UPrimitiveComponent* UInteractionPlacementTargetComponent::ResolveTriggerVolumeComponent() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	if (UPrimitiveComponent* ReferencedComponent = Cast<UPrimitiveComponent>(TriggerVolume.GetComponent(Owner)))
	{
		if (ReferencedComponent->GetOwner() == Owner)
		{
			return ReferencedComponent;
		}
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Owner->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	if (!TriggerVolumeComponentName.IsNone())
	{
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->GetFName() == TriggerVolumeComponentName)
			{
				return PrimitiveComponent;
			}
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementTarget: TriggerVolumeComponentName did not match any primitive component target=%s owner=%s componentName=%s"),
			*GetNameSafe(this),
			*GetNameSafe(Owner),
			*TriggerVolumeComponentName.ToString());
	}

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsUsableTriggerVolumeCandidate(PrimitiveComponent) || !PrimitiveComponent->IsAttachedTo(this))
		{
			continue;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementTarget: Auto-resolved attached trigger volume target=%s trigger=%s"),
			*GetNameSafe(this),
			*GetNameSafe(PrimitiveComponent));
		return PrimitiveComponent;
	}

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsUsableTriggerVolumeCandidate(PrimitiveComponent))
		{
			continue;
		}

		const FString ComponentName = PrimitiveComponent->GetName();
		if (ComponentName.Contains(TEXT("Trigger"))
			|| ComponentName.Contains(TEXT("Volume"))
			|| ComponentName.Contains(TEXT("Placement")))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("PlacementTarget: Auto-resolved named trigger volume target=%s trigger=%s"),
				*GetNameSafe(this),
				*GetNameSafe(PrimitiveComponent));
			return PrimitiveComponent;
		}
	}

	return nullptr;
}

bool UInteractionPlacementTargetComponent::IsUsableTriggerVolumeCandidate(const UPrimitiveComponent* CandidateComponent) const
{
	return CandidateComponent
		&& CandidateComponent->GetOwner() == GetOwner()
		&& CandidateComponent->GetGenerateOverlapEvents()
		&& CandidateComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
}

void UInteractionPlacementTargetComponent::ConfigureResolvedTriggerVolume()
{
	if (!bAutoConfigureTriggerVolumeCollision || !ResolvedTriggerVolume)
	{
		return;
	}

	ResolvedTriggerVolume->SetGenerateOverlapEvents(true);
	if (ResolvedTriggerVolume->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		ResolvedTriggerVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	ResolvedTriggerVolume->SetCollisionResponseToAllChannels(ECR_Overlap);
}

UManipulatableObjectComponent* UInteractionPlacementTargetComponent::ResolveManipulatableFromOverlap(AActor* OtherActor, UPrimitiveComponent* OtherComp) const
{
	if (OtherComp)
	{
		if (UManipulatableObjectComponent* OnOwner = OtherComp->GetOwner() ? OtherComp->GetOwner()->FindComponentByClass<UManipulatableObjectComponent>() : nullptr)
		{
			return OnOwner;
		}
	}

	return OtherActor ? OtherActor->FindComponentByClass<UManipulatableObjectComponent>() : nullptr;
}
