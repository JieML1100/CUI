#include "TreeView.h"
#include "EventInfrastructure.h"
#include "StyleInfrastructure.h"

#include "ScrollViewer.h"
#include "TemplateInfrastructure.h"
#include "Window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <stdexcept>
#include <unordered_set>

namespace
{
	constexpr float DefaultTreeIndent = 18.0f;
	constexpr float DefaultChevronSlot = 18.0f;
	constexpr float DefaultRowHeight = 28.0f;

	bool EqualsTypeName(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	template<typename TValue>
	DependencyPropertyOptions<TreeViewItem, TValue> TreeItemStateOptions(
		TValue defaultValue,
		int order,
		bool readOnly,
		DependencyPropertyPersistence persistence)
	{
		DependencyPropertyOptions<TreeViewItem, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"State";
		options.Design.CategoryOrder = 70;
		options.Design.Order = order;
		options.Design.Browsable = !readOnly;
		options.Design.Persistence = persistence;
		options.IsReadOnly = readOnly;
		return options;
	}

	void DrawChevron(
		D2DGraphics& render,
		float centerX,
		float centerY,
		bool expanded,
		D2D1_COLOR_F color)
	{
		constexpr float halfWidth = 2.5f;
		constexpr float halfHeight = 4.0f;
		if (expanded)
		{
			render.DrawLine(
				D2D1::Point2F(centerX - halfHeight, centerY - halfWidth),
				D2D1::Point2F(centerX, centerY + halfWidth), color, 1.6f);
			render.DrawLine(
				D2D1::Point2F(centerX, centerY + halfWidth),
				D2D1::Point2F(centerX + halfHeight, centerY - halfWidth),
				color, 1.6f);
		}
		else
		{
			render.DrawLine(
				D2D1::Point2F(centerX - halfWidth, centerY - halfHeight),
				D2D1::Point2F(centerX + halfWidth, centerY), color, 1.6f);
			render.DrawLine(
				D2D1::Point2F(centerX + halfWidth, centerY),
				D2D1::Point2F(centerX - halfWidth, centerY + halfHeight),
				color, 1.6f);
		}
	}

	bool PointInRect(float x, float y, const D2D1_RECT_F& rect) noexcept
	{
		return x >= rect.left && x <= rect.right
			&& y >= rect.top && y <= rect.bottom;
	}
}

TreeViewItem::TreeViewItem()
	: HeaderedItemsControl()
{
	EnsureBindingPropertiesRegistered();
	_accessibilityId = AllocateAccessibilityVirtualId();
	(void)TrySetPropertyValue(
		L"VerticalAlignment", BindingValue(::VerticalAlignment::Top),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(Thickness(0.0f)),
		DependencyPropertyValueSource::Theme);
	RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	RetainEventConnection(OnMouseDown.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			if (args.ChangedButton != MouseButton::Left || !_owner) return;

			auto* header = GetHeaderVisual();
			const auto headerLocation = header
				? header->GetActualLocationDip() : cui::core::Point{};
			const auto headerSize = header
				? header->GetActualSizeDip()
				: cui::core::Size{
					GetActualSizeDip().width, DefaultRowHeight };
			const float rowHeight = (std::max)(
				DefaultRowHeight, headerLocation.y + headerSize.height);
			if (args.Y < 0.0f || args.Y > rowHeight) return;

			if (auto* window = _owner->GetPresentationWindow())
				window->SetKeyboardFocus(_owner, true);
			if (_hasItems && args.X >= 0.0f
				&& args.X <= DefaultChevronSlot)
				SetCurrentIsExpanded(!_expanded);
			else
				(void)_owner->SelectItem(this, true);
			args.Handled = true;
		}));
	ApplyExpansionPresentation();
}

void TreeViewItem::RegisterDependencyProperties()
{
	HeaderedItemsControl::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyRegistry::Register<TreeViewItem, bool>(L"IsExpanded",
			[](TreeViewItem& target) { return target.GetIsExpanded(); },
			[](TreeViewItem& target, const bool& value)
			{ target.ApplyIsExpandedValue(value); },
			[](TreeViewItem& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._expandedChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(
				false, 10, false, DependencyPropertyPersistence::Metadata));
		DependencyPropertyRegistry::Register<TreeViewItem, bool>(L"HasItems",
			[](TreeViewItem& target) { return target.GetHasItems(); },
			[](TreeViewItem& target, const bool& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"HasItems", target._hasItems, value);
			},
			[](TreeViewItem& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._hasItemsChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(
				false, 20, true, DependencyPropertyPersistence::Transient));
		DependencyPropertyRegistry::Register<TreeViewItem, int>(L"Level",
			[](TreeViewItem& target) { return target.GetLevel(); },
			[](TreeViewItem& target, const int& value)
			{
				(void)target.SetReadOnlyPropertyField(
					L"Level", target._level, value);
			},
			[](TreeViewItem& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._levelChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(
				0, 30, true, DependencyPropertyPersistence::Transient));
		DependencyPropertyRegistry::Register<TreeViewItem, bool>(L"IsSelected",
			[](TreeViewItem& target) { return target.GetIsSelected(); },
			[](TreeViewItem& target, const bool& value)
			{ target.ApplyIsSelectedValue(value); },
			[](TreeViewItem& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedChanged.Subscribe(
					[handler = std::move(handler)](TreeViewItem*) { handler(); });
			}, TreeItemStateOptions(
				false, 40, false, DependencyPropertyPersistence::Metadata));
		return true;
	}();
	(void)registered;
}

cui::core::Insets
TreeViewItem::GetHeaderPresentationInsets() const noexcept
{
	return cui::core::Insets{ DefaultChevronSlot, 0.0f, 0.0f, 0.0f };
}

cui::core::Insets
TreeViewItem::GetItemsPresentationInsets() const noexcept
{
	return cui::core::Insets{ DefaultTreeIndent, 0.0f, 0.0f, 0.0f };
}

bool TreeViewItem::ValidateAuthoredItemControl(
	const Control& item, std::string& error) const
{
	if (dynamic_cast<const TreeViewItem*>(&item)) return true;
	error = "TreeViewItem authored Items must be TreeViewItem controls";
	return false;
}

BindingListReference TreeViewItem::GetItemsSource() const noexcept
{
	if (_generatedContainer && _hierarchicalItemsSource)
		return _hierarchicalItemsSource;
	return HeaderedItemsControl::GetItemsSource();
}

void TreeViewItem::SetItemsSource(BindingListReference value)
{
	if (!_generatedContainer)
	{
		HeaderedItemsControl::SetItemsSource(std::move(value));
		SyncHasItems();
		if (_owner && !_initializingGeneratedContainer)
			_owner->RefreshHierarchy();
		return;
	}

	_hierarchicalItemsObservation = {};
	_hierarchicalItemsSource = std::move(value);
	RefreshHierarchicalCollectionObservation();
	if (_expanded || GetMaterializedItemsSource())
		HeaderedItemsControl::SetItemsSource(_hierarchicalItemsSource);
	SyncHasItems();
	if (_owner && !_initializingGeneratedContainer)
		_owner->RefreshHierarchy();
}

size_t TreeViewItem::ItemCount() const noexcept
{
	if (_generatedContainer && _hierarchicalItemsSource)
		return _hierarchicalItemsSource.Get()->Count();
	return HeaderedItemsControl::ItemCount();
}

TreeViewItem* TreeViewItem::ContainerFromIndex(size_t index) const noexcept
{
	if (_generatedContainer && _hierarchicalItemsSource
		&& !GetMaterializedItemsSource()) return nullptr;
	return dynamic_cast<TreeViewItem*>(
		HeaderedItemsControl::GetGeneratedItem(index));
}

bool TreeViewItem::InitializeGenerated(
	TreeView& owner,
	const BindingSourceReference& item,
	ItemTemplateReference headerTemplate,
	const std::wstring& displayMemberPath,
	int level,
	std::wstring* outError)
{
	if (outError) outError->clear();
	_initializingGeneratedContainer = true;
	_generatedContainer = true;
	_owner = &owner;
	_parentItem = nullptr;
	_dataItem = item;
	_headerDataTemplate = std::move(headerTemplate);
	if (!SetReadOnlyPropertyField(L"Level", _level, level))
	{
		if (outError) *outError =
			L"TreeViewItem Level 只读属性状态无法发布。";
		_initializingGeneratedContainer = false;
		return false;
	}
	SetHeaderTypeName(_headerDataTemplate
		? _headerDataTemplate.Get()->DataTypeName()
		: std::wstring{});
	SetHeaderDisplayMemberPath(displayMemberPath);
	SetHeaderTemplate(_headerDataTemplate);
	if (!SetDataContext(item))
	{
		if (outError) *outError =
			L"TreeViewItem 无法采用数据项 DataContext。";
		_initializingGeneratedContainer = false;
		return false;
	}
	SetHeader(BindingValue(item));
	if (!LastHeaderError().empty())
	{
		if (outError) *outError = LastHeaderError();
		_initializingGeneratedContainer = false;
		return false;
	}
	if (!RefreshHierarchicalItemsSource(outError))
	{
		_initializingGeneratedContainer = false;
		return false;
	}
	_initializingGeneratedContainer = false;
	ApplyExpansionPresentation();
	SyncHasItems();
	return true;
}

bool TreeViewItem::RefreshHierarchicalItemsSource(std::wstring* outError)
{
	if (outError) outError->clear();
	BindingListReference source;
	if (_headerDataTemplate && _headerDataTemplate.Get()->IsHierarchical())
	{
		if (!_headerDataTemplate.Get()->TryGetVisualChildItemsSource(
			_dataItem, source, outError)) return false;
	}

	BindingPathObservation observation;
	if (_headerDataTemplate && _headerDataTemplate.Get()->IsHierarchical())
	{
		const ControlWeakReference self(this);
		observation = _headerDataTemplate.Get()->ObserveChildItemsSource(
			_dataItem,
			[self]
			{
				auto* item = dynamic_cast<TreeViewItem*>(self.Get());
				if (!item) return;
				std::wstring error;
				if (!item->RefreshHierarchicalItemsSource(&error)
					&& !error.empty()) item->SetLastTemplateError(std::move(error));
			});
	}

	const bool materialized = static_cast<bool>(GetMaterializedItemsSource());
	_hierarchicalItemsSource = std::move(source);
	_hierarchicalItemsObservation = std::move(observation);
	RefreshHierarchicalCollectionObservation();
	if ((_expanded || materialized)
		&& GetMaterializedItemsSource() != _hierarchicalItemsSource)
	{
		HeaderedItemsControl::SetItemsSource(_hierarchicalItemsSource);
		if (!LastTemplateError().empty())
		{
			if (outError) *outError = LastTemplateError();
			return false;
		}
	}
	else if (GetMaterializedItemsSource() == _hierarchicalItemsSource)
	{
		SetLastTemplateError({});
	}
	if (_owner && LastTemplateError().empty()) _owner->SetHierarchyError({});
	SyncHasItems();
	if (_owner && !_initializingGeneratedContainer)
		_owner->RefreshHierarchy();
	return true;
}

void TreeViewItem::RefreshHierarchicalCollectionObservation()
{
	_hierarchicalItemsChanged.Disconnect();
	if (!_hierarchicalItemsSource) return;
	auto* sourceIdentity = _hierarchicalItemsSource.Get();
	const ControlWeakReference self(this);
	_hierarchicalItemsChanged = sourceIdentity->SubscribeChanged(
		[self, sourceIdentity](const CollectionChangedEventArgs&)
		{
			auto* item = dynamic_cast<TreeViewItem*>(self.Get());
			if (!item
				|| item->_hierarchicalItemsSource.Get() != sourceIdentity) return;
			item->SyncHasItems();
		});
}

bool TreeViewItem::EnsureChildrenRealized()
{
	if (!_generatedContainer || !_hierarchicalItemsSource) return true;
	if (GetMaterializedItemsSource() == _hierarchicalItemsSource) return true;
	HeaderedItemsControl::SetItemsSource(_hierarchicalItemsSource);
	return LastTemplateError().empty();
}

void TreeViewItem::BindHierarchy(
	TreeView& owner, TreeViewItem* parent, int level)
{
	_owner = &owner;
	_parentItem = parent;
	if (_level != level)
	{
		(void)SetReadOnlyPropertyField(L"Level", _level, level);
		cui::framework::EventAccess::Raise(_levelChanged, this);
	}
	SyncHasItems();
	ApplyExpansionPresentation();
}

void TreeViewItem::UnbindHierarchy() noexcept
{
	_owner = nullptr;
	_parentItem = nullptr;
}

void TreeViewItem::ApplyIsExpandedValue(bool value)
{
	if (_expanded == value) return;
	if (!SetPropertyField(L"IsExpanded", _expanded, value)) return;
	cui::framework::EventAccess::Raise(_expandedChanged, this);
	RoutedEventArgs args;
	if (value) Expanded(this, args);
	else Collapsed(this, args);
	ApplyExpansionPresentation();
	if (_owner)
	{
		_owner->RefreshHierarchy();
		// Expansion mutates the realized hierarchy and therefore the geometry
		// used by the very next pointer hit test.  Commit the owning ItemsControl
		// boundary now so rendering and input cannot observe the old row map.
		_owner->UpdateLayout();
	}
}

void TreeViewItem::SyncHasItems()
{
	const auto source = GetItemsSource();
	const bool value = source
		? source.Get()->Count() != 0 : AuthoredItemCount() != 0;
	if (_hasItems == value) return;
	if (!SetReadOnlyPropertyField(L"HasItems", _hasItems, value)) return;
	cui::framework::EventAccess::Raise(_hasItemsChanged, this);
}

void TreeViewItem::ApplyIsSelectedValue(bool value)
{
	if (_selected == value) return;
	if (!SetPropertyField(L"IsSelected", _selected, value)) return;
	SetStyleState(ControlStyleState::Selected, value);
	const auto back = value
		? cui::theme::palette::AccentSelected
		: IsMouseOver ? cui::theme::palette::AccentSoft
		: D2D1_COLOR_F{ 0, 0, 0, 0 };
	(void)TrySetPropertyValue(L"Background", BindingValue(back),
		DependencyPropertyValueSource::Theme);
	cui::framework::EventAccess::Raise(_selectedChanged, this);
	RoutedEventArgs args;
	if (value) Selected(this, args);
	else Unselected(this, args);
}

void TreeViewItem::OnIsMouseOverChanged(bool, bool value)
{
	if (!_selected)
	{
		const auto back = value
			? cui::theme::palette::AccentSoft
			: D2D1_COLOR_F{ 0, 0, 0, 0 };
		(void)TrySetPropertyValue(L"Background", BindingValue(back),
			DependencyPropertyValueSource::Theme);
	}
}

void TreeViewItem::ApplyExpansionPresentation()
{
	if (auto* host = GetItemsHost())
		cui::framework::TemplateAccess::SetPresentationSuppressed(
			*host, !_expanded);
	RequestLayout();
	InvalidateVisual();
}

void TreeViewItem::SetIsExpanded(bool value)
{
	if (value && !EnsureChildrenRealized()) return;
	(void)SetPropertyField(L"IsExpanded", _expanded, value);
}

void TreeViewItem::SetCurrentIsExpanded(bool value)
{
	if (value && !EnsureChildrenRealized()) return;
	(void)SetCurrentPropertyField(L"IsExpanded", _expanded, value);
}

void TreeViewItem::SetIsSelected(bool value)
{
	if (!SetPropertyField(L"IsSelected", _selected, value)) return;
	if (_owner)
	{
		if (value) (void)_owner->SelectItem(this, true);
		else if (_owner->GetSelectedContainer() == this)
			(void)_owner->SelectItem(nullptr, false);
		return;
	}
}

void TreeViewItem::SetCurrentIsSelected(bool value)
{
	(void)SetCurrentPropertyField(L"IsSelected", _selected, value);
}

std::unique_ptr<Control> TreeViewItem::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t,
	BindingPathObservation& observation)
{
	observation = {};
	if (!_owner)
	{
		SetLastTemplateError(L"TreeViewItem 尚未连接所属 TreeView。");
		return {};
	}
	for (auto* ancestor = this; ancestor; ancestor = ancestor->_parentItem)
	{
		if (!ancestor->_dataItem || ancestor->_dataItem.Get() != item.Get())
			continue;
		auto error = std::wstring(L"TreeView 数据层级不能形成循环。");
		SetLastTemplateError(error);
		_owner->SetHierarchyError(std::move(error));
		return {};
	}
	auto source = GetMaterializedItemsSource();
	std::wstring error;
	auto container = _owner->CreateGeneratedContainer(
		source, item, GetItemTemplate(), GetDisplayMemberPath(),
		GetItemContainerStyle(),
		_level + 1, &error);
	if (!container)
	{
		SetLastTemplateError(error);
		_owner->SetHierarchyError(std::move(error));
	}
	else _owner->SetHierarchyError({});
	return container;
}

void TreeViewItem::OnBeforeGeneratedItemsRebuilt()
{
	if (_owner && !_initializingGeneratedContainer)
		_owner->PrepareHierarchyMutation();
}

void TreeViewItem::OnGeneratedItemsRebuilt()
{
	SyncHasItems();
	if (_owner && !_initializingGeneratedContainer)
		_owner->CompleteHierarchyMutation();
}

void TreeViewItem::OnGeneratedItemsRealized()
{
	SyncHasItems();
	if (_owner && !_initializingGeneratedContainer)
		_owner->RefreshHierarchy();
}

void TreeViewItem::OnAuthoredItemsChanged() noexcept
{
	SyncHasItems();
	if (_owner && !_initializingGeneratedContainer)
		_owner->RefreshHierarchy();
}

void TreeViewItem::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	auto* render = GetDrawingContext();
	auto* header = GetHeaderVisual();
	const auto itemSize = GetActualSizeDip();
	const auto headerLocation = header
		? header->GetActualLocationDip() : cui::core::Point{};
	const auto headerSize = header
		? header->GetActualSizeDip()
		: cui::core::Size{ itemSize.width, DefaultRowHeight };
	const float rowHeight = (std::max)(DefaultRowHeight,
		headerLocation.y + headerSize.height);

	if (!GetControlTemplateRoot())
	{
		BeginRender();
		if (RendererBackgroundColor.a > 0.0f)
			render->FillRoundRect(
				0.0f, 1.0f, itemSize.width, (std::max)(0.0f, rowHeight - 2.0f),
				RendererBackgroundColor, 5.0f);
		if (_hasItems)
			DrawChevron(*render, DefaultChevronSlot * 0.5f,
				rowHeight * 0.5f, _expanded, RendererForegroundColor);
		if (_selected)
			render->FillRoundRect(0.0f, 5.0f, 3.0f,
				(std::max)(6.0f, rowHeight - 10.0f),
				cui::theme::palette::Accent, 1.5f);
		EndRender();
	}
}

TreeView::TreeView()
	: ItemsControl()
{
	EnsureBindingPropertiesRegistered();
	(void)TrySetPropertyValue(
		L"Focusable", BindingValue(true),
		DependencyPropertyValueSource::Theme);
	RendererBackgroundColor = cui::theme::palette::Surface;
	RendererBorderColor = cui::theme::palette::Border;
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(Thickness(1.0f)),
		DependencyPropertyValueSource::Theme);
}

TreeView::~TreeView()
{
	for (const auto& reference : _boundContainers)
		if (auto* item = dynamic_cast<TreeViewItem*>(reference.Get()))
			item->UnbindHierarchy();
	_boundContainers.clear();
}

void TreeView::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyOptions<TreeView, std::wstring> pathOptions;
		pathOptions.DefaultValue = std::wstring{};
		pathOptions.Design.Category = L"Data";
		pathOptions.Design.CategoryOrder = 80;
		pathOptions.Design.Order = 40;
		pathOptions.Design.Editor = DependencyPropertyEditorKind::Text;
		pathOptions.Design.Persistence = DependencyPropertyPersistence::Metadata;
		DependencyPropertyRegistry::Register<TreeView, std::wstring>(
			L"SelectedValuePath",
			[](TreeView& target) { return target.GetSelectedValuePath(); },
			[](TreeView& target, const std::wstring& value)
			{ target.SetSelectedValuePath(value); }, {}, std::move(pathOptions));

		auto projectionOptions = [](int order)
		{
			DependencyPropertyOptions<TreeView, BindingValue> options;
			options.DefaultValue = BindingValue{};
			options.Design.Category = L"Data";
			options.Design.CategoryOrder = 80;
			options.Design.Order = order;
			options.Design.Browsable = false;
			options.Design.Persistence = DependencyPropertyPersistence::Transient;
			options.IsReadOnly = true;
			return options;
		};
		DependencyPropertyRegistry::Register<TreeView, BindingValue>(
			L"SelectedItem",
			[](TreeView& target) { return target.GetSelectedItem(); }, {},
			[](TreeView& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedItemChanged.Subscribe(
					[handler = std::move(handler)](TreeView*) { handler(); });
			}, projectionOptions(50));
		DependencyPropertyRegistry::Register<TreeView, BindingValue>(
			L"SelectedValue",
			[](TreeView& target) { return target.GetSelectedValue(); }, {},
			[](TreeView& target,
				DependencyPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode)
			{
				return target._selectedValueChanged.Subscribe(
					[handler = std::move(handler)](TreeView*) { handler(); });
			}, projectionOptions(60));
		return true;
	}();
	(void)registered;
}

bool TreeView::ValidateAuthoredItemControl(
	const Control& item, std::string& error) const
{
	if (dynamic_cast<const TreeViewItem*>(&item)) return true;
	error = "TreeView authored Items must be TreeViewItem controls";
	return false;
}

ItemTemplateReference TreeView::ResolveDataItemTemplate(
	const BindingListReference& source,
	const ItemTemplateReference& localTemplate,
	int level,
	std::wstring* outError) const
{
	if (outError) outError->clear();
	if (!source) return {};
	const auto& itemType = source.Get()->ItemTypeName();
	ItemTemplateReference result = localTemplate;
	if (!result && level != 0)
	{
		const auto rootTemplate = GetItemTemplate();
		if (rootTemplate && (itemType.empty()
			|| rootTemplate.Get()->DataTypeName().empty()
			|| EqualsTypeName(
				rootTemplate.Get()->DataTypeName(), itemType)))
			result = rootTemplate;
	}
	if (!result && _implicitItemTemplateResolver)
		result = _implicitItemTemplateResolver(itemType);
	if (result && !itemType.empty()
		&& !result.Get()->DataTypeName().empty()
		&& !EqualsTypeName(result.Get()->DataTypeName(), itemType))
	{
		if (outError) *outError =
			L"TreeView ItemTemplate DataType 与 ItemsSource ItemType 不一致："
			+ itemType;
		return {};
	}
	return result;
}

std::unique_ptr<TreeViewItem> TreeView::CreateGeneratedContainer(
	const BindingListReference& source,
	const BindingSourceReference& item,
	const ItemTemplateReference& localTemplate,
	const std::wstring& displayMemberPath,
	const std::wstring& containerStyle,
	int level,
	std::wstring* outError)
{
	if (outError) outError->clear();
	std::unique_ptr<TreeViewItem> container;
	if (_itemContainerTemplate)
	{
		if (_itemContainerTemplate.Get()->TargetType()
			!= UIClass::UI_TreeViewItem)
		{
			if (outError) *outError =
				L"ItemContainerStyle.Template TargetType 必须是 TreeViewItem。";
			return {};
		}
		std::wstring error;
		auto built = _itemContainerTemplate.Get()->Build(&error);
		auto* typed = dynamic_cast<TreeViewItem*>(built.get());
		if (!typed)
		{
			if (outError) *outError = error.empty()
				? L"ItemContainerStyle.Template 未生成 TreeViewItem。"
				: std::move(error);
			return {};
		}
		container.reset(static_cast<TreeViewItem*>(built.release()));
	}
	else container = std::make_unique<TreeViewItem>();

	std::wstring templateError;
	auto itemTemplate = ResolveDataItemTemplate(
		source, localTemplate, level, &templateError);
	if (!templateError.empty())
	{
		if (outError) *outError = std::move(templateError);
		return {};
	}
	cui::framework::StyleAccess::SetResourceKey(
		*container, containerStyle);
	if (!container->InitializeGenerated(
		*this, item, std::move(itemTemplate), displayMemberPath,
		level, outError)) return {};
	return container;
}

std::unique_ptr<Control> TreeView::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t,
	BindingPathObservation& observation)
{
	observation = {};
	std::wstring error;
	auto container = CreateGeneratedContainer(
		GetItemsSource(), item, GetItemTemplate(), GetDisplayMemberPath(),
		GetItemContainerStyle(), 0, &error);
	if (!container) SetLastTemplateError(std::move(error));
	return container;
}

void TreeView::SetItemsSource(BindingListReference value)
{
	ItemsControl::SetItemsSource(std::move(value));
	RefreshHierarchy();
}

void TreeView::SetItemTemplate(ItemTemplateReference value)
{
	ItemsControl::SetItemTemplate(std::move(value));
	RebuildAuthoredDataDescendants();
	RefreshHierarchy();
}

void TreeView::SetDisplayMemberPath(std::wstring value)
{
	ItemsControl::SetDisplayMemberPath(std::move(value));
	RebuildAuthoredDataDescendants();
	RefreshHierarchy();
}

void TreeView::SetImplicitItemTemplateResolver(
	ImplicitItemTemplateResolver value)
{
	_implicitItemTemplateResolver = std::move(value);
	if (GetItemsSource()) (void)RebuildGeneratedItems();
	RebuildAuthoredDataDescendants();
	RefreshHierarchy();
}

void TreeView::SetItemContainerTemplate(ControlTemplateReference value)
{
	if (_itemContainerTemplate == value) return;
	auto previous = _itemContainerTemplate;
	_itemContainerTemplate = std::move(value);
	if ((!GetItemsSource() || RebuildGeneratedItems()))
	{
		RebuildAuthoredDataDescendants();
		RefreshHierarchy();
		return;
	}
	const auto error = LastTemplateError();
	_itemContainerTemplate = std::move(previous);
	(void)RebuildGeneratedItems();
	RebuildAuthoredDataDescendants();
	SetLastTemplateError(error);
}

TreeViewItem* TreeView::ContainerFromIndex(size_t index) const noexcept
{
	return dynamic_cast<TreeViewItem*>(ItemsControl::GetGeneratedItem(index));
}

void TreeView::PrepareHierarchyMutation()
{
	if (_hierarchyMutationDepth++ != 0) return;
	_selectionRestoreIdentity = {};
	_selectionRestoreAuthored = nullptr;
	if (!_selectedContainer) return;
	if (_selectedContainer->_dataItem)
		_selectionRestoreIdentity = _selectedContainer->_dataItem;
	else _selectionRestoreAuthored = ControlWeakReference(_selectedContainer);
}

void TreeView::CompleteHierarchyMutation()
{
	if (_hierarchyMutationDepth == 0)
	{
		RefreshHierarchy();
		return;
	}
	if (--_hierarchyMutationDepth == 0) RefreshHierarchy(true);
}

void TreeView::CollectContainers(
	ItemsControl& owner,
	TreeViewItem* parent,
	int level,
	std::vector<TreeViewItem*>& output)
{
	for (size_t index = 0; index < owner.ItemCount(); ++index)
	{
		auto* item = parent
			? parent->ContainerFromIndex(index)
			: dynamic_cast<TreeViewItem*>(owner.GetGeneratedItem(index));
		if (!item) continue;
		item->BindHierarchy(*this, parent, level);
		output.push_back(item);
		CollectContainers(*item, item, level + 1, output);
	}
}

void TreeView::RefreshHierarchy(bool restoreSelection)
{
	TreeViewItem* previousLive = nullptr;
	for (const auto& reference : _boundContainers)
	{
		auto* item = dynamic_cast<TreeViewItem*>(reference.Get());
		if (!item) continue;
		if (item == _selectedContainer) previousLive = item;
		item->UnbindHierarchy();
	}
	_boundContainers.clear();

	std::vector<TreeViewItem*> containers;
	CollectContainers(*this, nullptr, 0, containers);
	_boundContainers.reserve(containers.size());
	for (auto* item : containers) _boundContainers.emplace_back(item);

	auto contains = [&](const TreeViewItem* candidate)
	{
		return candidate && std::find(
			containers.begin(), containers.end(), candidate) != containers.end();
	};
	TreeViewItem* candidate = contains(_selectedContainer)
		? _selectedContainer : nullptr;
	if (!candidate && restoreSelection && _selectionRestoreIdentity)
	{
		const auto identity = _selectionRestoreIdentity.Get();
		const auto found = std::find_if(
			containers.begin(), containers.end(),
			[identity](const TreeViewItem* item)
			{ return item && item->_dataItem.Get() == identity; });
		if (found != containers.end()) candidate = *found;
	}
	if (!candidate && restoreSelection)
	{
		auto* authored = dynamic_cast<TreeViewItem*>(
			_selectionRestoreAuthored.Get());
		if (contains(authored)) candidate = authored;
	}
	if (!candidate)
	{
		const auto selected = std::find_if(
			containers.begin(), containers.end(),
			[](const TreeViewItem* item)
			{ return item && item->GetIsSelected(); });
		if (selected != containers.end()) candidate = *selected;
	}

	const auto previousRaw = _selectedContainer;
	const BindingValue previousItem = GetSelectedItem();
	_selectedContainer = candidate;
	for (auto* item : containers)
		item->SetCurrentIsSelected(item == candidate);
	if (previousLive && previousLive != candidate
		&& !contains(previousLive)) previousLive->SetCurrentIsSelected(false);

	_selectionRestoreIdentity = {};
	_selectionRestoreAuthored = nullptr;
	if (previousRaw != candidate)
	{
		RoutedPropertyChangedEventArgs<BindingValue> args(
			previousItem, GetSelectedItem());
		SelectedItemChanged(this, args);
		NotifySelectionProjectionChanged(true);
	}
	else RefreshSelectedItemObservation();
	InvalidateVisual();
}

bool TreeView::ContainsContainer(const TreeViewItem* item) const noexcept
{
	return item && item->_owner == this
		&& std::any_of(_boundContainers.begin(), _boundContainers.end(),
			[item](const ControlWeakReference& reference)
			{ return reference.Get() == item; });
}

void TreeView::CollectVisibleContainers(std::vector<TreeViewItem*>& output)
{
	std::function<void(ItemsControl&, TreeViewItem*)> collect =
		[&](ItemsControl& owner, TreeViewItem* parent)
		{
			for (size_t index = 0; index < owner.ItemCount(); ++index)
			{
				auto* item = parent
					? parent->ContainerFromIndex(index)
					: dynamic_cast<TreeViewItem*>(owner.GetGeneratedItem(index));
				if (!item || !item->IsVisible) continue;
				output.push_back(item);
				if (item->GetIsExpanded()) collect(*item, item);
			}
		};
	collect(*this, nullptr);
}

BindingValue TreeView::GetSelectedItem() const
{
	if (!_selectedContainer) return {};
	if (_selectedContainer->_dataItem)
		return BindingValue(_selectedContainer->_dataItem);
	return BindingValue(static_cast<Control*>(_selectedContainer));
}

BindingValue TreeView::GetSelectedValue() const
{
	if (!_selectedContainer) return {};
	if (_selectedValuePath.empty()) return GetSelectedItem();
	BindingValue result;
	if (_selectedContainer->_dataItem)
		return TryGetBindingPathValue(
			*_selectedContainer->_dataItem.Get(), _selectedValuePath, result)
			? result : BindingValue{};
	return _selectedContainer->TryGetPropertyValue(_selectedValuePath, result)
		? result : BindingValue{};
}

void TreeView::SetSelectedValuePath(std::wstring value)
{
	if (_selectedValuePath == value) return;
	_selectedValuePath = std::move(value);
	RefreshSelectedItemObservation();
	cui::framework::EventAccess::Raise(_selectedValueChanged, this);
}

void TreeView::RefreshSelectedItemObservation()
{
	_selectedItemObservation = {};
	if (!_selectedContainer || !_selectedContainer->_dataItem
		|| _selectedValuePath.empty()) return;
	_selectedItemObservation = ObserveBindingPaths(
		_selectedContainer->_dataItem, { _selectedValuePath },
		[this]
		{
			RefreshSelectedItemObservation();
			cui::framework::EventAccess::Raise(_selectedValueChanged, this);
		});
}

void TreeView::NotifySelectionProjectionChanged(bool itemChanged)
{
	RefreshSelectedItemObservation();
	if (itemChanged)
		cui::framework::EventAccess::Raise(_selectedItemChanged, this);
	cui::framework::EventAccess::Raise(_selectedValueChanged, this);
}

bool TreeView::BringItemIntoView(
	TreeViewItem& item, bool expandAncestors)
{
	if (!ContainsContainer(&item)) return false;
	if (expandAncestors)
		for (auto* parent = item._parentItem; parent;
			parent = parent->_parentItem) parent->SetCurrentIsExpanded(true);
	UpdateLayout();
	for (auto* current = item.GetVisualParent(); current;
		current = current->GetVisualParent())
		if (auto* scroll = dynamic_cast<ScrollViewer*>(current))
			return scroll->BringDescendantIntoView(&item);
	return true;
}

bool TreeView::ApplySelection(TreeViewItem* item, bool bringIntoView)
{
	if (item && !ContainsContainer(item)) return false;
	if (_selectedContainer == item)
	{
		if (item && bringIntoView)
			(void)BringItemIntoView(*item, true);
		return false;
	}
	auto* previous = _selectedContainer;
	const BindingValue previousItem = GetSelectedItem();
	_selectedContainer = item;
	if (previous) previous->SetCurrentIsSelected(false);
	if (item)
	{
		item->SetCurrentIsSelected(true);
		if (bringIntoView) (void)BringItemIntoView(*item, true);
	}
	RoutedPropertyChangedEventArgs<BindingValue> args(
		previousItem, GetSelectedItem());
	SelectedItemChanged(this, args);
	NotifySelectionProjectionChanged(true);
	InvalidateVisual();
	return true;
}

bool TreeView::SelectItem(TreeViewItem* item, bool bringIntoView)
{
	return ApplySelection(item, bringIntoView);
}

void TreeView::UpdateHover(TreeViewItem* item)
{
	if (item && !ContainsContainer(item)) item = nullptr;
	if (_hoveredContainer == item) return;
	_hoveredContainer = item;
}

TreeViewItem* TreeView::HitTestItem(
	float localX, float localY, float* relativeRowY)
{
	if (relativeRowY) *relativeRowY = 0.5f;
	const auto treeBounds = GetAbsoluteBoundsDip();
	const float absoluteX = treeBounds.left + localX;
	const float absoluteY = treeBounds.top + localY;
	std::vector<TreeViewItem*> visible;
	CollectVisibleContainers(visible);
	for (auto current = visible.rbegin(); current != visible.rend(); ++current)
	{
		auto* item = *current;
		if (!item) continue;
		auto* header = item->GetHeaderVisual();
		const auto bounds = header
			? header->GetAbsoluteBoundsDip() : item->GetAbsoluteBoundsDip();
		if (!PointInRect(absoluteX, absoluteY, bounds)) continue;
		if (relativeRowY)
		{
			const float height = bounds.bottom - bounds.top;
			*relativeRowY = height > 0.0f
				? (std::clamp)((absoluteY - bounds.top) / height, 0.0f, 1.0f)
				: 0.5f;
		}
		return item;
	}
	return nullptr;
}

void TreeView::SetDropTarget(
	TreeViewItem* item, TreeViewDropPosition position)
{
	if (item && !ContainsContainer(item)) item = nullptr;
	if (!item) position = TreeViewDropPosition::None;
	if (_dropTarget == item && _dropPosition == position) return;
	_dropTarget = item;
	_dropPosition = position;
	InvalidateVisual();
}

void TreeView::ClearDropTarget()
{
	SetDropTarget(nullptr, TreeViewDropPosition::None);
}

bool TreeView::HandlesNavigationKey(Key key) const
{
	switch (key)
	{
	case Key::Up:
	case Key::Down:
	case Key::Left:
	case Key::Right:
	case Key::Home:
	case Key::End:
	case Key::PageUp:
	case Key::PageDown:
		return true;
	default:
		return false;
	}
}

bool TreeView::ProcessInput(const InputReport& input)
{
	if (!IsEnabled || !IsVisible) return true;
	bool handled = false;
	if (input.Kind == InputReportKind::PointerMove)
		UpdateHover(HitTestItem(
			static_cast<float>(input.X), static_cast<float>(input.Y)));
	else if (input.Kind == InputReportKind::PointerLeave)
		UpdateHover(nullptr);
	else if (input.Kind == InputReportKind::PointerDown
		&& input.ChangedButton == MouseButton::Left)
	{
		auto* item = HitTestItem(
			static_cast<float>(input.X), static_cast<float>(input.Y));
		if (item)
		{
			if (GetPresentationWindow()) GetPresentationWindow()->SetKeyboardFocus(this, true);
			const auto treeBounds = GetAbsoluteBoundsDip();
			auto* header = item->GetHeaderVisual();
			const auto headerBounds = header
				? header->GetAbsoluteBoundsDip() : item->GetAbsoluteBoundsDip();
			const float absoluteX = treeBounds.left + input.X;
			if (item->GetHasItems()
				&& absoluteX <= headerBounds.left + DefaultChevronSlot)
				item->SetCurrentIsExpanded(!item->GetIsExpanded());
			else (void)ApplySelection(item, true);
			handled = true;
		}
	}
	else if (input.Kind == InputReportKind::KeyDown)
	{
		std::vector<TreeViewItem*> visible;
		CollectVisibleContainers(visible);
		if (!visible.empty())
		{
			auto current = std::find(
				visible.begin(), visible.end(), _selectedContainer);
			int index = current == visible.end()
				? -1 : static_cast<int>(current - visible.begin());
			TreeViewItem* next = _selectedContainer;
			switch (input.Key)
			{
			case Key::Up:
				next = visible[static_cast<size_t>((std::max)(0, index - 1))];
				break;
			case Key::Down:
				next = visible[static_cast<size_t>((std::min)(
					static_cast<int>(visible.size()) - 1, index + 1))];
				break;
			case Key::Home: next = visible.front(); break;
			case Key::End: next = visible.back(); break;
			case Key::PageUp:
				next = visible[static_cast<size_t>((std::max)(0, index - 8))];
				break;
			case Key::PageDown:
				next = visible[static_cast<size_t>((std::min)(
					static_cast<int>(visible.size()) - 1, index + 8))];
				break;
			case Key::Left:
				if (_selectedContainer && _selectedContainer->GetIsExpanded())
					_selectedContainer->SetCurrentIsExpanded(false);
				else if (_selectedContainer)
					next = _selectedContainer->_parentItem;
				break;
			case Key::Right:
				if (_selectedContainer && _selectedContainer->GetHasItems()
					&& !_selectedContainer->GetIsExpanded())
					_selectedContainer->SetCurrentIsExpanded(true);
				else if (_selectedContainer && _selectedContainer->GetIsExpanded())
					next = _selectedContainer->ContainerFromIndex(0);
				break;
			default: next = nullptr; break;
			}
			if (next) (void)ApplySelection(next, true);
			handled = HandlesNavigationKey(input.Key);
		}
	}
	(void)Control::ProcessInput(input);
	return handled;
}

bool TreeView::ApplyItemContainerStyle()
{
	return ItemsControl::ApplyItemContainerStyle();
}

void TreeView::OnBeforeGeneratedItemsRebuilt()
{
	PrepareHierarchyMutation();
}

void TreeView::OnGeneratedItemsRebuilt()
{
	CompleteHierarchyMutation();
}

void TreeView::OnGeneratedItemsRealized()
{
	RefreshHierarchy();
}

void TreeView::OnAuthoredItemsChanged() noexcept
{
	RefreshHierarchy();
	(void)ApplyItemContainerStyle();
}

void TreeView::RebuildAuthoredDataDescendants()
{
	std::function<void(ItemsControl&)> rebuild = [&](ItemsControl& owner)
	{
		for (size_t index = 0; index < owner.AuthoredItemCount(); ++index)
		{
			auto* item = dynamic_cast<TreeViewItem*>(
				owner.GetAuthoredItem(index));
			if (!item) continue;
			if (item->GetMaterializedItemsSource())
				(void)item->RebuildGeneratedItems();
			else rebuild(*item);
		}
	};
	rebuild(*this);
}

void TreeView::DrawDropIndicator()
{
	if (!_dropTarget || _dropPosition == TreeViewDropPosition::None
		|| !ContainsContainer(_dropTarget)
		|| !GetPresentationWindow() || !GetDrawingContext()) return;
	auto* header = _dropTarget->GetHeaderVisual();
	const auto target = header
		? header->GetAbsoluteBoundsDip() : _dropTarget->GetAbsoluteBoundsDip();
	const auto tree = GetAbsoluteBoundsDip();
	const float left = (std::max)(0.0f, target.left - tree.left);
	const float top = (std::max)(0.0f, target.top - tree.top);
	const float right = (std::min)(
		GetActualSizeDip().width, target.right - tree.left);
	const float bottom = (std::min)(
		GetActualSizeDip().height, target.bottom - tree.top);
	if (right <= left || bottom <= top) return;
	auto* render = GetDrawingContext();
	const auto accent = cui::theme::palette::Accent;
	if (_dropPosition == TreeViewDropPosition::Inside)
		render->DrawRoundRect(left, top, right - left, bottom - top,
			accent, 1.5f, 4.0f);
	else
	{
		const float y = _dropPosition == TreeViewDropPosition::Before
			? top + 1.0f : bottom - 1.0f;
		render->DrawLine(left, y, right, y, accent, 2.0f);
	}
}

void TreeView::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	const auto size = GetActualSizeDip();
	BeginRender();
	if (RendererBackgroundColor.a > 0.0f)
		GetDrawingContext()->FillRect(0.0f, 0.0f,
			size.width, size.height, RendererBackgroundColor);
	EndRender();
	BeginRender();
	DrawDropIndicator();
	const float border = BorderThickness.MaxEdge();
	if (border > 0.0f && RendererBorderColor.a > 0.0f)
		GetDrawingContext()->DrawRect(
			0.0f, 0.0f, size.width, size.height,
			RendererBorderColor, border);
	EndRender();
}
