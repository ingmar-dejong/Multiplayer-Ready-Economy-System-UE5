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
	virtual FText GetInteractableActionText_Implementation(class APawn* Interactor, class UOwnSystemInteractionComponent* InteractionComp) const override;
	void SetSuppressedByMovePlacement(bool bSuppress);

protected:
	virtual void BeginFocus(class APawn* Interactor, class UOwnSystemInteractionComponent* InteractionComp) override;
	virtual void EndFocus(class APawn* Interactor, class UOwnSystemInteractionComponent* InteractionComp) override;

private:
	void ApplyMoveInteractionHotkey(class APawn* Interactor);
	void RemoveMoveInteractionHotkey();
	void HandleFocusedMovePressed();

	UPROPERTY(Transient)
	TWeakObjectPtr<class UProcessingStationMenuBase> ActiveProcessingMenu;

	UPROPERTY(Transient)
	TWeakObjectPtr<class APawn> FocusedInteractor;

	UPROPERTY(Transient)
	TWeakObjectPtr<class APlayerController> FocusedPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<class UInputComponent> FocusMoveHotkeyInputComponent;
};
