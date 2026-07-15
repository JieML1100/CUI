#include "ReportView.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#include <cwchar>

namespace
{
	bool PointInRect(float x, float y, const D2D1_RECT_F& rect)
	{
		return x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom;
	}

	float RectWidth(const D2D1_RECT_F& rect)
	{
		return (std::max)(0.0f, rect.right - rect.left);
	}

	float RectHeight(const D2D1_RECT_F& rect)
	{
		return (std::max)(0.0f, rect.bottom - rect.top);
	}

	D2D1_POINT_2F LerpPoint(const D2D1_POINT_2F& from, const D2D1_POINT_2F& to, float t)
	{
		return D2D1::Point2F(from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t);
	}

	D2D1_TRIANGLE BuildGroupTriangle(float cx, float cy, float progress)
	{
		D2D1_TRIANGLE collapsed{
			D2D1::Point2F(cx - 2.0f, cy - 5.0f),
			D2D1::Point2F(cx - 2.0f, cy + 5.0f),
			D2D1::Point2F(cx + 5.0f, cy)
		};
		D2D1_TRIANGLE expanded{
			D2D1::Point2F(cx - 4.0f, cy - 2.0f),
			D2D1::Point2F(cx + 4.0f, cy - 2.0f),
			D2D1::Point2F(cx, cy + 4.0f)
		};
		progress = std::clamp(progress, 0.0f, 1.0f);
		D2D1_TRIANGLE tri{};
		tri.point1 = LerpPoint(collapsed.point1, expanded.point1, progress);
		tri.point2 = LerpPoint(collapsed.point2, expanded.point2, progress);
		tri.point3 = LerpPoint(collapsed.point3, expanded.point3, progress);
		return tri;
	}

	bool TryParseDouble(const std::wstring& value, double& out)
	{
		if (value.empty()) return false;
		wchar_t* end = nullptr;
		out = wcstod(value.c_str(), &end);
		while (end && *end == L' ') ++end;
		return end && *end == L'\0';
	}

	int CompareCellText(const std::wstring& lhs, const std::wstring& rhs)
	{
		double ld = 0.0;
		double rd = 0.0;
		if (TryParseDouble(lhs, ld) && TryParseDouble(rhs, rd))
		{
			if (ld < rd) return -1;
			if (ld > rd) return 1;
			return 0;
		}
		return _wcsicmp(lhs.c_str(), rhs.c_str());
	}
}

ReportColumn::ReportColumn(std::wstring header, float width, ReportCellAlign align, bool sortable)
	: Header(std::move(header)), Width(width), Align(align), Sortable(sortable)
{
}

ReportRow::ReportRow(std::vector<std::wstring> cells)
	: Kind(ReportRowKind::Data), Cells(std::move(cells))
{
}

ReportRow ReportRow::Group(std::wstring caption, bool expanded)
{
	ReportRow row;
	row.Kind = ReportRowKind::Group;
	row.Caption = std::move(caption);
	row.Expanded = expanded;
	row.ExpandProgress = expanded ? 1.0f : 0.0f;
	row.AnimStartProgress = row.ExpandProgress;
	row.AnimTargetProgress = row.ExpandProgress;
	return row;
}

ReportRow ReportRow::Summary(std::wstring caption, std::vector<std::wstring> cells)
{
	ReportRow row;
	row.Kind = ReportRowKind::Summary;
	row.Caption = std::move(caption);
	row.Cells = std::move(cells);
	return row;
}

float ReportRow::CurrentExpandProgress()
{
	if (Kind != ReportRowKind::Group)
	{
		Animating = false;
		ExpandProgress = 1.0f;
		return ExpandProgress;
	}

	if (!Animating)
	{
		ExpandProgress = Expanded ? 1.0f : 0.0f;
		return ExpandProgress;
	}

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= AnimStartTick ? (now - AnimStartTick) : 0;
	float t = AnimDurationMs > 0 ? (float)elapsed / (float)AnimDurationMs : 1.0f;
	if (t >= 1.0f)
	{
		ExpandProgress = AnimTargetProgress;
		Animating = false;
		return ExpandProgress;
	}

	t = 1.0f - std::pow(1.0f - std::clamp(t, 0.0f, 1.0f), 3.0f);
	ExpandProgress = AnimStartProgress + (AnimTargetProgress - AnimStartProgress) * t;
	return ExpandProgress;
}

void ReportRow::SetExpanded(bool expanded, bool animate)
{
	if (Kind != ReportRowKind::Group)
	{
		Expanded = expanded;
		return;
	}

	const float current = CurrentExpandProgress();
	Expanded = expanded;
	AnimStartProgress = current;
	AnimTargetProgress = expanded ? 1.0f : 0.0f;
	if (!animate || std::fabs(AnimTargetProgress - AnimStartProgress) < 0.001f)
	{
		ExpandProgress = AnimTargetProgress;
		Animating = false;
		return;
	}

	AnimStartTick = ::GetTickCount64();
	Animating = true;
}

bool ReportRow::IsAnimationRunning()
{
	CurrentExpandProgress();
	return Animating;
}

UIClass ReportView::Type()
{
	return UIClass::UI_ReportView;
}

ReportView::ReportView(int x, int y, int width, int height)
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = D2D1_COLOR_F{ 0.60f, 0.66f, 0.76f, 0.52f };
	this->ForeColor = D2D1_COLOR_F{ 0.90f, 0.92f, 0.96f, 1.0f };
	this->Cursor = CursorKind::Arrow;
}

void ReportView::Clear()
{
	Columns.clear();
	Rows.clear();
	_visibleRows.clear();
	SelectedRowIndex = -1;
	UnderMouseRowIndex = -1;
	SortedColumnIndex = -1;
	ResetScroll();
}

void ReportView::AddColumn(const ReportColumn& column)
{
	Columns.push_back(column);
	InvalidateVisual();
}

int ReportView::AddRow(const ReportRow& row)
{
	Rows.push_back(row);
	InvalidateVisual();
	return (int)Rows.size() - 1;
}

int ReportView::AddGroup(const std::wstring& caption, bool expanded)
{
	return AddRow(ReportRow::Group(caption, expanded));
}

int ReportView::AddSummary(const std::wstring& caption, const std::vector<std::wstring>& cells)
{
	return AddRow(ReportRow::Summary(caption, cells));
}

void ReportView::SortByColumn(int column, bool ascending)
{
	if (column < 0 || column >= (int)Columns.size())
		return;
	if (!Columns[column].Sortable)
		return;

	auto sortSegment = [&](int first, int last)
	{
		if (last - first <= 1) return;
		std::stable_sort(Rows.begin() + first, Rows.begin() + last, [&](const ReportRow& lhs, const ReportRow& rhs)
		{
			std::wstring leftValue = column < (int)lhs.Cells.size() ? lhs.Cells[column] : L"";
			std::wstring rightValue = column < (int)rhs.Cells.size() ? rhs.Cells[column] : L"";
			int compareResult = CompareCellText(leftValue, rightValue);
			return ascending ? compareResult < 0 : compareResult > 0;
		});
	};

	int segmentStart = -1;
	for (int i = 0; i <= (int)Rows.size(); ++i)
	{
		bool isData = i < (int)Rows.size() && Rows[i].Kind == ReportRowKind::Data;
		if (isData && segmentStart < 0)
			segmentStart = i;
		if ((!isData || i == (int)Rows.size()) && segmentStart >= 0)
		{
			sortSegment(segmentStart, i);
			segmentStart = -1;
		}
	}

	SortedColumnIndex = column;
	SortAscending = ascending;
	InvalidateVisual();
}

void ReportView::ResetScroll()
{
	_scrollYOffset = 0.0f;
	ScrollChanged(this);
	InvalidateVisual();
}

bool ReportView::SetGroupExpanded(int rowIndex, bool expanded, bool animate)
{
	if (rowIndex < 0 || rowIndex >= (int)Rows.size())
		return false;
	if (Rows[rowIndex].Kind != ReportRowKind::Group)
		return false;

	Rows[rowIndex].SetExpanded(
		expanded, animate && AreSystemAnimationsEnabled());
	RebuildVisibleRows();
	const auto size = GetActualSizeDip();
	ClampScroll(size.width, size.height);
	InvalidateVisual();
	return true;
}

CursorKind ReportView::QueryCursor(int localX, int localY)
{
	if (!Enable) return CursorKind::Arrow;

	RebuildVisibleRows();
	const auto size = GetActualSizeDip();
	float width = size.width;
	float height = size.height;
	if (_inScroll)
		return CursorKind::SizeNS;
	if (GetMaxScrollY(width, height) > 0.0f && PointInRect((float)localX, (float)localY, GetScrollThumbRect(width, height)))
		return CursorKind::SizeNS;
	int col = HitTestHeaderColumn(localX, localY);
	if (col >= 0 && AllowSorting && Columns[col].Sortable)
		return CursorKind::Hand;
	int row = HitTestVisibleRow(localX, localY);
	if (row >= 0)
		return CursorKind::Hand;
	return Cursor;
}

bool ReportView::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)delta;
	const auto size = GetActualSizeDip();
	float width = size.width;
	float height = size.height;
	return PointInRect((float)localX, (float)localY, GetRowsRect(width, height)) && GetMaxScrollY(width, height) > 0.0f;
}

bool ReportView::IsAnimationRunning()
{
	if (!AreSystemAnimationsEnabled())
	{
		for (auto& row : Rows)
			row.SetExpanded(row.Expanded, false);
		return false;
	}
	bool running = false;
	for (auto& row : Rows)
	{
		if (row.IsAnimationRunning())
			running = true;
	}
	return running;
}

bool ReportView::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = AbsRect;
	return true;
}

void ReportView::Update()
{
	if (!IsVisual) return;
	auto d2d = ParentForm->Render;
	const auto size = GetActualSizeDip();
	float width = size.width;
	float height = size.height;
	RebuildVisibleRows();
	ClampScroll(width, height);

	BeginRender();
	DrawFrame(d2d, width, height);
	DrawHeader(d2d, width, height);
	DrawRows(d2d, width, height);
	DrawScrollBar(d2d, width, height);
	if (!Enable)
		d2d->FillRoundRect(0, 0, width, height, D2D1_COLOR_F{ 1,1,1,0.45f }, CornerRadius);
	EndRender();
}

bool ReportView::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!Enable || !Visible) return true;
	(void)lParam;
	const auto size = GetActualSizeDip();
	float width = size.width;
	float height = size.height;
	RebuildVisibleRows();

	switch (message)
	{
	case WM_MOUSEWHEEL:
	{
		int delta = GET_WHEEL_DELTA_WPARAM(wParam);
		float steps = (float)delta / (float)WHEEL_DELTA;
		SetScrollYOffset(_scrollYOffset - steps * RowHeight * 3.0f, width, height);
		MouseEventArgs eventObj(MouseButtons::None, 0, localX, localY, delta);
		OnMouseWheel(this, eventObj);
		break;
	}
	case WM_MOUSEMOVE:
	{
		if (ParentForm) ParentForm->UnderMouse = this;
		if (_inScroll)
		{
			auto track = GetScrollTrackRect(width, height);
			auto thumb = GetScrollThumbRect(width, height);
			float maxScroll = GetMaxScrollY(width, height);
			float travel = (std::max)(1.0f, RectHeight(track) - RectHeight(thumb));
			float y = (float)localY - _scrollGrabOffsetY;
			float t = (y - track.top) / travel;
			SetScrollYOffset(maxScroll * std::clamp(t, 0.0f, 1.0f), width, height);
		}
		int old = UnderMouseRowIndex;
		UnderMouseRowIndex = HitTestVisibleRow(localX, localY);
		if (old != UnderMouseRowIndex) InvalidateVisual();
		MouseEventArgs eventObj(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		OnMouseMove(this, eventObj);
		break;
	}
	case WM_LBUTTONDOWN:
	{
		if (ParentForm) ParentForm->SetSelectedControl(this, false);
		auto thumb = GetScrollThumbRect(width, height);
		auto track = GetScrollTrackRect(width, height);
		if (PointInRect((float)localX, (float)localY, thumb))
		{
			_inScroll = true;
			_scrollGrabOffsetY = (float)localY - thumb.top;
		}
		else if (PointInRect((float)localX, (float)localY, track))
		{
			float maxScroll = GetMaxScrollY(width, height);
			float t = ((float)localY - track.top) / (std::max)(1.0f, RectHeight(track));
			SetScrollYOffset(maxScroll * std::clamp(t, 0.0f, 1.0f), width, height);
			_inScroll = true;
			_scrollGrabOffsetY = RectHeight(GetScrollThumbRect(width, height)) * 0.5f;
		}
		else
		{
			int col = HitTestHeaderColumn(localX, localY);
			if (col >= 0 && AllowSorting && Columns[col].Sortable)
			{
				bool ascending = SortedColumnIndex == col ? !SortAscending : true;
				SortByColumn(col, ascending);
			}
			else
			{
				int row = HitTestVisibleRow(localX, localY);
				if (row >= 0)
				{
					if (Rows[row].Kind == ReportRowKind::Group)
					{
						SetGroupExpanded(row, !Rows[row].Expanded, true);
						OnGroupToggled(this, row, Rows[row].Expanded);
					}
					else
					{
						SelectedRowIndex = row;
						SelectionChanged(this);
						OnRowClick(this, row);
					}
					InvalidateVisual();
				}
			}
		}
		MouseEventArgs eventObj(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		OnMouseDown(this, eventObj);
		break;
	}
	case WM_LBUTTONUP:
	{
		_inScroll = false;
		MouseEventArgs eventObj(MouseButtons::Left, 0, localX, localY, HIWORD(wParam));
		OnMouseUp(this, eventObj);
		if (ParentForm && ParentForm->Selected == this)
			ParentForm->SetSelectedControl(nullptr, false);
		InvalidateVisual();
		break;
	}
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs eventObj(MouseButtons::Left, 2, localX, localY, HIWORD(wParam));
		OnMouseDoubleClick(this, eventObj);
		break;
	}
	default:
		return Control::ProcessMessage(message, wParam, lParam, localX, localY);
	}
	return true;
}

D2D1_RECT_F ReportView::GetContentRect(float width, float height) const
{
	return D2D1::RectF(8.0f, 8.0f, (std::max)(8.0f, width - 8.0f), (std::max)(8.0f, height - 8.0f));
}

D2D1_RECT_F ReportView::GetHeaderRect(float width, float height) const
{
	auto content = GetContentRect(width, height);
	float top = content.top + (ShowTitle ? (Subtitle.empty() ? 42.0f : 58.0f) : 8.0f);
	return D2D1::RectF(content.left, top, content.right - ScrollBarSize - 4.0f, top + HeaderHeight);
}

D2D1_RECT_F ReportView::GetRowsRect(float width, float height) const
{
	auto header = GetHeaderRect(width, height);
	auto content = GetContentRect(width, height);
	float footer = ShowFooter ? 26.0f : 8.0f;
	return D2D1::RectF(header.left, header.bottom, header.right, (std::max)(header.bottom, content.bottom - footer));
}

D2D1_RECT_F ReportView::GetScrollTrackRect(float width, float height) const
{
	auto rows = GetRowsRect(width, height);
	return D2D1::RectF(rows.right + 4.0f, rows.top, rows.right + 4.0f + ScrollBarSize, rows.bottom);
}

D2D1_RECT_F ReportView::GetScrollThumbRect(float width, float height)
{
	auto track = GetScrollTrackRect(width, height);
	float total = GetVisibleRowsHeight();
	float viewport = RectHeight(GetRowsRect(width, height));
	if (total <= viewport || total <= 0.0f)
		return D2D1::RectF(track.left, track.top, track.right, track.top);
	float thumbHeight = (std::max)(24.0f, viewport / total * RectHeight(track));
	thumbHeight = (std::min)(RectHeight(track), thumbHeight);
	float maxScroll = GetMaxScrollY(width, height);
	float travel = (std::max)(1.0f, RectHeight(track) - thumbHeight);
	float top = track.top + (_scrollYOffset / (std::max)(1.0f, maxScroll)) * travel;
	return D2D1::RectF(track.left, top, track.right, top + thumbHeight);
}

float ReportView::GetTotalColumnsWidth() const
{
	float width = 0.0f;
	for (const auto& col : Columns)
		width += (std::max)(24.0f, col.Width);
	return width;
}

float ReportView::GetVisibleRowsHeight()
{
	float height = 0.0f;
	for (int i = 0; i < (int)Rows.size();)
	{
		if (Rows[i].Kind == ReportRowKind::Group)
		{
			height += GetRowHeight(Rows[i]);
			int end = FindGroupEnd(i);
			height += GetRowsHeight(i + 1, end) * GetGroupProgress(i);
			i = end;
			continue;
		}

		height += GetRowHeight(Rows[i]);
		++i;
	}
	return height;
}

float ReportView::GetMaxScrollY(float width, float height)
{
	float total = GetVisibleRowsHeight();
	float viewport = RectHeight(GetRowsRect(width, height));
	return (std::max)(0.0f, total - viewport);
}

float ReportView::GetRowHeight(const ReportRow& row) const
{
	switch (row.Kind)
	{
	case ReportRowKind::Group: return GroupHeight;
	case ReportRowKind::Summary: return SummaryHeight;
	case ReportRowKind::Data:
	default: return RowHeight;
	}
}

int ReportView::FindGroupEnd(int groupRowIndex) const
{
	int end = groupRowIndex + 1;
	while (end < (int)Rows.size() && Rows[end].Kind != ReportRowKind::Group)
		++end;
	return end;
}

float ReportView::GetRowsHeight(int first, int last) const
{
	float height = 0.0f;
	first = std::clamp(first, 0, (int)Rows.size());
	last = std::clamp(last, first, (int)Rows.size());
	for (int i = first; i < last; ++i)
		height += GetRowHeight(Rows[i]);
	return height;
}

float ReportView::GetGroupProgress(int rowIndex)
{
	if (rowIndex < 0 || rowIndex >= (int)Rows.size())
		return 1.0f;
	return Rows[rowIndex].CurrentExpandProgress();
}

void ReportView::RebuildVisibleRows()
{
	_visibleRows.clear();
	for (int i = 0; i < (int)Rows.size();)
	{
		if (Rows[i].Kind == ReportRowKind::Group)
		{
			_visibleRows.push_back(i);
			int end = FindGroupEnd(i);
			if (GetGroupProgress(i) > 0.001f)
			{
				for (int child = i + 1; child < end; ++child)
					_visibleRows.push_back(child);
			}
			i = end;
			continue;
		}

		_visibleRows.push_back(i);
		++i;
	}
}

void ReportView::ClampScroll(float width, float height)
{
	float maxScroll = GetMaxScrollY(width, height);
	_scrollYOffset = std::clamp(_scrollYOffset, 0.0f, maxScroll);
}

void ReportView::SetScrollYOffset(float value, float width, float height)
{
	float old = _scrollYOffset;
	float maxScroll = GetMaxScrollY(width, height);
	_scrollYOffset = std::clamp(value, 0.0f, maxScroll);
	if (std::fabs(old - _scrollYOffset) > 0.1f)
	{
		ScrollChanged(this);
		InvalidateVisual();
	}
}

int ReportView::HitTestHeaderColumn(int localX, int localY)
{
	const auto size = GetActualSizeDip();
	auto header = GetHeaderRect(size.width, size.height);
	if (!PointInRect((float)localX, (float)localY, header))
		return -1;
	float x = header.left;
	for (int i = 0; i < (int)Columns.size(); ++i)
	{
		float colWidth = (std::max)(24.0f, Columns[i].Width);
		if ((float)localX >= x && (float)localX <= x + colWidth)
			return i;
		x += colWidth;
	}
	return -1;
}

int ReportView::HitTestVisibleRow(int localX, int localY)
{
	const auto size = GetActualSizeDip();
	auto rows = GetRowsRect(size.width, size.height);
	if (!PointInRect((float)localX, (float)localY, rows))
		return -1;
	float y = rows.top - _scrollYOffset;
	for (int i = 0; i < (int)Rows.size();)
	{
		if (Rows[i].Kind == ReportRowKind::Group)
		{
			float rh = GetRowHeight(Rows[i]);
			if ((float)localY >= y && (float)localY <= y + rh)
				return i;
			y += rh;

			int end = FindGroupEnd(i);
			float childrenHeight = GetRowsHeight(i + 1, end);
			float visibleHeight = childrenHeight * GetGroupProgress(i);
			if (visibleHeight > 0.001f)
			{
				const float clipBottom = y + visibleHeight;
				if ((float)localY >= y && (float)localY <= clipBottom)
				{
					float childY = y;
					for (int child = i + 1; child < end; ++child)
					{
						float childH = GetRowHeight(Rows[child]);
						if ((float)localY >= childY && (float)localY <= (std::min)(childY + childH, clipBottom))
							return child;
						childY += childH;
					}
					return -1;
				}
				y += visibleHeight;
			}
			i = end;
			continue;
		}

		float rh = GetRowHeight(Rows[i]);
		if ((float)localY >= y && (float)localY <= y + rh)
			return i;
		y += rh;
		++i;
	}
	return -1;
}

void ReportView::DrawFrame(D2DGraphics* d2d, float width, float height)
{
	d2d->FillRoundRect(Border * 0.5f, Border * 0.5f, width - Border, height - Border, BackColor, CornerRadius);
	d2d->DrawRoundRect(Border * 0.5f, Border * 0.5f, width - Border, height - Border, BorderColor, Border, CornerRadius);

	if (ShowTitle)
	{
		d2d->DrawString(Title, 14.0f, 10.0f, ForeColor, Font);
		if (!Subtitle.empty())
			d2d->DrawString(Subtitle, 14.0f, 32.0f, MutedTextColor, Font);
	}

	if (ShowFooter)
	{
		auto content = GetContentRect(width, height);
		std::wstring footer = FooterText.empty() ? (std::to_wstring(_visibleRows.size()) + L" rows") : FooterText;
		d2d->DrawLine(content.left, content.bottom - 24.0f, content.right, content.bottom - 24.0f, GridLineColor, 1.0f);
		d2d->DrawString(footer, content.left + 6.0f, content.bottom - 21.0f, MutedTextColor, Font);
	}
}

void ReportView::DrawHeader(D2DGraphics* d2d, float width, float height)
{
	auto header = GetHeaderRect(width, height);
	d2d->FillRoundRect(header, HeaderBackColor, 5.0f);
	float x = header.left;
	for (int i = 0; i < (int)Columns.size(); ++i)
	{
		float colWidth = (std::max)(24.0f, Columns[i].Width);
		D2D1_RECT_F rect = D2D1::RectF(x, header.top, x + colWidth, header.bottom);
		if (i > 0)
			d2d->DrawLine(rect.left, rect.top + 5.0f, rect.left, rect.bottom - 5.0f, GridLineColor, 1.0f);
		DrawCellText(d2d, Columns[i].Header, D2D1::RectF(rect.left + 8.0f, rect.top, rect.right - 8.0f, rect.bottom), Columns[i].Align, HeaderForeColor);
		if (SortedColumnIndex == i)
		{
			float cx = rect.right - 13.0f;
			float cy = rect.top + RectHeight(rect) * 0.5f;
			if (SortAscending)
			{
				D2D1_TRIANGLE tri{
					D2D1::Point2F(cx, cy - 4.0f),
					D2D1::Point2F(cx - 4.0f, cy + 3.0f),
					D2D1::Point2F(cx + 4.0f, cy + 3.0f)
				};
				d2d->FillTriangle(tri, HeaderForeColor);
			}
			else
			{
				D2D1_TRIANGLE tri{
					D2D1::Point2F(cx, cy + 4.0f),
					D2D1::Point2F(cx - 4.0f, cy - 3.0f),
					D2D1::Point2F(cx + 4.0f, cy - 3.0f)
				};
				d2d->FillTriangle(tri, HeaderForeColor);
			}
		}
		x += colWidth;
		if (x > header.right) break;
	}
}

void ReportView::DrawRows(D2DGraphics* d2d, float width, float height)
{
	auto rowsRect = GetRowsRect(width, height);
	d2d->PushDrawRect(rowsRect.left, rowsRect.top, RectWidth(rowsRect), RectHeight(rowsRect));
	float y = rowsRect.top - _scrollYOffset;
	int visualIndex = 0;
	for (int i = 0; i < (int)Rows.size();)
	{
		if (Rows[i].Kind == ReportRowKind::Group)
		{
			float groupHeight = GetRowHeight(Rows[i]);
			D2D1_RECT_F groupRect = D2D1::RectF(rowsRect.left, y, rowsRect.right, y + groupHeight);
			if (groupRect.bottom >= rowsRect.top && groupRect.top <= rowsRect.bottom)
				DrawSingleRow(d2d, i, groupRect, visualIndex);
			y += groupHeight;
			++visualIndex;
			int end = FindGroupEnd(i);
			float childrenHeight = GetRowsHeight(i + 1, end);
			float visibleHeight = childrenHeight * GetGroupProgress(i);
			if (visibleHeight > 0.001f)
			{
				const float clipTop = y;
				const float clipBottom = y + visibleHeight;
				const float drawTop = (std::max)(rowsRect.top, clipTop);
				const float drawBottom = (std::min)(rowsRect.bottom, clipBottom);
				if (drawBottom > drawTop)
				{
					d2d->PushDrawRect(rowsRect.left, drawTop, RectWidth(rowsRect), drawBottom - drawTop);
					float childY = y;
					for (int child = i + 1; child < end; ++child)
					{
						float childHeight = GetRowHeight(Rows[child]);
						D2D1_RECT_F childRect = D2D1::RectF(rowsRect.left, childY, rowsRect.right, childY + childHeight);
						if (childRect.bottom >= drawTop && childRect.top <= drawBottom)
							DrawSingleRow(d2d, child, childRect, visualIndex);
						childY += childHeight;
						++visualIndex;
					}
					d2d->PopDrawRect();
				}
				else
				{
					visualIndex += end - i - 1;
				}
				y += visibleHeight;
			}
			i = end;
			continue;
		}

		float rowHeight = GetRowHeight(Rows[i]);
		D2D1_RECT_F rowRect = D2D1::RectF(rowsRect.left, y, rowsRect.right, y + rowHeight);
		if (rowRect.bottom >= rowsRect.top && rowRect.top <= rowsRect.bottom)
			DrawSingleRow(d2d, i, rowRect, visualIndex);
		if (rowRect.top > rowsRect.bottom)
			break;
		y += rowHeight;
		++visualIndex;
		++i;
	}
	d2d->PopDrawRect();
}

void ReportView::DrawSingleRow(D2DGraphics* d2d, int rowIndex, const D2D1_RECT_F& rowRect, int visualIndex)
{
	if (rowIndex < 0 || rowIndex >= (int)Rows.size())
		return;

	auto& row = Rows[rowIndex];
	D2D1_COLOR_F back = RowBackColor;
	if (row.Kind == ReportRowKind::Group) back = GroupBackColor;
	else if (row.Kind == ReportRowKind::Summary) back = SummaryBackColor;
	else if (AlternatingRows && (visualIndex % 2) == 1) back = AlternateRowBackColor;
	d2d->FillRect(rowRect, back);
	if (UnderMouseRowIndex == rowIndex)
		d2d->FillRect(rowRect, UnderMouseRowBackColor);
	if (SelectedRowIndex == rowIndex)
		d2d->FillRect(rowRect, SelectedRowBackColor);
	d2d->DrawLine(rowRect.left, rowRect.bottom, rowRect.right, rowRect.bottom, GridLineColor, 1.0f);

	if (row.Kind == ReportRowKind::Group)
	{
		float cx = rowRect.left + 14.0f;
		float cy = rowRect.top + RectHeight(rowRect) * 0.5f;
		d2d->FillTriangle(BuildGroupTriangle(cx, cy, GetGroupProgress(rowIndex)), ForeColor);
		d2d->DrawString(row.Caption, rowRect.left + 28.0f, rowRect.top + 6.0f, ForeColor, Font);
		return;
	}

	float x = rowRect.left;
	for (int c = 0; c < (int)Columns.size(); ++c)
	{
		float colWidth = (std::max)(24.0f, Columns[c].Width);
		D2D1_RECT_F cell = D2D1::RectF(x, rowRect.top, x + colWidth, rowRect.bottom);
		if (c > 0)
			d2d->DrawLine(cell.left, cell.top + 5.0f, cell.left, cell.bottom - 5.0f, GridLineColor, 1.0f);
		std::wstring text = c < (int)row.Cells.size() ? row.Cells[c] : L"";
		D2D1_COLOR_F color = row.Kind == ReportRowKind::Summary ? ForeColor : ForeColor;
		if (row.Kind == ReportRowKind::Summary && c == 0 && !row.Caption.empty())
			text = row.Caption;
		DrawCellText(d2d, text, D2D1::RectF(cell.left + 8.0f, cell.top, cell.right - 8.0f, cell.bottom), Columns[c].Align, color);
		x += colWidth;
		if (x > rowRect.right) break;
	}
}

void ReportView::DrawScrollBar(D2DGraphics* d2d, float width, float height)
{
	auto track = GetScrollTrackRect(width, height);
	float maxScroll = GetMaxScrollY(width, height);
	if (maxScroll <= 0.0f)
		return;
	auto thumb = GetScrollThumbRect(width, height);
	d2d->FillRoundRect(track, ScrollBackColor, ScrollBarSize * 0.5f);
	d2d->FillRoundRect(thumb, ScrollForeColor, ScrollBarSize * 0.5f);
}

void ReportView::DrawCellText(D2DGraphics* d2d, const std::wstring& text, const D2D1_RECT_F& rect, ReportCellAlign align, D2D1_COLOR_F color)
{
	auto textSize = Font->GetTextSize(text, RectWidth(rect), RectHeight(rect));
	float x = rect.left;
	if (align == ReportCellAlign::Center)
		x = rect.left + (RectWidth(rect) - textSize.width) * 0.5f;
	else if (align == ReportCellAlign::Right)
		x = rect.right - textSize.width;
	x = (std::max)(rect.left, x);
	float y = rect.top + (RectHeight(rect) - textSize.height) * 0.5f;
	d2d->DrawString(text, x, y, (std::max)(0.0f, rect.right - x), RectHeight(rect), color, Font);
}
