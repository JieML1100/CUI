#pragma once
#include "Control.h"
#include <functional>
#pragma comment(lib, "Imm32.lib")
typedef Event<void(class GridView*, int c, int r, bool v) > OnGridViewCheckStateChangedEvent;
enum class ColumnType
{
	Text,
	Image,
	Check,
};

class CellValue;
class GridViewColumn
{
public:
	std::wstring Name = L"";
	float Width = 120;
	ColumnType Type = ColumnType::Text;
	bool CanEdit = true;
	std::function<int(const CellValue& lhs, const CellValue& rhs)> SortFunc = nullptr;
	GridViewColumn(std::wstring name = L"", float width = 120.0F, ColumnType type = ColumnType::Text, bool canEdit = false);
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
	// Pixel-based vertical scroll offset for smooth/position scrolling.
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
	OnGridViewCheckStateChangedEvent OnGridViewCheckStateChanged;
	SelectionChangedEvent SelectionChanged;
	float ScrollXOffset = 0.0f;
	GridViewRow& SelectedRow();
	std::wstring& SelectedValue();
	void Clear();
	void ChangeEditionSelected(int col, int row);
	void ReSizeRows(int count);
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
public:
	void Update() override;
	void AutoSizeColumn(int col);
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;
};
