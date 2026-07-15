#pragma once
#include "Panel.h"

/**
 * @file Expander.h
 * @brief Expander：带标题栏和展开动画的可折叠容器。
 */

typedef Event<void(class Expander*, bool expanded)> ExpanderExpandedChangedEvent;

class Expander : public Panel
{
protected:
	void PerformPendingLayout() override;

private:
	bool _isExpanded = true;
	float _headerHeight = 36.0f;
	float _headerPaddingX = 12.0f;
	float _chevronSize = 13.0f;
	float _border = 1.0f;
	int _animationDurationMs = 160;
	D2D1_COLOR_F _surfaceColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.90f };
	D2D1_COLOR_F _headerBackColor = D2D1_COLOR_F{ 0.94f, 0.96f, 0.99f, 0.96f };
	D2D1_COLOR_F _headerHoverBackColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.10f };
	D2D1_COLOR_F _contentBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.42f };
	D2D1_COLOR_F _accentColor = D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.95f };
	D2D1_COLOR_F _mutedTextColor = Colors::DimGrey;
	bool _hoverHeader = false;
	float _expandProgress = 1.0f;
	float _animStartProgress = 1.0f;
	float _animTargetProgress = 1.0f;
	ULONGLONG _animStartTick = 0;
	bool _animating = false;

	float CurrentExpandProgress();
	void PerformExpanderLayoutIfNeeded();
	bool HeaderHitTest(int localX, int localY) const;
	void ApplyExpandedStateChange(bool oldValue, bool newValue);
	void SetCurrentExpanded(bool value);

public:
	UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;
	Expander();
	Expander(std::wstring text, int x, int y, int width, int height);

	ExpanderExpandedChangedEvent OnExpandedChanged;

	PROPERTY(bool, IsExpanded);
	GET(bool, IsExpanded);
	SET(bool, IsExpanded);

#define CUI_EXPANDER_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	SET(type, name)

	CUI_EXPANDER_PROPERTY(float, HeaderHeight);
	CUI_EXPANDER_PROPERTY(float, HeaderPaddingX);
	CUI_EXPANDER_PROPERTY(float, ChevronSize);
	CUI_EXPANDER_PROPERTY(float, Border);
	CUI_EXPANDER_PROPERTY(float, CornerRadius);
	CUI_EXPANDER_PROPERTY(UINT, AnimationDurationMs);
	CUI_EXPANDER_PROPERTY(D2D1_COLOR_F, SurfaceColor);
	CUI_EXPANDER_PROPERTY(D2D1_COLOR_F, HeaderBackColor);
	CUI_EXPANDER_PROPERTY(D2D1_COLOR_F, HeaderHoverBackColor);
	CUI_EXPANDER_PROPERTY(D2D1_COLOR_F, ContentBackColor);
	CUI_EXPANDER_PROPERTY(D2D1_COLOR_F, AccentColor);
	CUI_EXPANDER_PROPERTY(D2D1_COLOR_F, MutedTextColor);
	CUI_EXPANDER_PROPERTY(D2D1_COLOR_F, DisabledOverlayColor);

#undef CUI_EXPANDER_PROPERTY

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
