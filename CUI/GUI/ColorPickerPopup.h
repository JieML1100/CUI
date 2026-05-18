#pragma once
#include "Control.h"

class Form;

typedef Event<void(class ColorPickerPopup*, D2D1_COLOR_F color, std::wstring value)> ColorPickerValueEvent;
typedef Event<void(class ColorPickerPopup*)> ColorPickerEvent;

class ColorPickerPopup : public Control
{
public:
	UIClass Type() override { return UIClass::UI_CUSTOM; }
	ColorPickerPopup(int width = 450, int height = 430);

	D2D1_COLOR_F SelectedColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 1.0f };
	D2D1_COLOR_F PanelBackColor = D2D1_COLOR_F{ 0.12f, 0.12f, 0.13f, 0.98f };
	D2D1_COLOR_F PanelBorderColor = D2D1_COLOR_F{ 0.35f, 0.36f, 0.38f, 1.0f };
	D2D1_COLOR_F TextColor = D2D1_COLOR_F{ 0.88f, 0.88f, 0.88f, 1.0f };
	D2D1_COLOR_F MutedTextColor = D2D1_COLOR_F{ 0.56f, 0.57f, 0.60f, 1.0f };
	D2D1_COLOR_F ButtonBackColor = D2D1_COLOR_F{ 0.16f, 0.16f, 0.17f, 1.0f };
	D2D1_COLOR_F ButtonBorderColor = D2D1_COLOR_F{ 0.54f, 0.54f, 0.56f, 1.0f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.39f, 0.40f, 0.95f, 1.0f };

	ColorPickerValueEvent OnColorChanged;
	ColorPickerValueEvent OnColorConfirmed;
	ColorPickerEvent OnCleared;
	ColorPickerEvent OnCancelled;

	bool AutoCloseOnOutsideClick() const override { return true; }
	bool AutoCloseOnFormFocusLoss() const override { return true; }
	void ClosePopup() override { Hide(false); }
	SIZE ActualSize() override { return this->Size; }
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	bool ContainsPoint(int xof, int yof) override;
	CursorKind QueryCursor(int xof, int yof) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

	void ShowAt(Form* form, int x, int y, D2D1_COLOR_F initialColor);
	void ShowAt(Control* relativeTo, const D2D1_RECT_F& anchorRect, D2D1_COLOR_F initialColor);
	void Hide(bool confirm);
	std::wstring CurrentValueText() const;
	float CurrentDropProgress();

	static bool TryParseColor(const std::wstring& text, D2D1_COLOR_F& out);
	static std::wstring ColorToString(D2D1_COLOR_F color);
	static std::vector<std::wstring> CommonColorValues();
	static std::vector<std::wstring> HistoryColorValues();

private:
	struct Layout
	{
		D2D1_RECT_F SvRect{ 0,0,0,0 };
		D2D1_RECT_F HueRect{ 0,0,0,0 };
		D2D1_RECT_F AlphaRect{ 0,0,0,0 };
		D2D1_RECT_F InputRect{ 0,0,0,0 };
		D2D1_RECT_F ClearRect{ 0,0,0,0 };
		D2D1_RECT_F OkRect{ 0,0,0,0 };
		std::vector<D2D1_RECT_F> CommonRects;
		std::vector<D2D1_RECT_F> HistoryRects;
	};

	float _hue = 214.0f;
	float _saturation = 0.79f;
	float _value = 0.95f;
	float _alpha = 1.0f;
	bool _visiblePopup = false;
	bool _dragSV = false;
	bool _dragHue = false;
	bool _dragAlpha = false;
	int _hoverCommon = -1;
	int _hoverHistory = -1;
	bool _hoverClear = false;
	bool _hoverOk = false;
	SIZE _preferredSize{ 450, 430 };
	bool _expanded = false;
	float _dropProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	UINT64 _animStartTick = 0;
	UINT _animDurationMs = 180;
	bool _animating = false;
	bool _collapseCleanupPending = false;
	bool _hasAnchorRect = false;
	D2D1_RECT_F _anchorRect{ 0,0,0,0 };
	Control* _owner = nullptr;

	Layout CalcLayout() const;
	void SetExpanded(bool expanded);
	void FinishCollapsed();
	void SetFromColor(D2D1_COLOR_F color);
	void UpdateColorFromHsv();
	void SetSVFromPoint(int xof, int yof);
	void SetHueFromPoint(int xof, int yof);
	void SetAlphaFromPoint(int xof, int yof);
	void UpdateHover(int xof, int yof);
	void Confirm();
	void ClearValue();
	void AddHistory(const std::wstring& value);

	void DrawCheckerBoard(D2DGraphics* d2d, const D2D1_RECT_F& rect, float cellSize) const;
	void DrawSwatch(D2DGraphics* d2d, const D2D1_RECT_F& rect, D2D1_COLOR_F color, bool selected, bool hover) const;
	void DrawSV(D2DGraphics* d2d, const D2D1_RECT_F& rect) const;
	void DrawHue(D2DGraphics* d2d, const D2D1_RECT_F& rect) const;
	void DrawAlpha(D2DGraphics* d2d, const D2D1_RECT_F& rect) const;
};
