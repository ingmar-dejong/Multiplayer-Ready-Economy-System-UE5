#include "Vampire/Interaction/BloodHarvestInteractableComponent.h"

#include "Items/InventoryComponent.h"
#include "Vampire/Economy/VampireEconomyComponent.h"
#include "Vampire/World/BloodHarvestPoint.h"

void UBloodHarvestInteractableComponent::OnInteract_Implementation(APawn* Interactor, UOwnSystemInteractionComponent* InteractionComp)
{
	Super::OnInteract_Implementation(Interactor, InteractionComp);

	ABloodHarvestPoint* HarvestPoint = Cast<ABloodHarvestPoint>(GetOwner());
	if (!HarvestPoint || !HarvestPoint->HarvestSourceData || !Interactor)
	{
		return;
	}

	UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor);
	UOwnSystemInventoryComponent* Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(Interactor);
	if (!Economy || !Inventory)
	{
		return;
	}

	UBloodProductItem* CreatedItem = nullptr;
	Economy->CreateHarvestedBloodItem(Inventory, HarvestPoint->HarvestSourceData, CreatedItem);
}
