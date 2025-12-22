#pragma once
#include "Panel.h"
#include "Form.h"
#pragma comment(lib, "Imm32.lib")

UIClass Panel::Type() { return UIClass::UI_Panel; }

Panel::Panel() {}

Panel::Panel(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
}

Panel::~Panel()
{
	if (_layoutEngine)
	{
		delete _layoutEngine;
		_layoutEngine = nullptr;
	}
}

void Panel::SetLayoutEngine(class LayoutEngine* engine)
{
	if (_layoutEngine)
	{
		delete _layoutEngine;
	}
	_layoutEngine = engine;
	InvalidateLayout();
}

void Panel::InvalidateLayout()
{
	_needsLayout = true;
	if (_layoutEngine)
	{
		_layoutEngine->Invalidate();
	}
}

void Panel::PerformLayout()
{
	if (!_layoutEngine)
	{
		// 默认布局：支持 Anchor 和 Margin
		SIZE containerSize = this->Size;
		
		for (int i = 0; i < this->Children.Count; i++)
		{
			auto child = this->Children[i];
			if (!child || !child->Visible) continue;
			
			POINT loc = child->Location;
			SIZE size = child->Size;
			Thickness margin = child->Margin;
			uint8_t anchor = child->AnchorStyles;
			
			// 应用 Anchor
			if (anchor != AnchorStyles::None)
			{
				// 左右都锚定：宽度随容器变化
				if ((anchor & AnchorStyles::Left) && (anchor & AnchorStyles::Right))
				{
					size.cx = containerSize.cx - loc.x - (LONG)margin.Right;
				}
				// 只锚定右边：跟随右边缘
				else if (anchor & AnchorStyles::Right)
				{
					loc.x = containerSize.cx - size.cx - (LONG)margin.Right;
				}
				
				// 上下都锚定：高度随容器变化
				if ((anchor & AnchorStyles::Top) && (anchor & AnchorStyles::Bottom))
				{
					size.cy = containerSize.cy - loc.y - (LONG)margin.Bottom;
				}
				// 只锚定下边：跟随下边缘
				else if (anchor & AnchorStyles::Bottom)
				{
					loc.y = containerSize.cy - size.cy - (LONG)margin.Bottom;
				}
			}
			
			child->ApplyLayout(loc, size);
		}
	}
	else
	{
		// 使用布局引擎
		if (_needsLayout || _layoutEngine->NeedsLayout())
		{
			SIZE availableSize = this->Size;
			_layoutEngine->Measure(this, availableSize);
			
			D2D1_RECT_F finalRect = { 
				0, 0, 
				(float)availableSize.cx, 
				(float)availableSize.cy 
			};
			_layoutEngine->Arrange(this, finalRect);
		}
	}
	
	_needsLayout = false;
}

void Panel::Update()
{
	if (this->IsVisual == false) return;
	
	// 执行布局
	if (_needsLayout || (_layoutEngine && _layoutEngine->NeedsLayout()))
	{
		PerformLayout();
	}
	
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		for (int i = 0; i < this->Count; i++)
		{
			auto c = this->operator[](i);
			c->Update();
		}
		d2d->DrawRect(abslocation.x, abslocation.y, size.cx, size.cy, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(abslocation.x, abslocation.y, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	d2d->PopDrawRect();
}

bool Panel::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	for (int i = 0; i < this->Count; i++)
	{
		auto c = this->operator[](i);
		auto location = c->Location;
		auto size = c->ActualSize();
		if (
			xof >= location.x &&
			yof >= location.y &&
			xof <= (location.x + size.cx) &&
			yof <= (location.y + size.cy)
			)
		{
			c->ProcessMessage(message, wParam, lParam, xof - location.x, yof - location.y);
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
		for (int i = 0; i < uFileNum; i++)
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