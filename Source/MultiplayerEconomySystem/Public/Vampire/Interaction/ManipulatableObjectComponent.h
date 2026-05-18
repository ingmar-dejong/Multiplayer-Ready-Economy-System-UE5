#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "ManipulatableObjectComponent.generated.h"

class UPrimitiveComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EManipulatableRotationAxes : uint8
{
	None UMETA(DisplayName = "None"),
	YawPitch UMETA(DisplayName = "Yaw And Pitch"),
	All UMETA(DisplayName = "All Axes"),
};

UCLASS(ClassGroup=(Vampire), meta=(BlueprintSpawnableComponent))
class VAMPIREEMPIRE_API UManipulatableObjectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UManipulatableObjectComponent();

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	bool CanBeGrabbed() const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	bool IsValidForPlacementTag(FGameplayTag PlacementTag) const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	UPrimitiveComponent* GetPrimaryPhysicsComponent() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	FGameplayTag ManipulationTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	FGameplayTag ObjectRoleTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	FGameplayTagContainer AllowedPlacementTags;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bCanBeGrabbed = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bAllowTranslation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bAllowRotation = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bAllowRoll = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	EManipulatableRotationAxes AllowedRotationAxes = EManipulatableRotationAxes::All;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	bool bSnapOnValidPlacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	int32 PlacementPriority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	FVector HeldOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	FRotator HeldRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	TObjectPtr<USceneComponent> GrabPivotComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	TObjectPtr<UPrimitiveComponent> PrimaryPhysicsComponent;
};
