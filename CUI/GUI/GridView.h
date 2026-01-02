#pragma once
#include "Control.h"
#include <functional>
#pragma comment(lib, "Imm32.lib")
typedef Event<void(class GridView*, int c, int r, bool v) > OnGridViewCheckStateChangedEvent;
typedef Event<void(class GridView*, int c, int r)> OnGridViewButtonClickEvent;
typedef Event<void(class GridView*, int c, int r, int selectedIndex, std::wstring selectedText)> OnGridViewComboBoxSelectionChangedEvent;
typedef Event<void(class GridView*, int newRowIndex)> OnGridViewUserAddedRowEvent;
enum class ColumnType
{
	Text,
	Image,
	Check,
	Button,
	ComboBox,
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
	List<std::wstring> ComboBoxItems = List<std::wstring>();
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
	std::wstring Text;
	ID2D1Bitmap* Image;
	__int64 Tag;
	CellValue();
	CellValue(std::wstring s);
	CellValue(wchar_t* s);
	CellValue(const wchar_t* s);
	CellValue(ID2D1Bitmap* img);
	CellValue(__int64 tag);
	CellValue(bool tag);
	CellValue(__int32 tag);
	CellValue(unsigned __int32 tag);
	CellValue(unsigned __int64 tag);
};
class GridViewRow
{
public:
	List<CellValue> Cells = List<CellValue>();
	CellValue& operator[](int idx);
};
class GridView : public Control
{
public:
	UIClass Type();
	CursorKind QueryCursor(int xof, int yof) override;
	GridView(int x = 0, int y = 0, int width = 120, int height = 20);
	~GridView() override;
	/** @brief 表头字体（为空则使用默认字体/继承字体）。 */
	class Font* HeadFont = NULL;
	bool InScroll = false;
	bool InHScroll = false;
	ScrollChangedEvent ScrollChanged;
	List<GridViewColumn> Columns = List<GridViewColumn>();
	List<GridViewRow> Rows = List<GridViewRow>();
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
	bool AllowUserToDeleteRows = true;         // 是否允许用户删除行
	D2D1_COLOR_F NewRowBackColor = { 0.95f, 0.95f, 0.95f, 1.0f };  // 新行背景色
	D2D1_COLOR_F NewRowForeColor = Colors::gray81;                     // 新行文字颜色
	D2D1_COLOR_F NewRowIndicatorColor = Colors::RoyalBlue;           // 新行指示符颜色
	OnGridViewCheckStateChangedEvent OnGridViewCheckStateChanged;
	OnGridViewButtonClickEvent OnGridViewButtonClick;
	OnGridViewComboBoxSelectionChangedEvent OnGridViewComboBoxSelectionChanged;
	OnGridViewUserAddedRowEvent OnUserAddedRow;
	SelectionChangedEvent SelectionChanged;
	float ScrollXOffset = 0.0f;
	GridViewRow& SelectedRow();
	std::wstring& SelectedValue();
	/** @brief 清空列与行数据。 */
	void Clear();
	/** @brief 切换编辑光标/选择到指定单元格。 */
	void ChangeEditionSelected(int col, int row);
	/** @brief 调整行集合大小。 */
	void ReSizeRows(int count);
	/** @brief 按指定列排序。 */
	void SortByColumn(int col, bool ascending = true);
private:
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
	void EditUpdateScroll(float cellWidth);
	int EditHitTestTextPosition(float cellWidth, float cellHeight, float x, float y);
	std::wstring EditGetSelectedString();
	void EditEnsureSelectionInRange();
	void EditSetImeCompositionWindow();

	// 新行相关方法
	bool IsNewRowArea(int x, int y);
	int HitTestNewRow(int x, int y, int& outColumnIndex);
	void DrawNewRowIndicator();
	void AddNewRow();
public:
	void Update() override;
	/** @brief 根据内容自动调整某列宽度。 */
	void AutoSizeColumn(int col);
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
