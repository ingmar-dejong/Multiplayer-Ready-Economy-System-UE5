#pragma once

#include "CoreMinimal.h"
#include "Interaction/InteractableComponent.h"
#include "BloodProcessingInteractableComponent.generated.h"

UCLASS(ClassGroup=(Vampire), meta=(BlueprintSpawnableComponent))
class VAMPIREEMPIRE_API UBloodProcessingInteractableComponent : public UOwnSystemInteractableComponent
{
	GENERATED_BODY()

public:
	virtual void OnInteract_Implementation(APawn* Interactor, UOwnSystemInteractionComponent* InteractionComp) override;
	static bool OpenStationMenuForInteractor(class ABloodProcessingStation* ProcessingStation, APawn* Interactor);

private:
	UPROPERTY(Transient)
	TWeakObjectPtr<class UVampireBarrelMenu> ActiveBarrelMenu;
};
