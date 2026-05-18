#include "Vampire/World/BloodPackagingStation.h"

#include "Items/InventoryComponent.h"
#include "Items/OwnSystemItem.h"
#include "Net/UnrealNetwork.h"
#include "Components/PrimitiveComponent.h"
#include "Vampire/Interaction/InteractionPlacementTargetComponent.h"
#include "Vampire/Interaction/ManipulatableObjectComponent.h"
#include "Vampire/Data/BloodProcessingRecipeDataAsset.h"
#include "Vampire/Economy/VampireEconomyComponent.h"
#include "Vampire/Items/BloodProductItem.h"

namespace
{
	bool DoesBloodItemMatchPackagingRequest(const UBloodProductItem* Item, const FBloodProcessingStartRequest& Request)
	{
		return Item
			&& Item->GetClass() == Request.ItemClass
			&& Item->SourceType == Request.SourceType
			&& Item->BaseQuality == Request.BaseQuality
			&& Item->ProcessingType == Request.ProcessingType
			&& Item->CreatedDay == Request.CreatedDay;
	}

		bool ConsumeMatchingUnitsForPackagingRequest(UOwnSystemInventoryComponent* Inventory, const FBloodProcessingStartRequest& Request)
		{
		if (!Inventory || !Request.ItemClass || Request.BloodQuantity <= 0)
		{
			return false;
		}

		int32 RemainingUnits = Request.BloodQuantity;
		TArray<UOwnSystemItem*> Items = Inventory->GetItems();
		for (UOwnSystemItem* Item : Items)
		{
			UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item);
			if (!DoesBloodItemMatchPackagingRequest(Candidate, Request))
			{
				continue;
			}

			const int32 UnitsFromThisItem = FMath::Min(Candidate->BloodQuantity, RemainingUnits);
			Candidate->BloodQuantity -= UnitsFromThisItem;
			RemainingUnits -= UnitsFromThisItem;

			if (Candidate->BloodQuantity <= 0)
			{
				Inventory->RemoveItem(Candidate);
			}
			else
			{
				Candidate->RefreshPresentation();
			}

			if (RemainingUnits <= 0)
			{
				return true;
			}
		}

		return RemainingUnits <= 0;
	}

	bool AddBloodItemFromRequestToInventory(
		UOwnSystemInventoryComponent* Inventory,
		const FBloodProcessingStartRequest& Request,
		const EBloodProcessingType ResultProcessingType,
		const bool bMarkAsPackaged,
		const EBloodProcessingType SourceProcessingType,
		UBloodProductItem*& OutInventoryItem,
		FText& OutReason)
	{
		OutInventoryItem = nullptr;
		OutReason = FText::GetEmpty();

		if (!Inventory || !Request.ItemClass)
		{
			OutReason = NSLOCTEXT("BloodPackagingStation", "PackagingRestoreMissingInventory", "Inventory of batchdata ontbreekt voor deze packaging-actie.");
			return false;
		}

		const FItemAddResult AddResult = Inventory->TryAddItemFromClass(Request.ItemClass, 1, false);
		if (AddResult.AmountGiven <= 0 || AddResult.Stacks.IsEmpty())
		{
			OutReason = NSLOCTEXT("BloodPackagingStation", "PackagingRestoreAddFailed", "Het blood item kon niet aan de inventory worden toegevoegd.");
			return false;
		}

		UBloodProductItem* InventoryItem = Cast<UBloodProductItem>(AddResult.Stacks[0]);
		if (!InventoryItem)
		{
			OutReason = NSLOCTEXT("BloodPackagingStation", "PackagingRestoreWrongItemClass", "De inventory output heeft een ongeldige item class.");
			return false;
		}

		InventoryItem->SourceType = Request.SourceType;
		InventoryItem->BaseQuality = Request.BaseQuality;
		InventoryItem->ProcessingType = ResultProcessingType;
		InventoryItem->bHasPackagedSourceProcessing = bMarkAsPackaged;
		InventoryItem->PackagedSourceProcessingType = bMarkAsPackaged ? SourceProcessingType : EBloodProcessingType::Vers;
		InventoryItem->BloodQuantity = Request.BloodQuantity;
		InventoryItem->CreatedDay = Request.CreatedDay;
		InventoryItem->RefreshPresentation();

		OutInventoryItem = InventoryItem;
		return true;
	}
}

#define LOCTEXT_NAMESPACE "BloodPackagingStation"

ABloodPackagingStation::ABloodPackagingStation()
{
	StationDisplayName = LOCTEXT("DefaultPackagingStationName", "Afvulstation");
	SyncPackagingRequirements();
	ReservedPackagingInventory = nullptr;

	RecipeData = CreateDefaultSubobject<UBloodProcessingRecipeDataAsset>(TEXT("PackagingRecipe"));
	if (RecipeData)
	{
		RecipeData->RecipeId = TEXT("PackagedAgedBlood");
		RecipeData->RecipeName = LOCTEXT("PackagingRecipeName", "Handelsklaar maken");
		RecipeData->RequiredInputProcessing = EBloodProcessingType::GerijptOpHout;
		RecipeData->bAcceptAnyInputProcessing = true;
		RecipeData->MinimumQuality = EBloodQuality::Gewoon;
		RecipeData->OutputProcessing = EBloodProcessingType::Handelsklaar;
		RecipeData->ProcessFactText = LOCTEXT("PackagingProcessFact", "De batch wordt afgevuld en klaargemaakt voor discrete verkoop.");
		RecipeData->ProcessingDurationDays = 0;
		RecipeData->AdditionalDurationPerSourceScore = 0;
		RecipeData->GoldCost = 0;
	}

	EnsurePackagingRecipeConstraints();
}

void ABloodPackagingStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABloodPackagingStation, PlacedPackagingItemCount);
	DOREPLIFETIME(ABloodPackagingStation, PlacedPackagingSlots);
}

bool ABloodPackagingStation::RequiresManualProcessingFlow() const
{
	return true;
}

bool ABloodPackagingStation::CanAcceptBloodItem(const UBloodProductItem* BloodItem, const int32 TotalAvailableUnits, FText& OutReason) const
{
	if (!BloodItem)
	{
		return Super::CanAcceptBloodItem(BloodItem, TotalAvailableUnits, OutReason);
	}

	if (BloodItem->ProcessingType == EBloodProcessingType::Handelsklaar || BloodItem->bHasPackagedSourceProcessing)
	{
		OutReason = LOCTEXT("PackagingRejectAlreadyPackaged", "Handelsklaar bloed is al verpakt en kan niet opnieuw worden ingepakt.");
		return false;
	}

	return Super::CanAcceptBloodItem(BloodItem, TotalAvailableUnits, OutReason);
}

void ABloodPackagingStation::OnConstruction(const FTransform& Transform)
{
	EnsurePackagingRecipeConstraints();
	SyncPackagingRequirements();
	Super::OnConstruction(Transform);
}

void ABloodPackagingStation::BeginPlay()
{
	EnsurePackagingRecipeConstraints();
	SyncPackagingRequirements();
	Super::BeginPlay();

	ResolvedPackagingPlacementTargets.Reset();
	for (const FComponentReference& TargetReference : PackagingPlacementTargets)
	{
		if (UActorComponent* ReferencedComponent = TargetReference.GetComponent(this))
		{
			if (UInteractionPlacementTargetComponent* PlacementTarget = Cast<UInteractionPlacementTargetComponent>(ReferencedComponent))
			{
				PlacementTarget->OnValidObjectPlaced.RemoveDynamic(this, &ABloodPackagingStation::HandlePackagingPlacementConfirmed);
				PlacementTarget->OnValidObjectPlaced.AddDynamic(this, &ABloodPackagingStation::HandlePackagingPlacementConfirmed);
				ResolvedPackagingPlacementTargets.Add(PlacementTarget);
			}
		}
	}
}

void ABloodPackagingStation::EnsurePackagingRecipeConstraints()
{
	if (!RecipeData)
	{
		return;
	}

	RecipeData->RequiredInputProcessing = EBloodProcessingType::GerijptOpHout;
	RecipeData->bAcceptAnyInputProcessing = true;
	RecipeData->OutputProcessing = EBloodProcessingType::Handelsklaar;
}

bool ABloodPackagingStation::PrepareManualProcessingRequest(const UBloodProductItem* BloodItem, const int32 TotalAvailableUnits, FText& OutReason)
{
	SyncPackagingRequirements();
	RefreshPackagingPlacementState();

	if (PlacedPackagingItemCount >= RequiredPackagingItemCount)
	{
		OutReason = FText::Format(
			LOCTEXT("PackagingBoxFull", "De doos is vol: {0}/{1} items geplaatst."),
			FText::AsNumber(PlacedPackagingItemCount),
			FText::AsNumber(RequiredPackagingItemCount));
		return false;
	}

	if (HasStagedManualProcessingRequest())
	{
		return Super::PrepareManualProcessingRequest(BloodItem, TotalAvailableUnits, OutReason);
	}

	APawn* Interactor = ActiveInteractor.Get();
	UOwnSystemInventoryComponent* Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(Interactor);
	if (!Inventory)
	{
		OutReason = LOCTEXT("PackagingMissingInventory", "Geen inventory beschikbaar voor het reserveren van de packaging-batch.");
		return false;
	}

	FBloodProcessingStartRequest Request;
	if (!BuildProcessingStartRequest(BloodItem, TotalAvailableUnits, Request, OutReason))
	{
		return false;
	}

	Request.BloodQuantity = FMath::Clamp(Request.BloodQuantity, RequiredPackagingItemCount, FMath::Max(RequiredPackagingItemCount, MaxReservedBatchUnits));

	if (!SpawnManualPreviewActorForRequest(Request, OutReason))
	{
		return false;
	}

	if (!ConsumeMatchingUnitsForPackagingRequest(Inventory, Request))
	{
		DestroyManualPreviewActor();
		OutReason = LOCTEXT("PackagingReserveFailed", "De geselecteerde batch kon niet worden gereserveerd voor packaging.");
		return false;
	}

	StagedManualStartRequest = Request;
	bHasStagedManualStartRequest = true;
	ReservedPackagingInventory = Inventory;
	ForceNetUpdate();
	OutReason = FText::Format(
		LOCTEXT("PackagingReservedFmt", "{0}: {1} units gereserveerd voor packaging en 1 packaging-item staat nu klaar op tafel."),
		StationDisplayName,
		FText::AsNumber(Request.BloodQuantity));
	return true;
}

void ABloodPackagingStation::HandleAbandonedManualFlow()
{
	if (!HasStagedManualProcessingRequest() && PlacedPackagingSlots.IsEmpty() && !HasSpawnedManualProcessingActor())
	{
		return;
	}

	FText OutReason;
	UOwnSystemInventoryComponent* Inventory = ReservedPackagingInventory.Get();
	if (!Inventory)
	{
		if (APawn* Interactor = ActiveInteractor.Get())
		{
			Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(Interactor);
		}

		if (!Inventory)
		{
			Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(CurrentOperator.Get());
		}
	}

	if (HasStagedManualProcessingRequest() && Inventory)
	{
		UBloodProductItem* RestoredItem = nullptr;
		const bool bRestored = AddBloodItemFromRequestToInventory(
			Inventory,
			StagedManualStartRequest,
			StagedManualStartRequest.ProcessingType,
			false,
			StagedManualStartRequest.ProcessingType,
			RestoredItem,
			OutReason);
		if (!bRestored)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("BloodPackagingStation: Failed to restore abandoned reserved batch for station=%s reason=%s"),
				*GetNameSafe(this),
				*OutReason.ToString());
		}
	}
	else if (HasStagedManualProcessingRequest())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("BloodPackagingStation: Abandoned reserved batch on station=%s could not be restored because no inventory was available"),
			*GetNameSafe(this));
	}

	ResetPackagingPlacementState();
	ClearStagedManualProcessingRequest();
}

void ABloodPackagingStation::RefreshPackagingPlacementState()
{
	SyncPackagingRequirements();
	RebuildPackagingPlacementRuntimeState();
	PlacedPackagingItemCount = FMath::Clamp(PlacedPackagingSlots.Num(), 0, RequiredPackagingItemCount);
}

bool ABloodPackagingStation::TryCommitPackagedBatch(FText& OutReason)
{
	OutReason = FText::GetEmpty();
	RefreshPackagingPlacementState();

	if (!HasStagedManualProcessingRequest())
	{
		OutReason = LOCTEXT("PackagingNoStagedRequest", "Er staat geen packaging-batch klaar.");
		return false;
	}

	if (PlacedPackagingItemCount < RequiredPackagingItemCount)
	{
		OutReason = FText::Format(
			LOCTEXT("PackagingNotFullYet", "De doos is nog niet vol: {0}/{1} items geplaatst."),
			FText::AsNumber(PlacedPackagingItemCount),
			FText::AsNumber(RequiredPackagingItemCount));
		return false;
	}

	APawn* Interactor = ActiveInteractor.Get();
	UOwnSystemInventoryComponent* Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(Interactor);
	UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor);
	if (!Inventory || !Economy)
	{
		OutReason = LOCTEXT("PackagingMissingInventoryOrEconomy", "Inventory of economy component ontbreekt voor packaging.");
		return false;
	}

	const UBloodProcessingRecipeDataAsset* ActiveRecipe = GetActiveRecipe();
	if (!ActiveRecipe)
	{
		OutReason = LOCTEXT("PackagingNoActiveRecipe", "Dit packaging station heeft geen actieve recipe.");
		return false;
	}

	const FBloodProcessingStartRequest Request = StagedManualStartRequest;
	UBloodProductItem* PackagedItem = nullptr;
	if (!AddBloodItemFromRequestToInventory(
		Inventory,
		Request,
		ActiveRecipe->OutputProcessing,
		true,
		Request.ProcessingType,
		PackagedItem,
		OutReason))
	{
		return false;
	}

	ResetPackagingPlacementState();
	ClearStagedManualProcessingRequest();
	ReservedPackagingInventory = nullptr;
	ForceNetUpdate();

	OutReason = FText::Format(
		LOCTEXT("PackagingCommittedFmt", "{0}: batch verpakt en toegevoegd aan inventory als {1}."),
		StationDisplayName,
		PackagedItem->GetBloodDisplayName());
	return true;
}

bool ABloodPackagingStation::CancelReservedPackagingRequest(FText& OutReason)
{
	OutReason = FText::GetEmpty();
	RefreshPackagingPlacementState();

	if (!HasStagedManualProcessingRequest())
	{
		OutReason = LOCTEXT("PackagingCancelNoRequest", "Er staat geen packaging-batch klaar om te annuleren.");
		return false;
	}

	APawn* Interactor = ActiveInteractor.Get();
	UOwnSystemInventoryComponent* Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(Interactor);
	if (!Inventory)
	{
		OutReason = LOCTEXT("PackagingCancelMissingInventory", "Geen inventory beschikbaar om de gereserveerde batch terug te zetten.");
		return false;
	}

	const FBloodProcessingStartRequest Request = StagedManualStartRequest;
	UBloodProductItem* RestoredItem = nullptr;
	if (!AddBloodItemFromRequestToInventory(
		Inventory,
		Request,
		Request.ProcessingType,
		false,
		Request.ProcessingType,
		RestoredItem,
		OutReason))
	{
		return false;
	}

	ResetPackagingPlacementState();
	ClearStagedManualProcessingRequest();
	ReservedPackagingInventory = nullptr;
	ForceNetUpdate();

	OutReason = FText::Format(
		LOCTEXT("PackagingCancelSuccessFmt", "{0}: packaging geannuleerd. {1} units teruggezet naar inventory."),
		StationDisplayName,
		FText::AsNumber(Request.BloodQuantity));
	return true;
}

bool ABloodPackagingStation::ConfirmPackagingPlacement(const int32 SlotIndex, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!HasStagedManualProcessingRequest())
	{
		OutReason = LOCTEXT("PackagingConfirmNoRequest", "Er staat geen packaging-batch klaar.");
		return false;
	}

	AActor* PreviewActor = SpawnedManualPreviewActor.Get();
	if (!PreviewActor)
	{
		OutReason = LOCTEXT("PackagingConfirmNoPreview", "Er ligt geen actief packaging-item op tafel.");
		return false;
	}

	if (!AssignPlacedPackagingActorToSlot(SlotIndex, PreviewActor, OutReason))
	{
		return false;
	}

	const bool bCommitted = PlacedPackagingItemCount >= RequiredPackagingItemCount && TryCommitPackagedBatch(OutReason);
	if (!bCommitted && OutReason.IsEmpty())
	{
		OutReason = FText::Format(
			LOCTEXT("PackagingPlacementProgressFeedback", "Packaging-item geplaatst: {0}/{1} in de doos."),
			FText::AsNumber(PlacedPackagingItemCount),
			FText::AsNumber(RequiredPackagingItemCount));
	}

	return true;
}

void ABloodPackagingStation::OnRep_PlacedPackagingSlots()
{
	RefreshPackagingPlacementState();
}

void ABloodPackagingStation::CleanupLoosePackagingPreviewActor()
{
	if (GetLocalRole() < ROLE_Authority)
	{
		return;
	}

	RefreshPackagingPlacementState();

	AActor* LoosePreviewActor = SpawnedManualPreviewActor.Get();
	if (!LoosePreviewActor)
	{
		return;
	}

	if (LoosePreviewActor->GetOwner() == this)
	{
		LoosePreviewActor->Destroy();
	}

	SpawnedManualPreviewActor = nullptr;
}

int32 ABloodPackagingStation::GetPlacedPackagingItemCount() const
{
	return PlacedPackagingItemCount;
}

int32 ABloodPackagingStation::GetRequiredPackagingItemCount() const
{
	return RequiredPackagingItemCount;
}

const FBloodProcessingStartRequest* ABloodPackagingStation::GetReservedPackagingRequest() const
{
	return HasStagedManualProcessingRequest() ? &StagedManualStartRequest : nullptr;
}

void ABloodPackagingStation::HandlePackagingPlacementConfirmed(
	UInteractionPlacementTargetComponent* TargetComponent,
	UManipulatableObjectComponent* ManipulatedObject)
{
	if (!TargetComponent || !ManipulatedObject || !HasStagedManualProcessingRequest())
	{
		return;
	}

	AActor* ManipulatedActor = ManipulatedObject->GetOwner();
	if (!ManipulatedActor || SpawnedManualPreviewActor != ManipulatedActor)
	{
		return;
	}

	const int32 SlotIndex = ResolvePackagingPlacementTargetIndex(TargetComponent);
	if (SlotIndex == INDEX_NONE)
	{
		return;
	}

	if (APawn* Interactor = ActiveInteractor.Get())
	{
		if (UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor))
		{
			if (Economy->GetOwnerRole() < ROLE_Authority)
			{
				FText RequestReason;
				Economy->RequestConfirmPackagingPlacement(this, SlotIndex, RequestReason);
				return;
			}
		}
	}

	FText ConfirmReason;
	if (!AssignPlacedPackagingActorToSlot(SlotIndex, ManipulatedActor, ConfirmReason))
	{
		if (!ConfirmReason.IsEmpty())
		{
			if (APawn* Interactor = ActiveInteractor.Get())
			{
				if (UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor))
				{
					Economy->SetInteractionFeedback(ConfirmReason, false);
				}
			}
		}
		return;
	}

	FText CommitReason;
	const bool bCommitted = PlacedPackagingItemCount >= RequiredPackagingItemCount && TryCommitPackagedBatch(CommitReason);
	if (bCommitted)
	{
		if (APawn* Interactor = ActiveInteractor.Get())
		{
			if (UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor))
			{
				Economy->SetInteractionFeedback(CommitReason, true);
			}
		}
		return;
	}

	if (!CommitReason.IsEmpty())
	{
		if (APawn* Interactor = ActiveInteractor.Get())
		{
			if (UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor))
			{
				Economy->SetInteractionFeedback(CommitReason, false);
			}
		}
		return;
	}

	BroadcastPackagingProgressFeedback();
}

void ABloodPackagingStation::SyncPackagingRequirements()
{
	RequiredPackagingItemCount = FMath::Max(1, RequiredPackagingItemCount);
	MaxReservedBatchUnits = FMath::Max(RequiredPackagingItemCount, MaxReservedBatchUnits);
	MinimumUnitsRequired = RequiredPackagingItemCount;
}

void ABloodPackagingStation::BroadcastPackagingProgressFeedback()
{
	APawn* Interactor = ActiveInteractor.Get();
	UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor);
	if (!Economy)
	{
		return;
	}

	const bool bBoxIsFull = PlacedPackagingItemCount >= RequiredPackagingItemCount;
	const FText FeedbackText = bBoxIsFull
		? FText::Format(
			LOCTEXT("PackagingBoxFilledFeedback", "Doos gevuld: {0}/{1} items geplaatst."),
			FText::AsNumber(PlacedPackagingItemCount),
			FText::AsNumber(RequiredPackagingItemCount))
		: FText::Format(
			LOCTEXT("PackagingPlacementProgressFeedback", "Packaging-item geplaatst: {0}/{1} in de doos."),
			FText::AsNumber(PlacedPackagingItemCount),
			FText::AsNumber(RequiredPackagingItemCount));

	Economy->SetInteractionFeedback(FeedbackText, true);
}

void ABloodPackagingStation::ResetPackagingPlacementState()
{
	TArray<TObjectPtr<AActor>> ActorsToDestroy = PlacedPackagingPreviewActors;
	for (const FPackagingPlacedSlotState& SlotState : PlacedPackagingSlots)
	{
		if (SlotState.PlacedActor)
		{
			ActorsToDestroy.AddUnique(SlotState.PlacedActor);
		}
	}

	if (AActor* CurrentPreviewActor = SpawnedManualPreviewActor.Get())
	{
		ActorsToDestroy.AddUnique(CurrentPreviewActor);
	}

	for (UInteractionPlacementTargetComponent* PlacementTarget : ResolvedPackagingPlacementTargets)
	{
		if (!PlacementTarget)
		{
			continue;
		}

		if (PlacementTarget->PlacedObject)
		{
			if (AActor* PlacedActor = PlacementTarget->PlacedObject->GetOwner())
			{
				ActorsToDestroy.AddUnique(PlacedActor);
			}
		}

		PlacementTarget->RemovePlacedObject();
	}

	for (AActor* ActorToDestroy : ActorsToDestroy)
	{
		if (ActorToDestroy && ActorToDestroy->GetOwner() == this)
		{
			ActorToDestroy->Destroy();
		}
	}

	PlacedPackagingPreviewActors.Reset();
	PlacedPackagingSlots.Reset();
	PlacedPackagingItemCount = 0;
	SpawnedManualPreviewActor = nullptr;
	ReservedPackagingInventory = nullptr;
	ForceNetUpdate();
}

int32 ABloodPackagingStation::ResolvePackagingPlacementTargetIndex(const UInteractionPlacementTargetComponent* TargetComponent) const
{
	return ResolvedPackagingPlacementTargets.IndexOfByKey(TargetComponent);
}

UInteractionPlacementTargetComponent* ABloodPackagingStation::GetPackagingPlacementTargetByIndex(const int32 SlotIndex) const
{
	return ResolvedPackagingPlacementTargets.IsValidIndex(SlotIndex)
		? ResolvedPackagingPlacementTargets[SlotIndex]
		: nullptr;
}

bool ABloodPackagingStation::IsPackagingSlotOccupied(const int32 SlotIndex) const
{
	return PlacedPackagingSlots.ContainsByPredicate([SlotIndex](const FPackagingPlacedSlotState& SlotState)
	{
		return SlotState.SlotIndex == SlotIndex && IsValid(SlotState.PlacedActor.Get());
	});
}

bool ABloodPackagingStation::AssignPlacedPackagingActorToSlot(const int32 SlotIndex, AActor* PackagingActor, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	UInteractionPlacementTargetComponent* PlacementTarget = GetPackagingPlacementTargetByIndex(SlotIndex);
	if (!PlacementTarget)
	{
		OutReason = LOCTEXT("PackagingInvalidSlot", "Het gekozen packaging-slot is ongeldig.");
		return false;
	}

	if (!PackagingActor || PackagingActor->GetOwner() != this)
	{
		OutReason = LOCTEXT("PackagingInvalidPlacedActor", "Het packaging-item kon niet correct aan een doos-slot worden gekoppeld.");
		return false;
	}

	if (const FPackagingPlacedSlotState* ExistingSlotState = PlacedPackagingSlots.FindByPredicate([SlotIndex](const FPackagingPlacedSlotState& SlotState)
	{
		return SlotState.SlotIndex == SlotIndex;
	}))
	{
		if (ExistingSlotState->PlacedActor == PackagingActor)
		{
			SpawnedManualPreviewActor = nullptr;
			RefreshPackagingPlacementState();
			return true;
		}

		OutReason = LOCTEXT("PackagingSlotOccupied", "Dit packaging-slot is al bezet.");
		return false;
	}

	SyncPlacedPackagingActorTransform(PackagingActor, PlacementTarget);

	FPackagingPlacedSlotState& NewSlotState = PlacedPackagingSlots.AddDefaulted_GetRef();
	NewSlotState.SlotIndex = SlotIndex;
	NewSlotState.PlacedActor = PackagingActor;

	SpawnedManualPreviewActor = nullptr;
	RefreshPackagingPlacementState();
	ForceNetUpdate();
	return true;
}

void ABloodPackagingStation::RebuildPackagingPlacementRuntimeState()
{
	PlacedPackagingPreviewActors.Reset();

	for (UInteractionPlacementTargetComponent* PlacementTarget : ResolvedPackagingPlacementTargets)
	{
		if (PlacementTarget)
		{
			PlacementTarget->PlacedObject = nullptr;
		}
	}

	bool bCurrentPreviewAlreadyPlaced = false;
	AActor* CurrentPreviewActor = SpawnedManualPreviewActor.Get();

	for (int32 SlotArrayIndex = PlacedPackagingSlots.Num() - 1; SlotArrayIndex >= 0; --SlotArrayIndex)
	{
		FPackagingPlacedSlotState& SlotState = PlacedPackagingSlots[SlotArrayIndex];
		UInteractionPlacementTargetComponent* PlacementTarget = GetPackagingPlacementTargetByIndex(SlotState.SlotIndex);
		AActor* PlacedActor = SlotState.PlacedActor.Get();
		if (!PlacementTarget || !PlacedActor || PlacedActor->GetOwner() != this)
		{
			if (HasAuthority())
			{
				PlacedPackagingSlots.RemoveAt(SlotArrayIndex);
			}
			continue;
		}

		if (UManipulatableObjectComponent* ManipulatableObject = PlacedActor->FindComponentByClass<UManipulatableObjectComponent>())
		{
			PlacementTarget->PlacedObject = ManipulatableObject;
		}

		SyncPlacedPackagingActorTransform(PlacedActor, PlacementTarget);
		PlacedPackagingPreviewActors.AddUnique(PlacedActor);
		bCurrentPreviewAlreadyPlaced |= (PlacedActor == CurrentPreviewActor);
	}

	if (bCurrentPreviewAlreadyPlaced)
	{
		SpawnedManualPreviewActor = nullptr;
	}
}

void ABloodPackagingStation::SyncPlacedPackagingActorTransform(AActor* PackagingActor, UInteractionPlacementTargetComponent* PlacementTarget) const
{
	if (!PackagingActor || !PlacementTarget)
	{
		return;
	}

	UManipulatableObjectComponent* ManipulatableObject = PackagingActor->FindComponentByClass<UManipulatableObjectComponent>();
	UPrimitiveComponent* PhysicsComponent = ManipulatableObject ? ManipulatableObject->GetPrimaryPhysicsComponent() : Cast<UPrimitiveComponent>(PackagingActor->GetRootComponent());
	const FTransform TargetTransform = PlacementTarget->BuildSnapTransform(ManipulatableObject);

	PackagingActor->SetActorTransform(TargetTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (PhysicsComponent)
	{
		PhysicsComponent->SetWorldTransform(TargetTransform, false, nullptr, ETeleportType::TeleportPhysics);
		PhysicsComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
		PhysicsComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		PhysicsComponent->SetSimulatePhysics(false);
	}
}

#undef LOCTEXT_NAMESPACE
