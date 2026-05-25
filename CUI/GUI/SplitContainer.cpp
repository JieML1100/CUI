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
	RECT splitterRect{ 0, 0, 0, 0 };
	auto containerSize = this->ActualSize();
	int distance = ClampSplitterDistance(this->SplitterDistance);
	int splitterWidth = (std::max)(1, this->SplitterWidth);
	if (SplitOrientation == Orientation::Horizontal)
	{
		splitterRect.left = distance;
		splitterRect.top = 0;
		splitterRect.right = (distance + splitterWidth) < containerSize.cx ? (distance + splitterWidth) : containerSize.cx;
		splitterRect.bottom = containerSize.cy;
	}
	else
	{
		splitterRect.left = 0;
		splitterRect.top = distance;
		splitterRect.right = containerSize.cx;
		splitterRect.bottom = (distance + splitterWidth) < containerSize.cy ? (distance + splitterWidth) : containerSize.cy;
	}
	return splitterRect;
}

int SplitContainer::ClampSplitterDistance(int value)
{
	auto containerSize = this->ActualSize();
	int splitterWidth = (std::max)(1, this->SplitterWidth);
	int availableLength = SplitOrientation == Orientation::Horizontal ? containerSize.cx : containerSize.cy;
	int maxDistance = availableLength - splitterWidth - Panel2MinSize;
	if (maxDistance < Panel1MinSize)
	{
		maxDistance = (std::max)(0, availableLength - splitterWidth);
	}
	int minDistance = (std::min)(Panel1MinSize, maxDistance);
	return (std::clamp)(value, minDistance, maxDistance);
}

void SplitContainer::SetSplitterDistanceInternal(int value)
{
	int clamped = ClampSplitterDistance(value);
	if (clamped == this->SplitterDistance) return;
	this->SplitterDistance = clamped;
	this->_needsLayout = true;
	this->InvalidateVisual();
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
	this->InvalidateVisual();
}

void SplitContainer::ArrangeSplitPanels()
{
	EnsureChildPanels();

	auto containerSize = this->ActualSize();
	int splitterWidth = (std::max)(1, this->SplitterWidth);
	int distance = ClampSplitterDistance(this->SplitterDistance);
	this->SplitterDistance = distance;

	if (SplitOrientation == Orientation::Horizontal)
	{
		_panel1->ApplyLayout(POINT{ 0, 0 }, SIZE{ distance, containerSize.cy });
		int panel2Width = containerSize.cx - distance - splitterWidth;
		if (panel2Width < 0) panel2Width = 0;
		_panel2->ApplyLayout(POINT{ distance + splitterWidth, 0 }, SIZE{ panel2Width, containerSize.cy });
	}
	else
	{
		_panel1->ApplyLayout(POINT{ 0, 0 }, SIZE{ containerSize.cx, distance });
		int panel2Height = containerSize.cy - distance - splitterWidth;
		if (panel2Height < 0) panel2Height = 0;
		_panel2->ApplyLayout(POINT{ 0, distance + splitterWidth }, SIZE{ containerSize.cx, panel2Height });
	}

	_needsLayout = false;
}

bool SplitContainer::HitSplitter(int localX, int localY)
{
	auto splitterRect = GetSplitterRect();
	return localX >= splitterRect.left && localX < splitterRect.right && localY >= splitterRect.top && localY < splitterRect.bottom;
}

bool SplitContainer::HitChildPanel(Panel* child, int localX, int localY, int& childX, int& childY)
{
	if (!child || !child->Visible || !child->Enable) return false;
	auto childLocation = child->ActualLocation;
	auto childSize = child->ActualSize();
	if (localX < childLocation.x || localY < childLocation.y || localX >= childLocation.x + childSize.cx || localY >= childLocation.y + childSize.cy)
		return false;
	childX = localX - childLocation.x;
	childY = localY - childLocation.y;
	return true;
}

CursorKind SplitContainer::QueryCursor(int localX, int localY)
{
	if (HitSplitter(localX, localY) && !IsSplitterFixed)
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
	auto containerSize = this->ActualSize();
	const float actualWidth = static_cast<float>(containerSize.cx);
	const float actualHeight = static_cast<float>(containerSize.cy);
	const float border = (std::max)(0.0f, this->BorderThickness);
	const float radius = (std::clamp)(this->CornerRadius, 0.0f, (std::min)(actualWidth, actualHeight) * 0.5f);
	auto splitterRect = GetSplitterRect();
	D2D1_COLOR_F splitterColor = _draggingSplitter ? SplitterPressedColor : (_isSplitterHovered ? SplitterHotColor : SplitterColor);

	this->BeginRender();
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, this->BackColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(radius);
		}
		if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
		{
			if (_panel1) _panel1->Update();
			if (_panel2) _panel2->Update();
		}
		{
			const float splitterX = static_cast<float>(splitterRect.left);
			const float splitterY = static_cast<float>(splitterRect.top);
			const float splitterW = static_cast<float>(splitterRect.right - splitterRect.left);
			const float splitterH = static_cast<float>(splitterRect.bottom - splitterRect.top);
			const float inset = (std::max)(0.0f, this->SplitterVisualInset);
			float visualX = splitterX;
			float visualY = splitterY;
			float visualW = splitterW;
			float visualH = splitterH;
			if (SplitOrientation == Orientation::Horizontal)
			{
				visualW = (std::max)(2.0f, splitterW - 2.0f);
				visualX = splitterX + (splitterW - visualW) * 0.5f;
				visualY = splitterY + (std::min)(inset, splitterH * 0.45f);
				visualH = (std::max)(0.0f, splitterH - (visualY - splitterY) * 2.0f);
			}
			else
			{
				visualH = (std::max)(2.0f, splitterH - 2.0f);
				visualY = splitterY + (splitterH - visualH) * 0.5f;
				visualX = splitterX + (std::min)(inset, splitterW * 0.45f);
				visualW = (std::max)(0.0f, splitterW - (visualX - splitterX) * 2.0f);
			}
			if (visualW > 0.0f && visualH > 0.0f)
			{
				const float splitterRadius = (std::clamp)(this->SplitterCornerRadius, 0.0f, (std::min)(visualW, visualH) * 0.5f);
				d2d->FillRoundRect(visualX, visualY, visualW, visualH, splitterColor, splitterRadius);
			}
		}
		if (border > 0.0f && this->BorderColor.a > 0.0f)
		{
			const float drawW = (std::max)(0.0f, actualWidth - border);
			const float drawH = (std::max)(0.0f, actualHeight - border);
			if (radius > 0.0f)
				d2d->DrawRoundRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border, radius);
			else
				d2d->DrawRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border);
		}
	}
	if (!this->Enable)
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor);
	}
	this->EndRender();
}

bool SplitContainer::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	if (_needsLayout)
	{
		ArrangeSplitPanels();
	}

	_isSplitterHovered = HitSplitter(localX, localY);

	if (_draggingSplitter && message == WM_MOUSEMOVE)
	{
		int pointerCoordinate = SplitOrientation == Orientation::Horizontal ? localX : localY;
		SetSplitterDistanceInternal(pointerCoordinate - _splitterDragOffset);
		return true;
	}
	if (_draggingSplitter && (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP))
	{
		_draggingSplitter = false;
		this->InvalidateVisual();
	}

	if (message == WM_LBUTTONDOWN && _isSplitterHovered && !IsSplitterFixed)
	{
		auto splitterRect = GetSplitterRect();
		_draggingSplitter = true;
		_splitterDragOffset = SplitOrientation == Orientation::Horizontal ? (localX - splitterRect.left) : (localY - splitterRect.top);
		this->InvalidateVisual();
		return true;
	}

	int childX = 0;
	int childY = 0;
	if (!_isSplitterHovered)
	{
		if (HitChildPanel(_panel2, localX, localY, childX, childY))
		{
			_panel2->ProcessMessage(message, wParam, lParam, childX, childY);
		}
		else if (HitChildPanel(_panel1, localX, localY, childX, childY))
		{
			_panel1->ProcessMessage(message, wParam, lParam, childX, childY);
		}
	}

	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xffffffff, nullptr, 0);
		TCHAR fileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (int fileIndex = 0; fileIndex < (int)fileCount; fileIndex++)
		{
			DragQueryFile(hDropInfo, fileIndex, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (files.size() > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDown(this, eventArgs);
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseUp(this, eventArgs);
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->OnMouseDoubleClick(this, eventArgs);
	}
	break;
	case WM_KEYDOWN:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, eventArgs);
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, eventArgs);
	}
	break;
	}

	return true;
}
