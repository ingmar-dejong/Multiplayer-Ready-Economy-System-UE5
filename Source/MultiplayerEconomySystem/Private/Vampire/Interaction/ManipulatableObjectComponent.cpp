#include "Vampire/Interaction/ManipulatableObjectComponent.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

UManipulatableObjectComponent::UManipulatableObjectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UManipulatableObjectComponent::CanBeGrabbed() const
{
	if (!bCanBeGrabbed)
	{
		return false;
	}

	const UPrimitiveComponent* PhysicsComponent = GetPrimaryPhysicsComponent();
	return PhysicsComponent && PhysicsComponent->IsSimulatingPhysics();
}

bool UManipulatableObjectComponent::IsValidForPlacementTag(const FGameplayTag PlacementTag) const
{
	return !PlacementTag.IsValid() || AllowedPlacementTags.IsEmpty() || AllowedPlacementTags.HasTagExact(PlacementTag);
}

UPrimitiveComponent* UManipulatableObjectComponent::GetPrimaryPhysicsComponent() const
{
	if (PrimaryPhysicsComponent && PrimaryPhysicsComponent->IsSimulatingPhysics())
	{
		return PrimaryPhysicsComponent;
	}

	if (AActor* OwnerActor = GetOwner())
	{
		TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(OwnerActor);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->IsSimulatingPhysics())
			{
				return PrimitiveComponent;
			}
		}

		return Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
	}

	return nullptr;
}
