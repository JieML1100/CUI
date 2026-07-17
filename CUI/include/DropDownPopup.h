#pragma once
#include "Control.h"

typedef Event<void(class DropDownPopup*, int selectedIndex, std::wstring selectedText)> DropDownPopupSelectionChangedEvent;
typedef Event<void(class DropDownPopup*)> DropDownPopupClosedEvent;

class DropDownPopup : public Control
{
public:
	UIClass Type() override;
	DropDownPopup();

	std::vector<std::wstring> Items;
	int SelectedIndex = -1;
	int HoveredIndex = -1;
	int MaxVisibleItems = 6;
	float ItemHeight = 26.0f;
	float MinWidth = 96.0f;
	float DropGap = 2.0f;
	float Border = 1.2f;
	float CornerRadius = 6.0f;
	float ItemCornerRadius = 4.0f;
	float HorizontalPadding = 8.0f;
	float VerticalPadding = 2.0f;
	float ScrollBarWidth = 6.0f;
	float ScrollTrackPadding = 5.0f;

	D2D1_COLOR_F DropBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F DropBorderColor = cui::theme::palette::Border;
	D2D1_COLOR_F AccentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F SelectedItemBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F SelectedItemForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F UnderMouseBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F UnderMouseForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F ScrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F ScrollForeColor = cui::theme::palette::ScrollThumb;

	DropDownPopupSelectionChangedEvent SelectionChanged;
	DropDownPopupClosedEvent Closed;

	bool IsOpen() const;
	float CurrentDropProgress();
	void ShowAt(class Form* form, Control* owner, const D2D1_RECT_F& anchorAbsRect,
		const std::vector<std::wstring>& items, int selectedIndex,
		float preferredWidth, float itemHeight, int maxVisibleItems = 6);
	void Hide(bool raiseClosed = true, bool immediate = false);
	void ClosePopup() override { Hide(true); }
	bool AutoCloseOnOutsideClick() const override { return true; }
	bool AutoCloseOnFormFocusLoss() const override { return true; }
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	SIZE ActualSize() override;
	CursorKind QueryCursor(int localX, int localY) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

private:
	Control* _owner = nullptr;
	D2D1_RECT_F _anchorAbsRect{ 0,0,0,0 };
	float _preferredWidth = 0.0f;
	int _visibleCount = 0;
	int _scrollOffset = 0;
	bool _dragScroll = false;
	float _scrollThumbGrabOffsetY = 0.0f;
	bool _expanded = false;
	float _dropProgress = 0.0f;
	float _animStartProgress = 0.0f;
	float _animTargetProgress = 0.0f;
	UINT64 _animStartTick = 0;
	UINT _animDurationMs = 160;
	bool _animating = false;
	bool _collapseCleanupPending = false;
	bool _raiseClosedAfterCollapse = false;

	int ItemCount() const;
	int VisibleItemCount() const;
	int MaxScrollOffset() const;
	bool HasScrollBar() const;
	float FullPopupHeight() const;
	float CurrentPopupHeight();
	void EnsureSelectionInRange();
	void EnsureScrollInRange();
	void EnsureSelectionVisible();
	void Reposition();
	void SetExpanded(bool expanded, bool raiseClosedAfterCollapse = true);
	void FinishCollapsed(bool raiseClosed);
	void ScrollBy(int deltaItems);
	void UpdateScrollByThumb(float localY);
	int HitTestItem(int localX, int localY) const;
	D2D1_RECT_F GetScrollTrackRect() const;
	D2D1_RECT_F GetScrollThumbRect() const;
	bool IsOverScrollBar(int localX, int localY) const;
	bool CommitSelection(int selectedIndex);
};
