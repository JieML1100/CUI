#pragma once
#include "Control.h"
#include <utility>
#include <vector>

enum class ReportCellAlign
{
	Left,
	Center,
	Right
};

enum class ReportRowKind
{
	Data,
	Group,
	Summary
};

struct ReportColumn
{
	std::wstring Header;
	float Width = 120.0f;
	ReportCellAlign Align = ReportCellAlign::Left;
	bool Sortable = true;

	ReportColumn() = default;
	ReportColumn(std::wstring header, float width = 120.0f, ReportCellAlign align = ReportCellAlign::Left, bool sortable = true);
};

struct ReportRow
{
	ReportRowKind Kind = ReportRowKind::Data;
	std::wstring Caption;
	std::vector<std::wstring> Cells;
	bool Expanded = true;
	float ExpandProgress = 1.0f;
	float AnimStartProgress = 1.0f;
	float AnimTargetProgress = 1.0f;
	ULONGLONG AnimStartTick = 0;
	UINT AnimDurationMs = 160;
	bool Animating = false;
	UINT64 Tag = 0;

	ReportRow() = default;
	explicit ReportRow(std::vector<std::wstring> cells);
	static ReportRow Group(std::wstring caption, bool expanded = true);
	static ReportRow Summary(std::wstring caption, std::vector<std::wstring> cells);
	float CurrentExpandProgress();
	void SetExpanded(bool expanded, bool animate = true);
	bool IsAnimationRunning();
};

typedef Event<void(class ReportView*, int rowIndex)> ReportRowEvent;
typedef Event<void(class ReportView*, int groupRowIndex, bool expanded)> ReportGroupToggledEvent;

class ReportView : public Control
{
public:
	UIClass Type() override;
	ReportView(int x = 0, int y = 0, int width = 420, int height = 260);

	std::wstring Title = L"Report";
	std::wstring Subtitle = L"";
	std::wstring FooterText = L"";

	std::vector<ReportColumn> Columns;
	std::vector<ReportRow> Rows;

	float HeaderHeight = 32.0f;
	float RowHeight = 30.0f;
	float GroupHeight = 30.0f;
	float SummaryHeight = 30.0f;
	float Border = 1.0f;
	float CornerRadius = 8.0f;
	float ScrollBarSize = 8.0f;

	bool ShowTitle = true;
	bool ShowFooter = true;
	bool AllowSorting = true;
	bool AlternatingRows = true;

	int SelectedRowIndex = -1;
	int UnderMouseRowIndex = -1;
	int SortedColumnIndex = -1;
	bool SortAscending = true;

	D2D1_COLOR_F HeaderBackColor = D2D1_COLOR_F{ 0.18f, 0.22f, 0.28f, 0.95f };
	D2D1_COLOR_F HeaderForeColor = D2D1_COLOR_F{ 0.90f, 0.93f, 0.98f, 1.0f };
	D2D1_COLOR_F RowBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.035f };
	D2D1_COLOR_F AlternateRowBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.065f };
	D2D1_COLOR_F GroupBackColor = D2D1_COLOR_F{ 0.20f, 0.48f, 0.82f, 0.20f };
	D2D1_COLOR_F SummaryBackColor = D2D1_COLOR_F{ 0.90f, 0.68f, 0.22f, 0.18f };
	D2D1_COLOR_F SelectedRowBackColor = D2D1_COLOR_F{ 0.20f, 0.52f, 0.95f, 0.36f };
	D2D1_COLOR_F UnderMouseRowBackColor = D2D1_COLOR_F{ 1.0f, 1.0f, 1.0f, 0.14f };
	D2D1_COLOR_F GridLineColor = D2D1_COLOR_F{ 0.55f, 0.60f, 0.68f, 0.24f };
	D2D1_COLOR_F MutedTextColor = D2D1_COLOR_F{ 0.72f, 0.76f, 0.82f, 1.0f };
	D2D1_COLOR_F ScrollBackColor = D2D1_COLOR_F{ 0.45f, 0.49f, 0.56f, 0.28f };
	D2D1_COLOR_F ScrollForeColor = D2D1_COLOR_F{ 0.76f, 0.81f, 0.90f, 0.86f };

	ReportRowEvent OnRowClick;
	ReportGroupToggledEvent OnGroupToggled;
	SelectionChangedEvent SelectionChanged;
	ScrollChangedEvent ScrollChanged;

	void Clear();
	void AddColumn(const ReportColumn& column);
	int AddRow(const ReportRow& row);
	int AddGroup(const std::wstring& caption, bool expanded = true);
	int AddSummary(const std::wstring& caption, const std::vector<std::wstring>& cells);
	void SortByColumn(int column, bool ascending);
	void ResetScroll();
	bool SetGroupExpanded(int rowIndex, bool expanded, bool animate = true);

	CursorKind QueryCursor(int xof, int yof) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int xof, int yof) override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof) override;

private:
	float _scrollYOffset = 0.0f;
	bool _inScroll = false;
	float _scrollGrabOffsetY = 0.0f;
	std::vector<int> _visibleRows;

	D2D1_RECT_F GetContentRect(float width, float height) const;
	D2D1_RECT_F GetHeaderRect(float width, float height) const;
	D2D1_RECT_F GetRowsRect(float width, float height) const;
	D2D1_RECT_F GetScrollTrackRect(float width, float height) const;
	D2D1_RECT_F GetScrollThumbRect(float width, float height);
	float GetTotalColumnsWidth() const;
	float GetVisibleRowsHeight();
	float GetMaxScrollY(float width, float height);
	float GetRowHeight(const ReportRow& row) const;
	int FindGroupEnd(int groupRowIndex) const;
	float GetRowsHeight(int first, int last) const;
	float GetGroupProgress(int rowIndex);
	void RebuildVisibleRows();
	void ClampScroll(float width, float height);
	void SetScrollYOffset(float value, float width, float height);
	int HitTestHeaderColumn(int xof, int yof);
	int HitTestVisibleRow(int xof, int yof);
	void DrawFrame(D2DGraphics* d2d, float width, float height);
	void DrawHeader(D2DGraphics* d2d, float width, float height);
	void DrawRows(D2DGraphics* d2d, float width, float height);
	void DrawSingleRow(D2DGraphics* d2d, int rowIndex, const D2D1_RECT_F& rowRect, int visualIndex);
	void DrawScrollBar(D2DGraphics* d2d, float width, float height);
	void DrawCellText(D2DGraphics* d2d, const std::wstring& text, const D2D1_RECT_F& rect, ReportCellAlign align, D2D1_COLOR_F color);
};
