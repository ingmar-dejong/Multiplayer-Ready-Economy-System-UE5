#pragma once

#include "CoreMinimal.h"
#include "Items/OwnSystemItem.h"
#include "PlaceableStationItem.generated.h"

class UPlaceableStationDataAsset;

UCLASS(Blueprintable)
class VAMPIREEMPIRE_API UPlaceableStationItem : public UOwnSystemItem
{
	GENERATED_BODY()

public:
	UPlaceableStationItem();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual FText GetRawDescription_Implementation() override;
	virtual void AddedToInventory(class UOwnSystemInventoryComponent* Inventory, const bool bFromLoad) override;
	virtual bool CanUse_Implementation() const override;
	virtual void Use(UOwnSystemItem* OtherItem = nullptr) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_PlaceablePresentationData, Category = "Placeable Station")
	TObjectPtr<UPlaceableStationDataAsset> StationDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, ReplicatedUsing = OnRep_PlaceablePresentationData, Category = "Placeable Station")
	FGuid StationInstanceId;

	UFUNCTION(BlueprintPure, Category = "Placeable Station")
	FText GetStationDisplayName() const;

	UFUNCTION(BlueprintPure, Category = "Placeable Station")
	FText GetStationSummaryText() const;

	UFUNCTION(BlueprintCallable, Category = "Placeable Station")
	void RefreshPresentation();

	UFUNCTION(BlueprintCallable, Category = "Placeable Station")
	void EnsureInstanceId();

private:
	UFUNCTION()
	void OnRep_PlaceablePresentationData();
};
