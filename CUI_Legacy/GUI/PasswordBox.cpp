#pragma once
#define NOMINMAX
#include "PasswordBox.h"
#include "Form.h"
#pragma comment(lib, "Imm32.lib")
UIClass PasswordBox::Type() { return UIClass::UI_PasswordBox; }
bool PasswordBox::HandlesNavigationKey(WPARAM key) const
{
	switch (key)
	{
	case VK_HOME:
	case VK_END:
	case VK_PRIOR:
	case VK_NEXT:
		return true;
	default:
		return false;
	}
}
PasswordBox::PasswordBox(std::wstring text, int x, int y, int width, int height)
{
	this->Text = text;
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 0.75f , 0.75f , 0.75f , 0.75f };
}
void PasswordBox::InputText(std::wstring input)
{
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	if (sele >= this->Text.size() && sels >= this->Text.size())
	{
		this->Text += input;
		SelectionEnd = SelectionStart = this->Text.size();
	}
	else
	{
		std::vector<wchar_t> tmp = std::vector<wchar_t>();
		tmp.insert(tmp.end(), this->Text.begin(), this->Text.end());
		if (sele > sels)
		{
			int sublen = sele - sels;
			for (int i = 0; i < sublen; i++)
			{
				tmp.erase(tmp.begin() + sels);
			}
			for (int i = 0; i < input.size(); i++)
			{
				tmp.insert(tmp.begin() + sels + i, input[i]);
			}
			SelectionEnd = SelectionStart = sels + (input.size());
			tmp.push_back(L'\0');
			this->Text = std::wstring(tmp.data());
		}
		else if (sele == sels && sele >= 0)
		{
			for (int i = 0; i < input.size(); i++)
			{
				tmp.insert(tmp.begin() + sels + i, input[i]);
			}
			SelectionEnd += input.size();
			SelectionStart += input.size();
			tmp.push_back(L'\0');
			this->Text = std::wstring(tmp.data());
		}
		else
		{
			this->Text += input;
			SelectionEnd = SelectionStart = this->Text.size();
		}
	}
	std::vector<wchar_t> tmp = std::vector<wchar_t>();
	tmp.insert(tmp.end(), this->Text.begin(), this->Text.end());
	tmp.push_back(L'\0');
	for (int i = 0; i < tmp.size(); i++)
	{
		if (tmp[i] == L'\r' || tmp[i] == L'\n')
		{
			tmp[i] = L' ';
		}
	}
	this->Text = std::wstring(tmp.data());
}
void PasswordBox::InputBack()
{
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	int selLen = sele - sels;
	if (selLen > 0)
	{
		std::vector<wchar_t> tmp = std::vector<wchar_t>((wchar_t*)this->Text.c_str(), (wchar_t*)this->Text.c_str() + this->Text.size());
		for (int i = 0; i < selLen; i++)
		{
			tmp.erase(tmp.begin() + sels);
		}
		tmp.push_back(L'\0');
		this->SelectionStart = this->SelectionEnd = sels;
		this->Text = tmp.data();
	}
	else
	{
		if (sels > 0)
		{
			std::vector<wchar_t> tmp = std::vector<wchar_t>((wchar_t*)this->Text.c_str(), (wchar_t*)this->Text.c_str() + this->Text.size());
			tmp.erase(tmp.begin() + sels - 1);
			tmp.push_back(L'\0');
			this->SelectionStart = this->SelectionEnd = sels - 1;
			this->Text = tmp.data();
		}
	}
}
void PasswordBox::InputDelete()
{
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	int selLen = sele - sels;
	if (selLen > 0)
	{
		std::vector<wchar_t> tmp = std::vector<wchar_t>((wchar_t*)this->Text.c_str(), (wchar_t*)this->Text.c_str() + this->Text.size());
		for (int i = 0; i < selLen; i++)
		{
			tmp.erase(tmp.begin() + sels);
		}
		tmp.push_back(L'\0');
		this->SelectionStart = this->SelectionEnd = sels;
		this->Text = tmp.data();
	}
	else
	{
		if (sels < this->Text.size())
		{
			std::vector<wchar_t> tmp = std::vector<wchar_t>((wchar_t*)this->Text.c_str(), (wchar_t*)this->Text.c_str() + this->Text.size());
			tmp.erase(tmp.begin() + sels);
			tmp.push_back(L'\0');
			this->SelectionStart = this->SelectionEnd = sels;
			this->Text = tmp.data();
		}
	}
}
void PasswordBox::UpdateScroll(bool arrival)
{
	float render_width = this->Width - (TextMargin * 2.0f);
	auto font = this->Font;
	std::wstring MaskText(this->Text.size(), L'*');
	auto lastSelect = font->HitTestTextRange(MaskText, (UINT32)SelectionEnd, (UINT32)0)[0];
	if ((lastSelect.left + lastSelect.width) - OffsetX > render_width)
	{
		OffsetX = (lastSelect.left + lastSelect.width) - render_width;
	}
	if (lastSelect.left - OffsetX < 0.0f)
	{
		OffsetX = lastSelect.left;
	}
}
std::wstring PasswordBox::GetSelectedString()
{
	int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
	int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
	if (sele > sels)
	{
		std::wstring s = L"";
		for (int i = sels; i < sele; i++)
		{
			s += this->Text[i];
		}
		return s;
	}
	return L"";
}
void PasswordBox::Update()
{
	if (this->IsVisual == false)return;
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	float render_height = this->Height - (TextMargin * 2.0f);
	std::wstring MaskText(this->Text.size(), L'*');
	textSize = font->GetTextSize(MaskText, FLT_MAX, render_height);
	float OffsetY = (this->Height - textSize.height) * 0.5f;
	if (OffsetY < 0.0f)OffsetY = 0.0f;
	auto size = this->ActualSize();
	bool isSelected = this->ParentForm->Selected == this;
	this->_caretRectCacheValid = false;
	bool shouldDrawCaret = false;
	D2D1_POINT_2F caretStart{};
	D2D1_POINT_2F caretEnd{};
	this->BeginRender();
	{
		auto backColor = this->BackColor;
		if (isUnderMouse || isSelected)
		{
			backColor.r = std::min(1.0f, backColor.r * 1.2f);
			backColor.g = std::min(1.0f, backColor.g * 1.2f);
			backColor.b = std::min(1.0f, backColor.b * 1.2f);
		}
		d2d->FillRect(0, 0, size.cx, size.cy, backColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		if (this->Text.size() > 0)
		{
			auto font = this->Font;
			if (isSelected)
			{
				int sels = SelectionStart <= SelectionEnd ? SelectionStart : SelectionEnd;
				int sele = SelectionEnd >= SelectionStart ? SelectionEnd : SelectionStart;
				int selLen = sele - sels;
				auto selRange = font->HitTestTextRange(MaskText, (UINT32)sels, (UINT32)selLen);
				if (selLen != 0)
				{
					for (auto sr : selRange)
					{
						d2d->FillRect(sr.left + TextMargin - OffsetX, sr.top + OffsetY, sr.width, sr.height, this->SelectedBackColor);
					}
				}
				else
				{
					if (!selRange.empty())
					{
						const auto caret = selRange[0];
						const float cx = caret.left + TextMargin - OffsetX;
						const float cy = caret.top + OffsetY;
						const float ch = caret.height > 0 ? caret.height : font->FontHeight;
						auto abs = this->AbsLocation;
						this->_caretRectCache = { abs.x + cx - 2.0f, abs.y + cy - 2.0f, abs.x + cx + 2.0f, abs.y + cy + ch + 2.0f };
						this->_caretRectCacheValid = true;
						shouldDrawCaret = true;
						caretStart = { selRange[0].left + TextMargin - OffsetX, selRange[0].top + OffsetY };
						caretEnd = { selRange[0].left + TextMargin - OffsetX, selRange[0].top + selRange[0].height + OffsetY };
					}
				}
				auto lot = Factory::CreateStringLayout(MaskText, FLT_MAX, render_height, font->FontObject);
				if (lot) {
					d2d->DrawStringLayoutEffect(lot,
						TextMargin - OffsetX, OffsetY,
						this->ForeColor,
						DWRITE_TEXT_RANGE{ (UINT32)sels, (UINT32)selLen },
						this->SelectedForeColor,
						font);
					lot->Release();
				}
			}
			else
			{
				auto lot = Factory::CreateStringLayout(MaskText, FLT_MAX, render_height, font->FontObject);
				if (lot) {
					d2d->DrawStringLayout(lot,
						TextMargin - OffsetX, OffsetY,
						this->ForeColor);
					lot->Release();
				}
			}
		}
		else
		{
			if (isSelected)
			{
				const float cx = (float)TextMargin - OffsetX;
				const float cy = OffsetY;
				const float ch = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
				auto abs = this->AbsLocation;
				this->_caretRectCache = { abs.x + cx - 2.0f, abs.y + cy - 2.0f, abs.x + cx + 2.0f, abs.y + cy + ch + 2.0f };
				this->_caretRectCacheValid = true;
				shouldDrawCaret = true;
				caretStart = { (float)TextMargin - OffsetX, OffsetY };
				caretEnd = { (float)TextMargin - OffsetX, OffsetY + 16.0f };
			}
		}
		UpdateCaretBlinkState(isSelected, this->SelectionStart, this->SelectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
		if (shouldDrawCaret && IsCaretBlinkVisible())
		{
			d2d->DrawLine(caretStart, caretEnd, this->ForeColor);
		}
		d2d->DrawRect(0, 0, size.cx, size.cy, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}

bool PasswordBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	return GetCaretBlinkInvalidRect(outRect);
}
bool PasswordBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0);
		TCHAR strFileName[MAX_PATH]{};
		std::vector<std::wstring> files;
		for (int i = 0; i < uFileNum; i++)
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
		this->ParentForm->UnderMouse = this;
		if ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) && this->ParentForm->Selected == this)
		{
			auto font = this->Font;
			float render_height = this->Height - (TextMargin * 2.0f);
			std::wstring MaskText(this->Text.size(), L'*');
			SelectionEnd = font->HitTestTextPosition(MaskText, FLT_MAX, render_height, (xof - TextMargin) + this->OffsetX, yof - TextMargin);
			UpdateScroll();
			this->PostRender();
		}
		MouseEventArgs event_obj = MouseEventArgs(MouseButtons::None, 0, xof, yof, HIWORD(wParam));
		this->OnMouseMove(this, event_obj);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		if (WM_LBUTTONDOWN == message)
		{
			if (this->ParentForm->Selected != this)
			{
				auto lse = this->ParentForm->Selected;
				this->ParentForm->Selected = this;
				if (lse) lse->PostRender();
			}
			auto font = this->Font;
			float render_height = this->Height - (TextMargin * 2.0f);
			std::wstring MaskText(this->Text.size(), L'*');
			this->SelectionStart = this->SelectionEnd = font->HitTestTextPosition(MaskText, FLT_MAX, render_height, (xof - TextMargin) + this->OffsetX, yof - TextMargin);
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDown(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		if (this->ParentForm->Selected == this)
		{
			float render_height = this->Height - (TextMargin * 2.0f);
			auto font = this->Font;
			std::wstring MaskText(this->Text.size(), L'*');
			SelectionEnd = font->HitTestTextPosition(MaskText, FLT_MAX, render_height, (xof - TextMargin) + this->OffsetX, yof - TextMargin);
		}
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseUp(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		this->ParentForm->Selected = this;
		MouseEventArgs event_obj = MouseEventArgs(FromParamToMouseButtons(message), 0, xof, yof, HIWORD(wParam));
		this->OnMouseDoubleClick(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_KEYDOWN:
	{
		if (this->ParentForm)
		{
			D2D1_RECT_F imeRect{};
			if (this->_caretRectCacheValid)
			{
				imeRect = this->_caretRectCache;
			}
			else
			{
				auto abs = this->AbsLocation;
				float caretX = (float)abs.x + this->TextMargin - this->OffsetX;
				float caretY = (float)abs.y;
				float caretH = (this->Font && this->Font->FontHeight > 0.0f) ? this->Font->FontHeight : 16.0f;
				imeRect = D2D1_RECT_F{ caretX, caretY, caretX + 1.0f, caretY + caretH };
			}
			this->ParentForm->SetImeCompositionWindowFromLogicalRect(imeRect);
		}
		if (wParam == VK_DELETE)
		{
			this->InputDelete();
			UpdateScroll();
		}
		else if (wParam == VK_RIGHT)
		{
			if (this->SelectionEnd < this->Text.size())
			{
				this->SelectionEnd = this->SelectionEnd + 1;
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				{
					this->SelectionStart = this->SelectionEnd;
				}
				if (this->SelectionEnd > this->Text.size())
				{
					this->SelectionEnd = this->Text.size();
				}
				UpdateScroll();
			}
		}
		else if (wParam == VK_LEFT)
		{
			if (this->SelectionEnd > 0)
			{
				this->SelectionEnd = this->SelectionEnd - 1;
				if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
				{
					this->SelectionStart = this->SelectionEnd;
				}
				if (this->SelectionEnd < 0)
				{
					this->SelectionEnd = 0;
				}
				UpdateScroll();
			}
		}
		else if (wParam == VK_HOME)
		{
			auto font = this->Font;
			std::wstring MaskText(this->Text.size(), L'*');
			auto hit = font->HitTestTextRange(MaskText, (UINT32)this->SelectionEnd, (UINT32)0);
			this->SelectionEnd = font->HitTestTextPosition(MaskText, 0, hit[0].top + (font->FontHeight * 0.5f));
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd < 0)
			{
				this->SelectionEnd = 0;
			}
			UpdateScroll();
		}
		else if (wParam == VK_END)
		{
			std::wstring MaskText(this->Text.size(), L'*');
			auto font = this->Font;
			auto hit = font->HitTestTextRange(MaskText, (UINT32)this->SelectionEnd, (UINT32)0);
			this->SelectionEnd = font->HitTestTextPosition(MaskText, FLT_MAX, hit[0].top + (font->FontHeight * 0.5f));
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd > this->Text.size())
			{
				this->SelectionEnd = this->Text.size();
			}
			UpdateScroll();
		}
		else if (wParam == VK_PRIOR)
		{
			auto font = this->Font;
			std::wstring MaskText(this->Text.size(), L'*');
			auto hit = font->HitTestTextRange(MaskText, (UINT32)this->SelectionEnd, (UINT32)0);
			this->SelectionEnd = font->HitTestTextPosition(MaskText, hit[0].left, hit[0].top - this->Height);
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd < 0)
			{
				this->SelectionEnd = 0;
			}
			UpdateScroll(true);
		}
		else if (wParam == VK_NEXT)
		{
			auto font = this->Font;
			std::wstring MaskText(this->Text.size(), L'*');
			auto hit = font->HitTestTextRange(MaskText, (UINT32)this->SelectionEnd, (UINT32)0);
			this->SelectionEnd = font->HitTestTextPosition(MaskText, hit[0].left, hit[0].top + this->Height);
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) == false)
			{
				this->SelectionStart = this->SelectionEnd;
			}
			if (this->SelectionEnd > this->Text.size())
			{
				this->SelectionEnd = this->Text.size();
			}
			UpdateScroll(true);
		}
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, event_obj);
		this->PostRender();
	}
	break;
	case WM_CHAR:
	{
		wchar_t ch = (wchar_t)(wParam);
		if (ch >= 32 && ch != 127)
		{
			const wchar_t c[] = { ch,L'\0' };
			this->InputText(c);
			UpdateScroll();
		}
		else if (ch == 1)
		{
			this->SelectionStart = 0;
			this->SelectionEnd = this->Text.size();
			UpdateScroll();
		}
		else if (ch == 8)
		{
			if (this->Text.size() > 0)
			{
				this->InputBack();
				UpdateScroll();
			}
		}
		else if (ch == 22)
		{
			if (OpenClipboard(this->ParentForm->Handle))
			{
				if (IsClipboardFormatAvailable(CF_UNICODETEXT))
				{
					HANDLE hClip = GetClipboardData(CF_UNICODETEXT);
					const wchar_t* pBuf = hClip ? (const wchar_t*)GlobalLock(hClip) : nullptr;
					if (pBuf)
					{
						this->InputText(pBuf);
						UpdateScroll();
						GlobalUnlock(hClip);
					}
				}
				CloseClipboard();
			}
		}
		this->PostRender();
	}
	break;
	case WM_IME_COMPOSITION:
	{
		if (lParam & GCS_RESULTSTR)
		{
			HIMC hIMC = ImmGetContext(this->ParentForm->Handle);
			if (hIMC)
			{
				LONG bytes = ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, NULL, 0);
				if (bytes > 0)
				{
					int wcharCount = bytes / (int)sizeof(wchar_t);
					std::wstring buffer;
					buffer.resize(wcharCount);
					ImmGetCompositionStringW(hIMC, GCS_RESULTSTR, buffer.data(), bytes);
					this->InputText(buffer);
				}
				ImmReleaseContext(this->ParentForm->Handle, hIMC);
			}
			UpdateScroll();
			this->PostRender();
		}
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs event_obj = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, event_obj);
		this->PostRender();
	}
	break;
	}
	return true;
}