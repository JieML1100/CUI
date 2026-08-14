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
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<RangeBaseAutomationPeer>(
			*this, AutomationControlType::Slider, L"Slider");
	}

private:
	static const DependencyPropertyKey& IsThumbDraggingPropertyKey();
	static void EnsureClassHandlers();
	static void HandleDescendantPointerPress(
		Control* sender, RoutedEventArgs& args);

	double _tickFrequency = 1.0;
	bool _isSnapToTickEnabled = false;
	bool _isMoveToPointEnabled = false;
	bool _isSelectionRangeEnabled = false;
	double _selectionStart = 0.0;
	double _selectionEnd = 0.0;
	::Orientation _orientation = Orientation::Horizontal;
	bool _isDirectionReversed = false;
	bool _isThumbDragging = false;
	double _dragStartValue = 0.0;
	int _dragStartX = 0;
	int _dragStartY = 0;

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
	float PositionRatio(double value) const;
	double PointToValue(int localX, int localY) const;
	double ValueFromDragDelta(int localX, int localY) const;
	double SnapToTick(double value) const;
	void UpdateValueFromInput(double value);
	void MoveToNextTick(double direction);
	bool IsPointOverThumb(int localX, int localY) const;
	bool IsOriginalSourceWithinThumb(Control* source) const;
	void HandlePointerPress(
		int localX, int localY, bool isThumbPress);
	void BeginThumbDrag(int localX, int localY);
	void SetThumbDragging(bool value);
	void UpdateTemplateParts();

public:
	/** @brief 创建滑动条。 */
	Slider();
	virtual UIClass Type() override;
	static const DependencyProperty& TickFrequencyProperty();
	static const DependencyProperty& OrientationProperty();
	static const DependencyProperty& IsSelectionRangeEnabledProperty();
	static const DependencyProperty& IsSnapToTickEnabledProperty();
	static const DependencyProperty& IsMoveToPointEnabledProperty();
	static const DependencyProperty& SelectionStartProperty();
	static const DependencyProperty& SelectionEndProperty();
	static const DependencyProperty& IsDirectionReversedProperty();
	static const DependencyProperty& IsThumbDraggingProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	void Arrange(cui::core::Rect finalRect) override;

	PROPERTY(double, TickFrequency);
	GET(double, TickFrequency);
	SET(double, TickFrequency);

	PROPERTY(bool, IsSnapToTickEnabled);
	GET(bool, IsSnapToTickEnabled);
	SET(bool, IsSnapToTickEnabled);

	PROPERTY(bool, IsMoveToPointEnabled);
	GET(bool, IsMoveToPointEnabled);
	SET(bool, IsMoveToPointEnabled);

	PROPERTY(bool, IsSelectionRangeEnabled);
	GET(bool, IsSelectionRangeEnabled);
	SET(bool, IsSelectionRangeEnabled);

	PROPERTY(double, SelectionStart);
	GET(double, SelectionStart);
	SET(double, SelectionStart);

	PROPERTY(double, SelectionEnd);
	GET(double, SelectionEnd);
	SET(double, SelectionEnd);

	PROPERTY(::Orientation, Orientation);
	GET(::Orientation, Orientation);
	SET(::Orientation, Orientation);

	PROPERTY(bool, IsDirectionReversed);
	GET(bool, IsDirectionReversed);
	SET(bool, IsDirectionReversed);

	/** Template-state projection of WPF Thumb.IsDragging. */
	READONLY_PROPERTY(bool, IsThumbDragging);
	GET(bool, IsThumbDragging);

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
	void PreparePresentation() override;
	bool ProcessInput(const InputReport& input) override;
	void OnMinimumChanged(double oldValue, double newValue) override;
	void OnMaximumChanged(double oldValue, double newValue) override;
	void OnRangeValueChanged(double oldValue, double newValue) override;
	void OnComputedLayoutSizeChanged() override;
	void OnControlTemplatePresentationChanged() override;
};
