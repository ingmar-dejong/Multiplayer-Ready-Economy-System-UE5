#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "OwnSystemSavableActor.h"
#include "GameplayTagContainer.h"
#include "UI/ProcessingStationMenuBase.h"
#include "Vampire/BloodTypes.h"
#include "BloodProcessingStation.generated.h"

class UBloodProcessingAttachmentDataAsset;
class UPlaceableStationDataAsset;
class UBloodProcessingInteractableComponent;
class UBloodProcessingRecipeDataAsset;
class UBloodProductItem;
class UCameraComponent;
class UInputMappingContext;
class UInputComponent;
class UInteractionPlacementTargetComponent;
class UManipulatableObjectComponent;
class UOwnSystemInventoryComponent;
class USceneComponent;
class UStaticMeshComponent;
class UVampireBarrelMenu;
class UVampireEconomyComponent;
class UVampireEconomySummaryMenu;
class AStationPlacementPreviewActor;
class AWorkspaceRoom;
class APlayerController;
class APawn;
class ULocalPlayer;
struct FWorkspacePlacedStationRecord;

USTRUCT(BlueprintType)
struct FProcessingStationMovePreviewState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Processing|Placement")
	bool bActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Processing|Placement")
	bool bValidPlacement = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Processing|Placement")
	FName RoomId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Processing|Placement")
	TObjectPtr<AWorkspaceRoom> WorkspaceRoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Processing|Placement")
	FIntPoint AnchorCell = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Processing|Placement")
	int32 RotationQuarterTurns = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Processing|Placement")
	TObjectPtr<UPlaceableStationDataAsset> StationDefinition;
};

USTRUCT(BlueprintType)
struct FProcessingStationAttachmentSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Processing")
	FName SlotId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	FText SlotDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	FName AnchorComponentName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	FGameplayTagContainer AllowedAttachmentTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Processing")
	TObjectPtr<UBloodProcessingAttachmentDataAsset> PlacedAttachment;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> SpawnedAttachmentMeshComponent;
};

USTRUCT(BlueprintType)
struct FBloodProcessingStartRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
	TSubclassOf<UBloodProductItem> ItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
	EBloodSourceType SourceType = EBloodSourceType::Rat;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
	EBloodQuality BaseQuality = EBloodQuality::Gewoon;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing")
	EBloodProcessingType ProcessingType = EBloodProcessingType::Vers;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing", meta = (ClampMin = 1))
	int32 BloodQuantity = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Processing", meta = (ClampMin = 0))
	int32 CreatedDay = 0;
};

UCLASS()
class VAMPIREEMPIRE_API ABloodProcessingStation : public AActor, public IOwnSystemSavableActor
{
	GENERATED_BODY()

public:
	ABloodProcessingStation();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PrepareForSave_Implementation() override;
	virtual void Load_Implementation() override;
	virtual bool ShouldRespawn_Implementation() const override;
	virtual void SetActorGUID_Implementation(const FGuid& SavedGUID) override;
	virtual FGuid GetActorGUID_Implementation() const override;
	virtual bool RequiresManualProcessingFlow() const;
	virtual void Tick(float DeltaSeconds) override;

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Processing")
	void UpdateReadyState(int32 CurrentSimDay);

	UFUNCTION(BlueprintPure, Category = "Processing")
	float GetProcessingDurationTimeUnits() const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	float GetProcessingElapsedTimeUnits(float CurrentAccumulatedTime) const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	float GetProcessingRemainingTimeUnits(float CurrentAccumulatedTime) const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	float GetProcessingProgress01(float CurrentAccumulatedTime) const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	virtual bool CanAcceptBloodItem(const UBloodProductItem* BloodItem, int32 TotalAvailableUnits, FText& OutReason) const;

	UFUNCTION(BlueprintCallable, Category = "Processing")
	bool TryStartProcessing(UOwnSystemInventoryComponent* Inventory, UVampireEconomyComponent* Economy, UBloodProductItem* BloodItem, int32 TotalAvailableUnits, FText& OutReason);

	UFUNCTION(BlueprintPure, Category = "Processing")
	bool BuildProcessingStartRequest(const UBloodProductItem* BloodItem, int32 TotalAvailableUnits, FBloodProcessingStartRequest& OutRequest, FText& OutReason) const;

	UFUNCTION(BlueprintCallable, Category = "Processing")
	bool CommitProcessingStartRequest(UOwnSystemInventoryComponent* Inventory, UVampireEconomyComponent* Economy, const FBloodProcessingStartRequest& Request, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Processing")
	bool StageManualProcessingRequest(const UBloodProductItem* BloodItem, int32 TotalAvailableUnits, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Processing")
	virtual bool PrepareManualProcessingRequest(const UBloodProductItem* BloodItem, int32 TotalAvailableUnits, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Processing")
	bool CommitStagedManualProcessingRequest(UOwnSystemInventoryComponent* Inventory, UVampireEconomyComponent* Economy, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Processing")
	void ClearStagedManualProcessingRequest();

	UFUNCTION(BlueprintCallable, Category = "Processing|Manual")
	virtual bool CancelManualProcessingRequest(FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Processing|Manual")
	virtual void HandleAbandonedManualFlow();

	UFUNCTION(BlueprintPure, Category = "Processing")
	bool HasStagedManualProcessingRequest() const;

	UFUNCTION(BlueprintPure, Category = "Processing|Manual")
	bool HasSpawnedManualProcessingActor() const;

	UFUNCTION(BlueprintCallable, Category = "Processing")
	bool TryHarvestProcessedBatch(UOwnSystemInventoryComponent* Inventory, UVampireEconomyComponent* Economy, FText& OutReason);

	UFUNCTION(BlueprintPure, Category = "Processing")
	FText BuildStationStatusText() const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	const UBloodProcessingRecipeDataAsset* GetActiveRecipe() const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	int32 GetRecipeCount() const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	int32 GetSelectedRecipeIndex() const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	const UBloodProcessingRecipeDataAsset* GetRecipeAtIndex(int32 RecipeIndex) const;

	UFUNCTION(BlueprintCallable, Category = "Processing")
	void SetSelectedRecipeIndex(int32 NewRecipeIndex);

	UFUNCTION(BlueprintCallable, Category = "Processing")
	void StepSelectedRecipe(int32 Direction);

	UFUNCTION(BlueprintPure, Category = "Processing")
	bool HasInstalledAttachmentTag(FGameplayTag Tag) const;

	UFUNCTION(BlueprintCallable, Category = "Processing")
	void AddStationUnlockTag(FGameplayTag Tag);

	UFUNCTION(BlueprintCallable, Category = "Processing")
	void RemoveStationUnlockTag(FGameplayTag Tag);

	UFUNCTION(BlueprintPure, Category = "Processing")
	FGameplayTagContainer GetInstalledAttachmentTags() const;

	UFUNCTION(BlueprintCallable, Category = "Processing")
	bool PlaceAttachmentInSlotByIndex(int32 SlotIndex, UBloodProcessingAttachmentDataAsset* AttachmentData, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Processing")
	bool RemoveAttachmentFromSlotByIndex(int32 SlotIndex, FText& OutReason);

	UFUNCTION(BlueprintPure, Category = "Processing")
	const UBloodProcessingAttachmentDataAsset* GetPlacedAttachmentInSlot(int32 SlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	bool DoesRecipeMeetStationRequirements(const UBloodProcessingRecipeDataAsset* Recipe, FText& OutReason) const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	int32 GetProcessingDurationDays() const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	int32 GetProcessingDurationDaysForSource(EBloodSourceType SourceType) const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	int32 GetProcessingDurationDaysForBlood(const UBloodProductItem* BloodItem) const;

	UFUNCTION(BlueprintPure, Category = "Processing")
	int32 GetStoredBatchProcessingDurationDays() const;

	UFUNCTION(BlueprintCallable, Category = "Processing|Input")
	void ApplyInteractionInputContext(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Processing|Input")
	void RemoveInteractionInputContext(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Processing|Input")
	void RemoveInteractionHotkeys(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Processing|Manual")
	void SetActiveInteractorContext(APawn* InInteractor);

	UFUNCTION(BlueprintCallable, Category = "Processing|Manual")
	void ClearActiveInteractorContext();

	UFUNCTION(BlueprintCallable, Category = "Processing|Authority")
	bool TryClaimOperator(APawn* InInteractor, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Processing|Authority")
	void ReleaseOperator(APawn* InInteractor);

	UFUNCTION(BlueprintPure, Category = "Processing|Authority")
	bool IsCurrentOperator(const APawn* InInteractor) const;

	UFUNCTION(BlueprintCallable, Category = "Processing|Placement")
	bool TryBeginMovePlacement(APawn* InInteractor, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Processing|Placement")
	void EndMovePlacement(APawn* InInteractor);

	UFUNCTION(BlueprintCallable, Category = "Processing|Placement")
	void UpdateMovePlacementPreview(AWorkspaceRoom* PreviewWorkspaceRoom, FIntPoint PreviewAnchorCell, int32 PreviewRotationQuarterTurns, bool bPreviewIsValid, UPlaceableStationDataAsset* PreviewStationDefinition);

	UFUNCTION(BlueprintPure, Category = "Processing|Placement")
	bool IsMovePlacementInProgress() const { return bMovePlacementInProgress; }

	UFUNCTION(BlueprintCallable, Category = "Processing|Placement")
	void SetPlacementContext(AWorkspaceRoom* InWorkspaceRoom, const FGuid& InPlacedStationInstanceId, const FName& InOwningRoomId, FIntPoint InGridAnchorCell, int32 InRotationQuarterTurns);

	UFUNCTION(BlueprintPure, Category = "Processing|Placement")
	const FGuid& GetPlacedStationInstanceId() const { return PlacedStationInstanceId; }

	UFUNCTION(BlueprintPure, Category = "Processing|Placement")
	AWorkspaceRoom* GetOwningWorkspaceRoom() const;

	UFUNCTION(BlueprintPure, Category = "Processing|Placement")
	bool HasRegisteredPlacementContext() const;

	UFUNCTION(BlueprintCallable, Category = "Processing|Placement")
	bool EnsurePlacementRecordForCurrentTransform(FText& OutReason);

	bool TryBuildPlacementRecordForWorkspace(
		AWorkspaceRoom* CandidateWorkspaceRoom,
		FWorkspacePlacedStationRecord& OutPlacementRecord,
		FText& OutReason) const;

	bool TryInferCurrentPlacementContext(
		AWorkspaceRoom*& OutWorkspaceRoom,
		FIntPoint& OutAnchorCell,
		int32& OutRotationQuarterTurns,
		const UPlaceableStationDataAsset*& OutPlacementDefinition,
		FText& OutReason) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UCameraComponent> InteractionCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBloodProcessingInteractableComponent> ProcessingInteractable;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	TObjectPtr<UBloodProcessingRecipeDataAsset> RecipeData;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing")
	TArray<TObjectPtr<UBloodProcessingRecipeDataAsset>> AvailableRecipes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Attachments")
	TArray<TObjectPtr<UBloodProcessingAttachmentDataAsset>> AvailableAttachmentOptions;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Processing|Attachments")
	TArray<FProcessingStationAttachmentSlot> AttachmentSlots;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Processing")
	FGameplayTagContainer StationUnlockTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Processing")
	int32 SelectedRecipeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|UI")
	TSubclassOf<UProcessingStationMenuBase> ProcessingMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|UI", meta = (DeprecatedProperty, DeprecationMessage = "Use ProcessingMenuClass instead."))
	TSubclassOf<UProcessingStationMenuBase> BarrelMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|UI", meta = (DeprecatedProperty, DeprecationMessage = "StationMenuClass is no longer used. Processing stations now open via ProcessingMenuClass only."))
	TSubclassOf<UVampireEconomySummaryMenu> StationMenuClass;

	UFUNCTION(BlueprintPure, Category = "Processing|UI")
	TSubclassOf<UProcessingStationMenuBase> GetResolvedProcessingMenuClass() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Processing")
	FText StationDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Placement")
	TObjectPtr<UPlaceableStationDataAsset> PlacementDefinitionOverride;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Placement")
	bool bRegisterAsPlacedStationOnBeginPlay = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Placement")
	TObjectPtr<AWorkspaceRoom> WorkspaceRoomOverride;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing = OnRep_PlacementState, Category = "Processing|Placement")
	FGuid PlacedStationInstanceId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing = OnRep_PlacementState, Category = "Processing|Placement")
	FName OwningRoomId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing = OnRep_PlacementState, Category = "Processing|Placement")
	FIntPoint GridAnchorCell = FIntPoint::ZeroValue;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, ReplicatedUsing = OnRep_PlacementState, Category = "Processing|Placement")
	int32 RotationQuarterTurns = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Category = "Save")
	FGuid SavedActorGuid;

	UPROPERTY(Transient)
	TWeakObjectPtr<AWorkspaceRoom> OwningWorkspaceRoom;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Camera")
	bool bUseInteractionCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Camera", meta = (ClampMin = 0.0))
	float InteractionCameraBlendTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Input")
	TObjectPtr<UInputMappingContext> InteractionInputMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Input")
	int32 InteractionInputMappingPriority = 10;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Manual", meta = (UseComponentPicker, AllowedClasses = "/Script/VampireEmpire.InteractionPlacementTargetComponent"))
	FComponentReference ManualPlacementTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Manual")
	TSubclassOf<AActor> ManualPreviewActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Manual")
	FName ManualPreviewSpawnPointComponentName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Manual", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.SceneComponent"))
	FComponentReference ManualPreviewSpawnPoint;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Processing", meta = (ClampMin = 1))
	int32 MinimumUnitsRequired = 10;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Processing")
	EBloodVatStationState StationState = EBloodVatStationState::Leeg;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Processing")
	FStoredBloodBatchData StoredBatch;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Processing")
	int32 ProcessingStartDay = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Processing")
	int32 ProcessingReadyDay = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Processing")
	float ProcessingStartAccumulatedTime = -1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, SaveGame, Replicated, Category = "Processing")
	float ProcessingReadyAccumulatedTime = -1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Replicated, Category = "Processing|Manual")
	bool bHasStagedManualStartRequest = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Replicated, Category = "Processing|Manual")
	FBloodProcessingStartRequest StagedManualStartRequest;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Processing|Manual")
	TObjectPtr<APawn> ActiveInteractor;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Replicated, Category = "Processing|Authority")
	TObjectPtr<APawn> CurrentOperator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, ReplicatedUsing = OnRep_MovePlacementState, Category = "Processing|Placement")
	bool bMovePlacementInProgress = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, ReplicatedUsing = OnRep_MovePreviewState, Category = "Processing|Placement")
	FProcessingStationMovePreviewState MovePreviewState;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Processing|Manual")
	TObjectPtr<UInteractionPlacementTargetComponent> ResolvedManualPlacementTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Replicated, Category = "Processing|Manual")
	TObjectPtr<AActor> SpawnedManualPreviewActor;

protected:
	UFUNCTION()
	void OnRep_PlacementState();

	UFUNCTION()
	void OnRep_MovePlacementState();

	UFUNCTION()
	void OnRep_MovePreviewState();

	UFUNCTION()
	void HandleManualPlacementConfirmed(UInteractionPlacementTargetComponent* TargetComponent, UManipulatableObjectComponent* ManipulatedObject);

	bool AddBloodItemFromRequestToInventory(
		UOwnSystemInventoryComponent* Inventory,
		const FBloodProcessingStartRequest& Request,
		EBloodProcessingType ResultProcessingType,
		bool bMarkAsPackaged,
		EBloodProcessingType SourceProcessingType,
		UBloodProductItem*& OutInventoryItem,
		FText& OutReason) const;

	UInteractionPlacementTargetComponent* ResolveManualPlacementTarget() const;
	USceneComponent* ResolveManualPreviewSpawnPoint() const;
	void BootstrapEditorPlacedStationPlacement();
	bool ValidateManualPreviewSetup(FText& OutReason) const;
	bool SpawnManualPreviewActorForRequest(const FBloodProcessingStartRequest& Request, FText& OutReason);
	void DestroyManualPreviewActor();
	void HandleInteractionCancelPressed();
	void HandleDebugForceOperatorUnpossessPressed();

private:
	bool InferPlacementStateFromCurrentTransform(
		AWorkspaceRoom*& OutWorkspaceRoom,
		FIntPoint& OutAnchorCell,
		int32& OutRotationQuarterTurns,
		const UPlaceableStationDataAsset*& OutPlacementDefinition,
		FText& OutReason) const;
	const UPlaceableStationDataAsset* ResolvePlacementDefinitionForBootstrap() const;
	AWorkspaceRoom* ResolveOwningWorkspaceRoomFromReplicatedState() const;
	void RefreshPlacementTransformFromReplicatedState();
	AWorkspaceRoom* ResolveWorkspaceRoomById(FName RoomIdToFind) const;
	void SyncMovePreviewActor();
	void DestroyMovePreviewActor();
	USceneComponent* ResolveAttachmentAnchor(const FProcessingStationAttachmentSlot& Slot) const;
	UStaticMeshComponent* FindOrCreateAttachmentMeshComponent(FProcessingStationAttachmentSlot& Slot);
	void RefreshAttachmentVisuals();
	void RefreshAttachmentVisualForSlot(FProcessingStationAttachmentSlot& Slot);
	void DestroyAttachmentVisual(FProcessingStationAttachmentSlot& Slot);
	bool CanSlotAcceptAttachment(const FProcessingStationAttachmentSlot& Slot, const UBloodProcessingAttachmentDataAsset* AttachmentData, FText& OutReason) const;

	UPROPERTY(Transient)
	TObjectPtr<UInputComponent> InteractionHotkeyInputComponent;

	UPROPERTY(Transient)
	TObjectPtr<APlayerController> InteractionHotkeyPlayerController;

	UPROPERTY(Transient)
	TObjectPtr<AStationPlacementPreviewActor> MovePreviewActor;

	static TMap<TWeakObjectPtr<ULocalPlayer>, TMap<TObjectPtr<UInputMappingContext>, int32>> SharedInteractionMappingContextRefs;
};
