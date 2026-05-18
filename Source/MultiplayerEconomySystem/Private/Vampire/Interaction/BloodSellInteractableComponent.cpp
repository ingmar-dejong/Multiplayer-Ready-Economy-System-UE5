#include "Vampire/Interaction/BloodSellInteractableComponent.h"

#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Items/InventoryComponent.h"
#include "UI/VampireEconomySummaryMenu.h"
#include "Vampire/Economy/VampireEconomyComponent.h"
#include "Vampire/Items/BloodProductItem.h"
#include "Vampire/World/BloodBuyerNPC.h"

void UBloodSellInteractableComponent::OnInteract_Implementation(APawn* Interactor, UOwnSystemInteractionComponent* InteractionComp)
{
	Super::OnInteract_Implementation(Interactor, InteractionComp);

	ABloodBuyerNPC* Buyer = Cast<ABloodBuyerNPC>(GetOwner());
	if (!Buyer || !Buyer->BuyerData || !Interactor)
	{
		return;
	}

	UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor);
	UOwnSystemInventoryComponent* Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(Interactor);
	if (!Economy || !Inventory)
	{
		return;
	}

	if (Buyer->BuyerMenuClass)
	{
		APlayerController* PlayerController = Cast<APlayerController>(Interactor->GetController());
		if (PlayerController)
		{
			if (UVampireEconomySummaryMenu* ExistingMenu = ActiveBuyerMenu.Get())
			{
				ExistingMenu->SetBuyerContext(Buyer, Interactor);
				if (!ExistingMenu->IsInViewport())
				{
					ExistingMenu->AddToViewport();
				}
				ExistingMenu->RefreshSummary();
				PlayerController->SetShowMouseCursor(true);
				FInputModeGameAndUI InputMode;
				InputMode.SetHideCursorDuringCapture(false);
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PlayerController->SetInputMode(InputMode);
				return;
			}

			if (UVampireEconomySummaryMenu* BuyerMenu = CreateWidget<UVampireEconomySummaryMenu>(PlayerController, Buyer->BuyerMenuClass))
			{
				BuyerMenu->SetBuyerContext(Buyer, Interactor);
				BuyerMenu->AddToViewport();
				BuyerMenu->RefreshSummary();
				ActiveBuyerMenu = BuyerMenu;

				PlayerController->SetShowMouseCursor(true);
				FInputModeGameAndUI InputMode;
				InputMode.SetHideCursorDuringCapture(false);
				InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
				PlayerController->SetInputMode(InputMode);
				return;
			}
		}
	}

	UBloodProductItem* BloodItemToSell = nullptr;
	FText Reason;
	for (UOwnSystemItem* Item : Inventory->GetItems())
	{
		if (UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item))
		{
			if (Economy->CanSellBloodItemToBuyer(Candidate, Buyer->BuyerData, Reason))
			{
				BloodItemToSell = Candidate;
				break;
			}
		}
	}

	if (!BloodItemToSell)
	{
		return;
	}

	int32 Payout = 0;
	Economy->SellBloodItemToBuyer(Inventory, BloodItemToSell, Buyer->BuyerData, Payout, Reason);
}
