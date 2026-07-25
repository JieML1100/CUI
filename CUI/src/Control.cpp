#include "Control.h"
#include "EventInfrastructure.h"
#include "Binding.h"
#include "Window.h"
#include "WindowInfrastructure.h"
#include "Panel.h"
#include "PropertyPath.h"
#include "Style.h"
#include "Core/Threading.h"
#include "InputManager.h"
#include "XamlInfrastructure.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <cwctype>
#include <atomic>
#include <unordered_set>
#include <type_traits>
#include <variant>

#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)

namespace
{
	std::atomic<uint32_t> NextAccessibilityRuntimeId{ 1 };
	std::atomic<uint32_t> NextAccessibilityVirtualRuntimeId{ 1 };
	std::vector<std::pair<ControlWeakReference, bool>>
		CaptureEffectiveIsEnabledSubtree(Control& root);
	std::vector<std::pair<ControlWeakReference, bool>>
		CaptureEffectiveIsVisibleSubtree(Control& root);

	uint32_t AllocateAccessibilityRuntimeId() noexcept
	{
		uint32_t value = NextAccessibilityRuntimeId.fetch_add(
			1, std::memory_order_relaxed);
		if (value == 0)
			value = NextAccessibilityRuntimeId.fetch_add(
				1, std::memory_order_relaxed);
		return value;
	}

	D2D1::Matrix3x2F AsMatrix(const D2D1_MATRIX_3X2_F& value) noexcept
	{
		return D2D1::Matrix3x2F(
			value._11, value._12, value._21,
			value._22, value._31, value._32);
	}

	bool IsFiniteMatrix(const D2D1_MATRIX_3X2_F& value) noexcept
	{
		return std::isfinite(value._11) && std::isfinite(value._12)
			&& std::isfinite(value._21) && std::isfinite(value._22)
			&& std::isfinite(value._31) && std::isfinite(value._32);
	}

	int StoredPropertySourceIndex(DependencyPropertyValueSource source) noexcept
	{
		const int value = static_cast<int>(source);
		return value >= static_cast<int>(DependencyPropertyValueSource::Inherited)
			&& value <= static_cast<int>(DependencyPropertyValueSource::Animation)
			? value - static_cast<int>(DependencyPropertyValueSource::Inherited)
			: -1;
	}

	cui::layout::Alignment ToLayoutAlignment(HorizontalAlignment value)
	{
		switch (value)
		{
		case HorizontalAlignment::Center: return cui::layout::Alignment::Center;
		case HorizontalAlignment::Right: return cui::layout::Alignment::End;
		case HorizontalAlignment::Stretch: return cui::layout::Alignment::Stretch;
		case HorizontalAlignment::Left:
		default: return cui::layout::Alignment::Start;
		}
	}

	cui::layout::Alignment ToLayoutAlignment(VerticalAlignment value)
	{
		switch (value)
		{
		case VerticalAlignment::Center: return cui::layout::Alignment::Center;
		case VerticalAlignment::Bottom: return cui::layout::Alignment::End;
		case VerticalAlignment::Stretch: return cui::layout::Alignment::Stretch;
		case VerticalAlignment::Top:
		default: return cui::layout::Alignment::Start;
		}
	}

	cui::core::Dip ToMaximumDip(LONG value)
	{
		return value >= INT_MAX
			? cui::core::Infinity
			: (std::max)(0.0f, (float)value);
	}

	LONG ToLayoutLong(cui::core::Dip value)
	{
		if (!(value > 0.0f)) return 0;
		const auto maximum = (cui::core::Dip)(std::numeric_limits<LONG>::max)();
		return value >= maximum ? (std::numeric_limits<LONG>::max)() : (LONG)value;
	}

	LONG ToMeasureLong(cui::core::Dip value)
	{
		if (!(value > 0.0f)) return 0;
		const auto maximum = (cui::core::Dip)(std::numeric_limits<LONG>::max)();
		return value >= maximum
			? (std::numeric_limits<LONG>::max)()
			: static_cast<LONG>(std::ceil(value));
	}

	LONG ToCoordinateLong(cui::core::Dip value)
	{
		if (value != value) return 0;
		const auto maximum = (cui::core::Dip)(std::numeric_limits<LONG>::max)();
		const auto minimum = (cui::core::Dip)(std::numeric_limits<LONG>::min)();
		if (value >= maximum) return (std::numeric_limits<LONG>::max)();
		if (value <= minimum) return (std::numeric_limits<LONG>::min)();
		return (LONG)value;
	}

	std::optional<cui::layout::Length> ConvertLayoutLength(
		const BindingValue& value)
	{
		cui::layout::Length exact;
		if (value.TryGet(exact))
		{
			if (exact.IsAuto()) return cui::layout::Length::Auto();
			if (!exact.IsFixed() || !std::isfinite(exact.value)
				|| exact.value < 0.0f) return std::nullopt;
			return cui::layout::Length::Fixed(exact.value);
		}

		BindingValue numericValue = value;
		std::wstring text;
		if (value.Kind() == BindingValueKind::String
			&& value.TryGetString(text))
		{
			const auto first = std::find_if_not(
				text.begin(), text.end(), [](wchar_t ch) { return std::iswspace(ch); });
			const auto last = std::find_if_not(
				text.rbegin(), text.rend(), [](wchar_t ch) { return std::iswspace(ch); }).base();
			text = first < last ? std::wstring(first, last) : std::wstring{};
			if (_wcsicmp(text.c_str(), L"Auto") == 0)
				return cui::layout::Length::Auto();
			numericValue = BindingValue(std::move(text));
		}

		float numeric = 0.0f;
		if (!numericValue.TryGetFloat(numeric)
			|| !std::isfinite(numeric) || numeric < 0.0f)
			return std::nullopt;
		return cui::layout::Length::Fixed(numeric);
	}

	cui::core::Rect ToCoreRect(D2D1_RECT_F value)
	{
		return cui::core::Rect::FromLTRB(
			value.left, value.top, value.right, value.bottom);
	}

	D2D1_RECT_F ToD2DRect(cui::core::Rect value)
	{
		return D2D1_RECT_F{
			value.Left(), value.Top(), value.Right(), value.Bottom() };
	}

	int ValidationSeverityRank(BindingValidationSeverity severity) noexcept
	{
		switch (severity)
		{
		case BindingValidationSeverity::Info: return 0;
		case BindingValidationSeverity::Warning: return 1;
		case BindingValidationSeverity::Error: return 2;
		}
		return 0;
	}

	D2D1_COLOR_F DefaultValidationColor(
		BindingValidationSeverity severity) noexcept
	{
		switch (severity)
		{
		case BindingValidationSeverity::Info:
			return D2D1_COLOR_F{ 0.12f, 0.52f, 0.88f, 1.0f };
		case BindingValidationSeverity::Warning:
			return D2D1_COLOR_F{ 0.95f, 0.62f, 0.12f, 1.0f };
		case BindingValidationSeverity::Error:
		default:
			return D2D1_COLOR_F{ 0.90f, 0.20f, 0.24f, 1.0f };
		}
	}

	DependencyPropertyDesignMetadata PropertyDesign(
		std::wstring category,
		int categoryOrder,
		int order,
		DependencyPropertyPersistence persistence,
		DependencyPropertyEditorKind editor = DependencyPropertyEditorKind::Auto,
		std::wstring displayName = {})
	{
		DependencyPropertyDesignMetadata design;
		design.DisplayName = std::move(displayName);
		design.Category = std::move(category);
		design.CategoryOrder = categoryOrder;
		design.Order = order;
		design.Editor = editor;
		design.Persistence = persistence;
		return design;
	}

	template<typename TOwner, typename TValue>
	DependencyPropertyOptions<TOwner, TValue> WithPropertyDesign(
		DependencyPropertyOptions<TOwner, TValue> options,
		DependencyPropertyDesignMetadata design)
	{
		options.Design = std::move(design);
		return options;
	}

	template<typename TValue>
	DependencyPropertyChoice PropertyChoice(std::wstring displayName, TValue value)
	{
		return { std::move(displayName), BindingValue(std::move(value)) };
	}

	std::wstring StripAccessKeyMarkers(const std::wstring& text)
	{
		std::wstring result;
		result.reserve(text.size());
		for (size_t index = 0; index < text.size(); ++index)
		{
			if (text[index] != L'_')
			{
				result.push_back(text[index]);
				continue;
			}
			if (index + 1 >= text.size())
			{
				result.push_back(L'_');
				continue;
			}
			if (text[index + 1] == L'_')
			{
				result.push_back(L'_');
				++index;
			}
		}
		return result;
	}

	wchar_t FindAccessKeyMarker(const std::wstring& text)
	{
		for (size_t index = 0; index + 1 < text.size(); ++index)
		{
			if (text[index] != L'_') continue;
			if (text[index + 1] == L'_')
			{
				++index;
				continue;
			}
			if (!std::iswspace(text[index + 1]))
				return static_cast<wchar_t>(std::towupper(text[index + 1]));
		}
		return L'\0';
	}
}

uint32_t AllocateAccessibilityVirtualId() noexcept
{
	uint32_t value = NextAccessibilityVirtualRuntimeId.fetch_add(
		1, std::memory_order_relaxed);
	if (value == 0)
		value = NextAccessibilityVirtualRuntimeId.fetch_add(
			1, std::memory_order_relaxed);
	return value;
}

Control::Control()
	: _text(L"")
{
	_visualChildren.SetOwnerSynchronizationDuringUpdates(true);
	_visualChildren.SetOwnerChangedHandler(
		[this](const CollectionChangedEventArgs& change)
		{ SynchronizeVisualChildCollection(change); });
	this->_accessibilityRuntimeId = AllocateAccessibilityRuntimeId();
	this->_layoutStyle.horizontalAlignment = cui::layout::Alignment::Stretch;
	this->_layoutStyle.verticalAlignment = cui::layout::Alignment::Stretch;
	_styleStateConnections.reserve(2);
	_styleStateConnections.push_back(OnMouseDown.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.OriginalSource == this
				&& (DefaultRaiseClickOnLeftButtonUp() || Focusable))
				SetStyleState(ControlStyleState::Pressed, true);
		}));
	_styleStateConnections.push_back(OnMouseUp.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.OriginalSource == this)
				SetStyleState(ControlStyleState::Pressed, false);
		}));
}

bool IDeclarativeComponentBehavior::SetReadOnlyProperty(
	Control& host,
	const std::wstring& propertyName,
	const BindingValue& value)
{
	if (host.GetDeclarativeComponentBehavior() != this) return false;
	return host.TrySetReadOnlyPropertyValue(propertyName, value);
}

bool IDeclarativeComponentBehavior::ClearReadOnlyProperty(
	Control& host,
	const std::wstring& propertyName)
{
	if (host.GetDeclarativeComponentBehavior() != this) return false;
	return host.ClearReadOnlyPropertyValue(propertyName);
}

std::unique_ptr<AutomationPeer> Control::OnCreateAutomationPeer()
{
	return std::make_unique<AutomationPeer>(
		*this, AutomationControlType::Custom, L"Control");
}

AutomationPeer& Control::GetAutomationPeer() const
{
	if (!_automationPeer)
	{
		auto peer = const_cast<Control*>(this)->OnCreateAutomationPeer();
		if (!peer)
			peer = std::make_unique<AutomationPeer>(
				*const_cast<Control*>(this),
				AutomationControlType::Custom, L"Control");
		_automationPeer = std::move(peer);
	}
	return *_automationPeer;
}

Control::~Control()
{
	// A routed callback may delete this control. Publish death before Control's
	// teardown mutates any tree/event storage so frozen routes cannot re-enter a
	// partially destroyed base object.
	InvalidateLifetimeToken();
	InvalidateCommandInfrastructureForDestruction();
	_isDestroying = true;
	ClearDeclarativeComponentBehavior();
	_declarativeVisualStates.reset();
	_visualChildren.SetOwnerChangedHandler({});
	if (auto* inheritanceParent = GetInheritanceParent())
		inheritanceParent->UnregisterInheritanceChild(this);
	if (_logicalParent)
	{
		auto& siblings = _logicalParent->_logicalChildren;
		siblings.erase(std::remove(siblings.begin(), siblings.end(), this),
			siblings.end());
		_logicalParent = nullptr;
	}
	const auto logicalChildren = _logicalChildren;
	for (auto* child : logicalChildren)
		if (child && child->_logicalParent == this)
			child->SetLogicalParentCore(nullptr);
	const auto inheritanceChildren = _inheritanceChildren;
	for (auto* child : inheritanceChildren)
		if (child && child->_templatedParent == this)
			child->SetTemplatedParentCore(nullptr);
	_logicalChildren.clear();
	_inheritanceChildren.clear();
	_templateNameScope.clear();
	_dataBindings.reset();
	for (auto child : this->_visualChildren)
	{
		if (child && child->_visualParent == this)
			child->_visualParent = nullptr;
		delete child;
	}
	_observedVisualChildren.clear();
}
UIClass GetUIClassBase(UIClass type) noexcept
{
	switch (type)
	{
	case UIClass::UI_FrameworkElement:
		return UIClass::UI_Base;
	case UIClass::UI_Control:
		return UIClass::UI_FrameworkElement;
	case UIClass::UI_Button:
		return UIClass::UI_ButtonBase;
	case UIClass::UI_CheckBox:
	case UIClass::UI_RadioButton:
	case UIClass::UI_Switch:
		return UIClass::UI_ToggleButton;
	case UIClass::UI_ToggleButton:
		return UIClass::UI_ButtonBase;
	case UIClass::UI_ButtonBase:
		return UIClass::UI_ContentControl;
	case UIClass::UI_Slider:
	case UIClass::UI_ProgressBar:
	case UIClass::UI_ProgressRing:
	case UIClass::UI_NumericUpDown:
		return UIClass::UI_RangeBase;
	case UIClass::UI_RangeBase:
		return UIClass::UI_Control;
	case UIClass::UI_ListViewItem:
		return UIClass::UI_ListBoxItem;
	case UIClass::UI_ListBoxItem:
	case UIClass::UI_StatusBarItem:
	case UIClass::UI_Window:
		return UIClass::UI_ContentControl;
	case UIClass::UI_ComboBoxItem:
		return UIClass::UI_ListBoxItem;
	case UIClass::UI_GroupBox:
	case UIClass::UI_Expander:
	case UIClass::UI_TabItem:
		return UIClass::UI_HeaderedContentControl;
	case UIClass::UI_HeaderedContentControl:
		return UIClass::UI_ContentControl;
	case UIClass::UI_ScrollViewer:
		return UIClass::UI_ContentControl;
	case UIClass::UI_Popup:
		return UIClass::UI_FrameworkElement;
	case UIClass::UI_ContentControl:
		return UIClass::UI_Control;
	case UIClass::UI_ContentPresenter:
	case UIClass::UI_ItemsPresenter:
		return UIClass::UI_FrameworkElement;
	case UIClass::UI_Border:
		return UIClass::UI_Decorator;
	case UIClass::UI_Decorator:
		return UIClass::UI_FrameworkElement;
	case UIClass::UI_Grid:
	case UIClass::UI_StackPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_Canvas:
		return UIClass::UI_Panel;
	case UIClass::UI_Panel:
		return UIClass::UI_FrameworkElement;
	case UIClass::UI_ItemsControl:
		return UIClass::UI_Control;
	case UIClass::UI_HeaderedItemsControl:
		return UIClass::UI_ItemsControl;
	case UIClass::UI_StatusBar:
	case UIClass::UI_Menu:
	case UIClass::UI_ContextMenu:
	case UIClass::UI_TreeView:
		return UIClass::UI_ItemsControl;
	case UIClass::UI_ToolBar:
		return UIClass::UI_HeaderedItemsControl;
	case UIClass::UI_MenuItem:
	case UIClass::UI_TreeViewItem:
		return UIClass::UI_HeaderedItemsControl;
	case UIClass::UI_Separator:
		return UIClass::UI_Control;
	case UIClass::UI_Selector:
		return UIClass::UI_ItemsControl;
	case UIClass::UI_ListBox:
	case UIClass::UI_ComboBox:
	case UIClass::UI_TabControl:
		return UIClass::UI_Selector;
	case UIClass::UI_ListView:
		return UIClass::UI_ListBox;
	case UIClass::UI_Label:
	case UIClass::UI_Image:
	case UIClass::UI_WebBrowser:
	case UIClass::UI_NativeSurface:
		return UIClass::UI_FrameworkElement;
	case UIClass::UI_Base:
		return UIClass::UI_Base;
	case UIClass::UI_CUSTOM:
		return UIClass::UI_Control;
	default:
		return UIClass::UI_Control;
	}
}

int GetUIClassInheritanceDistance(
	UIClass baseType,
	UIClass type) noexcept
{
	int distance = 0;
	for (;;)
	{
		if (type == baseType) return distance;
		if (type == UIClass::UI_Base) return -1;
		const auto next = GetUIClassBase(type);
		if (next == type) return -1;
		type = next;
		++distance;
	}
}

bool IsUIClassAssignableFrom(UIClass baseType, UIClass type) noexcept
{
	return GetUIClassInheritanceDistance(baseType, type) >= 0;
}

namespace
{
	bool IsFrameworkElementNativeProperty(std::wstring_view name) noexcept
	{
		static constexpr std::wstring_view properties[] = {
			L"DataContext",
			L"Visibility",
			L"IsVisible",
			L"IsEnabled",
			L"AllowDrop",
			L"Canvas.Left",
			L"Canvas.Top",
			L"Canvas.Right",
			L"Canvas.Bottom",
			L"Width",
			L"Height",
			L"ActualWidth",
			L"ActualHeight",
			L"Margin",
			L"HorizontalAlignment",
			L"VerticalAlignment",
			L"ZIndex",
			L"Grid.Row",
			L"Grid.Column",
			L"Grid.RowSpan",
			L"Grid.ColumnSpan",
			L"DockPanel.Dock",
			L"MinWidth",
			L"MinHeight",
			L"MaxWidth",
			L"MaxHeight",
			L"Clip",
			L"RenderTransform",
			L"RenderTransformOrigin",
			L"Validation.HasError",
			L"Validation.Errors",
			L"Tag",
			L"Cursor",
			L"Focusable",
			L"IsTabStop",
			L"TabIndex",
			L"IsFocused",
			L"IsKeyboardFocused",
			L"IsKeyboardFocusWithin",
			L"IsMouseOver",
			L"IsMouseDirectlyOver",
			L"FocusManager.IsFocusScope",
			L"KeyboardNavigation.TabNavigation",
			L"KeyboardNavigation.DirectionalNavigation",
			L"AutomationProperties.Name",
			L"AutomationProperties.FullDescription",
			L"AutomationProperties.HelpText",
			L"AutomationProperties.AutomationId",
		};
		return std::find(
			std::begin(properties), std::end(properties), name)
			!= std::end(properties);
	}

	bool IsTextBlockNativeProperty(std::wstring_view name) noexcept
	{
		return name == L"Background"
			|| name == L"Foreground"
			|| name == L"FontFamily"
			|| name == L"FontSize"
			|| name == L"Padding";
	}

	bool IsBorderNativeProperty(std::wstring_view name) noexcept
	{
		return name == L"Background"
			|| name == L"BorderBrush"
			|| name == L"BorderThickness"
			|| name == L"Padding";
	}
}

bool IsNativePropertySupportedByUIClass(
	UIClass type,
	const DependencyPropertyMetadata& metadata) noexcept
{
	if (type == UIClass::UI_Base) return false;

	// Metadata registered by a concrete behavior host is an explicit member of
	// that type. Shared base-host metadata is projected through the
	// authoritative WPF/XAML type hierarchy.
	if (metadata.OwnerType() != std::type_index(typeid(Control)))
		return true;

	if (IsUIClassAssignableFrom(UIClass::UI_Control, type))
		return true;
	if (IsFrameworkElementNativeProperty(metadata.Name()))
		return true;
	if (type == UIClass::UI_Label
		&& IsTextBlockNativeProperty(metadata.Name()))
		return true;
	if (IsUIClassAssignableFrom(UIClass::UI_Panel, type)
		&& metadata.Name() == L"Background")
		return true;
	if (type == UIClass::UI_Border
		&& IsBorderNativeProperty(metadata.Name()))
		return true;
	return false;
}

bool IsControlTemplateHostClass(UIClass type) noexcept
{
	return type != UIClass::UI_Window
		&& IsUIClassAssignableFrom(UIClass::UI_Control, type);
}

UIClass GetDefaultItemContainerType(UIClass itemsControlType) noexcept
{
	switch (itemsControlType)
	{
	case UIClass::UI_ListView:
		return UIClass::UI_ListViewItem;
	case UIClass::UI_ComboBox:
		return UIClass::UI_ComboBoxItem;
	case UIClass::UI_TreeView:
	case UIClass::UI_TreeViewItem:
		return UIClass::UI_TreeViewItem;
	case UIClass::UI_Menu:
	case UIClass::UI_MenuItem:
	case UIClass::UI_ContextMenu:
		return UIClass::UI_MenuItem;
	case UIClass::UI_TabControl:
		return UIClass::UI_TabItem;
	case UIClass::UI_StatusBar:
		return UIClass::UI_StatusBarItem;
	case UIClass::UI_ListBox:
	case UIClass::UI_Selector:
		return UIClass::UI_ListBoxItem;
	default:
		return IsUIClassAssignableFrom(
			UIClass::UI_ItemsControl, itemsControlType)
			? UIClass::UI_ContentPresenter : UIClass::UI_Base;
	}
}

UIClass Control::Type() { return UIClass::UI_Control; }

namespace
{
	void ClearGenericTemplateOwner(Control* root, Control* owner)
	{
		if (!root || !owner) return;
		std::vector<Control*> stack{ root };
		while (!stack.empty())
		{
			auto* current = stack.back();
			stack.pop_back();
			if (!current) continue;
			for (auto* child : current->GetVisualChildrenView())
				if (child) stack.push_back(child);
			if (current->GetTemplatedParent() == owner)
				cui::framework::XamlAccess::SetTemplatedParent(*current, nullptr);
		}
	}
}

void Control::ConfigureControlTemplateVisual(Control& child)
{
	(void)child;
}

Control* Control::SetControlTemplateRoot(std::unique_ptr<Control> value)
{
	if (value.get() == _controlTemplateRoot) return _controlTemplateRoot;
	(void)DetachVisualChildTemplateRoot();
	if (!value)
	{
		OnControlTemplatePresentationChanged();
		RequestLayout();
		InvalidateVisual();
		return nullptr;
	}

	ConfigureControlTemplateVisual(*value);
	_controlTemplateRoot = value.get();
	try
	{
		AddOwned(std::move(value));
		_controlTemplateRoot->SetLogicalParent(nullptr);
	}
	catch (...)
	{
		_controlTemplateRoot = nullptr;
		OnControlTemplatePresentationChanged();
		throw;
	}
	OnControlTemplatePresentationChanged();
	RequestLayout();
	InvalidateVisual();
	return _controlTemplateRoot;
}

std::unique_ptr<Control> Control::DetachVisualChildTemplateRoot()
{
	if (!_controlTemplateRoot) return {};
	auto* previous = _controlTemplateRoot;
	_controlTemplateRoot = nullptr;
	auto result = DetachVisualChild(previous);
	ClearGenericTemplateOwner(result.get(), this);
	ClearDeclarativeTemplateScope();
	OnControlTemplatePresentationChanged();
	RequestLayout();
	InvalidateVisual();
	return result;
}

void Control::RegisterInheritanceChild(Control* child)
{
	if (!child || std::find(_inheritanceChildren.begin(),
		_inheritanceChildren.end(), child) != _inheritanceChildren.end()) return;
	_inheritanceChildren.push_back(child);
}

void Control::UnregisterInheritanceChild(Control* child)
{
	_inheritanceChildren.erase(std::remove(
		_inheritanceChildren.begin(), _inheritanceChildren.end(), child),
		_inheritanceChildren.end());
}

void Control::RefreshInheritanceContext(bool recursive)
{
	RefreshInheritedPropertiesRecursive();
	RebuildStyleSubscriptions(recursive);
	(void)RefreshDynamicResourceValues(recursive);
	(void)RefreshStyleValues(recursive);
}

namespace
{
	bool WouldCreateRoutedParentCycle(
		const Control& child,
		Control* candidateParent) noexcept
	{
		std::unordered_set<const Control*> visited;
		std::vector<const Control*> pending;
		if (candidateParent) pending.push_back(candidateParent);
		while (!pending.empty())
		{
			const auto* current = pending.back();
			pending.pop_back();
			if (current == &child) return true;
			if (!visited.insert(current).second) continue;
			// Inspect the union of all structural parent edges, not only today's
			// precedence-selected GetRoutedParent().  Otherwise a logical/template
			// edge hidden by VisualParent could become a cycle on a later detach.
			if (auto* parent = current->GetVisualParent())
				pending.push_back(parent);
			if (auto* parent = current->GetLogicalParent())
				pending.push_back(parent);
			if (auto* parent = current->GetTemplatedParent())
				pending.push_back(parent);
		}
		return false;
	}
}

void Control::SetVisualParentCore(Control* value)
{
	if (_visualParent == value) return;
	if (WouldCreateRoutedParentCycle(*this, value))
		throw std::logic_error("可视/逻辑/模板路由树不能形成循环");
	const ControlWeakReference selfReference(this);
	const ControlWeakReference previousReference(_visualParent);
	const ControlWeakReference valueReference(value);
	auto enabledSnapshot = CaptureEffectiveIsEnabledSubtree(*this);
	auto visibleSnapshot = CaptureEffectiveIsVisibleSubtree(*this);
	_visualParent = value;
	PublishEffectiveIsEnabledChanges(std::move(enabledSnapshot));
	PublishEffectiveIsVisibleChanges(std::move(visibleSnapshot));
	auto* live = selfReference.Get();
	if (!live || (value && !valueReference)
		|| live->_visualParent != valueReference.Get()) return;
	cui::framework::EventAccess::Raise(
		live->OnVisualParentChanged,
		live, previousReference.Get(), valueReference.Get());
}

ControlWeakReference::ControlWeakReference(Control* target) noexcept
	: _target(target),
	_lifetime(target ? target->WeakLifetimeToken()
		: std::weak_ptr<const bool>{})
{
}

ControlWeakReference& ControlWeakReference::operator=(Control* target) noexcept
{
	_target = target;
	_lifetime = target ? target->WeakLifetimeToken()
		: std::weak_ptr<const bool>{};
	return *this;
}

Control* ControlWeakReference::Get() const noexcept
{
	if (!_target) return nullptr;
	const auto lifetime = _lifetime.lock();
	return lifetime && *lifetime ? _target : nullptr;
}

void Control::PropagatePresentationWindow(
	Control* control,
	Window* form)
{
	if (!control) return;
	const ControlWeakReference requestedWindowReference(form);
	std::unordered_set<Control*> activePath;
	struct ActivePathEntry final
	{
		std::unordered_set<Control*>& Path;
		Control* Value = nullptr;
		~ActivePathEntry() { Path.erase(Value); }
	};
	auto propagate = [&](auto&& self, ControlWeakReference controlReference) -> void
	{
		auto* live = controlReference.Get();
		if (!live || !activePath.insert(live).second) return;
		const ActivePathEntry active{ activePath, live };
		auto* requestedWindow = dynamic_cast<Window*>(
			requestedWindowReference.Get());
		if (form && !requestedWindow) return;

		const ControlWeakReference previousWindowReference(live->GetPresentationWindow());
		const bool windowChanged = live->GetPresentationWindow() != requestedWindow;
		if (windowChanged)
		{
			live->_hasLastInvalidatedClientRect = false;
			if (live->GetPresentationWindow())
				(void)RoutedCommandManager::InvalidateRequerySuggested(*live);
			live = controlReference.Get();
			requestedWindow = dynamic_cast<Window*>(
				requestedWindowReference.Get());
			if (!live || (form && !requestedWindow)
				|| live->GetPresentationWindow()
					!= dynamic_cast<Window*>(previousWindowReference.Get())) return;
		}
		live->SetPresentationWindowCore(requestedWindow);
		RoutedCommandManager::NotifySourceScopeChanged(*live);

		live = controlReference.Get();
		requestedWindow = dynamic_cast<Window*>(
			requestedWindowReference.Get());
		if (!live || (form && !requestedWindow)
			|| live->GetPresentationWindow() != requestedWindow) return;
		live->_layoutState.InvalidateMeasure();

		std::vector<ControlWeakReference> children;
		children.reserve(live->_visualChildren.size());
		for (auto* child : live->_visualChildren)
			if (child) children.emplace_back(child);
		for (const auto& childReference : children)
		{
			live = controlReference.Get();
			requestedWindow = dynamic_cast<Window*>(
				requestedWindowReference.Get());
			auto* child = childReference.Get();
			if (!live || (form && !requestedWindow)
				|| live->GetPresentationWindow() != requestedWindow) return;
			if (!child || child->GetVisualParent() != live) continue;
			self(self, childReference);
		}

		live = controlReference.Get();
		requestedWindow = dynamic_cast<Window*>(
			requestedWindowReference.Get());
		if (!live || (form && !requestedWindow)
			|| live->GetPresentationWindow() != requestedWindow) return;
		if (windowChanged)
			live->OnPresentationWindowChanged(
					dynamic_cast<Window*>(previousWindowReference.Get()),
					requestedWindow);
	};
	propagate(propagate, ControlWeakReference(control));
}

void Control::SetLogicalParentCore(Control* value)
{
	if (_logicalParent == value) return;
	if (WouldCreateRoutedParentCycle(*this, value))
		throw std::logic_error("可视/逻辑/模板路由树不能形成循环");
	const ControlWeakReference selfReference(this);
	const ControlWeakReference previousReference(_logicalParent);
	const ControlWeakReference valueReference(value);
	const bool routedThroughLogicalParent = _visualParent == nullptr;
	auto snapshot = routedThroughLogicalParent
		? CaptureEffectiveIsEnabledSubtree(*this)
		: std::vector<std::pair<ControlWeakReference, bool>>{};
	auto* previousInheritanceParent = GetInheritanceParent();
	if (_logicalParent)
	{
		auto& siblings = _logicalParent->_logicalChildren;
		siblings.erase(std::remove(siblings.begin(), siblings.end(), this),
			siblings.end());
	}
	_logicalParent = value;
	if (value && std::find(value->_logicalChildren.begin(),
		value->_logicalChildren.end(), this) == value->_logicalChildren.end())
		value->_logicalChildren.push_back(this);
	auto* currentInheritanceParent = GetInheritanceParent();
	if (previousInheritanceParent != currentInheritanceParent)
	{
		if (previousInheritanceParent)
			previousInheritanceParent->UnregisterInheritanceChild(this);
		if (currentInheritanceParent)
			currentInheritanceParent->RegisterInheritanceChild(this);
		RefreshInheritanceContext(true);
	}
	auto* live = selfReference.Get();
	if (!live || (value && !valueReference)
		|| live->_logicalParent != valueReference.Get()) return;
	if (routedThroughLogicalParent)
		live->PublishEffectiveIsEnabledChanges(std::move(snapshot));
	live = selfReference.Get();
	if (!live || (value && !valueReference)
		|| live->_logicalParent != valueReference.Get()) return;
	cui::framework::EventAccess::Raise(
		live->OnLogicalParentChanged,
		live, previousReference.Get(), valueReference.Get());
}

void Control::SetTemplatedParentCore(Control* value)
{
	if (_templatedParent == value) return;
	if (WouldCreateRoutedParentCycle(*this, value))
		throw std::logic_error("可视/逻辑/模板路由树不能形成循环");
	const ControlWeakReference selfReference(this);
	const ControlWeakReference previousReference(_templatedParent);
	const ControlWeakReference valueReference(value);
	const bool routedThroughTemplatedParent = _visualParent == nullptr
		&& _logicalParent == nullptr;
	auto snapshot = routedThroughTemplatedParent
		? CaptureEffectiveIsEnabledSubtree(*this)
		: std::vector<std::pair<ControlWeakReference, bool>>{};
	auto* previousInheritanceParent = GetInheritanceParent();
	_templatedParent = value;
	auto* currentInheritanceParent = GetInheritanceParent();
	if (previousInheritanceParent != currentInheritanceParent)
	{
		if (previousInheritanceParent)
			previousInheritanceParent->UnregisterInheritanceChild(this);
		if (currentInheritanceParent)
			currentInheritanceParent->RegisterInheritanceChild(this);
		RefreshInheritanceContext(true);
	}
	auto* live = selfReference.Get();
	if (!live || (value && !valueReference)
		|| live->_templatedParent != valueReference.Get()) return;
	if (routedThroughTemplatedParent)
		live->PublishEffectiveIsEnabledChanges(std::move(snapshot));
	live = selfReference.Get();
	if (!live || (value && !valueReference)
		|| live->_templatedParent != valueReference.Get()) return;
	cui::framework::EventAccess::Raise(
		live->OnTemplatedParentChanged,
		live, previousReference.Get(), valueReference.Get());
}

void Control::SynchronizeVisualChildCollection(
	const CollectionChangedEventArgs& change)
{
	VerifyAccess();
	const ControlWeakReference selfReference(this);
	const std::vector<Control*> previous = _observedVisualChildren;
	std::vector<ControlWeakReference> previousReferences;
	previousReferences.reserve(previous.size());
	for (auto* child : previous)
		previousReferences.emplace_back(child);
	const std::unordered_set<Control*> previousSet(
		previous.begin(), previous.end());
	std::unordered_set<Control*> currentSet;
	currentSet.reserve(_visualChildren.size());
	auto reject = [&](const char* message, bool invalidArgument = false)
		{
			_visualChildren.RestoreRejectedMutation(previous);
			if (invalidArgument) throw std::invalid_argument(message);
			throw std::logic_error(message);
		};

	for (auto* child : _visualChildren)
	{
		if (!child)
			reject("不能添加空控件", true);
		if (!currentSet.insert(child).second)
			reject("不能重复添加同一控件");
		if (WouldCreateRoutedParentCycle(*child, this))
			reject("不能将控件添加到自身或其结构后代");
		const bool alreadyObserved = previousSet.contains(child);
		if (alreadyObserved)
		{
			if (child->_visualParent != this)
				reject("子控件 VisualParent 已在集合外被修改");
		}
		else if (child->_isWindowRoot || child->_visualParent
			|| child->_logicalParent
			|| (child->GetPresentationWindow() && child->GetPresentationWindow() != this->GetPresentationWindow()))
		{
			reject("该控件已属于其他容器");
		}
	}

	std::string validationError;
	if (!ValidateVisualChildCollection(
			std::span<Control* const>{
				_visualChildren.data(), _visualChildren.size() },
			validationError))
	{
		_visualChildren.RestoreRejectedMutation(previous);
		throw std::logic_error(validationError.empty()
			? "Specialized container rejected the child collection"
			: validationError);
	}

	// Publish the structural snapshot before invoking parent-change callbacks.
	// A callback may perform a nested detach/reparent; the nested synchronizer
	// must observe the collection that actually triggered this pass.
	_observedVisualChildren.assign(
		_visualChildren.begin(), _visualChildren.end());

	auto stillContains = [](const Control& owner, const Control* child)
	{
		return child && std::find(owner._visualChildren.begin(),
			owner._visualChildren.end(), child) != owner._visualChildren.end();
	};
	for (const auto& childReference : previousReferences)
	{
		auto* owner = selfReference.Get();
		auto* child = childReference.Get();
		if (!owner) return;
		if (!child || currentSet.contains(child)
			|| stillContains(*owner, child)) continue;
		const ControlWeakReference windowReference(owner->GetPresentationWindow());
		if (auto* window = dynamic_cast<Window*>(windowReference.Get()))
			window->ClearDetachedControlReferences(child);
		owner = selfReference.Get();
		child = childReference.Get();
		if (!owner) return;
		if (!child || stillContains(*owner, child)) continue;
		if (child->_visualParent == owner)
			child->SetVisualParentCore(nullptr);
		owner = selfReference.Get();
		child = childReference.Get();
		if (!owner) return;
		if (!child || stillContains(*owner, child)) continue;
		if (child->_logicalParent == owner)
			child->SetLogicalParentCore(nullptr);
		owner = selfReference.Get();
		child = childReference.Get();
		if (!owner) return;
		if (!child || stillContains(*owner, child)) continue;
		child->_isWindowRoot = false;
		if (!child->_visualParent)
			PropagatePresentationWindow(child, nullptr);
	}

	std::vector<ControlWeakReference> currentReferences;
	if (auto* owner = selfReference.Get())
	{
		currentReferences.reserve(owner->_visualChildren.size());
		for (auto* child : owner->_visualChildren)
			if (child) currentReferences.emplace_back(child);
	}
	else return;
	for (const auto& childReference : currentReferences)
	{
		auto* owner = selfReference.Get();
		auto* child = childReference.Get();
		if (!owner) return;
		if (!child || previousSet.contains(child)
			|| !stillContains(*owner, child)) continue;
		if (child->_visualParent && child->_visualParent != owner) continue;
		if (child->_visualParent != owner)
			child->SetVisualParentCore(owner);
		owner = selfReference.Get();
		child = childReference.Get();
		if (!owner) return;
		if (!child || !stillContains(*owner, child)
			|| child->_visualParent != owner) continue;
		if (child->_logicalParent && child->_logicalParent != owner) continue;
		if (child->_logicalParent != owner)
			child->SetLogicalParentCore(owner);
		owner = selfReference.Get();
		child = childReference.Get();
		if (!owner) return;
		if (!child || !stillContains(*owner, child)
			|| child->_visualParent != owner
			|| child->_logicalParent != owner) continue;
		child->_isWindowRoot = false;
		PropagatePresentationWindow(child, owner->GetPresentationWindow());
		owner = selfReference.Get();
		child = childReference.Get();
		if (!owner) return;
		if (!child || !stillContains(*owner, child)
			|| child->_visualParent != owner) continue;
		if (owner->_themeStyleSheet)
			child->SetThemeStyleSheet(owner->_themeStyleSheet, true);
		owner = selfReference.Get();
		child = childReference.Get();
		if (!owner) return;
		if (!child || !stillContains(*owner, child)
			|| child->_visualParent != owner) continue;
		if (owner->_styleSheet)
			child->SetStyleSheet(owner->_styleSheet, true);
	}

	auto* owner = selfReference.Get();
	if (!owner) return;
	owner->_observedVisualChildren.assign(
		owner->_visualChildren.begin(), owner->_visualChildren.end());
	std::vector<Control*> callbackPrevious;
	callbackPrevious.reserve(previousReferences.size());
	for (const auto& childReference : previousReferences)
		callbackPrevious.push_back(childReference.Get());
	owner->OnVisualChildCollectionChanged(
		change,
		std::span<Control* const>{
			callbackPrevious.data(), callbackPrevious.size() });
	owner = selfReference.Get();
	if (!owner) return;
	if (const ControlWeakReference windowReference(owner->GetPresentationWindow());
		auto* window = dynamic_cast<Window*>(windowReference.Get()))
		window->InvalidatePresentationStructure();
	owner = selfReference.Get();
	if (!owner) return;
	owner->RequestLayout();
	owner = selfReference.Get();
	if (!owner) return;
	owner->NotifyAccessibilityStructureChanged();
}

void Control::OnRender() {}

void Control::RequestLayout()
{
	this->_layoutState.InvalidateMeasure();
	if (this->_layoutDeferral.IsSuspended())
	{
		this->_layoutDeferral.QueueLayout();
		return;
	}
	if (this->_visualParent)
	{
		auto* panelParent = dynamic_cast<Panel*>(this->_visualParent);
		if (panelParent)
		{
			panelParent->InvalidateLayout();
		}
		else
		{
			// Some composite controls are not Panel-derived but still participate in
			// the visual tree. Keep walking until a real layout boundary is found.
			this->_visualParent->RequestLayout();
		}
		return;
	}

	if (this->GetPresentationWindow())
	{
		this->GetPresentationWindow()->RequestLayout();
	}
}

void Control::RequestArrange()
{
	this->_layoutState.InvalidateArrange();
	if (this->_layoutDeferral.IsSuspended())
	{
		this->_layoutDeferral.QueueLayout();
		return;
	}
	if (this->_visualParent)
	{
		if (auto* panelParent = dynamic_cast<Panel*>(this->_visualParent))
			panelParent->InvalidateArrangeLayout();
		else
			this->_visualParent->RequestArrange();
		return;
	}
	if (this->GetPresentationWindow())
		this->GetPresentationWindow()->RequestArrangeLayout();
}

void Control::BeginLayoutUpdateDeferral() noexcept
{
	_layoutDeferral.Suspend();
}

void Control::EndLayoutUpdateDeferral(bool performLayout)
{
	const auto work = _layoutDeferral.Resume();
	if (!work.ready) return;
	if (work.layoutRequested)
	{
		RequestLayout();
		if (performLayout) PerformPendingLayout();
	}
	if (work.visualRequested && !work.visualBounds.IsEmpty())
		DispatchInvalidatedClientRect(ToD2DRect(work.visualBounds));
}

void Control::InvalidateMeasureSubtree()
{
	_layoutState.InvalidateMeasure();
	for (auto* child : _visualChildren)
	{
		if (child)
			child->InvalidateMeasureSubtree();
	}
}

void Control::InvalidateVisualSubtree()
{
	InvalidateVisual();
	for (auto* child : _visualChildren)
		if (child) child->InvalidateVisualSubtree();
}

void Control::InvalidateVisualBoundsSubtree()
{
	const auto bounds = GetAbsoluteRectDip();
	InvalidateVisualRectCore(D2D1::RectF(
		bounds.Left(), bounds.Top(), bounds.Right(), bounds.Bottom()), false);
	for (auto* child : _visualChildren)
		if (child) child->InvalidateVisualBoundsSubtree();
}

void Control::MarkPresentationInvalidation(
	PresentationInvalidationKind kind) noexcept
{
	auto advance = [](uint64_t& revision) noexcept
	{
		++revision;
		if (revision == 0) ++revision;
	};
	if (HasPresentationInvalidation(
		kind, PresentationInvalidationKind::Content))
		advance(_presentationRevisions.Content);
	if (HasPresentationInvalidation(
		kind, PresentationInvalidationKind::Geometry))
		advance(_presentationRevisions.Geometry);
	if (HasPresentationInvalidation(
		kind, PresentationInvalidationKind::Composition))
		advance(_presentationRevisions.Composition);
	if (GetPresentationWindow())
		GetPresentationWindow()->InvalidatePresentationNode(this, kind);
}

void Control::InvalidatePresentationGeometrySubtree() noexcept
{
	MarkPresentationInvalidation(PresentationInvalidationKind::Geometry);
}

void Control::InvalidateDescendantRenderGeometry() noexcept
{
	InvalidatePresentationGeometrySubtree();
}

D2DGraphics* Control::GetDrawingContext() const noexcept
{
	auto* window = GetPresentationWindow();
	return window ? window->GetCurrentDrawingContext() : nullptr;
}

void Control::BeginRender()
{
	auto renderSize = GetRenderSizeDip();
	BeginRender(renderSize.width, renderSize.height);
}
void Control::BeginRender(float clipW, float clipH)
{
	_activeGeometryClipCount = 0;
	if (!this->GetPresentationWindow() || !this->GetDrawingContext()) return;
	const float titleBarOffset = static_cast<float>(
		this->GetPresentationWindow()->GetTitleBarHeightDip());
	// Layout coordinates are relative to the form content. The control-local
	// transform is followed by ancestor transforms and finally the title bar.
	const auto transform = AsMatrix(GetLocalToRenderTransform())
		* D2D1::Matrix3x2F::Translation(0.0f, titleBarOffset);
	this->GetDrawingContext()->PushLocalTransform(transform, clipW, clipH);

	std::vector<const Control*> clipOwners;
	std::unordered_set<const Control*> visited;
	for (auto* current = this;
		current && visited.insert(current).second;
		current = current->_visualParent)
		if (current->_clip) clipOwners.push_back(current);
	if (clipOwners.empty()) return;
	std::reverse(clipOwners.begin(), clipOwners.end());
	auto renderToLocal = AsMatrix(GetLocalToRenderTransform());
	if (!renderToLocal.Invert()) return;
	for (const auto* owner : clipOwners)
	{
		const auto ownerToLocal = AsMatrix(owner->GetLocalToRenderTransform())
			* renderToLocal;
		Microsoft::WRL::ComPtr<ID2D1Geometry> native;
		native.Attach(owner->_clip->CreateD2DGeometry(&ownerToLocal));
		if (native && this->GetDrawingContext()->PushGeometryClip(native.Get()))
			++_activeGeometryClipCount;
	}
}
bool Control::SetDeclarativeComponentBehavior(
	std::unique_ptr<IDeclarativeComponentBehavior> behavior,
	const DeclarativeComponentBehaviorContext& context,
	std::wstring* outError)
{
	if (&context.Host != this)
	{
		if (outError) *outError = L"组件 Behavior 上下文与宿主不匹配。";
		return false;
	}
	ClearDeclarativeComponentBehavior();
	if (!behavior)
	{
		if (outError) outError->clear();
		return true;
	}

	_declarativeComponentBehavior = std::move(behavior);
	bool attached = false;
	try
	{
		attached = _declarativeComponentBehavior->Attach(
			*this, context, outError);
	}
	catch (...)
	{
		if (outError)
			*outError = L"组件 Behavior Attach 抛出异常。";
	}
	if (!attached)
	{
		auto failed = std::move(_declarativeComponentBehavior);
		try { failed->Detach(*this); } catch (...) {}
		if (outError && outError->empty())
			*outError = L"组件 Behavior 拒绝附加。";
		return false;
	}
	try
	{
		_declarativeComponentBehavior->DpiChanged(
			*this, GetPresentationWindow() ? GetPresentationWindow()->GetDpiScale() : 1.0f);
	}
	catch (...)
	{
	}
	InvalidateVisual();
	if (outError) outError->clear();
	return true;
}

void Control::ClearDeclarativeComponentBehavior() noexcept
{
	if (!_declarativeComponentBehavior) return;
	auto behavior = std::move(_declarativeComponentBehavior);
	try { behavior->Detach(*this); } catch (...) {}
	if (!_isDestroying) InvalidateVisual();
}

void Control::NotifyDpiChanged(float dpiScale)
{
	if (!_declarativeComponentBehavior) return;
	try
	{
		_declarativeComponentBehavior->DpiChanged(*this, dpiScale);
	}
	catch (...)
	{
	}
}

void Control::NotifyDeviceResourcesInvalidated() noexcept
{
	if (!_declarativeComponentBehavior) return;
	try
	{
		_declarativeComponentBehavior->DeviceResourcesInvalidated(*this);
	}
	catch (...)
	{
	}
}
void Control::ApplyBackgroundBrush(const cui::drawing::Brush& brush)
{
	if (brush.Kind == cui::drawing::BrushKind::None)
		_backgroundBrush.reset();
	else
		_backgroundBrush = brush;
	InvalidateVisual();
}
GET_CPP(Control, cui::drawing::Brush, Background)
{
	return GetComputedBackgroundBrush();
}
SET_CPP(Control, cui::drawing::Brush, Background)
{
	(void)TrySetPropertyValue(
		L"Background", BindingValue(std::move(value)),
		DependencyPropertyValueSource::Local);
}
void Control::ClearBackgroundBrush()
{
	if (!_backgroundBrush) return;
	_backgroundBrush.reset();
	InvalidateVisual();
}
ID2D1Brush* Control::CreateBackgroundBrush(
	D2DGraphics& graphics,
	D2D1_SIZE_F bounds) const
{
	return _backgroundBrush
		? _backgroundBrush->CreateBrush(graphics, bounds)
		: nullptr;
}
cui::drawing::Brush Control::GetComputedBackgroundBrush() const
{
	return _backgroundBrush.value_or(cui::drawing::NoBrush());
}
void Control::ApplyForegroundBrush(const cui::drawing::Brush& brush)
{
	if (brush.Kind == cui::drawing::BrushKind::None)
		_foregroundBrush.reset();
	else
		_foregroundBrush = brush;
	InvalidateVisual();
}
GET_CPP(Control, cui::drawing::Brush, Foreground)
{
	return GetComputedForegroundBrush();
}
SET_CPP(Control, cui::drawing::Brush, Foreground)
{
	(void)TrySetPropertyValue(
		L"Foreground", BindingValue(std::move(value)),
		DependencyPropertyValueSource::Local);
}
void Control::ClearForegroundBrush()
{
	if (!_foregroundBrush) return;
	_foregroundBrush.reset();
	InvalidateVisual();
}
ID2D1Brush* Control::CreateForegroundBrush(
	D2DGraphics& graphics,
	D2D1_SIZE_F bounds) const
{
	return _foregroundBrush
		? _foregroundBrush->CreateBrush(graphics, bounds)
		: nullptr;
}
cui::drawing::Brush Control::GetComputedForegroundBrush() const
{
	return _foregroundBrush.value_or(cui::drawing::NoBrush());
}
void Control::ApplyBorderBrush(const cui::drawing::Brush& brush)
{
	if (brush.Kind == cui::drawing::BrushKind::None)
		_borderBrush.reset();
	else
		_borderBrush = brush;
	InvalidateVisual();
}
GET_CPP(Control, cui::drawing::Brush, BorderBrush)
{
	return GetComputedBorderBrush();
}
SET_CPP(Control, cui::drawing::Brush, BorderBrush)
{
	(void)TrySetPropertyValue(
		L"BorderBrush", BindingValue(std::move(value)),
		DependencyPropertyValueSource::Local);
}
void Control::ClearBorderBrush()
{
	if (!_borderBrush) return;
	_borderBrush.reset();
	InvalidateVisual();
}
ID2D1Brush* Control::CreateBorderBrush(
	D2DGraphics& graphics,
	D2D1_SIZE_F bounds) const
{
	return _borderBrush
		? _borderBrush->CreateBrush(graphics, bounds)
		: nullptr;
}
cui::drawing::Brush Control::GetComputedBorderBrush() const
{
	return _borderBrush.value_or(cui::drawing::NoBrush());
}
void Control::SetClip(const cui::drawing::Geometry& geometry)
{
	if (_clip && *_clip == geometry) return;
	InvalidateVisualBoundsSubtree();
	_clip = geometry;
	InvalidatePresentationGeometrySubtree();
	InvalidateVisualBoundsSubtree();
}
void Control::ClearClip()
{
	if (!_clip) return;
	InvalidateVisualBoundsSubtree();
	_clip.reset();
	InvalidatePresentationGeometrySubtree();
	InvalidateVisualBoundsSubtree();
}
void Control::SetRenderTransform(const cui::drawing::Transform& transform)
{
	if (transform.Empty())
	{
		ClearRenderTransform();
		return;
	}
	if (_renderTransform && *_renderTransform == transform) return;
	InvalidateVisualBoundsSubtree();
	_renderTransform = transform;
	InvalidatePresentationGeometrySubtree();
	InvalidateVisualBoundsSubtree();
}
void Control::ClearRenderTransform()
{
	if (!_renderTransform) return;
	InvalidateVisualBoundsSubtree();
	_renderTransform.reset();
	InvalidatePresentationGeometrySubtree();
	InvalidateVisualBoundsSubtree();
}
void Control::SetRenderTransformOrigin(D2D1_POINT_2F origin)
{
	SetRenderTransformOriginDip(cui::core::Point{ origin.x, origin.y });
}
void Control::SetRenderTransformOriginDip(cui::core::Point origin)
{
	if (!std::isfinite(origin.x) || !std::isfinite(origin.y)) return;
	if (_renderTransformOrigin.x == origin.x
		&& _renderTransformOrigin.y == origin.y) return;
	InvalidateVisualBoundsSubtree();
	_renderTransformOrigin = D2D1::Point2F(origin.x, origin.y);
	InvalidatePresentationGeometrySubtree();
	InvalidateVisualBoundsSubtree();
}
void Control::EndRender()
{
	if (!this->GetPresentationWindow() || !this->GetDrawingContext()) return;
	if (_declarativeComponentBehavior)
	{
		try
		{
			_declarativeComponentBehavior->RenderOverlay(
				*this, *this->GetDrawingContext());
		}
		catch (...)
		{
		}
	}
	while (_activeGeometryClipCount > 0)
	{
		this->GetDrawingContext()->PopGeometryClip();
		--_activeGeometryClipCount;
	}
	this->GetDrawingContext()->PopLocalTransform();
	this->_layoutState.CommitPaint();
	// The previous invalidation is only a coalescing aid while a visual is
	// waiting to be painted.  Keeping it after a successful paint causes every
	// later region-only request to be unioned with stale (often full-control)
	// damage and eventually promotes local rendering back to a full frame.
	_hasLastInvalidatedClientRect = false;
}

void Control::InvalidateVisual()
{
	this->InvalidateVisualRect(ToD2DRect(GetAbsoluteRectDip()));
}

void Control::InvalidateVisualRect(const D2D1_RECT_F& contentRect)

{
	InvalidateVisualRectCore(contentRect, true);
}

void Control::InvalidateComposition()
{
	VerifyAccess();
	MarkPresentationInvalidation(PresentationInvalidationKind::Composition);
	InvalidateVisualRectCore(ToD2DRect(GetAbsoluteRectDip()), false);
}

void Control::InvalidateVisualRectCore(
	const D2D1_RECT_F& contentRect,
	bool contentChanged)
{
	VerifyAccess();
	if (contentChanged)
		MarkPresentationInvalidation(PresentationInvalidationKind::Content);
	this->_layoutState.InvalidatePaint();
	if (!this->IsVisible || !this->GetPresentationWindow()) return;
	const auto renderedRect = TransformAbsoluteRectToRenderSpace(contentRect);
	const RECT currentClientPixels = this->GetPresentationWindow()->ContentDipRectToClientPixels(renderedRect);
	const D2D1_RECT_F currentRect{
		(float)currentClientPixels.left,
		(float)currentClientPixels.top,
		(float)currentClientPixels.right,
		(float)currentClientPixels.bottom
	};

	D2D1_RECT_F invalidRect = currentRect;
	if (_hasLastInvalidatedClientRect)
	{
		invalidRect.left = (std::min)(_lastInvalidatedClientRect.left, currentRect.left);
		invalidRect.top = (std::min)(_lastInvalidatedClientRect.top, currentRect.top);
		invalidRect.right = (std::max)(_lastInvalidatedClientRect.right, currentRect.right);
		invalidRect.bottom = (std::max)(_lastInvalidatedClientRect.bottom, currentRect.bottom);
	}
	DispatchInvalidatedClientRect(invalidRect);

	_lastInvalidatedClientRect = currentRect;
	_hasLastInvalidatedClientRect = true;
}

void Control::DispatchInvalidatedClientRect(const D2D1_RECT_F& clientRect)
{
	std::unordered_set<Control*> visited;
	for (Control* current = this;
		current && visited.insert(current).second;
		current = current->_visualParent)
	{
		if (current->_layoutDeferral.IsSuspended())
		{
			current->_layoutDeferral.QueueVisual(ToCoreRect(clientRect));
			return;
		}
	}
	if (this->GetPresentationWindow())
		this->GetPresentationWindow()->Invalidate(clientRect, false);
}

void Control::UpdateCaretBlinkState(bool focused, int selectionStart, int selectionEnd, bool caretRectValid, const D2D1_RECT_F* caretRect)
{
	bool shouldResetBlink = false;
	if (focused != _caretBlinkFocused)
		shouldResetBlink = focused;
	if (selectionStart != _caretBlinkSelectionStart || selectionEnd != _caretBlinkSelectionEnd)
		shouldResetBlink = true;
	if (caretRectValid != _caretBlinkRectValid)
		shouldResetBlink = true;
	if (caretRectValid && caretRect)
	{
		if (!_caretBlinkRectValid ||
			std::fabs(_caretBlinkRect.left - caretRect->left) > 0.1f ||
			std::fabs(_caretBlinkRect.top - caretRect->top) > 0.1f ||
			std::fabs(_caretBlinkRect.right - caretRect->right) > 0.1f ||
			std::fabs(_caretBlinkRect.bottom - caretRect->bottom) > 0.1f)
		{
			shouldResetBlink = true;
		}
		_caretBlinkRect = *caretRect;
	}
	else
	{
		_caretBlinkRect = { 0,0,0,0 };
	}

	_caretBlinkFocused = focused;
	_caretBlinkSelectionStart = selectionStart;
	_caretBlinkSelectionEnd = selectionEnd;
	_caretBlinkRectValid = caretRectValid;

	if (shouldResetBlink || _caretBlinkResetTick == 0)
		_caretBlinkResetTick = ::GetTickCount64();
}

bool Control::IsCaretBlinkVisible() const
{
	if (!_caretBlinkFocused) return false;
	if (!_caretBlinkRectValid) return false;
	if (_caretBlinkSelectionStart != _caretBlinkSelectionEnd) return false;

	const UINT blinkTime = ::GetCaretBlinkTime();
	if (blinkTime == INFINITE || blinkTime == 0)
		return true;

	const ULONGLONG elapsed = ::GetTickCount64() - _caretBlinkResetTick;
	return ((elapsed / blinkTime) % 2ULL) == 0;
}

bool Control::IsCaretBlinkAnimating() const
{
	if (!_caretBlinkFocused) return false;
	if (!_caretBlinkRectValid) return false;
	if (_caretBlinkSelectionStart != _caretBlinkSelectionEnd) return false;

	const UINT blinkTime = ::GetCaretBlinkTime();
	return blinkTime != 0 && blinkTime != INFINITE;
}

bool Control::GetCaretBlinkInvalidRect(D2D1_RECT_F& outRect) const
{
	if (!_caretBlinkFocused) return false;
	if (!_caretBlinkRectValid) return false;
	if (_caretBlinkSelectionStart != _caretBlinkSelectionEnd) return false;
	outRect = _caretBlinkRect;
	return true;
}

Font* Control::GetRenderFont()
{
	if (!this->_renderFont)
		this->ApplyTypographyFont();
	if (!this->_renderFont) return nullptr;
	const float factor = this->GetPresentationWindow()
		? this->GetPresentationWindow()->GetTextScaleFactor() : 1.0f;
	if (!(factor > 1.0001f)) return this->_renderFont.get();
	const float sourceSize = this->_renderFont->FontSize;
	if (!this->_systemScaledFont
		|| std::fabs(this->_systemScaledFontSourceSize - sourceSize) > 0.001f
		|| std::fabs(this->_systemScaledFontFactor - factor) > 0.001f)
	{
		this->_systemScaledFont = std::make_unique<::Font>(
			this->_renderFont->FontFamily, sourceSize * factor);
		this->_systemScaledFontSourceSize = sourceSize;
		this->_systemScaledFontFactor = factor;
	}
	return this->_systemScaledFont.get();
}

GET_CPP(Control, const std::wstring&, FontFamily)
{
	return _fontName;
}

GET_CPP(Control, double, FontSize)
{
	return _fontSize;
}

void Control::ApplyTypographyFont()
{
	_systemScaledFont.reset();
	if (_renderFont)
	{
		_renderFont->FontFamily = _fontName;
		_renderFont->FontSize = static_cast<float>(_fontSize);
	}
	else
	{
		_renderFont = std::make_unique<::Font>(
			_fontName, static_cast<float>(_fontSize));
	}
}

GET_CPP(Control, BindingCollection&, DataBindings)
{
	if (!this->_dataBindings)
		this->_dataBindings = std::make_unique<BindingCollection>(this);
	return *this->_dataBindings;
}

bool Control::SetDeclarativeTypeDescriptor(
	std::shared_ptr<const DeclarativeTypeDescriptor> descriptor,
	std::wstring* outError)
{
	auto fail = [&](std::wstring message)
	{
		if (outError) *outError = std::move(message);
		return false;
	};
	if (!descriptor)
		return fail(L"声明类型描述符不能为空。");
	if (_declarativeTypeDescriptor)
	{
		if (_declarativeTypeDescriptor == descriptor)
		{
			if (outError) outError->clear();
			return true;
		}
		return fail(L"控件实例已经绑定到另一个声明类型；类型身份不可变。");
	}

	auto rejectNativeCollision = [&](const std::wstring& name)
	{
		if (!DependencyPropertyRegistry::FindNative(*this, name)) return false;
		if (outError) *outError = L"声明类型成员不能覆盖控件已有属性："
			+ name;
		return true;
	};
	for (const auto* property : descriptor->Properties())
		if (property && rejectNativeCollision(property->Name())) return false;
	for (const auto& event : descriptor->Events())
		if (rejectNativeCollision(event.Name)) return false;
	for (const auto& content : descriptor->ContentProperties())
		if (rejectNativeCollision(content.Name)) return false;

	std::vector<BindingValue> values;
	values.reserve(descriptor->PropertyCount());
	for (std::size_t slot = 0; slot < descriptor->PropertyCount(); ++slot)
	{
		BindingValue value;
		if (!descriptor->TryGetPropertyDefault(slot, value))
			return fail(L"声明类型描述符包含无效的属性值槽。");
		values.push_back(std::move(value));
	}
	_declarativePropertyValues = std::move(values);
	_declarativeTypeDescriptor = std::move(descriptor);
	RefreshStyleValues(false);
	if (_declarativeTypeDescriptor->HasInheritedProperties())
		RefreshInheritedPropertiesRecursive();
	if (outError) outError->clear();
	return true;
}

const DependencyPropertyMetadata* Control::FindDeclarativePropertyMetadata(
	const std::wstring& propertyName) const
{
	return _declarativeTypeDescriptor
		? _declarativeTypeDescriptor->FindProperty(propertyName) : nullptr;
}

std::vector<const DependencyPropertyMetadata*>
Control::GetDeclarativePropertyMetadata() const
{
	if (!_declarativeTypeDescriptor) return {};
	const auto properties = _declarativeTypeDescriptor->Properties();
	return std::vector<const DependencyPropertyMetadata*>(
		properties.begin(), properties.end());
}

bool Control::SupportsNativeProperty(
	const DependencyPropertyMetadata& metadata) const
{
	return IsNativePropertySupportedByUIClass(
		const_cast<Control*>(this)->Type(), metadata);
}

bool Control::TryGetDeclarativePropertyBacking(
	const DeclarativeTypeDescriptor& owner,
	std::size_t slot,
	BindingValue& out) const
{
	if (_declarativeTypeDescriptor.get() != &owner
		|| slot >= _declarativePropertyValues.size()) return false;
	out = _declarativePropertyValues[slot];
	return true;
}

bool Control::TrySetDeclarativePropertyBacking(
	const DeclarativeTypeDescriptor& owner,
	std::size_t slot,
	const BindingValue& value)
{
	if (_declarativeTypeDescriptor.get() != &owner
		|| slot >= _declarativePropertyValues.size()) return false;
	_declarativePropertyValues[slot] = value;
	return true;
}

const DependencyPropertyMetadata* Control::FindPropertyMetadata(
	const std::wstring& propertyName)
{
	return DependencyPropertyRegistry::Find(*this, propertyName);
}

bool Control::TryGetPropertyValue(
	const std::wstring& propertyName,
	BindingValue& out)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && metadata->TryGet(*this, out);
}

bool Control::TryGetPropertyValue(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source,
	BindingValue& out)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return false;
	if (source == DependencyPropertyValueSource::Default)
		return metadata->TryGetDefaultValue(out);
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(metadata);
	if (entry == _propertyValues.end()
		|| !entry->second.Slots[(size_t)index].ProposedValue.has_value())
		return false;
	out = *entry->second.Slots[(size_t)index].ProposedValue;
	return true;
}

bool Control::TrySetPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	return TrySetPropertyValue(
		propertyName, value, DependencyPropertyValueSource::Local);
}

bool Control::TrySetPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value,
	DependencyPropertyValueSource source)
{
	return TrySetPropertyValueOwned(propertyName, value, source, nullptr);
}

bool Control::TrySetPropertyValueOwned(
	const std::wstring& propertyName,
	const BindingValue& value,
	DependencyPropertyValueSource source,
	const Binding* owner,
	bool allowReadOnly)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0
		|| (!metadata->CanWrite()
			&& !(allowReadOnly && metadata->IsReadOnly()
				&& metadata->CanWriteInternally()))) return false;

	if (owner) return false;
	BindingValue converted;
	if (!metadata->TryConvert(value, converted)) return false;
	return TrySetEffectiveValueEntry(
		*metadata, std::move(converted), source,
		source == DependencyPropertyValueSource::Animation
			? DependencyPropertyExpressionKind::Animation
			: DependencyPropertyExpressionKind::None,
		nullptr, {}, allowReadOnly);
}

bool Control::TrySetEffectiveValueEntry(
	const DependencyPropertyMetadata& metadata,
	std::optional<BindingValue> proposedValue,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind,
	const Binding* owner,
	std::wstring resourceKey,
	bool allowReadOnly)
{
	VerifyAccess();
	const int index = StoredPropertySourceIndex(source);
	if (index < 0
		|| (!metadata.CanWrite()
			&& !(allowReadOnly && metadata.IsReadOnly()
				&& metadata.CanWriteInternally()))) return false;

	const bool bindingExpression =
		expressionKind == DependencyPropertyExpressionKind::Binding
		|| expressionKind == DependencyPropertyExpressionKind::TemplateBinding;
	const bool validExpression =
		(expressionKind == DependencyPropertyExpressionKind::None
			&& proposedValue.has_value() && !owner && resourceKey.empty())
		|| (expressionKind == DependencyPropertyExpressionKind::Binding
			&& source == DependencyPropertyValueSource::Local
			&& owner && resourceKey.empty())
		|| (expressionKind == DependencyPropertyExpressionKind::TemplateBinding
			&& source == DependencyPropertyValueSource::Template
			&& owner && resourceKey.empty())
		|| (expressionKind == DependencyPropertyExpressionKind::DynamicResource
			&& source != DependencyPropertyValueSource::Animation
			&& !owner && !resourceKey.empty())
		|| (expressionKind == DependencyPropertyExpressionKind::Animation
			&& source == DependencyPropertyValueSource::Animation
			&& proposedValue.has_value() && !owner && resourceKey.empty());
	if (!validExpression) return false;

	auto [entryIt, inserted] = _propertyValues.try_emplace(&metadata);
	auto& entry = entryIt->second;
	if (inserted)
	{
		// A Local value/expression replaces the previous local state. Its
		// fallback is metadata default, never a hidden copy resurrected later.
		entry.HasBaseValue = source == DependencyPropertyValueSource::Local
			&& metadata.TryGetDefaultValue(entry.BaseValue);
		if (!entry.HasBaseValue)
			entry.HasBaseValue = metadata.TryGet(*this, entry.BaseValue);
		if (!entry.HasBaseValue)
			entry.HasBaseValue = metadata.TryGetDefaultValue(entry.BaseValue);
	}

	const size_t sourceIndex = static_cast<size_t>(index);
	auto& slot = entry.Slots[sourceIndex];
	const bool previousWasBinding =
		slot.Expression == DependencyPropertyExpressionKind::Binding
		|| slot.Expression == DependencyPropertyExpressionKind::TemplateBinding;
	if (bindingExpression && previousWasBinding
		&& slot.BindingOwner && slot.BindingOwner != owner)
	{
		if (inserted) _propertyValues.erase(entryIt);
		return false;
	}

	BindingValue oldEffective;
	DependencyPropertyValueSource oldSource = DependencyPropertyValueSource::Default;
	const bool hadOldEffective = TryEvaluateEffectivePropertyValue(
		metadata, entry, oldEffective, oldSource);
	const auto previousSlot = slot;
	const Binding* retiredBinding = previousWasBinding
		&& (slot.BindingOwner != owner || slot.Expression != expressionKind)
		? slot.BindingOwner : nullptr;

	slot.ProposedValue = std::move(proposedValue);
	slot.Expression = expressionKind;
	slot.BindingOwner = bindingExpression ? owner : nullptr;
	slot.ResourceKey = expressionKind == DependencyPropertyExpressionKind::DynamicResource
		? std::move(resourceKey) : std::wstring{};

	BindingValue newEffective;
	DependencyPropertyValueSource newSource = DependencyPropertyValueSource::Default;
	if (!TryEvaluateEffectivePropertyValue(
		metadata, entry, newEffective, newSource))
	{
		slot = previousSlot;
		if (inserted) _propertyValues.erase(entryIt);
		return false;
	}

	BindingValue currentEffective;
	const bool backingStorageMatches = metadata.TryGet(*this, currentEffective)
		&& metadata.ValuesEqual(currentEffective, newEffective);
	const bool effectiveUnchanged = hadOldEffective
		&& oldSource == newSource
		&& metadata.ValuesEqual(oldEffective, newEffective)
		&& backingStorageMatches;
	if (!effectiveUnchanged
		&& !ApplyEffectivePropertyValue(
			metadata, newEffective, newSource, allowReadOnly))
	{
		slot = previousSlot;
		if (inserted) _propertyValues.erase(entryIt);
		return false;
	}

	if (retiredBinding)
		RetireBindingExpression(metadata.Name(), retiredBinding);
	return true;
}

bool Control::CanAcquireBindingPropertyValue(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!owner) return false;
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;
	const int index = StoredPropertySourceIndex(source);
	if (index < 0
		|| (expressionKind == DependencyPropertyExpressionKind::Binding
			&& source != DependencyPropertyValueSource::Local)
		|| (expressionKind == DependencyPropertyExpressionKind::TemplateBinding
			&& source != DependencyPropertyValueSource::Template))
		return false;
	const auto entry = _propertyValues.find(metadata);
	if (entry == _propertyValues.end()) return true;
	const auto& slot = entry->second.Slots[(size_t)index];
	const bool isBindingExpression =
		slot.Expression == DependencyPropertyExpressionKind::Binding
		|| slot.Expression == DependencyPropertyExpressionKind::TemplateBinding;
	return !isBindingExpression || !slot.BindingOwner
		|| slot.BindingOwner == owner;
}

bool Control::TryAttachBindingPropertyExpression(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!CanAcquireBindingPropertyValue(
		propertyName, owner, source, expressionKind)) return false;
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && TrySetEffectiveValueEntry(
		*metadata, std::nullopt, source, expressionKind, owner, {}, false);
}

bool Control::TrySetBindingPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!owner) return false;
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !IsBindingExpressionOwner(
		propertyName, owner, source, expressionKind)) return false;
	BindingValue converted;
	if (!metadata->TryConvert(value, converted)) return false;
	return TrySetEffectiveValueEntry(
		*metadata, std::move(converted), source,
		expressionKind, owner, {}, false);
}

bool Control::TrySetCurrentPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;

	BindingValue converted;
	BindingValue coerced;
	if (!metadata->TryConvert(value, converted)
		|| !metadata->TryCoerce(*this, converted, coerced)) return false;
	BindingValue current;
	if (metadata->TryGet(*this, current)
		&& metadata->ValuesEqual(current, coerced)) return true;

	DependencyPropertyValueSource source =
		DependencyPropertyValueSource::Default;
	const auto entry = _propertyValues.find(metadata);
	if (entry != _propertyValues.end())
	{
		BindingValue ignored;
		(void)TryResolveEffectivePropertyValue(
			*metadata, entry->second, ignored, source);
		const int sourceIndex = StoredPropertySourceIndex(source);
		const auto* slot = sourceIndex < 0 ? nullptr
			: &entry->second.Slots[static_cast<size_t>(sourceIndex)];
		if (slot
			&& (slot->Expression == DependencyPropertyExpressionKind::Binding
				|| slot->Expression
					== DependencyPropertyExpressionKind::DynamicResource
				|| slot->Expression
					== DependencyPropertyExpressionKind::TemplateBinding))
		{
			return TrySetEffectiveValueEntry(
				*metadata, std::move(converted),
				source, slot->Expression, slot->BindingOwner,
				slot->ResourceKey, false);
		}
	}

	// WPF SetCurrentValue changes the value carried by the existing source;
	// it never promotes a Default/Style/Template/Inherited value to Local.
	// A later update from that source therefore remains authoritative.
	return source == DependencyPropertyValueSource::Default
		? TrySetPropertyBaseValue(propertyName, converted)
		: TrySetPropertyValue(propertyName, converted, source);
}

bool Control::TrySetReadOnlyPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->IsReadOnly()) return false;
	return TrySetPropertyValueOwned(
		metadata->Name(), value, DependencyPropertyValueSource::Local, nullptr, true);
}

bool Control::ClearReadOnlyPropertyValue(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->IsReadOnly()) return false;
	return ClearPropertyValueOwned(
		metadata->Name(), DependencyPropertyValueSource::Local, nullptr, true);
}

bool Control::ReevaluatePropertyValue(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;

	const auto entry = _propertyValues.find(metadata);
	if (entry != _propertyValues.end())
	{
		BindingValue effective;
		DependencyPropertyValueSource source = DependencyPropertyValueSource::Default;
		if (!TryEvaluateEffectivePropertyValue(
			*metadata, entry->second, effective, source)) return false;
		BindingValue current;
		if (metadata->TryGet(*this, current)
			&& metadata->ValuesEqual(current, effective)) return true;
		return ApplyEffectivePropertyValue(*metadata, effective, source);
	}

	BindingValue proposed;
	if (!metadata->TryGetDefaultValue(proposed)
		&& !metadata->TryGet(*this, proposed)) return false;
	BindingValue converted;
	BindingValue effective;
	if (!metadata->TryConvert(proposed, converted)
		|| !metadata->TryCoerce(*this, converted, effective)) return false;

	BindingValue current;
	if (metadata->TryGet(*this, current)
		&& metadata->ValuesEqual(current, effective)) return true;
	return ApplyEffectivePropertyValue(
		*metadata, effective, DependencyPropertyValueSource::Default);
}

bool Control::ClearPropertyValue(
	const std::wstring& propertyName)
{
	return ClearPropertyValue(
		propertyName, DependencyPropertyValueSource::Local);
}

bool Control::ClearPropertyValue(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source)
{
	return ClearPropertyValueOwned(propertyName, source, nullptr);
}

bool Control::ClearPropertyValueOwned(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source,
	const Binding* owner,
	bool allowReadOnly)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0
		|| (!metadata->CanWrite()
			&& !(allowReadOnly && metadata->IsReadOnly()
				&& metadata->CanWriteInternally()))) return false;
	auto entryIt = _propertyValues.find(metadata);
	if (entryIt == _propertyValues.end()) return false;
	auto& entry = entryIt->second;
	const size_t sourceIndex = (size_t)index;
	auto& slot = entry.Slots[sourceIndex];
	if (!slot.IsOccupied()) return false;
	const bool bindingExpression =
		slot.Expression == DependencyPropertyExpressionKind::Binding
		|| slot.Expression == DependencyPropertyExpressionKind::TemplateBinding;
	if (owner && (!bindingExpression || slot.BindingOwner != owner)) return false;

	BindingValue oldEffective;
	DependencyPropertyValueSource oldSource = DependencyPropertyValueSource::Default;
	const bool hadOldEffective = TryEvaluateEffectivePropertyValue(
		*metadata, entry, oldEffective, oldSource);
	const auto previous = slot;
	const Binding* retiredBinding = bindingExpression && !owner
		? slot.BindingOwner : nullptr;
	slot.Reset();

	BindingValue newEffective;
	DependencyPropertyValueSource newSource = DependencyPropertyValueSource::Default;
	const bool hasNewEffective = TryEvaluateEffectivePropertyValue(
		*metadata, entry, newEffective, newSource);
	BindingValue currentEffective;
	const bool backingStorageMatches = hasNewEffective
		&& metadata->TryGet(*this, currentEffective)
		&& metadata->ValuesEqual(currentEffective, newEffective);
	const bool effectiveUnchanged = hadOldEffective && hasNewEffective
		&& oldSource == newSource
		&& metadata->ValuesEqual(oldEffective, newEffective)
		&& backingStorageMatches;
	const bool applied = effectiveUnchanged || !hasNewEffective
		|| ApplyEffectivePropertyValue(
			*metadata, newEffective, newSource, allowReadOnly);
	if (!applied)
	{
		slot = previous;
		return false;
	}
	if (!entry.HasSources()) _propertyValues.erase(entryIt);
	if (retiredBinding)
		RetireBindingExpression(metadata->Name(), retiredBinding);
	return true;

}

bool Control::ClearBindingPropertyValue(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind)
{
	if (!owner) return false;
	if (!IsBindingExpressionOwner(
		propertyName, owner, source, expressionKind)) return false;
	return ClearPropertyValueOwned(propertyName, source, owner);
}

bool Control::IsBindingExpressionOwner(
	const std::wstring& propertyName,
	const Binding* owner,
	DependencyPropertyValueSource source,
	DependencyPropertyExpressionKind expressionKind) const
{
	if (!owner) return false;
	auto* mutableThis = const_cast<Control*>(this);
	const auto* metadata = mutableThis->FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0) return false;
	const auto entry = _propertyValues.find(metadata);
	if (entry == _propertyValues.end()) return false;
	const auto& slot = entry->second.Slots[(size_t)index];
	return slot.Expression == expressionKind && slot.BindingOwner == owner;
}

void Control::RetireBindingExpression(
	const std::wstring& propertyName,
	const Binding* owner)
{
	if (!owner) return;
	if (_dataBindings && _dataBindings->Find(propertyName) == owner)
	{
		(void)_dataBindings->Remove(propertyName);
		return;
	}
	const_cast<Binding*>(owner)->DetachReplacedTargetExpression();
}

size_t Control::ClearPropertyValues(DependencyPropertyValueSource source)
{
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return 0;
	std::vector<std::wstring> properties;
	properties.reserve(_propertyValues.size());
	for (const auto& [metadata, entry] : _propertyValues)
	{
		if (metadata && entry.Slots[(size_t)index].IsOccupied())
			properties.push_back(metadata->Name());
	}
	size_t cleared = 0;
	for (const auto& property : properties)
	{
		if (ClearPropertyValue(property, source)) ++cleared;
	}
	return cleared;
}

size_t Control::ClearPropertyValues()
{
	return ClearPropertyValues(DependencyPropertyValueSource::Local);
}

bool Control::HasPropertyValue(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return false;
	if (source == DependencyPropertyValueSource::Default)
	{
		BindingValue ignored;
		return metadata->TryGetDefaultValue(ignored);
	}
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(metadata);
	return entry != _propertyValues.end()
		&& entry->second.Slots[(size_t)index].IsOccupied();
}

DependencyPropertyValueSource Control::GetPropertyValueSource(
	const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return DependencyPropertyValueSource::Default;
	const auto entry = _propertyValues.find(metadata);
	if (entry == _propertyValues.end())
		return DependencyPropertyValueSource::Default;
	BindingValue value;
	DependencyPropertyValueSource source = DependencyPropertyValueSource::Default;
	TryResolveEffectivePropertyValue(*metadata, entry->second, value, source);
	return source;
}

DependencyPropertyExpressionKind Control::GetPropertyExpressionKind(
	const std::wstring& propertyName,
	DependencyPropertyValueSource source)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0) return DependencyPropertyExpressionKind::None;
	const auto entry = _propertyValues.find(metadata);
	return entry == _propertyValues.end()
		? DependencyPropertyExpressionKind::None
		: entry->second.Slots[(size_t)index].Expression;
}

bool Control::ResetPropertyValue(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;
	if (ClearPropertyValue(propertyName, DependencyPropertyValueSource::Local))
		return true;
	const auto entry = _propertyValues.find(metadata);
	if (entry != _propertyValues.end() && entry->second.HasSources())
		return false;
	BindingValue defaultValue;
	BindingValue effective;
	return metadata->TryGetDefaultValue(defaultValue)
		&& metadata->TryCoerce(*this, defaultValue, effective)
		&& ApplyEffectivePropertyValue(
			*metadata, effective, DependencyPropertyValueSource::Default);
}

bool Control::TrySetPropertyBaseValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;
	BindingValue converted;
	if (!metadata->TryConvert(value, converted)) return false;

	auto entryIt = _propertyValues.find(metadata);
	if (entryIt == _propertyValues.end())
	{
		BindingValue effective;
		if (!metadata->TryCoerce(*this, converted, effective)) return false;
		return ApplyEffectivePropertyValue(
			*metadata, effective, DependencyPropertyValueSource::Default);
	}

	auto& entry = entryIt->second;
	const auto previousBase = entry.BaseValue;
	const bool previouslyHadBase = entry.HasBaseValue;
	BindingValue previousEffective;
	DependencyPropertyValueSource previousSource =
		DependencyPropertyValueSource::Default;
	const bool hadPreviousEffective = TryEvaluateEffectivePropertyValue(
		*metadata, entry, previousEffective, previousSource);
	entry.BaseValue = converted;
	entry.HasBaseValue = true;

	BindingValue nextEffective;
	DependencyPropertyValueSource nextSource = DependencyPropertyValueSource::Default;
	if (!TryEvaluateEffectivePropertyValue(
		*metadata, entry, nextEffective, nextSource))
	{
		entry.BaseValue = previousBase;
		entry.HasBaseValue = previouslyHadBase;
		return false;
	}
	if (nextSource != DependencyPropertyValueSource::Default
		|| (hadPreviousEffective
			&& previousSource == nextSource
			&& metadata->ValuesEqual(previousEffective, nextEffective)))
		return true;
	if (ApplyEffectivePropertyValue(*metadata, nextEffective, nextSource))
		return true;
	entry.BaseValue = previousBase;
	entry.HasBaseValue = previouslyHadBase;
	return false;
}

bool Control::IsPropertyValueDefault(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanRead()) return false;
	BindingValue currentValue;
	BindingValue defaultValue;
	return metadata->TryGet(*this, currentValue)
		&& metadata->TryGetDefaultValue(defaultValue)
		&& metadata->ValuesEqual(currentValue, defaultValue);
}

bool Control::TryResolveEffectivePropertyValue(
	const DependencyPropertyMetadata& metadata,
	const EffectiveValueEntry& entry,
	BindingValue& value,
	DependencyPropertyValueSource& source) const
{
	for (int index = (int)entry.Slots.size() - 1; index >= 0; --index)
	{
		const auto& slot = entry.Slots[(size_t)index];
		if (!slot.ProposedValue.has_value()) continue;
		value = *slot.ProposedValue;
		source = static_cast<DependencyPropertyValueSource>(
			index + static_cast<int>(DependencyPropertyValueSource::Inherited));
		return true;
	}
	if (entry.HasBaseValue)
	{
		value = entry.BaseValue;
		source = DependencyPropertyValueSource::Default;
		return true;
	}
	source = DependencyPropertyValueSource::Default;
	return metadata.TryGetDefaultValue(value);
}

bool Control::TryEvaluateEffectivePropertyValue(
	const DependencyPropertyMetadata& metadata,
	const EffectiveValueEntry& entry,
	BindingValue& value,
	DependencyPropertyValueSource& source) const
{
	BindingValue proposed;
	if (!TryResolveEffectivePropertyValue(
		metadata, entry, proposed, source)) return false;
	return metadata.TryCoerce(
		*const_cast<Control*>(this), proposed, value);
}

bool Control::ApplyEffectivePropertyValue(
	const DependencyPropertyMetadata& metadata,
	const BindingValue& value,
	DependencyPropertyValueSource source,
	bool allowReadOnly)
{
	const ControlWeakReference selfReference(this);
	const auto* previousMetadata = _applyingPropertyMetadata;
	const auto previousSource = _applyingPropertySource;
	_applyingPropertyMetadata = &metadata;
	_applyingPropertySource = source;
	bool result = false;
	try
	{
		if (!allowReadOnly && metadata.IsReadOnly()) result = false;
		else result = metadata.TrySetEffective(*this, value);
	}
	catch (...)
	{
		if (auto* live = selfReference.Get())
		{
			live->_applyingPropertyMetadata = previousMetadata;
			live->_applyingPropertySource = previousSource;
		}
		throw;
	}
	if (auto* live = selfReference.Get())
	{
		live->_applyingPropertyMetadata = previousMetadata;
		live->_applyingPropertySource = previousSource;
	}
	return result;
}

Control* Control::FindDeclarativeTemplatePart(
	const std::wstring& localName) noexcept
{
	return const_cast<Control*>(static_cast<const Control*>(this)
		->FindDeclarativeTemplatePart(localName));
}

const Control* Control::FindDeclarativeTemplatePart(
	const std::wstring& localName) const noexcept
{
	const auto found = _templateNameScope.find(localName);
	return found == _templateNameScope.end() ? nullptr : found->second;
}

Control* Control::FindDeclarativeContentPresenter(
	const std::wstring& propertyName) noexcept
{
	return const_cast<Control*>(static_cast<const Control*>(this)
		->FindDeclarativeContentPresenter(propertyName));
}

const Control* Control::FindDeclarativeContentPresenter(
	const std::wstring& propertyName) const noexcept
{
	const auto found = std::find_if(
		_declarativeContentPresenters.begin(),
		_declarativeContentPresenters.end(),
		[&](const auto& item) { return item.first == propertyName; });
	return found == _declarativeContentPresenters.end() ? nullptr : found->second;
}

bool Control::RegisterDeclarativeTemplatePart(
	std::wstring localName,
	Control* instance)
{
	if (localName.empty() || !instance
		|| instance->GetTemplatedParent() != this) return false;
	return _templateNameScope.emplace(
		std::move(localName), instance).second;
}

bool Control::RegisterDeclarativeContentPresenter(
	std::wstring propertyName,
	Control* instance)
{
	if (propertyName.empty() || !instance
		|| instance->GetTemplatedParent() != this
		|| FindDeclarativeContentPresenter(propertyName)) return false;
	_declarativeContentPresenters.emplace_back(
		std::move(propertyName), instance);
	return true;
}

void Control::ClearDeclarativeTemplateScope()
{
	_templateNameScope.clear();
	_declarativeContentPresenters.clear();
}

void Control::RefreshInheritedPropertyValues()
{
	const auto properties = DependencyPropertyRegistry::GetProperties(*this);
	for (const auto* metadata : properties)
	{
		if (!metadata || !HasDependencyPropertyFlag(
			metadata->Flags(), DependencyPropertyFlags::Inherits)) continue;

		BindingValue inheritedValue;
		bool found = false;
		std::unordered_set<Control*> visited;
		for (auto* ancestor = GetInheritanceParent();
			ancestor && visited.insert(ancestor).second;
			ancestor = ancestor->GetInheritanceParent())
		{
			const auto* candidate = DependencyPropertyRegistry::Find(
				*ancestor, metadata->Name());
			if (!candidate
				|| !HasDependencyPropertyFlag(
					candidate->Flags(), DependencyPropertyFlags::Inherits)
				|| !metadata->HasSameInheritanceIdentity(*candidate))
				continue;
			if (candidate->TryGet(*ancestor, inheritedValue))
			{
				found = true;
				break;
			}
		}

		if (found)
			(void)TrySetPropertyValueOwned(
				metadata->Name(), inheritedValue,
				DependencyPropertyValueSource::Inherited, nullptr, true);
		else
			(void)ClearPropertyValueOwned(
				metadata->Name(), DependencyPropertyValueSource::Inherited, nullptr, true);
	}
}

void Control::RefreshInheritedPropertiesRecursive()
{
	const ControlWeakReference selfReference(this);
	const bool previous = _refreshingInheritedProperties;
	_refreshingInheritedProperties = true;
	RefreshInheritedPropertyValues();
	auto* live = selfReference.Get();
	if (!live) return;
	std::vector<ControlWeakReference> children;
	children.reserve(live->_inheritanceChildren.size());
	for (auto* child : live->_inheritanceChildren)
		if (child) children.emplace_back(child);
	for (const auto& childReference : children)
	{
		live = selfReference.Get();
		if (!live) return;
		auto* child = childReference.Get();
		if (!child || std::find(live->_inheritanceChildren.begin(),
			live->_inheritanceChildren.end(), child)
			== live->_inheritanceChildren.end()) continue;
		child->RefreshInheritedPropertiesRecursive();
	}
	live = selfReference.Get();
	if (live) live->_refreshingInheritedProperties = previous;
}

void Control::ApplyPropertyMetadataChange(
	const DependencyPropertyMetadata& metadata,
	const BindingValue& oldValue,
	const BindingValue& newValue)
{
	const ControlWeakReference selfReference(this);
	const auto flags = metadata.Flags();
	const auto propertyName = metadata.Name();
	++_propertyChangeVersion;
	metadata.NotifyChanged(*this, oldValue, newValue);
	auto* live = selfReference.Get();
	if (!live) return;
	if (HasDependencyPropertyFlag(flags, DependencyPropertyFlags::AffectsMeasure))
		live->RequestLayout();
	else if (HasDependencyPropertyFlag(flags, DependencyPropertyFlags::AffectsArrange))
		live->RequestArrange();
	live = selfReference.Get();
	if (!live) return;
	if (HasDependencyPropertyFlag(flags, DependencyPropertyFlags::AffectsRender))
		live->InvalidateVisual();
	live = selfReference.Get();
	if (!live) return;
	const ControlWeakReference visualParentReference(live->_visualParent);
	if (auto* visualParent = visualParentReference.Get())
	{
		if (HasDependencyPropertyFlag(
			flags, DependencyPropertyFlags::AffectsParentMeasure))
			visualParent->RequestLayout();
		else if (HasDependencyPropertyFlag(
			flags, DependencyPropertyFlags::AffectsParentArrange))
		{
			// Attached layout properties such as Canvas.Left invalidate the
			// parent's child-arrangement policy.  A Panel must mark that policy
			// dirty; invalidating only its own arrange slot does not rerun the
			// panel engine when the slot itself is unchanged.
			if (auto* panelParent = dynamic_cast<Panel*>(visualParent))
				panelParent->InvalidateArrangeLayout();
			else
				visualParent->RequestArrange();
		}
	}
	live = selfReference.Get();
	if (!live) return;
	if (!live->_refreshingInheritedProperties
		&& HasDependencyPropertyFlag(flags, DependencyPropertyFlags::Inherits))
	{
		std::vector<ControlWeakReference> children;
		children.reserve(live->_inheritanceChildren.size());
		for (auto* child : live->_inheritanceChildren)
			if (child) children.emplace_back(child);
		for (const auto& childReference : children)
		{
			live = selfReference.Get();
			if (!live) return;
			auto* child = childReference.Get();
			if (!child || std::find(live->_inheritanceChildren.begin(),
				live->_inheritanceChildren.end(), child)
				== live->_inheritanceChildren.end()) continue;
			child->RefreshInheritedPropertiesRecursive();
		}
	}

	DependencyPropertyChangedEventArgs args{
		propertyName, oldValue, newValue };
	live = selfReference.Get();
	if (!live) return;
	cui::framework::EventAccess::Raise(
		live->OnPropertyValueChanged, live, args);
	live = selfReference.Get();
	if (!live) return;
	live->_bindingSourcePropertyChanged.Notify(propertyName);
}

const DeclarativeEventDefinition* Control::FindDeclarativeEvent(
	const std::wstring& eventName) const noexcept
{
	return _declarativeTypeDescriptor
		? _declarativeTypeDescriptor->FindEvent(eventName) : nullptr;
}

bool Control::RaiseDeclarativeEvent(
	std::wstring eventName,
	BindingValue value)
{
	DeclarativeEventArgs args;
	args.Name = std::move(eventName);
	args.Value = std::move(value);
	return RaiseDeclarativeEvent(args);
}

bool Control::RaiseDeclarativeEvent(DeclarativeEventArgs& args)
{
	const auto* definition = FindDeclarativeEvent(args.Name);
	if (!definition || definition->PayloadKind != args.Value.Kind()) return false;
	args.OwnerType = _declarativeTypeDescriptor->TypeId();
	args.RoutingStrategy = definition->RoutingStrategy;
	args.OriginalSource = this;
	args.Source = this;

	const auto route = BuildRoutedEventRoute(
		this, definition->RoutingStrategy);
	const ControlWeakReference sourceLifetime(this);
	for (const auto& currentReference : route)
	{
		if (!sourceLifetime) break;
		auto* current = currentReference.Get();
		if (!current) continue;
		args.CurrentTarget = current;
		cui::framework::EventAccess::Raise(
			current->OnDeclarativeEvent, current, args);
	}
	args.CurrentTarget = nullptr;
	if (!sourceLifetime)
	{
		args.OriginalSource = nullptr;
		args.Source = nullptr;
	}
	return true;
}

struct Control::DeclarativeVisualStateRuntime
{
	struct RuntimeCondition
	{
		const DependencyPropertyMetadata* Metadata = nullptr;
		BindingValue Value;
	};

	struct RuntimeSetter
	{
		Control* Target = nullptr;
		std::wstring PropertyName;
		BindingValue Value;
	};

	enum class TransformMember : unsigned char
	{
		X,
		Y,
		ScaleX,
		ScaleY,
		Angle,
		AngleX,
		AngleY,
		CenterX,
		CenterY,
		Matrix,
	};

	struct TransformAccessor
	{
		size_t OperationIndex = 0;
		cui::drawing::TransformKind OperationKind =
			cui::drawing::TransformKind::Translate;
		TransformMember Member = TransformMember::X;
		std::wstring CanonicalPath;
	};

	enum class GeometryMember : unsigned char
	{
		Rect,
		Center,
		RadiusX,
		RadiusY,
		FillRule,
	};

	struct GeometryAccessor
	{
		std::vector<size_t> ChildIndices;
		cui::drawing::GeometryKind GeometryKind =
			cui::drawing::GeometryKind::Rectangle;
		GeometryMember Member = GeometryMember::Rect;
		std::wstring CanonicalPath;
	};

	enum class PathGeometryMember : unsigned char
	{
		FigureStartPoint,
		FigureIsClosed,
		FigureIsFilled,
		SegmentPoint,
		SegmentPoint1,
		SegmentPoint2,
		SegmentPoint3,
		ArcSize,
		ArcRotationAngle,
		ArcIsLargeArc,
		ArcSweepDirection,
	};

	struct PathGeometryAccessor
	{
		std::vector<size_t> ChildIndices;
		size_t FigureIndex = 0;
		size_t SegmentIndex = 0;
		bool HasSegment = false;
		cui::drawing::PathSegmentKind SegmentKind =
			cui::drawing::PathSegmentKind::Line;
		PathGeometryMember Member = PathGeometryMember::FigureStartPoint;
		std::wstring CanonicalPath;
	};

	struct GeometryTransformAccessor
	{
		std::vector<size_t> ChildIndices;
		cui::drawing::GeometryKind GeometryKind =
			cui::drawing::GeometryKind::Rectangle;
		TransformAccessor Transform;
		std::wstring CanonicalPath;
	};

	enum class BrushMember : unsigned char
	{
		SolidColor,
		Opacity,
		StartPoint,
		EndPoint,
		Center,
		GradientOrigin,
		RadiusX,
		RadiusY,
		GradientStopColor,
		GradientStopOffset,
	};

	struct BrushAccessor
	{
		size_t StopIndex = 0;
		cui::drawing::BrushKind BrushKind =
			cui::drawing::BrushKind::LinearGradient;
		BrushMember Member = BrushMember::GradientStopColor;
		std::wstring CanonicalPath;
	};

	struct BrushTransformAccessor
	{
		cui::drawing::BrushKind BrushKind =
			cui::drawing::BrushKind::LinearGradient;
		bool Relative = false;
		TransformAccessor Transform;
		std::wstring CanonicalPath;
	};

	/**
	 * Identifies an adapter from a Storyboard object-property path to the
	 * animatable value at its leaf. New object graphs extend this single
	 * boundary instead of adding parallel path fields throughout the runtime.
	 */
	using ObjectPathAccessor = std::variant<
		TransformAccessor, GeometryAccessor, PathGeometryAccessor,
		GeometryTransformAccessor, BrushAccessor, BrushTransformAccessor>;

	struct RuntimeAnimation
	{
		DeclarativeAnimationKind Kind = DeclarativeAnimationKind::Double;
		Control* Target = nullptr;
		const DependencyPropertyMetadata* Metadata = nullptr;
		std::wstring PropertyName;
		std::optional<ObjectPathAccessor> ObjectPath;
		std::optional<BindingValue> From;
		std::optional<BindingValue> To;
		std::optional<BindingValue> By;
		bool IsAdditive = false;
		bool IsCumulative = false;
		std::vector<DeclarativeAnimationKeyFrame> KeyFrames;
		unsigned long long BeginTimeMilliseconds = 0;
		unsigned long long DurationMilliseconds = 0;
		DeclarativeRepeatBehaviorKind RepeatBehavior =
			DeclarativeRepeatBehaviorKind::Count;
		double RepeatCount = 1.0;
		unsigned long long RepeatDurationMilliseconds = 0;
		bool AutoReverse = false;
		DeclarativeTimelineFillBehavior FillBehavior =
			DeclarativeTimelineFillBehavior::HoldEnd;
		double SpeedRatio = 1.0;
		double AccelerationRatio = 0.0;
		double DecelerationRatio = 0.0;
		DeclarativeEasingKind Easing = DeclarativeEasingKind::Linear;
		DeclarativeEasingMode EasingMode = DeclarativeEasingMode::EaseOut;
	};

	struct RuntimeState
	{
		std::wstring Name;
		std::vector<RuntimeCondition> Conditions;
		std::vector<std::wstring> EventNames;
		std::vector<RuntimeSetter> Setters;
		std::vector<RuntimeAnimation> Animations;
	};

	struct RuntimeTransition
	{
		std::optional<size_t> FromState;
		std::optional<size_t> ToState;
		unsigned long long GeneratedDurationMilliseconds = 0;
		DeclarativeEasingKind GeneratedEasing = DeclarativeEasingKind::Linear;
		DeclarativeEasingMode GeneratedEasingMode =
			DeclarativeEasingMode::EaseOut;
		std::vector<RuntimeAnimation> Animations;
	};

	struct RuntimeEventStoryboard
	{
		std::wstring Name;
		std::vector<RuntimeAnimation> Animations;
		bool IsStyleStoryboard = false;
	};

	struct RuntimeEventTriggerAction
	{
		DeclarativeStoryboardActionKind Kind =
			DeclarativeStoryboardActionKind::Begin;
		size_t StoryboardIndex = 0;
		std::wstring PendingStoryboardName;
	};

	struct RuntimeEventTrigger
	{
		std::wstring EventName;
		std::vector<RuntimeEventTriggerAction> Actions;
	};

	struct RuntimeStyleTriggerScope
	{
		DependencyPropertyValueSource Source = DependencyPropertyValueSource::Style;
		const ControlStyleSheet* Sheet = nullptr;
		size_t RuleId = 0;
		bool Active = false;
		std::vector<size_t> StoryboardIndices;
		std::vector<RuntimeEventTriggerAction> EnterActions;
		std::vector<RuntimeEventTriggerAction> ExitActions;
	};

	struct PropertyKey
	{
		Control* Target = nullptr;
		std::wstring PropertyName;
	};

	struct PendingTransition
	{
		size_t TargetState = 0;
		unsigned long long EndTick = 0;
		std::vector<PropertyKey> Properties;
	};

	struct RuntimeGroup
	{
		std::wstring Name;
		std::vector<RuntimeState> States;
		std::vector<RuntimeTransition> Transitions;
		size_t FallbackState = 0;
		std::optional<size_t> CurrentState;
		std::optional<PendingTransition> Pending;
		std::vector<std::wstring> ConditionProperties;
	};

	struct PropertySnapshot
	{
		PropertyKey Key;
		std::optional<BindingValue> Value;
		DependencyPropertyValueSource Source =
			DependencyPropertyValueSource::VisualState;
	};

	struct ActiveAnimation
	{
		size_t GroupIndex = 0;
		Control* Target = nullptr;
		const DependencyPropertyMetadata* Metadata = nullptr;
		std::wstring PropertyName;
		DeclarativeAnimationKind Kind = DeclarativeAnimationKind::Double;
		BindingValue Base;
		BindingValue Foundation;
		BindingValue From;
		BindingValue To;
		std::vector<DeclarativeAnimationKeyFrame> KeyFrames;
		bool IsCumulative = false;
		std::optional<ObjectPathAccessor> ObjectPath;
		unsigned long long StartTick = 0;
		unsigned long long BeginTimeMilliseconds = 0;
		unsigned long long DurationMilliseconds = 0;
		DeclarativeRepeatBehaviorKind RepeatBehavior =
			DeclarativeRepeatBehaviorKind::Count;
		double RepeatCount = 1.0;
		unsigned long long RepeatDurationMilliseconds = 0;
		bool AutoReverse = false;
		DeclarativeTimelineFillBehavior FillBehavior =
			DeclarativeTimelineFillBehavior::HoldEnd;
		double SpeedRatio = 1.0;
		double AccelerationRatio = 0.0;
		double DecelerationRatio = 0.0;
		DeclarativeEasingKind Easing = DeclarativeEasingKind::Linear;
		DeclarativeEasingMode EasingMode = DeclarativeEasingMode::EaseOut;
		bool IsEventStoryboard = false;
		bool Paused = false;
		unsigned long long PauseTick = 0;
		bool Completed = false;
	};

	static const TransformAccessor* AsTransformPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<TransformAccessor>(&*path) : nullptr;
	}

	static const GeometryAccessor* AsGeometryPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<GeometryAccessor>(&*path) : nullptr;
	}

	static const GeometryTransformAccessor* AsGeometryTransformPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<GeometryTransformAccessor>(&*path) : nullptr;
	}

	static const PathGeometryAccessor* AsPathGeometryPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<PathGeometryAccessor>(&*path) : nullptr;
	}

	static const BrushAccessor* AsBrushPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<BrushAccessor>(&*path) : nullptr;
	}

	static const BrushTransformAccessor* AsBrushTransformPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<BrushTransformAccessor>(&*path) : nullptr;
	}

	static std::wstring_view ObjectPathCanonical(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		if (const auto* transform = AsTransformPath(path))
			return transform->CanonicalPath;
		if (const auto* geometry = AsGeometryPath(path))
			return geometry->CanonicalPath;
		if (const auto* pathGeometry = AsPathGeometryPath(path))
			return pathGeometry->CanonicalPath;
		if (const auto* geometryTransform = AsGeometryTransformPath(path))
			return geometryTransform->CanonicalPath;
		if (const auto* brush = AsBrushPath(path))
			return brush->CanonicalPath;
		if (const auto* brushTransform = AsBrushTransformPath(path))
			return brushTransform->CanonicalPath;
		return {};
	}

	static bool ObjectPathUsesFloat(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		if (const auto* transform = AsTransformPath(path))
			return transform->Member != TransformMember::Matrix;
		if (const auto* transform = AsGeometryTransformPath(path))
			return transform->Transform.Member != TransformMember::Matrix;
		if (const auto* transform = AsBrushTransformPath(path))
			return transform->Transform.Member != TransformMember::Matrix;
		if (const auto* pathGeometry = AsPathGeometryPath(path))
			return pathGeometry->Member == PathGeometryMember::ArcRotationAngle;
		if (const auto* geometry = AsGeometryPath(path))
			return geometry->Member == GeometryMember::RadiusX
				|| geometry->Member == GeometryMember::RadiusY;
		const auto* brush = AsBrushPath(path);
		return brush && (brush->Member == BrushMember::Opacity
			|| brush->Member == BrushMember::RadiusX
			|| brush->Member == BrushMember::RadiusY
			|| brush->Member == BrushMember::GradientStopOffset);
	}

	Control* Owner = nullptr;
	std::vector<RuntimeGroup> Groups;
	std::vector<EventConnection> Connections;
	std::vector<ActiveAnimation> ActiveAnimations;
	std::vector<RuntimeEventStoryboard> EventStoryboards;
	std::vector<RuntimeEventTrigger> EventTriggers;
	std::vector<RuntimeStyleTriggerScope> StyleTriggerScopes;
	std::vector<size_t> FreeStyleStoryboardIndices;
	bool DeclarativeInteractionsDefined = false;
	bool Applying = false;

	~DeclarativeVisualStateRuntime()
	{
		Connections.clear();
		ActiveAnimations.clear();
		ClearAppliedValues();
	}

	static bool EqualName(
		std::wstring_view left,
		std::wstring_view right) noexcept
	{
		return left == right;
	}

	static bool EqualValueToken(
		std::wstring_view left,
		std::wstring_view right) noexcept
	{
		return left.size() == right.size()
			&& std::equal(left.begin(), left.end(), right.begin(),
				[](wchar_t l, wchar_t r)
				{ return std::towlower(l) == std::towlower(r); });
	}

	static bool SameProperty(
		const PropertyKey& left,
		const PropertyKey& right) noexcept
	{
		return left.Target == right.Target
			&& EqualName(left.PropertyName, right.PropertyName);
	}

	static bool ContainsName(
		const std::vector<std::wstring>& values,
		const std::wstring& value)
	{
		return std::any_of(values.begin(), values.end(),
			[&](const auto& existing) { return EqualName(existing, value); });
	}

	static bool IsNumericKind(BindingValueKind kind) noexcept
	{
		return kind == BindingValueKind::Int
			|| kind == BindingValueKind::Int64
			|| kind == BindingValueKind::Float
			|| kind == BindingValueKind::Double;
	}

	static bool AnimationMatchesMetadata(
		DeclarativeAnimationKind kind,
		const DependencyPropertyMetadata& metadata) noexcept
	{
		if (kind == DeclarativeAnimationKind::Object) return true;
		if (kind == DeclarativeAnimationKind::Double)
			return IsNumericKind(metadata.ValueKind());
		if (kind == DeclarativeAnimationKind::Thickness)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType() == std::type_index(typeid(Thickness));
		if (kind == DeclarativeAnimationKind::Point)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType() == std::type_index(typeid(cui::core::Point));
		if (kind == DeclarativeAnimationKind::Vector)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType() == std::type_index(typeid(cui::core::Vector));
		if (kind == DeclarativeAnimationKind::Rect)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType() == std::type_index(typeid(cui::core::Rect));
		if (kind == DeclarativeAnimationKind::Size)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType() == std::type_index(typeid(cui::core::Size));
		if (kind == DeclarativeAnimationKind::Matrix)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType()
					== std::type_index(typeid(D2D1_MATRIX_3X2_F));
		return metadata.ValueKind() == BindingValueKind::Object
			&& metadata.ValueType() == std::type_index(typeid(D2D1_COLOR_F));
	}

	static std::optional<DeclarativeAnimationKind> GeneratedAnimationKind(
		const DependencyPropertyMetadata& metadata) noexcept
	{
		if (IsNumericKind(metadata.ValueKind()))
			return DeclarativeAnimationKind::Double;
		if (metadata.ValueKind() != BindingValueKind::Object)
			return std::nullopt;
		const auto type = metadata.ValueType();
		if (type == std::type_index(typeid(D2D1_COLOR_F)))
			return DeclarativeAnimationKind::Color;
		if (type == std::type_index(typeid(Thickness)))
			return DeclarativeAnimationKind::Thickness;
		if (type == std::type_index(typeid(cui::core::Point)))
			return DeclarativeAnimationKind::Point;
		if (type == std::type_index(typeid(cui::core::Vector)))
			return DeclarativeAnimationKind::Vector;
		if (type == std::type_index(typeid(cui::core::Rect)))
			return DeclarativeAnimationKind::Rect;
		if (type == std::type_index(typeid(cui::core::Size)))
			return DeclarativeAnimationKind::Size;
		if (type == std::type_index(typeid(D2D1_MATRIX_3X2_F)))
			return DeclarativeAnimationKind::Matrix;
		return std::nullopt;
	}

	static std::wstring_view LocalTypeName(std::wstring_view value) noexcept
	{
		const auto separator = value.rfind(L':');
		return separator == std::wstring_view::npos
			? value : value.substr(separator + 1);
	}

	static bool TryResolveTransformOperationAccessor(
		const cui::drawing::Transform& transform,
		size_t operationIndex,
		std::wstring_view owner,
		std::wstring_view property,
		std::wstring canonicalPrefix,
		TransformAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		if (operationIndex >= transform.Operations.size())
			return fail(L"动画目标没有路径所需的 Transform 操作。");
		const auto& operation = transform.Operations[operationIndex];
		auto assign = [&](TransformMember member,
			std::wstring_view canonicalOwner,
			std::wstring_view canonicalProperty)
		{
			output.OperationIndex = operationIndex;
			output.OperationKind = operation.Kind;
			output.Member = member;
			output.CanonicalPath = std::move(canonicalPrefix) + L"["
				+ std::to_wstring(operationIndex) + L"].("
				+ std::wstring(canonicalOwner) + L"."
				+ std::wstring(canonicalProperty) + L")";
			if (outError) outError->clear();
			return true;
		};
		switch (operation.Kind)
		{
		case cui::drawing::TransformKind::Translate:
			if (!EqualName(owner, L"TranslateTransform")) break;
			if (EqualName(property, L"X")) return assign(
				TransformMember::X, L"TranslateTransform", L"X");
			if (EqualName(property, L"Y")) return assign(
				TransformMember::Y, L"TranslateTransform", L"Y");
			break;
		case cui::drawing::TransformKind::Scale:
			if (!EqualName(owner, L"ScaleTransform")) break;
			if (EqualName(property, L"ScaleX")) return assign(
				TransformMember::ScaleX, L"ScaleTransform", L"ScaleX");
			if (EqualName(property, L"ScaleY")) return assign(
				TransformMember::ScaleY, L"ScaleTransform", L"ScaleY");
			if (EqualName(property, L"CenterX")) return assign(
				TransformMember::CenterX, L"ScaleTransform", L"CenterX");
			if (EqualName(property, L"CenterY")) return assign(
				TransformMember::CenterY, L"ScaleTransform", L"CenterY");
			break;
		case cui::drawing::TransformKind::Rotate:
			if (!EqualName(owner, L"RotateTransform")) break;
			if (EqualName(property, L"Angle")) return assign(
				TransformMember::Angle, L"RotateTransform", L"Angle");
			if (EqualName(property, L"CenterX")) return assign(
				TransformMember::CenterX, L"RotateTransform", L"CenterX");
			if (EqualName(property, L"CenterY")) return assign(
				TransformMember::CenterY, L"RotateTransform", L"CenterY");
			break;
		case cui::drawing::TransformKind::Skew:
			if (!EqualName(owner, L"SkewTransform")) break;
			if (EqualName(property, L"AngleX")) return assign(
				TransformMember::AngleX, L"SkewTransform", L"AngleX");
			if (EqualName(property, L"AngleY")) return assign(
				TransformMember::AngleY, L"SkewTransform", L"AngleY");
			if (EqualName(property, L"CenterX")) return assign(
				TransformMember::CenterX, L"SkewTransform", L"CenterX");
			if (EqualName(property, L"CenterY")) return assign(
				TransformMember::CenterY, L"SkewTransform", L"CenterY");
			break;
		case cui::drawing::TransformKind::Matrix:
			if (EqualName(owner, L"MatrixTransform")
				&& EqualName(property, L"Matrix"))
				return assign(TransformMember::Matrix,
					L"MatrixTransform", L"Matrix");
			break;
		default:
			break;
		}
		return fail(L"动画路径的 Transform 类型或末端属性与实际操作不匹配"
			L"（operationKind="
			+ std::to_wstring(static_cast<int>(operation.Kind))
			+ L"，owner=" + std::wstring(owner)
			+ L"，property=" + std::wstring(property) + L"）。");
	}

	static bool TryResolveTransformPath(
		Control& target,
		const std::wstring& text,
		const DependencyPropertyMetadata*& outMetadata,
		TransformAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		if (path.Segments.size() != 4
			|| path.Segments[0].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[1].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[2].Kind
				!= cui::xaml::PropertyPathSegmentKind::Index
			|| path.Segments[3].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| !EqualName(path.Segments[0].Name, L"RenderTransform")
			|| (!EqualName(LocalTypeName(path.Segments[0].OwnerType), L"Control")
				&& !EqualName(LocalTypeName(path.Segments[0].OwnerType), L"UIElement"))
			|| !EqualName(LocalTypeName(path.Segments[1].OwnerType), L"TransformGroup")
			|| !EqualName(path.Segments[1].Name, L"Children"))
			return fail(L"首批复合动画路径必须是 "
				L"(Control.RenderTransform).(TransformGroup.Children)[n]."
				L"(TransformType.Property)。");

		const auto* metadata = target.FindPropertyMetadata(L"RenderTransform");
		BindingValue current;
		cui::drawing::Transform transform;
		if (!metadata || !metadata->CanWrite()
			|| metadata->ValueType()
				!= std::type_index(typeid(cui::drawing::Transform))
			|| !metadata->TryGet(target, current)
			|| !current.TryGet(transform)
			|| path.Segments[2].Index >= transform.Operations.size())
			return fail(L"动画目标没有路径所需的 RenderTransform 操作。");

		const auto owner = LocalTypeName(path.Segments[3].OwnerType);
		const auto& property = path.Segments[3].Name;
		if (!TryResolveTransformOperationAccessor(transform,
			path.Segments[2].Index, owner, property,
			L"(Control.RenderTransform).(TransformGroup.Children)",
			output, outError)) return false;
		outMetadata = metadata;
		return true;
	}

	static const cui::drawing::Geometry* TryGetGeometryChild(
		const cui::drawing::Geometry& root,
		const std::vector<size_t>& childIndices) noexcept
	{
		const auto* current = &root;
		for (const auto index : childIndices)
		{
			if (current->Kind != cui::drawing::GeometryKind::Group
				|| index >= current->Children.size()) return nullptr;
			current = &current->Children[index];
		}
		return current;
	}

	static cui::drawing::Geometry* TryGetGeometryChild(
		cui::drawing::Geometry& root,
		const std::vector<size_t>& childIndices) noexcept
	{
		auto* current = &root;
		for (const auto index : childIndices)
		{
			if (current->Kind != cui::drawing::GeometryKind::Group
				|| index >= current->Children.size()) return nullptr;
			current = &current->Children[index];
		}
		return current;
	}

	/**
	 * Resolves zero or more GeometryGroup.Children[index] hops after
	 * Control.Clip and returns the concrete Geometry that owns the leaf.
	 */
	static bool TryResolveGeometryTreeTarget(
		Control& target,
		const cui::xaml::PropertyPath& path,
		const DependencyPropertyMetadata*& outMetadata,
		const cui::drawing::Geometry*& outGeometry,
		std::vector<size_t>& outChildIndices,
		size_t& outLeafStart,
		std::wstring& outCanonicalPrefix,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		if (path.Segments.empty()
			|| path.Segments[0].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| !EqualName(path.Segments[0].Name, L"Clip")
			|| (!EqualName(LocalTypeName(path.Segments[0].OwnerType), L"Control")
				&& !EqualName(LocalTypeName(path.Segments[0].OwnerType), L"UIElement")))
			return fail(L"Geometry 复合动画路径必须以 (Control.Clip) 开始。");

		const auto* metadata = target.FindPropertyMetadata(L"Clip");
		const auto& currentClip = target.GetClip();
		if (!metadata || !metadata->CanWrite()
			|| metadata->ValueType()
				!= std::type_index(typeid(cui::drawing::Geometry))
			|| !currentClip)
			return fail(L"动画目标必须显式持有 Control.Clip Geometry。");

		outChildIndices.clear();
		outCanonicalPrefix = L"(Control.Clip)";
		const auto* geometry = &*currentClip;
		size_t cursor = 1;
		while (cursor < path.Segments.size()
			&& path.Segments[cursor].Kind
				== cui::xaml::PropertyPathSegmentKind::Property
			&& EqualName(
				LocalTypeName(path.Segments[cursor].OwnerType), L"GeometryGroup")
			&& EqualName(path.Segments[cursor].Name, L"Children"))
		{
			if (cursor + 1 >= path.Segments.size()
				|| path.Segments[cursor + 1].Kind
					!= cui::xaml::PropertyPathSegmentKind::Index)
				return fail(L"GeometryGroup.Children 后必须提供有效索引。");
			const auto index = path.Segments[cursor + 1].Index;
			if (geometry->Kind != cui::drawing::GeometryKind::Group)
				return fail(L"GeometryGroup.Children 路径所有者与实际 Geometry 类型不匹配。");
			if (index >= geometry->Children.size())
				return fail(L"GeometryGroup.Children 动画索引超出范围。");
			outChildIndices.push_back(index);
			geometry = &geometry->Children[index];
			outCanonicalPrefix += L".(GeometryGroup.Children)["
				+ std::to_wstring(index) + L"]";
			cursor += 2;
		}
		if (cursor >= path.Segments.size())
			return fail(L"GeometryGroup.Children 索引后缺少动画末端属性。");
		outMetadata = metadata;
		outGeometry = geometry;
		outLeafStart = cursor;
		return true;
	}

	static bool TryResolveGeometryPath(
		Control& target,
		const std::wstring& text,
		const DependencyPropertyMetadata*& outMetadata,
		GeometryAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const DependencyPropertyMetadata* metadata = nullptr;
		const cui::drawing::Geometry* resolvedGeometry = nullptr;
		std::vector<size_t> childIndices;
		size_t leafStart = 0;
		std::wstring canonicalPrefix;
		if (!TryResolveGeometryTreeTarget(target, path, metadata,
			resolvedGeometry, childIndices, leafStart, canonicalPrefix, outError))
			return false;
		if (path.Segments.size() != leafStart + 1
			|| path.Segments[leafStart].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property)
			return fail(L"Geometry 动画路径必须在目标 Geometry 后定位一个公开成员。");
		const auto owner = LocalTypeName(path.Segments[leafStart].OwnerType);
		const auto& property = path.Segments[leafStart].Name;
		const auto& geometry = *resolvedGeometry;
		output = {};
		output.ChildIndices = std::move(childIndices);
		output.GeometryKind = geometry.Kind;
		auto assign = [&](GeometryMember member, std::wstring_view type,
			std::wstring_view canonicalProperty)
		{
			output.Member = member;
			output.CanonicalPath = canonicalPrefix + L".(" + std::wstring(type)
				+ L"." + std::wstring(canonicalProperty) + L")";
			outMetadata = metadata;
			if (outError) outError->clear();
			return true;
		};
		auto validRadius = [](float value)
		{
			return std::isfinite(value) && value >= 0.0f;
		};
		if (geometry.Kind == cui::drawing::GeometryKind::Rectangle
			&& EqualName(owner, L"RectangleGeometry"))
		{
			if (EqualName(property, L"Rect"))
			{
				const auto rect = ToCoreRect(geometry.Rect);
				if (!std::isfinite(rect.x) || !std::isfinite(rect.y)
					|| !std::isfinite(rect.width) || !std::isfinite(rect.height)
					|| rect.width < 0.0f || rect.height < 0.0f)
					return fail(L"动画目标的 RectangleGeometry.Rect 无效。");
				return assign(GeometryMember::Rect,
					L"RectangleGeometry", L"Rect");
			}
			if (EqualName(property, L"RadiusX")
				|| EqualName(property, L"RadiusY"))
			{
				const bool x = EqualName(property, L"RadiusX");
				if (!validRadius(x ? geometry.RadiusX : geometry.RadiusY))
					return fail(L"动画目标的 RectangleGeometry 圆角半径无效。");
				return assign(x ? GeometryMember::RadiusX : GeometryMember::RadiusY,
					L"RectangleGeometry", x ? L"RadiusX" : L"RadiusY");
			}
		}
		if (geometry.Kind == cui::drawing::GeometryKind::Ellipse
			&& EqualName(owner, L"EllipseGeometry"))
		{
			if (EqualName(property, L"Center"))
			{
				if (!std::isfinite(geometry.Center.x)
					|| !std::isfinite(geometry.Center.y))
					return fail(L"动画目标的 EllipseGeometry.Center 无效。");
				return assign(GeometryMember::Center,
					L"EllipseGeometry", L"Center");
			}
			if (EqualName(property, L"RadiusX")
				|| EqualName(property, L"RadiusY"))
			{
				const bool x = EqualName(property, L"RadiusX");
				if (!validRadius(x ? geometry.RadiusX : geometry.RadiusY))
					return fail(L"动画目标的 EllipseGeometry 半径无效。");
				return assign(x ? GeometryMember::RadiusX : GeometryMember::RadiusY,
					L"EllipseGeometry", x ? L"RadiusX" : L"RadiusY");
			}
		}
		if ((geometry.Kind == cui::drawing::GeometryKind::Path
				&& EqualName(owner, L"PathGeometry")
				|| geometry.Kind == cui::drawing::GeometryKind::Group
					&& EqualName(owner, L"GeometryGroup"))
			&& EqualName(property, L"FillRule"))
			return assign(GeometryMember::FillRule, owner, L"FillRule");
		return fail(L"Geometry 动画路径所有者或末端属性与实际 Clip 类型不匹配。");
	}

	static bool TryResolvePathGeometryPath(
		Control& target,
		const std::wstring& text,
		const DependencyPropertyMetadata*& outMetadata,
		PathGeometryAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const DependencyPropertyMetadata* metadata = nullptr;
		const cui::drawing::Geometry* resolvedGeometry = nullptr;
		std::vector<size_t> childIndices;
		size_t leafStart = 0;
		std::wstring canonicalPrefix;
		if (!TryResolveGeometryTreeTarget(target, path, metadata,
			resolvedGeometry, childIndices, leafStart, canonicalPrefix, outError))
			return false;
		const auto remaining = path.Segments.size() - leafStart;
		const bool figurePath = remaining == 3;
		const bool segmentPath = remaining == 5;
		if ((!figurePath && !segmentPath)
			|| path.Segments[leafStart].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[leafStart + 1].Kind
				!= cui::xaml::PropertyPathSegmentKind::Index
			|| path.Segments[leafStart + 2].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| (segmentPath
				&& (path.Segments[leafStart + 3].Kind
						!= cui::xaml::PropertyPathSegmentKind::Index
					|| path.Segments[leafStart + 4].Kind
						!= cui::xaml::PropertyPathSegmentKind::Property))
			|| !EqualName(
				LocalTypeName(path.Segments[leafStart].OwnerType), L"PathGeometry")
			|| !EqualName(path.Segments[leafStart].Name, L"Figures")
			|| !EqualName(
				LocalTypeName(path.Segments[leafStart + 2].OwnerType), L"PathFigure")
			|| (segmentPath
				&& !EqualName(path.Segments[leafStart + 2].Name, L"Segments")))
			return fail(L"PathGeometry 动画路径必须定位 Figures[n] 的 PathFigure "
				L"成员，或继续定位 Segments[n] 的具体 PathSegment 成员。");
		if (resolvedGeometry->Kind != cui::drawing::GeometryKind::Path)
			return fail(L"动画目标必须显式持有 PathGeometry 类型的 Control.Clip。");
		const auto figureIndex = path.Segments[leafStart + 1].Index;
		if (figureIndex >= resolvedGeometry->Figures.size())
			return fail(L"PathGeometry.Figures 动画索引超出范围。");
		const auto& figure = resolvedGeometry->Figures[figureIndex];
		output = {};
		output.ChildIndices = std::move(childIndices);
		output.FigureIndex = figureIndex;
		auto finitePoint = [](D2D1_POINT_2F point)
		{
			return std::isfinite(point.x) && std::isfinite(point.y);
		};
		auto assignFigure = [&](PathGeometryMember member,
			std::wstring_view property)
		{
			output.Member = member;
			output.CanonicalPath = canonicalPrefix + L".(PathGeometry.Figures)["
				+ std::to_wstring(figureIndex) + L"].(PathFigure."
				+ std::wstring(property) + L")";
			outMetadata = metadata;
			if (outError) outError->clear();
			return true;
		};
		if (figurePath)
		{
			const auto& property = path.Segments[leafStart + 2].Name;
			if (EqualName(property, L"StartPoint"))
			{
				if (!finitePoint(figure.StartPoint))
					return fail(L"动画目标的 PathFigure.StartPoint 无效。");
				return assignFigure(
					PathGeometryMember::FigureStartPoint, L"StartPoint");
			}
			if (EqualName(property, L"IsClosed"))
				return assignFigure(
					PathGeometryMember::FigureIsClosed, L"IsClosed");
			if (EqualName(property, L"IsFilled"))
				return assignFigure(
					PathGeometryMember::FigureIsFilled, L"IsFilled");
			return fail(L"尚未支持该 PathFigure 动画成员。");
		}

		const auto segmentIndex = path.Segments[leafStart + 3].Index;
		if (segmentIndex >= figure.Segments.size())
			return fail(L"PathFigure.Segments 动画索引超出范围。");
		const auto& segment = figure.Segments[segmentIndex];
		const auto owner = LocalTypeName(path.Segments[leafStart + 4].OwnerType);
		const auto& property = path.Segments[leafStart + 4].Name;
		output.HasSegment = true;
		output.SegmentIndex = segmentIndex;
		output.SegmentKind = segment.Kind;
		auto assignSegment = [&](PathGeometryMember member,
			std::wstring_view objectType, std::wstring_view canonicalProperty)
		{
			output.Member = member;
			output.CanonicalPath = canonicalPrefix + L".(PathGeometry.Figures)["
				+ std::to_wstring(figureIndex)
				+ L"].(PathFigure.Segments)["
				+ std::to_wstring(segmentIndex) + L"].("
				+ std::wstring(objectType) + L"."
				+ std::wstring(canonicalProperty) + L")";
			outMetadata = metadata;
			if (outError) outError->clear();
			return true;
		};
		auto pointMember = [&](PathGeometryMember member,
			std::wstring_view type, std::wstring_view name,
			D2D1_POINT_2F point)
		{
			if (!finitePoint(point))
				return fail(L"动画目标的 PathSegment Point 无效。");
			return assignSegment(member, type, name);
		};
		switch (segment.Kind)
		{
		case cui::drawing::PathSegmentKind::Line:
			if (EqualName(owner, L"LineSegment")
				&& EqualName(property, L"Point"))
				return pointMember(PathGeometryMember::SegmentPoint,
					L"LineSegment", L"Point", segment.Point);
			break;
		case cui::drawing::PathSegmentKind::Bezier:
			if (EqualName(owner, L"BezierSegment"))
			{
				if (EqualName(property, L"Point1")) return pointMember(
					PathGeometryMember::SegmentPoint1,
					L"BezierSegment", L"Point1", segment.Point1);
				if (EqualName(property, L"Point2")) return pointMember(
					PathGeometryMember::SegmentPoint2,
					L"BezierSegment", L"Point2", segment.Point2);
				if (EqualName(property, L"Point3")) return pointMember(
					PathGeometryMember::SegmentPoint3,
					L"BezierSegment", L"Point3", segment.Point3);
			}
			break;
		case cui::drawing::PathSegmentKind::QuadraticBezier:
			if (EqualName(owner, L"QuadraticBezierSegment"))
			{
				if (EqualName(property, L"Point1")) return pointMember(
					PathGeometryMember::SegmentPoint1,
					L"QuadraticBezierSegment", L"Point1", segment.Point1);
				if (EqualName(property, L"Point2")) return pointMember(
					PathGeometryMember::SegmentPoint2,
					L"QuadraticBezierSegment", L"Point2", segment.Point2);
			}
			break;
		case cui::drawing::PathSegmentKind::Arc:
			if (EqualName(owner, L"ArcSegment"))
			{
				if (EqualName(property, L"Point")) return pointMember(
					PathGeometryMember::SegmentPoint,
					L"ArcSegment", L"Point", segment.Point);
				if (EqualName(property, L"Size"))
				{
					if (!std::isfinite(segment.Size.width)
						|| !std::isfinite(segment.Size.height)
						|| segment.Size.width < 0.0f
						|| segment.Size.height < 0.0f)
						return fail(L"动画目标的 ArcSegment.Size 无效。");
					return assignSegment(PathGeometryMember::ArcSize,
						L"ArcSegment", L"Size");
				}
				if (EqualName(property, L"RotationAngle"))
				{
					if (!std::isfinite(segment.RotationAngle))
						return fail(L"动画目标的 ArcSegment.RotationAngle 无效。");
					return assignSegment(PathGeometryMember::ArcRotationAngle,
						L"ArcSegment", L"RotationAngle");
				}
				if (EqualName(property, L"IsLargeArc"))
					return assignSegment(PathGeometryMember::ArcIsLargeArc,
						L"ArcSegment", L"IsLargeArc");
				if (EqualName(property, L"SweepDirection"))
					return assignSegment(PathGeometryMember::ArcSweepDirection,
						L"ArcSegment", L"SweepDirection");
			}
			break;
		default:
			break;
		}
		return fail(L"PathSegment 动画路径所有者或末端属性与实际段类型不匹配。");
	}

	static bool TryResolveGeometryTransformPath(
		Control& target,
		const std::wstring& text,
		const DependencyPropertyMetadata*& outMetadata,
		GeometryTransformAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const DependencyPropertyMetadata* metadata = nullptr;
		const cui::drawing::Geometry* resolvedGeometry = nullptr;
		std::vector<size_t> childIndices;
		size_t leafStart = 0;
		std::wstring canonicalPrefix;
		if (!TryResolveGeometryTreeTarget(target, path, metadata,
			resolvedGeometry, childIndices, leafStart, canonicalPrefix, outError))
			return false;
		const auto geometryOwner = LocalTypeName(
			path.Segments[leafStart].OwnerType);
		if (path.Segments.size() != leafStart + 4
			|| path.Segments[leafStart].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[leafStart + 1].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[leafStart + 2].Kind
				!= cui::xaml::PropertyPathSegmentKind::Index
			|| path.Segments[leafStart + 3].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| (!EqualName(geometryOwner, L"Geometry")
				&& !EqualName(geometryOwner, L"RectangleGeometry")
				&& !EqualName(geometryOwner, L"EllipseGeometry")
				&& !EqualName(geometryOwner, L"PathGeometry")
				&& !EqualName(geometryOwner, L"GeometryGroup"))
			|| !EqualName(path.Segments[leafStart].Name, L"Transform")
			|| !EqualName(
				LocalTypeName(path.Segments[leafStart + 1].OwnerType), L"TransformGroup")
			|| !EqualName(path.Segments[leafStart + 1].Name, L"Children"))
			return fail(L"Geometry Transform 动画路径必须是 "
				L"(Control.Clip)...(Geometry.Transform)."
				L"(TransformGroup.Children)[n].(TransformType.Property)。");
		auto ownerMatches = [&]()
		{
			if (EqualName(geometryOwner, L"Geometry")) return true;
			switch (resolvedGeometry->Kind)
			{
			case cui::drawing::GeometryKind::Rectangle:
				return EqualName(geometryOwner, L"RectangleGeometry");
			case cui::drawing::GeometryKind::Ellipse:
				return EqualName(geometryOwner, L"EllipseGeometry");
			case cui::drawing::GeometryKind::Path:
				return EqualName(geometryOwner, L"PathGeometry");
			case cui::drawing::GeometryKind::Group:
				return EqualName(geometryOwner, L"GeometryGroup");
			default:
				return false;
			}
		};
		if (!ownerMatches())
			return fail(L"Geometry.Transform 路径所有者与实际 Geometry 类型不匹配。");
		if (!resolvedGeometry->LocalTransform)
			return fail(L"动画目标没有路径所需的 Geometry.Transform。");

		TransformAccessor transformAccessor;
		if (!TryResolveTransformOperationAccessor(
			*resolvedGeometry->LocalTransform, path.Segments[leafStart + 2].Index,
			LocalTypeName(path.Segments[leafStart + 3].OwnerType),
			path.Segments[leafStart + 3].Name,
			canonicalPrefix
				+ L".(Geometry.Transform).(TransformGroup.Children)",
			transformAccessor, outError)) return false;
		output.ChildIndices = std::move(childIndices);
		output.GeometryKind = resolvedGeometry->Kind;
		output.Transform = std::move(transformAccessor);
		output.CanonicalPath = output.Transform.CanonicalPath;
		outMetadata = metadata;
		if (outError) outError->clear();
		return true;
	}

	static bool TryResolveBrushPath(
		Control& target,
		const std::wstring& text,
		const DependencyPropertyMetadata*& outMetadata,
		BrushAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const auto brushOwner = path.Segments.size() > 1
			? LocalTypeName(path.Segments[1].OwnerType) : std::wstring_view{};
		if (path.Segments.size() < 2
			|| path.Segments[0].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[1].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| (!EqualName(LocalTypeName(path.Segments[0].OwnerType), L"Control")
				&& !EqualName(LocalTypeName(path.Segments[0].OwnerType), L"UIElement")))
			return fail(L"Brush 复合动画路径必须以 "
				L"(Control.BrushProperty).(BrushProperty) 开始。");
		const auto& rootProperty = path.Segments[0].Name;
		const auto* metadata = target.FindPropertyMetadata(rootProperty);
		BindingValue currentValue;
		cui::drawing::Brush currentBrushValue;
		if (!metadata || !metadata->CanWrite()
			|| metadata->ValueType()
				!= std::type_index(typeid(cui::drawing::Brush))
			|| !metadata->TryGet(target, currentValue)
			|| !currentValue.TryGet(currentBrushValue))
			return fail(L"动画目标必须持有 Control." + rootProperty + L" 画刷。");
		if (currentBrushValue.Kind == cui::drawing::BrushKind::None
			&& EqualName(rootProperty, L"Foreground"))
		{
			// Foreground has a WPF-style effective theme brush even when no
			// local/inherited Brush object is stored. Resolve the object path
			// against that effective value without manufacturing a Local or
			// Inherited dependency-property source.
			currentBrushValue = cui::drawing::MakeSolidColorBrush(
				target.RendererForegroundColor);
		}
		const auto* currentBrush = &currentBrushValue;

		auto ownerMatches = [&](std::wstring_view owner)
		{
			if (EqualName(owner, L"Brush")) return true;
			switch (currentBrush->Kind)
			{
			case cui::drawing::BrushKind::Solid:
				return EqualName(owner, L"SolidColorBrush");
			case cui::drawing::BrushKind::LinearGradient:
				return EqualName(owner, L"GradientBrush")
					|| EqualName(owner, L"LinearGradientBrush");
			case cui::drawing::BrushKind::RadialGradient:
				return EqualName(owner, L"GradientBrush")
					|| EqualName(owner, L"RadialGradientBrush");
			case cui::drawing::BrushKind::Image:
				return EqualName(owner, L"ImageBrush");
			default:
				return false;
			}
		};
		if (!ownerMatches(brushOwner))
			return fail(L"Brush 动画路径所有者与实际画刷类型不匹配。");

		output = {};
		output.BrushKind = currentBrush->Kind;
		if (path.Segments.size() == 2)
		{
			const auto& property = path.Segments[1].Name;
			auto assign = [&](BrushMember member, std::wstring_view owner,
				std::wstring_view canonicalProperty)
			{
				output.Member = member;
				output.CanonicalPath = L"(Control." + rootProperty + L").("
					+ std::wstring(owner) + L"."
					+ std::wstring(canonicalProperty) + L")";
				outMetadata = metadata;
				if (outError) outError->clear();
				return true;
			};
			if (EqualName(property, L"Opacity"))
			{
				if (!std::isfinite(currentBrush->Opacity)
					|| currentBrush->Opacity < 0.0f || currentBrush->Opacity > 1.0f)
					return fail(L"动画目标的 Brush.Opacity 无效。");
				return assign(BrushMember::Opacity, L"Brush", L"Opacity");
			}
			if (currentBrush->Kind == cui::drawing::BrushKind::Solid
				&& EqualName(brushOwner, L"SolidColorBrush")
				&& EqualName(property, L"Color"))
			{
				const auto& color = currentBrush->Color;
				if (!std::isfinite(color.r) || !std::isfinite(color.g)
					|| !std::isfinite(color.b) || !std::isfinite(color.a))
					return fail(L"动画目标的 SolidColorBrush.Color 无效。");
				return assign(BrushMember::SolidColor,
					L"SolidColorBrush", L"Color");
			}
			auto finitePoint = [](D2D1_POINT_2F point)
			{
				return std::isfinite(point.x) && std::isfinite(point.y);
			};
			if (currentBrush->Kind == cui::drawing::BrushKind::LinearGradient
				&& EqualName(brushOwner, L"LinearGradientBrush"))
			{
				if (EqualName(property, L"StartPoint"))
				{
					if (!finitePoint(currentBrush->StartPoint))
						return fail(L"动画目标的 LinearGradientBrush.StartPoint 无效。");
					return assign(BrushMember::StartPoint,
						L"LinearGradientBrush", L"StartPoint");
				}
				if (EqualName(property, L"EndPoint"))
				{
					if (!finitePoint(currentBrush->EndPoint))
						return fail(L"动画目标的 LinearGradientBrush.EndPoint 无效。");
					return assign(BrushMember::EndPoint,
						L"LinearGradientBrush", L"EndPoint");
				}
			}
			if (currentBrush->Kind == cui::drawing::BrushKind::RadialGradient
				&& EqualName(brushOwner, L"RadialGradientBrush"))
			{
				if (EqualName(property, L"Center"))
				{
					if (!finitePoint(currentBrush->Center))
						return fail(L"动画目标的 RadialGradientBrush.Center 无效。");
					return assign(BrushMember::Center,
						L"RadialGradientBrush", L"Center");
				}
				if (EqualName(property, L"GradientOrigin"))
				{
					if (!finitePoint(currentBrush->GradientOrigin))
						return fail(L"动画目标的 RadialGradientBrush.GradientOrigin 无效。");
					return assign(BrushMember::GradientOrigin,
						L"RadialGradientBrush", L"GradientOrigin");
				}
				if (EqualName(property, L"RadiusX")
					|| EqualName(property, L"RadiusY"))
				{
					const bool x = EqualName(property, L"RadiusX");
					const auto radius = x ? currentBrush->RadiusX : currentBrush->RadiusY;
					if (!std::isfinite(radius) || radius < 0.0f)
						return fail(L"动画目标的 RadialGradientBrush 半径无效。");
					return assign(x ? BrushMember::RadiusX : BrushMember::RadiusY,
						L"RadialGradientBrush", x ? L"RadiusX" : L"RadiusY");
				}
			}
			return fail(L"Brush 动画路径末端属性与实际画刷类型不匹配。");
		}

		if (path.Segments.size() != 4
			|| path.Segments[2].Kind != cui::xaml::PropertyPathSegmentKind::Index
			|| path.Segments[3].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| (!EqualName(brushOwner, L"GradientBrush")
				&& !EqualName(brushOwner, L"LinearGradientBrush")
				&& !EqualName(brushOwner, L"RadialGradientBrush"))
			|| !EqualName(path.Segments[1].Name, L"GradientStops")
			|| !EqualName(LocalTypeName(path.Segments[3].OwnerType), L"GradientStop")
			|| (!EqualName(path.Segments[3].Name, L"Color")
				&& !EqualName(path.Segments[3].Name, L"Offset"))
			|| (currentBrush->Kind != cui::drawing::BrushKind::LinearGradient
				&& currentBrush->Kind != cui::drawing::BrushKind::RadialGradient)
			|| path.Segments[2].Index >= currentBrush->GradientStops.size())
			return fail(L"GradientStop 复合动画路径必须是 "
				L"(Control.BrushProperty).(GradientBrush.GradientStops)[n]."
				L"(GradientStop.Color|Offset)。");
		const auto& stop = currentBrush->GradientStops[path.Segments[2].Index];
		if (!std::isfinite(stop.Offset) || stop.Offset < 0.0f || stop.Offset > 1.0f
			|| !std::isfinite(stop.Color.r) || !std::isfinite(stop.Color.g)
			|| !std::isfinite(stop.Color.b) || !std::isfinite(stop.Color.a))
			return fail(L"动画目标的 GradientStop 值无效。");

		output.StopIndex = path.Segments[2].Index;
		output.Member = EqualName(path.Segments[3].Name, L"Color")
			? BrushMember::GradientStopColor : BrushMember::GradientStopOffset;
		output.CanonicalPath = L"(Control." + rootProperty + L")."
			L"(GradientBrush.GradientStops)["
			+ std::to_wstring(output.StopIndex) + L"].(GradientStop."
			+ (output.Member == BrushMember::GradientStopColor
				? std::wstring(L"Color") : std::wstring(L"Offset")) + L")";
		outMetadata = metadata;
		if (outError) outError->clear();
		return true;
	}

	static bool TryResolveBrushTransformPath(
		Control& target,
		const std::wstring& text,
		const DependencyPropertyMetadata*& outMetadata,
		BrushTransformAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const auto brushOwner = path.Segments.size() > 1
			? LocalTypeName(path.Segments[1].OwnerType) : std::wstring_view{};
		if (path.Segments.size() != 5
			|| path.Segments[0].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[1].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[2].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[3].Kind != cui::xaml::PropertyPathSegmentKind::Index
			|| path.Segments[4].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| (!EqualName(LocalTypeName(path.Segments[0].OwnerType), L"Control")
				&& !EqualName(LocalTypeName(path.Segments[0].OwnerType), L"UIElement"))
			|| (!EqualName(brushOwner, L"Brush")
				&& !EqualName(brushOwner, L"SolidColorBrush")
				&& !EqualName(brushOwner, L"GradientBrush")
				&& !EqualName(brushOwner, L"LinearGradientBrush")
				&& !EqualName(brushOwner, L"RadialGradientBrush")
				&& !EqualName(brushOwner, L"ImageBrush"))
			|| (!EqualName(path.Segments[1].Name, L"Transform")
				&& !EqualName(path.Segments[1].Name, L"RelativeTransform"))
			|| !EqualName(LocalTypeName(path.Segments[2].OwnerType), L"TransformGroup")
			|| !EqualName(path.Segments[2].Name, L"Children"))
			return fail(L"Brush Transform 动画路径必须是 "
				L"(Control.BrushProperty).(Brush.Transform|RelativeTransform)."
				L"(TransformGroup.Children)[n].(TransformType.Property)。");

		const auto& rootProperty = path.Segments[0].Name;
		const auto* metadata = target.FindPropertyMetadata(rootProperty);
		BindingValue currentValue;
		cui::drawing::Brush currentBrushValue;
		if (!metadata || !metadata->CanWrite()
			|| metadata->ValueType() != std::type_index(typeid(cui::drawing::Brush))
			|| !metadata->TryGet(target, currentValue)
			|| !currentValue.TryGet(currentBrushValue))
			return fail(L"动画目标必须持有包含变换的 Control."
				+ rootProperty + L"。");
		const auto* currentBrush = &currentBrushValue;
		auto ownerMatches = [&]()
		{
			if (EqualName(brushOwner, L"Brush")) return true;
			switch (currentBrush->Kind)
			{
			case cui::drawing::BrushKind::Solid:
				return EqualName(brushOwner, L"SolidColorBrush");
			case cui::drawing::BrushKind::LinearGradient:
				return EqualName(brushOwner, L"GradientBrush")
					|| EqualName(brushOwner, L"LinearGradientBrush");
			case cui::drawing::BrushKind::RadialGradient:
				return EqualName(brushOwner, L"GradientBrush")
					|| EqualName(brushOwner, L"RadialGradientBrush");
			case cui::drawing::BrushKind::Image:
				return EqualName(brushOwner, L"ImageBrush");
			default:
				return false;
			}
		};
		if (!ownerMatches())
			return fail(L"Brush Transform 路径所有者与实际画刷类型不匹配。");
		const bool relative = EqualName(
			path.Segments[1].Name, L"RelativeTransform");
		const auto& transform = relative
			? currentBrush->RelativeTransform : currentBrush->Transform;
		if (!transform)
			return fail(L"动画目标没有路径所需的 Brush Transform。");

		TransformAccessor transformAccessor;
		const auto canonicalPrefix = L"(Control." + rootProperty + L").(Brush."
			+ std::wstring(relative ? L"RelativeTransform" : L"Transform")
			+ L").(TransformGroup.Children)";
		if (!TryResolveTransformOperationAccessor(
			*transform, path.Segments[3].Index,
			LocalTypeName(path.Segments[4].OwnerType), path.Segments[4].Name,
			canonicalPrefix, transformAccessor, outError)) return false;
		output.BrushKind = currentBrush->Kind;
		output.Relative = relative;
		output.Transform = std::move(transformAccessor);
		output.CanonicalPath = output.Transform.CanonicalPath;
		outMetadata = metadata;
		if (outError) outError->clear();
		return true;
	}

	static bool TryResolveObjectPath(
		Control& target,
		const std::wstring& text,
		DeclarativeAnimationKind animationKind,
		const DependencyPropertyMetadata*& outMetadata,
		ObjectPathAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError)
			|| path.Segments.empty()
			|| path.Segments.front().Kind
				!= cui::xaml::PropertyPathSegmentKind::Property)
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const auto& root = path.Segments.front().Name;
		if (EqualName(root, L"RenderTransform"))
		{
			TransformAccessor accessor;
			if (!TryResolveTransformPath(
				target, text, outMetadata, accessor, outError)) return false;
			const auto expected = accessor.Member == TransformMember::Matrix
				? DeclarativeAnimationKind::Matrix
				: DeclarativeAnimationKind::Double;
			if (animationKind != expected)
				return fail(L"RenderTransform 数值末端需要 DoubleAnimation，"
					L"MatrixTransform.Matrix 末端需要 MatrixAnimation。");
			output = std::move(accessor);
			return true;
		}
		if (EqualName(root, L"Clip"))
		{
			size_t geometryLeafStart = 1;
			while (geometryLeafStart + 1 < path.Segments.size()
				&& path.Segments[geometryLeafStart].Kind
					== cui::xaml::PropertyPathSegmentKind::Property
				&& EqualName(LocalTypeName(
					path.Segments[geometryLeafStart].OwnerType), L"GeometryGroup")
				&& EqualName(path.Segments[geometryLeafStart].Name, L"Children")
				&& path.Segments[geometryLeafStart + 1].Kind
					== cui::xaml::PropertyPathSegmentKind::Index)
				geometryLeafStart += 2;
			if (geometryLeafStart < path.Segments.size()
				&& EqualName(path.Segments[geometryLeafStart].Name, L"Figures"))
			{
				PathGeometryAccessor accessor;
				if (!TryResolvePathGeometryPath(
					target, text, outMetadata, accessor, outError)) return false;
				const bool pointMember = accessor.Member
					== PathGeometryMember::FigureStartPoint
					|| accessor.Member == PathGeometryMember::SegmentPoint
					|| accessor.Member == PathGeometryMember::SegmentPoint1
					|| accessor.Member == PathGeometryMember::SegmentPoint2
					|| accessor.Member == PathGeometryMember::SegmentPoint3;
				const bool sizeMember = accessor.Member
					== PathGeometryMember::ArcSize;
				const bool doubleMember = accessor.Member
					== PathGeometryMember::ArcRotationAngle;
				const bool objectMember = accessor.Member
					== PathGeometryMember::FigureIsClosed
					|| accessor.Member == PathGeometryMember::FigureIsFilled
					|| accessor.Member == PathGeometryMember::ArcIsLargeArc
					|| accessor.Member == PathGeometryMember::ArcSweepDirection;
				if ((pointMember && animationKind != DeclarativeAnimationKind::Point)
					|| (sizeMember && animationKind != DeclarativeAnimationKind::Size)
					|| (doubleMember && animationKind != DeclarativeAnimationKind::Double)
					|| (objectMember && animationKind != DeclarativeAnimationKind::Object))
					return fail(L"Path Point 末端需要 PointAnimation，Size 末端需要 "
						L"SizeAnimation，角度末端需要 DoubleAnimation，布尔/枚举末端"
						L"需要 ObjectAnimationUsingKeyFrames。");
				output = std::move(accessor);
				return true;
			}
			if (geometryLeafStart < path.Segments.size()
				&& EqualName(path.Segments[geometryLeafStart].Name, L"Transform"))
			{
				GeometryTransformAccessor accessor;
				if (!TryResolveGeometryTransformPath(
					target, text, outMetadata, accessor, outError)) return false;
				const auto expected = accessor.Transform.Member
					== TransformMember::Matrix
					? DeclarativeAnimationKind::Matrix
					: DeclarativeAnimationKind::Double;
				if (animationKind != expected)
					return fail(L"Geometry.Transform 数值末端需要 DoubleAnimation，"
						L"MatrixTransform.Matrix 末端需要 MatrixAnimation。");
				output = std::move(accessor);
				return true;
			}
			GeometryAccessor accessor;
			if (!TryResolveGeometryPath(
				target, text, outMetadata, accessor, outError)) return false;
			const bool rectMember = accessor.Member == GeometryMember::Rect;
			const bool pointMember = accessor.Member == GeometryMember::Center;
			const bool objectMember = accessor.Member == GeometryMember::FillRule;
			if ((rectMember && animationKind != DeclarativeAnimationKind::Rect)
				|| (pointMember && animationKind != DeclarativeAnimationKind::Point)
				|| (objectMember && animationKind != DeclarativeAnimationKind::Object)
				|| (!rectMember && !pointMember && !objectMember
					&& animationKind != DeclarativeAnimationKind::Double))
				return fail(L"Geometry Rect 末端需要 RectAnimation，Center 末端需要 "
					L"PointAnimation，半径末端需要 DoubleAnimation，FillRule 需要 "
					L"ObjectAnimationUsingKeyFrames。");
			output = std::move(accessor);
			return true;
		}
		const auto* rootMetadata = target.FindPropertyMetadata(root);
		if (path.Segments.size() > 1 && rootMetadata
			&& rootMetadata->ValueType()
				== std::type_index(typeid(cui::drawing::Brush)))
		{
			if (path.Segments.size() > 1
				&& (EqualName(path.Segments[1].Name, L"Transform")
					|| EqualName(path.Segments[1].Name, L"RelativeTransform")))
			{
				BrushTransformAccessor accessor;
				if (!TryResolveBrushTransformPath(
					target, text, outMetadata, accessor, outError)) return false;
				const auto expected = accessor.Transform.Member
					== TransformMember::Matrix
					? DeclarativeAnimationKind::Matrix
					: DeclarativeAnimationKind::Double;
				if (animationKind != expected)
					return fail(L"Brush Transform 数值末端需要 DoubleAnimation，"
						L"MatrixTransform.Matrix 末端需要 MatrixAnimation。");
				output = std::move(accessor);
				return true;
			}
			BrushAccessor accessor;
			if (!TryResolveBrushPath(
				target, text, outMetadata, accessor, outError)) return false;
			const bool colorMember = accessor.Member == BrushMember::SolidColor
				|| accessor.Member == BrushMember::GradientStopColor;
			const bool pointMember = accessor.Member == BrushMember::StartPoint
				|| accessor.Member == BrushMember::EndPoint
				|| accessor.Member == BrushMember::Center
				|| accessor.Member == BrushMember::GradientOrigin;
			if ((colorMember && animationKind != DeclarativeAnimationKind::Color)
				|| (pointMember && animationKind != DeclarativeAnimationKind::Point)
				|| (!colorMember && !pointMember
					&& animationKind != DeclarativeAnimationKind::Double))
				return fail(L"Brush Color 末端需要 ColorAnimation，Point 末端需要 "
					L"PointAnimation，其余数值末端需要 DoubleAnimation。");
			output = std::move(accessor);
			return true;
		}
		return fail(L"尚未注册可处理此 Storyboard.TargetProperty 的对象路径适配器。");
	}

	static bool TryReadTransformMember(
		const cui::drawing::Transform& transform,
		const TransformAccessor& accessor,
		BindingValue& output) noexcept
	{
		if (accessor.OperationIndex >= transform.Operations.size()) return false;
		const auto& operation = transform.Operations[accessor.OperationIndex];
		if (operation.Kind != accessor.OperationKind) return false;
		float number = 0.0f;
		switch (accessor.Member)
		{
		case TransformMember::X: number = operation.X; break;
		case TransformMember::Y: number = operation.Y; break;
		case TransformMember::ScaleX: number = operation.ScaleX; break;
		case TransformMember::ScaleY: number = operation.ScaleY; break;
		case TransformMember::Angle: number = operation.Angle; break;
		case TransformMember::AngleX: number = operation.AngleX; break;
		case TransformMember::AngleY: number = operation.AngleY; break;
		case TransformMember::CenterX: number = operation.CenterX; break;
		case TransformMember::CenterY: number = operation.CenterY; break;
		case TransformMember::Matrix:
			if (!IsFiniteMatrix(operation.Matrix)) return false;
			output = BindingValue(operation.Matrix);
			return true;
		default: return false;
		}
		if (!std::isfinite(number)) return false;
		output = BindingValue(number);
		return true;
	}

	static bool TryWriteTransformMember(
		cui::drawing::Transform& transform,
		const TransformAccessor& accessor,
		const BindingValue& value) noexcept
	{
		if (accessor.OperationIndex >= transform.Operations.size()) return false;
		auto& operation = transform.Operations[accessor.OperationIndex];
		if (operation.Kind != accessor.OperationKind) return false;
		if (accessor.Member == TransformMember::Matrix)
		{
			D2D1_MATRIX_3X2_F matrix{};
			if (!value.TryGet(matrix) || !IsFiniteMatrix(matrix)) return false;
			operation.Matrix = matrix;
			return true;
		}
		double number = 0.0;
		if (!value.TryGetDouble(number) || !std::isfinite(number)
			|| number < -(std::numeric_limits<float>::max)()
			|| number > (std::numeric_limits<float>::max)()) return false;
		const auto result = static_cast<float>(number);
		switch (accessor.Member)
		{
		case TransformMember::X: operation.X = result; break;
		case TransformMember::Y: operation.Y = result; break;
		case TransformMember::ScaleX: operation.ScaleX = result; break;
		case TransformMember::ScaleY: operation.ScaleY = result; break;
		case TransformMember::Angle: operation.Angle = result; break;
		case TransformMember::AngleX: operation.AngleX = result; break;
		case TransformMember::AngleY: operation.AngleY = result; break;
		case TransformMember::CenterX: operation.CenterX = result; break;
		case TransformMember::CenterY: operation.CenterY = result; break;
		case TransformMember::Matrix: return false;
		default: return false;
		}
		return true;
	}

	static bool TryReadGeometryMember(
		const cui::drawing::Geometry& root,
		const GeometryAccessor& accessor,
		BindingValue& output) noexcept
	{
		const auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		const auto& geometry = *resolved;
		if (geometry.Kind != accessor.GeometryKind) return false;
		switch (accessor.Member)
		{
		case GeometryMember::Rect:
		{
			if (geometry.Kind != cui::drawing::GeometryKind::Rectangle) return false;
			const auto rect = ToCoreRect(geometry.Rect);
			if (!std::isfinite(rect.x) || !std::isfinite(rect.y)
				|| !std::isfinite(rect.width) || !std::isfinite(rect.height)
				|| rect.width < 0.0f || rect.height < 0.0f) return false;
			output = BindingValue(rect);
			return true;
		}
		case GeometryMember::Center:
			if (geometry.Kind != cui::drawing::GeometryKind::Ellipse
				|| !std::isfinite(geometry.Center.x)
				|| !std::isfinite(geometry.Center.y)) return false;
			output = BindingValue(cui::core::Point{
				geometry.Center.x, geometry.Center.y });
			return true;
		case GeometryMember::RadiusX:
		case GeometryMember::RadiusY:
		{
			if (geometry.Kind != cui::drawing::GeometryKind::Rectangle
				&& geometry.Kind != cui::drawing::GeometryKind::Ellipse) return false;
			const auto radius = accessor.Member == GeometryMember::RadiusX
				? geometry.RadiusX : geometry.RadiusY;
			if (!std::isfinite(radius) || radius < 0.0f) return false;
			output = BindingValue(radius);
			return true;
		}
		case GeometryMember::FillRule:
			if (geometry.Kind != cui::drawing::GeometryKind::Path
				&& geometry.Kind != cui::drawing::GeometryKind::Group) return false;
			output = BindingValue(std::wstring(geometry.FillRule
				== cui::drawing::GeometryFillRule::Nonzero
				? L"Nonzero" : L"EvenOdd"));
			return true;
		default:
			return false;
		}
	}

	static bool TryWriteGeometryMember(
		cui::drawing::Geometry& root,
		const GeometryAccessor& accessor,
		const BindingValue& value) noexcept
	{
		auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		auto& geometry = *resolved;
		if (geometry.Kind != accessor.GeometryKind) return false;
		switch (accessor.Member)
		{
		case GeometryMember::Rect:
		{
			cui::core::Rect rect{};
			if (geometry.Kind != cui::drawing::GeometryKind::Rectangle
				|| !value.TryGet(rect)
				|| !std::isfinite(rect.x) || !std::isfinite(rect.y)
				|| !std::isfinite(rect.width) || !std::isfinite(rect.height)
				|| rect.width < 0.0f || rect.height < 0.0f) return false;
			const auto converted = ToD2DRect(rect);
			if (!std::isfinite(converted.left) || !std::isfinite(converted.top)
				|| !std::isfinite(converted.right) || !std::isfinite(converted.bottom))
				return false;
			geometry.Rect = converted;
			return true;
		}
		case GeometryMember::Center:
		{
			cui::core::Point point{};
			if (geometry.Kind != cui::drawing::GeometryKind::Ellipse
				|| !value.TryGet(point) || !std::isfinite(point.x)
				|| !std::isfinite(point.y)) return false;
			geometry.Center = D2D1::Point2F(point.x, point.y);
			return true;
		}
		case GeometryMember::RadiusX:
		case GeometryMember::RadiusY:
		{
			double radius = 0.0;
			if ((geometry.Kind != cui::drawing::GeometryKind::Rectangle
				&& geometry.Kind != cui::drawing::GeometryKind::Ellipse)
				|| !value.TryGetDouble(radius) || !std::isfinite(radius)
				|| radius < -(std::numeric_limits<float>::max)()
				|| radius > (std::numeric_limits<float>::max)()) return false;
			const auto result = static_cast<float>((std::max)(radius, 0.0));
			if (accessor.Member == GeometryMember::RadiusX)
				geometry.RadiusX = result;
			else geometry.RadiusY = result;
			return true;
		}
		case GeometryMember::FillRule:
		{
			std::wstring converted;
			if ((geometry.Kind != cui::drawing::GeometryKind::Path
					&& geometry.Kind != cui::drawing::GeometryKind::Group)
				|| !value.TryGet(converted)) return false;
			if (EqualValueToken(converted, L"Nonzero"))
				geometry.FillRule = cui::drawing::GeometryFillRule::Nonzero;
			else if (EqualValueToken(converted, L"EvenOdd"))
				geometry.FillRule = cui::drawing::GeometryFillRule::EvenOdd;
			else return false;
			return true;
		}
		default:
			return false;
		}
	}

	static bool TryReadPathGeometryMember(
		const cui::drawing::Geometry& root,
		const PathGeometryAccessor& accessor,
		BindingValue& output) noexcept
	{
		const auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		const auto& geometry = *resolved;
		if (geometry.Kind != cui::drawing::GeometryKind::Path
			|| accessor.FigureIndex >= geometry.Figures.size()) return false;
		const auto& figure = geometry.Figures[accessor.FigureIndex];
		auto readPoint = [&](D2D1_POINT_2F point)
		{
			if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
			output = BindingValue(cui::core::Point{ point.x, point.y });
			return true;
		};
		if (!accessor.HasSegment)
		{
			switch (accessor.Member)
			{
			case PathGeometryMember::FigureStartPoint:
				return readPoint(figure.StartPoint);
			case PathGeometryMember::FigureIsClosed:
				output = BindingValue(figure.IsClosed); return true;
			case PathGeometryMember::FigureIsFilled:
				output = BindingValue(figure.IsFilled); return true;
			default:
				return false;
			}
		}
		if (accessor.SegmentIndex >= figure.Segments.size()) return false;
		const auto& segment = figure.Segments[accessor.SegmentIndex];
		if (segment.Kind != accessor.SegmentKind) return false;
		switch (accessor.Member)
		{
		case PathGeometryMember::SegmentPoint:
			return readPoint(segment.Point);
		case PathGeometryMember::SegmentPoint1:
			return readPoint(segment.Point1);
		case PathGeometryMember::SegmentPoint2:
			return readPoint(segment.Point2);
		case PathGeometryMember::SegmentPoint3:
			return readPoint(segment.Point3);
		case PathGeometryMember::ArcSize:
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !std::isfinite(segment.Size.width)
				|| !std::isfinite(segment.Size.height)
				|| segment.Size.width < 0.0f || segment.Size.height < 0.0f)
				return false;
			output = BindingValue(cui::core::Size{
				segment.Size.width, segment.Size.height });
			return true;
		case PathGeometryMember::ArcRotationAngle:
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !std::isfinite(segment.RotationAngle)) return false;
			output = BindingValue(segment.RotationAngle);
			return true;
		case PathGeometryMember::ArcIsLargeArc:
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc) return false;
			output = BindingValue(segment.IsLargeArc);
			return true;
		case PathGeometryMember::ArcSweepDirection:
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc) return false;
			output = BindingValue(std::wstring(segment.Sweep
				== cui::drawing::SweepDirection::Clockwise
				? L"Clockwise" : L"Counterclockwise"));
			return true;
		default:
			return false;
		}
	}

	static bool TryWritePathGeometryMember(
		cui::drawing::Geometry& root,
		const PathGeometryAccessor& accessor,
		const BindingValue& value) noexcept
	{
		auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		auto& geometry = *resolved;
		if (geometry.Kind != cui::drawing::GeometryKind::Path
			|| accessor.FigureIndex >= geometry.Figures.size()) return false;
		auto& figure = geometry.Figures[accessor.FigureIndex];
		auto readPoint = [&](D2D1_POINT_2F& point)
		{
			cui::core::Point converted{};
			if (!value.TryGet(converted) || !std::isfinite(converted.x)
				|| !std::isfinite(converted.y)) return false;
			point = D2D1::Point2F(converted.x, converted.y);
			return true;
		};
		if (!accessor.HasSegment)
		{
			switch (accessor.Member)
			{
			case PathGeometryMember::FigureStartPoint:
				return readPoint(figure.StartPoint);
			case PathGeometryMember::FigureIsClosed:
			{
				bool converted = false;
				if (!value.TryGet(converted)) return false;
				figure.IsClosed = converted;
				return true;
			}
			case PathGeometryMember::FigureIsFilled:
			{
				bool converted = false;
				if (!value.TryGet(converted)) return false;
				figure.IsFilled = converted;
				return true;
			}
			default:
				return false;
			}
		}
		if (accessor.SegmentIndex >= figure.Segments.size()) return false;
		auto& segment = figure.Segments[accessor.SegmentIndex];
		if (segment.Kind != accessor.SegmentKind) return false;
		switch (accessor.Member)
		{
		case PathGeometryMember::SegmentPoint:
			return readPoint(segment.Point);
		case PathGeometryMember::SegmentPoint1:
			return readPoint(segment.Point1);
		case PathGeometryMember::SegmentPoint2:
			return readPoint(segment.Point2);
		case PathGeometryMember::SegmentPoint3:
			return readPoint(segment.Point3);
		case PathGeometryMember::ArcSize:
		{
			cui::core::Size converted{};
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !value.TryGet(converted)
				|| !std::isfinite(converted.width)
				|| !std::isfinite(converted.height)) return false;
			segment.Size = D2D1::SizeF(
				(std::max)(converted.width, 0.0f),
				(std::max)(converted.height, 0.0f));
			return true;
		}
		case PathGeometryMember::ArcRotationAngle:
		{
			double converted = 0.0;
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !value.TryGetDouble(converted) || !std::isfinite(converted)
				|| converted < -(std::numeric_limits<float>::max)()
				|| converted > (std::numeric_limits<float>::max)()) return false;
			segment.RotationAngle = static_cast<float>(converted);
			return true;
		}
		case PathGeometryMember::ArcIsLargeArc:
		{
			bool converted = false;
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !value.TryGet(converted)) return false;
			segment.IsLargeArc = converted;
			return true;
		}
		case PathGeometryMember::ArcSweepDirection:
		{
			std::wstring converted;
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !value.TryGet(converted)) return false;
			if (EqualValueToken(converted, L"Clockwise"))
				segment.Sweep = cui::drawing::SweepDirection::Clockwise;
			else if (EqualValueToken(converted, L"Counterclockwise"))
				segment.Sweep = cui::drawing::SweepDirection::Counterclockwise;
			else return false;
			return true;
		}
		default:
			return false;
		}
	}

	static bool TryReadGeometryTransformMember(
		const cui::drawing::Geometry& root,
		const GeometryTransformAccessor& accessor,
		BindingValue& output) noexcept
	{
		const auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		const auto& geometry = *resolved;
		if (geometry.Kind != accessor.GeometryKind) return false;
		if (!geometry.LocalTransform
			|| !TryReadTransformMember(
				*geometry.LocalTransform, accessor.Transform, output)) return false;
		return true;
	}

	static bool TryWriteGeometryTransformMember(
		cui::drawing::Geometry& root,
		const GeometryTransformAccessor& accessor,
		const BindingValue& value) noexcept
	{
		auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		auto& geometry = *resolved;
		return geometry.Kind == accessor.GeometryKind
			&& geometry.LocalTransform
			&& TryWriteTransformMember(
				*geometry.LocalTransform, accessor.Transform, value);
	}

	static bool TryReadBrushMember(
		const cui::drawing::Brush& brush,
		const BrushAccessor& accessor,
		BindingValue& output) noexcept
	{
		if (brush.Kind != accessor.BrushKind) return false;
		auto finiteColor = [](const D2D1_COLOR_F& color)
		{
			return std::isfinite(color.r) && std::isfinite(color.g)
				&& std::isfinite(color.b) && std::isfinite(color.a);
		};
		auto readPoint = [&](D2D1_POINT_2F point)
		{
			if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
			output = BindingValue(cui::core::Point{ point.x, point.y });
			return true;
		};
		switch (accessor.Member)
		{
		case BrushMember::SolidColor:
			if (brush.Kind != cui::drawing::BrushKind::Solid
				|| !finiteColor(brush.Color)) return false;
			output = BindingValue(brush.Color);
			return true;
		case BrushMember::Opacity:
			if (!std::isfinite(brush.Opacity)
				|| brush.Opacity < 0.0f || brush.Opacity > 1.0f) return false;
			output = BindingValue(brush.Opacity);
			return true;
		case BrushMember::StartPoint:
			return brush.Kind == cui::drawing::BrushKind::LinearGradient
				&& readPoint(brush.StartPoint);
		case BrushMember::EndPoint:
			return brush.Kind == cui::drawing::BrushKind::LinearGradient
				&& readPoint(brush.EndPoint);
		case BrushMember::Center:
			return brush.Kind == cui::drawing::BrushKind::RadialGradient
				&& readPoint(brush.Center);
		case BrushMember::GradientOrigin:
			return brush.Kind == cui::drawing::BrushKind::RadialGradient
				&& readPoint(brush.GradientOrigin);
		case BrushMember::RadiusX:
		case BrushMember::RadiusY:
		{
			if (brush.Kind != cui::drawing::BrushKind::RadialGradient) return false;
			const auto radius = accessor.Member == BrushMember::RadiusX
				? brush.RadiusX : brush.RadiusY;
			if (!std::isfinite(radius) || radius < 0.0f) return false;
			output = BindingValue(radius);
			return true;
		}
		case BrushMember::GradientStopColor:
		case BrushMember::GradientStopOffset:
		{
			if ((brush.Kind != cui::drawing::BrushKind::LinearGradient
					&& brush.Kind != cui::drawing::BrushKind::RadialGradient)
				|| accessor.StopIndex >= brush.GradientStops.size()) return false;
			const auto& stop = brush.GradientStops[accessor.StopIndex];
			if (accessor.Member == BrushMember::GradientStopColor)
			{
				if (!finiteColor(stop.Color)) return false;
				output = BindingValue(stop.Color);
				return true;
			}
			if (!std::isfinite(stop.Offset)
				|| stop.Offset < 0.0f || stop.Offset > 1.0f) return false;
			output = BindingValue(stop.Offset);
			return true;
		}
		default:
			return false;
		}
	}

	static bool TryWriteBrushMember(
		cui::drawing::Brush& brush,
		const BrushAccessor& accessor,
		const BindingValue& value) noexcept
	{
		if (brush.Kind != accessor.BrushKind) return false;
		auto readColor = [&](D2D1_COLOR_F& color)
		{
			return value.TryGet(color)
				&& std::isfinite(color.r) && std::isfinite(color.g)
				&& std::isfinite(color.b) && std::isfinite(color.a);
		};
		auto readPoint = [&](D2D1_POINT_2F& point)
		{
			cui::core::Point parsed{};
			if (!value.TryGet(parsed) || !std::isfinite(parsed.x)
				|| !std::isfinite(parsed.y)) return false;
			point = D2D1::Point2F(parsed.x, parsed.y);
			return true;
		};
		auto readNumber = [&](double& number)
		{
			return value.TryGetDouble(number) && std::isfinite(number)
				&& number >= -(std::numeric_limits<float>::max)()
				&& number <= (std::numeric_limits<float>::max)();
		};
		switch (accessor.Member)
		{
		case BrushMember::SolidColor:
		{
			D2D1_COLOR_F color{};
			if (brush.Kind != cui::drawing::BrushKind::Solid || !readColor(color))
				return false;
			brush.Color = color;
			return true;
		}
		case BrushMember::Opacity:
		{
			double opacity = 0.0;
			if (!readNumber(opacity)) return false;
			brush.Opacity = static_cast<float>((std::clamp)(opacity, 0.0, 1.0));
			return true;
		}
		case BrushMember::StartPoint:
			return brush.Kind == cui::drawing::BrushKind::LinearGradient
				&& readPoint(brush.StartPoint);
		case BrushMember::EndPoint:
			return brush.Kind == cui::drawing::BrushKind::LinearGradient
				&& readPoint(brush.EndPoint);
		case BrushMember::Center:
			return brush.Kind == cui::drawing::BrushKind::RadialGradient
				&& readPoint(brush.Center);
		case BrushMember::GradientOrigin:
			return brush.Kind == cui::drawing::BrushKind::RadialGradient
				&& readPoint(brush.GradientOrigin);
		case BrushMember::RadiusX:
		case BrushMember::RadiusY:
		{
			double radius = 0.0;
			if (brush.Kind != cui::drawing::BrushKind::RadialGradient
				|| !readNumber(radius)) return false;
			const auto result = static_cast<float>((std::max)(radius, 0.0));
			if (accessor.Member == BrushMember::RadiusX) brush.RadiusX = result;
			else brush.RadiusY = result;
			return true;
		}
		case BrushMember::GradientStopColor:
		case BrushMember::GradientStopOffset:
		{
			if ((brush.Kind != cui::drawing::BrushKind::LinearGradient
					&& brush.Kind != cui::drawing::BrushKind::RadialGradient)
				|| accessor.StopIndex >= brush.GradientStops.size()) return false;
			auto& stop = brush.GradientStops[accessor.StopIndex];
			if (accessor.Member == BrushMember::GradientStopColor)
			{
				D2D1_COLOR_F color{};
				if (!readColor(color)) return false;
				stop.Color = color;
				return true;
			}
			double offset = 0.0;
			if (!readNumber(offset)) return false;
			// GradientStop.Offset is authored in 0..1 in CUI. Animation results
			// pass through the same target-property coercion at frame write time.
			stop.Offset = static_cast<float>((std::clamp)(offset, 0.0, 1.0));
			return true;
		}
		default:
			return false;
		}
	}

	static bool TryReadBrushTransformMember(
		const cui::drawing::Brush& brush,
		const BrushTransformAccessor& accessor,
		BindingValue& output) noexcept
	{
		if (brush.Kind != accessor.BrushKind) return false;
		const auto& transform = accessor.Relative
			? brush.RelativeTransform : brush.Transform;
		if (!transform
			|| !TryReadTransformMember(*transform, accessor.Transform, output))
			return false;
		return true;
	}

	static bool TryWriteBrushTransformMember(
		cui::drawing::Brush& brush,
		const BrushTransformAccessor& accessor,
		const BindingValue& value) noexcept
	{
		if (brush.Kind != accessor.BrushKind) return false;
		auto& transform = accessor.Relative
			? brush.RelativeTransform : brush.Transform;
		return transform
			&& TryWriteTransformMember(*transform, accessor.Transform, value);
	}

	static bool TryReadObjectPathMember(
		const BindingValue& root,
		const ObjectPathAccessor& accessor,
		BindingValue& output) noexcept
	{
		return std::visit([&](const auto& typed)
		{
			using T = std::decay_t<decltype(typed)>;
			if constexpr (std::is_same_v<T, TransformAccessor>)
			{
				cui::drawing::Transform transform;
				if (!root.TryGet(transform)
					|| !TryReadTransformMember(transform, typed, output)) return false;
				return true;
			}
			else if constexpr (std::is_same_v<T, GeometryAccessor>)
			{
				cui::drawing::Geometry geometry;
				return root.TryGet(geometry)
					&& TryReadGeometryMember(geometry, typed, output);
			}
			else if constexpr (std::is_same_v<T, PathGeometryAccessor>)
			{
				cui::drawing::Geometry geometry;
				return root.TryGet(geometry)
					&& TryReadPathGeometryMember(geometry, typed, output);
			}
			else if constexpr (std::is_same_v<T, GeometryTransformAccessor>)
			{
				cui::drawing::Geometry geometry;
				return root.TryGet(geometry)
					&& TryReadGeometryTransformMember(geometry, typed, output);
			}
			else if constexpr (std::is_same_v<T, BrushAccessor>)
			{
				cui::drawing::Brush brush;
				return root.TryGet(brush)
					&& TryReadBrushMember(brush, typed, output);
			}
			else if constexpr (std::is_same_v<T, BrushTransformAccessor>)
			{
				cui::drawing::Brush brush;
				return root.TryGet(brush)
					&& TryReadBrushTransformMember(brush, typed, output);
			}
			else return false;
		}, accessor);
	}

	static bool TryWriteObjectPathMember(
		BindingValue& root,
		const ObjectPathAccessor& accessor,
		const BindingValue& member) noexcept
	{
		return std::visit([&](const auto& typed)
		{
			using T = std::decay_t<decltype(typed)>;
			if constexpr (std::is_same_v<T, TransformAccessor>)
			{
				cui::drawing::Transform transform;
				if (!root.TryGet(transform)
					|| !TryWriteTransformMember(transform, typed, member)) return false;
				root = BindingValue(std::move(transform));
				return true;
			}
			else if constexpr (std::is_same_v<T, GeometryAccessor>)
			{
				cui::drawing::Geometry geometry;
				if (!root.TryGet(geometry)
					|| !TryWriteGeometryMember(geometry, typed, member)) return false;
				root = BindingValue(std::move(geometry));
				return true;
			}
			else if constexpr (std::is_same_v<T, PathGeometryAccessor>)
			{
				cui::drawing::Geometry geometry;
				if (!root.TryGet(geometry)
					|| !TryWritePathGeometryMember(
						geometry, typed, member)) return false;
				root = BindingValue(std::move(geometry));
				return true;
			}
			else if constexpr (std::is_same_v<T, GeometryTransformAccessor>)
			{
				cui::drawing::Geometry geometry;
				if (!root.TryGet(geometry)
					|| !TryWriteGeometryTransformMember(
						geometry, typed, member)) return false;
				root = BindingValue(std::move(geometry));
				return true;
			}
			else if constexpr (std::is_same_v<T, BrushAccessor>)
			{
				cui::drawing::Brush brush;
				if (!root.TryGet(brush)
					|| !TryWriteBrushMember(brush, typed, member)) return false;
				root = BindingValue(std::move(brush));
				return true;
			}
			else if constexpr (std::is_same_v<T, BrushTransformAccessor>)
			{
				cui::drawing::Brush brush;
				if (!root.TryGet(brush)
					|| !TryWriteBrushTransformMember(brush, typed, member)) return false;
				root = BindingValue(std::move(brush));
				return true;
			}
			else return false;
		}, accessor);
	}

	static double Ease(
		double progress,
		DeclarativeEasingKind kind,
		DeclarativeEasingMode mode) noexcept
	{
		progress = (std::clamp)(progress, 0.0, 1.0);
		if (kind == DeclarativeEasingKind::Linear) return progress;
		auto easeIn = [kind](double value)
		{
			switch (kind)
			{
			case DeclarativeEasingKind::Quadratic: return value * value;
			case DeclarativeEasingKind::Cubic: return value * value * value;
			case DeclarativeEasingKind::Sine:
				return 1.0 - std::cos(value * 1.57079632679489661923);
			case DeclarativeEasingKind::Linear:
			default: return value;
			}
		};
		switch (mode)
		{
		case DeclarativeEasingMode::EaseIn:
			return easeIn(progress);
		case DeclarativeEasingMode::EaseInOut:
			return progress < 0.5
				? easeIn(progress * 2.0) * 0.5
				: 1.0 - easeIn((1.0 - progress) * 2.0) * 0.5;
		case DeclarativeEasingMode::EaseOut:
		default:
			return 1.0 - easeIn(1.0 - progress);
		}
	}

	static double CubicBezier(double t, double p1, double p2) noexcept
	{
		const auto inverse = 1.0 - t;
		return 3.0 * inverse * inverse * t * p1
			+ 3.0 * inverse * t * t * p2 + t * t * t;
	}

	static double CubicBezierDerivative(double t, double p1, double p2) noexcept
	{
		const auto inverse = 1.0 - t;
		return 3.0 * inverse * inverse * p1
			+ 6.0 * inverse * t * (p2 - p1)
			+ 3.0 * t * t * (1.0 - p2);
	}

	static double KeySplineProgress(
		double progress,
		const DeclarativeAnimationKeyFrame& keyFrame) noexcept
	{
		progress = (std::clamp)(progress, 0.0, 1.0);
		double parameter = progress;
		for (int iteration = 0; iteration < 8; ++iteration)
		{
			const auto error = CubicBezier(
				parameter, keyFrame.KeySplineX1, keyFrame.KeySplineX2) - progress;
			if (std::fabs(error) <= 1e-7) break;
			const auto derivative = CubicBezierDerivative(
				parameter, keyFrame.KeySplineX1, keyFrame.KeySplineX2);
			if (std::fabs(derivative) <= 1e-7) break;
			const auto candidate = parameter - error / derivative;
			if (candidate < 0.0 || candidate > 1.0) break;
			parameter = candidate;
		}
		double low = 0.0;
		double high = 1.0;
		for (int iteration = 0; iteration < 18; ++iteration)
		{
			const auto x = CubicBezier(
				parameter, keyFrame.KeySplineX1, keyFrame.KeySplineX2);
			if (std::fabs(x - progress) <= 1e-7) break;
			if (x < progress) low = parameter;
			else high = parameter;
			parameter = (low + high) * 0.5;
		}
		return CubicBezier(
			parameter, keyFrame.KeySplineY1, keyFrame.KeySplineY2);
	}

	static bool InterpolateValues(
		DeclarativeAnimationKind kind,
		const DependencyPropertyMetadata* metadata,
		bool transformPath,
		const BindingValue& fromValue,
		const BindingValue& toValue,
		double progress,
		BindingValue& output)
	{
		progress = (std::clamp)(progress, 0.0, 1.0);
		if (kind == DeclarativeAnimationKind::Thickness)
		{
			Thickness from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			Thickness result;
			if (!lerp(from.Left, to.Left, result.Left)
				|| !lerp(from.Top, to.Top, result.Top)
				|| !lerp(from.Right, to.Right, result.Right)
				|| !lerp(from.Bottom, to.Bottom, result.Bottom)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Size)
		{
			cui::core::Size from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Size result;
			if (!lerp(from.width, to.width, result.width)
				|| !lerp(from.height, to.height, result.height)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Matrix)
		{
			D2D1_MATRIX_3X2_F from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)
				|| !IsFiniteMatrix(from) || !IsFiniteMatrix(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			D2D1_MATRIX_3X2_F result{};
			if (!lerp(from._11, to._11, result._11)
				|| !lerp(from._12, to._12, result._12)
				|| !lerp(from._21, to._21, result._21)
				|| !lerp(from._22, to._22, result._22)
				|| !lerp(from._31, to._31, result._31)
				|| !lerp(from._32, to._32, result._32)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Point)
		{
			cui::core::Point from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Point result;
			if (!lerp(from.x, to.x, result.x)
				|| !lerp(from.y, to.y, result.y)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Vector)
		{
			cui::core::Vector from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Vector result;
			if (!lerp(from.x, to.x, result.x)
				|| !lerp(from.y, to.y, result.y)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Rect)
		{
			cui::core::Rect from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Rect result;
			if (!lerp(from.x, to.x, result.x)
				|| !lerp(from.y, to.y, result.y)
				|| !lerp(from.width, to.width, result.width)
				|| !lerp(from.height, to.height, result.height)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Color)
		{
			D2D1_COLOR_F from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right)
			{
				return static_cast<float>(left + (right - left) * progress);
			};
			output = BindingValue(D2D1_COLOR_F{
				lerp(from.r, to.r), lerp(from.g, to.g),
				lerp(from.b, to.b), lerp(from.a, to.a) });
			return true;
		}

		double from = 0.0;
		double to = 0.0;
		if (!fromValue.TryGetDouble(from)
			|| !toValue.TryGetDouble(to)) return false;
		const double value = from + (to - from) * progress;
		if (transformPath)
		{
			if (!std::isfinite(value)
				|| value < -(std::numeric_limits<float>::max)()
				|| value > (std::numeric_limits<float>::max)()) return false;
			output = BindingValue(static_cast<float>(value));
			return true;
		}
		if (!metadata) return false;
		switch (metadata->ValueKind())
		{
		case BindingValueKind::Int:
			if (value <= static_cast<double>((std::numeric_limits<int>::min)()))
				output = BindingValue((std::numeric_limits<int>::min)());
			else if (value >= static_cast<double>((std::numeric_limits<int>::max)()))
				output = BindingValue((std::numeric_limits<int>::max)());
			else
				output = BindingValue(static_cast<int>(std::llround(value)));
			return true;
		case BindingValueKind::Int64:
			// The double representation of LLONG_MAX rounds to 2^63, which is
			// outside llround's domain. Clamp before rounding so a legal endpoint
			// cannot introduce undefined behaviour on the final animation frame.
			if (value <= static_cast<double>((std::numeric_limits<long long>::min)()))
				output = BindingValue((std::numeric_limits<long long>::min)());
			else if (value >= static_cast<double>((std::numeric_limits<long long>::max)()))
				output = BindingValue((std::numeric_limits<long long>::max)());
			else
				output = BindingValue(static_cast<long long>(std::llround(value)));
			return true;
		case BindingValueKind::Float:
			output = BindingValue(static_cast<float>(value));
			return true;
		case BindingValueKind::Double:
			output = BindingValue(value);
			return true;
		default:
			return false;
		}
	}

	template<typename TAnimation>
	static bool CombineAnimationValues(
		const TAnimation& animation,
		const BindingValue& left,
		const BindingValue& right,
		long double rightScale,
		BindingValue& output)
	{
		if (animation.Kind == DeclarativeAnimationKind::Thickness)
		{
			Thickness base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			Thickness result;
			if (!combine(base.Left, increment.Left, result.Left)
				|| !combine(base.Top, increment.Top, result.Top)
				|| !combine(base.Right, increment.Right, result.Right)
				|| !combine(base.Bottom, increment.Bottom, result.Bottom)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Size)
		{
			cui::core::Size base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Size result;
			if (!combine(base.width, increment.width, result.width)
				|| !combine(base.height, increment.height, result.height)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Matrix)
		{
			D2D1_MATRIX_3X2_F base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)
				|| !IsFiniteMatrix(base) || !IsFiniteMatrix(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			D2D1_MATRIX_3X2_F result{};
			if (!combine(base._11, increment._11, result._11)
				|| !combine(base._12, increment._12, result._12)
				|| !combine(base._21, increment._21, result._21)
				|| !combine(base._22, increment._22, result._22)
				|| !combine(base._31, increment._31, result._31)
				|| !combine(base._32, increment._32, result._32)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Point)
		{
			cui::core::Point base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Point result;
			if (!combine(base.x, increment.x, result.x)
				|| !combine(base.y, increment.y, result.y)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Vector)
		{
			cui::core::Vector base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Vector result;
			if (!combine(base.x, increment.x, result.x)
				|| !combine(base.y, increment.y, result.y)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Rect)
		{
			cui::core::Rect base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Rect result;
			if (!combine(base.x, increment.x, result.x)
				|| !combine(base.y, increment.y, result.y)
				|| !combine(base.width, increment.width, result.width)
				|| !combine(base.height, increment.height, result.height)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Color)
		{
			D2D1_COLOR_F base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			const D2D1_COLOR_F result{
				static_cast<float>(base.r + increment.r * rightScale),
				static_cast<float>(base.g + increment.g * rightScale),
				static_cast<float>(base.b + increment.b * rightScale),
				static_cast<float>(base.a + increment.a * rightScale) };
			if (!std::isfinite(result.r) || !std::isfinite(result.g)
				|| !std::isfinite(result.b) || !std::isfinite(result.a)) return false;
			output = BindingValue(result);
			return true;
		}

		double base = 0.0;
		double increment = 0.0;
		if (!left.TryGetDouble(base) || !right.TryGetDouble(increment)) return false;
		const long double exact = static_cast<long double>(base)
			+ static_cast<long double>(increment) * rightScale;
		if (!std::isfinite(exact)
			|| exact < -(std::numeric_limits<double>::max)()
			|| exact > (std::numeric_limits<double>::max)()) return false;
		const double result = static_cast<double>(exact);
		if (!std::isfinite(result)) return false;
		if ((ObjectPathUsesFloat(animation.ObjectPath)
				|| (animation.Metadata
					&& animation.Metadata->ValueKind() == BindingValueKind::Float))
			&& (result < -(std::numeric_limits<float>::max)()
				|| result > (std::numeric_limits<float>::max)())) return false;
		output = BindingValue(result);
		return true;
	}

	template<typename TAnimation>
	static BindingValue ZeroAnimationValue(const TAnimation& animation)
	{
		if (animation.Kind == DeclarativeAnimationKind::Object)
			return BindingValue{};
		if (animation.Kind == DeclarativeAnimationKind::Thickness)
			return BindingValue(Thickness{});
		if (animation.Kind == DeclarativeAnimationKind::Point)
			return BindingValue(cui::core::Point{});
		if (animation.Kind == DeclarativeAnimationKind::Vector)
			return BindingValue(cui::core::Vector{});
		if (animation.Kind == DeclarativeAnimationKind::Rect)
			return BindingValue(cui::core::Rect{});
		if (animation.Kind == DeclarativeAnimationKind::Size)
			return BindingValue(cui::core::Size{});
		if (animation.Kind == DeclarativeAnimationKind::Matrix)
			return BindingValue(D2D1_MATRIX_3X2_F{});
		return animation.Kind == DeclarativeAnimationKind::Color
			? BindingValue(D2D1_COLOR_F{}) : BindingValue(0.0);
	}

	template<typename TAnimation>
	static bool AddAnimationValues(
		const TAnimation& animation,
		const BindingValue& left,
		const BindingValue& delta,
		BindingValue& output)
	{
		return CombineAnimationValues(animation, left, delta, 1.0L, output);
	}

	static bool ResolveAnimationEndpoints(
		const RuntimeAnimation& animation,
		const BindingValue& defaultOrigin,
		const BindingValue& defaultDestination,
		BindingValue& from,
		BindingValue& to,
		BindingValue& foundation)
	{
		if (defaultOrigin.Kind() == BindingValueKind::Empty
			|| defaultDestination.Kind() == BindingValueKind::Empty) return false;
		const auto zero = ZeroAnimationValue(animation);
		if (!animation.KeyFrames.empty())
		{
			if (animation.Kind == DeclarativeAnimationKind::Object)
			{
				from = defaultOrigin;
				to = defaultDestination;
				foundation = BindingValue{};
				return true;
			}
			from = animation.IsAdditive ? zero : defaultOrigin;
			to = defaultDestination;
			foundation = animation.IsAdditive ? defaultOrigin : zero;
			return true;
		}
		const bool hasFrom = animation.From.has_value();
		const bool hasTo = animation.To.has_value();
		const bool hasBy = animation.By.has_value() && !hasTo;
		if (!hasFrom && hasBy)
		{
			from = zero;
			to = *animation.By;
			foundation = defaultOrigin;
			return true;
		}
		from = hasFrom ? *animation.From : defaultOrigin;
		if (animation.To)
			to = *animation.To;
		else if (hasBy)
		{
			if (!AddAnimationValues(animation, from, *animation.By, to)) return false;
		}
		else to = defaultDestination;
		foundation = animation.IsAdditive && hasFrom && (hasTo || hasBy)
			? defaultOrigin : zero;
		return true;
	}

	static long double TimelineActiveDurationExact(
		unsigned long long durationMilliseconds,
		DeclarativeRepeatBehaviorKind repeatBehavior,
		double repeatCount,
		unsigned long long repeatDurationMilliseconds,
		bool autoReverse,
		double speedRatio) noexcept
	{
		if (repeatBehavior == DeclarativeRepeatBehaviorKind::Forever)
			return std::numeric_limits<long double>::infinity();
		if (repeatBehavior == DeclarativeRepeatBehaviorKind::Duration)
			return static_cast<long double>(repeatDurationMilliseconds);
		return static_cast<long double>(durationMilliseconds)
			* (autoReverse ? 2.0L : 1.0L)
			* static_cast<long double>(repeatCount)
			/ static_cast<long double>(speedRatio);
	}

	static long double TimelineActiveDurationExact(
		const RuntimeAnimation& animation) noexcept
	{
		return TimelineActiveDurationExact(animation.DurationMilliseconds,
			animation.RepeatBehavior, animation.RepeatCount,
			animation.RepeatDurationMilliseconds, animation.AutoReverse,
			animation.SpeedRatio);
	}

	static long double TimelineActiveDurationExact(
		const ActiveAnimation& animation) noexcept
	{
		return TimelineActiveDurationExact(animation.DurationMilliseconds,
			animation.RepeatBehavior, animation.RepeatCount,
			animation.RepeatDurationMilliseconds, animation.AutoReverse,
			animation.SpeedRatio);
	}

	static unsigned long long TimelineActiveDurationMilliseconds(
		const RuntimeAnimation& animation) noexcept
	{
		const auto duration = TimelineActiveDurationExact(animation);
		const auto maximum = (std::numeric_limits<unsigned long long>::max)();
		if (!std::isfinite(duration)
			|| duration >= static_cast<long double>(maximum)) return maximum;
		return static_cast<unsigned long long>(std::ceil(duration));
	}

	static unsigned long long TimelineActiveDurationMilliseconds(
		const ActiveAnimation& animation) noexcept
	{
		const auto duration = TimelineActiveDurationExact(animation);
		const auto maximum = (std::numeric_limits<unsigned long long>::max)();
		if (!std::isfinite(duration)
			|| duration >= static_cast<long double>(maximum)) return maximum;
		return static_cast<unsigned long long>(std::ceil(duration));
	}

	static long double ApplyTimelineAcceleration(
		long double simpleElapsed,
		long double simpleDuration,
		double accelerationRatio,
		double decelerationRatio) noexcept
	{
		if (simpleDuration <= 0.0L) return 0.0L;
		const long double transition = static_cast<long double>(
			accelerationRatio + decelerationRatio);
		if (transition <= 0.0L) return simpleElapsed;
		auto progress = (std::clamp)(
			simpleElapsed / simpleDuration, 0.0L, 1.0L);
		const auto acceleration = static_cast<long double>(accelerationRatio);
		const auto deceleration = static_cast<long double>(decelerationRatio);
		const auto maximumRate = 2.0L / (2.0L - transition);
		if (progress < acceleration)
			progress = maximumRate * progress * progress
				/ (2.0L * acceleration);
		else if (progress <= 1.0L - deceleration)
			progress = maximumRate * (progress - acceleration / 2.0L);
		else
		{
			const auto complement = 1.0L - progress;
			progress = 1.0L - maximumRate * complement * complement
				/ (2.0L * deceleration);
		}
		return (std::clamp)(progress, 0.0L, 1.0L) * simpleDuration;
	}

	static bool ComposeAnimationValue(
		const ActiveAnimation& animation,
		const BindingValue& localValue,
		long double completedIterations,
		BindingValue& output)
	{
		if (animation.Kind == DeclarativeAnimationKind::Object)
		{
			output = localValue;
			return true;
		}
		BindingValue value = localValue;
		if (animation.IsCumulative && completedIterations > 0.0L)
		{
			BindingValue delta;
			if (!animation.KeyFrames.empty())
				delta = animation.KeyFrames.back().Value;
			else if (!CombineAnimationValues(
				animation, animation.To, animation.From, -1.0L, delta))
				return false;
			BindingValue accumulated;
			if (!CombineAnimationValues(animation, value, delta,
				completedIterations, accumulated)) return false;
			value = std::move(accumulated);
		}
		return AddAnimationValues(
			animation, value, animation.Foundation, output);
	}

	static bool Interpolate(
		const ActiveAnimation& animation,
		unsigned long long activeElapsedMilliseconds,
		BindingValue& output)
	{
		const auto activeDurationExact = TimelineActiveDurationExact(animation);
		long double parentElapsed = static_cast<long double>(
			activeElapsedMilliseconds);
		const bool atActiveBoundary = std::isfinite(activeDurationExact)
			&& parentElapsed >= activeDurationExact;
		if (std::isfinite(activeDurationExact))
			parentElapsed = (std::min)(parentElapsed, activeDurationExact);
		long double simpleElapsed = parentElapsed
			* static_cast<long double>(animation.SpeedRatio);
		const auto simpleDuration = static_cast<long double>(
			animation.DurationMilliseconds);
		long double completedIterations = 0.0L;
		if (simpleDuration > 0.0L)
		{
			const auto repetitionDuration = simpleDuration
				* (animation.AutoReverse ? 2.0L : 1.0L);
			if (!std::isfinite(simpleElapsed)) return false;
			completedIterations = std::floor(simpleElapsed / repetitionDuration);
			auto local = std::fmod(simpleElapsed, repetitionDuration);
			// Interior repetition boundaries begin the next iteration. At the
			// finite active-period boundary, sample the completed repetition so
			// HoldEnd gets To (or From after an auto-reverse), matching WPF.
			if (atActiveBoundary
				&& simpleElapsed > 0.0L
				&& std::fabs(local) < 0.0000001L)
			{
				local = repetitionDuration;
				completedIterations = (std::max)(
					0.0L, completedIterations - 1.0L);
			}
			if (animation.AutoReverse && local > simpleDuration)
				local = repetitionDuration - local;
			simpleElapsed = (std::clamp)(
				local, 0.0L, simpleDuration);
		}
		else simpleElapsed = 0.0L;
		simpleElapsed = ApplyTimelineAcceleration(simpleElapsed, simpleDuration,
			animation.AccelerationRatio, animation.DecelerationRatio);
		if (animation.KeyFrames.empty())
		{
			const double progress = animation.DurationMilliseconds == 0
				? 1.0
				: (std::min)(1.0,
					static_cast<double>(simpleElapsed)
						/ static_cast<double>(animation.DurationMilliseconds));
			BindingValue localValue;
			if (!InterpolateValues(animation.Kind, animation.Metadata,
				ObjectPathUsesFloat(animation.ObjectPath),
				animation.From, animation.To,
				Ease(progress, animation.Easing, animation.EasingMode), localValue))
				return false;
			return ComposeAnimationValue(
				animation, localValue, completedIterations, output);
		}

		BindingValue previousValue = animation.From;
		unsigned long long previousTime = 0;
		for (const auto& keyFrame : animation.KeyFrames)
		{
			if (simpleElapsed
				< static_cast<long double>(keyFrame.KeyTimeMilliseconds))
			{
				if (keyFrame.Kind == DeclarativeKeyFrameKind::Discrete)
				{
					return ComposeAnimationValue(animation, previousValue,
						completedIterations, output);
				}
				const auto span = keyFrame.KeyTimeMilliseconds - previousTime;
				const double segmentProgress = span == 0 ? 1.0
					: static_cast<double>(simpleElapsed
						- static_cast<long double>(previousTime))
						/ static_cast<double>(span);
				double eased = segmentProgress;
				if (keyFrame.Kind == DeclarativeKeyFrameKind::Easing)
					eased = Ease(segmentProgress,
						keyFrame.Easing, keyFrame.EasingMode);
				else if (keyFrame.Kind == DeclarativeKeyFrameKind::Spline)
					eased = KeySplineProgress(segmentProgress, keyFrame);
				BindingValue localValue;
				if (!InterpolateValues(animation.Kind, animation.Metadata,
					ObjectPathUsesFloat(animation.ObjectPath), previousValue,
					keyFrame.Value, eased, localValue)) return false;
				return ComposeAnimationValue(animation, localValue,
					completedIterations, output);
			}
			previousTime = keyFrame.KeyTimeMilliseconds;
			previousValue = keyFrame.Value;
		}
		return ComposeAnimationValue(animation, previousValue,
			completedIterations, output);
	}

	struct AnimationFrameValue
	{
		const ActiveAnimation* Animation = nullptr;
		BindingValue Value;
	};

	static DependencyPropertyValueSource AnimationValueSource(
		const ActiveAnimation&) noexcept
	{
		return DependencyPropertyValueSource::Animation;
	}

	bool TryReadAnimationFrameRoot(
		Control* target,
		const DependencyPropertyMetadata* metadata,
		const std::wstring& propertyName,
		DependencyPropertyValueSource source,
		BindingValue& output)
	{
		if (!target || !metadata) return false;
		return target->TryGetPropertyValue(propertyName, source, output)
			|| metadata->TryGet(*target, output);
	}

	bool ApplyAnimationFrame(
		const std::vector<AnimationFrameValue>& values)
	{
		struct ObjectFrame
		{
			Control* Target = nullptr;
			const DependencyPropertyMetadata* Metadata = nullptr;
			std::wstring PropertyName;
			DependencyPropertyValueSource Source =
				DependencyPropertyValueSource::Animation;
			BindingValue Value;
		};
		std::vector<ObjectFrame> objects;
		for (const auto& frame : values)
		{
			const auto* animation = frame.Animation;
			if (!animation || !animation->Target || !animation->Metadata)
				return false;
			const auto source = AnimationValueSource(*animation);
			if (!animation->ObjectPath)
			{
				if (!animation->Target->TrySetPropertyValue(
					animation->PropertyName, frame.Value,
					source)) return false;
				continue;
			}
			auto found = std::find_if(objects.begin(), objects.end(),
			[&](const auto& candidate)
			{
				return candidate.Target == animation->Target
					&& candidate.Metadata == animation->Metadata
					&& candidate.Source == source;
			});
			if (found == objects.end())
			{
				BindingValue current;
				if (!TryReadAnimationFrameRoot(animation->Target,
					animation->Metadata, animation->PropertyName, source, current)
					|| !NormalizeObjectPathRoot(*animation, current))
					return false;
				objects.push_back({ animation->Target, animation->Metadata,
					animation->PropertyName, source, std::move(current) });
				found = std::prev(objects.end());
			}
			if (!TryWriteObjectPathMember(
				found->Value, *animation->ObjectPath, frame.Value)) return false;
		}
		for (auto& object : objects)
			if (!object.Target->TrySetPropertyValue(
				object.PropertyName, object.Value,
				object.Source)) return false;
		return true;
	}

	void ReleaseStoppedAnimationValues(
		const std::vector<const ActiveAnimation*>& stoppingAnimations,
		const std::vector<ActiveAnimation>& animations,
		unsigned long long nowMilliseconds) noexcept
	{
		for (const auto* stopping : stoppingAnimations)
		{
			if (!stopping || !stopping->Target || !stopping->Metadata) continue;
			const auto source = AnimationValueSource(*stopping);
			const auto siblingAffectsRoot = std::any_of(
				animations.begin(), animations.end(), [&](const auto& candidate)
				{
					if (&candidate == stopping
						|| candidate.IsEventStoryboard
							!= stopping->IsEventStoryboard
						|| candidate.Target != stopping->Target
						|| !EqualName(candidate.PropertyName,
							stopping->PropertyName)) return false;
					const auto clockTick = candidate.Paused
						? candidate.PauseTick : nowMilliseconds;
					const auto elapsed = clockTick >= candidate.StartTick
						? clockTick - candidate.StartTick : 0;
					if (elapsed < candidate.BeginTimeMilliseconds) return false;
					if (candidate.Completed)
						return candidate.FillBehavior
							== DeclarativeTimelineFillBehavior::HoldEnd;
					const auto completed = elapsed - candidate.BeginTimeMilliseconds
						>= TimelineActiveDurationMilliseconds(candidate);
					return !completed || candidate.FillBehavior
						== DeclarativeTimelineFillBehavior::HoldEnd;
				});
			if (!stopping->ObjectPath)
			{
				if (!siblingAffectsRoot)
					(void)stopping->Target->ClearPropertyValue(
						stopping->PropertyName, source);
				continue;
			}
			if (!siblingAffectsRoot)
			{
				(void)stopping->Target->ClearPropertyValue(
					stopping->PropertyName, source);
				continue;
			}
			BindingValue root;
			if (!(stopping->Target->TryGetPropertyValue(
				stopping->PropertyName, source, root)
					|| stopping->Metadata->TryGet(*stopping->Target, root))) continue;
			if (!NormalizeObjectPathRoot(*stopping, root)) continue;
			if (TryWriteObjectPathMember(
				root, *stopping->ObjectPath, stopping->Base))
				(void)stopping->Target->TrySetPropertyValue(
					stopping->PropertyName, root, source);
		}
	}

	bool HasActiveAnimations() const noexcept
	{
		return std::any_of(ActiveAnimations.begin(), ActiveAnimations.end(),
			[](const auto& animation)
			{ return !animation.Completed && !animation.Paused; })
			|| std::any_of(Groups.begin(), Groups.end(),
				[](const auto& group) { return group.Pending.has_value(); });
	}

	bool AdvanceAnimations(unsigned long long nowMilliseconds)
	{
		if (!HasActiveAnimations()) return false;
		const bool hadActive = true;
		Applying = true;
		std::vector<AnimationFrameValue> frameValues;
		std::vector<const ActiveAnimation*> stoppingAnimations;
		frameValues.reserve(ActiveAnimations.size());
		for (auto& animation : ActiveAnimations)
		{
			const auto clockTick = animation.Paused
				? animation.PauseTick : nowMilliseconds;
			const auto elapsed = clockTick >= animation.StartTick
				? clockTick - animation.StartTick : 0;
			if (elapsed < animation.BeginTimeMilliseconds) continue;
			const auto activeDuration =
				TimelineActiveDurationMilliseconds(animation);
			const auto activeElapsed = animation.Completed
				? activeDuration : elapsed - animation.BeginTimeMilliseconds;
			BindingValue value;
			if (Interpolate(animation, activeElapsed, value))
				frameValues.push_back({ &animation, std::move(value) });
			if (!animation.Completed && animation.FillBehavior
				== DeclarativeTimelineFillBehavior::Stop
				&& activeElapsed >= activeDuration)
				stoppingAnimations.push_back(&animation);
		}
		(void)ApplyAnimationFrame(frameValues);
		ReleaseStoppedAnimationValues(
			stoppingAnimations, ActiveAnimations, nowMilliseconds);
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](auto& animation)
			{
				const auto clockTick = animation.Paused
					? animation.PauseTick : nowMilliseconds;
				const auto elapsed = clockTick >= animation.StartTick
					? clockTick - animation.StartTick : 0;
				const bool completed = animation.Completed
					|| (elapsed >= animation.BeginTimeMilliseconds
					&& elapsed - animation.BeginTimeMilliseconds
						>= TimelineActiveDurationMilliseconds(animation));
				if (!completed) return false;
				if (animation.IsEventStoryboard && animation.FillBehavior
					== DeclarativeTimelineFillBehavior::HoldEnd)
				{
					animation.Completed = true;
					return false;
				}
				return true;
			}), ActiveAnimations.end());
		if (!stoppingAnimations.empty())
			(void)ApplyRetainedAnimationFrame(nowMilliseconds);
		Applying = false;
		for (size_t groupIndex = 0; groupIndex < Groups.size(); ++groupIndex)
		{
			auto& group = Groups[groupIndex];
			if (!group.Pending || nowMilliseconds < group.Pending->EndTick) continue;
			const auto targetState = group.Pending->TargetState;
			auto transitionProperties = std::move(group.Pending->Properties);
			group.Pending.reset();
			ActiveAnimations.erase(std::remove_if(
				ActiveAnimations.begin(), ActiveAnimations.end(),
				[&](const auto& animation)
				{ return !animation.IsEventStoryboard
					&& animation.GroupIndex == groupIndex; }),
				ActiveAnimations.end());
			if (targetState < group.States.size()
				&& GoToImmediate(groupIndex, targetState, nullptr,
					nowMilliseconds, false))
				ClearTransitionOnlyProperties(
					transitionProperties, group.States[targetState]);
		}
		return hadActive;
	}

	void ClearAppliedValues() noexcept
	{
		if (!Owner) return;
		Applying = true;
		std::vector<PropertyKey> stateValuesCleared;
		std::vector<PropertyKey> animationValuesCleared;
		auto clearOnce = [](std::vector<PropertyKey>& cleared,
			const PropertyKey& key, DependencyPropertyValueSource source)
		{
			if (!key.Target || std::any_of(cleared.begin(), cleared.end(),
				[&](const auto& existing) { return SameProperty(existing, key); }))
				return;
			cleared.push_back(key);
			(void)key.Target->ClearPropertyValue(key.PropertyName, source);
		};
		for (const auto& group : Groups)
		{
			if (group.Pending)
				for (const auto& key : group.Pending->Properties)
					clearOnce(animationValuesCleared, key,
						DependencyPropertyValueSource::Animation);
			if (!group.CurrentState || *group.CurrentState >= group.States.size())
				continue;
			for (const auto& setter : group.States[*group.CurrentState].Setters)
				clearOnce(stateValuesCleared,
					{ setter.Target, setter.PropertyName },
					DependencyPropertyValueSource::VisualState);
			for (const auto& animation : group.States[*group.CurrentState].Animations)
				clearOnce(animationValuesCleared,
					{ animation.Target, animation.PropertyName },
					DependencyPropertyValueSource::Animation);
		}
		for (const auto& storyboard : EventStoryboards)
			for (const auto& animation : storyboard.Animations)
				clearOnce(animationValuesCleared,
					{ animation.Target, animation.PropertyName },
					DependencyPropertyValueSource::Animation);
		Applying = false;
	}

	bool StateMatches(const RuntimeState& state) const
	{
		if (state.Conditions.empty()) return false;
		for (const auto& condition : state.Conditions)
		{
			BindingValue actual;
			if (!condition.Metadata
				|| !condition.Metadata->TryGet(*Owner, actual)
				|| !condition.Metadata->ValuesEqual(actual, condition.Value))
				return false;
		}
		return true;
	}

	size_t EvaluateState(const RuntimeGroup& group) const
	{
		for (size_t index = 0; index < group.States.size(); ++index)
			if (!group.States[index].Conditions.empty()
				&& StateMatches(group.States[index])) return index;
		return group.FallbackState;
	}

	bool RestoreSnapshots(const std::vector<PropertySnapshot>& snapshots) noexcept
	{
		bool restored = true;
		for (const auto& snapshot : snapshots)
		{
			if (!snapshot.Key.Target) continue;
			if (snapshot.Value)
				restored = snapshot.Key.Target->TrySetPropertyValue(
					snapshot.Key.PropertyName, *snapshot.Value,
					snapshot.Source) && restored;
			else if (snapshot.Key.Target->HasPropertyValue(
				snapshot.Key.PropertyName, snapshot.Source))
				restored = snapshot.Key.Target->ClearPropertyValue(
					snapshot.Key.PropertyName, snapshot.Source) && restored;
		}
		return restored;
	}

	bool GoToImmediate(
		size_t groupIndex,
		size_t stateIndex,
		std::wstring* outError,
		std::optional<unsigned long long> requestedStartTick,
		bool force)
	{
		if (groupIndex >= Groups.size()
			|| stateIndex >= Groups[groupIndex].States.size())
		{
			if (outError) *outError = L"视觉状态索引无效。";
			return false;
		}
		auto& group = Groups[groupIndex];
		if (!force && group.CurrentState && *group.CurrentState == stateIndex)
		{
			if (outError) outError->clear();
			return true;
		}
		const RuntimeState* previous = group.CurrentState
			? &group.States[*group.CurrentState] : nullptr;
		const auto& next = group.States[stateIndex];

		std::vector<PropertyKey> affected;
		auto addAffected = [&](Control* target, const std::wstring& propertyName)
		{
			PropertyKey key{ target, propertyName };
			if (std::none_of(affected.begin(), affected.end(),
				[&](const auto& existing) { return SameProperty(existing, key); }))
				affected.push_back(std::move(key));
		};
		if (previous)
		{
			for (const auto& setter : previous->Setters)
				addAffected(setter.Target, setter.PropertyName);
			for (const auto& animation : previous->Animations)
				addAffected(animation.Target, animation.PropertyName);
		}
		for (const auto& setter : next.Setters)
			addAffected(setter.Target, setter.PropertyName);
		for (const auto& animation : next.Animations)
			addAffected(animation.Target, animation.PropertyName);

		std::vector<PropertySnapshot> snapshots;
		snapshots.reserve(affected.size() * 2);
		for (const auto& key : affected)
		{
			for (const auto source : {
				DependencyPropertyValueSource::VisualState,
				DependencyPropertyValueSource::Animation })
			{
				PropertySnapshot snapshot;
				snapshot.Key = key;
				snapshot.Source = source;
				BindingValue value;
				if (key.Target && key.Target->TryGetPropertyValue(
					key.PropertyName, source, value))
					snapshot.Value = std::move(value);
				snapshots.push_back(std::move(snapshot));
			}
		}

		unsigned long long startTick = requestedStartTick.value_or(0);
		std::vector<ActiveAnimation> pendingAnimations;
		pendingAnimations.reserve(next.Animations.size());
		for (const auto& animation : next.Animations)
		{
			BindingValue current;
			if (!TryReadAnimationValue(animation, current))
			{
				if (outError) *outError = L"视觉状态动画无法捕获起始值："
					+ animation.PropertyName;
				return false;
			}
			BindingValue base;
			if (!TryReadBaseAnimationValue(animation, base))
			{
				if (outError) *outError = L"视觉状态动画无法捕获基础值："
					+ animation.PropertyName;
				return false;
			}
			BindingValue from;
			BindingValue to;
			BindingValue foundation;
			if (!ResolveAnimationEndpoints(
				animation, current, base, from, to, foundation))
			{
				if (outError) *outError = L"视觉状态动画无法解析 From/To/By："
					+ animation.PropertyName;
				return false;
			}
			pendingAnimations.push_back({
				groupIndex, animation.Target, animation.Metadata,
				animation.PropertyName, animation.Kind,
				std::move(base), std::move(foundation),
				std::move(from), std::move(to),
				animation.KeyFrames,
				animation.IsCumulative,
				animation.ObjectPath, startTick,
				animation.BeginTimeMilliseconds,
				animation.DurationMilliseconds,
				animation.RepeatBehavior, animation.RepeatCount,
				animation.RepeatDurationMilliseconds,
				animation.AutoReverse, animation.FillBehavior,
				animation.SpeedRatio, animation.AccelerationRatio,
				animation.DecelerationRatio,
				animation.Easing, animation.EasingMode });
		}

		auto stateHasSetter = [](const RuntimeState& state,
			Control* target, const std::wstring& propertyName)
		{
			return std::any_of(state.Setters.begin(), state.Setters.end(),
				[&](const auto& candidate)
				{
					return candidate.Target == target
						&& EqualName(candidate.PropertyName, propertyName);
				});
		};
		auto stateHasAnimation = [](const RuntimeState& state,
			Control* target, const std::wstring& propertyName)
		{
			return std::any_of(state.Animations.begin(), state.Animations.end(),
				[&](const auto& candidate)
				{
					return candidate.Target == target
						&& EqualName(candidate.PropertyName, propertyName);
				});
		};

		Applying = true;
		bool success = true;
		if (previous)
		{
			for (const auto& key : affected)
			{
				if (!key.Target) continue;
				if (stateHasSetter(*previous, key.Target, key.PropertyName)
					&& !stateHasSetter(next, key.Target, key.PropertyName)
					&& key.Target->HasPropertyValue(
						key.PropertyName, DependencyPropertyValueSource::VisualState)
					&& !key.Target->ClearPropertyValue(
						key.PropertyName,
						DependencyPropertyValueSource::VisualState))
				{
					success = false;
					break;
				}
				if (stateHasAnimation(*previous, key.Target, key.PropertyName)
					&& key.Target->HasPropertyValue(
						key.PropertyName, DependencyPropertyValueSource::Animation)
					&& !key.Target->ClearPropertyValue(
						key.PropertyName, DependencyPropertyValueSource::Animation))
				{
					success = false;
					break;
				}
			}
		}
		const bool animationsEnabled = Owner->AreSystemAnimationsEnabled();
		if (success)
		{
			for (const auto& setter : next.Setters)
				if (!setter.Target || !setter.Target->TrySetPropertyValue(
					setter.PropertyName, setter.Value,
					DependencyPropertyValueSource::VisualState))
				{
					success = false;
					break;
				}
		}
		if (success)
		{
			std::vector<AnimationFrameValue> initialValues;
			initialValues.reserve(pendingAnimations.size());
			for (const auto& animation : pendingAnimations)
			{
				const auto activeDuration =
					TimelineActiveDurationMilliseconds(animation);
				const bool active = animationsEnabled
					&& (animation.BeginTimeMilliseconds > 0
						|| activeDuration > 0);
				BindingValue value;
				if (active && animation.BeginTimeMilliseconds > 0)
					value = animation.Base;
				else if (!active && animation.FillBehavior
					== DeclarativeTimelineFillBehavior::Stop)
					value = animation.Base;
				else if (!Interpolate(animation,
					active ? 0 : activeDuration, value))
				{
					success = false;
					break;
				}
				initialValues.push_back({ &animation, std::move(value) });
			}
			if (success) success = ApplyAnimationFrame(initialValues);
			if (success)
			{
				startTick = requestedStartTick.value_or(::GetTickCount64());
				for (auto& animation : pendingAnimations)
					animation.StartTick = startTick;
				std::vector<const ActiveAnimation*> stopped;
				for (const auto& animation : pendingAnimations)
					if (animation.FillBehavior
						== DeclarativeTimelineFillBehavior::Stop
						&& (!animationsEnabled
							|| TimelineActiveDurationMilliseconds(animation) == 0))
						stopped.push_back(&animation);
				ReleaseStoppedAnimationValues(stopped, pendingAnimations,
					animationsEnabled ? startTick
						: (std::numeric_limits<unsigned long long>::max)());
			}
		}
		if (!success)
		{
			(void)RestoreSnapshots(snapshots);
			Applying = false;
			if (outError) *outError = L"视觉状态 Setter/Storyboard 无法事务性应用。";
			return false;
		}

		const auto oldState = previous ? previous->Name : std::wstring{};
		group.CurrentState = stateIndex;
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](const auto& animation) { return !animation.IsEventStoryboard
				&& animation.GroupIndex == groupIndex; }),
			ActiveAnimations.end());
		if (animationsEnabled)
			for (auto& animation : pendingAnimations)
				if (animation.BeginTimeMilliseconds > 0
					|| TimelineActiveDurationMilliseconds(animation) > 0)
					ActiveAnimations.push_back(std::move(animation));
		if (!ApplyRetainedAnimationFrame(startTick))
		{
			(void)RestoreSnapshots(snapshots);
			Applying = false;
			if (outError) *outError = L"视觉状态动画时钟无法重组。";
			return false;
		}
		// The implicit clock begins when the state transaction is committed,
		// after initial frame composition. Otherwise materialization work between
		// capturing GetTickCount64 and returning from GoToVisualState leaks into
		// the first externally advanced frame. Transition completion supplies an
		// explicit tick and intentionally keeps that shared timeline origin.
		if (!requestedStartTick)
		{
			startTick = ::GetTickCount64();
			for (auto& animation : ActiveAnimations)
				if (!animation.IsEventStoryboard
					&& animation.GroupIndex == groupIndex)
					animation.StartTick = startTick;
		}
		Applying = false;
		if (std::any_of(ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](const auto& animation) { return !animation.IsEventStoryboard
				&& animation.GroupIndex == groupIndex; }))
			Owner->InvalidateVisual();
		DeclarativeVisualStateChangedEventArgs args{
			group.Name, oldState, next.Name };
		cui::framework::EventAccess::Raise(
			Owner->OnVisualStateChanged, Owner, args);
		if (outError) outError->clear();
		return true;
	}

	static unsigned long long SaturatingAdd(
		unsigned long long left,
		unsigned long long right) noexcept
	{
		const auto maximum = (std::numeric_limits<unsigned long long>::max)();
		return right > maximum - left ? maximum : left + right;
	}

	static bool SameAnimationTarget(
		const RuntimeAnimation& left,
		const RuntimeAnimation& right) noexcept
	{
		if (left.Target != right.Target
			|| !EqualName(left.PropertyName, right.PropertyName)) return false;
		const auto leftPath = ObjectPathCanonical(left.ObjectPath);
		const auto rightPath = ObjectPathCanonical(right.ObjectPath);
		return leftPath.empty() || rightPath.empty()
			? leftPath.empty() && rightPath.empty()
			: EqualName(leftPath, rightPath);
	}

	static bool StateAnimatesProperty(
		const RuntimeState& state,
		const PropertyKey& key) noexcept
	{
		return std::any_of(state.Animations.begin(), state.Animations.end(),
			[&](const auto& animation)
			{
				return animation.Target == key.Target
					&& EqualName(animation.PropertyName, key.PropertyName);
			});
	}

	template<typename TAnimation>
	bool NormalizeObjectPathRoot(
		const TAnimation& animation,
		BindingValue& root) const
	{
		if (!AsBrushPath(animation.ObjectPath)
			&& !AsBrushTransformPath(animation.ObjectPath)) return true;
		cui::drawing::Brush brush;
		if (!root.TryGet(brush)) return false;
		if (brush.Kind == cui::drawing::BrushKind::None
			&& EqualName(animation.PropertyName, L"Foreground"))
		{
			if (!animation.Target) return false;
			root = BindingValue(cui::drawing::MakeSolidColorBrush(
				animation.Target->RendererForegroundColor));
		}
		return true;
	}

	bool TryReadAnimationValue(
		const RuntimeAnimation& animation,
		BindingValue& output) const
	{
		if (!animation.Target || !animation.Metadata) return false;
		BindingValue root;
		if (!animation.Metadata->TryGet(*animation.Target, root)) return false;
		if (!animation.ObjectPath)
		{
			output = std::move(root);
			return true;
		}
		if (!NormalizeObjectPathRoot(animation, root)) return false;
		return TryReadObjectPathMember(
			root, *animation.ObjectPath, output);
	}

	bool TryReadBaseAnimationValue(
		const RuntimeAnimation& animation,
		BindingValue& output)
	{
		if (!animation.Target || !animation.Metadata) return false;
		const bool previousApplying = Applying;
		Applying = true;
		BindingValue animationValue;
		const bool hadAnimation = animation.Target->TryGetPropertyValue(
			animation.PropertyName, DependencyPropertyValueSource::Animation,
			animationValue);
		if (hadAnimation && !animation.Target->ClearPropertyValue(
			animation.PropertyName,
			DependencyPropertyValueSource::Animation))
		{
			Applying = previousApplying;
			return false;
		}
		BindingValue root;
		const bool read = animation.Metadata->TryGet(*animation.Target, root);
		const bool restoredAnimation = !hadAnimation
			|| animation.Target->TrySetPropertyValue(
				animation.PropertyName, animationValue,
				DependencyPropertyValueSource::Animation);
		Applying = previousApplying;
		if (!read || !restoredAnimation) return false;
		if (!animation.ObjectPath)
		{
			output = std::move(root);
			return true;
		}
		if (!NormalizeObjectPathRoot(animation, root)) return false;
		return TryReadObjectPathMember(
			root, *animation.ObjectPath, output);
	}

	bool TryReadValueBelowVisualState(
		const RuntimeSetter& setter,
		BindingValue& output)
	{
		if (!setter.Target) return false;
		const auto* metadata = setter.Target->FindPropertyMetadata(
			setter.PropertyName);
		if (!metadata) return false;
		Control::EffectiveValueEntry candidate;
		const auto stored = setter.Target->_propertyValues.find(metadata);
		if (stored != setter.Target->_propertyValues.end())
			candidate = stored->second;
		const int visualStateIndex = StoredPropertySourceIndex(
			DependencyPropertyValueSource::VisualState);
		const int animationIndex = StoredPropertySourceIndex(
			DependencyPropertyValueSource::Animation);
		if (visualStateIndex >= 0)
			candidate.Slots[static_cast<size_t>(visualStateIndex)].Reset();
		if (animationIndex >= 0)
			candidate.Slots[static_cast<size_t>(animationIndex)].Reset();
		DependencyPropertyValueSource ignoredSource =
			DependencyPropertyValueSource::Default;
		return setter.Target->TryEvaluateEffectivePropertyValue(
			*metadata, candidate, output, ignoredSource);
	}

	static bool EnteringAnimationValue(
		const RuntimeAnimation& animation,
		const BindingValue& current,
		BindingValue& output)
	{
		if (!animation.KeyFrames.empty())
		{
			output = animation.KeyFrames.front().Value;
			if (animation.IsAdditive)
				return AddAnimationValues(animation, output, current, output);
			return true;
		}
		if (animation.From)
		{
			output = *animation.From;
			if (animation.IsAdditive && (animation.To || animation.By))
				return AddAnimationValues(animation, output, current, output);
			return true;
		}
		if (animation.To)
		{
			output = *animation.To;
			return true;
		}
		output = current;
		return true;
	}

	static ActiveAnimation MakeActiveAnimation(
		size_t groupIndex,
		const RuntimeAnimation& animation,
		BindingValue base,
		BindingValue foundation,
		BindingValue from,
		BindingValue to,
		unsigned long long startTick)
	{
		return {
			groupIndex, animation.Target, animation.Metadata,
			animation.PropertyName, animation.Kind,
			std::move(base), std::move(foundation),
			std::move(from), std::move(to),
			animation.KeyFrames,
			animation.IsCumulative,
			animation.ObjectPath, startTick,
			animation.BeginTimeMilliseconds,
			animation.DurationMilliseconds,
			animation.RepeatBehavior, animation.RepeatCount,
			animation.RepeatDurationMilliseconds,
			animation.AutoReverse, animation.FillBehavior,
			animation.SpeedRatio, animation.AccelerationRatio,
			animation.DecelerationRatio,
			animation.Easing, animation.EasingMode };
	}

	const RuntimeTransition* FindTransition(
		const RuntimeGroup& group,
		std::optional<size_t> fromState,
		size_t toState) const noexcept
	{
		const RuntimeTransition* best = nullptr;
		const RuntimeTransition* fallback = nullptr;
		int bestScore = -1;
		for (const auto& transition : group.Transitions)
		{
			if (!transition.FromState && !transition.ToState)
			{
				if (!fallback) fallback = &transition;
				continue;
			}
			int score = -1;
			if (transition.FromState == fromState) ++score;
			else if (transition.FromState) continue;
			if (transition.ToState && *transition.ToState == toState) score += 2;
			else if (transition.ToState) continue;
			if (score > bestScore)
			{
				bestScore = score;
				best = &transition;
			}
		}
		return best ? best : fallback;
	}

	void ClearTransitionOnlyProperties(
		const std::vector<PropertyKey>& properties,
		const RuntimeState& state) noexcept
	{
		for (const auto& key : properties)
			if (key.Target && !StateAnimatesProperty(state, key))
				(void)key.Target->ClearPropertyValue(
					key.PropertyName, DependencyPropertyValueSource::Animation);
	}

	bool GoTo(
		size_t groupIndex,
		size_t stateIndex,
		bool useTransitions,
		std::wstring* outError)
	{
		if (groupIndex >= Groups.size()
			|| stateIndex >= Groups[groupIndex].States.size())
		{
			if (outError) *outError = L"视觉状态索引无效。";
			return false;
		}
		auto& group = Groups[groupIndex];
		const auto logicalCurrent = group.Pending
			? std::optional<size_t>(group.Pending->TargetState)
			: group.CurrentState;
		if (logicalCurrent && *logicalCurrent == stateIndex
			&& (!group.Pending || useTransitions))
		{
			if (outError) outError->clear();
			return true;
		}
		const auto now = ::GetTickCount64();
		const auto* transition = useTransitions
			&& Owner->AreSystemAnimationsEnabled()
			? FindTransition(group, logicalCurrent, stateIndex) : nullptr;
		unsigned long long totalDuration = transition
			? transition->GeneratedDurationMilliseconds : 0;
		if (transition)
			for (const auto& animation : transition->Animations)
				totalDuration = (std::max)(totalDuration,
					SaturatingAdd(animation.BeginTimeMilliseconds,
						TimelineActiveDurationMilliseconds(animation)));

		if (!transition || totalDuration == 0)
		{
			std::vector<PropertyKey> oldTransitionProperties;
			const bool force = group.Pending.has_value();
			if (group.Pending)
				oldTransitionProperties = std::move(group.Pending->Properties);
			group.Pending.reset();
			ActiveAnimations.erase(std::remove_if(
				ActiveAnimations.begin(), ActiveAnimations.end(),
				[&](const auto& animation)
				{ return !animation.IsEventStoryboard
					&& animation.GroupIndex == groupIndex; }),
				ActiveAnimations.end());
			if (!GoToImmediate(
				groupIndex, stateIndex, outError, std::nullopt, force)) return false;
			ClearTransitionOnlyProperties(
				oldTransitionProperties, group.States[stateIndex]);
			return true;
		}

		const RuntimeState* fromState = logicalCurrent
			&& *logicalCurrent < group.States.size()
			? &group.States[*logicalCurrent] : nullptr;
		const auto& toState = group.States[stateIndex];
		std::vector<ActiveAnimation> pendingAnimations;
		std::vector<PropertyKey> pendingProperties;
		auto addProperty = [&](const RuntimeAnimation& animation)
		{
			PropertyKey key{ animation.Target, animation.PropertyName };
			if (std::none_of(pendingProperties.begin(), pendingProperties.end(),
				[&](const auto& existing) { return SameProperty(existing, key); }))
				pendingProperties.push_back(std::move(key));
		};
		auto explicitlyControls = [&](const RuntimeAnimation& candidate)
		{
			return std::any_of(transition->Animations.begin(),
				transition->Animations.end(), [&](const auto& explicitAnimation)
					{ return SameAnimationTarget(candidate, explicitAnimation); });
		};
		auto explicitlyControlsProperty = [&](Control* target,
			const std::wstring& propertyName)
		{
			return std::any_of(transition->Animations.begin(),
				transition->Animations.end(), [&](const auto& explicitAnimation)
				{
					return explicitAnimation.Target == target
						&& EqualName(explicitAnimation.PropertyName, propertyName);
				});
		};
		auto stateControlsProperty = [](const RuntimeState& state,
			Control* target, const std::wstring& propertyName)
		{
			return std::any_of(state.Setters.begin(), state.Setters.end(),
				[&](const auto& setter)
				{
					return setter.Target == target
						&& EqualName(setter.PropertyName, propertyName);
				}) || std::any_of(state.Animations.begin(), state.Animations.end(),
				[&](const auto& animation)
				{
					return animation.Target == target
						&& EqualName(animation.PropertyName, propertyName);
				});
		};
		auto addGeneratedSetterAnimation = [&](const RuntimeSetter& setter,
			BindingValue destination, const std::wstring& context)
		{
			if (!setter.Target) return false;
			const auto* metadata = setter.Target->FindPropertyMetadata(
				setter.PropertyName);
			if (!metadata) return false;
			const auto kind = GeneratedAnimationKind(*metadata);
			if (!kind) return true;
			RuntimeAnimation generated;
			generated.Kind = *kind;
			generated.Target = setter.Target;
			generated.Metadata = metadata;
			generated.PropertyName = metadata->Name();
			generated.DurationMilliseconds =
				transition->GeneratedDurationMilliseconds;
			generated.Easing = transition->GeneratedEasing;
			generated.EasingMode = transition->GeneratedEasingMode;
			BindingValue current;
			if (!TryReadAnimationValue(generated, current))
			{
				if (outError) *outError = L"VisualTransition 无法读取"
					+ context + L" Setter 起始值：" + setter.PropertyName;
				return false;
			}
			generated.From = current;
			generated.To = destination;
			BindingValue foundation = ZeroAnimationValue(generated);
			pendingAnimations.push_back(MakeActiveAnimation(
				groupIndex, generated, current, std::move(foundation),
				std::move(current), std::move(destination), now));
			addProperty(generated);
			return true;
		};
		Applying = true;
		if (transition->GeneratedDurationMilliseconds > 0)
		{
			for (const auto& setter : toState.Setters)
			{
				if (explicitlyControlsProperty(
					setter.Target, setter.PropertyName)) continue;
				if (!addGeneratedSetterAnimation(
					setter, setter.Value, L"进入"))
				{
					Applying = false;
					return false;
				}
			}
			if (fromState)
				for (const auto& setter : fromState->Setters)
				{
					if (stateControlsProperty(toState,
						setter.Target, setter.PropertyName)
						|| explicitlyControlsProperty(
							setter.Target, setter.PropertyName)) continue;
					BindingValue destination;
					if (!TryReadValueBelowVisualState(setter, destination)
						|| !addGeneratedSetterAnimation(
							setter, std::move(destination), L"退出"))
					{
						Applying = false;
						if (outError && outError->empty())
							*outError = L"VisualTransition 无法读取退出 Setter 基础值："
								+ setter.PropertyName;
						return false;
					}
				}
		}
		auto addObjectBaseHold = [&](const RuntimeAnimation& animation)
		{
			BindingValue current;
			BindingValue base;
			if (!TryReadAnimationValue(animation, current)
				|| !TryReadBaseAnimationValue(animation, base)) return false;
			auto generated = animation;
			generated.From.reset();
			generated.To.reset();
			generated.By.reset();
			generated.IsAdditive = false;
			generated.IsCumulative = false;
			generated.KeyFrames = { DeclarativeAnimationKeyFrame{
				DeclarativeKeyFrameKind::Discrete, 0, base } };
			generated.BeginTimeMilliseconds = 0;
			generated.DurationMilliseconds = totalDuration;
			generated.RepeatBehavior = DeclarativeRepeatBehaviorKind::Count;
			generated.RepeatCount = 1.0;
			generated.RepeatDurationMilliseconds = 0;
			generated.AutoReverse = false;
			generated.FillBehavior = DeclarativeTimelineFillBehavior::HoldEnd;
			generated.SpeedRatio = 1.0;
			generated.AccelerationRatio = 0.0;
			generated.DecelerationRatio = 0.0;
			generated.Easing = DeclarativeEasingKind::Linear;
			generated.EasingMode = DeclarativeEasingMode::EaseOut;
			pendingAnimations.push_back(MakeActiveAnimation(
				groupIndex, generated, base, BindingValue{},
				std::move(current), std::move(base), now));
			addProperty(generated);
			return true;
		};
		for (const auto& animation : toState.Animations)
			if (animation.Kind == DeclarativeAnimationKind::Object
				&& !explicitlyControls(animation)
				&& !addObjectBaseHold(animation))
			{
				Applying = false;
				if (outError) *outError = L"VisualTransition 无法释放 Object 动画基础值："
					+ animation.PropertyName;
				return false;
			}
		if (fromState)
			for (const auto& animation : fromState->Animations)
			{
				if (animation.Kind != DeclarativeAnimationKind::Object
					|| explicitlyControls(animation)
					|| std::any_of(toState.Animations.begin(),
						toState.Animations.end(), [&](const auto& nextAnimation)
						{ return SameAnimationTarget(animation, nextAnimation); }))
					continue;
				if (!addObjectBaseHold(animation))
				{
					Applying = false;
					if (outError) *outError = L"VisualTransition 无法释放 Object 动画基础值："
						+ animation.PropertyName;
					return false;
				}
			}
		if (transition->GeneratedDurationMilliseconds > 0)
		{
			for (const auto& animation : toState.Animations)
			{
				if (animation.Kind == DeclarativeAnimationKind::Object
					|| explicitlyControls(animation)) continue;
				BindingValue from;
				if (!TryReadAnimationValue(animation, from))
				{
					Applying = false;
					if (outError) *outError = L"VisualTransition 无法读取进入动画起始值："
						+ animation.PropertyName;
					return false;
				}
				auto generated = animation;
				generated.From = from;
				BindingValue to;
				if (!EnteringAnimationValue(animation, from, to))
				{
					Applying = false;
					if (outError) *outError = L"VisualTransition 无法合成进入动画值："
						+ animation.PropertyName;
					return false;
				}
				generated.To = to;
				generated.By.reset();
				generated.IsAdditive = false;
				generated.IsCumulative = false;
				generated.KeyFrames.clear();
				generated.BeginTimeMilliseconds = 0;
				generated.DurationMilliseconds =
					transition->GeneratedDurationMilliseconds;
				generated.RepeatBehavior =
					DeclarativeRepeatBehaviorKind::Count;
				generated.RepeatCount = 1.0;
				generated.RepeatDurationMilliseconds = 0;
				generated.AutoReverse = false;
				generated.FillBehavior =
					DeclarativeTimelineFillBehavior::HoldEnd;
				generated.SpeedRatio = 1.0;
				generated.AccelerationRatio = 0.0;
				generated.DecelerationRatio = 0.0;
				generated.Easing = transition->GeneratedEasing;
				generated.EasingMode = transition->GeneratedEasingMode;
				BindingValue base = from;
				BindingValue foundation = ZeroAnimationValue(generated);
				pendingAnimations.push_back(MakeActiveAnimation(
					groupIndex, generated, std::move(base),
					std::move(foundation),
					std::move(from), std::move(to), now));
				addProperty(generated);
			}
			if (fromState)
				for (const auto& animation : fromState->Animations)
				{
					if (animation.Kind == DeclarativeAnimationKind::Object
						|| explicitlyControls(animation)
						|| std::any_of(toState.Animations.begin(),
							toState.Animations.end(), [&](const auto& nextAnimation)
							{ return SameAnimationTarget(animation, nextAnimation); }))
						continue;
					BindingValue from;
					BindingValue to;
					if (!TryReadAnimationValue(animation, from)
						|| !TryReadBaseAnimationValue(animation, to))
					{
						Applying = false;
						if (outError) *outError = L"VisualTransition 无法生成退出动画："
							+ animation.PropertyName;
						return false;
					}
					auto generated = animation;
					generated.From = from;
					generated.To = to;
					generated.By.reset();
					generated.IsAdditive = false;
					generated.IsCumulative = false;
					generated.KeyFrames.clear();
					generated.BeginTimeMilliseconds = 0;
					generated.DurationMilliseconds =
						transition->GeneratedDurationMilliseconds;
					generated.RepeatBehavior =
						DeclarativeRepeatBehaviorKind::Count;
					generated.RepeatCount = 1.0;
					generated.RepeatDurationMilliseconds = 0;
					generated.AutoReverse = false;
					generated.FillBehavior =
						DeclarativeTimelineFillBehavior::HoldEnd;
					generated.SpeedRatio = 1.0;
					generated.AccelerationRatio = 0.0;
					generated.DecelerationRatio = 0.0;
					generated.Easing = transition->GeneratedEasing;
					generated.EasingMode = transition->GeneratedEasingMode;
					BindingValue base = from;
					BindingValue foundation = ZeroAnimationValue(generated);
					pendingAnimations.push_back(MakeActiveAnimation(
						groupIndex, generated, std::move(base),
						std::move(foundation),
						std::move(from), std::move(to), now));
					addProperty(generated);
				}
		}
		for (const auto& animation : transition->Animations)
		{
			BindingValue base;
			if (!TryReadAnimationValue(animation, base))
			{
				Applying = false;
				if (outError) *outError = L"VisualTransition Storyboard 无法捕获基础值："
					+ animation.PropertyName;
				return false;
			}
			BindingValue from;
			BindingValue to;
			BindingValue foundation;
			if (!ResolveAnimationEndpoints(
				animation, base, base, from, to, foundation))
			{
				Applying = false;
				if (outError) *outError = L"VisualTransition Storyboard 无法解析 From/To/By："
					+ animation.PropertyName;
				return false;
			}
			pendingAnimations.push_back(MakeActiveAnimation(
				groupIndex, animation, std::move(base),
				std::move(foundation),
				std::move(from), std::move(to), now));
			addProperty(animation);
		}

		std::vector<PropertyKey> oldTransitionProperties;
		if (group.Pending)
			oldTransitionProperties = group.Pending->Properties;
		std::vector<PropertyKey> changedProperties = oldTransitionProperties;
		for (const auto& key : pendingProperties)
			if (std::none_of(changedProperties.begin(), changedProperties.end(),
				[&](const auto& existing) { return SameProperty(existing, key); }))
				changedProperties.push_back(key);
		std::vector<PropertySnapshot> snapshots;
		snapshots.reserve(changedProperties.size() * 2);
		for (const auto& key : changedProperties)
		{
			for (const auto source : {
				DependencyPropertyValueSource::VisualState,
				DependencyPropertyValueSource::Animation })
			{
				PropertySnapshot snapshot;
				snapshot.Key = key;
				snapshot.Source = source;
				BindingValue value;
				if (key.Target && key.Target->TryGetPropertyValue(
					key.PropertyName, source, value))
					snapshot.Value = std::move(value);
				snapshots.push_back(std::move(snapshot));
			}
		}
		for (const auto& key : oldTransitionProperties)
			if (key.Target && key.Target->HasPropertyValue(
				key.PropertyName, DependencyPropertyValueSource::Animation))
				(void)key.Target->ClearPropertyValue(
					key.PropertyName, DependencyPropertyValueSource::Animation);
		std::vector<AnimationFrameValue> initialValues;
		initialValues.reserve(pendingAnimations.size());
		for (const auto& animation : pendingAnimations)
		{
			BindingValue value;
			if (animation.BeginTimeMilliseconds > 0
				|| (TimelineActiveDurationMilliseconds(animation) == 0
					&& animation.FillBehavior
						== DeclarativeTimelineFillBehavior::Stop))
				value = animation.Base;
			else if (!Interpolate(animation, 0, value))
			{
				(void)RestoreSnapshots(snapshots);
				Applying = false;
				if (outError) *outError = L"VisualTransition 初始帧无效。";
				return false;
			}
			initialValues.push_back({ &animation, std::move(value) });
		}
		if (!ApplyAnimationFrame(initialValues))
		{
			(void)RestoreSnapshots(snapshots);
			Applying = false;
			if (outError) *outError = L"VisualTransition 无法事务性应用。";
			return false;
		}
		const auto startTick = ::GetTickCount64();
		for (auto& animation : pendingAnimations)
			animation.StartTick = startTick;
		std::vector<const ActiveAnimation*> initiallyStopped;
		for (const auto& animation : pendingAnimations)
			if (animation.FillBehavior
				== DeclarativeTimelineFillBehavior::Stop
				&& animation.BeginTimeMilliseconds == 0
				&& TimelineActiveDurationMilliseconds(animation) == 0)
				initiallyStopped.push_back(&animation);
		ReleaseStoppedAnimationValues(
			initiallyStopped, pendingAnimations, startTick);
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](const auto& animation)
			{ return !animation.IsEventStoryboard
				&& animation.GroupIndex == groupIndex; }),
			ActiveAnimations.end());
		for (auto& animation : pendingAnimations)
			ActiveAnimations.push_back(std::move(animation));
		group.Pending = PendingTransition{
			stateIndex, SaturatingAdd(startTick, totalDuration),
			std::move(pendingProperties) };
		Applying = false;
		Owner->InvalidateVisual();
		if (outError) outError->clear();
		return true;
	}

	bool EvaluateGroup(size_t groupIndex, std::wstring* outError = nullptr)
	{
		if (groupIndex >= Groups.size()) return false;
		return GoTo(groupIndex, EvaluateState(Groups[groupIndex]), true, outError);
	}

	void OnHostPropertyChanged(const DependencyPropertyChangedEventArgs& args)
	{
		if (Applying) return;
		for (size_t index = 0; index < Groups.size(); ++index)
			if (ContainsName(Groups[index].ConditionProperties, args.PropertyName))
				(void)EvaluateGroup(index);
	}

	bool ApplyRetainedAnimationFrame(unsigned long long nowMilliseconds)
	{
		std::vector<AnimationFrameValue> values;
		values.reserve(ActiveAnimations.size());
		for (const auto& animation : ActiveAnimations)
		{
			const auto clockTick = animation.Paused
				? animation.PauseTick : nowMilliseconds;
			const auto elapsed = clockTick >= animation.StartTick
				? clockTick - animation.StartTick : 0;
			if (elapsed < animation.BeginTimeMilliseconds) continue;
			const auto activeDuration =
				TimelineActiveDurationMilliseconds(animation);
			BindingValue value;
			if (!Interpolate(animation, animation.Completed
				? activeDuration
				: elapsed - animation.BeginTimeMilliseconds, value)) return false;
			values.push_back({ &animation, std::move(value) });
		}
		return ApplyAnimationFrame(values);
	}

	bool BeginEventStoryboard(size_t storyboardIndex, std::wstring* outError)
	{
		if (storyboardIndex >= EventStoryboards.size())
		{
			if (outError) *outError = L"BeginStoryboard 索引无效。";
			return false;
		}
		const auto& storyboard = EventStoryboards[storyboardIndex];
		std::vector<PropertySnapshot> snapshots;
		for (const auto& animation : storyboard.Animations)
		{
			PropertyKey key{ animation.Target, animation.PropertyName };
			if (std::any_of(snapshots.begin(), snapshots.end(),
				[&](const auto& existing)
				{ return SameProperty(existing.Key, key); })) continue;
			PropertySnapshot snapshot;
			snapshot.Key = key;
			snapshot.Source = DependencyPropertyValueSource::Animation;
			BindingValue value;
			if (key.Target && key.Target->TryGetPropertyValue(
				key.PropertyName, DependencyPropertyValueSource::Animation, value))
				snapshot.Value = std::move(value);
			snapshots.push_back(std::move(snapshot));
		}

		std::vector<ActiveAnimation> pending;
		pending.reserve(storyboard.Animations.size());
		for (const auto& animation : storyboard.Animations)
		{
			BindingValue current;
			if (!TryReadAnimationValue(animation, current))
			{
				if (outError) *outError = L"BeginStoryboard 无法捕获当前值："
					+ animation.PropertyName;
				return false;
			}
			BindingValue from;
			BindingValue to;
			BindingValue foundation;
			if (!ResolveAnimationEndpoints(animation, current, current,
				from, to, foundation))
			{
				if (outError) *outError = L"BeginStoryboard 无法解析 From/To/By："
					+ animation.PropertyName;
				return false;
			}
			auto active = MakeActiveAnimation(storyboardIndex, animation,
				current, std::move(foundation), std::move(from), std::move(to), 0);
			active.IsEventStoryboard = true;
			pending.push_back(std::move(active));
		}

		const bool animationsEnabled = Owner->AreSystemAnimationsEnabled();
		Applying = true;
		std::vector<AnimationFrameValue> initialValues;
		initialValues.reserve(pending.size());
		bool success = true;
		for (const auto& animation : pending)
		{
			const auto activeDuration =
				TimelineActiveDurationMilliseconds(animation);
			const bool active = animationsEnabled
				&& (animation.BeginTimeMilliseconds > 0 || activeDuration > 0);
			BindingValue value;
			if (active && animation.BeginTimeMilliseconds > 0)
				value = animation.Base;
			else if (!active && animation.FillBehavior
				== DeclarativeTimelineFillBehavior::Stop)
				value = animation.Base;
			else if (!Interpolate(animation,
				active ? 0 : activeDuration, value))
			{
				success = false;
				break;
			}
			initialValues.push_back({ &animation, std::move(value) });
		}
		if (success) success = ApplyAnimationFrame(initialValues);
		if (!success)
		{
			(void)RestoreSnapshots(snapshots);
			Applying = false;
			if (outError) *outError = L"BeginStoryboard 初始帧无法事务性应用。";
			return false;
		}

		const auto startTick = ::GetTickCount64();
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](const auto& animation)
			{
				return animation.IsEventStoryboard
					&& animation.GroupIndex == storyboardIndex;
			}), ActiveAnimations.end());
		for (auto& animation : pending)
			animation.StartTick = startTick;
		std::vector<ActiveAnimation> releaseContext = ActiveAnimations;
		const auto pendingOffset = releaseContext.size();
		for (const auto& animation : pending)
			releaseContext.push_back(animation);
		std::vector<const ActiveAnimation*> initiallyStopped;
		for (size_t index = 0; index < pending.size(); ++index)
		{
			const auto& animation = pending[index];
			const auto activeDuration =
				TimelineActiveDurationMilliseconds(animation);
			const bool active = animationsEnabled
				&& (animation.BeginTimeMilliseconds > 0 || activeDuration > 0);
			if (!active && animation.FillBehavior
				== DeclarativeTimelineFillBehavior::Stop)
				initiallyStopped.push_back(
					&releaseContext[pendingOffset + index]);
		}
		ReleaseStoppedAnimationValues(
			initiallyStopped, releaseContext, startTick);
		for (auto& animation : pending)
		{
			const auto activeDuration =
				TimelineActiveDurationMilliseconds(animation);
			const bool active = animationsEnabled
				&& (animation.BeginTimeMilliseconds > 0 || activeDuration > 0);
			if (active)
				ActiveAnimations.push_back(std::move(animation));
			else if (animation.FillBehavior
				== DeclarativeTimelineFillBehavior::HoldEnd)
			{
				animation.Completed = true;
				ActiveAnimations.push_back(std::move(animation));
			}
		}
		if (!initiallyStopped.empty())
			(void)ApplyRetainedAnimationFrame(startTick);
		Applying = false;
		if (HasActiveAnimations() || !initiallyStopped.empty())
			Owner->InvalidateVisual();
		if (outError) outError->clear();
		return true;
	}

	bool PauseEventStoryboard(size_t storyboardIndex)
	{
		const auto now = ::GetTickCount64();
		bool changed = false;
		for (auto& animation : ActiveAnimations)
			if (animation.IsEventStoryboard
				&& animation.GroupIndex == storyboardIndex
				&& !animation.Completed && !animation.Paused)
			{
				animation.Paused = true;
				animation.PauseTick = now;
				changed = true;
			}
		return changed;
	}

	bool ResumeEventStoryboard(size_t storyboardIndex)
	{
		const auto now = ::GetTickCount64();
		bool changed = false;
		for (auto& animation : ActiveAnimations)
			if (animation.IsEventStoryboard
				&& animation.GroupIndex == storyboardIndex
				&& !animation.Completed && animation.Paused)
			{
				const auto pausedFor = now >= animation.PauseTick
					? now - animation.PauseTick : 0;
				animation.StartTick = SaturatingAdd(
					animation.StartTick, pausedFor);
				animation.Paused = false;
				animation.PauseTick = 0;
				changed = true;
			}
		if (changed) Owner->InvalidateVisual();
		return changed;
	}

	bool StopEventStoryboard(size_t storyboardIndex)
	{
		const auto now = ::GetTickCount64();
		std::vector<const ActiveAnimation*> stopping;
		for (const auto& animation : ActiveAnimations)
			if (animation.IsEventStoryboard
				&& animation.GroupIndex == storyboardIndex)
				stopping.push_back(&animation);
		if (stopping.empty()) return false;
		Applying = true;
		ReleaseStoppedAnimationValues(stopping, ActiveAnimations, now);
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](const auto& animation)
			{
				return animation.IsEventStoryboard
					&& animation.GroupIndex == storyboardIndex;
			}), ActiveAnimations.end());
		(void)ApplyRetainedAnimationFrame(now);
		Applying = false;
		Owner->InvalidateVisual();
		return true;
	}

	void ExecuteEventTriggerAction(const RuntimeEventTriggerAction& action)
	{
		switch (action.Kind)
		{
		case DeclarativeStoryboardActionKind::Begin:
			(void)BeginEventStoryboard(action.StoryboardIndex, nullptr);
			break;
		case DeclarativeStoryboardActionKind::Pause:
			(void)PauseEventStoryboard(action.StoryboardIndex);
			break;
		case DeclarativeStoryboardActionKind::Resume:
			(void)ResumeEventStoryboard(action.StoryboardIndex);
			break;
		case DeclarativeStoryboardActionKind::Stop:
			(void)StopEventStoryboard(action.StoryboardIndex);
			break;
		}
	}

	bool ExecuteStyleTriggerActions(
		const std::vector<RuntimeEventTriggerAction>& actions,
		std::wstring* outError)
	{
		for (const auto& action : actions)
		{
			switch (action.Kind)
			{
			case DeclarativeStoryboardActionKind::Begin:
				if (!BeginEventStoryboard(action.StoryboardIndex, outError))
					return false;
				break;
			case DeclarativeStoryboardActionKind::Pause:
				(void)PauseEventStoryboard(action.StoryboardIndex);
				break;
			case DeclarativeStoryboardActionKind::Resume:
				(void)ResumeEventStoryboard(action.StoryboardIndex);
				break;
			case DeclarativeStoryboardActionKind::Stop:
				(void)StopEventStoryboard(action.StoryboardIndex);
				break;
			}
		}
		if (outError) outError->clear();
		return true;
	}

	size_t AllocateStyleStoryboardIndex()
	{
		if (!FreeStyleStoryboardIndices.empty())
		{
			const auto index = FreeStyleStoryboardIndices.back();
			FreeStyleStoryboardIndices.pop_back();
			return index;
		}
		EventStoryboards.emplace_back();
		return EventStoryboards.size() - 1;
	}

	void ReleaseStyleStoryboardIndex(size_t index)
	{
		if (index >= EventStoryboards.size()) return;
		(void)StopEventStoryboard(index);
		EventStoryboards[index] = {};
		if (std::find(FreeStyleStoryboardIndices.begin(),
			FreeStyleStoryboardIndices.end(), index)
			== FreeStyleStoryboardIndices.end())
			FreeStyleStoryboardIndices.push_back(index);
	}

	bool CompileStyleTriggerScope(
		const ResolvedControlStyleTrigger& source,
		RuntimeStyleTriggerScope& scope,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = L"Style DataTrigger：" + std::move(message);
			return false;
		};
		std::vector<RuntimeEventStoryboard> storyboards;
		std::vector<RuntimeEventTriggerAction> enterActions;
		std::vector<RuntimeEventTriggerAction> exitActions;
		std::vector<std::wstring> beginNames;
		auto compileActions = [&](
			const std::vector<DeclarativeEventTriggerActionDefinition>& definitions,
			std::vector<RuntimeEventTriggerAction>& output) -> bool
		{
			for (const auto& definition : definitions)
			{
				RuntimeEventTriggerAction action;
				action.Kind = definition.Kind;
				if (definition.Kind == DeclarativeStoryboardActionKind::Begin)
				{
					if (!definition.StoryboardName.empty()
						&& ContainsName(beginNames, definition.StoryboardName))
						return fail(L"BeginStoryboard x:Name 重复："
							+ definition.StoryboardName);
					if (definition.Animations.empty())
						return fail(L"BeginStoryboard 的 Storyboard 不能为空。");
					RuntimeEventStoryboard storyboard;
					storyboard.Name = definition.StoryboardName;
					storyboard.IsStyleStoryboard = true;
					struct PropertyOwnership
					{
						PropertyKey Root;
						bool Exclusive = false;
						std::vector<std::wstring> Paths;
					};
					std::vector<PropertyOwnership> properties;
					for (const auto& sourceAnimation : definition.Animations)
					{
						if (!sourceAnimation.TargetName.empty())
							return fail(L"Style Storyboard 不支持 TargetName："
								+ sourceAnimation.TargetName);
						RuntimeAnimation animation;
						if (!TryBuildAnimation(sourceAnimation, animation,
							L"Style BeginStoryboard", outError)) return false;
						PropertyKey key{ animation.Target, animation.PropertyName };
						const auto path = std::wstring(
							ObjectPathCanonical(animation.ObjectPath));
						auto owner = std::find_if(properties.begin(), properties.end(),
							[&](const auto& existing)
							{ return SameProperty(existing.Root, key); });
						if (owner != properties.end())
						{
							if (path.empty() || owner->Exclusive
								|| ContainsName(owner->Paths, path))
								return fail(L"BeginStoryboard 目标重复："
									+ sourceAnimation.PropertyName);
							owner->Paths.push_back(path);
						}
						else
						{
							PropertyOwnership ownership;
							ownership.Root = key;
							ownership.Exclusive = path.empty();
							if (!path.empty()) ownership.Paths.push_back(path);
							properties.push_back(std::move(ownership));
						}
						storyboard.Animations.push_back(std::move(animation));
					}
					action.StoryboardIndex = storyboards.size();
					storyboards.push_back(std::move(storyboard));
					if (!definition.StoryboardName.empty())
						beginNames.push_back(definition.StoryboardName);
				}
				else
				{
					if (definition.StoryboardName.empty())
						return fail(L"Storyboard 控制动作缺少 BeginStoryboardName。");
					action.PendingStoryboardName = definition.StoryboardName;
				}
				output.push_back(std::move(action));
			}
			return true;
		};
		if (!compileActions(source.EnterActions, enterActions)
			|| !compileActions(source.ExitActions, exitActions)) return false;
		auto resolveReferences = [&](std::vector<RuntimeEventTriggerAction>& actions)
		{
			for (auto& action : actions)
			{
				if (action.Kind == DeclarativeStoryboardActionKind::Begin) continue;
				const auto found = std::find_if(storyboards.begin(), storyboards.end(),
					[&](const auto& storyboard)
					{ return EqualName(storyboard.Name,
						action.PendingStoryboardName); });
				if (found == storyboards.end())
					return fail(L"Storyboard 控制动作找不到 BeginStoryboard："
						+ action.PendingStoryboardName);
				action.StoryboardIndex = static_cast<size_t>(
					std::distance(storyboards.begin(), found));
				action.PendingStoryboardName.clear();
			}
			return true;
		};
		if (!resolveReferences(enterActions)
			|| !resolveReferences(exitActions)) return false;

		auto indices = scope.StoryboardIndices;
		while (indices.size() < storyboards.size())
			indices.push_back(AllocateStyleStoryboardIndex());
		for (size_t index = 0; index < storyboards.size(); ++index)
			EventStoryboards[indices[index]] = std::move(storyboards[index]);
		for (size_t index = storyboards.size(); index < indices.size(); ++index)
			ReleaseStyleStoryboardIndex(indices[index]);
		indices.resize(storyboards.size());
		auto mapIndices = [&](std::vector<RuntimeEventTriggerAction>& actions)
		{
			for (auto& action : actions)
				if (action.StoryboardIndex < indices.size())
					action.StoryboardIndex = indices[action.StoryboardIndex];
		};
		mapIndices(enterActions);
		mapIndices(exitActions);
		scope.StoryboardIndices = std::move(indices);
		scope.EnterActions = std::move(enterActions);
		scope.ExitActions = std::move(exitActions);
		if (outError) outError->clear();
		return true;
	}

	void RemoveStyleTriggerScope(size_t index)
	{
		if (index >= StyleTriggerScopes.size()) return;
		for (const auto storyboardIndex
			: StyleTriggerScopes[index].StoryboardIndices)
			ReleaseStyleStoryboardIndex(storyboardIndex);
		StyleTriggerScopes.erase(StyleTriggerScopes.begin() + index);
	}

	bool SynchronizeStyleTriggerActions(
		DependencyPropertyValueSource source,
		const ControlStyleSheet* sheet,
		const std::vector<ResolvedControlStyleTrigger>& triggers,
		std::wstring* outError)
	{
		bool success = true;
		for (const auto& trigger : triggers)
		{
			auto found = std::find_if(StyleTriggerScopes.begin(),
				StyleTriggerScopes.end(), [&](const auto& existing)
				{
					return existing.Source == source && existing.Sheet == sheet
						&& existing.RuleId == trigger.RuleId;
				});
			if (found == StyleTriggerScopes.end())
			{
				RuntimeStyleTriggerScope scope;
				scope.Source = source;
				scope.Sheet = sheet;
				scope.RuleId = trigger.RuleId;
				if (!CompileStyleTriggerScope(trigger, scope, outError))
				{
					success = false;
					continue;
				}
				StyleTriggerScopes.push_back(std::move(scope));
				found = std::prev(StyleTriggerScopes.end());
			}
			else if (!CompileStyleTriggerScope(trigger, *found, outError))
			{
				success = false;
				continue;
			}
			if (found->Active == trigger.IsActive) continue;
			const bool previous = found->Active;
			found->Active = trigger.IsActive;
			if (!ExecuteStyleTriggerActions(
				trigger.IsActive ? found->EnterActions : found->ExitActions,
				outError))
			{
				found->Active = previous;
				success = false;
			}
		}
		for (size_t index = StyleTriggerScopes.size(); index-- > 0;)
		{
			const auto& scope = StyleTriggerScopes[index];
			if (scope.Source != source || scope.Sheet != sheet) continue;
			if (std::none_of(triggers.begin(), triggers.end(),
				[&](const auto& trigger) { return trigger.RuleId == scope.RuleId; }))
				RemoveStyleTriggerScope(index);
		}
		if (success && outError) outError->clear();
		return success;
	}

	void PruneStyleTriggerActions(
		DependencyPropertyValueSource source,
		const std::vector<const ControlStyleSheet*>& visibleSheets)
	{
		for (size_t index = StyleTriggerScopes.size(); index-- > 0;)
		{
			const auto& scope = StyleTriggerScopes[index];
			if (scope.Source != source) continue;
			if (std::find(visibleSheets.begin(), visibleSheets.end(), scope.Sheet)
				== visibleSheets.end())
				RemoveStyleTriggerScope(index);
		}
	}

	void ResetFailedDeclarativeInteractionBuild()
	{
		Connections.clear();
		ClearAppliedValues();
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[](const auto& animation) { return !animation.IsEventStoryboard; }),
			ActiveAnimations.end());
		Groups.clear();
		EventTriggers.clear();
		for (auto& storyboard : EventStoryboards)
			if (!storyboard.IsStyleStoryboard) storyboard = {};
		(void)ApplyRetainedAnimationFrame(::GetTickCount64());
		DeclarativeInteractionsDefined = false;
	}

	void OnHostDeclarativeEvent(DeclarativeEventArgs& args)
	{
		if (Applying || args.OriginalSource != Owner
			|| args.OwnerType != Owner->GetDeclarativeTypeId()) return;
		for (size_t groupIndex = 0; groupIndex < Groups.size(); ++groupIndex)
		{
			auto& group = Groups[groupIndex];
			for (size_t stateIndex = 0;
				stateIndex < group.States.size(); ++stateIndex)
				if (ContainsName(
					group.States[stateIndex].EventNames, args.Name))
				{
					(void)GoTo(groupIndex, stateIndex, true, nullptr);
					break;
				}
		}
		for (const auto& trigger : EventTriggers)
			if (EqualName(trigger.EventName, args.Name))
				for (const auto& action : trigger.Actions)
					ExecuteEventTriggerAction(action);
	}

	bool TryBuildAnimation(
		const DeclarativeVisualStateAnimation& sourceAnimation,
		RuntimeAnimation& animation,
		const std::wstring& context,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = context + L"：" + std::move(message);
			return false;
		};
		Control* target = sourceAnimation.TargetName.empty()
			? Owner : Owner->FindDeclarativeTemplatePart(sourceAnimation.TargetName);
		if (!target)
			return fail(L"Storyboard 找不到模板部件："
				+ sourceAnimation.TargetName);
		const auto* metadata = target->FindPropertyMetadata(
			sourceAnimation.PropertyName);
		std::optional<ObjectPathAccessor> objectPath;
		if (!metadata || !metadata->CanWrite()
			|| !AnimationMatchesMetadata(sourceAnimation.Kind, *metadata))
		{
			std::wstring pathError;
			ObjectPathAccessor accessor;
			if (!TryResolveObjectPath(*target, sourceAnimation.PropertyName,
				sourceAnimation.Kind, metadata, accessor, &pathError))
				return fail(pathError + L"：" + sourceAnimation.PropertyName);
			objectPath = std::move(accessor);
		}
		BindingValue convertedScratch;
		auto validTypedAnimationValue = [&](const BindingValue& value)
		{
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Point)
			{
				cui::core::Point point{};
				return value.TryGet(point)
					&& std::isfinite(point.x) && std::isfinite(point.y);
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Vector)
			{
				cui::core::Vector vector{};
				return value.TryGet(vector)
					&& std::isfinite(vector.x) && std::isfinite(vector.y);
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Rect)
			{
				cui::core::Rect rect{};
				return value.TryGet(rect)
					&& std::isfinite(rect.x) && std::isfinite(rect.y)
					&& std::isfinite(rect.width) && std::isfinite(rect.height)
					&& rect.width >= 0.0f && rect.height >= 0.0f;
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Size)
			{
				cui::core::Size size{};
				return value.TryGet(size)
					&& std::isfinite(size.width) && std::isfinite(size.height)
					&& size.width >= 0.0f && size.height >= 0.0f;
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Matrix)
			{
				D2D1_MATRIX_3X2_F matrix{};
				return value.TryGet(matrix) && IsFiniteMatrix(matrix);
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Thickness)
			{
				Thickness thickness;
				return value.TryGet(thickness)
					&& std::isfinite(thickness.Left)
					&& std::isfinite(thickness.Top)
					&& std::isfinite(thickness.Right)
					&& std::isfinite(thickness.Bottom);
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Color)
			{
				D2D1_COLOR_F color{};
				return value.TryGet(color)
					&& std::isfinite(color.r) && std::isfinite(color.g)
					&& std::isfinite(color.b) && std::isfinite(color.a);
			}
			return true;
		};
		auto convertEndpoint = [&](const BindingValue& source,
			BindingValue& output, bool isDelta = false)
		{
			if (!objectPath)
				return metadata->TryConvert(source, convertedScratch)
					&& validTypedAnimationValue(convertedScratch)
					&& metadata->TryCoerce(*target, convertedScratch, output);
			if (sourceAnimation.Kind != DeclarativeAnimationKind::Double)
			{
				if (!validTypedAnimationValue(source)) return false;
				if (const auto* pathGeometry = AsPathGeometryPath(objectPath);
					pathGeometry && sourceAnimation.Kind
						== DeclarativeAnimationKind::Object)
				{
					if (pathGeometry->Member == PathGeometryMember::ArcSweepDirection)
					{
						std::wstring sweep;
						if (!source.TryGet(sweep)
							|| (!EqualValueToken(sweep, L"Clockwise")
								&& !EqualValueToken(
									sweep, L"Counterclockwise")))
							return false;
					}
					else
					{
						bool flag = false;
						if (!source.TryGet(flag)) return false;
					}
				}
				if (const auto* geometry = AsGeometryPath(objectPath);
					geometry && geometry->Member == GeometryMember::FillRule
					&& sourceAnimation.Kind == DeclarativeAnimationKind::Object)
				{
					std::wstring fillRule;
					if (!source.TryGet(fillRule)
						|| (!EqualValueToken(fillRule, L"EvenOdd")
							&& !EqualValueToken(
								fillRule, L"Nonzero"))) return false;
				}
				output = source;
				return true;
			}
			double value = 0.0;
			if (!source.TryGetDouble(value) || !std::isfinite(value)
				|| value < -(std::numeric_limits<float>::max)()
				|| value > (std::numeric_limits<float>::max)()) return false;
			if (!isDelta)
			{
				if (const auto* brushPath = AsBrushPath(objectPath))
				{
					if ((brushPath->Member == BrushMember::GradientStopOffset
							|| brushPath->Member == BrushMember::Opacity)
						&& (value < 0.0 || value > 1.0)) return false;
					if ((brushPath->Member == BrushMember::RadiusX
							|| brushPath->Member == BrushMember::RadiusY)
						&& value < 0.0) return false;
				}
				if (const auto* geometryPath = AsGeometryPath(objectPath);
					geometryPath
					&& (geometryPath->Member == GeometryMember::RadiusX
						|| geometryPath->Member == GeometryMember::RadiusY)
					&& value < 0.0) return false;
			}
			output = BindingValue(static_cast<float>(value));
			return true;
		};
		std::optional<BindingValue> coercedFrom;
		std::optional<BindingValue> coercedTo;
		std::optional<BindingValue> coercedBy;
		std::vector<DeclarativeAnimationKeyFrame> keyFrames;
		if (sourceAnimation.KeyFrames.empty())
		{
			if (sourceAnimation.To)
			{
				BindingValue value;
				if (!convertEndpoint(*sourceAnimation.To, value))
					return fail(L"动画 To 无效：" + sourceAnimation.PropertyName);
				coercedTo = std::move(value);
			}
			if (sourceAnimation.From)
			{
				BindingValue value;
				if (!convertEndpoint(*sourceAnimation.From, value))
					return fail(L"动画 From 无效："
						+ sourceAnimation.PropertyName);
				coercedFrom = std::move(value);
			}
			if (sourceAnimation.By)
			{
				BindingValue value;
				if (objectPath)
				{
					if (!convertEndpoint(*sourceAnimation.By, value, true))
						return fail(L"动画 By 无效："
							+ sourceAnimation.PropertyName);
				}
				else if (!metadata->TryConvert(*sourceAnimation.By, value)
					|| !validTypedAnimationValue(value))
					return fail(L"动画 By 无法转换为目标属性类型："
						+ sourceAnimation.PropertyName);
				coercedBy = std::move(value);
			}
		}
		else
		{
			if (sourceAnimation.From || sourceAnimation.To || sourceAnimation.By)
				return fail(L"关键帧动画不能同时声明 From/To/By："
					+ sourceAnimation.PropertyName);
			keyFrames.reserve(sourceAnimation.KeyFrames.size());
			for (auto keyFrame : sourceAnimation.KeyFrames)
			{
				if (sourceAnimation.Kind == DeclarativeAnimationKind::Object
					&& keyFrame.Kind != DeclarativeKeyFrameKind::Discrete)
					return fail(L"ObjectAnimationUsingKeyFrames 只能包含 DiscreteObjectKeyFrame。");
				if (keyFrame.Kind != DeclarativeKeyFrameKind::Discrete
					&& keyFrame.Kind != DeclarativeKeyFrameKind::Linear
					&& keyFrame.Kind != DeclarativeKeyFrameKind::Easing
					&& keyFrame.Kind != DeclarativeKeyFrameKind::Spline)
					return fail(L"关键帧类型无效。");
				if (keyFrame.Kind == DeclarativeKeyFrameKind::Spline
					&& (!std::isfinite(keyFrame.KeySplineX1)
						|| !std::isfinite(keyFrame.KeySplineY1)
						|| !std::isfinite(keyFrame.KeySplineX2)
						|| !std::isfinite(keyFrame.KeySplineY2)
						|| keyFrame.KeySplineX1 < 0.0f
						|| keyFrame.KeySplineX1 > 1.0f
						|| keyFrame.KeySplineY1 < 0.0f
						|| keyFrame.KeySplineY1 > 1.0f
						|| keyFrame.KeySplineX2 < 0.0f
						|| keyFrame.KeySplineX2 > 1.0f
						|| keyFrame.KeySplineY2 < 0.0f
						|| keyFrame.KeySplineY2 > 1.0f))
					return fail(L"KeySpline 控制点必须位于 0..1。");
				BindingValue value;
				if (!convertEndpoint(keyFrame.Value, value))
					return fail(L"关键帧值无效："
						+ sourceAnimation.PropertyName);
				keyFrame.Value = std::move(value);
				keyFrames.push_back(std::move(keyFrame));
			}
			std::stable_sort(keyFrames.begin(), keyFrames.end(),
				[](const auto& left, const auto& right)
				{
					return left.KeyTimeMilliseconds
						< right.KeyTimeMilliseconds;
				});
		}
		animation.Kind = sourceAnimation.Kind;
		animation.Target = target;
		animation.Metadata = metadata;
		animation.PropertyName = metadata->Name();
		animation.ObjectPath = std::move(objectPath);
		animation.From = std::move(coercedFrom);
		animation.To = std::move(coercedTo);
		animation.By = std::move(coercedBy);
		animation.IsAdditive = sourceAnimation.IsAdditive;
		animation.IsCumulative = sourceAnimation.IsCumulative;
		animation.KeyFrames = std::move(keyFrames);
		animation.BeginTimeMilliseconds = sourceAnimation.BeginTimeMilliseconds;
		animation.DurationMilliseconds = sourceAnimation.DurationMilliseconds;
		if (sourceAnimation.Kind == DeclarativeAnimationKind::Object)
		{
			if (sourceAnimation.KeyFrames.empty())
				return fail(L"ObjectAnimationUsingKeyFrames 至少需要一个关键帧。");
			if (sourceAnimation.IsAdditive || sourceAnimation.IsCumulative)
				return fail(L"ObjectAnimationUsingKeyFrames 不支持 IsAdditive/IsCumulative。");
			if (sourceAnimation.Easing != DeclarativeEasingKind::Linear)
				return fail(L"ObjectAnimationUsingKeyFrames 不支持 EasingFunction。");
		}
		if (sourceAnimation.RepeatBehavior
			== DeclarativeRepeatBehaviorKind::Count)
		{
			if (!std::isfinite(sourceAnimation.RepeatCount)
				|| sourceAnimation.RepeatCount <= 0.0)
				return fail(L"RepeatBehavior Count 必须是有限正数。");
		}
		else if (sourceAnimation.RepeatBehavior
			== DeclarativeRepeatBehaviorKind::Duration)
		{
			if (sourceAnimation.RepeatDurationMilliseconds == 0)
				return fail(L"RepeatBehavior Duration 必须大于零。");
		}
		else if (sourceAnimation.RepeatBehavior
			!= DeclarativeRepeatBehaviorKind::Forever)
			return fail(L"RepeatBehavior 类型无效。");
		if (sourceAnimation.FillBehavior
			!= DeclarativeTimelineFillBehavior::HoldEnd
			&& sourceAnimation.FillBehavior
				!= DeclarativeTimelineFillBehavior::Stop)
			return fail(L"FillBehavior 类型无效。");
		if (!std::isfinite(sourceAnimation.SpeedRatio)
			|| sourceAnimation.SpeedRatio <= 0.0)
			return fail(L"SpeedRatio 必须是有限正数。");
		if (!std::isfinite(sourceAnimation.AccelerationRatio)
			|| sourceAnimation.AccelerationRatio < 0.0
			|| sourceAnimation.AccelerationRatio > 1.0)
			return fail(L"AccelerationRatio 必须位于 0..1。");
		if (!std::isfinite(sourceAnimation.DecelerationRatio)
			|| sourceAnimation.DecelerationRatio < 0.0
			|| sourceAnimation.DecelerationRatio > 1.0)
			return fail(L"DecelerationRatio 必须位于 0..1。");
		if (sourceAnimation.AccelerationRatio
			+ sourceAnimation.DecelerationRatio > 1.0)
			return fail(L"AccelerationRatio 与 DecelerationRatio 之和不能超过 1。");
		animation.RepeatBehavior = sourceAnimation.RepeatBehavior;
		animation.RepeatCount = sourceAnimation.RepeatCount;
		animation.RepeatDurationMilliseconds =
			sourceAnimation.RepeatDurationMilliseconds;
		animation.AutoReverse = sourceAnimation.AutoReverse;
		animation.FillBehavior = sourceAnimation.FillBehavior;
		animation.SpeedRatio = sourceAnimation.SpeedRatio;
		animation.AccelerationRatio = sourceAnimation.AccelerationRatio;
		animation.DecelerationRatio = sourceAnimation.DecelerationRatio;
		animation.Easing = sourceAnimation.Easing;
		animation.EasingMode = sourceAnimation.EasingMode;
		if (outError) outError->clear();
		return true;
	}

	bool Build(
		std::vector<DeclarativeVisualStateGroupDefinition> definitions,
		std::vector<DeclarativeEventTriggerDefinition> eventTriggerDefinitions,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		if (!Owner || (definitions.empty() && eventTriggerDefinitions.empty()))
			return fail(L"视觉状态组和事件触发器不能同时为空。");
		std::vector<std::pair<PropertyKey, size_t>> groupProperties;
		for (auto& sourceGroup : definitions)
		{
			if (sourceGroup.Name.empty())
				return fail(L"视觉状态组名称不能为空。");
			if (std::any_of(Groups.begin(), Groups.end(), [&](const auto& existing)
				{ return EqualName(existing.Name, sourceGroup.Name); }))
				return fail(L"视觉状态组名称重复：" + sourceGroup.Name);
			if (sourceGroup.States.empty())
				return fail(L"视觉状态组至少需要一个状态：" + sourceGroup.Name);
			RuntimeGroup group;
			group.Name = std::move(sourceGroup.Name);
			std::optional<size_t> fallback;
			std::vector<std::wstring> groupEvents;
			for (auto& sourceState : sourceGroup.States)
			{
				if (sourceState.Name.empty())
					return fail(L"视觉状态名称不能为空。");
				if (std::any_of(group.States.begin(), group.States.end(),
					[&](const auto& existing)
					{ return EqualName(existing.Name, sourceState.Name); }))
					return fail(L"视觉状态名称重复：" + sourceState.Name);
				if (!sourceState.Conditions.empty()
					&& !sourceState.EventNames.empty())
					return fail(L"视觉状态不能同时声明属性和事件触发器："
						+ sourceState.Name);
				RuntimeState state;
				state.Name = std::move(sourceState.Name);
				if (sourceState.Conditions.empty()
					&& sourceState.EventNames.empty())
				{
					if (fallback)
						return fail(L"每个视觉状态组只能有一个无触发器的回退状态："
							+ group.Name);
					fallback = group.States.size();
				}
				std::vector<std::wstring> stateConditions;
				for (auto& sourceCondition : sourceState.Conditions)
				{
					if (sourceCondition.PropertyName.empty()
						|| ContainsName(stateConditions,
							sourceCondition.PropertyName))
						return fail(L"视觉状态条件属性为空或重复：" + state.Name);
					const auto* metadata = Owner->FindPropertyMetadata(
						sourceCondition.PropertyName);
					BindingValue converted;
					BindingValue coerced;
					if (!metadata || !metadata->CanRead()
						|| !metadata->TryConvert(sourceCondition.Value, converted)
						|| !metadata->TryCoerce(*Owner, converted, coerced))
						return fail(L"视觉状态条件属性不存在或值无效："
							+ sourceCondition.PropertyName);
					stateConditions.push_back(metadata->Name());
					if (!ContainsName(group.ConditionProperties, metadata->Name()))
						group.ConditionProperties.push_back(metadata->Name());
					state.Conditions.push_back({ metadata, std::move(coerced) });
				}
				for (auto& eventName : sourceState.EventNames)
				{
					const auto* event = Owner->FindDeclarativeEvent(eventName);
					if (eventName.empty() || !event
						|| ContainsName(groupEvents, eventName))
						return fail(L"视觉状态事件不存在或在组内重复：" + eventName);
					groupEvents.push_back(event->Name);
					state.EventNames.push_back(event->Name);
				}
				struct StatePropertyOwnership
				{
					PropertyKey Root;
					bool Exclusive = false;
					std::vector<std::wstring> AnimationPaths;
				};
				std::vector<StatePropertyOwnership> stateProperties;
				auto registerControlledProperty = [&](const PropertyKey& key,
					const std::wstring& source,
					const std::wstring& animationPath = {}) -> bool
				{
					auto stateOwner = std::find_if(
						stateProperties.begin(), stateProperties.end(),
						[&](const auto& existing)
						{ return SameProperty(existing.Root, key); });
					if (stateOwner != stateProperties.end())
					{
						if (animationPath.empty() || stateOwner->Exclusive
							|| ContainsName(stateOwner->AnimationPaths, animationPath))
							return fail(L"视觉状态 Setter/Storyboard 目标重复："
								+ state.Name + L"." + source);
						stateOwner->AnimationPaths.push_back(animationPath);
					}
					else
					{
						StatePropertyOwnership ownership;
						ownership.Root = key;
						ownership.Exclusive = animationPath.empty();
						if (!animationPath.empty())
							ownership.AnimationPaths.push_back(animationPath);
						stateProperties.push_back(std::move(ownership));
					}
					const auto owner = std::find_if(
						groupProperties.begin(), groupProperties.end(),
						[&](const auto& existing)
						{ return SameProperty(existing.first, key); });
					if (owner != groupProperties.end()
						&& owner->second != Groups.size())
						return fail(L"不同视觉状态组不能控制同一属性：" + source);
					if (owner == groupProperties.end())
						groupProperties.emplace_back(key, Groups.size());
					return true;
				};
				for (auto& sourceSetter : sourceState.Setters)
				{
					Control* target = sourceSetter.TargetName.empty()
						? Owner
						: Owner->FindDeclarativeTemplatePart(
							sourceSetter.TargetName);
					if (!target)
						return fail(L"视觉状态 Setter 找不到模板部件："
							+ sourceSetter.TargetName);
					const auto* metadata = target->FindPropertyMetadata(
						sourceSetter.PropertyName);
					BindingValue converted;
					BindingValue coerced;
					if (!metadata || !metadata->CanWrite()
						|| !metadata->TryConvert(sourceSetter.Value, converted)
						|| !metadata->TryCoerce(*target, converted, coerced))
						return fail(L"视觉状态 Setter 属性不存在、只读或值无效："
							+ sourceSetter.PropertyName);
					PropertyKey key{ target, metadata->Name() };
					if (!registerControlledProperty(key, metadata->Name())) return false;
					state.Setters.push_back({
						target, metadata->Name(), std::move(coerced) });
				}
				for (auto& sourceAnimation : sourceState.Animations)
				{
					RuntimeAnimation animation;
					if (!TryBuildAnimation(sourceAnimation, animation,
						L"视觉状态 Storyboard", outError)) return false;
					PropertyKey key{ animation.Target, animation.PropertyName };
					const auto path = std::wstring(
						ObjectPathCanonical(animation.ObjectPath));
					if (!registerControlledProperty(key,
						path.empty() ? animation.PropertyName : path, path))
						return false;
					state.Animations.push_back(std::move(animation));
				}
				group.States.push_back(std::move(state));
			}
			if (!fallback)
				return fail(L"视觉状态组缺少无触发器的回退状态：" + group.Name);
			group.FallbackState = *fallback;
			auto findState = [&](const std::wstring& name) -> std::optional<size_t>
			{
				if (name.empty()) return std::nullopt;
				for (size_t index = 0; index < group.States.size(); ++index)
					if (EqualName(group.States[index].Name, name)) return index;
				return std::nullopt;
			};
			for (const auto& sourceTransition : sourceGroup.Transitions)
			{
				RuntimeTransition transition;
				transition.FromState = findState(sourceTransition.FromState);
				transition.ToState = findState(sourceTransition.ToState);
				if (!sourceTransition.FromState.empty() && !transition.FromState)
					return fail(L"VisualTransition.From 状态不存在："
						+ sourceTransition.FromState);
				if (!sourceTransition.ToState.empty() && !transition.ToState)
					return fail(L"VisualTransition.To 状态不存在："
						+ sourceTransition.ToState);
				if (std::any_of(group.Transitions.begin(), group.Transitions.end(),
					[&](const auto& existing)
					{
						return existing.FromState == transition.FromState
							&& existing.ToState == transition.ToState;
					}))
					return fail(L"VisualTransition From/To 选择器重复："
						+ sourceTransition.FromState + L" -> "
						+ sourceTransition.ToState);
				transition.GeneratedDurationMilliseconds =
					sourceTransition.GeneratedDurationMilliseconds;
				transition.GeneratedEasing = sourceTransition.GeneratedEasing;
				transition.GeneratedEasingMode =
					sourceTransition.GeneratedEasingMode;
				struct TransitionPropertyOwnership
				{
					PropertyKey Root;
					bool Exclusive = false;
					std::vector<std::wstring> AnimationPaths;
				};
				std::vector<TransitionPropertyOwnership> transitionProperties;
				for (const auto& sourceAnimation : sourceTransition.Animations)
				{
					RuntimeAnimation animation;
					if (!TryBuildAnimation(sourceAnimation, animation,
						L"VisualTransition Storyboard", outError)) return false;
					PropertyKey key{ animation.Target, animation.PropertyName };
					const auto path = std::wstring(
						ObjectPathCanonical(animation.ObjectPath));
					auto owner = std::find_if(transitionProperties.begin(),
						transitionProperties.end(), [&](const auto& existing)
						{ return SameProperty(existing.Root, key); });
					if (owner != transitionProperties.end())
					{
						if (path.empty() || owner->Exclusive
							|| ContainsName(owner->AnimationPaths, path))
							return fail(L"VisualTransition Storyboard 目标重复："
								+ sourceAnimation.PropertyName);
						owner->AnimationPaths.push_back(path);
					}
					else
					{
						TransitionPropertyOwnership ownership;
						ownership.Root = key;
						ownership.Exclusive = path.empty();
						if (!path.empty()) ownership.AnimationPaths.push_back(path);
						transitionProperties.push_back(std::move(ownership));
					}
					const auto groupOwner = std::find_if(groupProperties.begin(),
						groupProperties.end(), [&](const auto& existing)
						{ return SameProperty(existing.first, key); });
					if (groupOwner != groupProperties.end()
						&& groupOwner->second != Groups.size())
						return fail(L"不同视觉状态组不能控制同一 Transition 属性："
							+ sourceAnimation.PropertyName);
					if (groupOwner == groupProperties.end())
						groupProperties.emplace_back(key, Groups.size());
					transition.Animations.push_back(std::move(animation));
				}
				group.Transitions.push_back(std::move(transition));
			}
			Groups.push_back(std::move(group));
		}

		for (auto& sourceTrigger : eventTriggerDefinitions)
		{
			const auto* sourceEvent = Owner->FindDeclarativeEvent(
				sourceTrigger.EventName);
			if (sourceTrigger.EventName.empty() || !sourceEvent)
				return fail(L"EventTrigger 事件不存在："
					+ sourceTrigger.EventName);
			if (sourceTrigger.Actions.empty())
				return fail(L"EventTrigger 至少需要一个 TriggerAction："
					+ sourceEvent->Name);
			RuntimeEventTrigger trigger;
			trigger.EventName = sourceEvent->Name;
			for (auto& sourceAction : sourceTrigger.Actions)
			{
				RuntimeEventTriggerAction action;
				action.Kind = sourceAction.Kind;
				if (sourceAction.Kind
					== DeclarativeStoryboardActionKind::Begin)
				{
					if (!sourceAction.StoryboardName.empty()
						&& std::any_of(EventStoryboards.begin(),
							EventStoryboards.end(), [&](const auto& existing)
							{ return !existing.IsStyleStoryboard
								&& EqualName(existing.Name,
								sourceAction.StoryboardName); }))
						return fail(L"BeginStoryboard x:Name 重复："
							+ sourceAction.StoryboardName);
					if (sourceAction.Animations.empty())
						return fail(L"BeginStoryboard 的 Storyboard 不能为空。");
					RuntimeEventStoryboard storyboard;
					storyboard.Name = std::move(sourceAction.StoryboardName);
					struct StoryboardPropertyOwnership
					{
						PropertyKey Root;
						bool Exclusive = false;
						std::vector<std::wstring> Paths;
					};
					std::vector<StoryboardPropertyOwnership> properties;
					for (const auto& sourceAnimation : sourceAction.Animations)
					{
						RuntimeAnimation animation;
						if (!TryBuildAnimation(sourceAnimation, animation,
							L"BeginStoryboard", outError)) return false;
						PropertyKey key{ animation.Target,
							animation.PropertyName };
						const auto path = std::wstring(
							ObjectPathCanonical(animation.ObjectPath));
						auto owner = std::find_if(properties.begin(),
							properties.end(), [&](const auto& existing)
							{ return SameProperty(existing.Root, key); });
						if (owner != properties.end())
						{
							if (path.empty() || owner->Exclusive
								|| ContainsName(owner->Paths, path))
								return fail(L"BeginStoryboard 目标重复："
									+ sourceAnimation.PropertyName);
							owner->Paths.push_back(path);
						}
						else
						{
							StoryboardPropertyOwnership ownership;
							ownership.Root = key;
							ownership.Exclusive = path.empty();
							if (!path.empty()) ownership.Paths.push_back(path);
							properties.push_back(std::move(ownership));
						}
						storyboard.Animations.push_back(std::move(animation));
					}
					action.StoryboardIndex = EventStoryboards.size();
					EventStoryboards.push_back(std::move(storyboard));
				}
				else
				{
					if (sourceAction.StoryboardName.empty())
						return fail(L"Storyboard 控制动作缺少 BeginStoryboardName。");
					action.PendingStoryboardName =
						std::move(sourceAction.StoryboardName);
				}
				trigger.Actions.push_back(std::move(action));
			}
			EventTriggers.push_back(std::move(trigger));
		}
		for (auto& trigger : EventTriggers)
			for (auto& action : trigger.Actions)
			{
				if (action.Kind == DeclarativeStoryboardActionKind::Begin)
					continue;
				const auto found = std::find_if(EventStoryboards.begin(),
					EventStoryboards.end(), [&](const auto& storyboard)
					{ return !storyboard.IsStyleStoryboard
						&& EqualName(storyboard.Name,
						action.PendingStoryboardName); });
				if (found == EventStoryboards.end())
					return fail(L"Storyboard 控制动作找不到 BeginStoryboard："
						+ action.PendingStoryboardName);
				action.StoryboardIndex = static_cast<size_t>(
					std::distance(EventStoryboards.begin(), found));
				action.PendingStoryboardName.clear();
			}

		Connections.push_back(Owner->OnPropertyValueChanged.Subscribe(
			[this](DependencyObject*, const DependencyPropertyChangedEventArgs& args)
			{ OnHostPropertyChanged(args); }));
		Connections.push_back(Owner->OnDeclarativeEvent.Subscribe(
			[this](Control*, DeclarativeEventArgs& args)
			{ OnHostDeclarativeEvent(args); }));
		for (size_t index = 0; index < Groups.size(); ++index)
			if (!GoTo(index, EvaluateState(Groups[index]), false, outError)) return false;
		if (outError) outError->clear();
		return true;
	}
};

bool Control::DefineVisualStateGroups(
	std::vector<DeclarativeVisualStateGroupDefinition> groups,
	std::wstring* outError)
{
	return DefineDeclarativeInteractions(
		std::move(groups), {}, outError);
}

bool Control::DefineDeclarativeInteractions(
	std::vector<DeclarativeVisualStateGroupDefinition> groups,
	std::vector<DeclarativeEventTriggerDefinition> eventTriggers,
	std::wstring* outError)
{
	if (_declarativeVisualStates
		&& _declarativeVisualStates->DeclarativeInteractionsDefined)
	{
		if (outError) *outError = L"声明交互已经安装。";
		return false;
	}
	if (!_declarativeVisualStates)
	{
		_declarativeVisualStates =
			std::make_unique<DeclarativeVisualStateRuntime>();
		_declarativeVisualStates->Owner = this;
	}
	if (!_declarativeVisualStates->Build(
		std::move(groups), std::move(eventTriggers), outError))
	{
		_declarativeVisualStates->ResetFailedDeclarativeInteractionBuild();
		return false;
	}
	_declarativeVisualStates->DeclarativeInteractionsDefined = true;
	if (outError) outError->clear();
	return true;
}

bool Control::SynchronizeStyleTriggerActions(
	DependencyPropertyValueSource source,
	const std::shared_ptr<const ControlStyleSheet>& sheet,
	const ControlStyleResolution& resolution)
{
	if (!_declarativeVisualStates && resolution.Triggers.empty()) return true;
	if (!_declarativeVisualStates)
	{
		_declarativeVisualStates =
			std::make_unique<DeclarativeVisualStateRuntime>();
		_declarativeVisualStates->Owner = this;
	}
	std::wstring ignored;
	return _declarativeVisualStates->SynchronizeStyleTriggerActions(
		source, sheet.get(), resolution.Triggers, &ignored);
}

void Control::PruneStyleTriggerActions(
	DependencyPropertyValueSource source,
	const std::vector<std::shared_ptr<const ControlStyleSheet>>& sheets)
{
	if (!_declarativeVisualStates) return;
	std::vector<const ControlStyleSheet*> visible;
	visible.reserve(sheets.size());
	for (const auto& sheet : sheets)
		if (sheet) visible.push_back(sheet.get());
	_declarativeVisualStates->PruneStyleTriggerActions(source, visible);
}

bool Control::GoToVisualState(
	const std::wstring& groupName,
	const std::wstring& stateName,
	std::wstring* outError)
{
	return GoToVisualState(groupName, stateName, true, outError);
}

bool Control::GoToVisualState(
	const std::wstring& groupName,
	const std::wstring& stateName,
	bool useTransitions,
	std::wstring* outError)
{
	if (!_declarativeVisualStates)
	{
		if (outError) *outError = L"控件未安装视觉状态组。";
		return false;
	}
	for (size_t groupIndex = 0;
		groupIndex < _declarativeVisualStates->Groups.size(); ++groupIndex)
	{
		auto& group = _declarativeVisualStates->Groups[groupIndex];
		if (!DeclarativeVisualStateRuntime::EqualName(group.Name, groupName))
			continue;
		for (size_t stateIndex = 0; stateIndex < group.States.size(); ++stateIndex)
			if (DeclarativeVisualStateRuntime::EqualName(
				group.States[stateIndex].Name, stateName))
				return _declarativeVisualStates->GoTo(
					groupIndex, stateIndex, useTransitions, outError);
		if (outError) *outError = L"视觉状态不存在：" + stateName;
		return false;
	}
	if (outError) *outError = L"视觉状态组不存在：" + groupName;
	return false;
}

bool Control::GoToVisualState(
	const std::wstring& stateName,
	std::wstring* outError)
{
	return GoToVisualState(stateName, true, outError);
}

bool Control::GoToVisualState(
	const std::wstring& stateName,
	bool useTransitions,
	std::wstring* outError)
{
	if (!_declarativeVisualStates)
	{
		if (outError) *outError = L"控件未安装视觉状态组。";
		return false;
	}
	std::optional<std::pair<size_t, size_t>> found;
	for (size_t groupIndex = 0;
		groupIndex < _declarativeVisualStates->Groups.size(); ++groupIndex)
	{
		const auto& group = _declarativeVisualStates->Groups[groupIndex];
		for (size_t stateIndex = 0; stateIndex < group.States.size(); ++stateIndex)
		{
			if (!DeclarativeVisualStateRuntime::EqualName(
				group.States[stateIndex].Name, stateName)) continue;
			if (found)
			{
				if (outError) *outError = L"视觉状态名称跨组重复，请指定组："
					+ stateName;
				return false;
			}
			found = std::pair{ groupIndex, stateIndex };
		}
	}
	if (!found)
	{
		if (outError) *outError = L"视觉状态不存在：" + stateName;
		return false;
	}
	return _declarativeVisualStates->GoTo(
		found->first, found->second, useTransitions, outError);
}

std::wstring Control::GetCurrentVisualState(
	const std::wstring& groupName) const
{
	if (!_declarativeVisualStates) return {};
	for (const auto& group : _declarativeVisualStates->Groups)
		if (DeclarativeVisualStateRuntime::EqualName(group.Name, groupName))
		{
			if (group.Pending
				&& group.Pending->TargetState < group.States.size())
				return group.States[group.Pending->TargetState].Name;
			if (group.CurrentState && *group.CurrentState < group.States.size())
				return group.States[*group.CurrentState].Name;
		}
	return {};
}

bool Control::HasActiveVisualStateAnimations() const noexcept
{
	return _declarativeVisualStates
		&& _declarativeVisualStates->HasActiveAnimations();
}

bool Control::AdvanceVisualStateAnimations(
	unsigned long long nowMilliseconds)
{
	return _declarativeVisualStates
		&& _declarativeVisualStates->AdvanceAnimations(nowMilliseconds);
}

bool Control::SetDataContext(BindingSourceReference value)
{
	return TrySetPropertyValue(
		L"DataContext", BindingValue(std::move(value)),
		DependencyPropertyValueSource::Local);
}

bool Control::ClearDataContext()
{
	return ClearPropertyValue(
		L"DataContext", DependencyPropertyValueSource::Local);
}

IBindingSource& Control::DataContextSource()
{
	if (!_dataContextSource)
		_dataContextSource = std::make_unique<BindingSourceProxy>(
			_effectiveDataContext);
	return *_dataContextSource;
}

void Control::SetInheritedDataContext(BindingSourceReference value)
{
	if (value)
		(void)TrySetPropertyValue(
			L"DataContext", BindingValue(std::move(value)),
			DependencyPropertyValueSource::Inherited);
	else
		(void)ClearPropertyValue(
			L"DataContext", DependencyPropertyValueSource::Inherited);
}

void Control::UpdateEffectiveDataContext(BindingSourceReference value)
{
	if (_effectiveDataContext == value) return;
	_effectiveDataContext = std::move(value);
	if (_dataContextSource)
		_dataContextSource->SetSource(_effectiveDataContext);
	RebuildStyleDataContextSubscriptions();
	RefreshStyleValues(false);
	_dataContextChanged.Notify(L"DataContext");
	if (!_applyingPropertyMetadata)
		_bindingSourcePropertyChanged.Notify(L"DataContext");
}

GET_CPP(Control, BindingValue, Tag)
{
	return _tag;
}

SET_CPP(Control, BindingValue, Tag)
{
	(void)SetPropertyField(L"Tag", _tag, std::move(value));
}

GET_CPP(Control, CursorKind, Cursor)
{
	return _cursor;
}

SET_CPP(Control, CursorKind, Cursor)
{
	(void)SetPropertyField(L"Cursor", _cursor, value);
}

CursorKind Control::ResolvePointerCursor(int localX, int localY)
{
	// Auto is the native projection of WPF's null/default Cursor. It remains
	// inheritable without suppressing a behavior host's region-sensitive cursor.
	// Every concrete shape is authoritative regardless of its value source.
	return Cursor == CursorKind::Auto
		? QueryCursor(localX, localY) : Cursor;
}

GET_CPP(Control, bool, Focusable)
{
	return _focusable;
}

SET_CPP(Control, bool, Focusable)
{
	if (!SetPropertyField(L"Focusable", _focusable, value)) return;
	if (!_focusable && GetPresentationWindow()
		&& GetPresentationWindow()->GetKeyboardFocusedElement() == this)
		GetPresentationWindow()->SetKeyboardFocus(
			nullptr, true, FocusChangeReason::EligibilityChanged);
	if (GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(
			this, AccessibilityChange::State);
}

GET_CPP(Control, bool, IsTabStop)
{
	return _isTabStop;
}

SET_CPP(Control, bool, IsTabStop)
{
	if (SetPropertyField(L"IsTabStop", _isTabStop, value) && GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(this, AccessibilityChange::State);
}

GET_CPP(Control, int, TabIndex)
{
	return _tabIndex;
}

SET_CPP(Control, int, TabIndex)
{
	if (SetPropertyField(L"TabIndex", _tabIndex, (std::max)(0, value)) && GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(nullptr, AccessibilityChange::Structure);
}

GET_CPP(Control, bool, IsFocused)
{
	return _isFocused;
}

GET_CPP(Control, bool, IsKeyboardFocused)
{
	return _isKeyboardFocused;
}

GET_CPP(Control, bool, IsKeyboardFocusWithin)
{
	return _isKeyboardFocusWithin;
}

GET_CPP(Control, bool, IsMouseOver)
{
	return _isMouseOver;
}

GET_CPP(Control, bool, IsMouseDirectlyOver)
{
	return _isMouseDirectlyOver;
}

void Control::SetIsFocusedCore(bool value)
{
	if (_isFocused == value) return;
	if (!SetReadOnlyPropertyField(L"IsFocused", _isFocused, value)) return;
	SetStyleState(ControlStyleState::LogicalFocused, value);
}

void Control::SetIsKeyboardFocusedCore(bool value)
{
	if (_isKeyboardFocused == value) return;
	if (!SetReadOnlyPropertyField(
		L"IsKeyboardFocused", _isKeyboardFocused, value)) return;
	if (!value)
	{
		// Caret animation is presentation state owned by the focused element.
		// Do not wait for a later OnRender to retire it: focus can move while the
		// old editor is culled or while input capture keeps producing frames.
		_caretBlinkFocused = false;
		_caretBlinkRectValid = false;
		_caretBlinkRect = { 0, 0, 0, 0 };
	}
	_defaultLeftButtonPressActive = value
		? _defaultLeftButtonPressActive : false;
	SetStyleState(ControlStyleState::Focused, value);
	if (!value) SetStyleState(ControlStyleState::Pressed, false);
}

void Control::SetIsKeyboardFocusWithinCore(bool value)
{
	if (_isKeyboardFocusWithin == value) return;
	if (!SetReadOnlyPropertyField(
		L"IsKeyboardFocusWithin", _isKeyboardFocusWithin, value)) return;
	SetStyleState(ControlStyleState::KeyboardFocusWithin, value);
}

void Control::SetMouseOverCore(
	bool isMouseOver, bool isMouseDirectlyOver)
{
	isMouseDirectlyOver = isMouseOver && isMouseDirectlyOver;
	const bool previous = _isMouseOver;
	if (_isMouseDirectlyOver != isMouseDirectlyOver)
		(void)SetReadOnlyPropertyField(
			L"IsMouseDirectlyOver", _isMouseDirectlyOver,
			isMouseDirectlyOver);
	if (_isMouseOver == isMouseOver) return;
	(void)SetReadOnlyPropertyField(L"IsMouseOver", _isMouseOver, isMouseOver);
	SetStyleState(ControlStyleState::Hovered, isMouseOver);
	OnIsMouseOverChanged(previous, isMouseOver);
}

GET_CPP(Control, bool, IsFocusScope)
{
	return _isFocusScope;
}

SET_CPP(Control, bool, IsFocusScope)
{
	if (SetPropertyField(
		L"FocusManager.IsFocusScope", _isFocusScope, value) && GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(nullptr, AccessibilityChange::Structure);
}

GET_CPP(Control, KeyboardNavigationMode, TabNavigation)
{
	return _tabNavigation;
}

SET_CPP(Control, KeyboardNavigationMode, TabNavigation)
{
	if (SetPropertyField(
		L"KeyboardNavigation.TabNavigation", _tabNavigation, value)
		&& GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(nullptr, AccessibilityChange::Structure);
}

GET_CPP(Control, KeyboardNavigationMode, DirectionalNavigation)
{
	return _directionalNavigation;
}

SET_CPP(Control, KeyboardNavigationMode, DirectionalNavigation)
{
	if (SetPropertyField(
		L"KeyboardNavigation.DirectionalNavigation",
		_directionalNavigation, value) && GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(nullptr, AccessibilityChange::Structure);
}

GET_CPP(Control, std::wstring, AutomationName)
{
	return _automationName;
}

SET_CPP(Control, std::wstring, AutomationName)
{
	if (SetPropertyField(
		L"AutomationProperties.Name", _automationName, std::move(value))
		&& GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(this, AccessibilityChange::Name);
}

GET_CPP(Control, std::wstring, AutomationFullDescription)
{
	return _automationFullDescription;
}

SET_CPP(Control, std::wstring, AutomationFullDescription)
{
	if (SetPropertyField(L"AutomationProperties.FullDescription",
		_automationFullDescription, std::move(value)) && GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(this, AccessibilityChange::Description);
}

GET_CPP(Control, std::wstring, AutomationHelpText)
{
	return _automationHelpText;
}

SET_CPP(Control, std::wstring, AutomationHelpText)
{
	if (SetPropertyField(L"AutomationProperties.HelpText",
		_automationHelpText, std::move(value)) && GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(this, AccessibilityChange::Help);
}

GET_CPP(Control, std::wstring, AutomationId)
{
	return _automationId;
}

SET_CPP(Control, std::wstring, AutomationId)
{
	if (SetPropertyField(L"AutomationProperties.AutomationId",
		_automationId, std::move(value)) && GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(this, AccessibilityChange::Structure);
}

std::vector<BindingValidationResult> Control::GetValidationResults() const
{
	return _dataBindings
		? _dataBindings->GetValidationResults()
		: std::vector<BindingValidationResult>{};
}

bool Control::HasValidationIssues() const
{
	return _dataBindings && _dataBindings->HasValidationIssues();
}

bool Control::HasValidationErrors() const
{
	return _dataBindings && _dataBindings->HasValidationErrors();
}

bool Control::TryGetValidationSeverity(
	BindingValidationSeverity& severity) const
{
	const auto results = GetValidationResults();
	if (results.empty()) return false;
	severity = BindingValidationSeverity::Info;
	for (const auto& result : results)
	{
		if (ValidationSeverityRank(result.Issue.Severity)
			> ValidationSeverityRank(severity))
			severity = result.Issue.Severity;
	}
	return true;
}

std::wstring Control::GetValidationSummary(size_t maxIssues) const
{
	const auto results = GetValidationResults();
	std::vector<BindingValidationIssue> unique;
	unique.reserve(results.size());
	for (const auto& result : results)
	{
		if (std::find(unique.begin(), unique.end(), result.Issue) == unique.end())
			unique.push_back(result.Issue);
	}

	const size_t visibleCount = maxIssues == 0
		? unique.size()
		: (std::min)(unique.size(), maxIssues);
	std::wstring summary;
	for (size_t index = 0; index < visibleCount; ++index)
	{
		if (!summary.empty()) summary += L"\r\n";
		summary += L"[";
		summary += BindingValidationSeverityName(unique[index].Severity);
		summary += L"] ";
		summary += unique[index].Message;
	}
	if (unique.size() > visibleCount)
	{
		if (!summary.empty()) summary += L"\r\n";
		summary += L"+" + std::to_wstring(unique.size() - visibleCount)
			+ L" more validation issue(s)";
	}
	return summary;
}

std::wstring Control::GetEffectiveAutomationFullDescription() const
{
	const auto validation = GetValidationSummary();
	if (_automationFullDescription.empty()) return validation;
	if (validation.empty()) return _automationFullDescription;
	return _automationFullDescription + L"\r\n" + validation;
}

std::wstring Control::GetEffectiveAutomationName() const
{
	if (!_automationName.empty()) return _automationName;
	// Editable content is a value, not a label. Password content must never leak.
	switch (GetAutomationPeer().GetAutomationControlType())
	{
	case AutomationControlType::Edit:
	case AutomationControlType::ComboBox:
		return {};
	default:
		break;
	}
	return GetDisplayText();
}

std::wstring Control::GetDisplayText() const
{
	const auto semanticText = GetSemanticText();
	switch (GetAutomationPeer().GetAutomationControlType())
	{
	case AutomationControlType::Button:
	case AutomationControlType::Hyperlink:
	case AutomationControlType::CheckBox:
	case AutomationControlType::RadioButton:
	case AutomationControlType::Group:
	case AutomationControlType::MenuItem:
	case AutomationControlType::TabItem:
		return StripAccessKeyMarkers(semanticText);
	default:
		return semanticText;
	}
}

std::wstring Control::GetSemanticText() const
{
	return _text;
}

wchar_t Control::GetEffectiveAccessKey() const
{
	switch (GetAutomationPeer().GetAutomationControlType())
	{
	case AutomationControlType::Button:
	case AutomationControlType::Hyperlink:
	case AutomationControlType::CheckBox:
	case AutomationControlType::RadioButton:
	case AutomationControlType::Group:
	case AutomationControlType::MenuItem:
	case AutomationControlType::TabItem:
		return FindAccessKeyMarker(GetSemanticText());
	default:
		return L'\0';
	}
}

std::wstring Control::GetEffectiveKeyboardShortcut() const
{
	const wchar_t key = GetEffectiveAccessKey();
	return key == L'\0' ? std::wstring{} : std::wstring(L"Alt+") + key;
}

bool Control::CanReceiveKeyboardFocus() const
{
	return _focusable && IsEffectivelyEnabled() && GetIsVisible();
}

bool Control::GetIsVisible() const
{
	const Control* current = this;
	const Control* fast = this;
	while (current)
	{
		if (current->_presentationSuppressed
			|| current->_visibility != ::Visibility::Visible) return false;
		current = current->_visualParent;
		if (fast) fast = fast->_visualParent;
		if (fast) fast = fast->_visualParent;
		if (current && current == fast) return false;
	}
	return true;
}

bool Control::IsEffectivelyEnabled() const noexcept
{
	const Control* current = this;
	const Control* fast = this;
	while (current)
	{
		if (!current->_localEnabled
			|| (current->_hasCommandCanExecute
				&& !current->_commandCanExecute))
			return false;
		current = current->GetRoutedParent();
		if (fast) fast = fast->GetRoutedParent();
		if (fast) fast = fast->GetRoutedParent();
		if (current && current == fast) return false;
	}
	return true;
}

namespace
{
	void CaptureEffectiveIsEnabledSubtree(
		Control& root,
		std::vector<std::pair<ControlWeakReference, bool>>& snapshot,
		std::unordered_set<Control*>& visited)
	{
		if (!visited.insert(&root).second) return;
		snapshot.emplace_back(
			ControlWeakReference(&root), root.IsEffectivelyEnabled());
		for (auto* child : root.GetLayoutChildrenView())
			if (child && child->GetRoutedParent() == &root)
				CaptureEffectiveIsEnabledSubtree(*child, snapshot, visited);
		for (auto* child : root.GetLogicalChildrenView())
			if (child && child->GetRoutedParent() == &root)
				CaptureEffectiveIsEnabledSubtree(*child, snapshot, visited);
	}

	std::vector<std::pair<ControlWeakReference, bool>>
		CaptureEffectiveIsEnabledSubtree(
		Control& root)
	{
		std::vector<std::pair<ControlWeakReference, bool>> snapshot;
		std::unordered_set<Control*> visited;
		CaptureEffectiveIsEnabledSubtree(root, snapshot, visited);
		return snapshot;
	}

	void CaptureEffectiveIsVisibleSubtree(
		Control& root,
		std::vector<std::pair<ControlWeakReference, bool>>& snapshot,
		std::unordered_set<Control*>& visited)
	{
		if (!visited.insert(&root).second) return;
		snapshot.emplace_back(
			ControlWeakReference(&root), root.GetIsVisible());
		for (auto* child : root.GetLayoutChildrenView())
			if (child && child->GetVisualParent() == &root)
				CaptureEffectiveIsVisibleSubtree(*child, snapshot, visited);
	}

	std::vector<std::pair<ControlWeakReference, bool>>
		CaptureEffectiveIsVisibleSubtree(Control& root)
	{
		std::vector<std::pair<ControlWeakReference, bool>> snapshot;
		std::unordered_set<Control*> visited;
		CaptureEffectiveIsVisibleSubtree(root, snapshot, visited);
		return snapshot;
	}
}

void Control::PublishEffectiveIsEnabledChanges(
	std::vector<std::pair<ControlWeakReference, bool>> snapshot)
{
	std::vector<ControlWeakReference> cursorRefreshWindows;
	for (const auto& [elementReference, previousValue] : snapshot)
	{
		auto* element = elementReference.Get();
		if (!element || element->IsDestroying()) continue;
		const bool current = element->IsEffectivelyEnabled();
		if (current == previousValue) continue;
		if (const auto* metadata = element->FindPropertyMetadata(L"IsEnabled"))
		{
			// Command availability is an effective-value coercion, not a new
			// local value. Publish it through the dependency-property channel so
			// bindings, triggers and accessibility stay coherent.
			element->ApplyPropertyMetadataChange(
				*metadata, BindingValue(previousValue), BindingValue(current));
		}
		element = elementReference.Get();
		if (!element || element->IsDestroying()
			|| element->IsEffectivelyEnabled() != current) continue;
		element->OnEffectiveIsEnabledChanged(previousValue, current);
		element = elementReference.Get();
		if (!element || element->IsDestroying()
			|| element->IsEffectivelyEnabled() != current) continue;
		element->RefreshStyleValues(false);
		element = elementReference.Get();
		if (!element || element->IsDestroying()
			|| element->IsEffectivelyEnabled() != current) continue;
		element->InvalidateVisual();
		element = elementReference.Get();
		if (!element || element->IsDestroying()
			|| element->IsEffectivelyEnabled() != current) continue;
		if (element->GetPresentationWindow())
		{
			ControlWeakReference windowReference(element->GetPresentationWindow());
			auto resolveWindow = [&]() -> Window*
			{
				auto* liveElement = elementReference.Get();
				auto* liveWindow = dynamic_cast<Window*>(windowReference.Get());
				return liveElement && !liveElement->IsDestroying()
					&& liveElement->IsEffectivelyEnabled() == current
					&& liveWindow
					&& liveElement->GetPresentationWindow() == liveWindow
					? liveWindow : nullptr;
			};
			auto* window = resolveWindow();
			if (!window) continue;
			if (!current)
			{
				if (window->GetKeyboardFocusedElement() == element)
					window->SetKeyboardFocus(
						nullptr, true, FocusChangeReason::EligibilityChanged);
				element = elementReference.Get();
				window = resolveWindow();
				if (!element || !window) continue;
				if (window->GetMouseCaptured() == element)
					(void)window->ReleaseMouseCapture(element);
				element = elementReference.Get();
				window = resolveWindow();
				if (!element || !window) continue;
				if (element->IsMouseOver)
					cursorRefreshWindows.emplace_back(window);
			}
			element = elementReference.Get();
			window = resolveWindow();
			if (!element || !window) continue;
			window->NotifyAccessibilityEvent(
				element, AccessibilityChange::State);
		}
	}
	for (const auto& windowReference : cursorRefreshWindows)
		if (auto* window = dynamic_cast<Window*>(windowReference.Get()))
			cui::framework::WindowAccess::UpdateCursorFromCurrentMouse(*window);
}

void Control::PublishEffectiveIsVisibleChanges(
	std::vector<std::pair<ControlWeakReference, bool>> snapshot)
{
	std::vector<ControlWeakReference> cursorRefreshWindows;
	for (const auto& [elementReference, previousValue] : snapshot)
	{
		auto* element = elementReference.Get();
		if (!element || element->IsDestroying()) continue;
		const bool current = element->GetIsVisible();
		if (current == previousValue) continue;
		if (const auto* metadata = element->FindPropertyMetadata(L"IsVisible"))
			element->ApplyPropertyMetadataChange(
				*metadata, BindingValue(previousValue), BindingValue(current));
		element = elementReference.Get();
		if (!element || element->IsDestroying()
			|| element->GetIsVisible() != current) continue;
		element->OnEffectiveIsVisibleChanged(previousValue, current);
		element = elementReference.Get();
		if (!element || element->IsDestroying()
			|| element->GetIsVisible() != current) continue;
		DependencyPropertyChangedEventArgs visibleChanged{
			L"IsVisible", BindingValue(previousValue), BindingValue(current) };
		cui::framework::EventAccess::Raise(
			element->IsVisibleChanged, element, visibleChanged);
		element = elementReference.Get();
		if (!element || element->IsDestroying()
			|| element->GetIsVisible() != current) continue;
		element->RefreshStyleValues(false);
		element = elementReference.Get();
		if (!element || element->IsDestroying()
			|| element->GetIsVisible() != current) continue;
		element->InvalidateVisual();
		element = elementReference.Get();
		if (!element || element->IsDestroying()
			|| element->GetIsVisible() != current || !element->GetPresentationWindow()) continue;
		ControlWeakReference windowReference(element->GetPresentationWindow());
		auto* window = dynamic_cast<Window*>(windowReference.Get());
		if (!window || element->GetPresentationWindow() != window) continue;
		if (!current)
		{
			auto* focused = window->GetKeyboardFocusedElement();
			std::unordered_set<Control*> visited;
			for (auto* candidate = focused;
				candidate && visited.insert(candidate).second;
				candidate = candidate->GetVisualParent())
				if (candidate == element)
				{
					window->SetKeyboardFocus(
						nullptr, true, FocusChangeReason::EligibilityChanged);
					break;
				}
			element = elementReference.Get();
			window = dynamic_cast<Window*>(windowReference.Get());
			if (!element || !window || element->GetPresentationWindow() != window) continue;
			if (window->GetMouseCaptured() == element)
				(void)window->ReleaseMouseCapture(element);
			element = elementReference.Get();
			window = dynamic_cast<Window*>(windowReference.Get());
			if (!element || !window || element->GetPresentationWindow() != window) continue;
			if (element->IsMouseOver)
				cursorRefreshWindows.emplace_back(window);
		}
		element = elementReference.Get();
		window = dynamic_cast<Window*>(windowReference.Get());
		if (element && window && element->GetPresentationWindow() == window)
			window->NotifyAccessibilityEvent(
				element, AccessibilityChange::State);
	}
	for (const auto& windowReference : cursorRefreshWindows)
		if (auto* window = dynamic_cast<Window*>(windowReference.Get()))
			cui::framework::WindowAccess::UpdateCursorFromCurrentMouse(*window);
}

void Control::SetLocalEnabled(bool value)
{
	VerifyAccess();
	if (_localEnabled == value) return;
	auto snapshot = CaptureEffectiveIsEnabledSubtree(*this);
	_localEnabled = value;
	PublishEffectiveIsEnabledChanges(std::move(snapshot));
}

void Control::SetCommandCanExecuteState(bool value)
{
	VerifyAccess();
	if (_hasCommandCanExecute && _commandCanExecute == value) return;
	auto snapshot = CaptureEffectiveIsEnabledSubtree(*this);
	_hasCommandCanExecute = true;
	_commandCanExecute = value;
	PublishEffectiveIsEnabledChanges(std::move(snapshot));
}

void Control::ClearCommandCanExecuteState()
{
	VerifyAccess();
	if (!_hasCommandCanExecute) return;
	auto snapshot = CaptureEffectiveIsEnabledSubtree(*this);
	_hasCommandCanExecute = false;
	_commandCanExecute = true;
	PublishEffectiveIsEnabledChanges(std::move(snapshot));
}

bool Control::CanParticipateInTabNavigation() const
{
	return _isTabStop && CanReceiveKeyboardFocus();
}

bool Control::Focus()
{
	if (!GetPresentationWindow() || !CanReceiveKeyboardFocus()) return false;
	if (GetPresentationWindow()->Handle && ::GetFocus() != GetPresentationWindow()->Handle)
		::SetFocus(GetPresentationWindow()->Handle);
	GetPresentationWindow()->SetKeyboardFocus(this, true);
	return GetPresentationWindow()->GetKeyboardFocusedElement() == this;
}

bool Control::CaptureMouse()
{
	return GetPresentationWindow() && GetPresentationWindow()->CaptureMouse(this);
}

bool Control::ReleaseMouseCapture()
{
	return GetPresentationWindow() && GetPresentationWindow()->ReleaseMouseCapture(this);
}

bool Control::IsMouseCaptured() const
{
	return GetPresentationWindow() && GetPresentationWindow()->GetMouseCaptured() == this;
}

bool Control::Invoke()
{
	return false;
}

bool Control::AreSystemAnimationsEnabled() const
{
	return !GetPresentationWindow() || GetPresentationWindow()->AreSystemAnimationsEnabled();
}

UINT Control::EffectiveAnimationDuration(UINT configuredDurationMs) const
{
	return AreSystemAnimationsEnabled() ? configuredDurationMs : 0U;
}

AccessibilitySnapshot Control::GetAccessibilitySnapshot() const
{
	AccessibilitySnapshot snapshot;
	auto& peer = GetAutomationPeer();
	snapshot.ControlType = peer.GetAutomationControlType();
	snapshot.Name = GetEffectiveAutomationName();
	snapshot.Description = GetEffectiveAutomationFullDescription();
	snapshot.HelpText = _automationHelpText;
	snapshot.AutomationId = _automationId;
	snapshot.KeyboardShortcut = GetEffectiveKeyboardShortcut();
	snapshot.Enabled = IsEffectivelyEnabled();
	snapshot.Visible = GetIsVisible();
	snapshot.Focusable = CanReceiveKeyboardFocus();
	snapshot.Focused = _isKeyboardFocused;
	if (!peer.TryGetSelectionItemSelected(snapshot.Selected))
	{
		BindingValue selectedValue;
		if (const_cast<Control*>(this)->TryGetPropertyValue(
			L"IsSelected", selectedValue))
			(void)selectedValue.TryGet(snapshot.Selected);
	}
	if (!peer.TryGetToggleState(snapshot.Checked))
		snapshot.Checked = IsCheckedForAccessibility();
	snapshot.Password = peer.IsPassword();
	snapshot.ReadOnly = peer.IsReadOnly();
	snapshot.Value = peer.GetValue();
	if (snapshot.Value.empty() && !snapshot.Password)
	{
		BindingValue value;
		if (const_cast<Control*>(this)->TryGetPropertyValue(L"Value", value))
			snapshot.Value = value.ToString();
	}
	return snapshot;
}

void Control::OnBindingValidationChanged(
	const std::wstring& targetProperty)
{
	auto nextErrors = GetValidationResults();
	const bool nextHasError = std::any_of(
		nextErrors.begin(), nextErrors.end(),
		[](const BindingValidationResult& result)
		{
			return result.Issue.Severity == BindingValidationSeverity::Error;
		});
	if (_validationErrors != nextErrors)
		(void)SetReadOnlyPropertyField(
			L"Validation.Errors", _validationErrors, std::move(nextErrors));
	if (_validationHasError != nextHasError)
		(void)SetReadOnlyPropertyField(
			L"Validation.HasError", _validationHasError, nextHasError);
	InvalidateVisual();
	if (GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(
			this, AccessibilityChange::Description);
	OnValidationStateChanged.Notify(targetProperty);
}

void Control::NotifyAccessibilityStructureChanged()
{
	if (GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(nullptr, AccessibilityChange::Structure);
}

void Control::NotifyAccessibilityStateChanged()
{
	if (GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(this, AccessibilityChange::State);
}

void Control::NotifyAccessibilityValueChanged()
{
	if (GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(this, AccessibilityChange::Value);
}

void Control::NotifyAccessibilityScrollChanged()
{
	if (GetPresentationWindow())
		GetPresentationWindow()->NotifyAccessibilityEvent(this, AccessibilityChange::Scroll);
}

void Control::NotifyAccessibilityVirtualChanged(
	uint32_t virtualId, AccessibilityChange change)
{
	if (GetPresentationWindow() && virtualId != 0)
		GetPresentationWindow()->NotifyAccessibilityVirtualEvent(this, virtualId, change);
}

void Control::RegisterDependencyProperties()
{
	static std::once_flag once;
	std::call_once(once, []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		auto dataContextDesign = PropertyDesign(
			L"Data", 250, 0, DependencyPropertyPersistence::Native);
		dataContextDesign.Browsable = false;
		DependencyPropertyOptions<Control, BindingSourceReference> dataContextOptions;
		dataContextOptions.DefaultValue = BindingSourceReference{};
		dataContextOptions.Flags = DependencyPropertyFlags::Inherits;
		dataContextOptions.Equals = [](const BindingSourceReference& left,
			const BindingSourceReference& right) { return left == right; };
		dataContextOptions.Design = std::move(dataContextDesign);
		DependencyPropertyRegistry::Register<Control, BindingSourceReference>(
			L"DataContext",
			[](Control& target) { return target.GetDataContext(); },
			[](Control& target, const BindingSourceReference& value)
			{
				target.UpdateEffectiveDataContext(value);
			},
			[](Control& target, Handler handler, DataSourceUpdateMode)
			{
				return target.DataContextChanged().Subscribe(
					[handler = std::move(handler)](const PropertyChangedEventArgs&)
					{ handler(); });
			},
			std::move(dataContextOptions));
		auto visibilityDesign = PropertyDesign(L"Common", 0, 30,
			DependencyPropertyPersistence::Native,
			DependencyPropertyEditorKind::Choice);
		visibilityDesign.Choices = {
			PropertyChoice(L"Visible", std::wstring(L"Visible")),
			PropertyChoice(L"Hidden", std::wstring(L"Hidden")),
			PropertyChoice(L"Collapsed", std::wstring(L"Collapsed"))
		};
		DependencyPropertyOptions<Control, std::wstring> visibilityOptions;
		visibilityOptions.DefaultValue = L"Visible";
		visibilityOptions.Flags = DependencyPropertyFlags::AffectsMeasure;
		visibilityOptions.Design = std::move(visibilityDesign);
		visibilityOptions.Coerce = [](Control&, const std::wstring& value)
			-> std::optional<std::wstring>
		{
			if (_wcsicmp(value.c_str(), L"Visible") == 0) return L"Visible";
			if (_wcsicmp(value.c_str(), L"Hidden") == 0) return L"Hidden";
			if (_wcsicmp(value.c_str(), L"Collapsed") == 0) return L"Collapsed";
			return std::nullopt;
		};
		DependencyPropertyRegistry::Register<Control, std::wstring>(L"Visibility",
			[](Control& target)
			{
				return std::wstring(VisibilityName(target.Visibility));
			},
			[](Control& target, const std::wstring& value)
			{
				target.Visibility = _wcsicmp(value.c_str(), L"Hidden") == 0
					? ::Visibility::Hidden
					: _wcsicmp(value.c_str(), L"Collapsed") == 0
						? ::Visibility::Collapsed : ::Visibility::Visible;
			}, {}, std::move(visibilityOptions));

		auto isVisibleDesign = PropertyDesign(L"Common", 0, 31,
			DependencyPropertyPersistence::Transient,
			DependencyPropertyEditorKind::Boolean);
		DependencyPropertyOptions<Control, bool> isVisibleOptions;
		isVisibleOptions.DefaultValue = true;
		isVisibleOptions.IsReadOnly = true;
		isVisibleOptions.Design = std::move(isVisibleDesign);
		DependencyPropertyRegistry::Register<Control, bool>(L"IsVisible",
			[](Control& target) { return target.GetIsVisible(); },
			{},
			[](Control& target, Handler handler, DataSourceUpdateMode)
			{
				return target.IsVisibleChanged.Subscribe(
					[handler = std::move(handler)](
						DependencyObject*,
						const DependencyPropertyChangedEventArgs&)
					{ handler(); });
			}, std::move(isVisibleOptions));

		auto enabledDesign = PropertyDesign(L"Common", 0, 20,
			DependencyPropertyPersistence::Native,
			DependencyPropertyEditorKind::Boolean, L"Is enabled");
		DependencyPropertyRegistry::Register<Control, bool>(L"IsEnabled",
			[](Control& target) { return target.IsEffectivelyEnabled(); },
			[](Control& target, const bool& value)
			{ target.SetLocalEnabled(value); },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, bool>{
				true, DependencyPropertyFlags::AffectsRender },
				std::move(enabledDesign)));

		auto allowDropDesign = PropertyDesign(L"Behavior", 300, 0,
			DependencyPropertyPersistence::Metadata,
			DependencyPropertyEditorKind::Boolean, L"Allow drop");
		DependencyPropertyRegistry::Register<Control, bool>(L"AllowDrop",
			[](Control& target) { return target.AllowDrop; },
			[](Control& target, const bool& value) { target.AllowDrop = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, bool>{
				false, DependencyPropertyFlags::Inherits },
				std::move(allowDropDesign)));

		auto sizedSubscriber = [](Control& target, Handler handler, DataSourceUpdateMode)
		{
			return target.SizeChanged.Subscribe(
				[handler = std::move(handler)](
					Control*, SizeChangedEventArgs&) { handler(); });
		};

		auto canvasOffsetOptions = [](int order, const wchar_t* displayName)
		{
			DependencyPropertyOptions<Control, float> options;
			options.DefaultValue = cui::layout::UnsetCanvasOffset;
			options.Flags = DependencyPropertyFlags::AffectsParentArrange;
			options.Coerce = [](Control&, const float& value)
				-> std::optional<float>
			{
				return std::isfinite(value) || std::isnan(value)
					? std::optional<float>{ value }
					: std::nullopt;
			};
			options.Equals = [](const float& left, const float& right)
			{
				return left == right
					|| (std::isnan(left) && std::isnan(right));
			};
			options.Design = PropertyDesign(
				L"Layout", 100, order,
				DependencyPropertyPersistence::Metadata,
				DependencyPropertyEditorKind::Number,
				displayName);
			options.Design.Step = 0.5;
			return options;
		};
		DependencyPropertyRegistry::Register<Control, float>(L"Canvas.Left",
			[](Control& target) { return target.CanvasLeft; },
			[](Control& target, const float& value) { target.CanvasLeft = value; },
			{}, canvasOffsetOptions(10, L"Canvas.Left"));
		DependencyPropertyRegistry::Register<Control, float>(L"Canvas.Top",
			[](Control& target) { return target.CanvasTop; },
			[](Control& target, const float& value) { target.CanvasTop = value; },
			{}, canvasOffsetOptions(20, L"Canvas.Top"));
		DependencyPropertyRegistry::Register<Control, float>(L"Canvas.Right",
			[](Control& target) { return target.CanvasRight; },
			[](Control& target, const float& value) { target.CanvasRight = value; },
			{}, canvasOffsetOptions(30, L"Canvas.Right"));
		DependencyPropertyRegistry::Register<Control, float>(L"Canvas.Bottom",
			[](Control& target) { return target.CanvasBottom; },
			[](Control& target, const float& value) { target.CanvasBottom = value; },
			{}, canvasOffsetOptions(40, L"Canvas.Bottom"));
		auto lengthOptions = [](int order)
		{
			DependencyPropertyOptions<Control, cui::layout::Length> options;
			options.DefaultValue = cui::layout::Length::Auto();
			options.Flags = DependencyPropertyFlags::AffectsMeasure;
			options.Convert = ConvertLayoutLength;
			options.Design = PropertyDesign(
				L"Layout", 100, order,
				DependencyPropertyPersistence::Metadata,
				DependencyPropertyEditorKind::Length);
			return options;
		};
		DependencyPropertyRegistry::Register<Control, cui::layout::Length>(L"Width",
			[](Control& target) { return target.Width; },
			[](Control& target, const cui::layout::Length& value) { target.Width = value; },
			{}, lengthOptions(30));
		DependencyPropertyRegistry::Register<Control, cui::layout::Length>(L"Height",
			[](Control& target) { return target.Height; },
			[](Control& target, const cui::layout::Length& value) { target.Height = value; },
			{}, lengthOptions(40));
		auto actualSizeOptions = [](int order)
		{
			DependencyPropertyOptions<Control, float> options;
			options.DefaultValue = 0.0f;
			options.IsReadOnly = true;
			options.Design = PropertyDesign(L"Layout", 100, order,
				DependencyPropertyPersistence::Transient,
				DependencyPropertyEditorKind::Number);
			options.Design.Browsable = false;
			return options;
		};
		DependencyPropertyRegistry::Register<Control, float>(L"ActualWidth",
			[](Control& target) { return target.ActualWidth; }, {}, sizedSubscriber,
			actualSizeOptions(50));
		DependencyPropertyRegistry::Register<Control, float>(L"ActualHeight",
			[](Control& target) { return target.ActualHeight; }, {}, sizedSubscriber,
			actualSizeOptions(60));
		DependencyPropertyRegistry::Register<Control, Thickness>(L"Margin",
			[](Control& target) { return target.Margin; },
			[](Control& target, const Thickness& value) { target.Margin = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, Thickness>{
				Thickness{}, DependencyPropertyFlags::AffectsMeasure },
				PropertyDesign(L"Layout", 100, 70, DependencyPropertyPersistence::Native)));
		DependencyPropertyRegistry::Register<Control, Thickness>(L"Padding",
			[](Control& target) { return target.Padding; },
			[](Control& target, const Thickness& value) { target.Padding = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, Thickness>{
				Thickness{}, DependencyPropertyFlags::AffectsMeasure },
				PropertyDesign(L"Layout", 100, 80, DependencyPropertyPersistence::Native)));
		auto horizontalAlignmentDesign = PropertyDesign(
			L"Layout", 100, 90, DependencyPropertyPersistence::Native,
			DependencyPropertyEditorKind::Choice);
		horizontalAlignmentDesign.Choices = {
			PropertyChoice(L"Left", ::HorizontalAlignment::Left),
			PropertyChoice(L"Center", ::HorizontalAlignment::Center),
			PropertyChoice(L"Right", ::HorizontalAlignment::Right),
			PropertyChoice(L"Stretch", ::HorizontalAlignment::Stretch)
		};
		DependencyPropertyRegistry::Register<Control, ::HorizontalAlignment>(
			L"HorizontalAlignment",
			[](Control& target) { return target.HorizontalAlignment; },
			[](Control& target, const ::HorizontalAlignment& value)
			{ target.HorizontalAlignment = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, ::HorizontalAlignment>{
				::HorizontalAlignment::Stretch, DependencyPropertyFlags::AffectsArrange },
				std::move(horizontalAlignmentDesign)));
		auto verticalAlignmentDesign = PropertyDesign(
			L"Layout", 100, 100, DependencyPropertyPersistence::Native,
			DependencyPropertyEditorKind::Choice);
		verticalAlignmentDesign.Choices = {
			PropertyChoice(L"Top", ::VerticalAlignment::Top),
			PropertyChoice(L"Center", ::VerticalAlignment::Center),
			PropertyChoice(L"Bottom", ::VerticalAlignment::Bottom),
			PropertyChoice(L"Stretch", ::VerticalAlignment::Stretch)
		};
		DependencyPropertyRegistry::Register<Control, ::VerticalAlignment>(
			L"VerticalAlignment",
			[](Control& target) { return target.VerticalAlignment; },
			[](Control& target, const ::VerticalAlignment& value)
			{ target.VerticalAlignment = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, ::VerticalAlignment>{
				::VerticalAlignment::Stretch, DependencyPropertyFlags::AffectsArrange },
				std::move(verticalAlignmentDesign)));
		DependencyPropertyRegistry::Register<Control, int>(L"ZIndex",
			[](Control& target) { return target.ZIndex; },
			[](Control& target, const int& value) { target.ZIndex = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, int>{
				0, DependencyPropertyFlags::None },
				PropertyDesign(L"Layout", 100, 105,
					DependencyPropertyPersistence::Native,
					DependencyPropertyEditorKind::Number)));
		auto gridPlacementDesign = [](int order)
		{
			auto design = PropertyDesign(L"Layout", 100, order,
				DependencyPropertyPersistence::Native,
				DependencyPropertyEditorKind::Number);
			design.BrowsableWhen = [](DependencyObject& object)
			{
				auto* target = dynamic_cast<Control*>(&object);
				return target && target->GetLogicalParent()
					&& target->GetLogicalParent()->Type() == UIClass::UI_Grid;
			};
			return design;
		};
		DependencyPropertyRegistry::Register<Control, int>(L"Grid.Row",
			[](Control& target) { return target.GridRow; },
			[](Control& target, const int& value) { target.GridRow = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, int>{
				0,
				DependencyPropertyFlags::AffectsMeasure,
				[](Control&, const int& proposed) -> std::optional<int>
				{
					return (std::max)(0, proposed);
				} }, gridPlacementDesign(110)));
		DependencyPropertyRegistry::Register<Control, int>(L"Grid.Column",
			[](Control& target) { return target.GridColumn; },
			[](Control& target, const int& value) { target.GridColumn = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, int>{
				0,
				DependencyPropertyFlags::AffectsMeasure,
				[](Control&, const int& proposed) -> std::optional<int>
				{
					return (std::max)(0, proposed);
				} }, gridPlacementDesign(120)));
		DependencyPropertyRegistry::Register<Control, int>(L"Grid.RowSpan",
			[](Control& target) { return target.GridRowSpan; },
			[](Control& target, const int& value) { target.GridRowSpan = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, int>{
				1,
				DependencyPropertyFlags::AffectsMeasure,
				[](Control&, const int& proposed) -> std::optional<int>
				{
					return (std::max)(1, proposed);
				} }, gridPlacementDesign(130)));
		DependencyPropertyRegistry::Register<Control, int>(L"Grid.ColumnSpan",
			[](Control& target) { return target.GridColumnSpan; },
			[](Control& target, const int& value) { target.GridColumnSpan = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, int>{
				1,
				DependencyPropertyFlags::AffectsMeasure,
				[](Control&, const int& proposed) -> std::optional<int>
				{
					return (std::max)(1, proposed);
				} }, gridPlacementDesign(140)));
		auto dockDesign = PropertyDesign(
			L"Layout", 100, 150, DependencyPropertyPersistence::Native,
			DependencyPropertyEditorKind::Choice);
		dockDesign.Choices = {
			PropertyChoice(L"Left", Dock::Left),
			PropertyChoice(L"Top", Dock::Top),
			PropertyChoice(L"Right", Dock::Right),
			PropertyChoice(L"Bottom", Dock::Bottom)
		};
		dockDesign.DisplayName = L"Dock";
		dockDesign.BrowsableWhen = [](DependencyObject& object)
		{
			auto* target = dynamic_cast<Control*>(&object);
			return target && target->GetLogicalParent()
				&& target->GetLogicalParent()->Type() == UIClass::UI_DockPanel;
		};
		DependencyPropertyOptions<Control, Dock> dockOptions{
			Dock::Left, DependencyPropertyFlags::AffectsMeasure };
		dockOptions.Coerce = [](Control&, const Dock& value)
			-> std::optional<Dock>
		{
			switch (value)
			{
			case Dock::Left:
			case Dock::Top:
			case Dock::Right:
			case Dock::Bottom:
				return value;
			default:
				return std::nullopt;
			}
		};
		DependencyPropertyRegistry::Register<Control, Dock>(L"DockPanel.Dock",
			[](Control& target) { return target.DockPosition; },
			[](Control& target, const Dock& value) { target.DockPosition = value; },
			{},
			WithPropertyDesign(std::move(dockOptions), std::move(dockDesign)));
		auto minimumOptions = [](int order)
		{
			DependencyPropertyOptions<Control, float> options;
			options.DefaultValue = 0.0f;
			options.Flags = DependencyPropertyFlags::AffectsMeasure;
			options.Coerce = [](Control&, const float& value)
				-> std::optional<float>
			{
				return std::isfinite(value) && value >= 0.0f
					? std::optional<float>{ value } : std::nullopt;
			};
			options.Design = PropertyDesign(L"Layout", 100, order,
				DependencyPropertyPersistence::Metadata,
				DependencyPropertyEditorKind::Number);
			return options;
		};
		auto maximumOptions = [](int order)
		{
			DependencyPropertyOptions<Control, float> options;
			options.DefaultValue = cui::core::Infinity;
			options.Flags = DependencyPropertyFlags::AffectsMeasure;
			options.Coerce = [](Control&, const float& value)
				-> std::optional<float>
			{
				return !std::isnan(value) && value >= 0.0f
					? std::optional<float>{ value } : std::nullopt;
			};
			options.Design = PropertyDesign(L"Layout", 100, order,
				DependencyPropertyPersistence::Metadata,
				DependencyPropertyEditorKind::Number);
			return options;
		};
		DependencyPropertyRegistry::Register<Control, float>(L"MinWidth",
			[](Control& target) { return target.MinWidth; },
			[](Control& target, const float& value) { target.MinWidth = value; },
			{}, minimumOptions(160));
		DependencyPropertyRegistry::Register<Control, float>(L"MinHeight",
			[](Control& target) { return target.MinHeight; },
			[](Control& target, const float& value) { target.MinHeight = value; },
			{}, minimumOptions(170));
		DependencyPropertyRegistry::Register<Control, float>(L"MaxWidth",
			[](Control& target) { return target.MaxWidth; },
			[](Control& target, const float& value) { target.MaxWidth = value; },
			{}, maximumOptions(180));
		DependencyPropertyRegistry::Register<Control, float>(L"MaxHeight",
			[](Control& target) { return target.MaxHeight; },
			[](Control& target, const float& value) { target.MaxHeight = value; },
			{}, maximumOptions(190));
		DependencyPropertyOptions<Control, std::wstring> fontNameOptions;
		fontNameOptions.DefaultValue = std::wstring(L"Arial");
		fontNameOptions.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		fontNameOptions.Coerce = [](Control&, const std::wstring& proposed)
			-> std::optional<std::wstring>
		{
			auto first = std::find_if_not(
				proposed.begin(), proposed.end(), iswspace);
			auto last = std::find_if_not(
				proposed.rbegin(), proposed.rend(), iswspace).base();
			if (first >= last) return std::nullopt;
			return std::wstring(first, last);
		};
		fontNameOptions.Design = PropertyDesign(
			L"Appearance", 200, 30,
			DependencyPropertyPersistence::Metadata,
			DependencyPropertyEditorKind::Text, L"Font name");
		DependencyPropertyRegistry::Register<Control, std::wstring>(L"FontFamily",
			[](Control& target) { return target._fontName; },
			[](Control& target, const std::wstring& value)
			{
				target._fontName = value;
				target.ApplyTypographyFont();
			}, {}, std::move(fontNameOptions));

		DependencyPropertyOptions<Control, double> fontSizeOptions;
		fontSizeOptions.DefaultValue = 14.0;
		fontSizeOptions.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		fontSizeOptions.Coerce = [](Control&, const double& proposed)
			-> std::optional<double>
		{
			return std::isfinite(proposed) && proposed >= 1.0 && proposed <= 200.0
				? std::optional<double>{ proposed } : std::nullopt;
		};
		fontSizeOptions.Design = PropertyDesign(
			L"Appearance", 200, 40,
			DependencyPropertyPersistence::Metadata,
			DependencyPropertyEditorKind::Number, L"Font size");
		fontSizeOptions.Design.Minimum = 1.0;
		fontSizeOptions.Design.Maximum = 200.0;
		fontSizeOptions.Design.Step = 0.5;
		DependencyPropertyRegistry::Register<Control, double>(L"FontSize",
			[](Control& target) { return target._fontSize; },
			[](Control& target, const double& value)
			{
				target._fontSize = value;
				target.ApplyTypographyFont();
			}, {}, std::move(fontSizeOptions));
		auto brushEquals = [](const cui::drawing::Brush& left,
			const cui::drawing::Brush& right)
		{
			return left.Kind == right.Kind
				&& left.MappingMode == right.MappingMode
				&& left.Color.r == right.Color.r
				&& left.Color.g == right.Color.g
				&& left.Color.b == right.Color.b
				&& left.Color.a == right.Color.a
				&& left.Opacity == right.Opacity
				&& left.StartPoint.x == right.StartPoint.x
				&& left.StartPoint.y == right.StartPoint.y
				&& left.EndPoint.x == right.EndPoint.x
				&& left.EndPoint.y == right.EndPoint.y
				&& left.Center.x == right.Center.x
				&& left.Center.y == right.Center.y
				&& left.GradientOrigin.x == right.GradientOrigin.x
				&& left.GradientOrigin.y == right.GradientOrigin.y
				&& left.RadiusX == right.RadiusX
				&& left.RadiusY == right.RadiusY
				&& left.GradientStops == right.GradientStops
				&& left.ImageSource == right.ImageSource
				&& left.Stretch == right.Stretch
				&& left.AlignmentX == right.AlignmentX
				&& left.AlignmentY == right.AlignmentY
				&& left.Transform == right.Transform
				&& left.RelativeTransform == right.RelativeTransform;
		};
		auto solidBrush = [](D2D1_COLOR_F color)
		{
			return cui::drawing::MakeSolidColorBrush(color);
		};
		auto convertBrush = [solidBrush](const BindingValue& value)
			-> std::optional<cui::drawing::Brush>
		{
			cui::drawing::Brush brush;
			if (value.TryGet(brush)) return brush;
			D2D1_COLOR_F color{};
			if (value.TryGet(color)) return solidBrush(color);
			return std::nullopt;
		};
		DependencyPropertyOptions<Control, cui::drawing::Brush> backgroundOptions;
		backgroundOptions.DefaultValue = cui::drawing::NoBrush();
		backgroundOptions.Flags = DependencyPropertyFlags::AffectsRender;
		backgroundOptions.Equals = brushEquals;
		backgroundOptions.Convert = convertBrush;
		backgroundOptions.Design = PropertyDesign(
			L"Appearance", 200, 10, DependencyPropertyPersistence::Metadata,
			DependencyPropertyEditorKind::Text, L"Background");
		backgroundOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<Control, cui::drawing::Brush>(
			L"Background",
			[](Control& target)
			{
				return target.GetComputedBackgroundBrush();
			},
			[](Control& target, const cui::drawing::Brush& value)
			{ target.ApplyBackgroundBrush(value); }, {}, std::move(backgroundOptions));

		DependencyPropertyOptions<Control, cui::drawing::Brush> foregroundOptions;
		foregroundOptions.DefaultValue = cui::drawing::NoBrush();
		foregroundOptions.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsRender;
		foregroundOptions.Equals = brushEquals;
		foregroundOptions.Convert = convertBrush;
		foregroundOptions.Design = PropertyDesign(
			L"Appearance", 200, 21, DependencyPropertyPersistence::Metadata,
			DependencyPropertyEditorKind::Text, L"Foreground");
		// Object editors are handled by XAML/Style resources in this batch.
		foregroundOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<Control, cui::drawing::Brush>(L"Foreground",
			[](Control& target)
			{
				return target.GetComputedForegroundBrush();
			},
			[](Control& target, const cui::drawing::Brush& value)
			{
				target.ApplyForegroundBrush(value);
			}, {}, std::move(foregroundOptions));

		DependencyPropertyOptions<Control, cui::drawing::Geometry> clipOptions;
		clipOptions.DefaultValue = cui::drawing::Geometry{};
		clipOptions.Flags = DependencyPropertyFlags::None;
		clipOptions.Equals = [](const cui::drawing::Geometry& left,
			const cui::drawing::Geometry& right) { return left == right; };
		clipOptions.Design = PropertyDesign(
			L"Appearance", 200, 27, DependencyPropertyPersistence::Metadata,
			DependencyPropertyEditorKind::Text, L"Clip geometry");
		clipOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<Control, cui::drawing::Geometry>(L"Clip",
			[](Control& target)
			{
				return target.GetClip().value_or(cui::drawing::Geometry{});
			},
			[](Control& target, const cui::drawing::Geometry& value)
			{
				if (value == cui::drawing::Geometry{}) target.ClearClip();
				else target.SetClip(value);
			}, {}, std::move(clipOptions));

		DependencyPropertyOptions<Control, cui::drawing::Transform> transformOptions;
		transformOptions.DefaultValue = cui::drawing::Transform{};
		transformOptions.Flags = DependencyPropertyFlags::None;
		transformOptions.Equals = [](const cui::drawing::Transform& left,
			const cui::drawing::Transform& right) { return left == right; };
		transformOptions.Design = PropertyDesign(
			L"Appearance", 200, 27, DependencyPropertyPersistence::Metadata,
			DependencyPropertyEditorKind::Text, L"Render transform");
		transformOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<Control, cui::drawing::Transform>(
			L"RenderTransform",
			[](Control& target)
			{
				return target.GetRenderTransform().value_or(cui::drawing::Transform{});
			},
			[](Control& target, const cui::drawing::Transform& value)
			{
				target.SetRenderTransform(value);
			}, {}, std::move(transformOptions));
		DependencyPropertyRegistry::Register<Control, cui::core::Point>(
			L"RenderTransformOrigin",
			[](Control& target) { return target.GetRenderTransformOriginDip(); },
			[](Control& target, const cui::core::Point& value)
			{ target.SetRenderTransformOriginDip(value); },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, cui::core::Point>{
				cui::core::Point{},
				DependencyPropertyFlags::None,
				[](Control&, const cui::core::Point& value)
					-> std::optional<cui::core::Point>
				{
					return std::isfinite(value.x) && std::isfinite(value.y)
						? std::optional<cui::core::Point>(value) : std::nullopt;
				}, {},
				[](const cui::core::Point& left, const cui::core::Point& right)
				{
					return left == right;
				} }, PropertyDesign(L"Appearance", 200, 28,
					DependencyPropertyPersistence::Metadata,
					DependencyPropertyEditorKind::Text, L"Transform origin")));
		DependencyPropertyOptions<Control, cui::drawing::Brush> borderBrushOptions;
		borderBrushOptions.DefaultValue = cui::drawing::NoBrush();
		borderBrushOptions.Flags = DependencyPropertyFlags::AffectsRender;
		borderBrushOptions.Equals = brushEquals;
		borderBrushOptions.Convert = convertBrush;
		borderBrushOptions.Design = PropertyDesign(
			L"Appearance", 200, 30, DependencyPropertyPersistence::Metadata,
			DependencyPropertyEditorKind::Text, L"BorderBrush");
		borderBrushOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<Control, cui::drawing::Brush>(
			L"BorderBrush",
			[](Control& target)
			{
				return target.GetComputedBorderBrush();
			},
			[](Control& target, const cui::drawing::Brush& value)
			{ target.ApplyBorderBrush(value); }, {}, std::move(borderBrushOptions));
		DependencyPropertyRegistry::Register<Control, Thickness>(L"BorderThickness",
			[](Control& target) { return target.BorderThickness; },
			[](Control& target, const Thickness& value)
			{ target.BorderThickness = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, Thickness>{
				Thickness{},
				DependencyPropertyFlags::AffectsMeasure
					| DependencyPropertyFlags::AffectsArrange
					| DependencyPropertyFlags::AffectsRender,
				[](Control&, const Thickness& proposed)
					-> std::optional<Thickness>
				{
					const bool valid = std::isfinite(proposed.Left)
						&& proposed.Left >= 0.0f
						&& std::isfinite(proposed.Top)
						&& proposed.Top >= 0.0f
						&& std::isfinite(proposed.Right)
						&& proposed.Right >= 0.0f
						&& std::isfinite(proposed.Bottom)
						&& proposed.Bottom >= 0.0f;
					return valid
						? std::optional<Thickness>{ proposed } : std::nullopt;
				} }, PropertyDesign(L"Appearance", 200, 40,
					DependencyPropertyPersistence::Metadata,
					DependencyPropertyEditorKind::Thickness)));
		auto validationSubscriber = [](
			Control& target, Handler handler, DataSourceUpdateMode)
		{
			return target.OnValidationStateChanged.Subscribe(
				[handler = std::move(handler)](
					const BindingValidationChangedEventArgs&)
				{ handler(); });
		};
		DependencyPropertyOptions<Control, bool> hasErrorOptions;
		hasErrorOptions.DefaultValue = false;
		hasErrorOptions.IsReadOnly = true;
		hasErrorOptions.Design = PropertyDesign(L"Validation", 400, 10,
			DependencyPropertyPersistence::Transient,
			DependencyPropertyEditorKind::Boolean, L"Has validation error");
		hasErrorOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<Control, bool>(
			L"Validation.HasError",
			[](Control& target) { return target._validationHasError; },
			[](Control& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"Validation.HasError",
					target._validationHasError, value);
			},
			validationSubscriber, std::move(hasErrorOptions));

		DependencyPropertyOptions<Control,
			std::vector<BindingValidationResult>> errorsOptions;
		errorsOptions.DefaultValue = std::vector<BindingValidationResult>{};
		errorsOptions.IsReadOnly = true;
		errorsOptions.Equals = [](
			const std::vector<BindingValidationResult>& left,
			const std::vector<BindingValidationResult>& right)
		{
			return left == right;
		};
		errorsOptions.Design = PropertyDesign(L"Validation", 400, 20,
			DependencyPropertyPersistence::Transient,
			DependencyPropertyEditorKind::Text, L"Validation errors");
		errorsOptions.Design.Browsable = false;
		DependencyPropertyRegistry::Register<Control,
			std::vector<BindingValidationResult>>(L"Validation.Errors",
			[](Control& target) { return target._validationErrors; },
			[](Control& target,
				const std::vector<BindingValidationResult>& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"Validation.Errors", target._validationErrors, value);
			},
			validationSubscriber, std::move(errorsOptions));
		DependencyPropertyOptions<Control, BindingValue> tagOptions;
		tagOptions.DefaultValue = BindingValue{};
		tagOptions.Flags = DependencyPropertyFlags::None;
		tagOptions.Design = PropertyDesign(
			L"Data", 250, 20,
			DependencyPropertyPersistence::Metadata,
			DependencyPropertyEditorKind::Text);
		DependencyPropertyRegistry::Register<Control, BindingValue>(L"Tag",
			[](Control& target) { return target.Tag; },
			[](Control& target, const BindingValue& value) { target.Tag = value; },
			{}, std::move(tagOptions));
		DependencyPropertyOptions<Control, CursorKind> cursorOptions;
		cursorOptions.DefaultValue = CursorKind::Auto;
		cursorOptions.Flags = DependencyPropertyFlags::Inherits;
		cursorOptions.Coerce = [](
			Control&, const CursorKind& proposed) -> std::optional<CursorKind>
		{
			switch (proposed)
			{
			case CursorKind::Auto:
			case CursorKind::Arrow:
			case CursorKind::Cross:
			case CursorKind::Hand:
			case CursorKind::IBeam:
			case CursorKind::SizeWE:
			case CursorKind::SizeNS:
			case CursorKind::SizeNWSE:
			case CursorKind::SizeNESW:
			case CursorKind::SizeAll:
			case CursorKind::No:
				return proposed;
			}
			return std::nullopt;
		};
		cursorOptions.Design = PropertyDesign(
			L"Behavior", 300, 10,
			DependencyPropertyPersistence::Metadata,
			DependencyPropertyEditorKind::Choice);
		cursorOptions.Design.Choices = {
			PropertyChoice(L"Auto", CursorKind::Auto),
			PropertyChoice(L"Arrow", CursorKind::Arrow),
			PropertyChoice(L"Cross", CursorKind::Cross),
			PropertyChoice(L"Hand", CursorKind::Hand),
			PropertyChoice(L"IBeam", CursorKind::IBeam),
			PropertyChoice(L"SizeWE", CursorKind::SizeWE),
			PropertyChoice(L"SizeNS", CursorKind::SizeNS),
			PropertyChoice(L"SizeNWSE", CursorKind::SizeNWSE),
			PropertyChoice(L"SizeNESW", CursorKind::SizeNESW),
			PropertyChoice(L"SizeAll", CursorKind::SizeAll),
			PropertyChoice(L"No", CursorKind::No)
		};
		DependencyPropertyRegistry::Register<Control, CursorKind>(L"Cursor",
			[](Control& target) { return target.Cursor; },
			[](Control& target, const CursorKind& value) { target.Cursor = value; },
			{}, std::move(cursorOptions));
		DependencyPropertyRegistry::Register<Control, bool>(L"Focusable",
			[](Control& target) { return target.Focusable; },
			[](Control& target, const bool& value) { target.Focusable = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, bool>{ false },
				PropertyDesign(L"Focus", 310, 0,
					DependencyPropertyPersistence::Metadata,
					DependencyPropertyEditorKind::Boolean)));
		DependencyPropertyRegistry::Register<Control, bool>(L"IsTabStop",
			[](Control& target) { return target.IsTabStop; },
			[](Control& target, const bool& value) { target.IsTabStop = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, bool>{ true },
				PropertyDesign(L"Behavior", 300, 20,
					DependencyPropertyPersistence::Metadata,
					DependencyPropertyEditorKind::Boolean)));
		DependencyPropertyRegistry::Register<Control, int>(L"TabIndex",
			[](Control& target) { return target.TabIndex; },
			[](Control& target, const int& value) { target.TabIndex = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, int>{
				0, DependencyPropertyFlags::None,
				[](Control&, const int& proposed) -> std::optional<int>
				{
					return (std::max)(0, proposed);
				} }, PropertyDesign(L"Behavior", 300, 30,
					DependencyPropertyPersistence::Metadata,
					DependencyPropertyEditorKind::Number)));
		auto focusStateOptions = [](int order)
		{
			DependencyPropertyOptions<Control, bool> options;
			options.DefaultValue = false;
			options.Flags = DependencyPropertyFlags::AffectsRender;
			options.IsReadOnly = true;
			options.Design = PropertyDesign(L"State", 70, order,
				DependencyPropertyPersistence::Transient,
				DependencyPropertyEditorKind::Boolean);
			options.Design.Browsable = false;
			return options;
		};
		auto focusStateSubscriber = [](std::wstring propertyName)
		{
			return [propertyName = std::move(propertyName)](
				Control& target, Handler handler, DataSourceUpdateMode)
			{
				return target.OnPropertyValueChanged.Subscribe(
					[propertyName, handler = std::move(handler)](
						DependencyObject*,
						const DependencyPropertyChangedEventArgs& args)
					{
						if (args.PropertyName == propertyName) handler();
					});
			};
		};
		DependencyPropertyRegistry::Register<Control, bool>(L"IsFocused",
			[](Control& target) { return target.IsFocused; },
			[](Control& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"IsFocused", target._isFocused, value);
			},
			focusStateSubscriber(L"IsFocused"),
			focusStateOptions(10));
		DependencyPropertyRegistry::Register<Control, bool>(
			L"IsKeyboardFocused",
			[](Control& target) { return target.IsKeyboardFocused; },
			[](Control& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"IsKeyboardFocused", target._isKeyboardFocused, value);
			},
			focusStateSubscriber(L"IsKeyboardFocused"),
			focusStateOptions(20));
		DependencyPropertyRegistry::Register<Control, bool>(
			L"IsKeyboardFocusWithin",
			[](Control& target) { return target.IsKeyboardFocusWithin; },
			[](Control& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"IsKeyboardFocusWithin",
					target._isKeyboardFocusWithin, value);
			},
			focusStateSubscriber(L"IsKeyboardFocusWithin"),
			focusStateOptions(30));
		DependencyPropertyRegistry::Register<Control, bool>(L"IsMouseOver",
			[](Control& target) { return target.IsMouseOver; },
			[](Control& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"IsMouseOver", target._isMouseOver, value);
			},
			focusStateSubscriber(L"IsMouseOver"),
			focusStateOptions(40));
		DependencyPropertyRegistry::Register<Control, bool>(
			L"IsMouseDirectlyOver",
			[](Control& target) { return target.IsMouseDirectlyOver; },
			[](Control& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"IsMouseDirectlyOver",
					target._isMouseDirectlyOver, value);
			},
			focusStateSubscriber(L"IsMouseDirectlyOver"),
			focusStateOptions(50));
		DependencyPropertyRegistry::Register<Control, bool>(
			L"FocusManager.IsFocusScope",
			[](Control& target) { return target.IsFocusScope; },
			[](Control& target, const bool& value) { target.IsFocusScope = value; },
			{},
			WithPropertyDesign(DependencyPropertyOptions<Control, bool>{ false },
				PropertyDesign(L"Focus", 310, 10,
					DependencyPropertyPersistence::Metadata,
					DependencyPropertyEditorKind::Boolean,
					L"IsFocusScope")));
		auto navigationOptions = [](int order, std::wstring displayName)
		{
			DependencyPropertyOptions<Control, KeyboardNavigationMode> options;
			options.DefaultValue = KeyboardNavigationMode::Continue;
			options.Design = PropertyDesign(L"Focus", 310, order,
				DependencyPropertyPersistence::Metadata,
				DependencyPropertyEditorKind::Choice,
				std::move(displayName));
			options.Design.Choices = {
				{ L"Continue", BindingValue(KeyboardNavigationMode::Continue) },
				{ L"Once", BindingValue(KeyboardNavigationMode::Once) },
				{ L"Cycle", BindingValue(KeyboardNavigationMode::Cycle) },
				{ L"None", BindingValue(KeyboardNavigationMode::None) },
				{ L"Contained", BindingValue(KeyboardNavigationMode::Contained) },
				{ L"Local", BindingValue(KeyboardNavigationMode::Local) },
			};
			return options;
		};
		DependencyPropertyRegistry::Register<Control, KeyboardNavigationMode>(
			L"KeyboardNavigation.TabNavigation",
			[](Control& target) { return target.TabNavigation; },
			[](Control& target, const KeyboardNavigationMode& value)
			{ target.TabNavigation = value; }, {},
			navigationOptions(20, L"TabNavigation"));
		DependencyPropertyRegistry::Register<Control, KeyboardNavigationMode>(
			L"KeyboardNavigation.DirectionalNavigation",
			[](Control& target) { return target.DirectionalNavigation; },
			[](Control& target, const KeyboardNavigationMode& value)
			{ target.DirectionalNavigation = value; }, {},
			navigationOptions(30, L"DirectionalNavigation"));
		DependencyPropertyRegistry::Register<Control, std::wstring>(
			L"AutomationProperties.Name",
			[](Control& target) { return target.AutomationName; },
			[](Control& target, const std::wstring& value) { target.AutomationName = value; },
			{},
			WithPropertyDesign(
				DependencyPropertyOptions<Control, std::wstring>{ std::wstring{} },
				PropertyDesign(L"Automation", 500, 20,
					DependencyPropertyPersistence::Metadata, DependencyPropertyEditorKind::Text)));
		DependencyPropertyRegistry::Register<Control, std::wstring>(
			L"AutomationProperties.FullDescription",
			[](Control& target) { return target.AutomationFullDescription; },
			[](Control& target, const std::wstring& value) { target.AutomationFullDescription = value; },
			{},
			WithPropertyDesign(
				DependencyPropertyOptions<Control, std::wstring>{ std::wstring{} },
				PropertyDesign(L"Automation", 500, 30,
					DependencyPropertyPersistence::Metadata, DependencyPropertyEditorKind::Text)));
		DependencyPropertyRegistry::Register<Control, std::wstring>(
			L"AutomationProperties.HelpText",
			[](Control& target) { return target.AutomationHelpText; },
			[](Control& target, const std::wstring& value) { target.AutomationHelpText = value; },
			{},
			WithPropertyDesign(
				DependencyPropertyOptions<Control, std::wstring>{ std::wstring{} },
				PropertyDesign(L"Automation", 500, 40,
					DependencyPropertyPersistence::Metadata, DependencyPropertyEditorKind::Text)));
		DependencyPropertyRegistry::Register<Control, std::wstring>(
			L"AutomationProperties.AutomationId",
			[](Control& target) { return target.AutomationId; },
			[](Control& target, const std::wstring& value) { target.AutomationId = value; },
			{},
			WithPropertyDesign(
				DependencyPropertyOptions<Control, std::wstring>{ std::wstring{} },
				PropertyDesign(L"Automation", 500, 50,
					DependencyPropertyPersistence::Metadata, DependencyPropertyEditorKind::Text)));
	});
}

Control* Control::GetVisualChild(int index) const noexcept
{
	if (index < 0 || static_cast<size_t>(index) >= _visualChildren.size())
		return nullptr;
	return _visualChildren[static_cast<size_t>(index)];
}

Control* Control::FindControlByDesignId(int designId) noexcept
{
	return const_cast<Control*>(
		static_cast<const Control*>(this)->FindControlByDesignId(designId));
}

const Control* Control::FindControlByDesignId(int designId) const noexcept
{
	if (designId <= 0) return nullptr;
	if (GetDesignId() == designId) return this;
	for (const auto* child : _visualChildren)
	{
		if (!child) continue;
		if (const auto* match = child->FindControlByDesignId(designId))
			return match;
	}
	return nullptr;
}

std::vector<Control*> Control::GetVisualChildrenInZOrder() const
{
	std::vector<Control*> result(
		this->_visualChildren.begin(), this->_visualChildren.end());
	std::stable_sort(result.begin(), result.end(), [](Control* left, Control* right)
		{
			if (!left || !right) return left != nullptr;
			return left->ZIndex < right->ZIndex;
		});
	return result;
}

std::vector<Control*> Control::GetVisualChildrenInReverseZOrder() const
{
	auto result = GetVisualChildrenInZOrder();
	std::reverse(result.begin(), result.end());
	return result;
}

bool Control::MoveVisualChild(int oldIndex, int newIndex)
{
	if (oldIndex < 0 || newIndex < 0)
		return false;
	const auto oldPosition = static_cast<size_t>(oldIndex);
	const auto newPosition = static_cast<size_t>(newIndex);
	if (oldPosition >= _visualChildren.size()
		|| newPosition >= _visualChildren.size())
		return false;
	return _visualChildren.Move(oldPosition, newPosition);
}

std::unique_ptr<Control> Control::DetachVisualChild(Control* child)
{
	if (!child)
		return {};
	auto position = std::find(
		this->_visualChildren.begin(), this->_visualChildren.end(), child);
	if (position == this->_visualChildren.end())
		return {};

	this->_visualChildren.erase(position);
	return std::unique_ptr<Control>(child);
}

std::unique_ptr<Control> Control::DetachVisualChildAt(int index)
{
	if (index < 0 || static_cast<size_t>(index) >= _visualChildren.size())
		return {};
	return DetachVisualChild(_visualChildren[static_cast<size_t>(index)]);
}

bool Control::DeleteVisualChild(Control* child)
{
	auto detached = DetachVisualChild(child);
	return detached != nullptr;
}

bool Control::DeleteVisualChildAt(int index)
{
	auto detached = DetachVisualChildAt(index);
	return detached != nullptr;
}

void Control::ClearVisualChildren()
{
	if (_visualChildren.empty()) return;
	std::vector<Control*> removed(
		_visualChildren.begin(), _visualChildren.end());
	_visualChildren.clear();
	for (auto* child : removed) delete child;
}

int Control::IndexOfVisualChild(const Control* child) const noexcept
{
	if (!child) return -1;
	auto found = std::find(
		_visualChildren.begin(), _visualChildren.end(), child);
	return found == _visualChildren.end()
		? -1 : static_cast<int>(found - _visualChildren.begin());
}
GET_CPP(Control, bool, IsEnabled)
{
	return IsEffectivelyEnabled();
}
SET_CPP(Control, bool, IsEnabled)
{
	// The CLR-shaped wrapper is SetValue(IsEnabledProperty), not a second
	// native state mutation path. Command and ancestor constraints are folded
	// into the effective projection separately.
	(void)TrySetPropertyValue(
		L"IsEnabled", BindingValue(value),
		DependencyPropertyValueSource::Local);
}
GET_CPP(Control, bool, AllowDrop)
{
	return _allowDrop;
}
SET_CPP(Control, bool, AllowDrop)
{
	(void)SetPropertyField(L"AllowDrop", _allowDrop, value);
}
GET_CPP(Control, ::Visibility, Visibility)
{
	return _visibility;
}
SET_CPP(Control, ::Visibility, Visibility)
{
	VerifyAccess();
	if (value != ::Visibility::Visible
		&& value != ::Visibility::Hidden
		&& value != ::Visibility::Collapsed)
		throw std::invalid_argument("Visibility value is invalid");
	auto* metadata = DependencyPropertyRegistry::Find(*this, L"Visibility");
	if (!metadata) return;
	if (_applyingPropertyMetadata != metadata)
	{
		(void)TrySetPropertyValue(
			L"Visibility",
			BindingValue(std::wstring(VisibilityName(value))),
			DependencyPropertyValueSource::Local);
		return;
	}
	if (_visibility == value) return;
	auto snapshot = CaptureEffectiveIsVisibleSubtree(*this);
	const bool collapsedChanged = (_visibility == ::Visibility::Collapsed)
		!= (value == ::Visibility::Collapsed);
	_visibility = value;
	if (collapsedChanged) RequestLayout();
	else InvalidateVisual();
	PublishEffectiveIsVisibleChanges(std::move(snapshot));

	if (this->GetPresentationWindow())
	{
		this->GetPresentationWindow()->InvalidatePresentationStructure();
		this->GetPresentationWindow()->Invalidate(false);
		this->GetPresentationWindow()->NotifyAccessibilityEvent(
			this, AccessibilityChange::State);
	}
}

void Control::SetPresentationSuppressed(bool value)
{
	VerifyAccess();
	if (_presentationSuppressed == value) return;
	auto snapshot = CaptureEffectiveIsVisibleSubtree(*this);
	_presentationSuppressed = value;
	RequestLayout();
	PublishEffectiveIsVisibleChanges(std::move(snapshot));
	if (GetPresentationWindow())
	{
		GetPresentationWindow()->InvalidatePresentationStructure();
		GetPresentationWindow()->Invalidate(false);
		GetPresentationWindow()->NotifyAccessibilityEvent(
			this, AccessibilityChange::State);
	}
}

void Control::SetParticipatesInPresentationScene(bool value)
{
	VerifyAccess();
	if (_participatesInPresentationScene == value) return;
	_participatesInPresentationScene = value;
	if (GetPresentationWindow())
	{
		GetPresentationWindow()->InvalidatePresentationStructure();
		GetPresentationWindow()->Invalidate(false);
	}
}

GET_CPP(Visual, int, ZIndex)
{
	return _zIndex;
}
SET_CPP(Visual, int, ZIndex)
{
	if (auto* control = dynamic_cast<Control*>(this);
		control && control->RouteVisualZIndexSet(value))
		return;
	if (_zIndex == value) return;
	_zIndex = value;
	if (GetPresentationWindow())
	{
		GetPresentationWindow()->InvalidatePresentationStructure();
		GetPresentationWindow()->Invalidate(false);
	}
}

bool Control::RouteVisualZIndexSet(int value)
{
	auto* metadata = DependencyPropertyRegistry::Find(*this, L"ZIndex");
	if (!metadata || _applyingPropertyMetadata == metadata) return false;
	// The Visual backing field is written only by the metadata application
	// re-entry above. A failed conversion/coercion must not fall through and
	// mutate it outside the dependency-property store.
	(void)TrySetPropertyValue(
		L"ZIndex", BindingValue(value),
		DependencyPropertyValueSource::Local);
	return true;
}

GET_CPP(Control, float, CanvasLeft)
{
	return _canvasLeft;
}
SET_CPP(Control, float, CanvasLeft)
{
	(void)SetPropertyField(L"Canvas.Left", _canvasLeft, value);
}
GET_CPP(Control, float, CanvasTop)
{
	return _canvasTop;
}
SET_CPP(Control, float, CanvasTop)
{
	(void)SetPropertyField(L"Canvas.Top", _canvasTop, value);
}
GET_CPP(Control, float, CanvasRight)
{
	return _canvasRight;
}
SET_CPP(Control, float, CanvasRight)
{
	(void)SetPropertyField(L"Canvas.Right", _canvasRight, value);
}
GET_CPP(Control, float, CanvasBottom)
{
	return _canvasBottom;
}
SET_CPP(Control, float, CanvasBottom)
{
	(void)SetPropertyField(L"Canvas.Bottom", _canvasBottom, value);
}
GET_CPP(Control, cui::layout::Length, Width)
{
	return _layoutStyle.width;
}
SET_CPP(Control, cui::layout::Length, Width)
{
	value = value.IsFixed()
		? cui::layout::Length::Fixed(value.value)
		: cui::layout::Length::Auto();
	(void)SetPropertyField(L"Width", _layoutStyle.width, value);
}
GET_CPP(Control, cui::layout::Length, Height)
{
	return _layoutStyle.height;
}
SET_CPP(Control, cui::layout::Length, Height)
{
	value = value.IsFixed()
		? cui::layout::Length::Fixed(value.value)
		: cui::layout::Length::Auto();
	(void)SetPropertyField(L"Height", _layoutStyle.height, value);
}
GET_CPP(Control, float, MinWidth)
{
	return _layoutStyle.minimumSize.width;
}
SET_CPP(Control, float, MinWidth)
{
	(void)SetPropertyField(L"MinWidth", _layoutStyle.minimumSize.width, value);
}
GET_CPP(Control, float, MinHeight)
{
	return _layoutStyle.minimumSize.height;
}
SET_CPP(Control, float, MinHeight)
{
	(void)SetPropertyField(L"MinHeight", _layoutStyle.minimumSize.height, value);
}
GET_CPP(Control, float, MaxWidth)
{
	return _layoutStyle.maximumSize.width;
}
SET_CPP(Control, float, MaxWidth)
{
	(void)SetPropertyField(L"MaxWidth", _layoutStyle.maximumSize.width, value);
}
GET_CPP(Control, float, MaxHeight)
{
	return _layoutStyle.maximumSize.height;
}
SET_CPP(Control, float, MaxHeight)
{
	(void)SetPropertyField(L"MaxHeight", _layoutStyle.maximumSize.height, value);
}
GET_CPP(Control, float, ActualWidth)
{
	return GetActualSizeDip().width;
}
GET_CPP(Control, float, ActualHeight)
{
	return GetActualSizeDip().height;
}
GET_CPP(Control, std::wstring, Text)
{
	return _text;
}
SET_CPP(Control, std::wstring, Text)
{
	const auto previous = _text;
	const auto* metadata = DependencyPropertyRegistry::Find(*this, L"Text");
	// Text is owned only by the concrete WPF text-bearing types which register
	// it.  Control's private behavior backing must not revive the old universal
	// WinForms-style Text property on structural or content controls.
	if (!metadata) return;
	const bool applyingMetadata = metadata && _applyingPropertyMetadata == metadata;
	(void)SetPropertyField(L"Text", _text, std::move(value));
	// A public wrapper re-enters through the registered metadata setter.  Only
	// that guarded application frame publishes native accessibility effects;
	// the outer SetValue frame must not report the same change twice.
	if (!applyingMetadata) return;

	const ControlWeakReference selfReference(this);
	auto* live = selfReference.Get();
	if (!live || previous == live->_text) return;
	if (auto* window = live->GetPresentationWindow())
	{
		window->NotifyAccessibilityEvent(live, AccessibilityChange::Name);
		window->NotifyAccessibilityEvent(live, AccessibilityChange::Value);
	}
}
GET_CPP(Control, D2D1_COLOR_F, RendererBorderColor)
{
	if (_borderBrush
		&& _borderBrush->Kind == cui::drawing::BrushKind::Solid)
	{
		auto color = _borderBrush->Color;
		color.a *= (std::clamp)(_borderBrush->Opacity, 0.0f, 1.0f);
		return color;
	}
	return _bordercolor;
}
SET_CPP(Control, D2D1_COLOR_F, RendererBorderColor)
{
	if (_bordercolor.r == value.r && _bordercolor.g == value.g
		&& _bordercolor.b == value.b && _bordercolor.a == value.a) return;
	_bordercolor = value;
	InvalidateVisual();
}
GET_CPP(Control, Thickness, BorderThickness)
{
	return _borderThickness;
}
SET_CPP(Control, Thickness, BorderThickness)
{
	(void)SetPropertyField(L"BorderThickness", _borderThickness, value);
}
GET_CPP(Control, D2D1_COLOR_F, RendererBackgroundColor)
{
	if (_backgroundBrush
		&& _backgroundBrush->Kind == cui::drawing::BrushKind::Solid)
	{
		auto color = _backgroundBrush->Color;
		color.a *= (std::clamp)(_backgroundBrush->Opacity, 0.0f, 1.0f);
		return color;
	}
	return GetPresentationWindow()
		? cui::framework::WindowAccess::EffectiveControlBackColor(
			*GetPresentationWindow(), _backcolor) : _backcolor;
}
SET_CPP(Control, D2D1_COLOR_F, RendererBackgroundColor)
{
	if (_backcolor.r == value.r && _backcolor.g == value.g
		&& _backcolor.b == value.b && _backcolor.a == value.a) return;
	_backcolor = value;
	InvalidateVisual();
}
GET_CPP(Control, D2D1_COLOR_F, RendererForegroundColor)
{
	if (_foregroundBrush
		&& _foregroundBrush->Kind == cui::drawing::BrushKind::Solid)
	{
		auto color = _foregroundBrush->Color;
		color.a *= (std::clamp)(_foregroundBrush->Opacity, 0.0f, 1.0f);
		return color;
	}
	return GetPresentationWindow()
		? cui::framework::WindowAccess::EffectiveControlForeColor(
			*GetPresentationWindow(), _forecolor) : _forecolor;
}
SET_CPP(Control, D2D1_COLOR_F, RendererForegroundColor)
{
	if (_forecolor.r == value.r && _forecolor.g == value.g
		&& _forecolor.b == value.b && _forecolor.a == value.a) return;
	_forecolor = value;
	InvalidateVisual();
}
bool Control::DispatchInput(const InputReport& input)
{
	const ControlWeakReference selfReference(this);
	const bool previousDispatch = _dispatchingComponentBehaviorInput;
	_dispatchingComponentBehaviorInput = true;
	if (!previousDispatch && _declarativeComponentBehavior)
	{
		bool handled = false;
		try
		{
			handled = _declarativeComponentBehavior->HandleInput(*this, input);
		}
		catch (...)
		{
		}
		auto* live = selfReference.Get();
		if (!live) return true;
		if (handled)
		{
			live->_dispatchingComponentBehaviorInput = previousDispatch;
			return true;
		}
	}
	try
	{
		auto* live = selfReference.Get();
		if (!live) return true;
		const bool result = live->ProcessInput(input);
		live = selfReference.Get();
		if (live)
			live->_dispatchingComponentBehaviorInput = previousDispatch;
		return result;
	}
	catch (...)
	{
		if (auto* live = selfReference.Get())
			live->_dispatchingComponentBehaviorInput = previousDispatch;
		throw;
	}
}

bool Control::DispatchTextInput(TextCompositionEventArgs& input)
{
	const ControlWeakReference selfReference(this);
	if (!IsEffectivelyEnabled() || !this->IsVisible || input.Text.empty()) return false;
	if (_declarativeComponentBehavior)
	{
		try
		{
			if (_declarativeComponentBehavior->HandleTextInput(*this, input))
				return true;
		}
		catch (...)
		{
		}
	}
	auto* live = selfReference.Get();
	return live ? live->ApplyTextInput(input) : true;
}

bool Control::ResolveTextInputCaretRect(D2D1_RECT_F& outRect)
{
	const ControlWeakReference selfReference(this);
	if (_declarativeComponentBehavior)
	{
		try
		{
			if (_declarativeComponentBehavior->TryGetTextInputCaretRect(
				*this, outRect)) return true;
		}
		catch (...)
		{
		}
	}
	auto* live = selfReference.Get();
	return live && live->TryGetTextInputCaretRect(outRect);
}

bool Control::ProcessInput(const InputReport& input)
{
	const ControlWeakReference sourceReference(this);
	if (!_dispatchingComponentBehaviorInput
		&& _declarativeComponentBehavior)
		return DispatchInput(input);
	if (!IsEffectivelyEnabled() || !this->IsVisible) return true;
	auto resolve = [&]() noexcept { return sourceReference.Get(); };
	switch (input.Kind)
	{
	case InputReportKind::MouseWheel:
	case InputReportKind::HorizontalMouseWheel:
	{
		auto eventArgs = input.CreateMouseEventArgs();
		if (auto* source = resolve())
			source->OnMouseWheel(source, eventArgs);
	}
	break;
	case InputReportKind::PointerMove:
	{
		auto eventArgs = input.CreateMouseEventArgs();
		auto* source = resolve();
		if (!source) return true;
		source->BeforeDefaultMouseMove(eventArgs);
		source = resolve();
		if (!source) return true;
		source->OnMouseMove(source, eventArgs);
	}
	break;
	case InputReportKind::PointerDown:
	{
		auto* source = resolve();
		if (!source) return true;
		if (input.ChangedButton == MouseButton::Left
			&& source->DefaultRaiseClickOnLeftButtonUp())
		{
			source->_defaultLeftButtonPressActive = true;
			(void)source->CaptureMouse();
		}
		source = resolve();
		if (!source) return true;
		const bool selectOnDown = input.ChangedButton == MouseButton::Left
			&& source->DefaultSelectOnLeftButtonDown();
		source = resolve();
		if (!source) return true;
		if (selectOnDown && source->GetPresentationWindow())
		{
			const ControlWeakReference windowReference(source->GetPresentationWindow());
			if (auto* window = dynamic_cast<Window*>(windowReference.Get()))
				window->SetKeyboardFocus(source, false);
		}
		auto eventArgs = input.CreateMouseEventArgs();
		source = resolve();
		if (!source) return true;
		source->BeforeDefaultMouseDown(input.ChangedButton, eventArgs);
		source = resolve();
		if (!source) return true;
		source->OnMouseDown(source, eventArgs);
		source = resolve();
		if (!source) return true;
		const bool invalidate = source->DefaultInvalidateVisualOnMouseDown(
			input.ChangedButton);
		source = resolve();
		if (source && invalidate) source->InvalidateVisual();
	}
	break;
	case InputReportKind::PointerUp:
	{
		auto* source = resolve();
		if (!source) return true;
		const bool hasMatchingPress = input.ChangedButton != MouseButton::Left
			|| source->_defaultLeftButtonPressActive;
		if (input.ChangedButton == MouseButton::Left)
			source->_defaultLeftButtonPressActive = false;
		auto eventArgs = input.CreateMouseEventArgs();
		source->BeforeDefaultMouseUp(
			input.ChangedButton, eventArgs, hasMatchingPress);
		source = resolve();
		if (!source) return true;
		const bool raiseClick = input.ChangedButton == MouseButton::Left
			&& hasMatchingPress
			&& source->DefaultRaiseClickOnLeftButtonUp();
		source = resolve();
		if (!source) return true;
		if (raiseClick)
		{
			source->BeforeDefaultClick(input.ChangedButton, eventArgs);
			source = resolve();
			if (!source) return true;
			RoutedEventArgs clickArgs;
			source->Click(source, clickArgs);
			source = resolve();
			if (!source) return true;
			source->AfterDefaultClick(input.ChangedButton, eventArgs);
			source = resolve();
			if (!source) return true;
		}
		source->OnMouseUp(source, eventArgs);
		source = resolve();
		if (!source) return true;
		const bool invalidate = source->DefaultInvalidateVisualOnMouseUp(
			input.ChangedButton);
		source = resolve();
		if (source && invalidate) source->InvalidateVisual();
		source = resolve();
		if (source && input.ChangedButton == MouseButton::Left
			&& source->IsMouseCaptured())
			(void)source->ReleaseMouseCapture();
	}
	break;
	case InputReportKind::PointerDoubleClick:
	{
		auto* source = resolve();
		if (!source) return true;
		if (input.ChangedButton == MouseButton::Left
			&& source->DefaultRaiseClickOnLeftButtonUp())
		{
			source->_defaultLeftButtonPressActive = true;
			(void)source->CaptureMouse();
		}
		source = resolve();
		if (!source) return true;
		const bool selectOnDoubleClick = input.ChangedButton == MouseButton::Left
			&& source->DefaultSelectOnLeftButtonDown();
		source = resolve();
		if (!source) return true;
		if (selectOnDoubleClick && source->GetPresentationWindow())
		{
			const ControlWeakReference windowReference(source->GetPresentationWindow());
			if (auto* window = dynamic_cast<Window*>(windowReference.Get()))
				window->SetKeyboardFocus(source, false);
		}
		auto eventArgs = input.CreateMouseEventArgs();
		source = resolve();
		if (!source) return true;
		source->BeforeDefaultMouseDown(input.ChangedButton, eventArgs);
		source = resolve();
		if (!source) return true;
		source->OnMouseDoubleClick(source, eventArgs);
		source = resolve();
		if (source && source->DefaultInvalidateVisualOnMouseDown(
			input.ChangedButton)) source->InvalidateVisual();
	}
	break;
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
		if (auto* source = resolve())
		{
			source->_defaultLeftButtonPressActive = false;
			source->SetStyleState(ControlStyleState::Pressed, false);
			if (input.Kind == InputReportKind::Cancel
				&& source->IsMouseCaptured())
				(void)source->ReleaseMouseCapture();
		}
		break;
	case InputReportKind::KeyDown:
	{
		auto eventArgs = input.CreateKeyEventArgs();
		if (auto* source = resolve())
			source->OnKeyDown(source, eventArgs);
	}
	break;
	case InputReportKind::KeyUp:
	{
		auto eventArgs = input.CreateKeyEventArgs();
		if (auto* source = resolve())
			source->OnKeyUp(source, eventArgs);
	}
	break;
	}
	return true;
}

// 布局属性实现
GET_CPP(Control, Thickness, Margin)
{
	return _margin;
}
SET_CPP(Control, Thickness, Margin)
{
	auto* metadata = DependencyPropertyRegistry::Find(*this, L"Margin");
	const bool applyingMetadata =
		metadata && _applyingPropertyMetadata == metadata;
	if (!SetPropertyField(L"Margin", _margin, value)
		|| (metadata && !applyingMetadata)) return;
	_layoutStyle.margin = cui::core::Insets{
		_margin.Left, _margin.Top, _margin.Right, _margin.Bottom };
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, Thickness, Padding)
{
	return _padding;
}
SET_CPP(Control, Thickness, Padding)
{
	auto* metadata = DependencyPropertyRegistry::Find(*this, L"Padding");
	const bool applyingMetadata =
		metadata && _applyingPropertyMetadata == metadata;
	if (!SetPropertyField(L"Padding", _padding, value)
		|| (metadata && !applyingMetadata)) return;
	_layoutStyle.padding = cui::core::Insets{
		_padding.Left, _padding.Top, _padding.Right, _padding.Bottom };
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, ::HorizontalAlignment, HorizontalAlignment)
{
	return _horizontalAlignment;
}
SET_CPP(Control, ::HorizontalAlignment, HorizontalAlignment)
{
	auto* metadata = DependencyPropertyRegistry::Find(
		*this, L"HorizontalAlignment");
	const bool applyingMetadata =
		metadata && _applyingPropertyMetadata == metadata;
	if (!SetPropertyField(
		L"HorizontalAlignment", _horizontalAlignment, value)
		|| (metadata && !applyingMetadata)) return;
	_layoutStyle.horizontalAlignment =
		ToLayoutAlignment(_horizontalAlignment);
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, ::VerticalAlignment, VerticalAlignment)
{
	return _verticalAlignment;
}
SET_CPP(Control, ::VerticalAlignment, VerticalAlignment)
{
	auto* metadata = DependencyPropertyRegistry::Find(
		*this, L"VerticalAlignment");
	const bool applyingMetadata =
		metadata && _applyingPropertyMetadata == metadata;
	if (!SetPropertyField(
		L"VerticalAlignment", _verticalAlignment, value)
		|| (metadata && !applyingMetadata)) return;
	_layoutStyle.verticalAlignment =
		ToLayoutAlignment(_verticalAlignment);
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, int, GridRow)
{
	return _gridRow;
}
SET_CPP(Control, int, GridRow)
{
	SetPropertyField(L"Grid.Row", _gridRow, value);
}

GET_CPP(Control, int, GridColumn)
{
	return _gridColumn;
}
SET_CPP(Control, int, GridColumn)
{
	SetPropertyField(L"Grid.Column", _gridColumn, value);
}

GET_CPP(Control, int, GridRowSpan)
{
	return _gridRowSpan;
}
SET_CPP(Control, int, GridRowSpan)
{
	SetPropertyField(L"Grid.RowSpan", _gridRowSpan, value);
}

GET_CPP(Control, int, GridColumnSpan)
{
	return _gridColumnSpan;
}
SET_CPP(Control, int, GridColumnSpan)
{
	SetPropertyField(L"Grid.ColumnSpan", _gridColumnSpan, value);
}

GET_CPP(Control, Dock, DockPosition)
{
	return _dock;
}
SET_CPP(Control, Dock, DockPosition)
{
	SetPropertyField(L"DockPanel.Dock", _dock, value);
}

cui::core::Size Control::GetMinSizeDip() const noexcept
{
	return _layoutStyle.minimumSize;
}

void Control::SetMinSizeDip(cui::core::Size value)
{
	MinWidth = value.width;
	MinHeight = value.height;
}

cui::core::Size Control::GetMaxSizeDip() const noexcept
{
	return _layoutStyle.maximumSize;
}

void Control::SetMaxSizeDip(cui::core::Size value)
{
	MaxWidth = value.width;
	MaxHeight = value.height;
}

cui::core::Size Control::MeasureCore(const cui::core::Constraints& available)
{
	(void)available;
	return _naturalSize.NonNegative();
}

cui::core::Size Control::ResolveDesiredSize(
	cui::core::Size intrinsicSize,
	const cui::core::Constraints& available) const
{
	intrinsicSize = intrinsicSize.NonNegative();
	if (_layoutStyle.width.IsFixed())
		intrinsicSize.width = _layoutStyle.width.value;
	if (_layoutStyle.height.IsFixed())
		intrinsicSize.height = _layoutStyle.height.value;

	const auto styleConstraints = _layoutStyle.SizeConstraints();
	const auto availableConstraints = available.Normalized();
	const cui::core::Size minimum{
		(std::max)(styleConstraints.minimum.width, availableConstraints.minimum.width),
		(std::max)(styleConstraints.minimum.height, availableConstraints.minimum.height) };
	const cui::core::Size maximum{
		(std::max)(minimum.width, (std::min)(styleConstraints.maximum.width, availableConstraints.maximum.width)),
		(std::max)(minimum.height, (std::min)(styleConstraints.maximum.height, availableConstraints.maximum.height)) };
	return cui::core::Constraints{ minimum, maximum }.Constrain(intrinsicSize);
}

cui::core::Size Control::Measure(const cui::core::Constraints& available)
{
	const cui::core::Constraints constraints = available.Normalized();
	if (IsCollapsed())
	{
		if (_layoutState.NeedsMeasure()
			|| _layoutState.lastMeasureConstraints != constraints
			|| _layoutState.desiredSize != cui::core::Size{})
			_layoutState.CommitMeasure({}, constraints);
		return {};
	}
	if (_layoutState.NeedsMeasure() ||
		_layoutState.lastMeasureConstraints != constraints)
	{
		const auto intrinsic = GetControlTemplateRoot()
			? GetControlTemplateRoot()->Measure(constraints)
			: MeasureCore(constraints);
		_layoutState.CommitMeasure(ResolveDesiredSize(intrinsic, constraints), constraints);
	}
	return _layoutState.desiredSize;
}


cui::core::Point Control::GetActualLocationDip() const
{
	if (_layoutState.hasArranged)
		return _layoutState.arrangedRect.Origin();
	return {};
}

cui::core::Size Control::GetActualSizeDip() const
{
	return _layoutState.hasArranged
		? _layoutState.arrangedRect.Extent().NonNegative()
		: cui::core::Size{};
}

cui::core::Size Control::GetRenderSizeDip()
{
	return GetActualSizeDip();
}

cui::core::Point Control::GetAbsoluteLocationDip() const
{
	const Control* ancestor = this;
	cui::core::Point absoluteLocation = ancestor->GetActualLocationDip();
	std::unordered_set<const Control*> visited;
	while (ancestor->_visualParent)
	{
		if (!visited.insert(ancestor).second) break;
		ancestor = ancestor->_visualParent;
		const auto ancestorLocation = ancestor->GetActualLocationDip();
		absoluteLocation += cui::core::Vector{
			ancestorLocation.x, ancestorLocation.y };
		const auto childOffset = ancestor->GetVisualChildrenRenderOffset();
		absoluteLocation += cui::core::Vector{
			(float)childOffset.x, (float)childOffset.y };
	}
	return absoluteLocation;
}

cui::core::Rect Control::GetAbsoluteRectDip() const
{
	return cui::core::Rect{
		GetAbsoluteLocationDip(), GetActualSizeDip() };
}

D2D1_RECT_F Control::GetAbsoluteBoundsDip() const
{
	const auto rect = GetAbsoluteRectDip();
	return D2D1::RectF(
		rect.Left(), rect.Top(), rect.Right(), rect.Bottom());
}

D2D1_MATRIX_3X2_F Control::GetInheritedRenderTransform() const
{
	auto result = D2D1::Matrix3x2F::Identity();
	std::unordered_set<const Control*> visited;
	visited.insert(this);
	for (auto* ancestor = this->_visualParent;
		ancestor && visited.insert(ancestor).second;
		ancestor = ancestor->_visualParent)
		result = result * AsMatrix(
			ancestor->GetEffectiveDescendantRenderTransform());
	return result;
}

D2D1_MATRIX_3X2_F Control::GetEffectiveDescendantRenderTransform() const
{
	auto result = D2D1::Matrix3x2F::Identity();
	if (_renderTransform)
	{
		const auto size = const_cast<Control*>(this)->GetActualSizeDip();
		const auto local = AsMatrix(_renderTransform->ToMatrix(
			D2D1::SizeF(size.width, size.height), _renderTransformOrigin));
		const auto absolute = GetAbsoluteLocationDip();
		result = D2D1::Matrix3x2F::Translation(-absolute.x, -absolute.y)
			* local
			* D2D1::Matrix3x2F::Translation(absolute.x, absolute.y);
	}
	D2D1_MATRIX_3X2_F extra{};
	if (TryGetDescendantRenderTransform(extra))
		result = result * AsMatrix(extra);
	return result;
}

D2D1_MATRIX_3X2_F Control::GetLocalToRenderTransform() const
{
	const auto size = const_cast<Control*>(this)->GetActualSizeDip();
	const auto local = _renderTransform
		? AsMatrix(_renderTransform->ToMatrix(
			D2D1::SizeF(size.width, size.height), _renderTransformOrigin))
		: D2D1::Matrix3x2F::Identity();
	const auto absolute = GetAbsoluteLocationDip();
	return local
		* D2D1::Matrix3x2F::Translation(absolute.x, absolute.y)
		* AsMatrix(GetInheritedRenderTransform());
}

bool Control::TryTransformRenderPointToLocal(
	D2D1_POINT_2F renderPoint,
	D2D1_POINT_2F& localPoint) const
{
	auto inverse = AsMatrix(GetLocalToRenderTransform());
	if (!inverse.Invert()) return false;
	localPoint = inverse.TransformPoint(renderPoint);
	return std::isfinite(localPoint.x) && std::isfinite(localPoint.y);
}

bool Control::IsRenderPointInsideClip(D2D1_POINT_2F renderPoint) const
{
	std::unordered_set<const Control*> visited;
	for (auto* current = this;
		current && visited.insert(current).second;
		current = current->_visualParent)
	{
		if (!current->_clip) continue;
		D2D1_POINT_2F local{};
		if (!current->TryTransformRenderPointToLocal(renderPoint, local)
			|| !current->_clip->ContainsPoint(local)) return false;
	}
	return true;
}

D2D1_RECT_F Control::TransformAbsoluteRectToRenderSpace(
	const D2D1_RECT_F& rect) const
{
	const auto absolute = GetAbsoluteLocationDip();
	const auto transform = D2D1::Matrix3x2F::Translation(
		-absolute.x, -absolute.y)
		* AsMatrix(GetLocalToRenderTransform());
	const D2D1_POINT_2F points[] = {
		transform.TransformPoint(D2D1::Point2F(rect.left, rect.top)),
		transform.TransformPoint(D2D1::Point2F(rect.right, rect.top)),
		transform.TransformPoint(D2D1::Point2F(rect.left, rect.bottom)),
		transform.TransformPoint(D2D1::Point2F(rect.right, rect.bottom))
	};
	D2D1_RECT_F bounds{
		points[0].x, points[0].y, points[0].x, points[0].y };
	for (size_t index = 1; index < std::size(points); ++index)
	{
		bounds.left = (std::min)(bounds.left, points[index].x);
		bounds.top = (std::min)(bounds.top, points[index].y);
		bounds.right = (std::max)(bounds.right, points[index].x);
		bounds.bottom = (std::max)(bounds.bottom, points[index].y);
	}
	return bounds;
}

D2D1_RECT_F Control::GetRenderedAbsoluteRectDip()
{
	const cui::core::Rect rect{
		GetAbsoluteLocationDip(), GetRenderSizeDip() };
	return TransformAbsoluteRectToRenderSpace(D2D1_RECT_F{
		rect.Left(), rect.Top(), rect.Right(), rect.Bottom() });
}

void Control::Arrange(cui::core::Rect finalRect)
{
	finalRect = finalRect.Normalized();
	const cui::core::Rect previousRect = _layoutState.hasArranged
		? _layoutState.arrangedRect
		: cui::core::Rect{};
	const bool geometryChanged = previousRect != finalRect;
	const bool sizeChanged = previousRect.width != finalRect.width
		|| previousRect.height != finalRect.height;

	if (geometryChanged)
		InvalidateVisualBoundsSubtree();
	_layoutState.CommitArrange(finalRect);
	if (auto* templateRoot = GetControlTemplateRoot())
	{
		templateRoot->Arrange(cui::core::Rect{
			0.0f, 0.0f, finalRect.width, finalRect.height });
	}
	if (geometryChanged)
	{
		InvalidatePresentationGeometrySubtree();
		InvalidateVisualBoundsSubtree();
	}

	if (sizeChanged)
	{
		if (const auto* metadata = FindPropertyMetadata(L"ActualWidth"))
			ApplyPropertyMetadataChange(*metadata,
				BindingValue(previousRect.width), BindingValue(finalRect.width));
		if (const auto* metadata = FindPropertyMetadata(L"ActualHeight"))
			ApplyPropertyMetadataChange(*metadata,
				BindingValue(previousRect.height), BindingValue(finalRect.height));
		SizeChangedEventArgs args(
			previousRect.Extent(), finalRect.Extent());
		this->SizeChanged(this, args);
	}
	if (sizeChanged)
		this->OnComputedLayoutSizeChanged();
}
