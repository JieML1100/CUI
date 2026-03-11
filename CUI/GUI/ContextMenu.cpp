#include "ContextMenu.h"
#include "Form.h"
#include <algorithm>

UIClass ContextMenu::Type() { return UIClass::UI_ContextMenu; }

ContextMenu::ContextMenu()
{
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BolderColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->ForeColor = PopupTextColor;
	this->Location = POINT{ 0, 0 };
	this->Size = SIZE{ 0, 0 };
	this->Visible = true;
	this->Cursor = CursorKind::Arrow;
}

ContextMenu::~ContextMenu()
{
	ClearItems();
}

float ContextMenu::CalcPanelWidth(const std::vector<MenuItem*>& items)
{
	float w = 120.0f;
	auto font = this->Font;
	for (auto* it : items)
	{
		if (!it || it->Separator) continue;
		auto ts = font->GetTextSize(it->Text);
		float tw = ts.width + ItemPaddingX * 2.0f + 24.0f;
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
	if (this->ParentForm)
	{
		float maxW = (float)this->ParentForm->ClientSize.cx - 8.0f;
		if (w > maxW) w = maxW;
	}
	return w;
}

std::vector<ContextMenu::PopupPanel> ContextMenu::BuildPanels()
{
	std::vector<PopupPanel> panels;
	if (!_popupVisible || _items.empty())
		return panels;

	auto clampPanelXY = [&](float& x, float& y, float w, float h)
		{
			if (!this->ParentForm) return;
			float maxX = (float)this->ParentForm->ClientSize.cx;
			float maxY = (float)this->ParentForm->ClientSize.cy;
			if (x < 0.0f) x = 0.0f;
			if (y < 0.0f) y = 0.0f;
			if (x + w > maxX) x = (std::max)(0.0f, maxX - w);
			if (y + h > maxY) y = (std::max)(0.0f, maxY - h);
		};

	PopupPanel root;
	root.Items = &_items;
	root.X = (float)_anchor.x;
	root.Y = (float)_anchor.y;
	root.W = CalcPanelWidth(*root.Items);
	root.H = DropPaddingY * 2.0f + (float)root.Items->size() * (float)ItemHeight;
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

		PopupPanel p;
		p.Items = &owner->SubItems;
		p.W = CalcPanelWidth(*p.Items);
		p.H = DropPaddingY * 2.0f + (float)p.Items->size() * (float)ItemHeight;
		p.X = prev.X + prev.W - 1.0f;
		p.Y = prev.Y + DropPaddingY + (float)openIdx * (float)ItemHeight;
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

	return panels;
}

void ContextMenu::ClearHoverState()
{
	_hoverPath.clear();
	_openPath.clear();
}

SIZE ContextMenu::ActualSize()
{
	if (!this->ParentForm)
		return SIZE{ 0, 0 };
	return this->ParentForm->ClientSize;
}

bool ContextMenu::ContainsPoint(int xof, int yof)
{
	if (!_popupVisible)
		return false;
	auto panels = BuildPanels();
	for (const auto& pn : panels)
	{
		if (xof >= pn.X && xof <= pn.X + pn.W && yof >= pn.Y && yof <= pn.Y + pn.H)
			return true;
	}
	return false;
}

void ContextMenu::Update()
{
	if (!this->IsVisual || !_popupVisible || !this->ParentForm || _items.empty())
		return;

	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	auto font = this->Font;
	auto panels = BuildPanels();

	this->BeginRender((float)size.cx, (float)size.cy);
	{
		for (size_t level = 0; level < panels.size(); level++)
		{
			const auto& pn = panels[level];
			if (!pn.Items) continue;
			d2d->FillRoundRect(pn.X, pn.Y, pn.W, pn.H, PopupBackColor, 4.0f);
			d2d->DrawRoundRect(pn.X, pn.Y, pn.W, pn.H, PopupBorderColor, Border, 4.0f);

			int hoverIdx = (level < _hoverPath.size() ? _hoverPath[level] : -1);
			int openIdx = (level < _openPath.size() ? _openPath[level] : -1);
			for (int i = 0; i < (int)pn.Items->size(); i++)
			{
				auto* it = (*pn.Items)[i];
				float iy = pn.Y + DropPaddingY + (float)i * (float)ItemHeight;
				if (it && it->Separator)
				{
					float y = iy + (float)ItemHeight * 0.5f;
					d2d->DrawLine(pn.X + 10.0f, y, pn.X + pn.W - 10.0f, y, PopupSeparatorColor, 1.0f);
					continue;
				}
				if (i == hoverIdx || i == openIdx)
					d2d->FillRect(pn.X + 2.0f, iy, pn.W - 4.0f, (float)ItemHeight, PopupHoverColor);
				if (!it) continue;

				auto ts = font->GetTextSize(it->Text);
				float ty = iy + ((float)ItemHeight - ts.height) * 0.5f;
				if (ty < iy) ty = iy;
				d2d->DrawString(it->Text, pn.X + ItemPaddingX, ty, PopupTextColor, font);
				if (!it->Shortcut.empty())
				{
					auto ss = font->GetTextSize(it->Shortcut);
					float sx = pn.X + pn.W - 12.0f - ss.width;
					d2d->DrawString(it->Shortcut, sx, ty, PopupTextColor, font);
				}
				if (!it->SubItems.empty())
				{
					std::wstring arrow = L">";
					if (i == openIdx && (level + 1) < panels.size() && panels[level + 1].OpenedToLeft)
						arrow = L"<";
					auto as = font->GetTextSize(arrow);
					float ax = pn.X + pn.W - 12.0f - as.width;
					d2d->DrawString(arrow, ax, ty, PopupTextColor, font);
				}
			}
		}
	}
	this->EndRender();
}

bool ContextMenu::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	if (!this->Enable || !this->Visible || !_popupVisible)
		return true;

	if (_ignoreNextMouseUp && (message == WM_LBUTTONUP || message == WM_RBUTTONUP || message == WM_MBUTTONUP))
	{
		_ignoreNextMouseUp = false;
		return true;
	}

	auto panels = BuildPanels();
	auto pointInRect = [&](float x, float y, const PopupPanel& pn) -> bool
		{
			return x >= pn.X && x <= pn.X + pn.W && y >= pn.Y && y <= pn.Y + pn.H;
		};
	auto ensureSize = [](std::vector<int>& values, size_t count)
		{
			if (values.size() < count) values.resize(count, -1);
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
		float bridgeL = (std::min)(a.X + a.W - 2.0f, b.X + 2.0f);
		float bridgeR = (std::max)(a.X + a.W - 2.0f, b.X + 2.0f);
		float bridgeT = b.Y;
		float bridgeB = b.Y + b.H;
		if ((float)xof >= bridgeL && (float)xof <= bridgeR && (float)yof >= bridgeT && (float)yof <= bridgeB)
		{
			inBridge = true;
			break;
		}
	}

	if (message == WM_MOUSEMOVE)
	{
		this->ParentForm->UnderMouse = this;
		if (hitLevel >= 0)
		{
			const auto& pn = panels[hitLevel];
			int idx = (int)(((float)yof - (pn.Y + DropPaddingY)) / (float)ItemHeight);
			int cnt = pn.Items ? (int)pn.Items->size() : 0;
			if (idx < 0 || idx >= cnt) idx = -1;
			bool need = false;

			ensureSize(_hoverPath, (size_t)hitLevel + 1);
			ensureSize(_openPath, (size_t)hitLevel + 1);
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
			if (hovered && !hovered->Separator && !hovered->SubItems.empty())
				newOpen = idx;

			if (_openPath[hitLevel] != newOpen)
			{
				_openPath[hitLevel] = newOpen;
				need = true;
			}
			if (need) this->ParentForm->Invalidate(false);
		}
		else if (!inBridge)
		{
			if (!_hoverPath.empty() || !_openPath.empty())
			{
				ClearHoverState();
				this->ParentForm->Invalidate(false);
			}
		}
		return true;
	}

	if (message == WM_LBUTTONUP || message == WM_RBUTTONUP)
	{
		if (hitLevel >= 0)
		{
			const auto& pn = panels[hitLevel];
			int idx = (int)(((float)yof - (pn.Y + DropPaddingY)) / (float)ItemHeight);
			int cnt = pn.Items ? (int)pn.Items->size() : 0;
			if (idx >= 0 && idx < cnt)
			{
				auto* it = (*pn.Items)[idx];
				if (it && !it->Separator)
				{
					if (!it->SubItems.empty())
					{
						ensureSize(_openPath, (size_t)hitLevel + 1);
						_openPath[hitLevel] = idx;
						this->ParentForm->Invalidate(false);
					}
					else
					{
						OnMenuCommand(this, it->Id);
						Hide();
					}
				}
			}
		}
		return true;
	}

	return true;
}

MenuItem* ContextMenu::AddItem(std::wstring text, int id)
{
	auto* item = new MenuItem(text, id);
	_items.push_back(item);
	return item;
}

MenuItem* ContextMenu::AddSeparator()
{
	auto* item = MenuItem::CreateSeparator();
	_items.push_back(item);
	return item;
}

void ContextMenu::ClearItems()
{
	for (auto* item : _items)
		delete item;
	_items.clear();
	ClearHoverState();
}

void ContextMenu::ShowAt(int x, int y, bool ignoreNextMouseUp)
{
	if (!this->ParentForm || _items.empty())
		return;
	_anchor = POINT{ x, y };
	_popupVisible = true;
	_ignoreNextMouseUp = ignoreNextMouseUp;
	ClearHoverState();
	if (this->ParentForm->ForegroundControl && this->ParentForm->ForegroundControl != this && this->ParentForm->ForegroundControl->AutoCloseOnOutsideClick())
		this->ParentForm->ForegroundControl->ClosePopup();
	this->ParentForm->ForegroundControl = this;
	this->ParentForm->Invalidate(true);
}

void ContextMenu::ShowAt(class Control* relativeTo, int x, int y, bool ignoreNextMouseUp)
{
	if (!relativeTo)
	{
		ShowAt(x, y, ignoreNextMouseUp);
		return;
	}
	auto abs = relativeTo->AbsLocation;
	ShowAt(abs.x + x, abs.y + y, ignoreNextMouseUp);
}

void ContextMenu::Hide()
{
	if (!_popupVisible)
		return;
	_popupVisible = false;
	_ignoreNextMouseUp = false;
	ClearHoverState();
	if (this->ParentForm && this->ParentForm->ForegroundControl == this)
		this->ParentForm->ForegroundControl = NULL;
	if (this->ParentForm)
		this->ParentForm->Invalidate(true);
}