#pragma once

#include "ContentControl.h"

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
	ItemContainerControl();
	void EnsureBindingPropertiesRegistered() override;

	bool InitializeItem(
		const BindingSourceReference& item,
		const ItemTemplateReference& contentTemplate,
		const std::wstring& displayMemberPath,
		size_t index,
		const std::wstring& publicTypeName,
		std::wstring* outError = nullptr);

	void SetSelected(bool value);
	bool GetIsSelected() const noexcept { return _selected; }
	__declspec(property(get = GetIsSelected)) bool IsSelected;
	void SetMouseOver(bool value);
	bool GetIsMouseOver() const noexcept { return _mouseOver; }
	__declspec(property(get = GetIsMouseOver)) bool IsMouseOver;
	void SetKeyboardFocusWithin(bool value);
	bool GetIsKeyboardFocusWithin() const noexcept
	{
		return _keyboardFocusWithin;
	}
	__declspec(property(get = GetIsKeyboardFocusWithin))
		bool IsKeyboardFocusWithin;

	size_t ItemIndex() const noexcept { return _index; }
	void SetItemIndex(size_t value) noexcept { _index = value; }
	Control* Content() const noexcept { return GetGeneratedContent(); }

protected:
	virtual void ActivateItem() {}
	virtual void FocusOwner() {}

private:
	size_t _index = 0;
	bool _selected = false;
	bool _mouseOver = false;
	bool _keyboardFocusWithin = false;
	bool _selectionGestureAttached = false;
	Event<void(ItemContainerControl*)> _selectedChanged;
	Event<void(ItemContainerControl*)> _mouseOverChanged;
	Event<void(ItemContainerControl*)> _keyboardFocusWithinChanged;

	void AttachSelectionGesture(Control& control);
	void UpdateThemeBackground();
};
