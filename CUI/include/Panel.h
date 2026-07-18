#pragma once
#include "Control.h"
#include "Layout/LayoutEngine.h"
#include <algorithm>
#include <cmath>
#include <utility>
#pragma comment(lib, "Imm32.lib")

/**
 * @file Panel.h
 * @brief Panel：通用容器控件（可承载子控件并驱动布局）。
 *
 * Panel 本身继承自 Control，并额外提供布局能力：
 * - 未设置 LayoutEngine 时，默认容器以 Location 负责绝对定位，Margin 负责对齐/锚定附加间距，Padding 只收缩内容区（见 Panel.cpp）
 * - 设置 LayoutEngine 后，按 Measure/Arrange 两阶段布局
 *
 * 所有权：SetLayoutEngine 会接管传入指针并负责 delete。
 */

class Panel : public Control
{
private:
	float _borderThickness = 1.5f;
	float _cornerRadius = 0.0f;
	D2D1_COLOR_F _disabledOverlayColor =
		cui::theme::palette::DisabledOverlay;

protected:
	std::unique_ptr<class LayoutEngine> _layoutEngine;
	bool _needsLayout = false;
	void RequestLayout() override;
	void OnComputedLayoutSizeChanged() override;
	void PerformPendingLayout() override;
	cui::core::Size MeasureCore(const cui::core::Constraints& available) override;
	void InitializePanelCornerRadiusDefault(float value) noexcept
	{
		_cornerRadius = value;
	}
	void InitializePanelDisabledOverlayColorDefault(
		D2D1_COLOR_F value) noexcept
	{
		_disabledOverlayColor = value;
	}

	template<typename TOwner>
	static void RegisterPanelCornerRadiusMetadata(float defaultValue)
	{
		ControlPropertyOptions<TOwner, float> options;
		options.DefaultValue = defaultValue;
		options.Flags = ControlPropertyFlags::AffectsRender
			| ControlPropertyFlags::TracksLocalValue;
		options.Coerce = [](
			TOwner&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = 20;
		options.Design.Editor = ControlPropertyEditorKind::Number;
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;

		BindingPropertyRegistry::Register<TOwner, float>(L"CornerRadius",
			[](TOwner& target)
			{
				return static_cast<Panel&>(target).GetCornerRadius();
			},
			[](TOwner& target, const float& value)
			{
				static_cast<Panel&>(target).SetCornerRadius(value);
			},
			[](TOwner& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target.OnPropertyValueChanged.Subscribe(
					[handler = std::move(handler)](
						Control*, const ControlPropertyChangedEventArgs& args)
					{
						if (_wcsicmp(
							args.PropertyName.c_str(), L"CornerRadius") == 0)
							handler();
					});
			},
			std::move(options));
	}
	
public:
	virtual UIClass Type();
	void EnsureBindingPropertiesRegistered() override;

	PROPERTY(float, BorderThickness);
	GET(float, BorderThickness);
	SET(float, BorderThickness);
	PROPERTY(float, CornerRadius);
	GET(float, CornerRadius);
	SET(float, CornerRadius);
	PROPERTY(D2D1_COLOR_F, DisabledOverlayColor);
	GET(D2D1_COLOR_F, DisabledOverlayColor);
	SET(D2D1_COLOR_F, DisabledOverlayColor);

	Panel();
	Panel(int x, int y, int width, int height);
	virtual ~Panel();
	
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
	bool ClipsChildren() override { return true; }
	cui::core::Point GetChildrenLayoutOriginDip() override
	{
		return { Padding.Left, Padding.Top };
	}
	
	// 布局引擎管理
	/**
	 * @brief 设置布局引擎。
	 *
	 * 传入 nullptr 表示恢复为默认布局（Location + Anchor/Margin/Align）。
	 * @param engine 布局引擎指针（由 Panel 接管并在析构/替换时 delete）。
	 */
	void SetLayoutEngine(class LayoutEngine* engine);
	/** @brief 获取当前布局引擎（不转移所有权）。 */
	class LayoutEngine* GetLayoutEngine() const { return _layoutEngine.get(); }
	
	// 触发布局
	/**
	 * @brief 立即执行一次布局（必要时会 Measure/Arrange）。
	 *
	 * 通常由 Update 在检测到布局脏标记时调用。
	 */
	void PerformLayout();
	/** @brief 标记布局失效，下一帧重新布局。 */
	void InvalidateLayout();
	
	// 重写 AddControl 以支持布局触发
	template<typename T>
	T AddControl(T control) {
		return Control::AddControl(control);
	}
};
