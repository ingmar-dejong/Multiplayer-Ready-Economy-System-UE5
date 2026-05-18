#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BloodHarvestPoint.generated.h"

class UBloodHarvestInteractableComponent;
class UBloodHarvestSourceDataAsset;
class USceneComponent;

UCLASS()
class VAMPIREEMPIRE_API ABloodHarvestPoint : public AActor
{
	GENERATED_BODY()

public:
	ABloodHarvestPoint();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBloodHarvestInteractableComponent> HarvestInteractable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blood")
	TObjectPtr<UBloodHarvestSourceDataAsset> HarvestSourceData;
};
