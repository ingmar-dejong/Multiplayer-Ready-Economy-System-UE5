#include "Vampire/World/StationPlacementPreviewActor.h"

#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

AStationPlacementPreviewActor::AStationPlacementPreviewActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SetReplicates(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PreviewMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMeshComponent->SetupAttachment(SceneRoot);
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMeshComponent->SetGenerateOverlapEvents(false);
	PreviewMeshComponent->SetCanEverAffectNavigation(false);
	PreviewMeshComponent->SetCastShadow(false);
	PreviewMeshComponent->SetReceivesDecals(false);
	PreviewMeshComponent->SetRenderCustomDepth(true);
	PreviewMeshComponent->SetCustomDepthStencilValue(1);
}

void AStationPlacementPreviewActor::ConfigurePreview(UStaticMesh* InPreviewMesh, const bool bInIsValidPlacement)
{
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetStaticMesh(InPreviewMesh);
		PreviewMeshComponent->SetVisibility(InPreviewMesh != nullptr);
	}

	bIsValidPlacement = bInIsValidPlacement;
	ApplyValidityVisuals();
}

void AStationPlacementPreviewActor::SetPlacementValidity(const bool bInIsValidPlacement)
{
	if (bIsValidPlacement == bInIsValidPlacement)
	{
		return;
	}

	bIsValidPlacement = bInIsValidPlacement;
	ApplyValidityVisuals();
}

void AStationPlacementPreviewActor::ApplyValidityVisuals()
{
	if (!PreviewMeshComponent)
	{
		return;
	}

	PreviewMeshComponent->SetRenderCustomDepth(true);
	PreviewMeshComponent->SetCustomDepthStencilValue(bIsValidPlacement ? 1 : 2);
	PreviewMeshComponent->SetScalarParameterValueOnMaterials(TEXT("Opacity"), bIsValidPlacement ? 0.45f : 0.25f);
	PreviewMeshComponent->SetVectorParameterValueOnMaterials(
		TEXT("Tint"),
		bIsValidPlacement ? FVector(0.15f, 0.85f, 0.25f) : FVector(0.95f, 0.2f, 0.2f));
}
