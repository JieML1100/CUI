#pragma once
#include "RangeBase.h"
#include <algorithm>

/**
 * @file Slider.h
 * @brief Slider：滑动条控件。
 *
 * 约定：
 * - 通过 Minimum/Maximum/Value 表示数值范围
 * - 拖拽滑块会更新 Value，并触发 routed ValueChanged
 * - 可启用 IsSnapToTickEnabled，以 TickFrequency 对 Value 吸附
 */

/**
 * @brief 数值变化事件。
 * @param oldValue 变化前的值。
 * @param newValue 变化后的值。
 */
class Slider : public RangeBase
{
protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<RangeBaseAutomationPeer>(
			*this, AutomationControlType::Slider, L"Slider");
	}

private:
	double _smallChange = 1.0;
	double _largeChange = 10.0;
	double _tickFrequency = 1.0;
	bool _isSnapToTickEnabled = false;
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

	float TrackLeftLocal() { return (std::max)(12.0f, _thumbRadius + _thumbDragRadiusDelta + 4.0f); }
	float TrackRightLocal() { return this->ActualWidth - TrackLeftLocal(); }
	float TrackYLocal() { return this->ActualHeight * 0.5f; }
	float ValueToT()
	{
		const double range = Maximum - Minimum;
		if (range <= 0.0000001) return 0.0f;
		return static_cast<float>((Value - Minimum) / range);
	}
	double XToValue(int localX)
	{
		float trackLeft = TrackLeftLocal();
		float trackRight = TrackRightLocal();
		if (trackRight <= trackLeft) return Minimum;
		float ratio = ((float)localX - trackLeft) / (trackRight - trackLeft);
		ratio = std::clamp(ratio, 0.0f, 1.0f);
		return Minimum + static_cast<double>(ratio) * (Maximum - Minimum);
	}

public:
	/** @brief 创建滑动条。 */
	Slider();
	virtual UIClass Type() override;
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }

	PROPERTY(double, SmallChange);
	GET(double, SmallChange);
	SET(double, SmallChange);

	PROPERTY(double, LargeChange);
	GET(double, LargeChange);
	SET(double, LargeChange);

	PROPERTY(double, TickFrequency);
	GET(double, TickFrequency);
	SET(double, TickFrequency);

	PROPERTY(bool, IsSnapToTickEnabled);
	GET(bool, IsSnapToTickEnabled);
	SET(bool, IsSnapToTickEnabled);
	/** @brief 在当前值上递增 SmallChange（或指定 delta）。 */
	void Increment(double delta);
	void Increment();
	/** @brief 在当前值上递减 SmallChange（或指定 delta）。 */
	void Decrement(double delta);
	void Decrement();
	/** @brief 将 Value 重置为 Minimum。 */
	void Reset();

	CursorKind QueryCursor(int localX, int localY) override;
protected:
	void OnRender() override;
	bool ProcessInput(const InputReport& input) override;
	double CoerceRangeValue(double value) const override;
};
