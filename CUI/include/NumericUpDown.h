#pragma once
#include "Control.h"

/**
 * @file NumericUpDown.h
 * @brief NumericUpDown：TextBox + Up/Down 步进按钮形态的数值输入控件。
 */

typedef Event<void(class NumericUpDown*, double oldValue, double newValue)> NumericUpDownValueChangedEvent;

class NumericUpDown : public Control
{
private:
	double _min = 0.0;
	double _max = 100.0;
	double _value = 0.0;
	double _step = 1.0;
	int _decimalPlaces = 0;
	bool _snapToStep = true;
	bool _selectAllOnFocus = true;
	bool _useMouseWheel = true;
	float _border = 1.0f;
	float _cornerRadius = 6.0f;
	float _buttonWidth = 28.0f;
	float _textPaddingX = 8.0f;
	float _focusBorder = 1.6f;
	D2D1_COLOR_F _panelBackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F _buttonBackColor = D2D1_COLOR_F{ 0.92f, 0.94f, 0.98f, 0.95f };
	D2D1_COLOR_F _buttonHoverColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.12f };
	D2D1_COLOR_F _buttonPressedColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.20f };
	D2D1_COLOR_F _accentColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.95f };
	D2D1_COLOR_F _focusBorderColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.80f };
	D2D1_COLOR_F _selectedBackColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.34f };
	D2D1_COLOR_F _selectedForeColor = Colors::White;
	D2D1_COLOR_F _mutedTextColor = Colors::DimGrey;
	D2D1_COLOR_F _disabledOverlayColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };
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

	double ClampValue(double value) const;
	double SnapValue(double value) const;
	void SetCurrentValue(double value);
	void ReevaluateValue();
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
	void UpdateImeCompositionWindow();
	D2D1_RECT_F ButtonPanelRect() const;
	D2D1_RECT_F UpButtonRect() const;
	D2D1_RECT_F DownButtonRect() const;
	D2D1_RECT_F TextRect() const;
	int HitTestButton(int localX, int localY) const;
	int HitTestTextPosition(int localX, int localY);
	void StepBy(int direction);
	void StartHoverAnimation(float target);
	float CurrentHoverProgress();
	std::wstring FormatValue() const;

public:
	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;
	NumericUpDown(int x = 0, int y = 0, int width = 140, int height = 30);

	NumericUpDownValueChangedEvent OnValueChanged;

	int SelectionStart = 0;
	int SelectionEnd = 0;
	float HorizontalScrollOffset = 0.0f;
	D2D1_SIZE_F textSize = { 0,0 };

	PROPERTY(double, Min);
	GET(double, Min);
	SET(double, Min);

	PROPERTY(double, Max);
	GET(double, Max);
	SET(double, Max);

	PROPERTY(double, Value);
	GET(double, Value);
	SET(double, Value);

	PROPERTY(double, Step);
	GET(double, Step);
	SET(double, Step);

	PROPERTY(int, DecimalPlaces);
	GET(int, DecimalPlaces);
	SET(int, DecimalPlaces);

	PROPERTY(bool, SnapToStep);
	GET(bool, SnapToStep);
	SET(bool, SnapToStep);

	PROPERTY(bool, SelectAllOnFocus);
	GET(bool, SelectAllOnFocus);
	SET(bool, SelectAllOnFocus);

	PROPERTY(bool, UseMouseWheel);
	GET(bool, UseMouseWheel);
	SET(bool, UseMouseWheel);

#define CUI_NUMERIC_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	SET(type, name)

	CUI_NUMERIC_PROPERTY(float, Border);
	CUI_NUMERIC_PROPERTY(float, CornerRadius);
	CUI_NUMERIC_PROPERTY(float, ButtonWidth);
	CUI_NUMERIC_PROPERTY(float, TextPaddingX);
	CUI_NUMERIC_PROPERTY(float, FocusBorder);
	CUI_NUMERIC_PROPERTY(D2D1_COLOR_F, PanelBackColor);
	CUI_NUMERIC_PROPERTY(D2D1_COLOR_F, ButtonBackColor);
	CUI_NUMERIC_PROPERTY(D2D1_COLOR_F, ButtonHoverColor);
	CUI_NUMERIC_PROPERTY(D2D1_COLOR_F, ButtonPressedColor);
	CUI_NUMERIC_PROPERTY(D2D1_COLOR_F, AccentColor);
	CUI_NUMERIC_PROPERTY(D2D1_COLOR_F, FocusBorderColor);
	CUI_NUMERIC_PROPERTY(D2D1_COLOR_F, SelectedBackColor);
	CUI_NUMERIC_PROPERTY(D2D1_COLOR_F, SelectedForeColor);
	CUI_NUMERIC_PROPERTY(D2D1_COLOR_F, MutedTextColor);
	CUI_NUMERIC_PROPERTY(D2D1_COLOR_F, DisabledOverlayColor);

#undef CUI_NUMERIC_PROPERTY

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return _useMouseWheel; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};
