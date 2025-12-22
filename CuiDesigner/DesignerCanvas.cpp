#include "DesignerCanvas.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Button.h"
#include "../CUI/GUI/TextBox.h"
#include "../CUI/GUI/CheckBox.h"
#include "../CUI/GUI/RadioBox.h"
#include "../CUI/GUI/ComboBox.h"
#include "../CUI/GUI/ProgressBar.h"
#include "../CUI/GUI/Slider.h"
#include "../CUI/GUI/PictureBox.h"
#include "../CUI/GUI/Switch.h"
#include "../CUI/GUI/RichTextBox.h"
#include "../CUI/GUI/PasswordBox.h"
#include "../CUI/GUI/RoundTextBox.h"
#include "../CUI/GUI/GridView.h"
#include "../CUI/GUI/TreeView.h"
#include "../CUI/GUI/TabControl.h"
#include "../CUI/GUI/ToolBar.h"
#include "../CUI/GUI/WebBrowser.h"
#include "../CUI/GUI/Layout/StackPanel.h"
#include "../CUI/GUI/Layout/GridPanel.h"
#include "../CUI/GUI/Layout/DockPanel.h"
#include "../CUI/GUI/Layout/WrapPanel.h"
#include "../CUI/GUI/Layout/RelativePanel.h"
#include "../CUI/GUI/Form.h"
#include <windowsx.h>
#include <algorithm>
#include <cmath>

DesignerCanvas::DesignerCanvas(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	this->BackColor = Colors::White;
	this->Boder = 2.0f;
}

DesignerCanvas::~DesignerCanvas()
{
}

void DesignerCanvas::Update()
{
	Panel::Update();
	
	// 绘制网格
	DrawGrid();
	
	// 绘制所有控件
	for (auto& dc : _designerControls)
	{
		if (dc->ControlInstance && dc->ControlInstance->Visible)
		{
			dc->ControlInstance->Update();
		}
	}
	
	// 绘制选中控件的调整手柄
	if (_selectedControl)
	{
		DrawSelectionHandles(_selectedControl);
	}
}

void DesignerCanvas::DrawGrid()
{
	if (!this->ParentForm) return;
	auto d2d = this->ParentForm->Render;
	int gridSize = 10;
	auto absloc = this->AbsLocation;
	
	// 绘制浅色网格
	D2D1_COLOR_F gridColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);
	
	for (int x = 0; x < this->Width; x += gridSize)
	{
		d2d->DrawLine(absloc.x + x, absloc.y, absloc.x + x, absloc.y + this->Height, gridColor, 0.5f);
	}
	
	for (int y = 0; y < this->Height; y += gridSize)
	{
		d2d->DrawLine(absloc.x, absloc.y + y, absloc.x + this->Width, absloc.y + y, gridColor, 0.5f);
	}
}

void DesignerCanvas::DrawSelectionHandles(std::shared_ptr<DesignerControl> dc)
{
	if (!dc || !dc->ControlInstance || !this->ParentForm) return;
	
	auto d2d = this->ParentForm->Render;
	auto absloc = this->AbsLocation;
	auto rect = GetControlRectInCanvas(dc->ControlInstance);
	int w = rect.right - rect.left;
	int h = rect.bottom - rect.top;
	
	// 绘制选中边框
	float x = (float)(absloc.x + rect.left);
	float y = (float)(absloc.y + rect.top);
	d2d->DrawRect(x, y, (float)w, (float)h, Colors::DodgerBlue, 2.0f);
	
	// 绘制8个调整手柄
	auto rects = GetHandleRectsFromRect(rect, 6);
	
	for (const auto& r : rects)
	{
		float hx = (float)(absloc.x + r.left);
		float hy = (float)(absloc.y + r.top);
		float hw = (float)(r.right - r.left);
		float hh = (float)(r.bottom - r.top);
		d2d->FillRect(hx, hy, hw, hh, Colors::White);
		d2d->DrawRect(hx, hy, hw, hh, Colors::DodgerBlue, 1.0f);
	}
}

std::shared_ptr<DesignerControl> DesignerCanvas::HitTestControl(POINT pt)
{
	auto pointInRect = [](POINT p, POINT loc, SIZE sz) -> bool {
		return p.x >= loc.x && p.y >= loc.y && p.x <= (loc.x + sz.cx) && p.y <= (loc.y + sz.cy);
	};

	// 在“控件树”中找最深层命中（而不是仅在 DesignerControl 列表里找矩形）。
	// 这样当控件已被放入容器时，点击会优先命中子控件。
	std::function<Control*(Control*, POINT)> hitDeepest = [&](Control* parent, POINT ptLocal) -> Control* {
		if (!parent) return nullptr;
		// 从后往前：后添加的绘制在上面
		for (int i = parent->Count - 1; i >= 0; i--)
		{
			auto* child = parent->operator[](i);
			if (!child) continue;
			if (!child->Visible) continue;

			auto loc = child->Location;
			auto sz = child->ActualSize();
			if (!pointInRect(ptLocal, loc, sz))
				continue;

			POINT childLocal{ ptLocal.x - loc.x, ptLocal.y - loc.y };
			if (child->HitTestChildren() && child->Count > 0)
			{
				auto* deeper = hitDeepest(child, childLocal);
				if (deeper) return deeper;
			}
			return child;
		}
		return nullptr;
	};

	Control* hit = hitDeepest(this, pt);
	if (!hit) return nullptr;

	// 将命中的 Control 映射到最近的 DesignerControl（有些内部控件如 TabPage/自动生成 Button
	// 可能没有对应的 DesignerControl 包装，此时向上回溯到最近的可设计控件）。
	auto findDesigner = [&](Control* c) -> std::shared_ptr<DesignerControl> {
		while (c && c != this)
		{
			for (auto it = _designerControls.rbegin(); it != _designerControls.rend(); ++it)
			{
				auto& dc = *it;
				if (dc && dc->ControlInstance == c)
					return dc;
			}
			c = c->Parent;
		}
		return nullptr;
	};

	return findDesigner(hit);
}

RECT DesignerCanvas::GetControlRectInCanvas(Control* c)
{
	RECT r{ 0,0,0,0 };
	if (!c) return r;
	auto abs = c->AbsLocation;
	auto canvasAbs = this->AbsLocation;
	auto size = c->ActualSize();
	int left = abs.x - canvasAbs.x;
	int top = abs.y - canvasAbs.y;
	r.left = left;
	r.top = top;
	r.right = left + size.cx;
	r.bottom = top + size.cy;
	return r;
}

std::vector<RECT> DesignerCanvas::GetHandleRectsFromRect(const RECT& r, int handleSize)
{
	std::vector<RECT> rects;
	int half = handleSize / 2;
	int cx = (r.left + r.right) / 2;
	int cy = (r.top + r.bottom) / 2;

	// TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left
	rects.push_back({ r.left - half, r.top - half, r.left + half, r.top + half });
	rects.push_back({ cx - half, r.top - half, cx + half, r.top + half });
	rects.push_back({ r.right - half, r.top - half, r.right + half, r.top + half });
	rects.push_back({ r.right - half, cy - half, r.right + half, cy + half });
	rects.push_back({ r.right - half, r.bottom - half, r.right + half, r.bottom + half });
	rects.push_back({ cx - half, r.bottom - half, cx + half, r.bottom + half });
	rects.push_back({ r.left - half, r.bottom - half, r.left + half, r.bottom + half });
	rects.push_back({ r.left - half, cy - half, r.left + half, cy + half });
	return rects;
}

DesignerControl::ResizeHandle DesignerCanvas::HitTestHandleFromRect(const RECT& r, POINT pt, int handleSize)
{
	auto rects = GetHandleRectsFromRect(r, handleSize);
	for (size_t i = 0; i < rects.size(); i++)
	{
		auto& hr = rects[i];
		if (pt.x >= hr.left && pt.x <= hr.right && pt.y >= hr.top && pt.y <= hr.bottom)
			return (DesignerControl::ResizeHandle)(i + 1);
	}
	return DesignerControl::ResizeHandle::None;
}

bool DesignerCanvas::IsDescendantOf(Control* ancestor, Control* node)
{
	if (!ancestor || !node) return false;
	auto* p = node->Parent;
	while (p)
	{
		if (p == ancestor) return true;
		p = p->Parent;
	}
	return false;
}

void DesignerCanvas::RemoveDesignerControlsInSubtree(Control* root)
{
	if (!root) return;

	auto isInSubtree = [this, root](Control* node) -> bool {
		if (!node) return false;
		if (node == root) return true;
		return IsDescendantOf(root, node);
	};

	bool selectionRemoved = false;
	if (_selectedControl && _selectedControl->ControlInstance)
	{
		selectionRemoved = isInSubtree(_selectedControl->ControlInstance);
	}

	_designerControls.erase(
		std::remove_if(_designerControls.begin(), _designerControls.end(),
			[&](const std::shared_ptr<DesignerControl>& dc) {
				return dc && dc->ControlInstance && isInSubtree(dc->ControlInstance);
			}),
		_designerControls.end());

	if (selectionRemoved)
	{
		_selectedControl = nullptr;
		OnControlSelected(nullptr);
	}
}

bool DesignerCanvas::IsContainerControl(Control* c)
{
	if (!c) return false;
	switch (c->Type())
	{
	case UIClass::UI_Panel:
	case UIClass::UI_StackPanel:
	case UIClass::UI_GridPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_TabControl:
	case UIClass::UI_ToolBar:
	case UIClass::UI_TabPage:
		return true;
	default:
		return false;
	}
}

Control* DesignerCanvas::NormalizeContainerForDrop(Control* container)
{
	if (!container) return nullptr;
	if (container->Type() == UIClass::UI_TabControl)
	{
		auto* tc = (TabControl*)container;
		if (tc->Count <= 0)
		{
			tc->AddPage(L"Page 1");
		}
		if (tc->Count <= 0) return tc;
		if (tc->SelectIndex < 0) tc->SelectIndex = 0;
		if (tc->SelectIndex >= tc->Count) tc->SelectIndex = tc->Count - 1;
		return tc->operator[](tc->SelectIndex);
	}
	return container;
}

POINT DesignerCanvas::CanvasToContainerPoint(POINT ptCanvas, Control* container)
{
	if (!container) return ptCanvas;
	auto canvasAbs = this->AbsLocation;
	auto abs = container->AbsLocation;
	POINT p{ ptCanvas.x - (abs.x - canvasAbs.x), ptCanvas.y - (abs.y - canvasAbs.y) };
	// TabPage content 的坐标已经是 page 本地坐标，不需要额外处理
	return p;
}

Control* DesignerCanvas::FindBestContainerAtPoint(POINT ptCanvas, Control* ignore)
{
	Control* best = nullptr;
	int bestArea = INT_MAX;

	for (auto& dc : _designerControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		auto* c = dc->ControlInstance;
		if (!c->Visible || !c->Enable) continue;
		if (!IsContainerControl(c)) continue;
		if (ignore && (c == ignore || IsDescendantOf(ignore, c))) continue;

		auto r = GetControlRectInCanvas(c);
		if (ptCanvas.x >= r.left && ptCanvas.x <= r.right && ptCanvas.y >= r.top && ptCanvas.y <= r.bottom)
		{
			int area = (r.right - r.left) * (r.bottom - r.top);
			if (area < bestArea)
			{
				best = c;
				bestArea = area;
			}
		}
	}

	return best;
}

void DesignerCanvas::DeleteControlRecursive(Control* c)
{
	if (!c) return;
	// 先删子控件
	while (c->Count > 0)
	{
		auto child = c->operator[](c->Count - 1);
		c->RemoveControl(child);
		DeleteControlRecursive(child);
	}
	delete c;
}

void DesignerCanvas::TryReparentSelectedAfterDrag()
{
	if (!_selectedControl || !_selectedControl->ControlInstance) return;
	auto* moving = _selectedControl->ControlInstance;

	// ToolBar 限制：只允许 Button
	auto movingType = moving->Type();

	auto r = GetControlRectInCanvas(moving);
	POINT center{ (r.left + r.right) / 2, (r.top + r.bottom) / 2 };

	Control* rawContainer = FindBestContainerAtPoint(center, moving);
	Control* container = NormalizeContainerForDrop(rawContainer);
	if (!container) {
		// 落在画布空白：归为根级
		if (_selectedControl->DesignerParent != nullptr)
		{
			// 计算当前在画布中的左上角
			POINT newCanvasPos{ r.left, r.top };
			// 从旧父移除
			if (moving->Parent) moving->Parent->RemoveControl(moving);
			// 加到画布
			this->AddControl(moving);
			moving->Location = newCanvasPos;
			_selectedControl->DesignerParent = nullptr;
			this->PostRender();
		}
		return;
	}

	// TabControl 的 content 已归一化为 TabPage；ToolBar 需要额外限制
	if (container->Type() == UIClass::UI_ToolBar && movingType != UIClass::UI_Button)
		return;

	bool containerChanged = (_selectedControl->DesignerParent != container);

	// 防止把自己塞进自己的子树
	if (container == moving || IsDescendantOf(moving, container))
		return;

	// 计算保持视觉不动的目标位置
	POINT canvasTopLeft{ r.left, r.top };
	POINT newLocal = CanvasToContainerPoint(canvasTopLeft, container);
	POINT dropLocalCenter = CanvasToContainerPoint(center, container);

	if (containerChanged)
	{
		// 从旧父移除
		if (moving->Parent)
			moving->Parent->RemoveControl(moving);

		// 加入新容器
		if (container->Type() == UIClass::UI_ToolBar)
		{
			auto* tb = (ToolBar*)container;
			tb->AddToolButton((Button*)moving);
		}
		else
		{
			container->AddControl(moving);
		}

		_selectedControl->DesignerParent = container;
	}

	// 布局容器：无论是否换容器，只要落点变化就要更新布局表达
	if (container->Type() == UIClass::UI_GridPanel)
	{
		auto* gp = (GridPanel*)container;
		int row = 0, col = 0;
		if (gp->TryGetCellAtPoint(dropLocalCenter, row, col))
		{
			moving->GridRow = row;
			moving->GridColumn = col;
		}
		// Grid 默认让子控件填充单元格
		moving->HAlign = HorizontalAlignment::Stretch;
		moving->VAlign = VerticalAlignment::Stretch;
		moving->Location = { 0,0 };
	}
	else if (container->Type() == UIClass::UI_StackPanel)
	{
		auto* sp = (StackPanel*)container;
		int insertIndex = sp->Count - 1;
		Orientation orient = sp->GetOrientation();
		for (int i = 0; i < sp->Count; i++)
		{
			auto* c = sp->operator[](i);
			if (!c || c == moving || !c->Visible) continue;
			auto loc = c->Location;
			auto sz = c->ActualSize();
			float mid = (orient == Orientation::Vertical)
				? (loc.y + sz.cy * 0.5f)
				: (loc.x + sz.cx * 0.5f);
			float dropAxis = (orient == Orientation::Vertical) ? (float)dropLocalCenter.y : (float)dropLocalCenter.x;
			if (dropAxis < mid)
			{
				insertIndex = i;
				break;
			}
		}
		int curIndex = sp->Children.IndexOf(moving);
		if (curIndex >= 0)
		{
			while (curIndex > insertIndex)
			{
				sp->Children.Swap(curIndex, curIndex - 1);
				curIndex--;
			}
			while (curIndex < insertIndex)
			{
				sp->Children.Swap(curIndex, curIndex + 1);
				curIndex++;
			}
		}
		moving->Location = { 0,0 };
	}
	else if (container->Type() == UIClass::UI_DockPanel)
	{
		auto cs = container->Size;
		float w = (float)cs.cx;
		float h = (float)cs.cy;
		float x = (float)dropLocalCenter.x;
		float y = (float)dropLocalCenter.y;
		float left = x;
		float right = w - x;
		float top = y;
		float bottom = h - y;

		float minDim = (w < h) ? w : h;
		float snap = (std::min)(40.0f, (std::max)(12.0f, minDim * 0.25f));
		Dock dock = Dock::Fill;
		float minDist = left;
		dock = Dock::Left;
		if (top < minDist) { minDist = top; dock = Dock::Top; }
		if (right < minDist) { minDist = right; dock = Dock::Right; }
		if (bottom < minDist) { minDist = bottom; dock = Dock::Bottom; }
		if (minDist > snap) dock = Dock::Fill;
		moving->DockPosition = dock;
		moving->Location = { 0,0 };
	}
	else if (container->Type() == UIClass::UI_WrapPanel)
	{
		auto* wp = (WrapPanel*)container;
		int insertIndex = wp->Count - 1;
		Orientation orient = wp->GetOrientation();
		const float lineTol = 10.0f;
		for (int i = 0; i < wp->Count; i++)
		{
			auto* c = wp->operator[](i);
			if (!c || c == moving || !c->Visible) continue;
			auto loc = c->Location;
			auto sz = c->ActualSize();
			float childPrimary = (orient == Orientation::Horizontal) ? (float)loc.y : (float)loc.x;
			float childSecondaryMid = (orient == Orientation::Horizontal)
				? (loc.x + sz.cx * 0.5f)
				: (loc.y + sz.cy * 0.5f);
			float dropPrimary = (orient == Orientation::Horizontal) ? (float)dropLocalCenter.y : (float)dropLocalCenter.x;
			float dropSecondary = (orient == Orientation::Horizontal) ? (float)dropLocalCenter.x : (float)dropLocalCenter.y;
			if (childPrimary > dropPrimary + lineTol || (std::fabs(childPrimary - dropPrimary) <= lineTol && dropSecondary < childSecondaryMid))
			{
				insertIndex = i;
				break;
			}
		}
		int curIndex = wp->Children.IndexOf(moving);
		if (curIndex >= 0)
		{
			while (curIndex > insertIndex)
			{
				wp->Children.Swap(curIndex, curIndex - 1);
				curIndex--;
			}
			while (curIndex < insertIndex)
			{
				wp->Children.Swap(curIndex, curIndex + 1);
				curIndex++;
			}
		}
		moving->Location = { 0,0 };
	}
	else if (container->Type() == UIClass::UI_RelativePanel)
	{
		auto m = moving->Margin;
		m.Left = (float)newLocal.x;
		m.Top = (float)newLocal.y;
		moving->Margin = m;
		moving->Location = { 0,0 };
	}
	else
	{
		if (containerChanged)
			moving->Location = newLocal;
	}

	if (auto* p = dynamic_cast<Panel*>(container))
	{
		p->InvalidateLayout();
		p->PerformLayout();
	}
	this->PostRender();
}

CursorKind DesignerCanvas::GetResizeCursor(DesignerControl::ResizeHandle handle)
{
	switch (handle)
	{
	case DesignerControl::ResizeHandle::TopLeft:
	case DesignerControl::ResizeHandle::BottomRight:
		return CursorKind::SizeNWSE;
	case DesignerControl::ResizeHandle::TopRight:
	case DesignerControl::ResizeHandle::BottomLeft:
		return CursorKind::SizeNESW;
	case DesignerControl::ResizeHandle::Top:
	case DesignerControl::ResizeHandle::Bottom:
		return CursorKind::SizeNS;
	case DesignerControl::ResizeHandle::Left:
	case DesignerControl::ResizeHandle::Right:
		return CursorKind::SizeWE;
	default:
		return CursorKind::Arrow;
	}
}

bool DesignerCanvas::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable) return false;
	
	// Note: xof/yof are already local coordinates relative to this canvas.
	POINT mousePos = { xof, yof };
	
	switch (message)
	{
	case WM_KEYDOWN:
	{
		// 设计器模式下，把键盘操作收敛到画布
		if (wParam == VK_ESCAPE)
		{
			// 取消“点击添加控件”模式
			_controlToAdd = UIClass::UI_Base;
			this->Cursor = CursorKind::Arrow;
			return true;
		}

		if (wParam == VK_DELETE || wParam == VK_BACK)
		{
			DeleteSelectedControl();
			this->PostRender();
			return true;
		}

		if (!_selectedControl || !_selectedControl->ControlInstance)
		{
			break;
		}

		int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 10 : 1;
		auto* moving = _selectedControl->ControlInstance;
		auto loc = moving->Location;
		bool useRelativeMargin = (moving->Parent && moving->Parent->Type() == UIClass::UI_RelativePanel);
		auto margin = moving->Margin;

		switch (wParam)
		{
		case VK_LEFT:
			if (useRelativeMargin) margin.Left -= step; else loc.x -= step;
			break;
		case VK_RIGHT:
			if (useRelativeMargin) margin.Left += step; else loc.x += step;
			break;
		case VK_UP:
			if (useRelativeMargin) margin.Top -= step; else loc.y -= step;
			break;
		case VK_DOWN:
			if (useRelativeMargin) margin.Top += step; else loc.y += step;
			break;
		default:
			break;
		}
		if (useRelativeMargin)
		{
			if (margin.Left != moving->Margin.Left || margin.Top != moving->Margin.Top)
			{
				moving->Margin = margin;
				if (auto* p = dynamic_cast<Panel*>(moving->Parent))
				{
					p->InvalidateLayout();
					p->PerformLayout();
				}
				this->PostRender();
				return true;
			}
		}
		else
		{
			if (loc.x != moving->Location.x || loc.y != moving->Location.y)
			{
				moving->Location = loc;
				this->PostRender();
				return true;
			}
		}
		break;
	}
	case WM_LBUTTONDOWN:
	{
		// 确保键盘消息会转发到画布（Form 优先发给 Selected）
		if (this->ParentForm)
		{
			this->ParentForm->Selected = this;
		}

		// 如果有待添加的控件，点击时添加
		if (_controlToAdd != UIClass::UI_Base)
		{
			AddControlToCanvas(_controlToAdd, mousePos);
			_controlToAdd = UIClass::UI_Base;
			this->Cursor = CursorKind::Arrow;
			return true;
		}
		
		// 检查是否点击选中手柄
		if (_selectedControl)
		{
			auto rect = GetControlRectInCanvas(_selectedControl->ControlInstance);
			auto handle = HitTestHandleFromRect(rect, mousePos, 6);
			if (handle != DesignerControl::ResizeHandle::None)
			{
				_isResizing = true;
				_resizeHandle = handle;
				auto r = GetControlRectInCanvas(_selectedControl->ControlInstance);
				_resizeStartRect = r;
				_dragStartPoint = mousePos;
				return true;
			}
		}
		
		// 选中控件
		auto hitControl = HitTestControl(mousePos);
		if (hitControl)
		{
			// 取消之前的选中
			if (_selectedControl)
			{
				_selectedControl->IsSelected = false;
			}
			
			_selectedControl = hitControl;
			_selectedControl->IsSelected = true;
			_isDragging = true;
			_dragStartPoint = mousePos;
			_controlStartLocation = hitControl->ControlInstance->Location;
			
			OnControlSelected(_selectedControl);
			return true;
		}
		else
		{
			// 取消选中
			if (_selectedControl)
			{
				_selectedControl->IsSelected = false;
				_selectedControl = nullptr;
				OnControlSelected(nullptr);
			}
		}
		break;
	}
	case WM_MOUSEMOVE:
	{
		// 拖拽控件
		if (_isDragging && _selectedControl && _selectedControl->ControlInstance)
		{
			int dx = mousePos.x - _dragStartPoint.x;
			int dy = mousePos.y - _dragStartPoint.y;
			
			_selectedControl->ControlInstance->Location = {
				_controlStartLocation.x + dx,
				_controlStartLocation.y + dy
			};
			this->Cursor = CursorKind::SizeAll;
			return true;
		}
		
		// 调整大小
		if (_isResizing && _selectedControl && _selectedControl->ControlInstance)
		{
			int dx = mousePos.x - _dragStartPoint.x;
			int dy = mousePos.y - _dragStartPoint.y;
			
			RECT newRect = _resizeStartRect;
			
			switch (_resizeHandle)
			{
			case DesignerControl::ResizeHandle::TopLeft:
				newRect.left += dx; newRect.top += dy; break;
			case DesignerControl::ResizeHandle::Top:
				newRect.top += dy; break;
			case DesignerControl::ResizeHandle::TopRight:
				newRect.right += dx; newRect.top += dy; break;
			case DesignerControl::ResizeHandle::Right:
				newRect.right += dx; break;
			case DesignerControl::ResizeHandle::BottomRight:
				newRect.right += dx; newRect.bottom += dy; break;
			case DesignerControl::ResizeHandle::Bottom:
				newRect.bottom += dy; break;
			case DesignerControl::ResizeHandle::BottomLeft:
				newRect.left += dx; newRect.bottom += dy; break;
			case DesignerControl::ResizeHandle::Left:
				newRect.left += dx; break;
			}
			
			// 最小尺寸限制
			int minSize = 20;
			if (newRect.right - newRect.left >= minSize && newRect.bottom - newRect.top >= minSize)
			{
				_selectedControl->ControlInstance->Location = {newRect.left, newRect.top};
				_selectedControl->ControlInstance->Size = {newRect.right - newRect.left, newRect.bottom - newRect.top};
			}
			
			this->Cursor = GetResizeCursor(_resizeHandle);
			return true;
		}
		
		// 更新鼠标样式
		if (_selectedControl)
		{
			auto rect = GetControlRectInCanvas(_selectedControl->ControlInstance);
			auto handle = HitTestHandleFromRect(rect, mousePos, 6);
			if (handle != DesignerControl::ResizeHandle::None)
			{
				this->Cursor = GetResizeCursor(handle);
				return true;
			}
		}
		
		// 如果是添加控件模式
		if (_controlToAdd != UIClass::UI_Base)
		{
			this->Cursor = CursorKind::Hand;
		}
		else
		{
			this->Cursor = CursorKind::Arrow;
		}
		break;
	}
	case WM_LBUTTONUP:
	{
		// 拖拽结束：尝试将控件放入容器
		if (_isDragging)
		{
			TryReparentSelectedAfterDrag();
		}
		_isDragging = false;
		_isResizing = false;
		_resizeHandle = DesignerControl::ResizeHandle::None;
		this->Cursor = CursorKind::Arrow;
		return true;
	}
	}
	
	return Panel::ProcessMessage(message, wParam, lParam, xof, yof);
}

void DesignerCanvas::AddControlToCanvas(UIClass type, POINT canvasPos)
{
	Control* newControl = nullptr;
	std::wstring typeName;
	
	// 在点击位置创建控件（左上角对齐，稍微偏移避免手感奇怪）
	int centerX = std::max(0L, canvasPos.x - 30);
	int centerY = std::max(0L, canvasPos.y - 12);
	
	switch (type)
	{
	case UIClass::UI_Label:
		newControl = new Label(L"标签", centerX, centerY);
		typeName = L"Label";
		break;
	case UIClass::UI_Button:
		newControl = new Button(L"按钮", centerX, centerY, 120, 30);
		typeName = L"Button";
		break;
	case UIClass::UI_TextBox:
		newControl = new TextBox(L"", centerX, centerY, 200, 25);
		typeName = L"TextBox";
		break;
	case UIClass::UI_RichTextBox:
		newControl = new RichTextBox(L"", centerX, centerY, 300, 160);
		typeName = L"RichTextBox";
		break;
	case UIClass::UI_PasswordBox:
		newControl = new PasswordBox(L"", centerX, centerY, 200, 25);
		typeName = L"PasswordBox";
		break;
	case UIClass::UI_Panel:
		newControl = new Panel(centerX, centerY, 200, 200);
		typeName = L"Panel";
		break;
	case UIClass::UI_StackPanel:
		newControl = new StackPanel(centerX, centerY, 200, 200);
		typeName = L"StackPanel";
		break;
	case UIClass::UI_GridPanel:
		newControl = new GridPanel(centerX, centerY, 200, 200);
		typeName = L"GridPanel";
		break;
	case UIClass::UI_DockPanel:
		newControl = new DockPanel(centerX, centerY, 200, 200);
		typeName = L"DockPanel";
		break;
	case UIClass::UI_WrapPanel:
		newControl = new WrapPanel(centerX, centerY, 200, 200);
		typeName = L"WrapPanel";
		break;
	case UIClass::UI_RelativePanel:
		newControl = new RelativePanel(centerX, centerY, 200, 200);
		typeName = L"RelativePanel";
		break;
	case UIClass::UI_CheckBox:
		newControl = new CheckBox(L"复选框", centerX, centerY);
		typeName = L"CheckBox";
		break;
	case UIClass::UI_RadioBox:
		newControl = new RadioBox(L"单选框", centerX, centerY);
		typeName = L"RadioBox";
		break;
	case UIClass::UI_ComboBox:
		newControl = new ComboBox(L"", centerX, centerY, 150, 25);
		typeName = L"ComboBox";
		break;
	case UIClass::UI_GridView:
		newControl = new GridView(centerX, centerY, 360, 200);
		typeName = L"GridView";
		break;
	case UIClass::UI_TreeView:
		newControl = new TreeView(centerX, centerY, 220, 220);
		typeName = L"TreeView";
		break;
	case UIClass::UI_ProgressBar:
		newControl = new ProgressBar(centerX, centerY, 200, 20);
		typeName = L"ProgressBar";
		break;
	case UIClass::UI_Slider:
		newControl = new Slider(centerX, centerY, 200, 30);
		typeName = L"Slider";
		break;
	case UIClass::UI_PictureBox:
		newControl = new PictureBox(centerX, centerY, 150, 150);
		typeName = L"PictureBox";
		break;
	case UIClass::UI_Switch:
		newControl = new Switch(centerX, centerY, 60, 30);
		typeName = L"Switch";
		break;
	case UIClass::UI_TabControl:
		newControl = new TabControl(centerX, centerY, 360, 240);
		typeName = L"TabControl";
		break;
	case UIClass::UI_ToolBar:
		newControl = new ToolBar(centerX, centerY, 360, 34);
		typeName = L"ToolBar";
		break;
	case UIClass::UI_WebBrowser:
		newControl = new WebBrowser(centerX, centerY, 500, 360);
		typeName = L"WebBrowser";
		break;
	default:
		return;
	}
	
	if (newControl)
	{
		// 确定父容器：鼠标点下命中的最内层容器（TabControl 会归一化到当前页）
		Control* rawContainer = FindBestContainerAtPoint(canvasPos, nullptr);
		Control* container = NormalizeContainerForDrop(rawContainer);
		Control* designerParent = nullptr;

		if (container)
		{
			// ToolBar 只接受 Button
			if (container->Type() == UIClass::UI_ToolBar && type != UIClass::UI_Button)
			{
				container = nullptr;
			}
		}

		if (container)
		{
			designerParent = container;
			POINT local = CanvasToContainerPoint({ centerX, centerY }, container);
			POINT dropLocal = CanvasToContainerPoint(canvasPos, container);
			if (container->Type() == UIClass::UI_ToolBar)
			{
				((ToolBar*)container)->AddToolButton((Button*)newControl);
			}
			else
			{
				container->AddControl(newControl);
				// 布局容器：按规则设置布局属性/顺序
				if (container->Type() == UIClass::UI_GridPanel)
				{
					auto* gp = (GridPanel*)container;
					int row = 0, col = 0;
					if (gp->TryGetCellAtPoint(dropLocal, row, col))
					{
						newControl->GridRow = row;
						newControl->GridColumn = col;
					}
					// Grid 默认让子控件填充单元格
					newControl->HAlign = HorizontalAlignment::Stretch;
					newControl->VAlign = VerticalAlignment::Stretch;
					newControl->Location = { 0,0 };
				}
				else if (container->Type() == UIClass::UI_StackPanel)
				{
					newControl->Location = { 0,0 };
				}
				else if (container->Type() == UIClass::UI_DockPanel)
				{
					auto cs = container->Size;
					float w = (float)cs.cx;
					float h = (float)cs.cy;
					float x = (float)dropLocal.x;
					float y = (float)dropLocal.y;
					float left = x;
					float right = w - x;
					float top = y;
					float bottom = h - y;

					float minDim = (w < h) ? w : h;
					float snap = (std::min)(40.0f, (std::max)(12.0f, minDim * 0.25f));
					Dock dock = Dock::Fill;
					float minDist = left;
					dock = Dock::Left;
					if (top < minDist) { minDist = top; dock = Dock::Top; }
					if (right < minDist) { minDist = right; dock = Dock::Right; }
					if (bottom < minDist) { minDist = bottom; dock = Dock::Bottom; }
					if (minDist > snap) dock = Dock::Fill;
					newControl->DockPosition = dock;
					newControl->Location = { 0,0 };
				}
				else if (container->Type() == UIClass::UI_WrapPanel)
				{
					auto* wp = (WrapPanel*)container;
					int insertIndex = wp->Count - 1;
					Orientation orient = wp->GetOrientation();
					const float lineTol = 10.0f;
					for (int i = 0; i < wp->Count; i++)
					{
						auto* c = wp->operator[](i);
						if (!c || c == newControl || !c->Visible) continue;
						auto locc = c->Location;
						auto sz = c->ActualSize();
						float childPrimary = (orient == Orientation::Horizontal) ? (float)locc.y : (float)locc.x;
						float childSecondaryMid = (orient == Orientation::Horizontal)
							? (locc.x + sz.cx * 0.5f)
							: (locc.y + sz.cy * 0.5f);
						float dropPrimary = (orient == Orientation::Horizontal) ? (float)dropLocal.y : (float)dropLocal.x;
						float dropSecondary = (orient == Orientation::Horizontal) ? (float)dropLocal.x : (float)dropLocal.y;
						if (childPrimary > dropPrimary + lineTol || (std::fabs(childPrimary - dropPrimary) <= lineTol && dropSecondary < childSecondaryMid))
						{
							insertIndex = i;
							break;
						}
					}
					int curIndex = wp->Children.IndexOf(newControl);
					if (curIndex >= 0)
					{
						while (curIndex > insertIndex)
						{
							wp->Children.Swap(curIndex, curIndex - 1);
							curIndex--;
						}
						while (curIndex < insertIndex)
						{
							wp->Children.Swap(curIndex, curIndex + 1);
							curIndex++;
						}
					}
					newControl->Location = { 0,0 };
				}
				else if (container->Type() == UIClass::UI_RelativePanel)
				{
					auto m = newControl->Margin;
					m.Left = (float)local.x;
					m.Top = (float)local.y;
					newControl->Margin = m;
					newControl->Location = { 0,0 };
				}
				else
				{
					newControl->Location = local;
				}

				if (auto* p = dynamic_cast<Panel*>(container))
				{
					p->InvalidateLayout();
					p->PerformLayout();
				}
			}
		}
		else
		{
			// 根级
			this->AddControl(newControl);
			newControl->Location = { centerX, centerY };
		}
		
		// 生成唯一名称
		_controlCounter++;
		std::wstring name = typeName + std::to_wstring(_controlCounter);
		
		// 创建设计器控件包装
		auto dc = std::make_shared<DesignerControl>(newControl, name, type, designerParent);
		_designerControls.push_back(dc);
		
		// 自动选中新添加的控件
		if (_selectedControl)
		{
			_selectedControl->IsSelected = false;
		}
		_selectedControl = dc;
		_selectedControl->IsSelected = true;
		
		OnControlSelected(_selectedControl);
	}
}

void DesignerCanvas::DeleteSelectedControl()
{
	if (!_selectedControl) return;
	
	// 从父容器移除并递归删除
	auto* inst = _selectedControl->ControlInstance;
	if (!inst)
	{
		_selectedControl = nullptr;
		OnControlSelected(nullptr);
		return;
	}

	// 删除控件前：先移除该子树下所有 DesignerControl，避免悬挂指针
	RemoveDesignerControlsInSubtree(inst);

	if (inst->Parent)
		inst->Parent->RemoveControl(inst);
	DeleteControlRecursive(inst);
}

void DesignerCanvas::ClearCanvas()
{
	// 清空所有控件（递归释放）
	while (this->Count > 0)
	{
		auto c = this->operator[](this->Count - 1);
		this->RemoveControl(c);
		DeleteControlRecursive(c);
	}
	_designerControls.clear();
	_selectedControl = nullptr;
	_controlCounter = 0;
	
	OnControlSelected(nullptr);
}
