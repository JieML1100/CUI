#include "TabControl.h"

#include "Layout/OverlayLayout.h"
#include "StyleInfrastructure.h"
#include "TemplateInfrastructure.h"
#include "Window.h"
#include "WindowInfrastructure.h"
#include "XamlInfrastructure.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

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
	auto PropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			TOwner& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == propertyName) handler();
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

	D2D1_COLOR_F WithAlpha(D2D1_COLOR_F color, float alpha) noexcept
	{
		color.a = (std::clamp)(alpha, 0.0f, 1.0f);
		return color;
	}
}

TabItem::TabItem()
{
	RegisterDependencyProperties();
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(Thickness(0.0f)),
		DependencyPropertyValueSource::Theme);
	SetPresentationSuppressed(true);
}

void TabItem::RegisterDependencyProperties()
{
	HeaderedContentControl::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyOptions<TabItem, bool> options;
		options.DefaultValue = false;
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.IsReadOnly = false;
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Boolean;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		options.Design.Browsable = true;
		DependencyPropertyRegistry::Register<TabItem, bool>(
			L"IsSelected",
			[](TabItem& target) { return target.IsSelected; },
			[](TabItem& target, const bool& value)
			{ target.ApplyIsSelectedValue(value); },
			PropertySubscriber<TabItem>(L"IsSelected"),
			std::move(options));
		return true;
	}();
	(void)registered;
}

GET_CPP(TabItem, bool, IsSelected)
{
	return _isSelected;
}

SET_CPP(TabItem, bool, IsSelected)
{
	if (!SetPropertyField(L"IsSelected", _isSelected, value)) return;
	auto* owner = dynamic_cast<TabControl*>(GetLogicalParent());
	if (!owner) return;
	const int index = owner->IndexOfItem(this);
	if (value) (void)owner->SelectItem(index);
	else if (owner->GetSelectedIndex() == index)
		(void)owner->SelectIndex(-1);
}

void TabItem::SetCurrentIsSelected(bool value)
{
	(void)SetCurrentPropertyField(L"IsSelected", _isSelected, value);
}

void TabItem::ApplyIsSelectedValue(bool value)
{
	if (_isSelected == value) return;
	if (!SetPropertyField(L"IsSelected", _isSelected, value)) return;
	SetPresentationSuppressed(!value);
	SetStyleState(ControlStyleState::Selected, value);
	RoutedEventArgs args;
	if (value) Selected(this, args);
	else Unselected(this, args);
	InvalidateVisual();
}

void TabItem::ConfigureHeaderVisual(Control& child)
{
	HeaderedContentControl::ConfigureHeaderVisual(child);
	// The owning TabControl presents Header in its strip; this instance is the
	// page content projection only.
	cui::framework::TemplateAccess::SetPresentationSuppressed(child, true);
}

void TabItem::ReleaseHeaderVisual(Control& child)
{
	cui::framework::TemplateAccess::SetPresentationSuppressed(child, false);
	HeaderedContentControl::ReleaseHeaderVisual(child);
}

float TabItem::GetHeaderSlotHeightDip(float)
{
	return 0.0f;
}

void TabControl::RegisterDependencyProperties()
{
	Selector::RegisterDependencyProperties();
	TabItem::RegisterDependencyProperties();
	static const bool registered = []
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
		DependencyPropertyRegistry::Register<TabControl, Dock>(
			L"TabStripPlacement",
			[](TabControl& target) { return target.TabStripPlacement; },
			[](TabControl& target, const Dock& value)
			{ target.TabStripPlacement = value; },
			PropertySubscriber<TabControl>(L"TabStripPlacement"),
			std::move(options));
		return true;
	}();
	(void)registered;
}

GET_CPP(TabControl, Dock, TabStripPlacement)
{
	return _tabStripPlacement;
}

SET_CPP(TabControl, Dock, TabStripPlacement)
{
	(void)SetPropertyField(
		L"TabStripPlacement", _tabStripPlacement, value);
}

TabControl::TabControl()
	: Selector()
{
	RegisterDependencyProperties();
	ReplaceItemsHostCore(std::make_unique<TabItemsHost>());
	(void)TrySetPropertyValue(
		L"Focusable", BindingValue(true),
		DependencyPropertyValueSource::Theme);
	RetainEventConnection(OnMouseMove.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.OriginalSource == this) return;
			int hovered = -1;
			(void)TryGetTabHeaderIndexAt(args.X, args.Y, hovered);
			if (_hoveredHeaderIndex == hovered) return;
			_hoveredHeaderIndex = hovered;
			InvalidateVisual();
		}));
}

D2D1_RECT_F TabControl::GetTabStripRect() const noexcept
{
	const auto size = GetActualSizeDip().NonNegative();
	const float width = size.width;
	const float height = size.height;
	const float horizontal = (std::min)(HorizontalStripExtent, height);
	const float vertical = (std::min)(VerticalStripExtent, width);
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
	if (IsVerticalStrip(_tabStripPlacement))
	{
		const float extent = RectHeight(strip) / static_cast<float>(count);
		return D2D1::RectF(
			strip.left, strip.top + extent * index,
			strip.right, strip.top + extent * (index + 1));
	}
	const float extent = RectWidth(strip) / static_cast<float>(count);
	return D2D1::RectF(
		strip.left + extent * index, strip.top,
		strip.left + extent * (index + 1), strip.bottom);
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

D2D1_RECT_F TabControl::GetVisualChildrenClipRect()
{
	return GetContentRect();
}

bool TabControl::ShouldHitTestChildrenAt(int localX, int localY) const
{
	int headerIndex = -1;
	return !TryGetTabHeaderIndexAt(localX, localY, headerIndex);
}

CursorKind TabControl::QueryCursor(int localX, int localY)
{
	int index = -1;
	return IsEnabled && TryGetTabHeaderIndexAt(localX, localY, index)
		? CursorKind::Hand : Control::QueryCursor(localX, localY);
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
	if (auto* page = GetItem(SelectedIndex)) ArrangePage(page);
}

void TabControl::ArrangePage(TabItem* page)
{
	if (!page) return;
	const auto content = GetContentRect();
	page->Arrange(cui::core::Rect{
		0.0f, 0.0f, RectWidth(content), RectHeight(content) });
}

void TabControl::PerformPendingLayout()
{
	const auto content = GetContentRect();
	if (auto* host = GetItemsHost())
	{
		host->Arrange(cui::core::Rect::FromLTRB(
			content.left, content.top, content.right, content.bottom));
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

void TabControl::SynchronizeSelectionProjection()
{
	const int selected = SelectedIndex;
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
		if (auto* page = GetItem(index))
			page->SetCurrentIsSelected(index == selected);
	_selectedTabIdentity = GetItem(selected);
}

void TabControl::OnSelectedIndexChanged(int, int)
{
	SynchronizeSelectionProjection();
	RequestLayout();
	InvalidateVisual();
}

void TabControl::PrepareItemMutation()
{
	_hoveredHeaderIndex = -1;
	_pressedHeaderIndex = -1;
	(void)ReleaseMouseCapture();
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
	if (GetSelectedValuePath().empty()) return BindingValue(item);
	BindingValue result;
	return item->TryGetPropertyValue(GetSelectedValuePath(), result)
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
	if (GetSelectedValuePath().empty())
	{
		TabItem* item = nullptr;
		SetCurrentSelectedIndex(value.TryGet(item) ? IndexOfItem(item) : -1);
		return;
	}
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
	{
		BindingValue candidate;
		auto* item = GetItem(index);
		if (item && item->TryGetPropertyValue(
			GetSelectedValuePath(), candidate)
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
	page->SetHeader(BindingValue(
		GetBindingRecordText(item, GetDisplayMemberPath())));
	page->SetContent(BindingValue(item));
	page->SetContentTemplate(GetItemTemplate());
	page->SetDisplayMemberPath(GetDisplayMemberPath());
	auto* pagePointer = page.get();
	std::weak_ptr<IBindingSource> itemIdentity = item.Shared();
	const auto displayPath = GetDisplayMemberPath();
	observation = ObserveBindingPaths(
		item, { displayPath },
		[pagePointer, itemIdentity, displayPath]
		{
			const auto source = itemIdentity.lock();
			if (!source) return;
			pagePointer->SetHeader(BindingValue(GetBindingRecordText(
				BindingSourceReference(source), displayPath)));
		});
	return page;
}

void TabControl::OnGeneratedItemsRebuilt()
{
	Selector::OnGeneratedItemsRebuilt();
	if (ItemCount() > 0 && SelectedIndex < 0)
		SetCurrentSelectedIndex(0);
	for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
		if (auto* item = GetItem(index)) cui::framework::XamlAccess::SetLogicalParent(*item, this);
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

void TabControl::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	SynchronizeSelectionProjection();
	auto* drawing = GetDrawingContext();
	const auto size = GetActualSizeDip().NonNegative();
	const auto strip = GetTabStripRect();
	const auto content = GetContentRect();
	auto* window = GetPresentationWindow();
	const auto stripBack = WithAlpha(RendererBackgroundColor, 0.38f);
	const auto hoverBack =
		cui::framework::WindowAccess::EffectiveControlBackColor(
			*window, cui::theme::palette::AccentSoft);
	const auto selectedBack =
		cui::framework::WindowAccess::EffectiveControlBackColor(
			*window, cui::theme::palette::AccentSelected);
	const auto accent =
		cui::framework::WindowAccess::EffectiveControlBackColor(
			*window, cui::theme::palette::Accent);
	const auto mutedText = WithAlpha(
		cui::framework::WindowAccess::EffectiveControlForeColor(
			*window, RendererForegroundColor), 0.72f);
	const auto activeText =
		cui::framework::WindowAccess::EffectiveControlForeColor(
			*window, RendererForegroundColor);

	BeginRender();
	drawing->FillRect(0.0f, 0.0f, size.width, size.height, RendererBackgroundColor);
	if (RectWidth(strip) > 0.0f && RectHeight(strip) > 0.0f)
	{
		// DirectWrite may wrap a long header even when its nominal layout height
		// is one line. The tab strip is a distinct presentation viewport: no
		// header glyph or hover chrome may leak into the selected page.
		drawing->PushDrawRect(
			strip.left, strip.top, RectWidth(strip), RectHeight(strip));
		drawing->FillRect(
			strip.left, strip.top, RectWidth(strip), RectHeight(strip), stripBack);
		for (int index = 0; index < static_cast<int>(ItemCount()); ++index)
		{
			const auto header = GetTabHeaderRect(index);
			drawing->PushDrawRect(
				header.left, header.top,
				RectWidth(header), RectHeight(header));
			const bool selected = index == SelectedIndex;
			const bool hovered = index == _hoveredHeaderIndex;
			if (selected || hovered)
				drawing->FillRect(
					header.left + 1.0f, header.top + 1.0f,
					(std::max)(0.0f, RectWidth(header) - 2.0f),
					(std::max)(0.0f, RectHeight(header) - 2.0f),
					selected ? selectedBack : hoverBack);

			if (selected)
			{
				constexpr float line = 3.0f;
				switch (_tabStripPlacement)
				{
				case Dock::Bottom:
					drawing->FillRect(header.left, header.top,
						RectWidth(header), line, accent); break;
				case Dock::Left:
					drawing->FillRect(header.right - line, header.top,
						line, RectHeight(header), accent); break;
				case Dock::Right:
					drawing->FillRect(header.left, header.top,
						line, RectHeight(header), accent); break;
				case Dock::Top:
				default:
					drawing->FillRect(header.left, header.bottom - line,
						RectWidth(header), line, accent); break;
				}
			}

			auto* item = GetItem(index);
			std::wstring text;
			if (item) (void)item->GetHeader().TryGet(text);
			if (!text.empty())
			{
				const auto textSize = GetRenderFont()->GetTextSize(text);
				const float x = header.left + 8.0f;
				const float y = header.top
					+ (RectHeight(header) - textSize.height) * 0.5f;
				drawing->DrawString(
					text, x, (std::max)(header.top, y),
					(std::max)(1.0f, RectWidth(header) - 16.0f),
					textSize.height + 2.0f,
					selected ? activeText : mutedText, GetRenderFont());
			}
			drawing->PopDrawRect();
		}
		drawing->PopDrawRect();
	}

	if (RectWidth(content) > 0.0f && RectHeight(content) > 0.0f)
	{
		const float border = BorderThickness.MaxEdge();
		if (border > 0.0f)
			drawing->DrawRect(
				content.left, content.top,
				RectWidth(content), RectHeight(content),
				RendererBorderColor, border);
	}
	if (!IsEnabled)
		drawing->FillRect(
			0.0f, 0.0f, size.width, size.height,
			D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.45f });
	EndRender();
}

bool TabControl::ProcessInput(const InputReport& input)
{
	if (!IsEnabled || !IsVisible) return true;
	if (input.Kind == InputReportKind::PointerMove
		|| input.Kind == InputReportKind::PointerDown)
	{
		int hovered = -1;
		(void)TryGetTabHeaderIndexAt(input.X, input.Y, hovered);
		if (_hoveredHeaderIndex != hovered)
		{
			_hoveredHeaderIndex = hovered;
			InvalidateVisual();
		}
	}
	else if (input.Kind == InputReportKind::PointerLeave
		&& _hoveredHeaderIndex != -1)
	{
		_hoveredHeaderIndex = -1;
		InvalidateVisual();
	}

	if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Left)
	{
		int index = -1;
		if (TryGetTabHeaderIndexAt(input.X, input.Y, index))
		{
			_pressedHeaderIndex = index;
			(void)CaptureMouse();
			(void)SelectItem(index);
			return true;
		}
	}
	else if (input.Kind == InputReportKind::PointerUp
		&& _pressedHeaderIndex >= 0)
	{
		_pressedHeaderIndex = -1;
		(void)ReleaseMouseCapture();
		return true;
	}
	else if (input.Kind == InputReportKind::KeyDown
		&& HandlesNavigationKey(input.Key) && ItemCount() > 0)
	{
		int next = SelectedIndex < 0 ? 0 : SelectedIndex;
		switch (input.Key)
		{
		case Key::Left:
		case Key::Up: --next; break;
		case Key::Right:
		case Key::Down: ++next; break;
		case Key::Home: next = 0; break;
		case Key::End: next = static_cast<int>(ItemCount()) - 1; break;
		default: break;
		}
		(void)SelectItem((std::clamp)(
			next, 0, static_cast<int>(ItemCount()) - 1));
		return true;
	}
	return Control::ProcessInput(input);
}
