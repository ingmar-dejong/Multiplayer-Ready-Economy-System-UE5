#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Vampire/BloodTypes.h"
#include "BloodBuyerDataAsset.generated.h"

UCLASS(BlueprintType)
class VAMPIREEMPIRE_API UBloodBuyerDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buyer")
	FText BuyerName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buyer")
	TArray<EBloodSourceType> AcceptedSources;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buyer")
	EBloodQuality MinimumQuality = EBloodQuality::Gewoon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buyer")
	TArray<EBloodProcessingType> AcceptedProcessingTypes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buyer", meta = (ClampMin = 0.0))
	float BasePayoutPerUnit = 1.0f;
};
