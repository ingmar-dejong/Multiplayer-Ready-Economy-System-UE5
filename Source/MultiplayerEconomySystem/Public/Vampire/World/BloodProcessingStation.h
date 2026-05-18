#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "GameplayTagContainer.h"
#include "Vampire/BloodTypes.h"
#include "BloodProcessingStation.generated.h"

class UBloodProcessingAttachmentDataAsset;
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
class APlayerController;
class APawn;

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
class VAMPIREEMPIRE_API ABloodProcessingStation : public AActor
{
	GENERATED_BODY()

public:
	ABloodProcessingStation();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
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
	TSubclassOf<UVampireBarrelMenu> BarrelMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|UI")
	TSubclassOf<UVampireEconomySummaryMenu> StationMenuClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, SaveGame, Category = "Processing")
	FText StationDisplayName;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Processing|Manual")
	TObjectPtr<UInteractionPlacementTargetComponent> ResolvedManualPlacementTarget;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Replicated, Category = "Processing|Manual")
	TObjectPtr<AActor> SpawnedManualPreviewActor;

protected:
	UFUNCTION()
	void HandleManualPlacementConfirmed(UInteractionPlacementTargetComponent* TargetComponent, UManipulatableObjectComponent* ManipulatedObject);

	UInteractionPlacementTargetComponent* ResolveManualPlacementTarget() const;
	USceneComponent* ResolveManualPreviewSpawnPoint() const;
	bool SpawnManualPreviewActorForRequest(const FBloodProcessingStartRequest& Request, FText& OutReason);
	void DestroyManualPreviewActor();
	void HandleInteractionCancelPressed();
	void HandleDebugForceOperatorUnpossessPressed();

private:
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
};
