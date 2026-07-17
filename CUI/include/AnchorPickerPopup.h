#pragma once
#include "Control.h"
#include "Layout/LayoutTypes.h"

class Form;

typedef Event<void(class AnchorPickerPopup*, uint8_t anchors, std::wstring value)> AnchorPickerValueEvent;
typedef Event<void(class AnchorPickerPopup*)> AnchorPickerEvent;

/** WinForms-style visual editor for the four combinable AnchorStyles edges. */
class AnchorPickerPopup : public Control
{
public:
	UIClass Type() override { return UIClass::UI_CUSTOM; }
	AnchorPickerPopup(int width = 244, int height = 260);

	uint8_t SelectedAnchors = AnchorStyles::None;
	D2D1_COLOR_F PanelBackColor = D2D1_COLOR_F{ 0.12f, 0.12f, 0.13f, 0.98f };
	D2D1_COLOR_F PanelBorderColor = D2D1_COLOR_F{ 0.35f, 0.36f, 0.38f, 1.0f };
	D2D1_COLOR_F TextColor = D2D1_COLOR_F{ 0.88f, 0.88f, 0.88f, 1.0f };
	D2D1_COLOR_F MutedTextColor = D2D1_COLOR_F{ 0.56f, 0.57f, 0.60f, 1.0f };
	D2D1_COLOR_F ButtonBackColor = D2D1_COLOR_F{ 0.16f, 0.16f, 0.17f, 1.0f };
	D2D1_COLOR_F ButtonBorderColor = D2D1_COLOR_F{ 0.54f, 0.54f, 0.56f, 1.0f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.39f, 0.40f, 0.95f, 1.0f };

	AnchorPickerValueEvent OnAnchorChanged;
	AnchorPickerValueEvent OnAnchorConfirmed;
	AnchorPickerEvent OnCancelled;

	bool AutoCloseOnOutsideClick() const override { return true; }
	bool AutoCloseOnFormFocusLoss() const override { return true; }
	void ClosePopup() override { Hide(false); }
	SIZE ActualSize() override { return this->Size; }
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	bool ContainsPoint(int localX, int localY) override;
	CursorKind QueryCursor(int localX, int localY) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam,
		int localX, int localY) override;

	void ShowAt(Form* form, int x, int y, uint8_t initialAnchors);
	void ShowAt(Control* relativeTo, const D2D1_RECT_F& anchorRect,
		uint8_t initialAnchors);
	void Hide(bool confirm);
	void SetSelectedAnchors(uint8_t anchors);
	bool ToggleAnchor(uint8_t edge);
	std::wstring CurrentValueText() const;
	float CurrentDropProgress();

	static bool TryParseAnchors(const std::wstring& text, uint8_t& out);
	static std::wstring AnchorToString(uint8_t anchors);

private:
	struct Layout
	{
		D2D1_RECT_F DiagramPanelRect{ 0,0,0,0 };
		D2D1_RECT_F ParentRect{ 0,0,0,0 };
		D2D1_RECT_F ChildRect{ 0,0,0,0 };
		D2D1_RECT_F EdgeRects[4]{};
		D2D1_RECT_F SummaryRect{ 0,0,0,0 };
		D2D1_RECT_F NoneRect{ 0,0,0,0 };
		D2D1_RECT_F OkRect{ 0,0,0,0 };
	};

	SIZE _preferredSize{ 244, 260 };
	bool _visiblePopup = false;
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
	int _hoverEdge = -1;
	int _focusEdge = 0;
	bool _hoverNone = false;
	bool _hoverOk = false;

	Layout CalcLayout() const;
	void SetExpanded(bool expanded);
	void FinishCollapsed();
	void UpdateHover(int localX, int localY);
	void Confirm();
	void NotifyChanged();
};
