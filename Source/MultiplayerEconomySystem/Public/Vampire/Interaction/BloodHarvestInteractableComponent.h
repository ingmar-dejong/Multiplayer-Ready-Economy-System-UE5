#pragma once

#include "CoreMinimal.h"
#include "Interaction/InteractableComponent.h"
#include "BloodHarvestInteractableComponent.generated.h"

UCLASS(ClassGroup=(Vampire), meta=(BlueprintSpawnableComponent))
class VAMPIREEMPIRE_API UBloodHarvestInteractableComponent : public UOwnSystemInteractableComponent
{
	GENERATED_BODY()

public:
	virtual void OnInteract_Implementation(APawn* Interactor, UOwnSystemInteractionComponent* InteractionComp) override;
};
