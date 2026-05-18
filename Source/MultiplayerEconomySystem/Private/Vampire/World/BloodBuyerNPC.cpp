#include "Vampire/World/BloodBuyerNPC.h"

#include "Components/SceneComponent.h"
#include "Vampire/Interaction/BloodSellInteractableComponent.h"

ABloodBuyerNPC::ABloodBuyerNPC()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SellInteractable = CreateDefaultSubobject<UBloodSellInteractableComponent>(TEXT("SellInteractable"));
}
