#pragma once
#include "ButtonBase.h"
#include <cstdint>

/**
 * @file Button.h
 * @brief Button：基础按钮控件。
 *
 * 主要行为：
 * - 通过 ButtonBase/Control 的点击与输入事件对外通知
 * - 执行 RoutedCommand，并参与 Window 的默认/取消按钮路由
 *
 * 默认外观完全由 framework theme 的 Style/ControlTemplate 提供。
 */
class Button : public ButtonBase
{
protected:
	bool OnClick() override;
	void OnEffectiveIsEnabledChanged(
		bool previousValue, bool currentValue) override;
	void OnPresentationWindowChanged(
		PresentationWindow* previousWindow,
		PresentationWindow* currentWindow) override;
private:
	friend class Window;
	bool _isDefault = false;
	bool _isCancel = false;
	bool _isDefaulted = false;
	static const DependencyPropertyKey& IsDefaultedPropertyKey();
	void UpdateIsDefaulted(Control* focused);
public:
	virtual UIClass Type();
	static const DependencyProperty& IsDefaultProperty();
	static const DependencyProperty& IsCancelProperty();
	static const DependencyProperty& IsDefaultedProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif
	/** XAML-authored default action for an otherwise unhandled Enter key. */
	PROPERTY(bool, IsDefault);
	GET(bool, IsDefault);
	SET(bool, IsDefault);
	/** XAML-authored cancel action for an otherwise unhandled Escape key. */
	PROPERTY(bool, IsCancel);
	GET(bool, IsCancel);
	SET(bool, IsCancel);
	/** True when this enabled default button owns the current Return action. */
	READONLY_PROPERTY(bool, IsDefaulted);
	GET(bool, IsDefaulted);
	Button();
};
