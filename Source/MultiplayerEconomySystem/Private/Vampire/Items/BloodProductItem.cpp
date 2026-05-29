#include "Vampire/Items/BloodProductItem.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "BloodProductItem"

UBloodProductItem::UBloodProductItem()
{
	bStackable = false;
	MaxStackSize = 1;
	Weight = 0.1f;
	BaseValue = 10;

	// OwnSystem adds a generic stack quantity stat by default. Blood items use BloodQuantity
	// instead, which is already shown in the generated description.
	Stats.RemoveAll([](const FOwnSystemItemStat& Stat)
	{
		return Stat.StringVariable == TEXT("Quantity");
	});

	RefreshPresentation();
}

void UBloodProductItem::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UBloodProductItem, SourceType);
	DOREPLIFETIME(UBloodProductItem, BaseQuality);
	DOREPLIFETIME(UBloodProductItem, ProcessingType);
	DOREPLIFETIME(UBloodProductItem, bHasPackagedSourceProcessing);
	DOREPLIFETIME(UBloodProductItem, PackagedSourceProcessingType);
	DOREPLIFETIME(UBloodProductItem, BloodQuantity);
	DOREPLIFETIME(UBloodProductItem, CreatedDay);
}

void UBloodProductItem::AddedToInventory(UOwnSystemInventoryComponent* Inventory, const bool bFromLoad)
{
	Super::AddedToInventory(Inventory, bFromLoad);

	// New runtime items should immediately rebuild their derived presentation.
	RefreshPresentation();
}

void UBloodProductItem::PostInventoryLoaded()
{
	Super::PostInventoryLoaded();

	// OwnSystem restores item byte data after AddedToInventory, so the loaded BloodQuantity
	// only exists here. Rebuild the derived presentation after deserialization completes.
	RefreshPresentation();
}

FText UBloodProductItem::GetBloodDisplayName() const
{
	if (ProcessingType == EBloodProcessingType::Handelsklaar && bHasPackagedSourceProcessing)
	{
		return FText::Format(
			LOCTEXT("PackagedBloodDisplayNameFmt", "Ingepakt {0} {1} bloed {2}x"),
			GetProcessingDisplayName(PackagedSourceProcessingType),
			GetSourceDisplayName(SourceType),
			FText::AsNumber(BloodQuantity));
	}

	return FText::Format(
		LOCTEXT("BloodDisplayNameFmt", "{0} {1} bloed"),
		GetProcessingDisplayName(ProcessingType),
		GetSourceDisplayName(SourceType));
}

FText UBloodProductItem::GetBloodSummaryText() const
{
	if (ProcessingType == EBloodProcessingType::Handelsklaar && bHasPackagedSourceProcessing)
	{
		return FText::Format(
			LOCTEXT("PackagedBloodSummaryFmt", "Verpakt product | Batch: {0} | Bron: {1} | Kwaliteit: {2} | Hoeveelheid: {3}"),
			GetProcessingDisplayName(PackagedSourceProcessingType),
			GetSourceDisplayName(SourceType),
			GetQualityDisplayName(BaseQuality),
			FText::AsNumber(BloodQuantity));
	}

	return FText::Format(
		LOCTEXT("BloodSummaryFmt", "Bron: {0} | Kwaliteit: {1} | Verwerking: {2} | Hoeveelheid: {3}"),
		GetSourceDisplayName(SourceType),
		GetQualityDisplayName(BaseQuality),
		GetProcessingDisplayName(ProcessingType),
		FText::AsNumber(BloodQuantity));
}

FText UBloodProductItem::GetRawDescription_Implementation()
{
	return GetBloodSummaryText();
}

FString UBloodProductItem::GetStringVariable_Implementation(const FString& VariableName)
{
	if (VariableName == TEXT("Quantity"))
	{
		return FString::FromInt(BloodQuantity);
	}

	return Super::GetStringVariable_Implementation(VariableName);
}

void UBloodProductItem::RefreshPresentation()
{
	DisplayName = GetBloodDisplayName();
	Description = GetBloodSummaryText();
	MarkDirtyForReplication();
	OnItemModified.Broadcast();
}

void UBloodProductItem::OnRep_BloodPresentationData()
{
	RefreshPresentation();
}

FText UBloodProductItem::GetSourceDisplayName(const EBloodSourceType InSourceType)
{
	switch (InSourceType)
	{
	case EBloodSourceType::Rat:
		return LOCTEXT("SourceRat", "Ratten");
	case EBloodSourceType::Varken:
		return LOCTEXT("SourcePig", "Varkens");
	case EBloodSourceType::Geit:
		return LOCTEXT("SourceGoat", "Geiten");
	case EBloodSourceType::Hert:
		return LOCTEXT("SourceDeer", "Herten");
	case EBloodSourceType::Bedelaar:
		return LOCTEXT("SourceBeggar", "Bedelaars");
	case EBloodSourceType::Boer:
		return LOCTEXT("SourceFarmer", "Boeren");
	case EBloodSourceType::Handelaar:
		return LOCTEXT("SourceTrader", "Handelaars");
	case EBloodSourceType::Edelman:
		return LOCTEXT("SourceNoble", "Edelmannen");
	case EBloodSourceType::Priester:
		return LOCTEXT("SourcePriest", "Priesters");
	default:
		return LOCTEXT("SourceUnknown", "Onbekend");
	}
}

FText UBloodProductItem::GetQualityDisplayName(const EBloodQuality InQuality)
{
	switch (InQuality)
	{
	case EBloodQuality::Gewoon:
		return LOCTEXT("QualityCommon", "Gewoon");
	case EBloodQuality::Goed:
		return LOCTEXT("QualityGood", "Goed");
	case EBloodQuality::Premium:
		return LOCTEXT("QualityPremium", "Premium");
	default:
		return LOCTEXT("QualityUnknown", "Onbekend");
	}
}

FText UBloodProductItem::GetProcessingDisplayName(const EBloodProcessingType InProcessingType)
{
	switch (InProcessingType)
	{
	case EBloodProcessingType::Vers:
		return LOCTEXT("ProcessingFresh", "Vers");
	case EBloodProcessingType::GerijptOpHout:
		return LOCTEXT("ProcessingAged", "Gerijpt op hout");
	case EBloodProcessingType::Handelsklaar:
		return LOCTEXT("ProcessingMarketReady", "Handelsklaar");
	case EBloodProcessingType::Gekruid:
		return LOCTEXT("ProcessingSpiced", "Gekruid");
	case EBloodProcessingType::Gezuiverd:
		return LOCTEXT("ProcessingPurified", "Gezuiverd");
	case EBloodProcessingType::Verdund:
		return LOCTEXT("ProcessingDiluted", "Verdund");
	case EBloodProcessingType::RitueelBehandeld:
		return LOCTEXT("ProcessingRitual", "Ritueel behandeld");
	case EBloodProcessingType::GekoeldBewaard:
		return LOCTEXT("ProcessingChilled", "Gekoeld bewaard");
	default:
		return LOCTEXT("ProcessingUnknown", "Onbekend");
	}
}

#undef LOCTEXT_NAMESPACE
