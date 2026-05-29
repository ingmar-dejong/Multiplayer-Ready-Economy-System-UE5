#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StationPlacementPreviewActor.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class VAMPIREEMPIRE_API AStationPlacementPreviewActor : public AActor
{
	GENERATED_BODY()

public:
	AStationPlacementPreviewActor();

	UFUNCTION(BlueprintCallable, Category = "Placement Preview")
	void ConfigurePreview(UStaticMesh* InPreviewMesh, bool bInIsValidPlacement);

	UFUNCTION(BlueprintCallable, Category = "Placement Preview")
	void SetPlacementValidity(bool bInIsValidPlacement);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement Preview")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Placement Preview")
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;

private:
	void ApplyValidityVisuals();

	bool bIsValidPlacement = false;
};
