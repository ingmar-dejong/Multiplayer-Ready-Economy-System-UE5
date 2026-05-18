#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BloodProcessingAttachmentDataAsset.generated.h"

class UStaticMesh;

UCLASS(BlueprintType)
class VAMPIREEMPIRE_API UBloodProcessingAttachmentDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	FName AttachmentId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	FGameplayTag AttachmentTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	TObjectPtr<UStaticMesh> AttachmentMesh;
};
