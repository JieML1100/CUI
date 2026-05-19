#pragma once
#include "Panel.h"
#include "Form.h"
#include <algorithm>
#pragma comment(lib, "Imm32.lib")

UIClass Panel::Type() { return UIClass::UI_Panel; }

Panel::Panel()
{
	// Panel 作为容器：当自身尺寸变化时应重新布局子控件
	this->OnSizeChanged += [&](class Control* s) {
		(void)s;
		this->InvalidateLayout();
	};
}

Panel::Panel(int x, int y, int width, int height)
	: Panel()
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
		Thickness padding = this->Padding;
		float contentLeft = padding.Left;
		float contentTop = padding.Top;
		float contentWidth = (float)containerSize.cx - padding.Left - padding.Right;
		float contentHeight = (float)containerSize.cy - padding.Top - padding.Bottom;
		if (contentWidth < 0) contentWidth = 0;
		if (contentHeight < 0) contentHeight = 0;
		
		for (size_t i = 0; i < this->Children.size(); i++)
		{
			auto child = this->Children[i];
			if (!child || !child->Visible) continue;
			
			POINT location = child->Location;
			Thickness margin = child->Margin;
			uint8_t anchor = child->AnchorStyles;
			HorizontalAlignment hAlign = child->HAlign;
			VerticalAlignment vAlign = child->VAlign;
			SIZE size = child->MeasureCore({ (LONG)contentWidth, (LONG)contentHeight });

			float x = contentLeft + margin.Left;
			float y = contentTop + margin.Top;
			float w = (float)size.cx;
			float h = (float)size.cy;
			
			// 应用 Anchor
			if (anchor != AnchorStyles::None)
			{
				// 左右都锚定：宽度随容器变化
				if ((anchor & AnchorStyles::Left) && (anchor & AnchorStyles::Right))
				{
					x = contentLeft + (float)location.x;
					w = contentWidth - (float)location.x - margin.Right;
					if (w < 0) w = 0;
				}
				// 只锚定右边：跟随右边缘
				else if (anchor & AnchorStyles::Right)
				{
					x = contentLeft + contentWidth - margin.Right - w;
				}
				else
				{
					x = contentLeft + (float)location.x;
				}
				
				// 上下都锚定：高度随容器变化
				if ((anchor & AnchorStyles::Top) && (anchor & AnchorStyles::Bottom))
				{
					y = contentTop + (float)location.y;
					h = contentHeight - (float)location.y - margin.Bottom;
					if (h < 0) h = 0;
				}
				// 只锚定下边：跟随下边缘
				else if (anchor & AnchorStyles::Bottom)
				{
					y = contentTop + contentHeight - margin.Bottom - h;
				}
				else
				{
					y = contentTop + (float)location.y;
				}
			}
			else
			{
				// 默认容器中，Location 负责绝对定位；Center/Right/Stretch 改由对齐+Margin 管理。
				if (hAlign == HorizontalAlignment::Stretch)
				{
					x = contentLeft + margin.Left;
					w = contentWidth - margin.Left - margin.Right;
				}
				else if (hAlign == HorizontalAlignment::Center)
				{
					float availableWidth = contentWidth - margin.Left - margin.Right;
					if (availableWidth < 0) availableWidth = 0;
					x = contentLeft + margin.Left + (availableWidth - w) / 2.0f;
				}
				else if (hAlign == HorizontalAlignment::Right)
				{
					x = contentLeft + contentWidth - margin.Right - w;
				}
				else
				{
					x = contentLeft + (float)location.x;
				}
				
				if (vAlign == VerticalAlignment::Stretch)
				{
					y = contentTop + margin.Top;
					h = contentHeight - margin.Top - margin.Bottom;
				}
				else if (vAlign == VerticalAlignment::Top)
				{
					y = contentTop + (float)location.y;
				}
				else if (vAlign == VerticalAlignment::Center)
				{
					float availableHeight = contentHeight - margin.Top - margin.Bottom;
					if (availableHeight < 0) availableHeight = 0;
					y = contentTop + margin.Top + (availableHeight - h) / 2.0f;
				}
				else if (vAlign == VerticalAlignment::Bottom)
				{
					y = contentTop + contentHeight - margin.Bottom - h;
				}
			}

			if (w < 0) w = 0;
			if (h < 0) h = 0;
			
			POINT finalLoc = { (LONG)x, (LONG)y };
			SIZE finalSize = { (LONG)w, (LONG)h };
			child->ApplyLayout(finalLoc, finalSize);
		}
	}
	else
	{
		// 使用布局引擎
		if (_needsLayout || _layoutEngine->NeedsLayout())
		{
			SIZE availableSize = this->Size;
			Thickness padding = this->Padding;
			availableSize.cx = (LONG)((std::max)(0.0f, (float)availableSize.cx - padding.Left - padding.Right));
			availableSize.cy = (LONG)((std::max)(0.0f, (float)availableSize.cy - padding.Top - padding.Bottom));
			_layoutEngine->Measure(this, availableSize);
			
			D2D1_RECT_F finalRect = { 
				padding.Left,
				padding.Top,
				padding.Left + (float)availableSize.cx,
				padding.Top + (float)availableSize.cy
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
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	const float border = (std::max)(0.0f, this->BorderThickness);
	const float radius = (std::clamp)(this->CornerRadius, 0.0f, (std::min)(actualWidth, actualHeight) * 0.5f);
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
			for (auto child : this->GetChildrenInZOrder())
			{
				if (!child || !child->Visible) continue;
				child->Update();
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

bool Panel::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible) return true;
	for (auto child : this->GetChildrenInReverseZOrder())
	{
		if (!child || !child->Visible || !child->Enable) continue;
		auto childLocation = child->ActualLocation;
		auto childSize = child->ActualSize();
		if (
			localX >= childLocation.x &&
			localY >= childLocation.y &&
			localX <= (childLocation.x + childSize.cx) &&
			localY <= (childLocation.y + childSize.cy)
			)
		{
			child->ProcessMessage(message, wParam, lParam, localX - childLocation.x, localY - childLocation.y);
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
		for (UINT fileIndex = 0; fileIndex < fileCount; fileIndex++)
		{
			DragQueryFile(hDropInfo, fileIndex, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (!files.empty())
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
