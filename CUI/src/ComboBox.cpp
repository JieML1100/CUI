#include "ComboBox.h"
#include "DependencyPropertyInfrastructure.h"
#include "EventInfrastructure.h"
#include "StyleInfrastructure.h"

#include "ItemsPresenter.h"
#include "InputManager.h"
#include "Popup.h"
#include "ScrollViewer.h"
#include "TemplateInfrastructure.h"
#include "TextBox.h"
#include "TreeInfrastructure.h"
#include "Window.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
	const ItemsPanelTemplateReference& DefaultComboBoxItemsPanel()
	{
		static const auto definition = []
		{
			auto value = std::make_shared<ItemsPanelTemplate>();
			value->Kind = ItemsPanelKind::VirtualizingStack;
			value->Orientation = Orientation::Vertical;
			value->ItemHeight = 28.0f;
			value->CacheLength = 1.0f;
			return ItemsPanelTemplateReference(std::move(value));
		}();
		return definition;
	}

	bool StartsWith(
		const std::wstring& value,
		const std::wstring& prefix,
		bool caseSensitive)
	{
		if (prefix.size() > value.size()) return false;
		for (size_t index = 0; index < prefix.size(); ++index)
		{
			auto left = value[index];
			auto right = prefix[index];
			if (!caseSensitive)
			{
				left = static_cast<wchar_t>(std::towlower(left));
				right = static_cast<wchar_t>(std::towlower(right));
			}
			if (left != right) return false;
		}
		return true;
	}

	template<typename TValue>
	DependencyPropertyOptions<ComboBox, TValue> ComboBoxOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			const wchar_t* category,
			int categoryOrder,
			int order,
			DependencyPropertyEditorKind editor),
		DependencyPropertyFlags flags)
	{
		DependencyPropertyOptions<ComboBox, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return options;
	}

	template<typename TValue>
	DependencyPropertyOptions<ComboBox, TValue> ComboBoxProjectionOptions(
		TValue defaultValue CUI_DESIGN_METADATA_ARGUMENTS(int order))
	{
		auto options = ComboBoxOptions(
			std::move(defaultValue) CUI_DESIGN_METADATA_ARGUMENTS(
				L"State", 70, order, DependencyPropertyEditorKind::Auto),
			DependencyPropertyFlags::AffectsRender);
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		)
		return options;
	}

	using DependencyPropertyAccessor = const DependencyProperty& (*)();

	auto ComboBoxSubscriber(DependencyPropertyAccessor propertyAccessor)
	{
		return [propertyAccessor](
			ComboBox& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyAccessor, handler = std::move(handler)](
					DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.Property == &propertyAccessor()) handler();
				});
		};
	}

	bool Intersects(const D2D1_RECT_F& left, const D2D1_RECT_F& right) noexcept
	{
		return left.left < right.right && left.right > right.left
			&& left.top < right.bottom && left.bottom > right.top;
	}

	bool IsWithinVisualSubtree(
		Control* source, const Control* subtreeRoot) noexcept
	{
		if (!source || !subtreeRoot) return false;
		for (auto* current = source; current;
			current = current->GetVisualParent())
			if (current == subtreeRoot) return true;
		return false;
	}

}

const DependencyProperty& ComboBoxItem::IsHighlightedProperty()
{
	return IsHighlightedPropertyKey().Property();
}

const DependencyPropertyKey& ComboBoxItem::IsHighlightedPropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ComboBoxItem, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = 30;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Browsable = false;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			ComboBoxItem, bool>(
				DependencyPropertyRegistrationLiteral(L"IsHighlighted"),
				[](ComboBoxItem& target) { return target.GetIsHighlighted(); },
				[](ComboBoxItem& target, const bool& value)
				{
					(void)target.SetReadOnlyPropertyField(
						IsHighlightedPropertyKey(),
						target._isHighlighted, value);
				}, {}, std::move(options));
	}();
	return registration.Key();
}

ComboBoxItem::ComboBoxItem() = default;

void ComboBoxItem::RegisterDependencyProperties()
{
	ListBoxItem::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsHighlightedProperty();
#endif
}

bool ComboBoxItem::ProcessInput(const InputReport& input)
{
	auto* owner = dynamic_cast<ComboBox*>(GetLogicalParent());
	if (owner && input.Kind == InputReportKind::KeyDown
		&& owner->ProcessItemKey(ItemIndex(), input))
		return true;
	return ListBoxItem::ProcessInput(input);
}

void ComboBoxItem::ActivateItem(
	MouseButton button, ModifierKeys)
{
	if (button != MouseButton::Left) return;
	auto* owner = dynamic_cast<ComboBox*>(GetLogicalParent());
	if (!owner) return;
	owner->SetHighlightedIndex(
		static_cast<int>(ItemIndex()), false);
	owner->CommitHighlightedSelection();
}

void ComboBoxItem::FocusOwner()
{
	auto* owner = dynamic_cast<ComboBox*>(GetLogicalParent());
	if (owner && owner->GetPresentationWindow())
		owner->GetPresentationWindow()->SetKeyboardFocus(owner, true);
}

void ComboBoxItem::OnIsSelectedRequested(bool value)
{
	auto* owner = dynamic_cast<ComboBox*>(GetLogicalParent());
	if (!owner) return;
	const int index = static_cast<int>(ItemIndex());
	if (value) (void)owner->SelectIndex(index);
	else if (owner->GetSelectedIndex() == index)
		(void)owner->SelectIndex(-1);
}

void ComboBoxItem::OnIsMouseOverChanged(bool oldValue, bool newValue)
{
	ListBoxItem::OnIsMouseOverChanged(oldValue, newValue);
	if (!newValue) return;
	if (auto* owner = dynamic_cast<ComboBox*>(GetLogicalParent()))
		owner->NotifyItemHighlighted(ItemIndex());
}

void ComboBoxItem::SetIsHighlighted(bool value)
{
	if (_isHighlighted == value) return;
	if (!SetReadOnlyPropertyField(
		IsHighlightedPropertyKey(),
		_isHighlighted, value)) return;
	SetStyleState(ControlStyleState::Selected, value);
	InvalidateVisual();
}

const DependencyProperty& ComboBox::TextProperty()
{
	static const auto registration = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		DependencyPropertyOptions<ComboBox, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender
			| DependencyPropertyFlags::BindsTwoWayByDefault;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		options.Changed = [](
			ComboBox& target,
			const std::wstring& oldValue, const std::wstring& newValue)
		{
			target.ApplyTextChange(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<
			ComboBox, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"Text"),
				[](ComboBox& target, Handler handler,
					DataSourceUpdateMode mode)
				{
					if (mode == DataSourceUpdateMode::OnValidation)
					{
						return target.OnLostFocus.Subscribe(
							[handler = std::move(handler)](Control*)
							{ handler(); });
					}
					return target.OnPropertyValueChanged.Subscribe(
						[handler = std::move(handler)](
							DependencyObject*,
							const DependencyPropertyChangedEventArgs& args)
						{
							if (args.Property == &ComboBox::TextProperty())
								handler();
						});
				}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ComboBox::IsDropDownOpenProperty()
{
	static const auto registration = []
	{
		auto options = ComboBoxOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				L"Behavior", 110, 10, DependencyPropertyEditorKind::Boolean),
			DependencyPropertyFlags::AffectsArrange
				| DependencyPropertyFlags::AffectsRender);
		options.Changed = [](
			ComboBox& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyIsDropDownOpenChange(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<ComboBox, bool>(
			DependencyPropertyRegistrationLiteral(L"IsDropDownOpen"),
			[](ComboBox& target) { return target.GetIsDropDownOpen(); },
			[](ComboBox& target, const bool& value)
			{ target.SetIsDropDownOpen(value); },
			ComboBoxSubscriber(&ComboBox::IsDropDownOpenProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ComboBox::IsEditableProperty()
{
	static const auto registration = []
	{
		auto options = ComboBoxOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				L"Behavior", 110, 20, DependencyPropertyEditorKind::Boolean),
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender);
		options.Changed = [](
			ComboBox& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyIsEditableChange(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<ComboBox, bool>(
			DependencyPropertyRegistrationLiteral(L"IsEditable"),
			[](ComboBox& target) { return target.GetIsEditable(); },
			[](ComboBox& target, const bool& value)
			{ target.SetIsEditable(value); },
			ComboBoxSubscriber(&ComboBox::IsEditableProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ComboBox::IsReadOnlyProperty()
{
	static const auto registration = []
	{
		auto options = ComboBoxOptions(
			false CUI_DESIGN_METADATA_ARGUMENTS(
				L"Behavior", 110, 30, DependencyPropertyEditorKind::Boolean),
			DependencyPropertyFlags::None);
		options.Changed = [](
			ComboBox& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyIsReadOnlyChange(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<ComboBox, bool>(
			DependencyPropertyRegistrationLiteral(L"IsReadOnly"),
			[](ComboBox& target) { return target.GetIsReadOnly(); },
			[](ComboBox& target, const bool& value)
			{ target.SetIsReadOnly(value); },
			ComboBoxSubscriber(&ComboBox::IsReadOnlyProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ComboBox::StaysOpenOnEditProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<ComboBox, bool>(
			DependencyPropertyRegistrationLiteral(L"StaysOpenOnEdit"),
			[](ComboBox& target) { return target.GetStaysOpenOnEdit(); },
			[](ComboBox& target, const bool& value)
			{ target.SetStaysOpenOnEdit(value); },
			ComboBoxSubscriber(&ComboBox::StaysOpenOnEditProperty),
			ComboBoxOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					L"Behavior", 110, 40,
					DependencyPropertyEditorKind::Boolean),
				DependencyPropertyFlags::None));
	}();
	return *registration;
}

const DependencyProperty&
ComboBox::ShouldPreserveUserEnteredPrefixProperty()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterStatic<ComboBox, bool>(
			DependencyPropertyRegistrationLiteral(
				L"ShouldPreserveUserEnteredPrefix"),
			[](ComboBox& target)
			{ return target.GetShouldPreserveUserEnteredPrefix(); },
			[](ComboBox& target, const bool& value)
			{ target.SetShouldPreserveUserEnteredPrefix(value); },
			ComboBoxSubscriber(
				&ComboBox::ShouldPreserveUserEnteredPrefixProperty),
			ComboBoxOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(
					L"Behavior", 110, 50,
					DependencyPropertyEditorKind::Boolean),
				DependencyPropertyFlags::None));
	}();
	return *registration;
}

const DependencyProperty& ComboBox::MaxDropDownHeightProperty()
{
	static const auto registration = []
	{
		auto options = ComboBoxOptions(
			320.0f CUI_DESIGN_METADATA_ARGUMENTS(
				L"Layout", 100, 10, DependencyPropertyEditorKind::Number),
			DependencyPropertyFlags::AffectsArrange);
		options.Validate = [](const float& proposed)
		{
			return std::isfinite(proposed);
		};
		options.Coerce = [](
			ComboBox&, const float& proposed) -> std::optional<float>
		{
			return (std::max)(0.0f, proposed);
		};
		options.Changed = [](
			ComboBox& target, const float&, const float&)
		{
			target.ApplyMaxDropDownHeight();
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Minimum = 0.0;
		options.Design.Step = 1.0;
		)
		return DependencyPropertyRegistry::RegisterStatic<ComboBox, float>(
			DependencyPropertyRegistrationLiteral(L"MaxDropDownHeight"),
			[](ComboBox& target) { return target.GetMaxDropDownHeight(); },
			[](ComboBox& target, const float& value)
			{ target.SetMaxDropDownHeight(value); },
			ComboBoxSubscriber(&ComboBox::MaxDropDownHeightProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& ComboBox::SelectionBoxItemProperty()
{
	return SelectionBoxItemPropertyKey().Property();
}

const DependencyPropertyKey& ComboBox::SelectionBoxItemPropertyKey()
{
	static const auto registration = []
	{
		auto options = ComboBoxProjectionOptions(
			BindingValue(std::wstring{})
			CUI_DESIGN_METADATA_ARGUMENTS(60));
		// SelectionBoxItem is commonly a BindingSourceReference.  BindingValue's
		// generic object comparison is intentionally conservative, so compare
		// projected records by identity just as Selector does for SelectedItem.
		// Otherwise PreparePresentation republishes the same selected record and
		// AffectsRender schedules another frame indefinitely.
		options.Equals = [](const BindingValue& left, const BindingValue& right)
		{
			return BindingItemValuesEqual(left, right);
		};
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			ComboBox, BindingValue>(
				DependencyPropertyRegistrationLiteral(L"SelectionBoxItem"),
				[](ComboBox& target) { return target.GetSelectionBoxItem(); },
				[](ComboBox& target, const BindingValue& value)
				{
					(void)target.SetReadOnlyPropertyField(
						SelectionBoxItemPropertyKey(),
						target._selectionBoxItem, value);
				}, {}, std::move(options));
	}();
	return registration.Key();
}

const DependencyProperty& ComboBox::SelectionBoxItemTemplateProperty()
{
	return SelectionBoxItemTemplatePropertyKey().Property();
}

const DependencyPropertyKey& ComboBox::SelectionBoxItemTemplatePropertyKey()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			ComboBox, ItemTemplateReference>(
				DependencyPropertyRegistrationLiteral(
					L"SelectionBoxItemTemplate"),
				[](ComboBox& target)
				{ return target.GetSelectionBoxItemTemplate(); },
				[](ComboBox& target, const ItemTemplateReference& value)
				{
					(void)target.SetReadOnlyPropertyField(
						SelectionBoxItemTemplatePropertyKey(),
						target._selectionBoxItemTemplate, value);
				}, {}, ComboBoxProjectionOptions(
					ItemTemplateReference{}
					CUI_DESIGN_METADATA_ARGUMENTS(70)));
	}();
	return registration.Key();
}

const DependencyProperty& ComboBox::SelectionBoxItemStringFormatProperty()
{
	return SelectionBoxItemStringFormatPropertyKey().Property();
}

const DependencyPropertyKey&
ComboBox::SelectionBoxItemStringFormatPropertyKey()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			ComboBox, std::wstring>(
				DependencyPropertyRegistrationLiteral(
					L"SelectionBoxItemStringFormat"),
				[](ComboBox& target)
				{ return target.GetSelectionBoxItemStringFormat(); },
				[](ComboBox& target, const std::wstring& value)
				{
					(void)target.SetReadOnlyPropertyField(
						SelectionBoxItemStringFormatPropertyKey(),
						target._selectionBoxItemStringFormat, value);
				}, {}, ComboBoxProjectionOptions(
					std::wstring{} CUI_DESIGN_METADATA_ARGUMENTS(80)));
	}();
	return registration.Key();
}

const DependencyProperty& ComboBox::IsSelectionBoxHighlightedProperty()
{
	return IsSelectionBoxHighlightedPropertyKey().Property();
}

const DependencyPropertyKey&
ComboBox::IsSelectionBoxHighlightedPropertyKey()
{
	static const auto registration = []
	{
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<ComboBox, bool>(
			DependencyPropertyRegistrationLiteral(L"IsSelectionBoxHighlighted"),
			[](ComboBox& target)
			{ return target.GetIsSelectionBoxHighlighted(); },
			[](ComboBox& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					IsSelectionBoxHighlightedPropertyKey(),
					target._isSelectionBoxHighlighted, value);
			}, {}, ComboBoxProjectionOptions(
				false CUI_DESIGN_METADATA_ARGUMENTS(90)));
	}();
	return registration.Key();
}

void ComboBox::RegisterDependencyProperties()
{
	Selector::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)TextProperty();
	(void)IsDropDownOpenProperty();
	(void)IsEditableProperty();
	(void)IsReadOnlyProperty();
	(void)StaysOpenOnEditProperty();
	(void)ShouldPreserveUserEnteredPrefixProperty();
	(void)MaxDropDownHeightProperty();
	(void)SelectionBoxItemProperty();
	(void)SelectionBoxItemTemplateProperty();
	(void)SelectionBoxItemStringFormatProperty();
	(void)IsSelectionBoxHighlightedProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)RegisterControlBorderThicknessMetadata<
		ComboBox, Selector>(
			1.0f CUI_DESIGN_METADATA_ARGUMENTS(30));
	)
}

const DependencyPropertyMetadata*
ComboBox::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &Control::BorderThicknessProperty())
	{
		return &RegisterControlBorderThicknessMetadata<
			ComboBox, Selector>(
				1.0f CUI_DESIGN_METADATA_ARGUMENTS(30)).Metadata();
	}
	return Selector::ResolveExactDependencyPropertyMetadata(property);
}

GET_CPP(ComboBox, std::wstring, Text)
{
	return GetDependencyPropertyValue<std::wstring>(TextProperty());
}

std::wstring ComboBox::GetSemanticText() const
{
	return GetDependencyPropertyValue<std::wstring>(TextProperty());
}

SET_CPP(ComboBox, std::wstring, Text)
{
	(void)SetDependencyPropertyValue(TextProperty(), std::move(value));
}

void ComboBox::EnsureClassHandlers()
{
	static const std::vector<EventConnection> handlers = []
	{
		std::vector<EventConnection> result;
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_ComboBox, RoutedEventId::MouseDown,
			&ComboBox::HandleDescendantPointerPress));
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_ComboBox, RoutedEventId::MouseDoubleClick,
			&ComboBox::HandleDescendantPointerPress));
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_ComboBox, RoutedEventId::MouseUp,
			&ComboBox::HandleDescendantPointerRelease));
		return result;
	}();
	(void)handlers;
}

void ComboBox::HandleDescendantPointerPress(
	Control* sender, RoutedEventArgs& args)
{
	auto* combo = dynamic_cast<ComboBox*>(sender);
	if (!combo || args.OriginalSource == combo
		|| IsWithinVisualSubtree(args.OriginalSource, combo->_popup)
		|| !combo->IsEffectivelyEnabled() || !combo->IsVisible)
		return;
	auto& mouse = static_cast<MouseEventArgs&>(args);
	combo->BeginPointerPress(mouse);
}

void ComboBox::HandleDescendantPointerRelease(
	Control* sender, RoutedEventArgs& args)
{
	auto* combo = dynamic_cast<ComboBox*>(sender);
	if (!combo || args.OriginalSource == combo
		|| IsWithinVisualSubtree(args.OriginalSource, combo->_popup))
		return;
	auto& mouse = static_cast<MouseEventArgs&>(args);
	if (combo->CompletePointerPress(mouse))
		args.Handled = true;
}

bool ComboBox::IsOriginalSourceWithinTemplatePart(
	Control* source, TemplatePartToken part) const noexcept
{
	if (!source) return false;
	auto* partControl = const_cast<ComboBox*>(this)
		->FindDeclarativeTemplatePart(part);
	if (!partControl) return false;
	for (auto* current = source; current;
		current = current->GetVisualParent())
	{
		if (current == partControl) return true;
		if (current == this) break;
	}
	return false;
}

void ComboBox::BeginPointerPress(MouseEventArgs& args)
{
	if (args.ChangedButton != MouseButton::Left) return;
	if (_isEditable && IsOriginalSourceWithinTemplatePart(
		args.OriginalSource,
		MakeTemplatePartToken(L"PART_EditableTextBox")))
	{
		// Match WPF's editable ComboBox contract: the text editor is inside
		// the closed face, but it still counts as an outside-popup click unless
		// StaysOpenOnEdit was requested.
		if (_isDropDownOpen && !_staysOpenOnEdit)
			SetCurrentIsDropDownOpen(false);
		return;
	}
	_pointerPressActive = true;
	(void)Focus();
	(void)CaptureMouse();
}

bool ComboBox::CompletePointerPress(MouseEventArgs& args)
{
	if (args.ChangedButton != MouseButton::Left
		|| !_pointerPressActive) return false;
	_pointerPressActive = false;
	if (IsMouseCaptured()) (void)ReleaseMouseCapture();
	if (!IsEffectivelyEnabled() || !IsVisible) return true;
	SetCurrentIsDropDownOpen(!_isDropDownOpen);
	return true;
}

ComboBox::ComboBox()
	: Selector()
{
	EnsureClassHandlers();
	RegisterDependencyProperties();
	RendererBackgroundColor = cui::theme::palette::Surface;
	RendererBorderColor = cui::theme::palette::BorderStrong;
	RendererForegroundColor = cui::theme::palette::TextPrimary;
	(void)TrySetPropertyValue(
		Control::CursorProperty(), BindingValue(CursorKind::Hand),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		ItemsControl::ItemsPanelProperty(),
		BindingValue(DefaultComboBoxItemsPanel()),
		DependencyPropertyValueSource::Theme);
	if (auto* host = GetItemsHost())
		cui::framework::TemplateAccess::SetPresentationSuppressed(*host, true);
	RefreshItems();
}

ComboBox::~ComboBox()
{
	_popupOpened.Disconnect();
	_popupClosed.Disconnect();
	_editableTextChanged.Disconnect();
	if (_popup)
		(void)_popup->TrySetCurrentPropertyValue(
			Popup::IsOpenProperty(), BindingValue(false));
}

void ComboBox::SetIsDropDownOpen(bool value)
{
	(void)SetPropertyField(
		IsDropDownOpenProperty(), _isDropDownOpen, value);
}

void ComboBox::SetIsEditable(bool value)
{
	(void)SetPropertyField(IsEditableProperty(), _isEditable, value);
}

void ComboBox::SetIsReadOnly(bool value)
{
	(void)SetPropertyField(IsReadOnlyProperty(), _isReadOnly, value);
}

void ComboBox::SetStaysOpenOnEdit(bool value)
{
	(void)SetPropertyField(
		StaysOpenOnEditProperty(), _staysOpenOnEdit, value);
}

void ComboBox::SetShouldPreserveUserEnteredPrefix(bool value)
{
	(void)SetPropertyField(
		ShouldPreserveUserEnteredPrefixProperty(),
		_shouldPreserveUserEnteredPrefix, value);
}

void ComboBox::SetMaxDropDownHeight(float value)
{
	(void)SetPropertyField(
		MaxDropDownHeightProperty(), _maxDropDownHeight, value);
}

void ComboBox::ApplyIsDropDownOpenChange(bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	if (newValue)
	{
		_selectionBeforeDropDown = SelectedIndex;
		SetHighlightedIndex(SelectedIndex, false);
		if (EnsureDropDownInfrastructure() && _popup)
		{
			UpdateItemsHostPresentation();
			ApplyMaxDropDownHeight();
			(void)_popup->TrySetCurrentPropertyValue(
				Popup::IsOpenProperty(), BindingValue(true));
			_popup->UpdatePlacement();
			if (SelectedIndex >= 0)
				(void)BringItemIntoView(static_cast<size_t>(SelectedIndex));
		}
	}
	else if (_popup)
	{
		(void)_popup->TrySetCurrentPropertyValue(
			Popup::IsOpenProperty(), BindingValue(false));
	}
	UpdateItemsHostPresentation();
	if (newValue)
		cui::framework::EventAccess::Raise(DropDownOpened, this);
	else
		cui::framework::EventAccess::Raise(DropDownClosed, this);
	UpdateSelectionBoxState();
	NotifyAccessibilityStateChanged();
	InvalidateVisual();
}

void ComboBox::ApplyIsEditableChange(bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	SyncEditableTextBox();
	UpdateSelectionBoxState();
	RequestLayout();
	InvalidateVisual();
}

void ComboBox::ApplyIsReadOnlyChange(bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	if (_editableTextBox)
		(void)_editableTextBox->TrySetCurrentPropertyValue(
			TextBoxBase::IsReadOnlyProperty(), BindingValue(newValue));
}

void ComboBox::ApplyTextChange(
	const std::wstring& oldValue,
	const std::wstring& newValue)
{
	if (oldValue == newValue) return;
	SyncEditableTextBox();
	if (_isEditable && !_updatingTextFromSelection)
	{
		const int exact = FindItemByTextPrefix(newValue, true);
		if (exact >= 0)
		{
			_preserveTextDuringSelection =
				_shouldPreserveUserEnteredPrefix;
			(void)SelectItem(exact);
			_preserveTextDuringSelection = false;
			SetHighlightedIndex(exact, false);
		}
	}
}

void ComboBox::ApplyMaxDropDownHeight()
{
	if (!_popup) return;
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, Control::MaxHeightProperty(),
		BindingValue(_maxDropDownHeight),
		DependencyPropertyValueSource::Template);
	// Popup constrains the transient surface, while ScrollViewer owns the
	// viewport/extent contract. Constrain both so layout and automation observe
	// the same drop-down viewport.
	if (_dropDownScroll)
		(void)cui::framework::DependencyPropertyAccess::SetValue(
			*_dropDownScroll, Control::MaxHeightProperty(),
			BindingValue(_maxDropDownHeight),
			DependencyPropertyValueSource::Template);
	if (_popup->GetIsOpen()) _popup->UpdatePlacement();
}

Popup* ComboBox::ResolvePopupPart() const noexcept
{
	auto* presenter = GetTemplateItemsPresenter();
	for (auto* current = presenter ? presenter->GetVisualParent() : nullptr;
		current && current != this; current = current->GetVisualParent())
		if (auto* popup = dynamic_cast<Popup*>(current)) return popup;
	return dynamic_cast<Popup*>(GetControlTemplateRoot());
}

ScrollViewer* ComboBox::ResolveScrollOwner() const noexcept
{
	auto* presenter = GetTemplateItemsPresenter();
	for (auto* current = presenter ? presenter->GetVisualParent() : nullptr;
		current && current != this; current = current->GetVisualParent())
	{
		if (current == _popup) break;
		if (auto* scroll = dynamic_cast<ScrollViewer*>(current)) return scroll;
	}
	return nullptr;
}

void ComboBox::ConfigurePopupPart(Popup* popup)
{
	if (_popup == popup)
	{
		_dropDownScroll = ResolveScrollOwner();
		ApplyMaxDropDownHeight();
		return;
	}
	_popupOpened.Disconnect();
	_popupClosed.Disconnect();
	if (_popup && _popup != popup)
		(void)_popup->TrySetCurrentPropertyValue(
			Popup::IsOpenProperty(), BindingValue(false));
	_popup = popup;
	_dropDownScroll = nullptr;
	if (!_popup) return;
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, Popup::PlacementTargetProperty(),
		BindingValue(ControlWeakReference(this)),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, Popup::PlacementProperty(),
		BindingValue(PlacementMode::Bottom),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*_popup, Popup::StaysOpenProperty(), BindingValue(false),
		DependencyPropertyValueSource::Template);
	_popupOpened = _popup->Opened.Subscribe([this](Popup*)
	{
		if (!_isDropDownOpen)
			(void)SetCurrentPropertyField(
				IsDropDownOpenProperty(), _isDropDownOpen, true);
	});
	_popupClosed = _popup->Closed.Subscribe([this](Popup*)
	{
		if (_isDropDownOpen)
			(void)SetCurrentPropertyField(
				IsDropDownOpenProperty(), _isDropDownOpen, false);
	});
	_dropDownScroll = ResolveScrollOwner();
	ApplyMaxDropDownHeight();
}

bool ComboBox::EnsureDropDownInfrastructure()
{
	if (auto* resolved = ResolvePopupPart())
	{
		ConfigurePopupPart(resolved);
		_dropDownScroll = ResolveScrollOwner();
		UpdateItemsHostPresentation();
		return true;
	}
	// An effective authored/theme template may not have been materialized yet.
	// Never replace it with the popup-only native fallback: normal layout will
	// apply that template, then its TemplateBinding and presentation preparation
	// synchronize the pending IsDropDownOpen value. The fallback is reserved for
	// truly untemplated use.
	if (GetControlTemplateRoot() || GetTemplate())
	{
		if (GetControlTemplateRoot())
			SetLastTemplateError(
				L"ComboBox ControlTemplate 必须包含承载 ItemsPresenter 的 Popup。");
		UpdateItemsHostPresentation();
		return false;
	}
	if (!GetPresentationWindow()) return false;

	auto popup = std::make_unique<Popup>();
	auto* popupRaw = popup.get();
	cui::framework::TreeAccess::SetTemplatedParent(*popupRaw, this);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*popupRaw, Popup::PlacementTargetProperty(),
		BindingValue(ControlWeakReference(this)),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*popupRaw, Popup::PlacementProperty(),
		BindingValue(PlacementMode::Bottom),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*popupRaw, Popup::StaysOpenProperty(), BindingValue(false),
		DependencyPropertyValueSource::Template);

	auto scroll = std::make_unique<ScrollViewer>();
	auto* scrollRaw = scroll.get();
	cui::framework::TreeAccess::SetTemplatedParent(*scrollRaw, this);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*scrollRaw, Control::VerticalAlignmentProperty(),
		BindingValue(VerticalAlignment::Top),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*scrollRaw, Control::BackgroundProperty(),
		BindingValue(cui::drawing::MakeSolidColorBrush(
			cui::theme::palette::Surface)),
		DependencyPropertyValueSource::Theme);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*scrollRaw, Control::BorderBrushProperty(),
		BindingValue(cui::drawing::MakeSolidColorBrush(
			cui::theme::palette::Border)),
		DependencyPropertyValueSource::Theme);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*scrollRaw, Control::BorderThicknessProperty(),
		BindingValue(Thickness(1.0f)),
		DependencyPropertyValueSource::Template);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		*scrollRaw, ScrollViewer::HorizontalScrollBarVisibilityProperty(),
		BindingValue(ScrollBarVisibility::Disabled),
		DependencyPropertyValueSource::Template);

	auto presenter = std::make_unique<ItemsPresenter>();
	auto* presenterRaw = presenter.get();
	cui::framework::TreeAccess::SetTemplatedParent(*presenterRaw, this);
	scrollRaw->SetVisualContent(std::move(presenter));
	popupRaw->SetChild(std::move(scroll));

	_defaultPopup = popupRaw;
	_buildingDropDownInfrastructure = true;
	try
	{
		cui::framework::TemplateAccess::SetTemplateRoot(*this, std::move(popup));
		_buildingDropDownInfrastructure = false;
	}
	catch (...)
	{
		_buildingDropDownInfrastructure = false;
		throw;
	}
	if (!cui::framework::TemplateAccess::RegisterItemsPresenter(
		*this, presenterRaw))
		throw std::logic_error(
			"ComboBox fallback ItemsPresenter registration failed");
	ConfigurePopupPart(popupRaw);
	_dropDownScroll = scrollRaw;
	SetLastTemplateError({});
	UpdateItemsHostPresentation();
	return true;
}

void ComboBox::UpdateItemsHostPresentation()
{
	auto* host = GetItemsHost();
	if (!host) return;
	bool inPopup = false;
	if (_popup)
	{
		for (auto* current = host->GetVisualParent(); current;
			current = current->GetVisualParent())
		{
			if (current == _popup)
			{
				inPopup = true;
				break;
			}
			if (current == this) break;
		}
	}
	cui::framework::TemplateAccess::SetPresentationSuppressed(
		*host, !inPopup || !_isDropDownOpen);
}

void ComboBox::OnControlTemplatePresentationChanged()
{
	_popupOpened.Disconnect();
	_popupClosed.Disconnect();
	_editableTextChanged.Disconnect();
	if (_popup)
		(void)_popup->TrySetCurrentPropertyValue(
			Popup::IsOpenProperty(), BindingValue(false));
	_popup = nullptr;
	_dropDownScroll = nullptr;
	_editableTextBox = nullptr;
	if (GetControlTemplateRoot() != _defaultPopup)
		_defaultPopup = nullptr;
	ConfigurePopupPart(ResolvePopupPart());
	_editableTextBox = dynamic_cast<TextBox*>(
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_EditableTextBox")));
	if (_editableTextBox)
	{
		_editableTextChanged =
			_editableTextBox->OnPropertyValueChanged.Subscribe(
				[this](DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.Property != &TextBox::TextProperty()
						|| _updatingEditableTextBox
						|| !_editableTextBox) return;
					(void)TrySetCurrentPropertyValue(
						TextProperty(),
						BindingValue(_editableTextBox->GetText()));
				});
	}
	SyncEditableTextBox();
	ApplyIsReadOnlyChange(!_isReadOnly, _isReadOnly);
	UpdateSelectionBoxState();
	UpdateItemsHostPresentation();
	if (_popup && !_buildingDropDownInfrastructure)
		(void)_popup->TrySetCurrentPropertyValue(
			Popup::IsOpenProperty(), BindingValue(_isDropDownOpen));
}

void ComboBox::OnPresentationWindowChanged(
	Window* previousWindow, Window* currentWindow)
{
	Selector::OnPresentationWindowChanged(previousWindow, currentWindow);
	if (_isDropDownOpen && currentWindow
		&& EnsureDropDownInfrastructure() && _popup)
		(void)_popup->TrySetCurrentPropertyValue(
			Popup::IsOpenProperty(), BindingValue(true));
	UpdateSelectionBoxState();
}

std::unique_ptr<Control> ComboBox::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t index,
	BindingPathObservation& observation)
{
	observation = {};
	std::unique_ptr<ComboBoxItem> container;
	const auto containerTemplate = GetItemContainerTemplate();
	if (containerTemplate)
	{
		if (containerTemplate.Get()->TargetType()
			!= UIClass::UI_ComboBoxItem)
		{
			SetLastTemplateError(
				L"ItemContainerTemplate TargetType 必须是 ComboBoxItem。");
			return {};
		}
		std::wstring error;
		auto built = containerTemplate.Get()->Build(&error);
		auto* itemContainer = dynamic_cast<ComboBoxItem*>(built.get());
		if (!itemContainer)
		{
			SetLastTemplateError(error.empty()
				? L"ItemContainerTemplate 未生成 ComboBoxItem。"
				: std::move(error));
			return {};
		}
		container.reset(static_cast<ComboBoxItem*>(built.release()));
	}
	else container = std::make_unique<ComboBoxItem>();

	cui::framework::StyleAccess::SetResourceKey(
		*container, GetItemContainerStyle());
	std::wstring error;
	bool initialized = false;
#if CUI_ENABLE_DYNAMIC_XAML
	if (GetCompiledDisplayMemberPath().Empty())
		initialized = container->InitializeItem(
			item, GetItemTemplate(), GetDisplayMemberPath(),
			index, L"ComboBoxItem", &error);
	else
#endif
		initialized = container->InitializeItem(
			item, GetItemTemplate(), GetCompiledDisplayMemberPath(),
			index, L"ComboBoxItem", &error);
	if (!initialized)
	{
		SetLastTemplateError(error.empty()
			? L"ComboBoxItem 内容初始化失败。" : std::move(error));
		return {};
	}
	return container;
}

void ComboBox::OnGeneratedItemsRebuilt()
{
	Selector::OnGeneratedItemsRebuilt();
	RefreshItems();
}

void ComboBox::OnGeneratedItemsRealized()
{
	Selector::OnGeneratedItemsRealized();
	UpdateGeneratedItemStates();
}

void ComboBox::OnGeneratedItemIndexChanged(
	Control& visual, size_t oldIndex, size_t newIndex)
{
	Selector::OnGeneratedItemIndexChanged(visual, oldIndex, newIndex);
	if (auto* item = dynamic_cast<ComboBoxItem*>(&visual))
		item->SetItemIndex(newIndex);
}

bool ComboBox::ValidateAuthoredItemControl(
	const Control& item, std::string& error) const
{
	if (dynamic_cast<const ComboBoxItem*>(&item)) return true;
	error = "ComboBox authored Items must be ComboBoxItem controls";
	return false;
}

void ComboBox::OnAuthoredItemsChanged() noexcept
{
	Selector::OnAuthoredItemsChanged();
	try { RefreshItems(); }
	catch (...) {}
}

void ComboBox::OnItemsSourceChanged(
	const BindingListReference& oldValue,
	const BindingListReference& newValue)
{
	Selector::OnItemsSourceChanged(oldValue, newValue);
	RefreshItems();
}

std::wstring ComboBox::GetAuthoredItemText(size_t index) const
{
	auto* item = dynamic_cast<ComboBoxItem*>(GetAuthoredItem(index));
	if (!item) return {};
	std::wstring text;
	if (item->GetContent().TryGet(text)) return text;
	if (auto* content = item->GetVisualContent())
	{
		const auto displayText = content->GetDisplayText();
		if (!displayText.empty()) return displayText;
	}
	const auto displayText = item->GetDisplayText();
	if (!displayText.empty()) return displayText;
	return item->GetContent().ToString();
}

std::wstring ComboBox::GetItemDisplayText(size_t index) const
{
	const auto source = GetItemsView();
	if (!source) return index < AuthoredItemCount()
		? GetAuthoredItemText(index) : std::wstring{};
	if (index >= source.Get()->Count()) return {};
	BindingSourceReference item;
	if (!source.Get()->TryGetItem(index, item) || !item) return {};
	return GetDisplayMemberText(item);
}

void ComboBox::RefreshItems()
{
	_itemSourceObservations.clear();
	_authoredItemChanges.clear();
	const auto source = GetItemsView();
	if (source)
	{
		_itemSourceObservations.reserve(source.Get()->Count());
		for (size_t index = 0; index < source.Get()->Count(); ++index)
		{
			BindingSourceReference item;
			(void)source.Get()->TryGetItem(index, item);
			_itemSourceObservations.push_back(ObserveItemProjectionPaths(
				item,
				[this, index] { RefreshDataItem(index); }));
		}
	}
	else
	{
		_authoredItemChanges.reserve(AuthoredItemCount());
		for (size_t index = 0; index < AuthoredItemCount(); ++index)
		{
			auto* item = dynamic_cast<ComboBoxItem*>(GetAuthoredItem(index));
			if (!item) continue;
			item->SetItemIndex(index);
			if (cui::framework::StyleAccess::ResourceKey(*item).empty()
				&& !GetItemContainerStyle().empty())
				cui::framework::StyleAccess::SetResourceKey(
					*item, GetItemContainerStyle());
			_authoredItemChanges.push_back(
				item->OnPropertyValueChanged.Subscribe(
					[this, index](DependencyObject*,
						const DependencyPropertyChangedEventArgs& args)
					{
						if (args.Property == &ContentControl::ContentProperty())
							RefreshDataItem(index);
					}));
		}
	}
	ReconcileAccessibilityItemIds();
	SyncTextWithSelection();
	UpdateSelectionBoxState();
	if (_highlightedIndex >= static_cast<int>(ItemCount()))
		_highlightedIndex = -1;
	UpdateGeneratedItemStates();
	_selectedAccessibilityItemId = SelectedIndex >= 0
		&& static_cast<size_t>(SelectedIndex) < _accessibilityItemIds.size()
		? _accessibilityItemIds[static_cast<size_t>(SelectedIndex)] : 0;
	UpdateItemsHostPresentation();
	NotifyAccessibilityStructureChanged();
	NotifyAccessibilityScrollChanged();
	InvalidateVisual();
}

void ComboBox::RefreshDataItem(size_t index)
{
	const auto source = GetItemsView();
	if (source)
	{
		if (index >= source.Get()->Count()
			|| index >= _itemSourceObservations.size())
		{
			RefreshItems();
			return;
		}
		BindingSourceReference item;
		(void)source.Get()->TryGetItem(index, item);
		_itemSourceObservations[index] = ObserveItemProjectionPaths(
			item,
			[this, index] { RefreshDataItem(index); });
	}
	if (static_cast<int>(index) == SelectedIndex)
	{
		SyncTextWithSelection();
		UpdateSelectionBoxState();
	}
	if (index < _accessibilityItemIds.size())
		NotifyAccessibilityVirtualChanged(
			_accessibilityItemIds[index], AccessibilityChange::Name);
	InvalidateVisual();
}

void ComboBox::SyncTextWithSelection()
{
	if (_preserveTextDuringSelection) return;
	const auto value = SelectedIndex >= 0
		&& static_cast<size_t>(SelectedIndex) < ItemCount()
		? GetItemDisplayText(static_cast<size_t>(SelectedIndex)) : std::wstring{};
	// Selection synchronization is framework behavior, not a new author Local
	// value. Preserve an existing Binding expression like WPF SetCurrentValue.
	_updatingTextFromSelection = true;
	(void)TrySetCurrentPropertyValue(TextProperty(), BindingValue(value));
	_updatingTextFromSelection = false;
	SyncEditableTextBox();
}

void ComboBox::SyncEditableTextBox()
{
	if (!_editableTextBox || _updatingEditableTextBox) return;
	_updatingEditableTextBox = true;
	(void)_editableTextBox->TrySetCurrentPropertyValue(
		TextBox::TextProperty(), BindingValue(Text));
	(void)_editableTextBox->TrySetCurrentPropertyValue(
		TextBoxBase::IsReadOnlyProperty(), BindingValue(_isReadOnly));
	_updatingEditableTextBox = false;
}

void ComboBox::UpdateSelectionBoxState()
{
	BindingValue item(std::wstring{});
	ItemTemplateReference itemTemplate;
	if (SelectedIndex >= 0
		&& static_cast<size_t>(SelectedIndex) < ItemCount())
	{
		const auto source = GetItemsView();
		if (source)
		{
			BindingSourceReference selected;
			if (source.Get()->TryGetItem(
				static_cast<size_t>(SelectedIndex), selected))
				item = BindingValue(selected);
			itemTemplate = GetItemTemplate();
		}
		else if (auto* authored = dynamic_cast<ComboBoxItem*>(
			GetAuthoredItem(static_cast<size_t>(SelectedIndex))))
		{
			item = authored->GetContent();
			itemTemplate = authored->GetContentTemplate();
		}
	}
	(void)SetReadOnlyPropertyField(
		SelectionBoxItemPropertyKey(),
		_selectionBoxItem, item);
	(void)SetReadOnlyPropertyField(
		SelectionBoxItemTemplatePropertyKey(),
		_selectionBoxItemTemplate, itemTemplate);
	UpdateSelectionBoxHighlightState();
}

void ComboBox::UpdateSelectionBoxHighlightState()
{
	bool focusWithin = false;
	if (auto* window = GetPresentationWindow())
		for (auto* current = window->GetKeyboardFocusedElement();
			current; current = current->GetRoutedParent())
		{
			if (current == this)
			{
				focusWithin = true;
				break;
			}
		}
	const bool highlighted = focusWithin && !_isDropDownOpen;
	(void)SetReadOnlyPropertyField(
		IsSelectionBoxHighlightedPropertyKey(),
		_isSelectionBoxHighlighted, highlighted);
}

void ComboBox::UpdateGeneratedItemStates()
{
	UpdateContainerSelection();
	for (size_t index = 0; index < GeneratedItemCount(); ++index)
		if (auto* item = GetGeneratedItem(index))
			item->SetIsHighlighted(
				static_cast<int>(index) == _highlightedIndex);
}

void ComboBox::OnSelectedIndexChanged(int oldValue, int newValue)
{
	if (oldValue == newValue) return;
	SyncTextWithSelection();
	UpdateSelectionBoxState();
	if (_accessibilityItemIds.size() != ItemCount())
		ReconcileAccessibilityItemIds();
	_selectedAccessibilityItemId = newValue >= 0
		&& static_cast<size_t>(newValue) < _accessibilityItemIds.size()
		? _accessibilityItemIds[static_cast<size_t>(newValue)] : 0;
	UpdateGeneratedItemStates();
	if (_isDropDownOpen && newValue >= 0)
		(void)BringItemIntoView(static_cast<size_t>(newValue));
}

bool ComboBox::SelectItem(int index)
{
	if (index < 0 || static_cast<size_t>(index) >= ItemCount()) return false;
	if (SelectedIndex == index)
	{
		if (_isDropDownOpen)
			(void)BringItemIntoView(static_cast<size_t>(index));
		return true;
	}
	return SelectIndex(index);
}

void ComboBox::SetHighlightedIndex(int value, bool focusItem)
{
	if (value < 0 || static_cast<size_t>(value) >= ItemCount())
		value = -1;
	if (_highlightedIndex == value)
	{
		if (focusItem && value >= 0)
			if (auto* item = GetGeneratedItem(
				static_cast<size_t>(value)))
				(void)item->Focus();
		return;
	}
	_highlightedIndex = value;
	UpdateGeneratedItemStates();
	if (value >= 0)
	{
		(void)BringItemIntoView(static_cast<size_t>(value));
		if (focusItem)
			if (auto* item = GetGeneratedItem(
				static_cast<size_t>(value)))
				(void)item->Focus();
	}
}

void ComboBox::CommitHighlightedSelection()
{
	if (_highlightedIndex >= 0)
		(void)SelectItem(_highlightedIndex);
	CloseDropDown(true);
}

void ComboBox::CloseDropDown(bool commitSelection)
{
	if (commitSelection && _highlightedIndex >= 0)
		(void)SelectItem(_highlightedIndex);
	else if (!commitSelection && _selectionBeforeDropDown >= -1)
		(void)SelectIndex(_selectionBeforeDropDown);
	SetCurrentIsDropDownOpen(false);
	_selectionBeforeDropDown = -1;
}

int ComboBox::FindItemByTextPrefix(
	const std::wstring& text, bool exact) const
{
	if (text.empty()) return -1;
	const bool caseSensitive = GetIsTextSearchCaseSensitive();
	for (size_t index = 0; index < ItemCount(); ++index)
	{
		const auto candidate = GetItemDisplayText(index);
		if (exact)
		{
			if (candidate.size() != text.size()) continue;
		}
		if (StartsWith(candidate, text, caseSensitive))
			return static_cast<int>(index);
	}
	return -1;
}

void ComboBox::NotifyItemHighlighted(size_t index)
{
	if (!_isDropDownOpen || index >= ItemCount()) return;
	SetHighlightedIndex(static_cast<int>(index), false);
}

std::wstring ComboBox::GetTextSearchItemText(size_t index) const
{
	return GetItemDisplayText(index);
}

void ComboBox::OnTextSearchMatch(size_t index)
{
	if (index >= ItemCount()) return;
	SetHighlightedIndex(static_cast<int>(index), false);
	if (!_isDropDownOpen)
		(void)SelectItem(static_cast<int>(index));
}

bool ComboBox::ProcessItemKey(
	size_t itemIndex, const InputReport& input)
{
	if (input.Kind != InputReportKind::KeyDown) return false;
	if (itemIndex < ItemCount())
		SetHighlightedIndex(static_cast<int>(itemIndex), false);
	switch (input.Key)
	{
	case Key::Return:
	case Key::Space:
		CommitHighlightedSelection();
		return true;
	case Key::Escape:
		CloseDropDown(false);
		return true;
	case Key::Up:
		SetHighlightedIndex(
			(std::max)(0, _highlightedIndex - 1), true);
		return true;
	case Key::Down:
		SetHighlightedIndex(
			(std::min)(
				static_cast<int>(ItemCount()) - 1,
				_highlightedIndex + 1), true);
		return true;
	case Key::Home:
		SetHighlightedIndex(0, true);
		return true;
	case Key::End:
		SetHighlightedIndex(
			static_cast<int>(ItemCount()) - 1, true);
		return true;
	default:
		return false;
	}
}

ComboBoxItem* ComboBox::AddItem(std::unique_ptr<ComboBoxItem> item)
{
	return static_cast<ComboBoxItem*>(AddItemControl(std::move(item)));
}

ComboBoxItem* ComboBox::InsertItem(
	int index, std::unique_ptr<ComboBoxItem> item)
{
	if (index < 0 || static_cast<size_t>(index) > AuthoredItemCount())
		throw std::out_of_range("ComboBox item index is out of range");
	return static_cast<ComboBoxItem*>(InsertItemControl(
		static_cast<size_t>(index), std::move(item)));
}

ComboBoxItem* ComboBox::GetItem(int index) const noexcept
{
	return index < 0 ? nullptr : dynamic_cast<ComboBoxItem*>(
		GetAuthoredItem(static_cast<size_t>(index)));
}

int ComboBox::IndexOfItem(const ComboBoxItem* item) const noexcept
{
	if (!item) return -1;
	for (size_t index = 0; index < AuthoredItemCount(); ++index)
		if (GetAuthoredItem(index) == item) return static_cast<int>(index);
	return -1;
}

std::unique_ptr<ComboBoxItem> ComboBox::DetachItemAt(int index)
{
	if (index < 0) return {};
	auto owner = DetachItemControlAt(static_cast<size_t>(index));
	if (!owner) return {};
	auto* item = static_cast<ComboBoxItem*>(owner.release());
	return std::unique_ptr<ComboBoxItem>(item);
}

std::unique_ptr<ComboBoxItem> ComboBox::DetachItem(ComboBoxItem* item)
{
	return DetachItemAt(IndexOfItem(item));
}

bool ComboBox::RemoveItemAt(int index)
{
	return static_cast<bool>(DetachItemAt(index));
}

bool ComboBox::RemoveItem(ComboBoxItem* item)
{
	return static_cast<bool>(DetachItem(item));
}

void ComboBox::ClearItems()
{
	ClearItemControls();
}

bool ComboBox::HandlesNavigationKey(Key key) const
{
	switch (key)
	{
	case Key::Return:
	case Key::Space:
	case Key::Escape:
	case Key::F4:
		return true;
	default:
		return Selector::HandlesNavigationKey(key);
	}
}

cui::core::Size ComboBox::MeasureCore(
	const cui::core::Constraints& available)
{
	if (GetControlTemplateRoot()
		&& GetControlTemplateRoot() != _defaultPopup)
		return ItemsControl::MeasureCore(available);
	const auto padding = GetSpecifiedLayout().padding;
	cui::core::Size textSize{};
	if (GetRenderFont())
	{
		const auto measured = GetRenderFont()->GetTextSize(Text);
		textSize = { measured.width, measured.height };
	}
	return available.Constrain({
		textSize.width + padding.Horizontal() + 28.0f,
		(std::max)(24.0f, textSize.height + padding.Vertical()) });
}

void ComboBox::Arrange(cui::core::Rect finalRect)
{
	if (GetControlTemplateRoot()
		&& GetControlTemplateRoot() != _defaultPopup)
		ItemsControl::Arrange(finalRect);
	else
		Control::Arrange(finalRect);
	if (_isDropDownOpen && EnsureDropDownInfrastructure() && _popup)
		_popup->UpdatePlacement();
}

void ComboBox::PreparePresentation()
{
	Selector::PreparePresentation();
	if (_isDropDownOpen) (void)EnsureDropDownInfrastructure();
	else ConfigurePopupPart(ResolvePopupPart());
	_dropDownScroll = ResolveScrollOwner();
	UpdateItemsHostPresentation();
	if (_popup && _popup->GetIsOpen()) _popup->UpdatePlacement();
	// Focus containment can change between retained frames.  The selected-item
	// projection itself is refreshed by selection, item, template, and window
	// changes, matching WPF's UpdateSelectionBoxItem lifecycle.
	UpdateSelectionBoxHighlightState();
}

bool ComboBox::ProcessInput(const InputReport& input)
{
	if (!IsEffectivelyEnabled() || !IsVisible) return true;
	if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Left)
	{
		_pointerPressActive = true;
		(void)CaptureMouse();
		if (GetPresentationWindow())
			GetPresentationWindow()->SetKeyboardFocus(this, true);
	}

	if (input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Left)
	{
		const bool activate = _pointerPressActive
			&& ContainsPoint(input.X, input.Y);
		_pointerPressActive = false;
		if (IsMouseCaptured()) (void)ReleaseMouseCapture();
		if (activate) SetCurrentIsDropDownOpen(!_isDropDownOpen);
		return Selector::ProcessInput(input);
	}
	if (input.Kind == InputReportKind::Cancel
		|| input.Kind == InputReportKind::CaptureLost)
	{
		_pointerPressActive = false;
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		return Selector::ProcessInput(input);
	}

	if (input.Kind == InputReportKind::KeyDown)
	{
		bool handled = true;
		switch (input.Key)
		{
		case Key::F4:
			SetCurrentIsDropDownOpen(!_isDropDownOpen);
			break;
		case Key::Return:
			if (_isDropDownOpen) CommitHighlightedSelection();
			else SetCurrentIsDropDownOpen(true);
			break;
		case Key::Space:
			if (_isEditable) handled = false;
			else if (_isDropDownOpen) CommitHighlightedSelection();
			else SetCurrentIsDropDownOpen(true);
			break;
		case Key::Escape:
			if (_isDropDownOpen) CloseDropDown(false);
			else handled = false;
			break;
		case Key::Down:
			if (input.HasModifier(ModifierKeys::Alt))
				SetCurrentIsDropDownOpen(true);
			else if (_isDropDownOpen)
				SetHighlightedIndex(
					(std::min)(
						static_cast<int>(ItemCount()) - 1,
						_highlightedIndex + 1), false);
			else handled = false;
			break;
		case Key::Up:
			if (input.HasModifier(ModifierKeys::Alt))
				SetCurrentIsDropDownOpen(false);
			else if (_isDropDownOpen)
				SetHighlightedIndex(
					(std::max)(0, _highlightedIndex - 1), false);
			else handled = false;
			break;
		default:
			handled = false;
			break;
		}
		if (handled)
		{
			auto args = input.CreateKeyEventArgs();
			OnKeyDown(this, args);
			return true;
		}
	}
	return Selector::ProcessInput(input);
}

void ComboBox::ReconcileAccessibilityItemIds()
{
	const size_t count = ItemCount();
	std::vector<uint32_t> nextIds(count, 0);
	std::vector<BindingSourceReference> nextSources(count);
	std::vector<ControlWeakReference> nextAuthored(count);
	const auto source = GetItemsView();
	struct ReusableSourceIds final
	{
		std::vector<size_t> Indices;
		size_t Next = 0;
	};
	std::unordered_map<IBindingSource*, ReusableSourceIds> reusableSourceIds;
	std::vector<bool> usedAuthoredIds;
	if (source)
	{
		reusableSourceIds.reserve(_accessibilitySourceIdentities.size());
		for (size_t old = 0;
			old < _accessibilityItemIds.size()
				&& old < _accessibilitySourceIdentities.size(); ++old)
		{
			reusableSourceIds[_accessibilitySourceIdentities[old].Get()]
				.Indices.push_back(old);
		}
	}
	else
	{
		usedAuthoredIds.resize(_accessibilityItemIds.size(), false);
	}
	for (size_t index = 0; index < count; ++index)
	{
		if (source)
		{
			(void)source.Get()->TryGetItem(index, nextSources[index]);
			const auto reusable = reusableSourceIds.find(
				nextSources[index].Get());
			if (reusable != reusableSourceIds.end()
				&& reusable->second.Next < reusable->second.Indices.size())
			{
				nextIds[index] = _accessibilityItemIds[
					reusable->second.Indices[reusable->second.Next++]];
			}
		}
		else
		{
			nextAuthored[index] = GetAuthoredItem(index);
			for (size_t old = 0; old < _accessibilityItemIds.size(); ++old)
			{
				if (usedAuthoredIds[old]
					|| old >= _accessibilityAuthoredIdentities.size()
					|| _accessibilityAuthoredIdentities[old]
						!= nextAuthored[index]) continue;
				nextIds[index] = _accessibilityItemIds[old];
				usedAuthoredIds[old] = true;
				break;
			}
		}
		if (nextIds[index] == 0)
			nextIds[index] = AllocateAccessibilityVirtualId();
	}
	_accessibilityItemIds = std::move(nextIds);
	_accessibilitySourceIdentities = std::move(nextSources);
	_accessibilityAuthoredIdentities = std::move(nextAuthored);
	RebuildAccessibilityItemIndex();
}

void ComboBox::RebuildAccessibilityItemIndex()
{
	_accessibilityItemIndexById.clear();
	for (size_t index = 0; index < _accessibilityItemIds.size(); ++index)
	{
		auto& id = _accessibilityItemIds[index];
		while (id == 0
			|| !_accessibilityItemIndexById.emplace(id, index).second)
			id = AllocateAccessibilityVirtualId();
	}
}

int ComboBox::FindAccessibilityItem(uint32_t id)
{
	if (id == 0) return -1;
	if (_accessibilityItemIds.size() != ItemCount()
		|| _accessibilityItemIndexById.size() != _accessibilityItemIds.size())
		ReconcileAccessibilityItemIds();
	const auto found = _accessibilityItemIndexById.find(id);
	return found == _accessibilityItemIndexById.end()
		? -1 : static_cast<int>(found->second);
}

bool ComboBox::TryGetItemBounds(
	size_t index, D2D1_RECT_F& bounds, bool& visible) const noexcept
{
	bounds = D2D1::RectF();
	visible = false;
	if (!_popup || !_popup->GetIsOpen()) return false;
	auto* item = GetGeneratedItem(index);
	if (!item) return true;
	bounds = item->GetRenderedAbsoluteRectDip();
	const auto popupBounds = _popup->GetRenderedAbsoluteRectDip();
	visible = item->IsVisible && Intersects(bounds, popupBounds);
	if (!visible) bounds = D2D1::RectF();
	return true;
}

size_t ComboBox::GetAccessibilityVirtualChildCount(uint32_t parentId)
{
	return parentId == 0 ? ItemCount() : 0;
}

bool ComboBox::TryGetAccessibilityVirtualChildAt(
	uint32_t parentId, size_t index, uint32_t& result)
{
	result = 0;
	if (parentId != 0 || index >= ItemCount()) return false;
	if (_accessibilityItemIds.size() != ItemCount())
		ReconcileAccessibilityItemIds();
	result = _accessibilityItemIds[index];
	return result != 0;
}

bool ComboBox::TryGetAccessibilityVirtualSibling(
	uint32_t parentId, uint32_t id, bool next, uint32_t& result)
{
	result = 0;
	if (parentId != 0) return false;
	const int index = FindAccessibilityItem(id);
	if (index < 0) return false;
	const int sibling = next ? index + 1 : index - 1;
	if (sibling < 0
		|| sibling >= static_cast<int>(_accessibilityItemIds.size()))
		return false;
	result = _accessibilityItemIds[static_cast<size_t>(sibling)];
	return result != 0;
}

bool ComboBox::TryHitTestAccessibilityVirtualNode(
	float localX, float localY, uint32_t& result)
{
	result = 0;
	if (!_popup || !_popup->GetIsOpen()) return false;
	const auto rawTransform = GetLocalToRenderTransform();
	const auto renderPoint = D2D1::Matrix3x2F(
		rawTransform._11, rawTransform._12,
		rawTransform._21, rawTransform._22,
		rawTransform._31, rawTransform._32)
		.TransformPoint(D2D1::Point2F(localX, localY));
	const size_t count = ItemCount();
	for (size_t index = 0; index < count; ++index)
	{
		D2D1_RECT_F bounds{};
		bool visible = false;
		if (!TryGetItemBounds(index, bounds, visible) || !visible) continue;
		if (renderPoint.x < bounds.left || renderPoint.x > bounds.right
			|| renderPoint.y < bounds.top || renderPoint.y > bounds.bottom) continue;
		return TryGetAccessibilityVirtualChildAt(0, index, result);
	}
	return false;
}

bool ComboBox::TryGetAccessibilityVirtualNode(
	uint32_t id, AccessibilityVirtualNode& result)
{
	const int index = FindAccessibilityItem(id);
	if (index < 0) return false;
	D2D1_RECT_F bounds{};
	bool visible = false;
	(void)TryGetItemBounds(static_cast<size_t>(index), bounds, visible);
	result = {};
	result.Id = id;
	result.ControlType = AutomationControlType::ListItem;
	result.Patterns = AutomationPattern::SelectionItem
		| AutomationPattern::ScrollItem
		| AutomationPattern::VirtualizedItem;
	result.Name = GetItemDisplayText(static_cast<size_t>(index));
	result.Value = result.Name;
	const auto ownerId = GetAccessibilitySnapshot().AutomationId;
	result.AutomationId = ownerId.empty()
		? L"item-" + std::to_wstring(id)
		: ownerId + L".item-" + std::to_wstring(id);
	result.BoundsDip = bounds;
	result.BoundsAreRenderSpace = true;
	result.Enabled = IsEffectivelyEnabled();
	result.Visible = IsVisible && visible;
	result.Selected = index == SelectedIndex;
	result.Row = index;
	result.Column = 0;
	return true;
}

AccessibilityVirtualContainerInfo
ComboBox::GetAccessibilityVirtualContainerInfo() const noexcept
{
	AccessibilityVirtualContainerInfo result;
	result.Patterns = AutomationPattern::Selection
		| AutomationPattern::Scroll;
	result.CanSelectMultiple = false;
	result.IsSelectionRequired = ItemCount() != 0;
	result.RowCount = static_cast<int>(ItemCount());
	result.ColumnCount = 1;
	return result;
}

void ComboBox::GetAccessibilityVirtualSelection(
	std::vector<uint32_t>& result)
{
	result.clear();
	if (_accessibilityItemIds.size() != ItemCount())
		ReconcileAccessibilityItemIds();
	if (SelectedIndex >= 0
		&& static_cast<size_t>(SelectedIndex) < _accessibilityItemIds.size())
		result.push_back(
			_accessibilityItemIds[static_cast<size_t>(SelectedIndex)]);
}

bool ComboBox::SelectAccessibilityVirtualNode(
	uint32_t id, AccessibilitySelectionAction action)
{
	if (action == AccessibilitySelectionAction::Remove) return false;
	const int index = FindAccessibilityItem(id);
	const bool selected = IsEffectivelyEnabled()
		&& index >= 0 && SelectItem(index);
	if (selected)
		NotifyAccessibilityVirtualChanged(id, AccessibilityChange::Selection);
	return selected;
}

bool ComboBox::ScrollAccessibilityVirtualNodeIntoView(uint32_t id)
{
	const int index = FindAccessibilityItem(id);
	if (index < 0 || !IsEffectivelyEnabled()) return false;
	SetCurrentIsDropDownOpen(true);
	if (!EnsureDropDownInfrastructure()) return false;
	if (_popup) _popup->UpdatePlacement();
	return BringItemIntoView(static_cast<size_t>(index));
}

bool ComboBox::GetScrollMetrics(
	float& extent, float& viewport, float& offset) const noexcept
{
	extent = viewport = offset = 0.0f;
	if (!_dropDownScroll) return false;
	try
	{
		auto* scroll = const_cast<ScrollViewer*>(_dropDownScroll);
		scroll->UpdateLayout();
		extent = static_cast<float>(scroll->ExtentHeight);
		viewport = static_cast<float>(scroll->ViewportHeight);
		offset = static_cast<float>(scroll->VerticalOffset);
		return extent > viewport && viewport > 0.0f;
	}
	catch (...)
	{
		return false;
	}
}

bool ComboBox::GetAccessibilityScrollInfo(
	AccessibilityScrollInfo& result) const noexcept
{
	result = {};
	float extent = 0.0f;
	float viewport = 0.0f;
	float offset = 0.0f;
	if (!GetScrollMetrics(extent, viewport, offset)) return true;
	const float maximum = (std::max)(0.0f, extent - viewport);
	result.VerticallyScrollable = maximum > 0.0f;
	if (result.VerticallyScrollable)
	{
		result.VerticalScrollPercent = (std::clamp)(
			static_cast<double>(offset / maximum * 100.0f), 0.0, 100.0);
		result.VerticalViewSize = (std::clamp)(
			static_cast<double>(viewport / extent * 100.0f), 0.0, 100.0);
	}
	return true;
}

bool ComboBox::ScrollAccessibility(
	AccessibilityScrollAmount horizontal,
	AccessibilityScrollAmount vertical)
{
	if (horizontal != AccessibilityScrollAmount::NoAmount) return false;
	if (vertical == AccessibilityScrollAmount::NoAmount) return true;
	SetCurrentIsDropDownOpen(true);
	if (!EnsureDropDownInfrastructure() || !_dropDownScroll) return false;
	float extent = 0.0f;
	float viewport = 0.0f;
	float offset = 0.0f;
	if (!GetScrollMetrics(extent, viewport, offset)) return false;
	const int line = 48;
	const int page = (std::max)(line,
		static_cast<int>(std::floor(viewport)) - line);
	int delta = 0;
	switch (vertical)
	{
	case AccessibilityScrollAmount::LargeDecrement: delta = -page; break;
	case AccessibilityScrollAmount::SmallDecrement: delta = -line; break;
	case AccessibilityScrollAmount::LargeIncrement: delta = page; break;
	case AccessibilityScrollAmount::SmallIncrement: delta = line; break;
	case AccessibilityScrollAmount::NoAmount: return true;
	}
	_dropDownScroll->ScrollToVerticalOffset(
		_dropDownScroll->VerticalOffset + static_cast<double>(delta));
	return true;
}

bool ComboBox::SetAccessibilityScrollPercent(
	double horizontalPercent, double verticalPercent)
{
	if (horizontalPercent != AccessibilityScrollNoChange) return false;
	if (verticalPercent == AccessibilityScrollNoChange) return true;
	if (!std::isfinite(verticalPercent)
		|| verticalPercent < 0.0 || verticalPercent > 100.0) return false;
	SetCurrentIsDropDownOpen(true);
	if (!EnsureDropDownInfrastructure() || !_dropDownScroll) return false;
	float extent = 0.0f;
	float viewport = 0.0f;
	float offset = 0.0f;
	if (!GetScrollMetrics(extent, viewport, offset)) return false;
	const float maximum = (std::max)(0.0f, extent - viewport);
	_dropDownScroll->ScrollToVerticalOffset(maximum
		* static_cast<float>(verticalPercent / 100.0));
	return true;
}
