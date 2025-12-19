#pragma once
#include "Control.h"
#pragma comment(lib, "Imm32.lib")
typedef Event<void(class GridView*, int c, int r, bool v) > OnGridViewCheckStateChangedEvent;
enum class ColumnType
{
	Text,
	Image,
	Check,
};
class GridViewColumn
{
public:
	std::wstring Name = L"";
	float Width = 120;
	ColumnType Type = ColumnType::Text;
	bool CanEdit = true;
	GridViewColumn(std::wstring name = L"", float width = 120.0F, ColumnType type = ColumnType::Text, bool canEdit = false);
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
	GridView(int x = 0, int y = 0, int width = 120, int height = 20);
	// 编辑态光标闪烁需要周期刷新（媒体播放器等实时控件也可用同机制）
	int DesiredFrameIntervalMs() override
	{
		return (this->Editing && this->IsSelected()) ? 100 : 0;
	}
	class Font* HeadFont = NULL;
	bool InScroll = false;
	ScrollChangedEvent ScrollChanged;
	List<GridViewColumn> Columns = List<GridViewColumn>();
	List<GridViewRow> Rows = List<GridViewRow>();
	GridViewRow& operator[](int idx);
	float HeadHeight = 0.0f;
	float RowHeight = 0.0f;
	float Boder = 1.5f;
	D2D1_COLOR_F HeadBackColor = Colors::Snow3;
	D2D1_COLOR_F HeadForeColor = Colors::Black;
	int ScrollRowPosition = 0;
	int SelectedColumnIndex = -1;
	int SelectedRowIndex = -1;
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
	GridViewRow& SelectedRow();
	std::wstring& SelectedValue();
	void Clear();
	void ChangeEditionSelected(int col, int row);
	void ReSizeRows(int count);
private:
	POINT GetGridViewUnderMouseItem(int x, int y, GridView* ct);
	D2D1_RECT_F GetGridViewScrollBlockRect(GridView* ct);
	int GetGridViewRenderRowCount(GridView* ct);
	void DrawScroll();
	void SetScrollByPos(float yof);
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

	// 内建编辑器状态（替代嵌入 TextBox）
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
