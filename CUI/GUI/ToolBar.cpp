#include "ToolBar.h"
#include "Form.h"
#include <algorithm>

namespace
{
	void ApplyDefaultToolButtonStyle(Button* button)
	{
		if (!button) return;
		button->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
		button->BolderColor = D2D1_COLOR_F{ 0,0,0,0 };
		button->ForeColor = Colors::WhiteSmoke;
		button->UnderMouseColor = D2D1_COLOR_F{ 0.28f,0.58f,0.96f,0.26f };
		button->CheckedColor = D2D1_COLOR_F{ 0.28f,0.58f,0.96f,0.34f };
		button->Round = 0.28f;
		button->Boder = 0.0f;
	}
}

UIClass ToolBarSeparator::Type() { return UIClass::UI_CUSTOM; }

ToolBarSeparator::ToolBarSeparator(int width, int height)
{
	if (width < 1) width = 1;
	if (height < 1) height = 20;
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->BolderColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->LineColor = D2D1_COLOR_F{ 1,1,1,0.18f };
	this->Enable = false;
}

void ToolBarSeparator::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	if (actualWidth <= 0.0f || actualHeight <= 0.0f) return;
	const float lineWidth = (std::min)(1.5f, (std::max)(1.0f, actualWidth));
	const float x = (actualWidth - lineWidth) * 0.5f;
	const float inset = (std::min)(6.0f, actualHeight * 0.28f);
	const float lineHeight = (std::max)(1.0f, actualHeight - inset * 2.0f);
	this->BeginRender();
	{
		if (this->LineColor.a > 0.0f)
			d2d->FillRoundRect(x, inset, lineWidth, lineHeight, this->LineColor, lineWidth * 0.5f);
	}
	this->EndRender();
}

UIClass ToolBarSpacer::Type() { return UIClass::UI_CUSTOM; }

ToolBarSpacer::ToolBarSpacer(int width)
{
	if (width < 0) width = 0;
	this->Size = SIZE{ width,1 };
	this->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->BolderColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->Enable = false;
}

void ToolBarSpacer::Update()
{
}

UIClass ToolBar::Type() { return UIClass::UI_ToolBar; }

ToolBar::ToolBar(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 1,1,1,0.12f };
	this->BolderColor = D2D1_COLOR_F{ 1,1,1,0.12f };
	this->Boder = 1.0f;
	ItemHeight = static_cast<int>(height * 0.75f);
	if (ItemHeight < 1) ItemHeight = 1;
}

SIZE ToolBar::GetToolItemLayoutSize(Control* item)
{
	if (!item) return SIZE{ 0,0 };

	SIZE size = item->Size;
	switch (item->Type())
	{
	case UIClass::UI_Label:
	case UIClass::UI_LinkLabel:
	{
		SIZE desired = item->MeasureCore(SIZE{ this->Width, this->Height });
		if (desired.cx > 0) size.cx = desired.cx;
		if (desired.cy > 0) size.cy = desired.cy;
		break;
	}
	case UIClass::UI_CheckBox:
	case UIClass::UI_RadioBox:
	{
		SIZE actual = item->ActualSize();
		if (actual.cx > 0) size.cx = actual.cx;
		if (actual.cy > 0) size.cy = actual.cy;
		break;
	}
	default:
		if (size.cx <= 0 || size.cy <= 0)
		{
			SIZE desired = item->MeasureCore(SIZE{ this->Width, this->Height });
			if (size.cx <= 0 && desired.cx > 0) size.cx = desired.cx;
			if (size.cy <= 0 && desired.cy > 0) size.cy = desired.cy;
		}
		break;
	}

	auto overrideIt = _toolItemSizeOverrides.find(item);
	if (overrideIt != _toolItemSizeOverrides.end())
	{
		if (overrideIt->second.cx > 0) size.cx = overrideIt->second.cx;
		if (overrideIt->second.cy > 0) size.cy = overrideIt->second.cy;
	}

	if (size.cx < 0) size.cx = 0;
	if (size.cy <= 0) size.cy = ItemHeight;
	return size;
}

Control* ToolBar::AddToolItem(Control* item, int width, int height)
{
	if (!item) return NULL;
	Control* c = Panel::AddControl(item);
	_toolItemSizeOverrides[c] = SIZE{ width,height };
	if (width > 0) c->Width = width;
	if (height > 0) c->Height = height;
	LayoutItems();
	return c;
}

Button* ToolBar::AddTextButton(std::wstring text, int width)
{
	return AddToolButton(text, width);
}

Button* ToolBar::AddToolButton(std::wstring text, int width)
{
	Button* button = new Button(text, 0, 0, width, ItemHeight);
	ApplyDefaultToolButtonStyle(button);
	button->Round = this->ItemCornerRatio;
	return (Button*)AddToolItem(button, width, ItemHeight);
}

Button* ToolBar::AddToolButton(Button* button)
{
	if (!button) return NULL;
	if (button->Width <= 0) button->Width = ItemHeight;
	button->Height = ItemHeight;
	return (Button*)AddToolItem(button);
}

Button* ToolBar::AddIconButton(std::shared_ptr<BitmapSource> image, int width, std::wstring text)
{
	if (width <= 0) width = ItemHeight;
	Button* button = new Button(text, 0, 0, width, ItemHeight);
	button->Image = image;
	button->SizeMode = ImageSizeMode::CenterImage;
	ApplyDefaultToolButtonStyle(button);
	button->Round = this->ItemCornerRatio;
	return (Button*)AddToolItem(button, width, ItemHeight);
}

ComboBox* ToolBar::AddToolComboBox(std::wstring text, int width)
{
	ComboBox* combo = new ComboBox(text, 0, 0, width, ItemHeight);
	combo->BackColor = D2D1_COLOR_F{ 1,1,1,0.10f };
	combo->BolderColor = D2D1_COLOR_F{ 1,1,1,0.18f };
	combo->ForeColor = Colors::WhiteSmoke;
	combo->ButtonBackColor = D2D1_COLOR_F{ 1,1,1,0.12f };
	combo->HeaderHoverBackColor = D2D1_COLOR_F{ 1,1,1,0.10f };
	combo->AccentColor = D2D1_COLOR_F{ 0.28f,0.63f,0.98f,0.92f };
	combo->SelectedItemBackColor = D2D1_COLOR_F{ 0.28f,0.63f,0.98f,0.18f };
	combo->UnderMouseBackColor = D2D1_COLOR_F{ 1,1,1,0.10f };
	combo->UnderMouseForeColor = Colors::White;
	combo->ScrollBackColor = D2D1_COLOR_F{ 1,1,1,0.16f };
	combo->ScrollForeColor = D2D1_COLOR_F{ 1,1,1,0.45f };
	combo->CornerRadius = 6.0f;
	combo->DropCornerRadius = 8.0f;
	combo->DropGap = 5.0f;
	combo->Boder = 1.0f;
	return (ComboBox*)AddToolItem(combo, width, ItemHeight);
}

CheckBox* ToolBar::AddToolCheckBox(std::wstring text, int width)
{
	CheckBox* checkBox = new CheckBox(text, 0, 0);
	checkBox->ForeColor = Colors::WhiteSmoke;
	checkBox->UnderMouseColor = Colors::SkyBlue;
	if (width > 0)
	{
		checkBox->Width = width;
	}
	return (CheckBox*)AddToolItem(checkBox);
}

ToolBarSeparator* ToolBar::AddSeparator(int width)
{
	auto* separator = new ToolBarSeparator(width, ItemHeight);
	separator->LineColor = this->SeparatorColor;
	return (ToolBarSeparator*)AddToolItem(separator, width, ItemHeight);
}

ToolBarSpacer* ToolBar::AddSpacer(int width)
{
	auto* spacer = new ToolBarSpacer(width);
	return (ToolBarSpacer*)AddToolItem(spacer, width, 1);
}

void ToolBar::LayoutItems()
{
	int x = Padding;
	for (int i = 0; i < this->Count; i++)
	{
		auto c = this->operator[](i);
		if (!c || !c->Visible) continue;

		SIZE itemSize = GetToolItemLayoutSize(c);
		int y = (this->Height - itemSize.cy) / 2;
		if (y < 0) y = 0;
		c->SetRuntimeLocation(POINT{ x, y });
		x += itemSize.cx + Gap;
	}
}

void ToolBar::Update()
{
	if (this->ParentForm && this->Parent == NULL)
	{
		this->ParentForm->MainToolBar = this;
	}

	if (this->IsVisual == false) return;
	LayoutItems();

	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	this->BeginRender();
	{
		const float border = (std::max)(0.0f, this->Boder);
		const D2D1_RECT_F surface = D2D1::RectF(border * 0.5f, border * 0.5f,
			(std::max)(border * 0.5f, actualWidth - border * 0.5f),
			(std::max)(border * 0.5f, actualHeight - border * 0.5f));
		d2d->FillRoundRect(surface, this->BackColor, this->CornerRadius);
		if (this->Image)
		{
			this->RenderImage(this->CornerRadius);
		}
		if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
		{
			for (int i = 0; i < this->Count; i++)
			{
				auto c = this->operator[](i);
				if (!c || !c->Visible) continue;
				if (this->ParentForm && this->ParentForm->ForegroundControl == c) continue;
				if (auto* separator = dynamic_cast<ToolBarSeparator*>(c))
					separator->LineColor = this->SeparatorColor;
				c->Update();
			}
		}
		if (this->ShowBottomLine && this->BottomLineColor.a > 0.0f && actualHeight > 0.0f)
			d2d->DrawLine(0.0f, actualHeight - 0.5f, actualWidth, actualHeight - 0.5f, this->BottomLineColor, 1.0f);
		if (border > 0.0f && this->BolderColor.a > 0.0f)
		{
			d2d->DrawRoundRect(surface, this->BolderColor, border, this->CornerRadius);
		}
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}

bool ToolBar::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;

	LayoutItems();
	for (auto c : this->GetChildrenInReverseZOrder())
	{
		if (!c || !c->Visible || !c->Enable) continue;
		auto location = c->ActualLocation;
		auto size = GetToolItemLayoutSize(c);
		if (
			xof >= location.x &&
			yof >= location.y &&
			xof <= (location.x + size.cx) &&
			yof <= (location.y + size.cy)
			)
		{
			c->ProcessMessage(message, wParam, lParam, xof - location.x, yof - location.y);
			break;
		}
	}

	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
		TCHAR strFileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (UINT i = 0; i < uFileNum; i++)
		{
			DragQueryFile(hDropInfo, i, strFileName, MAX_PATH);
			files.push_back(strFileName);
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
