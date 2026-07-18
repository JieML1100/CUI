#include "ContextMenu.h"
#include "Form.h"
#include <algorithm>
#include <cmath>

namespace
{
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

	D2D1_SIZE_F LogicalPopupExtent(Form* form)
	{
		if (!form) return D2D1::SizeF(0.0f, 0.0f);
		float scale = form->GetDpiScale();
		if (scale <= 0.0f) scale = 1.0f;
		const float head = form->VisibleHead
			? static_cast<float>(form->HeadHeight) : 0.0f;
		return D2D1::SizeF(
			(static_cast<float>(form->ClientSize.cx) / scale),
			((std::max)(0.0f,
				static_cast<float>(form->ClientSize.cy) - head) / scale));
	}
}

UIClass ContextMenu::Type() { return UIClass::UI_ContextMenu; }

ContextMenu::ContextMenu()
{
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->BorderColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
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
		float tw = ts.width + ItemPaddingX * 2.0f + 42.0f;
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
		float maxW = LogicalPopupExtent(this->ParentForm).width - 8.0f;
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
			const auto extent = LogicalPopupExtent(this->ParentForm);
			float maxX = extent.width;
			float maxY = extent.height;
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
			float maxX = LogicalPopupExtent(this->ParentForm).width;
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

float ContextMenu::CurrentPopupProgress()
{
	if (!_popupVisible)
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
	const UINT duration = EffectiveAnimationDuration(PopupAnimationDurationMs);
	float t = duration > 0 ? (float)elapsed / (float)duration : 1.0f;
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

void ContextMenu::BeginPopupReveal(float startProgress)
{
	_popupStartProgress = (std::clamp)(startProgress, 0.0f, 1.0f);
	_popupTargetProgress = 1.0f;
	_popupProgress = _popupStartProgress;
	_popupAnimStartTick = ::GetTickCount64();
	_popupAnimating = EffectiveAnimationDuration(PopupAnimationDurationMs) > 0
		&& _popupStartProgress < _popupTargetProgress;
	if (!_popupAnimating)
		_popupProgress = _popupTargetProgress;
}

bool ContextMenu::IsAnimationRunning()
{
	CurrentPopupProgress();
	return _popupAnimating;
}

bool ContextMenu::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	if (!_popupAnimating)
		return false;
	outRect = this->AbsRect;
	return true;
}

SIZE ContextMenu::ActualSize()
{
	if (!this->ParentForm)
		return SIZE{ 0, 0 };
	return this->ParentForm->ClientSize;
}

bool ContextMenu::ContainsPoint(int localX, int localY)
{
	if (!_popupVisible)
		return false;
	auto panels = BuildPanels();
	for (const auto& pn : panels)
	{
		if (localX >= pn.X && localX <= pn.X + pn.W && localY >= pn.Y && localY <= pn.Y + pn.H)
			return true;
	}
	return false;
}

void ContextMenu::Update()
{
	if (!this->IsVisual || !_popupVisible || !this->ParentForm || _items.empty())
		return;

	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	auto font = this->Font;
	auto panels = BuildPanels();
	const float popupProgress = CurrentPopupProgress();

	this->BeginRender(size.width, size.height);
	{
		for (size_t level = 0; level < panels.size(); level++)
		{
			const auto& pn = panels[level];
			if (!pn.Items) continue;
			const float panelProgress = level == 0 ? popupProgress : 1.0f;
			if (panelProgress <= 0.001f) continue;
			const float revealH = (std::max)(1.0f, pn.H * panelProgress);
			const float alpha = level == 0 ? (0.28f + 0.72f * panelProgress) : 1.0f;
			d2d->PushDrawRect(pn.X, pn.Y, pn.W, revealH);
			d2d->FillRoundRect(pn.X, pn.Y, pn.W, pn.H, FadeColor(PopupBackColor, alpha), PopupCornerRadius);
			d2d->DrawRoundRect(pn.X, pn.Y, pn.W, pn.H, FadeColor(PopupBorderColor, alpha), Border, PopupCornerRadius);

			int hoverIdx = (level < _hoverPath.size() ? _hoverPath[level] : -1);
			int openIdx = (level < _openPath.size() ? _openPath[level] : -1);
			for (int i = 0; i < (int)pn.Items->size(); i++)
			{
				auto* it = (*pn.Items)[i];
				float iy = pn.Y + DropPaddingY + (float)i * (float)ItemHeight;
				if (it && it->Separator)
				{
					float y = iy + (float)ItemHeight * 0.5f;
					d2d->DrawLine(pn.X + 12.0f, y, pn.X + pn.W - 12.0f, y, FadeColor(PopupSeparatorColor, alpha), 1.0f);
					continue;
				}
				if (i == hoverIdx || i == openIdx)
				{
					const float inset = (std::max)(0.0f, ItemHorizontalInset);
					auto itemRect = D2D1::RectF(pn.X + inset, iy + 2.0f, pn.X + pn.W - inset, iy + (float)ItemHeight - 2.0f);
					const auto hoverColor = FadeColor(PopupHoverColor, alpha);
					d2d->FillRoundRect(itemRect, hoverColor, ItemCornerRadius);
					d2d->DrawRoundRect(itemRect, BoostAlpha(hoverColor, i == openIdx ? 2.1f : 1.7f), 1.0f, ItemCornerRadius);
					const float stripeH = (std::max)(0.0f, itemRect.bottom - itemRect.top - 8.0f);
					if (stripeH > 0.0f)
						d2d->FillRoundRect(itemRect.left + 4.0f, itemRect.top + 4.0f, 3.0f, stripeH, BoostAlpha(hoverColor, 3.0f), 1.5f);
				}
				if (!it) continue;

				auto ts = font->GetTextSize(it->Text);
				float ty = iy + ((float)ItemHeight - ts.height) * 0.5f;
				if (ty < iy) ty = iy;
				const float checkSlot = 18.0f;
				if (it->Checked)
				{
					auto checkSize = font->GetTextSize(L"\u2713");
					const float checkY = iy
						+ ((float)ItemHeight - checkSize.height) * 0.5f;
					d2d->DrawString(
						L"\u2713", pn.X + ItemPaddingX, checkY,
						FadeColor(PopupTextColor, alpha), font);
				}
				d2d->DrawString(
					it->Text, pn.X + ItemPaddingX + checkSlot, ty,
					FadeColor(PopupTextColor, alpha), font);
				const float arrowReserve = !it->SubItems.empty() ? 18.0f : 0.0f;
				if (!it->Shortcut.empty())
				{
					auto ss = font->GetTextSize(it->Shortcut);
					float sx = pn.X + pn.W - 14.0f - arrowReserve - ss.width;
					d2d->DrawString(it->Shortcut, sx, ty, FadeColor(PopupTextColor, alpha), font);
				}
				if (!it->SubItems.empty())
				{
					std::wstring arrow = L"\u203A";
					if (i == openIdx && (level + 1) < panels.size() && panels[level + 1].OpenedToLeft)
						arrow = L"\u2039";
					auto as = font->GetTextSize(arrow);
					float ax = pn.X + pn.W - 14.0f - as.width;
					d2d->DrawString(arrow, ax, ty, FadeColor(PopupTextColor, alpha), font);
				}
			}
			d2d->PopDrawRect();
		}
	}
	this->EndRender();
}

bool ContextMenu::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!this->Enable || !this->Visible || !_popupVisible)
		return true;

	if (_ignoreNextMouseUp && (message == WM_LBUTTONDOWN || message == WM_RBUTTONDOWN || message == WM_MBUTTONDOWN))
	{
		_ignoreNextMouseUp = false;
	}

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
		if (pointInRect((float)localX, (float)localY, panels[i]))
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
		if ((float)localX >= bridgeL && (float)localX <= bridgeR && (float)localY >= bridgeT && (float)localY <= bridgeB)
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
			int itemIndex = (int)(((float)localY - (pn.Y + DropPaddingY)) / (float)ItemHeight);
			int itemCount = pn.Items ? (int)pn.Items->size() : 0;
			if (itemIndex < 0 || itemIndex >= itemCount) itemIndex = -1;
			bool needsUpdate = false;

			ensureSize(_hoverPath, (size_t)hitLevel + 1);
			ensureSize(_openPath, (size_t)hitLevel + 1);
			if (_hoverPath.size() > (size_t)hitLevel + 1)
				_hoverPath.resize((size_t)hitLevel + 1, -1);
			if (_openPath.size() > (size_t)hitLevel + 1)
				_openPath.resize((size_t)hitLevel + 1, -1);

			if (_hoverPath[hitLevel] != itemIndex)
			{
				_hoverPath[hitLevel] = itemIndex;
				needsUpdate = true;
			}

			int newOpen = -1;
			MenuItem* hovered = nullptr;
			if (itemIndex >= 0 && pn.Items && itemIndex < (int)pn.Items->size())
				hovered = (*pn.Items)[itemIndex];
			if (hovered && !hovered->Separator && !hovered->SubItems.empty())
				newOpen = itemIndex;

			if (_openPath[hitLevel] != newOpen)
			{
				_openPath[hitLevel] = newOpen;
				needsUpdate = true;
			}
			if (needsUpdate) this->ParentForm->Invalidate(false);
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
			int itemIndex = (int)(((float)localY - (pn.Y + DropPaddingY)) / (float)ItemHeight);
			int itemCount = pn.Items ? (int)pn.Items->size() : 0;
			if (itemIndex >= 0 && itemIndex < itemCount)
			{
				auto* item = (*pn.Items)[itemIndex];
				if (item && !item->Separator)
				{
					if (!item->SubItems.empty())
					{
						ensureSize(_openPath, (size_t)hitLevel + 1);
						_openPath[hitLevel] = itemIndex;
						this->ParentForm->Invalidate(false);
					}
					else
					{
						OnMenuCommand(this, item->Id);
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
	return AddItem(std::make_unique<MenuItem>(std::move(text), id));
}

MenuItem* ContextMenu::AddItem(std::unique_ptr<MenuItem> item)
{
	return InsertItem(static_cast<int>(_items.size()), std::move(item));
}

MenuItem* ContextMenu::InsertItem(
	int index, std::unique_ptr<MenuItem> item)
{
	if (index < 0 || index > static_cast<int>(_items.size())
		|| !item || item->Parent || item->ParentItem())
		return nullptr;
	if (_popupVisible) Hide();
	auto* result = item.get();
	result->SetStructureChangedHandler([this]()
		{
			ClearHoverState();
			if (_popupVisible) Hide();
			else InvalidateVisual();
		});
	_items.insert(_items.begin() + index, result);
	item.release();
	InvalidateVisual();
	return result;
}

MenuItem* ContextMenu::AddSeparator()
{
	return AddItem(
		std::unique_ptr<MenuItem>(MenuItem::CreateSeparator()));
}

MenuItem* ContextMenu::GetItem(int index) const noexcept
{
	if (index < 0 || static_cast<size_t>(index) >= _items.size())
		return nullptr;
	return _items[static_cast<size_t>(index)];
}

int ContextMenu::IndexOfItem(const MenuItem* item) const noexcept
{
	if (!item) return -1;
	auto found = std::find(_items.begin(), _items.end(), item);
	return found == _items.end()
		? -1 : static_cast<int>(found - _items.begin());
}

static MenuItem* FindContextMenuItemById(
	const std::vector<MenuItem*>& items, int id, bool recursive) noexcept
{
	for (auto* item : items)
	{
		if (!item) continue;
		if (item->Id == id) return item;
		if (recursive)
		{
			auto* found = FindContextMenuItemById(
				item->SubItems, id, true);
			if (found) return found;
		}
	}
	return nullptr;
}

static MenuItem* FindContextMenuItemByText(
	const std::vector<MenuItem*>& items,
	const std::wstring& text, bool recursive) noexcept
{
	for (auto* item : items)
	{
		if (!item) continue;
		if (item->Text == text) return item;
		if (recursive)
		{
			auto* found = FindContextMenuItemByText(
				item->SubItems, text, true);
			if (found) return found;
		}
	}
	return nullptr;
}

MenuItem* ContextMenu::FindItemById(
	int id, bool recursive) const noexcept
{
	return FindContextMenuItemById(_items, id, recursive);
}

MenuItem* ContextMenu::FindItemByText(
	const std::wstring& text, bool recursive) const noexcept
{
	return FindContextMenuItemByText(_items, text, recursive);
}

std::unique_ptr<MenuItem> ContextMenu::DetachItemAt(int index)
{
	if (index < 0 || static_cast<size_t>(index) >= _items.size())
		return {};
	if (_popupVisible) Hide();
	auto* item = _items[static_cast<size_t>(index)];
	_items.erase(_items.begin() + index);
	if (item) item->SetStructureChangedHandler({});
	ClearHoverState();
	InvalidateVisual();
	return std::unique_ptr<MenuItem>(item);
}

std::unique_ptr<MenuItem> ContextMenu::DetachItem(MenuItem* item)
{
	if (!item) return {};
	auto* root = item;
	while (root->ParentItem()) root = root->ParentItem();
	if (IndexOfItem(root) < 0) return {};
	if (auto* parent = item->ParentItem())
		return parent->DetachSubItem(item);
	return DetachItemAt(IndexOfItem(item));
}

bool ContextMenu::RemoveItemAt(int index)
{
	auto item = DetachItemAt(index);
	return item != nullptr;
}

bool ContextMenu::RemoveItem(MenuItem* item)
{
	auto removed = DetachItem(item);
	return removed != nullptr;
}

bool ContextMenu::RemoveItemById(int id, bool recursive)
{
	return RemoveItem(FindItemById(id, recursive));
}

void ContextMenu::ClearItems()
{
	if (_popupVisible) Hide();
	for (auto* item : _items)
	{
		if (item) item->SetStructureChangedHandler({});
		delete item;
	}
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
	BeginPopupReveal(0.08f);
	if (this->ParentForm->ForegroundControl && this->ParentForm->ForegroundControl != this && this->ParentForm->ForegroundControl->AutoCloseOnOutsideClick())
		this->ParentForm->ForegroundControl->ClosePopup();
	if (this->ParentForm->MainMenu && this->ParentForm->MainMenu->AutoCloseOnOutsideClick())
		this->ParentForm->MainMenu->ClosePopup();
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
	const auto relativeAbs = relativeTo->GetAbsoluteLocationDip();
	const auto menuAbs = this->GetAbsoluteLocationDip();
	ShowAt(
		static_cast<int>(std::round(relativeAbs.x - menuAbs.x + (float)x)),
		static_cast<int>(std::round(relativeAbs.y - menuAbs.y + (float)y)),
		ignoreNextMouseUp);
}

void ContextMenu::Hide()
{
	if (!_popupVisible)
		return;
	_popupVisible = false;
	_ignoreNextMouseUp = false;
	_popupAnimating = false;
	_popupProgress = 0.0f;
	ClearHoverState();
	if (this->ParentForm && this->ParentForm->ForegroundControl == this)
		this->ParentForm->ForegroundControl = nullptr;
	if (this->ParentForm)
		this->ParentForm->Invalidate(true);
}
