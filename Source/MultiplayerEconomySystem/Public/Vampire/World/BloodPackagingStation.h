#pragma once

#include "CoreMinimal.h"
#include "Vampire/World/BloodProcessingStation.h"
#include "BloodPackagingStation.generated.h"

class UInteractionPlacementTargetComponent;
class UManipulatableObjectComponent;
class UOwnSystemInventoryComponent;

USTRUCT(BlueprintType)
struct FPackagingPlacedSlotState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Packaging")
	int32 SlotIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Packaging")
	TObjectPtr<AActor> PlacedActor;
};

UCLASS()
class VAMPIREEMPIRE_API ABloodPackagingStation : public ABloodProcessingStation
{
	GENERATED_BODY()

public:
	ABloodPackagingStation();
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual bool RequiresManualProcessingFlow() const override;
	virtual bool CanAcceptBloodItem(const UBloodProductItem* BloodItem, int32 TotalAvailableUnits, FText& OutReason) const override;
	virtual bool PrepareManualProcessingRequest(const UBloodProductItem* BloodItem, int32 TotalAvailableUnits, FText& OutReason) override;
	virtual bool CancelManualProcessingRequest(FText& OutReason) override;
	virtual void HandleAbandonedManualFlow() override;

	UFUNCTION(BlueprintCallable, Category = "Processing|Packaging")
	void RefreshPackagingPlacementState();

	UFUNCTION(BlueprintCallable, Category = "Processing|Packaging")
	bool TryCommitPackagedBatch(FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Processing|Packaging")
	bool ConfirmPackagingPlacement(int32 SlotIndex, FText& OutReason);

	UFUNCTION(BlueprintCallable, Category = "Processing|Packaging")
	void CleanupLoosePackagingPreviewActor();

	UFUNCTION(BlueprintPure, Category = "Processing|Packaging")
	int32 GetPlacedPackagingItemCount() const;

	UFUNCTION(BlueprintPure, Category = "Processing|Packaging")
	int32 GetRequiredPackagingItemCount() const;

	const FBloodProcessingStartRequest* GetReservedPackagingRequest() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Packaging", meta = (ClampMin = 1))
	int32 RequiredPackagingItemCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Packaging", meta = (ClampMin = 1))
	int32 MaxReservedBatchUnits = 12;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Processing|Packaging", meta = (UseComponentPicker, AllowedClasses = "/Script/VampireEmpire.InteractionPlacementTargetComponent"))
	TArray<FComponentReference> PackagingPlacementTargets;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Replicated, Category = "Processing|Packaging")
	int32 PlacedPackagingItemCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, ReplicatedUsing = OnRep_PlacedPackagingSlots, Category = "Processing|Packaging")
	TArray<FPackagingPlacedSlotState> PlacedPackagingSlots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Processing|Packaging")
	TArray<TObjectPtr<UInteractionPlacementTargetComponent>> ResolvedPackagingPlacementTargets;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Processing|Packaging")
	TArray<TObjectPtr<AActor>> PlacedPackagingPreviewActors;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Processing|Packaging")
	TObjectPtr<UOwnSystemInventoryComponent> ReservedPackagingInventory;

private:
	void EnsurePackagingRecipeConstraints();
	void SyncPackagingRequirements();
	void BroadcastPackagingProgressFeedback();
	void ResetPackagingPlacementState();
	int32 ResolvePackagingPlacementTargetIndex(const UInteractionPlacementTargetComponent* TargetComponent) const;
	UInteractionPlacementTargetComponent* GetPackagingPlacementTargetByIndex(int32 SlotIndex) const;
	bool IsPackagingSlotOccupied(int32 SlotIndex) const;
	bool AssignPlacedPackagingActorToSlot(int32 SlotIndex, AActor* PackagingActor, FText& OutReason);
	void RebuildPackagingPlacementRuntimeState();
	void SyncPlacedPackagingActorTransform(AActor* PackagingActor, UInteractionPlacementTargetComponent* PlacementTarget) const;

	UFUNCTION()
	void OnRep_PlacedPackagingSlots();

	UFUNCTION()
	void HandlePackagingPlacementConfirmed(UInteractionPlacementTargetComponent* TargetComponent, UManipulatableObjectComponent* ManipulatedObject);
};
