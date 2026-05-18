#pragma once
#include "Control.h"
#include "ColorPickerPopup.h"

/**
 * @file ColorPicker.h
 * @brief ColorPicker：带下拉颜色面板的颜色选择输入框。
 */

typedef Event<void(class ColorPicker*, D2D1_COLOR_F oldColor, D2D1_COLOR_F newColor, std::wstring value)> ColorPickerColorChangedEvent;
typedef Event<void(class ColorPicker*)> ColorPickerDropDownEvent;

class ColorPicker : public Control
{
private:
	D2D1_COLOR_F _selectedColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 1.0f };
	D2D1_COLOR_F _popupStartColor = _selectedColor;
	ColorPickerPopup* _popup = nullptr;
	bool _hovered = false;
	bool _popupOpen = false;
	float _hoverProgress = 0.0f;
	float _hoverStartProgress = 0.0f;
	float _hoverTargetProgress = 0.0f;
	ULONGLONG _hoverAnimStartTick = 0;
	bool _hoverAnimating = false;
	UINT _hoverAnimDurationMs = 120;

	ColorPickerPopup* EnsurePopup();
	void ApplyPopupTheme(ColorPickerPopup* popup);
	void OpenPopup();
	void ClosePopup(bool confirm);
	void SetSelectedColorInternal(D2D1_COLOR_F color, bool fireEvent);
	float CurrentHoverProgress();
	void StartHoverAnimation(float target);
	D2D1_RECT_F SwatchRect() const;
	D2D1_RECT_F ArrowRect() const;

protected:
	bool DefaultTrackUnderMouse() const override { return true; }

public:
	UIClass Type() override;
	ColorPicker(int x = 0, int y = 0, int width = 180, int height = 30);
	~ColorPicker() override;

	ColorPickerColorChangedEvent OnColorChanged;
	ColorPickerDropDownEvent OnDropDownOpened;
	ColorPickerDropDownEvent OnDropDownClosed;

	float Border = 1.0f;
	float CornerRadius = 6.0f;
	float SwatchSize = 18.0f;
	float TextPaddingX = 8.0f;
	float ButtonWidth = 28.0f;
	float FocusBorder = 1.6f;
	bool UpdateOnPreview = true;
	bool RevertPreviewOnCancel = true;
	int PopupWidth = 450;
	int PopupHeight = 430;

	D2D1_COLOR_F PanelBackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F PanelHoverColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.08f };
	D2D1_COLOR_F ButtonBackColor = D2D1_COLOR_F{ 0.92f, 0.94f, 0.98f, 0.95f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.95f };
	D2D1_COLOR_F FocusBorderColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.80f };
	D2D1_COLOR_F MutedTextColor = Colors::DimGrey;
	D2D1_COLOR_F DisabledOverlayColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };

	PROPERTY(D2D1_COLOR_F, SelectedColor);
	GET(D2D1_COLOR_F, SelectedColor);
	SET(D2D1_COLOR_F, SelectedColor);

	PROPERTY(std::wstring, ValueText);
	GET(std::wstring, ValueText);
	SET(std::wstring, ValueText);

	static bool TryParseColor(const std::wstring& text, D2D1_COLOR_F& out);
	static std::wstring ColorToString(D2D1_COLOR_F color);

	CursorKind QueryCursor(int xof, int yof) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
