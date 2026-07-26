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
	static void EnsureClassHandlers();
	static void HandleDescendantPointerPress(
		Control* sender, RoutedEventArgs& args);

	double _smallChange = 1.0;
	double _largeChange = 10.0;
	double _tickFrequency = 1.0;
	bool _isSnapToTickEnabled = false;
	::Orientation _orientation = Orientation::Horizontal;
	bool _isDirectionReversed = false;
	bool _dragging = false;

	struct TrackGeometry final
	{
		float Left = 0.0f;
		float Top = 0.0f;
		float Width = 0.0f;
		float Height = 0.0f;
	};

	TrackGeometry ResolveTrackGeometry() const;
	float ValueRatio() const;
	float PositionRatio() const;
	double PointToValue(int localX, int localY) const;
	void BeginPointerInteraction(int localX, int localY);
	void UpdateTemplateParts();

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

	PROPERTY(::Orientation, Orientation);
	GET(::Orientation, Orientation);
	SET(::Orientation, Orientation);

	PROPERTY(bool, IsDirectionReversed);
	GET(bool, IsDirectionReversed);
	SET(bool, IsDirectionReversed);
	/** @brief 在当前值上递增 SmallChange（或指定 delta）。 */
	void Increment(double delta);
	void Increment();
	/** @brief 在当前值上递减 SmallChange（或指定 delta）。 */
	void Decrement(double delta);
	void Decrement();
	/** @brief 将 Value 重置为 Minimum。 */
	void Reset();

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesNavigationKey(Key key) const override;
protected:
	bool ProcessInput(const InputReport& input) override;
	double CoerceRangeValue(double value) const override;
	void OnRangeValueChanged(double oldValue, double newValue) override;
	void OnComputedLayoutSizeChanged() override;
	void OnControlTemplatePresentationChanged() override;
};
