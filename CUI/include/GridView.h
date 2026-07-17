#pragma once
#include "Control.h"
#include "ObservableCollection.h"
#include <functional>
#include <set>
#include <unordered_map>
#include <vector>
#pragma comment(lib, "Imm32.lib")
typedef Event<void(class GridView*, int c, int r, bool v) > OnGridViewCheckStateChangedEvent;
typedef Event<void(class GridView*, int c, int r)> OnGridViewButtonClickEvent;
typedef Event<void(class GridView*, int c, int r, std::wstring text)> OnGridViewLinkedTextClickEvent;
typedef Event<void(class GridView*, int c, int r, int selectedIndex, std::wstring selectedText)> OnGridViewComboBoxSelectionChangedEvent;
typedef Event<void(class GridView*, bool& cancel)> OnGridViewUserAddingRowEvent;
typedef Event<void(class GridView*, int newRowIndex)> OnGridViewUserAddedRowEvent;
enum class ColumnType
{
	Text,
	Image,
	Check,
	Button,
	ComboBox,
	LinkedText,
};

/**
 * @file GridView.h
 * @brief GridView：表格控件（列定义 + 行数据 + 编辑/排序/滚动）。
 *
 * 特性概览：
 * - 多列类型：Text/Image/Check/Button/ComboBox/LinkedText
 * - 支持单元格编辑（文本/组合框）与按钮点击事件
 * - 支持列头点击排序（可为列配置 SortFunc）
 * - 支持平滑滚动（ScrollYOffset）与行级滚动（ScrollRowPosition）
 */

class CellValue;
class GridViewColumn
{
public:
	mutable uint32_t AccessibilityId = 0;
	/** @brief 列标题。 */
	std::wstring Name = L"";
	/** @brief 列宽（像素）。 */
	float Width = 120;
	/** @brief 列类型。 */
	ColumnType Type = ColumnType::Text;
	/** @brief 是否允许编辑。 */
	bool CanEdit = true;
	// ComboBox 列：下拉选项列表（当 Count>0 时默认选中第 0 项）
	std::vector<std::wstring> ComboBoxItems = std::vector<std::wstring>();
	// Button 列：按钮文字
	std::wstring ButtonText = L"";
	std::function<int(const CellValue& lhs, const CellValue& rhs)> SortFunc = nullptr;
	GridViewColumn(std::wstring name = L"", float width = 120.0F, ColumnType type = ColumnType::Text, bool canEdit = false);
	/**
	 * @brief 设置排序比较函数。
	 * @return 比较结果：<0 lhs<rhs，0 相等，>0 lhs>rhs。
	 */
	void SetSortFunc(std::function<int(const CellValue& lhs, const CellValue& rhs)> func)
	{
		SortFunc = std::move(func);
	}
};
class CellValue
{
public:
	mutable uint32_t AccessibilityId = 0;
	std::wstring Text;
	std::shared_ptr<BitmapSource> Image;
	Microsoft::WRL::ComPtr<ID2D1Bitmap> ImageCache;
	ID2D1RenderTarget* ImageCacheTarget = nullptr;
	const BitmapSource* ImageCacheSource = nullptr;
	__int64 Tag;
	CellValue();
	CellValue(std::wstring s);
	CellValue(wchar_t* s);
	CellValue(const wchar_t* s);
	CellValue(std::shared_ptr<BitmapSource> img);
	CellValue(__int64 tag);
	CellValue(bool tag);
	CellValue(__int32 tag);
	CellValue(unsigned __int32 tag);
	CellValue(unsigned __int64 tag);
	CellValue(PVOID tag);
	std::wstring GetText() const;
	void SetText(const std::wstring& text);
	__int64 GetTag() const;
	void SetTag(__int64 tag);
	bool GetBool() const;
	void SetBool(bool value);
	PVOID GetPointer() const;
	void SetPointer(PVOID value);
	void SetComboSelection(int selectedIndex, const std::wstring& selectedText);
	ID2D1Bitmap* GetImageBitmap(D2DGraphics* render);
};
class GridViewRow
{
public:
	mutable uint32_t AccessibilityId = 0;
	std::vector<CellValue> Cells = std::vector<CellValue>();
	CellValue& operator[](int index);
};
class GridView : public Control, public IAccessibilityVirtualizedControl
{
public:
	class UpdateScope
	{
	public:
		explicit UpdateScope(GridView& owner) noexcept;
		~UpdateScope();
		UpdateScope(const UpdateScope&) = delete;
		UpdateScope& operator=(const UpdateScope&) = delete;
		UpdateScope(UpdateScope&& other) noexcept;
		UpdateScope& operator=(UpdateScope&& other) noexcept;
		/** @brief 提前结束批量更新；可重复调用。 */
		void Commit() noexcept;

	private:
		GridView* _owner = nullptr;
	};

	UIClass Type();
	void EnsureBindingPropertiesRegistered() override;
	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override { return IsCaretBlinkAnimating(); }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override { return GetCaretBlinkInvalidRect(outRect); }
	GridView(int x = 0, int y = 0, int width = 120, int height = 20);
	~GridView() override;
	/** @brief 表头字体（为空则使用默认字体/继承字体）。 */
	class Font* HeadFont = nullptr;
	bool InScroll = false;
	bool InHScroll = false;
	ScrollChangedEvent ScrollChanged;
	using ColumnCollection = ObservableCollection<GridViewColumn>;
	using RowCollection = ObservableCollection<GridViewRow>;
	ColumnCollection Columns;
	RowCollection Rows;
	GridViewRow& operator[](int index);

#define CUI_GRID_VIEW_PROPERTY(type, name) \
	PROPERTY(type, name); \
	GET(type, name); \
	SET(type, name)

	CUI_GRID_VIEW_PROPERTY(bool, FullRowSelect);
	CUI_GRID_VIEW_PROPERTY(bool, AllowUserToAddRows);
	CUI_GRID_VIEW_PROPERTY(bool, AllowUserToDeleteRows);
	CUI_GRID_VIEW_PROPERTY(float, HeadHeight);
	CUI_GRID_VIEW_PROPERTY(float, RowHeight);
	CUI_GRID_VIEW_PROPERTY(float, BorderThickness);
	CUI_GRID_VIEW_PROPERTY(float, CellCornerRadius);
	CUI_GRID_VIEW_PROPERTY(float, CellHorizontalPadding);
	CUI_GRID_VIEW_PROPERTY(float, CellVerticalPadding);
	CUI_GRID_VIEW_PROPERTY(float, SelectedAccentWidth);
	CUI_GRID_VIEW_PROPERTY(float, EditTextMargin);
	CUI_GRID_VIEW_PROPERTY(float, ScrollBarSize);
	/** @brief 像素级垂直滚动偏移（用于平滑滚动/位置滚动）。 */
	CUI_GRID_VIEW_PROPERTY(float, ScrollYOffset);
	CUI_GRID_VIEW_PROPERTY(float, ScrollXOffset);
	CUI_GRID_VIEW_PROPERTY(int, ScrollRowPosition);
	CUI_GRID_VIEW_PROPERTY(int, SelectedColumnIndex);
	CUI_GRID_VIEW_PROPERTY(int, SelectedRowIndex);
	CUI_GRID_VIEW_PROPERTY(int, SortedColumnIndex);
	CUI_GRID_VIEW_PROPERTY(bool, SortAscending);
	CUI_GRID_VIEW_PROPERTY(int, UnderMouseColumnIndex);
	CUI_GRID_VIEW_PROPERTY(int, UnderMouseRowIndex);
	/** @brief 是否启用行级多选（Ctrl 切换 / Shift 范围）。 */
	CUI_GRID_VIEW_PROPERTY(bool, MultiSelect);

	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, HeadBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, HeadForeColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, HeadHoverBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, GridLineColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, AccentColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, ButtonBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, ButtonCheckedColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, ButtonHoverBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, ButtonPressedBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, ButtonBorderDarkColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, ButtonBorderLightColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, SelectedItemBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, SelectedItemForeColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, UnderMouseItemBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, UnderMouseItemForeColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, LinkedTextColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, LinkedTextHoverColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, ScrollBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, ScrollForeColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, EditBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, EditForeColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, EditSelectedBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, EditSelectedForeColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, NewRowBackColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, NewRowForeColor);
	CUI_GRID_VIEW_PROPERTY(D2D1_COLOR_F, NewRowIndicatorColor);

#undef CUI_GRID_VIEW_PROPERTY
	OnGridViewCheckStateChangedEvent OnGridViewCheckStateChanged;
	OnGridViewButtonClickEvent OnGridViewButtonClick;
	OnGridViewLinkedTextClickEvent OnGridViewLinkedTextClick;
	OnGridViewComboBoxSelectionChangedEvent OnGridViewComboBoxSelectionChanged;
	OnGridViewUserAddingRowEvent OnUserAddingRow;
	OnGridViewUserAddedRowEvent OnUserAddedRow;
	SelectionChangedEvent SelectionChanged;
	std::function<bool(GridView* sender, int columnIndex, bool ascending)> SortRequestHandler = nullptr;
	GridViewRow& SelectedRow();
	std::wstring& SelectedValue();
	/** @brief 清空列与行数据。 */
	void Clear();
	/** @brief 清空行数据（设计器兼容 API）。 */
	void ClearRows();
	/** @brief 清空列定义（设计器兼容 API）。 */
	void ClearColumns();
	/** @brief 添加一行（设计器兼容 API）。 */
	void AddRow(const GridViewRow& row);
	/**
	 * @brief 原子替换全部行；内部用一次批量更新，仅触发一次重排/重绘，
	 * 适合一次性加载大量数据（比逐行 AddRow 高效得多）。
	 */
	void SetRows(const std::vector<GridViewRow>& rows);
	void SetRows(std::vector<GridViewRow>&& rows);
	/** @brief 添加一列（设计器兼容 API）。 */
	void AddColumn(const GridViewColumn& column);
	/** @brief 行数（设计器兼容 API）。 */
	size_t RowCount() const;
	/** @brief 列数（设计器兼容 API）。 */
	size_t ColumnCount() const;
	/** @brief 获取指定行（设计器兼容 API）。 */
	GridViewRow& RowAt(int index);
	const GridViewRow& RowAt(int index) const;
	/** @brief 获取指定列（设计器兼容 API）。 */
	GridViewColumn& ColumnAt(int index);
	const GridViewColumn& ColumnAt(int index) const;
	/** @brief 交换行（设计器兼容 API）。 */
	void SwapRows(int indexA, int indexB);
	/** @brief 删除行（设计器兼容 API）。 */
	bool RemoveRowAt(int index);
	/** @brief 删除列及各行中对应的单元格。 */
	bool RemoveColumnAt(int index);
	/** @brief 安全获取单元格；坐标无效或该行尚无对应单元格时返回 nullptr。 */
	CellValue* GetCell(int col, int row) noexcept;
	const CellValue* GetCell(int col, int row) const noexcept;
	/** @brief 写入单元格；必要时自动扩展该行的 Cells。 */
	bool SetCellValue(int col, int row, const CellValue& value);
	/** @brief 选择单元格并可确保其可见。 */
	bool SelectCell(int col, int row, bool ensureVisible = true);
	/** @brief 选择指定行，沿用当前有效列或选择首列。 */
	bool SelectRow(int row, bool ensureVisible = true);
	/** @brief 清除当前选择；返回选择是否发生变化。 */
	bool ClearSelection();

	// ---- 多选 API（MultiSelect 启用时有效；单选时等价于仅焦点行） ----
	/** @brief 返回所有选中行索引（升序）。单选模式下返回焦点行（若有）。 */
	std::vector<int> GetSelectedRows() const;
	/** @brief 选中行数。 */
	int GetSelectedRowCount() const;
	/** @brief 指定行是否处于选中状态。 */
	bool IsRowSelected(int row) const;
	/** @brief 程序方式设置某行选中/取消（不改变焦点行），MultiSelect 下可用。 */
	bool SetRowSelected(int row, bool selected, bool ensureVisible = false);
	/** @brief 选中 [startRow, endRow] 范围（含端点），MultiSelect 下可用。 */
	bool SelectRowRange(int startRow, int endRow, bool ensureVisible = false);
	/** @brief 全选所有行，MultiSelect 下可用。 */
	void SelectAllRows();
	/** @brief 开始、提交或取消文本单元格编辑。 */
	bool BeginEdit(int col, int row);
	bool CommitEdit();
	bool CancelEdit();
	bool IsEditing() const noexcept { return Editing; }
	int GetEditingColumnIndex() const noexcept { return EditingColumnIndex; }
	int GetEditingRowIndex() const noexcept { return EditingRowIndex; }
	const std::wstring& GetEditingText() const noexcept { return EditingText; }
	bool SetEditingText(const std::wstring& text);
	void GetAccessibilityVirtualChildren(
		uint32_t parentId, std::vector<uint32_t>& result) override;
	bool TryGetAccessibilityVirtualNode(
		uint32_t id, AccessibilityVirtualNode& result) override;
	size_t GetAccessibilityVirtualChildCount(uint32_t parentId) override;
	bool TryGetAccessibilityVirtualChildAt(
		uint32_t parentId, size_t index, uint32_t& result) override;
	bool TryGetAccessibilityVirtualSibling(
		uint32_t parentId, uint32_t id, bool next, uint32_t& result) override;
	bool TryHitTestAccessibilityVirtualNode(
		float localX, float localY, uint32_t& result) override;
	/** Number of cell identities created by UIA GetItem/navigation/hit testing. */
	size_t MaterializedAccessibilityCellCount() const noexcept
	{
		return _accessibilityCellIds.size();
	}
	AccessibilityVirtualContainerInfo
		GetAccessibilityVirtualContainerInfo() const noexcept override;
	void GetAccessibilityVirtualSelection(
		std::vector<uint32_t>& result) override;
	bool GetAccessibilityVirtualItemAt(
		int row, int column, uint32_t& id) override;
	void GetAccessibilityVirtualColumnHeaders(
		std::vector<uint32_t>& result) override;
	bool InvokeAccessibilityVirtualNode(uint32_t id) override;
	bool ToggleAccessibilityVirtualNode(uint32_t id) override;
	bool SetAccessibilityVirtualNodeValue(
		uint32_t id, const std::wstring& value) override;
	bool SelectAccessibilityVirtualNode(
		uint32_t id, AccessibilitySelectionAction action) override;
	bool ScrollAccessibilityVirtualNodeIntoView(uint32_t id) override;
	bool GetAccessibilityScrollInfo(
		AccessibilityScrollInfo& result) const noexcept override;
	bool ScrollAccessibility(
		AccessibilityScrollAmount horizontal,
		AccessibilityScrollAmount vertical) override;
	bool SetAccessibilityScrollPercent(
		double horizontalPercent, double verticalPercent) override;
	/** @brief 开始/结束可嵌套的批量更新。最外层结束时只调整和重绘一次。 */
	void BeginUpdate() noexcept;
	void EndUpdate() noexcept;
	bool IsUpdating() const noexcept { return _updateDepth > 0; }
	[[nodiscard]] UpdateScope DeferUpdates() noexcept { return UpdateScope(*this); }
	/** @brief 切换编辑光标/选择到指定单元格。 */
	void ChangeEditionSelected(int col, int row);
	/** @brief 调整行集合大小。 */
	void ReSizeRows(int count);
	/** @brief 按指定列排序。 */
	void SortByColumn(int col, bool ascending = true);

	// ---- 运行时列管理 ----
	/** @brief 设置列可见性；隐藏列不参与渲染与命中，但保留数据与索引。 */
	bool SetColumnVisible(int col, bool visible);
	/** @brief 查询列可见性。 */
	bool IsColumnVisible(int col) const;
	/** @brief 将列从 fromIndex 移动到 toIndex，各行单元格同步移动。 */
	bool MoveColumn(int fromIndex, int toIndex);

protected:
	void OnComputedLayoutSizeChanged() override;

private:
	struct AccessibilityNodeLocation
	{
		enum class Kind { Header, Row, Cell } NodeKind = Kind::Row;
		size_t Row = 0;
		size_t Column = 0;
	};
	std::unordered_map<uint32_t, AccessibilityNodeLocation>
		_accessibilityNodeLocationById;
	std::unordered_map<uint64_t, uint32_t> _accessibilityCellIds;
	std::unordered_map<uint32_t, uint64_t> _accessibilityCellKeyById;
	bool _accessibilityIdsDirty = true;
	bool _fullRowSelect = true;
	bool _allowUserToAddRows = false;
	bool _allowUserToDeleteRows = true;
	float _headHeight = 0.0f;
	float _rowHeight = 0.0f;
	float _borderThickness = 1.5f;
	float _cellCornerRadius = 6.0f;
	float _cellHorizontalPadding = 8.0f;
	float _cellVerticalPadding = 3.0f;
	float _selectedAccentWidth = 3.0f;
	float _editTextMargin = 3.0f;
	float _scrollBarSize = 8.0f;
	float _scrollYOffset = 0.0f;
	float _scrollXOffset = 0.0f;
	int _scrollRowPosition = 0;
	int _selectedColumnIndex = -1;
	int _selectedRowIndex = -1;
	int _sortedColumnIndex = -1;
	bool _sortAscending = true;
	int _underMouseColumnIndex = -1;
	int _underMouseRowIndex = -1;
	bool _multiSelect = false;
	// 多选集合：行索引。焦点/锚点行仍由 _selectedRowIndex 表达（用于范围选择与编辑）。
	std::set<int> _selectedRows;
	int _selectionAnchorRow = -1;
	// 列可见性：隐藏列索引集合（保留数据与原始索引）。
	std::set<int> _hiddenColumns;
	D2D1_COLOR_F _headBackColor = cui::theme::palette::SurfaceMuted;
	D2D1_COLOR_F _headForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _headHoverBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _gridLineColor = cui::theme::palette::Border;
	D2D1_COLOR_F _accentColor = cui::theme::palette::Accent;
	D2D1_COLOR_F _buttonBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _buttonCheckedColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F _buttonHoverBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _buttonPressedBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F _buttonBorderDarkColor = cui::theme::palette::BorderStrong;
	D2D1_COLOR_F _buttonBorderLightColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _selectedItemBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F _selectedItemForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _underMouseItemBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F _underMouseItemForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _linkedTextColor = cui::theme::palette::Accent;
	D2D1_COLOR_F _linkedTextHoverColor = cui::theme::palette::AccentHover;
	D2D1_COLOR_F _scrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F _scrollForeColor = cui::theme::palette::ScrollThumb;
	D2D1_COLOR_F _editBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F _editForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _editSelectedBackColor = cui::theme::palette::SelectionBack;
	D2D1_COLOR_F _editSelectedForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F _newRowBackColor = cui::theme::palette::SurfaceSubtle;
	D2D1_COLOR_F _newRowForeColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F _newRowIndicatorColor = cui::theme::palette::Accent;

	float _vScrollThumbGrabOffsetY = 0.0f;
	float _hScrollThumbGrabOffsetX = 0.0f;
	struct ScrollLayout
	{
		bool NeedV = false;
		bool NeedH = false;
		float ScrollBarSize = 8.0f;
		float RenderWidth = 0.0f;   		float RenderHeight = 0.0f;  		float HeadHeight = 0.0f;
		float RowHeight = 0.0f;
		float TotalColumnsWidth = 0.0f;
		float ContentHeight = 0.0f;
		float TotalRowsHeight = 0.0f;
		float MaxScrollY = 0.0f;
		int VisibleRows = 0;
		int MaxScrollRow = 0;
		float MaxScrollX = 0.0f;
	};
	ScrollLayout CalcScrollLayout() const;
	float GetTotalColumnsWidth() const;
	POINT GetGridViewUnderMouseItem(int x, int y, GridView* ct);
	int HitTestHeaderColumn(int x, int y);
	int HitTestHeaderDivider(int x, int y);
	D2D1_RECT_F GetGridViewScrollBlockRect(GridView* ct);
	int GetGridViewRenderRowCount(GridView* ct);
	void DrawScroll();
	void DrawHScroll(const ScrollLayout& l);
	void DrawCorner(const ScrollLayout& l);
	void SetScrollByPos(float localY);
	void SetHScrollByPos(float localX);
	void HandleDropFiles(WPARAM wParam);
	void HandleMouseWheel(WPARAM wParam, int localX, int localY);
	void HandleMouseMove(int localX, int localY);
	void HandleLeftButtonDown(int localX, int localY);
	void HandleLeftButtonUp(int localX, int localY);
	void HandleLeftButtonDoubleClick(WPARAM wParam, int localX, int localY);
	void HandleKeyDown(WPARAM wParam);
	void HandleKeyUp(WPARAM wParam);
	void HandleCharInput(WPARAM wParam);
	void HandleImeComposition(LPARAM lParam);
	void HandleCellClick(int col, int row);
	void ToggleCheckState(int col, int row);
	void RaiseLinkedTextClick(int col, int row);
	void StartEditingCell(int col, int row);
	void EnsureComboBoxCellDefaultSelection(int col, int row);
	void ToggleDropDownEditor(int col, int row);
	void CloseDropDownEditor(bool immediate = false);
	void CancelEditing(bool revert = true);
	void SaveCurrentEditingCell(bool commit = true);
	void ResetEditingState() noexcept;
	void AdjustScrollPosition();
	void RequestRefresh(bool adjustScroll = true);
	void SetCurrentSelection(int col, int row, bool ensureVisible, bool raiseEvent = true);
	void OnSelectionPropertyChanged();
	void SetCurrentSelectedColumnIndex(int value);
	void SetCurrentSelectedRowIndex(int value);
	void SetCurrentUnderMouseColumnIndex(int value);
	void SetCurrentUnderMouseRowIndex(int value);
	void SetCurrentScrollYOffset(float value);
	void SetCurrentScrollXOffset(float value);
	void SetCurrentScrollRowPosition(int value);
	void SetCurrentSortedColumnIndex(int value);
	void SetCurrentSortAscending(bool value);
	bool CanScrollDown();
	void UpdateUnderMouseIndices(int localX, int localY);

	bool _resizingColumn = false;
	int _resizeColumnIndex = -1;
	float _resizeStartX = 0.0f;
	float _resizeStartWidth = 0.0f;
	float _minColumnWidth = 24.0f;
	unsigned int _updateDepth = 0;
	bool _updatePendingAdjustScroll = false;
	bool _updatePendingInvalidate = false;
	bool _selectionUpdateInProgress = false;
	bool _scrollUpdateInProgress = false;
	bool _collectionStructurePending = false;
	uint32_t _selectedRowAccessibilityId = 0;
	uint32_t _selectedColumnAccessibilityId = 0;

	bool Editing = false;
	int EditingColumnIndex = -1;
	int EditingRowIndex = -1;
	std::wstring EditingText;
	std::wstring EditingOriginalText;
	int EditSelectionStart = 0;
	int EditSelectionEnd = 0;
	float EditOffsetX = 0.0f;

	class DropDownPopup* _dropDownPopup = nullptr;
	int _dropDownPopupColumnIndex = -1;
	int _dropDownPopupRowIndex = -1;

	bool _buttonMouseDown = false;
	int _buttonDownColumnIndex = -1;
	int _buttonDownRowIndex = -1;

	bool _linkedTextMouseDown = false;
	int _linkedTextDownColumnIndex = -1;
	int _linkedTextDownRowIndex = -1;
	void EnsureAccessibilityIds();
	uint32_t EnsureAccessibilityCellId(size_t row, size_t column);
	void CaptureSelectionAccessibilityIds();
	void OnRowsCollectionChanged(const CollectionChangedEventArgs& change);
	void OnColumnsCollectionChanged(const CollectionChangedEventArgs& change);
	void NotifyCollectionStructureChanged();

	// 新行相关成员变量
	bool _isUnderNewRow = false;   // 鼠标是否在新行区域
	int _newRowAreaHitTest = -1;   // 新行区域的列索引 (-1表示不在新行区域)

	float GetRowHeightPx() const;
	float GetHeadHeightPx() const;
	bool TryGetCellRectLocal(int col, int row, D2D1_RECT_F& outRect);
	bool IsEditableTextCell(int col, int row);
	bool SetEditingCaretFromMousePoint(int localX, int localY);
	void EditInputText(const std::wstring& input);
	void EditInputBack();
	void EditInputDelete();
	void EditUpdateScroll(float cellWidth);
	int EditHitTestTextPosition(float cellWidth, float cellHeight, float x, float y);
	std::wstring EditGetSelectedString();
	void EditEnsureSelectionInRange();
	void EditSetImeCompositionWindow();

	// 新行相关方法
	bool IsNewRowArea(int x, int y);
	int HitTestNewRow(int x, int y, int& outColumnIndex);
	void AddNewRow();
public:
	void Update() override;
	/** @brief 根据内容自动调整某列宽度。 */
	void AutoSizeColumn(int col);
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
};
