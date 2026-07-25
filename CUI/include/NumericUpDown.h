#pragma once
#include "RangeBase.h"

/**
 * @file NumericUpDown.h
 * @brief NumericUpDown：TextBox + Up/Down 步进按钮形态的数值输入控件。
 */

class NumericUpDown : public RangeBase
{
protected:
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
	float _cornerRadius = 6.0f;
	float _buttonWidth = 28.0f;
	float _focusBorder = 1.6f;
	D2D1_COLOR_F _buttonBackColor = cui::theme::palette::SurfaceMuted;
	D2D1_COLOR_F _buttonHoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _buttonPressedColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F _selectedBackColor = cui::theme::palette::SelectionBack;
	D2D1_COLOR_F _selectedForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _mutedTextColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F _disabledOverlayColor = cui::theme::palette::DisabledOverlay;
	std::wstring _editText;
	bool _editing = false;
	bool _dragText = false;
	bool _dragUp = false;
	bool _dragDown = false;
	int _hoverButton = 0;
	float _hoverProgress = 0.0f;
	float _targetHoverProgress = 0.0f;
	ULONGLONG _animStartTick = 0;
	float _animStartProgress = 0.0f;
	bool _animating = false;
	UINT _animDurationMs = 120;
	D2D1_RECT_F _caretRectCache = { 0,0,0,0 };
	bool _caretRectCacheValid = false;
	int _selectionStart = 0;
	int _selectionEnd = 0;
	float _horizontalScrollOffset = 0.0f;
	D2D1_SIZE_F _textSize = { 0,0 };

	struct UndoRecord
	{
		int pos = 0;
		std::wstring removedText;
		std::wstring insertedText;
		int selStartBefore = 0;
		int selEndBefore = 0;
		int selStartAfter = 0;
		int selEndAfter = 0;
	};

	std::vector<UndoRecord> undoStack;
	std::vector<UndoRecord> redoStack;
	bool isApplyingUndoRedo = false;

	void SyncTextFromValue();
	bool TryParseEditText(const std::wstring& text, double& value) const;
	bool IsEditTextAllowed(const std::wstring& text) const;
	bool CommitEdit();
	void CancelEdit();
	void BeginEdit(bool selectAll);
	void SelectAllText();
	void InputText(std::wstring input);
	void InputBack();
	void InputDelete();
	void UpdateScroll(bool arrival = false);
	void ApplyUndoRecord(const UndoRecord& rec, bool isUndo);
	void Undo();
	void Redo();
	std::wstring GetSelectedString();
	D2D1_RECT_F ButtonPanelRect() const;
	D2D1_RECT_F UpButtonRect() const;
	D2D1_RECT_F DownButtonRect() const;
	D2D1_RECT_F TextRect();
	int HitTestButton(int localX, int localY) const;
	int HitTestTextPosition(int localX, int localY);
	void StepBy(int direction, bool accelerated = false);
	void StartHoverAnimation(float target);
	float CurrentHoverProgress();
	std::wstring FormatValue() const;

public:
	UIClass Type() override;
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
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
	bool HandlesMouseWheel() const override { return _useMouseWheel; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(Key key) const override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
protected:
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
	double CoerceRangeValue(double value) const override;
	void OnRangeValueChanged(double oldValue, double newValue) override;
};
