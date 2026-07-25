#pragma once

#include "ContentControl.h"

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
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }

	bool InitializeItem(
		const BindingSourceReference& item,
		const ItemTemplateReference& contentTemplate,
		const std::wstring& displayMemberPath,
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
	virtual void ActivateItem() {}
	virtual void FocusOwner() {}
	virtual void OnIsSelectedRequested(bool) {}
	void OnIsMouseOverChanged(bool, bool) override
	{
		UpdateThemeBackground();
	}

private:
	friend class Selector;
	size_t _index = 0;
	bool _selected = false;
	Event<void(ItemContainerControl*)> _selectedChanged;

	void ApplyIsSelectedValue(bool value);
	void SetCurrentIsSelected(bool value);
	void UpdateThemeBackground();
};
