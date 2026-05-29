#pragma once

#include "CoreMinimal.h"
#include "Items/OwnSystemItem.h"
#include "Vampire/BloodTypes.h"
#include "BloodProductItem.generated.h"

UCLASS(Blueprintable)
class VAMPIREEMPIRE_API UBloodProductItem : public UOwnSystemItem
{
	GENERATED_BODY()

public:
	UBloodProductItem();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void AddedToInventory(class UOwnSystemInventoryComponent* Inventory, const bool bFromLoad) override;
	virtual void PostInventoryLoaded() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_BloodPresentationData, Category = "Blood")
	EBloodSourceType SourceType = EBloodSourceType::Rat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_BloodPresentationData, Category = "Blood")
	EBloodQuality BaseQuality = EBloodQuality::Gewoon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_BloodPresentationData, Category = "Blood")
	EBloodProcessingType ProcessingType = EBloodProcessingType::Vers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_BloodPresentationData, Category = "Blood")
	bool bHasPackagedSourceProcessing = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_BloodPresentationData, Category = "Blood", meta = (EditCondition = "bHasPackagedSourceProcessing"))
	EBloodProcessingType PackagedSourceProcessingType = EBloodProcessingType::Vers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_BloodPresentationData, Category = "Blood", meta = (ClampMin = 1))
	int32 BloodQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_BloodPresentationData, Category = "Blood", meta = (ClampMin = 0))
	int32 CreatedDay = 0;

	UFUNCTION(BlueprintPure, Category = "Blood")
	FText GetBloodDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Blood")
	FText GetBloodSummaryText() const;

	static FText GetSourceDisplayName(EBloodSourceType InSourceType);
	static FText GetQualityDisplayName(EBloodQuality InQuality);
	static FText GetProcessingDisplayName(EBloodProcessingType InProcessingType);

	virtual FText GetRawDescription_Implementation() override;
	virtual FString GetStringVariable_Implementation(const FString& VariableName) override;

	void RefreshPresentation();

private:
	UFUNCTION()
	void OnRep_BloodPresentationData();
};
