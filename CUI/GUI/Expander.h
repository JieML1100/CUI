#pragma once
#include "Panel.h"

/**
 * @file Expander.h
 * @brief Expander：带标题栏和展开动画的可折叠容器。
 */

typedef Event<void(class Expander*, bool expanded)> ExpanderExpandedChangedEvent;

class Expander : public Panel
{
private:
	bool _isExpanded = true;
	bool _hoverHeader = false;
	float _expandProgress = 1.0f;
	float _animStartProgress = 1.0f;
	float _animTargetProgress = 1.0f;
	ULONGLONG _animStartTick = 0;
	bool _animating = false;

	float CurrentExpandProgress();
	void PerformExpanderLayoutIfNeeded();
	bool HeaderHitTest(int localX, int localY) const;
	void SetExpandedInternal(bool value, bool fireEvent);

public:
	UIClass Type() override;
	Expander();
	Expander(std::wstring text, int x, int y, int width, int height);

	ExpanderExpandedChangedEvent OnExpandedChanged;

	float HeaderHeight = 36.0f;
	float HeaderPaddingX = 12.0f;
	float ChevronSize = 13.0f;
	float Border = 1.0f;
	float CornerRadius = 7.0f;
	UINT AnimationDurationMs = 160;

	D2D1_COLOR_F SurfaceColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.90f };
	D2D1_COLOR_F HeaderBackColor = D2D1_COLOR_F{ 0.94f, 0.96f, 0.99f, 0.96f };
	D2D1_COLOR_F HeaderHoverBackColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.10f };
	D2D1_COLOR_F ContentBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };
	D2D1_COLOR_F AccentColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.95f };
	D2D1_COLOR_F MutedTextColor = Colors::DimGrey;
	D2D1_COLOR_F DisabledOverlayColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };

	PROPERTY(bool, IsExpanded);
	GET(bool, IsExpanded);
	SET(bool, IsExpanded);

	void SetExpanded(bool value);
	void Toggle();

	SIZE ActualSize() override;
	CursorKind QueryCursor(int localX, int localY) override;
	bool ShouldHitTestChildrenAt(int localX, int localY) const override;
	D2D1_RECT_F GetChildrenClipRect() override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override;
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};
