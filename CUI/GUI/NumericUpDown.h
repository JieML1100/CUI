#pragma once
#include "Control.h"

/**
 * @file NumericUpDown.h
 * @brief NumericUpDown：带步进按钮的数值输入控件。
 */

typedef Event<void(class NumericUpDown*, double oldValue, double newValue)> NumericUpDownValueChangedEvent;

class NumericUpDown : public Control
{
private:
	double _min = 0.0;
	double _max = 100.0;
	double _value = 0.0;
	std::wstring _editText;
	bool _editing = false;
	bool _dragUp = false;
	bool _dragDown = false;
	int _hoverButton = 0;
	float _hoverProgress = 0.0f;
	float _targetHoverProgress = 0.0f;
	bool _selectAllPending = false;
	ULONGLONG _animStartTick = 0;
	float _animStartProgress = 0.0f;
	bool _animating = false;
	UINT _animDurationMs = 120;

	double ClampValue(double value) const;
	double SnapValue(double value) const;
	void SetValueInternal(double value, bool fireEvent);
	void SyncTextFromValue();
	bool CommitEdit();
	void CancelEdit();
	void BeginEdit();
	D2D1_RECT_F ButtonPanelRect() const;
	D2D1_RECT_F UpButtonRect() const;
	D2D1_RECT_F DownButtonRect() const;
	D2D1_RECT_F TextRect() const;
	int HitTestButton(int localX, int localY) const;
	void StepBy(int direction);
	void StartHoverAnimation(float target);
	float CurrentHoverProgress();
	std::wstring FormatValue() const;

public:
	UIClass Type() override;
	NumericUpDown(int x = 0, int y = 0, int width = 140, int height = 30);

	NumericUpDownValueChangedEvent OnValueChanged;

	float Border = 1.0f;
	float CornerRadius = 6.0f;
	float ButtonWidth = 28.0f;
	float TextPaddingX = 8.0f;
	float FocusBorder = 1.6f;
	int DecimalPlaces = 0;
	double Step = 1.0;
	bool SnapToStep = true;
	bool SelectAllOnFocus = true;
	bool UseMouseWheel = true;

	D2D1_COLOR_F PanelBackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F ButtonBackColor = D2D1_COLOR_F{ 0.92f, 0.94f, 0.98f, 0.95f };
	D2D1_COLOR_F ButtonHoverColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.12f };
	D2D1_COLOR_F ButtonPressedColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.20f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.95f };
	D2D1_COLOR_F FocusBorderColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.80f };
	D2D1_COLOR_F MutedTextColor = Colors::DimGrey;
	D2D1_COLOR_F DisabledOverlayColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };

	PROPERTY(double, Min);
	GET(double, Min);
	SET(double, Min);

	PROPERTY(double, Max);
	GET(double, Max);
	SET(double, Max);

	PROPERTY(double, Value);
	GET(double, Value);
	SET(double, Value);

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};
