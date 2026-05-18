#include "UI/VampireBloodBatchRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"

namespace
{
	template <typename WidgetType>
	WidgetType* FindWidgetByPrefix(const UUserWidget* RootWidget, const FString& Prefix)
	{
		if (!RootWidget || Prefix.IsEmpty() || !RootWidget->WidgetTree)
		{
			return nullptr;
		}

		TArray<UWidget*> Widgets;
		RootWidget->WidgetTree->GetAllWidgets(Widgets);
		for (UWidget* Widget : Widgets)
		{
			if (WidgetType* TypedWidget = Cast<WidgetType>(Widget))
			{
				if (TypedWidget->GetName().StartsWith(Prefix, ESearchCase::IgnoreCase))
				{
					return TypedWidget;
				}
			}
		}

		return nullptr;
	}
}

void UVampireBloodBatchRowWidget::ConfigureRow(const int32 InRowIndex, const FText& InBatchName, const FText& InBatchMeta, const FText& InUnitsText, const FText& InTagText, const bool bInIsValid, const bool bInIsSelected)
{
	RowIndex = InRowIndex;

	ResolveWidgetRefs();

	if (IsValid(TxtBatchName))
	{
		TxtBatchName->SetText(InBatchName);
	}

	if (IsValid(TxtBatchMeta))
	{
		TxtBatchMeta->SetText(InBatchMeta);
	}

	if (IsValid(TxtUnits))
	{
		TxtUnits->SetText(InUnitsText);
	}

	if (IsValid(TxtTag))
	{
		TxtTag->SetText(InTagText);
	}

	ApplyStateVisuals(bInIsValid, bInIsSelected);
}

void UVampireBloodBatchRowWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ResolveWidgetRefs();

	if (IsValid(Button))
	{
		Button->OnClicked.AddDynamic(this, &UVampireBloodBatchRowWidget::HandleButtonClicked);
	}
}

void UVampireBloodBatchRowWidget::NativeDestruct()
{
	if (IsValid(Button))
	{
		Button->OnClicked.RemoveAll(this);
	}

	Super::NativeDestruct();
}

void UVampireBloodBatchRowWidget::HandleButtonClicked()
{
	OnBatchRowClicked.Broadcast(RowIndex);
}

void UVampireBloodBatchRowWidget::ResolveWidgetRefs()
{
	if (!Button)
	{
		Button = FindWidgetByPrefix<UButton>(this, TEXT("Button"));
	}

	if (!ImgValidationDot)
	{
		ImgValidationDot = FindWidgetByPrefix<UImage>(this, TEXT("ImgValidationDot"));
	}

	if (!TxtBatchName)
	{
		TxtBatchName = FindWidgetByPrefix<UTextBlock>(this, TEXT("TxtBatchName"));
	}

	if (!TxtBatchMeta)
	{
		TxtBatchMeta = FindWidgetByPrefix<UTextBlock>(this, TEXT("TxtBatchMeta"));
	}

	if (!TxtTag)
	{
		TxtTag = FindWidgetByPrefix<UTextBlock>(this, TEXT("TxtTag"));
	}

	if (!TxtUnits)
	{
		TxtUnits = FindWidgetByPrefix<UTextBlock>(this, TEXT("TxtUnits"));
	}

	if (!BorderRowBackground)
	{
		BorderRowBackground = FindWidgetByPrefix<UBorder>(this, TEXT("BorderRowBackground"));
		if (!BorderRowBackground)
		{
			BorderRowBackground = FindWidgetByPrefix<UBorder>(this, TEXT("Border-BatchRow"));
		}
	}

	if (!BorderTagBackground)
	{
		BorderTagBackground = FindWidgetByPrefix<UBorder>(this, TEXT("BorderTagBackground"));
		if (!BorderTagBackground)
		{
			BorderTagBackground = FindWidgetByPrefix<UBorder>(this, TEXT("Border-Tag"));
		}
	}

	if (!BorderSelectionOutline)
	{
		BorderSelectionOutline = FindWidgetByPrefix<UBorder>(this, TEXT("BorderSelectionOutline"));
	}
}

void UVampireBloodBatchRowWidget::ApplyStateVisuals(const bool bInIsValid, const bool bInIsSelected)
{
	const FLinearColor ValidationColor = bInIsValid ? ValidAccentColor : InvalidAccentColor;

	if (IsValid(ImgValidationDot))
	{
		ImgValidationDot->SetColorAndOpacity(ValidationColor);
	}

	if (IsValid(TxtTag))
	{
		TxtTag->SetColorAndOpacity(FSlateColor(FLinearColor::White));
	}

	if (IsValid(BorderTagBackground))
	{
		BorderTagBackground->SetBrushColor(FLinearColor(ValidationColor.R, ValidationColor.G, ValidationColor.B, 0.95f));
	}

	if (IsValid(BorderSelectionOutline))
	{
		BorderSelectionOutline->SetBrushColor(bInIsSelected ? SelectedOutlineColor : IdleOutlineColor);
	}

	if (IsValid(BorderRowBackground))
	{
		BorderRowBackground->SetBrushColor(bInIsSelected ? SelectedBorderColor : IdleBorderColor);
	}
}
