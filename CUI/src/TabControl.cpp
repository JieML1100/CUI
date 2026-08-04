#include "TabControl.h"

#include "Canvas.h"
#include "InputManager.h"
#include "Layout/OverlayLayout.h"
#include "StyleInfrastructure.h"
#include "TemplateInfrastructure.h"
#include "TreeInfrastructure.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <vector>

namespace
{
	class TabItemsHost final : public Panel
	{
	public:
		cui::core::Size MeasureCore(
			const cui::core::Constraints& available) override
		{
			return cui::layout::MeasureOverlayChildren(
				GetLayoutChildrenView(), available,
				GetSpecifiedLayout().padding);
		}

	protected:
		void RequestLayout() override
		{
			_layoutPending = true;
			Panel::RequestLayout();
		}

		void OnComputedLayoutSizeChanged() override
		{
			_layoutPending = true;
		}

		void PerformPendingLayout() override
		{
			if (IsLayoutSuspended() || !_layoutPending) return;
			const auto size = GetActualSizeDip();
			const auto padding = GetSpecifiedLayout().padding;
			cui::layout::ArrangeOverlayChildren(
				GetLayoutChildrenView(),
				cui::core::Rect{
					padding.left,
					padding.top,
					(std::max)(0.0f,
						size.width - padding.Horizontal()),
					(std::max)(0.0f,
						size.height - padding.Vertical()) });
			_layoutPending = false;
		}

		bool ValidateVisualChildCollection(
			std::span<Control* const> children,
			std::string& error) const override
		{
			for (auto* child : children)
			{
				if (dynamic_cast<TabItem*>(child)) continue;
				error = "TabControl ItemsHost accepts TabItem children only";
				return false;
			}
			return true;
		}

	private:
		bool _layoutPending = true;
	};

	template<typename TOwner>
	auto PropertySubscriber(
		const DependencyProperty& (*propertyAccessor)())
	{
		return [propertyAccessor](
			TOwner& target,
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

	bool PointInRect(
		const D2D1_RECT_F& rect, float x, float y) noexcept
	{
		return x >= rect.left && x < rect.right
			&& y >= rect.top && y < rect.bottom;
	}

	float RectWidth(const D2D1_RECT_F& rect) noexcept
	{
		return (std::max)(0.0f, rect.right - rect.left);
	}

	float RectHeight(const D2D1_RECT_F& rect) noexcept
	{
		return (std::max)(0.0f, rect.bottom - rect.top);
	}

	bool IsVerticalStrip(Dock placement) noexcept
	{
		return placement == Dock::Left || placement == Dock::Right;
	}

}

const DependencyProperty& TabItem::IsSelectedProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TabItem, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.IsReadOnly = false;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Browsable = true;
		)
		return DependencyPropertyRegistry::RegisterStatic<TabItem, bool>(
			DependencyPropertyRegistrationLiteral(L"IsSelected"),
			[](TabItem& target) { return target.IsSelected; },
			[](TabItem& target, const bool& value)
			{ target.ApplyIsSelectedValue(value); },
			PropertySubscriber<TabItem>(&TabItem::IsSelectedProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TabItem::TabStripPlacementProperty()
{
	return TabStripPlacementPropertyKey().Property();
}

const DependencyProperty& TabControl::TabStripPlacementProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TabControl, Dock> options;
		options.DefaultValue = Dock::Top;
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		options.Validate = [](const Dock& value)
		{
			switch (value)
			{
			case Dock::Left:
			case Dock::Top:
			case Dock::Right:
			case Dock::Bottom:
				return true;
			default:
				return false;
			}
		};
		options.Changed = [](
			TabControl& target, const Dock&, const Dock&)
		{
			target.RequestLayout();
			target.InvalidateVisual();
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Layout";
		options.Design.CategoryOrder = 100;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Choices = {
			{ L"Left", BindingValue(Dock::Left) },
			{ L"Top", BindingValue(Dock::Top) },
			{ L"Right", BindingValue(Dock::Right) },
			{ L"Bottom", BindingValue(Dock::Bottom) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<TabControl, Dock>(
			DependencyPropertyRegistrationLiteral(L"TabStripPlacement"),
			[](TabControl& target) { return target.TabStripPlacement; },
			[](TabControl& target, const Dock& value)
			{ target.TabStripPlacement = value; },
			PropertySubscriber<TabControl>(
				&TabControl::TabStripPlacementProperty),
			std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TabControl::ContentTemplateProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TabControl, ItemTemplateReference> options;
		options.DefaultValue = ItemTemplateReference{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Content";
		options.Design.CategoryOrder = 60;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TabControl, ItemTemplateReference>(
				DependencyPropertyRegistrationLiteral(L"ContentTemplate"),
				[](TabControl& target) { return target.GetContentTemplate(); },
				[](TabControl& target, const ItemTemplateReference& value)
				{ target.SetContentTemplate(value); },
				PropertySubscriber<TabControl>(
					&TabControl::ContentTemplateProperty),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TabControl::SelectedContentProperty()
{
	return SelectedContentPropertyKey().Property();
}

const DependencyProperty& TabControl::SelectedContentTemplateProperty()
{
	return SelectedContentTemplatePropertyKey().Property();
}

const DependencyPropertyKey& TabItem::TabStripPlacementPropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TabItem, Dock> options;
		options.DefaultValue = Dock::Top;
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		options.IsReadOnly = true;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Layout";
		options.Design.CategoryOrder = 100;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Browsable = false;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		options.Design.Choices = {
			{ L"Left", BindingValue(Dock::Left) },
			{ L"Top", BindingValue(Dock::Top) },
			{ L"Right", BindingValue(Dock::Right) },
			{ L"Bottom", BindingValue(Dock::Bottom) }
		};
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<TabItem, Dock>(
			DependencyPropertyRegistrationLiteral(L"TabStripPlacement"),
			[](TabItem& target) { return target.TabStripPlacement; },
			[](TabItem& target, const Dock& value)
			{
				(void)target.SetReadOnlyPropertyField(
					TabStripPlacementPropertyKey(),
					target._tabStripPlacement, value);
			},
			PropertySubscriber<TabItem>(
				&TabItem::TabStripPlacementProperty),
			std::move(options));
	}();
	return registration.Key();
}

const DependencyPropertyKey& TabControl::SelectedContentPropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TabControl, BindingValue> options;
		options.DefaultValue = BindingValue{};
		options.IsReadOnly = true;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Content";
		options.Design.CategoryOrder = 60;
		options.Design.Order = 20;
		options.Design.Browsable = false;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			TabControl, BindingValue>(
				DependencyPropertyRegistrationLiteral(L"SelectedContent"),
				[](TabControl& target) { return target.GetSelectedContent(); },
				[](TabControl& target, const BindingValue& value)
				{
					(void)target.SetReadOnlyPropertyField(
						SelectedContentPropertyKey(),
						target._selectedContent, value);
				},
				PropertySubscriber<TabControl>(
					&TabControl::SelectedContentProperty),
				std::move(options));
	}();
	return registration.Key();
}

const DependencyPropertyKey& TabControl::SelectedContentTemplatePropertyKey()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TabControl, ItemTemplateReference> options;
		options.DefaultValue = ItemTemplateReference{};
		options.IsReadOnly = true;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Content";
		options.Design.CategoryOrder = 60;
		options.Design.Order = 30;
		options.Design.Browsable = false;
		options.Design.Persistence = DependencyPropertyPersistence::Transient;
		)
		return DependencyPropertyRegistry::RegisterReadOnlyStatic<
			TabControl, ItemTemplateReference>(
				DependencyPropertyRegistrationLiteral(
					L"SelectedContentTemplate"),
				[](TabControl& target)
				{ return target.GetSelectedContentTemplate(); },
				[](TabControl& target, const ItemTemplateReference& value)
				{
					(void)target.SetReadOnlyPropertyField(
						SelectedContentTemplatePropertyKey(),
						target._selectedContentTemplate, value);
				},
				PropertySubscriber<TabControl>(
					&TabControl::SelectedContentTemplateProperty),
				std::move(options));
	}();
	return registration.Key();
}

TabItem::TabItem()
{
	RegisterDependencyProperties();
	EnsureClassHandlers();
}

void TabItem::RegisterDependencyProperties()
{
	HeaderedContentControl::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)IsSelectedProperty();
	(void)TabStripPlacementProperty();
#endif
}

GET_CPP(TabItem, bool, IsSelected)
{
	return _isSelected;
}

SET_CPP(TabItem, bool, IsSelected)
{
	if (!SetPropertyField(IsSelectedProperty(), _isSelected, value)) return;
	auto* owner = dynamic_cast<TabControl*>(GetLogicalParent());
	if (!owner) return;
	const int index = owner->IndexOfItem(this);
	if (value) (void)owner->SelectItem(index);
	else if (owner->GetSelectedIndex() == index)
		(void)owner->SelectIndex(-1);
}

void TabItem::SetCurrentIsSelected(bool value)
{
	(void)SetCurrentPropertyField(IsSelectedProperty(), _isSelected, value);
}

void TabItem::ApplyIsSelectedValue(bool value)
{
	if (_isSelected == value) return;
	if (!SetPropertyField(IsSelectedProperty(), _isSelected, value)) return;
	SetStyleState(ControlStyleState::Selected, value);
	RoutedEventArgs args;
	if (value) Selected(this, args);
	else Unselected(this, args);
	InvalidateVisual();
}

GET_CPP(TabItem, Dock, TabStripPlacement)
{
	return _tabStripPlacement;
}

void TabItem::SetTabStripPlacementProjection(Dock value)
{
	(void)SetReadOnlyPropertyField(
		TabStripPlacementPropertyKey(),
		_tabStripPlacement,
		value);
}

void TabItem::EnsureClassHandlers()
{
	static const std::vector<EventConnection> handlers = []
	{
		std::vector<EventConnection> result;
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_TabItem,
			RoutedEventId::MouseDown,
			&TabItem::HandleDescendantPointerPress));
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_TabItem,
			RoutedEventId::MouseDoubleClick,
			&TabItem::HandleDescendantPointerPress));
		return result;
	}();
	(void)handlers;
}

bool TabItem::IsOriginalSourceWithinHeader(Control* source) const noexcept
{
	auto* header = const_cast<TabItem*>(this)
		->FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_Header"));
	if (!header || !source) return false;
	for (auto* current = source; current; current = current->GetRoutedParent())
	{
		if (current == header) return true;
		if (current == this) break;
	}
	return false;
}

void TabItem::HandleDescendantPointerPress(
	Control* sender,
	RoutedEventArgs& args)
{
	auto* item = dynamic_cast<TabItem*>(sender);
	if (!item || args.Handled || !item->IsEffectivelyEnabled()
		|| !item->IsVisible) return;
	auto& mouse = static_cast<MouseEventArgs&>(args);
	if (mouse.ChangedButton != MouseButton::Left
		|| !item->IsOriginalSourceWithinHeader(args.OriginalSource)) return;
	auto* owner = dynamic_cast<TabControl*>(item->GetLogicalParent());
	if (!owner) return;
	const int index = owner->IndexOfItem(item);
	if (index >= 0 && owner->FocusAndSelectItem(index))
		args.Handled = true;
}

void TabItem::PreparePresentation()
{
	HeaderedContentControl::PreparePresentation();
	auto arrangePart = [](Control* part, const D2D1_RECT_F& rect)
	{
		if (!part) return;
		part->Arrange(cui::core::Rect::FromLTRB(
			rect.left, rect.top, rect.right, rect.bottom));
	};
	arrangePart(
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_Header")),
		_headerHitRect);
	auto* contentHost = FindDeclarativeTemplatePart(
		MakeTemplatePartToken(L"PART_SelectedContentHost"));
	if (contentHost)
		(void)contentHost->TrySetCurrentPropertyValue(
			Control::ClipToBoundsProperty(), BindingValue(true));
	arrangePart(contentHost, _contentHitRect);
}

bool TabItem::ContainsPoint(int localX, int localY)
{
	if (!Control::ContainsPoint(localX, localY)) return false;
	const float x = static_cast<float>(localX);
	const float y = static_cast<float>(localY);
	return PointInRect(_headerHitRect, x, y)
		|| (_isSelected && PointInRect(_contentHitRect, x, y));
}

bool TabItem::ShouldHitTestChildrenAt(int localX, int localY) const
{
	const float x = static_cast<float>(localX);
	const float y = static_cast<float>(localY);
	return PointInRect(_headerHitRect, x, y)
		|| (_isSelected && PointInRect(_contentHitRect, x, y));
}

bool TabItem::HandlesNavigationKey(Key key) const
{
	auto* owner = dynamic_cast<TabControl*>(GetLogicalParent());
	return owner
		? owner->HandlesNavigationKey(key)
		: HeaderedContentControl::HandlesNavigationKey(key);
}

bool TabItem::ProcessInput(const InputReport& input)
{
	if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Left
		&& PointInRect(
			_headerHitRect,
			static_cast<float>(input.X),
			static_cast<float>(input.Y)))
	{
		auto* owner = dynamic_cast<TabControl*>(GetLogicalParent());
		if (owner && owner->FocusAndSelectItem(owner->IndexOfItem(this)))
			return true;
	}
	if (input.Kind == InputReportKind::KeyDown)
	{
		auto* owner = dynamic_cast<TabControl*>(GetLogicalParent());
		if ((input.Key == Key::Space || input.Key == Key::Return)
			&& owner)
			return owner->FocusAndSelectItem(owner->IndexOfItem(this));
		if (owner && owner->ProcessTabNavigationKey(input)) return true;
	}
	return HeaderedContentControl::ProcessInput(input);
}

void TabControl::RegisterDependencyProperties()
{
	Selector::RegisterDependencyProperties();
	TabItem::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)TabStripPlacementProperty();
	(void)ContentTemplateProperty();
	(void)SelectedContentProperty();
	(void)SelectedContentTemplateProperty();
#endif
}

GET_CPP(TabControl, Dock, TabStripPlacement)
{
	return _tabStripPlacement;
}

SET_CPP(TabControl, Dock, TabStripPlacement)
{
	if (!SetPropertyField(
		TabStripPlacementProperty(), _tabStripPlacement, value)) return;
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
		if (auto* item = GetItem(index))
			item->SetTabStripPlacementProjection(value);
	RefreshHeaderMetrics();
	RequestLayout();
}

void TabControl::SetContentTemplate(ItemTemplateReference value)
{
	if (!SetPropertyField(
		ContentTemplateProperty(), _contentTemplate, std::move(value))) return;
	RefreshSelectedContentProjection();
	RequestLayout();
}

TabControl::TabControl()
	: Selector()
{
	RegisterDependencyProperties();
	EnsureClassHandlers();
	ReplaceItemsHostCore(std::make_unique<TabItemsHost>());
}

void TabControl::EnsureClassHandlers()
{
	static const std::vector<EventConnection> handlers = []
	{
		std::vector<EventConnection> result;
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_TabControl,
			RoutedEventId::MouseDown,
			&TabControl::HandleRoutedPointerPress));
		result.push_back(RoutedEventManager::RegisterClassHandler(
			UIClass::UI_TabControl,
			RoutedEventId::MouseDoubleClick,
			&TabControl::HandleRoutedPointerPress));
		return result;
	}();
	(void)handlers;
}

void TabControl::HandleRoutedPointerPress(
	Control* sender,
	RoutedEventArgs& args)
{
	auto* owner = dynamic_cast<TabControl*>(sender);
	if (!owner || args.Handled || !owner->IsEffectivelyEnabled()
		|| !owner->IsVisible) return;
	auto& mouse = static_cast<MouseEventArgs&>(args);
	if (mouse.ChangedButton != MouseButton::Left) return;
	int index = -1;
	if (owner->TryGetTabHeaderIndexAt(mouse.X, mouse.Y, index)
		&& owner->FocusAndSelectItem(index))
		args.Handled = true;
}

D2D1_RECT_F TabControl::GetTabStripRect() const noexcept
{
	const auto size = GetActualSizeDip().NonNegative();
	const float width = size.width;
	const float height = size.height;
	const float horizontal = (std::min)(
		(std::max)(DefaultHeaderExtent, _tabStripCrossExtent),
		height);
	const float vertical = (std::min)(
		(std::max)(DefaultVerticalStripExtent, _tabStripCrossExtent),
		width);
	switch (_tabStripPlacement)
	{
	case Dock::Bottom:
		return D2D1::RectF(0.0f, height - horizontal, width, height);
	case Dock::Left:
		return D2D1::RectF(0.0f, 0.0f, vertical, height);
	case Dock::Right:
		return D2D1::RectF(width - vertical, 0.0f, width, height);
	case Dock::Top:
	default:
		return D2D1::RectF(0.0f, 0.0f, width, horizontal);
	}
}

D2D1_RECT_F TabControl::GetContentRect() const noexcept
{
	const auto size = GetActualSizeDip().NonNegative();
	const auto strip = GetTabStripRect();
	switch (_tabStripPlacement)
	{
	case Dock::Bottom:
		return D2D1::RectF(0.0f, 0.0f, size.width, strip.top);
	case Dock::Left:
		return D2D1::RectF(strip.right, 0.0f, size.width, size.height);
	case Dock::Right:
		return D2D1::RectF(0.0f, 0.0f, strip.left, size.height);
	case Dock::Top:
	default:
		return D2D1::RectF(0.0f, strip.bottom, size.width, size.height);
	}
}

D2D1_RECT_F TabControl::GetTabHeaderRect(int index) const noexcept
{
	const int count = static_cast<int>(ItemCount());
	if (index < 0 || index >= count || count <= 0) return {};
	const auto strip = GetTabStripRect();
	const bool vertical = IsVerticalStrip(_tabStripPlacement);
	const float available = vertical
		? RectHeight(strip)
		: RectWidth(strip);
	float total = 0.0f;
	for (int current = 0; current < count; ++current)
		total += current < static_cast<int>(_headerPrimaryExtents.size())
			? _headerPrimaryExtents[static_cast<size_t>(current)]
			: DefaultHeaderExtent;
	const float scale = total > available && total > 0.0f
		? available / total
		: 1.0f;
	float start = 0.0f;
	for (int current = 0; current < index; ++current)
		start += (current < static_cast<int>(_headerPrimaryExtents.size())
			? _headerPrimaryExtents[static_cast<size_t>(current)]
			: DefaultHeaderExtent) * scale;
	const float extent = (index < static_cast<int>(
		_headerPrimaryExtents.size())
		? _headerPrimaryExtents[static_cast<size_t>(index)]
		: DefaultHeaderExtent) * scale;
	if (vertical)
	{
		return D2D1::RectF(
			strip.left,
			strip.top + start,
			strip.right,
			strip.top + start + extent);
	}
	return D2D1::RectF(
		strip.left + start,
		strip.top,
		strip.left + start + extent,
		strip.bottom);
}

bool TabControl::TryGetTabHeaderIndexAt(
	int localX, int localY, int& outIndex) const noexcept
{
	outIndex = -1;
	const auto strip = GetTabStripRect();
	if (!PointInRect(strip,
		static_cast<float>(localX), static_cast<float>(localY))) return false;
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
	{
		if (!PointInRect(GetTabHeaderRect(index),
			static_cast<float>(localX), static_cast<float>(localY))) continue;
		outIndex = index;
		return true;
	}
	return false;
}

bool TabControl::ShouldHitTestChildrenAt(int localX, int localY) const
{
	int headerIndex = -1;
	if (TryGetTabHeaderIndexAt(localX, localY, headerIndex))
		return true;
	return PointInRect(
		GetContentRect(),
		static_cast<float>(localX),
		static_cast<float>(localY));
}

bool TabControl::HandlesNavigationKey(Key key) const
{
	const bool vertical = IsVerticalStrip(_tabStripPlacement);
	switch (key)
	{
	case Key::Left:
	case Key::Right:
		return !vertical;
	case Key::Up:
	case Key::Down:
		return vertical;
	case Key::Home:
	case Key::End:
		return true;
	default:
		return false;
	}
}

void TabControl::PreparePresentation()
{
	Selector::PreparePresentation();
	SynchronizeSelectionProjection();
	RefreshHeaderMetrics();
	PerformPendingLayout();
}

void TabControl::ArrangePage(TabItem* page)
{
	if (!page) return;
	const auto size = GetActualSizeDip().NonNegative();
	page->Arrange(cui::core::Rect{
		0.0f, 0.0f, size.width, size.height });
	const int index = IndexOfItem(page);
	page->SetHeaderHitRect(GetTabHeaderRect(index));
	page->SetContentHitRect(GetContentRect());
	page->PreparePresentation();
}

void TabControl::PerformPendingLayout()
{
	RefreshHeaderMetrics();
	const auto size = GetActualSizeDip().NonNegative();
	const auto content = GetContentRect();
	if (auto* chrome =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_ContentChrome")))
		chrome->Arrange(cui::core::Rect::FromLTRB(
			content.left, content.top,
			content.right, content.bottom));
	if (auto* presenter =
		FindDeclarativeTemplatePart(
			MakeTemplatePartToken(L"PART_ItemsPresenter")))
		presenter->Arrange(cui::core::Rect{
			0.0f, 0.0f, size.width, size.height });
	if (auto* host = GetItemsHost())
	{
		host->Arrange(cui::core::Rect{
			0.0f, 0.0f, size.width, size.height });
	}
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
	{
		if (auto* page = GetItem(index))
		{
			ArrangePage(page);
			if (page->IsSelected) page->UpdateLayout();
		}
	}
	SynchronizeSelectionProjection();
}

void TabControl::RefreshHeaderMetrics()
{
	_headerPrimaryExtents.clear();
	_headerPrimaryExtents.reserve(ItemCount());
	const bool vertical = IsVerticalStrip(_tabStripPlacement);
	float crossExtent = vertical
		? DefaultVerticalStripExtent
		: DefaultHeaderExtent;
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
	{
		auto* item = GetItem(index);
		cui::core::Size desired{};
		if (item)
		{
			item->SetTabStripPlacementProjection(_tabStripPlacement);
			item->PreparePresentation();
			if (auto* header =
				item->FindDeclarativeTemplatePart(
					MakeTemplatePartToken(L"PART_Header")))
			{
				(void)header->Measure(
					cui::core::Constraints::Unbounded());
				desired = header->GetDesiredSizeDip();
			}
		}
		const float primary = vertical
			? desired.height
			: desired.width;
		const float cross = vertical
			? desired.width
			: desired.height;
		_headerPrimaryExtents.push_back(
			(std::max)(DefaultHeaderExtent, primary));
		crossExtent = (std::max)(crossExtent, cross);
	}
	_tabStripCrossExtent = crossExtent;
}

void TabControl::RefreshSelectedContentProjection()
{
	BindingValue content;
	ItemTemplateReference contentTemplate = _contentTemplate;
	if (auto* item = GetItem(SelectedIndex))
	{
		content = item->GetContent();
		if (item->GetContentTemplate())
			contentTemplate = item->GetContentTemplate();
	}
	(void)SetReadOnlyPropertyField(
		SelectedContentPropertyKey(),
		_selectedContent,
		std::move(content));
	(void)SetReadOnlyPropertyField(
		SelectedContentTemplatePropertyKey(),
		_selectedContentTemplate,
		std::move(contentTemplate));
}

void TabControl::SynchronizeSelectionProjection()
{
	const int selected = SelectedIndex;
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
		if (auto* page = GetItem(index))
		{
			page->SetTabStripPlacementProjection(_tabStripPlacement);
			page->SetCurrentIsSelected(index == selected);
		}
	_selectedTabIdentity = GetItem(selected);
	RefreshSelectedContentProjection();
}

void TabControl::OnSelectedIndexChanged(int, int)
{
	SynchronizeSelectionProjection();
	RequestLayout();
	InvalidateVisual();
}

void TabControl::PrepareItemMutation()
{
	_selectedTabIdentity = GetItem(SelectedIndex);
}

void TabControl::ReconcileItemsAfterMutation(
	TabItem* previouslySelectedItem)
{
	int explicitlySelected = -1;
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
	{
		auto* item = GetItem(index);
		if (item && item != previouslySelectedItem && item->IsSelected)
			explicitlySelected = index;
	}
	Selector::OnAuthoredItemsChanged();
	if (explicitlySelected >= 0)
		SetCurrentSelectedIndex(explicitlySelected);
	else if (ItemCount() > 0 && SelectedIndex < 0)
		SetCurrentSelectedIndex(0);
	SynchronizeSelectionProjection();
	RequestLayout();
	InvalidateVisual();
}

TabItem* TabControl::AddItem(std::unique_ptr<TabItem> page)
{
	return InsertItem(static_cast<int>(ItemCount()), std::move(page));
}

TabItem* TabControl::InsertItem(
	int index, std::unique_ptr<TabItem> page)
{
	if (!page) throw std::invalid_argument("cannot add a null TabItem");
	if (index < 0 || index > static_cast<int>(ItemCount()))
		throw std::out_of_range("TabItem index is out of range");
	return static_cast<TabItem*>(InsertItemControl(
		static_cast<size_t>(index), std::move(page)));
}

TabItem* TabControl::GetItem(int index) const noexcept
{
	return index < 0 ? nullptr : static_cast<TabItem*>(
		GetGeneratedItem(static_cast<size_t>(index)));
}

int TabControl::IndexOfItem(const TabItem* page) const noexcept
{
	if (!page) return -1;
	for (size_t index = 0; index < ItemCount(); ++index)
		if (GetGeneratedItem(index) == page) return static_cast<int>(index);
	return -1;
}

std::unique_ptr<TabItem> TabControl::DetachItemAt(int index)
{
	if (index < 0 || index >= static_cast<int>(ItemCount())) return {};
	PrepareItemMutation();
	auto detached = DetachItemControlAt(static_cast<size_t>(index));
	return std::unique_ptr<TabItem>(
		static_cast<TabItem*>(detached.release()));
}

std::unique_ptr<TabItem> TabControl::DetachItem(TabItem* page)
{
	return DetachItemAt(IndexOfItem(page));
}

bool TabControl::RemoveItemAt(int index)
{
	return DetachItemAt(index) != nullptr;
}

bool TabControl::RemoveItem(TabItem* page)
{
	return RemoveItemAt(IndexOfItem(page));
}

bool TabControl::MoveItem(int oldIndex, int newIndex)
{
	if (oldIndex < 0 || newIndex < 0) return false;
	PrepareItemMutation();
	return MoveItemControl(
		static_cast<size_t>(oldIndex), static_cast<size_t>(newIndex));
}

void TabControl::ClearItems()
{
	PrepareItemMutation();
	if (GetItemsSource()) SetItemsSource({});
	else ClearItemControls();
	// SelectedIndex may be authored before Items are populated, but clearing an
	// established collection is a committed selection change, not a pending
	// future index.
	SetCurrentSelectedIndex(-1);
}

bool TabControl::SelectItem(int index)
{
	return index >= 0 && index < static_cast<int>(ItemCount())
		&& (SelectedIndex == index || SelectIndex(index));
}

BindingValue TabControl::GetSelectedItem() const
{
	if (GetItemsSource()) return Selector::GetSelectedItem();
	auto* item = GetItem(SelectedIndex);
	return item ? BindingValue(item) : BindingValue{};
}

void TabControl::SetSelectedItem(const BindingValue& value)
{
	if (GetItemsSource())
	{
		Selector::SetSelectedItem(value);
		return;
	}
	if (value.Empty())
	{
		SetCurrentSelectedIndex(-1);
		return;
	}
	TabItem* item = nullptr;
	SetCurrentSelectedIndex(value.TryGet(item) ? IndexOfItem(item) : -1);
}

BindingValue TabControl::GetSelectedValue() const
{
	if (GetItemsSource()) return Selector::GetSelectedValue();
	auto* item = GetItem(SelectedIndex);
	if (!item) return {};
	if (!HasSelectedValuePath()) return BindingValue(item);
	BindingValue result;
	return TryReadSelectedValue(*item, result)
		? result : BindingValue{};
}

void TabControl::SetSelectedValue(const BindingValue& value)
{
	if (GetItemsSource())
	{
		Selector::SetSelectedValue(value);
		return;
	}
	if (value.Empty())
	{
		SetCurrentSelectedIndex(-1);
		return;
	}
	if (!HasSelectedValuePath())
	{
		TabItem* item = nullptr;
		SetCurrentSelectedIndex(value.TryGet(item) ? IndexOfItem(item) : -1);
		return;
	}
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
	{
		BindingValue candidate;
		auto* item = GetItem(index);
		if (item && TryReadSelectedValue(*item, candidate)
			&& BindingItemValuesEqual(candidate, value))
		{
			SetCurrentSelectedIndex(index);
			return;
		}
	}
	SetCurrentSelectedIndex(-1);
}

std::unique_ptr<Panel> TabControl::CreateItemsHost() const
{
	return std::make_unique<TabItemsHost>();
}

std::unique_ptr<Control> TabControl::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t,
	BindingPathObservation& observation)
{
	auto page = std::make_unique<TabItem>();
	cui::framework::StyleAccess::SetResourceKey(
		*page, GetItemContainerStyle());
	page->SetDataContext(item);
	page->SetHeader(BindingValue(GetDisplayMemberText(item)));
	page->SetContent(BindingValue(item));
	page->SetContentTemplate(GetItemTemplate());
	page->SetCompiledDisplayMemberPath(GetCompiledDisplayMemberPath());
#if CUI_ENABLE_DYNAMIC_XAML
	if (GetCompiledDisplayMemberPath().Empty())
		page->SetDisplayMemberPath(GetDisplayMemberPath());
#endif
	auto* pagePointer = page.get();
	std::weak_ptr<IBindingSource> itemIdentity = item.Shared();
	const auto compiledDisplayPath = GetCompiledDisplayMemberPath();
#if CUI_ENABLE_DYNAMIC_XAML
	const auto dynamicDisplayPath = GetDisplayMemberPath();
#endif
	observation = ObserveDisplayMemberPath(
		item,
		[pagePointer, itemIdentity, compiledDisplayPath
#if CUI_ENABLE_DYNAMIC_XAML
			, dynamicDisplayPath
#endif
		]
		{
			const auto source = itemIdentity.lock();
			if (!source) return;
			const BindingSourceReference item(source);
			if (!compiledDisplayPath.Empty())
				pagePointer->SetHeader(BindingValue(
					GetBindingRecordText(item, compiledDisplayPath)));
#if CUI_ENABLE_DYNAMIC_XAML
			else pagePointer->SetHeader(BindingValue(
				GetBindingRecordText(item, dynamicDisplayPath)));
#endif
		});
	return page;
}

void TabControl::OnGeneratedItemsRebuilt()
{
	Selector::OnGeneratedItemsRebuilt();
	if (ItemCount() > 0 && SelectedIndex < 0)
		SetCurrentSelectedIndex(0);
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
		if (auto* item = GetItem(index))
			cui::framework::TreeAccess::SetLogicalParent(*item, this);
	SynchronizeSelectionProjection();
	RequestLayout();
}

bool TabControl::ValidateAuthoredItemControl(
	const Control& item, std::string& error) const
{
	if (dynamic_cast<const TabItem*>(&item)) return true;
	error = "TabControl Items can contain TabItem controls only";
	return false;
}

void TabControl::OnAuthoredItemsChanged() noexcept
{
	try
	{
		auto* identity = _selectedTabIdentity;
		PrepareItemMutation();
		ReconcileItemsAfterMutation(identity);
	}
	catch (...)
	{
		_selectedTabIdentity = nullptr;
	}
}

int TabControl::FindNextEligibleTab(
	int startIndex,
	int direction,
	bool wrap) const noexcept
{
	const int count = static_cast<int>(ItemCount());
	if (count <= 0 || direction == 0) return -1;
	int index = startIndex;
	for (int visited = 0; visited < count; ++visited)
	{
		index += direction;
		if (index < 0 || index >= count)
		{
			if (!wrap) return -1;
			index = index < 0 ? count - 1 : 0;
		}
		auto* item = GetItem(index);
		if (item && item->IsVisible
			&& item->IsEffectivelyEnabled()
			&& item->CanReceiveKeyboardFocus())
			return index;
	}
	return -1;
}

bool TabControl::FocusAndSelectItem(int index)
{
	auto* item = GetItem(index);
	if (!item || !item->IsVisible || !item->IsEffectivelyEnabled())
		return false;
	const ControlWeakReference itemLifetime(item);
	if (!SelectItem(index)) return false;
	item = dynamic_cast<TabItem*>(itemLifetime.Get());
	return item && item->Focus();
}

bool TabControl::ProcessTabNavigationKey(const InputReport& input)
{
	if (input.Kind != InputReportKind::KeyDown || ItemCount() == 0)
		return false;
	int start = SelectedIndex;
	int direction = 0;
	bool wrap = true;
	if (input.Key == Key::Tab
		&& input.HasModifier(ModifierKeys::Control))
		direction = input.HasModifier(ModifierKeys::Shift) ? -1 : 1;
	else if (input.Key == Key::Home)
	{
		start = -1;
		direction = 1;
		wrap = false;
	}
	else if (input.Key == Key::End)
	{
		start = static_cast<int>(ItemCount());
		direction = -1;
		wrap = false;
	}
	else
	{
		const bool vertical = IsVerticalStrip(_tabStripPlacement);
		if ((!vertical && input.Key == Key::Left)
			|| (vertical && input.Key == Key::Up))
			direction = -1;
		else if ((!vertical && input.Key == Key::Right)
			|| (vertical && input.Key == Key::Down))
			direction = 1;
	}
	if (direction == 0) return false;
	const int next = FindNextEligibleTab(start, direction, wrap);
	return next >= 0 && next != SelectedIndex
		&& FocusAndSelectItem(next);
}

bool TabControl::ProcessInput(const InputReport& input)
{
	if (ProcessTabNavigationKey(input))
	{
		auto args = input.CreateKeyEventArgs();
		OnKeyDown(this, args);
		return true;
	}
	return Selector::ProcessInput(input);
}
