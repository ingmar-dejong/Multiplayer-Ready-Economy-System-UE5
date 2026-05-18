#include "Vampire/World/BloodHarvestPoint.h"

#include "Components/SceneComponent.h"
#include "Vampire/Interaction/BloodHarvestInteractableComponent.h"

ABloodHarvestPoint::ABloodHarvestPoint()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	HarvestInteractable = CreateDefaultSubobject<UBloodHarvestInteractableComponent>(TEXT("HarvestInteractable"));
}
