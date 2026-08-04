#pragma once

#include "ContentControl.h"
#include "InputReport.h"

class Selector;

/**
 * Shared runtime contract for generated selector item containers.
 *
 * This base is intentionally not exposed as a XAML element. Concrete controls
 * provide the public type identity and owner-specific activation behavior;
 * content generation and observable interaction states remain identical.
 */
class ItemContainerControl : public ContentControl
{
public:
	using UIElement::Selected;
	using UIElement::Unselected;
	ItemContainerControl();
	static const DependencyProperty& IsSelectedProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
#endif

#if CUI_ENABLE_DYNAMIC_XAML
	bool InitializeItem(
		const BindingSourceReference& item,
		const ItemTemplateReference& contentTemplate,
		const std::wstring& displayMemberPath,
		size_t index,
		const std::wstring& publicTypeName,
		std::wstring* outError = nullptr);
#endif
	bool InitializeItem(
		const BindingSourceReference& item,
		const ItemTemplateReference& contentTemplate,
		CompiledBindingPathView displayMemberPath,
		size_t index,
		const std::wstring& publicTypeName,
		std::wstring* outError = nullptr);

	void SetIsSelected(bool value);
	bool GetIsSelected() const noexcept { return _selected; }
	__declspec(property(get = GetIsSelected, put = SetIsSelected))
		bool IsSelected;
	size_t ItemIndex() const noexcept { return _index; }
	void SetItemIndex(size_t value) noexcept { _index = value; }
	Control* Content() const noexcept { return GetGeneratedContent(); }

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::ListItem, L"ListItem");
	}
	bool ProcessInput(const InputReport& input) override;
	bool ApplyTextInput(
		const TextCompositionEventArgs& input) override;
	virtual void ActivateItem(
		MouseButton button, ModifierKeys modifiers)
	{
		(void)button;
		(void)modifiers;
	}
	virtual void FocusOwner() {}
	virtual void OnIsSelectedRequested(bool) {}
	virtual bool ActivatesOnPointerUp() const noexcept { return false; }

private:
	friend class Selector;
	static void EnsureClassHandlers();
	static void HandleDescendantPointerPress(
		Control* sender, RoutedEventArgs& args);
	static void HandleDescendantPointerRelease(
		Control* sender, RoutedEventArgs& args);
	void BeginPointerPress(MouseEventArgs& args);
	bool CompletePointerPress(MouseEventArgs& args);
	size_t _index = 0;
	bool _selected = false;
	bool _pointerPressActive = false;
	Event<void(ItemContainerControl*)> _selectedChanged;

	void ApplyIsSelectedValue(bool value);
	void SetCurrentIsSelected(bool value);
};
