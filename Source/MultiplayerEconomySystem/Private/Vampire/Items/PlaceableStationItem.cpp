#include "Vampire/Items/PlaceableStationItem.h"

#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "Vampire/Economy/VampireEconomyComponent.h"
#include "Vampire/Data/PlaceableStationDataAsset.h"

#define LOCTEXT_NAMESPACE "PlaceableStationItem"

UPlaceableStationItem::UPlaceableStationItem()
{
	bStackable = false;
	MaxStackSize = 1;
	Weight = 10.0f;
	BaseValue = 0;
	bAddDefaultUseOption = true;
	UseActionText = LOCTEXT("PlaceableStationUseAction", "Plaatsen");
	RefreshPresentation();
}

void UPlaceableStationItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UPlaceableStationItem, StationDefinition);
	DOREPLIFETIME(UPlaceableStationItem, StationInstanceId);
}

FText UPlaceableStationItem::GetRawDescription_Implementation()
{
	return GetStationSummaryText();
}

void UPlaceableStationItem::AddedToInventory(UOwnSystemInventoryComponent* Inventory, const bool bFromLoad)
{
	Super::AddedToInventory(Inventory, bFromLoad);
	if (bFromLoad)
	{
		EnsureInstanceId();
	}
	else
	{
		StationInstanceId = FGuid::NewGuid();
		MarkDirtyForReplication();
	}
	RefreshPresentation();
}

bool UPlaceableStationItem::CanUse_Implementation() const
{
	if (!StationDefinition)
	{
		return false;
	}

	const APawn* OwningPawn = GetOwningPawn();
	return UVampireEconomyComponent::ResolveEconomyFromActor(OwningPawn) != nullptr;
}

void UPlaceableStationItem::Use(UOwnSystemItem* OtherItem)
{
	Super::Use(OtherItem);

	APawn* OwningPawn = GetOwningPawn();
	UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(OwningPawn);
	if (!Economy)
	{
		return;
	}

	APlayerController* PlayerController = OwningPawn ? Cast<APlayerController>(OwningPawn->GetController()) : nullptr;
	if (PlayerController && PlayerController->IsLocalController())
	{
		FText Result;
		const bool bSuccess = Economy->StartStationPlacementMode(this, Result);
		Economy->SetInteractionFeedback(Result, bSuccess);
	}
}

FText UPlaceableStationItem::GetStationDisplayName() const
{
	if (StationDefinition && !StationDefinition->StationDisplayName.IsEmpty())
	{
		return StationDefinition->StationDisplayName;
	}

	return LOCTEXT("UnnamedPlaceableStation", "Onbekend station");
}

FText UPlaceableStationItem::GetStationSummaryText() const
{
	if (StationDefinition)
	{
		if (!StationDefinition->PlacementSummary.IsEmpty())
		{
			return StationDefinition->PlacementSummary;
		}

		return FText::Format(
			LOCTEXT("PlaceableStationSummaryFmt", "Plaatsbaar station | Id: {0}"),
			FText::FromName(StationDefinition->StationId));
	}

	return LOCTEXT("PlaceableStationSummaryMissingDefinition", "Plaatsbaar station zonder geldige definitie.");
}

void UPlaceableStationItem::RefreshPresentation()
{
	DisplayName = GetStationDisplayName();
	Description = GetStationSummaryText();
	MarkDirtyForReplication();
	OnItemModified.Broadcast();
}

void UPlaceableStationItem::EnsureInstanceId()
{
	if (!StationInstanceId.IsValid())
	{
		StationInstanceId = FGuid::NewGuid();
		MarkDirtyForReplication();
	}
}

void UPlaceableStationItem::OnRep_PlaceablePresentationData()
{
	RefreshPresentation();
}

#undef LOCTEXT_NAMESPACE
