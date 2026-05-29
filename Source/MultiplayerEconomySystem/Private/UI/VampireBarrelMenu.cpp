#include "UI/VampireBarrelMenu.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ProgressBar.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Items/InventoryComponent.h"
#include "Items/OwnSystemItem.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "UnrealFramework/OwnSystemGameState.h"
#include "Vampire/Data/BloodProcessingAttachmentDataAsset.h"
#include "Vampire/Interaction/BloodProcessingInteractableComponent.h"
#include "UI/VampireBloodBatchRowWidget.h"
#include "Vampire/Data/BloodProcessingRecipeDataAsset.h"
#include "Vampire/Economy/VampireEconomyComponent.h"
#include "Vampire/Items/BloodProductItem.h"
#include "Vampire/World/BloodPackagingStation.h"
#include "Vampire/World/BloodProcessingStation.h"
#include "Widgets/OwnSystemCommonButtonBase.h"
#include "Widgets/OwnSystemCommonTextBlock.h"

namespace
{
	bool ShouldAutoRefreshBarrelMenu(const ABloodProcessingStation* Station)
	{
		if (!Station)
		{
			return false;
		}

		if (Station->StationState != EBloodVatStationState::Leeg)
		{
			return true;
		}

		return Station->RequiresManualProcessingFlow()
			&& (Station->HasStagedManualProcessingRequest() || Station->HasSpawnedManualProcessingActor());
	}

	template <typename WidgetType>
	WidgetType* FindBarrelMenuWidgetByPrefix(const UUserWidget* RootWidget, const FString& Prefix)
	{
		if (!RootWidget || Prefix.IsEmpty() || !RootWidget->WidgetTree)
		{
			return nullptr;
		}

		TArray<UWidget*> PendingWidgets;
		RootWidget->WidgetTree->GetAllWidgets(PendingWidgets);
		for (int32 Index = 0; Index < PendingWidgets.Num(); ++Index)
		{
			UWidget* Widget = PendingWidgets[Index];
			if (WidgetType* TypedWidget = Cast<WidgetType>(Widget))
			{
				if (TypedWidget->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase))
				{
					return TypedWidget;
				}
			}

			if (const UUserWidget* ChildUserWidget = Cast<UUserWidget>(Widget))
			{
				if (ChildUserWidget->WidgetTree)
				{
					TArray<UWidget*> ChildWidgets;
					ChildUserWidget->WidgetTree->GetAllWidgets(ChildWidgets);
					PendingWidgets.Append(ChildWidgets);
				}
			}
		}

		return nullptr;
	}

	template <typename WidgetType>
	WidgetType* FindBarrelMenuWidgetByPrefixInUserWidget(const UUserWidget* RootWidget, const FString& Prefix)
	{
		return FindBarrelMenuWidgetByPrefix<WidgetType>(RootWidget, Prefix);
	}

	template <typename WidgetType>
	WidgetType* FindBarrelMenuWidgetByPrefixInWidgetTree(UWidget* RootWidget, const FString& Prefix)
	{
		if (!RootWidget || Prefix.IsEmpty())
		{
			return nullptr;
		}

		TArray<UWidget*> PendingWidgets;
		PendingWidgets.Add(RootWidget);

		for (int32 Index = 0; Index < PendingWidgets.Num(); ++Index)
		{
			UWidget* Widget = PendingWidgets[Index];
			if (!Widget)
			{
				continue;
			}

			if (WidgetType* TypedWidget = Cast<WidgetType>(Widget))
			{
				if (TypedWidget->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase))
				{
					return TypedWidget;
				}
			}

			if (UPanelWidget* PanelWidget = Cast<UPanelWidget>(Widget))
			{
				const int32 ChildCount = PanelWidget->GetChildrenCount();
				for (int32 ChildIndex = 0; ChildIndex < ChildCount; ++ChildIndex)
				{
					if (UWidget* ChildWidget = PanelWidget->GetChildAt(ChildIndex))
					{
						PendingWidgets.Add(ChildWidget);
					}
				}
			}

			if (const UUserWidget* ChildUserWidget = Cast<UUserWidget>(Widget))
			{
				if (ChildUserWidget->WidgetTree)
				{
					TArray<UWidget*> ChildWidgets;
					ChildUserWidget->WidgetTree->GetAllWidgets(ChildWidgets);
					PendingWidgets.Append(ChildWidgets);
				}
			}
		}

		return nullptr;
	}

	void SetResolvedText(UOwnSystemCommonTextBlock* OwnSystemText, UTextBlock* FallbackText, const FText& NewText)
	{
		if (OwnSystemText)
		{
			OwnSystemText->SetText(NewText);
		}
		else if (FallbackText)
		{
			FallbackText->SetText(NewText);
		}
	}

	void SetResolvedTextColor(UOwnSystemCommonTextBlock* OwnSystemText, UTextBlock* FallbackText, const FSlateColor& NewColor)
	{
		if (OwnSystemText)
		{
			OwnSystemText->SetColorAndOpacity(NewColor);
		}
		else if (FallbackText)
		{
			FallbackText->SetColorAndOpacity(NewColor);
		}
	}

	void SetResolvedImageColor(UImage* ImageWidget, const FSlateColor& NewColor)
	{
		if (ImageWidget)
		{
			ImageWidget->SetColorAndOpacity(NewColor.GetSpecifiedColor());
		}
	}

	void SetResolvedProgressPercent(UProgressBar* ProgressBar, const float Percent)
	{
		if (ProgressBar)
		{
			ProgressBar->SetPercent(Percent);
		}
	}

	FText FormatProcessingDurationText(const float TimeUnits)
	{
		const float ClampedUnits = FMath::Max(0.0f, TimeUnits);
		const int32 Nights = FMath::FloorToInt(ClampedUnits / 2400.0f);
		const float RemainderUnits = FMath::Fmod(ClampedUnits, 2400.0f);
		const int32 Hours = FMath::FloorToInt(RemainderUnits / 100.0f);
		const int32 Minutes = FMath::TruncToInt((FMath::Fmod(RemainderUnits, 100.0f) / 100.0f) * 60.0f);

		FNumberFormattingOptions TwoDigitOptions;
		TwoDigitOptions.MinimumIntegralDigits = 2;
		TwoDigitOptions.MaximumIntegralDigits = 2;
		TwoDigitOptions.UseGrouping = false;

		if (Nights > 0)
		{
			return FText::Format(
				Nights == 1
					? NSLOCTEXT("ProcessingStationMenuBase", "ProcessingDurationNightFmt", "{0} nacht {1}:{2}")
					: NSLOCTEXT("ProcessingStationMenuBase", "ProcessingDurationNightsFmt", "{0} nachten {1}:{2}"),
				FText::AsNumber(Nights),
				FText::AsNumber(Hours, &TwoDigitOptions),
				FText::AsNumber(Minutes, &TwoDigitOptions));
		}

		return FText::Format(
			NSLOCTEXT("ProcessingStationMenuBase", "ProcessingDurationHoursFmt", "{0}:{1}"),
			FText::AsNumber(Hours, &TwoDigitOptions),
			FText::AsNumber(Minutes, &TwoDigitOptions));
	}

	float GetOwnSystemAccumulatedTime(const UObject* WorldContextObject)
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

	void SetResolvedButtonVisibility(UOwnSystemCommonButtonBase* OwnSystemButton, UButton* FallbackButton, const ESlateVisibility NewVisibility)
	{
		if (OwnSystemButton)
		{
			OwnSystemButton->SetVisibility(NewVisibility);
		}
		else if (FallbackButton)
		{
			FallbackButton->SetVisibility(NewVisibility);
		}
	}

	void SetResolvedButtonText(UOwnSystemCommonButtonBase* OwnSystemButton, const FText& NewText)
	{
		if (OwnSystemButton)
		{
			OwnSystemButton->SetButtonText(NewText);
		}
	}

	void SetWidgetVisibility(UWidget* Widget, const ESlateVisibility NewVisibility)
	{
		if (Widget)
		{
			Widget->SetVisibility(NewVisibility);
		}
	}

	bool DoBloodItemsMatchForBarrel(const UBloodProductItem* A, const UBloodProductItem* B)
	{
		return A
			&& B
			&& A->GetClass() == B->GetClass()
			&& A->SourceType == B->SourceType
			&& A->BaseQuality == B->BaseQuality
			&& A->ProcessingType == B->ProcessingType;
	}

	FString JoinGameplayTags(const FGameplayTagContainer& Tags)
	{
		FString Result;
		bool bIsFirst = true;
		for (const FGameplayTag& Tag : Tags)
		{
			if (!Tag.IsValid())
			{
				continue;
			}

			if (!bIsFirst)
			{
				Result += TEXT(", ");
			}

			Result += Tag.ToString();
			bIsFirst = false;
		}

		return Result;
	}

	bool IsPackagingStation(const ABloodProcessingStation* Station)
	{
		return Cast<ABloodPackagingStation>(Station) != nullptr;
	}
}

#define LOCTEXT_NAMESPACE "ProcessingStationMenuBase"

UProcessingStationMenuBase::UProcessingStationMenuBase()
{
	InputConfig = EOwnSystemWidgetInputMode::GameAndMenu;
}

UVampireBarrelMenu::UVampireBarrelMenu() = default;

void UProcessingStationMenuBase::SetProcessingStationContext(ABloodProcessingStation* InStation, APawn* InInteractor)
{
	BoundStation = InStation;
	BoundInteractor = InInteractor;
	SelectedGroupIndex = 0;
	SelectedAttachmentSlotIndex = 0;
	SelectedAttachmentOptionIndex = 0;
	SelectionMode = EProcessingStationMenuSelectionMode::Batches;
	bPendingCloseAfterSuccessfulHarvest = false;
}

void UProcessingStationMenuBase::NativeConstruct()
{
	Super::NativeConstruct();

	SetIsFocusable(true);
	ResolveImportedWidgetRefs();

	const UWorld* World = GetWorld();
	if (IsDesignTime() || !World || !World->IsGameWorld())
	{
		return;
	}

	BoundInventory = ResolveInventory(this);
	BoundEconomy = ResolveEconomy(this);

	if (BoundInventory)
	{
		BoundInventory->OnInventoryUpdated.AddDynamic(this, &UProcessingStationMenuBase::HandleInventoryUpdated);
		BoundInventory->OnCurrencyChanged.AddDynamic(this, &UProcessingStationMenuBase::HandleCurrencyChanged);
	}

	if (BoundEconomy)
	{
		BoundEconomy->OnEconomyUpdated.AddDynamic(this, &UProcessingStationMenuBase::HandleEconomyUpdated);
	}

	if (BtnPrevBatch)
	{
		BtnPrevBatch->OnClicked().AddUObject(this, &UProcessingStationMenuBase::HandlePrevBatchClicked);
	}

	if (BtnNextBatch)
	{
		BtnNextBatch->OnClicked().AddUObject(this, &UProcessingStationMenuBase::HandleNextBatchClicked);
	}

	if (BtnPrimaryAction)
	{
		BtnPrimaryAction->OnClicked().AddUObject(this, &UProcessingStationMenuBase::HandlePrimaryActionClicked);
	}

	if (BtnClose)
	{
		BtnClose->OnClicked().AddUObject(this, &UProcessingStationMenuBase::HandleCloseClicked);
	}

	if (UWorld* MutableWorld = GetWorld())
	{
		MutableWorld->GetTimerManager().SetTimer(LiveRefreshTimerHandle, this, &UProcessingStationMenuBase::HandleLiveRefreshTick, 0.1f, true);
	}

	RefreshMenu();
	SetKeyboardFocus();
}

void UProcessingStationMenuBase::NativeDestruct()
{
	const UWorld* World = GetWorld();
	const bool bIsRuntimeWorld = World && World->IsGameWorld() && !IsDesignTime();

	if (bIsRuntimeWorld && BoundInventory)
	{
		BoundInventory->OnInventoryUpdated.RemoveDynamic(this, &UProcessingStationMenuBase::HandleInventoryUpdated);
		BoundInventory->OnCurrencyChanged.RemoveDynamic(this, &UProcessingStationMenuBase::HandleCurrencyChanged);
	}

	if (bIsRuntimeWorld && BoundEconomy)
	{
		BoundEconomy->OnEconomyUpdated.RemoveDynamic(this, &UProcessingStationMenuBase::HandleEconomyUpdated);
	}

	if (bIsRuntimeWorld && BtnPrevBatch)
	{
		BtnPrevBatch->OnClicked().RemoveAll(this);
	}

	if (bIsRuntimeWorld && BtnNextBatch)
	{
		BtnNextBatch->OnClicked().RemoveAll(this);
	}

	if (bIsRuntimeWorld && BtnPrimaryAction)
	{
		BtnPrimaryAction->OnClicked().RemoveAll(this);
	}

	if (bIsRuntimeWorld)
	{
		UnbindFallbackButtons();
	}

	if (bIsRuntimeWorld && BtnClose)
	{
		BtnClose->OnClicked().RemoveAll(this);
	}

	if (bIsRuntimeWorld)
	{
		if (UWorld* MutableWorld = GetWorld())
		{
			MutableWorld->GetTimerManager().ClearTimer(LiveRefreshTimerHandle);
		}
	}

	Super::NativeDestruct();
}

FReply UProcessingStationMenuBase::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	return NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UProcessingStationMenuBase::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Delete || InKeyEvent.GetKey() == EKeys::Escape)
	{
		FText Reason;
		const bool bCancelled = HandleCancelPackaging(Reason);
		if (BoundEconomy && !Reason.IsEmpty())
		{
			BoundEconomy->SetInteractionFeedback(Reason, bCancelled);
		}
		if (bCancelled)
		{
			RefreshMenu();
			return FReply::Handled();
		}
	}

	if (InKeyEvent.GetKey() == EKeys::Up || InKeyEvent.GetKey() == EKeys::W)
	{
		HandlePrevBatchClicked();
		return FReply::Handled();
	}

	if (InKeyEvent.GetKey() == EKeys::Down || InKeyEvent.GetKey() == EKeys::S)
	{
		HandleNextBatchClicked();
		return FReply::Handled();
	}

	if (InKeyEvent.GetKey() == EKeys::Q || InKeyEvent.GetKey() == EKeys::Left)
	{
		HandlePrevBatchClicked();
		return FReply::Handled();
	}

	if (InKeyEvent.GetKey() == EKeys::E || InKeyEvent.GetKey() == EKeys::Right)
	{
		HandleNextBatchClicked();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FReply UProcessingStationMenuBase::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	SetKeyboardFocus();
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

UWidget* UProcessingStationMenuBase::NativeGetDesiredFocusTarget() const
{
	return const_cast<UProcessingStationMenuBase*>(this);
}

void UProcessingStationMenuBase::RefreshMenu()
{
	const UWorld* World = GetWorld();
	if (IsDesignTime() || !World || !World->IsGameWorld())
	{
		return;
	}

	if (ABloodProcessingStation* Station = BoundStation.Get())
	{
		if (ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(Station))
		{
			PackagingStation->RefreshPackagingPlacementState();
		}
	}

	ClampSelectedGroupIndex();
	ClampSelectedAttachmentState();

	if (StateSwitcher && BoundStation.IsValid())
	{
		switch (BoundStation->StationState)
		{
		case EBloodVatStationState::Leeg:
			StateSwitcher->SetActiveWidgetIndex(0);
			break;
		case EBloodVatStationState::Rijpt:
			StateSwitcher->SetActiveWidgetIndex(1);
			break;
		case EBloodVatStationState::Klaar:
			StateSwitcher->SetActiveWidgetIndex(2);
			break;
		default:
			break;
		}
	}

	ResolveActiveStateWidgetRefs();
	BindFallbackButtons();

	SetResolvedText(TxtTitle, TxtTitleFallback, ResolveConfiguredTitleText());
	SetResolvedText(TxtState, TxtStateFallback, GetStateText());
	SetResolvedText(TxtRule, TxtRuleFallback, BuildRuleText());

	SetResolvedText(
		TxtDuration,
		TxtDurationFallback,
		BuildRecipeSummaryText());
	SetResolvedText(TxtStatusBannerTitle, TxtStatusBannerTitleFallback, BuildStatusBannerTitle());
	SetResolvedText(TxtStatusBannerSubtitle, TxtStatusBannerSubtitleFallback, BuildStatusBannerSubtitle());
	if (ShouldShowBatchSelection() && !BatchListContainer && !BatchListContainerFallback)
	{
		SetResolvedText(TxtBatchList, TxtBatchListFallback, BuildBatchListText());
	}
	else
	{
		SetResolvedText(TxtBatchList, TxtBatchListFallback, FText::GetEmpty());
	}
	const ESlateVisibility BatchSelectionVisibility = ShouldShowBatchSelection() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	SetWidgetVisibility(TxtBatchList, BatchSelectionVisibility);
	SetWidgetVisibility(TxtBatchListFallback, BatchSelectionVisibility);
	SetWidgetVisibility(BatchListContainer, BatchSelectionVisibility);
	SetWidgetVisibility(BatchListContainerFallback, BatchSelectionVisibility);
	SetResolvedText(TxtDetailHeader, TxtDetailHeaderFallback, BuildDetailHeaderText());
	SetResolvedText(TxtSelectedBatch, TxtSelectedBatchFallback, BuildSelectedBatchText());
	SetResolvedText(TxtSelectedUnits, TxtSelectedUnitsFallback, BuildSelectedUnitsText());
	SetResolvedText(TxtSelectedMeta, TxtSelectedMetaFallback, BuildSelectedMetaText());
	SetResolvedText(TxtValidation, TxtValidationFallback, BuildValidationText());
	const bool bDisplayProgress = ShouldShowProcessingProgress();
	SetResolvedText(TxtProcessFact, TxtProcessFactFallback, bDisplayProgress ? BuildProcessFactText() : FText::GetEmpty());
	SetResolvedProgressPercent(ProgressBar_Processing ? ProgressBar_Processing : ProgressBarProcessingFallback, bDisplayProgress ? BuildProcessingProgress01() : 0.0f);
	SetResolvedText(Txt_ProgressPct, TxtProgressPctFallback, bDisplayProgress ? BuildProcessingProgressPctText() : FText::GetEmpty());
	SetResolvedText(Txt_Elapsed, TxtElapsedFallback, bDisplayProgress ? BuildProcessingElapsedText() : FText::GetEmpty());
	SetResolvedText(Txt_Remaining, TxtRemainingFallback, bDisplayProgress ? BuildProcessingRemainingText() : FText::GetEmpty());
	const ESlateVisibility ProgressVisibility = bDisplayProgress ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
	SetWidgetVisibility(TxtProcessFact, ProgressVisibility);
	SetWidgetVisibility(TxtProcessFactFallback, ProgressVisibility);
	SetWidgetVisibility(ProgressBar_Processing, ProgressVisibility);
	SetWidgetVisibility(ProgressBarProcessingFallback, ProgressVisibility);
	SetWidgetVisibility(Txt_ProgressPct, ProgressVisibility);
	SetWidgetVisibility(TxtProgressPctFallback, ProgressVisibility);
	SetWidgetVisibility(Txt_Elapsed, ProgressVisibility);
	SetWidgetVisibility(TxtElapsedFallback, ProgressVisibility);
	SetWidgetVisibility(Txt_Remaining, ProgressVisibility);
	SetWidgetVisibility(TxtRemainingFallback, ProgressVisibility);
	SetResolvedTextColor(TxtValidation, TxtValidationFallback, BuildValidationColor());
	SetResolvedImageColor(ImgValidationDot ? ImgValidationDot : ImgValidationDotFallback, BuildValidationColor());
	SetResolvedText(TxtFooterHelp, TxtFooterHelpFallback, BuildFooterHelpText());
	RebuildBatchRowList();

	UpdateButtonState();

	if (bPendingCloseAfterSuccessfulHarvest
		&& BoundStation.IsValid()
		&& BoundStation->StationState == EBloodVatStationState::Leeg)
	{
		bPendingCloseAfterSuccessfulHarvest = false;
		HandleCloseClicked();
	}
}

void UProcessingStationMenuBase::HandleInventoryUpdated()
{
	RefreshMenu();
}

void UProcessingStationMenuBase::HandleCurrencyChanged(const int32 OldCurrency, const int32 NewCurrency)
{
	RefreshMenu();
}

void UProcessingStationMenuBase::HandleEconomyUpdated()
{
	RefreshMenu();
}

void UProcessingStationMenuBase::HandlePrevBatchClickedFallback()
{
	HandlePrevBatchClicked();
}

void UProcessingStationMenuBase::HandleNextBatchClickedFallback()
{
	HandleNextBatchClicked();
}

void UProcessingStationMenuBase::HandlePrimaryActionClickedFallback()
{
	HandlePrimaryActionClicked();
}

void UProcessingStationMenuBase::HandleCloseClickedFallback()
{
	HandleCloseClicked();
}

void UProcessingStationMenuBase::HandleBatchRowClicked(const int32 ClickedIndex)
{
	if (ClickedIndex < 0)
	{
		return;
	}

	SelectedGroupIndex = ClickedIndex;
	RefreshMenu();
}

void UProcessingStationMenuBase::HandleLiveRefreshTick()
{
	if (bPendingCloseAfterSuccessfulHarvest || ShouldAutoRefreshBarrelMenu(BoundStation.Get()))
	{
		RefreshMenu();
	}
}

void UProcessingStationMenuBase::HandlePrevBatchClicked()
{
	if (!BoundStation.IsValid() || BoundStation->StationState != EBloodVatStationState::Leeg)
	{
		return;
	}

	TArray<UBloodProductItem*> Representatives;
	TArray<int32> TotalUnits;
	GetCandidateGroups(Representatives, TotalUnits);
	if (Representatives.IsEmpty())
	{
		return;
	}

	SelectedGroupIndex = (SelectedGroupIndex - 1 + Representatives.Num()) % Representatives.Num();
	RefreshMenu();
}

void UProcessingStationMenuBase::HandleNextBatchClicked()
{
	if (!BoundStation.IsValid() || BoundStation->StationState != EBloodVatStationState::Leeg)
	{
		return;
	}

	TArray<UBloodProductItem*> Representatives;
	TArray<int32> TotalUnits;
	GetCandidateGroups(Representatives, TotalUnits);
	if (Representatives.IsEmpty())
	{
		return;
	}

	SelectedGroupIndex = (SelectedGroupIndex + 1) % Representatives.Num();
	RefreshMenu();
}

void UProcessingStationMenuBase::HandlePrevRecipeClicked()
{
	if (!BoundStation.IsValid() || !BoundEconomy || BoundStation->StationState != EBloodVatStationState::Leeg || BoundStation->GetRecipeCount() <= 1)
	{
		return;
	}

	FText Reason;
	if (BoundEconomy->RequestStepSelectedRecipe(BoundStation.Get(), -1, Reason) && !Reason.IsEmpty())
	{
		BoundEconomy->SetInteractionFeedback(Reason, true);
	}
}

void UProcessingStationMenuBase::HandleNextRecipeClicked()
{
	if (!BoundStation.IsValid() || !BoundEconomy || BoundStation->StationState != EBloodVatStationState::Leeg || BoundStation->GetRecipeCount() <= 1)
	{
		return;
	}

	FText Reason;
	if (BoundEconomy->RequestStepSelectedRecipe(BoundStation.Get(), 1, Reason) && !Reason.IsEmpty())
	{
		BoundEconomy->SetInteractionFeedback(Reason, true);
	}
}

void UProcessingStationMenuBase::HandleToggleSelectionMode()
{
	if (!bAllowAttachmentSelection || !BoundStation.IsValid() || BoundStation->StationState != EBloodVatStationState::Leeg || BoundStation->AttachmentSlots.IsEmpty())
	{
		return;
	}

	SelectionMode = SelectionMode == EProcessingStationMenuSelectionMode::Batches
		? EProcessingStationMenuSelectionMode::Attachments
		: EProcessingStationMenuSelectionMode::Batches;
	RefreshMenu();
}

void UProcessingStationMenuBase::HandlePrevAttachmentSlot()
{
	if (!bAllowAttachmentSelection || !BoundStation.IsValid() || BoundStation->AttachmentSlots.IsEmpty() || BoundStation->StationState != EBloodVatStationState::Leeg)
	{
		return;
	}

	SelectedAttachmentSlotIndex = (SelectedAttachmentSlotIndex - 1 + BoundStation->AttachmentSlots.Num()) % BoundStation->AttachmentSlots.Num();
	SelectedAttachmentOptionIndex = 0;
	RefreshMenu();
}

void UProcessingStationMenuBase::HandleNextAttachmentSlot()
{
	if (!bAllowAttachmentSelection || !BoundStation.IsValid() || BoundStation->AttachmentSlots.IsEmpty() || BoundStation->StationState != EBloodVatStationState::Leeg)
	{
		return;
	}

	SelectedAttachmentSlotIndex = (SelectedAttachmentSlotIndex + 1) % BoundStation->AttachmentSlots.Num();
	SelectedAttachmentOptionIndex = 0;
	RefreshMenu();
}

void UProcessingStationMenuBase::HandlePrevAttachmentOption()
{
	TArray<UBloodProcessingAttachmentDataAsset*> Options;
	GetCompatibleAttachmentOptions(Options);
	if (Options.IsEmpty())
	{
		return;
	}

	SelectedAttachmentOptionIndex = (SelectedAttachmentOptionIndex - 1 + Options.Num()) % Options.Num();
	RefreshMenu();
}

void UProcessingStationMenuBase::HandleNextAttachmentOption()
{
	TArray<UBloodProcessingAttachmentDataAsset*> Options;
	GetCompatibleAttachmentOptions(Options);
	if (Options.IsEmpty())
	{
		return;
	}

	SelectedAttachmentOptionIndex = (SelectedAttachmentOptionIndex + 1) % Options.Num();
	RefreshMenu();
}

void UProcessingStationMenuBase::HandlePrimaryActionClicked()
{
	FText Reason;
	bool bSuccess = false;
	const bool bWasReadyBeforeAction = BoundStation.IsValid() && BoundStation->StationState == EBloodVatStationState::Klaar;
	const bool bWasManualPrepAction = BoundStation.IsValid()
		&& BoundStation->StationState == EBloodVatStationState::Leeg
		&& BoundStation->RequiresManualProcessingFlow()
		&& !IsPackagingStation(BoundStation.Get());

	if (BoundStation.IsValid())
	{
		if (BoundStation->StationState == EBloodVatStationState::Klaar)
		{
			bSuccess = HandleHarvest(Reason);
		}
		else if (BoundStation->StationState == EBloodVatStationState::Leeg)
		{
			bSuccess = HandleStartAging(Reason);
		}
		else
		{
			Reason = BoundStation->BuildStationStatusText();
		}
	}

	if (BoundEconomy && !Reason.IsEmpty())
	{
		BoundEconomy->SetInteractionFeedback(Reason, bSuccess);
	}

	if (bSuccess && bWasManualPrepAction && BoundStation.IsValid() && BoundStation->HasSpawnedManualProcessingActor())
	{
		HandleCloseClicked();
		return;
	}

	if (bWasReadyBeforeAction && bSuccess && bAutoCloseAfterSuccessfulHarvest)
	{
		bPendingCloseAfterSuccessfulHarvest = true;
	}

	RefreshMenu();
}

void UProcessingStationMenuBase::HandleCloseClicked()
{
	if (BoundStation.IsValid())
	{
		if (UBloodProcessingInteractableComponent* ProcessingInteractable = BoundStation->GetComponentByClass<UBloodProcessingInteractableComponent>())
		{
			ProcessingInteractable->Activate(true);
		}
	}

	if (BoundEconomy && BoundStation.IsValid())
	{
		FText ReleaseReason;
		BoundEconomy->RequestReleaseProcessingStation(BoundStation.Get(), ReleaseReason);
	}

	if (APlayerController* PlayerController = GetOwningPlayer())
	{
		if (BoundStation.IsValid())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("ProcessingStationMenu: Close station=%s requiresManual=%s removing=%s"),
				*GetNameSafe(BoundStation.Get()),
				BoundStation->RequiresManualProcessingFlow() ? TEXT("true") : TEXT("false"),
				TEXT("FullInteractionIMC"));

			BoundStation->RemoveInteractionInputContext(PlayerController);
		}

		if (BoundInteractor.IsValid())
		{
			PlayerController->SetViewTargetWithBlend(BoundInteractor.Get(), BoundStation.IsValid() ? BoundStation->InteractionCameraBlendTime : 0.25f);
		}
		PlayerController->SetShowMouseCursor(false);
		FInputModeGameOnly InputMode;
		PlayerController->SetInputMode(InputMode);
	}

	DeactivateWidget();
	RemoveFromParent();
}

void UProcessingStationMenuBase::ResolveImportedWidgetRefs()
{
	StateSwitcher = FindBarrelMenuWidgetByPrefix<UWidgetSwitcher>(this, TEXT("StateSwitcher"));
}

void UProcessingStationMenuBase::ResolveActiveStateWidgetRefs()
{
	UUserWidget* ActiveStateWidget = StateSwitcher ? Cast<UUserWidget>(StateSwitcher->GetActiveWidget()) : nullptr;

	TxtTitleFallback = TxtTitle || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtTitle"));
	TxtStateFallback = TxtState || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtState"));
	TxtRuleFallback = TxtRule || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtRule"));
	TxtDurationFallback = TxtDuration || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtDuration"));
	TxtStatusBannerTitleFallback = TxtStatusBannerTitle || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtStatusBannerTitle"));
	TxtStatusBannerSubtitleFallback = TxtStatusBannerSubtitle || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtStatusBannerSubtitle"));
	TxtBatchListFallback = TxtBatchList || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtBatchList"));
	TxtDetailHeaderFallback = TxtDetailHeader || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtDetailHeader"));
	TxtSelectedBatchFallback = TxtSelectedBatch || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtSelectedBatch"));
	TxtSelectedUnitsFallback = TxtSelectedUnits || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtSelectedUnits"));
	if (!TxtSelectedUnits && !TxtSelectedUnitsFallback && ActiveStateWidget)
	{
		TxtSelectedUnitsFallback = FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("Txt_SelectedUnits"));
	}
	TxtSelectedMetaFallback = TxtSelectedMeta || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtSelectedMeta"));
	if (!TxtSelectedMeta && !TxtSelectedMetaFallback && ActiveStateWidget)
	{
		TxtSelectedMetaFallback = FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("Txt_SelectedMeta"));
	}
	TxtValidationFallback = TxtValidation || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtValidation"));
	TxtProcessFactFallback = TxtProcessFact || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtProcessFact"));
	ProgressBarProcessingFallback = ProgressBar_Processing || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UProgressBar>(ActiveStateWidget, TEXT("ProgressBar_Processing"));
	TxtProgressPctFallback = Txt_ProgressPct || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("Txt_ProgressPct"));
	TxtElapsedFallback = Txt_Elapsed || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("Txt_Elapsed"));
	TxtRemainingFallback = Txt_Remaining || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("Txt_Remaining"));
	TxtFooterHelpFallback = TxtFooterHelp || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtFooterHelp"));
	ImgValidationDotFallback = nullptr;
	if (!ImgValidationDot && ActiveStateWidget)
	{
		// Prefer the legacy selected-detail dot name first so dynamic list-row dots
		// do not hijack the selected-batch validation indicator lookup.
		ImgValidationDotFallback = FindBarrelMenuWidgetByPrefixInUserWidget<UImage>(ActiveStateWidget, TEXT("Dot_Valid"));
		if (!ImgValidationDotFallback)
		{
			ImgValidationDotFallback = FindBarrelMenuWidgetByPrefixInUserWidget<UImage>(ActiveStateWidget, TEXT("ImgSelectedValidationDot"));
		}
		if (!ImgValidationDotFallback)
		{
			ImgValidationDotFallback = FindBarrelMenuWidgetByPrefixInUserWidget<UImage>(ActiveStateWidget, TEXT("ImgValidationDot"));
		}
	}
	BatchListContainerFallback = BatchListContainer || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UScrollBox>(ActiveStateWidget, TEXT("BatchListContainer"));
	if (!BatchListContainer && !BatchListContainerFallback && ActiveStateWidget)
	{
		BatchListContainerFallback = FindBarrelMenuWidgetByPrefixInUserWidget<UScrollBox>(ActiveStateWidget, TEXT("TxtBatchList"));
	}

	BtnPrevBatchFallback = BtnPrevBatch || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UButton>(ActiveStateWidget, TEXT("BtnPrevBatch"));
	BtnNextBatchFallback = BtnNextBatch || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UButton>(ActiveStateWidget, TEXT("BtnNextBatch"));
	BtnPrimaryActionFallback = BtnPrimaryAction || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UButton>(ActiveStateWidget, TEXT("BtnPrimaryAction"));
	TxtPrimaryActionLabelFallback = nullptr;
	if (!BtnPrimaryAction && BtnPrimaryActionFallback)
	{
		TxtPrimaryActionLabelFallback = FindBarrelMenuWidgetByPrefixInWidgetTree<UTextBlock>(BtnPrimaryActionFallback, TEXT("Txt_PrimaryLabel"));
		if (!TxtPrimaryActionLabelFallback)
		{
			TxtPrimaryActionLabelFallback = FindBarrelMenuWidgetByPrefixInWidgetTree<UTextBlock>(BtnPrimaryActionFallback, TEXT("TxtPrimaryLabel"));
		}
	}

	if (!TxtPrimaryActionLabelFallback && ActiveStateWidget)
	{
		TxtPrimaryActionLabelFallback = FindBarrelMenuWidgetByPrefixInUserWidget<UTextBlock>(ActiveStateWidget, TEXT("TxtPrimaryLabel"));
	}
	BtnCloseFallback = BtnClose || !ActiveStateWidget ? nullptr : FindBarrelMenuWidgetByPrefixInUserWidget<UButton>(ActiveStateWidget, TEXT("BtnClose"));
}

void UProcessingStationMenuBase::BindFallbackButtons()
{
	UnbindFallbackButtons();

	if (!BtnPrevBatch && BtnPrevBatchFallback)
	{
		BtnPrevBatchFallback->OnClicked.AddDynamic(this, &UProcessingStationMenuBase::HandlePrevBatchClickedFallback);
	}

	if (!BtnNextBatch && BtnNextBatchFallback)
	{
		BtnNextBatchFallback->OnClicked.AddDynamic(this, &UProcessingStationMenuBase::HandleNextBatchClickedFallback);
	}

	if (!BtnPrimaryAction && BtnPrimaryActionFallback)
	{
		BtnPrimaryActionFallback->OnClicked.AddDynamic(this, &UProcessingStationMenuBase::HandlePrimaryActionClickedFallback);
	}

	if (!BtnClose && BtnCloseFallback)
	{
		BtnCloseFallback->OnClicked.AddDynamic(this, &UProcessingStationMenuBase::HandleCloseClickedFallback);
	}
}

void UProcessingStationMenuBase::UnbindFallbackButtons()
{
	if (BtnPrevBatchFallback)
	{
		BtnPrevBatchFallback->OnClicked.RemoveAll(this);
	}

	if (BtnNextBatchFallback)
	{
		BtnNextBatchFallback->OnClicked.RemoveAll(this);
	}

	if (BtnPrimaryActionFallback)
	{
		BtnPrimaryActionFallback->OnClicked.RemoveAll(this);
	}

	if (BtnCloseFallback)
	{
		BtnCloseFallback->OnClicked.RemoveAll(this);
	}
}

UOwnSystemInventoryComponent* UProcessingStationMenuBase::ResolveInventory(const UUserWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}

	if (const APlayerController* PC = Widget->GetOwningPlayer())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			if (UOwnSystemInventoryComponent* Inventory = UVampireEconomyComponent::ResolveInventoryFromActor(Pawn))
			{
				return Inventory;
			}
		}

		return UVampireEconomyComponent::ResolveInventoryFromActor(PC);
	}

	return nullptr;
}

UVampireEconomyComponent* UProcessingStationMenuBase::ResolveEconomy(const UUserWidget* Widget)
{
	if (!Widget)
	{
		return nullptr;
	}

	if (const APlayerController* PC = Widget->GetOwningPlayer())
	{
		if (const APawn* Pawn = PC->GetPawn())
		{
			if (UVampireEconomyComponent* Economy = UVampireEconomyComponent::ResolveEconomyFromActor(Pawn))
			{
				return Economy;
			}
		}

		return UVampireEconomyComponent::ResolveEconomyFromActor(PC);
	}

	return nullptr;
}

void UProcessingStationMenuBase::GetCandidateGroups(TArray<UBloodProductItem*>& OutRepresentatives, TArray<int32>& OutTotalUnits) const
{
	OutRepresentatives.Reset();
	OutTotalUnits.Reset();

	if (!BoundInventory)
	{
		return;
	}

	for (UOwnSystemItem* Item : BoundInventory->GetItems())
	{
		UBloodProductItem* Candidate = Cast<UBloodProductItem>(Item);
		if (!Candidate)
		{
			continue;
		}

		int32 ExistingIndex = INDEX_NONE;
		for (int32 Index = 0; Index < OutRepresentatives.Num(); ++Index)
		{
			if (DoBloodItemsMatchForBarrel(OutRepresentatives[Index], Candidate))
			{
				ExistingIndex = Index;
				break;
			}
		}

		if (ExistingIndex == INDEX_NONE)
		{
			OutRepresentatives.Add(Candidate);
			OutTotalUnits.Add(Candidate->BloodQuantity);
		}
		else
		{
			OutTotalUnits[ExistingIndex] += Candidate->BloodQuantity;
		}
	}
}

void UProcessingStationMenuBase::ClampSelectedGroupIndex()
{
	TArray<UBloodProductItem*> Representatives;
	TArray<int32> TotalUnits;
	GetCandidateGroups(Representatives, TotalUnits);

	if (Representatives.IsEmpty())
	{
		SelectedGroupIndex = 0;
		return;
	}

	SelectedGroupIndex = FMath::Clamp(SelectedGroupIndex, 0, Representatives.Num() - 1);
}

void UProcessingStationMenuBase::ClampSelectedAttachmentState()
{
	if (!bAllowAttachmentSelection || !BoundStation.IsValid() || BoundStation->AttachmentSlots.IsEmpty())
	{
		SelectedAttachmentSlotIndex = 0;
		SelectedAttachmentOptionIndex = 0;
		SelectionMode = EProcessingStationMenuSelectionMode::Batches;
		return;
	}

	SelectedAttachmentSlotIndex = FMath::Clamp(SelectedAttachmentSlotIndex, 0, BoundStation->AttachmentSlots.Num() - 1);

	TArray<UBloodProcessingAttachmentDataAsset*> Options;
	GetCompatibleAttachmentOptions(Options);
	if (Options.IsEmpty())
	{
		SelectedAttachmentOptionIndex = 0;
		return;
	}

	SelectedAttachmentOptionIndex = FMath::Clamp(SelectedAttachmentOptionIndex, 0, Options.Num() - 1);
}

int32 UProcessingStationMenuBase::GetSelectedGroupUnits() const
{
	TArray<UBloodProductItem*> Representatives;
	TArray<int32> TotalUnits;
	GetCandidateGroups(Representatives, TotalUnits);
	if (Representatives.IsEmpty())
	{
		return 0;
	}

	const int32 SafeIndex = FMath::Clamp(SelectedGroupIndex, 0, TotalUnits.Num() - 1);
	return TotalUnits.IsValidIndex(SafeIndex) ? TotalUnits[SafeIndex] : 0;
}

UBloodProductItem* UProcessingStationMenuBase::GetSelectedRepresentative() const
{
	TArray<UBloodProductItem*> Representatives;
	TArray<int32> TotalUnits;
	GetCandidateGroups(Representatives, TotalUnits);
	if (Representatives.IsEmpty())
	{
		return nullptr;
	}

	const int32 SafeIndex = FMath::Clamp(SelectedGroupIndex, 0, Representatives.Num() - 1);
	return Representatives[SafeIndex];
}

void UProcessingStationMenuBase::GetCompatibleAttachmentOptions(TArray<UBloodProcessingAttachmentDataAsset*>& OutOptions) const
{
	OutOptions.Reset();

	if (!bAllowAttachmentSelection || !BoundStation.IsValid() || !BoundStation->AttachmentSlots.IsValidIndex(SelectedAttachmentSlotIndex))
	{
		return;
	}

	const FProcessingStationAttachmentSlot& AttachmentSlot = BoundStation->AttachmentSlots[SelectedAttachmentSlotIndex];
	for (UBloodProcessingAttachmentDataAsset* AttachmentOption : BoundStation->AvailableAttachmentOptions)
	{
		if (!AttachmentOption || !AttachmentOption->AttachmentTag.IsValid())
		{
			continue;
		}

		if (!AttachmentSlot.AllowedAttachmentTags.IsEmpty() && !AttachmentSlot.AllowedAttachmentTags.HasTagExact(AttachmentOption->AttachmentTag))
		{
			continue;
		}

		OutOptions.Add(AttachmentOption);
	}
}

const FProcessingStationAttachmentSlot* UProcessingStationMenuBase::GetSelectedAttachmentSlot() const
{
	return BoundStation.IsValid() && BoundStation->AttachmentSlots.IsValidIndex(SelectedAttachmentSlotIndex)
		? &BoundStation->AttachmentSlots[SelectedAttachmentSlotIndex]
		: nullptr;
}

UBloodProcessingAttachmentDataAsset* UProcessingStationMenuBase::GetSelectedAttachmentOption() const
{
	TArray<UBloodProcessingAttachmentDataAsset*> Options;
	GetCompatibleAttachmentOptions(Options);
	return Options.IsValidIndex(SelectedAttachmentOptionIndex) ? Options[SelectedAttachmentOptionIndex] : nullptr;
}

FText UProcessingStationMenuBase::ResolveOverrideText(const FText& OverrideText, const FText& DefaultText) const
{
	return OverrideText.IsEmpty() ? DefaultText : OverrideText;
}

FText UProcessingStationMenuBase::ResolveConfiguredTitleText() const
{
	if (!MenuTitleOverride.IsEmpty())
	{
		return MenuTitleOverride;
	}

	return BoundStation.IsValid()
		? BoundStation->StationDisplayName
		: LOCTEXT("DefaultTitle", "Processing Station");
}

bool UProcessingStationMenuBase::ShouldShowBatchSelection() const
{
	return bShowBatchSelection;
}

bool UProcessingStationMenuBase::ShouldShowProcessingProgress() const
{
	return bShowProcessingProgress;
}

FText UProcessingStationMenuBase::GetStateText() const
{
	if (!BoundStation.IsValid())
	{
		return LOCTEXT("UnknownState", "Onbekend");
	}

	switch (BoundStation->StationState)
	{
	case EBloodVatStationState::Leeg:
		return LOCTEXT("StateEmpty", "Leeg");
	case EBloodVatStationState::Rijpt:
		return LOCTEXT("StateAging", "Bezig");
	case EBloodVatStationState::Klaar:
		return LOCTEXT("StateReady", "Klaar");
	default:
		return LOCTEXT("StateFallback", "Onbekend");
	}
}

FText UProcessingStationMenuBase::GetPrimaryActionText() const
{
	if (!BoundStation.IsValid())
	{
		return LOCTEXT("ActionFallback", "Actie");
	}

	switch (BoundStation->StationState)
	{
	case EBloodVatStationState::Leeg:
		if (IsPackagingStation(BoundStation.Get()))
		{
			if (BoundStation->HasStagedManualProcessingRequest())
			{
				if (const ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(BoundStation.Get()))
				{
					if (PackagingStation->GetPlacedPackagingItemCount() >= PackagingStation->GetRequiredPackagingItemCount())
					{
						return LOCTEXT("ActionPackagingBoxFull", "Doos is vol");
					}
				}

				return BoundStation->HasSpawnedManualProcessingActor()
					? LOCTEXT("ActionPackagingItemReady", "Item ligt op tafel")
					: LOCTEXT("ActionPackagingRespawn", "Zet item opnieuw klaar");
			}

			return ResolveOverrideText(IdlePrimaryActionTextOverride, LOCTEXT("ActionPackagingPrepare", "Zet item klaar op tafel"));
		}

		if (BoundStation->HasStagedManualProcessingRequest())
		{
			return ResolveOverrideText(
				IdlePrimaryActionTextOverride,
				BoundStation->HasSpawnedManualProcessingActor()
					? LOCTEXT("ActionAwaitPlacement", "Plaats item op station")
					: LOCTEXT("ActionRespawnJug", "Zet item opnieuw klaar"));
		}

		return ResolveOverrideText(IdlePrimaryActionTextOverride, LOCTEXT("ActionPreparePlacement", "Zet batch klaar op tafel"));
	case EBloodVatStationState::Rijpt:
		return LOCTEXT("ActionOccupied", "Station bezet");
	case EBloodVatStationState::Klaar:
		return ResolveOverrideText(ReadyPrimaryActionTextOverride, LOCTEXT("ActionHarvest", "Neem batch"));
	default:
		return LOCTEXT("ActionUnknown", "Actie");
	}
}

FText UProcessingStationMenuBase::BuildRuleText() const
{
	if (!BoundStation.IsValid())
	{
		return FText::GetEmpty();
	}

	const UBloodProcessingRecipeDataAsset* ActiveRecipe = BoundStation->GetActiveRecipe();
	const int32 MinimumUnits = BoundStation->MinimumUnitsRequired;
	if (ActiveRecipe)
	{
		const FText AcceptedInputText = ActiveRecipe->bAcceptAnyInputProcessing
			? LOCTEXT("RuleTextAnyProcessing", "alle soorten")
			: UBloodProductItem::GetProcessingDisplayName(ActiveRecipe->RequiredInputProcessing);

		return FText::Format(
			LOCTEXT("RuleTextFmt", "Alleen {0} bloed. Minimaal {1} units, kwaliteit {2} of hoger."),
			AcceptedInputText,
			FText::AsNumber(MinimumUnits),
			UBloodProductItem::GetQualityDisplayName(ActiveRecipe->MinimumQuality));
	}

	return FText::Format(
		LOCTEXT("RuleTextFallbackFmt", "Minimaal {0} units vereist."),
		FText::AsNumber(MinimumUnits));
}

FText UProcessingStationMenuBase::BuildRecipeSummaryText() const
{
	if (!BoundStation.IsValid())
	{
		return FText::GetEmpty();
	}

	const UBloodProcessingRecipeDataAsset* ActiveRecipe = BoundStation->GetActiveRecipe();
	if (!ActiveRecipe)
	{
		return LOCTEXT("NoRecipeSummary", "Geen actieve recipe.");
	}

	const FText RecipeName = !ActiveRecipe->RecipeName.IsEmpty()
		? ActiveRecipe->RecipeName
		: LOCTEXT("UnnamedRecipeSummary", "Onbenoemde recipe");

	return FText::Format(
		LOCTEXT("RecipeSummarySimpleFmt", "Verwerking: {0}"),
		RecipeName);
}

FText UProcessingStationMenuBase::BuildStatusBannerTitle() const
{
	if (!BoundStation.IsValid())
	{
		return FText::GetEmpty();
	}

	switch (BoundStation->StationState)
	{
	case EBloodVatStationState::Leeg:
		if (IsPackagingStation(BoundStation.Get()))
		{
			if (const ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(BoundStation.Get()))
			{
				if (PackagingStation->GetPlacedPackagingItemCount() >= PackagingStation->GetRequiredPackagingItemCount())
				{
					return LOCTEXT("BannerPackagingFullTitle", "Packaging voltooid");
				}
			}

				return BoundStation->HasStagedManualProcessingRequest()
					? ResolveOverrideText(PreparedBannerTitleOverride, LOCTEXT("BannerPackagingPreparedTitle", "Packaging-item klaar"))
					: ResolveOverrideText(EmptyBannerTitleOverride, LOCTEXT("BannerPackagingIdleTitle", "Packaging klaar"));
			}

			return BoundStation->HasStagedManualProcessingRequest()
				? ResolveOverrideText(PreparedBannerTitleOverride, LOCTEXT("BannerJugReadyTitle", "Item staat klaar"))
				: ResolveOverrideText(EmptyBannerTitleOverride, LOCTEXT("BannerEmptyTitle", "Station gereed"));
		case EBloodVatStationState::Rijpt:
			return ResolveOverrideText(BusyBannerTitleOverride, LOCTEXT("BannerAgingTitle", "Verwerking actief"));
		case EBloodVatStationState::Klaar:
			return ResolveOverrideText(ReadyBannerTitleOverride, LOCTEXT("BannerReadyTitle", "Verwerking voltooid"));
	default:
		return FText::GetEmpty();
	}
}

FText UProcessingStationMenuBase::BuildStatusBannerSubtitle() const
{
	if (!BoundStation.IsValid())
	{
		return FText::GetEmpty();
	}

	switch (BoundStation->StationState)
	{
	case EBloodVatStationState::Leeg:
		if (IsPackagingStation(BoundStation.Get()))
		{
			if (const ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(BoundStation.Get()))
			{
				const FText ProgressText = FText::Format(
					LOCTEXT("PackagingProgressFmt", "In doos: {0}/{1}."),
					FText::AsNumber(PackagingStation->GetPlacedPackagingItemCount()),
					FText::AsNumber(PackagingStation->GetRequiredPackagingItemCount()));

				if (PackagingStation->GetPlacedPackagingItemCount() >= PackagingStation->GetRequiredPackagingItemCount())
				{
					return FText::Format(
						LOCTEXT("BannerPackagingFullSubtitle", "{0} De batch wordt nu direct verpakt."),
						ProgressText);
				}

					return BoundStation->HasStagedManualProcessingRequest()
						? ResolveOverrideText(
							PreparedBannerSubtitleOverride,
							BoundStation->HasSpawnedManualProcessingActor()
								? FText::Format(LOCTEXT("BannerPackagingPreparedSubtitle", "{0} Er ligt nu 1 packaging-item op tafel."), ProgressText)
								: FText::Format(LOCTEXT("BannerPackagingRespawnSubtitle", "{0} Er ligt geen actief item op tafel. Zet opnieuw 1 item klaar."), ProgressText))
						: ResolveOverrideText(
							EmptyBannerSubtitleOverride,
							FText::Format(LOCTEXT("BannerPackagingIdleSubtitle", "{0} Kies een geldige batch en zet daarna 1 packaging-item op tafel."), ProgressText));
				}
			}

			return BoundStation->HasStagedManualProcessingRequest()
				? ResolveOverrideText(PreparedBannerSubtitleOverride, LOCTEXT("BannerJugReadySubtitle", "De geselecteerde batch staat klaar als handmatig world-item. Plaats die nu fysiek op de stationtarget."))
				: ResolveOverrideText(EmptyBannerSubtitleOverride, LOCTEXT("BannerEmptySubtitle", "Kies een geldige batch en zet die daarna klaar voor de stationflow."));
		case EBloodVatStationState::Rijpt:
			return ResolveOverrideText(
				BusyBannerSubtitleOverride,
				FText::Format(
					LOCTEXT("BannerAgingSubtitle", "{0} units worden verwerkt. Klaar op dag {1}."),
					FText::AsNumber(BoundStation->StoredBatch.BloodQuantity),
					FText::AsNumber(BoundStation->ProcessingReadyDay)));
		case EBloodVatStationState::Klaar:
			return ResolveOverrideText(
				ReadyBannerSubtitleOverride,
				FText::Format(
					LOCTEXT("BannerReadySubtitle", "{0} units verwerkt bloed wachten op ophalen."),
					FText::AsNumber(BoundStation->StoredBatch.BloodQuantity)));
	default:
		return FText::GetEmpty();
	}
}

FText UProcessingStationMenuBase::BuildBatchListText() const
{
	if (BoundStation.IsValid() && BoundStation->StationState != EBloodVatStationState::Leeg)
	{
		return LOCTEXT("BatchListHiddenWhileBusy", "Het station is bezet. Er kunnen nu geen nieuwe batches geselecteerd worden.");
	}

	TArray<UBloodProductItem*> Representatives;
	TArray<int32> TotalUnits;
	GetCandidateGroups(Representatives, TotalUnits);

	FString Result = TEXT("Beschikbare batches\n\n");
	if (Representatives.IsEmpty())
	{
		Result += TEXT("Geen bloodgroepen beschikbaar.");
		return FText::FromString(Result);
	}

	for (int32 Index = 0; Index < Representatives.Num(); ++Index)
	{
		const UBloodProductItem* Item = Representatives[Index];
		if (!Item || !BoundStation.IsValid())
		{
			continue;
		}

		const int32 Units = TotalUnits.IsValidIndex(Index) ? TotalUnits[Index] : 0;
		FText Reason;
		const bool bValid = BoundStation->CanAcceptBloodItem(Item, Units, Reason);
		const FString StatusText = bValid
			? TEXT("Klaar")
			: FString::Printf(TEXT("Niet klaar: %s"), *Reason.ToString());
		const FString SelectedMarker = Index == SelectedGroupIndex ? TEXT("  < geselecteerd") : TEXT("");

		Result += FString::Printf(TEXT("%d. %s | %s | %d units | %s%s\n"),
			Index + 1,
			*UBloodProductItem::GetSourceDisplayName(Item->SourceType).ToString(),
			*UBloodProductItem::GetQualityDisplayName(Item->BaseQuality).ToString(),
			Units,
			*StatusText,
			*SelectedMarker);
	}

	return FText::FromString(Result);
}

bool UProcessingStationMenuBase::HandleCancelPackaging(FText& OutReason)
{
	if (!BoundStation.IsValid() || !BoundEconomy)
	{
		OutReason = LOCTEXT("CancelPackagingMissingStation", "Packaging context ontbreekt.");
		return false;
	}

	ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(BoundStation.Get());
	if (!PackagingStation || !PackagingStation->HasStagedManualProcessingRequest())
	{
		OutReason = FText::GetEmpty();
		return false;
	}

	return BoundEconomy->RequestCancelManualProcessing(PackagingStation, OutReason);
}

FText UProcessingStationMenuBase::BuildBatchRowNameText(const UBloodProductItem* BloodItem) const
{
	if (!BloodItem)
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		LOCTEXT("BatchRowNameFmt", "{0} bloed - {1}"),
		UBloodProductItem::GetSourceDisplayName(BloodItem->SourceType),
		UBloodProductItem::GetProcessingDisplayName(BloodItem->ProcessingType));
}

FText UProcessingStationMenuBase::BuildBatchRowMetaText(const UBloodProductItem* BloodItem) const
{
	if (!BoundStation.IsValid() || !BloodItem)
	{
		return FText::GetEmpty();
	}

	const int32 DurationDays = BoundStation->GetProcessingDurationDaysForBlood(BloodItem);
	const FText NightUnitText = DurationDays == 1
		? LOCTEXT("BatchRowMetaNightSingular", "nacht")
		: LOCTEXT("BatchRowMetaNightPlural", "nachten");

	return FText::Format(
		LOCTEXT("BatchRowMetaFmt", "{0} · {1} {2}"),
		UBloodProductItem::GetQualityDisplayName(BloodItem->BaseQuality),
		FText::AsNumber(DurationDays),
		NightUnitText);
}

FText UProcessingStationMenuBase::BuildBatchRowTagText(const bool bIsValid) const
{
	return bIsValid
		? LOCTEXT("BatchRowTagValid", "GELDIG")
		: LOCTEXT("BatchRowTagInvalid", "ONGELDIG");
}

FText UProcessingStationMenuBase::BuildDetailHeaderText() const
{
	if (!BoundStation.IsValid())
	{
		return LOCTEXT("DetailHeaderDefault", "Geselecteerde batch");
	}

	return BoundStation->StationState == EBloodVatStationState::Klaar
		? LOCTEXT("DetailHeaderFinished", "Voltooide batch")
		: GetSelectedRepresentative()
			? FText::Format(
				LOCTEXT("DetailHeaderSelectedIndexed", "Geselecteerde batch ({0})"),
				FText::AsNumber(SelectedGroupIndex + 1))
			: LOCTEXT("DetailHeaderSelected", "Geselecteerde batch");
}

FText UProcessingStationMenuBase::BuildSelectedBatchText() const
{
	if (!BoundStation.IsValid())
	{
		return FText::GetEmpty();
	}

	if (BoundStation->StationState != EBloodVatStationState::Leeg)
	{
		return FText::Format(
			LOCTEXT("StoredBatchFmt", "{0} bloed - {1} - {2}"),
			UBloodProductItem::GetSourceDisplayName(BoundStation->StoredBatch.SourceType),
			UBloodProductItem::GetQualityDisplayName(BoundStation->StoredBatch.BaseQuality),
			UBloodProductItem::GetProcessingDisplayName(BoundStation->StoredBatch.ProcessingType));
	}

	if (const ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(BoundStation.Get()))
	{
		if (const FBloodProcessingStartRequest* ReservedRequest = PackagingStation->GetReservedPackagingRequest())
		{
			return FText::Format(
				LOCTEXT("ReservedPackagingBatchFmt", "{0} bloed - {1} - {2}"),
				UBloodProductItem::GetSourceDisplayName(ReservedRequest->SourceType),
				UBloodProductItem::GetQualityDisplayName(ReservedRequest->BaseQuality),
				UBloodProductItem::GetProcessingDisplayName(ReservedRequest->ProcessingType));
		}
	}

	if (const UBloodProductItem* SelectedItem = GetSelectedRepresentative())
	{
		return FText::Format(
			LOCTEXT("SelectedBatchFmt", "{0} bloed - {1} - {2}"),
			UBloodProductItem::GetSourceDisplayName(SelectedItem->SourceType),
			UBloodProductItem::GetQualityDisplayName(SelectedItem->BaseQuality),
			UBloodProductItem::GetProcessingDisplayName(SelectedItem->ProcessingType));
	}

	return LOCTEXT("NoSelectedBatch", "Nog geen batch geselecteerd.");
}

FText UProcessingStationMenuBase::BuildValidationText() const
{
	if (!BoundStation.IsValid())
	{
		return FText::GetEmpty();
	}

	if (BoundStation->StationState == EBloodVatStationState::Rijpt)
	{
		return LOCTEXT("ValidationBusy", "Dit station is bezig met verwerken.");
	}

	if (BoundStation->StationState == EBloodVatStationState::Klaar)
	{
		return LOCTEXT("ValidationReady", "Verwerkte batch - klaar om op te halen.");
	}

	if (BoundStation->HasStagedManualProcessingRequest())
	{
		if (IsPackagingStation(BoundStation.Get()))
		{
			if (const ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(BoundStation.Get()))
			{
				if (PackagingStation->GetPlacedPackagingItemCount() >= PackagingStation->GetRequiredPackagingItemCount())
				{
					return LOCTEXT("ValidationPackagingFull", "De doos is gevuld. De batch wordt nu verpakt.");
				}
			}

			return BoundStation->HasSpawnedManualProcessingActor()
				? LOCTEXT("ValidationPackagingPrepared", "Er ligt nu 1 packaging-item op tafel. Je kunt nog geen tweede klaarzetten tot dit item gebruikt is.")
				: LOCTEXT("ValidationPackagingRespawn", "De packaging-selectie staat klaar, maar er ligt geen item meer op tafel. Zet opnieuw 1 item klaar.");
		}

		return BoundStation->HasSpawnedManualProcessingActor()
			? LOCTEXT("ValidationAwaitPlacement", "De batch staat als gevulde jug op tafel. Plaats de jug nu in het vat om de rijping te starten.")
			: LOCTEXT("ValidationRespawnPlacement", "De batch is voorbereid, maar er staat geen jug meer klaar. Zet de jug opnieuw op tafel.");
	}

	if (const UBloodProcessingRecipeDataAsset* ActiveRecipe = BoundStation->GetActiveRecipe())
	{
		FText RequirementReason;
		if (!BoundStation->DoesRecipeMeetStationRequirements(ActiveRecipe, RequirementReason))
		{
			return RequirementReason;
		}
	}

	const UBloodProductItem* SelectedItem = GetSelectedRepresentative();
	if (!SelectedItem)
	{
		TArray<UBloodProductItem*> Representatives;
		TArray<int32> TotalUnits;
		GetCandidateGroups(Representatives, TotalUnits);
		if (Representatives.IsEmpty())
		{
			return LOCTEXT("ValidationNoInventoryBatches", "Geen blood batches aanwezig in de inventory. Vind meer bloed.");
		}

		return LOCTEXT("ValidationSelectBatch", "Kies hierboven een batch om de verwerkingseisen te controleren.");
	}

	FText Reason;
	return BoundStation->CanAcceptBloodItem(SelectedItem, GetSelectedGroupUnits(), Reason)
		? LOCTEXT("ValidationValid", "Geldig. Deze batch kan direct worden verwerkt.")
		: Reason;
}

FText UProcessingStationMenuBase::BuildProcessFactText() const
{
	if (!BoundStation.IsValid() || BoundStation->StationState != EBloodVatStationState::Rijpt)
	{
		return FText::GetEmpty();
	}

	if (const UBloodProcessingRecipeDataAsset* ActiveRecipe = BoundStation->GetActiveRecipe();
		ActiveRecipe && !ActiveRecipe->ProcessFactText.IsEmpty())
	{
		return ActiveRecipe->ProcessFactText;
	}

	return LOCTEXT("DefaultProcessFact", "Deze verwerking verandert de batch in een waardevoller product.");
}

float UProcessingStationMenuBase::BuildProcessingProgress01() const
{
	if (!BoundStation.IsValid())
	{
		return 0.0f;
	}

	if (BoundStation->StationState == EBloodVatStationState::Klaar)
	{
		return 1.0f;
	}

	if (BoundStation->StationState != EBloodVatStationState::Rijpt)
	{
		return 0.0f;
	}

	return BoundStation->GetProcessingProgress01(GetOwnSystemAccumulatedTime(this));
}

FText UProcessingStationMenuBase::BuildProcessingProgressPctText() const
{
	if (!BoundStation.IsValid() || BoundStation->StationState != EBloodVatStationState::Rijpt)
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		LOCTEXT("ProcessingProgressPctFmt", "{0}%"),
		FText::AsNumber(FMath::RoundToInt(BuildProcessingProgress01() * 100.0f)));
}

FText UProcessingStationMenuBase::BuildProcessingElapsedText() const
{
	if (!BoundStation.IsValid() || BoundStation->StationState != EBloodVatStationState::Rijpt)
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		LOCTEXT("ProcessingElapsedFmt", "Verstreken: {0}"),
		FormatProcessingDurationText(BoundStation->GetProcessingElapsedTimeUnits(GetOwnSystemAccumulatedTime(this))));
}

FText UProcessingStationMenuBase::BuildProcessingRemainingText() const
{
	if (!BoundStation.IsValid())
	{
		return FText::GetEmpty();
	}

	if (BoundStation->StationState == EBloodVatStationState::Klaar)
	{
		return LOCTEXT("ProcessingRemainingDone", "Resterend: 00:00");
	}

	if (BoundStation->StationState != EBloodVatStationState::Rijpt)
	{
		return FText::GetEmpty();
	}

	return FText::Format(
		LOCTEXT("ProcessingRemainingFmt", "Resterend: {0}"),
		FormatProcessingDurationText(BoundStation->GetProcessingRemainingTimeUnits(GetOwnSystemAccumulatedTime(this))));
}

FText UProcessingStationMenuBase::BuildSelectedUnitsText() const
{
	if (!BoundStation.IsValid())
	{
		return FText::GetEmpty();
	}

	if (BoundStation->StationState != EBloodVatStationState::Leeg)
	{
		return FText::Format(
			LOCTEXT("SelectedUnitsFmt", "{0} units"),
			FText::AsNumber(BoundStation->StoredBatch.BloodQuantity));
	}

	if (const ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(BoundStation.Get()))
	{
		if (const FBloodProcessingStartRequest* ReservedRequest = PackagingStation->GetReservedPackagingRequest())
		{
			return FText::Format(
				LOCTEXT("ReservedPackagingUnitsFmt", "{0} units gereserveerd. In doos: {1}/{2}."),
				FText::AsNumber(ReservedRequest->BloodQuantity),
				FText::AsNumber(PackagingStation->GetPlacedPackagingItemCount()),
				FText::AsNumber(PackagingStation->GetRequiredPackagingItemCount()));
		}
	}

	if (GetSelectedRepresentative())
	{
		return FText::Format(
			LOCTEXT("SelectedUnitsFmt", "{0} units beschikbaar. Minimum {1}."),
			FText::AsNumber(GetSelectedGroupUnits()),
			FText::AsNumber(BoundStation->MinimumUnitsRequired));
	}

	return FText::GetEmpty();
}

FText UProcessingStationMenuBase::BuildSelectedMetaText() const
{
	if (!BoundStation.IsValid())
	{
		return FText::GetEmpty();
	}

	int32 DurationDays = BoundStation->GetProcessingDurationDays();
	if (BoundStation->StationState == EBloodVatStationState::Leeg)
	{
		if (const ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(BoundStation.Get()))
		{
			if (const FBloodProcessingStartRequest* ReservedRequest = PackagingStation->GetReservedPackagingRequest())
			{
				const int32 ReservedDurationDays = BoundStation->GetProcessingDurationDaysForSource(ReservedRequest->SourceType);
				const FText ReservedNightUnitText = ReservedDurationDays == 1
					? LOCTEXT("ReservedMetaNightSingular", "nacht")
					: LOCTEXT("ReservedMetaNightPlural", "nachten");

				return FText::Format(
					LOCTEXT("ReservedPackagingMetaFmt", "Gereserveerde batch. Verwerkingstijd: {0} {1}"),
					FText::AsNumber(ReservedDurationDays),
					ReservedNightUnitText);
			}
		}

		const UBloodProductItem* SelectedItem = GetSelectedRepresentative();
		if (!SelectedItem)
		{
			return FText::GetEmpty();
		}

		DurationDays = BoundStation->GetProcessingDurationDaysForBlood(SelectedItem);
	}
	else
	{
		DurationDays = BoundStation->GetStoredBatchProcessingDurationDays();
	}

	const FText NightUnitText = DurationDays == 1
		? LOCTEXT("SelectedMetaNightSingular", "nacht")
		: LOCTEXT("SelectedMetaNightPlural", "nachten");

	if (const UBloodProcessingRecipeDataAsset* ActiveRecipe = BoundStation->GetActiveRecipe();
		BoundStation->StationState == EBloodVatStationState::Leeg && ActiveRecipe)
	{
		const FText CostText = ActiveRecipe->GoldCost > 0
			? FText::Format(
				LOCTEXT("SelectedMetaCostFmt", "{0} goud"),
				FText::AsNumber(ActiveRecipe->GoldCost))
			: LOCTEXT("SelectedMetaNoCost", "geen goud");

		return FText::Format(
			LOCTEXT("SelectedMetaWithCostFmt", "Verwerkingstijd: {0} {1}. Kost: {2}."),
			FText::AsNumber(DurationDays),
			NightUnitText,
			CostText);
	}

	return FText::Format(
		LOCTEXT("SelectedMetaFmt", "Verwerkingstijd: {0} {1}"),
		FText::AsNumber(DurationDays),
		NightUnitText);
}

FSlateColor UProcessingStationMenuBase::BuildValidationColor() const
{
	const FLinearColor SuccessColor(0.24f, 0.52f, 0.20f, 1.0f);
	const FLinearColor FailureColor(0.55f, 0.14f, 0.10f, 0.85f);

	if (!BoundStation.IsValid())
	{
		return FSlateColor(FailureColor);
	}

	if (BoundStation->StationState == EBloodVatStationState::Klaar)
	{
		return FSlateColor(SuccessColor);
	}

	if (BoundStation->StationState == EBloodVatStationState::Rijpt)
	{
		return FSlateColor(FailureColor);
	}

	if (BoundStation->HasStagedManualProcessingRequest())
	{
		return FSlateColor(SuccessColor);
	}

	const UBloodProductItem* SelectedItem = GetSelectedRepresentative();
	if (!SelectedItem)
	{
		return FSlateColor(FailureColor);
	}

	FText Reason;
	const bool bValid = BoundStation->CanAcceptBloodItem(SelectedItem, GetSelectedGroupUnits(), Reason);
	return FSlateColor(bValid ? SuccessColor : FailureColor);
}

FText UProcessingStationMenuBase::BuildFooterHelpText() const
{
	if (!BoundStation.IsValid())
	{
		return FText::GetEmpty();
	}

	switch (BoundStation->StationState)
	{
	case EBloodVatStationState::Leeg:
		if (IsPackagingStation(BoundStation.Get()))
		{
			if (const ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(BoundStation.Get()))
			{
				if (PackagingStation->GetPlacedPackagingItemCount() >= PackagingStation->GetRequiredPackagingItemCount())
				{
					return LOCTEXT("FooterHelpPackagingFull", "De doos is gevuld. De batch wordt nu direct verpakt.");
				}
			}

					return BoundStation->HasStagedManualProcessingRequest()
						? ResolveOverrideText(
							PreparedFooterHelpOverride,
							BoundStation->HasSpawnedManualProcessingActor()
								? LOCTEXT("FooterHelpPackagingPrepared", "Er ligt nu 1 item op tafel. Plaats dit in een vrij doos-slot om de teller te verhogen. Druk C om deze packaging-run te annuleren.")
								: LOCTEXT("FooterHelpPackagingRespawn", "Het actieve packaging-item ontbreekt. Zet opnieuw 1 item klaar op tafel, of druk C om de gereserveerde batch te annuleren."))
						: ResolveOverrideText(
							EmptyFooterHelpOverride,
							LOCTEXT("FooterHelpPackagingEmpty", "Gebruik W/S of Q/E om een batch te kiezen. Zet daarna 1 packaging-item klaar op tafel."));
			}

		if (BoundStation->HasStagedManualProcessingRequest())
		{
			return BoundStation->HasSpawnedManualProcessingActor()
				? ResolveOverrideText(PreparedFooterHelpOverride, LOCTEXT("FooterHelpAwaitPlacement", "Pak het voorbereide station-item van tafel en plaats het op de stationtarget om de verwerking echt te starten."))
				: ResolveOverrideText(PreparedFooterHelpOverride, LOCTEXT("FooterHelpRespawnPlacement", "De voorbereiding staat nog klaar, maar het actieve item ontbreekt. Zet het item opnieuw op tafel."));
		}

		return ResolveOverrideText(EmptyFooterHelpOverride, LOCTEXT("FooterHelpEmpty", "Gebruik W/S of Q/E om batches te wisselen. Zet daarna de gekozen batch klaar voor de stationflow."));
	case EBloodVatStationState::Rijpt:
			return ResolveOverrideText(BusyFooterHelpOverride, LOCTEXT("FooterHelpAging", "Het station is bezet. Kom later terug om de verwerking te controleren."));
		case EBloodVatStationState::Klaar:
			return ResolveOverrideText(ReadyFooterHelpOverride, LOCTEXT("FooterHelpReady", "Gebruik de primaire knop om de verwerkte batch terug te nemen."));
	default:
		return FText::GetEmpty();
	}
}

bool UProcessingStationMenuBase::HandleStartAging(FText& OutReason)
{
	if (!BoundStation.IsValid() || !BoundInventory || !BoundEconomy)
	{
		OutReason = LOCTEXT("NoBarrelContext", "Processing station context ontbreekt.");
		return false;
	}

	if (BoundStation->HasStagedManualProcessingRequest())
	{
		if (IsPackagingStation(BoundStation.Get()))
		{
			if (ABloodPackagingStation* PackagingStation = Cast<ABloodPackagingStation>(BoundStation.Get()))
			{
				if (PackagingStation->GetPlacedPackagingItemCount() >= PackagingStation->GetRequiredPackagingItemCount())
				{
					OutReason = LOCTEXT("PackagingBoxAlreadyFull", "De doos is al gevuld. Rond eerst deze packaging-actie af.");
					return false;
				}
			}

			if (BoundStation->HasSpawnedManualProcessingActor())
			{
				OutReason = LOCTEXT("PackagingRequestAlreadySpawned", "Er ligt al 1 packaging-item op tafel. Zet eerst dit item weg voordat je een nieuw item klaarzet.");
				return false;
			}

			return BoundEconomy && BoundEconomy->RequestPrepareManualProcessing(BoundStation.Get(), nullptr, 0, true, OutReason);
		}

		if (BoundStation->HasSpawnedManualProcessingActor())
		{
			OutReason = LOCTEXT("ManualRequestAlreadyStaged", "Deze batch staat al als gevulde jug op tafel. Plaats de jug nu in het vat.");
			return false;
		}
	}

	UBloodProductItem* SelectedItem = GetSelectedRepresentative();
	if (!SelectedItem)
	{
		OutReason = LOCTEXT("NoBatchForAging", "Geen geldige batch geselecteerd.");
		return false;
	}

	if (IsPackagingStation(BoundStation.Get()))
	{
		return BoundEconomy && BoundEconomy->RequestPrepareManualProcessing(BoundStation.Get(), SelectedItem, GetSelectedGroupUnits(), false, OutReason);
	}

	return BoundEconomy && BoundEconomy->RequestPrepareManualProcessing(BoundStation.Get(), SelectedItem, GetSelectedGroupUnits(), false, OutReason);
}

bool UProcessingStationMenuBase::HandleHarvest(FText& OutReason)
{
	if (!BoundStation.IsValid() || !BoundInventory || !BoundEconomy)
	{
		OutReason = LOCTEXT("NoHarvestContext", "Houten vat context ontbreekt.");
		return false;
	}

	return BoundEconomy->RequestHarvestProcessedBatch(BoundStation.Get(), OutReason);
}

bool UProcessingStationMenuBase::HandleAttachmentAction(FText& OutReason)
{
	if (!BoundStation.IsValid())
	{
		OutReason = LOCTEXT("NoAttachmentContext", "Attachment context ontbreekt.");
		return false;
	}

	if (const FProcessingStationAttachmentSlot* SelectedAttachmentSlot = GetSelectedAttachmentSlot())
	{
		if (SelectedAttachmentSlot->PlacedAttachment)
		{
			return BoundStation->RemoveAttachmentFromSlotByIndex(SelectedAttachmentSlotIndex, OutReason);
		}

		if (UBloodProcessingAttachmentDataAsset* AttachmentOption = GetSelectedAttachmentOption())
		{
			return BoundStation->PlaceAttachmentInSlotByIndex(SelectedAttachmentSlotIndex, AttachmentOption, OutReason);
		}
	}

	OutReason = LOCTEXT("NoAttachmentActionAvailable", "Geen attachment actie beschikbaar.");
	return false;
}

void UProcessingStationMenuBase::RebuildBatchRowList()
{
	UScrollBox* ActiveBatchListContainer = BatchListContainer ? BatchListContainer.Get() : BatchListContainerFallback.Get();
	if (!ActiveBatchListContainer)
	{
		return;
	}

	ActiveBatchListContainer->ClearChildren();

	if (!BoundStation.IsValid() || BoundStation->StationState != EBloodVatStationState::Leeg || !BatchRowClass)
	{
		return;
	}

	TArray<UBloodProductItem*> Representatives;
	TArray<int32> TotalUnits;
	GetCandidateGroups(Representatives, TotalUnits);

	if (Representatives.IsEmpty())
	{
		UVampireBloodBatchRowWidget* EmptyRowWidget = CreateWidget<UVampireBloodBatchRowWidget>(GetOwningPlayer(), BatchRowClass);
		if (EmptyRowWidget)
		{
			ActiveBatchListContainer->AddChild(EmptyRowWidget);
			EmptyRowWidget->ConfigureRow(
				INDEX_NONE,
				LOCTEXT("NoInventoryBatchRowName", "Geen batch aanwezig"),
				LOCTEXT("NoInventoryBatchRowMeta", "Vind meer bloed"),
				FText::GetEmpty(),
				LOCTEXT("NoInventoryBatchRowTag", "LEEG"),
				false,
				false);
		}

		return;
	}

	for (int32 Index = 0; Index < Representatives.Num(); ++Index)
	{
		UBloodProductItem* Item = Representatives[Index];
		if (!Item)
		{
			continue;
		}

		const int32 Units = TotalUnits.IsValidIndex(Index) ? TotalUnits[Index] : 0;
		FText Reason;
		const bool bIsValid = BoundStation->CanAcceptBloodItem(Item, Units, Reason);

		UVampireBloodBatchRowWidget* RowWidget = CreateWidget<UVampireBloodBatchRowWidget>(GetOwningPlayer(), BatchRowClass);
		if (!RowWidget)
		{
			continue;
		}

		ActiveBatchListContainer->AddChild(RowWidget);

		RowWidget->ConfigureRow(
			Index,
			BuildBatchRowNameText(Item),
			BuildBatchRowMetaText(Item),
			FText::Format(LOCTEXT("BatchRowUnitsFmt", "{0} units"), FText::AsNumber(Units)),
			BuildBatchRowTagText(bIsValid),
			bIsValid,
			Index == SelectedGroupIndex);
		RowWidget->OnBatchRowClicked.AddDynamic(this, &UProcessingStationMenuBase::HandleBatchRowClicked);
	}
}

void UProcessingStationMenuBase::UpdateButtonState()
{
	const ESlateVisibility BatchNavVisibility = ShouldShowBatchSelection() && BoundStation.IsValid() && BoundStation->StationState == EBloodVatStationState::Leeg
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed;

	SetResolvedButtonText(BtnPrevBatch, LOCTEXT("PrevBatchBtn", "Vorige batch"));
	SetResolvedButtonVisibility(BtnPrevBatch, BtnPrevBatchFallback, BatchNavVisibility);

	SetResolvedButtonText(BtnNextBatch, LOCTEXT("NextBatchBtn", "Volgende batch"));
	SetResolvedButtonVisibility(BtnNextBatch, BtnNextBatchFallback, BatchNavVisibility);

	SetResolvedButtonText(BtnPrimaryAction, GetPrimaryActionText());
	if (TxtPrimaryActionLabelFallback)
	{
		TxtPrimaryActionLabelFallback->SetText(GetPrimaryActionText());
	}
	SetResolvedButtonText(BtnClose, LOCTEXT("CloseBtn", "Sluiten"));
}

#undef LOCTEXT_NAMESPACE
