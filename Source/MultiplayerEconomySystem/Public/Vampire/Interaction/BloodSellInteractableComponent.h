#pragma once

#include "CoreMinimal.h"
#include "Interaction/InteractableComponent.h"
#include "BloodSellInteractableComponent.generated.h"

UCLASS(ClassGroup=(Vampire), meta=(BlueprintSpawnableComponent))
class VAMPIREEMPIRE_API UBloodSellInteractableComponent : public UOwnSystemInteractableComponent
{
	GENERATED_BODY()

public:
	virtual void OnInteract_Implementation(APawn* Interactor, UOwnSystemInteractionComponent* InteractionComp) override;

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<class UVampireEconomySummaryMenu> ActiveBuyerMenu;
};
