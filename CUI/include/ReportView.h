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

	D2D1_COLOR_F HeaderBackColor = cui::theme::palette::SurfaceMuted;
	D2D1_COLOR_F HeaderForeColor = cui::theme::palette::TextPrimary;
	D2D1_COLOR_F RowBackColor = cui::theme::palette::Surface;
	D2D1_COLOR_F AlternateRowBackColor = cui::theme::palette::SurfaceSubtle;
	D2D1_COLOR_F GroupBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F SummaryBackColor = D2D1_COLOR_F{ 0.820f, 0.541f, 0.071f, 0.14f };
	D2D1_COLOR_F SelectedRowBackColor = cui::theme::palette::AccentSelected;
	D2D1_COLOR_F UnderMouseRowBackColor = cui::theme::palette::AccentSoft;
	D2D1_COLOR_F GridLineColor = cui::theme::palette::Border;
	D2D1_COLOR_F MutedTextColor = cui::theme::palette::TextMuted;
	D2D1_COLOR_F ScrollBackColor = cui::theme::palette::ScrollTrack;
	D2D1_COLOR_F ScrollForeColor = cui::theme::palette::ScrollThumb;

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

	CursorKind QueryCursor(int localX, int localY) override;
	bool HandlesMouseWheel() const override { return true; }
	bool CanHandleMouseWheel(int delta, int localX, int localY) override;
	bool IsAnimationRunning() override;
	UINT GetAnimationIntervalMs() override { return 16; }
	bool GetAnimatedInvalidRect(D2D1_RECT_F& outRect) override;
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;

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
	int HitTestHeaderColumn(int localX, int localY);
	int HitTestVisibleRow(int localX, int localY);
	void DrawFrame(D2DGraphics* d2d, float width, float height);
	void DrawHeader(D2DGraphics* d2d, float width, float height);
	void DrawRows(D2DGraphics* d2d, float width, float height);
	void DrawSingleRow(D2DGraphics* d2d, int rowIndex, const D2D1_RECT_F& rowRect, int visualIndex);
	void DrawScrollBar(D2DGraphics* d2d, float width, float height);
	void DrawCellText(D2DGraphics* d2d, const std::wstring& text, const D2D1_RECT_F& rect, ReportCellAlign align, D2D1_COLOR_F color);
};
