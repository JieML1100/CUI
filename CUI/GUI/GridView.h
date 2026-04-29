#pragma once
#include "Control.h"
#include <functional>
#include <cstdint>
#include <list>
#include <unordered_map>
#include <variant>
#pragma comment(lib, "Imm32.lib")
typedef Event<void(class GridView*, int c, int r, bool v) > OnGridViewCheckStateChangedEvent;
typedef Event<void(class GridView*, int c, int r)> OnGridViewButtonClickEvent;
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
};

enum class CellValueKind
{
	Empty,
	Text,
	Image,
	Integer,
	Boolean,
	Pointer,
	ComboSelection,
};

/**
 * @file GridView.h
 * @brief GridView：表格控件（列定义 + 行数据 + 编辑/排序/滚动）。
 *
 * 特性概览：
 * - 多列类型：Text/Image/Check/Button/ComboBox
 * - 支持单元格编辑（文本/组合框）与按钮点击事件
 * - 支持列头点击排序（可为列配置 SortFunc）
 * - 支持平滑滚动（ScrollYOffset）与行级滚动（ScrollRowPosition）
 */

class CellValue;
class GridViewColumn
{
public:
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
	struct ComboSelectionValue
	{
		int SelectedIndex = -1;
		std::wstring Text;
	};
	struct TaggedTextValue
	{
		std::wstring Text;
		__int64 Tag = 0;
	};

	CellValueKind Kind = CellValueKind::Empty;
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
	void SetText(std::wstring text);
	std::shared_ptr<BitmapSource> GetImage() const;
	void SetImage(std::shared_ptr<BitmapSource> image);
	__int64 GetTag() const;
	void SetTag(__int64 tag);
	PVOID GetPointer() const;
	void SetPointer(PVOID pointer);
	bool GetBool() const;
	void SetBool(bool value);
	int GetComboIndex() const;
	void SetComboSelection(int selectedIndex, const std::wstring& selectedText);
private:
	using Storage = std::variant<std::monostate, std::wstring, std::shared_ptr<BitmapSource>, __int64, bool, PVOID, ComboSelectionValue, TaggedTextValue>;
	Storage _storage;
};
class GridViewRow
{
public:
	std::vector<CellValue> Cells = std::vector<CellValue>();
	CellValue& operator[](int idx);
};
class GridView : public Control
{
public:
	UIClass Type();
	CursorKind QueryCursor(int xof, int yof) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int xof, int yof) override;
	bool HandlesNavigationKey(WPARAM key) const override;
	bool IsAnimationRunning() override { return IsCaretBlinkAnimating(); }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override { return GetCaretBlinkInvalidRect(outRect); }
	GridView(int x = 0, int y = 0, int width = 120, int height = 20);
	~GridView() override;
	/** @brief 表头字体（为空则使用默认字体/继承字体）。 */
	class Font* HeadFont = NULL;
	bool InScroll = false;
	bool InHScroll = false;
	ScrollChangedEvent ScrollChanged;
	GridViewRow& operator[](int idx);
	float HeadHeight = 0.0f;
	float RowHeight = 0.0f;
	float Boder = 1.5f;
	D2D1_COLOR_F HeadBackColor = Colors::Snow3;
	D2D1_COLOR_F HeadForeColor = Colors::Black;
	/** @brief 像素级垂直滚动偏移（用于平滑滚动/位置滚动）。 */
	float ScrollYOffset = 0.0f;
	int ScrollRowPosition = 0;
	int SelectedColumnIndex = -1;
	int SelectedRowIndex = -1;
	/** @brief 是否按整行显示选中效果。 */
	bool FullRowSelect = true;
	int SortedColumnIndex = -1;
	bool SortAscending = true;
	int UnderMouseColumnIndex = -1;
	int UnderMouseRowIndex = -1;
	D2D1_COLOR_F ButtonBackColor = Colors::GhostWhite;
	D2D1_COLOR_F ButtonCheckedColor = Colors::White;
	// Button 列：独立的悬浮/按下效果（尽量模拟 WinForms Button）
	D2D1_COLOR_F ButtonHoverBackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F ButtonPressedBackColor = Colors::LightGray;
	D2D1_COLOR_F ButtonBorderDarkColor = Colors::DimGrey;
	D2D1_COLOR_F ButtonBorderLightColor = Colors::White;
	D2D1_COLOR_F SelectedItemBackColor = { 0.f , 0.f , 1.f , 0.5f };
	D2D1_COLOR_F SelectedItemForeColor = Colors::White;
	D2D1_COLOR_F UnderMouseItemBackColor = { 0.5961f , 0.9608f , 1.f , 0.5f };
	D2D1_COLOR_F UnderMouseItemForeColor = Colors::Black;
	D2D1_COLOR_F ScrollBackColor = Colors::LightGray;
	D2D1_COLOR_F ScrollForeColor = Colors::DimGrey;
	D2D1_COLOR_F EditBackColor = Colors::White;
	D2D1_COLOR_F EditForeColor = Colors::Black;
	D2D1_COLOR_F EditSelectedBackColor = { 0.f , 0.f , 1.f , 0.5f };
	D2D1_COLOR_F EditSelectedForeColor = Colors::White;
	float EditTextMargin = 3.0f;
	
	// 新行相关属性
	/** @brief 新增行区域：是否允许用户手动添加新行。 */
	bool AllowUserToAddRows = false;           // 是否允许用户手动添加新行
	D2D1_COLOR_F NewRowBackColor = { 0.95f, 0.95f, 0.95f, 1.0f };  // 新行背景色
	D2D1_COLOR_F NewRowForeColor = Colors::gray81;                     // 新行文字颜色
	D2D1_COLOR_F NewRowIndicatorColor = Colors::RoyalBlue;           // 新行指示符颜色
	OnGridViewCheckStateChangedEvent OnGridViewCheckStateChanged;
	OnGridViewButtonClickEvent OnGridViewButtonClick;
	OnGridViewComboBoxSelectionChangedEvent OnGridViewComboBoxSelectionChanged;
	OnGridViewUserAddingRowEvent OnUserAddingRow;
	OnGridViewUserAddedRowEvent OnUserAddedRow;
	SelectionChangedEvent SelectionChanged;
	float ScrollXOffset = 0.0f;
	GridViewRow& SelectedRow();
	std::wstring SelectedValue() const;
	/** @brief 行数。 */
	int RowCount() const;
	/** @brief 列数。 */
	int ColumnCount() const;
	const std::vector<GridViewColumn>& GetColumns() const;
	const std::vector<GridViewRow>& GetRows() const;
	GridViewColumn* GetColumn(int index);
	const GridViewColumn* GetColumn(int index) const;
	GridViewColumn& ColumnAt(int index);
	const GridViewColumn& ColumnAt(int index) const;
	GridViewRow* GetRow(int index);
	const GridViewRow* GetRow(int index) const;
	GridViewRow& RowAt(int index);
	const GridViewRow& RowAt(int index) const;
	/** @brief 添加列并返回新列索引。 */
	int AddColumn(const GridViewColumn& column);
	void ClearColumns();
	/** @brief 移除指定列。 */
	bool RemoveColumnAt(int index);
	/** @brief 添加行并返回新行索引。 */
	int AddRow(const GridViewRow& row = GridViewRow());
	void ClearRows();
	bool SwapRows(int first, int second);
	/** @brief 移除指定行。 */
	bool RemoveRowAt(int index);
	/** @brief 获取单元格，越界返回 nullptr。 */
	CellValue* GetCell(int col, int row);
	/** @brief 获取单元格，越界返回 nullptr。 */
	const CellValue* GetCell(int col, int row) const;
	/** @brief 设置单元格值，必要时扩展该行单元格数量。 */
	bool SetCellValue(int col, int row, const CellValue& value);
	/** @brief 程序化选择单元格。 */
	void SelectCell(int col, int row, bool fireEvent = true);
	/** @brief 程序化选择整行，活动列会尽量保留。 */
	void SelectRow(int row, bool fireEvent = true);
	/** @brief 清空列与行数据。 */
	void Clear();
	/** @brief 切换编辑光标/选择到指定单元格。 */
	void ChangeEditionSelected(int col, int row);
	/** @brief 调整行集合大小。 */
	void ReSizeRows(int count);
	void NormalizeRows();
	/** @brief 按指定列排序。 */
	void SortByColumn(int col, bool ascending = true);
	/** @brief 设置图片位图缓存上限，避免超大数据集滚动时无限增长。 */
	void SetImageCacheLimit(size_t limit);
	/** @brief 获取图片位图缓存上限。 */
	size_t GetImageCacheLimit() const;
private:
	std::vector<GridViewColumn> _columns;
	std::vector<GridViewRow> _rows;

	struct ImageCacheKey
	{
		const BitmapSource* Source = nullptr;
		ID2D1RenderTarget* Target = nullptr;

		bool operator==(const ImageCacheKey& other) const
		{
			return Source == other.Source && Target == other.Target;
		}
	};

	struct ImageCacheKeyHash
	{
		size_t operator()(const ImageCacheKey& key) const
		{
			auto source = reinterpret_cast<std::uintptr_t>(key.Source);
			auto target = reinterpret_cast<std::uintptr_t>(key.Target);
			return static_cast<size_t>(source ^ (target + 0x9e3779b97f4a7c15ull + (source << 6) + (source >> 2)));
		}
	};

	struct ImageCacheEntry
	{
		ImageCacheKey Key;
		Microsoft::WRL::ComPtr<ID2D1Bitmap> Bitmap;
	};
	using ImageCacheList = std::list<ImageCacheEntry>;
	ImageCacheList _imageCacheLru;
	std::unordered_map<ImageCacheKey, ImageCacheList::iterator, ImageCacheKeyHash> _imageCacheIndex;
	size_t _imageCacheLimit = 2048;

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
	ScrollLayout CalcScrollLayout();
	float GetTotalColumnsWidth();
	POINT GetGridViewUnderMouseItem(int x, int y, GridView* ct);
	int HitTestHeaderColumn(int x, int y);
	int HitTestHeaderDivider(int x, int y);
	D2D1_RECT_F GetGridViewScrollBlockRect(GridView* ct);
	int GetGridViewRenderRowCount(GridView* ct);
	void DrawScroll();
	void DrawHScroll(const ScrollLayout& l);
	void DrawCorner(const ScrollLayout& l);
	void SetScrollByPos(float yof);
	void SetHScrollByPos(float xof);
	void HandleDropFiles(WPARAM wParam);
	void HandleMouseWheel(WPARAM wParam, int xof, int yof);
	void HandleMouseMove(int xof, int yof);
	void HandleLeftButtonDown(int xof, int yof);
	void HandleLeftButtonUp(int xof, int yof);
	void HandleKeyDown(WPARAM wParam);
	void HandleKeyUp(WPARAM wParam);
	void HandleCharInput(WPARAM wParam);
	void HandleImeComposition(LPARAM lParam);
	void HandleCellClick(int col, int row);
	void ToggleCheckState(int col, int row);
	void StartEditingCell(int col, int row);
	void EnsureComboBoxCellDefaultSelection(int col, int row);
	void ToggleComboBoxEditor(int col, int row);
	void CloseComboBoxEditor();
	void CancelEditing(bool revert = true);
	void SaveCurrentEditingCell(bool commit = true);
	void AdjustScrollPosition();
	bool CanScrollDown();
	void UpdateUnderMouseIndices(int xof, int yof);

	bool _resizingColumn = false;
	int _resizeColumnIndex = -1;
	float _resizeStartX = 0.0f;
	float _resizeStartWidth = 0.0f;
	float _minColumnWidth = 24.0f;

	bool Editing = false;
	int EditingColumnIndex = -1;
	int EditingRowIndex = -1;
	std::wstring EditingText;
	std::wstring EditingOriginalText;
	int EditSelectionStart = 0;
	int EditSelectionEnd = 0;
	float EditOffsetX = 0.0f;
	struct EditHistoryState
	{
		std::wstring Text;
		int SelectionStart = 0;
		int SelectionEnd = 0;
	};
	std::vector<EditHistoryState> _editUndoStack;
	std::vector<EditHistoryState> _editRedoStack;
	size_t _editHistoryLimit = 128;

	class ComboBox* _cellComboBox = NULL;
	int _cellComboBoxColumnIndex = -1;
	int _cellComboBoxRowIndex = -1;

	bool _buttonMouseDown = false;
	int _buttonDownColumnIndex = -1;
	int _buttonDownRowIndex = -1;

	// 新行相关成员变量
	bool _isUnderNewRow = false;   // 鼠标是否在新行区域
	int _newRowAreaHitTest = -1;   // 新行区域的列索引 (-1表示不在新行区域)

	float GetRowHeightPx();
	float GetHeadHeightPx();
	bool TryGetCellRectLocal(int col, int row, D2D1_RECT_F& outRect);
	bool IsEditableTextCell(int col, int row);
	void EditInputText(const std::wstring& input);
	void EditInputBack();
	void EditInputDelete();
	void EditPushUndoState();
	void EditClearHistory();
	void EditRestoreState(const EditHistoryState& state);
	void EditSyncCellText();
	bool EditUndo();
	bool EditRedo();
	void EditUpdateScroll(float cellWidth);
	int EditHitTestTextPosition(float cellWidth, float cellHeight, float x, float y);
	bool BeginEditSelectionFromMouse(int xof, int yof);
	std::wstring EditGetSelectedString();
	void EditEnsureSelectionInRange();
	void EditSetImeCompositionWindow();

	// 新行相关方法
	bool IsNewRowArea(int x, int y);
	int HitTestNewRow(int x, int y, int& outColumnIndex);
	void AddNewRow();
	ID2D1Bitmap* GetImageBitmap(const std::shared_ptr<BitmapSource>& image, D2DGraphics* render);
	void ClearImageCache();
public:
	void Update() override;
	/** @brief 根据内容自动调整某列宽度。 */
	void AutoSizeColumn(int col);
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
