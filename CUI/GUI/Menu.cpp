#include "Menu.h"
#include "Form.h"
#include <algorithm>

UIClass MenuItem::Type() { return UIClass::UI_MenuItem; }

MenuItem::MenuItem(std::wstring text, int id)
{
	this->Text = text;
	this->Id = id;
	// 使用私有成员直接赋值,避免触发PostRender
	this->_backcolor = D2D1_COLOR_F{ 0,0,0,0 };
	this->_boldercolor = D2D1_COLOR_F{ 0,0,0,0 };
	this->_forecolor = Colors::WhiteSmoke;
	this->Cursor = CursorKind::Hand;
}

MenuItem::~MenuItem()
{
	for (auto* it : SubItems)
		delete it;
	SubItems.clear();
}

MenuItem* MenuItem::AddSubItem(std::wstring text, int id)
{
	auto* it = new MenuItem(text, id);
	// ForeColor 已经在构造函数中设置,无需重复设置
	SubItems.push_back(it);
	return it;
}

MenuItem* MenuItem::AddSeparator()
{
	SubItems.push_back(MenuItem::CreateSeparator());
	return SubItems.back();
}

MenuItem* MenuItem::CreateSeparator()
{
	auto* it = new MenuItem(L"", 0);
	it->Separator = true;
	it->Enable = false;
	return it;
}

void MenuItem::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm->Render;
	auto abs = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		bool hover = (this->ParentForm->UnderMouse == this);
		if (hover)
			d2d->FillRect(abs.x, abs.y, size.cx, size.cy, HoverBackColor);

		auto font = this->Font;
		auto ts = font->GetTextSize(this->Text);
		float tx = 10.0f;
		float ty = ((float)this->Height - ts.height) * 0.5f;
		if (ty < 0) ty = 0;
		d2d->DrawString(this->Text, abs.x + tx, abs.y + ty, this->ForeColor, font);
	}
	d2d->PopDrawRect();
}

bool MenuItem::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_MOUSEMOVE:
		this->ParentForm->UnderMouse = this;
		break;
	}
	return Control::ProcessMessage(message, wParam, lParam, xof, yof);
}

UIClass Menu::Type() { return UIClass::UI_Menu; }

Menu::Menu(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BarHeight = height;
	this->BackColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->BolderColor = D2D1_COLOR_F{ 0,0,0,0 };
	this->Cursor = CursorKind::Arrow;
}

MenuItem* Menu::AddItem(std::wstring text)
{
	auto* item = this->AddControl(new MenuItem(text, 0));
	item->Height = this->BarHeight;
	// ForeColor 已经在构造函数中设置,无需重复设置
	return item;
}

int Menu::DropCount()
{
	if (!_expand) return 0;
	if (_expandIndex < 0 || _expandIndex >= this->Count) return 0;
	auto* top = (MenuItem*)this->operator[](_expandIndex);
	if (!top) return 0;
	return (int)top->SubItems.size();
}

float Menu::DropLeftLocal()
{
	if (_expandIndex < 0 || _expandIndex >= this->Count) return 0.0f;
	auto* top = (MenuItem*)this->operator[](_expandIndex);
	if (!top) return 0.0f;
	return (float)top->Left;
}

float Menu::DropWidthLocal()
{
	if (!_expand) return 0.0f;
	if (_expandIndex < 0 || _expandIndex >= this->Count) return 0.0f;
	auto* top = (MenuItem*)this->operator[](_expandIndex);
	if (!top) return 0.0f;
	float w = 120.0f;
	auto font = this->Font;
	for (auto* it : top->SubItems)
	{
		if (!it) continue;
		if (it->Separator) continue;
		auto ts = font->GetTextSize(it->Text);
		float tw = ts.width + 24.0f;
		if (!it->Shortcut.empty())
		{
			auto ss = font->GetTextSize(it->Shortcut);
			tw += ss.width + 20.0f;
		}
		if (tw > w) w = tw;
	}
	float maxw = (float)this->Width - DropLeftLocal();
	if (w > maxw) w = maxw;
	if (w < 80.0f) w = 80.0f;
	return w;
}

float Menu::DropHeightLocal()
{
	int c = DropCount();
	if (c <= 0) return 0.0f;
	return DropPaddingY * 2.0f + (float)c * (float)DropItemHeight;
}

bool Menu::HasSubMenu(int dropIndex)
{
	if (_expandIndex < 0 || _expandIndex >= this->Count) return false;
	auto* top = (MenuItem*)this->operator[](_expandIndex);
	if (!top) return false;
	if (dropIndex < 0 || dropIndex >= (int)top->SubItems.size()) return false;
	auto* it = top->SubItems[dropIndex];
	return it && !it->Separator && !it->SubItems.empty();
}

SIZE Menu::ActualSize()
{
	auto s = this->Size;
	if (_expand)
	{
		// 展开时尽量覆盖整个内容区域，便于点击空白处收起
		if (this->ParentForm)
		{
			// 注意：Form::ClientSize 本身就是“内容区高度”（已扣除 HeadHeight）
			// 这里不应再次扣除 top，否则会导致展开区域变小，点击空白处无法正确命中/收起。
			int contentH = this->ParentForm->ClientSize.cy;
			if (contentH < BarHeight) contentH = BarHeight;
			s.cy = contentH;
		}
		else
		{
			s.cy = (int)((float)BarHeight + DropHeightLocal());
		}
	}
	else
	{
		s.cy = BarHeight;
	}
	return s;
}

void Menu::Update()
{
	if (!this->IsVisual) return;
	// 主菜单单独管理：记录到 Form::MainMenu（不再使用 ForegroundControls 容器）
	if (this->ParentForm)
	{
		this->ParentForm->MainMenu = this;
	}
	auto d2d = this->ParentForm->Render;
	auto abs = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;
	absRect.bottom = absRect.top + size.cy;
	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		// bar
		d2d->FillRect(abs.x, abs.y, (float)this->Width, (float)BarHeight, BarBackColor);
		d2d->DrawRect(abs.x, abs.y + (float)BarHeight - Boder, (float)this->Width, Boder, BarBorderColor, Boder);

		// layout top items
		float x = 6.0f;
		auto font = this->Font;
		for (int i = 0; i < this->Count; i++)
		{
			auto* it = (MenuItem*)this->operator[](i);
			if (!it) continue;
			auto ts = font->GetTextSize(it->Text);
			int w = (int)(ts.width + ItemPaddingX * 2.0f);
			if (w < 50) w = 50;
			it->Location = POINT{ (int)x, 0 };
			it->Size = SIZE{ w, BarHeight };
			x += (float)w;
			it->Update();
		}

		// dropdown
		if (_expand && DropCount() > 0)
		{
			float dl = abs.x + DropLeftLocal();
			float dt = abs.y + DropTopLocal();
			float dw = DropWidthLocal();
			float dh = DropHeightLocal();
			d2d->FillRoundRect(dl, dt, dw, dh, DropBackColor, 4.0f);
			d2d->DrawRoundRect(dl, dt, dw, dh, DropBorderColor, 1.0f, 4.0f);

			auto* top = (MenuItem*)this->operator[](_expandIndex);
			for (int i = 0; i < (int)top->SubItems.size(); i++)
			{
				auto* sub = top->SubItems[i];
				float iy = dt + DropPaddingY + (float)i * (float)DropItemHeight;
				if (sub && sub->Separator)
				{
					float y = iy + (float)DropItemHeight * 0.5f;
					d2d->DrawLine(dl + 10.0f, y, dl + dw - 10.0f, y, DropSeparatorColor, 1.0f);
					continue;
				}
				if (i == _hoverDropIndex || i == _openSubOwnerIndex)
					d2d->FillRect(dl + 2.0f, iy, dw - 4.0f, (float)DropItemHeight, DropHoverColor);

				if (sub)
				{
					auto ts = font->GetTextSize(sub->Text);
					float ty = iy + ((float)DropItemHeight - ts.height) * 0.5f;
					if (ty < iy) ty = iy;
					d2d->DrawString(sub->Text, dl + 12.0f, ty, DropTextColor, font);
					if (!sub->Shortcut.empty())
					{
						auto ss = font->GetTextSize(sub->Shortcut);
						float sx = dl + dw - 12.0f - ss.width;
						d2d->DrawString(sub->Shortcut, sx, ty, DropTextColor, font);
					}
					// 子菜单指示
					if (!sub->SubItems.empty())
					{
						auto as = font->GetTextSize(L"›");
						float ax = dl + dw - 12.0f - as.width;
						d2d->DrawString(L"›", ax, ty, DropTextColor, font);
					}
				}
			}

			// 二级子菜单
			if (_openSubOwnerIndex >= 0 && _openSubOwnerIndex < (int)top->SubItems.size())
			{
				auto* owner = top->SubItems[_openSubOwnerIndex];
				if (owner && !owner->Separator && !owner->SubItems.empty())
				{
					// 计算子菜单宽度
					float subw = 140.0f;
					for (auto* it : owner->SubItems)
					{
						if (!it || it->Separator) continue;
						auto ts = font->GetTextSize(it->Text);
						float tw = ts.width + 24.0f;
						if (!it->Shortcut.empty())
						{
							auto ss = font->GetTextSize(it->Shortcut);
							tw += ss.width + 20.0f;
						}
						if (tw > subw) subw = tw;
					}
					if (subw < 100.0f) subw = 100.0f;

					float subh = DropPaddingY * 2.0f + (float)owner->SubItems.size() * (float)DropItemHeight;
					float subx = dl + dw + 4.0f;
					float suby = dt + DropPaddingY + (float)_openSubOwnerIndex * (float)DropItemHeight;

					// 右侧放不下则放左侧
					if (this->ParentForm)
					{
						float maxX = (float)this->ParentForm->ClientSize.cx;
						if (subx + subw > maxX)
							subx = dl - subw - 4.0f;
						if (subx < 0.0f) subx = 0.0f;
					}

					d2d->FillRoundRect(subx, suby, subw, subh, DropBackColor, 4.0f);
					d2d->DrawRoundRect(subx, suby, subw, subh, DropBorderColor, 1.0f, 4.0f);

					for (int j = 0; j < (int)owner->SubItems.size(); j++)
					{
						auto* it = owner->SubItems[j];
						float iy = suby + DropPaddingY + (float)j * (float)DropItemHeight;
						if (it && it->Separator)
						{
							float y = iy + (float)DropItemHeight * 0.5f;
							d2d->DrawLine(subx + 10.0f, y, subx + subw - 10.0f, y, DropSeparatorColor, 1.0f);
							continue;
						}
						if (j == _hoverSubIndex)
							d2d->FillRect(subx + 2.0f, iy, subw - 4.0f, (float)DropItemHeight, DropHoverColor);
						if (it)
						{
							auto ts = font->GetTextSize(it->Text);
							float ty = iy + ((float)DropItemHeight - ts.height) * 0.5f;
							if (ty < iy) ty = iy;
							d2d->DrawString(it->Text, subx + 12.0f, ty, DropTextColor, font);
							if (!it->Shortcut.empty())
							{
								auto ss = font->GetTextSize(it->Shortcut);
								float sx = subx + subw - 12.0f - ss.width;
								d2d->DrawString(it->Shortcut, sx, ty, DropTextColor, font);
							}
						}
					}
				}
			}
		}
	}
	d2d->PopDrawRect();
}

bool Menu::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible) return true;

	// 主菜单单独管理：记录到 Form::MainMenu
	if (this->ParentForm)
	{
		this->ParentForm->MainMenu = this;
	}

	// route to top items (bar area only)
	if (yof >= 0 && yof < BarHeight)
	{
		_hoverTopIndex = -1;
		for (int i = 0; i < this->Count; i++)
		{
			auto* it = (MenuItem*)this->operator[](i);
			if (!it) continue;
			auto loc = it->Location;
			auto sz = it->ActualSize();
			if (xof >= loc.x && xof <= (loc.x + sz.cx))
			{
				_hoverTopIndex = i;
				it->ProcessMessage(message, wParam, lParam, xof - loc.x, yof - loc.y);
				break;
			}
		}

		if (message == WM_MOUSEMOVE)
		{
			this->ParentForm->UnderMouse = this;
			// hover切换展开的一级菜单
			if (_expand && _hoverTopIndex >= 0 && _hoverTopIndex != _expandIndex)
			{
				_expandIndex = _hoverTopIndex;
				_hoverDropIndex = -1;
				this->PostRender();
			}
		}
		else if (message == WM_LBUTTONUP)
		{
			if (_hoverTopIndex >= 0)
			{
				if (_expand && _expandIndex == _hoverTopIndex)
				{
					_expand = false;
					_expandIndex = -1;
					_hoverDropIndex = -1;
					_openSubOwnerIndex = -1;
					_hoverSubIndex = -1;
				}
				else
				{
					_expand = true;
					_expandIndex = _hoverTopIndex;
					_hoverDropIndex = -1;
					_openSubOwnerIndex = -1;
					_hoverSubIndex = -1;
				}
				// 展开/收起：立即触发一次重绘，避免 WM_PAINT 延后导致残影
				if (this->ParentForm) this->ParentForm->Invalidate(true);
				else this->PostRender();
			}
		}

		return Control::ProcessMessage(message, wParam, lParam, xof, yof);
	}

	// dropdown interactions
	if (_expand && DropCount() > 0)
	{
		float dl = DropLeftLocal();
		float dt = DropTopLocal();
		float dw = DropWidthLocal();
		float dh = DropHeightLocal();

		bool inDrop = ((float)xof >= dl && (float)xof <= dl + dw && (float)yof >= dt && (float)yof <= dt + dh);

		// 二级子菜单区域
		bool inSub = false;
		bool inBridge = false;
		float subx = 0, suby = 0, subw = 0, subh = 0;
		auto* top = (MenuItem*)this->operator[](_expandIndex);
		MenuItem* owner = nullptr;
		if (top && _openSubOwnerIndex >= 0 && _openSubOwnerIndex < (int)top->SubItems.size())
		{
			owner = top->SubItems[_openSubOwnerIndex];
			if (owner && !owner->Separator && !owner->SubItems.empty())
			{
				// compute same as Update()
				auto font = this->Font;
				subw = 140.0f;
				for (auto* it : owner->SubItems)
				{
					if (!it || it->Separator) continue;
					auto ts = font->GetTextSize(it->Text);
					float tw = ts.width + 24.0f;
					if (!it->Shortcut.empty())
					{
						auto ss = font->GetTextSize(it->Shortcut);
						tw += ss.width + 20.0f;
					}
					if (tw > subw) subw = tw;
				}
				if (subw < 100.0f) subw = 100.0f;
				subh = DropPaddingY * 2.0f + (float)owner->SubItems.size() * (float)DropItemHeight;
				// 子菜单贴紧一级菜单（避免出现鼠标穿越空隙导致丢失 hover）
				subx = dl + dw - 1.0f;
				suby = dt + DropPaddingY + (float)_openSubOwnerIndex * (float)DropItemHeight;
				if (this->ParentForm)
				{
					float maxX = (float)this->ParentForm->ClientSize.cx;
					if (subx + subw > maxX) subx = dl - subw - 4.0f;
					if (subx < 0.0f) subx = 0.0f;
				}
				inSub = ((float)xof >= subx && (float)xof <= subx + subw && (float)yof >= suby && (float)yof <= suby + subh);
				// “桥接区域”：允许鼠标在一级菜单右边缘与子菜单左边缘之间移动，不触发关闭/丢失
				float bridgeL = dl + dw - 2.0f;
				float bridgeR = subx + 2.0f;
				float bridgeT = suby;
				float bridgeB = suby + subh;
				inBridge = ((float)xof >= bridgeL && (float)xof <= bridgeR && (float)yof >= bridgeT && (float)yof <= bridgeB);
			}
		}

		if (message == WM_MOUSEMOVE)
		{
			if (inSub && owner)
			{
				int idx = (int)(((float)yof - (suby + DropPaddingY)) / (float)DropItemHeight);
				int cnt = (int)owner->SubItems.size();
				if (idx < 0 || idx >= cnt) idx = -1;
				if (idx != _hoverSubIndex)
				{
					_hoverSubIndex = idx;
					this->PostRender();
				}
			}
			else if (inDrop || inBridge)
			{
				// 只有在真正位于一级下拉区域时才更新 hover；桥接区只负责“不断开”
				if (inDrop)
				{
					int idx = (int)(((float)yof - (dt + DropPaddingY)) / (float)DropItemHeight);
					int cnt = DropCount();
					if (idx < 0 || idx >= cnt) idx = -1;
					bool need = false;
					if (idx != _hoverDropIndex)
					{
						_hoverDropIndex = idx;
						_hoverSubIndex = -1;
						need = true;
					}
					// hover 到有子菜单的项：打开二级菜单
					int newOwner = (HasSubMenu(_hoverDropIndex) ? _hoverDropIndex : -1);
					if (newOwner != _openSubOwnerIndex)
					{
						_openSubOwnerIndex = newOwner;
						_hoverSubIndex = -1;
						need = true;
					}
					if (need) this->PostRender();
				}
			}
			else
			{
				// 离开下拉区域：取消 hover（但保持展开，点击空白会收起）
				if (_hoverDropIndex != -1 || _hoverSubIndex != -1 || _openSubOwnerIndex != -1)
				{
					_hoverDropIndex = -1;
					_hoverSubIndex = -1;
					_openSubOwnerIndex = -1;
					this->PostRender();
				}
			}
		}
		else if (message == WM_LBUTTONUP)
		{
			if (inSub && owner)
			{
				int idx = (int)(((float)yof - (suby + DropPaddingY)) / (float)DropItemHeight);
				int cnt = (int)owner->SubItems.size();
				if (idx >= 0 && idx < cnt)
				{
					auto* it = owner->SubItems[idx];
					if (it && !it->Separator && it->SubItems.empty() && it->Id != 0)
					{
						this->OnMenuCommand(this, it->Id);
					}
				}
				_expand = false;
				_expandIndex = -1;
				_hoverDropIndex = -1;
				_openSubOwnerIndex = -1;
				_hoverSubIndex = -1;
				this->ParentForm->Invalidate();
			}
			else if (inDrop)
			{
				int idx = (int)(((float)yof - (dt + DropPaddingY)) / (float)DropItemHeight);
				int cnt = DropCount();
				if (idx >= 0 && idx < cnt)
				{
					auto* sub = top ? top->SubItems[idx] : nullptr;
					if (sub && !sub->Separator)
					{
						// 有二级菜单：只展开二级，不直接触发
						if (!sub->SubItems.empty())
						{
							_openSubOwnerIndex = idx;
							_hoverDropIndex = idx;
							_hoverSubIndex = -1;
							this->PostRender();
						}
						else
						{
							if (sub->Id != 0)
								this->OnMenuCommand(this, sub->Id);
							_expand = false;
							_expandIndex = -1;
							_hoverDropIndex = -1;
							_openSubOwnerIndex = -1;
							_hoverSubIndex = -1;
							this->ParentForm->Invalidate(true);
						}
					}
				}
			}
			else
			{
				// 点击到下拉外区域：只收起
				_expand = false;
				_expandIndex = -1;
				_hoverDropIndex = -1;
				_openSubOwnerIndex = -1;
				_hoverSubIndex = -1;
				// 收起时：强制立即全量重绘，清除 Overlay 残影
				if (this->ParentForm) this->ParentForm->Invalidate(true);
				else this->PostRender();
			}
		}
	}
	// 展开时点击菜单栏/下拉之外：收起（配合 ActualSize 覆盖内容区）
	else if (_expand && message == WM_LBUTTONUP)
	{
				_expand = false;
				_expandIndex = -1;
				_hoverDropIndex = -1;
				_openSubOwnerIndex = -1;
				_hoverSubIndex = -1;
				// 收起时：强制立即全量重绘，清除 Overlay 残影
				if (this->ParentForm) this->ParentForm->Invalidate(true);
				else this->PostRender();
	}

	return Control::ProcessMessage(message, wParam, lParam, xof, yof);
}

