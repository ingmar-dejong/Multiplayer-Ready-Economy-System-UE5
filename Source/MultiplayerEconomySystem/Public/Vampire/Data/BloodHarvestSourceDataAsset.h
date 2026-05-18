#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Vampire/BloodTypes.h"
#include "BloodHarvestSourceDataAsset.generated.h"

class UBloodProductItem;

UCLASS(BlueprintType)
class VAMPIREEMPIRE_API UBloodHarvestSourceDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood")
	TSubclassOf<UBloodProductItem> BloodItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood")
	EBloodSourceType SourceType = EBloodSourceType::Rat;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood", meta = (ClampMin = 0, ClampMax = 3))
	int32 ConditionScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood", meta = (ClampMin = 0, ClampMax = 3))
	int32 SetupScore = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood", meta = (ClampMin = 1))
	int32 MinBloodQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood", meta = (ClampMin = 1))
	int32 MaxBloodQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood")
	EBloodProcessingType InitialProcessingType = EBloodProcessingType::Vers;
};
