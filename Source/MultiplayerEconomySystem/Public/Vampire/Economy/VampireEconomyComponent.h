#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "OwnSystemSavableComponent.h"
#include "Vampire/BloodTypes.h"
#include "VampireEconomyComponent.generated.h"

class UBloodBuyerDataAsset;
class UBloodHarvestSourceDataAsset;
class UPlaceableStationItem;
class UPlaceableStationDataAsset;
class UBloodProcessingRecipeDataAsset;
class UBloodProductItem;
class ABloodProcessingStation;
class ABloodPackagingStation;
class AStationPlacementPreviewActor;
class AWorkspaceRoom;
class UOwnSystemInventoryComponent;
class UOwnSystemItem;
class UOwnSystemMenu;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnVampireEconomyUpdated);

UCLASS(ClassGroup=(Vampire), meta=(BlueprintSpawnableComponent))
class VAMPIREEMPIRE_API UVampireEconomyComponent : public UActorComponent, public IOwnSystemSavableComponent
{
	GENERATED_BODY()

public:
	UVampireEconomyComponent();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy")
	bool CreateHarvestedBloodItem(UOwnSystemInventoryComponent* TargetInventory, const UBloodHarvestSourceDataAsset* HarvestSource, UBloodProductItem*& OutCreatedItem);

	UFUNCTION(BlueprintPure, Category = "Vampire|Economy")
	bool CanSellBloodItemToBuyer(const UBloodProductItem* BloodItem, const UBloodBuyerDataAsset* BuyerData, FText& OutReason) const;

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy")
	bool SellBloodItemToBuyer(UOwnSystemInventoryComponent* TargetInventory, UBloodProductItem* BloodItem, const UBloodBuyerDataAsset* BuyerData, int32& OutPayout, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy")
	bool ProcessBloodItem(UBloodProductItem* BloodItem, const UBloodProcessingRecipeDataAsset* RecipeData, FText& OutReason);

	UFUNCTION(BlueprintPure, Category = "Vampire|Economy")
	UOwnSystemInventoryComponent* FindOwningInventory() const;

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy")
	void SetCurrentSimDay(int32 NewDay);

	UFUNCTION(BlueprintPure, Category = "Vampire|Economy")
	int32 GetCurrentSimDay() const;

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy")
	void AddThrallUnit(const FThrallUpkeepUnit& ThrallUnit);

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy")
	bool ProcessDailyThrallUpkeep(FText& OutSummary);

	UFUNCTION(BlueprintPure, Category = "Vampire|Economy")
	const TArray<FThrallUpkeepUnit>& GetThrallUnits() const { return ThrallUnits; }

	UFUNCTION(BlueprintPure, Category = "Vampire|Economy|UI")
	int32 GetActiveThrallCount() const;

	UFUNCTION(BlueprintPure, Category = "Vampire|Economy|UI")
	int32 GetTotalDailyBloodUpkeep() const;

	UFUNCTION(BlueprintPure, Category = "Vampire|Economy|UI")
	FText GetThrallUpkeepSummaryText() const;

	UFUNCTION(BlueprintPure, Category = "Vampire|Economy|UI")
	FText GetLastFeedbackMessage() const { return LastFeedbackMessage; }

	UFUNCTION(BlueprintPure, Category = "Vampire|Economy|UI")
	bool WasLastActionSuccessful() const { return bLastActionSuccessful; }

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy|UI")
	void SetInteractionFeedback(const FText& NewFeedbackMessage, bool bWasSuccessful);

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy|Placement")
	bool StartStationPlacementMode(UPlaceableStationItem* StationItem, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy|Placement")
	bool StartMovePlacedStationMode(ABloodProcessingStation* ProcessingStation, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy|Placement")
	bool CancelStationPlacementMode(FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy|Placement")
	bool ConfirmStationPlacementMode(FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Vampire|Economy|Placement")
	void RotateStationPlacementMode();

	UFUNCTION(BlueprintPure, Category = "Vampire|Economy|Placement")
	bool IsStationPlacementModeActive() const { return bStationPlacementModeActive; }

	UFUNCTION(Client, Reliable)
	void ClientOpenProcessingStationMenu(ABloodProcessingStation* ProcessingStation);

	UFUNCTION(Client, Reliable)
	void ClientReceiveInteractionFeedback(const FText& FeedbackMessage, bool bWasSuccessful);

	bool RequestStepSelectedRecipe(ABloodProcessingStation* ProcessingStation, int32 Direction, FText& OutReason);
	bool RequestOpenProcessingStationMenu(ABloodProcessingStation* ProcessingStation, FText& OutReason);
	bool RequestReleaseProcessingStation(ABloodProcessingStation* ProcessingStation, FText& OutReason);
	bool RequestPrepareManualProcessing(ABloodProcessingStation* ProcessingStation, UBloodProductItem* SelectedItem, int32 TotalAvailableUnits, bool bRespawnOnly, FText& OutReason);
	bool RequestStartProcessing(ABloodProcessingStation* ProcessingStation, UBloodProductItem* SelectedItem, int32 TotalAvailableUnits, FText& OutReason);
	bool RequestHarvestProcessedBatch(ABloodProcessingStation* ProcessingStation, FText& OutReason);
	bool RequestCancelManualProcessing(ABloodProcessingStation* ProcessingStation, FText& OutReason);
	bool RequestCancelReservedPackaging(ABloodPackagingStation* PackagingStation, FText& OutReason);
	bool RequestConfirmManualPlacement(ABloodProcessingStation* ProcessingStation, FText& OutReason);
	bool RequestConfirmPackagingPlacement(ABloodPackagingStation* PackagingStation, int32 SlotIndex, FText& OutReason);
	bool RequestProcessDailyThrallUpkeep(FText& OutReason);
	bool RequestDebugForceOperatorUnpossess(FText& OutReason);
	bool RequestAutoPlaceStationItem(UPlaceableStationItem* StationItem, FText& OutReason);
	bool RequestPlaceStationItem(AWorkspaceRoom* WorkspaceRoom, UPlaceableStationItem* StationItem, FIntPoint AnchorCell, int32 RotationQuarterTurns, FText& OutReason);
	bool RequestMovePlacedStation(ABloodProcessingStation* ProcessingStation, AWorkspaceRoom* WorkspaceRoom, FIntPoint AnchorCell, int32 RotationQuarterTurns, FText& OutReason);

	UPROPERTY(BlueprintAssignable, Category = "Vampire|Economy")
	FOnVampireEconomyUpdated OnEconomyUpdated;

	static UOwnSystemInventoryComponent* ResolveInventoryFromActor(const AActor* Actor);
	static UVampireEconomyComponent* ResolveEconomyFromActor(const AActor* Actor);

protected:
	UPROPERTY(EditAnywhere, SaveGame, Replicated, Category = "Economy")
	int32 CurrentSimDay = 0;

	UPROPERTY(VisibleAnywhere, SaveGame, Replicated, Category = "Economy")
	int32 LifetimeBloodHarvested = 0;

	UPROPERTY(VisibleAnywhere, SaveGame, Replicated, Category = "Economy")
	int32 LifetimeBloodSold = 0;

	UPROPERTY(EditAnywhere, SaveGame, Replicated, Category = "Economy|Thralls")
	TArray<FThrallUpkeepUnit> ThrallUnits;

	UPROPERTY(VisibleAnywhere, Replicated, Category = "Economy|Feedback")
	FText LastFeedbackMessage;

	UPROPERTY(VisibleAnywhere, Replicated, Category = "Economy|Feedback")
	bool bLastActionSuccessful = false;

	UPROPERTY(
		EditDefaultsOnly,
		Category = "Economy|Placement|UI",
		meta = (
			ToolTip = "OwnSystem menu widget class that must be deactivated before starting station placement from inventory. Use this to mirror OwnSystem's normal Button_Exit -> DeactivateWidget() flow for the inventory menu. Leave unset only if the owning flow closes the inventory menu elsewhere."
		)
	)
	TSoftClassPtr<UOwnSystemMenu> InventoryMenuToDeactivateOnStationUse;

	UPROPERTY(Transient)
	bool bStationPlacementModeActive = false;

	UPROPERTY(Transient)
	TObjectPtr<UPlaceableStationItem> PendingPlacementItem;

	UPROPERTY(Transient)
	TObjectPtr<ABloodProcessingStation> PendingMoveStation;

	UPROPERTY(Transient)
	TObjectPtr<UPlaceableStationDataAsset> PendingPlacementDefinition;

	UPROPERTY(Transient)
	TObjectPtr<AWorkspaceRoom> OriginalPlacementWorkspaceRoom;

	UPROPERTY(Transient)
	FIntPoint OriginalPlacementAnchorCell = FIntPoint::ZeroValue;

	UPROPERTY(Transient)
	int32 OriginalPlacementRotationQuarterTurns = 0;

	UPROPERTY(Transient)
	TObjectPtr<AWorkspaceRoom> PlacementWorkspaceRoom;

	UPROPERTY(Transient)
	FIntPoint PlacementAnchorCell = FIntPoint::ZeroValue;

	UPROPERTY(Transient)
	int32 PlacementRotationQuarterTurns = 0;

	UPROPERTY(Transient)
	bool bPlacementCandidateValid = false;

	UPROPERTY(Transient)
	bool bPlacementConfirmPending = false;

	UPROPERTY(Transient)
	FText PlacementCandidateReason;

	UPROPERTY(Transient)
	TObjectPtr<class UInputComponent> PlacementHotkeyInputComponent;

	UPROPERTY(Transient)
	TObjectPtr<class APlayerController> PlacementHotkeyPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<AStationPlacementPreviewActor> PlacementPreviewActor;

	UPROPERTY(Transient)
	TObjectPtr<ABloodProcessingStation> LastReplicatedMovePreviewStation;

	UPROPERTY(Transient)
	TObjectPtr<AWorkspaceRoom> LastReplicatedMovePreviewWorkspaceRoom;

	UPROPERTY(Transient)
	FIntPoint LastReplicatedMovePreviewAnchorCell = FIntPoint::ZeroValue;

	UPROPERTY(Transient)
	int32 LastReplicatedMovePreviewRotationQuarterTurns = 0;

	UPROPERTY(Transient)
	bool bLastReplicatedMovePreviewValid = false;

	UPROPERTY(Transient)
	TObjectPtr<UPlaceableStationDataAsset> LastReplicatedMovePreviewDefinition;

	UPROPERTY(Transient)
	bool bHasLastReplicatedMovePreview = false;

private:
	UFUNCTION(Server, Reliable)
	void ServerStepSelectedRecipe(ABloodProcessingStation* ProcessingStation, int32 Direction);

	UFUNCTION(Server, Reliable)
	void ServerOpenProcessingStationMenu(ABloodProcessingStation* ProcessingStation);

	UFUNCTION(Server, Reliable)
	void ServerReleaseProcessingStation(ABloodProcessingStation* ProcessingStation);

	UFUNCTION(Server, Reliable)
	void ServerBeginMovePlacedStationPreview(ABloodProcessingStation* ProcessingStation);

	UFUNCTION(Server, Reliable)
	void ServerEndMovePlacedStationPreview(ABloodProcessingStation* ProcessingStation);

	UFUNCTION(Server, Unreliable)
	void ServerUpdateMovePlacedStationPreview(ABloodProcessingStation* ProcessingStation, AWorkspaceRoom* WorkspaceRoom, FIntPoint AnchorCell, int32 RotationQuarterTurns, bool bIsValidPlacement, UPlaceableStationDataAsset* StationDefinition);

	UFUNCTION(Server, Reliable)
	void ServerPrepareManualProcessing(ABloodProcessingStation* ProcessingStation, UBloodProductItem* SelectedItem, int32 TotalAvailableUnits, bool bRespawnOnly);

	UFUNCTION(Server, Reliable)
	void ServerStartProcessing(ABloodProcessingStation* ProcessingStation, UBloodProductItem* SelectedItem, int32 TotalAvailableUnits);

	UFUNCTION(Server, Reliable)
	void ServerHarvestProcessedBatch(ABloodProcessingStation* ProcessingStation);

	UFUNCTION(Server, Reliable)
	void ServerCancelManualProcessing(ABloodProcessingStation* ProcessingStation);

	UFUNCTION(Server, Reliable)
	void ServerCancelReservedPackaging(ABloodPackagingStation* PackagingStation);

	UFUNCTION(Server, Reliable)
	void ServerConfirmManualPlacement(ABloodProcessingStation* ProcessingStation);

	UFUNCTION(Server, Reliable)
	void ServerConfirmPackagingPlacement(ABloodPackagingStation* PackagingStation, int32 SlotIndex);

	UFUNCTION(Server, Reliable)
	void ServerProcessDailyThrallUpkeep();

	UFUNCTION(Server, Reliable)
	void ServerDebugForceOperatorUnpossess();

	UFUNCTION(Server, Reliable)
	void ServerPlaceStationItem(AWorkspaceRoom* WorkspaceRoom, UPlaceableStationItem* StationItem, FIntPoint AnchorCell, int32 RotationQuarterTurns);

	UFUNCTION(Server, Reliable)
	void ServerMovePlacedStation(ABloodProcessingStation* ProcessingStation, AWorkspaceRoom* WorkspaceRoom, FIntPoint AnchorCell, int32 RotationQuarterTurns);

	static int32 GetSourceScore(EBloodSourceType SourceType);
	static EBloodQuality ComputeQuality(EBloodSourceType SourceType, int32 ConditionScore, int32 SetupScore);
	static float GetQualityMultiplier(EBloodQuality Quality);
	static float GetProcessingMultiplier(EBloodProcessingType ProcessingType);
	static int32 GetTotalBloodUnitsByMinimumQuality(const TArray<UOwnSystemItem*>& Items, EBloodQuality MinimumQuality);
	static bool ConsumeBloodUnitsByMinimumQuality(UOwnSystemInventoryComponent* Inventory, EBloodQuality MinimumQuality, int32 UnitsToConsume);
	const UPlaceableStationDataAsset* GetActivePlacementDefinition() const;
	const FGuid* GetIgnoredPlacementInstanceId() const;
	void UpdateStationPlacementPreview();
	void SyncMovePlacementPreviewReplication();
	bool ResolvePlacementCandidate(AWorkspaceRoom*& OutWorkspaceRoom, FIntPoint& OutAnchorCell, FText& OutReason) const;
	void DrawStationPlacementPreview() const;
	void SyncPlacementPreviewActor();
	void ApplyPlacementInputContext(class APlayerController* PlayerController);
	void RemovePlacementInputContext(class APlayerController* PlayerController);
	void RestorePendingMoveStationAfterPlacementCancel();
	void RestoreControllerInputAfterPlacement(class APlayerController* PlayerController);
	void StopStationPlacementMode(bool bRestorePendingMoveStation = true);
	void HandlePlacementConfirmPressed();
	void HandlePlacementCancelPressed();
	void HandlePlacementRotatePressed();
	void NotifyEconomyUpdated();
};
