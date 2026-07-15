#define NOMINMAX
#include "PagedGridView.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

static int ComparePagedGridStringDefault(const std::wstring& a, const std::wstring& b)
{
	if (a == b) return 0;
	return (a < b) ? -1 : 1;
}

static std::wstring PagedGridCellToStringDefault(const CellValue* value)
{
	if (!value) return L"";
	if (!value->Text.empty()) return value->Text;
	return std::to_wstring((__int64)value->Tag);
}

UIClass PagedGridView::Type()
{
	return UIClass::UI_PagedGridView;
}

namespace
{
	static bool PagedGridColorEquals(
		const D2D1_COLOR_F& left, const D2D1_COLOR_F& right)
	{
		return std::fabs(left.r - right.r) < 1e-6f
			&& std::fabs(left.g - right.g) < 1e-6f
			&& std::fabs(left.b - right.b) < 1e-6f
			&& std::fabs(left.a - right.a) < 1e-6f;
	}

	template<typename TValue>
	ControlPropertyOptions<PagedGridView, TValue> PagedGridPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<PagedGridView, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto PagedGridPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			PagedGridView& target,
			BindingPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					Control*, const ControlPropertyChangedEventArgs& args)
				{
					if (_wcsicmp(args.PropertyName.c_str(), propertyName.c_str()) == 0)
						handler();
				});
		};
	}

	ControlPropertyOptions<PagedGridView, float> PagedGridMetricOptions(
		float defaultValue, int order)
	{
		auto options = PagedGridPropertyOptions(
			defaultValue, L"Layout", 100, order,
			ControlPropertyEditorKind::Number,
			ControlPropertyFlags::AffectsArrange | ControlPropertyFlags::AffectsRender);
		options.Coerce = [](
			PagedGridView&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

	ControlPropertyOptions<PagedGridView, D2D1_COLOR_F> PagedGridColorOptions(
		D2D1_COLOR_F defaultValue, int order)
	{
		auto options = PagedGridPropertyOptions(
			defaultValue, L"Appearance", 200, order,
			ControlPropertyEditorKind::Color);
		options.Equals = PagedGridColorEquals;
		return options;
	}
}

void PagedGridView::EnsureBindingPropertiesRegistered()
{
	Panel::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		RegisterPanelCornerRadiusMetadata<PagedGridView>(8.0f);

		auto pageIndexOptions = PagedGridPropertyOptions(
			0, L"Behavior", 110, 100,
			ControlPropertyEditorKind::Number);
		pageIndexOptions.Coerce = [](
			PagedGridView& target, const int& proposed) -> std::optional<int>
		{
			return (std::clamp)(proposed, 0, target.GetPageCount() - 1);
		};
		pageIndexOptions.Design.Browsable = false;
		pageIndexOptions.Design.Persistence = ControlPropertyPersistence::Transient;
		BindingPropertyRegistry::Register<PagedGridView, int>(L"PageIndex",
			[](PagedGridView& target) { return target.PageIndex; },
			[](PagedGridView& target, const int& value) { target.PageIndex = value; },
			PagedGridPropertySubscriber(L"PageIndex"), std::move(pageIndexOptions));

		auto pageSizeOptions = PagedGridPropertyOptions(
			50, L"Behavior", 110, 10,
			ControlPropertyEditorKind::Number);
		pageSizeOptions.Coerce = [](
			PagedGridView&, const int& proposed) -> std::optional<int>
		{
			return (std::max)(1, proposed);
		};
		pageSizeOptions.Design.Minimum = 1.0;
		pageSizeOptions.Design.Step = 1.0;
		BindingPropertyRegistry::Register<PagedGridView, int>(L"PageSize",
			[](PagedGridView& target) { return target.PageSize; },
			[](PagedGridView& target, const int& value) { target.PageSize = value; },
			PagedGridPropertySubscriber(L"PageSize"), std::move(pageSizeOptions));

#define CUI_REGISTER_PAGED_BOOL(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<PagedGridView, bool>(propertyName, \
			[](PagedGridView& target) { return target.name; }, \
			[](PagedGridView& target, const bool& value) { target.name = value; }, \
			PagedGridPropertySubscriber(propertyName), \
			PagedGridPropertyOptions(defaultValue, L"Behavior", 110, order, \
				ControlPropertyEditorKind::Boolean))

		CUI_REGISTER_PAGED_BOOL(ShowPager, L"ShowPager", true, 20);
		CUI_REGISTER_PAGED_BOOL(SyncGridEditsOnPageChange,
			L"SyncGridEditsOnPageChange", true, 30);

#undef CUI_REGISTER_PAGED_BOOL

#define CUI_REGISTER_PAGED_METRIC(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<PagedGridView, float>(propertyName, \
			[](PagedGridView& target) { return target.name; }, \
			[](PagedGridView& target, const float& value) { target.name = value; }, \
			PagedGridPropertySubscriber(propertyName), \
			PagedGridMetricOptions(defaultValue, order))

		CUI_REGISTER_PAGED_METRIC(PagerHeight, L"PagerHeight", 42.0f, 10);
		CUI_REGISTER_PAGED_METRIC(PagerGap, L"PagerGap", 6.0f, 20);
		CUI_REGISTER_PAGED_METRIC(PagerButtonWidth, L"PagerButtonWidth", 38.0f, 30);
		CUI_REGISTER_PAGED_METRIC(PagerButtonHeight, L"PagerButtonHeight", 26.0f, 40);
		CUI_REGISTER_PAGED_METRIC(PagerPaddingX, L"PagerPaddingX", 8.0f, 50);

#undef CUI_REGISTER_PAGED_METRIC

#define CUI_REGISTER_PAGED_COLOR(name, propertyName, defaultValue, order) \
		BindingPropertyRegistry::Register<PagedGridView, D2D1_COLOR_F>(propertyName, \
			[](PagedGridView& target) { return target.name; }, \
			[](PagedGridView& target, const D2D1_COLOR_F& value) { target.name = value; }, \
			PagedGridPropertySubscriber(propertyName), \
			PagedGridColorOptions(defaultValue, order))

		CUI_REGISTER_PAGED_COLOR(PagerBackColor, L"PagerBackColor",
			(D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.72f }), 10);
		CUI_REGISTER_PAGED_COLOR(PagerBorderColor, L"PagerBorderColor",
			(D2D1_COLOR_F{ 0.72f, 0.75f, 0.82f, 0.55f }), 20);
		CUI_REGISTER_PAGED_COLOR(PagerButtonBackColor, L"PagerButtonBackColor",
			(D2D1_COLOR_F{ 0.97f, 0.98f, 0.99f, 1.0f }), 30);
		CUI_REGISTER_PAGED_COLOR(PagerButtonHoverColor, L"PagerButtonHoverColor",
			(D2D1_COLOR_F{ 0.20f, 0.46f, 0.90f, 0.16f }), 40);
		CUI_REGISTER_PAGED_COLOR(PagerButtonCheckedColor, L"PagerButtonCheckedColor",
			(D2D1_COLOR_F{ 0.20f, 0.46f, 0.90f, 0.28f }), 50);
		CUI_REGISTER_PAGED_COLOR(PagerTextColor, L"PagerTextColor", Colors::Black, 60);
		CUI_REGISTER_PAGED_COLOR(AccentColor, L"AccentColor",
			(D2D1_COLOR_F{ 0.20f, 0.55f, 0.95f, 0.95f }), 70);

#undef CUI_REGISTER_PAGED_COLOR
		return true;
	}();
	(void)registered;
}

#define CUI_PAGED_GRID_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(PagedGridView, type, name) { return field; } \
	SET_CPP(PagedGridView, type, name) { (void)SetPropertyField(propertyName, field, value); }

CUI_PAGED_GRID_PROPERTY_IMPL(float, PagerHeight, _pagerHeight, L"PagerHeight")
CUI_PAGED_GRID_PROPERTY_IMPL(float, PagerGap, _pagerGap, L"PagerGap")
CUI_PAGED_GRID_PROPERTY_IMPL(float, PagerButtonWidth, _pagerButtonWidth, L"PagerButtonWidth")
CUI_PAGED_GRID_PROPERTY_IMPL(float, PagerButtonHeight, _pagerButtonHeight, L"PagerButtonHeight")
CUI_PAGED_GRID_PROPERTY_IMPL(float, PagerPaddingX, _pagerPaddingX, L"PagerPaddingX")
CUI_PAGED_GRID_PROPERTY_IMPL(bool, ShowPager, _showPager, L"ShowPager")
CUI_PAGED_GRID_PROPERTY_IMPL(bool, SyncGridEditsOnPageChange, _syncGridEditsOnPageChange, L"SyncGridEditsOnPageChange")
CUI_PAGED_GRID_PROPERTY_IMPL(D2D1_COLOR_F, PagerBackColor, _pagerBackColor, L"PagerBackColor")
CUI_PAGED_GRID_PROPERTY_IMPL(D2D1_COLOR_F, PagerBorderColor, _pagerBorderColor, L"PagerBorderColor")
CUI_PAGED_GRID_PROPERTY_IMPL(D2D1_COLOR_F, PagerButtonBackColor, _pagerButtonBackColor, L"PagerButtonBackColor")
CUI_PAGED_GRID_PROPERTY_IMPL(D2D1_COLOR_F, PagerButtonHoverColor, _pagerButtonHoverColor, L"PagerButtonHoverColor")
CUI_PAGED_GRID_PROPERTY_IMPL(D2D1_COLOR_F, PagerButtonCheckedColor, _pagerButtonCheckedColor, L"PagerButtonCheckedColor")
CUI_PAGED_GRID_PROPERTY_IMPL(D2D1_COLOR_F, PagerTextColor, _pagerTextColor, L"PagerTextColor")
CUI_PAGED_GRID_PROPERTY_IMPL(D2D1_COLOR_F, AccentColor, _accentColor, L"AccentColor")

#undef CUI_PAGED_GRID_PROPERTY_IMPL

PagedGridView::UpdateScope::UpdateScope(PagedGridView& owner) noexcept
	: _owner(&owner)
{
	_owner->BeginUpdate();
}

PagedGridView::UpdateScope::~UpdateScope()
{
	Commit();
}

PagedGridView::UpdateScope::UpdateScope(UpdateScope&& other) noexcept
	: _owner(other._owner)
{
	other._owner = nullptr;
}

PagedGridView::UpdateScope& PagedGridView::UpdateScope::operator=(UpdateScope&& other) noexcept
{
	if (this == &other) return *this;
	Commit();
	_owner = other._owner;
	other._owner = nullptr;
	return *this;
}

void PagedGridView::UpdateScope::Commit() noexcept
{
	if (!_owner) return;
	_owner->EndUpdate();
	_owner = nullptr;
}

PagedGridView::PagedGridView(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	InitializePanelCornerRadiusDefault(8.0f);
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderThickness = 0.0f;

	Grid = this->AddControl(new GridView(0, 0, width, (std::max)(1, height - (int)PagerHeight)));
	Grid->AllowUserToAddRows = false;
	Rows.SetOwnerChangedHandler(
		[this](const CollectionChangedEventArgs& change)
		{ OnRowsCollectionChanged(change); });
	_columnsChangedConnection = Grid->Columns.Changed.Subscribe(
		[this](GridView::ColumnCollection*,
			const CollectionChangedEventArgs& change)
		{ OnColumnsCollectionChanged(change); });
	Grid->SortRequestHandler = [this](GridView*, int columnIndex, bool ascending) -> bool
		{
			SortByColumn(columnIndex, ascending);
			return true;
		};
	Grid->OnUserAddedRow += [this](GridView* sender, int newRowIndex)
		{
			if (_syncingPage || !sender) return;
			int insertIndex = std::clamp(_pageIndex * _pageSize + newRowIndex, 0, (int)Rows.size());
			if (newRowIndex >= 0 && newRowIndex < (int)sender->Rows.size())
				Rows.insert(Rows.begin() + insertIndex, sender->Rows[newRowIndex]);
		};

	CreatePagerControls();
	_lastSourceSize = Rows.size();
	_lastPageSize = _pageSize;
}

GET_CPP(PagedGridView, int, PageIndex)
{
	return _pageIndex;
}

SET_CPP(PagedGridView, int, PageIndex)
{
	int pageCount = GetPageCount();
	int next = std::clamp(value, 0, pageCount - 1);
	if (_pageIndex == next && !_pageDirty) return;
	int old = _pageIndex;
	if (!_changingPageSize && SyncGridEditsOnPageChange)
		SyncPageToSource(old, _pageSize);
	if (!SetPropertyField(L"PageIndex", _pageIndex, next))
	{
		if (_pageDirty) RefreshPage();
		return;
	}
	MarkPageDirty();
	RefreshPage();
	if (old != _pageIndex)
		OnPageChanged(this, old, _pageIndex);
}

GET_CPP(PagedGridView, int, PageSize)
{
	return _pageSize;
}

SET_CPP(PagedGridView, int, PageSize)
{
	int nextSize = std::max(1, value);
	if (_pageSize == nextSize) return;
	if (SyncGridEditsOnPageChange)
		SyncPageToSource(_pageIndex, _pageSize);
	BeginUpdate();
	if (!SetPropertyField(L"PageSize", _pageSize, nextSize))
	{
		EndUpdate();
		return;
	}
	const int clamped = std::clamp(_pageIndex, 0, GetPageCount() - 1);
	_changingPageSize = true;
	SetCurrentPageIndex(clamped);
	_changingPageSize = false;
	MarkPageDirty();
	EndUpdate();
}

GET_CPP(PagedGridView, int, PageCount)
{
	int size = std::max(1, _pageSize);
	int total = (int)Rows.size();
	return std::max(1, (total + size - 1) / size);
}

GET_CPP(PagedGridView, int, TotalRowCount)
{
	return (int)Rows.size();
}

void PagedGridView::CreatePagerControls()
{
	_firstButton = this->AddControl(new Button(L"<<", 0, 0, 38, 26));
	_prevButton = this->AddControl(new Button(L"<", 0, 0, 38, 26));
	_nextButton = this->AddControl(new Button(L">", 0, 0, 38, 26));
	_lastButton = this->AddControl(new Button(L">>", 0, 0, 38, 26));
	_pageInfoLabel = this->AddControl(new Label(L"Page 1 / 1", 0, 0));
	_pageInfoLabel->Size = { 220, 24 };
	_pageInfoLabel->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };

	_firstButton->OnMouseClick += [this](Control*, MouseEventArgs) { FirstPage(); };
	_prevButton->OnMouseClick += [this](Control*, MouseEventArgs) { PreviousPage(); };
	_nextButton->OnMouseClick += [this](Control*, MouseEventArgs) { NextPage(); };
	_lastButton->OnMouseClick += [this](Control*, MouseEventArgs) { LastPage(); };
}

void PagedGridView::StylePagerButton(Button* button)
{
	if (!button) return;
	button->BackColor = PagerButtonBackColor;
	button->ForeColor = PagerTextColor;
	button->BorderColor = PagerBorderColor;
	button->UnderMouseColor = PagerButtonHoverColor;
	button->CheckedColor = PagerButtonCheckedColor;
	button->DisabledOverlayColor = D2D1_COLOR_F{ 1, 1, 1, 0.38f };
	button->Round = 6.0f;
	button->BorderThickness = 1.0f;
}

void PagedGridView::LayoutChildren()
{
	if (!Grid) return;
	int w = this->Width;
	int h = this->Height;
	int pagerH = ShowPager ? (int)std::round(std::clamp(PagerHeight, 0.0f, (float)h)) : 0;
	int gridH = std::max(1, h - pagerH);
	Grid->Location = { 0, 0 };
	Grid->Size = { w, gridH };

	int buttonW = (int)std::round(std::max(24.0f, PagerButtonWidth));
	int buttonH = (int)std::round(std::max(22.0f, PagerButtonHeight));
	int gap = (int)std::round(std::max(0.0f, PagerGap));
	int padX = (int)std::round(std::max(0.0f, PagerPaddingX));
	int footerTop = gridH;
	int buttonY = footerTop + std::max(0, (pagerH - buttonH) / 2);
	int x = std::max(padX, w - padX - buttonW * 4 - gap * 3);
	Button* buttons[] = { _firstButton, _prevButton, _nextButton, _lastButton };
	for (auto* button : buttons)
	{
		if (!button) continue;
		button->Visible = ShowPager;
		button->Location = { x, buttonY };
		button->Size = { buttonW, buttonH };
		x += buttonW + gap;
	}
	if (_pageInfoLabel)
	{
		_pageInfoLabel->Visible = ShowPager;
		_pageInfoLabel->Location = { padX, footerTop + std::max(0, (pagerH - 22) / 2) };
		_pageInfoLabel->Size = { std::max(1, w - padX * 2 - buttonW * 4 - gap * 4), 24 };
	}
}

void PagedGridView::UpdatePagerState()
{
	const int pageCount = GetPageCount();
	_pageIndex = std::clamp(_pageIndex, 0, pageCount - 1);
	if (_pageInfoLabel)
	{
		int start = Rows.empty() ? 0 : _pageIndex * _pageSize + 1;
		int end = Rows.empty() ? 0 : std::min((int)Rows.size(), (_pageIndex + 1) * _pageSize);
		_pageInfoLabel->Text = L"Page " + std::to_wstring(_pageIndex + 1) + L" / " + std::to_wstring(pageCount) +
			L"   " + std::to_wstring(start) + L"-" + std::to_wstring(end) +
			L" of " + std::to_wstring((int)Rows.size());
		_pageInfoLabel->ForeColor = PagerTextColor;
	}
	bool canPrev = _pageIndex > 0;
	bool canNext = _pageIndex < pageCount - 1;
	if (_firstButton) _firstButton->Enable = canPrev;
	if (_prevButton) _prevButton->Enable = canPrev;
	if (_nextButton) _nextButton->Enable = canNext;
	if (_lastButton) _lastButton->Enable = canNext;
}

void PagedGridView::MarkPageDirty()
{
	_pageDirty = true;
	if (!IsUpdating())
		InvalidateVisual();
}

void PagedGridView::EnsureSourceAccessibilityIds()
{
	std::unordered_set<uint32_t> used;
	if (Grid)
	{
		std::vector<uint32_t> columnIds;
		Grid->GetAccessibilityVirtualColumnHeaders(columnIds);
		used.insert(columnIds.begin(), columnIds.end());
	}
	auto ensure = [&used](uint32_t& id)
	{
		while (id == 0 || !used.insert(id).second)
			id = AllocateAccessibilityVirtualId();
	};
	for (auto& row : Rows)
	{
		ensure(row.AccessibilityId);
		for (auto& cell : row.Cells)
			ensure(cell.AccessibilityId);
	}
}

void PagedGridView::OnRowsCollectionChanged(
	const CollectionChangedEventArgs& change)
{
	const size_t columnCount = Grid ? Grid->Columns.size() : 0;
	if (columnCount != 0)
	{
		size_t first = 0;
		size_t end = Rows.size();
		if (change.Action == CollectionChangeAction::Add
			&& change.NewIndex != CollectionChangedEventArgs::Npos)
		{
			first = (std::min)(change.NewIndex, Rows.size());
			end = (std::min)(Rows.size(), first + change.NewCount);
		}
		else if (change.Action == CollectionChangeAction::Replace
			&& change.NewIndex != CollectionChangedEventArgs::Npos)
		{
			first = (std::min)(change.NewIndex, Rows.size());
			end = (std::min)(Rows.size(), first + change.NewCount);
		}
		else if (change.Action != CollectionChangeAction::Reset)
		{
			first = end;
		}
		for (size_t index = first; index < end; ++index)
			Rows[index].Cells.resize(columnCount);
	}
	EnsureSourceAccessibilityIds();
	const int clamped = (std::clamp)(
		_pageIndex, 0, GetPageCount() - 1);
	_changingPageSize = true;
	SetCurrentPageIndex(clamped);
	_changingPageSize = false;
	MarkPageDirty();
	RefreshPage();
}

void PagedGridView::OnColumnsCollectionChanged(
	const CollectionChangedEventArgs& change)
{
	if (!Grid) return;
	std::vector<uint32_t> currentIds;
	Grid->GetAccessibilityVirtualColumnHeaders(currentIds);
	std::unordered_map<uint32_t, size_t> oldIndices;
	for (size_t index = 0; index < _sourceColumnIds.size(); ++index)
	{
		if (_sourceColumnIds[index] != 0)
			oldIndices.emplace(_sourceColumnIds[index], index);
	}
	size_t matches = 0;
	for (uint32_t id : currentIds)
	{
		if (oldIndices.contains(id)) ++matches;
	}
	const bool resetByPosition =
		change.Action == CollectionChangeAction::Reset && matches == 0;
	for (auto& row : Rows)
	{
		if (resetByPosition)
		{
			row.Cells.resize(currentIds.size());
			continue;
		}
		if (_sourceColumnIds.empty()
			&& row.Cells.size() > currentIds.size()
			&& change.Action == CollectionChangeAction::Add)
		{
			// Rows may be supplied before their column definitions. Keep the
			// unbound trailing cells until enough columns arrive to name them.
			continue;
		}
		auto oldCells = std::move(row.Cells);
		std::vector<CellValue> nextCells(currentIds.size());
		for (size_t next = 0; next < currentIds.size(); ++next)
		{
			const auto old = oldIndices.find(currentIds[next]);
			if (old != oldIndices.end() && old->second < oldCells.size())
				nextCells[next] = std::move(oldCells[old->second]);
			else if (oldCells.size() > _sourceColumnIds.size()
				&& next >= _sourceColumnIds.size()
				&& next < oldCells.size())
				nextCells[next] = std::move(oldCells[next]);
		}
		row.Cells = std::move(nextCells);
	}
	_sourceColumnIds = std::move(currentIds);
	EnsureSourceAccessibilityIds();
	MarkPageDirty();
	RefreshPage();
}

void PagedGridView::BeginUpdate() noexcept
{
	if (_updateDepth++ == 0)
		Rows.BeginUpdate();
}

void PagedGridView::EndUpdate() noexcept
{
	if (_updateDepth <= 0) return;
	if (_updateDepth > 1)
	{
		--_updateDepth;
		return;
	}
	_updateDepth = 0;
	// Let the owner callback synchronize the current page before the public
	// coalesced Rows.Changed event is delivered.
	Rows.EndUpdate();
	if (_pageDirty)
		RefreshPage();
}

void PagedGridView::Clear()
{
	auto update = DeferUpdates();
	ClearRows();
	ClearColumns();
}

void PagedGridView::ClearRows()
{
	Rows.clear();
}

void PagedGridView::SetRows(std::vector<GridViewRow> rows)
{
	auto update = DeferUpdates();
	Rows = std::move(rows);
}

void PagedGridView::ClearColumns()
{
	if (Grid) Grid->Columns.clear();
}

void PagedGridView::SetColumns(std::vector<GridViewColumn> columns)
{
	if (!Grid) return;
	auto update = DeferUpdates();
	Grid->Columns = std::move(columns);
}

void PagedGridView::AddRow(const GridViewRow& row)
{
	Rows.push_back(row);
}

void PagedGridView::AddColumn(const GridViewColumn& column)
{
	if (Grid) Grid->Columns.push_back(column);
}

size_t PagedGridView::RowCount() const
{
	return Rows.size();
}

size_t PagedGridView::ColumnCount() const
{
	return Grid ? Grid->ColumnCount() : 0;
}

GridViewRow& PagedGridView::RowAt(int index)
{
	return Rows.at((size_t)index);
}

PagedGridView::ColumnCollection& PagedGridView::GetColumns()
{
	if (!Grid) throw std::logic_error("PagedGridView has no Grid");
	return Grid->Columns;
}

const PagedGridView::ColumnCollection& PagedGridView::GetColumns() const
{
	if (!Grid) throw std::logic_error("PagedGridView has no Grid");
	return Grid->Columns;
}

const GridViewRow& PagedGridView::RowAt(int index) const
{
	return Rows.at((size_t)index);
}

GridViewColumn& PagedGridView::ColumnAt(int index)
{
	if (!Grid) throw std::out_of_range("PagedGridView has no Grid");
	return Grid->ColumnAt(index);
}

const GridViewColumn& PagedGridView::ColumnAt(int index) const
{
	if (!Grid) throw std::out_of_range("PagedGridView has no Grid");
	return Grid->ColumnAt(index);
}

bool PagedGridView::RemoveRowAt(int index)
{
	if (index < 0 || index >= (int)Rows.size()) return false;
	auto update = DeferUpdates();
	Rows.erase(Rows.begin() + index);
	return true;
}

bool PagedGridView::RemoveColumnAt(int index)
{
	if (!Grid || index < 0
		|| index >= static_cast<int>(Grid->Columns.size())) return false;
	Grid->Columns.erase(Grid->Columns.begin() + index);
	return true;
}

void PagedGridView::FirstPage()
{
	SetCurrentPageIndex(0);
}

void PagedGridView::PreviousPage()
{
	SetCurrentPageIndex(_pageIndex - 1);
}

void PagedGridView::NextPage()
{
	SetCurrentPageIndex(_pageIndex + 1);
}

void PagedGridView::LastPage()
{
	SetCurrentPageIndex(GetPageCount() - 1);
}

void PagedGridView::SortByColumn(int col, bool ascending)
{
	if (!Grid) return;
	if (col < 0 || col >= (int)Grid->Columns.size()) return;

	if (SyncGridEditsOnPageChange)
		SyncCurrentPageToSource();

	if (Rows.size() > 1)
	{
		const auto sortFunc = Grid->Columns[(size_t)col].SortFunc;
		Rows.Sort(
			[&](const GridViewRow& a, const GridViewRow& b) -> bool
			{
				const int aCount = (int)a.Cells.size();
				const int bCount = (int)b.Cells.size();
				const CellValue* av = (aCount > col) ? (a.Cells.data() + col) : nullptr;
				const CellValue* bv = (bCount > col) ? (b.Cells.data() + col) : nullptr;

				int cmp = 0;
				if (sortFunc)
				{
					static CellValue empty;
					cmp = sortFunc(av ? *av : empty, bv ? *bv : empty);
				}
				else
				{
					cmp = ComparePagedGridStringDefault(PagedGridCellToStringDefault(av), PagedGridCellToStringDefault(bv));
				}

				return ascending ? (cmp < 0) : (cmp > 0);
			});
	}

	(void)Grid->TrySetCurrentPropertyValue(
		L"SortedColumnIndex", BindingValue(col));
	(void)Grid->TrySetCurrentPropertyValue(
		L"SortAscending", BindingValue(ascending));
}

void PagedGridView::SyncCurrentPageToSource()
{
	SyncPageToSource(_pageIndex, _pageSize);
}

void PagedGridView::SyncPageToSource(int pageIndex, int pageSize)
{
	if (!Grid || _syncingPage || pageSize <= 0) return;
	int start = pageIndex * pageSize;
	for (int i = 0; i < (int)Grid->Rows.size(); i++)
	{
		int sourceIndex = start + i;
		if (sourceIndex >= 0 && sourceIndex < (int)Rows.size())
			Rows[(size_t)sourceIndex] = Grid->Rows[(size_t)i];
	}
}

void PagedGridView::SetCurrentPageIndex(int value)
{
	if (_pageIndex == value) return;
	(void)SetCurrentPropertyField(L"PageIndex", _pageIndex, value);
}

void PagedGridView::RefreshPage()
{
	if (IsUpdating())
	{
		_pageDirty = true;
		return;
	}
	if (!Grid) return;
	_syncingPage = true;
	auto gridUpdate = Grid->DeferUpdates();
	Grid->ClearRows();
	_pageIndex = std::clamp(_pageIndex, 0, GetPageCount() - 1);
	int start = _pageIndex * _pageSize;
	int end = std::min((int)Rows.size(), start + _pageSize);
	for (int i = start; i < end; i++)
		Grid->AddRow(Rows[(size_t)i]);
	(void)Grid->TrySetCurrentPropertyValue(L"ScrollYOffset", BindingValue(0.0f));
	(void)Grid->TrySetCurrentPropertyValue(L"ScrollRowPosition", BindingValue(0));
	(void)Grid->TrySetCurrentPropertyValue(L"SelectedColumnIndex", BindingValue(-1));
	(void)Grid->TrySetCurrentPropertyValue(L"SelectedRowIndex", BindingValue(-1));
	(void)Grid->TrySetCurrentPropertyValue(L"UnderMouseColumnIndex", BindingValue(-1));
	(void)Grid->TrySetCurrentPropertyValue(L"UnderMouseRowIndex", BindingValue(-1));
	_syncingPage = false;
	_pageDirty = false;
	_lastSourceSize = Rows.size();
	_lastPageSize = _pageSize;
	UpdatePagerState();
	InvalidateVisual();
}

void PagedGridView::Update()
{
	if (!this->IsVisual) return;
	if (Rows.size() != _lastSourceSize || _pageSize != _lastPageSize)
		MarkPageDirty();
	if (_pageDirty)
		RefreshPage();

	LayoutChildren();
	StylePagerButton(_firstButton);
	StylePagerButton(_prevButton);
	StylePagerButton(_nextButton);
	StylePagerButton(_lastButton);
	UpdatePagerState();

	auto d2d = this->ParentForm ? this->ParentForm->Render : nullptr;
	if (d2d && ShowPager)
	{
		const float w = (float)this->Width;
		const float h = (float)this->Height;
		const float pagerH = std::clamp(PagerHeight, 0.0f, h);
		this->BeginRender(w, h);
		{
			const float top = h - pagerH;
			d2d->FillRoundRect(0.0f, top, w, pagerH, PagerBackColor, std::clamp(CornerRadius, 0.0f, pagerH * 0.5f));
			d2d->DrawLine(0.0f, top + 0.5f, w, top + 0.5f, PagerBorderColor, 1.0f);
			if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
			{
				for (auto c : this->GetChildrenInZOrder())
				{
					if (!c || !c->Visible) continue;
					c->Update();
				}
			}
		}
		if (!Enable)
			d2d->FillRoundRect(0.0f, 0.0f, w, h, DisabledOverlayColor, CornerRadius);
		this->EndRender();
		return;
	}

	Panel::Update();
}

bool PagedGridView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (message == WM_KEYDOWN)
	{
		if (wParam == VK_PRIOR)
		{
			PreviousPage();
			return true;
		}
		if (wParam == VK_NEXT)
		{
			NextPage();
			return true;
		}
	}
	return Panel::ProcessMessage(message, wParam, lParam, localX, localY);
}
