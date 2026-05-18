#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Components/SceneComponent.h"
#include "GameplayTagContainer.h"
#include "InteractionPlacementTargetComponent.generated.h"

class UManipulatableObjectComponent;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlacementObjectChanged, UInteractionPlacementTargetComponent*, TargetComponent, UManipulatableObjectComponent*, ManipulatedObject);

UCLASS(ClassGroup=(Vampire), meta=(BlueprintSpawnableComponent))
class VAMPIREEMPIRE_API UInteractionPlacementTargetComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UInteractionPlacementTargetComponent();
	virtual void BeginPlay() override;
	virtual void OnRegister() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	bool CanAcceptObject(const UManipulatableObjectComponent* ManipulatedObject) const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	bool IsObjectWithinPlacementTolerance(const UManipulatableObjectComponent* ManipulatedObject) const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	FTransform BuildSnapTransform(const UManipulatableObjectComponent* ManipulatedObject) const;

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	bool TryPlaceObject(UManipulatableObjectComponent* ManipulatedObject);

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	void RemovePlacedObject();

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	bool HasPlacedObject() const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	bool UsesSnapPlacement() const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	UPrimitiveComponent* GetResolvedTriggerVolume() const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	USceneComponent* GetEffectiveSnapComponent() const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	FVector GetEffectiveSnapLocation() const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	float GetPlacementDistance(const UManipulatableObjectComponent* ManipulatedObject) const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	FText BuildPlacementDebugText(const UManipulatableObjectComponent* ManipulatedObject) const;

	void DebugDraw(const UManipulatableObjectComponent* ManipulatedObject, bool bIsBestCandidate, float Duration) const;

	UPROPERTY(BlueprintAssignable, Category = "Manipulation")
	FOnPlacementObjectChanged OnValidObjectPlaced;

	UPROPERTY(BlueprintAssignable, Category = "Manipulation")
	FOnPlacementObjectChanged OnPlacedObjectRemoved;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	FGameplayTag PlacementTargetTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	FGameplayTagContainer AcceptedObjectTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	TObjectPtr<USceneComponent> SnapComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bUseSnapPlacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bUseTriggerVolume = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation", meta = (UseComponentPicker, AllowedClasses = "/Script/Engine.PrimitiveComponent"))
	FComponentReference TriggerVolume;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation", meta = (ClampMin = 0.0))
	float PositionTolerance = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation", meta = (ClampMin = 0.0))
	float RotationToleranceDegrees = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation|Debug")
	bool bEnableDebugDraw = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation|Debug")
	bool bEnableIdleDebugDraw = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bRequireRotationMatch = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bLockObjectWhenPlaced = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bConsumePlacement = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bAllowOnlyOneObject = true;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Manipulation")
	TObjectPtr<UManipulatableObjectComponent> PlacedObject;

	UPROPERTY(Transient, BlueprintReadOnly, Category = "Manipulation")
	TObjectPtr<UPrimitiveComponent> ResolvedTriggerVolume;

private:
	void EvaluatePlacementDebug(
		const UManipulatableObjectComponent* ManipulatedObject,
		bool& bOutCanAccept,
		bool& bOutWithinTolerance,
		bool& bOutRotationMismatch,
		float& OutDistance,
		FText& OutReason) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	void UpdateEditorVisualization();
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<USphereComponent> EditorToleranceSphere;
#endif

	UFUNCTION()
	void HandleTriggerVolumeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleTriggerVolumeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UManipulatableObjectComponent* ResolveManipulatableFromOverlap(AActor* OtherActor, UPrimitiveComponent* OtherComp) const;
};
