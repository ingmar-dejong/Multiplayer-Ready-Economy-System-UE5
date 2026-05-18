#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "VampireBloodBatchRowWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBloodBatchRowClicked, int32, RowIndex);

UCLASS(Blueprintable)
class VAMPIREEMPIRE_API UVampireBloodBatchRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Vampire|UI")
	void ConfigureRow(int32 InRowIndex, const FText& InBatchName, const FText& InBatchMeta, const FText& InUnitsText, const FText& InTagText, bool bInIsValid, bool bInIsSelected);

	UPROPERTY(BlueprintAssignable, Category = "Vampire|UI")
	FOnBloodBatchRowClicked OnBatchRowClicked;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Style")
	FLinearColor ValidAccentColor = FLinearColor(0.24f, 0.52f, 0.20f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Style")
	FLinearColor InvalidAccentColor = FLinearColor(0.55f, 0.14f, 0.10f, 0.85f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Style")
	FLinearColor SelectedBorderColor = FLinearColor(0.75f, 0.61f, 0.31f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Style")
	FLinearColor IdleBorderColor = FLinearColor(0.20f, 0.17f, 0.14f, 0.95f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Style")
	FLinearColor SelectedOutlineColor = FLinearColor(0.88f, 0.74f, 0.43f, 1.0f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vampire|UI|Style")
	FLinearColor IdleOutlineColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UButton> Button;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UImage> ImgValidationDot;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UTextBlock> TxtBatchName;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UTextBlock> TxtBatchMeta;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UTextBlock> TxtTag;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Vampire|UI")
	TObjectPtr<UTextBlock> TxtUnits;

private:
	UFUNCTION()
	void HandleButtonClicked();

	void ResolveWidgetRefs();
	void ApplyStateVisuals(bool bInIsValid, bool bInIsSelected);

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BorderRowBackground;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BorderTagBackground;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> BorderSelectionOutline;

	UPROPERTY(Transient)
	int32 RowIndex = INDEX_NONE;
};
