#pragma once
#include "TabControl.h"
#include "Panel.h"
#include "Form.h"
#include <algorithm>
#include <cmath>
#pragma comment(lib, "Imm32.lib")

UIClass TabPage::Type() { return UIClass::UI_TabPage; }
TabPage::TabPage()
{
	this->Text = L"Page";
}
TabPage::TabPage(std::wstring text)
{
	this->Text = text;
}

UIClass TabControl::Type() { return UIClass::UI_TabControl; }
TabControl::TabControl(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
}

static float EaseOutCubic01(float t)
{
	t = (std::clamp)(t, 0.0f, 1.0f);
	return 1.0f - std::pow(1.0f - t, 3.0f);
}

static void SyncNativeChildWindowsRecursive(Control* root)
{
	if (!root) return;
	if (root->Type() == UIClass::UI_WebBrowser)
	{
		root->SyncNativeSurface();
	}
	for (auto* child : root->Children)
	{
		SyncNativeChildWindowsRecursive(child);
	}
}

static void SyncNativeChildWindowsForAllPages(TabControl* tc)
{
	if (!tc) return;
	for (int i = 0; i < tc->Count; i++)
	{
		auto page = tc->operator[](i);
		SyncNativeChildWindowsRecursive(page);
	}
}

void TabControl::ClampSelectedIndex()
{
	if (this->Count <= 0)
	{
		this->SelectedIndex = 0;
		return;
	}
	if (this->SelectedIndex < 0) this->SelectedIndex = 0;
	if (this->SelectedIndex >= this->Count) this->SelectedIndex = this->Count - 1;
}

void TabControl::LayoutPage(TabPage* page, int offsetX)
{
	if (!page) return;
	page->SetRuntimeLocation(POINT{ offsetX, this->TitleHeight });
	SIZE s = this->Size;
	s.cy = std::max(0L, s.cy - this->TitleHeight);
	page->Size = s;
}

void TabControl::SyncPageVisibility()
{
	for (int i = 0; i < this->Count; i++)
	{
		auto* page = this->operator[](i);
		if (!page) continue;
		if (_animating)
			page->Visible = (i == _animFromIndex || i == _animToIndex);
		else
			page->Visible = (i == _displayIndex);
	}
}

void TabControl::FinishTransition()
{
	if (this->Count <= 0)
	{
		_animating = false;
		_animFromIndex = -1;
		_animToIndex = -1;
		_displayIndex = -1;
		return;
	}
	ClampSelectedIndex();
	_animating = false;
	_animProgress = 1.0f;
	_displayIndex = this->SelectedIndex;
	_animFromIndex = _displayIndex;
	_animToIndex = _displayIndex;
	SyncPageVisibility();
	if (_displayIndex >= 0 && _displayIndex < this->Count)
		LayoutPage((TabPage*)this->operator[](_displayIndex), 0);
	SyncNativeChildWindowsForAllPages(this);
	_lastSelectIndex = _displayIndex;
}

void TabControl::StartTransitionTo(int newIndex)
{
	if (this->Count <= 0)
	{
		this->SelectedIndex = 0;
		FinishTransition();
		return;
	}
	if (_animating)
		FinishTransition();
	if (_displayIndex < 0 || _displayIndex >= this->Count)
		_displayIndex = (std::clamp)(this->SelectedIndex, 0, this->Count - 1);
	newIndex = (std::clamp)(newIndex, 0, this->Count - 1);
	this->SelectedIndex = newIndex;
	if (this->AnimationMode == TabControlAnimationMode::DirectReplace || _displayIndex == newIndex)
	{
		FinishTransition();
		return;
	}
	_animFromIndex = _displayIndex;
	_animToIndex = newIndex;
	_animStartTick = ::GetTickCount64();
	_animProgress = 0.0f;
	_animating = true;
	SyncPageVisibility();
	LayoutPage((TabPage*)this->operator[](_animFromIndex), 0);
	LayoutPage((TabPage*)this->operator[](_animToIndex), 0);
	SyncNativeChildWindowsForAllPages(this);
	_lastSelectIndex = -1;
}

void TabControl::EnsureSelectionState()
{
	if (this->Count <= 0)
	{
		_displayIndex = -1;
		_animating = false;
		return;
	}
	ClampSelectedIndex();
	if (_displayIndex < 0 || _displayIndex >= this->Count)
		_displayIndex = this->SelectedIndex;
	if (_animating)
	{
		CurrentTransitionProgress();
		return;
	}
	if (this->SelectedIndex != _displayIndex)
		StartTransitionTo(this->SelectedIndex);
	else
		SyncPageVisibility();
}

float TabControl::CurrentTransitionProgress()
{
	if (!_animating) return _animProgress;
	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _animStartTick ? (now - _animStartTick) : 0;
	float t = _animDurationMs > 0 ? (float)elapsed / (float)_animDurationMs : 1.0f;
	if (t >= 1.0f)
	{
		FinishTransition();
		return 1.0f;
	}
	_animProgress = EaseOutCubic01(t);
	return _animProgress;
}

bool TabControl::IsAnimationRunning()
{
	EnsureSelectionState();
	return _animating;
}

bool TabControl::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!IsAnimationRunning()) return false;
	outRect = this->AbsRect;
	return true;
}

TabPage* TabControl::AddPage(std::wstring name)
{
	TabPage* result = this->AddControl(new TabPage(name));
	result->BackColor = this->BackColor;
	result->SetRuntimeLocation(POINT{ 0, this->TitleHeight });
	{
		SIZE s = this->Size;
		s.cy = std::max(0L, s.cy - this->TitleHeight);
		result->Size = s;
	}
	for (int i = 0; i < this->Count; i++)
	{
		this->operator[](i)->Visible = (this->SelectedIndex == i);
	}
	_displayIndex = (std::clamp)(this->SelectedIndex, 0, this->Count - 1);
	_animFromIndex = _displayIndex;
	_animToIndex = _displayIndex;
	_animating = false;
	// 新增页后也同步一次原生子窗口（避免被隐藏页“遗留显示”）
	SyncNativeChildWindowsForAllPages(this);
	return result;
}

int TabControl::InsertPage(int index, TabPage* page)
{
	if (!page || page->Parent) return -1;
	const int insertIndex = std::clamp(index, 0, this->Count);
	page->Parent = this;
	page->ParentForm = this->ParentForm;
	Control::SetChildrenParentForm(page, this->ParentForm);
	this->Children.insert(this->Children.begin() + insertIndex, page);
	page->BackColor = this->BackColor;
	LayoutPage(page, 0);
	if (this->Count == 1)
	{
		this->SelectedIndex = 0;
	}
	else if (insertIndex <= this->SelectedIndex)
	{
		this->SelectedIndex++;
	}
	FinishTransition();
	this->PostRender();
	return insertIndex;
}

bool TabControl::RemovePageAt(int index, bool deletePage)
{
	if (index < 0 || index >= this->Count) return false;
	TabPage* page = static_cast<TabPage*>(this->operator[](index));
	const bool selectedChanged = index == this->SelectedIndex;
	this->Children.erase(this->Children.begin() + index);
	page->Parent = NULL;
	page->ParentForm = NULL;
	if (deletePage)
		delete page;
	if (this->Count <= 0)
	{
		this->SelectedIndex = 0;
	}
	else if (this->SelectedIndex >= this->Count)
	{
		this->SelectedIndex = this->Count - 1;
	}
	else if (index < this->SelectedIndex)
	{
		this->SelectedIndex--;
	}
	FinishTransition();
	if (selectedChanged)
		this->OnSelectedChanged(this);
	this->PostRender();
	return true;
}

bool TabControl::RemovePage(TabPage* page, bool deletePage)
{
	if (!page) return false;
	for (int i = 0; i < this->Count; i++)
	{
		if (this->operator[](i) == page)
			return RemovePageAt(i, deletePage);
	}
	return false;
}

void TabControl::ClearPages(bool deletePages)
{
	if (this->Children.empty()) return;
	auto pages = this->Children;
	this->Children.clear();
	for (auto* page : pages)
	{
		if (!page) continue;
		page->Parent = NULL;
		page->ParentForm = NULL;
		if (deletePages)
			delete page;
	}
	this->SelectedIndex = 0;
	FinishTransition();
	this->OnSelectedChanged(this);
	this->PostRender();
}

int TabControl::FindPage(const std::wstring& text) const
{
	for (int i = 0; i < static_cast<int>(this->Children.size()); i++)
	{
		if (this->Children[static_cast<size_t>(i)] && this->Children[static_cast<size_t>(i)]->Text == text)
			return i;
	}
	return -1;
}

TabPage* TabControl::SelectedPage() const
{
	if (this->SelectedIndex < 0 || this->SelectedIndex >= static_cast<int>(this->Children.size()))
		return nullptr;
	return static_cast<TabPage*>(this->Children[static_cast<size_t>(this->SelectedIndex)]);
}

void TabControl::SelectPage(int index, bool fireEvent)
{
	if (this->Count <= 0)
	{
		this->SelectedIndex = 0;
		return;
	}
	const int newIndex = std::clamp(index, 0, this->Count - 1);
	if (this->SelectedIndex == newIndex)
	{
		EnsureSelectionState();
		return;
	}
	this->SelectedIndex = newIndex;
	StartTransitionTo(newIndex);
	if (fireEvent)
		this->OnSelectedChanged(this);
	this->PostRender();
}

GET_CPP(TabControl, int, PageCount)
{
	return this->Count;
}
GET_CPP(TabControl, std::vector<Control*>&, Pages)
{
	return this->Children;
}
void TabControl::Update()
{
	if (this->IsVisual == false)return;
	auto d2d = this->ParentForm->Render;
	auto font = this->Font;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	const float titleWidth = static_cast<float>(this->TitleWidth);
	const float titleHeight = static_cast<float>(this->TitleHeight);
	const float contentHeight = static_cast<float>(std::max(0L, size.cy - this->TitleHeight));
	EnsureSelectionState();
	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		
		if (this->Count > 0)
		{
			ClampSelectedIndex();

			for (int i = 0; i < this->Count; i++)
			{
				auto textsize = font->GetTextSize(this->operator[](i)->Text);
				float lf = (titleWidth - textsize.width) / 2.0f;
				if (lf < 0)lf = 0;
				float tf = (titleHeight - textsize.height) / 2.0f;
				if (tf < 0)tf = 0;
				const float titleLeft = titleWidth * static_cast<float>(i);
				d2d->PushDrawRect(titleLeft, 0, titleWidth, titleHeight);
				if (i == this->SelectedIndex)
					d2d->FillRect(titleLeft, 0, titleWidth, titleHeight, this->SelectedTitleBackColor);
				else
					d2d->FillRect(titleLeft, 0, titleWidth, titleHeight, this->TitleBackColor);
				d2d->DrawString(this->operator[](i)->Text, titleLeft + lf, tf, this->ForeColor, font);
				d2d->DrawRect(titleLeft, 0, titleWidth, titleHeight, this->BolderColor, this->Boder);
				d2d->PopDrawRect();
			}
			if (contentHeight > 0.0f)
			{
				d2d->PushDrawRect(0.0f, titleHeight, actualWidth, contentHeight);
				if (_animating && this->AnimationMode == TabControlAnimationMode::SlideHorizontal &&
					_animFromIndex >= 0 && _animFromIndex < this->Count && _animToIndex >= 0 && _animToIndex < this->Count)
				{
					const float progress = CurrentTransitionProgress();
					const int direction = (_animToIndex >= _animFromIndex) ? 1 : -1;
					const int contentWidth = size.cx;
					auto* fromPage = (TabPage*)this->operator[](_animFromIndex);
					auto* toPage = (TabPage*)this->operator[](_animToIndex);
					LayoutPage(fromPage, (int)std::lround(-(float)direction * progress * contentWidth));
					LayoutPage(toPage, (int)std::lround((float)direction * (1.0f - progress) * contentWidth));
					fromPage->Update();
					toPage->Update();
				}
				else if (_displayIndex >= 0 && _displayIndex < this->Count)
				{
					auto* page = (TabPage*)this->operator[](_displayIndex);
					LayoutPage(page, 0);
					page->Update();
					if (this->_lastSelectIndex != _displayIndex)
					{
						SyncNativeChildWindowsForAllPages(this);
						this->_lastSelectIndex = _displayIndex;
					}
				}
				d2d->PopDrawRect();
			}
		}
		d2d->DrawRect(0, titleHeight, actualWidth, contentHeight, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}
bool TabControl::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;

	if (this->Count > 0)
	{
		EnsureSelectionState();

		// 先处理标题栏点击（切换页）：
		if (message == WM_LBUTTONDOWN && yof < this->TitleHeight)
		{
			if (xof < (this->Count * this->TitleWidth))
			{
				int newSelected = xof / this->TitleWidth;
				if (this->SelectedIndex != newSelected)
				{
					this->SelectedIndex = newSelected;
					StartTransitionTo(newSelected);
					this->OnSelectedChanged(this);
				}

				this->_capturedChild = NULL;
				if (GetCapture() == this->ParentForm->Handle)
					ReleaseCapture();
				this->PostRender();
			}
		}

		// Content 区域坐标
		const int cy = yof - this->TitleHeight;
		TabPage* page = (_displayIndex >= 0 && _displayIndex < this->Count)
			? (TabPage*)this->operator[](_displayIndex)
			: (TabPage*)this->operator[](this->SelectedIndex);

		auto forwardToChild = [&](Control* c)
			{
				if (!c) return;
				auto location = c->ActualLocation;
				c->ProcessMessage(message, wParam, lParam, xof - location.x, cy - location.y);
			};

		// 鼠标按住期间：持续转发到按下时命中的子控件（解决拖动/松开丢失）
		bool mousePressed = (wParam & MK_LBUTTON) || (wParam & MK_RBUTTON) || (wParam & MK_MBUTTON);
		if (_animating)
		{
			if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
			{
				this->_capturedChild = NULL;
				if (GetCapture() == this->ParentForm->Handle)
					ReleaseCapture();
			}
		}
		else if ((message == WM_MOUSEMOVE || message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP) && this->_capturedChild)
		{
			forwardToChild(this->_capturedChild);
			if (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP)
			{
				this->_capturedChild = NULL;
				if (GetCapture() == this->ParentForm->Handle)
					ReleaseCapture();
			}
		}
		else if ((message == WM_MOUSEMOVE && mousePressed) && this->_capturedChild)
		{
			forwardToChild(this->_capturedChild);
		}
		else
		{
			// 按下时：命中哪个子控件就捕获它
			if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN ||
				message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP ||
				message == WM_LBUTTONDBLCLK || message == WM_RBUTTONDBLCLK || message == WM_MBUTTONDBLCLK ||
				message == WM_MOUSEMOVE || message == WM_MOUSEWHEEL)
			{
				// 只在 content 区域才命中子控件
				if (cy >= 0)
				{
					Control* hit = NULL;
					for (int i = page->Count - 1; i >= 0; i--)
					{
						auto c = page->operator[](i);
						if (!c || !c->Visible || !c->Enable) continue;
						auto loc = c->ActualLocation;
						auto sz = c->ActualSize();
						if (xof >= loc.x && cy >= loc.y && xof <= (loc.x + sz.cx) && cy <= (loc.y + sz.cy))
						{
							hit = c;
							break;
						}
					}

					if (hit)
					{
						// 捕获鼠标，确保鼠标移出窗口也能持续收到 move/up（拖动选中/下拉框等）
						if (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN)
						{
							this->_capturedChild = hit;
							if (this->ParentForm && this->ParentForm->Handle)
								SetCapture(this->ParentForm->Handle);
						}
						forwardToChild(hit);
					}
					else if (page)
					{
						page->ProcessMessage(message, wParam, lParam, xof, cy);
					}
				}
			}
		}
	}

	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT uFileNum = DragQueryFile(hDropInfo, 0xffffffff, NULL, 0);
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
		// 防御：如果捕获还在，释放掉
		if (this->_capturedChild && (GetCapture() == this->ParentForm->Handle))
			ReleaseCapture();
		this->_capturedChild = NULL;
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
