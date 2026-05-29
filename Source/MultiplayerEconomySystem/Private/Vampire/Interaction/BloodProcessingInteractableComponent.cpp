#include "Vampire/Interaction/BloodProcessingInteractableComponent.h"

#include "Engine/Engine.h"
#include "Blueprint/UserWidget.h"
#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"
#include "Interaction/PlayerInteractionComponent.h"
#include "Items/InventoryComponent.h"
#include "UI/ProcessingStationMenuBase.h"
#include "Vampire/Data/BloodProcessingRecipeDataAsset.h"
#include "Vampire/Economy/VampireEconomyComponent.h"
#include "Vampire/Items/BloodProductItem.h"
#include "Vampire/World/BloodProcessingStation.h"

namespace
{
	bool CanMovePlacedStationFromInteractable(const ABloodProcessingStation* Station)
	{
		return Station
			&& Station->StationState == EBloodVatStationState::Leeg
			&& !Station->HasStagedManualProcessingRequest()
			&& !Station->HasSpawnedManualProcessingActor()
			&& !Station->IsMovePlacementInProgress()
			&& Station->HasRegisteredPlacementContext();
	}

	bool DoBloodItemsMatchForStationSelection(const UBloodProductItem* A, const UBloodProductItem* B)
	{
		return A
			&& B
			&& A->GetClass() == B->GetClass()
			&& A->SourceType == B->SourceType
			&& A->BaseQuality == B->BaseQuality
			&& A->ProcessingType == B->ProcessingType;
	}

	int32 GetTotalMatchingUnitsForSelection(const UOwnSystemInventoryComponent* Inventory, const UBloodProductItem* SeedItem)
	{
		if (!Inventory || !SeedItem)
		{
			return 0;
		}

		int32 TotalUnits = 0;
		for (UOwnSystemItem* Item : Inventory->GetItems())
		{
			if (const UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item))
			{
				if (DoBloodItemsMatchForStationSelection(SeedItem, Candidate))
				{
					TotalUnits += Candidate->BloodQuantity;
				}
			}
		}

		return TotalUnits;
	}

	void ReportProcessingDebug(const FText& Message, UVampireEconomyComponent* Economy, const bool bWasSuccessful)
	{
		if (Economy)
		{
			Economy->SetInteractionFeedback(Message, bWasSuccessful);
		}

		UE_LOG(LogTemp, Warning, TEXT("BloodProcessingStation: %s"), *Message.ToString());

		if (GEngine)
		{
			const FColor MessageColor = bWasSuccessful ? FColor::Green : FColor::Red;
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, MessageColor, Message.ToString());
		}
	}

	void SuspendProcessingInteraction(UBloodProcessingInteractableComponent* InteractableComp, APawn* Interactor)
	{
		if (!InteractableComp)
		{
			return;
		}

		if (AController* Controller = Interactor ? Interactor->GetController() : nullptr)
		{
			if (UPlayerInteractionComponent* PlayerInteraction = Controller->GetComponentByClass<UPlayerInteractionComponent>())
			{
				PlayerInteraction->ClearViewedInteractable();
			}
		}

		InteractableComp->Deactivate();
	}
}

FText UBloodProcessingInteractableComponent::GetInteractableActionText_Implementation(APawn* Interactor, UOwnSystemInteractionComponent* InteractionComp) const
{
	const ABloodProcessingStation* ProcessingStation = Cast<ABloodProcessingStation>(GetOwner());
	if (ProcessingStation && ProcessingStation->IsMovePlacementInProgress())
	{
		return NSLOCTEXT("BloodProcessingInteractable", "StationMovingAction", "Station wordt verplaatst");
	}

	const FText DefaultActionText = Super::GetInteractableActionText_Implementation(Interactor, InteractionComp);
	if (!CanMovePlacedStationFromInteractable(ProcessingStation))
	{
		return DefaultActionText;
	}

	return FText::Format(
		NSLOCTEXT("BloodProcessingInteractable", "InteractActionWithMoveFmt", "{0} | L Verplaatsen"),
		DefaultActionText);
}

void UBloodProcessingInteractableComponent::BeginFocus(APawn* Interactor, UOwnSystemInteractionComponent* InteractionComp)
{
	Super::BeginFocus(Interactor, InteractionComp);
	ApplyMoveInteractionHotkey(Interactor);
}

void UBloodProcessingInteractableComponent::EndFocus(APawn* Interactor, UOwnSystemInteractionComponent* InteractionComp)
{
	RemoveMoveInteractionHotkey();
	Super::EndFocus(Interactor, InteractionComp);
}

void UBloodProcessingInteractableComponent::SetSuppressedByMovePlacement(const bool bSuppress)
{
	if (bSuppress)
	{
		if (APawn* Interactor = FocusedInteractor.Get())
		{
			if (AController* Controller = Interactor->GetController())
			{
				if (UPlayerInteractionComponent* PlayerInteraction = Controller->GetComponentByClass<UPlayerInteractionComponent>())
				{
					PlayerInteraction->ClearViewedInteractable();
				}
			}
		}

		RemoveMoveInteractionHotkey();
		Deactivate();
		return;
	}

	Activate(true);
}

bool UBloodProcessingInteractableComponent::OpenStationMenuForInteractor(ABloodProcessingStation* ProcessingStation, APawn* Interactor)
{
	if (!ProcessingStation || !Interactor)
	{
		return false;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Interactor->GetController());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		return false;
	}

	ProcessingStation->SetActiveInteractorContext(Interactor);

	UBloodProcessingInteractableComponent* ProcessingInteractable = ProcessingStation->ProcessingInteractable;
	if (!ProcessingInteractable)
	{
		return false;
	}

	if (TSubclassOf<UProcessingStationMenuBase> ProcessingMenuClass = ProcessingStation->GetResolvedProcessingMenuClass())
	{
		if (UProcessingStationMenuBase* ExistingMenu = ProcessingInteractable->ActiveProcessingMenu.Get())
		{
			SuspendProcessingInteraction(ProcessingInteractable, Interactor);
			ExistingMenu->SetProcessingStationContext(ProcessingStation, Interactor);
			if (!ExistingMenu->IsInViewport())
			{
				ExistingMenu->AddToViewport();
			}
			ExistingMenu->RefreshMenu();
			if (ProcessingStation->bUseInteractionCamera)
			{
				PlayerController->SetViewTargetWithBlend(ProcessingStation, ProcessingStation->InteractionCameraBlendTime);
			}
			ProcessingStation->ApplyInteractionInputContext(PlayerController);
			PlayerController->SetShowMouseCursor(true);
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetWidgetToFocus(ExistingMenu->TakeWidget());
			PlayerController->SetInputMode(InputMode);
			ExistingMenu->SetKeyboardFocus();
			return true;
		}

		if (UProcessingStationMenuBase* ProcessingMenu = CreateWidget<UProcessingStationMenuBase>(PlayerController, ProcessingMenuClass))
		{
			SuspendProcessingInteraction(ProcessingInteractable, Interactor);
			ProcessingMenu->SetProcessingStationContext(ProcessingStation, Interactor);
			ProcessingMenu->AddToViewport();
			ProcessingMenu->RefreshMenu();
			ProcessingInteractable->ActiveProcessingMenu = ProcessingMenu;

			if (ProcessingStation->bUseInteractionCamera)
			{
				PlayerController->SetViewTargetWithBlend(ProcessingStation, ProcessingStation->InteractionCameraBlendTime);
			}
			ProcessingStation->ApplyInteractionInputContext(PlayerController);
			PlayerController->SetShowMouseCursor(true);
			FInputModeGameAndUI InputMode;
			InputMode.SetHideCursorDuringCapture(false);
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetWidgetToFocus(ProcessingMenu->TakeWidget());
			PlayerController->SetInputMode(InputMode);
			ProcessingMenu->SetKeyboardFocus();
			return true;
		}
	}

	return false;
}

void UBloodProcessingInteractableComponent::ApplyMoveInteractionHotkey(APawn* Interactor)
{
	ABloodProcessingStation* ProcessingStation = Cast<ABloodProcessingStation>(GetOwner());
	if (!CanMovePlacedStationFromInteractable(ProcessingStation) || !Interactor)
	{
		RemoveMoveInteractionHotkey();
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(Interactor->GetController());
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		RemoveMoveInteractionHotkey();
		return;
	}

	if (FocusedPlayerController.IsValid() && FocusedPlayerController.Get() != PlayerController)
	{
		RemoveMoveInteractionHotkey();
	}

	if (!FocusMoveHotkeyInputComponent)
	{
		FocusMoveHotkeyInputComponent = NewObject<UInputComponent>(PlayerController, TEXT("FocusedStationMoveHotkey"));
	}

	if (!FocusMoveHotkeyInputComponent)
	{
		return;
	}

	if (FocusedPlayerController.Get() == PlayerController)
	{
		PlayerController->PopInputComponent(FocusMoveHotkeyInputComponent);
	}

	FocusedInteractor = Interactor;
	FocusedPlayerController = PlayerController;
	FocusMoveHotkeyInputComponent->ClearActionBindings();
	FocusMoveHotkeyInputComponent->Priority = 1002;
	FocusMoveHotkeyInputComponent->bBlockInput = false;
	FocusMoveHotkeyInputComponent->BindKey(EKeys::L, IE_Pressed, this, &UBloodProcessingInteractableComponent::HandleFocusedMovePressed);
	PlayerController->PushInputComponent(FocusMoveHotkeyInputComponent);
}

void UBloodProcessingInteractableComponent::RemoveMoveInteractionHotkey()
{
	if (FocusMoveHotkeyInputComponent && FocusedPlayerController.IsValid())
	{
		FocusedPlayerController->PopInputComponent(FocusMoveHotkeyInputComponent);
	}

	FocusedInteractor.Reset();
	FocusedPlayerController.Reset();
}

void UBloodProcessingInteractableComponent::HandleFocusedMovePressed()
{
	ABloodProcessingStation* ProcessingStation = Cast<ABloodProcessingStation>(GetOwner());
	APawn* Interactor = FocusedInteractor.Get();
	APlayerController* PlayerController = FocusedPlayerController.Get();
	if (!CanMovePlacedStationFromInteractable(ProcessingStation) || !Interactor || !PlayerController)
	{
		return;
	}

	if (UPlayerInteractionComponent* PlayerInteraction = PlayerController->GetComponentByClass<UPlayerInteractionComponent>())
	{
		PlayerInteraction->ClearViewedInteractable();
	}

	FText Reason;
	if (UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor))
	{
		const bool bStarted = Economy->StartMovePlacedStationMode(ProcessingStation, Reason);
		if (!Reason.IsEmpty())
		{
			Economy->SetInteractionFeedback(Reason, bStarted);
		}
	}
}

void UBloodProcessingInteractableComponent::OnInteract_Implementation(APawn* Interactor, UOwnSystemInteractionComponent* InteractionComp)
{
	Super::OnInteract_Implementation(Interactor, InteractionComp);

	ABloodProcessingStation* ProcessingStation = Cast<ABloodProcessingStation>(GetOwner());
	if (!Interactor)
	{
		return;
	}

	UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor);
	UOwnSystemInventoryComponent* Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(Interactor);
	if (!ProcessingStation)
	{
		ReportProcessingDebug(NSLOCTEXT("BloodProcessingInteractable", "NoProcessingStation", "Interactie mislukt: owner is geen BloodProcessingStation."), Economy, false);
		return;
	}

	if (!ProcessingStation->GetActiveRecipe())
	{
		ReportProcessingDebug(NSLOCTEXT("BloodProcessingInteractable", "NoRecipeAssigned", "Interactie mislukt: dit processing station heeft geen actieve recipe toegewezen."), Economy, false);
		return;
	}

	if (!Economy)
	{
		ReportProcessingDebug(NSLOCTEXT("BloodProcessingInteractable", "NoEconomyFound", "Interactie mislukt: geen VampireEconomyComponent gevonden op de interactor."), nullptr, false);
		return;
	}

	if (!Inventory)
	{
		ReportProcessingDebug(NSLOCTEXT("BloodProcessingInteractable", "NoInventoryFound", "Interactie mislukt: geen OwnSystemInventoryComponent gevonden op de interactor."), Economy, false);
		return;
	}

	if (ProcessingStation->GetResolvedProcessingMenuClass())
	{
		FText OpenReason;
		const bool bRequested = Economy->RequestOpenProcessingStationMenu(ProcessingStation, OpenReason);
		if (!bRequested)
		{
			ReportProcessingDebug(OpenReason, Economy, bRequested);
		}
		return;
	}

	FText InteractionFeedback;
	if (ProcessingStation->StationState == EBloodVatStationState::Klaar)
	{
		const bool bHarvested = Economy->RequestHarvestProcessedBatch(ProcessingStation, InteractionFeedback);
		ReportProcessingDebug(
			bHarvested ? InteractionFeedback : FText::Format(
				NSLOCTEXT("BloodProcessingInteractable", "HarvestFailedFmt", "{0}\n{1}"),
				ProcessingStation->BuildStationStatusText(),
				InteractionFeedback),
			Economy,
			bHarvested);
		return;
	}

	if (ProcessingStation->StationState == EBloodVatStationState::Rijpt)
	{
		ReportProcessingDebug(ProcessingStation->BuildStationStatusText(), Economy, true);
		return;
	}

	if (ProcessingStation->RequiresManualProcessingFlow())
	{
		ReportProcessingDebug(
			NSLOCTEXT("BloodProcessingInteractable", "ManualFlowRequiresMenu", "Dit station gebruikt nu een handmatige flow. Open het stationmenu om eerst een batch te kiezen."),
			Economy,
			true);
		return;
	}

	UBloodProductItem* BloodItemToProcess = nullptr;
	int32 BloodItemUnitsToProcess = 0;
	FText LastRejectReason;
	for (UOwnSystemItem* Item : Inventory->GetItems())
	{
		if (UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item))
		{
			const int32 TotalMatchingUnits = GetTotalMatchingUnitsForSelection(Inventory, Candidate);
			FText CandidateReason;
			if (ProcessingStation->CanAcceptBloodItem(Candidate, TotalMatchingUnits, CandidateReason))
			{
				BloodItemToProcess = Candidate;
				BloodItemUnitsToProcess = TotalMatchingUnits;
				break;
			}

			if (LastRejectReason.IsEmpty())
			{
				LastRejectReason = CandidateReason;
			}
		}
	}

	if (!BloodItemToProcess)
	{
		ReportProcessingDebug(
			LastRejectReason.IsEmpty()
				? NSLOCTEXT("BloodProcessingInteractable", "NoValidBloodBatch", "Geen geldige verse blood batch van minstens 10 units beschikbaar voor dit vat.")
				: LastRejectReason,
			Economy,
			false);
		return;
	}

	const bool bStarted = Economy->RequestStartProcessing(ProcessingStation, BloodItemToProcess, BloodItemUnitsToProcess, InteractionFeedback);
	ReportProcessingDebug(
		bStarted ? InteractionFeedback : FText::Format(
			NSLOCTEXT("BloodProcessingInteractable", "StartFailedFmt", "{0}\n{1}"),
			ProcessingStation->BuildStationStatusText(),
			InteractionFeedback),
		Economy,
		bStarted);
}
