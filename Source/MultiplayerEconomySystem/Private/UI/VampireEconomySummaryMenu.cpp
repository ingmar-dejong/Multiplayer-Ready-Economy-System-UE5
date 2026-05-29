#include "UI/VampireEconomySummaryMenu.h"

#include "Blueprint/WidgetTree.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "UnrealFramework/OwnSystemGameState.h"
#include "Items/InventoryComponent.h"
#include "Items/OwnSystemItem.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Vampire/Data/BloodBuyerDataAsset.h"
#include "Vampire/Economy/VampireEconomyComponent.h"
#include "Vampire/Items/BloodProductItem.h"
#include "Vampire/Items/PlaceableStationItem.h"
#include "Vampire/World/BloodBuyerNPC.h"
#include "Vampire/World/WorkspaceRoom.h"
#include "Widgets/OwnSystemCommonButtonBase.h"
#include "Widgets/OwnSystemCommonTextBlock.h"

namespace
{
	bool DoBloodItemsMatchForUiBatch(const UBloodProductItem* A, const UBloodProductItem* B)
	{
		return A
			&& B
			&& A->GetClass() == B->GetClass()
			&& A->SourceType == B->SourceType
			&& A->BaseQuality == B->BaseQuality
			&& A->ProcessingType == B->ProcessingType;
	}

	FString JoinSourceTypes(const TArray<EBloodSourceType>& SourceTypes)
	{
		if (SourceTypes.IsEmpty())
		{
			return TEXT("Alle bronnen");
		}

		FString Result;
		for (int32 Index = 0; Index < SourceTypes.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(", ");
			}

			Result += UBloodProductItem::GetSourceDisplayName(SourceTypes[Index]).ToString();
		}

		return Result;
	}

	FString JoinProcessingTypes(const TArray<EBloodProcessingType>& ProcessingTypes)
	{
		if (ProcessingTypes.IsEmpty())
		{
			return TEXT("Alle verwerkingen");
		}

		FString Result;
		for (int32 Index = 0; Index < ProcessingTypes.Num(); ++Index)
		{
			if (Index > 0)
			{
				Result += TEXT(", ");
			}

			Result += UBloodProductItem::GetProcessingDisplayName(ProcessingTypes[Index]).ToString();
		}

		return Result;
	}
}

#define LOCTEXT_NAMESPACE "VampireEconomySummaryMenu"

UVampireEconomySummaryMenu::UVampireEconomySummaryMenu()
{
	InputConfig = EOwnSystemWidgetInputMode::GameAndMenu;
}

void UVampireEconomySummaryMenu::SetBuyerContext(ABloodBuyerNPC* InBuyer, APawn* InInteractor)
{
	BoundBuyer = InBuyer;
	BoundBuyerInteractor = InInteractor;
	SelectedBloodItemIndex = 0;
}

void UVampireEconomySummaryMenu::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);

	BoundInventory = ResolveInventory(this);
	BoundEconomy = ResolveEconomy(this);

	if (BoundInventory)
	{
		BoundInventory->OnInventoryUpdated.AddDynamic(this, &UVampireEconomySummaryMenu::HandleInventoryUpdated);
		BoundInventory->OnCurrencyChanged.AddDynamic(this, &UVampireEconomySummaryMenu::HandleCurrencyChanged);
	}

	if (BoundEconomy)
	{
		BoundEconomy->OnEconomyUpdated.AddDynamic(this, &UVampireEconomySummaryMenu::HandleEconomyUpdated);
	}

	if (BtnProcessDailyUpkeep)
	{
		BtnProcessDailyUpkeep->OnClicked().AddUObject(this, &UVampireEconomySummaryMenu::HandleProcessDailyUpkeepClicked);
	}

	if (BtnPrevBloodItem)
	{
		BtnPrevBloodItem->OnClicked().AddUObject(this, &UVampireEconomySummaryMenu::HandlePrevBloodItemClicked);
	}

	if (BtnNextBloodItem)
	{
		BtnNextBloodItem->OnClicked().AddUObject(this, &UVampireEconomySummaryMenu::HandleNextBloodItemClicked);
	}

	if (BtnProcessSelected)
	{
		BtnProcessSelected->OnClicked().AddUObject(this, &UVampireEconomySummaryMenu::HandleProcessSelectedClicked);
	}

	if (BtnPrevThrall)
	{
		BtnPrevThrall->OnClicked().AddUObject(this, &UVampireEconomySummaryMenu::HandlePrevThrallClicked);
	}

	if (BtnNextThrall)
	{
		BtnNextThrall->OnClicked().AddUObject(this, &UVampireEconomySummaryMenu::HandleNextThrallClicked);
	}

	if (BtnAddThrall)
	{
		BtnAddThrall->OnClicked().AddUObject(this, &UVampireEconomySummaryMenu::HandleAddThrallClicked);
	}

	if (BtnClose)
	{
		BtnClose->OnClicked().AddUObject(this, &UVampireEconomySummaryMenu::HandleCloseClicked);
	}

	RefreshSummary();
}

void UVampireEconomySummaryMenu::NativeDestruct()
{
	if (BoundInventory)
	{
		BoundInventory->OnInventoryUpdated.RemoveDynamic(this, &UVampireEconomySummaryMenu::HandleInventoryUpdated);
		BoundInventory->OnCurrencyChanged.RemoveDynamic(this, &UVampireEconomySummaryMenu::HandleCurrencyChanged);
	}

	if (BoundEconomy)
	{
		BoundEconomy->OnEconomyUpdated.RemoveDynamic(this, &UVampireEconomySummaryMenu::HandleEconomyUpdated);
	}

	if (BtnProcessDailyUpkeep)
	{
		BtnProcessDailyUpkeep->OnClicked().RemoveAll(this);
	}

	if (BtnPrevBloodItem)
	{
		BtnPrevBloodItem->OnClicked().RemoveAll(this);
	}

	if (BtnNextBloodItem)
	{
		BtnNextBloodItem->OnClicked().RemoveAll(this);
	}

	if (BtnProcessSelected)
	{
		BtnProcessSelected->OnClicked().RemoveAll(this);
	}

	if (BtnPrevThrall)
	{
		BtnPrevThrall->OnClicked().RemoveAll(this);
	}

	if (BtnNextThrall)
	{
		BtnNextThrall->OnClicked().RemoveAll(this);
	}

	if (BtnAddThrall)
	{
		BtnAddThrall->OnClicked().RemoveAll(this);
	}

	if (BtnClose)
	{
		BtnClose->OnClicked().RemoveAll(this);
	}

	Super::NativeDestruct();
}

UWidget* UVampireEconomySummaryMenu::NativeGetDesiredFocusTarget() const
{
	return const_cast<UVampireEconomySummaryMenu*>(this);
}

FReply UVampireEconomySummaryMenu::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (!IsInBuyerContext() && InKeyEvent.GetKey() == EKeys::P)
	{
		if (HandleDebugPlaceFirstStation())
		{
			return FReply::Handled();
		}
	}

	if (IsInBuyerContext())
	{
		if (InKeyEvent.GetKey() == EKeys::Q || InKeyEvent.GetKey() == EKeys::Left)
		{
			HandlePrevBloodItemClicked();
			return FReply::Handled();
		}

		if (InKeyEvent.GetKey() == EKeys::E || InKeyEvent.GetKey() == EKeys::Right)
		{
			HandleNextBloodItemClicked();
			return FReply::Handled();
		}
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UVampireEconomySummaryMenu::RefreshSummary()
{
	ClampSelectedBloodItemIndex();
	ClampSelectedThrallIndex();

	if (TxtEconomySummary)
	{
		TxtEconomySummary->SetText(BuildSummaryText());
	}

	if (TxtBloodItems)
	{
		TxtBloodItems->SetText(BuildBloodItemsText());
	}

	if (TxtThralls)
	{
		TxtThralls->SetText(BuildThrallListText());
	}

	if (TxtSelectedBloodItem)
	{
		TxtSelectedBloodItem->SetText(BuildSelectedBloodItemText());
	}

	if (TxtSelectedThrall)
	{
		TxtSelectedThrall->SetText(BuildSelectedThrallText());
	}

	UpdateButtonStateForCurrentContext();
}

FText UVampireEconomySummaryMenu::BuildSummaryText() const
{
	if (const ABloodBuyerNPC* Buyer = BoundBuyer.Get())
	{
		const UOwnSystemInventoryComponent* Inventory = ResolveInventory(this);
		const int32 Currency = Inventory ? Inventory->GetCurrency() : 0;
		const FText LastFeedback = BoundEconomy ? BoundEconomy->GetLastFeedbackMessage() : LOCTEXT("NoBuyerFeedback", "Nog geen buyeractie uitgevoerd.");
		const FText BuyerName = Buyer->BuyerData && !Buyer->BuyerData->BuyerName.IsEmpty()
			? Buyer->BuyerData->BuyerName
			: LOCTEXT("DefaultBuyerName", "Blood Buyer");

		return FText::Format(
			LOCTEXT("BuyerSummaryFmt", "{0}\n\nKoopt locatiegebonden blood batches.\nMinimum quality: {1}\nBasispayout per unit: {2}\nGoud: {3}\n\nLaatste actie:\n{4}"),
			BuyerName,
			Buyer->BuyerData ? UBloodProductItem::GetQualityDisplayName(Buyer->BuyerData->MinimumQuality) : LOCTEXT("UnknownQualityReq", "Onbekend"),
			FText::AsNumber(Buyer->BuyerData ? Buyer->BuyerData->BasePayoutPerUnit : 0.0f),
			FText::AsNumber(Currency),
			LastFeedback);
	}

	const UOwnSystemInventoryComponent* Inventory = ResolveInventory(this);
	const UVampireEconomyComponent* Economy = ResolveEconomy(this);

	const int32 Currency = Inventory ? Inventory->GetCurrency() : 0;
	const int32 BloodItemCount = CountBloodItems(Inventory);
	const int32 ActiveThralls = Economy ? Economy->GetActiveThrallCount() : 0;
	const int32 DailyUpkeep = Economy ? Economy->GetTotalDailyBloodUpkeep() : 0;
	const FText LastFeedback = Economy ? Economy->GetLastFeedbackMessage() : LOCTEXT("NoFeedback", "Nog geen acties uitgevoerd.");

	return FText::Format(
		LOCTEXT("EconomySummaryFmt", "Blood Economy\n\nGoud: {0}\nBlood items: {1}\nActieve thralls: {2}\nDagelijkse upkeep: {3} bloed\n\nLaatste actie:\n{4}"),
		FText::AsNumber(Currency),
		FText::AsNumber(BloodItemCount),
		FText::AsNumber(ActiveThralls),
		FText::AsNumber(DailyUpkeep),
		LastFeedback);
}

FText UVampireEconomySummaryMenu::BuildBloodItemsText() const
{
	if (IsInBuyerContext())
	{
		TArray<UBloodProductItem*> Representatives;
		TArray<int32> TotalUnits;
		GetBuyerCandidateGroups(Representatives, TotalUnits);

		FString Result = TEXT("Verkoopbare groepen\n\n");
		if (Representatives.IsEmpty())
		{
			Result += TEXT("Geen blood groepen aanwezig in inventory.");
			return FText::FromString(Result);
		}

		for (int32 Index = 0; Index < Representatives.Num(); ++Index)
		{
			const UBloodProductItem* Item = Representatives[Index];
			if (!Item)
			{
				continue;
			}

			FText Reason;
			const int32 GroupUnits = TotalUnits.IsValidIndex(Index) ? TotalUnits[Index] : 0;
			const bool bValid = BoundEconomy && BoundBuyer.IsValid() && BoundBuyer->BuyerData
				? BoundEconomy->CanSellBloodItemToBuyer(Item, BoundBuyer->BuyerData, Reason)
				: false;
			Result += FString::Printf(
				TEXT("%d. %s | Totaal units: %d | %s%s\n"),
				Index + 1,
				*Item->GetBloodDisplayName().ToString(),
				GroupUnits,
				bValid ? TEXT("Verkoopbaar") : *Reason.ToString(),
				Index == SelectedBloodItemIndex ? TEXT("  <") : TEXT(""));
		}

		return FText::FromString(Result);
	}

	const UOwnSystemInventoryComponent* Inventory = ResolveInventory(this);
	if (!Inventory)
	{
		return LOCTEXT("NoInventoryBloodItems", "Blood items\n\nGeen inventory beschikbaar.");
	}

	FString Result = TEXT("Blood items\n\n");
	int32 EntryIndex = 1;

	for (UOwnSystemItem* Item : Inventory->GetItems())
	{
		if (const UBloodProductItem* BloodItem = Cast<UBloodProductItem>(Item))
		{
			Result += FString::Printf(
				TEXT("%d. %s | %d units\n"),
				EntryIndex,
				*BloodItem->GetBloodSummaryText().ToString(),
				BloodItem->BloodQuantity);
			EntryIndex++;
		}
	}

	if (EntryIndex == 1)
	{
		Result += TEXT("Geen blood items aanwezig.");
	}

	return FText::FromString(Result);
}

FText UVampireEconomySummaryMenu::BuildThrallListText() const
{
	if (const ABloodBuyerNPC* Buyer = BoundBuyer.Get())
	{
		const FText BuyerName = Buyer->BuyerData && !Buyer->BuyerData->BuyerName.IsEmpty()
			? Buyer->BuyerData->BuyerName
			: LOCTEXT("FallbackBuyerName", "Blood Buyer");
		const FString AcceptedSources = Buyer->BuyerData ? JoinSourceTypes(Buyer->BuyerData->AcceptedSources) : TEXT("Onbekend");
		const FString AcceptedProcessing = Buyer->BuyerData ? JoinProcessingTypes(Buyer->BuyerData->AcceptedProcessingTypes) : TEXT("Onbekend");
		return FText::Format(
			LOCTEXT("BuyerInfoPane", "Buyer info\n\nNaam: {0}\nMinimum quality: {1}\nAccepteert bronnen: {2}\nAccepteert verwerking: {3}"),
			BuyerName,
			Buyer->BuyerData ? UBloodProductItem::GetQualityDisplayName(Buyer->BuyerData->MinimumQuality) : LOCTEXT("UnknownBuyerMinQuality", "Onbekend"),
			FText::FromString(AcceptedSources),
			FText::FromString(AcceptedProcessing));
	}

	const UVampireEconomyComponent* Economy = ResolveEconomy(this);
	if (!Economy)
	{
		return LOCTEXT("NoEconomyThralls", "Thralls\n\nGeen economy component beschikbaar.");
	}

	FString Result = TEXT("Thralls\n\n");
	const TArray<FThrallUpkeepUnit>& Thralls = Economy->GetThrallUnits();

	if (Thralls.IsEmpty())
	{
		Result += TEXT("Geen thralls aanwezig.");
		return FText::FromString(Result);
	}

	for (const FThrallUpkeepUnit& Thrall : Thralls)
	{
		const FString TierText = Thrall.Tier == EThrallTier::ThrallII ? TEXT("Thrall II") : TEXT("Thrall I");
		const FString QualityText =
			Thrall.RequiredQuality == EBloodQuality::Premium ? TEXT("Premium") :
			Thrall.RequiredQuality == EBloodQuality::Goed ? TEXT("Goed") :
			TEXT("Gewoon");

		Result += FString::Printf(TEXT("%s | %s | Loyalty %.2f | Upkeep %d | Min %s"),
			Thrall.ThrallId.IsNone() ? TEXT("Thrall") : *Thrall.ThrallId.ToString(),
			*TierText,
			static_cast<double>(Thrall.Loyalty),
			Thrall.RequiredBloodUnitsPerDay,
			*QualityText);
		Result += Thrall.bActive ? TEXT("\n") : TEXT(" | Inactief\n");
	}

	return FText::FromString(Result);
}

FText UVampireEconomySummaryMenu::BuildSelectedBloodItemText() const
{
	if (IsInBuyerContext())
	{
		TArray<UBloodProductItem*> Representatives;
		TArray<int32> TotalUnits;
		GetBuyerCandidateGroups(Representatives, TotalUnits);
		if (Representatives.IsEmpty())
		{
			return LOCTEXT("NoBuyerBatchSelected", "Geselecteerde verkoopgroep\n\nGeen blood groepen beschikbaar.");
		}

		const int32 SafeIndex = FMath::Clamp(SelectedBloodItemIndex, 0, Representatives.Num() - 1);
		const UBloodProductItem* SelectedItem = Representatives[SafeIndex];
		const int32 GroupUnits = TotalUnits.IsValidIndex(SafeIndex) ? TotalUnits[SafeIndex] : 0;
		FText Reason;
		const bool bValid = BoundEconomy && BoundBuyer.IsValid() && BoundBuyer->BuyerData
			? BoundEconomy->CanSellBloodItemToBuyer(SelectedItem, BoundBuyer->BuyerData, Reason)
			: false;
		const int32 EstimatedPayout = EstimateBuyerGroupPayout(SelectedItem, GroupUnits);
		const FString AcceptedSources = BoundBuyer.IsValid() && BoundBuyer->BuyerData ? JoinSourceTypes(BoundBuyer->BuyerData->AcceptedSources) : TEXT("Onbekend");
		const FString AcceptedProcessing = BoundBuyer.IsValid() && BoundBuyer->BuyerData ? JoinProcessingTypes(BoundBuyer->BuyerData->AcceptedProcessingTypes) : TEXT("Onbekend");

		return FText::Format(
			LOCTEXT("SelectedBuyerBatchFmt", "Geselecteerde verkoopgroep\n\nIndex: {0}/{1}\n{2}\nTotaal units: {3}\nGeschatte payout: {4}\nBuyer zoekt:\n- Min quality: {5}\n- Bronnen: {6}\n- Verwerking: {7}\n\nValidatie: {8}"),
			FText::AsNumber(SafeIndex + 1),
			FText::AsNumber(Representatives.Num()),
			SelectedItem ? SelectedItem->GetBloodSummaryText() : LOCTEXT("InvalidBuyerBatch", "Ongeldige groep"),
			FText::AsNumber(GroupUnits),
			FText::AsNumber(EstimatedPayout),
			BoundBuyer.IsValid() && BoundBuyer->BuyerData ? UBloodProductItem::GetQualityDisplayName(BoundBuyer->BuyerData->MinimumQuality) : LOCTEXT("UnknownBuyerQuality", "Onbekend"),
			FText::FromString(AcceptedSources),
			FText::FromString(AcceptedProcessing),
			bValid ? LOCTEXT("BuyerGroupValid", "Verkoopbaar aan deze buyer") : Reason);
	}

	const TArray<UBloodProductItem*> BloodItems = GetBloodItems();
	if (BloodItems.IsEmpty())
	{
		return LOCTEXT("NoSelectedBloodItem", "Selected blood item\n\nGeen blood item geselecteerd.");
	}

	const int32 SafeIndex = FMath::Clamp(SelectedBloodItemIndex, 0, BloodItems.Num() - 1);
	const UBloodProductItem* SelectedItem = BloodItems[SafeIndex];
	if (!SelectedItem)
	{
		return LOCTEXT("InvalidSelectedBloodItem", "Selected blood item\n\nSelectie ongeldig.");
	}

	return FText::Format(
		LOCTEXT("SelectedBloodItemFmt", "Selected blood item\n\nIndex: {0}/{1}\n{2}"),
		FText::AsNumber(SafeIndex + 1),
		FText::AsNumber(BloodItems.Num()),
		SelectedItem->GetBloodSummaryText());
}

FText UVampireEconomySummaryMenu::BuildSelectedThrallText() const
{
	if (IsInBuyerContext())
	{
		return FText::Format(
			LOCTEXT("BuyerActionPane", "Actie\n\nGebruik vorige/volgende of Q/E om tussen verkoopgroepen te wisselen.\nPrimaire actie: {0}."),
			GetBuyerPrimaryActionText());
	}

	const int32 ThrallCount = GetThrallCount();
	if (ThrallCount <= 0)
	{
		return LOCTEXT("NoSelectedThrall", "Selected thrall\n\nGeen thrall geselecteerd.");
	}

	const FThrallUpkeepUnit* SelectedThrall = GetSelectedThrall();
	if (!SelectedThrall)
	{
		return LOCTEXT("InvalidSelectedThrall", "Selected thrall\n\nSelectie ongeldig.");
	}

	const FString TierText = SelectedThrall->Tier == EThrallTier::ThrallII ? TEXT("Thrall II") : TEXT("Thrall I");
	const FString QualityText =
		SelectedThrall->RequiredQuality == EBloodQuality::Premium ? TEXT("Premium") :
		SelectedThrall->RequiredQuality == EBloodQuality::Goed ? TEXT("Goed") :
		TEXT("Gewoon");

	return FText::FromString(FString::Printf(
		TEXT("Selected thrall\n\nIndex: %d/%d\nId: %s\nTier: %s\nLoyalty: %.2f\nDaily upkeep: %d\nMinimum quality: %s\nStatus: %s"),
		FMath::Clamp(SelectedThrallIndex, 0, ThrallCount - 1) + 1,
		ThrallCount,
		SelectedThrall->ThrallId.IsNone() ? TEXT("Thrall") : *SelectedThrall->ThrallId.ToString(),
		*TierText,
		static_cast<double>(SelectedThrall->Loyalty),
		SelectedThrall->RequiredBloodUnitsPerDay,
		*QualityText,
		SelectedThrall->bActive ? TEXT("Actief") : TEXT("Inactief")));
}

UOwnSystemInventoryComponent* UVampireEconomySummaryMenu::ResolveInventory(const UUserWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}

	if (const APlayerController* PC = Widget->GetOwningPlayer())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			if (UOwnSystemInventoryComponent* Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(Pawn))
			{
				return Inventory;
			}
		}

		return UVampireEconomyComponent::ResolveInventoryFromActor(PC);
	}

	return nullptr;
}

UVampireEconomyComponent* UVampireEconomySummaryMenu::ResolveEconomy(const UUserWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}

	if (const APlayerController* PC = Widget->GetOwningPlayer())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			if (UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Pawn))
			{
				return Economy;
			}
		}

		return UVampireEconomyComponent::ResolveEconomyFromActor(PC);
	}

	return nullptr;
}

int32 UVampireEconomySummaryMenu::CountBloodItems(const UOwnSystemInventoryComponent* Inventory)
{
	if (!Inventory)
	{
		return 0;
	}

	int32 Count = 0;
	for (UOwnSystemItem* Item : Inventory->GetItems())
	{
		if (Cast<UBloodProductItem>(Item))
		{
			Count++;
		}
	}

	return Count;
}

TArray<UBloodProductItem*> UVampireEconomySummaryMenu::GetBloodItems() const
{
	TArray<UBloodProductItem*> BloodItems;
	const UOwnSystemInventoryComponent* Inventory = ResolveInventory(this);
	if (!Inventory)
	{
		return BloodItems;
	}

	for (UOwnSystemItem* Item : Inventory->GetItems())
	{
		if (UBloodProductItem* BloodItem = Cast<UBloodProductItem>(Item))
		{
			BloodItems.Add(BloodItem);
		}
	}

	return BloodItems;
}

UBloodProductItem* UVampireEconomySummaryMenu::GetSelectedBloodItem() const
{
	const TArray<UBloodProductItem*> BloodItems = GetBloodItems();
	if (BloodItems.IsEmpty())
	{
		return nullptr;
	}

	const int32 SafeIndex = FMath::Clamp(SelectedBloodItemIndex, 0, BloodItems.Num() - 1);
	return BloodItems[SafeIndex];
}

void UVampireEconomySummaryMenu::ClampSelectedBloodItemIndex()
{
	if (IsInBuyerContext())
	{
		TArray<UBloodProductItem*> Representatives;
		TArray<int32> TotalUnits;
		GetBuyerCandidateGroups(Representatives, TotalUnits);
		if (Representatives.IsEmpty())
		{
			SelectedBloodItemIndex = 0;
			return;
		}

		SelectedBloodItemIndex = FMath::Clamp(SelectedBloodItemIndex, 0, Representatives.Num() - 1);
		return;
	}

	const TArray<UBloodProductItem*> BloodItems = GetBloodItems();
	if (BloodItems.IsEmpty())
	{
		SelectedBloodItemIndex = 0;
		return;
	}

	SelectedBloodItemIndex = FMath::Clamp(SelectedBloodItemIndex, 0, BloodItems.Num() - 1);
}

bool UVampireEconomySummaryMenu::IsInBuyerContext() const
{
	return BoundBuyer.IsValid();
}

void UVampireEconomySummaryMenu::GetBuyerCandidateGroups(TArray<UBloodProductItem*>& OutRepresentatives, TArray<int32>& OutTotalUnits) const
{
	OutRepresentatives.Reset();
	OutTotalUnits.Reset();

	const UOwnSystemInventoryComponent* Inventory = ResolveInventory(this);
	if (!Inventory)
	{
		return;
	}

	for (UOwnSystemItem* Item : Inventory->GetItems())
	{
		UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item);
		if (!Candidate)
		{
			continue;
		}

		int32 ExistingIndex = INDEX_NONE;
		for (int32 Index = 0; Index < OutRepresentatives.Num(); ++Index)
		{
			if (DoBloodItemsMatchForUiBatch(OutRepresentatives[Index], Candidate))
			{
				ExistingIndex = Index;
				break;
			}
		}

		if (ExistingIndex == INDEX_NONE)
		{
			OutRepresentatives.Add(Candidate);
			OutTotalUnits.Add(Candidate->BloodQuantity);
		}
		else
		{
			OutTotalUnits[ExistingIndex] += Candidate->BloodQuantity;
		}
	}
}

int32 UVampireEconomySummaryMenu::GetSelectedBuyerGroupUnits() const
{
	TArray<UBloodProductItem*> Representatives;
	TArray<int32> TotalUnits;
	GetBuyerCandidateGroups(Representatives, TotalUnits);
	if (Representatives.IsEmpty())
	{
		return 0;
	}

	const int32 SafeIndex = FMath::Clamp(SelectedBloodItemIndex, 0, TotalUnits.Num() - 1);
	return TotalUnits.IsValidIndex(SafeIndex) ? TotalUnits[SafeIndex] : 0;
}

int32 UVampireEconomySummaryMenu::EstimateBuyerGroupPayout(const UBloodProductItem* Representative, const int32 TotalUnits) const
{
	if (!Representative || !BoundBuyer.IsValid() || !BoundBuyer->BuyerData)
	{
		return 0;
	}

	const float QualityMultiplier =
		Representative->BaseQuality == EBloodQuality::Premium ? 2.25f :
		Representative->BaseQuality == EBloodQuality::Goed ? 1.5f :
		1.0f;

	const float ProcessingMultiplier =
		Representative->ProcessingType == EBloodProcessingType::GerijptOpHout ? 1.35f :
		Representative->ProcessingType == EBloodProcessingType::Gekruid ? 1.2f :
		Representative->ProcessingType == EBloodProcessingType::Gezuiverd ? 1.4f :
		Representative->ProcessingType == EBloodProcessingType::Verdund ? 0.75f :
		Representative->ProcessingType == EBloodProcessingType::RitueelBehandeld ? 1.75f :
		Representative->ProcessingType == EBloodProcessingType::GekoeldBewaard ? 1.15f :
		1.0f;

	const float RawPayout = BoundBuyer->BuyerData->BasePayoutPerUnit
		* TotalUnits
		* QualityMultiplier
		* ProcessingMultiplier;

	return FMath::Max(1, FMath::RoundToInt(RawPayout));
}

FText UVampireEconomySummaryMenu::GetBuyerPrimaryActionText() const
{
	return LOCTEXT("BuyerPrimaryAction", "Verkoop batch");
}

void UVampireEconomySummaryMenu::UpdateButtonStateForCurrentContext()
{
	if (IsInBuyerContext())
	{
		if (BtnPrevBloodItem)
		{
			BtnPrevBloodItem->SetButtonText(LOCTEXT("BuyerPrevBatchBtn", "Vorige groep"));
		}

		if (BtnNextBloodItem)
		{
			BtnNextBloodItem->SetButtonText(LOCTEXT("BuyerNextBatchBtn", "Volgende groep"));
		}

		if (BtnProcessSelected)
		{
			BtnProcessSelected->SetIsEnabled(true);
			BtnProcessSelected->SetVisibility(ESlateVisibility::Visible);
			BtnProcessSelected->SetButtonText(GetBuyerPrimaryActionText());
		}

		if (BtnPrevThrall)
		{
			BtnPrevThrall->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (BtnNextThrall)
		{
			BtnNextThrall->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (BtnProcessDailyUpkeep)
		{
			BtnProcessDailyUpkeep->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (BtnAddThrall)
		{
			BtnAddThrall->SetVisibility(ESlateVisibility::Collapsed);
		}

		if (BtnClose)
		{
			BtnClose->SetButtonText(LOCTEXT("BuyerCloseBtn", "Sluiten"));
		}

		return;
	}
	if (BtnPrevBloodItem)
	{
		BtnPrevBloodItem->SetVisibility(ESlateVisibility::Visible);
		BtnPrevBloodItem->SetButtonText(LOCTEXT("EconomyPrevItemBtn", "Vorig item"));
	}

	if (BtnNextBloodItem)
	{
		BtnNextBloodItem->SetVisibility(ESlateVisibility::Visible);
		BtnNextBloodItem->SetButtonText(LOCTEXT("EconomyNextItemBtn", "Volgend item"));
	}

	if (BtnProcessSelected)
	{
		BtnProcessSelected->SetIsEnabled(true);
		BtnProcessSelected->SetVisibility(ESlateVisibility::Visible);
		BtnProcessSelected->SetButtonText(LOCTEXT("EconomyDebugPlaceBtn", "Debug plaats station"));
	}

	if (BtnPrevThrall)
	{
		BtnPrevThrall->SetVisibility(ESlateVisibility::Visible);
	}

	if (BtnNextThrall)
	{
		BtnNextThrall->SetVisibility(ESlateVisibility::Visible);
	}

	if (BtnProcessDailyUpkeep)
	{
		BtnProcessDailyUpkeep->SetVisibility(ESlateVisibility::Visible);
	}

	if (BtnAddThrall)
	{
		BtnAddThrall->SetVisibility(ESlateVisibility::Visible);
	}

	if (BtnClose)
	{
		BtnClose->SetButtonText(LOCTEXT("EconomyCloseBtn", "Sluiten"));
	}
}

bool UVampireEconomySummaryMenu::HandleBuyerSellAction()
{
	if (!BoundBuyer.IsValid() || !BoundBuyer->BuyerData || !BoundEconomy || !BoundInventory)
	{
		return false;
	}

	TArray<UBloodProductItem*> Representatives;
	TArray<int32> TotalUnits;
	GetBuyerCandidateGroups(Representatives, TotalUnits);
	if (Representatives.IsEmpty())
	{
		BoundEconomy->SetInteractionFeedback(LOCTEXT("NoBuyerGroupsAvailable", "Geen verkoopgroepen beschikbaar."), false);
		return false;
	}

	const int32 SafeIndex = FMath::Clamp(SelectedBloodItemIndex, 0, Representatives.Num() - 1);
	UBloodProductItem* SelectedRepresentative = Representatives[SafeIndex];
	FText Reason;
	if (!BoundEconomy->CanSellBloodItemToBuyer(SelectedRepresentative, BoundBuyer->BuyerData, Reason))
	{
		BoundEconomy->SetInteractionFeedback(Reason, false);
		return false;
	}

	TArray<UBloodProductItem*> ItemsToSell;
	for (UOwnSystemItem* Item : BoundInventory->GetItems())
	{
		if (UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item))
		{
			if (DoBloodItemsMatchForUiBatch(SelectedRepresentative, Candidate))
			{
				ItemsToSell.Add(Candidate);
			}
		}
	}

	int32 TotalPayout = 0;
	int32 SoldItemCount = 0;
	for (UBloodProductItem* ItemToSell : ItemsToSell)
	{
		int32 Payout = 0;
		FText SellReason;
		if (BoundEconomy->SellBloodItemToBuyer(BoundInventory, ItemToSell, BoundBuyer->BuyerData, Payout, SellReason))
		{
			TotalPayout += Payout;
			SoldItemCount++;
		}
		else
		{
			BoundEconomy->SetInteractionFeedback(SellReason, false);
			return false;
		}
	}

	BoundEconomy->SetInteractionFeedback(
		FText::Format(
			LOCTEXT("BuyerSellSuccessFmt", "Verkoop geslaagd: {0} item(s) verkocht voor totaal {1} goud."),
			FText::AsNumber(SoldItemCount),
			FText::AsNumber(TotalPayout)),
		true);
	return SoldItemCount > 0;
}

bool UVampireEconomySummaryMenu::HandleDebugPlaceFirstStation()
{
	if (!BoundEconomy)
	{
		RefreshSummary();
		return false;
	}

	UPlaceableStationItem* StationItem = GetFirstPlaceableStationItem();
	if (!StationItem)
	{
		BoundEconomy->SetInteractionFeedback(LOCTEXT("DebugPlaceNoStationItem", "Debug place: geen placeable station-item in inventory gevonden."), false);
		RefreshSummary();
		return true;
	}

	FText Reason;
	const bool bSuccess = BoundEconomy->RequestAutoPlaceStationItem(StationItem, Reason);
	BoundEconomy->SetInteractionFeedback(Reason, bSuccess);
	RefreshSummary();
	return true;
}

UPlaceableStationItem* UVampireEconomySummaryMenu::GetFirstPlaceableStationItem() const
{
	const UOwnSystemInventoryComponent* Inventory = ResolveInventory(this);
	if (!Inventory)
	{
		return nullptr;
	}

	for (UOwnSystemItem* Item : Inventory->GetItems())
	{
		if (UPlaceableStationItem* StationItem = Cast<UPlaceableStationItem>(Item))
		{
			return StationItem;
		}
	}

	return nullptr;
}

AWorkspaceRoom* UVampireEconomySummaryMenu::FindFirstWorkspaceRoom() const
{
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AWorkspaceRoom> It(World); It; ++It)
		{
			if (AWorkspaceRoom* WorkspaceRoom = *It)
			{
				return WorkspaceRoom;
			}
		}
	}

	return nullptr;
}

int32 UVampireEconomySummaryMenu::GetThrallCount() const
{
	const UVampireEconomyComponent* Economy = ResolveEconomy(this);
	return Economy ? Economy->GetThrallUnits().Num() : 0;
}

const FThrallUpkeepUnit* UVampireEconomySummaryMenu::GetSelectedThrall() const
{
	const UVampireEconomyComponent* Economy = ResolveEconomy(this);
	if (!Economy)
	{
		return nullptr;
	}

	const TArray<FThrallUpkeepUnit>& Thralls = Economy->GetThrallUnits();
	if (Thralls.IsEmpty())
	{
		return nullptr;
	}

	const int32 SafeIndex = FMath::Clamp(SelectedThrallIndex, 0, Thralls.Num() - 1);
	return &Thralls[SafeIndex];
}

void UVampireEconomySummaryMenu::ClampSelectedThrallIndex()
{
	const int32 ThrallCount = GetThrallCount();
	if (ThrallCount <= 0)
	{
		SelectedThrallIndex = 0;
		return;
	}

	SelectedThrallIndex = FMath::Clamp(SelectedThrallIndex, 0, ThrallCount - 1);
}

void UVampireEconomySummaryMenu::HandleInventoryUpdated()
{
	RefreshSummary();
}

void UVampireEconomySummaryMenu::HandleCurrencyChanged(const int32 OldCurrency, const int32 NewCurrency)
{
	RefreshSummary();
}

void UVampireEconomySummaryMenu::HandleEconomyUpdated()
{
	RefreshSummary();
}

void UVampireEconomySummaryMenu::HandleProcessDailyUpkeepClicked()
{
	if (BoundEconomy)
	{
		FText Summary;
		BoundEconomy->RequestProcessDailyThrallUpkeep(Summary);
	}
}

void UVampireEconomySummaryMenu::HandleAddThrallClicked()
{
	if (!BoundEconomy)
	{
		return;
	}

	FThrallUpkeepUnit ThrallUnit;
	ThrallUnit.ThrallId = FName(*FString::Printf(TEXT("Thrall_%d"), BoundEconomy->GetThrallUnits().Num() + 1));
	ThrallUnit.Tier = EThrallTier::ThrallI;
	ThrallUnit.RequiredQuality = EBloodQuality::Gewoon;
	ThrallUnit.RequiredBloodUnitsPerDay = 1;
	ThrallUnit.Loyalty = 1.0f;
	ThrallUnit.bActive = true;

	BoundEconomy->AddThrallUnit(ThrallUnit);
}

void UVampireEconomySummaryMenu::HandlePrevBloodItemClicked()
{
	if (IsInBuyerContext())
	{
		TArray<UBloodProductItem*> Representatives;
		TArray<int32> TotalUnits;
		GetBuyerCandidateGroups(Representatives, TotalUnits);
		if (Representatives.IsEmpty())
		{
			return;
		}

		SelectedBloodItemIndex = (SelectedBloodItemIndex - 1 + Representatives.Num()) % Representatives.Num();
		RefreshSummary();
		return;
	}

	const TArray<UBloodProductItem*> BloodItems = GetBloodItems();
	if (BloodItems.IsEmpty())
	{
		return;
	}

	SelectedBloodItemIndex = (SelectedBloodItemIndex - 1 + BloodItems.Num()) % BloodItems.Num();
	RefreshSummary();
}

void UVampireEconomySummaryMenu::HandleNextBloodItemClicked()
{
	if (IsInBuyerContext())
	{
		TArray<UBloodProductItem*> Representatives;
		TArray<int32> TotalUnits;
		GetBuyerCandidateGroups(Representatives, TotalUnits);
		if (Representatives.IsEmpty())
		{
			return;
		}

		SelectedBloodItemIndex = (SelectedBloodItemIndex + 1) % Representatives.Num();
		RefreshSummary();
		return;
	}

	const TArray<UBloodProductItem*> BloodItems = GetBloodItems();
	if (BloodItems.IsEmpty())
	{
		return;
	}

	SelectedBloodItemIndex = (SelectedBloodItemIndex + 1) % BloodItems.Num();
	RefreshSummary();
}

void UVampireEconomySummaryMenu::HandleProcessSelectedClicked()
{
	if (IsInBuyerContext())
	{
		HandleBuyerSellAction();
		RefreshSummary();
		return;
	}

	HandleDebugPlaceFirstStation();
	RefreshSummary();
}

void UVampireEconomySummaryMenu::HandlePrevThrallClicked()
{
	const int32 ThrallCount = GetThrallCount();
	if (ThrallCount <= 0)
	{
		return;
	}

	SelectedThrallIndex = (SelectedThrallIndex - 1 + ThrallCount) % ThrallCount;
	RefreshSummary();
}

void UVampireEconomySummaryMenu::HandleNextThrallClicked()
{
	const int32 ThrallCount = GetThrallCount();
	if (ThrallCount <= 0)
	{
		return;
	}

	SelectedThrallIndex = (SelectedThrallIndex + 1) % ThrallCount;
	RefreshSummary();
}

void UVampireEconomySummaryMenu::HandleCloseClicked()
{
	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (BoundBuyerInteractor.IsValid())
		{
			PlayerController->SetViewTargetWithBlend(BoundBuyerInteractor.Get(), 0.25f);
		}
		PlayerController->SetShowMouseCursor(false);
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
	}

	DeactivateWidget();
	RemoveFromParent();
}

#undef LOCTEXT_NAMESPACE
