#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BloodBuyerNPC.generated.h"

class UBloodBuyerDataAsset;
class UBloodSellInteractableComponent;
class USceneComponent;
class UVampireEconomySummaryMenu;

UCLASS()
class VAMPIREEMPIRE_API ABloodBuyerNPC : public AActor
{
	GENERATED_BODY()

public:
	ABloodBuyerNPC();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBloodSellInteractableComponent> SellInteractable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buyer")
	TObjectPtr<UBloodBuyerDataAsset> BuyerData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Buyer|UI")
	TSubclassOf<UVampireEconomySummaryMenu> BuyerMenuClass;
};
