#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Vampire/BloodTypes.h"
#include "BloodProcessingRecipeDataAsset.generated.h"

UCLASS(BlueprintType)
class VAMPIREEMPIRE_API UBloodProcessingRecipeDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	FName RecipeId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	FText RecipeName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	EBloodProcessingType RequiredInputProcessing = EBloodProcessingType::Vers;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	bool bAcceptAnyInputProcessing = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	EBloodQuality MinimumQuality = EBloodQuality::Gewoon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	EBloodProcessingType OutputProcessing = EBloodProcessingType::GerijptOpHout;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|UI")
	FText ProcessFactText;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing", meta = (ClampMin = 0))
	int32 ProcessingDurationDays = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing", meta = (ClampMin = 0))
	int32 AdditionalDurationPerSourceScore = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing", meta = (ClampMin = 0))
	int32 GoldCost = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Requirements")
	FGameplayTagContainer RequiredStationTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Requirements")
	FGameplayTagContainer RequiredUnlockTags;
};
