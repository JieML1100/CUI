#include "ItemContainer.h"
#include "EventInfrastructure.h"
#include "InputManager.h"
#include "ItemsControl.h"

#include <utility>
#include <vector>

void ItemContainerControl::EnsureClassHandlers()
{
	static const std::vector<EventConnection> handlers = []
	{
		std::vector<EventConnection> result;
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_ListBoxItem, RoutedEventId::MouseDown,
			&ItemContainerControl::HandleDescendantPointerPress));
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_ListBoxItem, RoutedEventId::MouseDoubleClick,
			&ItemContainerControl::HandleDescendantPointerPress));
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_ListBoxItem, RoutedEventId::MouseUp,
			&ItemContainerControl::HandleDescendantPointerRelease));
		return result;
	}();
	(void)handlers;
}

void ItemContainerControl::HandleDescendantPointerPress(
	Control* sender,
	RoutedEventArgs& args)
{
	auto* container = dynamic_cast<ItemContainerControl*>(sender);
	if (!container || args.OriginalSource == container
		|| !container->IsEffectivelyEnabled()
		|| !container->IsVisible) return;
	auto& mouse = static_cast<MouseEventArgs&>(args);
	container->BeginPointerPress(mouse);
}

void ItemContainerControl::HandleDescendantPointerRelease(
	Control* sender,
	RoutedEventArgs& args)
{
	auto* container = dynamic_cast<ItemContainerControl*>(sender);
	if (!container || args.OriginalSource == container) return;
	auto& mouse = static_cast<MouseEventArgs&>(args);
	if (container->CompletePointerPress(mouse))
		args.Handled = true;
}

ItemContainerControl::ItemContainerControl()
	: ContentControl()
{
	EnsureClassHandlers();
#if CUI_ENABLE_DYNAMIC_XAML
	EnsureBindingPropertiesRegistered();
#endif
}

void ItemContainerControl::BeginPointerPress(MouseEventArgs& args)
{
	if (args.ChangedButton != MouseButton::Left
		|| !IsEffectivelyEnabled() || !IsVisible) return;
	_pointerPressActive = true;
	const ControlWeakReference lifetime(this);
	if (!ActivatesOnPointerUp())
	{
		ActivateItem(args.ChangedButton, args.Modifiers);
		auto* live = dynamic_cast<ItemContainerControl*>(lifetime.Get());
		if (!live) return;
		live->FocusOwner();
	}
	if (auto* live = dynamic_cast<ItemContainerControl*>(lifetime.Get()))
		(void)live->CaptureMouse();
}

bool ItemContainerControl::CompletePointerPress(MouseEventArgs& args)
{
	if (args.ChangedButton != MouseButton::Left
		|| !_pointerPressActive) return false;
	_pointerPressActive = false;
	if (IsMouseCaptured()) (void)ReleaseMouseCapture();
	if (!IsEffectivelyEnabled() || !IsVisible) return true;
	if (ActivatesOnPointerUp() && ContainsPoint(args.X, args.Y))
	{
		const ControlWeakReference lifetime(this);
		ActivateItem(args.ChangedButton, args.Modifiers);
		auto* live = dynamic_cast<ItemContainerControl*>(lifetime.Get());
		if (live) live->FocusOwner();
	}
	return true;
}

bool ItemContainerControl::ProcessInput(const InputReport& input)
{
	const ControlWeakReference lifetime(this);
	if ((input.Kind == InputReportKind::PointerDown
		|| input.Kind == InputReportKind::PointerDoubleClick)
		&& input.ChangedButton == MouseButton::Left)
	{
		auto args = input.CreateMouseEventArgs();
		BeginPointerPress(args);
	}
	auto* live = dynamic_cast<ItemContainerControl*>(lifetime.Get());
	if (!live) return true;
	const bool completePointerUpBeforeBase =
		input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Left
		&& live->ActivatesOnPointerUp();
	if (completePointerUpBeforeBase)
	{
		auto args = input.CreateMouseEventArgs();
		(void)live->CompletePointerPress(args);
		live = dynamic_cast<ItemContainerControl*>(lifetime.Get());
		if (!live) return true;
	}
	const bool result = live->ContentControl::ProcessInput(input);
	live = dynamic_cast<ItemContainerControl*>(lifetime.Get());
	if (!live) return result;
	if (input.Kind == InputReportKind::PointerUp
		&& input.ChangedButton == MouseButton::Left
		&& !completePointerUpBeforeBase)
	{
		auto args = input.CreateMouseEventArgs();
		(void)live->CompletePointerPress(args);
	}
	else if (input.Kind == InputReportKind::Cancel
		|| input.Kind == InputReportKind::CaptureLost)
	{
		live->_pointerPressActive = false;
	}
	else if (input.Kind == InputReportKind::KeyDown
		&& input.Key == Key::Back)
	{
		if (auto* owner =
			dynamic_cast<ItemsControl*>(live->GetLogicalParent()))
			owner->ProcessTextSearchKey(input);
	}
	return result;
}

bool ItemContainerControl::ApplyTextInput(
	const TextCompositionEventArgs& input)
{
	if (auto* owner =
		dynamic_cast<ItemsControl*>(GetLogicalParent()))
		if (owner->ProcessTextSearchInput(input))
			return true;
	return ContentControl::ApplyTextInput(input);
}

bool ItemContainerControl::InitializeItem(
	const BindingSourceReference& item,
	const ItemTemplateReference& contentTemplate,
	CompiledBindingPathView displayMemberPath,
	size_t index,
	const std::wstring& publicTypeName,
	std::wstring* outError)
{
	if (!item)
	{
		if (outError) *outError = publicTypeName + L" 缺少数据项。";
		return false;
	}
	_index = index;
	SetContentTypeToken(contentTemplate
		? contentTemplate.Get()->GetDataTypeToken() : DataTypeToken{});
	SetCompiledDisplayMemberPath(displayMemberPath);
	SetContentTemplate(contentTemplate);
	SetContent(BindingValue(item));
	if (!LastContentError().empty())
	{
		if (outError) *outError = LastContentError();
		return false;
	}
	if (outError) outError->clear();
	return true;
}

const DependencyProperty& ItemContainerControl::IsSelectedProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<ItemContainerControl, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = true;
		options.Design.Category = L"State";
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			ItemContainerControl, bool>(
				DependencyPropertyRegistrationLiteral(L"IsSelected"),
				[](ItemContainerControl& target)
				{ return target.GetIsSelected(); },
				[](ItemContainerControl& target, const bool& value)
				{ target.ApplyIsSelectedValue(value); },
				[](ItemContainerControl& target,
					DependencyPropertyMetadata::ChangeHandler handler,
					DataSourceUpdateMode)
				{
					return target._selectedChanged.Subscribe(
						[handler = std::move(handler)](ItemContainerControl*)
						{ handler(); });
				}, std::move(options));
	}();
	return *registration;
}

#if !CUI_ENABLE_DYNAMIC_XAML
void ItemContainerControl::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
}
#endif

void ItemContainerControl::SetIsSelected(bool value)
{
	if (!SetPropertyField(IsSelectedProperty(), _selected, value)) return;
	OnIsSelectedRequested(_selected);
}

void ItemContainerControl::SetCurrentIsSelected(bool value)
{
	(void)SetCurrentPropertyField(IsSelectedProperty(), _selected, value);
}

void ItemContainerControl::ApplyIsSelectedValue(bool value)
{
	if (_selected == value) return;
	if (!SetPropertyField(IsSelectedProperty(), _selected, value)) return;
	SetStyleState(ControlStyleState::Selected, value);
	cui::framework::EventAccess::Raise(_selectedChanged, this);
	RoutedEventArgs args;
	if (value) Selected(this, args);
	else Unselected(this, args);
}
