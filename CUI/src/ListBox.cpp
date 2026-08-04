#include "ListBox.h"

#include "DependencyPropertyInfrastructure.h"
#include "Window.h"

#include <algorithm>
#include <limits>
#include <utility>

namespace
{
	bool ContainsIndex(const std::vector<int>& values, int value)
	{
		return std::binary_search(values.begin(), values.end(), value);
	}
}

const DependencyProperty& ListBox::SelectionModeProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ListBox, int> options;
		options.DefaultValue = static_cast<int>(SelectionMode::Single);
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const int& value)
		{
			return value >= static_cast<int>(SelectionMode::Single)
				&& value <= static_cast<int>(SelectionMode::Extended);
		};
		options.Changed = [](
			ListBox& target, const int& oldValue, const int& newValue)
		{
			target.ApplySelectionModeChange(
				static_cast<SelectionMode>(oldValue),
				static_cast<SelectionMode>(newValue));
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 110;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"Single", BindingValue(static_cast<int>(SelectionMode::Single)) },
			{ L"Multiple", BindingValue(static_cast<int>(SelectionMode::Multiple)) },
			{ L"Extended", BindingValue(static_cast<int>(SelectionMode::Extended)) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<ListBox, int>(
			DependencyPropertyRegistrationLiteral(L"SelectionMode"),
			[](ListBox& target)
			{ return static_cast<int>(target._selectionMode); },
			[](ListBox& target, const int& value)
			{ target._selectionMode = static_cast<SelectionMode>(value); },
			{}, std::move(options));
	}();
	return *registration;
}

ListBox::ListBox()
	: Selector()
{
#if CUI_ENABLE_DYNAMIC_XAML
	EnsureBindingPropertiesRegistered();
#endif
}

void ListBox::RegisterDependencyProperties()
{
	Selector::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)SelectionModeProperty();
#endif
}

void ListBox::SetSelectionMode(SelectionMode value)
{
	(void)TrySetPropertyValue(
		SelectionModeProperty(),
		BindingValue(static_cast<int>(value)));
}

std::vector<BindingValue> ListBox::GetSelectedItems() const
{
	std::vector<BindingValue> result;
	result.reserve(_selectedIndices.size());
	for (const int index : _selectedIndices)
	{
		if (index >= 0
			&& static_cast<size_t>(index) < SelectionItemCount())
			result.push_back(GetSelectionItemAt(static_cast<size_t>(index)));
	}
	return result;
}

void ListBox::SelectAll()
{
	if (_selectionMode == SelectionMode::Single) return;
	std::vector<int> selected;
	selected.reserve(SelectionItemCount());
	for (size_t index = 0; index < SelectionItemCount(); ++index)
		selected.push_back(static_cast<int>(index));
	(void)ApplySelection(
		std::move(selected),
		GetSelectedIndex() >= 0 ? GetSelectedIndex() : 0,
		GetSelectedIndex());
}

void ListBox::UnselectAll()
{
	(void)ApplySelection({}, -1, GetSelectedIndex());
}

bool ListBox::SelectIndex(int value)
{
	const int normalized = ClampIndex(value);
	if (normalized < 0)
	{
		if (_selectedIndices.empty() && GetSelectedIndex() < 0)
			return false;
		return ApplySelection({}, -1, GetSelectedIndex());
	}
	if (_selectionMode == SelectionMode::Single)
		return ApplySelection({ normalized }, normalized, normalized);

	auto selected = _selectedIndices;
	if (!ContainsIndex(selected, normalized))
		selected.push_back(normalized);
	return ApplySelection(std::move(selected), normalized, normalized);
}

bool ListBox::IsIndexSelected(size_t index) const noexcept
{
	if (index > static_cast<size_t>((std::numeric_limits<int>::max)()))
		return false;
	return ContainsIndex(_selectedIndices, static_cast<int>(index));
}

void ListBox::ApplySelectionModeChange(
	SelectionMode,
	SelectionMode newValue)
{
	if (newValue == SelectionMode::Single && _selectedIndices.size() > 1)
	{
		const int primary = GetSelectedIndex() >= 0
			? GetSelectedIndex() : _selectedIndices.front();
		(void)ApplySelection({ primary }, primary, primary);
	}
}

bool ListBox::ApplySelection(
	std::vector<int> indices,
	int preferredPrimary,
	int actionIndex)
{
	const int count = static_cast<int>(SelectionItemCount());
	indices.erase(std::remove_if(
		indices.begin(), indices.end(),
		[count](int value) { return value < 0 || value >= count; }),
		indices.end());
	std::sort(indices.begin(), indices.end());
	indices.erase(std::unique(indices.begin(), indices.end()), indices.end());
	if (_selectionMode == SelectionMode::Single && indices.size() > 1)
		indices = { preferredPrimary >= 0
			? preferredPrimary : indices.back() };

	int primary = -1;
	if (preferredPrimary >= 0 && ContainsIndex(indices, preferredPrimary))
		primary = preferredPrimary;
	else if (!indices.empty())
		primary = indices.back();

	const auto previousIndices = _selectedIndices;
	const int previousPrimary = GetSelectedIndex();
	if (previousIndices == indices && previousPrimary == primary)
	{
		if (actionIndex >= 0) FocusIndex(actionIndex);
		return false;
	}

	std::vector<BindingValue> removed;
	std::vector<BindingValue> added;
	for (const int index : previousIndices)
		if (!ContainsIndex(indices, index))
			removed.push_back(
				GetSelectionItemAt(static_cast<size_t>(index)));
	for (const int index : indices)
		if (!ContainsIndex(previousIndices, index))
			added.push_back(
				GetSelectionItemAt(static_cast<size_t>(index)));

	_applyingSelection = true;
	_selectedIndices = std::move(indices);
	try
	{
		SetCurrentSelectedIndexWithoutSelectionChanged(primary);
		RefreshSelectedItemState(false);
		_applyingSelection = false;
	}
	catch (...)
	{
		_selectedIndices = previousIndices;
		_applyingSelection = false;
		throw;
	}
	CaptureSelectionIdentities();
	RaiseSelectionChanged(
		previousPrimary, primary, std::move(removed), std::move(added));
	if (actionIndex >= 0) FocusIndex(actionIndex);
	return true;
}

void ListBox::SelectOnly(int index)
{
	(void)ApplySelection(
		index < 0 ? std::vector<int>{} : std::vector<int>{ index },
		index, index);
	UpdateAnchor(index);
}

void ListBox::ToggleIndex(int index)
{
	auto selected = _selectedIndices;
	const auto found = std::lower_bound(
		selected.begin(), selected.end(), index);
	if (found != selected.end() && *found == index)
		selected.erase(found);
	else
		selected.insert(found, index);
	(void)ApplySelection(
		std::move(selected),
		IsIndexSelected(static_cast<size_t>(index))
			? GetSelectedIndex() : index,
		index);
	UpdateAnchor(index);
}

void ListBox::SelectRange(int index, bool clearCurrent)
{
	if (_anchorIndex < 0) _anchorIndex =
		GetSelectedIndex() >= 0 ? GetSelectedIndex() : index;
	const int first = (std::min)(_anchorIndex, index);
	const int last = (std::max)(_anchorIndex, index);
	std::vector<int> selected = clearCurrent
		? std::vector<int>{} : _selectedIndices;
	for (int candidate = first; candidate <= last; ++candidate)
		selected.push_back(candidate);
	(void)ApplySelection(std::move(selected), index, index);
}

void ListBox::MakeKeyboardSelection(
	int index,
	ModifierKeys modifiers)
{
	const bool control =
		HasModifier(modifiers, ModifierKeys::Control);
	const bool shift =
		HasModifier(modifiers, ModifierKeys::Shift);
	if (_selectionMode == SelectionMode::Extended && shift)
		SelectRange(index, !control);
	else if (_selectionMode == SelectionMode::Extended && control)
		FocusIndex(index);
	else
		SelectOnly(index);
}

void ListBox::NotifyItemClicked(
	size_t index,
	MouseButton button,
	ModifierKeys modifiers)
{
	if (index >= SelectionItemCount()) return;
	const int itemIndex = static_cast<int>(index);
	if (button == MouseButton::Right)
	{
		if (!IsIndexSelected(index)) SelectOnly(itemIndex);
		return;
	}
	if (button != MouseButton::Left && button != MouseButton::None) return;

	if (_selectionMode == SelectionMode::Single)
		SelectOnly(itemIndex);
	else if (_selectionMode == SelectionMode::Multiple)
		ToggleIndex(itemIndex);
	else if (HasModifier(modifiers, ModifierKeys::Shift))
		SelectRange(
			itemIndex,
			!HasModifier(modifiers, ModifierKeys::Control));
	else if (HasModifier(modifiers, ModifierKeys::Control))
		ToggleIndex(itemIndex);
	else
		SelectOnly(itemIndex);
}

void ListBox::RequestItemSelection(size_t index, bool selected)
{
	if (index >= SelectionItemCount()) return;
	const int itemIndex = static_cast<int>(index);
	if (selected)
	{
		if (_selectionMode == SelectionMode::Single)
			SelectOnly(itemIndex);
		else
			(void)SelectIndex(itemIndex);
	}
	else if (IsIndexSelected(index))
	{
		if (_selectionMode == SelectionMode::Single)
			UnselectAll();
		else
			ToggleIndex(itemIndex);
	}
}

bool ListBox::ProcessItemKey(
	size_t itemIndex,
	const InputReport& input)
{
	if (itemIndex >= SelectionItemCount()
		|| input.Kind != InputReportKind::KeyDown) return false;
	if (input.Key == Key::Space || input.Key == Key::Return)
	{
		const int index = static_cast<int>(itemIndex);
		if (_selectionMode == SelectionMode::Multiple
			|| (_selectionMode == SelectionMode::Extended
				&& input.HasModifier(ModifierKeys::Control)))
			ToggleIndex(index);
		else if (_selectionMode == SelectionMode::Extended
			&& input.HasModifier(ModifierKeys::Shift))
			SelectRange(index, true);
		else
			SelectOnly(index);
		return true;
	}
	return HandleSelectionKey(
		static_cast<int>(itemIndex), input, true);
}

bool ListBox::HandleSelectionKey(
	int itemIndex,
	const InputReport& input,
	bool)
{
	if (input.Kind != InputReportKind::KeyDown
		|| SelectionItemCount() == 0) return false;
	const int count = static_cast<int>(SelectionItemCount());
	int next = itemIndex >= 0 ? itemIndex : GetSelectedIndex();
	if (next < 0) next = 0;
	switch (input.Key)
	{
	case Key::Up: --next; break;
	case Key::Down: ++next; break;
	case Key::Home: next = 0; break;
	case Key::End: next = count - 1; break;
	case Key::PageUp: next -= 5; break;
	case Key::PageDown: next += 5; break;
	default:
		if (input.Key == Key::A
			&& input.HasModifier(ModifierKeys::Control)
			&& _selectionMode != SelectionMode::Single)
		{
			SelectAll();
			return true;
		}
		return false;
	}
	next = (std::clamp)(next, 0, count - 1);
	MakeKeyboardSelection(next, input.Modifiers);
	return true;
}

bool ListBox::HandlesNavigationKey(Key key) const
{
	return key == Key::Space || key == Key::Return
		|| key == Key::A
		|| Selector::HandlesNavigationKey(key);
}

bool ListBox::ProcessInput(const InputReport& input)
{
	if (HandleSelectionKey(
		_focusedIndex >= 0 ? _focusedIndex : GetSelectedIndex(),
		input, false))
	{
		auto args = input.CreateKeyEventArgs();
		OnKeyDown(this, args);
		return true;
	}
	return Selector::ProcessInput(input);
}

void ListBox::OnSelectedIndexChanged(int, int newValue)
{
	if (_applyingSelection || _restoringSelectionIdentities) return;
	_selectedIndices.clear();
	if (newValue >= 0) _selectedIndices.push_back(newValue);
	_focusedIndex = newValue;
	UpdateAnchor(newValue);
	CaptureSelectionIdentities();
}

void ListBox::OnGeneratedItemsRebuilt()
{
	if (_selectionMode == SelectionMode::Single)
	{
		// Selector owns WPF's single-selection remapping rules, including
		// keeping the index on Replace and following the CollectionView
		// current item when the selected record leaves the view.
		Selector::OnGeneratedItemsRebuilt();
		_selectedIndices.clear();
		if (GetSelectedIndex() >= 0)
			_selectedIndices.push_back(GetSelectedIndex());
		CaptureSelectionIdentities();
		return;
	}
	const auto sourceIdentities = _selectedSourceIdentities;
	const auto primaryIdentity = _primarySourceIdentity;
	_restoringSelectionIdentities = true;
	Selector::OnGeneratedItemsRebuilt();
	_restoringSelectionIdentities = false;
	_selectedSourceIdentities = sourceIdentities;
	_primarySourceIdentity = primaryIdentity;
	RestoreSelectionIdentities();
}

void ListBox::OnAuthoredItemsChanged() noexcept
{
	if (_selectionMode == SelectionMode::Single)
	{
		Selector::OnAuthoredItemsChanged();
		_selectedIndices.clear();
		if (GetSelectedIndex() >= 0)
			_selectedIndices.push_back(GetSelectedIndex());
		CaptureSelectionIdentities();
		return;
	}
	const auto authoredIdentities = _selectedAuthoredIdentities;
	const auto primaryIdentity = _primaryAuthoredIdentity;
	_restoringSelectionIdentities = true;
	Selector::OnAuthoredItemsChanged();
	_restoringSelectionIdentities = false;
	_selectedAuthoredIdentities = authoredIdentities;
	_primaryAuthoredIdentity = primaryIdentity;
	try { RestoreSelectionIdentities(); }
	catch (...) {}
}

void ListBox::FocusIndex(int index)
{
	if (index < 0
		|| static_cast<size_t>(index) >= SelectionItemCount()) return;
	_focusedIndex = index;
	(void)BringItemIntoView(static_cast<size_t>(index));
	auto* item = GetItemsView()
		? GetGeneratedItem(static_cast<size_t>(index))
		: GetAuthoredItem(static_cast<size_t>(index));
	if (auto* window = GetPresentationWindow())
		window->SetKeyboardFocus(item ? item : this, true);
}

void ListBox::UpdateAnchor(int index) noexcept
{
	_anchorIndex = index >= 0 ? index : -1;
}

void ListBox::CaptureSelectionIdentities() noexcept
{
	_selectedSourceIdentities.clear();
	_selectedAuthoredIdentities.clear();
	_primarySourceIdentity = {};
	_primaryAuthoredIdentity = nullptr;
	try
	{
		if (const auto source = GetItemsView())
		{
			for (const int index : _selectedIndices)
			{
				BindingSourceReference item;
				if (index >= 0
					&& static_cast<size_t>(index) < source.Get()->Count()
					&& source.Get()->TryGetItem(
						static_cast<size_t>(index), item)
					&& item)
					_selectedSourceIdentities.push_back(std::move(item));
			}
			const int primary = GetSelectedIndex();
			if (primary >= 0
				&& static_cast<size_t>(primary) < source.Get()->Count())
				(void)source.Get()->TryGetItem(
					static_cast<size_t>(primary), _primarySourceIdentity);
			return;
		}
		for (const int index : _selectedIndices)
			if (index >= 0)
				_selectedAuthoredIdentities.emplace_back(
					GetAuthoredItem(static_cast<size_t>(index)));
		if (GetSelectedIndex() >= 0)
			_primaryAuthoredIdentity =
				GetAuthoredItem(static_cast<size_t>(GetSelectedIndex()));
	}
	catch (...) {}
}

void ListBox::RestoreSelectionIdentities()
{
	std::vector<int> restored;
	int primary = -1;
	if (const auto source = GetItemsView())
	{
		for (size_t index = 0; index < source.Get()->Count(); ++index)
		{
			BindingSourceReference item;
			if (!source.Get()->TryGetItem(index, item) || !item) continue;
			if (_primarySourceIdentity
				&& item.Shared() == _primarySourceIdentity.Shared())
				primary = static_cast<int>(index);
			if (std::any_of(
				_selectedSourceIdentities.begin(),
				_selectedSourceIdentities.end(),
				[&](const BindingSourceReference& selected)
				{ return selected.Shared() == item.Shared(); }))
				restored.push_back(static_cast<int>(index));
		}
	}
	else
	{
		for (size_t index = 0; index < AuthoredItemCount(); ++index)
		{
			auto* item = GetAuthoredItem(index);
			if (_primaryAuthoredIdentity.Get() == item)
				primary = static_cast<int>(index);
			if (std::any_of(
				_selectedAuthoredIdentities.begin(),
				_selectedAuthoredIdentities.end(),
				[item](const ControlWeakReference& selected)
				{ return selected.Get() == item; }))
				restored.push_back(static_cast<int>(index));
		}
	}
	if (primary < 0 && !restored.empty()) primary = restored.back();
	_applyingSelection = true;
	_selectedIndices = std::move(restored);
	SetCurrentSelectedIndexWithoutSelectionChanged(primary);
	RefreshSelectedItemState(false);
	_applyingSelection = false;
	CaptureSelectionIdentities();
}
