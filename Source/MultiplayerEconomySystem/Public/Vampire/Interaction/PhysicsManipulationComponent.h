#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicsManipulationComponent.generated.h"

class APlayerController;
class UInteractionPlacementTargetComponent;
class UManipulatableObjectComponent;
class UPhysicsHandleComponent;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnManipulatedObjectChanged, UPhysicsManipulationComponent*, ManipulationComponent, UManipulatableObjectComponent*, ManipulatedObject);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlacementTargetChanged, UPhysicsManipulationComponent*, ManipulationComponent, UInteractionPlacementTargetComponent*, PlacementTarget);

UENUM(BlueprintType)
enum class EPhysicsManipulationMode : uint8
{
	None UMETA(DisplayName = "None"),
	Translate UMETA(DisplayName = "Translate"),
	Rotate UMETA(DisplayName = "Rotate"),
};

UCLASS(ClassGroup=(Vampire), meta=(BlueprintSpawnableComponent))
class VAMPIREEMPIRE_API UPhysicsManipulationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPhysicsManipulationComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	bool TryGrabFromView(APlayerController* PlayerController);

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	bool TryGrabComponent(UPrimitiveComponent* PrimitiveComponent, const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	void ReleaseHeldObject();

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	void SetManipulationMode(EPhysicsManipulationMode NewMode);

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	void AddPlanarInput(FVector2D InputAxis);

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	void AddDepthInput(float InputAxis);

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	void AddRotationInput(FVector2D InputAxis);

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	void AddRollInput(float InputAxis);

	UFUNCTION(BlueprintCallable, Category = "Manipulation")
	bool TryPlaceHeldObjectAtTarget(UInteractionPlacementTargetComponent* PlacementTarget);

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	bool HasHeldObject() const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	UManipulatableObjectComponent* GetHeldManipulatable() const;

	UFUNCTION(BlueprintPure, Category = "Manipulation")
	UPrimitiveComponent* GetHeldPrimitive() const;

	UPROPERTY(BlueprintAssignable, Category = "Manipulation")
	FOnManipulatedObjectChanged OnObjectGrabbed;

	UPROPERTY(BlueprintAssignable, Category = "Manipulation")
	FOnManipulatedObjectChanged OnObjectReleased;

	UPROPERTY(BlueprintAssignable, Category = "Manipulation")
	FOnPlacementTargetChanged OnPlacementTargetEntered;

	UPROPERTY(BlueprintAssignable, Category = "Manipulation")
	FOnPlacementTargetChanged OnPlacementTargetLeft;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	TEnumAsByte<ECollisionChannel> GrabTraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation", meta = (ClampMin = 0.0))
	float GrabTraceDistance = 400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation", meta = (ClampMin = 0.0))
	float MinHoldDistance = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation", meta = (ClampMin = 0.0))
	float MaxHoldDistance = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation", meta = (ClampMin = 0.0))
	float DefaultHoldDistance = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation", meta = (ClampMin = 0.0))
	float TranslationSpeed = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation", meta = (ClampMin = 0.0))
	float DepthSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation", meta = (ClampMin = 0.0))
	float RotationSpeedDegrees = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation|Debug")
	bool bEnablePlacementDebugDraw = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation|Debug", meta = (ClampMin = 0.0))
	float PlacementDebugDrawDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation")
	EPhysicsManipulationMode CurrentMode = EPhysicsManipulationMode::Translate;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation|Networking", meta = (ClampMin = 0.01))
	float ServerTargetSendInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation|Networking", meta = (ClampMin = 0.0))
	float ServerTargetLocationThreshold = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Manipulation|Networking", meta = (ClampMin = 0.0))
	float ServerTargetRotationThresholdDegrees = 1.0f;

protected:
	void EnsurePhysicsHandle();
	void ApplyHeldTarget();
	void SetCurrentPlacementTarget(UInteractionPlacementTargetComponent* NewTarget);
	UManipulatableObjectComponent* ResolveManipulatableFromComponent(UPrimitiveComponent* PrimitiveComponent) const;
	bool IsOwnerLocallyDrivingManipulation() const;
	void BeginRemoteManipulationIfNeeded(UPrimitiveComponent* PrimitiveComponent, const FHitResult& HitResult);
	void EndRemoteManipulationIfNeeded(UPrimitiveComponent* PrimitiveComponent);
	void SendRemoteManipulationTargetIfNeeded(const FVector& TargetLocation, const FRotator& TargetRotation);
	bool AcquireHeldObject(UPrimitiveComponent* PrimitiveComponent, UManipulatableObjectComponent* Manipulatable, const FVector& GrabLocation, float InHeldDistance, const FRotator& InitialTargetRotation);
	void ClearHeldObjectState(bool bBroadcastRelease);

	UFUNCTION(Server, Reliable)
	void ServerBeginRemoteManipulation(AActor* ManipulatedActor, FVector GrabLocation, float InHeldDistance, FRotator InitialTargetRotation);

	UFUNCTION(Server, Unreliable)
	void ServerUpdateRemoteManipulationTarget(AActor* ManipulatedActor, FVector TargetLocation, FRotator TargetRotation);

	UFUNCTION(Server, Reliable)
	void ServerEndRemoteManipulation(AActor* ManipulatedActor);

	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> PhysicsHandle;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> HeldPrimitive;

	UPROPERTY(Transient)
	TObjectPtr<UManipulatableObjectComponent> HeldManipulatable;

	UPROPERTY(Transient)
	TObjectPtr<UInteractionPlacementTargetComponent> CurrentPlacementTarget;

	UPROPERTY(Transient)
	FVector HeldLocalPlanarOffset = FVector::ZeroVector;

	UPROPERTY(Transient)
	float HeldDistance = 0.0f;

	UPROPERTY(Transient)
	FRotator HeldTargetRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	bool bServerDrivenRemoteHold = false;

	UPROPERTY(Transient)
	double LastServerTargetSendTimeSeconds = -1.0;

	UPROPERTY(Transient)
	FVector LastSentServerTargetLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FRotator LastSentServerTargetRotation = FRotator::ZeroRotator;
};
