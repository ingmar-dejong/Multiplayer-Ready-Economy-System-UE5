#include "Vampire/World/BloodProcessingStation.h"

#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "Components/InputComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Items/InventoryComponent.h"
#include "Misc/SecureHash.h"
#include "Net/UnrealNetwork.h"
#include "Containers/StringConv.h"
#include "UObject/UObjectIterator.h"
#include "UnrealFramework/OwnSystemGameState.h"
#include "Vampire/Data/PlaceableStationDataAsset.h"
#include "Vampire/Data/BloodProcessingAttachmentDataAsset.h"
#include "Vampire/Data/BloodProcessingRecipeDataAsset.h"
#include "Vampire/Economy/VampireEconomyComponent.h"
#include "Vampire/Interaction/BloodProcessingInteractableComponent.h"
#include "Vampire/Interaction/InteractionPlacementTargetComponent.h"
#include "Vampire/Interaction/ManipulatableObjectComponent.h"
#include "Vampire/Items/BloodProductItem.h"
#include "Vampire/World/StationPlacementPreviewActor.h"
#include "Vampire/World/WorkspaceRoom.h"

#define LOCTEXT_NAMESPACE "BloodProcessingStation"

namespace
{
	constexpr float StationTimeUnitsPerDay = 2400.0f;

	bool HasAllRequiredTags(const FGameplayTagContainer& OwnedTags, const FGameplayTagContainer& RequiredTags)
	{
		for (const FGameplayTag& RequiredTag : RequiredTags)
		{
			if (RequiredTag.IsValid() && !OwnedTags.HasTagExact(RequiredTag))
			{
				return false;
			}
		}

		return true;
	}

	FGameplayTag FindFirstMissingTag(const FGameplayTagContainer& OwnedTags, const FGameplayTagContainer& RequiredTags)
	{
		for (const FGameplayTag& RequiredTag : RequiredTags)
		{
			if (RequiredTag.IsValid() && !OwnedTags.HasTagExact(RequiredTag))
			{
				return RequiredTag;
			}
		}

		return FGameplayTag();
	}

	const UBloodProcessingRecipeDataAsset* ResolvePrimaryRecipe(
		const UBloodProcessingRecipeDataAsset* RecipeData,
		const TArray<TObjectPtr<UBloodProcessingRecipeDataAsset>>& AvailableRecipes)
	{
		if (RecipeData)
		{
			return RecipeData;
		}

		for (const UBloodProcessingRecipeDataAsset* CandidateRecipe : AvailableRecipes)
		{
			if (CandidateRecipe)
			{
				return CandidateRecipe;
			}
		}

		return nullptr;
	}

	int32 FindNextValidRecipeIndex(const TArray<TObjectPtr<UBloodProcessingRecipeDataAsset>>& Recipes, const int32 StartIndex, const int32 Direction)
	{
		if (Recipes.IsEmpty())
		{
			return INDEX_NONE;
		}

		const int32 SafeDirection = Direction < 0 ? -1 : 1;
		const int32 RecipeCount = Recipes.Num();
		int32 CandidateIndex = FMath::Clamp(StartIndex, 0, RecipeCount - 1);

		for (int32 Attempt = 0; Attempt < RecipeCount; ++Attempt)
		{
			if (Recipes.IsValidIndex(CandidateIndex) && Recipes[CandidateIndex])
			{
				return CandidateIndex;
			}

			CandidateIndex = (CandidateIndex + SafeDirection + RecipeCount) % RecipeCount;
		}

		return INDEX_NONE;
	}

	FName BuildAttachmentMeshComponentName(const FName SlotId)
	{
		return SlotId.IsNone()
			? FName(TEXT("AttachmentMesh"))
			: FName(*FString::Printf(TEXT("AttachmentMesh_%s"), *SlotId.ToString()));
	}

	bool IsGeneratedAttachmentMeshComponent(const UStaticMeshComponent* MeshComponent)
	{
		return MeshComponent && MeshComponent->GetFName().ToString().StartsWith(TEXT("AttachmentMesh"));
	}

	float GetStationOwnSystemAccumulatedTime(const UObject* WorldContextObject)
	{
		if (!WorldContextObject)
		{
			return 0.0f;
		}

		if (const UWorld* World = WorldContextObject->GetWorld())
		{
			if (const AOwnSystemGameState* OwnSystemGameState = World->GetGameState<AOwnSystemGameState>())
			{
				return OwnSystemGameState->GetAccumulatedTime();
			}
		}

		return 0.0f;
	}

	FGuid BuildDeterministicGuidFromString(const FString& Source)
	{
		FMD5 Md5;
		FTCHARToUTF8 Utf8(*Source);
		Md5.Update(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());

		uint8 Digest[16];
		Md5.Final(Digest);

		uint32 A = 0;
		uint32 B = 0;
		uint32 C = 0;
		uint32 D = 0;
		FMemory::Memcpy(&A, Digest + 0, sizeof(uint32));
		FMemory::Memcpy(&B, Digest + 4, sizeof(uint32));
		FMemory::Memcpy(&C, Digest + 8, sizeof(uint32));
		FMemory::Memcpy(&D, Digest + 12, sizeof(uint32));
		return FGuid(A, B, C, D);
	}

	bool IsOperatorContextStale(const APawn* OperatorPawn)
	{
		return !IsValid(OperatorPawn)
			|| OperatorPawn->IsActorBeingDestroyed()
			|| OperatorPawn->IsUnreachable()
			|| OperatorPawn->GetController() == nullptr;
	}

	int32 GetBloodSourceScore(const EBloodSourceType SourceType)
	{
		switch (SourceType)
		{
		case EBloodSourceType::Rat:
			return 0;
		case EBloodSourceType::Varken:
		case EBloodSourceType::Geit:
		case EBloodSourceType::Bedelaar:
			return 1;
		case EBloodSourceType::Hert:
		case EBloodSourceType::Boer:
			return 2;
		case EBloodSourceType::Handelaar:
		case EBloodSourceType::Edelman:
		case EBloodSourceType::Priester:
			return 3;
		default:
			return 0;
		}
	}

	bool DoBloodItemsMatchForVatBatch(const UBloodProductItem* A, const UBloodProductItem* B)
	{
		return A
			&& B
			&& A->GetClass() == B->GetClass()
			&& A->SourceType == B->SourceType
			&& A->BaseQuality == B->BaseQuality
			&& A->ProcessingType == B->ProcessingType;
	}

	bool DoesBloodItemMatchStartRequest(const UBloodProductItem* Item, const FBloodProcessingStartRequest& Request)
	{
		return Item
			&& Item->GetClass() == Request.ItemClass
			&& Item->SourceType == Request.SourceType
			&& Item->BaseQuality == Request.BaseQuality
			&& Item->ProcessingType == Request.ProcessingType
			&& Item->CreatedDay == Request.CreatedDay;
	}

	int32 GetTotalMatchingUnitsInInventory(const UOwnSystemInventoryComponent* Inventory, const UBloodProductItem* SeedItem)
	{
		if (!Inventory || !SeedItem)
		{
			return 0;
		}

		int32 TotalUnits = 0;
		for (UOwnSystemItem* Item : Inventory->GetItems())
		{
			if (const UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item))
			{
				if (DoBloodItemsMatchForVatBatch(SeedItem, Candidate))
				{
					TotalUnits += Candidate->BloodQuantity;
				}
			}
		}

		return TotalUnits;
	}

	bool ConsumeMatchingUnitsFromInventory(UOwnSystemInventoryComponent* Inventory, const UBloodProductItem* SeedItem, const int32 UnitsToConsume)
	{
		if (!Inventory || !SeedItem || UnitsToConsume <= 0)
		{
			return false;
		}

		int32 RemainingUnits = UnitsToConsume;
		TArray<UOwnSystemItem*> Items = Inventory->GetItems();
		for (UOwnSystemItem* Item : Items)
		{
			UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item);
			if (!DoBloodItemsMatchForVatBatch(SeedItem, Candidate))
			{
				continue;
			}

			const int32 UnitsFromThisItem = FMath::Min(Candidate->BloodQuantity, RemainingUnits);
			Candidate->BloodQuantity -= UnitsFromThisItem;
			RemainingUnits -= UnitsFromThisItem;

			if (Candidate->BloodQuantity <= 0)
			{
				Inventory->RemoveItem(Candidate);
			}
			else
			{
				Candidate->RefreshPresentation();
			}

			if (RemainingUnits <= 0)
			{
				return true;
			}
		}

		return RemainingUnits <= 0;
	}

	void LogInteractionInputAudit(
		const TCHAR* Phase,
		const ABloodProcessingStation* Station,
		APlayerController* PlayerController,
		const UInputMappingContext* MappingContext)
	{
		const FString StationName = GetNameSafe(Station);
		const FString PlayerControllerName = GetNameSafe(PlayerController);
		const FString MappingContextName = GetNameSafe(MappingContext);
		const bool bShowMouseCursor = PlayerController ? PlayerController->ShouldShowMouseCursor() : false;

		if (!PlayerController)
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("BloodProcessingStation: InputAudit phase=%s station=%s pc=None imc=%s"),
				Phase,
				*StationName,
				*MappingContextName);
			return;
		}

		if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				int32 FoundPriority = INDEX_NONE;
				const bool bHasMappingContext = MappingContext && InputSubsystem->HasMappingContext(MappingContext, FoundPriority);
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("BloodProcessingStation: InputAudit phase=%s station=%s pc=%s imc=%s hasIMC=%s priority=%d showMouseCursor=%s"),
					Phase,
					*StationName,
					*PlayerControllerName,
					*MappingContextName,
					bHasMappingContext ? TEXT("true") : TEXT("false"),
					FoundPriority,
					bShowMouseCursor ? TEXT("true") : TEXT("false"));
				return;
			}
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("BloodProcessingStation: InputAudit phase=%s station=%s pc=%s imc=%s subsystem=None showMouseCursor=%s"),
			Phase,
			*StationName,
			*PlayerControllerName,
			*MappingContextName,
			bShowMouseCursor ? TEXT("true") : TEXT("false"));
	}

	int32 GetTotalMatchingUnitsForRequest(const UOwnSystemInventoryComponent* Inventory, const FBloodProcessingStartRequest& Request)
	{
		if (!Inventory || !Request.ItemClass)
		{
			return 0;
		}

		int32 TotalUnits = 0;
		for (UOwnSystemItem* Item : Inventory->GetItems())
		{
			if (const UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item))
			{
				if (DoesBloodItemMatchStartRequest(Candidate, Request))
				{
					TotalUnits += Candidate->BloodQuantity;
				}
			}
		}

		return TotalUnits;
	}

bool ConsumeMatchingUnitsForRequest(UOwnSystemInventoryComponent* Inventory, const FBloodProcessingStartRequest& Request)
	{
		if (!Inventory || !Request.ItemClass || Request.BloodQuantity <= 0)
		{
			return false;
		}

		int32 RemainingUnits = Request.BloodQuantity;
		TArray<UOwnSystemItem*> Items = Inventory->GetItems();
		for (UOwnSystemItem* Item : Items)
		{
			UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item);
			if (!DoesBloodItemMatchStartRequest(Candidate, Request))
			{
				continue;
			}

			const int32 UnitsFromThisItem = FMath::Min(Candidate->BloodQuantity, RemainingUnits);
			Candidate->BloodQuantity -= UnitsFromThisItem;
			RemainingUnits -= UnitsFromThisItem;

			if (Candidate->BloodQuantity <= 0)
			{
				Inventory->RemoveItem(Candidate);
			}
			else
			{
				Candidate->RefreshPresentation();
			}

			if (RemainingUnits <= 0)
			{
				return true;
			}
		}

		return RemainingUnits <= 0;
	}
}

TMap<TWeakObjectPtr<ULocalPlayer>, TMap<TObjectPtr<UInputMappingContext>, int32>> ABloodProcessingStation::SharedInteractionMappingContextRefs;

TSubclassOf<UProcessingStationMenuBase> ABloodProcessingStation::GetResolvedProcessingMenuClass() const
{
	return ProcessingMenuClass ? ProcessingMenuClass : BarrelMenuClass;
}

ABloodProcessingStation::ABloodProcessingStation()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f;
	bReplicates = true;
	SetReplicateMovement(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("InteractionCamera"));
	InteractionCamera->SetupAttachment(SceneRoot);
	InteractionCamera->SetRelativeLocation(FVector(-160.0f, 0.0f, 110.0f));
	InteractionCamera->SetRelativeRotation(FRotator(-10.0f, 0.0f, 0.0f));
	InteractionCamera->bAutoActivate = true;

	ProcessingInteractable = CreateDefaultSubobject<UBloodProcessingInteractableComponent>(TEXT("ProcessingInteractable"));
	StationDisplayName = LOCTEXT("DefaultStationName", "Houten Vat");
}

void ABloodProcessingStation::PrepareForSave_Implementation()
{
	if (!IsNetStartupActor() && !SavedActorGuid.IsValid())
	{
		SavedActorGuid = FGuid::NewGuid();
	}

	if (HasAuthority() && PlacedStationInstanceId.IsValid())
	{
		FText PlacementReason;
		EnsurePlacementRecordForCurrentTransform(PlacementReason);
	}
}

void ABloodProcessingStation::Load_Implementation()
{
	RefreshAttachmentVisuals();
	RefreshPlacementTransformFromReplicatedState();

	if (HasAuthority() && PlacedStationInstanceId.IsValid())
	{
		FText PlacementReason;
		if (!EnsurePlacementRecordForCurrentTransform(PlacementReason))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("BloodProcessingStation: Failed to restore placement record after load for station=%s reason=%s"),
				*GetNameSafe(this),
				*PlacementReason.ToString());
		}
	}

	const EBloodVatStationState PreviousState = StationState;
	const int32 SharedSimDay = FMath::Max(0, FMath::FloorToInt(GetStationOwnSystemAccumulatedTime(this) / StationTimeUnitsPerDay));
	UpdateReadyState(SharedSimDay);
	if (StationState != PreviousState)
	{
		ForceNetUpdate();
	}
}

bool ABloodProcessingStation::ShouldRespawn_Implementation() const
{
	return !IsNetStartupActor();
}

void ABloodProcessingStation::SetActorGUID_Implementation(const FGuid& SavedGUID)
{
	if (SavedGUID.IsValid())
	{
		SavedActorGuid = SavedGUID;
	}
}

FGuid ABloodProcessingStation::GetActorGUID_Implementation() const
{
	if (PlacedStationInstanceId.IsValid())
	{
		return PlacedStationInstanceId;
	}

	if (SavedActorGuid.IsValid())
	{
		return SavedActorGuid;
	}

	if (IsNetStartupActor())
	{
		return BuildDeterministicGuidFromString(GetPathName());
	}

	return FGuid();
}

bool ABloodProcessingStation::AddBloodItemFromRequestToInventory(
	UOwnSystemInventoryComponent* Inventory,
	const FBloodProcessingStartRequest& Request,
	const EBloodProcessingType ResultProcessingType,
	const bool bMarkAsPackaged,
	const EBloodProcessingType SourceProcessingType,
	UBloodProductItem*& OutInventoryItem,
	FText& OutReason) const
{
	OutInventoryItem = nullptr;
	OutReason = FText::GetEmpty();

	if (!Inventory || !Request.ItemClass)
	{
		OutReason = LOCTEXT("ManualRestoreMissingInventory", "Inventory of batchdata ontbreekt voor deze actie.");
		return false;
	}

	const FItemAddResult AddResult = Inventory->TryAddItemFromClass(Request.ItemClass, 1, false);
	if (AddResult.AmountGiven <= 0 || AddResult.Stacks.IsEmpty())
	{
		OutReason = LOCTEXT("ManualRestoreAddFailed", "Het blood item kon niet aan de inventory worden toegevoegd.");
		return false;
	}

	UBloodProductItem* InventoryItem = Cast<UBloodProductItem>(AddResult.Stacks[0]);
	if (!InventoryItem)
	{
		OutReason = LOCTEXT("ManualRestoreWrongItemClass", "De inventory output heeft een ongeldige item class.");
		return false;
	}

	InventoryItem->SourceType = Request.SourceType;
	InventoryItem->BaseQuality = Request.BaseQuality;
	InventoryItem->ProcessingType = ResultProcessingType;
	InventoryItem->bHasPackagedSourceProcessing = bMarkAsPackaged;
	InventoryItem->PackagedSourceProcessingType = bMarkAsPackaged ? SourceProcessingType : EBloodProcessingType::Vers;
	InventoryItem->BloodQuantity = Request.BloodQuantity;
	InventoryItem->CreatedDay = Request.CreatedDay;
	InventoryItem->RefreshPresentation();

	OutInventoryItem = InventoryItem;
	return true;
}

void ABloodProcessingStation::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ABloodProcessingStation, AttachmentSlots);
	DOREPLIFETIME(ABloodProcessingStation, StationUnlockTags);
	DOREPLIFETIME(ABloodProcessingStation, SelectedRecipeIndex);
	DOREPLIFETIME(ABloodProcessingStation, StationState);
	DOREPLIFETIME(ABloodProcessingStation, StoredBatch);
	DOREPLIFETIME(ABloodProcessingStation, ProcessingStartDay);
	DOREPLIFETIME(ABloodProcessingStation, ProcessingReadyDay);
	DOREPLIFETIME(ABloodProcessingStation, ProcessingStartAccumulatedTime);
	DOREPLIFETIME(ABloodProcessingStation, ProcessingReadyAccumulatedTime);
	DOREPLIFETIME(ABloodProcessingStation, bHasStagedManualStartRequest);
	DOREPLIFETIME(ABloodProcessingStation, StagedManualStartRequest);
	DOREPLIFETIME(ABloodProcessingStation, SpawnedManualPreviewActor);
	DOREPLIFETIME(ABloodProcessingStation, CurrentOperator);
	DOREPLIFETIME(ABloodProcessingStation, bMovePlacementInProgress);
	DOREPLIFETIME(ABloodProcessingStation, MovePreviewState);
	DOREPLIFETIME(ABloodProcessingStation, PlacedStationInstanceId);
	DOREPLIFETIME(ABloodProcessingStation, OwningRoomId);
	DOREPLIFETIME(ABloodProcessingStation, GridAnchorCell);
	DOREPLIFETIME(ABloodProcessingStation, RotationQuarterTurns);
}

bool ABloodProcessingStation::RequiresManualProcessingFlow() const
{
	return false;
}

void ABloodProcessingStation::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshAttachmentVisuals();
}

void ABloodProcessingStation::BeginPlay()
{
	Super::BeginPlay();
	RefreshAttachmentVisuals();
	BootstrapEditorPlacedStationPlacement();
	RefreshPlacementTransformFromReplicatedState();

	if (RequiresManualProcessingFlow())
	{
		FText ManualPreviewValidationReason;
		if (!ValidateManualPreviewSetup(ManualPreviewValidationReason))
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("BloodProcessingStation: Manual preview setup invalid for station=%s reason=%s"),
				*GetNameSafe(this),
				*ManualPreviewValidationReason.ToString());
		}
	}

	ResolvedManualPlacementTarget = ResolveManualPlacementTarget();
	if (ResolvedManualPlacementTarget)
	{
		ResolvedManualPlacementTarget->OnValidObjectPlaced.RemoveDynamic(this, &ABloodProcessingStation::HandleManualPlacementConfirmed);
		ResolvedManualPlacementTarget->OnValidObjectPlaced.AddDynamic(this, &ABloodProcessingStation::HandleManualPlacementConfirmed);
	}
	else if (!RequiresManualProcessingFlow())
	{
		UE_LOG(LogTemp, Warning, TEXT("BloodProcessingStation: No manual placement target resolved for station=%s"), *GetNameSafe(this));
	}

	OnRep_MovePlacementState();
}

void ABloodProcessingStation::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!HasAuthority())
	{
		return;
	}

	// Release abandoned locks so stations do not remain blocked after disconnect/unpossess.
	if (CurrentOperator && IsOperatorContextStale(CurrentOperator.Get()))
	{
		HandleAbandonedManualFlow();
		ReleaseOperator(nullptr);
	}

	if (StationState != EBloodVatStationState::Rijpt)
	{
		return;
	}

	const EBloodVatStationState PreviousState = StationState;
	const int32 SharedSimDay = FMath::Max(0, FMath::FloorToInt(GetStationOwnSystemAccumulatedTime(this) / StationTimeUnitsPerDay));
	UpdateReadyState(SharedSimDay);
	if (StationState != PreviousState)
	{
		ForceNetUpdate();
	}
}

void ABloodProcessingStation::UpdateReadyState(const int32 CurrentSimDay)
{
	if (StationState != EBloodVatStationState::Rijpt)
	{
		return;
	}

	const float CurrentAccumulatedTime = GetStationOwnSystemAccumulatedTime(this);
	if (ProcessingReadyAccumulatedTime >= 0.0f && CurrentAccumulatedTime >= ProcessingReadyAccumulatedTime)
	{
		StationState = EBloodVatStationState::Klaar;
		return;
	}

	if (ProcessingReadyDay >= 0 && CurrentSimDay >= ProcessingReadyDay)
	{
		StationState = EBloodVatStationState::Klaar;
	}
}

const UBloodProcessingRecipeDataAsset* ABloodProcessingStation::GetActiveRecipe() const
{
	return ResolvePrimaryRecipe(RecipeData, AvailableRecipes);
}

int32 ABloodProcessingStation::GetRecipeCount() const
{
	return GetActiveRecipe() ? 1 : 0;
}

int32 ABloodProcessingStation::GetSelectedRecipeIndex() const
{
	return GetActiveRecipe() ? 0 : INDEX_NONE;
}

const UBloodProcessingRecipeDataAsset* ABloodProcessingStation::GetRecipeAtIndex(const int32 RecipeIndex) const
{
	return RecipeIndex == 0 ? GetActiveRecipe() : nullptr;
}

void ABloodProcessingStation::SetSelectedRecipeIndex(const int32 NewRecipeIndex)
{
	(void)NewRecipeIndex;
	SelectedRecipeIndex = 0;
}

void ABloodProcessingStation::StepSelectedRecipe(const int32 Direction)
{
	(void)Direction;
	SelectedRecipeIndex = 0;
}

bool ABloodProcessingStation::HasInstalledAttachmentTag(const FGameplayTag Tag) const
{
	return Tag.IsValid() && GetInstalledAttachmentTags().HasTagExact(Tag);
}

void ABloodProcessingStation::AddStationUnlockTag(const FGameplayTag Tag)
{
	if (Tag.IsValid())
	{
		StationUnlockTags.AddTag(Tag);
	}
}

void ABloodProcessingStation::RemoveStationUnlockTag(const FGameplayTag Tag)
{
	if (Tag.IsValid())
	{
		StationUnlockTags.RemoveTag(Tag);
	}
}

FGameplayTagContainer ABloodProcessingStation::GetInstalledAttachmentTags() const
{
	FGameplayTagContainer Result;

	for (const FProcessingStationAttachmentSlot& Slot : AttachmentSlots)
	{
		if (Slot.PlacedAttachment && Slot.PlacedAttachment->AttachmentTag.IsValid())
		{
			Result.AddTag(Slot.PlacedAttachment->AttachmentTag);
		}
	}

	return Result;
}

bool ABloodProcessingStation::PlaceAttachmentInSlotByIndex(const int32 SlotIndex, UBloodProcessingAttachmentDataAsset* AttachmentData, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!AttachmentSlots.IsValidIndex(SlotIndex))
	{
		OutReason = LOCTEXT("InvalidAttachmentSlot", "Ongeldige attachment slot.");
		return false;
	}

	if (!AttachmentData)
	{
		OutReason = LOCTEXT("MissingAttachmentData", "Geen attachment data opgegeven.");
		return false;
	}

	FProcessingStationAttachmentSlot& Slot = AttachmentSlots[SlotIndex];
	if (!CanSlotAcceptAttachment(Slot, AttachmentData, OutReason))
	{
		return false;
	}

	Slot.PlacedAttachment = AttachmentData;
	RefreshAttachmentVisualForSlot(Slot);

	OutReason = FText::Format(
		LOCTEXT("AttachmentPlacedFmt", "{0} geplaatst in slot {1}."),
		AttachmentData->DisplayName.IsEmpty() ? FText::FromName(AttachmentData->AttachmentId) : AttachmentData->DisplayName,
		Slot.SlotDisplayName.IsEmpty() ? FText::FromName(Slot.SlotId) : Slot.SlotDisplayName);
	return true;
}

bool ABloodProcessingStation::RemoveAttachmentFromSlotByIndex(const int32 SlotIndex, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!AttachmentSlots.IsValidIndex(SlotIndex))
	{
		OutReason = LOCTEXT("InvalidAttachmentSlotRemove", "Ongeldige attachment slot.");
		return false;
	}

	FProcessingStationAttachmentSlot& Slot = AttachmentSlots[SlotIndex];
	if (!Slot.PlacedAttachment)
	{
		OutReason = LOCTEXT("NoAttachmentInSlot", "Er is geen attachment geplaatst in dit slot.");
		return false;
	}

	const FText RemovedName = Slot.PlacedAttachment->DisplayName.IsEmpty()
		? FText::FromName(Slot.PlacedAttachment->AttachmentId)
		: Slot.PlacedAttachment->DisplayName;
	Slot.PlacedAttachment = nullptr;
	DestroyAttachmentVisual(Slot);

	OutReason = FText::Format(
		LOCTEXT("AttachmentRemovedFmt", "{0} verwijderd uit slot {1}."),
		RemovedName,
		Slot.SlotDisplayName.IsEmpty() ? FText::FromName(Slot.SlotId) : Slot.SlotDisplayName);
	return true;
}

const UBloodProcessingAttachmentDataAsset* ABloodProcessingStation::GetPlacedAttachmentInSlot(const int32 SlotIndex) const
{
	return AttachmentSlots.IsValidIndex(SlotIndex) ? AttachmentSlots[SlotIndex].PlacedAttachment : nullptr;
}

bool ABloodProcessingStation::DoesRecipeMeetStationRequirements(const UBloodProcessingRecipeDataAsset* Recipe, FText& OutReason) const
{
	OutReason = FText::GetEmpty();

	if (!Recipe)
	{
		OutReason = LOCTEXT("RecipeMissing", "Geen actieve recipe geselecteerd.");
		return false;
	}

	const FGameplayTagContainer InstalledAttachmentTags = GetInstalledAttachmentTags();
	if (!HasAllRequiredTags(InstalledAttachmentTags, Recipe->RequiredStationTags))
	{
		const FGameplayTag MissingTag = FindFirstMissingTag(InstalledAttachmentTags, Recipe->RequiredStationTags);

		OutReason = FText::Format(
			LOCTEXT("RecipeMissingStationTag", "Station mist vereiste upgrade of attachment: {0}."),
			MissingTag.IsValid() ? FText::FromString(MissingTag.ToString()) : LOCTEXT("UnknownStationTag", "Onbekend"));
		return false;
	}

	if (!HasAllRequiredTags(StationUnlockTags, Recipe->RequiredUnlockTags))
	{
		const FGameplayTag MissingUnlock = FindFirstMissingTag(StationUnlockTags, Recipe->RequiredUnlockTags);

		OutReason = FText::Format(
			LOCTEXT("RecipeMissingUnlockTag", "Recipe nog niet vrijgespeeld: ontbreekt {0}."),
			MissingUnlock.IsValid() ? FText::FromString(MissingUnlock.ToString()) : LOCTEXT("UnknownUnlockTag", "Onbekend"));
		return false;
	}

	return true;
}

int32 ABloodProcessingStation::GetProcessingDurationDays() const
{
	const UBloodProcessingRecipeDataAsset* ActiveRecipe = GetActiveRecipe();
	return ActiveRecipe ? FMath::Max(0, ActiveRecipe->ProcessingDurationDays) : 0;
}

int32 ABloodProcessingStation::GetProcessingDurationDaysForSource(const EBloodSourceType SourceType) const
{
	const int32 BaseDuration = GetProcessingDurationDays();
	const UBloodProcessingRecipeDataAsset* ActiveRecipe = GetActiveRecipe();
	if (!ActiveRecipe)
	{
		return BaseDuration;
	}

	return BaseDuration + GetBloodSourceScore(SourceType) * FMath::Max(0, ActiveRecipe->AdditionalDurationPerSourceScore);
}

int32 ABloodProcessingStation::GetProcessingDurationDaysForBlood(const UBloodProductItem* BloodItem) const
{
	return BloodItem ? GetProcessingDurationDaysForSource(BloodItem->SourceType) : GetProcessingDurationDays();
}

int32 ABloodProcessingStation::GetStoredBatchProcessingDurationDays() const
{
	return GetProcessingDurationDaysForSource(StoredBatch.SourceType);
}

void ABloodProcessingStation::ApplyInteractionInputContext(APlayerController* PlayerController)
{
	const FString StationName = GetName();
	const FString PlayerControllerName = PlayerController ? PlayerController->GetName() : TEXT("None");
	const FString MappingContextName = InteractionInputMappingContext ? InteractionInputMappingContext.Get()->GetName() : TEXT("None");
	LogInteractionInputAudit(TEXT("BeforeApplyInteractionIMC"), this, PlayerController, InteractionInputMappingContext);

	if (!PlayerController || !InteractionInputMappingContext)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("BloodProcessingStation: Skip ApplyInteractionInputContext for %s (PC=%s, IMC=%s)"),
			*StationName,
			*PlayerControllerName,
			*MappingContextName);
		return;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			TMap<TObjectPtr<UInputMappingContext>, int32>& MappingRefs = SharedInteractionMappingContextRefs.FindOrAdd(LocalPlayer);
			int32& RefCount = MappingRefs.FindOrAdd(InteractionInputMappingContext);
			if (RefCount == 0)
			{
				InputSubsystem->AddMappingContext(InteractionInputMappingContext, InteractionInputMappingPriority);
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("BloodProcessingStation: Added interaction IMC %s to %s at priority %d"),
					*MappingContextName,
					*PlayerControllerName,
					InteractionInputMappingPriority);
			}
			else
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("BloodProcessingStation: Reused interaction IMC %s for %s (refcount now %d)"),
					*MappingContextName,
					*PlayerControllerName,
					RefCount + 1);
			}

			++RefCount;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BloodProcessingStation: No EnhancedInput subsystem found while adding IMC for %s"), *GetNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BloodProcessingStation: No LocalPlayer found while adding IMC for %s"), *GetNameSafe(this));
	}

	LogInteractionInputAudit(TEXT("AfterApplyInteractionIMC"), this, PlayerController, InteractionInputMappingContext);

	if (InteractionHotkeyPlayerController && InteractionHotkeyPlayerController != PlayerController)
	{
		RemoveInteractionInputContext(InteractionHotkeyPlayerController);
	}

	if (!InteractionHotkeyInputComponent)
	{
		InteractionHotkeyInputComponent = NewObject<UInputComponent>(PlayerController, TEXT("StationInteractionHotkeys"));
	}

	if (InteractionHotkeyInputComponent)
	{
		if (InteractionHotkeyPlayerController == PlayerController)
		{
			PlayerController->PopInputComponent(InteractionHotkeyInputComponent);
		}

		InteractionHotkeyInputComponent->ClearActionBindings();
		InteractionHotkeyInputComponent->Priority = InteractionInputMappingPriority + 1;
		InteractionHotkeyInputComponent->bBlockInput = false;
		InteractionHotkeyInputComponent->BindKey(EKeys::Delete, IE_Pressed, this, &ABloodProcessingStation::HandleInteractionCancelPressed);
		InteractionHotkeyInputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &ABloodProcessingStation::HandleInteractionCancelPressed);
		InteractionHotkeyInputComponent->BindKey(EKeys::F10, IE_Pressed, this, &ABloodProcessingStation::HandleDebugForceOperatorUnpossessPressed);
		PlayerController->PushInputComponent(InteractionHotkeyInputComponent);
		InteractionHotkeyPlayerController = PlayerController;
	}
}

void ABloodProcessingStation::RemoveInteractionInputContext(APlayerController* PlayerController)
{
	const FString StationName = GetName();
	const FString PlayerControllerName = PlayerController ? PlayerController->GetName() : TEXT("None");
	const FString MappingContextName = InteractionInputMappingContext ? InteractionInputMappingContext.Get()->GetName() : TEXT("None");
	LogInteractionInputAudit(TEXT("BeforeRemoveInteractionIMC"), this, PlayerController, InteractionInputMappingContext);

	if (!PlayerController || !InteractionInputMappingContext)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("BloodProcessingStation: Skip RemoveInteractionInputContext for %s (PC=%s, IMC=%s)"),
			*StationName,
			*PlayerControllerName,
			*MappingContextName);
		return;
	}

	if (ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer())
	{
		if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
		{
			if (TMap<TObjectPtr<UInputMappingContext>, int32>* MappingRefs = SharedInteractionMappingContextRefs.Find(LocalPlayer))
			{
				if (int32* RefCount = MappingRefs->Find(InteractionInputMappingContext))
				{
					*RefCount = FMath::Max(0, *RefCount - 1);
					if (*RefCount == 0)
					{
						InputSubsystem->RemoveMappingContext(InteractionInputMappingContext);
						MappingRefs->Remove(InteractionInputMappingContext);
						if (MappingRefs->IsEmpty())
						{
							SharedInteractionMappingContextRefs.Remove(LocalPlayer);
						}

						UE_LOG(
							LogTemp,
							Warning,
							TEXT("BloodProcessingStation: Removed interaction IMC %s from %s"),
							*MappingContextName,
							*PlayerControllerName);
					}
					else
					{
						UE_LOG(
							LogTemp,
							Warning,
							TEXT("BloodProcessingStation: Kept interaction IMC %s for %s (refcount now %d)"),
							*MappingContextName,
							*PlayerControllerName,
							*RefCount);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("BloodProcessingStation: No EnhancedInput subsystem found while removing IMC for %s"), *GetNameSafe(this));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("BloodProcessingStation: No LocalPlayer found while removing IMC for %s"), *GetNameSafe(this));
	}

	LogInteractionInputAudit(TEXT("AfterRemoveInteractionIMC"), this, PlayerController, InteractionInputMappingContext);

	RemoveInteractionHotkeys(PlayerController);
}

void ABloodProcessingStation::RemoveInteractionHotkeys(APlayerController* PlayerController)
{
	if (InteractionHotkeyInputComponent && InteractionHotkeyPlayerController == PlayerController && PlayerController)
	{
		PlayerController->PopInputComponent(InteractionHotkeyInputComponent);
		InteractionHotkeyPlayerController = nullptr;
	}
}

void ABloodProcessingStation::SetActiveInteractorContext(APawn* InInteractor)
{
	ActiveInteractor = InInteractor;
}

void ABloodProcessingStation::ClearActiveInteractorContext()
{
	ActiveInteractor = nullptr;
}

bool ABloodProcessingStation::TryClaimOperator(APawn* InInteractor, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!InInteractor)
	{
		OutReason = LOCTEXT("ClaimOperatorMissingInteractor", "Geen geldige speler gevonden voor dit station.");
		return false;
	}

	if (bMovePlacementInProgress && CurrentOperator != InInteractor)
	{
		OutReason = LOCTEXT("ClaimOperatorMoveBusy", "Dit station wordt momenteel door een andere speler verplaatst.");
		return false;
	}

	if (StationState == EBloodVatStationState::Rijpt && (!CurrentOperator || CurrentOperator != InInteractor))
	{
		OutReason = LOCTEXT("ClaimOperatorProcessingBusy", "Dit station is momenteel bezig met verwerken en kan nu door niemand anders bediend worden.");
		return false;
	}

	if (CurrentOperator && CurrentOperator != InInteractor)
	{
		OutReason = StationState == EBloodVatStationState::Rijpt
			? LOCTEXT("ClaimOperatorBusyProcessing", "Dit station is al in gebruik en verwerkt momenteel een batch.")
			: LOCTEXT("ClaimOperatorBusy", "Dit station wordt al door een andere speler bediend.");
		return false;
	}

	CurrentOperator = InInteractor;
	ActiveInteractor = InInteractor;
	ForceNetUpdate();
	return true;
}

void ABloodProcessingStation::ReleaseOperator(APawn* InInteractor)
{
	if (!CurrentOperator)
	{
		return;
	}

	if (InInteractor && CurrentOperator != InInteractor)
	{
		return;
	}

	CurrentOperator = nullptr;
	if (!InInteractor || ActiveInteractor == InInteractor)
	{
		ActiveInteractor = nullptr;
	}
	ForceNetUpdate();
}

bool ABloodProcessingStation::IsCurrentOperator(const APawn* InInteractor) const
{
	return InInteractor && CurrentOperator == InInteractor;
}

bool ABloodProcessingStation::TryBeginMovePlacement(APawn* InInteractor, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!TryClaimOperator(InInteractor, OutReason))
	{
		return false;
	}

	bMovePlacementInProgress = true;
	MovePreviewState = FProcessingStationMovePreviewState();
	MovePreviewState.bActive = true;
	OnRep_MovePlacementState();
	OnRep_MovePreviewState();
	ForceNetUpdate();
	return true;
}

void ABloodProcessingStation::EndMovePlacement(APawn* InInteractor)
{
	if (!bMovePlacementInProgress)
	{
		return;
	}

	if (InInteractor && CurrentOperator && CurrentOperator != InInteractor)
	{
		return;
	}

	bMovePlacementInProgress = false;
	MovePreviewState = FProcessingStationMovePreviewState();
	OnRep_MovePlacementState();
	OnRep_MovePreviewState();
	ReleaseOperator(InInteractor);
	ForceNetUpdate();
}

void ABloodProcessingStation::UpdateMovePlacementPreview(
	AWorkspaceRoom* PreviewWorkspaceRoom,
	const FIntPoint PreviewAnchorCell,
	const int32 PreviewRotationQuarterTurns,
	const bool bPreviewIsValid,
	UPlaceableStationDataAsset* PreviewStationDefinition)
{
	if (!HasAuthority() || !bMovePlacementInProgress)
	{
		return;
	}

	MovePreviewState.bActive = PreviewWorkspaceRoom != nullptr && PreviewStationDefinition != nullptr;
	MovePreviewState.bValidPlacement = bPreviewIsValid;
	MovePreviewState.RoomId = PreviewWorkspaceRoom ? PreviewWorkspaceRoom->RoomId : NAME_None;
	MovePreviewState.WorkspaceRoom = PreviewWorkspaceRoom;
	MovePreviewState.AnchorCell = PreviewAnchorCell;
	MovePreviewState.RotationQuarterTurns = PreviewRotationQuarterTurns;
	MovePreviewState.StationDefinition = PreviewStationDefinition;
	OnRep_MovePreviewState();
	ForceNetUpdate();
}

void ABloodProcessingStation::SetPlacementContext(AWorkspaceRoom* InWorkspaceRoom, const FGuid& InPlacedStationInstanceId, const FName& InOwningRoomId, const FIntPoint InGridAnchorCell, const int32 InRotationQuarterTurns)
{
	OwningWorkspaceRoom = InWorkspaceRoom;
	PlacedStationInstanceId = InPlacedStationInstanceId;
	if (InPlacedStationInstanceId.IsValid())
	{
		SavedActorGuid = InPlacedStationInstanceId;
	}
	OwningRoomId = InOwningRoomId;
	GridAnchorCell = InGridAnchorCell;
	RotationQuarterTurns = InRotationQuarterTurns;
	RefreshPlacementTransformFromReplicatedState();
}

AWorkspaceRoom* ABloodProcessingStation::GetOwningWorkspaceRoom() const
{
	if (AWorkspaceRoom* WorkspaceRoom = OwningWorkspaceRoom.Get())
	{
		if (OwningRoomId.IsNone() || WorkspaceRoom->RoomId == OwningRoomId)
		{
			return WorkspaceRoom;
		}
	}

	return ResolveOwningWorkspaceRoomFromReplicatedState();
}

bool ABloodProcessingStation::HasRegisteredPlacementContext() const
{
	if (!PlacedStationInstanceId.IsValid())
	{
		return false;
	}

	if (AWorkspaceRoom* WorkspaceRoom = GetOwningWorkspaceRoom())
	{
		return WorkspaceRoom->FindPlacementRecordByInstanceId(PlacedStationInstanceId) != nullptr;
	}

	return false;
}

AWorkspaceRoom* ABloodProcessingStation::ResolveOwningWorkspaceRoomFromReplicatedState() const
{
	if (!GetWorld())
	{
		return nullptr;
	}

	for (TActorIterator<AWorkspaceRoom> It(GetWorld()); It; ++It)
	{
		AWorkspaceRoom* WorkspaceRoom = *It;
		if (!WorkspaceRoom)
		{
			continue;
		}

		if (!OwningRoomId.IsNone() && WorkspaceRoom->RoomId == OwningRoomId)
		{
			const_cast<ABloodProcessingStation*>(this)->OwningWorkspaceRoom = WorkspaceRoom;
			return WorkspaceRoom;
		}
	}

	if (!PlacedStationInstanceId.IsValid())
	{
		return nullptr;
	}

	for (TActorIterator<AWorkspaceRoom> It(GetWorld()); It; ++It)
	{
		AWorkspaceRoom* WorkspaceRoom = *It;
		if (!WorkspaceRoom)
		{
			continue;
		}

		const bool bHasMatchingPlacementRecord = WorkspaceRoom->GetPlacedStations().ContainsByPredicate(
			[this](const FWorkspacePlacedStationRecord& Record)
			{
				return Record.StationInstanceId == PlacedStationInstanceId;
			});
		if (!bHasMatchingPlacementRecord)
		{
			continue;
		}

		const_cast<ABloodProcessingStation*>(this)->OwningWorkspaceRoom = WorkspaceRoom;
		return WorkspaceRoom;
	}

	return nullptr;
}

void ABloodProcessingStation::RefreshPlacementTransformFromReplicatedState()
{
	AWorkspaceRoom* WorkspaceRoom = GetOwningWorkspaceRoom();
	if (!WorkspaceRoom)
	{
		return;
	}

	SetActorLocationAndRotation(
		WorkspaceRoom->GetCellWorldLocation(GridAnchorCell),
		WorkspaceRoom->GetPlacementWorldRotation(RotationQuarterTurns));
}

AWorkspaceRoom* ABloodProcessingStation::ResolveWorkspaceRoomById(const FName RoomIdToFind) const
{
	if (RoomIdToFind.IsNone() || !GetWorld())
	{
		return nullptr;
	}

	for (TActorIterator<AWorkspaceRoom> It(GetWorld()); It; ++It)
	{
		AWorkspaceRoom* WorkspaceRoom = *It;
		if (WorkspaceRoom && WorkspaceRoom->RoomId == RoomIdToFind)
		{
			return WorkspaceRoom;
		}
	}

	return nullptr;
}

void ABloodProcessingStation::SyncMovePreviewActor()
{
	if (!MovePreviewState.bActive || !MovePreviewState.StationDefinition || !GetWorld())
	{
		DestroyMovePreviewActor();
		return;
	}

	if (CurrentOperator)
	{
		if (APlayerController* LocalPlayerController = GetWorld()->GetFirstPlayerController())
		{
			if (LocalPlayerController->IsLocalController() && LocalPlayerController->GetPawn() == CurrentOperator)
			{
				DestroyMovePreviewActor();
				return;
			}
		}
	}

	AWorkspaceRoom* PreviewWorkspaceRoom = MovePreviewState.WorkspaceRoom
		? MovePreviewState.WorkspaceRoom.Get()
		: ResolveWorkspaceRoomById(MovePreviewState.RoomId);
	if (!PreviewWorkspaceRoom)
	{
		DestroyMovePreviewActor();
		return;
	}

	if (!MovePreviewActor)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		MovePreviewActor = GetWorld()->SpawnActor<AStationPlacementPreviewActor>(
			AStationPlacementPreviewActor::StaticClass(),
			PreviewWorkspaceRoom->GetCellWorldLocation(MovePreviewState.AnchorCell),
			PreviewWorkspaceRoom->GetPlacementWorldRotation(MovePreviewState.RotationQuarterTurns),
			SpawnParameters);
	}

	if (!MovePreviewActor)
	{
		return;
	}

	UStaticMesh* PreviewMesh = MovePreviewState.StationDefinition->PreviewMesh.LoadSynchronous();
	MovePreviewActor->SetActorLocationAndRotation(
		PreviewWorkspaceRoom->GetCellWorldLocation(MovePreviewState.AnchorCell),
		PreviewWorkspaceRoom->GetPlacementWorldRotation(MovePreviewState.RotationQuarterTurns));
	MovePreviewActor->ConfigurePreview(
		PreviewMesh,
		MovePreviewState.bValidPlacement);
}

void ABloodProcessingStation::DestroyMovePreviewActor()
{
	if (MovePreviewActor)
	{
		MovePreviewActor->Destroy();
		MovePreviewActor = nullptr;
	}
}

void ABloodProcessingStation::OnRep_PlacementState()
{
	RefreshPlacementTransformFromReplicatedState();
}

void ABloodProcessingStation::OnRep_MovePlacementState()
{
	if (ProcessingInteractable)
	{
		ProcessingInteractable->SetSuppressedByMovePlacement(bMovePlacementInProgress);
	}

	if (!bMovePlacementInProgress)
	{
		DestroyMovePreviewActor();
	}
}

void ABloodProcessingStation::OnRep_MovePreviewState()
{
	SyncMovePreviewActor();
}

UInteractionPlacementTargetComponent* ABloodProcessingStation::ResolveManualPlacementTarget() const
{
	if (UActorComponent* ReferencedComponent = ManualPlacementTarget.GetComponent(const_cast<ABloodProcessingStation*>(this)))
	{
		if (UInteractionPlacementTargetComponent* PlacementTarget = Cast<UInteractionPlacementTargetComponent>(ReferencedComponent))
		{
			return PlacementTarget;
		}
	}

	TArray<UInteractionPlacementTargetComponent*> PlacementTargets;
	GetComponents<UInteractionPlacementTargetComponent>(PlacementTargets);
	for (UInteractionPlacementTargetComponent* PlacementTarget : PlacementTargets)
	{
		if (PlacementTarget)
		{
			return PlacementTarget;
		}
	}

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("BloodProcessingStation: ResolveManualPlacementTarget station=%s no referenced component"),
		*GetNameSafe(this));
	return nullptr;
}

USceneComponent* ABloodProcessingStation::ResolveManualPreviewSpawnPoint() const
{
	if (!ManualPreviewSpawnPointComponentName.IsNone())
	{
		TArray<USceneComponent*> SceneComponents;
		GetComponents<USceneComponent>(SceneComponents);
		for (USceneComponent* SceneComponent : SceneComponents)
		{
			if (SceneComponent
				&& SceneComponent != GetRootComponent()
				&& SceneComponent->GetFName() == ManualPreviewSpawnPointComponentName)
			{
				return SceneComponent;
			}
		}

		UE_LOG(
			LogTemp,
			Warning,
			TEXT("BloodProcessingStation: ManualPreviewSpawnPointComponentName did not match any scene component station=%s componentName=%s"),
			*GetNameSafe(this),
			*ManualPreviewSpawnPointComponentName.ToString());
	}

	if (UActorComponent* ReferencedComponent = ManualPreviewSpawnPoint.GetComponent(const_cast<ABloodProcessingStation*>(this)))
	{
		if (USceneComponent* ReferencedSceneComponent = Cast<USceneComponent>(ReferencedComponent))
		{
			if (ReferencedSceneComponent != GetRootComponent())
			{
				return ReferencedSceneComponent;
			}
		}
	}

	TArray<USceneComponent*> SceneComponents;
	GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (!SceneComponent || SceneComponent == GetRootComponent())
		{
			continue;
		}

		const FString ComponentName = SceneComponent->GetName();
		if (ComponentName.Contains(TEXT("JugSpawn"), ESearchCase::IgnoreCase)
			|| ComponentName.Contains(TEXT("PreviewSpawn"), ESearchCase::IgnoreCase)
			|| ComponentName.Contains(TEXT("ManualPreview"), ESearchCase::IgnoreCase)
			|| ComponentName.Contains(TEXT("SpawnPoint"), ESearchCase::IgnoreCase))
		{
			return SceneComponent;
		}
	}

	return nullptr;
}

void ABloodProcessingStation::BootstrapEditorPlacedStationPlacement()
{
	if (!HasAuthority() || !bRegisterAsPlacedStationOnBeginPlay || HasRegisteredPlacementContext() || !GetWorld())
	{
		return;
	}

	FText BootstrapReason;
	if (!EnsurePlacementRecordForCurrentTransform(BootstrapReason))
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("BloodProcessingStation: Failed to register level-authored placed station=%s reason=%s"),
			*GetNameSafe(this),
			*BootstrapReason.ToString());
	}
}

const UPlaceableStationDataAsset* ABloodProcessingStation::ResolvePlacementDefinitionForBootstrap() const
{
	if (PlacementDefinitionOverride)
	{
		return PlacementDefinitionOverride;
	}

	for (TObjectIterator<UPlaceableStationDataAsset> It; It; ++It)
	{
		const UPlaceableStationDataAsset* PlacementDefinition = *It;
		if (!PlacementDefinition || !PlacementDefinition->PlacedActorClass)
		{
			continue;
		}

		if (GetClass()->IsChildOf(PlacementDefinition->PlacedActorClass))
		{
			return PlacementDefinition;
		}
	}

	return nullptr;
}

bool ABloodProcessingStation::TryBuildPlacementRecordForWorkspace(
	AWorkspaceRoom* CandidateWorkspaceRoom,
	FWorkspacePlacedStationRecord& OutPlacementRecord,
	FText& OutReason) const
{
	OutPlacementRecord = FWorkspacePlacedStationRecord();
	OutReason = FText::GetEmpty();

	if (!CandidateWorkspaceRoom)
	{
		OutReason = LOCTEXT("BuildPlacementMissingWorkspace", "Geen geldige werkruimte beschikbaar voor stationregistratie.");
		return false;
	}

	if (CandidateWorkspaceRoom->CellSize <= 0.0f)
	{
		OutReason = LOCTEXT("BuildPlacementInvalidWorkspace", "De werkruimte heeft een ongeldige cell size.");
		return false;
	}

	const UPlaceableStationDataAsset* PlacementDefinition = ResolvePlacementDefinitionForBootstrap();
	if (!PlacementDefinition)
	{
		OutReason = LOCTEXT("BuildPlacementMissingDefinition", "Geen placeable stationdefinitie gevonden voor dit station.");
		return false;
	}

	const FVector LocalLocation = CandidateWorkspaceRoom->GetActorTransform().InverseTransformPosition(GetActorLocation());
	const FIntPoint AnchorCell(
		FMath::RoundToInt(LocalLocation.X / CandidateWorkspaceRoom->CellSize),
		FMath::RoundToInt(LocalLocation.Y / CandidateWorkspaceRoom->CellSize));
	const int32 LocalRotationTurns = ((FMath::RoundToInt((GetActorRotation().Yaw - CandidateWorkspaceRoom->GetActorRotation().Yaw) / 90.0f) % 4) + 4) % 4;
	const FGuid StationInstanceId = PlacedStationInstanceId.IsValid() ? PlacedStationInstanceId : FGuid::NewGuid();

	if (!CandidateWorkspaceRoom->ValidatePlacement(PlacementDefinition, AnchorCell, LocalRotationTurns, OutReason, &StationInstanceId))
	{
		return false;
	}

	OutPlacementRecord.StationInstanceId = StationInstanceId;
	OutPlacementRecord.StationDefinition = const_cast<UPlaceableStationDataAsset*>(PlacementDefinition);
	OutPlacementRecord.AnchorCell = AnchorCell;
	OutPlacementRecord.RotationQuarterTurns = LocalRotationTurns;
	return true;
}

bool ABloodProcessingStation::InferPlacementStateFromCurrentTransform(
	AWorkspaceRoom*& OutWorkspaceRoom,
	FIntPoint& OutAnchorCell,
	int32& OutRotationQuarterTurns,
	const UPlaceableStationDataAsset*& OutPlacementDefinition,
	FText& OutReason) const
{
	OutWorkspaceRoom = nullptr;
	OutAnchorCell = FIntPoint::ZeroValue;
	OutRotationQuarterTurns = 0;
	OutPlacementDefinition = ResolvePlacementDefinitionForBootstrap();
	OutReason = FText::GetEmpty();

	if (!GetWorld())
	{
		OutReason = LOCTEXT("InferPlacementNoWorld", "Wereldcontext ontbreekt voor placement-inferentie.");
		return false;
	}

	if (!OutPlacementDefinition)
	{
		OutReason = LOCTEXT("InferPlacementNoDefinition", "Geen placeable stationdefinitie gevonden voor dit station.");
		return false;
	}

	if (WorkspaceRoomOverride)
	{
		FWorkspacePlacedStationRecord PlacementRecord;
		if (TryBuildPlacementRecordForWorkspace(WorkspaceRoomOverride, PlacementRecord, OutReason))
		{
			OutWorkspaceRoom = WorkspaceRoomOverride;
			OutAnchorCell = PlacementRecord.AnchorCell;
			OutRotationQuarterTurns = PlacementRecord.RotationQuarterTurns;
			return true;
		}

		return false;
	}

	for (TActorIterator<AWorkspaceRoom> It(GetWorld()); It; ++It)
	{
		AWorkspaceRoom* WorkspaceRoom = *It;
		if (!WorkspaceRoom || WorkspaceRoom->CellSize <= 0.0f)
		{
			continue;
		}

		FWorkspacePlacedStationRecord PlacementRecord;
		FText ValidationReason;
		if (!TryBuildPlacementRecordForWorkspace(WorkspaceRoom, PlacementRecord, ValidationReason))
		{
			continue;
		}

		OutWorkspaceRoom = WorkspaceRoom;
		OutAnchorCell = PlacementRecord.AnchorCell;
		OutRotationQuarterTurns = PlacementRecord.RotationQuarterTurns;
		return true;
	}

	OutReason = LOCTEXT("InferPlacementNoWorkspaceFit", "Dit station kon niet aan een geldige werkruimte-gridpositie worden gekoppeld.");
	return false;
}

bool ABloodProcessingStation::TryInferCurrentPlacementContext(
	AWorkspaceRoom*& OutWorkspaceRoom,
	FIntPoint& OutAnchorCell,
	int32& OutRotationQuarterTurns,
	const UPlaceableStationDataAsset*& OutPlacementDefinition,
	FText& OutReason) const
{
	return InferPlacementStateFromCurrentTransform(
		OutWorkspaceRoom,
		OutAnchorCell,
		OutRotationQuarterTurns,
		OutPlacementDefinition,
		OutReason);
}

bool ABloodProcessingStation::EnsurePlacementRecordForCurrentTransform(FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!HasAuthority())
	{
		OutReason = LOCTEXT("EnsurePlacementRecordServerOnly", "Plaatsingsrecords kunnen alleen server-side worden aangemaakt.");
		return false;
	}

	if (HasRegisteredPlacementContext())
	{
		return true;
	}

	AWorkspaceRoom* WorkspaceRoom = nullptr;
	FIntPoint AnchorCell = FIntPoint::ZeroValue;
	int32 RotationTurns = 0;
	const UPlaceableStationDataAsset* PlacementDefinition = nullptr;
	if (!InferPlacementStateFromCurrentTransform(WorkspaceRoom, AnchorCell, RotationTurns, PlacementDefinition, OutReason))
	{
		return false;
	}

	const FGuid StationInstanceId = PlacedStationInstanceId.IsValid() ? PlacedStationInstanceId : FGuid::NewGuid();
	FWorkspacePlacedStationRecord PlacementRecord;
	PlacementRecord.StationInstanceId = StationInstanceId;
	PlacementRecord.StationDefinition = const_cast<UPlaceableStationDataAsset*>(PlacementDefinition);
	PlacementRecord.AnchorCell = AnchorCell;
	PlacementRecord.RotationQuarterTurns = RotationTurns;

	return WorkspaceRoom->RegisterPlacedStationActor(this, PlacementRecord, OutReason);
}

bool ABloodProcessingStation::ValidateManualPreviewSetup(FText& OutReason) const
{
	if (!ManualPreviewActorClass)
	{
		OutReason = LOCTEXT("ManualPreviewValidateMissingClass", "Manual preview actor class ontbreekt.");
		return false;
	}

	if (!ResolveManualPreviewSpawnPoint())
	{
		OutReason = LOCTEXT("ManualPreviewValidateMissingSpawnPoint", "Manual preview spawn point ontbreekt.");
		return false;
	}

	if (!ResolveManualPlacementTarget())
	{
		OutReason = LOCTEXT("ManualPreviewValidateMissingPlacementTarget", "Manual placement target ontbreekt.");
		return false;
	}

	return true;
}

void ABloodProcessingStation::HandleManualPlacementConfirmed(
	UInteractionPlacementTargetComponent* TargetComponent,
	UManipulatableObjectComponent* ManipulatedObject)
{
	if (!TargetComponent || TargetComponent != ResolvedManualPlacementTarget || !HasStagedManualProcessingRequest())
	{
		return;
	}

	if (HasSpawnedManualProcessingActor() && (!ManipulatedObject || ManipulatedObject->GetOwner() != SpawnedManualPreviewActor))
	{
		return;
	}

	APawn* Interactor = ActiveInteractor.Get();
	UOwnSystemInventoryComponent* Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(Interactor);
	UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor);

	if (Economy && Economy->GetOwnerRole() < ROLE_Authority)
	{
		FText RequestReason;
		Economy->RequestConfirmManualPlacement(this, RequestReason);
		return;
	}

	FText Reason;
	const bool bCommitted = CommitStagedManualProcessingRequest(Inventory, Economy, Reason);
	if (Economy && !Reason.IsEmpty())
	{
		Economy->SetInteractionFeedback(Reason, bCommitted);
	}
}

float ABloodProcessingStation::GetProcessingDurationTimeUnits() const
{
	return static_cast<float>(GetStoredBatchProcessingDurationDays()) * StationTimeUnitsPerDay;
}

float ABloodProcessingStation::GetProcessingElapsedTimeUnits(const float CurrentAccumulatedTime) const
{
	if (StationState != EBloodVatStationState::Rijpt || ProcessingStartAccumulatedTime < 0.0f)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, CurrentAccumulatedTime - ProcessingStartAccumulatedTime);
}

float ABloodProcessingStation::GetProcessingRemainingTimeUnits(const float CurrentAccumulatedTime) const
{
	if (StationState != EBloodVatStationState::Rijpt || ProcessingReadyAccumulatedTime < 0.0f)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, ProcessingReadyAccumulatedTime - CurrentAccumulatedTime);
}

float ABloodProcessingStation::GetProcessingProgress01(const float CurrentAccumulatedTime) const
{
	if (StationState == EBloodVatStationState::Klaar)
	{
		return 1.0f;
	}

	const float Duration = ProcessingReadyAccumulatedTime - ProcessingStartAccumulatedTime;
	if (StationState != EBloodVatStationState::Rijpt || ProcessingStartAccumulatedTime < 0.0f || ProcessingReadyAccumulatedTime < 0.0f || Duration <= 0.0f)
	{
		return 0.0f;
	}

	return FMath::Clamp((CurrentAccumulatedTime - ProcessingStartAccumulatedTime) / Duration, 0.0f, 1.0f);
}

bool ABloodProcessingStation::CanAcceptBloodItem(const UBloodProductItem* BloodItem, const int32 TotalAvailableUnits, FText& OutReason) const
{
	OutReason = FText::GetEmpty();
	const UBloodProcessingRecipeDataAsset* ActiveRecipe = GetActiveRecipe();

	if (StationState != EBloodVatStationState::Leeg)
	{
		OutReason = LOCTEXT("StationNotEmpty", "Dit vat is niet leeg.");
		return false;
	}

	if (!ActiveRecipe)
	{
		OutReason = LOCTEXT("NoRecipeData", "Dit processing station heeft geen geldige recipe.");
		return false;
	}

	if (!DoesRecipeMeetStationRequirements(ActiveRecipe, OutReason))
	{
		return false;
	}

	if (!BloodItem)
	{
		OutReason = LOCTEXT("NoBloodItem", "Geen blood batch geselecteerd.");
		return false;
	}

	if (!ActiveRecipe->bAcceptAnyInputProcessing && BloodItem->ProcessingType != ActiveRecipe->RequiredInputProcessing)
	{
		OutReason = FText::Format(
			LOCTEXT("WrongProcessingInput", "Alleen {0} bloed kan in dit station worden geplaatst."),
			UBloodProductItem::GetProcessingDisplayName(ActiveRecipe->RequiredInputProcessing));
		return false;
	}

	if (TotalAvailableUnits < MinimumUnitsRequired)
	{
		OutReason = FText::Format(
			LOCTEXT("NotEnoughUnits", "Te weinig units. Minimaal {0} vereist."),
			FText::AsNumber(MinimumUnitsRequired));
		return false;
	}

	if (static_cast<uint8>(BloodItem->BaseQuality) < static_cast<uint8>(ActiveRecipe->MinimumQuality))
	{
		OutReason = LOCTEXT("ProcessingQualityTooLow", "De kwaliteit van deze batch is te laag voor dit vat.");
		return false;
	}

	return true;
}

bool ABloodProcessingStation::TryStartProcessing(UOwnSystemInventoryComponent* Inventory, UVampireEconomyComponent* Economy, UBloodProductItem* BloodItem, const int32 TotalAvailableUnits, FText& OutReason)
{
	FBloodProcessingStartRequest Request;
	if (!BuildProcessingStartRequest(BloodItem, TotalAvailableUnits, Request, OutReason))
	{
		return false;
	}

	return CommitProcessingStartRequest(Inventory, Economy, Request, OutReason);
}

bool ABloodProcessingStation::BuildProcessingStartRequest(const UBloodProductItem* BloodItem, const int32 TotalAvailableUnits, FBloodProcessingStartRequest& OutRequest, FText& OutReason) const
{
	OutReason = FText::GetEmpty();

	if (!CanAcceptBloodItem(BloodItem, TotalAvailableUnits, OutReason))
	{
		return false;
	}

	if (!BloodItem)
	{
		OutReason = LOCTEXT("NoBloodItemForRequest", "Geen blood batch geselecteerd.");
		return false;
	}

	OutRequest.ItemClass = BloodItem->GetClass();
	OutRequest.SourceType = BloodItem->SourceType;
	OutRequest.BaseQuality = BloodItem->BaseQuality;
	OutRequest.ProcessingType = BloodItem->ProcessingType;
	OutRequest.BloodQuantity = TotalAvailableUnits;
	OutRequest.CreatedDay = BloodItem->CreatedDay;
	return true;
}

bool ABloodProcessingStation::CommitProcessingStartRequest(UOwnSystemInventoryComponent* Inventory, UVampireEconomyComponent* Economy, const FBloodProcessingStartRequest& Request, FText& OutReason)
{
	OutReason = FText::GetEmpty();
	const UBloodProcessingRecipeDataAsset* ActiveRecipe = GetActiveRecipe();

	if (!Inventory || !Economy)
	{
		OutReason = LOCTEXT("NoInventoryOrEconomy", "Inventory of economy component ontbreekt.");
		return false;
	}

	if (!ActiveRecipe)
	{
		OutReason = LOCTEXT("NoActiveRecipeOnStart", "Dit processing station heeft geen actieve recipe.");
		return false;
	}

	if (!DoesRecipeMeetStationRequirements(ActiveRecipe, OutReason))
	{
		return false;
	}

	if (ActiveRecipe->GoldCost > 0 && Inventory->GetCurrency() < ActiveRecipe->GoldCost)
	{
		OutReason = LOCTEXT("NotEnoughGoldForVat", "Niet genoeg goud om deze verwerking te starten.");
		return false;
	}

	if (GetTotalMatchingUnitsForRequest(Inventory, Request) < Request.BloodQuantity)
	{
		OutReason = LOCTEXT("MatchingUnitsChanged", "De beschikbare blood units zijn veranderd voordat het station gevuld kon worden.");
		return false;
	}

	StoredBatch.ItemClass = Request.ItemClass;
	StoredBatch.SourceType = Request.SourceType;
	StoredBatch.BaseQuality = Request.BaseQuality;
	StoredBatch.ProcessingType = Request.ProcessingType;
	StoredBatch.BloodQuantity = Request.BloodQuantity;
	StoredBatch.CreatedDay = Request.CreatedDay;

	if (!ConsumeMatchingUnitsForRequest(Inventory, Request))
	{
		OutReason = LOCTEXT("FailedToMoveBatchIntoStation", "De geselecteerde batch kon niet volledig in het station worden geplaatst.");
		return false;
	}

	if (ActiveRecipe->GoldCost > 0)
	{
		Inventory->AddCurrency(-ActiveRecipe->GoldCost);
	}

	const float CurrentAccumulatedTime = GetStationOwnSystemAccumulatedTime(this);
	const float DurationTimeUnits = static_cast<float>(GetStoredBatchProcessingDurationDays()) * StationTimeUnitsPerDay;

	ProcessingStartAccumulatedTime = CurrentAccumulatedTime;
	ProcessingReadyAccumulatedTime = ProcessingStartAccumulatedTime + DurationTimeUnits;
	ProcessingStartDay = Economy->GetCurrentSimDay();
	ProcessingReadyDay = FMath::FloorToInt(ProcessingReadyAccumulatedTime / StationTimeUnitsPerDay);
	StationState = DurationTimeUnits <= 0.0f ? EBloodVatStationState::Klaar : EBloodVatStationState::Rijpt;
	ForceNetUpdate();

	OutReason = DurationTimeUnits <= 0.0f
		? FText::Format(
			LOCTEXT("ProcessingStartedInstantFmt", "{0}: {1} units geplaatst. Verwerking direct voltooid."),
			StationDisplayName,
			FText::AsNumber(Request.BloodQuantity))
		: FText::Format(
			LOCTEXT("ProcessingStartedFmt", "{0}: {1} units geplaatst. Verwerking gestart, klaar op dag {2}."),
			StationDisplayName,
			FText::AsNumber(Request.BloodQuantity),
			FText::AsNumber(ProcessingReadyDay));
	return true;
}

bool ABloodProcessingStation::StageManualProcessingRequest(const UBloodProductItem* BloodItem, const int32 TotalAvailableUnits, FText& OutReason)
{
	FBloodProcessingStartRequest Request;
	if (!BuildProcessingStartRequest(BloodItem, TotalAvailableUnits, Request, OutReason))
	{
		return false;
	}

	StagedManualStartRequest = Request;
	bHasStagedManualStartRequest = true;
	ForceNetUpdate();
	OutReason = FText::Format(
		LOCTEXT("StagedManualRequestFmt", "{0}: batch voorbereid. Wacht op fysieke bevestiging."),
		StationDisplayName);
	return true;
}

bool ABloodProcessingStation::PrepareManualProcessingRequest(const UBloodProductItem* BloodItem, const int32 TotalAvailableUnits, FText& OutReason)
{
	if (bHasStagedManualStartRequest)
	{
		if (HasSpawnedManualProcessingActor())
		{
			OutReason = FText::Format(
				LOCTEXT("ManualRequestAlreadySpawnedFmt", "{0}: de gevulde jug staat al op tafel. Pak hem op en plaats hem in het vat."),
				StationDisplayName);
			return false;
		}

		const bool bRespawned = SpawnManualPreviewActorForRequest(StagedManualStartRequest, OutReason);
		if (bRespawned)
		{
			OutReason = FText::Format(
				LOCTEXT("ManualRequestRespawnedFmt", "{0}: de gevulde jug staat opnieuw klaar op tafel."),
				StationDisplayName);
		}
		return bRespawned;
	}

	FBloodProcessingStartRequest Request;
	if (!BuildProcessingStartRequest(BloodItem, TotalAvailableUnits, Request, OutReason))
	{
		return false;
	}

	if (!SpawnManualPreviewActorForRequest(Request, OutReason))
	{
		return false;
	}

	StagedManualStartRequest = Request;
	bHasStagedManualStartRequest = true;
	ForceNetUpdate();
	OutReason = FText::Format(
		LOCTEXT("PreparedManualRequestFmt", "{0}: batch staat nu als gevulde jug op tafel. Pak de jug op en plaats hem in het vat."),
		StationDisplayName);
	return true;
}

bool ABloodProcessingStation::CommitStagedManualProcessingRequest(UOwnSystemInventoryComponent* Inventory, UVampireEconomyComponent* Economy, FText& OutReason)
{
	if (!bHasStagedManualStartRequest)
	{
		OutReason = LOCTEXT("NoStagedManualRequest", "Er staat geen handmatige verwerkingsactie klaar.");
		return false;
	}

	const bool bCommitted = CommitProcessingStartRequest(Inventory, Economy, StagedManualStartRequest, OutReason);
	if (bCommitted)
	{
		ClearStagedManualProcessingRequest();
	}

	return bCommitted;
}

void ABloodProcessingStation::HandleAbandonedManualFlow()
{
	if (!HasStagedManualProcessingRequest() && !HasSpawnedManualProcessingActor())
	{
		return;
	}

	ClearStagedManualProcessingRequest();
}

void ABloodProcessingStation::ClearStagedManualProcessingRequest()
{
	bHasStagedManualStartRequest = false;
	StagedManualStartRequest = FBloodProcessingStartRequest();
	DestroyManualPreviewActor();
	ForceNetUpdate();
}

bool ABloodProcessingStation::CancelManualProcessingRequest(FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!HasStagedManualProcessingRequest())
	{
		OutReason = LOCTEXT("ManualCancelNoRequest", "Er staat geen batch klaar om te annuleren.");
		return false;
	}

	ClearStagedManualProcessingRequest();

	OutReason = FText::Format(
		LOCTEXT("ManualCancelSuccessFmt", "{0}: batch geannuleerd. De jug is van tafel gehaald."),
		StationDisplayName);
	return true;
}

bool ABloodProcessingStation::HasStagedManualProcessingRequest() const
{
	return bHasStagedManualStartRequest;
}

bool ABloodProcessingStation::HasSpawnedManualProcessingActor() const
{
	return IsValid(SpawnedManualPreviewActor.Get());
}

bool ABloodProcessingStation::SpawnManualPreviewActorForRequest(const FBloodProcessingStartRequest& Request, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (HasSpawnedManualProcessingActor())
	{
		OutReason = LOCTEXT("ManualPreviewAlreadyExists", "Er staat al een gevulde jug klaar bij dit station.");
		return false;
	}

	if (!Request.ItemClass)
	{
		OutReason = LOCTEXT("ManualPreviewMissingItemClass", "De geselecteerde batch is ongeldig voor jug-spawn.");
		return false;
	}

	if (!ManualPreviewActorClass)
	{
		OutReason = LOCTEXT("ManualPreviewClassMissing", "Dit station heeft nog geen jug-preview class ingesteld.");
		return false;
	}

	USceneComponent* SpawnPoint = ResolveManualPreviewSpawnPoint();
	if (!SpawnPoint)
	{
		OutReason = LOCTEXT("ManualPreviewSpawnPointMissing", "Dit station heeft nog geen jug-spawnpunt ingesteld.");
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		OutReason = LOCTEXT("ManualPreviewNoWorld", "Wereldcontext ontbreekt voor het spawnen van de jug.");
		return false;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(
		ManualPreviewActorClass,
		SpawnPoint->GetComponentTransform(),
		SpawnParameters);

	if (!SpawnedActor)
	{
		OutReason = LOCTEXT("ManualPreviewSpawnFailed", "De gevulde jug kon niet op tafel worden gezet.");
		return false;
	}

	SpawnedActor->SetReplicates(true);
	SpawnedActor->SetReplicateMovement(true);

	UPrimitiveComponent* PhysicsComponent = nullptr;
	if (const UManipulatableObjectComponent* ManipulatableObject = SpawnedActor->FindComponentByClass<UManipulatableObjectComponent>())
	{
		PhysicsComponent = ManipulatableObject->GetPrimaryPhysicsComponent();
	}

	if (!PhysicsComponent)
	{
		PhysicsComponent = Cast<UPrimitiveComponent>(SpawnedActor->GetRootComponent());
	}

	if (PhysicsComponent)
	{
		PhysicsComponent->SetSimulatePhysics(true);
		PhysicsComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PhysicsComponent->SetEnableGravity(true);
		PhysicsComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
		PhysicsComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		PhysicsComponent->SetWorldLocation(SpawnPoint->GetComponentLocation() + FVector(0.0f, 0.0f, 8.0f));
		PhysicsComponent->WakeAllRigidBodies();
	}

	SpawnedManualPreviewActor = SpawnedActor;
	ForceNetUpdate();
	return true;
}

void ABloodProcessingStation::DestroyManualPreviewActor()
{
	if (AActor* PreviewActor = SpawnedManualPreviewActor.Get())
	{
		PreviewActor->Destroy();
	}

	SpawnedManualPreviewActor = nullptr;
	ForceNetUpdate();
}

void ABloodProcessingStation::HandleInteractionCancelPressed()
{
	if (!HasStagedManualProcessingRequest())
	{
		return;
	}

	APawn* Interactor = ActiveInteractor.Get();
	UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor);
	if (Economy && Economy->GetOwnerRole() < ROLE_Authority)
	{
		FText RequestReason;
		Economy->RequestCancelManualProcessing(this, RequestReason);
		return;
	}

	FText OutReason;
	const bool bCancelled = CancelManualProcessingRequest(OutReason);
	if (!OutReason.IsEmpty())
	{
		if (Interactor && Economy)
		{
			Economy->SetInteractionFeedback(OutReason, bCancelled);
		}
	}
}

void ABloodProcessingStation::HandleDebugForceOperatorUnpossessPressed()
{
	APawn* Interactor = ActiveInteractor.Get();
	UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Interactor);
	if (!Economy)
	{
		return;
	}

	FText RequestReason;
	Economy->RequestDebugForceOperatorUnpossess(RequestReason);
}

bool ABloodProcessingStation::TryHarvestProcessedBatch(UOwnSystemInventoryComponent* Inventory, UVampireEconomyComponent* Economy, FText& OutReason)
{
	OutReason = FText::GetEmpty();

	if (!Inventory || !Economy)
	{
		OutReason = LOCTEXT("NoInventoryForHarvest", "Inventory of economy component ontbreekt.");
		return false;
	}

	UpdateReadyState(Economy->GetCurrentSimDay());

	if (StationState != EBloodVatStationState::Klaar)
	{
		OutReason = LOCTEXT("StationNotReady", "Deze batch is nog niet klaar om op te halen.");
		return false;
	}

	if (!StoredBatch.ItemClass)
	{
		OutReason = LOCTEXT("StoredBatchMissingClass", "Opgeslagen batchdata is ongeldig.");
		return false;
	}

	const FItemAddResult AddResult = Inventory->TryAddItemFromClass(StoredBatch.ItemClass, 1, false);
	if (AddResult.AmountGiven <= 0 || AddResult.Stacks.IsEmpty())
	{
		OutReason = LOCTEXT("HarvestAddFailed", "Resultaat kon niet aan de inventory worden toegevoegd.");
		return false;
	}

	UBloodProductItem* HarvestedItem = Cast<UBloodProductItem>(AddResult.Stacks[0]);
	if (!HarvestedItem)
	{
		OutReason = LOCTEXT("HarvestWrongClass", "De geoogste batch heeft een ongeldige item class.");
		return false;
	}

	const UBloodProcessingRecipeDataAsset* ActiveRecipe = GetActiveRecipe();
	HarvestedItem->SourceType = StoredBatch.SourceType;
	HarvestedItem->BaseQuality = StoredBatch.BaseQuality;
	HarvestedItem->ProcessingType = ActiveRecipe ? ActiveRecipe->OutputProcessing : StoredBatch.ProcessingType;
	HarvestedItem->BloodQuantity = StoredBatch.BloodQuantity;
	HarvestedItem->CreatedDay = StoredBatch.CreatedDay;
	HarvestedItem->RefreshPresentation();

	StoredBatch = FStoredBloodBatchData();
	StationState = EBloodVatStationState::Leeg;
	ProcessingStartDay = INDEX_NONE;
	ProcessingReadyDay = INDEX_NONE;
	ProcessingStartAccumulatedTime = -1.0f;
	ProcessingReadyAccumulatedTime = -1.0f;
	ForceNetUpdate();

	OutReason = FText::Format(
		LOCTEXT("HarvestedVatBatchFmt", "{0}: verwerkte batch opgehaald en terug in inventory geplaatst."),
		StationDisplayName);
	return true;
}

USceneComponent* ABloodProcessingStation::ResolveAttachmentAnchor(const FProcessingStationAttachmentSlot& Slot) const
{
	if (!Slot.AnchorComponentName.IsNone())
	{
		TInlineComponentArray<USceneComponent*> SceneComponents;
		GetComponents(SceneComponents);

		for (USceneComponent* SceneComponent : SceneComponents)
		{
			if (SceneComponent && SceneComponent->GetFName() == Slot.AnchorComponentName)
			{
				return SceneComponent;
			}
		}
	}

	return SceneRoot ? SceneRoot.Get() : GetRootComponent();
}

UStaticMeshComponent* ABloodProcessingStation::FindOrCreateAttachmentMeshComponent(FProcessingStationAttachmentSlot& Slot)
{
	const FName ComponentName = BuildAttachmentMeshComponentName(Slot.SlotId);
	UStaticMeshComponent* MeshComponent = Slot.SpawnedAttachmentMeshComponent.Get();

	if (!MeshComponent)
	{
		TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents;
		GetComponents(StaticMeshComponents);
		for (UStaticMeshComponent* CandidateComponent : StaticMeshComponents)
		{
			if (CandidateComponent && CandidateComponent->GetFName() == ComponentName)
			{
				MeshComponent = CandidateComponent;
				break;
			}
		}
	}

	if (!MeshComponent)
	{
		MeshComponent = NewObject<UStaticMeshComponent>(this, ComponentName);
		if (!MeshComponent)
		{
			return nullptr;
		}

		MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		MeshComponent->SetMobility(EComponentMobility::Movable);
		AddInstanceComponent(MeshComponent);
		MeshComponent->RegisterComponent();
	}

	if (USceneComponent* AnchorComponent = ResolveAttachmentAnchor(Slot))
	{
		MeshComponent->AttachToComponent(AnchorComponent, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}

	Slot.SpawnedAttachmentMeshComponent = MeshComponent;
	return MeshComponent;
}

void ABloodProcessingStation::RefreshAttachmentVisuals()
{
	TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents;
	GetComponents(StaticMeshComponents);

	for (UStaticMeshComponent* MeshComponent : StaticMeshComponents)
	{
		if (IsGeneratedAttachmentMeshComponent(MeshComponent))
		{
			MeshComponent->DestroyComponent();
		}
	}

	for (FProcessingStationAttachmentSlot& Slot : AttachmentSlots)
	{
		Slot.SpawnedAttachmentMeshComponent = nullptr;
		RefreshAttachmentVisualForSlot(Slot);
	}
}

void ABloodProcessingStation::RefreshAttachmentVisualForSlot(FProcessingStationAttachmentSlot& Slot)
{
	if (!Slot.PlacedAttachment || !Slot.PlacedAttachment->AttachmentMesh)
	{
		DestroyAttachmentVisual(Slot);
		return;
	}

	if (UStaticMeshComponent* MeshComponent = FindOrCreateAttachmentMeshComponent(Slot))
	{
		MeshComponent->SetStaticMesh(Slot.PlacedAttachment->AttachmentMesh);
		MeshComponent->SetRelativeLocation(FVector::ZeroVector);
		MeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
		MeshComponent->SetRelativeScale3D(FVector::OneVector);
		MeshComponent->SetVisibility(true);
	}
}

void ABloodProcessingStation::DestroyAttachmentVisual(FProcessingStationAttachmentSlot& Slot)
{
	UStaticMeshComponent* MeshComponent = Slot.SpawnedAttachmentMeshComponent.Get();
	if (!MeshComponent)
	{
		const FName ComponentName = BuildAttachmentMeshComponentName(Slot.SlotId);
		TInlineComponentArray<UStaticMeshComponent*> StaticMeshComponents;
		GetComponents(StaticMeshComponents);
		for (UStaticMeshComponent* CandidateComponent : StaticMeshComponents)
		{
			if (CandidateComponent && CandidateComponent->GetFName() == ComponentName)
			{
				MeshComponent = CandidateComponent;
				break;
			}
		}
	}

	if (MeshComponent)
	{
		MeshComponent->DestroyComponent();
	}

	Slot.SpawnedAttachmentMeshComponent = nullptr;
}

bool ABloodProcessingStation::CanSlotAcceptAttachment(const FProcessingStationAttachmentSlot& Slot, const UBloodProcessingAttachmentDataAsset* AttachmentData, FText& OutReason) const
{
	OutReason = FText::GetEmpty();

	if (!AttachmentData)
	{
		OutReason = LOCTEXT("SlotAcceptNoData", "Geen attachment data opgegeven.");
		return false;
	}

	if (!AttachmentData->AttachmentTag.IsValid())
	{
		OutReason = LOCTEXT("SlotAcceptNoTag", "Attachment heeft geen geldige gameplay tag.");
		return false;
	}

	if (!Slot.AllowedAttachmentTags.IsEmpty() && !Slot.AllowedAttachmentTags.HasTagExact(AttachmentData->AttachmentTag))
	{
		OutReason = FText::Format(
			LOCTEXT("SlotRejectWrongAttachment", "Attachment {0} past niet in slot {1}."),
			AttachmentData->DisplayName.IsEmpty() ? FText::FromName(AttachmentData->AttachmentId) : AttachmentData->DisplayName,
			Slot.SlotDisplayName.IsEmpty() ? FText::FromName(Slot.SlotId) : Slot.SlotDisplayName);
		return false;
	}

	return true;
}

FText ABloodProcessingStation::BuildStationStatusText() const
{
	switch (StationState)
	{
	case EBloodVatStationState::Leeg:
		return FText::Format(
			LOCTEXT("StationStatusEmptyFmt", "{0} is leeg. Plaats een geldige batch van minstens {1} units."),
			StationDisplayName,
			FText::AsNumber(MinimumUnitsRequired));
	case EBloodVatStationState::Rijpt:
		return FText::Format(
			LOCTEXT("StationStatusAgingFmt", "{0} verwerkt momenteel {1} units. Klaar op dag {2}."),
			StationDisplayName,
			FText::AsNumber(StoredBatch.BloodQuantity),
			FText::AsNumber(ProcessingReadyDay));
	case EBloodVatStationState::Klaar:
		return FText::Format(
			LOCTEXT("StationStatusReadyFmt", "{0} is klaar. Haal de batch op om het station weer vrij te maken."),
			StationDisplayName);
	default:
		return StationDisplayName;
	}
}

#undef LOCTEXT_NAMESPACE
