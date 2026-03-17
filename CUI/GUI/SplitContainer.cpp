#include "SplitContainer.h"
#include "Form.h"

#include <algorithm>

UIClass SplitContainer::Type() { return UIClass::UI_SplitContainer; }

SplitContainer::SplitContainer()
	: Panel()
{
	EnsureChildPanels();
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
}

SplitContainer::SplitContainer(int x, int y, int width, int height)
	: SplitContainer()
{
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
}

void SplitContainer::EnsureChildPanels()
{
	if (_panel1 && _panel2) return;
	_panel1 = Panel::AddControl(new Panel(0, 0, 100, 100));
	_panel2 = Panel::AddControl(new Panel(0, 0, 100, 100));
	_panel1->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	_panel2->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
}

RECT SplitContainer::GetSplitterRect()
{
	RECT rc{ 0, 0, 0, 0 };
	auto size = this->ActualSize();
	int distance = ClampSplitterDistance(this->SplitterDistance);
	int splitter = (std::max)(1, this->SplitterWidth);
	if (SplitOrientation == Orientation::Horizontal)
	{
		rc.left = distance;
		rc.top = 0;
		rc.right = (distance + splitter) < size.cx ? (distance + splitter) : size.cx;
		rc.bottom = size.cy;
	}
	else
	{
		rc.left = 0;
		rc.top = distance;
		rc.right = size.cx;
		rc.bottom = (distance + splitter) < size.cy ? (distance + splitter) : size.cy;
	}
	return rc;
}

int SplitContainer::ClampSplitterDistance(int value)
{
	auto size = this->ActualSize();
	int splitter = (std::max)(1, this->SplitterWidth);
	int total = SplitOrientation == Orientation::Horizontal ? size.cx : size.cy;
	int maxDistance = (std::max)(Panel1MinSize, total - splitter - Panel2MinSize);
	if (maxDistance < Panel1MinSize)
	{
		maxDistance = Panel1MinSize;
	}
	return (std::clamp)(value, Panel1MinSize, maxDistance);
}

void SplitContainer::SetSplitterDistanceInternal(int value)
{
	int clamped = ClampSplitterDistance(value);
	if (clamped == this->SplitterDistance) return;
	this->SplitterDistance = clamped;
	this->_needsLayout = true;
	this->PostRender();
}

void SplitContainer::SetSplitterDistance(int value)
{
	int clamped = ClampSplitterDistance(value);
	if (clamped != this->SplitterDistance)
	{
		this->SplitterDistance = clamped;
	}
	RefreshSplitterLayout();
}

void SplitContainer::RefreshSplitterLayout()
{
	this->_needsLayout = true;
	ArrangeSplitPanels();
	this->PostRender();
}

void SplitContainer::ArrangeSplitPanels()
{
	EnsureChildPanels();

	auto size = this->ActualSize();
	int splitter = (std::max)(1, this->SplitterWidth);
	int distance = ClampSplitterDistance(this->SplitterDistance);
	this->SplitterDistance = distance;

	if (SplitOrientation == Orientation::Horizontal)
	{
		_panel1->ApplyLayout(POINT{ 0, 0 }, SIZE{ distance, size.cy });
		int panel2Width = size.cx - distance - splitter;
		if (panel2Width < 0) panel2Width = 0;
		_panel2->ApplyLayout(POINT{ distance + splitter, 0 }, SIZE{ panel2Width, size.cy });
	}
	else
	{
		_panel1->ApplyLayout(POINT{ 0, 0 }, SIZE{ size.cx, distance });
		int panel2Height = size.cy - distance - splitter;
		if (panel2Height < 0) panel2Height = 0;
		_panel2->ApplyLayout(POINT{ 0, distance + splitter }, SIZE{ size.cx, panel2Height });
	}

	_needsLayout = false;
}

bool SplitContainer::HitSplitter(int xof, int yof)
{
	auto rc = GetSplitterRect();
	return xof >= rc.left && xof <= rc.right && yof >= rc.top && yof <= rc.bottom;
}

bool SplitContainer::HitChildPanel(Panel* child, int xof, int yof, int& childX, int& childY)
{
	if (!child || !child->Visible || !child->Enable) return false;
	auto loc = child->ActualLocation;
	auto sz = child->ActualSize();
	if (xof < loc.x || yof < loc.y || xof > loc.x + sz.cx || yof > loc.y + sz.cy)
		return false;
	childX = xof - loc.x;
	childY = yof - loc.y;
	return true;
}

CursorKind SplitContainer::QueryCursor(int xof, int yof)
{
	if (HitSplitter(xof, yof) && !IsSplitterFixed)
	{
		return SplitOrientation == Orientation::Horizontal ? CursorKind::SizeWE : CursorKind::SizeNS;
	}
	return this->Cursor;
}

void SplitContainer::Update()
{
	if (this->IsVisual == false) return;
	if (_needsLayout)
	{
		ArrangeSplitPanels();
	}

	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	auto splitterRect = GetSplitterRect();
	D2D1_COLOR_F splitterColor = _draggingSplitter ? SplitterPressedColor : (_hoverSplitter ? SplitterHotColor : SplitterColor);

	this->BeginRender();
	{
		d2d->FillRect(0, 0, size.cx, size.cy, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		if (_panel1) _panel1->Update();
		if (_panel2) _panel2->Update();
		d2d->FillRect((float)splitterRect.left, (float)splitterRect.top,
			(float)(splitterRect.right - splitterRect.left), (float)(splitterRect.bottom - splitterRect.top),
			splitterColor);
		d2d->DrawRect(0, 0, size.cx, size.cy, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}

bool SplitContainer::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	if (_needsLayout)
	{
		ArrangeSplitPanels();
	}

	_hoverSplitter = HitSplitter(xof, yof);

	if (_draggingSplitter && message == WM_MOUSEMOVE)
	{
		int coord = SplitOrientation == Orientation::Horizontal ? xof : yof;
		SetSplitterDistanceInternal(coord - _dragOffset);
		return true;
	}
	if (_draggingSplitter && (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP))
	{
		_draggingSplitter = false;
		this->PostRender();
	}

	if (message == WM_LBUTTONDOWN && _hoverSplitter && !IsSplitterFixed)
	{
		auto splitterRect = GetSplitterRect();
		_draggingSplitter = true;
		_dragOffset = SplitOrientation == Orientation::Horizontal ? (xof - splitterRect.left) : (yof - splitterRect.top);
		this->PostRender();
		return true;
	}

	int childX = 0;
	int childY = 0;
	if (!_hoverSplitter)
	{
		if (HitChildPanel(_panel2, xof, yof, childX, childY))
		{
			_panel2->ProcessMessage(message, wParam, lParam, childX, childY);
		}
		else if (HitChildPanel(_panel1, xof, yof, childX, childY))
		{
			_panel1->ProcessMessage(message, wParam, lParam, childX, childY);
		}
	}

	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xffffffff, NULL, 0);
		TCHAR strFileName[MAX_PATH];
		List<std::wstring> files;
		for (int i = 0; i < (int)uFileNum; i++)
		{
			DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
			files.Add(strFileName);
		}
		DragFinish(hDropInfo);
		if (files.Count > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, event_obj);
	}
	break;
	case WM_MOUSEMOVE:
	{
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, event_obj);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, event_obj);
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDoubleClick(this, event_obj);
	}
	break;
	case WM_KEYDOWN:
	{
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, event_obj);
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, event_obj);
	}
	break;
	}

	return true;
}
