#pragma once

#include "Button.h"
#include "CheckBox.h"
#include "CollectionViewSource.h"
#include "ContentControl.h"
#include "Label.h"
#include "Layout/Grid.h"
#include "ListBox.h"
#include "TextBox.h"

#include <limits>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

class DataGrid;
class DataGridCell;
class DataGridColumn;
class DataGridRow;
class DataGridRowHeader;
class DataGridCellsPresenter;
class ScrollViewer;

/** WPF DataGrid selection unit values. */
enum class DataGridSelectionUnit : int
{
	Cell = 0,
	FullRow = 1,
	CellOrRowHeader = 2,
};

/** WPF-compatible bit values consumed independently by row/column chrome. */
enum class DataGridHeadersVisibility : int
{
	None = 0,
	Column = 1,
	Row = 2,
	All = 3,
};

enum class DataGridGridLinesVisibility : int
{
	All = 0,
	Horizontal = 1,
	None = 2,
	Vertical = 3,
};

enum class DataGridLengthUnitType : int
{
	Auto = 0,
	Pixel = 1,
	SizeToCells = 2,
	SizeToHeader = 3,
	Star = 4,
};

/** Value-only WPF DataGridLength projection used by declarative columns. */
struct DataGridLength final
{
	double Value = 1.0;
	DataGridLengthUnitType UnitType = DataGridLengthUnitType::Auto;

	constexpr DataGridLength() noexcept = default;
	explicit constexpr DataGridLength(double pixels) noexcept
		: Value(pixels), UnitType(DataGridLengthUnitType::Pixel) {}
	constexpr DataGridLength(
		double value, DataGridLengthUnitType unitType) noexcept
		: Value(value), UnitType(unitType) {}

	[[nodiscard]] static constexpr DataGridLength Auto() noexcept
	{
		return { 1.0, DataGridLengthUnitType::Auto };
	}
	[[nodiscard]] static constexpr DataGridLength SizeToCells() noexcept
	{
		return { 1.0, DataGridLengthUnitType::SizeToCells };
	}
	[[nodiscard]] static constexpr DataGridLength SizeToHeader() noexcept
	{
		return { 1.0, DataGridLengthUnitType::SizeToHeader };
	}
	[[nodiscard]] static constexpr DataGridLength Star(
		double weight = 1.0) noexcept
	{
		return { weight, DataGridLengthUnitType::Star };
	}

	[[nodiscard]] bool IsValid() const noexcept;
	[[nodiscard]] static bool TryParse(
		std::wstring_view text, DataGridLength& out) noexcept;
	bool operator==(const DataGridLength&) const noexcept = default;
};

enum class DataGridEditAction : int
{
	Cancel = 0,
	Commit = 1,
};

/** Stable cell identity; it deliberately is not a realized-container handle. */
struct DataGridCellInfo final
{
	static constexpr size_t InvalidIndex =
		(std::numeric_limits<size_t>::max)();

	BindingSourceReference Item;
	DataGridColumn* Column = nullptr;
	size_t RowIndex = InvalidIndex;
	size_t ColumnIndex = InvalidIndex;

	[[nodiscard]] bool IsValid() const noexcept
	{
		return Item && Column && RowIndex != InvalidIndex
			&& ColumnIndex != InvalidIndex;
	}
	bool operator==(const DataGridCellInfo& other) const noexcept
	{
		return Item.Shared() == other.Item.Shared()
			&& Column == other.Column
			&& _itemOccurrence == other._itemOccurrence;
	}

private:
	// Stable CollectionView source-slot identity for repeated references to the
	// same object. RowIndex remains only a remappable public locator.
	size_t _itemOccurrence = InvalidIndex;
	friend class DataGrid;
	friend class DataGridSelectedCellCollection;
};

/**
 * WPF-shaped lazy selected-cell collection.
 *
 * Sparse selections retain exact occurrence identities. Dense selections may
 * retain rectangular row/column regions and create DataGridCellInfo values only
 * when an indexer, iterator, or automation client actually asks for them.
 */
class DataGridSelectedCellCollection final
{
public:
	struct RegionExcludedOffsets final
	{
		struct RowInterval final
		{
			size_t Start = 0;
			size_t Count = 0;
			// Number of interval-covered rows before Start.  This makes
			// ordinal-to-snapshot translation logarithmic in the number of
			// retired runs without expanding a run into one offset per row.
			size_t ExcludedBefore = 0;
		};
		std::vector<RowInterval> RowIntervals;
		std::vector<size_t> Rows;
		std::vector<size_t> Cells;
	};
	class const_iterator final
	{
	public:
		using iterator_category = std::input_iterator_tag;
		using value_type = DataGridCellInfo;
		using difference_type = std::ptrdiff_t;
		using pointer = void;
		using reference = DataGridCellInfo;

		const_iterator() noexcept = default;
		DataGridCellInfo operator*() const;
		const_iterator& operator++() noexcept
		{
			++_index;
			return *this;
		}
		const_iterator operator++(int) noexcept
		{
			auto previous = *this;
			++*this;
			return previous;
		}
		bool operator==(const const_iterator& other) const noexcept
		{
			return _owner == other._owner && _index == other._index;
		}

	private:
		const_iterator(
			const DataGridSelectedCellCollection* owner,
			size_t index,
			std::shared_ptr<const std::vector<RegionExcludedOffsets>>
				excludedOffsets = {}) noexcept
			: _owner(owner), _index(index),
			  _excludedOffsets(std::move(excludedOffsets)) {}

		const DataGridSelectedCellCollection* _owner = nullptr;
		size_t _index = 0;
		std::shared_ptr<const std::vector<RegionExcludedOffsets>>
			_excludedOffsets;
		friend class DataGridSelectedCellCollection;
	};

	DataGridSelectedCellCollection() noexcept = default;
	[[nodiscard]] size_t size() const noexcept;
	[[nodiscard]] bool empty() const noexcept { return size() == 0; }
	[[nodiscard]] DataGridCellInfo at(size_t index) const;
	[[nodiscard]] DataGridCellInfo operator[](size_t index) const
	{
		return at(index);
	}
	[[nodiscard]] DataGridCellInfo front() const { return at(0); }
	[[nodiscard]] const_iterator begin() const;
	[[nodiscard]] const_iterator end() const noexcept
	{
		return const_iterator(this, size());
	}
	[[nodiscard]] bool Contains(
		const DataGridCellInfo& candidate) const noexcept;
	[[nodiscard]] bool SameLogicalCells(
		const DataGridSelectedCellCollection& other) const noexcept;
	/** White-box scale probe; not part of the XAML/application contract. */
	[[nodiscard]] size_t RetiredSnapshotRowIntervalCountForTesting()
		const noexcept
	{
		size_t result = 0;
		for (const auto& region : _regions)
			result += region.ExcludedSnapshotRowIntervals.size();
		return result;
	}
	[[nodiscard]] size_t RetiredSnapshotRowIdentityCountForTesting()
		const noexcept
	{
		size_t result = 0;
		for (const auto& region : _regions)
			result += region.ExcludedRows.size();
		return result;
	}

private:
	struct CellRegion final
	{
		size_t Top = 0;
		size_t Height = 0;
		std::vector<DataGridColumn*> Columns;
		std::vector<size_t> ColumnIndices;
		struct ExcludedRow final
		{
			BindingSourceReference Item;
			size_t Occurrence = DataGridCellInfo::InvalidIndex;
			size_t SnapshotRow = DataGridCellInfo::InvalidIndex;
		};
		struct ExcludedSnapshotRowInterval final
		{
			size_t Start = 0;
			size_t Count = 0;
		};
		// Empty means every row in [Top, Top + Height). A non-empty vector is a
		// sparse, sorted row projection into that immutable snapshot range.
		std::vector<size_t> IncludedRowOffsets;
		// Sorted, disjoint absolute ranges in the immutable region snapshot.
		// Mutable Remove/Replace retires a continuous old range as one entry
		// instead of retaining one BindingSourceReference per removed row.
		std::vector<ExcludedSnapshotRowInterval>
			ExcludedSnapshotRowIntervals;
		std::vector<ExcludedRow> ExcludedRows;
		bool UseSnapshotLocators = false;
	};

	ControlWeakReference _ownerLifetime;
	bool _ownerBound = false;
	BindingListReference _ownerSourceIdentity;
	BindingListReference _source;
	std::vector<DataGridCellInfo> _cells;
	std::vector<CellRegion> _regions;
	std::vector<DataGridCellInfo> _excludedCells;
	mutable std::shared_ptr<const std::vector<RegionExcludedOffsets>>
		_excludedOffsetsCache;

	explicit DataGridSelectedCellCollection(DataGrid* owner) noexcept;
	void SetSparse(
		DataGrid* owner,
		BindingListReference source,
		std::vector<DataGridCellInfo> cells) noexcept;
	void SetFullRegion(
		DataGrid* owner,
		BindingListReference source,
		size_t rowCount,
		std::vector<DataGridColumn*> columns) noexcept;
	[[nodiscard]] bool IsRegionBacked() const noexcept
	{
		return !_regions.empty();
	}
	[[nodiscard]] bool TryCreateRegionCell(
		const CellRegion& region,
		size_t rowOffset,
		size_t columnOffset,
		DataGridCellInfo& result) const;
	[[nodiscard]] RegionExcludedOffsets ExcludedOffsetsForRegion(
		const CellRegion& region) const;
	[[nodiscard]] std::shared_ptr<
		const std::vector<RegionExcludedOffsets>> ExcludedOffsets() const;
	void InvalidateExcludedOffsets() const noexcept
	{
		_excludedOffsetsCache.reset();
	}
	[[nodiscard]] DataGridCellInfo AtWithExcludedOffsets(
		size_t index,
		const std::vector<RegionExcludedOffsets>& excludedOffsets) const;
	[[nodiscard]] DataGridSelectedCellCollection Difference(
		const DataGridSelectedCellCollection& other) const;
	void PruneColumns(const std::vector<std::unique_ptr<DataGridColumn>>& columns);
	bool Exclude(const DataGridCellInfo& cell);
	bool Include(const DataGridCellInfo& cell);

	friend class DataGrid;
};

struct DataGridSortingEventArgs final : EventArgs
{
	DataGridColumn* Column = nullptr;
	CollectionSortDirection Direction = CollectionSortDirection::Ascending;
	bool MultiColumn = false;
	bool Handled = false;
};

struct DataGridBeginningEditEventArgs final : EventArgs
{
	DataGridColumn* Column = nullptr;
	DataGridRow* Row = nullptr;
	DataGridCell* Cell = nullptr;
	/** Non-owning input which initiated the synchronous edit transaction. */
	const RoutedEventArgs* EditingEventArgs = nullptr;
	bool Cancel = false;
};

struct DataGridPreparingCellForEditEventArgs final : EventArgs
{
	DataGridColumn* Column = nullptr;
	DataGridRow* Row = nullptr;
	DataGridCell* Cell = nullptr;
	Control* EditingElement = nullptr;
	/** Same non-owning activation payload exposed by BeginningEdit. */
	const RoutedEventArgs* EditingEventArgs = nullptr;
};

struct DataGridCellEditEndingEventArgs final : EventArgs
{
	DataGridColumn* Column = nullptr;
	DataGridRow* Row = nullptr;
	DataGridCell* Cell = nullptr;
	Control* EditingElement = nullptr;
	DataGridEditAction EditAction = DataGridEditAction::Commit;
	bool Cancel = false;
};

struct DataGridCurrentCellChangedEventArgs final : EventArgs
{
	DataGridCellInfo OldCell;
	DataGridCellInfo NewCell;
};

/** Net cell-selection changes published after container state is synchronized. */
struct DataGridSelectedCellsChangedEventArgs final : EventArgs
{
	DataGridSelectedCellCollection AddedCells;
	DataGridSelectedCellCollection RemovedCells;
};

/**
 * Non-visual WPF column definition. The row item is supplied only when a cell
 * visual is built; a column never inherits or evaluates DataContext itself.
 */
class DataGridColumn : public DependencyObject
{
public:
	virtual ~DataGridColumn() = default;

	const BindingValue& GetHeader() const noexcept { return _header; }
	void SetHeader(BindingValue value);
	const DataGridLength& GetWidth() const noexcept { return _width; }
	void SetWidth(DataGridLength value);
	double GetMinWidth() const noexcept { return _minWidth; }
	void SetMinWidth(double value);
	double GetMaxWidth() const noexcept { return _maxWidth; }
	void SetMaxWidth(double value);
	bool GetIsReadOnly() const noexcept { return _isReadOnly; }
	void SetIsReadOnly(bool value);
	bool GetCanUserSort() const noexcept { return _canUserSort; }
	void SetCanUserSort(bool value);
	bool GetCanUserResize() const noexcept { return _canUserResize; }
	void SetCanUserResize(bool value);

#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& GetSortMemberPath() const noexcept
	{
		return _sortMemberPath;
	}
	void SetSortMemberPath(std::wstring value);
#endif
	CompiledBindingPathView GetCompiledSortMemberPath() const noexcept
	{
		return _compiledSortMemberPath;
	}
	void SetCompiledSortMemberPath(CompiledBindingPathView value);
	std::optional<CollectionSortDirection> GetSortDirection() const noexcept
	{
		return _sortDirection;
	}
	bool GetIsAutoGenerated() const noexcept { return _isAutoGenerated; }
	DataGrid* GetDataGridOwner() const noexcept { return _owner; }

protected:
	virtual std::unique_ptr<Control> GenerateElement(
		DataGridCell& cell,
		const BindingSourceReference& item) const = 0;
	virtual std::unique_ptr<Control> GenerateEditingElement(
		DataGridCell& cell,
		const BindingSourceReference& item) const = 0;
	void NotifyOwnerChanged();

private:
	friend class DataGrid;
	friend class DataGridCell;
	friend class DataGridColumnHeader;
	friend class DataGridRow;
	struct RuntimeWidthState final
	{
		double Desired = (std::numeric_limits<double>::quiet_NaN)();
		double Display = (std::numeric_limits<double>::quiet_NaN)();
		bool HasDisplayOverride = false;
	};
	BindingValue _header;
	DataGridLength _width = DataGridLength::SizeToHeader();
	// DataGridLength remains the declaration-facing value. Interactive resize
	// keeps WPF's desired/display values privately so Star columns retain their
	// unit and compensating columns do not have their declarations rewritten.
	RuntimeWidthState _runtimeWidth;
	double _minWidth = 20.0;
	double _maxWidth = (std::numeric_limits<double>::infinity)();
	bool _isReadOnly = false;
	bool _canUserSort = true;
	bool _canUserResize = true;
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _sortMemberPath;
#endif
	CompiledBindingPathView _compiledSortMemberPath;
	std::optional<CollectionSortDirection> _sortDirection;
	DataGrid* _owner = nullptr;
	uint32_t _accessibilityIdentity = 0;
	bool _isAutoGenerated = false;
	bool _isRuntimeAutoGenerated = false;
};

class DataGridBoundColumn : public DataGridColumn
{
public:
#if CUI_ENABLE_DYNAMIC_XAML
	const std::wstring& GetBindingPath() const noexcept
	{
		return _bindingPath;
	}
	void SetBindingPath(std::wstring value);
#endif
	CompiledBindingPathView GetCompiledBindingPath() const noexcept
	{
		return _compiledBindingPath;
	}
	void SetCompiledBindingPath(CompiledBindingPathView value);
	BindingMode GetBindingMode() const noexcept { return _bindingMode; }
	void SetBindingMode(BindingMode value);
	DataSourceUpdateMode GetDataSourceUpdateMode() const noexcept
	{
		return _dataSourceUpdateMode;
	}
	void SetDataSourceUpdateMode(DataSourceUpdateMode value);
	const std::shared_ptr<const IBindingValueConverter>&
		GetBindingConverter() const noexcept { return _converter; }
	void SetBindingConverter(
		std::shared_ptr<const IBindingValueConverter> value);
	const std::optional<BindingValue>& GetFallbackValue() const noexcept
	{
		return _fallbackValue;
	}
	void SetFallbackValue(std::optional<BindingValue> value);
	const std::optional<BindingValue>& GetTargetNullValue() const noexcept
	{
		return _targetNullValue;
	}
	void SetTargetNullValue(std::optional<BindingValue> value);
	const std::optional<BindingValue>& GetConverterParameter() const noexcept
	{
		return _converterParameter;
	}
	void SetConverterParameter(std::optional<BindingValue> value);
	const std::optional<std::wstring>& GetStringFormat() const noexcept
	{
		return _stringFormat;
	}
	void SetStringFormat(std::optional<std::wstring> value);

protected:
	Binding* ApplyBinding(
		Control& target,
		const DependencyProperty& targetProperty,
		const BindingSourceReference& item,
		BindingMode defaultMode,
		bool forceMode = false) const;

private:
#if CUI_ENABLE_DYNAMIC_XAML
	std::wstring _bindingPath;
#endif
	CompiledBindingPathView _compiledBindingPath;
	BindingMode _bindingMode = BindingMode::Default;
	DataSourceUpdateMode _dataSourceUpdateMode = DataSourceUpdateMode::Default;
	std::shared_ptr<const IBindingValueConverter> _converter;
	std::optional<BindingValue> _fallbackValue;
	std::optional<BindingValue> _targetNullValue;
	std::optional<BindingValue> _converterParameter;
	std::optional<std::wstring> _stringFormat;
};

class DataGridTextColumn final : public DataGridBoundColumn
{
protected:
	std::unique_ptr<Control> GenerateElement(
		DataGridCell& cell,
		const BindingSourceReference& item) const override;
	std::unique_ptr<Control> GenerateEditingElement(
		DataGridCell& cell,
		const BindingSourceReference& item) const override;
};

class DataGridCheckBoxColumn final : public DataGridBoundColumn
{
public:
	bool GetIsThreeState() const noexcept { return _isThreeState; }
	void SetIsThreeState(bool value);

protected:
	std::unique_ptr<Control> GenerateElement(
		DataGridCell& cell,
		const BindingSourceReference& item) const override;
	std::unique_ptr<Control> GenerateEditingElement(
		DataGridCell& cell,
		const BindingSourceReference& item) const override;

private:
	bool _isThreeState = false;
};

class DataGridTemplateColumn final : public DataGridColumn
{
public:
	ItemTemplateReference GetCellTemplate() const noexcept
	{
		return _cellTemplate;
	}
	void SetCellTemplate(ItemTemplateReference value);
	ItemTemplateReference GetCellEditingTemplate() const noexcept
	{
		return _cellEditingTemplate;
	}
	void SetCellEditingTemplate(ItemTemplateReference value);

protected:
	std::unique_ptr<Control> GenerateElement(
		DataGridCell& cell,
		const BindingSourceReference& item) const override;
	std::unique_ptr<Control> GenerateEditingElement(
		DataGridCell& cell,
		const BindingSourceReference& item) const override;

private:
	ItemTemplateReference _cellTemplate;
	ItemTemplateReference _cellEditingTemplate;
};

class DataGridCell : public ContentControl
{
public:
	DataGridCell();
	UIClass Type() override { return UIClass::UI_DataGridCell; }
	static const DependencyProperty& IsSelectedProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	bool GetIsSelected() const noexcept { return _isSelected; }
	void SetIsSelected(bool value);
	bool GetIsEditing() const noexcept { return _isEditing; }
	bool GetIsReadOnly() const noexcept;
	DataGridColumn* GetColumn() const noexcept { return _column; }
	DataGridRow* GetRowOwner() const noexcept { return _row; }
	Control* GetEditingElement() const noexcept { return _editingElement; }
	bool BeginEdit();
	bool CommitEdit();
	bool CancelEdit();

protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	bool DefaultSelectOnLeftButtonDown() const override { return false; }
	bool ProcessInput(const InputReport& input) override;
	void OnRender() override;

private:
	friend class DataGrid;
	friend class DataGridCellsPresenter;
	friend class DataGridRow;
	bool Initialize(
		DataGridRow& row,
		DataGridColumn& column,
		const BindingSourceReference& item,
		size_t columnIndex,
		std::wstring* outError);
	bool ReplaceContent(bool editing, std::wstring* outError = nullptr);
	void ApplyIsSelectedValue(bool value);
	void SetCurrentIsSelected(
		bool value, bool suppressRoutedEvents = false);
	DataGridRow* _row = nullptr;
	DataGridColumn* _column = nullptr;
	BindingSourceReference _item;
	size_t _columnIndex = DataGridCellInfo::InvalidIndex;
	Control* _editingElement = nullptr;
	bool _isSelected = false;
	bool _synchronizingIsSelected = false;
	bool _suppressIsSelectedRoutedEvents = false;
	bool _isEditing = false;
};

class DataGridRow : public ListBoxItem
{
public:
	DataGridRow();
	UIClass Type() override { return UIClass::UI_DataGridRow; }
	DataGrid* GetDataGridOwner() const noexcept;
	const BindingSourceReference& GetItem() const noexcept { return _item; }
	DataGridCell* GetCell(size_t columnIndex) const noexcept;
	DataGridRowHeader* GetRowHeader() const noexcept { return _rowHeader; }
	// Enumerates realized containers.  With column virtualization enabled this
	// is the contiguous viewport strip, not a logical ColumnCount-sized array.
	std::span<DataGridCell* const> GetCells() const noexcept
	{
		return { _cells.data(), _cells.size() };
	}

private:
	friend class DataGrid;
	friend class DataGridCellsPresenter;
	bool Initialize(
		DataGrid& owner,
		const BindingSourceReference& item,
		size_t index,
		std::wstring* outError);
	bool RefreshRealizedColumns(
		size_t begin, size_t end, std::wstring* outError);
	void UpdateColumnWidths(bool propagateLayoutInvalidation = true);
	void UpdateRowHeader();
	void UpdateHorizontalScrollOffset(double offset);
	ControlWeakReference _ownerLifetime;
	BindingSourceReference _item;
	Grid* _rowLayoutGrid = nullptr;
	Grid* _rowHeaderHost = nullptr;
	DataGridCellsPresenter* _cellsGrid = nullptr;
	DataGridRowHeader* _rowHeader = nullptr;
	std::vector<DataGridCell*> _cells;
	size_t _appliedColumnWidthProjectionRevision = 0;
	size_t _appliedCellSelectionRevision = 0;
	bool _rowHeaderProjectionInitialized = false;
	bool _appliedRowHeaderVisible = false;
	bool _appliedRowHeaderAutoWidth = false;
	double _appliedRowHeaderWidth = 0.0;
	double _appliedHorizontalScrollOffset =
		(std::numeric_limits<double>::quiet_NaN)();
	bool _columnStorageIsSparse = false;
	size_t _realizedColumnBegin = 0;
	size_t _realizedColumnEnd = 0;
};

/** WPF-shaped row selector surface hosted by every realized DataGridRow. */
class DataGridRowHeader : public Button
{
public:
	DataGridRowHeader();
	UIClass Type() override { return UIClass::UI_DataGridRowHeader; }
	static const DependencyProperty& IsRowSelectedProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	bool GetIsRowSelected() const noexcept { return _isRowSelected; }
	DataGridRow* GetRowOwner() const noexcept;

protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	cui::core::Size FinalizeMeasureCore(
		cui::core::Size intrinsic,
		const cui::core::Constraints& available) override;
	bool ProcessInput(const InputReport& input) override;
	bool OnClick() override;

private:
	friend class DataGrid;
	friend class DataGridRow;
	static const DependencyPropertyKey& IsRowSelectedPropertyKey();
	void Initialize(DataGridRow& row);
	void InvalidateSharedWidthMeasure();
	void SetCurrentIsRowSelected(bool value);
	ControlWeakReference _rowLifetime;
	ModifierKeys _activationModifiers = ModifierKeys::None;
	bool _isRowSelected = false;
};

class DataGridColumnHeader : public Button
{
public:
	DataGridColumnHeader();
	UIClass Type() override { return UIClass::UI_DataGridColumnHeader; }
	DataGridColumn* GetColumn() const noexcept { return _column; }

protected:
	bool ProcessInput(const InputReport& input) override;
	bool OnClick() override;
	CursorKind QueryCursor(int localX, int localY) override;

private:
	friend class DataGridColumnHeadersPresenter;
	void Initialize(DataGrid& owner, DataGridColumn& column, size_t index);
	bool TryResolveResizeColumn(int localX, size_t& columnIndex) const noexcept;
	bool BeginColumnResize(int localX);
	bool ContinueColumnResize(int localX);
	void EndColumnResize(bool cancel);
	DataGrid* _owner = nullptr;
	DataGridColumn* _column = nullptr;
	size_t _columnIndex = DataGridCellInfo::InvalidIndex;
	bool _multiColumnSortRequested = false;
	bool _isResizing = false;
	size_t _resizingColumnIndex = DataGridCellInfo::InvalidIndex;
	double _resizeStartRenderX = 0.0;
	double _resizeStartWidth = 0.0;
};

class DataGridColumnHeadersPresenter : public Grid
{
public:
	DataGridColumnHeadersPresenter();
	UIClass Type() override
	{
		return UIClass::UI_DataGridColumnHeadersPresenter;
	}
	bool Initialize(DataGrid& owner, std::wstring* outError = nullptr);
	DataGrid* GetDataGridOwner() const noexcept { return _owner; }
	DataGridColumnHeader* GetHeader(size_t columnIndex) const noexcept
	{
		if (!_columnStorageIsSparse)
			return columnIndex < _headers.size() ? _headers[columnIndex] : nullptr;
		return columnIndex >= _realizedColumnBegin
			&& columnIndex < _realizedColumnEnd
			&& columnIndex - _realizedColumnBegin < _headers.size()
			? _headers[columnIndex - _realizedColumnBegin] : nullptr;
	}
	size_t GetRealizedHeaderSlotCount() const noexcept
	{
		return _headers.size();
	}
	void UpdateColumnWidths(bool propagateLayoutInvalidation = true);

protected:
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;

private:
	friend class DataGrid;
	bool RefreshRealizedColumns(
		size_t begin, size_t end, std::wstring* outError);
	DataGrid* _owner = nullptr;
	std::vector<DataGridColumnHeader*> _headers;
	size_t _appliedColumnWidthProjectionRevision = 0;
	bool _columnStorageIsSparse = false;
	size_t _realizedColumnBegin = 0;
	size_t _realizedColumnEnd = 0;
};

/**
 * WPF-shaped tabular selector. Row selection and item virtualization remain
 * owned by ListBox/ItemsControl; this type adds non-visual columns, cell
 * composition, sorting, and cell editing.
 */
class DataGrid : public ListBox
{
public:
	DataGrid();
	UIClass Type() override { return UIClass::UI_DataGrid; }

	static const DependencyProperty& AutoGenerateColumnsProperty();
	static const DependencyProperty& IsReadOnlyProperty();
	static const DependencyProperty& CanUserSortColumnsProperty();
	static const DependencyProperty& CanUserResizeColumnsProperty();
	static const DependencyProperty& EnableColumnVirtualizationProperty();
	static const DependencyProperty& SelectionUnitProperty();
	static const DependencyProperty& ColumnHeaderHeightProperty();
	static const DependencyProperty& RowHeaderWidthProperty();
	static const DependencyProperty& RowHeaderActualWidthProperty();
	static const DependencyProperty& RowHeightProperty();
	static const DependencyProperty& HeadersVisibilityProperty();
	static const DependencyProperty& GridLinesVisibilityProperty();
	static const DependencyProperty& RowBackgroundProperty();
	static const DependencyProperty& AlternatingRowBackgroundProperty();
	static const DependencyProperty& HorizontalGridLinesBrushProperty();
	static const DependencyProperty& VerticalGridLinesBrushProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif

	BindingListReference GetItemsSource() const noexcept override
	{
		return _source;
	}
	void SetItemsSource(BindingListReference value) override;

	bool GetAutoGenerateColumns() const noexcept { return _autoGenerateColumns; }
	void SetAutoGenerateColumns(bool value);
	bool GetIsReadOnly() const noexcept { return _isReadOnly; }
	void SetIsReadOnly(bool value);
	bool GetCanUserSortColumns() const noexcept { return _canUserSortColumns; }
	void SetCanUserSortColumns(bool value);
	bool GetCanUserResizeColumns() const noexcept { return _canUserResizeColumns; }
	void SetCanUserResizeColumns(bool value);
	bool GetEnableColumnVirtualization() const noexcept
	{
		return _enableColumnVirtualization;
	}
	void SetEnableColumnVirtualization(bool value);
	DataGridSelectionUnit GetSelectionUnit() const noexcept
	{
		return _selectionUnit;
	}
	void SetSelectionUnit(DataGridSelectionUnit value);
	double GetColumnHeaderHeight() const noexcept { return _columnHeaderHeight; }
	void SetColumnHeaderHeight(double value);
	double GetRowHeaderWidth() const noexcept { return _rowHeaderWidth; }
	void SetRowHeaderWidth(double value);
	double GetRowHeaderActualWidth() const noexcept
	{
		return _rowHeaderActualWidth;
	}
	double GetRowHeight() const noexcept { return _rowHeight; }
	void SetRowHeight(double value);
	DataGridHeadersVisibility GetHeadersVisibility() const noexcept
	{
		return _headersVisibility;
	}
	void SetHeadersVisibility(DataGridHeadersVisibility value);
	DataGridGridLinesVisibility GetGridLinesVisibility() const noexcept
	{
		return _gridLinesVisibility;
	}
	void SetGridLinesVisibility(DataGridGridLinesVisibility value);
	const cui::drawing::Brush& GetRowBackground() const noexcept
	{
		return _rowBackground;
	}
	void SetRowBackground(cui::drawing::Brush value);
	const cui::drawing::Brush& GetAlternatingRowBackground() const noexcept
	{
		return _alternatingRowBackground;
	}
	void SetAlternatingRowBackground(cui::drawing::Brush value);
	const cui::drawing::Brush& GetHorizontalGridLinesBrush() const noexcept
	{
		return _horizontalGridLinesBrush;
	}
	void SetHorizontalGridLinesBrush(cui::drawing::Brush value);
	const cui::drawing::Brush& GetVerticalGridLinesBrush() const noexcept
	{
		return _verticalGridLinesBrush;
	}
	void SetVerticalGridLinesBrush(cui::drawing::Brush value);

	DataGridColumn* AddColumn(std::unique_ptr<DataGridColumn> column);
	/** AOT schema expansion entry; marks the supplied column as generated. */
	DataGridColumn* AddAutoGeneratedColumn(
		std::unique_ptr<DataGridColumn> column);
	/** Takes ownership of an unattached column produced by a materializer. */
	DataGridColumn* AdoptColumn(DataGridColumn* column);
	void ClearColumns();
	size_t ColumnCount() const noexcept { return _columns.size(); }
	DataGridColumn* GetColumn(size_t index) const noexcept;

	template<typename TColumn, typename... TArgs>
	TColumn* AddColumn(TArgs&&... args)
	{
		static_assert(std::is_base_of_v<DataGridColumn, TColumn>);
		auto column = std::make_unique<TColumn>(
			std::forward<TArgs>(args)...);
		auto* raw = column.get();
		(void)AddColumn(std::move(column));
		return raw;
	}

	const DataGridCellInfo& GetCurrentCell() const noexcept
	{
		return _currentCell;
	}
	bool SetCurrentCell(size_t rowIndex, size_t columnIndex);
	const DataGridSelectedCellCollection& GetSelectedCells() const noexcept
	{
		return _selectedCells;
	}
	bool IsCellSelected(size_t rowIndex, size_t columnIndex) const;
	bool SelectCell(size_t rowIndex, size_t columnIndex);
	bool UnselectCell(size_t rowIndex, size_t columnIndex);
	void SelectAllCells();
	void UnselectAllCells();
	bool BeginEdit();
	bool BeginEdit(const RoutedEventArgs* editingEventArgs);
	bool CommitEdit();
	bool CancelEdit();
	bool PerformSort(DataGridColumn& column, bool multiColumn);
	bool ResizeColumn(size_t columnIndex, double pixelWidth);
	std::unique_ptr<DataGridColumnHeadersPresenter>
		CreateColumnHeadersPresenter();
	DataGridColumnHeadersPresenter* GetColumnHeadersPresenter() const noexcept
	{
		return dynamic_cast<DataGridColumnHeadersPresenter*>(
			_headersPresenter.Get());
	}
	Button* GetSelectAllButton() const noexcept
	{
		return dynamic_cast<Button*>(_selectAllButton.Get());
	}

	/** WPF DataGridAutomationPeer logical Grid/Table/Selection surface. */
	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result);
	size_t GetAccessibilityVirtualChildCount(uint32_t parentId) const;
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result) const;
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next,
		uint32_t& result) const;
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result) const;
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) const;
	bool GetAccessibilityVirtualItemAt(
		int row, int column, uint32_t& result) const;
	void GetAccessibilityVirtualColumnHeaders(
		std::vector<uint32_t>& result) const;
	void GetAccessibilityVirtualRowHeaders(
		std::vector<uint32_t>& result) const;
	AutomationOperationResult FocusAccessibilityVirtualNode(uint32_t id);
	bool TryGetAccessibilityVirtualFocusedNode(uint32_t& result) const;
	bool InvokeAccessibilityVirtualNode(uint32_t id);
	bool SetAccessibilityVirtualNodeValue(
		uint32_t id, const std::wstring& value);
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action);
	bool ScrollAccessibilityVirtualNodeIntoView(uint32_t id);
	bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept;
	bool ScrollAccessibility(
		AccessibilityScrollAmount horizontal,
		AccessibilityScrollAmount vertical);
	bool SetAccessibilityScrollPercent(
		double horizontalPercent, double verticalPercent);

	Event<void(DataGrid*, DataGridSortingEventArgs&)> Sorting;
	Event<void(DataGrid*, DataGridBeginningEditEventArgs&)> BeginningEdit;
	Event<void(DataGrid*, DataGridPreparingCellForEditEventArgs&)>
		PreparingCellForEdit;
	Event<void(DataGrid*, DataGridCellEditEndingEventArgs&)> CellEditEnding;
	Event<void(DataGrid*, DataGridCurrentCellChangedEventArgs&)>
		CurrentCellChanged;
	Event<void(DataGrid*, DataGridSelectedCellsChangedEventArgs&)>
		SelectedCellsChanged;
	void Arrange(cui::core::Rect finalRect) override;

protected:
	void PrepareMeasureCore(
		const cui::core::Constraints& available) override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override;
	std::unique_ptr<ItemsSourceTransactionState>
		CaptureItemsSourceTransactionState() override;
	void RestoreItemsSourceTransactionState(
		ItemsSourceTransactionState& state) noexcept override;
	std::unique_ptr<Control> BuildGeneratedItem(
		const BindingSourceReference& item,
		size_t index,
		BindingPathObservation& observation) override;
	bool CanRecycleGeneratedItemAcrossIndices(
		const Control& visual, size_t oldIndex) const noexcept override;
	bool TryRebindGeneratedItemAcrossIndices(
		Control& visual,
		size_t oldIndex,
		size_t newIndex,
		const BindingSourceReference& item,
		BindingPathObservation& observation,
		std::wstring* outError) override;
	void OnBeforeGeneratedItemsPrepared() override;
	void OnGeneratedItemsRebuilt() override;
	void OnGeneratedItemsRealized() override;
	void OnItemsSourceTransactionCommitted() override;
	void OnItemsSourceCollectionChangePreparing(
		const CollectionChangedEventArgs& change,
		const BindingListReference& previousSnapshot) override;
	void OnItemsSourceCollectionChangeCommitted(
		const CollectionChangedEventArgs& change) override;
	void OnItemsSourceChanged(
		const BindingListReference& oldValue,
		const BindingListReference& newValue) override;
	void OnGeneratedItemIndexChanged(
		Control& visual, size_t oldIndex, size_t newIndex) override;
	void OnSelectedIndexChanged(int oldValue, int newValue) override;
	void OnSelectionChanged(SelectionChangedEventArgs& args) override;
	void OnControlTemplatePresentationChanged() override;
	void PreparePresentation() override;
	bool ShouldRealizeVirtualItemsWithoutViewport() const noexcept override
	{
		// A DataGrid commonly lives on a non-selected TabItem.  Until its
		// ScrollViewer supplies a finite viewport, realizing rows only creates a
		// complete off-screen cell tree that will be discarded during templating.
		return false;
	}
	bool UseVisibleOnlyRangeDuringVerticalThumbDrag() const noexcept override
	{
		return true;
	}
	float GetVirtualizedItemHeight() const noexcept override;
	double GetVirtualizedHorizontalExtent() const override;
	bool HandlesNavigationKey(Key key) const override;
	bool ApplyTextInput(const TextCompositionEventArgs& input) override;
	bool ProcessInput(const InputReport& input) override;

private:
	friend class DataGridSelectedCellCollection;
	struct DataGridItemsSourceTransactionState;
	struct ColumnResizeSnapshot final
	{
		DataGridColumn* Column = nullptr;
		DataGridLength Width;
		DataGridColumn::RuntimeWidthState RuntimeWidth;
		double Desired = (std::numeric_limits<double>::quiet_NaN)();
		double Display = (std::numeric_limits<double>::quiet_NaN)();
	};
	enum class AccessibilityNodeKind : uint8_t
	{
		ColumnHeader,
		Row,
		RowHeader,
		Cell,
	};
	struct AccessibilityColumnIdentity final
	{
		DataGridColumn* Column = nullptr;
		uint32_t ColumnIdentity = 0;
		uint32_t HeaderId = 0;
	};
	struct AccessibilityCellIdentity final
	{
		DataGridColumn* Column = nullptr;
		uint32_t ColumnIdentity = 0;
		size_t ColumnIndex = DataGridCellInfo::InvalidIndex;
		uint32_t Id = 0;
	};
	struct AccessibilityRowIdentity final
	{
		BindingSourceReference Item;
		size_t Occurrence = DataGridCellInfo::InvalidIndex;
		size_t ViewIndex = DataGridCellInfo::InvalidIndex;
		uint32_t RowId = 0;
		uint32_t HeaderId = 0;
		std::vector<AccessibilityCellIdentity> Cells;
	};
	struct AccessibilityNodeLocator final
	{
		AccessibilityNodeKind Kind = AccessibilityNodeKind::Row;
		size_t RowIndex = DataGridCellInfo::InvalidIndex;
		size_t ColumnIndex = DataGridCellInfo::InvalidIndex;
	};
	struct AccessibilityIdentityState final
	{
		std::vector<AccessibilityColumnIdentity> Columns;
		std::vector<AccessibilityRowIdentity> Rows;
		bool StructureChangePending = false;
	};
	friend class DataGridCell;
	friend class DataGridColumn;
	friend class DataGridBoundColumn;
	friend class DataGridCheckBoxColumn;
	friend class DataGridTemplateColumn;
	friend class DataGridTextColumn;
	friend class DataGridColumnHeader;
	friend class DataGridColumnHeadersPresenter;
	friend class DataGridCellsPresenter;
	friend class DataGridRow;
	friend class DataGridRowHeader;

	BindingListReference _source;
	std::shared_ptr<CollectionViewSource> _itemsView;
	std::vector<std::unique_ptr<DataGridColumn>> _columns;
	mutable std::vector<std::optional<GridLength>> _resolvedColumnWidths;
	std::vector<ColumnResizeSnapshot> _columnResizeSnapshot;
	std::vector<ColumnResizeSnapshot> _columnResizeWorkingSnapshot;
	size_t _columnResizeTransactionIndex = DataGridCellInfo::InvalidIndex;
	double _columnResizeLastRawWidth =
		(std::numeric_limits<double>::quiet_NaN)();
	double _columnResizeInputBias = 0.0;
	// Realized header/row grids cache the revision they projected. This keeps a
	// vertical viewport change from rebuilding identical column definitions.
	size_t _columnWidthProjectionRevision = 1;
	bool _columnWidthRefreshPending = false;
	size_t _columnWidthDirtyBegin = DataGridCellInfo::InvalidIndex;
	size_t _columnWidthDirtyEnd = DataGridCellInfo::InvalidIndex;
	std::vector<uint8_t> _columnWidthMeasureDirty;
	// Prefix[i] is the logical left edge of column i; Prefix.back() is the
	// complete data-column width. It turns viewport, bring-into-view and UIA
	// column positioning into O(log C)/O(1) operations for very wide grids.
	mutable std::vector<double> _columnWidthPrefix;
	// Content measurement is independent from viewport/Star resolution.  Keep
	// the sampled cell widths across row-only visual regeneration so an Auto
	// column does not reread the first 1000 source records for every row style
	// change.
	mutable std::vector<std::optional<double>> _sampledColumnContentWidths;
	mutable size_t _columnContentWidthCacheEpoch = 1;
	double _columnViewportWidth =
		(std::numeric_limits<double>::quiet_NaN)();
	mutable bool _accessibilityIdentitiesDirty = true;
	mutable bool _buildingAccessibilityIdentities = false;
	mutable size_t _accessibilityIdentityRevision = 1;
	mutable std::vector<AccessibilityColumnIdentity>
		_accessibilityColumns;
	mutable std::vector<AccessibilityRowIdentity> _accessibilityRows;
	mutable std::unordered_map<size_t, size_t>
		_accessibilityRowIndexLookup;
	mutable std::unordered_map<uint32_t, AccessibilityNodeLocator>
		_accessibilityNodeLookup;
	bool _accessibilityStructureChangePending = false;
	DataGridCellInfo _currentCell;
	DataGridSelectedCellCollection _selectedCells;
	std::optional<DataGridCellInfo> _selectionAnchor;
	std::vector<DataGridCellInfo> _selectionRangeBase;
	std::vector<std::pair<BindingSourceReference, size_t>>
		_selectedRowSnapshot;
	// Mirrors ListBox's compact row-selection shape.  FullRow SelectAll keeps
	// [0, Count) plus sparse holes here instead of duplicating one item/index
	// pair per selected row in _selectedRowSnapshot.
	SelectedIndexCollection _selectedRowIndexSnapshot;
	ControlWeakReference _headersPresenter;
	ControlWeakReference _selectAllButton;
	ControlWeakReference _scrollViewer;
	EventConnection _selectAllClick;
	EventConnection _dataGridScrollChanged;
	EventConnection _rowHeaderSourceChanged;
	double _horizontalScrollOffset = 0.0;
	bool _autoColumnsChangedDuringPreparation = false;
	bool _settingItemsSource = false;
	bool _currentCellChangeDeferred = false;
	DataGridCellInfo _deferredCurrentCellOld;
	bool _selectedCellsChangeDeferred = false;
	DataGridSelectedCellCollection _deferredSelectedCellsOld;
	bool _deferredSelectedCellsIgnoreLocators = false;
	size_t _selectedCellsApplyDepth = 0;
	DataGridSelectedCellCollection _selectedCellsApplyOld;
	bool _reconcilingSelectedCellLocators = false;
	bool _selectedCellsVisualRefreshPending = false;
	size_t _rowSelectionRevision = 1;
	// Selection.GetSelection validates a copied cell-selection shape against
	// this monotonic boundary after every extensible source/identity callback.
	// Unlike transaction state, a rollback never restores an older value.
	size_t _cellSelectionRevision = 1;
	bool _selectionRangeActive = false;
	bool _suppressRowSelectionCellSync = false;
	bool _replaceCellSelectionFromRows = false;
	bool _updatingCellSelectionVisuals = false;
	bool _autoGenerateColumns = true;
	bool _isReadOnly = false;
	bool _raisingBeginningEdit = false;
	bool _endingCellEdit = false;
	bool _canUserSortColumns = true;
	bool _canUserResizeColumns = true;
	bool _enableColumnVirtualization = false;
	size_t _realizedColumnBegin = DataGridCellInfo::InvalidIndex;
	size_t _realizedColumnEnd = DataGridCellInfo::InvalidIndex;
	bool _refreshingRealizedColumns = false;
	DataGridSelectionUnit _selectionUnit = DataGridSelectionUnit::FullRow;
	double _columnHeaderHeight =
		(std::numeric_limits<double>::quiet_NaN)();
	double _rowHeaderWidth =
		(std::numeric_limits<double>::quiet_NaN)();
	double _rowHeaderActualWidth = 0.0;
	size_t _rowHeaderWidthEpoch = 1;
	double _rowHeight = (std::numeric_limits<double>::quiet_NaN)();
	DataGridHeadersVisibility _headersVisibility =
		DataGridHeadersVisibility::All;
	DataGridGridLinesVisibility _gridLinesVisibility =
		DataGridGridLinesVisibility::All;
	cui::drawing::Brush _rowBackground;
	cui::drawing::Brush _alternatingRowBackground;
	cui::drawing::Brush _horizontalGridLinesBrush;
	cui::drawing::Brush _verticalGridLinesBrush;
	bool _preserveColumnContentWidthsDuringRowRebuild = false;

	void RefreshColumns();
	void RemoveAutoGeneratedColumns();
	void EnsureAutoGeneratedColumns();
	DataGridColumn* AddColumnCore(
		std::unique_ptr<DataGridColumn> column,
		bool autoGenerated);
	DataGridCell* ResolveCurrentCellContainer() const noexcept;
	DataGridRow* ResolveRow(size_t rowIndex) const noexcept;
	void InvalidateRows();
	void RefreshHeaderPresenter();
	void RefreshHeadersVisibility();
	void RefreshHorizontalScrollAlignment();
	bool RefreshRowHeaderActualWidthProjection();
	bool RefreshRowHeaderActualWidth();
	double ResolveRowHeaderWidth() const noexcept;
	static const DependencyPropertyKey& RowHeaderActualWidthPropertyKey();
	bool SetCurrentRowHeaderActualWidth(double value);
	void InvalidateRowHeaderWidthBaseline() noexcept;
	bool TryShareRowHeaderDesiredWidth(
		DataGridRowHeader& header, double desiredWidth, double& sharedWidth);
	bool HandleRowHeaderClick(
		DataGridRow& row, ModifierKeys modifiers);
	void HandleSelectAll();
	bool RaiseCurrentCellChanged(DataGridCellInfo previous);
	bool ReconcileCurrentCellColumn();
	bool TryCreateCellInfo(
		size_t rowIndex, size_t columnIndex, DataGridCellInfo& result) const;
	bool TryResolveItemOccurrence(
		const BindingSourceReference& item,
		size_t occurrence,
		size_t& rowIndex) const;
	bool TryGetItemOccurrenceAt(
		size_t rowIndex, size_t& occurrence) const;
	bool TryGetStableSelectedCellRegionSnapshot(
		BindingListReference& snapshot) const;
	void InvalidateItemOccurrenceCache() const noexcept;
	bool EnsureAccessibilityVirtualIdentities() const;
	AccessibilityIdentityState CaptureAccessibilityIdentityState() const;
	void RestoreAccessibilityIdentityState(
		AccessibilityIdentityState&& state) noexcept;
	void InvalidateAccessibilityVirtualIdentities() const noexcept;
	void PruneAccessibilityColumnIdentities() noexcept;
	void RequestAccessibilityStructureChanged();
	void FlushAccessibilityStructureChange();
	bool TryResolveAccessibilityVirtualNode(
		uint32_t id, AccessibilityNodeLocator& result) const;
	AccessibilityRowIdentity* EnsureAccessibilityRowIdentity(
		size_t rowIndex, bool ensureCells) const;
	AccessibilityCellIdentity* EnsureAccessibilityCellIdentity(
		size_t rowIndex, size_t columnIndex) const;
	AccessibilityRowIdentity* FindAccessibilityRowIdentity(
		size_t rowIndex) const noexcept;
	bool TryReadAccessibilityCellValue(
		size_t rowIndex, size_t columnIndex,
		std::wstring& result) const;
	bool ApplySelectedCells(std::vector<DataGridCellInfo> cells);
	bool ApplySelectedCellCollection(
		DataGridSelectedCellCollection cells);
	bool BeginEditCore(
		const RoutedEventArgs* editingEventArgs,
		bool toggleCheckBox);
	bool RaiseSelectedCellsChanged(
		const DataGridSelectedCellCollection& previous,
		bool ignoreLocators = false);
	bool RaiseSelectedCellsChangedCore(
		const DataGridSelectedCellCollection& previous,
		const DataGridSelectedCellCollection& current,
		bool ignoreLocators);
	bool RefreshSelectedCellContainers(
		bool suppressRoutedEvents = false,
		bool onlyStaleRows = false);
	void RefreshSelectedCellContainersAfterRollback() noexcept;
	bool ReconcileSelectedCells();
	bool SynchronizeSelectedCellsFromRows();
	bool SynchronizeRangeSelectedCellsFromRows(
		SelectedIndexCollection selectedIndices);
	void HandleRowSelectionChanged(SelectionChangedEventArgs& args);
	void OnSelectionUnitChanged(
		DataGridSelectionUnit oldValue, DataGridSelectionUnit newValue);
	void OnCellIsSelectedChanged(DataGridCell& cell, bool selected);
	bool ApplySelectionForCellInput(
		size_t rowIndex, size_t columnIndex, ModifierKeys modifiers,
		bool allowsRange, bool allowsToggle);
	bool DeselectAllRowsWithoutCellSync();
	void ResetSelectionRange() noexcept;
	void FlushCommittedItemsSourceState();
	void InvalidateColumnWidthCache() noexcept;
	void InvalidateColumnContentWidthCache() noexcept;
	void RedistributeRuntimeWidthsForViewportChange(
		double oldViewportWidth, double newViewportWidth) noexcept;
	bool RebaseColumnResizeTransaction();
	bool BeginColumnResizeTransaction(size_t columnIndex);
	bool ResizeColumnInTransaction(size_t columnIndex, double pixelWidth);
	void EndColumnResizeTransaction(bool cancel);
	bool ResizeColumnCore(
		size_t columnIndex, double pixelWidth,
		bool preserveRealizedColumnRange);
	void ApplyPendingColumnWidths(bool refreshVirtualMetrics = true);
	void CommitColumnWidthLayoutToAncestors();
	void InvalidatePendingColumnResizeVisual();
	void RefreshColumnWidths(bool preserveRealizedColumnRange = false);
	std::pair<size_t, size_t> ResolveRealizedColumnRange() const;
	void InvalidateRealizedColumnRange() noexcept;
	void RefreshRealizedColumns();
	bool BringColumnIntoView(size_t columnIndex);
	void UpdateColumnViewportWidth(double availableWidth);
	double ResolveColumnViewportWidth(double fallbackWidth) const noexcept;
	void RebuildResolvedColumnWidths() const;
	std::pair<size_t, size_t>
		ResolveDeferredColumnSampleRange() const;
	bool EnsureColumnWidthPrefix() const;
	bool TryResolveColumnBounds(
		size_t columnIndex, double& left, double& right) const;
	double GetColumnDisplayWidth(size_t columnIndex) const;
	bool AutoSizeColumn(size_t columnIndex);
	GridLength ResolveColumnGridLength(size_t columnIndex) const;
	double EstimateColumnWidth(size_t columnIndex, bool includeHeader,
		bool includeCells) const;
	CollectionSortDescription MakeSortDescription(
		const DataGridColumn& column,
		CollectionSortDirection direction) const;
	bool HandleCellKey(Key key, ModifierKeys modifiers);
};
