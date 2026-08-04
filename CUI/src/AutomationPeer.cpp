#include "AutomationPeer.h"

#include "Control.h"
#include "ComboBox.h"
#include "Expander.h"
#include "ListBox.h"
#include "Menu.h"
#include "NumericUpDown.h"
#include "RangeBase.h"
#include "Slider.h"
#include "TabControl.h"
#include "TextBox.h"
#include "ToggleButton.h"
#include "RadioButton.h"
#include "TreeView.h"

#include <cmath>
#include <utility>

namespace
{
	AutomationOperationResult EnsureEnabled(const Control& owner) noexcept
	{
		return owner.IsEffectivelyEnabled()
			? AutomationOperationResult::Succeeded
			: AutomationOperationResult::ElementNotEnabled;
	}
}

AutomationPeer::AutomationPeer(Control& owner,
	AutomationControlType controlType, std::wstring fallbackClassName) :
	_owner(owner),
	_controlType(controlType),
	_fallbackClassName(std::move(fallbackClassName))
{
}

AutomationControlType AutomationPeer::GetAutomationControlType() const noexcept
{
	return _controlType;
}

std::wstring AutomationPeer::GetAutomationClassName() const
{
#if CUI_ENABLE_DYNAMIC_XAML
	const auto& declarativeName = _owner.GetDeclarativeTypeName();
	const auto& name = declarativeName.empty()
		? _fallbackClassName : declarativeName;
#else
	const auto& name = _fallbackClassName;
#endif
	if (name.empty()) return L"CUI.Control";
	return name.find(L'.') == std::wstring::npos
		? L"CUI." + name : name;
}

AutomationPattern AutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::None;
}

std::wstring AutomationPeer::GetValue() const
{
	return {};
}

bool AutomationPeer::IsReadOnly() const
{
	return _owner.IsAccessibilityReadOnly();
}

AutomationOperationResult AutomationPeer::Invoke()
{
	return AutomationOperationResult::NotSupported;
}

AutomationOperationResult AutomationPeer::Toggle()
{
	return AutomationOperationResult::NotSupported;
}

bool AutomationPeer::TryGetToggleState(AutomationToggleState&) const
{
	return false;
}

AutomationOperationResult AutomationPeer::SetValue(const std::wstring&)
{
	return AutomationOperationResult::NotSupported;
}

bool AutomationPeer::TryGetRangeValue(AutomationRangeValue&) const
{
	return false;
}

AutomationOperationResult AutomationPeer::SetRangeValue(double)
{
	return AutomationOperationResult::NotSupported;
}

bool AutomationPeer::TryGetExpanded(bool&) const
{
	return false;
}

AutomationOperationResult AutomationPeer::SetExpanded(bool)
{
	return AutomationOperationResult::NotSupported;
}

bool AutomationPeer::TryGetSelectionItemSelected(bool&) const
{
	return false;
}

AutomationOperationResult AutomationPeer::Select()
{
	return AutomationOperationResult::NotSupported;
}

Control* AutomationPeer::GetSelectionContainer() const
{
	return nullptr;
}

Control* AutomationPeer::GetSelectedItem() const
{
	return nullptr;
}

bool AutomationPeer::TryGetAccessibilityVirtualNode(
	uint32_t, AccessibilityVirtualNode&)
{
	return false;
}

size_t AutomationPeer::GetAccessibilityVirtualChildCount(uint32_t)
{
	return 0;
}

bool AutomationPeer::TryGetAccessibilityVirtualChildAt(
	uint32_t, size_t, uint32_t& result)
{
	result = 0;
	return false;
}

bool AutomationPeer::TryGetAccessibilityVirtualSibling(
	uint32_t, uint32_t, bool, uint32_t& result)
{
	result = 0;
	return false;
}

bool AutomationPeer::TryHitTestAccessibilityVirtualNode(
	float, float, uint32_t& result)
{
	result = 0;
	return false;
}

void AutomationPeer::GetAccessibilityVirtualChildren(
	uint32_t parentId, std::vector<uint32_t>& result)
{
	result.clear();
	const size_t count = GetAccessibilityVirtualChildCount(parentId);
	result.reserve(count);
	for (size_t index = 0; index < count; ++index)
	{
		uint32_t id = 0;
		if (TryGetAccessibilityVirtualChildAt(parentId, index, id) && id != 0)
			result.push_back(id);
	}
}

AccessibilityVirtualContainerInfo
AutomationPeer::GetAccessibilityVirtualContainerInfo() const noexcept
{
	return {};
}

void AutomationPeer::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	result.clear();
}

bool AutomationPeer::GetAccessibilityVirtualItemAt(int, int, uint32_t& result)
{
	result = 0;
	return false;
}

void AutomationPeer::GetAccessibilityVirtualColumnHeaders(
	std::vector<uint32_t>& result)
{
	result.clear();
}

bool AutomationPeer::InvokeAccessibilityVirtualNode(uint32_t)
{
	return false;
}

bool AutomationPeer::ToggleAccessibilityVirtualNode(uint32_t)
{
	return false;
}

bool AutomationPeer::SetAccessibilityVirtualNodeValue(
	uint32_t, const std::wstring&)
{
	return false;
}

bool AutomationPeer::SetAccessibilityVirtualNodeExpanded(uint32_t, bool)
{
	return false;
}

bool AutomationPeer::SelectAccessibilityVirtualNode(
	uint32_t, AccessibilitySelectionAction)
{
	return false;
}

bool AutomationPeer::ScrollAccessibilityVirtualNodeIntoView(uint32_t)
{
	return false;
}

bool AutomationPeer::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	result = {};
	return false;
}

bool AutomationPeer::ScrollAccessibility(
	AccessibilityScrollAmount, AccessibilityScrollAmount)
{
	return false;
}

bool AutomationPeer::SetAccessibilityScrollPercent(double, double)
{
	return false;
}

InvokeAutomationPeer::InvokeAutomationPeer(Control& owner,
	AutomationControlType controlType, std::wstring fallbackClassName) :
	AutomationPeer(owner, controlType, std::move(fallbackClassName))
{
}

AutomationPattern InvokeAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::Invoke;
}

AutomationOperationResult InvokeAutomationPeer::Invoke()
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	return Owner().Invoke() ? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

MenuItemAutomationPeer::MenuItemAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::MenuItem, L"MenuItem")
{
}

AutomationPattern MenuItemAutomationPeer::GetPatternSet() const noexcept
{
	const auto* item = dynamic_cast<const MenuItem*>(&Owner());
	if (!item) return AutomationPattern::None;
	AutomationPattern patterns = item->_items.empty()
		? AutomationPattern::Invoke
		: AutomationPattern::ExpandCollapse;
	if (item->_isCheckable)
		patterns |= AutomationPattern::Toggle;
	return patterns;
}

AutomationOperationResult MenuItemAutomationPeer::Invoke()
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* item = dynamic_cast<MenuItem*>(&Owner());
	if (!item) return AutomationOperationResult::NotSupported;
	if (!item->_items.empty()) return AutomationOperationResult::NotSupported;
	return item->InvokeLeafAndDismiss()
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

AutomationOperationResult MenuItemAutomationPeer::Toggle()
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* item = dynamic_cast<MenuItem*>(&Owner());
	if (!item || !item->_isCheckable)
		return AutomationOperationResult::NotSupported;
	if (!item->_items.empty())
		return AutomationOperationResult::InvalidOperation;
	return item->InvokeLeafAndDismiss()
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

bool MenuItemAutomationPeer::TryGetToggleState(
	AutomationToggleState& value) const
{
	const auto* item = dynamic_cast<const MenuItem*>(&Owner());
	if (!item || !item->_isCheckable) return false;
	value = item->_isChecked
		? AutomationToggleState::On : AutomationToggleState::Off;
	return true;
}

bool MenuItemAutomationPeer::TryGetExpanded(bool& value) const
{
	const auto* item = dynamic_cast<const MenuItem*>(&Owner());
	if (!item || item->_items.empty()) return false;
	value = item->_isSubmenuOpen;
	return true;
}

AutomationOperationResult MenuItemAutomationPeer::SetExpanded(bool value)
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* item = dynamic_cast<MenuItem*>(&Owner());
	if (!item || item->_items.empty())
		return AutomationOperationResult::NotSupported;
	item->SetIsSubmenuOpenCore(value);
	return AutomationOperationResult::Succeeded;
}

ToggleAutomationPeer::ToggleAutomationPeer(Control& owner,
	AutomationControlType controlType, std::wstring fallbackClassName) :
	AutomationPeer(owner, controlType, std::move(fallbackClassName))
{
}

AutomationPattern ToggleAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::Toggle;
}

AutomationOperationResult ToggleAutomationPeer::Toggle()
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* owner = dynamic_cast<ToggleButton*>(&Owner());
	if (!owner) return AutomationOperationResult::InvalidOperation;
	owner->OnToggle();
	return AutomationOperationResult::Succeeded;
}

bool ToggleAutomationPeer::TryGetToggleState(
	AutomationToggleState& value) const
{
	value = Owner().GetToggleStateForAccessibility();
	return true;
}

RadioButtonAutomationPeer::RadioButtonAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::RadioButton, L"RadioButton")
{
}

AutomationPattern RadioButtonAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::SelectionItem;
}

bool RadioButtonAutomationPeer::TryGetSelectionItemSelected(bool& value) const
{
	value = Owner().IsCheckedForAccessibility();
	return true;
}

AutomationOperationResult RadioButtonAutomationPeer::Select()
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* owner = dynamic_cast<RadioButton*>(&Owner());
	if (!owner) return AutomationOperationResult::InvalidOperation;
	owner->SetChecked(true);
	return AutomationOperationResult::Succeeded;
}

Control* RadioButtonAutomationPeer::GetSelectionContainer() const
{
	// WPF's RadioButtonAutomationPeer deliberately does not manufacture a
	// Selection container for an ordinary visual/logical parent.
	return nullptr;
}

TextBoxAutomationPeer::TextBoxAutomationPeer(Control& owner,
	std::wstring fallbackClassName, bool password) :
	AutomationPeer(owner, AutomationControlType::Edit,
		std::move(fallbackClassName)),
	_password(password)
{
}

AutomationPattern TextBoxAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::Value;
}

std::wstring TextBoxAutomationPeer::GetValue() const
{
	return _password ? std::wstring{} : Owner().GetSemanticText();
}

bool TextBoxAutomationPeer::IsReadOnly() const
{
	return Owner().IsAccessibilityReadOnly();
}

AutomationOperationResult TextBoxAutomationPeer::SetValue(
	const std::wstring& value)
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	if (IsReadOnly()) return AutomationOperationResult::InvalidOperation;
	return Owner().TrySetCurrentPropertyValue(
		TextBox::TextProperty(), BindingValue(value))
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

RangeBaseAutomationPeer::RangeBaseAutomationPeer(Control& owner,
	AutomationControlType controlType, std::wstring fallbackClassName,
	bool readOnly) :
	AutomationPeer(owner, controlType, std::move(fallbackClassName)),
	_readOnly(readOnly)
{
}

AutomationPattern RangeBaseAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::RangeValue;
}

std::wstring RangeBaseAutomationPeer::GetValue() const
{
	auto* range = dynamic_cast<RangeBase*>(&Owner());
	return range ? BindingValue(range->Value).ToString() : std::wstring{};
}

bool RangeBaseAutomationPeer::TryGetRangeValue(
	AutomationRangeValue& value) const
{
	auto* range = dynamic_cast<RangeBase*>(&Owner());
	if (!range) return false;
	value.Value = range->Value;
	value.Minimum = range->Minimum;
	value.Maximum = range->Maximum;
	value.IsReadOnly = _readOnly;
	if (auto* slider = dynamic_cast<Slider*>(range))
	{
		value.SmallChange = slider->SmallChange;
		value.LargeChange = slider->LargeChange;
	}
	else if (auto* numeric = dynamic_cast<NumericUpDown*>(range))
	{
		value.SmallChange = numeric->Increment;
		value.LargeChange = numeric->Increment * 10.0;
	}
	return true;
}

AutomationOperationResult RangeBaseAutomationPeer::SetRangeValue(double value)
{
	if (!std::isfinite(value)) return AutomationOperationResult::InvalidArgument;
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	if (_readOnly) return AutomationOperationResult::InvalidOperation;
	auto* range = dynamic_cast<RangeBase*>(&Owner());
	if (!range) return AutomationOperationResult::NotSupported;
	if (value < range->Minimum || value > range->Maximum)
		return AutomationOperationResult::InvalidArgument;
	return range->TrySetCurrentPropertyValue(
		RangeBase::ValueProperty(), BindingValue(value))
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

ListBoxAutomationPeer::ListBoxAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::List, L"ListBox")
{
}

AutomationPattern ListBoxAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::Selection;
}

Control* ListBoxAutomationPeer::GetSelectedItem() const
{
	auto* list = dynamic_cast<ListBox*>(&Owner());
	if (!list || list->GetSelectedIndex() < 0) return nullptr;
	const auto index = static_cast<size_t>(list->GetSelectedIndex());
	return list->GetItemsSource()
		? list->GetGeneratedItem(index)
		: list->GetAuthoredItem(index);
}

bool ListBoxAutomationPeer::CanSelectMultiple() const noexcept
{
	const auto* list = dynamic_cast<const ListBox*>(&Owner());
	return list && list->GetSelectionMode() != SelectionMode::Single;
}

ListBoxItemAutomationPeer::ListBoxItemAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::ListItem, L"ListBoxItem")
{
}

AutomationPattern
ListBoxItemAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::SelectionItem
		| AutomationPattern::ScrollItem;
}

bool ListBoxItemAutomationPeer::TryGetSelectionItemSelected(
	bool& value) const
{
	const auto* item = dynamic_cast<const ListBoxItem*>(&Owner());
	if (!item) return false;
	value = item->GetIsSelected();
	return true;
}

AutomationOperationResult ListBoxItemAutomationPeer::Select()
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* item = dynamic_cast<ListBoxItem*>(&Owner());
	auto* selector = dynamic_cast<Selector*>(GetSelectionContainer());
	if (!item || !selector)
		return AutomationOperationResult::InvalidOperation;
	if (auto* list = dynamic_cast<ListBox*>(selector))
		list->RequestItemSelection(item->ItemIndex(), true);
	else
		(void)selector->SelectIndex(static_cast<int>(item->ItemIndex()));
	return item->GetIsSelected()
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

Control* ListBoxItemAutomationPeer::GetSelectionContainer() const
{
	for (auto* parent = Owner().GetLogicalParent(); parent;
		parent = parent->GetLogicalParent())
		if (dynamic_cast<Selector*>(parent)) return parent;
	return nullptr;
}

ComboBoxAutomationPeer::ComboBoxAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::ComboBox, L"ComboBox")
{
}

AutomationPattern ComboBoxAutomationPeer::GetPatternSet() const noexcept
{
	const auto* combo = dynamic_cast<const ComboBox*>(&Owner());
	auto patterns = AutomationPattern::ExpandCollapse
		| AutomationPattern::Selection;
	if (combo && combo->GetIsEditable())
		patterns |= AutomationPattern::Value;
	return patterns;
}

std::wstring ComboBoxAutomationPeer::GetValue() const
{
	return Owner().GetSemanticText();
}

bool ComboBoxAutomationPeer::IsReadOnly() const
{
	const auto* combo = dynamic_cast<const ComboBox*>(&Owner());
	return !combo || !combo->GetIsEditable()
		|| combo->GetIsReadOnly();
}

AutomationOperationResult ComboBoxAutomationPeer::SetValue(
	const std::wstring& value)
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* combo = dynamic_cast<ComboBox*>(&Owner());
	if (!combo || !combo->GetIsEditable())
		return AutomationOperationResult::NotSupported;
	if (combo->GetIsReadOnly())
		return AutomationOperationResult::InvalidOperation;
	return combo->TrySetCurrentPropertyValue(
		ComboBox::TextProperty(), BindingValue(value))
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

bool ComboBoxAutomationPeer::TryGetExpanded(bool& value) const
{
	const auto* combo = dynamic_cast<const ComboBox*>(&Owner());
	if (!combo) return false;
	value = combo->GetIsDropDownOpen();
	return true;
}

AutomationOperationResult ComboBoxAutomationPeer::SetExpanded(bool value)
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* combo = dynamic_cast<ComboBox*>(&Owner());
	if (!combo) return AutomationOperationResult::NotSupported;
	combo->SetIsDropDownOpen(value);
	return AutomationOperationResult::Succeeded;
}

Control* ComboBoxAutomationPeer::GetSelectedItem() const
{
	auto* combo = dynamic_cast<ComboBox*>(&Owner());
	if (!combo || combo->GetSelectedIndex() < 0) return nullptr;
	const int index = combo->GetSelectedIndex();
	return combo->GetItemsSource()
		? combo->GetGeneratedItem(static_cast<size_t>(index))
		: combo->GetItem(index);
}

bool ComboBoxAutomationPeer::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	return static_cast<ComboBox&>(Owner()).TryGetAccessibilityVirtualNode(
		id, result);
}

size_t ComboBoxAutomationPeer::GetAccessibilityVirtualChildCount(
	uint32_t parentId)
{
	return static_cast<ComboBox&>(Owner()).GetAccessibilityVirtualChildCount(
		parentId);
}

bool ComboBoxAutomationPeer::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	return static_cast<ComboBox&>(Owner()).TryGetAccessibilityVirtualChildAt(
		parentId, index, result);
}

bool ComboBoxAutomationPeer::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result)
{
	return static_cast<ComboBox&>(Owner()).TryGetAccessibilityVirtualSibling(
		parentId, id, next, result);
}

bool ComboBoxAutomationPeer::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	return static_cast<ComboBox&>(Owner()).TryHitTestAccessibilityVirtualNode(
		localX, localY, result);
}

AccessibilityVirtualContainerInfo
ComboBoxAutomationPeer::GetAccessibilityVirtualContainerInfo() const noexcept
{
	return static_cast<const ComboBox&>(Owner())
		.GetAccessibilityVirtualContainerInfo();
}

void ComboBoxAutomationPeer::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	static_cast<ComboBox&>(Owner()).GetAccessibilityVirtualSelection(result);
}

bool ComboBoxAutomationPeer::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	return static_cast<ComboBox&>(Owner()).SelectAccessibilityVirtualNode(
		id, action);
}

bool ComboBoxAutomationPeer::ScrollAccessibilityVirtualNodeIntoView(uint32_t id)
{
	return static_cast<ComboBox&>(Owner())
		.ScrollAccessibilityVirtualNodeIntoView(id);
}

bool ComboBoxAutomationPeer::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	return static_cast<const ComboBox&>(Owner())
		.GetAccessibilityScrollInfo(result);
}

bool ComboBoxAutomationPeer::ScrollAccessibility(
	AccessibilityScrollAmount horizontal,
	AccessibilityScrollAmount vertical)
{
	return static_cast<ComboBox&>(Owner()).ScrollAccessibility(
		horizontal, vertical);
}

bool ComboBoxAutomationPeer::SetAccessibilityScrollPercent(
	double horizontalPercent, double verticalPercent)
{
	return static_cast<ComboBox&>(Owner()).SetAccessibilityScrollPercent(
		horizontalPercent, verticalPercent);
}

ExpanderAutomationPeer::ExpanderAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::Group, L"Expander")
{
}

AutomationPattern ExpanderAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::ExpandCollapse;
}

bool ExpanderAutomationPeer::TryGetExpanded(bool& value) const
{
	auto* expander = dynamic_cast<Expander*>(&Owner());
	if (!expander) return false;
	value = expander->IsExpanded;
	return true;
}

AutomationOperationResult ExpanderAutomationPeer::SetExpanded(bool value)
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* expander = dynamic_cast<Expander*>(&Owner());
	if (!expander) return AutomationOperationResult::NotSupported;
	return expander->TrySetCurrentPropertyValue(
		Expander::IsExpandedProperty(), BindingValue(value))
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

TreeViewItemAutomationPeer::TreeViewItemAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::TreeItem, L"TreeViewItem")
{
}

AutomationPattern TreeViewItemAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::ExpandCollapse
		| AutomationPattern::SelectionItem;
}

bool TreeViewItemAutomationPeer::TryGetExpanded(bool& value) const
{
	auto* item = dynamic_cast<TreeViewItem*>(&Owner());
	if (!item) return false;
	value = item->GetIsExpanded();
	return true;
}

AutomationOperationResult TreeViewItemAutomationPeer::SetExpanded(bool value)
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* item = dynamic_cast<TreeViewItem*>(&Owner());
	if (!item) return AutomationOperationResult::NotSupported;
	item->SetIsExpanded(value);
	return item->GetIsExpanded() == value
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

bool TreeViewItemAutomationPeer::TryGetSelectionItemSelected(bool& value) const
{
	auto* item = dynamic_cast<TreeViewItem*>(&Owner());
	if (!item) return false;
	value = item->GetIsSelected();
	return true;
}

Control* TreeViewItemAutomationPeer::GetSelectionContainer() const
{
	for (auto* parent = Owner().GetLogicalParent(); parent;
		parent = parent->GetLogicalParent())
		if (dynamic_cast<TreeView*>(parent)) return parent;
	return nullptr;
}

AutomationOperationResult TreeViewItemAutomationPeer::Select()
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* item = dynamic_cast<TreeViewItem*>(&Owner());
	auto* tree = dynamic_cast<TreeView*>(GetSelectionContainer());
	if (!item || !tree) return AutomationOperationResult::InvalidOperation;
	return item->GetIsSelected() || tree->SelectItem(item)
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

TreeViewAutomationPeer::TreeViewAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::Tree, L"TreeView")
{
}

AutomationPattern TreeViewAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::Selection;
}

Control* TreeViewAutomationPeer::GetSelectedItem() const
{
	const auto* tree = dynamic_cast<const TreeView*>(&Owner());
	return tree ? tree->GetSelectedContainer() : nullptr;
}

TabItemAutomationPeer::TabItemAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::TabItem, L"TabItem")
{
}

AutomationPattern TabItemAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::SelectionItem;
}

bool TabItemAutomationPeer::TryGetSelectionItemSelected(bool& value) const
{
	auto* item = dynamic_cast<TabItem*>(&Owner());
	auto* tabs = item
		? dynamic_cast<TabControl*>(item->GetLogicalParent()) : nullptr;
	if (!item || !tabs) return false;
	value = tabs->IndexOfItem(item) == tabs->SelectedIndex;
	return true;
}

AutomationOperationResult TabItemAutomationPeer::Select()
{
	const auto enabled = EnsureEnabled(Owner());
	if (enabled != AutomationOperationResult::Succeeded) return enabled;
	auto* item = dynamic_cast<TabItem*>(&Owner());
	auto* tabs = item
		? dynamic_cast<TabControl*>(item->GetLogicalParent()) : nullptr;
	if (!item || !tabs) return AutomationOperationResult::InvalidOperation;
	const int index = tabs->IndexOfItem(item);
	if (index < 0) return AutomationOperationResult::InvalidOperation;
	return tabs->SelectedIndex == index || tabs->SelectItem(index)
		? AutomationOperationResult::Succeeded
		: AutomationOperationResult::InvalidOperation;
}

Control* TabItemAutomationPeer::GetSelectionContainer() const
{
	return dynamic_cast<TabControl*>(Owner().GetLogicalParent());
}

TabControlAutomationPeer::TabControlAutomationPeer(Control& owner) :
	AutomationPeer(owner, AutomationControlType::Tab, L"TabControl")
{
}

AutomationPattern TabControlAutomationPeer::GetPatternSet() const noexcept
{
	return AutomationPattern::Selection;
}

Control* TabControlAutomationPeer::GetSelectedItem() const
{
	auto* tabs = dynamic_cast<TabControl*>(&Owner());
	return tabs ? tabs->GetItem(tabs->SelectedIndex) : nullptr;
}
