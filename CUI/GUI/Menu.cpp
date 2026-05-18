#include "Menu.h"
#include "Form.h"
#include <algorithm>

namespace
{
	struct MenuPanel
	{
		MenuItem* Owner = nullptr;                 // 该层面板对应的 owner（其 SubItems 为该面板内容）
		const std::vector<MenuItem*>* Items = nullptr;
		float X = 0;
		float Y = 0;
		float W = 0;
		float H = 0;
		bool OpenedToLeft = false;               // 相对上一层面板是否向左展开
	};

	float EaseOutCubic(float t)
	{
		t = (std::clamp)(t, 0.0f, 1.0f);
		const float inv = 1.0f - t;
		return 1.0f - inv * inv * inv;
	}

	D2D1_COLOR_F FadeColor(D2D1_COLOR_F color, float alpha)
	{
		color.a *= (std::clamp)(alpha, 0.0f, 1.0f);
		return color;
	}

	D2D1_COLOR_F BoostAlpha(D2D1_COLOR_F color, float factor)
	{
		color.a = (std::clamp)(color.a * factor, 0.0f, 1.0f);
		return color;
	}
}

UIClass MenuItem::Type() { return UIClass::UI_MenuItem; }

MenuItem::MenuItem(std::wstring text, int id)
{
	this->Text = text;
	this->Id = id;
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
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	this->BeginRender();
	{
		bool hover = (this->ParentForm->UnderMouse == this);
		bool active = this->Checked;
		if (active || hover)
		{
			const float insetX = 3.0f;
			const float insetY = 3.0f;
			auto itemRect = D2D1::RectF(insetX, insetY,
				(std::max)(insetX, actualWidth - insetX),
				(std::max)(insetY, actualHeight - insetY));
			const auto stateColor = active ? ActiveBackColor : HoverBackColor;
			d2d->FillRoundRect(itemRect, stateColor, CornerRadius);
			d2d->DrawRoundRect(itemRect, BoostAlpha(stateColor, 1.9f), 1.0f, CornerRadius);
			const float stripeH = (std::max)(0.0f, itemRect.bottom - itemRect.top - 8.0f);
			if (stripeH > 0.0f)
				d2d->FillRoundRect(itemRect.left + 3.0f, itemRect.top + 4.0f, 3.0f, stripeH, BoostAlpha(stateColor, 3.0f), 1.5f);
		}

		auto font = this->Font;
		auto ts = font->GetTextSize(this->Text);
		float tx = 10.0f;
		float ty = ((float)this->Height - ts.height) * 0.5f;
		if (ty < 0) ty = 0;
		d2d->DrawString(this->Text, tx, ty, this->ForeColor, font);
	}
	this->EndRender();
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
	return item;
}

bool Menu::ContainsPoint(int xof, int yof)
{
	if (xof >= 0 && xof <= this->Width && yof >= 0 && yof < BarHeight)
		return true;

	if (!_expand || DropCount() <= 0)
		return false;
	if (_expandIndex < 0 || _expandIndex >= this->Count)
		return false;

	auto* top = (MenuItem*)this->operator[](_expandIndex);
	if (!top)
		return false;

	auto calcPanelWidth = [&](const std::vector<MenuItem*>& items) -> float
		{
			float w = 120.0f;
			auto font = this->Font;
			for (auto* it : items)
			{
				if (!it || it->Separator) continue;
				auto ts = font->GetTextSize(it->Text);
				float tw = ts.width + 24.0f;
				if (!it->Shortcut.empty())
				{
					auto ss = font->GetTextSize(it->Shortcut);
					tw += ss.width + 20.0f;
				}
				if (!it->SubItems.empty())
					tw += 18.0f;
				if (tw > w) w = tw;
			}
			if (w < 80.0f) w = 80.0f;
			return w;
		};

	auto clampPanelXY = [&](float& x, float& y, float w, float h)
		{
			if (!this->ParentForm) return;
			float maxX = (float)this->ParentForm->ClientSize.cx;
			float maxY = (float)this->ParentForm->ClientSize.cy;
			if (x < 0.0f) x = 0.0f;
			if (y < 0.0f) y = 0.0f;
			if (x + w > maxX) x = std::max(0.0f, maxX - w);
			if (y + h > maxY) y = std::max(0.0f, maxY - h);
		};

	std::vector<MenuPanel> panels;
	panels.reserve(8);
	MenuPanel root;
	root.Owner = top;
	root.Items = &top->SubItems;
	root.X = DropLeftLocal();
	root.Y = DropTopLocal();
	root.W = DropWidthLocal();
	root.H = DropPaddingY * 2.0f + (float)root.Items->size() * (float)DropItemHeight;
	clampPanelXY(root.X, root.Y, root.W, root.H);
	panels.push_back(root);

	for (size_t level = 0; level < _openPath.size(); level++)
	{
		int openIdx = _openPath[level];
		if (openIdx < 0) break;
		const auto& prev = panels.back();
		if (!prev.Items) break;
		if (openIdx >= (int)prev.Items->size()) break;
		auto* owner = (*prev.Items)[openIdx];
		if (!owner || owner->Separator || owner->SubItems.empty()) break;

		MenuPanel panel;
		panel.Owner = owner;
		panel.Items = &owner->SubItems;
		panel.W = calcPanelWidth(*panel.Items);
		panel.H = DropPaddingY * 2.0f + (float)panel.Items->size() * (float)DropItemHeight;
		panel.X = prev.X + prev.W - 1.0f;
		panel.Y = prev.Y + DropPaddingY + (float)openIdx * (float)DropItemHeight;

		if (this->ParentForm)
		{
			float maxX = (float)this->ParentForm->ClientSize.cx;
			if (panel.X + panel.W > maxX)
			{
				panel.X = prev.X - panel.W - 4.0f;
				panel.OpenedToLeft = true;
			}
			if (panel.X < 0.0f) panel.X = 0.0f;
		}
		clampPanelXY(panel.X, panel.Y, panel.W, panel.H);
		panels.push_back(panel);
		if (panels.size() > 32) break;
	}

	for (const auto& panel : panels)
	{
		if (xof >= panel.X && xof <= panel.X + panel.W && yof >= panel.Y && yof <= panel.Y + panel.H)
			return true;
	}

	return false;
}

void Menu::ClosePopup()
{
	if (!_expand)
		return;

	_expand = false;
	_expandIndex = -1;
	_popupAnimating = false;
	_popupProgress = 0.0f;
	_hoverPath.clear();
	_openPath.clear();
	if (this->ParentForm && this->ParentForm->Selected)
	{
		for (Control* selected = this->ParentForm->Selected; selected; selected = selected->Parent)
		{
			if (selected == this)
			{
				this->ParentForm->SetSelectedControl(NULL, false);
				break;
			}
		}
	}
	if (this->ParentForm)
		this->ParentForm->Invalidate(true);
	else
		this->PostRender();
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
	return (float)top->ActualLocation.x;
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

float Menu::CurrentPopupProgress()
{
	if (!_expand)
	{
		_popupAnimating = false;
		_popupProgress = 0.0f;
		return _popupProgress;
	}

	if (!_popupAnimating)
	{
		_popupProgress = 1.0f;
		return _popupProgress;
	}

	const ULONGLONG now = ::GetTickCount64();
	const ULONGLONG elapsed = now >= _popupAnimStartTick ? (now - _popupAnimStartTick) : 0;
	float t = PopupAnimationDurationMs > 0 ? (float)elapsed / (float)PopupAnimationDurationMs : 1.0f;
	if (t >= 1.0f)
	{
		_popupProgress = _popupTargetProgress;
		_popupAnimating = false;
		return _popupProgress;
	}

	t = EaseOutCubic(t);
	_popupProgress = _popupStartProgress + (_popupTargetProgress - _popupStartProgress) * t;
	return _popupProgress;
}

void Menu::BeginPopupReveal(float startProgress)
{
	_popupStartProgress = (std::clamp)(startProgress, 0.0f, 1.0f);
	_popupTargetProgress = 1.0f;
	_popupProgress = _popupStartProgress;
	_popupAnimStartTick = ::GetTickCount64();
	_popupAnimating = PopupAnimationDurationMs > 0 && _popupStartProgress < _popupTargetProgress;
	if (!_popupAnimating)
		_popupProgress = _popupTargetProgress;
}

bool Menu::IsAnimationRunning()
{
	CurrentPopupProgress();
	return _popupAnimating;
}

bool Menu::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!_popupAnimating)
		return false;
	outRect = this->AbsRect;
	return true;
}

SIZE Menu::ActualSize()
{
	auto s = this->Size;
	if (_expand)
	{
		if (this->ParentForm)
		{
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
	if (this->ParentForm)
	{
		this->ParentForm->MainMenu = this;
	}
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	this->BeginRender((float)size.cx, (float)size.cy);
	{
		d2d->FillRect(0, 0, (float)this->Width, (float)BarHeight, BarBackColor);
		if (Boder > 0.0f && BarBorderColor.a > 0.0f)
			d2d->DrawLine(0.0f, (float)BarHeight - 0.5f, (float)this->Width, (float)BarHeight - 0.5f, BarBorderColor, Boder);

		float x = 6.0f;
		auto font = this->Font;
		for (int i = 0; i < this->Count; i++)
		{
			auto* it = (MenuItem*)this->operator[](i);
			if (!it) continue;
			// Menu 是复合控件：顶层 MenuItem 始终跟随当前 Menu 字体，避免保留旧字体指针。
			it->SetFontEx(font, false);
			auto ts = font->GetTextSize(it->Text);
			int w = (int)(ts.width + ItemPaddingX * 2.0f);
			if (w < 50) w = 50;
			it->SetRuntimeLocation(POINT{ (int)x, 0 });
			it->Size = SIZE{ w, BarHeight };
			x += (float)w;
			it->ForeColor = this->_forecolor;
			it->HoverBackColor = this->BarItemHoverColor;
			it->ActiveBackColor = this->BarItemActiveColor;
			it->CornerRadius = this->BarItemCornerRadius;
			it->Checked = (_expand && i == _expandIndex);
			it->Update();
		}

		if (_expand && DropCount() > 0)
		{
			auto* top = (MenuItem*)this->operator[](_expandIndex);
			if (top)
			{
				const float popupProgress = CurrentPopupProgress();
				auto calcPanelWidth = [&](const std::vector<MenuItem*>& items, float maxW) -> float
					{
						float w = 120.0f;
						for (auto* it : items)
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
							// 预留子菜单指示符空间
							if (!it->SubItems.empty())
								tw += 18.0f;
							if (tw > w) w = tw;
						}
						if (w < 80.0f) w = 80.0f;
						if (maxW > 0.0f && w > maxW) w = maxW;
						return w;
					};

				auto clampPanelXY = [&](float& x, float& y, float w, float h)
					{
						if (!this->ParentForm) return;
						float maxX = (float)this->ParentForm->ClientSize.cx;
						float maxY = (float)this->ParentForm->ClientSize.cy;
						if (x < 0.0f) x = 0.0f;
						if (y < 0.0f) y = 0.0f;
						if (x + w > maxX) x = std::max(0.0f, maxX - w);
						if (y + h > maxY) y = std::max(0.0f, maxY - h);
					};

				// build panels based on open path
				std::vector<MenuPanel> panels;
				panels.reserve(8);

				MenuPanel p0;
				p0.Owner = top;
				p0.Items = &top->SubItems;
				p0.X = DropLeftLocal();
				p0.Y = DropTopLocal();
				{
					float maxw = (float)this->Width - DropLeftLocal();
					p0.W = calcPanelWidth(*p0.Items, maxw);
					p0.H = DropPaddingY * 2.0f + (float)p0.Items->size() * (float)DropItemHeight;
					clampPanelXY(p0.X, p0.Y, p0.W, p0.H);
				}
				panels.push_back(p0);

				for (size_t level = 0; level < _openPath.size(); level++)
				{
					int openIdx = _openPath[level];
					if (openIdx < 0) break;
					const auto& prev = panels.back();
					if (!prev.Items) break;
					if (openIdx >= (int)prev.Items->size()) break;
					auto* owner = (*prev.Items)[openIdx];
					if (!owner || owner->Separator || owner->SubItems.empty()) break;

					MenuPanel p;
					p.Owner = owner;
					p.Items = &owner->SubItems;
					p.W = calcPanelWidth(*p.Items, 0.0f);
					p.H = DropPaddingY * 2.0f + (float)p.Items->size() * (float)DropItemHeight;
					p.X = prev.X + prev.W - 1.0f;
					p.Y = prev.Y + DropPaddingY + (float)openIdx * (float)DropItemHeight;

					if (this->ParentForm)
					{
						float maxX = (float)this->ParentForm->ClientSize.cx;
						if (p.X + p.W > maxX)
						{
							p.X = prev.X - p.W - 4.0f;
							p.OpenedToLeft = true;
						}
						if (p.X < 0.0f) p.X = 0.0f;
					}
					clampPanelXY(p.X, p.Y, p.W, p.H);
					panels.push_back(p);
					if (panels.size() > 32) break;
				}

				for (size_t level = 0; level < panels.size(); level++)
				{
					const auto& pn = panels[level];
					if (!pn.Items) continue;
					const float panelProgress = level == 0 ? popupProgress : 1.0f;
					if (panelProgress <= 0.001f) continue;
					const float revealH = (std::max)(1.0f, pn.H * panelProgress);
					const float alpha = level == 0 ? (0.28f + 0.72f * panelProgress) : 1.0f;
					d2d->PushDrawRect(pn.X, pn.Y, pn.W, revealH);
					d2d->FillRoundRect(pn.X, pn.Y, pn.W, pn.H, FadeColor(DropBackColor, alpha), DropCornerRadius);
					d2d->DrawRoundRect(pn.X, pn.Y, pn.W, pn.H, FadeColor(DropBorderColor, alpha), 1.0f, DropCornerRadius);

					int hoverIdx = (level < _hoverPath.size() ? _hoverPath[level] : -1);
					int openIdx = (level < _openPath.size() ? _openPath[level] : -1);
					for (int i = 0; i < (int)pn.Items->size(); i++)
					{
						auto* it = (*pn.Items)[i];
						float iy = pn.Y + DropPaddingY + (float)i * (float)DropItemHeight;
						if (it && it->Separator)
						{
							float y = iy + (float)DropItemHeight * 0.5f;
							d2d->DrawLine(pn.X + 12.0f, y, pn.X + pn.W - 12.0f, y, FadeColor(DropSeparatorColor, alpha), 1.0f);
							continue;
						}
						if (i == hoverIdx || i == openIdx)
						{
							const float inset = (std::max)(0.0f, this->DropItemHorizontalInset);
							auto itemRect = D2D1::RectF(pn.X + inset, iy + 2.0f, pn.X + pn.W - inset, iy + (float)DropItemHeight - 2.0f);
							const auto hoverColor = FadeColor(DropHoverColor, alpha);
							d2d->FillRoundRect(itemRect, hoverColor, DropItemCornerRadius);
							d2d->DrawRoundRect(itemRect, BoostAlpha(hoverColor, i == openIdx ? 2.1f : 1.7f), 1.0f, DropItemCornerRadius);
							const float stripeH = (std::max)(0.0f, itemRect.bottom - itemRect.top - 8.0f);
							if (stripeH > 0.0f)
								d2d->FillRoundRect(itemRect.left + 4.0f, itemRect.top + 4.0f, 3.0f, stripeH, BoostAlpha(hoverColor, 3.0f), 1.5f);
						}

						if (!it) continue;
						auto ts = font->GetTextSize(it->Text);
						float ty = iy + ((float)DropItemHeight - ts.height) * 0.5f;
						if (ty < iy) ty = iy;
						d2d->DrawString(it->Text, pn.X + 14.0f, ty, FadeColor(DropTextColor, alpha), font);
						const float arrowReserve = !it->SubItems.empty() ? 18.0f : 0.0f;
						if (!it->Shortcut.empty())
						{
							auto ss = font->GetTextSize(it->Shortcut);
							float sx = pn.X + pn.W - 14.0f - arrowReserve - ss.width;
							d2d->DrawString(it->Shortcut, sx, ty, FadeColor(DropTextColor, alpha), font);
						}

						if (!it->SubItems.empty())
						{
							std::wstring arrow = L"\u203A";
							if (i == openIdx && (level + 1) < panels.size() && panels[level + 1].OpenedToLeft)
								arrow = L"\u2039";
							auto as = font->GetTextSize(arrow);
							float ax = pn.X + pn.W - 14.0f - as.width;
							d2d->DrawString(arrow, ax, ty, FadeColor(DropTextColor, alpha), font);
						}
					}
					d2d->PopDrawRect();
				}
			}
		}
	}
	this->EndRender();
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
			auto loc = it->ActualLocation;
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
				_hoverPath.clear();
				_openPath.clear();
				BeginPopupReveal(0.42f);
				this->PostRender();
			}
		}
		else if (message == WM_LBUTTONUP)
		{
			if (_hoverTopIndex >= 0)
			{
				if (_expand && _expandIndex == _hoverTopIndex)
				{
					ClosePopup();
				}
				else
				{
					_expand = true;
					_expandIndex = _hoverTopIndex;
					_hoverPath.clear();
					_openPath.clear();
					BeginPopupReveal(0.08f);
				}
				if (this->ParentForm) this->ParentForm->Invalidate(true);
				else this->PostRender();
			}
		}

		return Control::ProcessMessage(message, wParam, lParam, xof, yof);
	}

	// dropdown interactions (支持任意层级)
	if (_expand && DropCount() > 0)
	{
		auto* top = (MenuItem*)this->operator[](_expandIndex);
		if (top)
		{
			auto calcPanelWidth = [&](const std::vector<MenuItem*>& items) -> float
				{
					float w = 120.0f;
					auto font = this->Font;
					for (auto* it : items)
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
						if (!it->SubItems.empty())
							tw += 18.0f;
						if (tw > w) w = tw;
					}
					if (w < 80.0f) w = 80.0f;
					return w;
				};

			auto clampPanelXY = [&](float& x, float& y, float w, float h)
				{
					if (!this->ParentForm) return;
					float maxX = (float)this->ParentForm->ClientSize.cx;
					float maxY = (float)this->ParentForm->ClientSize.cy;
					if (x < 0.0f) x = 0.0f;
					if (y < 0.0f) y = 0.0f;
					if (x + w > maxX) x = std::max(0.0f, maxX - w);
					if (y + h > maxY) y = std::max(0.0f, maxY - h);
				};

			// build panels (local coords)
			std::vector<MenuPanel> panels;
			panels.reserve(8);
			MenuPanel p0;
			p0.Owner = top;
			p0.Items = &top->SubItems;
			p0.X = DropLeftLocal();
			p0.Y = DropTopLocal();
			p0.W = DropWidthLocal();
			p0.H = DropPaddingY * 2.0f + (float)p0.Items->size() * (float)DropItemHeight;
			clampPanelXY(p0.X, p0.Y, p0.W, p0.H);
			panels.push_back(p0);

			for (size_t level = 0; level < _openPath.size(); level++)
			{
				int openIdx = _openPath[level];
				if (openIdx < 0) break;
				const auto& prev = panels.back();
				if (!prev.Items) break;
				if (openIdx >= (int)prev.Items->size()) break;
				auto* owner = (*prev.Items)[openIdx];
				if (!owner || owner->Separator || owner->SubItems.empty()) break;
				MenuPanel p;
				p.Owner = owner;
				p.Items = &owner->SubItems;
				p.W = calcPanelWidth(*p.Items);
				p.H = DropPaddingY * 2.0f + (float)p.Items->size() * (float)DropItemHeight;
				p.X = prev.X + prev.W - 1.0f;
				p.Y = prev.Y + DropPaddingY + (float)openIdx * (float)DropItemHeight;

				if (this->ParentForm)
				{
					float maxX = (float)this->ParentForm->ClientSize.cx;
					if (p.X + p.W > maxX)
					{
						p.X = prev.X - p.W - 4.0f;
						p.OpenedToLeft = true;
					}
					if (p.X < 0.0f) p.X = 0.0f;
				}
				clampPanelXY(p.X, p.Y, p.W, p.H);
				panels.push_back(p);
				if (panels.size() > 32) break;
			}

			auto pointInRect = [&](float x, float y, const MenuPanel& pn) -> bool
				{
					return (x >= pn.X && x <= pn.X + pn.W && y >= pn.Y && y <= pn.Y + pn.H);
				};

			int hitLevel = -1;
			for (int i = (int)panels.size() - 1; i >= 0; i--)
			{
				if (pointInRect((float)xof, (float)yof, panels[i]))
				{
					hitLevel = i;
					break;
				}
			}

			bool inBridge = false;
			for (size_t i = 0; i + 1 < panels.size(); i++)
			{
				const auto& a = panels[i];
				const auto& b = panels[i + 1];
				float bridgeL = std::min(a.X + a.W - 2.0f, b.X + 2.0f);
				float bridgeR = std::max(a.X + a.W - 2.0f, b.X + 2.0f);
				float bridgeT = b.Y;
				float bridgeB = b.Y + b.H;
				if ((float)xof >= bridgeL && (float)xof <= bridgeR && (float)yof >= bridgeT && (float)yof <= bridgeB)
				{
					inBridge = true;
					break;
				}
			}

			auto ensureSize = [](std::vector<int>& v, size_t n)
				{
					if (v.size() < n) v.resize(n, -1);
				};

			auto itemHasSubMenu = [](MenuItem* it) -> bool
				{
					return it && !it->Separator && !it->SubItems.empty();
				};

			if (message == WM_MOUSEMOVE)
			{
				if (hitLevel >= 0)
				{
					const auto& pn = panels[hitLevel];
					int idx = (int)(((float)yof - (pn.Y + DropPaddingY)) / (float)DropItemHeight);
					int cnt = pn.Items ? (int)pn.Items->size() : 0;
					if (idx < 0 || idx >= cnt) idx = -1;
					bool need = false;

					ensureSize(_hoverPath, (size_t)hitLevel + 1);
					ensureSize(_openPath, (size_t)hitLevel + 1);
					// 清理更深层状态（鼠标在更浅层活动时）
					if (_hoverPath.size() > (size_t)hitLevel + 1)
						_hoverPath.resize((size_t)hitLevel + 1, -1);
					if (_openPath.size() > (size_t)hitLevel + 1)
						_openPath.resize((size_t)hitLevel + 1, -1);

					if (_hoverPath[hitLevel] != idx)
					{
						_hoverPath[hitLevel] = idx;
						need = true;
					}

					int newOpen = -1;
					MenuItem* hovered = nullptr;
					if (idx >= 0 && pn.Items && idx < (int)pn.Items->size())
						hovered = (*pn.Items)[idx];
					if (itemHasSubMenu(hovered))
						newOpen = idx;

					if (_openPath[hitLevel] != newOpen)
					{
						_openPath[hitLevel] = newOpen;
						need = true;
					}
					if (need) this->PostRender();
				}
				else if (inBridge)
				{
					// 桥接区：不断开即可
				}
				else
				{
					// 离开所有面板：清空 hover/open（保持展开）
					if (!_hoverPath.empty() || !_openPath.empty())
					{
						_hoverPath.clear();
						_openPath.clear();
						this->PostRender();
					}
				}
			}
			else if (message == WM_LBUTTONUP)
			{
				if (hitLevel >= 0)
				{
					const auto& pn = panels[hitLevel];
					int idx = (int)(((float)yof - (pn.Y + DropPaddingY)) / (float)DropItemHeight);
					int cnt = pn.Items ? (int)pn.Items->size() : 0;
					if (idx >= 0 && idx < cnt)
					{
						auto* it = (*pn.Items)[idx];
						if (it && !it->Separator)
						{
							// 点击有子菜单项：展开下一层但不触发命令
							if (!it->SubItems.empty())
							{
								ensureSize(_hoverPath, (size_t)hitLevel + 1);
								ensureSize(_openPath, (size_t)hitLevel + 1);
								_hoverPath.resize((size_t)hitLevel + 1, -1);
								_openPath.resize((size_t)hitLevel + 1, -1);
								_hoverPath[hitLevel] = idx;
								_openPath[hitLevel] = idx;
								this->PostRender();
								return Control::ProcessMessage(message, wParam, lParam, xof, yof);
							}
							// 叶子项：触发命令并收起
							if (it->Id != 0)
								this->OnMenuCommand(this, it->Id);
							ClosePopup();
							return Control::ProcessMessage(message, wParam, lParam, xof, yof);
						}
					}
					// 点击分隔符：不处理
				}
				else
				{
					// 点击到下拉外区域：只收起
					ClosePopup();
					return Control::ProcessMessage(message, wParam, lParam, xof, yof);
				}
			}
		}
	}
	// 展开时点击菜单栏/下拉之外：收起（配合 ActualSize 覆盖内容区）
	else if (_expand && message == WM_LBUTTONUP)
	{
		ClosePopup();
	}

	return Control::ProcessMessage(message, wParam, lParam, xof, yof);
}

