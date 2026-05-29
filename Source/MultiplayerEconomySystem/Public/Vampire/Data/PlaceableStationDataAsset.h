#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PlaceableStationDataAsset.generated.h"

class AActor;
class UStaticMesh;

UENUM(BlueprintType)
enum class EPlaceableStationCategory : uint8
{
	Workstation,
	Storage,
	Living
};

UCLASS(BlueprintType)
class VAMPIREEMPIRE_API UPlaceableStationDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable Station")
	FName StationId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable Station")
	FText StationDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable Station")
	EPlaceableStationCategory StationCategory = EPlaceableStationCategory::Workstation;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable Station")
	TSubclassOf<AActor> PlacedActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable Station")
	TArray<FIntPoint> FootprintCells;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable Station")
	bool bRequiresFullFrontClearance = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable Station")
	bool bRequiresAnyAdjacentClearCell = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable Station")
	TSoftObjectPtr<UStaticMesh> PreviewMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Placeable Station")
	FText PlacementSummary;
};
