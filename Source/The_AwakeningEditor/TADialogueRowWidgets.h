// Source/The_AwakeningEditor/TADialogueRowWidgets.h
// UMG 控件的动态委托包装：转播为普通多播委托，支持 AddLambda
// （UMG 的 OnClicked/OnTextChanged/OnSelectionChanged/OnCheckStateChanged 都是
//  DYNAMIC_MULTICAST 委托，只能 AddDynamic + UFUNCTION，不能直接绑 lambda。
//  注意：NativeConstruct 只有 UUserWidget 才有，普通 UWidget 子类要在构造函数里绑定）
#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ComboBoxString.h"
#include "Components/CheckBox.h"
#include "TADialogueRowWidgets.generated.h"

// ---------------- 按钮 ----------------

DECLARE_MULTICAST_DELEGATE(FOnTAButtonClickedNative);

UCLASS()
class THE_AWAKENINGEDITOR_API UTADialogueRowButton : public UButton
{
	GENERATED_BODY()

public:
	UTADialogueRowButton()
	{
		OnClicked.AddDynamic(this, &UTADialogueRowButton::HandleClicked);
	}

	/** 与 OnClicked 等价，但支持 AddLambda */
	FOnTAButtonClickedNative OnClickedNative;

protected:
	UFUNCTION()
	void HandleClicked()
	{
		OnClickedNative.Broadcast();
	}
};

// ---------------- 单行文本框 ----------------

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTATextChangedNative, const FText&);

UCLASS()
class THE_AWAKENINGEDITOR_API UTADialogueRowTextBox : public UEditableTextBox
{
	GENERATED_BODY()

public:
	UTADialogueRowTextBox()
	{
		OnTextChanged.AddDynamic(this, &UTADialogueRowTextBox::HandleTextChanged);
	}

	/** 与 OnTextChanged 等价，但支持 AddLambda */
	FOnTATextChangedNative OnTextChangedNative;

protected:
	UFUNCTION()
	void HandleTextChanged(const FText& InText)
	{
		OnTextChangedNative.Broadcast(InText);
	}
};

// ---------------- 多行文本框 ----------------

UCLASS()
class THE_AWAKENINGEDITOR_API UTADialogueRowMultiLineBox : public UMultiLineEditableTextBox
{
	GENERATED_BODY()

public:
	UTADialogueRowMultiLineBox()
	{
		OnTextChanged.AddDynamic(this, &UTADialogueRowMultiLineBox::HandleTextChanged);
	}

	/** 与 OnTextChanged 等价，但支持 AddLambda */
	FOnTATextChangedNative OnTextChangedNative;

protected:
	UFUNCTION()
	void HandleTextChanged(const FText& InText)
	{
		OnTextChangedNative.Broadcast(InText);
	}
};

// ---------------- 下拉框 ----------------

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnTAComboSelectionNative, FString, ESelectInfo::Type);

UCLASS()
class THE_AWAKENINGEDITOR_API UTADialogueRowCombo : public UComboBoxString
{
	GENERATED_BODY()

public:
	UTADialogueRowCombo()
	{
		// 动态委托要求 FString 按值传参
		OnSelectionChanged.AddDynamic(this, &UTADialogueRowCombo::ForwardSelectionChanged);
	}

	void SetRowFont(const FSlateFontInfo& InFontInfo)
	{
		InitFont(InFontInfo);
	}

	void SetRowForegroundColor(const FSlateColor& InForegroundColor)
	{
		InitForegroundColor(InForegroundColor);
	}

	/** 与 OnSelectionChanged 等价，但支持 AddLambda */
	FOnTAComboSelectionNative OnSelectionNative;

protected:
	virtual TSharedRef<SWidget> HandleGenerateWidget(TSharedPtr<FString> Item) const override;

	UFUNCTION()
	void ForwardSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
	{
		OnSelectionNative.Broadcast(SelectedItem, SelectionType);
	}
};

// ---------------- 复选框 ----------------

DECLARE_MULTICAST_DELEGATE_OneParam(FOnTACheckChangedNative, bool);

UCLASS()
class THE_AWAKENINGEDITOR_API UTADialogueRowCheckBox : public UCheckBox
{
	GENERATED_BODY()

public:
	UTADialogueRowCheckBox()
	{
		OnCheckStateChanged.AddDynamic(this, &UTADialogueRowCheckBox::HandleCheckStateChanged);
	}

	/** 与 OnCheckStateChanged 等价，但支持 AddLambda */
	FOnTACheckChangedNative OnCheckChangedNative;

protected:
	UFUNCTION()
	void HandleCheckStateChanged(bool bIsChecked)
	{
		OnCheckChangedNative.Broadcast(bIsChecked);
	}
};
