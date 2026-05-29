#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Vampire/Data/PlaceableStationDataAsset.h"
#include "WorkspaceRoom.generated.h"

class UPlaceableStationDataAsset;
class USceneComponent;
class ABloodProcessingStation;

USTRUCT(BlueprintType)
struct FWorkspacePlacedStationRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Workspace")
	FGuid StationInstanceId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Workspace")
	TObjectPtr<UPlaceableStationDataAsset> StationDefinition;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Workspace")
	FIntPoint AnchorCell = FIntPoint::ZeroValue;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Workspace", meta = (ClampMin = 0, ClampMax = 3))
	int32 RotationQuarterTurns = 0;
};

UCLASS()
class VAMPIREEMPIRE_API AWorkspaceRoom : public AActor
{
	GENERATED_BODY()

public:
	AWorkspaceRoom();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual bool ShouldTickIfViewportsOnly() const override;

	UFUNCTION(BlueprintPure, Category = "Workspace")
	bool IsCellWithinBounds(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Workspace")
	FVector GetCellWorldLocation(FIntPoint Cell) const;

	UFUNCTION(BlueprintPure, Category = "Workspace")
	FRotator GetPlacementWorldRotation(int32 RotationQuarterTurns) const;

	UFUNCTION(BlueprintPure, Category = "Workspace")
	FVector GetGridWorldSize() const;

	UFUNCTION(BlueprintPure, Category = "Workspace")
	FVector GetGridWorldCenter() const;

	bool ValidatePlacement(const UPlaceableStationDataAsset* StationDefinition, FIntPoint AnchorCell, int32 RotationQuarterTurns, FText& OutReason, const FGuid* IgnoredStationInstanceId = nullptr) const;

	UFUNCTION(BlueprintCallable, Category = "Workspace")
	bool AddPlacementRecord(const FWorkspacePlacedStationRecord& Record, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Workspace")
	bool RemovePlacementRecordByInstanceId(const FGuid& StationInstanceId);

	UFUNCTION(BlueprintCallable, Category = "Workspace")
	AActor* SpawnPlacedActor(const FWorkspacePlacedStationRecord& Record, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Workspace")
	bool DestroyPlacedActorByInstanceId(const FGuid& StationInstanceId);

	const FWorkspacePlacedStationRecord* FindPlacementRecordByInstanceId(const FGuid& StationInstanceId) const;

	bool RegisterPlacedStationActor(ABloodProcessingStation* ProcessingStation, const FWorkspacePlacedStationRecord& Record, FText& OutReason);

	UFUNCTION(BlueprintPure, Category = "Workspace")
	const TArray<FWorkspacePlacedStationRecord>& GetPlacedStations() const { return PlacedStations; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Workspace")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Workspace")
	FName RoomId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Workspace", meta = (ClampMin = 1))
	int32 GridWidth = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Workspace", meta = (ClampMin = 1))
	int32 GridHeight = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Workspace", meta = (ClampMin = 1.0))
	float CellSize = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Workspace")
	TArray<FIntPoint> DoorCells;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workspace|Debug")
	bool bShowWorkspaceDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workspace|Debug")
	bool bShowWorkspaceDebugInGame = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workspace|Debug", meta = (ClampMin = 0.0))
	float DebugDrawHeight = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workspace|Debug", meta = (ClampMin = 0.0))
	float DebugLabelHeight = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workspace|Debug")
	FColor GridLineColor = FColor(90, 170, 255);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workspace|Debug")
	FColor BoundsColor = FColor(80, 255, 120);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workspace|Debug")
	FColor DoorCellColor = FColor(255, 170, 40);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Workspace|Debug")
	FColor OccupiedCellColor = FColor(255, 80, 80);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Workspace")
	TArray<FWorkspacePlacedStationRecord> PlacedStations;

private:
	static FIntPoint RotateLocalCell(FIntPoint LocalCell, int32 RotationQuarterTurns);
	static TArray<FIntPoint> BuildOccupiedCells(const UPlaceableStationDataAsset* StationDefinition, FIntPoint AnchorCell, int32 RotationQuarterTurns);
	static TArray<FIntPoint> BuildFrontAccessCells(const UPlaceableStationDataAsset* StationDefinition, FIntPoint AnchorCell, int32 RotationQuarterTurns);
	static TArray<FIntPoint> BuildAdjacentAccessCells(const UPlaceableStationDataAsset* StationDefinition, FIntPoint AnchorCell, int32 RotationQuarterTurns);

	void DrawWorkspaceDebug() const;
	bool ShouldDrawWorkspaceDebug() const;
	void RegisterLevelAuthoredStations();
	bool IsDoorCell(FIntPoint Cell) const;
	bool IsCellOccupied(FIntPoint Cell, const FGuid* IgnoredStationInstanceId = nullptr) const;
};
