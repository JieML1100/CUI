#include "ItemsControl.h"

#include "Form.h"
#include "Label.h"
#include "Layout/StackPanel.h"
#include "Layout/WrapPanel.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <limits>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
	bool EqualsIgnoreCase(const std::wstring& left, const std::wstring& right)
	{
		if (left.size() != right.size()) return false;
		for (size_t index = 0; index < left.size(); ++index)
			if (std::towlower(left[index]) != std::towlower(right[index]))
				return false;
		return true;
	}

	template<typename TValue>
	ControlPropertyOptions<ItemsControl, TValue> DataOptions(
		TValue defaultValue,
		int order,
		ControlPropertyPersistence persistence = ControlPropertyPersistence::Metadata)
	{
		ControlPropertyOptions<ItemsControl, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = ControlPropertyFlags::AffectsMeasure
			| ControlPropertyFlags::AffectsArrange
			| ControlPropertyFlags::AffectsRender;
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = order;
		options.Design.Editor = ControlPropertyEditorKind::Auto;
		options.Design.Persistence = persistence;
		return options;
	}

	const ItemsPanelTemplate& DefaultItemsPanel() noexcept
	{
		static const ItemsPanelTemplate value{};
		return value;
	}

	bool IsFiniteNonNegative(float value) noexcept
	{
		return std::isfinite(value) && value >= 0.0f;
	}

	bool IsValidItemsPanel(const ItemsPanelTemplate& value) noexcept
	{
		if (!IsFiniteNonNegative(value.Spacing)
			|| !IsFiniteNonNegative(value.ItemWidth)
			|| !IsFiniteNonNegative(value.ItemHeight)
			|| !IsFiniteNonNegative(value.CacheLength)) return false;
		if (value.Kind == ItemsPanelKind::VirtualizingStack)
			return value.Orientation == Orientation::Vertical
				&& value.ItemHeight > 0.0f;
		return true;
	}

	class VirtualizingItemsHost;

	class VirtualizingStackLayoutEngine final : public LayoutEngine
	{
	public:
		explicit VirtualizingStackLayoutEngine(VirtualizingItemsHost& owner)
			: _owner(owner) {}
		cui::core::Size Measure(
			LayoutContext& context,
			const cui::core::Constraints& available) override;
		void Arrange(LayoutContext& context, D2D1_RECT_F finalRect) override;

	private:
		VirtualizingItemsHost& _owner;
	};

	class VirtualizingItemsHost final : public Panel
	{
	public:
		VirtualizingItemsHost()
		{
			SetLayoutEngine(new VirtualizingStackLayoutEngine(*this));
			SetAutoSize(true, true);
			HAlign = HorizontalAlignment::Stretch;
			VAlign = VerticalAlignment::Top;
			BorderThickness = 0.0f;
		}

		void SetConfiguration(
			size_t itemCount,
			float itemHeight,
			float spacing,
			std::vector<size_t> headerCounts,
			float headerHeight,
			float headerSpacing)
		{
			if (_itemCount == itemCount && _itemHeight == itemHeight
				&& _spacing == spacing && _headerCounts == headerCounts
				&& _headerHeight == headerHeight
				&& _headerSpacing == headerSpacing) return;
			_itemCount = itemCount;
			_itemHeight = itemHeight;
			_spacing = spacing;
			_headerCounts = std::move(headerCounts);
			_headerHeight = headerHeight;
			_headerSpacing = headerSpacing;
			_offsets.assign(_itemCount + 1, 0.0f);
			for (size_t index = 0; index < _itemCount; ++index)
			{
				const size_t headerCount = index < _headerCounts.size()
					? _headerCounts[index] : 0;
				const float extent = _itemHeight
					+ static_cast<float>(headerCount)
						* (_headerHeight + _headerSpacing);
				_offsets[index + 1] = _offsets[index] + extent
					+ (index + 1 < _itemCount ? _spacing : 0.0f);
			}
			InvalidateLayout();
		}
		void RegisterItem(Control* control, size_t index)
		{
			if (!control) return;
			_indices[control] = index;
			InvalidateLayout();
		}
		void UnregisterItem(Control* control)
		{
			_indices.erase(control);
			InvalidateLayout();
		}
		size_t IndexOf(const Control* control) const noexcept
		{
			const auto found = _indices.find(const_cast<Control*>(control));
			return found == _indices.end()
				? (std::numeric_limits<size_t>::max)() : found->second;
		}
		float ItemHeight() const noexcept { return _itemHeight; }
		float Spacing() const noexcept { return _spacing; }
		float ItemTop(size_t index) const noexcept
		{
			return index < _offsets.size() ? _offsets[index] : ContentHeight();
		}
		float ItemExtent(size_t index) const noexcept
		{
			if (index >= _itemCount || index + 1 >= _offsets.size()) return 0.0f;
			return _offsets[index + 1] - _offsets[index]
				- (index + 1 < _itemCount ? _spacing : 0.0f);
		}
		size_t IndexAtOffset(float offset) const noexcept
		{
			if (_itemCount == 0) return 0;
			offset = (std::clamp)(offset, 0.0f,
				(std::max)(0.0f, ContentHeight() - 0.0001f));
			const auto found = std::upper_bound(
				_offsets.begin(), _offsets.end(), offset);
			return (std::min)(_itemCount - 1,
				found == _offsets.begin() ? size_t{ 0 }
					: static_cast<size_t>(std::distance(
						_offsets.begin(), found) - 1));
		}
		float ContentHeight() const noexcept
		{
			return _offsets.empty() ? 0.0f : _offsets.back();
		}

	private:
		size_t _itemCount = 0;
		float _itemHeight = 0.0f;
		float _spacing = 0.0f;
		float _headerHeight = 0.0f;
		float _headerSpacing = 0.0f;
		std::vector<size_t> _headerCounts;
		std::vector<float> _offsets;
		std::unordered_map<Control*, size_t> _indices;
	};

	class GroupedItemHost final : public StackPanel
	{
	public:
		explicit GroupedItemHost(std::unique_ptr<Control> item)
			: StackPanel(0, 0, 0, 0)
		{
			SetOrientation(Orientation::Vertical);
			SetHorizontalContentAlignment(HorizontalAlignment::Stretch);
			SetVerticalContentAlignment(VerticalAlignment::Top);
			SetAutoSize(true, true);
			BorderThickness = 0.0f;
			_item = item.get();
			if (_item) _originalItemHeight = _item->GetLayoutHeight();
			AddOwned(std::move(item));
		}

		Control* Item() const noexcept { return _item; }
		std::unique_ptr<Control> TakeItem()
		{
			if (_item && _itemHeightOverridden)
				_item->SetLayoutHeight(_originalItemHeight);
			auto result = DetachControl(_item);
			_item = nullptr;
			return result;
		}
		void SetHeaders(
			std::vector<std::unique_ptr<Control>> headers,
			std::vector<BindingSourceReference> contexts,
			float spacing,
			float fixedHeaderHeight = 0.0f,
			float fixedItemHeight = 0.0f)
		{
			auto item = DetachControl(_item);
			while (!Children.empty())
			{
				auto discarded = DetachControl(Children.front());
				(void)discarded;
			}
			SetSpacing(spacing);
			_contexts = std::move(contexts);
			for (auto& header : headers)
			{
				if (fixedHeaderHeight > 0.0f)
					header->SetLayoutHeight(cui::layout::Length::Fixed(
						fixedHeaderHeight));
				AddOwned(std::move(header));
			}
			_item = item.get();
			if (_item && fixedItemHeight > 0.0f)
			{
				_item->SetLayoutHeight(cui::layout::Length::Fixed(fixedItemHeight));
				_itemHeightOverridden = true;
			}
			else if (_item && _itemHeightOverridden)
			{
				_item->SetLayoutHeight(_originalItemHeight);
				_itemHeightOverridden = false;
			}
			AddOwned(std::move(item));
		}

	private:
		Control* _item = nullptr;
		cui::layout::Length _originalItemHeight;
		bool _itemHeightOverridden = false;
		std::vector<BindingSourceReference> _contexts;
	};

	cui::core::Size VirtualizingStackLayoutEngine::Measure(
		LayoutContext& context,
		const cui::core::Constraints& available)
	{
		const auto maximum = available.Normalized().maximum;
		float desiredWidth = 0.0f;
		for (int index = 0; index < context.ChildCount(); ++index)
		{
			auto* child = context.ChildAt(index);
			if (!child || !child->Visible) continue;
			const auto margin = child->Margin;
			const float width = std::isfinite(maximum.width)
				? (std::max)(0.0f, maximum.width - margin.Left - margin.Right)
				: cui::core::Infinity;
			const auto itemIndex = _owner.IndexOf(child);
			if (itemIndex == (std::numeric_limits<size_t>::max)()) continue;
			const float height = (std::max)(0.0f,
				_owner.ItemExtent(itemIndex) - margin.Top - margin.Bottom);
			const auto desired = child->Measure(
				cui::core::Constraints{ cui::core::Size{ width, height } });
			desiredWidth = (std::max)(desiredWidth,
				desired.width + margin.Left + margin.Right);
		}
		_needsLayout = false;
		return cui::core::Size{ desiredWidth, _owner.ContentHeight() };
	}

	void VirtualizingStackLayoutEngine::Arrange(
		LayoutContext& context,
		D2D1_RECT_F finalRect)
	{
		const float width = (std::max)(0.0f, finalRect.right - finalRect.left);
		for (int childIndex = 0; childIndex < context.ChildCount(); ++childIndex)
		{
			auto* child = context.ChildAt(childIndex);
			if (!child || !child->Visible) continue;
			const auto itemIndex = _owner.IndexOf(child);
			if (itemIndex == (std::numeric_limits<size_t>::max)()) continue;
			const auto margin = child->Margin;
			const float cellTop = _owner.ItemTop(itemIndex);
			child->ApplyLayout(cui::core::Rect{
				finalRect.left + margin.Left,
				finalRect.top + cellTop + margin.Top,
				(std::max)(0.0f, width - margin.Left - margin.Right),
				(std::max)(0.0f,
					_owner.ItemExtent(itemIndex) - margin.Top - margin.Bottom) });
		}
		_needsLayout = false;
	}
}

ItemsControl::ItemsControl(int x, int y, int width, int height)
	: ScrollView(x, y, width, height)
{
	AlwaysShowHScroll = false;
	auto itemsHost = CreateItemsHost();
	_itemsHost = itemsHost.get();
	_changingItemsHost = true;
	AddOwned(std::move(itemsHost));
	_changingItemsHost = false;
	RefreshItemsScrollOwner();
}

bool ItemsControl::ValidateChildCollection(
	std::span<Control* const> children,
	std::string& error) const
{
	if (_changingItemsHost || _changingTemplateInfrastructure) return true;
	if (_controlTemplateRoot && children.size() == 1
		&& children.front() == _controlTemplateRoot)
		return true;
	if (!_controlTemplateRoot && _itemsHost && children.size() == 1
		&& children.front() == _itemsHost)
		return true;
	error = "ItemsControl direct children are owned by its ControlTemplate or ItemsPanelTemplate host";
	return false;
}

void ItemsControl::EnsureBindingPropertiesRegistered()
{
	ScrollView::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto sourceOptions = DataOptions(
			BindingListReference{}, 10,
			ControlPropertyPersistence::Transient);
		sourceOptions.Design.Browsable = false;
		BindingPropertyRegistry::Register<ItemsControl, BindingListReference>(
			L"ItemsSource",
			[](ItemsControl& target) { return target.GetItemsSource(); },
			[](ItemsControl& target, const BindingListReference& value)
			{ target.SetItemsSource(value); }, {}, std::move(sourceOptions));

		auto templateOptions = DataOptions(
			ItemTemplateReference{}, 20,
			ControlPropertyPersistence::Transient);
		templateOptions.Design.Browsable = false;
		BindingPropertyRegistry::Register<ItemsControl, ItemTemplateReference>(
			L"ItemTemplate",
			[](ItemsControl& target) { return target.GetItemTemplate(); },
			[](ItemsControl& target, const ItemTemplateReference& value)
			{ target.SetItemTemplate(value); }, {}, std::move(templateOptions));

		auto groupOptions = DataOptions(
			GroupStyleReference{}, 25,
			ControlPropertyPersistence::Transient);
		groupOptions.Design.Browsable = false;
		BindingPropertyRegistry::Register<ItemsControl, GroupStyleReference>(
			L"GroupStyle",
			[](ItemsControl& target) { return target.GetGroupStyle(); },
			[](ItemsControl& target, const GroupStyleReference& value)
			{ target.SetGroupStyle(value); }, {}, std::move(groupOptions));

		auto panelOptions = DataOptions(
			ItemsPanelTemplateReference{}, 30,
			ControlPropertyPersistence::Transient);
		panelOptions.Design.Browsable = false;
		BindingPropertyRegistry::Register<ItemsControl, ItemsPanelTemplateReference>(
			L"ItemsPanel",
			[](ItemsControl& target) { return target.GetItemsPanel(); },
			[](ItemsControl& target, const ItemsPanelTemplateReference& value)
			{ target.SetItemsPanel(value); }, {}, std::move(panelOptions));

		auto pathOptions = DataOptions(std::wstring{}, 40);
		pathOptions.Design.Editor = ControlPropertyEditorKind::Text;
		BindingPropertyRegistry::Register<ItemsControl, std::wstring>(
			L"DisplayMemberPath",
			[](ItemsControl& target) { return target.GetDisplayMemberPath(); },
			[](ItemsControl& target, const std::wstring& value)
			{ target.SetDisplayMemberPath(value); }, {}, std::move(pathOptions));
		return true;
	}();
	(void)registered;
}

const ItemsPanelTemplate& ItemsControl::EffectiveItemsPanel() const noexcept
{
	return _itemsPanel ? *_itemsPanel.Get() : DefaultItemsPanel();
}

bool ItemsControl::IsVirtualizing() const noexcept
{
	return EffectiveItemsPanel().Kind == ItemsPanelKind::VirtualizingStack;
}

void ItemsControl::ConfigureVirtualHost()
{
	auto* host = dynamic_cast<VirtualizingItemsHost*>(_itemsHost);
	if (!host) return;
	std::vector<size_t> headerCounts(ItemCount(), 0);
	float headerHeight = 0.0f;
	float headerSpacing = 0.0f;
	if (IsGroupingActive() && _groupStyle)
	{
		headerHeight = _groupStyle.Get()->HeaderHeight;
		headerSpacing = _groupStyle.Get()->HeaderSpacing;
		if (const auto* grouped = dynamic_cast<const IBindingListGroupView*>(
			_itemsSource.Get()))
			for (const auto& group : grouped->Groups())
				if (group.StartIndex < headerCounts.size())
					++headerCounts[group.StartIndex];
	}
	const auto& panel = EffectiveItemsPanel();
	host->SetConfiguration(ItemCount(), panel.ItemHeight, panel.Spacing,
		std::move(headerCounts), headerHeight, headerSpacing);
}

std::unique_ptr<Panel> ItemsControl::CreateItemsHost() const
{
	const auto& definition = EffectiveItemsPanel();
	std::unique_ptr<Panel> result;
	if (definition.Kind == ItemsPanelKind::Wrap)
	{
		auto panel = std::make_unique<WrapPanel>(0, 0, 0, 0);
		panel->SetOrientation(definition.Orientation);
		panel->SetItemWidth(definition.ItemWidth);
		panel->SetItemHeight(definition.ItemHeight);
		result = std::move(panel);
	}
	else if (definition.Kind == ItemsPanelKind::VirtualizingStack)
	{
		result = std::make_unique<VirtualizingItemsHost>();
	}
	else
	{
		auto panel = std::make_unique<StackPanel>(0, 0, 0, 0);
		panel->SetOrientation(definition.Orientation);
		panel->SetSpacing(definition.Spacing);
		panel->SetHorizontalContentAlignment(HorizontalAlignment::Stretch);
		panel->SetVerticalContentAlignment(VerticalAlignment::Top);
		result = std::move(panel);
	}
	result->SetAutoSize(true, true);
	result->HAlign = HorizontalAlignment::Stretch;
	result->VAlign = VerticalAlignment::Top;
	result->BorderThickness = 0.0f;
	return result;
}

std::unique_ptr<Panel> ItemsControl::TakeItemsHost()
{
	if (!_itemsHost) return {};
	if (_templateItemsPresenter
		&& _templateItemsPresenter->GetItemsHost() == _itemsHost)
		return _templateItemsPresenter->DetachItemsHost();
	if (_detachedItemsHost)
		return std::move(_detachedItemsHost);
	if (_itemsHost->Parent != this) return {};
	_changingItemsHost = true;
	std::unique_ptr<Control> detached;
	try
	{
		detached = DetachControl(_itemsHost);
		_changingItemsHost = false;
	}
	catch (...)
	{
		_changingItemsHost = false;
		throw;
	}
	return std::unique_ptr<Panel>(
		static_cast<Panel*>(detached.release()));
}

void ItemsControl::PlaceItemsHost(std::unique_ptr<Panel> host)
{
	if (!host) throw std::invalid_argument("ItemsControl ItemsHost is null");
	auto* raw = host.get();
	_itemsHost = raw;
	if (_templateItemsPresenter)
	{
		try
		{
			_templateItemsPresenter->SetItemsHost(std::move(host));
		}
		catch (...)
		{
			_itemsHost = _templateItemsPresenter->GetItemsHost() == raw
				? raw : nullptr;
			throw;
		}
	}
	else if (_controlTemplateRoot)
	{
		_detachedItemsHost = std::move(host);
	}
	else
	{
		_changingItemsHost = true;
		try
		{
			AddOwned(std::move(host));
			_changingItemsHost = false;
		}
		catch (...)
		{
			_changingItemsHost = false;
			_itemsHost = ContainsControl(raw) ? raw : nullptr;
			throw;
		}
	}
	RefreshItemsScrollOwner();
}

void ItemsControl::RefreshItemsScrollOwner()
{
	_scrollChanged.Disconnect();
	_itemsScrollOwner = nullptr;
	if (!_controlTemplateRoot)
	{
		_itemsScrollOwner = this;
	}
	else if (_templateItemsPresenter)
	{
		for (auto* current = _templateItemsPresenter->Parent;
			current && current != this; current = current->Parent)
		{
			if (auto* scroll = dynamic_cast<ScrollView*>(current))
			{
				_itemsScrollOwner = scroll;
				break;
			}
		}
	}
	if (_itemsScrollOwner)
		_scrollChanged = _itemsScrollOwner->OnScrollChanged.Subscribe(
			[this](Control*)
			{
				if (IsVirtualizing() && !_applyingCollectionChange)
					(void)RealizeVirtualViewport();
			});
}

bool ItemsControl::RegisterTemplateItemsPresenter(
	ItemsPresenter* presenter)
{
	if (!presenter || (_templateItemsPresenter
		&& _templateItemsPresenter != presenter)) return false;
	if (_templateItemsPresenter == presenter) return true;
	auto host = TakeItemsHost();
	if (!host) return false;
	_templateItemsPresenter = presenter;
	_itemsPresenterParentChanged = presenter->OnParentChanged.Subscribe(
		[this](Control*, Control*, Control*)
		{
			RefreshItemsScrollOwner();
			if (IsVirtualizing()) (void)RealizeVirtualViewport();
		});
	try
	{
		PlaceItemsHost(std::move(host));
		return true;
	}
	catch (...)
	{
		if (presenter->GetItemsHost() == _itemsHost)
		{
			RefreshItemsScrollOwner();
			throw;
		}
		_itemsPresenterParentChanged.Disconnect();
		_templateItemsPresenter = nullptr;
		throw;
	}
}

Control* ItemsControl::SetControlTemplateRoot(
	std::unique_ptr<Control> value)
{
	if (!value)
	{
		(void)DetachControlTemplateRoot();
		return nullptr;
	}
	if (_controlTemplateRoot)
		throw std::logic_error("ItemsControl already owns a ControlTemplate root");
	if (!_templateItemsPresenter)
	{
		auto host = TakeItemsHost();
		if (!host) throw std::logic_error("ItemsControl lost its ItemsHost");
		_detachedItemsHost = std::move(host);
	}
	value->HAlign = HorizontalAlignment::Stretch;
	value->VAlign = VerticalAlignment::Stretch;
	_controlTemplateRoot = value.get();
	_changingTemplateInfrastructure = true;
	try
	{
		AddOwned(std::move(value));
		_changingTemplateInfrastructure = false;
	}
	catch (...)
	{
		_changingTemplateInfrastructure = false;
		_controlTemplateRoot = nullptr;
		if (_detachedItemsHost)
			PlaceItemsHost(std::move(_detachedItemsHost));
		throw;
	}
	RefreshItemsScrollOwner();
	InvalidateLayout();
	InvalidateVisual();
	return _controlTemplateRoot;
}

std::unique_ptr<Control> ItemsControl::DetachControlTemplateRoot()
{
	if (!_controlTemplateRoot) return {};
	_changingTemplateInfrastructure = true;
	std::unique_ptr<Control> root;
	try
	{
		root = DetachControl(_controlTemplateRoot);
		_changingTemplateInfrastructure = false;
	}
	catch (...)
	{
		_changingTemplateInfrastructure = false;
		throw;
	}
	auto host = TakeItemsHost();
	_itemsPresenterParentChanged.Disconnect();
	_templateItemsPresenter = nullptr;
	_controlTemplateRoot = nullptr;
	if (host) PlaceItemsHost(std::move(host));
	InvalidateLayout();
	InvalidateVisual();
	return root;
}

void ItemsControl::SetItemsPanel(ItemsPanelTemplateReference value)
{
	if (_itemsPanel == value) return;
	const auto& definition = value ? *value.Get() : DefaultItemsPanel();
	if (!IsValidItemsPanel(definition))
	{
		_lastTemplateError = L"ItemsPanelTemplate 配置无效；VirtualizingStackPanel 需要正数 ItemHeight 且只支持 Vertical。";
		return;
	}
	(void)ReplaceItemsHost(std::move(value));
}

bool ItemsControl::ReplaceItemsHost(ItemsPanelTemplateReference value)
{
	const auto previousPanel = _itemsPanel;
	_itemsPanel = std::move(value);
	std::unique_ptr<Panel> replacement;
	try
	{
		replacement = CreateItemsHost();
	}
	catch (...)
	{
		_itemsPanel = previousPanel;
		_lastTemplateError = L"ItemsPanelTemplate 无法创建 ItemsHost。";
		return false;
	}

	auto oldGenerator = std::move(_generator);
	auto oldHost = TakeItemsHost();
	if (!oldHost)
	{
		_itemsPanel = previousPanel;
		_generator = std::move(oldGenerator);
		_lastTemplateError = L"ItemsHost 所有权无效。";
		return false;
	}
	PlaceItemsHost(std::move(replacement));

	if (RebuildGeneratedItems()) return true;
	const auto error = _lastTemplateError;
	auto failedHost = TakeItemsHost();
	_itemsPanel = previousPanel;
	PlaceItemsHost(std::move(oldHost));
	_generator = std::move(oldGenerator);
	_lastTemplateError = error;
	return false;
}

void ItemsControl::SetItemsSource(BindingListReference value)
{
	if (_itemsSource == value) return;
	const auto previous = _itemsSource;
	_itemsSourceChanged.Disconnect();
	_groupsChanged.Disconnect();
	_itemsSource = std::move(value);
	if (_itemsSource)
	{
		_itemsSourceChanged = _itemsSource.Get()->SubscribeChanged(
			[this](const CollectionChangedEventArgs& change)
			{
				if (!ApplyCollectionChange(change))
					(void)RebuildGeneratedItems();
			});
		if (auto* grouped = dynamic_cast<IBindingListGroupView*>(
			_itemsSource.Get()))
			_groupsChanged = grouped->SubscribeGroupsChanged(
				[this] { RefreshGroupHeaders(); });
	}
	if (RebuildGeneratedItems()) return;
	const auto error = _lastTemplateError;
	_itemsSourceChanged.Disconnect();
	_groupsChanged.Disconnect();
	_itemsSource = previous;
	if (_itemsSource)
	{
		_itemsSourceChanged = _itemsSource.Get()->SubscribeChanged(
			[this](const CollectionChangedEventArgs& change)
			{
				if (!ApplyCollectionChange(change))
					(void)RebuildGeneratedItems();
			});
		if (auto* grouped = dynamic_cast<IBindingListGroupView*>(
			_itemsSource.Get()))
			_groupsChanged = grouped->SubscribeGroupsChanged(
				[this] { RefreshGroupHeaders(); });
	}
	_lastTemplateError = error;
}

void ItemsControl::SetItemTemplate(ItemTemplateReference value)
{
	if (_itemTemplate == value) return;
	const auto previous = _itemTemplate;
	_itemTemplate = std::move(value);
	if (RebuildGeneratedItems()) return;
	const auto error = _lastTemplateError;
	_itemTemplate = previous;
	_lastTemplateError = error;
}

void ItemsControl::SetGroupStyle(GroupStyleReference value)
{
	if (_groupStyle == value) return;
	const auto previous = _groupStyle;
	_groupStyle = std::move(value);
	if (RebuildGeneratedItems()) return;
	const auto error = _lastTemplateError;
	_groupStyle = previous;
	(void)RebuildGeneratedItems();
	_lastTemplateError = error;
}

void ItemsControl::SetDisplayMemberPath(std::wstring value)
{
	if (_displayMemberPath == value) return;
	_displayMemberPath = std::move(value);
	if (_itemsSource && !_itemTemplate) (void)RebuildGeneratedItems();
}

Control* ItemsControl::UnwrapGeneratedItem(Control* visual) noexcept
{
	if (auto* grouped = dynamic_cast<GroupedItemHost*>(visual))
		return grouped->Item();
	return visual;
}

Control* ItemsControl::GetGeneratedItem(size_t index) const noexcept
{
	return UnwrapGeneratedItem(_generator.GetRealized(index));
}

bool ItemsControl::IsGroupingActive() const noexcept
{
	if (!_groupStyle || !_itemsSource) return false;
	const auto* grouped = dynamic_cast<const IBindingListGroupView*>(
		_itemsSource.Get());
	return grouped && !grouped->Groups().empty();
}

ItemsControl::PreparedGroupHeaders ItemsControl::BuildGroupHeaders(
	size_t index,
	const BindingSourceReference& item)
{
	PreparedGroupHeaders result;
	if (!IsGroupingActive()) return result;
	const auto* grouped = dynamic_cast<const IBindingListGroupView*>(
		_itemsSource.Get());
	if (!grouped) return result;
	for (const auto& group : grouped->Groups())
	{
		if (group.StartIndex != index) continue;
		auto groupItems = std::make_shared<ObservableBindingList>(
			_itemsSource.Get()->ItemTypeName());
		for (size_t offset = 0; offset < group.ItemCount; ++offset)
		{
			BindingSourceReference member;
			if (_itemsSource.Get()->TryGetItem(group.StartIndex + offset, member)
				&& member) groupItems->Items.push_back(std::move(member));
		}
		const bool isBottomLevel = std::none_of(
			grouped->Groups().begin(), grouped->Groups().end(),
			[&](const BindingListGroup& candidate)
			{
				return candidate.Level == group.Level + 1
					&& candidate.StartIndex >= group.StartIndex
					&& candidate.StartIndex < group.StartIndex + group.ItemCount;
			});
		auto context = std::make_shared<ObservableObject>();
		auto aggregates = std::make_shared<ObservableObject>();
		for (const auto& [name, value] : group.Aggregates)
			(void)aggregates->DefineProperty(name, value, true, false, true);
		(void)context->DefineProperty(L"Key", group.Key.ToString(), true, false, true);
		(void)context->DefineProperty(L"Name", group.Key.ToString(), true, false, true);
		(void)context->DefineProperty(L"PropertyName", group.PropertyName,
			true, false, true);
		(void)context->DefineProperty(L"Level", static_cast<int>(group.Level),
			true, false, true);
		(void)context->DefineProperty(L"StartIndex",
			static_cast<long long>(group.StartIndex), true, false, true);
		(void)context->DefineProperty(L"ItemCount",
			static_cast<long long>(group.ItemCount), true, false, true);
		(void)context->DefineProperty(L"IsBottomLevel", isBottomLevel,
			true, false, true);
		(void)context->DefineProperty(L"FirstItem", item, true, false, true);
		(void)context->DefineProperty(L"Items",
			BindingListReference(groupItems), true, false, true);
		(void)context->DefineProperty(L"Aggregates",
			BindingSourceReference(aggregates), true, false, true);
		BindingSourceReference contextReference(context);
		std::unique_ptr<Control> header;
		if (_groupStyle.Get()->HeaderTemplate)
			header = _groupStyle.Get()->HeaderTemplate.Get()->Build(
				contextReference, index, &_lastTemplateError);
		else
			header = std::make_unique<Label>(group.Key.ToString(), 0, 0);
		if (!header)
		{
			if (_lastTemplateError.empty())
				_lastTemplateError = L"GroupStyle HeaderTemplate 未生成视觉根。";
			return {};
		}
		auto margin = header->Margin;
		margin.Left += _groupStyle.Get()->HeaderIndent
			* static_cast<float>(group.Level);
		header->Margin = margin;
		result.Visuals.push_back(std::move(header));
		result.Contexts.push_back(std::move(contextReference));
	}
	return result;
}

void ItemsControl::RefreshGroupHeaders()
{
	_lastTemplateError.clear();
	const bool active = IsGroupingActive();
	if (active
		&& ((!IsVirtualizing() && _generator.RealizedCount() != ItemCount())
			|| (IsVirtualizing() && ItemCount() != 0
				&& _generator.RealizedCount() == 0)))
	{
		(void)RebuildGeneratedItems();
		return;
	}
	for (const auto& [index, realized] : _generator.RealizedItems())
	{
		const bool wrapped = dynamic_cast<GroupedItemHost*>(
			realized.Visual) != nullptr;
		if (wrapped != active)
		{
			(void)RebuildGeneratedItems();
			return;
		}
	}
	if (!active) return;
	struct Pending final
	{
		GroupedItemHost* Host = nullptr;
		std::vector<std::unique_ptr<Control>> Headers;
		std::vector<BindingSourceReference> Contexts;
	};
	std::vector<Pending> pending;
	pending.reserve(_generator.RealizedCount());
	for (const auto& [index, realized] : _generator.RealizedItems())
	{
		BindingSourceReference item;
		if (!_itemsSource.Get()->TryGetItem(index, item) || !item)
		{
			_lastTemplateError = L"分组 ItemsSource 无法读取项。";
			return;
		}
		auto headers = BuildGroupHeaders(index, item);
		if (!_lastTemplateError.empty()) return;
		pending.push_back({
			static_cast<GroupedItemHost*>(realized.Visual),
			std::move(headers.Visuals), std::move(headers.Contexts) });
	}
	for (auto& item : pending)
		item.Host->SetHeaders(
			std::move(item.Headers), std::move(item.Contexts),
			_groupStyle.Get()->HeaderSpacing,
			IsVirtualizing() ? _groupStyle.Get()->HeaderHeight : 0.0f,
			IsVirtualizing() ? EffectiveItemsPanel().ItemHeight : 0.0f);
	ConfigureVirtualHost();
	_itemsHost->InvalidateLayout();
	InvalidateLayout();
	InvalidateVisual();
}

bool ItemsControl::PrepareGeneratedItem(
	size_t index,
	PreparedItem& output,
	bool allowRecycle)
{
	output = {};
	output.Index = index;
	ItemContainerGenerator::RecycledItem recycled;
	if (allowRecycle && _generator.TakeRecycled(index, recycled))
	{
		output.Visual = std::move(recycled.Visual);
		output.Observation = std::move(recycled.Observation);
		output.WasRecycled = true;
		auto* grouped = dynamic_cast<GroupedItemHost*>(output.Visual.get());
		if (!IsGroupingActive())
		{
			if (grouped) output.Visual = grouped->TakeItem();
			return output.Visual != nullptr;
		}
		BindingSourceReference recycledItem;
		if (!_itemsSource || !_itemsSource.Get()->TryGetItem(index, recycledItem)
			|| !recycledItem)
		{
			_lastTemplateError = L"ItemsSource 无法读取回收项索引 "
				+ std::to_wstring(index) + L"。";
			return false;
		}
		auto headers = BuildGroupHeaders(index, recycledItem);
		if (!_lastTemplateError.empty()) return false;
		if (grouped)
			grouped->SetHeaders(
				std::move(headers.Visuals), std::move(headers.Contexts),
				_groupStyle.Get()->HeaderSpacing,
				IsVirtualizing() ? _groupStyle.Get()->HeaderHeight : 0.0f,
				IsVirtualizing() ? EffectiveItemsPanel().ItemHeight : 0.0f);
		else
		{
			auto host = std::make_unique<GroupedItemHost>(
				std::move(output.Visual));
			host->SetHeaders(
				std::move(headers.Visuals), std::move(headers.Contexts),
				_groupStyle.Get()->HeaderSpacing,
				IsVirtualizing() ? _groupStyle.Get()->HeaderHeight : 0.0f,
				IsVirtualizing() ? EffectiveItemsPanel().ItemHeight : 0.0f);
			output.Visual = std::move(host);
		}
		return true;
	}

	BindingSourceReference item;
	if (!_itemsSource || !_itemsSource.Get()->TryGetItem(index, item) || !item)
	{
		_lastTemplateError = L"ItemsSource 无法读取索引 "
			+ std::to_wstring(index) + L"。";
		return false;
	}
	auto visual = BuildGeneratedItem(item, index, output.Observation);
	if (!visual)
	{
		if (_lastTemplateError.empty())
			_lastTemplateError = L"项容器未生成视觉根。";
		return false;
	}
	if (IsGroupingActive())
	{
		auto headers = BuildGroupHeaders(index, item);
		if (!_lastTemplateError.empty()) return false;
		auto grouped = std::make_unique<GroupedItemHost>(std::move(visual));
		grouped->SetHeaders(
			std::move(headers.Visuals), std::move(headers.Contexts),
			_groupStyle.Get()->HeaderSpacing,
			IsVirtualizing() ? _groupStyle.Get()->HeaderHeight : 0.0f,
			IsVirtualizing() ? EffectiveItemsPanel().ItemHeight : 0.0f);
		visual = std::move(grouped);
	}
	output.Visual = std::move(visual);
	return true;
}

std::unique_ptr<Control> ItemsControl::BuildGeneratedItem(
	const BindingSourceReference& item,
	size_t index,
	BindingPathObservation& observation)
{
	std::unique_ptr<Control> visual;
	if (_itemTemplate)
	{
		visual = _itemTemplate.Get()->Build(item, index, &_lastTemplateError);
		if (!visual)
		{
			if (_lastTemplateError.empty())
				_lastTemplateError = L"ItemTemplate 未生成视觉根。";
			return {};
		}
	}
	else
	{
		visual = std::make_unique<Label>(
			GetBindingRecordText(item, _displayMemberPath,
				{ L"Text", L"Content", L"Name" }), 0, 0);
		std::weak_ptr<IBindingSource> itemIdentity = item.Shared();
		observation = ObserveBindingPaths(
			item, { _displayMemberPath },
			[this, itemIdentity]
			{ RefreshGeneratedItem(itemIdentity); });
	}
	return WrapGeneratedItem(std::move(visual), item, index);
}

void ItemsControl::AttachPreparedItem(PreparedItem&& item)
{
	if (!_itemsHost || !item.Visual) return;
	const auto index = item.Index;
	auto* visual = item.Visual.get();
	_itemsHost->AddOwned(std::move(item.Visual));
	if (auto* virtualHost = dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
		virtualHost->RegisterItem(visual, index);
	_generator.StoreRealized(
		index, visual, std::move(item.Observation));
}

void ItemsControl::ReorderRealizedChildren()
{
	if (!_itemsHost || IsVirtualizing()
		|| _generator.RealizedCount() < 2) return;
	std::vector<std::unique_ptr<Control>> ordered;
	ordered.reserve(_generator.RealizedCount());
	for (const auto& [index, item] : _generator.RealizedItems())
	{
		(void)index;
		auto detached = _itemsHost->DetachControl(item.Visual);
		if (detached) ordered.push_back(std::move(detached));
	}
	for (auto& item : ordered) _itemsHost->AddOwned(std::move(item));
}

void ItemsControl::ClearRealizedItems(bool keepForRecycle)
{
	if (!_itemsHost)
	{
		_generator.ClearRealized();
		return;
	}
	std::vector<size_t> indices;
	indices.reserve(_generator.RealizedCount());
	for (const auto& [index, item] : _generator.RealizedItems())
	{
		(void)item;
		indices.push_back(index);
	}
	for (const auto index : indices)
	{
		auto item = _generator.TakeRealized(index);
		auto* visual = item.Visual;
		if (auto* virtualHost = dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
			virtualHost->UnregisterItem(visual);
		auto detached = _itemsHost->DetachControl(visual);
		if (keepForRecycle && detached)
			_generator.StoreRecycled(index, {
				std::move(detached), std::move(item.Observation) });
	}
}

std::pair<size_t, size_t> ItemsControl::VirtualRangeForViewport() const noexcept

{
	const auto* scroll = ItemsScrollOwner();
	if (!scroll) return { 0, ItemCount() };
	return VirtualRangeForOffset(static_cast<float>(scroll->ScrollYOffset));
}

std::pair<size_t, size_t> ItemsControl::VirtualRangeForOffset(
	float offset) const noexcept
{
	const size_t count = ItemCount();
	if (count == 0) return { 0, 0 };
	const auto& panel = EffectiveItemsPanel();
	auto* self = const_cast<ItemsControl*>(this);
	self->ConfigureVirtualHost();
	const auto* host = dynamic_cast<const VirtualizingItemsHost*>(_itemsHost);
	if (!host || host->ContentHeight() <= 0.0f) return { 0, count };
	const auto* scroll = ItemsScrollOwner();
	if (!scroll) return { 0, count };
	const auto size = const_cast<ScrollView*>(scroll)->GetActualSizeDip();
	const float viewport = (std::max)(1.0f, size.height);
	const float cache = panel.CacheLength * viewport;
	const size_t first = host->IndexAtOffset(
		(std::max)(0.0f, offset - cache));
	const float endOffset = (std::min)(host->ContentHeight(),
		(std::max)(0.0f, offset) + viewport + cache);
	const size_t last = endOffset >= host->ContentHeight()
		? count : (std::min)(count, host->IndexAtOffset(endOffset) + 1);
	return { first, last };
}

bool ItemsControl::RealizeVirtualRange(size_t first, size_t last)
{
	if (!IsVirtualizing() || !_itemsHost || _realizingViewport) return true;
	const size_t count = ItemCount();
	first = (std::min)(first, count);
	last = (std::clamp)(last, first, count);
	_realizingViewport = true;
	std::vector<PreparedItem> additions;
	for (size_t index = first; index < last; ++index)
	{
		if (_generator.ContainsRealized(index)) continue;
		PreparedItem item;
		if (!PrepareGeneratedItem(index, item))
		{
			for (auto& prepared : additions)
				if (prepared.WasRecycled)
					_generator.StoreRecycled(prepared.Index, {
						std::move(prepared.Visual),
						std::move(prepared.Observation) });
			_realizingViewport = false;
			return false;
		}
		additions.push_back(std::move(item));
	}

	std::vector<size_t> removals;
	for (const auto& [index, item] : _generator.RealizedItems())
	{
		(void)item;
		if (index < first || index >= last) removals.push_back(index);
	}
	for (const auto index : removals)
	{
		auto item = _generator.TakeRealized(index);
		auto* visual = item.Visual;
		if (auto* virtualHost = dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
			virtualHost->UnregisterItem(visual);
		auto detached = _itemsHost->DetachControl(visual);
		if (detached)
			_generator.StoreRecycled(index, {
				std::move(detached), std::move(item.Observation) });
	}
	for (auto& addition : additions)
		AttachPreparedItem(std::move(addition));
	TrimRecyclePool(first, last);
	_itemsHost->InvalidateLayout();
	InvalidateLayout();
	OnGeneratedItemsRealized();
	_realizingViewport = false;
	return true;
}

bool ItemsControl::RealizeVirtualViewport()
{
	if (!IsVirtualizing()) return true;
	const auto [first, last] = VirtualRangeForViewport();
	return RealizeVirtualRange(first, last);
}

void ItemsControl::TrimRecyclePool(size_t first, size_t last)
{
	const size_t visible = (std::max)(size_t{ 1 }, last - first);
	const size_t limit = (std::max)(size_t{ 4 }, visible * 2);
	const size_t center = first + visible / 2;
	auto& recycled = _generator.RecycledItems();
	while (recycled.size() > limit)
	{
		const auto low = recycled.begin();
		const auto high = std::prev(recycled.end());
		const size_t lowDistance = center > low->first
			? center - low->first : low->first - center;
		const size_t highDistance = center > high->first
			? center - high->first : high->first - center;
		recycled.erase(highDistance >= lowDistance ? high : low);
	}
}

bool ItemsControl::ApplyCollectionChange(
	const CollectionChangedEventArgs& change)
{
	const size_t newCount = ItemCount();
	if (!_itemsHost || _applyingCollectionChange
		|| !_generator.CanApply(change, newCount)) return false;

	auto* scrollOwner = ItemsScrollOwner();
	float desiredScroll = scrollOwner
		? static_cast<float>(scrollOwner->ScrollYOffset) : 0.0f;
	if (IsVirtualizing() && change.OldSize != 0)
	{
		auto* virtualHost = dynamic_cast<VirtualizingItemsHost*>(_itemsHost);
		if (virtualHost)
		{
			const size_t oldAnchor = (std::min)(change.OldSize - 1,
				virtualHost->IndexAtOffset(desiredScroll));
			const float withinItem = desiredScroll
				- virtualHost->ItemTop(oldAnchor);
			auto mappedAnchor = ItemContainerGenerator::MapIndex(
				change, oldAnchor);
			if (!mappedAnchor && newCount != 0)
			{
				const size_t replacement = change.Action
					== CollectionChangeAction::Replace
					? change.NewIndex : change.OldIndex;
				mappedAnchor = (std::min)(replacement, newCount - 1);
			}
			ConfigureVirtualHost();
			desiredScroll = mappedAnchor
				? virtualHost->ItemTop(*mappedAnchor) + withinItem : 0.0f;
			const float contentHeight = virtualHost->ContentHeight();
			const float viewport = scrollOwner
				? (std::max)(1.0f, scrollOwner->GetActualSizeDip().height)
				: contentHeight;
			desiredScroll = (std::clamp)(
				desiredScroll, 0.0f,
				(std::max)(0.0f, contentHeight - viewport));
		}
	}
	else if (IsVirtualizing()) ConfigureVirtualHost();

	size_t first = 0;
	size_t last = newCount;
	if (IsVirtualizing())
		std::tie(first, last) = VirtualRangeForOffset(desiredScroll);
	std::unordered_set<size_t> occupied;
	occupied.reserve(_generator.RealizedCount());
	for (const auto& [oldIndex, item] : _generator.RealizedItems())
	{
		(void)item;
		if (const auto mapped = ItemContainerGenerator::MapIndex(
			change, oldIndex)) occupied.insert(*mapped);
	}

	std::vector<PreparedItem> additions;
	additions.reserve(last - first);
	for (size_t index = first; index < last; ++index)
	{
		if (occupied.contains(index)) continue;
		PreparedItem item;
		if (!PrepareGeneratedItem(index, item, false)) return false;
		additions.push_back(std::move(item));
	}

	_applyingCollectionChange = true;
	{
		cui::layout::LayoutScope<ItemsControl> layout(*this);
		OnBeforeGeneratedItemsRebuilt();
		for (const auto index
			: _generator.InvalidatedRealizedIndices(change))
		{
			auto item = _generator.TakeRealized(index);
			if (!item.Visual) continue;
			if (auto* virtualHost =
				dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
				virtualHost->UnregisterItem(item.Visual);
			(void)_itemsHost->DetachControl(item.Visual);
		}
		for (const auto& indexChange : _generator.Apply(change))
		{
			if (!indexChange.Visual) continue;
			if (auto* virtualHost =
				dynamic_cast<VirtualizingItemsHost*>(_itemsHost))
				virtualHost->RegisterItem(
					indexChange.Visual, indexChange.NewIndex);
			OnGeneratedItemIndexChanged(
				*indexChange.Visual,
				indexChange.OldIndex,
				indexChange.NewIndex);
		}
		ConfigureVirtualHost();
		for (auto& addition : additions)
		{
			_generator.DiscardRecycled(addition.Index);
			AttachPreparedItem(std::move(addition));
		}
		ReorderRealizedChildren();
		_itemsHost->InvalidateLayout();
		InvalidateLayout();
		InvalidateVisual();
	}
	UpdateLayout();
	if (IsVirtualizing() && scrollOwner)
		scrollOwner->SetScrollOffset(
			scrollOwner->ScrollXOffset,
			static_cast<int>(std::lround(desiredScroll)));
	OnGeneratedItemsRebuilt();
	if (IsVirtualizing())
	{
		(void)RealizeVirtualViewport();
		const auto range = VirtualRangeForViewport();
		TrimRecyclePool(range.first, range.second);
	}
	OnGeneratedItemsRealized();
	_applyingCollectionChange = false;
	return true;
}

void ItemsControl::RefreshGeneratedItem(
	const std::weak_ptr<IBindingSource>& itemIdentity)
{
	const auto item = itemIdentity.lock();
	if (!item || !_itemsSource) return;
	const int index = FindBindingListItemByValue(
		_itemsSource, std::wstring{},
		BindingValue(BindingSourceReference(item)));
	if (index < 0) return;
	const size_t itemIndex = static_cast<size_t>(index);
	if (!_generator.ContainsRealized(itemIndex))
	{
		_generator.DiscardRecycled(itemIndex);
		return;
	}
	const CollectionChangedEventArgs refresh{
		CollectionChangeAction::Replace,
		itemIndex,
		itemIndex,
		1,
		1,
		ItemCount(),
		ItemCount()
	};
	(void)ApplyCollectionChange(refresh);
}

bool ItemsControl::RebuildGeneratedItems()
{
	_lastTemplateError.clear();
	const auto sourceItemType = _itemsSource
		? _itemsSource.Get()->ItemTypeName() : std::wstring{};
	if (_itemsSource && _itemTemplate
		&& !sourceItemType.empty()
		&& !_itemTemplate.Get()->DataTypeName().empty()
		&& !EqualsIgnoreCase(_itemTemplate.Get()->DataTypeName(),
			sourceItemType))
	{
		_lastTemplateError = L"ItemTemplate DataType 与 ItemsSource ItemType 不一致。";
		return false;
	}
	if (_groupStyle)
	{
		const auto* style = _groupStyle.Get();
		if (!std::isfinite(style->HeaderIndent) || style->HeaderIndent < 0.0f
			|| !std::isfinite(style->HeaderSpacing) || style->HeaderSpacing < 0.0f
			|| !std::isfinite(style->HeaderHeight) || style->HeaderHeight <= 0.0f)
		{
			_lastTemplateError = L"GroupStyle HeaderIndent/HeaderSpacing 必须为有限非负数，HeaderHeight 必须为有限正数。";
			return false;
		}
		if (style->HeaderTemplate
			&& !EqualsIgnoreCase(
				style->HeaderTemplate.Get()->DataTypeName(),
				std::wstring(CollectionViewGroupDataTypeName)))
		{
			_lastTemplateError = L"GroupStyle HeaderTemplate DataType 必须为 CollectionViewGroup。";
			return false;
		}
	}
	if (!_itemsHost)
	{
		_lastTemplateError = L"ItemsHost 不可用。";
		return false;
	}

	_generator.ClearRecycled();
	std::vector<PreparedItem> prepared;
	size_t first = 0;
	size_t last = ItemCount();
	if (IsVirtualizing())
		std::tie(first, last) = VirtualRangeForViewport();
	prepared.reserve(last - first);
	for (size_t index = first; index < last; ++index)
	{
		PreparedItem item;
		if (!PrepareGeneratedItem(index, item)) return false;
		prepared.push_back(std::move(item));
	}

	cui::layout::LayoutScope<ItemsControl> layout(*this);
	OnBeforeGeneratedItemsRebuilt();
	ClearRealizedItems(false);
	_generator.SetSourceCount(ItemCount());
	ConfigureVirtualHost();
	for (auto& item : prepared) AttachPreparedItem(std::move(item));
	OnGeneratedItemsRebuilt();
	_itemsHost->InvalidateLayout();
	InvalidateLayout();
	InvalidateVisual();
	return true;
}

bool ItemsControl::BringItemIntoView(size_t index)
{
	if (index >= ItemCount()) return false;
	auto* scrollOwner = ItemsScrollOwner();
	if (IsVirtualizing())
	{
		ConfigureVirtualHost();
		auto* host = dynamic_cast<VirtualizingItemsHost*>(_itemsHost);
		if (!host) return false;
		const float top = host->ItemTop(index);
		const float bottom = top + host->ItemExtent(index);
		if (!scrollOwner)
		{
			(void)RealizeVirtualRange(0, ItemCount());
			return GetGeneratedItem(index) != nullptr;
		}
		const float viewport = (std::max)(
			1.0f, scrollOwner->GetActualSizeDip().height);
		float target = static_cast<float>(scrollOwner->ScrollYOffset);
		if (top < target) target = top;
		else if (bottom > target + viewport)
			target = bottom - viewport;
		UpdateLayout();
		scrollOwner->SetScrollOffset(scrollOwner->ScrollXOffset,
			static_cast<int>(std::ceil((std::max)(0.0f, target))));
		(void)RealizeVirtualViewport();
		if (auto* item = GetGeneratedItem(index))
		{
			(void)scrollOwner->BringDescendantIntoView(item);
			return true;
		}
		return false;
	}
	if (auto* item = GetGeneratedItem(index))
		return scrollOwner
			? scrollOwner->BringDescendantIntoView(item) : true;
	return false;
}

void ItemsControl::Update()
{
	if (IsVirtualizing()) (void)RealizeVirtualViewport();
	if (!_controlTemplateRoot)
	{
		ScrollView::Update();
		return;
	}
	if (!IsVisual || !ParentForm || !ParentForm->Render) return;
	PerformPendingLayout();
	BeginRender();
	if (!ParentForm->IsDCompSceneRenderActive())
		for (auto* child : GetChildrenInZOrder())
			if (child && child->Visible) child->Update();
	EndRender();
}

void ItemsControl::PerformPendingLayout()
{
	if (_controlTemplateRoot) Panel::PerformPendingLayout();
	else ScrollView::PerformPendingLayout();
}

bool ItemsControl::ProcessMessage(
	UINT message, WPARAM wParam, LPARAM lParam,
	int localX, int localY)
{
	return _controlTemplateRoot
		? Panel::ProcessMessage(message, wParam, lParam, localX, localY)
		: ScrollView::ProcessMessage(message, wParam, lParam, localX, localY);
}

bool ItemsControl::HandlesNavigationKey(WPARAM key) const
{
	return !_controlTemplateRoot && ScrollView::HandlesNavigationKey(key);
}

CursorKind ItemsControl::QueryCursor(int localX, int localY)
{
	return _controlTemplateRoot
		? Control::QueryCursor(localX, localY)
		: ScrollView::QueryCursor(localX, localY);
}

bool ItemsControl::ShouldHitTestChildrenAt(int localX, int localY) const
{
	return _controlTemplateRoot
		? Control::ShouldHitTestChildrenAt(localX, localY)
		: ScrollView::ShouldHitTestChildrenAt(localX, localY);
}

POINT ItemsControl::GetChildrenRenderOffset() const
{
	return _controlTemplateRoot
		? POINT{ 0, 0 } : ScrollView::GetChildrenRenderOffset();
}

D2D1_RECT_F ItemsControl::GetChildrenClipRect()
{
	return _controlTemplateRoot
		? Control::GetChildrenClipRect() : ScrollView::GetChildrenClipRect();
}

std::unique_ptr<Control> ItemsControl::WrapGeneratedItem(
	std::unique_ptr<Control> visual,
	const BindingSourceReference&,
	size_t)
{
	return visual;
}
