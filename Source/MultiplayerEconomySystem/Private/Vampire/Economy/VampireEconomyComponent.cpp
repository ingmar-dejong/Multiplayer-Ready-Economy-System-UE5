#include "Vampire/Economy/VampireEconomyComponent.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/InventoryComponent.h"
#include "Items/OwnSystemItem.h"
#include "Net/UnrealNetwork.h"
#include "UnrealFramework/OwnSystemGameState.h"
#include "Vampire/Data/BloodBuyerDataAsset.h"
#include "Vampire/Data/BloodHarvestSourceDataAsset.h"
#include "Vampire/Data/BloodProcessingRecipeDataAsset.h"
#include "Vampire/Interaction/BloodProcessingInteractableComponent.h"
#include "Vampire/Items/BloodProductItem.h"
#include "Vampire/World/BloodPackagingStation.h"
#include "Vampire/World/BloodProcessingStation.h"

#define LOCTEXT_NAMESPACE "VampireEconomyComponent"

namespace
{
	constexpr float TimeUnitsPerDay = 2400.0f;

	APawn* ResolveOwningPawn(const UActorComponent* Component)
	{
		return Component ? Cast<APawn>(Component->GetOwner()) : nullptr;
	}

	UOwnSystemInventoryComponent* ResolveInventoryForOwnedActor(const UActorComponent* Component)
	{
		return UVampireEconomyComponent::ResolveInventoryFromActor(Component ? Component->GetOwner() : nullptr);
	}

	float GetSharedOwnSystemAccumulatedTime(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			return 0.0f;
		}

		if (const UWorld* World = WorldContextObject->GetWorld())
		{
			if (const AOwnSystemGameState* OwnSystemGameState = World->GetGameState<AOwnSystemGameState>())
			{
				return FMath::Max(0.0f, OwnSystemGameState->GetAccumulatedTime());
			}
		}

		return 0.0f;
	}

	int32 ResolveSharedSimDay(const UObject* WorldContextObject)
	{
		return FMath::Max(0, FMath::FloorToInt(GetSharedOwnSystemAccumulatedTime(WorldContextObject) / TimeUnitsPerDay));
	}

	bool ClaimStationForComponent(UVampireEconomyComponent* EconomyComponent, ABloodProcessingStation* ProcessingStation, FText& OutReason)
	{
		if (!EconomyComponent || !ProcessingStation)
		{
			OutReason = LOCTEXT("ClaimStationMissingContext", "Station context ontbreekt.");
			return false;
		}

		APawn* OwningPawn = ResolveOwningPawn(EconomyComponent);
		if (!ProcessingStation->TryClaimOperator(OwningPawn, OutReason))
		{
			return false;
		}

		ProcessingStation->SetActiveInteractorContext(OwningPawn);
		return true;
	}

	bool CanOpenStationMenuWithoutClaim(const ABloodProcessingStation* ProcessingStation)
	{
		return ProcessingStation
			&& (ProcessingStation->StationState == EBloodVatStationState::Rijpt
				|| ProcessingStation->StationState == EBloodVatStationState::Klaar);
	}

}

UVampireEconomyComponent::UVampireEconomyComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
	LastFeedbackMessage = FText::GetEmpty();
}

void UVampireEconomyComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UVampireEconomyComponent, CurrentSimDay);
	DOREPLIFETIME(UVampireEconomyComponent, LifetimeBloodHarvested);
	DOREPLIFETIME(UVampireEconomyComponent, LifetimeBloodSold);
	DOREPLIFETIME(UVampireEconomyComponent, ThrallUnits);
	DOREPLIFETIME(UVampireEconomyComponent, LastFeedbackMessage);
	DOREPLIFETIME(UVampireEconomyComponent, bLastActionSuccessful);
}

void UVampireEconomyComponent::PrepareForSave_Implementation()
{
}

void UVampireEconomyComponent::Load_Implementation()
{
}

bool UVampireEconomyComponent::CreateHarvestedBloodItem(UOwnSystemInventoryComponent* TargetInventory, const UBloodHarvestSourceDataAsset* HarvestSource, UBloodProductItem*& OutCreatedItem)
{
	OutCreatedItem = nullptr;

	if (!TargetInventory || !HarvestSource || !HarvestSource->BloodItemClass)
	{
		LastFeedbackMessage = LOCTEXT("HarvestFailedInvalidSetup", "Harvest mislukt: ongeldige harvest setup.");
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	const int32 MinQuantity = FMath::Max(1, HarvestSource->MinBloodQuantity);
	const int32 MaxQuantity = FMath::Max(MinQuantity, HarvestSource->MaxBloodQuantity);
	const int32 HarvestQuantity = FMath::RandRange(MinQuantity, MaxQuantity);

	const FItemAddResult AddResult = TargetInventory->TryAddItemFromClass(HarvestSource->BloodItemClass, 1, false);
	if (AddResult.AmountGiven <= 0 || AddResult.Stacks.IsEmpty())
	{
		LastFeedbackMessage = LOCTEXT("HarvestFailedInventory", "Harvest mislukt: blood item kon niet aan inventory worden toegevoegd.");
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	UBloodProductItem* BloodItem = Cast<UBloodProductItem>(AddResult.Stacks[0]);
	if (!BloodItem)
	{
		LastFeedbackMessage = LOCTEXT("HarvestFailedWrongClass", "Harvest mislukt: item class is geen blood product item.");
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	BloodItem->SourceType = HarvestSource->SourceType;
	BloodItem->BaseQuality = ComputeQuality(HarvestSource->SourceType, HarvestSource->ConditionScore, HarvestSource->SetupScore);
	BloodItem->ProcessingType = HarvestSource->InitialProcessingType;
	BloodItem->BloodQuantity = HarvestQuantity;
	BloodItem->CreatedDay = GetCurrentSimDay();
	BloodItem->RefreshPresentation();

	LifetimeBloodHarvested += HarvestQuantity;
	OutCreatedItem = BloodItem;
	LastFeedbackMessage = FText::Format(
		LOCTEXT("HarvestSuccessFmt", "Harvest geslaagd: {0} bloed toegevoegd."),
		BloodItem->GetBloodDisplayName());
	bLastActionSuccessful = true;
	NotifyEconomyUpdated();
	return true;
}

bool UVampireEconomyComponent::CanSellBloodItemToBuyer(const UBloodProductItem* BloodItem, const UBloodBuyerDataAsset* BuyerData, FText& OutReason) const
{
	OutReason = FText::GetEmpty();

	if (!BloodItem)
	{
		OutReason = LOCTEXT("SellReasonNoItem", "Geen bloeditem geselecteerd.");
		return false;
	}

	if (!BuyerData)
	{
		OutReason = LOCTEXT("SellReasonNoBuyer", "Geen koperdata beschikbaar.");
		return false;
	}

	if (!BuyerData->AcceptedSources.IsEmpty() && !BuyerData->AcceptedSources.Contains(BloodItem->SourceType))
	{
		OutReason = LOCTEXT("SellReasonWrongSource", "Deze koper accepteert deze bloedbron niet.");
		return false;
	}

	if (static_cast<uint8>(BloodItem->BaseQuality) < static_cast<uint8>(BuyerData->MinimumQuality))
	{
		OutReason = LOCTEXT("SellReasonLowQuality", "De basiskwaliteit van dit bloed is te laag.");
		return false;
	}

	if (!BuyerData->AcceptedProcessingTypes.IsEmpty() && !BuyerData->AcceptedProcessingTypes.Contains(BloodItem->ProcessingType))
	{
		OutReason = LOCTEXT("SellReasonWrongProcessing", "Deze koper accepteert deze verwerking niet.");
		return false;
	}

	return true;
}

bool UVampireEconomyComponent::SellBloodItemToBuyer(UOwnSystemInventoryComponent* TargetInventory, UBloodProductItem* BloodItem, const UBloodBuyerDataAsset* BuyerData, int32& OutPayout, FText& OutReason)
{
	OutPayout = 0;

	if (!TargetInventory)
	{
		OutReason = LOCTEXT("SellFailedNoInventory", "Geen inventory beschikbaar.");
		LastFeedbackMessage = OutReason;
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	if (!CanSellBloodItemToBuyer(BloodItem, BuyerData, OutReason))
	{
		LastFeedbackMessage = OutReason;
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	const float RawPayout = BuyerData->BasePayoutPerUnit
		* BloodItem->BloodQuantity
		* GetQualityMultiplier(BloodItem->BaseQuality)
		* GetProcessingMultiplier(BloodItem->ProcessingType);

	OutPayout = FMath::Max(1, FMath::RoundToInt(RawPayout));

	if (!TargetInventory->RemoveItem(BloodItem))
	{
		OutReason = LOCTEXT("SellFailedRemove", "Verkoop mislukt: item kon niet uit inventory verwijderd worden.");
		OutPayout = 0;
		LastFeedbackMessage = OutReason;
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	TargetInventory->AddCurrency(OutPayout);
	LifetimeBloodSold += BloodItem->BloodQuantity;
	LastFeedbackMessage = FText::Format(
		LOCTEXT("SellSuccessFmt", "Verkoop geslaagd: {0} goud ontvangen."),
		FText::AsNumber(OutPayout));
	bLastActionSuccessful = true;
	NotifyEconomyUpdated();
	return true;
}

bool UVampireEconomyComponent::ProcessBloodItem(UBloodProductItem* BloodItem, const UBloodProcessingRecipeDataAsset* RecipeData, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!BloodItem)
	{
		OutReason = LOCTEXT("ProcessReasonNoItem", "Geen bloeditem geselecteerd.");
		LastFeedbackMessage = OutReason;
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	if (!RecipeData)
	{
		OutReason = LOCTEXT("ProcessReasonNoRecipe", "Geen processing recipe beschikbaar.");
		LastFeedbackMessage = OutReason;
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	if (BloodItem->ProcessingType != RecipeData->RequiredInputProcessing)
	{
		OutReason = LOCTEXT("ProcessReasonWrongInput", "Dit bloed heeft niet de juiste invoerverwerking.");
		LastFeedbackMessage = OutReason;
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	if (static_cast<uint8>(BloodItem->BaseQuality) < static_cast<uint8>(RecipeData->MinimumQuality))
	{
		OutReason = LOCTEXT("ProcessReasonLowQuality", "De basiskwaliteit van dit bloed is te laag voor deze verwerking.");
		LastFeedbackMessage = OutReason;
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	if (UOwnSystemInventoryComponent* Inventory = BloodItem->OwningInventory)
	{
		if (RecipeData->GoldCost > 0 && Inventory->GetCurrency() < RecipeData->GoldCost)
		{
			OutReason = LOCTEXT("ProcessReasonNoGold", "Niet genoeg goud voor deze verwerking.");
			LastFeedbackMessage = OutReason;
			bLastActionSuccessful = false;
			NotifyEconomyUpdated();
			return false;
		}

		if (RecipeData->GoldCost > 0)
		{
			Inventory->AddCurrency(-RecipeData->GoldCost);
		}
	}

	BloodItem->ProcessingType = RecipeData->OutputProcessing;
	BloodItem->RefreshPresentation();

	if (!RecipeData->RecipeName.IsEmpty())
	{
		LastFeedbackMessage = FText::Format(
			LOCTEXT("ProcessingSuccessWithRecipeFmt", "Processing geslaagd via {0}: nieuw product {1}."),
			RecipeData->RecipeName,
			BloodItem->GetBloodSummaryText());
	}
	else
	{
		LastFeedbackMessage = FText::Format(
			LOCTEXT("ProcessingSuccessFmt", "Processing geslaagd: nieuw product {0}."),
			BloodItem->GetBloodSummaryText());
	}

	bLastActionSuccessful = true;
	NotifyEconomyUpdated();
	return true;
}

UOwnSystemInventoryComponent* UVampireEconomyComponent::FindOwningInventory() const
{
	return ResolveInventoryFromActor(GetOwner());
}

void UVampireEconomyComponent::SetCurrentSimDay(const int32 NewDay)
{
	CurrentSimDay = FMath::Max(0, NewDay);
}

int32 UVampireEconomyComponent::GetCurrentSimDay() const
{
	return FMath::Max(CurrentSimDay, ResolveSharedSimDay(this));
}

void UVampireEconomyComponent::ClientOpenProcessingStationMenu_Implementation(ABloodProcessingStation* ProcessingStation)
{
	APawn* OwningPawn = Cast<APawn>(GetOwner());
	if (!OwningPawn || !ProcessingStation)
	{
		return;
	}

	UBloodProcessingInteractableComponent::OpenStationMenuForInteractor(ProcessingStation, OwningPawn);
}

void UVampireEconomyComponent::ClientReceiveInteractionFeedback_Implementation(const FText& FeedbackMessage, const bool bWasSuccessful)
{
	SetInteractionFeedback(FeedbackMessage, bWasSuccessful);
}

bool UVampireEconomyComponent::RequestStepSelectedRecipe(ABloodProcessingStation* ProcessingStation, const int32 Direction, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!ProcessingStation)
	{
		OutReason = LOCTEXT("RecipeStepMissingStation", "Station context ontbreekt.");
		return false;
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (!ClaimStationForComponent(this, ProcessingStation, OutReason))
		{
			return false;
		}
		ProcessingStation->StepSelectedRecipe(Direction);
		OutReason = LOCTEXT("RecipeStepUpdated", "Recipe selectie bijgewerkt.");
		return true;
	}

	ServerStepSelectedRecipe(ProcessingStation, Direction);
	OutReason = LOCTEXT("StationRequestSent", "Verzoek verzonden.");
	return true;
}

bool UVampireEconomyComponent::RequestOpenProcessingStationMenu(ABloodProcessingStation* ProcessingStation, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!ProcessingStation)
	{
		OutReason = LOCTEXT("OpenStationMissingStation", "Station context ontbreekt.");
		return false;
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (!CanOpenStationMenuWithoutClaim(ProcessingStation) && !ClaimStationForComponent(this, ProcessingStation, OutReason))
		{
			return false;
		}

		ProcessingStation->SetActiveInteractorContext(ResolveOwningPawn(this));
		ClientOpenProcessingStationMenu(ProcessingStation);
		OutReason = LOCTEXT("StationOpenSuccess", "Station geopend.");
		return true;
	}

	ServerOpenProcessingStationMenu(ProcessingStation);
	OutReason = LOCTEXT("StationRequestSent", "Verzoek verzonden.");
	return true;
}

bool UVampireEconomyComponent::RequestReleaseProcessingStation(ABloodProcessingStation* ProcessingStation, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!ProcessingStation)
	{
		return false;
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		ProcessingStation->ReleaseOperator(ResolveOwningPawn(this));
		return true;
	}

	ServerReleaseProcessingStation(ProcessingStation);
	return true;
}

bool UVampireEconomyComponent::RequestPrepareManualProcessing(ABloodProcessingStation* ProcessingStation, UBloodProductItem* SelectedItem, const int32 TotalAvailableUnits, const bool bRespawnOnly, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!ProcessingStation)
	{
		OutReason = LOCTEXT("PrepareMissingStation", "Station context ontbreekt.");
		return false;
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (!ClaimStationForComponent(this, ProcessingStation, OutReason))
		{
			return false;
		}
		return ProcessingStation->PrepareManualProcessingRequest(bRespawnOnly ? nullptr : SelectedItem, bRespawnOnly ? 0 : TotalAvailableUnits, OutReason);
	}

	ServerPrepareManualProcessing(ProcessingStation, SelectedItem, TotalAvailableUnits, bRespawnOnly);
	OutReason = LOCTEXT("StationRequestSent", "Verzoek verzonden.");
	return true;
}

bool UVampireEconomyComponent::RequestStartProcessing(ABloodProcessingStation* ProcessingStation, UBloodProductItem* SelectedItem, const int32 TotalAvailableUnits, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!ProcessingStation || !SelectedItem)
	{
		OutReason = LOCTEXT("StartProcessingMissingContext", "Station- of batchcontext ontbreekt.");
		return false;
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (!ClaimStationForComponent(this, ProcessingStation, OutReason))
		{
			return false;
		}
		return ProcessingStation->TryStartProcessing(ResolveInventoryForOwnedActor(this), this, SelectedItem, TotalAvailableUnits, OutReason);
	}

	ServerStartProcessing(ProcessingStation, SelectedItem, TotalAvailableUnits);
	OutReason = LOCTEXT("StationRequestSent", "Verzoek verzonden.");
	return true;
}

bool UVampireEconomyComponent::RequestHarvestProcessedBatch(ABloodProcessingStation* ProcessingStation, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!ProcessingStation)
	{
		OutReason = LOCTEXT("HarvestMissingStation", "Station context ontbreekt.");
		return false;
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (!ClaimStationForComponent(this, ProcessingStation, OutReason))
		{
			return false;
		}
		return ProcessingStation->TryHarvestProcessedBatch(ResolveInventoryForOwnedActor(this), this, OutReason);
	}

	ServerHarvestProcessedBatch(ProcessingStation);
	OutReason = LOCTEXT("StationRequestSent", "Verzoek verzonden.");
	return true;
}

bool UVampireEconomyComponent::RequestCancelReservedPackaging(ABloodPackagingStation* PackagingStation, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!PackagingStation)
	{
		return false;
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (!ClaimStationForComponent(this, PackagingStation, OutReason))
		{
			return false;
		}
		return PackagingStation->CancelReservedPackagingRequest(OutReason);
	}

	ServerCancelReservedPackaging(PackagingStation);
	OutReason = LOCTEXT("StationRequestSent", "Verzoek verzonden.");
	return true;
}

bool UVampireEconomyComponent::RequestConfirmManualPlacement(ABloodProcessingStation* ProcessingStation, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!ProcessingStation)
	{
		OutReason = LOCTEXT("ConfirmManualMissingStation", "Station context ontbreekt.");
		return false;
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (!ClaimStationForComponent(this, ProcessingStation, OutReason))
		{
			return false;
		}
		const bool bSuccess = ProcessingStation->CommitStagedManualProcessingRequest(ResolveInventoryForOwnedActor(this), this, OutReason);
		return bSuccess;
	}

	ServerConfirmManualPlacement(ProcessingStation);
	OutReason = LOCTEXT("StationRequestSent", "Verzoek verzonden.");
	return true;
}

bool UVampireEconomyComponent::RequestConfirmPackagingPlacement(ABloodPackagingStation* PackagingStation, const int32 SlotIndex, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!PackagingStation)
	{
		OutReason = LOCTEXT("ConfirmPackagingMissingStation", "Packaging context ontbreekt.");
		return false;
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (!ClaimStationForComponent(this, PackagingStation, OutReason))
		{
			return false;
		}
		return PackagingStation->ConfirmPackagingPlacement(SlotIndex, OutReason);
	}

	ServerConfirmPackagingPlacement(PackagingStation, SlotIndex);
	OutReason = LOCTEXT("StationRequestSent", "Verzoek verzonden.");
	return true;
}

bool UVampireEconomyComponent::RequestProcessDailyThrallUpkeep(FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (GetOwnerRole() >= ROLE_Authority)
	{
		const bool bSuccess = ProcessDailyThrallUpkeep(OutReason);
		if (bSuccess)
		{
			if (UWorld* World = GetWorld())
			{
				if (AOwnSystemGameState* OwnSystemGameState = World->GetGameState<AOwnSystemGameState>())
				{
					OwnSystemGameState->AdvanceTimeOfDay(2400.0f);
				}
			}

		}
		return bSuccess;
	}

	ServerProcessDailyThrallUpkeep();
	OutReason = LOCTEXT("StationRequestSent", "Verzoek verzonden.");
	return true;
}

bool UVampireEconomyComponent::RequestDebugForceOperatorUnpossess(FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (GetOwnerRole() >= ROLE_Authority)
	{
		APawn* OwningPawn = ResolveOwningPawn(this);
		APlayerController* OwningController = OwningPawn ? Cast<APlayerController>(OwningPawn->GetController()) : nullptr;
		if (!OwningController)
		{
			OutReason = LOCTEXT("DebugForceUnpossessMissingController", "Debug unpossess mislukt: geen geldige controller gevonden.");
			return false;
		}

		OwningController->UnPossess();
		OutReason = LOCTEXT("DebugForceUnpossessExecuted", "Debug: operator is geforceerd ge-unpossessed.");
		return true;
	}

	ServerDebugForceOperatorUnpossess();
	OutReason = LOCTEXT("DebugForceUnpossessRequested", "Debug: force unpossess aangevraagd.");
	return true;
}

void UVampireEconomyComponent::ServerStepSelectedRecipe_Implementation(ABloodProcessingStation* ProcessingStation, const int32 Direction)
{
	FText Reason;
	if (!ProcessingStation)
	{
		ClientReceiveInteractionFeedback(LOCTEXT("RecipeStepServerMissingStation", "Station context ontbreekt."), false);
		return;
	}

	if (!ClaimStationForComponent(this, ProcessingStation, Reason))
	{
		ClientReceiveInteractionFeedback(Reason, false);
		return;
	}

	ProcessingStation->StepSelectedRecipe(Direction);
	ClientReceiveInteractionFeedback(LOCTEXT("RecipeStepUpdated", "Recipe selectie bijgewerkt."), true);
}

void UVampireEconomyComponent::ServerOpenProcessingStationMenu_Implementation(ABloodProcessingStation* ProcessingStation)
{
	FText Reason;
	if (!ProcessingStation)
	{
		ClientReceiveInteractionFeedback(LOCTEXT("OpenStationServerMissingStation", "Station context ontbreekt."), false);
		return;
	}

	if (!CanOpenStationMenuWithoutClaim(ProcessingStation) && !ClaimStationForComponent(this, ProcessingStation, Reason))
	{
		ClientReceiveInteractionFeedback(Reason, false);
		return;
	}

	ProcessingStation->SetActiveInteractorContext(ResolveOwningPawn(this));
	ClientOpenProcessingStationMenu(ProcessingStation);
}

void UVampireEconomyComponent::ServerReleaseProcessingStation_Implementation(ABloodProcessingStation* ProcessingStation)
{
	if (!ProcessingStation)
	{
		return;
	}

	ProcessingStation->ReleaseOperator(ResolveOwningPawn(this));
}

void UVampireEconomyComponent::ServerPrepareManualProcessing_Implementation(ABloodProcessingStation* ProcessingStation, UBloodProductItem* SelectedItem, const int32 TotalAvailableUnits, const bool bRespawnOnly)
{
	FText Reason;
	if (!ProcessingStation)
	{
		ClientReceiveInteractionFeedback(LOCTEXT("PrepareServerMissingStation", "Station context ontbreekt."), false);
		return;
	}

	if (!ClaimStationForComponent(this, ProcessingStation, Reason))
	{
		ClientReceiveInteractionFeedback(Reason, false);
		return;
	}

	const bool bSuccess = ProcessingStation->PrepareManualProcessingRequest(bRespawnOnly ? nullptr : SelectedItem, bRespawnOnly ? 0 : TotalAvailableUnits, Reason);
	ClientReceiveInteractionFeedback(Reason, bSuccess);
}

void UVampireEconomyComponent::ServerStartProcessing_Implementation(ABloodProcessingStation* ProcessingStation, UBloodProductItem* SelectedItem, const int32 TotalAvailableUnits)
{
	FText Reason;
	if (!ProcessingStation || !SelectedItem)
	{
		ClientReceiveInteractionFeedback(LOCTEXT("StartServerMissingContext", "Station- of batchcontext ontbreekt."), false);
		return;
	}

	if (!ClaimStationForComponent(this, ProcessingStation, Reason))
	{
		ClientReceiveInteractionFeedback(Reason, false);
		return;
	}

	const bool bSuccess = ProcessingStation->TryStartProcessing(ResolveInventoryForOwnedActor(this), this, SelectedItem, TotalAvailableUnits, Reason);
	ClientReceiveInteractionFeedback(bSuccess ? Reason : FText::Format(
		LOCTEXT("StartServerFailedFmt", "{0}\n{1}"),
		ProcessingStation->BuildStationStatusText(),
		Reason), bSuccess);
}

void UVampireEconomyComponent::ServerHarvestProcessedBatch_Implementation(ABloodProcessingStation* ProcessingStation)
{
	FText Reason;
	if (!ProcessingStation)
	{
		ClientReceiveInteractionFeedback(LOCTEXT("HarvestServerMissingStation", "Station context ontbreekt."), false);
		return;
	}

	if (!ClaimStationForComponent(this, ProcessingStation, Reason))
	{
		ClientReceiveInteractionFeedback(Reason, false);
		return;
	}

	const bool bSuccess = ProcessingStation->TryHarvestProcessedBatch(ResolveInventoryForOwnedActor(this), this, Reason);
	ClientReceiveInteractionFeedback(bSuccess ? Reason : FText::Format(
		LOCTEXT("HarvestServerFailedFmt", "{0}\n{1}"),
		ProcessingStation->BuildStationStatusText(),
		Reason), bSuccess);
}

void UVampireEconomyComponent::ServerCancelReservedPackaging_Implementation(ABloodPackagingStation* PackagingStation)
{
	if (!PackagingStation)
	{
		ClientReceiveInteractionFeedback(FText::GetEmpty(), false);
		return;
	}

	FText Reason;
	if (!ClaimStationForComponent(this, PackagingStation, Reason))
	{
		ClientReceiveInteractionFeedback(Reason, false);
		return;
	}

	const bool bSuccess = PackagingStation->CancelReservedPackagingRequest(Reason);
	ClientReceiveInteractionFeedback(Reason, bSuccess);
}

void UVampireEconomyComponent::ServerConfirmManualPlacement_Implementation(ABloodProcessingStation* ProcessingStation)
{
	FText Reason;
	if (!ProcessingStation)
	{
		ClientReceiveInteractionFeedback(LOCTEXT("ConfirmManualServerMissingStation", "Station context ontbreekt."), false);
		return;
	}

	if (!ClaimStationForComponent(this, ProcessingStation, Reason))
	{
		ClientReceiveInteractionFeedback(Reason, false);
		return;
	}

	const bool bSuccess = ProcessingStation->CommitStagedManualProcessingRequest(ResolveInventoryForOwnedActor(this), this, Reason);
	ClientReceiveInteractionFeedback(Reason, bSuccess);
}

void UVampireEconomyComponent::ServerConfirmPackagingPlacement_Implementation(ABloodPackagingStation* PackagingStation, const int32 SlotIndex)
{
	if (!PackagingStation)
	{
		ClientReceiveInteractionFeedback(LOCTEXT("ConfirmPackagingServerMissingStation", "Packaging context ontbreekt."), false);
		return;
	}

	FText Reason;
	const bool bSuccess = RequestConfirmPackagingPlacement(PackagingStation, SlotIndex, Reason);
	ClientReceiveInteractionFeedback(Reason, bSuccess);
}

void UVampireEconomyComponent::ServerProcessDailyThrallUpkeep_Implementation()
{
	FText Summary;
	const bool bSuccess = ProcessDailyThrallUpkeep(Summary);
	if (bSuccess)
	{
		if (UWorld* World = GetWorld())
		{
			if (AOwnSystemGameState* OwnSystemGameState = World->GetGameState<AOwnSystemGameState>())
			{
				OwnSystemGameState->AdvanceTimeOfDay(2400.0f);
			}
		}

	}

	ClientReceiveInteractionFeedback(Summary, bSuccess);
}

void UVampireEconomyComponent::ServerDebugForceOperatorUnpossess_Implementation()
{
	FText Result;
	const bool bSuccess = RequestDebugForceOperatorUnpossess(Result);
	ClientReceiveInteractionFeedback(Result, bSuccess);
}

void UVampireEconomyComponent::AddThrallUnit(const FThrallUpkeepUnit& ThrallUnit)
{
	ThrallUnits.Add(ThrallUnit);
	NotifyEconomyUpdated();
}

bool UVampireEconomyComponent::ProcessDailyThrallUpkeep(FText& OutSummary)
{
	UOwnSystemInventoryComponent* Inventory = FindOwningInventory();
	if (!Inventory)
	{
		OutSummary = LOCTEXT("ThrallNoInventory", "Geen inventory beschikbaar voor thrall upkeep.");
		LastFeedbackMessage = OutSummary;
		bLastActionSuccessful = false;
		NotifyEconomyUpdated();
		return false;
	}

	int32 SupportedThralls = 0;
	int32 FailedThralls = 0;

	for (FThrallUpkeepUnit& Thrall : ThrallUnits)
	{
		if (!Thrall.bActive)
		{
			continue;
		}

		if (ConsumeBloodUnitsByMinimumQuality(Inventory, Thrall.RequiredQuality, Thrall.RequiredBloodUnitsPerDay))
		{
			Thrall.Loyalty = FMath::Min(1.0f, Thrall.Loyalty + 0.05f);
			SupportedThralls++;
		}
		else
		{
			Thrall.Loyalty = FMath::Max(0.0f, Thrall.Loyalty - 0.25f);
			FailedThralls++;
		}
	}

	CurrentSimDay = GetCurrentSimDay() + 1;
	OutSummary = FText::Format(
		LOCTEXT("ThrallSummaryProcessFmt", "Thrall upkeep verwerkt. Gevoerd: {0}, tekort: {1}."),
		FText::AsNumber(SupportedThralls),
		FText::AsNumber(FailedThralls));
	LastFeedbackMessage = OutSummary;
	bLastActionSuccessful = (FailedThralls == 0);
	if (AActor* OwnerActor = GetOwner())
	{
		OwnerActor->ForceNetUpdate();
	}
	NotifyEconomyUpdated();
	return FailedThralls == 0;
}

int32 UVampireEconomyComponent::GetActiveThrallCount() const
{
	int32 Count = 0;
	for (const FThrallUpkeepUnit& Thrall : ThrallUnits)
	{
		if (Thrall.bActive)
		{
			Count++;
		}
	}

	return Count;
}

int32 UVampireEconomyComponent::GetTotalDailyBloodUpkeep() const
{
	int32 Total = 0;
	for (const FThrallUpkeepUnit& Thrall : ThrallUnits)
	{
		if (Thrall.bActive)
		{
			Total += Thrall.RequiredBloodUnitsPerDay;
		}
	}

	return Total;
}

FText UVampireEconomyComponent::GetThrallUpkeepSummaryText() const
{
	return FText::Format(
		LOCTEXT("ThrallSummaryFmt", "Actieve thralls: {0} | Dagelijkse upkeep: {1} bloed"),
		FText::AsNumber(GetActiveThrallCount()),
		FText::AsNumber(GetTotalDailyBloodUpkeep()));
}

void UVampireEconomyComponent::SetInteractionFeedback(const FText& NewFeedbackMessage, const bool bWasSuccessful)
{
	LastFeedbackMessage = NewFeedbackMessage;
	bLastActionSuccessful = bWasSuccessful;
	NotifyEconomyUpdated();
}

UOwnSystemInventoryComponent* UVampireEconomyComponent::ResolveInventoryFromActor(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UOwnSystemInventoryComponent* Inventory = Actor->FindComponentByClass<UOwnSystemInventoryComponent>())
	{
		return Inventory;
	}

	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (APlayerState* PlayerState = Pawn->GetPlayerState())
		{
			if (UOwnSystemInventoryComponent* Inventory = PlayerState->FindComponentByClass<UOwnSystemInventoryComponent>())
			{
				return Inventory;
			}
		}

		if (AController* Controller = Pawn->GetController())
		{
			if (UOwnSystemInventoryComponent* Inventory = Controller->FindComponentByClass<UOwnSystemInventoryComponent>())
			{
				return Inventory;
			}
		}
	}

	return nullptr;
}

UVampireEconomyComponent* UVampireEconomyComponent::ResolveEconomyFromActor(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UVampireEconomyComponent* Economy = Actor->FindComponentByClass<UVampireEconomyComponent>())
	{
		return Economy;
	}

	if (const APawn* Pawn = Cast<APawn>(Actor))
	{
		if (APlayerState* PlayerState = Pawn->GetPlayerState())
		{
			if (UVampireEconomyComponent* Economy = PlayerState->FindComponentByClass<UVampireEconomyComponent>())
			{
				return Economy;
			}
		}

		if (AController* Controller = Pawn->GetController())
		{
			if (UVampireEconomyComponent* Economy = Controller->FindComponentByClass<UVampireEconomyComponent>())
			{
				return Economy;
			}
		}
	}

	return nullptr;
}

int32 UVampireEconomyComponent::GetSourceScore(const EBloodSourceType SourceType)
{
	switch (SourceType)
	{
	case EBloodSourceType::Rat:
		return 0;
	case EBloodSourceType::Varken:
	case EBloodSourceType::Geit:
	case EBloodSourceType::Bedelaar:
		return 1;
	case EBloodSourceType::Hert:
	case EBloodSourceType::Boer:
		return 2;
	case EBloodSourceType::Handelaar:
	case EBloodSourceType::Edelman:
	case EBloodSourceType::Priester:
		return 3;
	default:
		return 0;
	}
}

EBloodQuality UVampireEconomyComponent::ComputeQuality(const EBloodSourceType SourceType, const int32 ConditionScore, const int32 SetupScore)
{
	const int32 TotalScore = FMath::Clamp(GetSourceScore(SourceType) + ConditionScore + SetupScore, 0, 6);
	if (TotalScore <= 2)
	{
		return EBloodQuality::Gewoon;
	}

	if (TotalScore <= 4)
	{
		return EBloodQuality::Goed;
	}

	return EBloodQuality::Premium;
}

float UVampireEconomyComponent::GetQualityMultiplier(const EBloodQuality Quality)
{
	switch (Quality)
	{
	case EBloodQuality::Gewoon:
		return 1.0f;
	case EBloodQuality::Goed:
		return 1.5f;
	case EBloodQuality::Premium:
		return 2.25f;
	default:
		return 1.0f;
	}
}

float UVampireEconomyComponent::GetProcessingMultiplier(const EBloodProcessingType ProcessingType)
{
	switch (ProcessingType)
	{
	case EBloodProcessingType::Vers:
		return 1.0f;
	case EBloodProcessingType::GerijptOpHout:
		return 1.35f;
	case EBloodProcessingType::Handelsklaar:
		return 1.5f;
	case EBloodProcessingType::Gekruid:
		return 1.2f;
	case EBloodProcessingType::Gezuiverd:
		return 1.4f;
	case EBloodProcessingType::Verdund:
		return 0.75f;
	case EBloodProcessingType::RitueelBehandeld:
		return 1.75f;
	case EBloodProcessingType::GekoeldBewaard:
		return 1.15f;
	default:
		return 1.0f;
	}
}

int32 UVampireEconomyComponent::GetTotalBloodUnitsByMinimumQuality(const TArray<UOwnSystemItem*>& Items, const EBloodQuality MinimumQuality)
{
	int32 TotalUnits = 0;
	for (UOwnSystemItem* Item : Items)
	{
		if (const UBloodProductItem* BloodItem = Cast<UBloodProductItem>(Item))
		{
			if (static_cast<uint8>(BloodItem->BaseQuality) >= static_cast<uint8>(MinimumQuality))
			{
				TotalUnits += BloodItem->BloodQuantity;
			}
		}
	}

	return TotalUnits;
}

bool UVampireEconomyComponent::ConsumeBloodUnitsByMinimumQuality(UOwnSystemInventoryComponent* Inventory, const EBloodQuality MinimumQuality, const int32 UnitsToConsume)
{
	if (!Inventory || UnitsToConsume <= 0)
	{
		return false;
	}

	TArray<UOwnSystemItem*> Items = Inventory->GetItems();
	if (GetTotalBloodUnitsByMinimumQuality(Items, MinimumQuality) < UnitsToConsume)
	{
		return false;
	}

	int32 RemainingUnits = UnitsToConsume;
	for (UOwnSystemItem* Item : Items)
	{
		UBloodProductItem* BloodItem = Cast<UBloodProductItem>(Item);
		if (!BloodItem || static_cast<uint8>(BloodItem->BaseQuality) < static_cast<uint8>(MinimumQuality))
		{
			continue;
		}

		const int32 UnitsFromThisItem = FMath::Min(BloodItem->BloodQuantity, RemainingUnits);
		BloodItem->BloodQuantity -= UnitsFromThisItem;
		RemainingUnits -= UnitsFromThisItem;

		if (BloodItem->BloodQuantity <= 0)
		{
			Inventory->RemoveItem(BloodItem);
		}
		else
		{
			BloodItem->RefreshPresentation();
		}

		if (RemainingUnits <= 0)
		{
			return true;
		}
	}

	return RemainingUnits <= 0;
}

void UVampireEconomyComponent::NotifyEconomyUpdated()
{
	OnEconomyUpdated.Broadcast();
}

#undef LOCTEXT_NAMESPACE
