#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Widgets/OwnSystemMenu.h"
#include "VampireBarrelMenu.generated.h"

class ABloodProcessingStation;
class APawn;
struct FProcessingStationAttachmentSlot;
class UBloodProductItem;
class UButton;
class UImage;
class UBloodProcessingAttachmentDataAsset;
class UOwnSystemCommonButtonBase;
class UOwnSystemCommonTextBlock;
class UOwnSystemInventoryComponent;
class UProgressBar;
class UScrollBox;
class UTextBlock;
class UWidgetSwitcher;
class UVampireBloodBatchRowWidget;
class UVampireEconomyComponent;

enum class EProcessingStationMenuSelectionMode : uint8
{
	Batches,
	Attachments
};

UCLASS(Blueprintable)
class VAMPIREEMPIRE_API UProcessingStationMenuBase : public UOwnSystemMenu
{
	GENERATED_BODY()

public:
	UProcessingStationMenuBase();

	UFUNCTION(BlueprintCallable, Category = "Vampire|UI")
	void SetProcessingStationContext(ABloodProcessingStation* InStation, APawn* InInteractor);

	UFUNCTION(BlueprintCallable, Category = "Vampire|UI")
	void RefreshMenu();

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtState;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtRule;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtDuration;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtStatusBannerTitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtStatusBannerSubtitle;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtBatchList;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtDetailHeader;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtSelectedBatch;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtSelectedUnits;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtSelectedMeta;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtValidation;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtProcessFact;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UProgressBar> ProgressBar_Processing;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> Txt_ProgressPct;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> Txt_Elapsed;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> Txt_Remaining;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtFooterHelp;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UImage> ImgValidationDot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UScrollBox> BatchListContainer;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnPrevBatch;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnNextBatch;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnPrimaryAction;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnClose;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI")
	TSubclassOf<UVampireBloodBatchRowWidget> BatchRowClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText MenuTitleOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText IdlePrimaryActionTextOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText ReadyPrimaryActionTextOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText EmptyBannerTitleOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText PreparedBannerTitleOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText BusyBannerTitleOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText ReadyBannerTitleOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText EmptyBannerSubtitleOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText PreparedBannerSubtitleOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText BusyBannerSubtitleOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText ReadyBannerSubtitleOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText EmptyFooterHelpOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText PreparedFooterHelpOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText BusyFooterHelpOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	FText ReadyFooterHelpOverride;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	bool bShowBatchSelection = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	bool bShowProcessingProgress = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	bool bAllowAttachmentSelection = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Config")
	bool bAutoCloseAfterSuccessfulHarvest = true;

private:
	UFUNCTION()
	void HandleInventoryUpdated();

	UFUNCTION()
	void HandleCurrencyChanged(int32 OldCurrency, int32 NewCurrency);

	UFUNCTION()
	void HandleEconomyUpdated();

	UFUNCTION()
	void HandlePrevBatchClickedFallback();

	UFUNCTION()
	void HandleNextBatchClickedFallback();

	UFUNCTION()
	void HandlePrimaryActionClickedFallback();

	UFUNCTION()
	void HandleCloseClickedFallback();

	UFUNCTION()
	void HandleBatchRowClicked(int32 ClickedIndex);

	UFUNCTION()
	void HandleLiveRefreshTick();

	void HandlePrevBatchClicked();
	void HandleNextBatchClicked();
	void HandlePrevRecipeClicked();
	void HandleNextRecipeClicked();
	void HandleToggleSelectionMode();
	void HandlePrevAttachmentSlot();
	void HandleNextAttachmentSlot();
	void HandlePrevAttachmentOption();
	void HandleNextAttachmentOption();
	bool HandleCancelPackaging(FText& OutReason);
	void HandlePrimaryActionClicked();
	void HandleCloseClicked();

	static UOwnSystemInventoryComponent* ResolveInventory(const UUserWidget* Widget);
	static UVampireEconomyComponent* ResolveEconomy(const UUserWidget* Widget);
	void GetCandidateGroups(TArray<UBloodProductItem*>& OutRepresentatives, TArray<int32>& OutTotalUnits) const;
	void ClampSelectedGroupIndex();
	void ClampSelectedAttachmentState();
	int32 GetSelectedGroupUnits() const;
	UBloodProductItem* GetSelectedRepresentative() const;
	void GetCompatibleAttachmentOptions(TArray<UBloodProcessingAttachmentDataAsset*>& OutOptions) const;
	const FProcessingStationAttachmentSlot* GetSelectedAttachmentSlot() const;
	UBloodProcessingAttachmentDataAsset* GetSelectedAttachmentOption() const;
	FText GetStateText() const;
	FText GetPrimaryActionText() const;
	FText BuildRuleText() const;
	FText BuildRecipeSummaryText() const;
	FText BuildStatusBannerTitle() const;
	FText BuildStatusBannerSubtitle() const;
	FText BuildBatchListText() const;
	FText BuildBatchRowNameText(const UBloodProductItem* BloodItem) const;
	FText BuildBatchRowMetaText(const UBloodProductItem* BloodItem) const;
	FText BuildBatchRowTagText(bool bIsValid) const;
	FText BuildDetailHeaderText() const;
	FText BuildSelectedBatchText() const;
	FText BuildSelectedUnitsText() const;
	FText BuildSelectedMetaText() const;
	FText BuildValidationText() const;
	FText BuildProcessFactText() const;
	float BuildProcessingProgress01() const;
	FText BuildProcessingProgressPctText() const;
	FText BuildProcessingElapsedText() const;
	FText BuildProcessingRemainingText() const;
	FSlateColor BuildValidationColor() const;
	FText BuildFooterHelpText() const;
	FText ResolveOverrideText(const FText& OverrideText, const FText& DefaultText) const;
	FText ResolveConfiguredTitleText() const;
	bool ShouldShowBatchSelection() const;
	bool ShouldShowProcessingProgress() const;
	bool HandleStartAging(FText& OutReason);
	bool HandleHarvest(FText& OutReason);
	bool HandleAttachmentAction(FText& OutReason);
	void ResolveImportedWidgetRefs();
	void ResolveActiveStateWidgetRefs();
	void RebuildBatchRowList();
	void BindFallbackButtons();
	void UnbindFallbackButtons();
	void UpdateButtonState();

	UPROPERTY(Transient)
	TObjectPtr<UWidgetSwitcher> StateSwitcher;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtTitleFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtStateFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtRuleFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtDurationFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtStatusBannerTitleFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtStatusBannerSubtitleFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtBatchListFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtDetailHeaderFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtSelectedBatchFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtSelectedUnitsFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtSelectedMetaFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtValidationFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtProcessFactFallback;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ProgressBarProcessingFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtProgressPctFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtElapsedFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtRemainingFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtFooterHelpFallback;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ImgValidationDotFallback;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> BatchListContainerFallback;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BtnPrevBatchFallback;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BtnNextBatchFallback;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BtnPrimaryActionFallback;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TxtPrimaryActionLabelFallback;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BtnCloseFallback;

	UPROPERTY(Transient)
	TWeakObjectPtr<ABloodProcessingStation> BoundStation;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> BoundInteractor;

	UPROPERTY(Transient)
	TObjectPtr<UOwnSystemInventoryComponent> BoundInventory;

	UPROPERTY(Transient)
	TObjectPtr<UVampireEconomyComponent> BoundEconomy;

	UPROPERTY(Transient)
	int32 SelectedGroupIndex = 0;

	EProcessingStationMenuSelectionMode SelectionMode = EProcessingStationMenuSelectionMode::Batches;

	int32 SelectedAttachmentSlotIndex = 0;

	int32 SelectedAttachmentOptionIndex = 0;

	bool bPendingCloseAfterSuccessfulHarvest = false;

	FTimerHandle LiveRefreshTimerHandle;
};

UCLASS(Blueprintable)
class VAMPIREEMPIRE_API UVampireBarrelMenu : public UProcessingStationMenuBase
{
	GENERATED_BODY()

public:
	UVampireBarrelMenu();
};
