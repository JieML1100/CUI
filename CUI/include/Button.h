#pragma once
#include "ButtonBase.h"
#include <cstdint>

/**
 * @file Button.h
 * @brief Button：基础按钮控件。
 *
 * 主要行为：
 * - 根据鼠标悬停和按下状态绘制不同背景
 * - 通过 ButtonBase/Control 的点击与输入事件对外通知
 */
class Button : public ButtonBase
{
protected:
	void AfterDefaultClick(MouseButton button, MouseEventArgs& e) override;
	void ConfigureContentVisual(Control& child) override;
private:
	std::wstring _command;
	std::wstring _commandParameter;
	ControlWeakReference _commandTarget;
	bool _isDefault = false;
	bool _isCancel = false;
	EventConnection _commandCanExecuteConnection;
	std::uint64_t _commandSourceRefreshVersion = 0;
	void ApplyCommandTarget(const ControlWeakReference& value);
	void RefreshCommandSource();
	bool ExecuteCommandSource();
public:
	virtual UIClass Type();
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
	/** XAML-authored routed-command identity. */
	PROPERTY(std::wstring, Command);
	GET(std::wstring, Command);
	SET(std::wstring, Command);
	/** XAML scalar projected as the command parameter. */
	PROPERTY(std::wstring, CommandParameter);
	GET(std::wstring, CommandParameter);
	SET(std::wstring, CommandParameter);
	/** Optional authored routed-command target. An expired target stays authored. */
	PROPERTY(class Control*, CommandTarget);
	GET(class Control*, CommandTarget);
	SET(class Control*, CommandTarget);
	bool HasAuthoredCommandTarget() const noexcept
	{
		return _commandTarget.HasValue();
	}
	/** Removes the authored target so focus-based resolution is used again. */
	void ClearCommandTarget();
	/** XAML-authored default action for an otherwise unhandled Enter key. */
	PROPERTY(bool, IsDefault);
	GET(bool, IsDefault);
	SET(bool, IsDefault);
	/** XAML-authored cancel action for an otherwise unhandled Escape key. */
	PROPERTY(bool, IsCancel);
	GET(bool, IsCancel);
	SET(bool, IsCancel);
	Button();
	bool Invoke() override;
protected:
	void OnRender() override;
};
