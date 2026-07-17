#pragma once
#include "Control.h"
#include <algorithm>

/**
 * @file Slider.h
 * @brief Slider：滑动条控件。
 *
 * 约定：
 * - 通过 Min/Max/Value 表示数值范围
 * - 拖拽滑块会更新 Value，并触发 OnValueChanged
 * - 可启用 SnapToStep 以 Step 为步进对 Value 进行吸附
 */

/**
 * @brief 数值变化事件。
 * @param oldValue 变化前的值。
 * @param newValue 变化后的值。
 */
typedef Event<void(class Control*, float oldValue, float newValue)> ValueChangedEvent;

class Slider : public Control
{
private:
	float _min = 0.0f;
	float _max = 100.0f;
	float _value = 0.0f;
	float _step = 1.0f;
	bool _snapToStep = false;
	D2D1_COLOR_F _trackBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F _trackForeColor = cui::theme::palette::Accent;
	D2D1_COLOR_F _trackHoverColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _trackBorderColor = cui::theme::palette::Border;
	D2D1_COLOR_F _thumbColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _thumbHoverColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _thumbBorderColor = cui::theme::palette::BorderStrong;
	D2D1_COLOR_F _thumbShadowColor = cui::theme::palette::Shadow;
	D2D1_COLOR_F _disabledOverlayColor = cui::theme::palette::DisabledOverlay;
	float _trackHeight = 5.0f;
	float _thumbRadius = 8.0f;
	float _thumbHoverRadiusDelta = 1.0f;
	float _thumbDragRadiusDelta = 2.0f;
	bool _dragging = false;

	float TrackLeftLocal() { return (std::max)(12.0f, ThumbRadius + ThumbDragRadiusDelta + 4.0f); }
	float TrackRightLocal() { return (float)this->Width - TrackLeftLocal(); }
	float TrackYLocal() { return (float)this->Height * 0.5f; }
	float ValueToT()
	{
		float range = (_max - _min);
		if (range <= 0.00001f) return 0.0f;
		return (_value - _min) / range;
	}
	float XToValue(int localX)
	{
		float trackLeft = TrackLeftLocal();
		float trackRight = TrackRightLocal();
		if (trackRight <= trackLeft) return _min;
		float ratio = ((float)localX - trackLeft) / (trackRight - trackLeft);
		ratio = std::clamp(ratio, 0.0f, 1.0f);
		return _min + ratio * (_max - _min);
	}
	float CoerceValue(float value) const;
	void SetCurrentValue(float value);
	void ReevaluateValue();

public:
	/** @brief 值变化事件。 */
	ValueChangedEvent OnValueChanged;

	/** @brief 创建滑动条。 */
	Slider(int x, int y, int width = 240, int height = 32);
	virtual UIClass Type() override;
	void EnsureBindingPropertiesRegistered() override;

	PROPERTY(float, Min);
	GET(float, Min);
	SET(float, Min);

	PROPERTY(float, Max);
	GET(float, Max);
	SET(float, Max);

	PROPERTY(float, Value);
	GET(float, Value);
	SET(float, Value);

	PROPERTY(float, Step);
	GET(float, Step);
	SET(float, Step);

	PROPERTY(bool, SnapToStep);
	GET(bool, SnapToStep);
	SET(bool, SnapToStep);

#define CUI_SLIDER_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	SET(type, name)

	CUI_SLIDER_PROPERTY(D2D1_COLOR_F, TrackBackColor);
	CUI_SLIDER_PROPERTY(D2D1_COLOR_F, TrackForeColor);
	CUI_SLIDER_PROPERTY(D2D1_COLOR_F, TrackHoverColor);
	CUI_SLIDER_PROPERTY(D2D1_COLOR_F, TrackBorderColor);
	CUI_SLIDER_PROPERTY(D2D1_COLOR_F, ThumbColor);
	CUI_SLIDER_PROPERTY(D2D1_COLOR_F, ThumbHoverColor);
	CUI_SLIDER_PROPERTY(D2D1_COLOR_F, ThumbBorderColor);
	CUI_SLIDER_PROPERTY(D2D1_COLOR_F, ThumbShadowColor);
	CUI_SLIDER_PROPERTY(D2D1_COLOR_F, DisabledOverlayColor);
	CUI_SLIDER_PROPERTY(float, TrackHeight);
	CUI_SLIDER_PROPERTY(float, ThumbRadius);
	CUI_SLIDER_PROPERTY(float, ThumbHoverRadiusDelta);
	CUI_SLIDER_PROPERTY(float, ThumbDragRadiusDelta);

#undef CUI_SLIDER_PROPERTY

	CursorKind QueryCursor(int localX, int localY) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};

