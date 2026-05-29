#pragma once

#include "CoreMinimal.h"
#include "Vampire/BloodTypes.h"
#include "Widgets/OwnSystemMenu.h"
#include "VampireEconomySummaryMenu.generated.h"

class UOwnSystemCommonTextBlock;
class UOwnSystemCommonButtonBase;
class UOwnSystemInventoryComponent;
class UVampireEconomyComponent;
class UBloodProductItem;
class UPlaceableStationItem;
class AWorkspaceRoom;
class ABloodBuyerNPC;
class APawn;

UCLASS(Blueprintable)
class VAMPIREEMPIRE_API UVampireEconomySummaryMenu : public UOwnSystemMenu
{
	GENERATED_BODY()

public:
	UVampireEconomySummaryMenu();

	UFUNCTION(BlueprintCallable, Category = "Vampire|UI")
	void SetBuyerContext(ABloodBuyerNPC* InBuyer, APawn* InInteractor);

	UFUNCTION(BlueprintCallable, Category = "Vampire|UI")
	void RefreshSummary();

	UFUNCTION(BlueprintPure, Category = "Vampire|UI")
	FText BuildSummaryText() const;

	UFUNCTION(BlueprintPure, Category = "Vampire|UI")
	FText BuildBloodItemsText() const;

	UFUNCTION(BlueprintPure, Category = "Vampire|UI")
	FText BuildThrallListText() const;

	UFUNCTION(BlueprintPure, Category = "Vampire|UI")
	FText BuildSelectedBloodItemText() const;

	UFUNCTION(BlueprintPure, Category = "Vampire|UI")
	FText BuildSelectedThrallText() const;

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
	virtual UWidget* NativeGetDesiredFocusTarget() const override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtEconomySummary;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtBloodItems;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtThralls;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtSelectedBloodItem;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonTextBlock> TxtSelectedThrall;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnPrevBloodItem;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnNextBloodItem;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnProcessSelected;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnPrevThrall;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnNextThrall;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnProcessDailyUpkeep;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnAddThrall;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UOwnSystemCommonButtonBase> BtnClose;

private:
	UFUNCTION()
	void HandleInventoryUpdated();

	UFUNCTION()
	void HandleCurrencyChanged(int32 OldCurrency, int32 NewCurrency);

	UFUNCTION()
	void HandleEconomyUpdated();

	void HandleProcessDailyUpkeepClicked();
	void HandleAddThrallClicked();
	void HandlePrevBloodItemClicked();
	void HandleNextBloodItemClicked();
	void HandleProcessSelectedClicked();
	void HandlePrevThrallClicked();
	void HandleNextThrallClicked();

	void HandleCloseClicked();

	static UOwnSystemInventoryComponent* ResolveInventory(const UUserWidget* Widget);
	static UVampireEconomyComponent* ResolveEconomy(const UUserWidget* Widget);
	static int32 CountBloodItems(const UOwnSystemInventoryComponent* Inventory);
	TArray<UBloodProductItem*> GetBloodItems() const;
	UBloodProductItem* GetSelectedBloodItem() const;
	void ClampSelectedBloodItemIndex();
	bool IsInBuyerContext() const;
	void GetBuyerCandidateGroups(TArray<UBloodProductItem*>& OutRepresentatives, TArray<int32>& OutTotalUnits) const;
	int32 GetSelectedBuyerGroupUnits() const;
	int32 EstimateBuyerGroupPayout(const UBloodProductItem* Representative, int32 TotalUnits) const;
	FText GetBuyerPrimaryActionText() const;
	void UpdateButtonStateForCurrentContext();
	bool HandleBuyerSellAction();
	bool HandleDebugPlaceFirstStation();
	UPlaceableStationItem* GetFirstPlaceableStationItem() const;
	AWorkspaceRoom* FindFirstWorkspaceRoom() const;
	int32 GetThrallCount() const;
	const FThrallUpkeepUnit* GetSelectedThrall() const;
	void ClampSelectedThrallIndex();

	UPROPERTY(Transient)
	TObjectPtr<UOwnSystemInventoryComponent> BoundInventory;

	UPROPERTY(Transient)
	TObjectPtr<UVampireEconomyComponent> BoundEconomy;

	UPROPERTY(Transient)
	TWeakObjectPtr<ABloodBuyerNPC> BoundBuyer;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> BoundBuyerInteractor;

	UPROPERTY(Transient)
	int32 SelectedBloodItemIndex = 0;

	UPROPERTY(Transient)
	int32 SelectedThrallIndex = 0;
};
