#pragma once
#include "RangeBase.h"

class ButtonBase;
class TextBox;

/**
 * @file NumericUpDown.h
 * @brief NumericUpDown：TextBox + Up/Down 步进按钮形态的数值输入控件。
 */

class NumericUpDown : public RangeBase
{
protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<RangeBaseAutomationPeer>(
			*this, AutomationControlType::Spinner, L"NumericUpDown");
	}

private:
	double _increment = 1.0;
	int _decimalPlaces = 0;
	bool _isSnapToIncrementEnabled = true;
	bool _selectAllOnFocus = true;
	bool _useMouseWheel = true;
	std::wstring _editText;
	bool _editing = false;
	bool _synchronizingText = false;
	int _capturedSpinDirection = 0;
	TextBox* _textBoxPart = nullptr;
	ButtonBase* _increaseButtonPart = nullptr;
	ButtonBase* _decreaseButtonPart = nullptr;

	void SyncTextFromValue();
	bool TryParseEditText(const std::wstring& text, double& value) const;
	bool IsEditTextAllowed(const std::wstring& text) const;
	bool CommitEdit();
	void CancelEdit();
	void BeginEdit(bool selectAll);
	int HitTestSpinButton(int localX, int localY) const noexcept;
	void StepBy(int direction);
	std::wstring FormatValue() const;
	void OnEditorTextChanged(TextChangedEventArgs& args);
	void OnEditorKeyDown(KeyEventArgs& args);
	void OnEditorMouseWheel(MouseEventArgs& args);

public:
	UIClass Type() override;
	static const DependencyProperty& IncrementProperty();
	static const DependencyProperty& DecimalPlacesProperty();
	static const DependencyProperty& IsSnapToIncrementEnabledProperty();
	static const DependencyProperty& SelectAllOnFocusProperty();
	static const DependencyProperty& UseMouseWheelProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	NumericUpDown();
	bool ApplyTextInput(const TextCompositionEventArgs& input) override;
	bool TryGetTextInputCaretRect(D2D1_RECT_F& outRect) override;

	PROPERTY(double, Increment);
	GET(double, Increment);
	SET(double, Increment);

	PROPERTY(int, DecimalPlaces);
	GET(int, DecimalPlaces);
	SET(int, DecimalPlaces);

	PROPERTY(bool, IsSnapToIncrementEnabled);
	GET(bool, IsSnapToIncrementEnabled);
	SET(bool, IsSnapToIncrementEnabled);

	PROPERTY(bool, SelectAllOnFocus);
	GET(bool, SelectAllOnFocus);
	SET(bool, SelectAllOnFocus);

	PROPERTY(bool, UseMouseWheel);
	GET(bool, UseMouseWheel);
	SET(bool, UseMouseWheel);
	CursorKind QueryCursor(int localX, int localY) override;
	bool ShouldHitTestChildrenAt(
		int localX, int localY) const override
	{
		(void)localX;
		(void)localY;
		return true;
	}
	bool HandlesMouseWheel() const override { return _useMouseWheel; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(Key key) const override;
protected:
	bool ProcessInput(const InputReport& input) override;
	double CoerceRangeValue(double value) const override;
	void OnRangeValueChanged(double oldValue, double newValue) override;
	void OnControlTemplatePresentationChanged() override;
};
