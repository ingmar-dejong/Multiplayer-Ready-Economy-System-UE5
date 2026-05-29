#include "Vampire/Economy/VampireEconomyComponent.h"

#include "DrawDebugHelpers.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "EngineUtils.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Items/InventoryComponent.h"
#include "Items/OwnSystemItem.h"
#include "Items/Widgets/InventoryWidget.h"
#include "Net/UnrealNetwork.h"
#include "OwnSystemActivatableWidget.h"
#include "Widgets/OwnSystemMenu.h"
#include "UnrealFramework/OwnSystemGameState.h"
#include "UI/VampireEconomySummaryMenu.h"
#include "Vampire/Data/BloodBuyerDataAsset.h"
#include "Vampire/Data/BloodHarvestSourceDataAsset.h"
#include "Vampire/Data/BloodProcessingRecipeDataAsset.h"
#include "Vampire/Interaction/BloodProcessingInteractableComponent.h"
#include "Vampire/Items/BloodProductItem.h"
#include "Vampire/Items/PlaceableStationItem.h"
#include "Vampire/World/BloodPackagingStation.h"
#include "Vampire/World/BloodProcessingStation.h"
#include "Vampire/World/StationPlacementPreviewActor.h"
#include "Vampire/World/WorkspaceRoom.h"

#define LOCTEXT_NAMESPACE "VampireEconomyComponent"

namespace
{
	constexpr float EconomyTimeUnitsPerDay = 2400.0f;

	APawn* ResolveOwningPawn(const UActorComponent* Component)
	{
		if (!Component)
		{
			return nullptr;
		}

		AActor* Owner = Component->GetOwner();
		if (APawn* PawnOwner = Cast<APawn>(Owner))
		{
			return PawnOwner;
		}

		if (AController* ControllerOwner = Cast<AController>(Owner))
		{
			return ControllerOwner->GetPawn();
		}

		if (APlayerState* PlayerStateOwner = Cast<APlayerState>(Owner))
		{
			return PlayerStateOwner->GetPawn();
		}

		return nullptr;
	}

	APlayerController* ResolveOwningPlayerController(const UActorComponent* Component)
	{
		if (!Component)
		{
			return nullptr;
		}

		if (APlayerController* PlayerControllerOwner = Cast<APlayerController>(Component->GetOwner()))
		{
			return PlayerControllerOwner;
		}

		if (APawn* OwningPawn = ResolveOwningPawn(Component))
		{
			return Cast<APlayerController>(OwningPawn->GetController());
		}

		return nullptr;
	}

	UOwnSystemInventoryComponent* ResolveInventoryForOwnedActor(const UActorComponent* Component)
	{
		return UVampireEconomyComponent::ResolveInventoryFromActor(Component ? Component->GetOwner() : nullptr);
	}

	template <typename TWidgetClass>
	void CloseOwnedWidgetsOfClass(APlayerController* PlayerController, UObject* WorldContextObject, const TCHAR* ReasonTag)
	{
		if (!PlayerController || !WorldContextObject)
		{
			return;
		}

		TArray<UUserWidget*> Widgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContextObject, Widgets, TWidgetClass::StaticClass(), false);

		int32 ClosedCount = 0;
		for (UUserWidget* Widget : Widgets)
		{
			TWidgetClass* TypedWidget = Cast<TWidgetClass>(Widget);
			if (!TypedWidget || TypedWidget->GetOwningPlayer() != PlayerController)
			{
				continue;
			}

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("PlacementInputAudit: Closing widget reason=%s class=%s widget=%s owner=%s"),
				ReasonTag ? ReasonTag : TEXT("Unknown"),
				*TWidgetClass::StaticClass()->GetName(),
				*GetNameSafe(TypedWidget),
				*GetNameSafe(PlayerController));

			TypedWidget->DeactivateWidget();
			TypedWidget->RemoveFromParent();
			++ClosedCount;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementInputAudit: Widget close summary reason=%s class=%s owner=%s closed=%d"),
			ReasonTag ? ReasonTag : TEXT("Unknown"),
			*TWidgetClass::StaticClass()->GetName(),
			*GetNameSafe(PlayerController),
			ClosedCount);
	}

	void LogOwnedActivatableWidgets(APlayerController* PlayerController, UObject* WorldContextObject, const TCHAR* ReasonTag)
	{
		if (!PlayerController || !WorldContextObject)
		{
			return;
		}

		TArray<UUserWidget*> ActivatableWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContextObject, ActivatableWidgets, UOwnSystemActivatableWidget::StaticClass(), false);

		int32 LoggedCount = 0;
		for (UUserWidget* Widget : ActivatableWidgets)
		{
			UOwnSystemActivatableWidget* ActivatableWidget = Cast<UOwnSystemActivatableWidget>(Widget);
			if (!ActivatableWidget || ActivatableWidget->GetOwningPlayer() != PlayerController)
			{
				continue;
			}

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("PlacementInputAudit: Widget snapshot reason=%s class=%s widget=%s active=%d inViewport=%d visible=%d hasFocus=%d"),
				ReasonTag ? ReasonTag : TEXT("Unknown"),
				*ActivatableWidget->GetClass()->GetName(),
				*GetNameSafe(ActivatableWidget),
				ActivatableWidget->IsActivated() ? 1 : 0,
				ActivatableWidget->IsInViewport() ? 1 : 0,
				ActivatableWidget->IsVisible() ? 1 : 0,
				ActivatableWidget->HasAnyUserFocus() ? 1 : 0);
			++LoggedCount;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementInputAudit: Widget snapshot summary reason=%s owner=%s widgets=%d"),
			ReasonTag ? ReasonTag : TEXT("Unknown"),
			*GetNameSafe(PlayerController),
			LoggedCount);
	}

	void DeactivateOwnedMenuByClass(APlayerController* PlayerController, UObject* WorldContextObject, const UClass* MenuClass, const TCHAR* ReasonTag)
	{
		if (!PlayerController || !WorldContextObject || !MenuClass)
		{
			return;
		}

		TArray<UUserWidget*> MenuWidgets;
		UWidgetBlueprintLibrary::GetAllWidgetsOfClass(WorldContextObject, MenuWidgets, UOwnSystemMenu::StaticClass(), false);

		int32 DeactivatedCount = 0;
		for (UUserWidget* Widget : MenuWidgets)
		{
			UOwnSystemMenu* OwnSystemMenu = Cast<UOwnSystemMenu>(Widget);
			if (!OwnSystemMenu || OwnSystemMenu->GetOwningPlayer() != PlayerController)
			{
				continue;
			}

			if (!OwnSystemMenu->GetClass()->IsChildOf(MenuClass))
			{
				continue;
			}

			UE_LOG(
				LogTemp,
				Warning,
				TEXT("PlacementInputAudit: Deactivating menu reason=%s class=%s widget=%s owner=%s"),
				ReasonTag ? ReasonTag : TEXT("Unknown"),
				*GetNameSafe(OwnSystemMenu->GetClass()),
				*GetNameSafe(OwnSystemMenu),
				*GetNameSafe(PlayerController));

			OwnSystemMenu->DeactivateWidget();
			++DeactivatedCount;
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementInputAudit: Menu deactivate summary reason=%s class=%s owner=%s deactivated=%d"),
			ReasonTag ? ReasonTag : TEXT("Unknown"),
			*GetNameSafe(MenuClass),
			*GetNameSafe(PlayerController),
			DeactivatedCount);
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
		return FMath::Max(0, FMath::FloorToInt(GetSharedOwnSystemAccumulatedTime(WorldContextObject) / EconomyTimeUnitsPerDay));
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

	bool RestorePlaceableStationItemToInventory(UOwnSystemInventoryComponent* Inventory, const UPlaceableStationItem* RemovedItem)
	{
		if (!Inventory || !RemovedItem)
		{
			return false;
		}

		const FItemAddResult AddResult = Inventory->TryAddItemFromClass(RemovedItem->GetClass(), 1, false);
		if (AddResult.AmountGiven <= 0 || AddResult.Stacks.IsEmpty())
		{
			return false;
		}

		UPlaceableStationItem* RestoredItem = Cast<UPlaceableStationItem>(AddResult.Stacks[0]);
		if (!RestoredItem)
		{
			return false;
		}

		RestoredItem->StationDefinition = RemovedItem->StationDefinition;
		RestoredItem->StationInstanceId = RemovedItem->StationInstanceId;
		RestoredItem->RefreshPresentation();
		return true;
	}

	FIntPoint RotatePlacementCell(const FIntPoint LocalCell, const int32 RotationQuarterTurns)
	{
		switch ((RotationQuarterTurns % 4 + 4) % 4)
		{
		case 1:
			return FIntPoint(-LocalCell.Y, LocalCell.X);
		case 2:
			return FIntPoint(-LocalCell.X, -LocalCell.Y);
		case 3:
			return FIntPoint(LocalCell.Y, -LocalCell.X);
		default:
			return LocalCell;
		}
	}

	TArray<FIntPoint> BuildPreviewCells(const UPlaceableStationDataAsset* StationDefinition, const FIntPoint AnchorCell, const int32 RotationQuarterTurns)
	{
		TArray<FIntPoint> Result;
		if (!StationDefinition)
		{
			return Result;
		}

		if (StationDefinition->FootprintCells.IsEmpty())
		{
			Result.Add(AnchorCell);
			return Result;
		}

		for (const FIntPoint& LocalCell : StationDefinition->FootprintCells)
		{
			Result.Add(AnchorCell + RotatePlacementCell(LocalCell, RotationQuarterTurns));
		}

		return Result;
	}

}

UVampireEconomyComponent::UVampireEconomyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	SetIsReplicatedByDefault(true);
	LastFeedbackMessage = FText::GetEmpty();
}

void UVampireEconomyComponent::TickComponent(const float DeltaTime, const ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bStationPlacementModeActive)
	{
		return;
	}

	UpdateStationPlacementPreview();
	SyncMovePlacementPreviewReplication();
	SyncPlacementPreviewActor();
	DrawStationPlacementPreview();
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
	if (bPlacementConfirmPending)
	{
		bPlacementConfirmPending = false;
		if (bWasSuccessful)
		{
			StopStationPlacementMode(false);
		}
	}
	else if (!bWasSuccessful && bStationPlacementModeActive && PendingMoveStation)
	{
		StopStationPlacementMode(true);
	}

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

	if (ProcessingStation->IsMovePlacementInProgress() && !ProcessingStation->IsCurrentOperator(ResolveOwningPawn(this)))
	{
		OutReason = LOCTEXT("OpenStationMoveInProgress", "Dit station wordt momenteel door een andere speler verplaatst.");
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

bool UVampireEconomyComponent::RequestCancelManualProcessing(ABloodProcessingStation* ProcessingStation, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!ProcessingStation)
	{
		OutReason = LOCTEXT("CancelManualMissingStation", "Station context ontbreekt.");
		return false;
	}

	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (!ClaimStationForComponent(this, ProcessingStation, OutReason))
		{
			return false;
		}
		return ProcessingStation->CancelManualProcessingRequest(OutReason);
	}

	ServerCancelManualProcessing(ProcessingStation);
	OutReason = LOCTEXT("StationRequestSent", "Verzoek verzonden.");
	return true;
}

bool UVampireEconomyComponent::RequestCancelReservedPackaging(ABloodPackagingStation* PackagingStation, FText& OutReason)
{
	return RequestCancelManualProcessing(PackagingStation, OutReason);
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

bool UVampireEconomyComponent::RequestAutoPlaceStationItem(UPlaceableStationItem* StationItem, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!StationItem || !StationItem->StationDefinition)
	{
		OutReason = LOCTEXT("AutoPlaceMissingItem", "Geen geldig station-item geselecteerd.");
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutReason = LOCTEXT("AutoPlaceMissingWorld", "Geen geldige wereldcontext gevonden voor stationplaatsing.");
		return false;
	}

	FText LastValidationReason = LOCTEXT("AutoPlaceNoWorkspaceRoom", "Geen AWorkspaceRoom in het level gevonden.");
	for (TActorIterator<AWorkspaceRoom> It(World); It; ++It)
	{
		AWorkspaceRoom* WorkspaceRoom = *It;
		if (!WorkspaceRoom)
		{
			continue;
		}

		for (int32 Rotation = 0; Rotation < 4; ++Rotation)
		{
			for (int32 Y = 0; Y < WorkspaceRoom->GridHeight; ++Y)
			{
				for (int32 X = 0; X < WorkspaceRoom->GridWidth; ++X)
				{
					const FIntPoint AnchorCell(X, Y);
					if (!WorkspaceRoom->ValidatePlacement(StationItem->StationDefinition, AnchorCell, Rotation, LastValidationReason))
					{
						continue;
					}

					return RequestPlaceStationItem(WorkspaceRoom, StationItem, AnchorCell, Rotation, OutReason);
				}
			}
		}
	}

	OutReason = LastValidationReason;
	return false;
}

bool UVampireEconomyComponent::RequestPlaceStationItem(AWorkspaceRoom* WorkspaceRoom, UPlaceableStationItem* StationItem, const FIntPoint AnchorCell, const int32 RotationQuarterTurns, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!WorkspaceRoom || !StationItem || !StationItem->StationDefinition)
	{
		OutReason = LOCTEXT("PlaceStationMissingContext", "Werkruimte of station-item context ontbreekt.");
		return false;
	}

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerPlaceStationItem(WorkspaceRoom, StationItem, AnchorCell, RotationQuarterTurns);
		OutReason = LOCTEXT("PlaceStationRequestSent", "Plaatsingsverzoek verzonden.");
		return true;
	}

	UOwnSystemInventoryComponent* Inventory = ResolveInventoryForOwnedActor(this);
	if (!Inventory)
	{
		OutReason = LOCTEXT("PlaceStationMissingInventory", "Geen inventory beschikbaar voor stationplaatsing.");
		return false;
	}

	if (StationItem->OwningInventory != Inventory)
	{
		OutReason = LOCTEXT("PlaceStationWrongInventory", "Dit station-item zit niet in de inventory van deze speler.");
		return false;
	}

	FWorkspacePlacedStationRecord PlacementRecord;
	PlacementRecord.StationInstanceId = StationItem->StationInstanceId;
	PlacementRecord.StationDefinition = StationItem->StationDefinition;
	PlacementRecord.AnchorCell = AnchorCell;
	PlacementRecord.RotationQuarterTurns = RotationQuarterTurns;

	if (!PlacementRecord.StationInstanceId.IsValid())
	{
		StationItem->EnsureInstanceId();
		PlacementRecord.StationInstanceId = StationItem->StationInstanceId;
	}

	if (!WorkspaceRoom->ValidatePlacement(PlacementRecord.StationDefinition, PlacementRecord.AnchorCell, PlacementRecord.RotationQuarterTurns, OutReason))
	{
		return false;
	}

	if (!Inventory->RemoveItem(StationItem))
	{
		OutReason = LOCTEXT("PlaceStationRemoveFailed", "Kon het station-item niet uit inventory verwijderen.");
		return false;
	}

	if (!WorkspaceRoom->AddPlacementRecord(PlacementRecord, OutReason))
	{
		RestorePlaceableStationItemToInventory(Inventory, StationItem);
		return false;
	}

	AActor* SpawnedActor = WorkspaceRoom->SpawnPlacedActor(PlacementRecord, OutReason);
	if (!SpawnedActor)
	{
		WorkspaceRoom->RemovePlacementRecordByInstanceId(PlacementRecord.StationInstanceId);
		RestorePlaceableStationItemToInventory(Inventory, StationItem);
		return false;
	}

	OutReason = LOCTEXT("PlaceStationSuccess", "Station succesvol geplaatst in de werkruimte.");
	return true;
}

bool UVampireEconomyComponent::RequestMovePlacedStation(ABloodProcessingStation* ProcessingStation, AWorkspaceRoom* WorkspaceRoom, const FIntPoint AnchorCell, const int32 RotationQuarterTurns, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!ProcessingStation || !WorkspaceRoom)
	{
		OutReason = LOCTEXT("MoveStationMissingContext", "Station- of werkruimtecontext ontbreekt voor verplaatsen.");
		return false;
	}

	if (GetOwnerRole() < ROLE_Authority)
	{
		ServerMovePlacedStation(ProcessingStation, WorkspaceRoom, AnchorCell, RotationQuarterTurns);
		OutReason = LOCTEXT("MoveStationRequestSent", "Verplaatsverzoek verzonden.");
		return true;
	}

	if (!ProcessingStation->IsCurrentOperator(ResolveOwningPawn(this)))
	{
		OutReason = ProcessingStation->IsMovePlacementInProgress()
			? LOCTEXT("MoveStationOwnedByOther", "Dit station wordt momenteel door een andere speler verplaatst.")
			: LOCTEXT("MoveStationNotClaimed", "Dit station is niet door deze speler gereserveerd voor verplaatsen.");
		return false;
	}

	if (ProcessingStation->StationState != EBloodVatStationState::Leeg
		|| ProcessingStation->HasStagedManualProcessingRequest()
		|| ProcessingStation->HasSpawnedManualProcessingActor())
	{
		OutReason = LOCTEXT("MoveStationNotSafe", "Dit station kan alleen worden verplaatst als het leeg en veilig is.");
		return false;
	}

	if (!ProcessingStation->HasRegisteredPlacementContext())
	{
		OutReason = LOCTEXT("MoveStationMissingRegistration", "Dit station is nog niet als geplaatst processing station geregistreerd.");
		return false;
	}

	AWorkspaceRoom* OriginalWorkspaceRoom = ProcessingStation->GetOwningWorkspaceRoom();
	if (!OriginalWorkspaceRoom)
	{
		OutReason = LOCTEXT("MoveStationMissingOriginalRoom", "Dit station heeft geen geldige oorspronkelijke werkruimtecontext.");
		return false;
	}

	const FGuid StationInstanceId = ProcessingStation->GetPlacedStationInstanceId();
	if (!StationInstanceId.IsValid())
	{
		OutReason = LOCTEXT("MoveStationMissingGuid", "Dit station mist een geldige placement instance-id.");
		return false;
	}

	const FWorkspacePlacedStationRecord* MatchingRecord = OriginalWorkspaceRoom->FindPlacementRecordByInstanceId(StationInstanceId);
	if (!MatchingRecord || !MatchingRecord->StationDefinition)
	{
		OutReason = LOCTEXT("MoveStationMissingRecord", "Geen geldig placementrecord gevonden voor dit station.");
		return false;
	}

	if (!WorkspaceRoom->ValidatePlacement(MatchingRecord->StationDefinition, AnchorCell, RotationQuarterTurns, OutReason, &StationInstanceId))
	{
		return false;
	}

	const FWorkspacePlacedStationRecord OriginalRecord = *MatchingRecord;
	FWorkspacePlacedStationRecord UpdatedRecord = OriginalRecord;
	UpdatedRecord.AnchorCell = AnchorCell;
	UpdatedRecord.RotationQuarterTurns = RotationQuarterTurns;

	OriginalWorkspaceRoom->RemovePlacementRecordByInstanceId(StationInstanceId);
	if (!WorkspaceRoom->AddPlacementRecord(UpdatedRecord, OutReason))
	{
		FText RestoreReason;
		OriginalWorkspaceRoom->AddPlacementRecord(OriginalRecord, RestoreReason);
		return false;
	}

	if (WorkspaceRoom != OriginalWorkspaceRoom)
	{
		OriginalWorkspaceRoom->DestroyPlacedActorByInstanceId(StationInstanceId);
	}

	if (!WorkspaceRoom->RegisterPlacedStationActor(ProcessingStation, UpdatedRecord, OutReason))
	{
		FText RestoreReason;
		WorkspaceRoom->RemovePlacementRecordByInstanceId(StationInstanceId);
		OriginalWorkspaceRoom->AddPlacementRecord(OriginalRecord, RestoreReason);
		OriginalWorkspaceRoom->RegisterPlacedStationActor(ProcessingStation, OriginalRecord, RestoreReason);
		return false;
	}

	ProcessingStation->SetActorHiddenInGame(false);
	ProcessingStation->SetActorEnableCollision(true);
	ProcessingStation->EndMovePlacement(ResolveOwningPawn(this));

	OutReason = LOCTEXT("MoveStationSuccess", "Station succesvol verplaatst.");
	return true;
}

const UPlaceableStationDataAsset* UVampireEconomyComponent::GetActivePlacementDefinition() const
{
	if (PendingPlacementDefinition)
	{
		return PendingPlacementDefinition;
	}

	return PendingPlacementItem ? PendingPlacementItem->StationDefinition : nullptr;
}

const FGuid* UVampireEconomyComponent::GetIgnoredPlacementInstanceId() const
{
	if (PendingMoveStation && PendingMoveStation->GetPlacedStationInstanceId().IsValid())
	{
		return &PendingMoveStation->GetPlacedStationInstanceId();
	}

	return nullptr;
}

void UVampireEconomyComponent::UpdateStationPlacementPreview()
{
	if (!bStationPlacementModeActive)
	{
		return;
	}

	const UPlaceableStationDataAsset* ActivePlacementDefinition = GetActivePlacementDefinition();
	if (!ActivePlacementDefinition)
	{
		StopStationPlacementMode();
		SetInteractionFeedback(
			PendingMoveStation
				? LOCTEXT("MovePlacementLost", "Verplaats mode gestopt: het station is niet meer geldig.")
				: LOCTEXT("PlacementModeItemLost", "Placement mode gestopt: het station-item is niet meer geldig."),
			false);
		return;
	}

	AWorkspaceRoom* CandidateRoom = nullptr;
	FIntPoint CandidateCell = FIntPoint::ZeroValue;
	FText CandidateReason;
	bPlacementCandidateValid = ResolvePlacementCandidate(CandidateRoom, CandidateCell, CandidateReason);
	PlacementWorkspaceRoom = CandidateRoom;
	PlacementAnchorCell = CandidateCell;
	PlacementCandidateReason = CandidateReason;

	if (bPlacementCandidateValid && PlacementWorkspaceRoom)
	{
		FText ValidationReason;
		bPlacementCandidateValid = PlacementWorkspaceRoom->ValidatePlacement(
			ActivePlacementDefinition,
			PlacementAnchorCell,
			PlacementRotationQuarterTurns,
			ValidationReason,
			GetIgnoredPlacementInstanceId());
		PlacementCandidateReason = bPlacementCandidateValid ? LOCTEXT("PlacementModeValid", "Geldige plaatsing.") : ValidationReason;
	}
}

void UVampireEconomyComponent::SyncMovePlacementPreviewReplication()
{
	if (!bStationPlacementModeActive || !PendingMoveStation)
	{
		return;
	}

	UPlaceableStationDataAsset* ActivePlacementDefinition = PendingPlacementDefinition.Get();
	if (!ActivePlacementDefinition)
	{
		return;
	}

	const bool bPreviewChanged = !bHasLastReplicatedMovePreview
		|| LastReplicatedMovePreviewStation != PendingMoveStation
		|| LastReplicatedMovePreviewWorkspaceRoom != PlacementWorkspaceRoom
		|| LastReplicatedMovePreviewAnchorCell != PlacementAnchorCell
		|| LastReplicatedMovePreviewRotationQuarterTurns != PlacementRotationQuarterTurns
		|| bLastReplicatedMovePreviewValid != bPlacementCandidateValid
		|| LastReplicatedMovePreviewDefinition != ActivePlacementDefinition;
	if (!bPreviewChanged)
	{
		return;
	}

	LastReplicatedMovePreviewStation = PendingMoveStation;
	LastReplicatedMovePreviewWorkspaceRoom = PlacementWorkspaceRoom;
	LastReplicatedMovePreviewAnchorCell = PlacementAnchorCell;
	LastReplicatedMovePreviewRotationQuarterTurns = PlacementRotationQuarterTurns;
	bLastReplicatedMovePreviewValid = bPlacementCandidateValid;
	LastReplicatedMovePreviewDefinition = ActivePlacementDefinition;
	bHasLastReplicatedMovePreview = true;

	if (GetOwnerRole() >= ROLE_Authority)
	{
		PendingMoveStation->UpdateMovePlacementPreview(
			PlacementWorkspaceRoom,
			PlacementAnchorCell,
			PlacementRotationQuarterTurns,
			bPlacementCandidateValid,
			ActivePlacementDefinition);
		return;
	}

	ServerUpdateMovePlacedStationPreview(
		PendingMoveStation,
		PlacementWorkspaceRoom,
		PlacementAnchorCell,
		PlacementRotationQuarterTurns,
		bPlacementCandidateValid,
		ActivePlacementDefinition);
}

bool UVampireEconomyComponent::ResolvePlacementCandidate(AWorkspaceRoom*& OutWorkspaceRoom, FIntPoint& OutAnchorCell, FText& OutReason) const
{
	OutWorkspaceRoom = nullptr;
	OutAnchorCell = FIntPoint::ZeroValue;
	OutReason = LOCTEXT("PlacementNoRoomHit", "Richt met de crosshair op een werkruimte om te plaatsen.");

	APlayerController* PlayerController = ResolveOwningPlayerController(this);
	if (!PlayerController)
	{
		OutReason = LOCTEXT("PlacementMissingController", "Geen playercontroller beschikbaar voor placement preview.");
		return false;
	}

	APawn* OwningPawn = ResolveOwningPawn(this);

	FVector EyesLoc = FVector::ZeroVector;
	FRotator EyesRot = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(EyesLoc, EyesRot);

	const FVector PawnLoc = OwningPawn ? OwningPawn->GetActorLocation() : EyesLoc;
	const FVector AimDir = EyesRot.Vector();
	const FVector FocalLoc = EyesLoc + (AimDir * 1024.0f);
	const FVector StartPoint = FocalLoc + (((PawnLoc - FocalLoc) | AimDir) * AimDir);
	const FVector TraceStart = StartPoint;
	const FVector TraceEnd = TraceStart + (AimDir * 5000.0f);

	FHitResult TraceHit;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(StationPlacementTrace), false);
	if (OwningPawn)
	{
		QueryParams.AddIgnoredActor(OwningPawn);
	}

	const bool bHasBlockingHit = GetWorld() && GetWorld()->LineTraceSingleByChannel(TraceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	const FVector ReferencePoint = bHasBlockingHit ? TraceHit.ImpactPoint : TraceEnd;

	double BestScore = TNumericLimits<double>::Max();
	AWorkspaceRoom* BestRoom = nullptr;
	FIntPoint BestCell = FIntPoint::ZeroValue;

	for (TActorIterator<AWorkspaceRoom> It(GetWorld()); It; ++It)
	{
		AWorkspaceRoom* WorkspaceRoom = *It;
		if (!WorkspaceRoom)
		{
			continue;
		}

		const FVector LocalHit = WorkspaceRoom->GetActorTransform().InverseTransformPosition(ReferencePoint);
		const FIntPoint CandidateCell(
			FMath::FloorToInt(LocalHit.X / WorkspaceRoom->CellSize),
			FMath::FloorToInt(LocalHit.Y / WorkspaceRoom->CellSize));

		if (!WorkspaceRoom->IsCellWithinBounds(CandidateCell))
		{
			continue;
		}

		const double Score = FMath::Abs(static_cast<double>(LocalHit.Z));
		if (Score >= BestScore)
		{
			continue;
		}

		BestScore = Score;
		BestRoom = WorkspaceRoom;
		BestCell = CandidateCell;
	}

	if (!BestRoom)
	{
		OutReason = LOCTEXT("PlacementNoWorkspaceFromCrosshair", "De crosshair wijst nu niet naar een geldige werkruimtecel.");
		return false;
	}

	OutWorkspaceRoom = BestRoom;
	OutAnchorCell = BestCell;

	if (!BestRoom->IsCellWithinBounds(BestCell))
	{
		OutReason = LOCTEXT("PlacementOutOfBounds", "Deze cursorpositie valt buiten de werkruimte-grid.");
		return false;
	}

	OutReason = FText::GetEmpty();
	return true;
}

void UVampireEconomyComponent::DrawStationPlacementPreview() const
{
	const UPlaceableStationDataAsset* ActivePlacementDefinition = GetActivePlacementDefinition();
	if (!bStationPlacementModeActive || !ActivePlacementDefinition || !PlacementWorkspaceRoom || !GetWorld())
	{
		return;
	}

	const TArray<FIntPoint> PreviewCells = BuildPreviewCells(
		ActivePlacementDefinition,
		PlacementAnchorCell,
		PlacementRotationQuarterTurns);
	const FColor PreviewColor = bPlacementCandidateValid ? FColor::Green : FColor::Red;

	for (const FIntPoint& PreviewCell : PreviewCells)
	{
		const FVector CellCenter = PlacementWorkspaceRoom->GetCellWorldLocation(PreviewCell)
			+ FVector(PlacementWorkspaceRoom->CellSize * 0.5f, PlacementWorkspaceRoom->CellSize * 0.5f, 6.0f);
		const FVector Extent(PlacementWorkspaceRoom->CellSize * 0.45f, PlacementWorkspaceRoom->CellSize * 0.45f, 6.0f);
		DrawDebugSolidBox(GetWorld(), CellCenter, Extent, FColor(PreviewColor.R, PreviewColor.G, PreviewColor.B, 48));
		DrawDebugBox(GetWorld(), CellCenter, Extent, PreviewColor, false, 0.0f, 0, 2.0f);
	}

	const FString DebugText = FString::Printf(
		TEXT("%s\nRotatie: %d\nConfirm: Enter/E\nRotate: R\nCancel: X/Delete"),
		*PlacementCandidateReason.ToString(),
		PlacementRotationQuarterTurns);
	DrawDebugString(
		GetWorld(),
		PlacementWorkspaceRoom->GetCellWorldLocation(PlacementAnchorCell) + FVector(0.0f, 0.0f, 36.0f),
		DebugText,
		nullptr,
		PreviewColor,
		0.0f,
		false,
		1.1f);
}

void UVampireEconomyComponent::SyncPlacementPreviewActor()
{
	const UPlaceableStationDataAsset* ActivePlacementDefinition = GetActivePlacementDefinition();
	if (!bStationPlacementModeActive || !ActivePlacementDefinition || !PlacementWorkspaceRoom || !GetWorld())
	{
		if (PlacementPreviewActor)
		{
			PlacementPreviewActor->Destroy();
			PlacementPreviewActor = nullptr;
		}
		return;
	}

	if (!PlacementPreviewActor)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwner();
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		PlacementPreviewActor = GetWorld()->SpawnActor<AStationPlacementPreviewActor>(
			AStationPlacementPreviewActor::StaticClass(),
			PlacementWorkspaceRoom->GetCellWorldLocation(PlacementAnchorCell),
			PlacementWorkspaceRoom->GetPlacementWorldRotation(PlacementRotationQuarterTurns),
			SpawnParameters);
	}

	if (!PlacementPreviewActor)
	{
		return;
	}

	PlacementPreviewActor->SetActorLocationAndRotation(
		PlacementWorkspaceRoom->GetCellWorldLocation(PlacementAnchorCell),
		PlacementWorkspaceRoom->GetPlacementWorldRotation(PlacementRotationQuarterTurns));
	PlacementPreviewActor->ConfigurePreview(
		ActivePlacementDefinition->PreviewMesh.LoadSynchronous(),
		bPlacementCandidateValid);
}

void UVampireEconomyComponent::ApplyPlacementInputContext(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	if (PlacementHotkeyPlayerController && PlacementHotkeyPlayerController != PlayerController)
	{
		RemovePlacementInputContext(PlacementHotkeyPlayerController);
	}

	if (!PlacementHotkeyInputComponent)
	{
		PlacementHotkeyInputComponent = NewObject<UInputComponent>(PlayerController, TEXT("StationPlacementHotkeys"));
	}

	if (!PlacementHotkeyInputComponent)
	{
		return;
	}

	if (PlacementHotkeyPlayerController == PlayerController)
	{
		PlayerController->PopInputComponent(PlacementHotkeyInputComponent);
	}

	PlacementHotkeyInputComponent->ClearActionBindings();
	PlacementHotkeyInputComponent->Priority = 1001;
	PlacementHotkeyInputComponent->bBlockInput = false;
	PlacementHotkeyInputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &UVampireEconomyComponent::HandlePlacementConfirmPressed);
	PlacementHotkeyInputComponent->BindKey(EKeys::E, IE_Pressed, this, &UVampireEconomyComponent::HandlePlacementConfirmPressed);
	PlacementHotkeyInputComponent->BindKey(EKeys::R, IE_Pressed, this, &UVampireEconomyComponent::HandlePlacementRotatePressed);
	PlacementHotkeyInputComponent->BindKey(EKeys::X, IE_Pressed, this, &UVampireEconomyComponent::HandlePlacementCancelPressed);
	PlacementHotkeyInputComponent->BindKey(EKeys::Delete, IE_Pressed, this, &UVampireEconomyComponent::HandlePlacementCancelPressed);
	PlayerController->PushInputComponent(PlacementHotkeyInputComponent);
	PlacementHotkeyPlayerController = PlayerController;
}

void UVampireEconomyComponent::RemovePlacementInputContext(APlayerController* PlayerController)
{
	if (PlacementHotkeyInputComponent && PlacementHotkeyPlayerController == PlayerController && PlayerController)
	{
		PlayerController->PopInputComponent(PlacementHotkeyInputComponent);
		PlayerController->FlushPressedKeys();
		PlacementHotkeyPlayerController = nullptr;
	}
}

void UVampireEconomyComponent::RestorePendingMoveStationAfterPlacementCancel()
{
	if (PendingMoveStation)
	{
		if (OriginalPlacementWorkspaceRoom)
		{
			PendingMoveStation->SetActorLocationAndRotation(
				OriginalPlacementWorkspaceRoom->GetCellWorldLocation(OriginalPlacementAnchorCell),
				OriginalPlacementWorkspaceRoom->GetPlacementWorldRotation(OriginalPlacementRotationQuarterTurns),
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}

		PendingMoveStation->SetActorHiddenInGame(false);
		PendingMoveStation->SetActorEnableCollision(true);
		PendingMoveStation->ForceNetUpdate();
	}
}

void UVampireEconomyComponent::RestoreControllerInputAfterPlacement(APlayerController* PlayerController)
{
	if (!PlayerController)
	{
		return;
	}

	PlayerController->ResetIgnoreMoveInput();
	PlayerController->ResetIgnoreLookInput();
	PlayerController->FlushPressedKeys();
	PlayerController->SetShowMouseCursor(false);

	FInputModeGameOnly InputMode;
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetPause(false);
}

void UVampireEconomyComponent::StopStationPlacementMode(const bool bRestorePendingMoveStation)
{
	APlayerController* PlayerControllerToRestore = PlacementHotkeyPlayerController
		? PlacementHotkeyPlayerController.Get()
		: ResolveOwningPlayerController(this);
	ABloodProcessingStation* MoveStationToUnlock = PendingMoveStation.Get();

	if (bRestorePendingMoveStation)
	{
		RestorePendingMoveStationAfterPlacementCancel();
		if (MoveStationToUnlock)
		{
			if (GetOwnerRole() >= ROLE_Authority)
			{
				MoveStationToUnlock->EndMovePlacement(ResolveOwningPawn(this));
			}
			else
			{
				ServerEndMovePlacedStationPreview(MoveStationToUnlock);
			}
		}
	}
	else if (PendingMoveStation)
	{
		PendingMoveStation->SetActorHiddenInGame(false);
		PendingMoveStation->SetActorEnableCollision(true);
		PendingMoveStation->ForceNetUpdate();
	}

	RemovePlacementInputContext(PlacementHotkeyPlayerController);
	RestoreControllerInputAfterPlacement(PlayerControllerToRestore);

	if (PlacementPreviewActor)
	{
		PlacementPreviewActor->Destroy();
		PlacementPreviewActor = nullptr;
	}
	bStationPlacementModeActive = false;
	PendingPlacementItem = nullptr;
	PendingMoveStation = nullptr;
	PendingPlacementDefinition = nullptr;
	OriginalPlacementWorkspaceRoom = nullptr;
	OriginalPlacementAnchorCell = FIntPoint::ZeroValue;
	OriginalPlacementRotationQuarterTurns = 0;
	PlacementWorkspaceRoom = nullptr;
	PlacementAnchorCell = FIntPoint::ZeroValue;
	PlacementRotationQuarterTurns = 0;
	bPlacementCandidateValid = false;
	bPlacementConfirmPending = false;
	PlacementCandidateReason = FText::GetEmpty();
	LastReplicatedMovePreviewStation = nullptr;
	LastReplicatedMovePreviewWorkspaceRoom = nullptr;
	LastReplicatedMovePreviewAnchorCell = FIntPoint::ZeroValue;
	LastReplicatedMovePreviewRotationQuarterTurns = 0;
	bLastReplicatedMovePreviewValid = false;
	LastReplicatedMovePreviewDefinition = nullptr;
	bHasLastReplicatedMovePreview = false;
}

void UVampireEconomyComponent::HandlePlacementConfirmPressed()
{
	FText Result;
	const bool bSuccess = ConfirmStationPlacementMode(Result);
	SetInteractionFeedback(Result, bSuccess);
}

void UVampireEconomyComponent::HandlePlacementCancelPressed()
{
	FText Result;
	const bool bSuccess = CancelStationPlacementMode(Result);
	SetInteractionFeedback(Result, bSuccess);
}

void UVampireEconomyComponent::HandlePlacementRotatePressed()
{
	if (!bStationPlacementModeActive)
	{
		return;
	}

	RotateStationPlacementMode();
	SetInteractionFeedback(
		FText::Format(LOCTEXT("PlacementRotateFeedback", "Placement rotatie gewijzigd naar stand {0}."), FText::AsNumber(PlacementRotationQuarterTurns)),
		true);
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

	if (ProcessingStation->IsMovePlacementInProgress() && !ProcessingStation->IsCurrentOperator(ResolveOwningPawn(this)))
	{
		ClientReceiveInteractionFeedback(LOCTEXT("OpenStationServerMoveInProgress", "Dit station wordt momenteel door een andere speler verplaatst."), false);
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

void UVampireEconomyComponent::ServerBeginMovePlacedStationPreview_Implementation(ABloodProcessingStation* ProcessingStation)
{
	FText Reason;
	if (!ProcessingStation)
	{
		ClientReceiveInteractionFeedback(LOCTEXT("BeginMoveServerMissingStation", "Station context ontbreekt voor verplaatsen."), false);
		return;
	}

	if (!ProcessingStation->TryBeginMovePlacement(ResolveOwningPawn(this), Reason))
	{
		ClientReceiveInteractionFeedback(Reason, false);
	}
}

void UVampireEconomyComponent::ServerEndMovePlacedStationPreview_Implementation(ABloodProcessingStation* ProcessingStation)
{
	if (!ProcessingStation)
	{
		return;
	}

	ProcessingStation->EndMovePlacement(ResolveOwningPawn(this));
}

void UVampireEconomyComponent::ServerUpdateMovePlacedStationPreview_Implementation(
	ABloodProcessingStation* ProcessingStation,
	AWorkspaceRoom* WorkspaceRoom,
	const FIntPoint AnchorCell,
	const int32 RotationQuarterTurns,
	const bool bIsValidPlacement,
	UPlaceableStationDataAsset* StationDefinition)
{
	if (!ProcessingStation || !ProcessingStation->IsCurrentOperator(ResolveOwningPawn(this)))
	{
		return;
	}

	ProcessingStation->UpdateMovePlacementPreview(
		WorkspaceRoom,
		AnchorCell,
		RotationQuarterTurns,
		bIsValidPlacement,
		StationDefinition);
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
	ServerCancelManualProcessing_Implementation(PackagingStation);
}

void UVampireEconomyComponent::ServerCancelManualProcessing_Implementation(ABloodProcessingStation* ProcessingStation)
{
	if (!ProcessingStation)
	{
		ClientReceiveInteractionFeedback(FText::GetEmpty(), false);
		return;
	}

	FText Reason;
	if (!ClaimStationForComponent(this, ProcessingStation, Reason))
	{
		ClientReceiveInteractionFeedback(Reason, false);
		return;
	}

	const bool bSuccess = ProcessingStation->CancelManualProcessingRequest(Reason);
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

void UVampireEconomyComponent::ServerPlaceStationItem_Implementation(AWorkspaceRoom* WorkspaceRoom, UPlaceableStationItem* StationItem, const FIntPoint AnchorCell, const int32 RotationQuarterTurns)
{
	FText Result;
	const bool bSuccess = RequestPlaceStationItem(WorkspaceRoom, StationItem, AnchorCell, RotationQuarterTurns, Result);
	ClientReceiveInteractionFeedback(Result, bSuccess);
}

void UVampireEconomyComponent::ServerMovePlacedStation_Implementation(ABloodProcessingStation* ProcessingStation, AWorkspaceRoom* WorkspaceRoom, const FIntPoint AnchorCell, const int32 RotationQuarterTurns)
{
	FText Result;
	const bool bSuccess = RequestMovePlacedStation(ProcessingStation, WorkspaceRoom, AnchorCell, RotationQuarterTurns, Result);
	if (ProcessingStation)
	{
		ProcessingStation->EndMovePlacement(ResolveOwningPawn(this));
	}
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

bool UVampireEconomyComponent::StartStationPlacementMode(UPlaceableStationItem* StationItem, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!StationItem || !StationItem->StationDefinition)
	{
		OutReason = LOCTEXT("PlacementModeMissingItem", "Geen geldig station-item geselecteerd.");
		return false;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController(this);
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		OutReason = LOCTEXT("PlacementModeMissingController", "Geen lokale playercontroller gevonden voor placement mode.");
		return false;
	}

	if (bStationPlacementModeActive)
	{
		OutReason = LOCTEXT("PlacementModeAlreadyActive", "Station placement mode is al actief.");
		return false;
	}

	PendingPlacementItem = StationItem;
	PendingMoveStation = nullptr;
	PendingPlacementDefinition = StationItem->StationDefinition;
	OriginalPlacementWorkspaceRoom = nullptr;
	OriginalPlacementAnchorCell = FIntPoint::ZeroValue;
	OriginalPlacementRotationQuarterTurns = 0;
	PlacementWorkspaceRoom = nullptr;
	PlacementAnchorCell = FIntPoint::ZeroValue;
	PlacementRotationQuarterTurns = 0;
	bPlacementCandidateValid = false;
	PlacementCandidateReason = LOCTEXT("PlacementModeSearching", "Zoek een geldige plaats in een werkruimte.");
	bStationPlacementModeActive = true;

	LogOwnedActivatableWidgets(PlayerController, this, TEXT("StartStationPlacementMode_PreClose"));
	if (InventoryMenuToDeactivateOnStationUse.IsNull())
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementInputAudit: InventoryMenuToDeactivateOnStationUse is not configured on %s. Set this to the OwnSystem inventory menu class so station placement can mirror the normal DeactivateWidget() close flow."),
			*GetNameSafe(this));
	}
	else if (UClass* InventoryMenuClass = InventoryMenuToDeactivateOnStationUse.LoadSynchronous())
	{
		DeactivateOwnedMenuByClass(PlayerController, this, InventoryMenuClass, TEXT("StartStationPlacementMode"));
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("PlacementInputAudit: Failed to load InventoryMenuToDeactivateOnStationUse on %s. Verify the configured menu class is valid."),
			*GetNameSafe(this));
	}
	ApplyPlacementInputContext(PlayerController);
	CloseOwnedWidgetsOfClass<UInventoryWidget>(PlayerController, this, TEXT("StartStationPlacementMode"));
	LogOwnedActivatableWidgets(PlayerController, this, TEXT("StartStationPlacementMode_PostClose"));

	PlayerController->SetShowMouseCursor(false);
	FInputModeGameOnly InputMode;
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetPause(false);

	OutReason = LOCTEXT("PlacementModeStarted", "Placement mode actief. Beweeg de muis over een werkruimte, druk Enter of E om te plaatsen, R om te roteren en X of Delete om te annuleren.");
	return true;
}

bool UVampireEconomyComponent::StartMovePlacedStationMode(ABloodProcessingStation* ProcessingStation, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!ProcessingStation)
	{
		OutReason = LOCTEXT("MovePlacementMissingStation", "Geen geldig geplaatst station geselecteerd.");
		return false;
	}

	if (bStationPlacementModeActive)
	{
		OutReason = LOCTEXT("MovePlacementAlreadyActive", "Er is al een placement mode actief.");
		return false;
	}

	if (ProcessingStation->IsMovePlacementInProgress() && !ProcessingStation->IsCurrentOperator(ResolveOwningPawn(this)))
	{
		OutReason = LOCTEXT("MovePlacementAlreadyMovedByOther", "Dit station wordt momenteel door een andere speler verplaatst.");
		return false;
	}

	if (ProcessingStation->StationState != EBloodVatStationState::Leeg
		|| ProcessingStation->HasStagedManualProcessingRequest()
		|| ProcessingStation->HasSpawnedManualProcessingActor())
	{
		OutReason = LOCTEXT("MovePlacementNotSafe", "Een station kan alleen verplaatst worden als het leeg en veilig is.");
		return false;
	}

	if (!ProcessingStation->HasRegisteredPlacementContext())
	{
		OutReason = LOCTEXT("MovePlacementMissingPlacementContext", "Dit station is nog niet als geplaatst processing station geregistreerd.");
		return false;
	}

	AWorkspaceRoom* WorkspaceRoom = ProcessingStation->GetOwningWorkspaceRoom();
	const FGuid StationInstanceId = ProcessingStation->GetPlacedStationInstanceId();
	const FWorkspacePlacedStationRecord* MatchingRecord = WorkspaceRoom
		? WorkspaceRoom->FindPlacementRecordByInstanceId(StationInstanceId)
		: nullptr;
	if (!WorkspaceRoom || !MatchingRecord || !MatchingRecord->StationDefinition)
	{
		OutReason = LOCTEXT("MovePlacementMissingRecord", "Geen geldig plaatsingsrecord gevonden voor dit station.");
		return false;
	}

	APlayerController* PlayerController = ResolveOwningPlayerController(this);
	if (!PlayerController || !PlayerController->IsLocalController())
	{
		OutReason = LOCTEXT("MovePlacementMissingController", "Geen lokale playercontroller gevonden voor verplaatsen.");
		return false;
	}

	PendingPlacementItem = nullptr;
	PendingMoveStation = ProcessingStation;
	PendingPlacementDefinition = MatchingRecord->StationDefinition;
	OriginalPlacementWorkspaceRoom = WorkspaceRoom;
	OriginalPlacementAnchorCell = MatchingRecord->AnchorCell;
	OriginalPlacementRotationQuarterTurns = MatchingRecord->RotationQuarterTurns;
	PlacementWorkspaceRoom = WorkspaceRoom;
	PlacementAnchorCell = MatchingRecord->AnchorCell;
	PlacementRotationQuarterTurns = MatchingRecord->RotationQuarterTurns;
	bPlacementCandidateValid = false;
	PlacementCandidateReason = LOCTEXT("MovePlacementSearching", "Kies een nieuwe plaats voor dit station.");
	bStationPlacementModeActive = true;

	if (GetOwnerRole() >= ROLE_Authority)
	{
		if (!ProcessingStation->TryBeginMovePlacement(ResolveOwningPawn(this), OutReason))
		{
			bStationPlacementModeActive = false;
			PendingMoveStation = nullptr;
			PendingPlacementDefinition = nullptr;
			return false;
		}
	}
	else
	{
		ServerBeginMovePlacedStationPreview(ProcessingStation);
	}

	ProcessingStation->SetActorHiddenInGame(true);
	ProcessingStation->SetActorEnableCollision(false);

	LogOwnedActivatableWidgets(PlayerController, this, TEXT("StartMovePlacedStationMode_PreClose"));
	ApplyPlacementInputContext(PlayerController);
	TArray<UUserWidget*> ActivatableWidgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, ActivatableWidgets, UOwnSystemActivatableWidget::StaticClass(), false);
	for (UUserWidget* Widget : ActivatableWidgets)
	{
		UOwnSystemActivatableWidget* ActivatableWidget = Cast<UOwnSystemActivatableWidget>(Widget);
		if (!ActivatableWidget || ActivatableWidget->GetOwningPlayer() != PlayerController)
		{
			continue;
		}

		ActivatableWidget->DeactivateWidget();
		ActivatableWidget->RemoveFromParent();
	}
	LogOwnedActivatableWidgets(PlayerController, this, TEXT("StartMovePlacedStationMode_PostClose"));

	PlayerController->SetShowMouseCursor(false);
	FInputModeGameOnly InputMode;
	PlayerController->SetInputMode(InputMode);
	PlayerController->SetPause(false);

	OutReason = LOCTEXT("MovePlacementStarted", "Verplaats mode actief. Richt op een nieuwe plek, druk Enter of E om te bevestigen, R om te roteren en X of Delete om te annuleren.");
	return true;
}

bool UVampireEconomyComponent::CancelStationPlacementMode(FText& OutReason)
{
	if (!bStationPlacementModeActive)
	{
		OutReason = LOCTEXT("PlacementModeNotActive", "Er is geen actieve placement mode om te annuleren.");
		return false;
	}

	StopStationPlacementMode();
	OutReason = LOCTEXT("PlacementModeCancelled", "Placement mode geannuleerd.");
	return true;
}

bool UVampireEconomyComponent::ConfirmStationPlacementMode(FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!bStationPlacementModeActive || (!PendingPlacementItem && !PendingMoveStation))
	{
		OutReason = LOCTEXT("PlacementModeNoPendingTarget", "Er is geen station geselecteerd om te plaatsen of verplaatsen.");
		return false;
	}

	if (!bPlacementCandidateValid || !PlacementWorkspaceRoom)
	{
		OutReason = PlacementCandidateReason.IsEmpty()
			? LOCTEXT("PlacementModeInvalidCandidate", "Kies eerst een geldige plaats in een werkruimte.")
			: PlacementCandidateReason;
		return false;
	}

	if (bPlacementConfirmPending)
	{
		OutReason = LOCTEXT("PlacementModeConfirmPending", "Plaatsingsverzoek al verzonden. Wacht op serverbevestiging.");
		return false;
	}

	const bool bSuccess = PendingMoveStation
		? RequestMovePlacedStation(
			PendingMoveStation,
			PlacementWorkspaceRoom,
			PlacementAnchorCell,
			PlacementRotationQuarterTurns,
			OutReason)
		: RequestPlaceStationItem(
			PlacementWorkspaceRoom,
			PendingPlacementItem,
			PlacementAnchorCell,
			PlacementRotationQuarterTurns,
			OutReason);

	if (GetOwnerRole() < ROLE_Authority && bSuccess)
	{
		bPlacementConfirmPending = true;
		return true;
	}

	if (bSuccess)
	{
		StopStationPlacementMode(false);
	}

	return bSuccess;
}

void UVampireEconomyComponent::RotateStationPlacementMode()
{
	if (!bStationPlacementModeActive)
	{
		return;
	}

	PlacementRotationQuarterTurns = (PlacementRotationQuarterTurns + 1) % 4;
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
