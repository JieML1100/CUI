#include "DesignerCanvas.h"
#include <CppUtils/Utils/json.h>
#include <CppUtils/Utils/Convert.h>
#include "FakeWebBrowser.h"
#include "../CUI_Legacy/GUI/Label.h"
#include "../CUI_Legacy/GUI/LinkLabel.h"
#include "../CUI_Legacy/GUI/Button.h"
#include "../CUI_Legacy/GUI/TextBox.h"
#include "../CUI_Legacy/GUI/CheckBox.h"
#include "../CUI_Legacy/GUI/RadioBox.h"
#include "../CUI_Legacy/GUI/ComboBox.h"
#include "../CUI_Legacy/GUI/ProgressBar.h"
#include "../CUI_Legacy/GUI/Slider.h"
#include "../CUI_Legacy/GUI/PictureBox.h"
#include "../CUI_Legacy/GUI/Switch.h"
#include "../CUI_Legacy/GUI/RichTextBox.h"
#include "../CUI_Legacy/GUI/PasswordBox.h"
#include "../CUI_Legacy/GUI/RoundTextBox.h"
#include "../CUI_Legacy/GUI/GridView.h"
#include "../CUI_Legacy/GUI/TreeView.h"
#include "../CUI_Legacy/GUI/TabControl.h"
#include "../CUI_Legacy/GUI/ToolBar.h"
#include "../CUI_Legacy/GUI/Menu.h"
#include "../CUI_Legacy/GUI/StatusBar.h"
#include "../CUI_Legacy/GUI/MediaPlayer.h"
#include "../CUI_Legacy/GUI/Layout/StackPanel.h"
#include "../CUI_Legacy/GUI/Layout/GridPanel.h"
#include "../CUI_Legacy/GUI/Layout/DockPanel.h"
#include "../CUI_Legacy/GUI/Layout/WrapPanel.h"
#include "../CUI_Legacy/GUI/Layout/RelativePanel.h"
#include "../CUI_Legacy/GUI/Form.h"
#include <windowsx.h>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <fstream>

#ifdef log
#undef log
#endif

using Json = JsonLib::json;

static RECT IntersectRectSafe(const RECT& a, const RECT& b)
{
	RECT r;
	r.left = (std::max)(a.left, b.left);
	r.top = (std::max)(a.top, b.top);
	r.right = (std::min)(a.right, b.right);
	r.bottom = (std::min)(a.bottom, b.bottom);
	if (r.right < r.left) r.right = r.left;
	if (r.bottom < r.top) r.bottom = r.top;
	return r;
}

DesignerCanvas::DesignerCanvas(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	// 初始化窗体字体为框架默认字体
	if (auto* def = GetDefaultFontObject())
	{
		_designedFormFontSize = def->FontSize;
	}
	_designedFormFontName = L"";

	// 画布（外围）与设计面板（内部）区分：设计面板负责裁剪/承载被设计控件
	this->BackColor = Colors::WhiteSmoke;
	this->Boder = 2.0f;

	_designSurface = new Panel(_designSurfaceOrigin.x, _designSurfaceOrigin.y, _designedFormSize.cx, _designedFormSize.cy);
	_designSurface->BackColor = Colors::WhiteSmoke;
	_designSurface->Boder = 0.0f; // 边框由画布统一绘制
	this->AddControl(_designSurface);

	{
		int top = DesignedClientTop();
		int h = _designedFormSize.cy - top;
		if (h < 0) h = 0;
		_clientSurface = new Panel(0, top, _designedFormSize.cx, h);
	}
	_clientSurface->BackColor = _designedFormBackColor;
	_clientSurface->Boder = 0.0f;
	_designSurface->AddControl(_clientSurface);

	// 确保 clientSurface 使用窗体默认字体（初始为默认，不创建共享对象）
	RebuildDesignedFormSharedFont();
}

DesignerCanvas::~DesignerCanvas()
{
	if (_designedFormSharedFont)
	{
		delete _designedFormSharedFont;
		_designedFormSharedFont = nullptr;
	}
	for (auto* f : _retiredDesignedFormSharedFonts)
	{
		delete f;
	}
	_retiredDesignedFormSharedFonts.clear();
}

void DesignerCanvas::SetDesignedFormFontName(const std::wstring& name)
{
	_designedFormFontName = name;
	RebuildDesignedFormSharedFont();
	this->PostRender();
}

void DesignerCanvas::SetDesignedFormFontSize(float size)
{
	if (std::isnan(size) || std::isinf(size)) return;
	if (size < 1.0f) size = 1.0f;
	if (size > 200.0f) size = 200.0f;
	_designedFormFontSize = size;
	RebuildDesignedFormSharedFont();
	this->PostRender();
}

void DesignerCanvas::RebuildDesignedFormSharedFont()
{
	auto rebindFontOf = [](Control* c, ::Font* value) {
		if (!c) return;
		if (value) c->SetFontEx(value, false);
		else c->SetFontEx(nullptr, false);
	};

	auto isDefaultLikeFont = [](const ::Font* cur, const ::Font* oldShared) -> bool {
		if (cur == GetDefaultFontObject()) return true;
		if (oldShared && cur == oldShared) return true;
		return false;
	};

	auto rebindFontsRecursive = [&](Control* root, ::Font* oldShared, ::Font* newShared) {
		if (!root) return;
		std::vector<Control*> stack;
		stack.reserve(128);
		stack.push_back(root);
		while (!stack.empty())
		{
			auto* c = stack.back();
			stack.pop_back();
			if (!c) continue;
			::Font* cur = c->Font;
			if (isDefaultLikeFont(cur, oldShared))
				rebindFontOf(c, newShared);
			for (int i = 0; i < c->Children.Count; i++)
			{
				stack.push_back(c->Children[i]);
			}
		}
	};

	auto isFontUsedRecursive = [&](Control* root, const ::Font* f) -> bool {
		if (!root || !f) return false;
		std::vector<Control*> stack;
		stack.reserve(128);
		stack.push_back(root);
		while (!stack.empty())
		{
			auto* c = stack.back();
			stack.pop_back();
			if (!c) continue;
			if (c->Font == f) return true;
			for (int i = 0; i < c->Children.Count; i++)
				stack.push_back(c->Children[i]);
		}
		return false;
	};

	::Font* oldShared = _designedFormSharedFont;
	_designedFormSharedFont = nullptr;

	auto* def = GetDefaultFontObject();
	std::wstring defName = def ? def->FontName : L"Arial";
	float defSize = def ? def->FontSize : 18.0f;

	std::wstring desiredName = _designedFormFontName.empty() ? defName : _designedFormFontName;
	float desiredSize = _designedFormFontSize;
	if (desiredSize < 1.0f) desiredSize = 1.0f;

	bool needShared = true;
	// 当字体名未显式设置且字号等于框架默认值时，使用框架默认字体（不创建共享对象）
	if (_designedFormFontName.empty() && std::fabs(desiredSize - defSize) < 1e-6f && desiredName == defName)
	{
		needShared = false;
	}

	::Font* newShared = nullptr;
	if (needShared)
	{
		try
		{
			newShared = new ::Font(desiredName, desiredSize);
		}
		catch (...)
		{
			newShared = nullptr;
		}
	}

	_designedFormSharedFont = newShared;

	// 让 clientSurface 也使用窗体字体（不拥有）
	if (_clientSurface)
	{
		rebindFontOf(_clientSurface, newShared);
	}

	// 将“默认字体”的控件绑定到新的共享字体；显式字体不受影响。
	// 注意：仅遍历 _designerControls 可能遗漏复合控件内部对象；这里额外递归遍历设计面板子树。
	for (auto& dc : _designerControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		auto* c = dc->ControlInstance;
		::Font* cur = c->Font;
		if (!isDefaultLikeFont(cur, oldShared)) continue;
		rebindFontOf(c, newShared);
	}
	rebindFontsRecursive(_designSurface ? (Control*)_designSurface : (Control*)_clientSurface, oldShared, newShared);

	// 释放策略（设计器安全优先）：
	// 字体对象可能被某些复合控件/缓存/延迟渲染路径引用，但不一定挂在 designSurface 子树下。
	// 为避免“修改字号两次”触发 UAF 崩溃，这里不在重建时 delete 旧共享字体，统一留到析构释放。
	if (oldShared)
		_retiredDesignedFormSharedFonts.push_back(oldShared);
}

void DesignerCanvas::Update()
{
	if (this->IsVisual == false) return;
	if (!this->ParentForm) return;

	// TabControl 切页会隐藏旧 TabPage：若当前选中控件变为不可见，需要清除选中，避免残留选框。
	auto isEffectivelyVisible = [&](Control* c) -> bool {
		while (c && c != this)
		{
			if (!c->Visible) return false;
			c = c->Parent;
		}
		return true;
	};
	bool selectionChangedByVisibility = false;
	if (!_selectedControls.empty())
	{
		auto it = std::remove_if(_selectedControls.begin(), _selectedControls.end(),
			[&](const std::shared_ptr<DesignerControl>& dc) {
				if (!dc || !dc->ControlInstance) return true;
				if (!isEffectivelyVisible(dc->ControlInstance))
				{
					dc->IsSelected = false;
					selectionChangedByVisibility = true;
					return true;
				}
				return false;
			});
		_selectedControls.erase(it, _selectedControls.end());
		if (_selectedControl && !IsSelected(_selectedControl))
		{
			_selectedControl = _selectedControls.empty() ? nullptr : _selectedControls.back();
			selectionChangedByVisibility = true;
		}
		if (selectionChangedByVisibility)
		{
			OnControlSelected(_selectedControl);
		}
	}

	auto d2d = this->ParentForm->Render;
	auto abslocation = this->AbsLocation;
	auto size = this->ActualSize();
	auto absRect = this->AbsRect;

	d2d->PushDrawRect(absRect.left, absRect.top, absRect.right - absRect.left, absRect.bottom - absRect.top);
	{
		d2d->FillRect(abslocation.x, abslocation.y, (float)size.cx, (float)size.cy, this->BackColor);
		DrawGrid();
		if (_designSurface)
		{
			auto* oldMainMenu = this->ParentForm->MainMenu;
			auto* oldMainStatusBar = this->ParentForm->MainStatusBar;

			auto isInternalDesignedControl = [&](Control* c) -> bool {
				if (!c || !_designSurface) return false;
				if (c == _designSurface) return true;
				return IsDescendantOf(_designSurface, c);
			};

			this->ParentForm->MainMenu = nullptr;
			this->ParentForm->MainStatusBar = nullptr;
			_designSurface->Update();

			this->ParentForm->MainMenu = (oldMainMenu && !isInternalDesignedControl((Control*)oldMainMenu)) ? oldMainMenu : nullptr;
			this->ParentForm->MainStatusBar = (oldMainStatusBar && !isInternalDesignedControl((Control*)oldMainStatusBar)) ? oldMainStatusBar : nullptr;
		}

		// 绘制“仿真窗体”边框 + 标题栏（不影响控件布局，控件都在 clientSurface 内）
		{
			auto canvasAbs = this->AbsLocation;
			auto formRect = GetDesignSurfaceRectInCanvas();
			float fx = (float)(canvasAbs.x + formRect.left);
			float fy = (float)(canvasAbs.y + formRect.top);
			float fw = (float)(formRect.right - formRect.left);
			float fh = (float)(formRect.bottom - formRect.top);

			// 窗体边框
			d2d->DrawRect(fx, fy, fw, fh, Colors::DimGrey, 1.0f);

			// 标题栏
			int headH = DesignedClientTop();
			if (headH > 0)
			{
				D2D1_COLOR_F headBack = D2D1::ColorF(0.5f, 0.5f, 0.5f, 0.25f);
				d2d->FillRect(fx, fy, fw, (float)headH, headBack);

				// 标题文字
				std::wstring title = _designedFormText.empty() ? L"Form" : _designedFormText;
				float textY = fy + (float)((headH - 14) * 0.5f);
				if (textY < fy) textY = fy;
				float pad = 8.0f;
				float btnW = (float)headH;
				int buttonCount = (_designedFormMinBox ? 1 : 0) + (_designedFormMaxBox ? 1 : 0) + (_designedFormCloseBox ? 1 : 0);
				float rightPad = (float)buttonCount * btnW;
				if (_designedFormCenterTitle)
				{
					// 简化：居中绘制（不做精确测量，按经验偏移）
					::Font* titleFont = _designedFormSharedFont ? _designedFormSharedFont : GetDefaultFontObject();
					if (!titleFont) titleFont = this->Font;
					d2d->DrawString(title, fx + (fw - rightPad) * 0.5f - 30.0f, textY, _designedFormForeColor, titleFont);
				}
				else
				{
					::Font* titleFont = _designedFormSharedFont ? _designedFormSharedFont : GetDefaultFontObject();
					if (!titleFont) titleFont = this->Font;
					d2d->DrawString(title, fx + pad, textY, _designedFormForeColor, titleFont);
				}

				// 右侧标题栏按钮（按 Form 的方式绘制图标）
				float xRight = fx + fw;
				auto drawBtnIcon = [&](bool enabled, int kind)
				{
					if (!enabled) return;
					xRight -= btnW;

					const float left = xRight;
					const float top = fy;
					const float bw = btnW;
					const float bh = (float)headH;
					const float s = (bw < bh) ? bw : bh;
					const float cx = left + bw * 0.5f;
					const float cy = top + bh * 0.5f;

					const float icon = s * 0.42f;
					const float half = icon * 0.5f;
					float stroke = s * 0.08f;
					if (stroke < 1.0f) stroke = 1.0f;

					auto drawMinimize = [&]()
						{
							const float y = cy + half * 0.35f;
							d2d->DrawLine(cx - half, y, cx + half, y, Colors::Black, stroke);
						};
					auto drawMaximize = [&]()
						{
							const float x = cx - half;
							const float y = cy - half;
							d2d->DrawRect(x, y, icon, icon, Colors::Black, stroke);
						};
					auto drawClose = [&]()
						{
							d2d->DrawLine(cx - half, cy - half, cx + half, cy + half, Colors::Black, stroke);
							d2d->DrawLine(cx + half, cy - half, cx - half, cy + half, Colors::Black, stroke);
						};

					switch (kind)
					{
					case 0: // Minimize
						drawMinimize();
						break;
					case 1: // Maximize
						drawMaximize();
						break;
					case 2: // Close
						drawClose();
						break;
					}
				};

				// 顺序与 Form 一致：Close / Max / Min
				drawBtnIcon(_designedFormCloseBox, 2);
				drawBtnIcon(_designedFormMaxBox, 1);
				drawBtnIcon(_designedFormMinBox, 0);
			}
		}
		// 选中边框/手柄/框选矩形：裁剪到设计面板
		{
			auto clip = GetClientSurfaceRectInCanvas();
			RECT canvasRect{ 0,0,this->Width,this->Height };
			auto finalClip = IntersectRectSafe(clip, canvasRect);
			auto canvasAbs = this->AbsLocation;
			d2d->PushDrawRect((float)(canvasAbs.x + finalClip.left), (float)(canvasAbs.y + finalClip.top),
				(float)(finalClip.right - finalClip.left), (float)(finalClip.bottom - finalClip.top));

			// 参考线（拖拽/缩放期间）
			if ((_isDragging || _isResizing) && (!_vGuides.empty() || !_hGuides.empty()))
			{
				float left = (float)(canvasAbs.x + clip.left);
				float top = (float)(canvasAbs.y + clip.top);
				float right = (float)(canvasAbs.x + clip.right);
				float bottom = (float)(canvasAbs.y + clip.bottom);
				auto c = Colors::DodgerBlue;
				c.a = 0.85f;
				for (int xCanvas : _vGuides)
				{
					float x = (float)(canvasAbs.x + xCanvas);
					d2d->DrawLine(x, top, x, bottom, c, 1.0f);
				}
				for (int yCanvas : _hGuides)
				{
					float y = (float)(canvasAbs.y + yCanvas);
					d2d->DrawLine(left, y, right, y, c, 1.0f);
				}
			}

			// 先绘制所有选中的边框
			for (auto& dc : _selectedControls)
			{
				if (!dc || !dc->ControlInstance) continue;
				auto rect = GetControlRectInCanvas(dc->ControlInstance);
				int w = rect.right - rect.left;
				int h = rect.bottom - rect.top;
				float x = (float)(canvasAbs.x + rect.left);
				float y = (float)(canvasAbs.y + rect.top);
				d2d->DrawRect(x, y, (float)w, (float)h, Colors::DodgerBlue, 2.0f);
			}

			// 主选中控件的调整手柄
			if (_selectedControl)
			{
				DrawSelectionHandles(_selectedControl);
			}

			// 框选矩形
			if (_isBoxSelecting)
			{
				auto r = _boxSelectRect;
				int w = r.right - r.left;
				int h = r.bottom - r.top;
				float x = (float)(canvasAbs.x + r.left);
				float y = (float)(canvasAbs.y + r.top);
				D2D1_COLOR_F c = D2D1::ColorF(0.12f, 0.50f, 0.95f, 0.25f);
				d2d->FillRect(x, y, (float)w, (float)h, c);
				d2d->DrawRect(x, y, (float)w, (float)h, Colors::DodgerBlue, 1.0f);
			}

			d2d->PopDrawRect();
		}

		d2d->DrawRect(abslocation.x, abslocation.y, (float)size.cx, (float)size.cy, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(abslocation.x, abslocation.y, (float)size.cx, (float)size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	d2d->PopDrawRect();
}

void DesignerCanvas::ClearSelection()
{
	for (auto& dc : _selectedControls)
	{
		if (dc) dc->IsSelected = false;
	}
	_selectedControls.clear();
	_selectedControl = nullptr;
}

bool DesignerCanvas::IsSelected(const std::shared_ptr<DesignerControl>& dc) const
{
	if (!dc) return false;
	for (auto& s : _selectedControls)
	{
		if (s == dc) return true;
	}
	return false;
}

void DesignerCanvas::SetPrimarySelection(const std::shared_ptr<DesignerControl>& dc, bool fireEvent)
{
	_selectedControl = dc;
	if (fireEvent)
		OnControlSelected(_selectedControl);
}

void DesignerCanvas::AddToSelection(const std::shared_ptr<DesignerControl>& dc, bool setPrimary, bool fireEvent)
{
	if (!dc || !dc->ControlInstance) return;
	if (!IsSelected(dc))
	{
		// 最小约束：多选只允许同一运行时父容器，避免跨容器移动/布局难题
		if (!_selectedControls.empty() && _selectedControl && _selectedControl->ControlInstance)
		{
			auto* p0 = _selectedControl->ControlInstance->Parent;
			auto* p1 = dc->ControlInstance->Parent;
			if (p0 != p1)
				return;
		}
		_selectedControls.push_back(dc);
		dc->IsSelected = true;
	}
	if (setPrimary)
		SetPrimarySelection(dc, fireEvent);
	else if (fireEvent)
		OnControlSelected(_selectedControl);
}

void DesignerCanvas::ToggleSelection(const std::shared_ptr<DesignerControl>& dc, bool fireEvent)
{
	if (!dc || !dc->ControlInstance) return;
	if (!IsSelected(dc))
	{
		AddToSelection(dc, true, fireEvent);
		return;
	}

	// remove
	_selectedControls.erase(
		std::remove_if(_selectedControls.begin(), _selectedControls.end(),
			[&](const std::shared_ptr<DesignerControl>& x) { return x == dc; }),
		_selectedControls.end());
	dc->IsSelected = false;

	if (_selectedControl == dc)
	{
		_selectedControl = _selectedControls.empty() ? nullptr : _selectedControls.back();
	}
	if (fireEvent)
		OnControlSelected(_selectedControl);
}

RECT DesignerCanvas::GetSelectionBoundsInCanvas() const
{
	RECT out{ 0,0,0,0 };
	bool first = true;
	for (auto& dc : _selectedControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		auto r = const_cast<DesignerCanvas*>(this)->GetControlRectInCanvas(dc->ControlInstance);
		if (first) { out = r; first = false; }
		else
		{
			out.left = (std::min)(out.left, r.left);
			out.top = (std::min)(out.top, r.top);
			out.right = (std::max)(out.right, r.right);
			out.bottom = (std::max)(out.bottom, r.bottom);
		}
	}
	return out;
}

void DesignerCanvas::BeginDragFromCurrentSelection(POINT mousePos)
{
	_dragStartItems.clear();
	for (auto& dc : _selectedControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		auto* c = dc->ControlInstance;
		DragStartItem it;
		it.ControlInstance = c;
		it.Parent = c->Parent ? c->Parent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
		it.StartRectInCanvas = GetControlRectInCanvas(c);
		it.StartLocation = c->Location;
		it.StartMargin = c->Margin;
		it.UsesRelativeMargin = (it.Parent && it.Parent->Type() == UIClass::UI_RelativePanel);
		_dragStartItems.push_back(it);
	}
	_isDragging = !_dragStartItems.empty();
	_dragHasMoved = false;
	_dragLiftedToRoot = false;
	_dragStartPoint = mousePos;
	if (_selectedControl && _selectedControl->ControlInstance)
		_dragStartRectInCanvas = GetControlRectInCanvas(_selectedControl->ControlInstance);
}

bool DesignerCanvas::IsLayoutContainer(Control* c) const
{
	if (!c) return false;
	switch (c->Type())
	{
	case UIClass::UI_GridPanel:
	case UIClass::UI_StackPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_ToolBar:
		return true;
	default:
		return false;
	}
}

void DesignerCanvas::LiftSelectedToRootForDrag()
{
	if (_dragLiftedToRoot) return;
	if (!_selectedControl || !_selectedControl->ControlInstance) return;
	if (!_clientSurface) return;

	auto* moving = _selectedControl->ControlInstance;
	auto* parent = moving->Parent;
	if (!parent) return;
	if (parent == _clientSurface) return;
	if (!IsLayoutContainer(parent)) return;

	const auto parentType = parent->Type();
	const bool fromGrid = (parentType == UIClass::UI_GridPanel);
	const bool fromRelative = (parentType == UIClass::UI_RelativePanel);

	// 抬升前先拿到当前视觉矩形，保持“画面不跳”
	RECT r = GetControlRectInCanvas(moving);
	POINT newLocal = CanvasToContainerPoint({ r.left, r.top }, _clientSurface);
	int w = r.right - r.left;
	int h = r.bottom - r.top;
	if (w < 0) w = 0;
	if (h < 0) h = 0;

	// 从布局容器移除，加入根客户区；这样拖动时不再受布局/裁剪限制
	parent->RemoveControl(moving);
	_clientSurface->AddControl(moving);
	moving->Location = newLocal;
	moving->Size = { w, h };
	// 从 GridPanel 抬升到根：避免默认 Stretch 直接把控件“铺满”
	if (fromGrid)
	{
		moving->HAlign = HorizontalAlignment::Left;
		moving->VAlign = VerticalAlignment::Top;
	}
	// 从 RelativePanel 抬升到根：清掉用作定位的 Margin，回到 Location 语义
	if (fromRelative)
	{
		auto m = moving->Margin;
		m.Left = 0.0f;
		m.Top = 0.0f;
		m.Right = 0.0f;
		m.Bottom = 0.0f;
		moving->Margin = m;
	}
	_selectedControl->DesignerParent = nullptr;
	_dragLiftedToRoot = true;

	if (auto* p = dynamic_cast<Panel*>(parent))
	{
		p->InvalidateLayout();
		p->PerformLayout();
	}
}

void DesignerCanvas::ApplyMoveDeltaToSelection(int dx, int dy)
{
	if (_dragStartItems.empty()) return;

	// 多选移动：目前只支持同一父容器（AddToSelection 已约束）
	for (auto& it : _dragStartItems)
	{
		if (!it.ControlInstance) continue;
		RECT newRect = it.StartRectInCanvas;
		newRect.left += dx;
		newRect.right += dx;
		newRect.top += dy;
		newRect.bottom += dy;

		// 根级控件：约束到客户区；容器内控件不做全局 clamp（由容器布局决定）
		if (_clientSurface && it.ControlInstance->Parent == _clientSurface)
		{
			auto bounds = GetClientSurfaceRectInCanvas();
			newRect = ClampRectToBounds(newRect, bounds, true);
		}

		ApplyRectToControl(it.ControlInstance, newRect);
	}
}

Thickness DesignerCanvas::GetPaddingOfContainer(Control* container)
{
	if (!container) return Thickness();
	if (auto* p = dynamic_cast<Panel*>(container))
		return p->Padding;
	return Thickness();
}

void DesignerCanvas::ApplyAnchorStylesKeepingBounds(Control* c, uint8_t newAnchorStyles)
{
	if (!c) return;
	// 以“当前视觉矩形”为准，切换 Anchor 后通过 Margin/Location 换算保持不变
	RECT r = GetControlRectInCanvas(c);
	c->AnchorStyles = newAnchorStyles;
	ApplyRectToControl(c, r);
}

void DesignerCanvas::ApplyRectToControl(Control* c, const RECT& rectInCanvas)
{
	if (!c) return;
	Control* parent = c->Parent ? c->Parent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
	if (!parent) return;

	POINT newLocal = CanvasToContainerPoint({ rectInCanvas.left, rectInCanvas.top }, parent);
	int newW = rectInCanvas.right - rectInCanvas.left;
	int newH = rectInCanvas.bottom - rectInCanvas.top;
	if (newW < 0) newW = 0;
	if (newH < 0) newH = 0;

	// RelativePanel：运行时主要用 Margin 表达定位
	if (parent->Type() == UIClass::UI_RelativePanel)
	{
		auto m = c->Margin;
		m.Left = (float)newLocal.x;
		m.Top = (float)newLocal.y;
		c->Margin = m;
		c->Location = { 0,0 };
		c->Size = { newW, newH };
		if (auto* p = dynamic_cast<Panel*>(parent))
		{
			p->InvalidateLayout();
			p->PerformLayout();
		}
		return;
	}

	// 其他容器：沿用运行时默认 Anchor+Margin 规则。
	// 设计器这里需要在 Right/Bottom 锚定时同步换算 Margin.Right/Bottom，避免运行时贴边/拉伸异常。
	Thickness pad = GetPaddingOfContainer(parent);
	SIZE ps = parent->Size;
	const int innerRight = (int)ps.cx - (int)pad.Right;
	const int innerBottom = (int)ps.cy - (int)pad.Bottom;
	const int x = newLocal.x;
	const int y = newLocal.y;

	int leftDist = x - (int)pad.Left;
	int topDist = y - (int)pad.Top;
	int rightDist = innerRight - (x + newW);
	int bottomDist = innerBottom - (y + newH);
	if (leftDist < 0) leftDist = 0;
	if (topDist < 0) topDist = 0;
	if (rightDist < 0) rightDist = 0;
	if (bottomDist < 0) bottomDist = 0;

	auto m = c->Margin;
	uint8_t a = c->AnchorStyles;

	// 水平：
	if (a & AnchorStyles::Right)
	{
		m.Right = (float)rightDist;
	}
	if (a & AnchorStyles::Left)
	{
		// Left 锚定时：如果当前已经用 Margin.Left 表达，则保持这种语义；否则用 Location 表达
		if (m.Left != 0.0f)
			m.Left = (float)leftDist;
		else
			c->Location = { leftDist, c->Location.y };
	}
	else
	{
		// 没有 Left：运行时会用 loc.x + margin.Left
		c->Location = { (int)std::lround((double)leftDist - (double)m.Left), c->Location.y };
	}

	// 垂直：
	if (a & AnchorStyles::Bottom)
	{
		m.Bottom = (float)bottomDist;
	}
	if (a & AnchorStyles::Top)
	{
		if (m.Top != 0.0f)
			m.Top = (float)topDist;
		else
			c->Location = { c->Location.x, topDist };
	}
	else
	{
		c->Location = { c->Location.x, (int)std::lround((double)topDist - (double)m.Top) };
	}

	// 尺寸：在 Left+Right / Top+Bottom 同时锚定时，运行时会由 margin 推导出 size。
	// 这里仍然写回 Size，主要用于保存/显示；实际布局以运行时计算为准。
	c->Size = { newW, newH };
	c->Margin = m;

	if (auto* p = dynamic_cast<Panel*>(parent))
	{
		p->InvalidateLayout();
		p->PerformLayout();
	}
}

void DesignerCanvas::NotifySelectionChangedThrottled()
{
	// 拖动中频繁重建 PropertyGrid 可能较重，这里做一个简单节流。
	// Designer.cpp 已订阅 OnControlSelected 并调用 PropertyGrid::LoadControl。
	DWORD now = GetTickCount();
	if (now - _lastPropSyncTick < 40) return; // ~25fps
	_lastPropSyncTick = now;
	OnControlSelected(_selectedControl);
}

void DesignerCanvas::DrawGrid()
{
	if (!this->ParentForm) return;
	if (!_clientSurface) return;
	auto d2d = this->ParentForm->Render;
	int gridSize = _gridSize;
	auto canvasAbs = this->AbsLocation;
	auto surfRect = GetClientSurfaceRectInCanvas();
	auto surfAbsLeft = (float)(canvasAbs.x + surfRect.left);
	auto surfAbsTop = (float)(canvasAbs.y + surfRect.top);
	auto surfW = (float)(surfRect.right - surfRect.left);
	auto surfH = (float)(surfRect.bottom - surfRect.top);

	// 裁剪到设计面板
	d2d->PushDrawRect(surfAbsLeft, surfAbsTop, surfW, surfH);
	
	// 绘制浅色网格
	D2D1_COLOR_F gridColor = D2D1::ColorF(0.9f, 0.9f, 0.9f, 1.0f);

	for (int x = 0; x < (surfRect.right - surfRect.left); x += gridSize)
	{
		d2d->DrawLine(surfAbsLeft + x, surfAbsTop, surfAbsLeft + x, surfAbsTop + surfH, gridColor, 0.5f);
	}

	for (int y = 0; y < (surfRect.bottom - surfRect.top); y += gridSize)
	{
		d2d->DrawLine(surfAbsLeft, surfAbsTop + y, surfAbsLeft + surfW, surfAbsTop + y, gridColor, 0.5f);
	}

	d2d->PopDrawRect();
}

void DesignerCanvas::ClearAlignmentGuides()
{
	_vGuides.clear();
	_hGuides.clear();
}

void DesignerCanvas::AddVGuide(int xCanvas)
{
	_vGuides.push_back(xCanvas);
}

void DesignerCanvas::AddHGuide(int yCanvas)
{
	_hGuides.push_back(yCanvas);
}

RECT DesignerCanvas::ApplyMoveSnap(RECT desiredRectInCanvas, Control* referenceParent)
{
	ClearAlignmentGuides();
	if (!_clientSurface) return desiredRectInCanvas;
	if (!_snapToGrid && !_snapToGuides) return desiredRectInCanvas;

	auto surfRect = GetClientSurfaceRectInCanvas();
	int dx = 0;
	int dy = 0;

	auto snapToGrid1 = [&](int value, int origin) {
		if (_gridSize <= 1) return value;
		int rel = value - origin;
		int snapped = origin + (int)std::lround((double)rel / (double)_gridSize) * _gridSize;
		if (std::abs(snapped - value) <= _snapThreshold) return snapped;
		return value;
	};

	if (_snapToGrid)
	{
		int newLeft = snapToGrid1(desiredRectInCanvas.left, surfRect.left);
		int newTop = snapToGrid1(desiredRectInCanvas.top, surfRect.top);
		dx += (newLeft - desiredRectInCanvas.left);
		dy += (newTop - desiredRectInCanvas.top);
	}

	if (_snapToGuides)
	{
		std::vector<int> refX;
		std::vector<int> refY;
		refX.reserve(_designerControls.size() * 3 + 4);
		refY.reserve(_designerControls.size() * 3 + 4);

		// design surface edges/centers
		refX.push_back(surfRect.left);
		refX.push_back(surfRect.right);
		refX.push_back((surfRect.left + surfRect.right) / 2);
		refY.push_back(surfRect.top);
		refY.push_back(surfRect.bottom);
		refY.push_back((surfRect.top + surfRect.bottom) / 2);

		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Type == UIClass::UI_TabPage) continue;
			Control* c = dc->ControlInstance;
			if (referenceParent && c->Parent != referenceParent) continue;
			if (IsSelected(dc)) continue;
			auto r = GetControlRectInCanvas(c);
			refX.push_back(r.left);
			refX.push_back(r.right);
			refX.push_back((r.left + r.right) / 2);
			refY.push_back(r.top);
			refY.push_back(r.bottom);
			refY.push_back((r.top + r.bottom) / 2);
		}

		RECT moved = desiredRectInCanvas;
		moved.left += dx; moved.right += dx;
		moved.top += dy; moved.bottom += dy;

		int candX[3] = { moved.left, moved.right, (moved.left + moved.right) / 2 };
		int candY[3] = { moved.top, moved.bottom, (moved.top + moved.bottom) / 2 };

		int bestDx = 0; int bestAbsX = _snapThreshold + 1; int bestGuideX = INT_MIN;
		for (int rx : refX)
		{
			for (int cx : candX)
			{
				int d = rx - cx;
				int a = std::abs(d);
				if (a <= _snapThreshold && a < bestAbsX)
				{
					bestAbsX = a;
					bestDx = d;
					bestGuideX = rx;
				}
			}
		}

		int bestDy = 0; int bestAbsY = _snapThreshold + 1; int bestGuideY = INT_MIN;
		for (int ry : refY)
		{
			for (int cy : candY)
			{
				int d = ry - cy;
				int a = std::abs(d);
				if (a <= _snapThreshold && a < bestAbsY)
				{
					bestAbsY = a;
					bestDy = d;
					bestGuideY = ry;
				}
			}
		}

		dx += bestDx;
		dy += bestDy;
		if (bestGuideX != INT_MIN) AddVGuide(bestGuideX);
		if (bestGuideY != INT_MIN) AddHGuide(bestGuideY);
	}

	desiredRectInCanvas.left += dx;
	desiredRectInCanvas.right += dx;
	desiredRectInCanvas.top += dy;
	desiredRectInCanvas.bottom += dy;
	return desiredRectInCanvas;
}

RECT DesignerCanvas::ApplyResizeSnap(RECT desiredRectInCanvas, Control* referenceParent, DesignerControl::ResizeHandle handle)
{
	ClearAlignmentGuides();
	if (!_clientSurface) return desiredRectInCanvas;
	if (!_snapToGrid && !_snapToGuides) return desiredRectInCanvas;

	auto surfRect = GetClientSurfaceRectInCanvas();

	auto snapToGridEdge = [&](int value, int origin) {
		if (_gridSize <= 1) return value;
		int rel = value - origin;
		int snapped = origin + (int)std::lround((double)rel / (double)_gridSize) * _gridSize;
		if (std::abs(snapped - value) <= _snapThreshold) return snapped;
		return value;
	};

	auto collectRefX = [&]() {
		std::vector<int> refX;
		refX.reserve(_designerControls.size() * 2 + 2);
		refX.push_back(surfRect.left);
		refX.push_back(surfRect.right);
		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Type == UIClass::UI_TabPage) continue;
			Control* c = dc->ControlInstance;
			if (referenceParent && c->Parent != referenceParent) continue;
			if (IsSelected(dc)) continue;
			auto r = GetControlRectInCanvas(c);
			refX.push_back(r.left);
			refX.push_back(r.right);
		}
		return refX;
	};
	auto collectRefY = [&]() {
		std::vector<int> refY;
		refY.reserve(_designerControls.size() * 2 + 2);
		refY.push_back(surfRect.top);
		refY.push_back(surfRect.bottom);
		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Type == UIClass::UI_TabPage) continue;
			Control* c = dc->ControlInstance;
			if (referenceParent && c->Parent != referenceParent) continue;
			if (IsSelected(dc)) continue;
			auto r = GetControlRectInCanvas(c);
			refY.push_back(r.top);
			refY.push_back(r.bottom);
		}
		return refY;
	};

	auto hasLeft = (handle == DesignerControl::ResizeHandle::Left || handle == DesignerControl::ResizeHandle::TopLeft || handle == DesignerControl::ResizeHandle::BottomLeft);
	auto hasRight = (handle == DesignerControl::ResizeHandle::Right || handle == DesignerControl::ResizeHandle::TopRight || handle == DesignerControl::ResizeHandle::BottomRight);
	auto hasTop = (handle == DesignerControl::ResizeHandle::Top || handle == DesignerControl::ResizeHandle::TopLeft || handle == DesignerControl::ResizeHandle::TopRight);
	auto hasBottom = (handle == DesignerControl::ResizeHandle::Bottom || handle == DesignerControl::ResizeHandle::BottomLeft || handle == DesignerControl::ResizeHandle::BottomRight);

	if (_snapToGrid)
	{
		if (hasLeft) desiredRectInCanvas.left = snapToGridEdge(desiredRectInCanvas.left, surfRect.left);
		if (hasRight) desiredRectInCanvas.right = snapToGridEdge(desiredRectInCanvas.right, surfRect.left);
		if (hasTop) desiredRectInCanvas.top = snapToGridEdge(desiredRectInCanvas.top, surfRect.top);
		if (hasBottom) desiredRectInCanvas.bottom = snapToGridEdge(desiredRectInCanvas.bottom, surfRect.top);
	}

	if (_snapToGuides)
	{
		if (hasLeft || hasRight)
		{
			auto refX = collectRefX();
			int edge = hasLeft ? desiredRectInCanvas.left : desiredRectInCanvas.right;
			int bestDx = 0; int bestAbs = _snapThreshold + 1; int bestGuide = INT_MIN;
			for (int rx : refX)
			{
				int d = rx - edge;
				int a = std::abs(d);
				if (a <= _snapThreshold && a < bestAbs)
				{
					bestAbs = a;
					bestDx = d;
					bestGuide = rx;
				}
			}
			if (bestGuide != INT_MIN)
			{
				if (hasLeft) desiredRectInCanvas.left += bestDx;
				else desiredRectInCanvas.right += bestDx;
				AddVGuide(bestGuide);
			}
		}
		if (hasTop || hasBottom)
		{
			auto refY = collectRefY();
			int edge = hasTop ? desiredRectInCanvas.top : desiredRectInCanvas.bottom;
			int bestDy = 0; int bestAbs = _snapThreshold + 1; int bestGuide = INT_MIN;
			for (int ry : refY)
			{
				int d = ry - edge;
				int a = std::abs(d);
				if (a <= _snapThreshold && a < bestAbs)
				{
					bestAbs = a;
					bestDy = d;
					bestGuide = ry;
				}
			}
			if (bestGuide != INT_MIN)
			{
				if (hasTop) desiredRectInCanvas.top += bestDy;
				else desiredRectInCanvas.bottom += bestDy;
				AddHGuide(bestGuide);
			}
		}
	}

	return desiredRectInCanvas;
}

RECT DesignerCanvas::GetDesignSurfaceRectInCanvas() const
{
	if (!_designSurface) return RECT{ 0,0,0,0 };
	RECT r;
	r.left = _designSurface->Location.x;
	r.top = _designSurface->Location.y;
	r.right = r.left + _designSurface->Size.cx;
	r.bottom = r.top + _designSurface->Size.cy;
	return r;
}

RECT DesignerCanvas::GetClientSurfaceRectInCanvas() const
{
	if (!_designSurface || !_clientSurface)
		return GetDesignSurfaceRectInCanvas();
	auto ds = GetDesignSurfaceRectInCanvas();
	RECT r;
	r.left = ds.left + _clientSurface->Location.x;
	r.top = ds.top + _clientSurface->Location.y;
	r.right = r.left + _clientSurface->Size.cx;
	r.bottom = r.top + _clientSurface->Size.cy;
	return r;
}

void DesignerCanvas::UpdateClientSurfaceLayout()
{
	if (!_designSurface || !_clientSurface) return;
	int top = DesignedClientTop();
	int h = _designSurface->Size.cy - top;
	if (h < 0) h = 0;
	_clientSurface->Location = { 0, top };
	_clientSurface->Size = { _designSurface->Size.cx, h };
	if (auto* p = dynamic_cast<Panel*>(_clientSurface))
	{
		p->InvalidateLayout();
		p->PerformLayout();
	}
}

bool DesignerCanvas::IsPointInDesignSurface(POINT ptCanvas) const
{
	auto r = GetClientSurfaceRectInCanvas();
	return ptCanvas.x >= r.left && ptCanvas.x <= r.right && ptCanvas.y >= r.top && ptCanvas.y <= r.bottom;
}

RECT DesignerCanvas::ClampRectToBounds(RECT r, const RECT& bounds, bool keepSize) const
{
	int w = r.right - r.left;
	int h = r.bottom - r.top;
	if (keepSize)
	{
		if (r.left < bounds.left) { r.left = bounds.left; r.right = r.left + w; }
		if (r.top < bounds.top) { r.top = bounds.top; r.bottom = r.top + h; }
		if (r.right > bounds.right) { r.right = bounds.right; r.left = r.right - w; }
		if (r.bottom > bounds.bottom) { r.bottom = bounds.bottom; r.top = r.bottom - h; }
	}
	else
	{
		if (r.left < bounds.left) r.left = bounds.left;
		if (r.top < bounds.top) r.top = bounds.top;
		if (r.right > bounds.right) r.right = bounds.right;
		if (r.bottom > bounds.bottom) r.bottom = bounds.bottom;
	}
	// 防御：避免反转
	if (r.right < r.left) r.right = r.left;
	if (r.bottom < r.top) r.bottom = r.top;
	return r;
}

bool DesignerCanvas::TryHandleTabHeaderClick(POINT ptCanvas)
{
	// DesignerCanvas 自己处理选中/拖拽，导致 TabControl 无法收到点击切页。
	// 这里在画布层模拟 TabControl 标题栏点击，设置 SelectedIndex。
	TabControl* bestTc = nullptr;
	std::shared_ptr<DesignerControl> bestDc = nullptr;
	int bestArea = INT_MAX;

	for (auto& dc : _designerControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		if (dc->Type != UIClass::UI_TabControl) continue;
		auto* tc = dynamic_cast<TabControl*>(dc->ControlInstance);
		if (!tc) continue;
		auto r = GetControlRectInCanvas(tc);
		if (ptCanvas.x < r.left || ptCanvas.x > r.right || ptCanvas.y < r.top || ptCanvas.y > r.bottom) continue;
		int area = (r.right - r.left) * (r.bottom - r.top);
		if (area < bestArea)
		{
			bestArea = area;
			bestTc = tc;
			bestDc = dc;
		}
	}

	if (!bestTc || !bestDc) return false;

	auto r = GetControlRectInCanvas(bestTc);
	int localX = ptCanvas.x - r.left;
	int localY = ptCanvas.y - r.top;
	if (localY >= bestTc->TitleHeight) return false;
	if (bestTc->Count <= 0) return false;
	if (localX >= bestTc->Count * bestTc->TitleWidth) return false;
	int idx = localX / bestTc->TitleWidth;
	if (idx < 0) idx = 0;
	if (idx >= bestTc->Count) idx = bestTc->Count - 1;

	int oldIdx = bestTc->SelectedIndex;
	bestTc->SelectedIndex = idx;
	for (int i = 0; i < bestTc->Count; i++)
	{
		auto* page = bestTc->operator[](i);
		if (!page) continue;
		page->Visible = (i == bestTc->SelectedIndex);
		if (i == bestTc->SelectedIndex)
		{
			page->Location = POINT{ 0,(int)bestTc->TitleHeight };
			SIZE s = bestTc->Size;
			s.cy = std::max(0L, s.cy - bestTc->TitleHeight);
			page->Size = s;
			if (auto* p = dynamic_cast<Panel*>(page))
			{
				p->InvalidateLayout();
				p->PerformLayout();
			}
		}
	}
	bestTc->PostRender();

	// 切页后：清除之前页上选中的控件，避免选框残留；并把 TabControl 设为当前选中。
	// 即使 idx 未变化，点击标题栏也视为在操作 TabControl。
	ClearSelection();
	AddToSelection(bestDc, true, true);
	return true;
}

void DesignerCanvas::SetDesignedFormSize(SIZE s)
{
	if (s.cx < 50) s.cx = 50;
	if (s.cy < 50) s.cy = 50;
	_designedFormSize = s;
	if (_designSurface)
	{
		_designSurface->Size = s;
		if (auto* p = dynamic_cast<Panel*>(_designSurface))
		{
			p->InvalidateLayout();
			p->PerformLayout();
		}
	}
	UpdateClientSurfaceLayout();
	// 尺寸变化后：尽量把现有控件也约束到设计面板内
	for (auto& dc : _designerControls)
	{
		if (dc && dc->ControlInstance)
			ClampControlToDesignSurface(dc->ControlInstance);
	}
	this->PostRender();
}

void DesignerCanvas::ClampControlToDesignSurface(Control* c)
{
	if (!c) return;
	if (_clientSurface && c->Parent == _clientSurface)
	{
		auto rCanvas = GetControlRectInCanvas(c);
		auto bounds = GetClientSurfaceRectInCanvas();
		RECT clamped = ClampRectToBounds(rCanvas, bounds, true);
		POINT newTopLeftCanvas{ clamped.left, clamped.top };
		POINT newLocal = CanvasToContainerPoint(newTopLeftCanvas, _clientSurface);
		c->Location = newLocal;
		return;
	}
	if (_designSurface && c->Parent == _designSurface)
	{
		auto rCanvas = GetControlRectInCanvas(c);
		auto bounds = GetDesignSurfaceRectInCanvas();
		RECT clamped = ClampRectToBounds(rCanvas, bounds, true);
		POINT newTopLeftCanvas{ clamped.left, clamped.top };
		POINT newLocal = CanvasToContainerPoint(newTopLeftCanvas, _designSurface);
		c->Location = newLocal;
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

	// Alt 点击：优先选择父容器（解决“子控件铺满后容器难选中”）
	if (GetAsyncKeyState(VK_MENU) & 0x8000)
	{
		Control* p = hit->Parent;
		while (p && p != this)
		{
			auto dc = findDesigner(p);
			if (dc) return dc;
			p = p->Parent;
		}
	}

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
	for (auto& s : _selectedControls)
	{
		if (s && s->ControlInstance && isInSubtree(s->ControlInstance))
		{
			selectionRemoved = true;
			break;
		}
	}

	_designerControls.erase(
		std::remove_if(_designerControls.begin(), _designerControls.end(),
			[&](const std::shared_ptr<DesignerControl>& dc) {
				return dc && dc->ControlInstance && isInSubtree(dc->ControlInstance);
			}),
		_designerControls.end());

	if (selectionRemoved)
	{
		ClearSelection();
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
		if (tc->SelectedIndex < 0) tc->SelectedIndex = 0;
		if (tc->SelectedIndex >= tc->Count) tc->SelectedIndex = tc->Count - 1;
		return tc->operator[](tc->SelectedIndex);
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
		// 关键：必须尊重“祖先可见性”。例如 TabControl 未选中页里的容器，控件自身 Visible 可能仍为 true，
		// 但其 TabPage 为隐藏状态，此时应当不参与命中，否则会把当前页控件错误塞进隐藏页容器。
		if (!c->IsVisual || !c->Visible || !c->Enable) continue;
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
	if (!_designSurface || !_clientSurface) return;

	// ToolBar 限制：只允许 Button
	auto movingType = moving->Type();

	auto r = GetControlRectInCanvas(moving);
	POINT center{ (r.left + r.right) / 2, (r.top + r.bottom) / 2 };

	Control* rawContainer = FindBestContainerAtPoint(center, moving);
	Control* container = NormalizeContainerForDrop(rawContainer);
	if (!container) {
		// 落在容器之外：归为根级（客户区），DesignerParent 仍为 nullptr
		POINT newCanvasPos{ r.left, r.top };
		POINT newLocal = CanvasToContainerPoint(newCanvasPos, _clientSurface);
		if (moving->Parent) moving->Parent->RemoveControl(moving);
		_clientSurface->AddControl(moving);
		moving->Location = newLocal;
		_selectedControl->DesignerParent = nullptr;
		ClampControlToDesignSurface(moving);
		this->PostRender();
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
		m.Right = 0.0f;
		m.Bottom = 0.0f;
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

		// Ctrl+A：全选当前容器
		if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000))
		{
			Control* requiredParent = _clientSurface ? (Control*)_clientSurface : (Control*)_designSurface;
			if (_selectedControl && _selectedControl->ControlInstance)
				requiredParent = _selectedControl->ControlInstance->Parent ? _selectedControl->ControlInstance->Parent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
			if (!requiredParent) requiredParent = _clientSurface ? (Control*)_clientSurface : (Control*)_designSurface;

			ClearSelection();
			std::shared_ptr<DesignerControl> first = nullptr;
			for (auto& dc : _designerControls)
			{
				if (!dc || !dc->ControlInstance) continue;
				if (dc->Type == UIClass::UI_TabPage) continue;
				if (dc->ControlInstance->Parent != requiredParent) continue;
				if (!first) { first = dc; AddToSelection(dc, true, false); }
				else AddToSelection(dc, false, false);
			}
			OnControlSelected(_selectedControl);
			this->PostRender();
			return true;
		}

		if (_selectedControls.empty() || !_selectedControl || !_selectedControl->ControlInstance)
		{
			break;
		}

		int step = (GetKeyState(VK_SHIFT) & 0x8000) ? 10 : 1;
		bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		int dx = 0, dy = 0;

		switch (wParam)
		{
		case VK_LEFT:
			dx = -step;
			break;
		case VK_RIGHT:
			dx = step;
			break;
		case VK_UP:
			dy = -step;
			break;
		case VK_DOWN:
			dy = step;
			break;
		default:
			break;
		}
		(void)shift;
		if (dx != 0 || dy != 0)
		{
			BeginDragFromCurrentSelection(_dragStartPoint);
			ApplyMoveDeltaToSelection(dx, dy);
			// 根级控件约束
			for (auto& sdc : _selectedControls)
				if (sdc && sdc->ControlInstance) ClampControlToDesignSurface(sdc->ControlInstance);
			this->PostRender();
			return true;
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

		// 如果有待添加的控件，点击时添加（必须在设计面板内）
		if (_controlToAdd != UIClass::UI_Base)
		{
			if (IsPointInDesignSurface(mousePos))
				AddControlToCanvas(_controlToAdd, mousePos);
			_controlToAdd = UIClass::UI_Base;
			this->Cursor = CursorKind::Arrow;
			return true;
		}

		// 先处理 TabControl 标题栏点击（切页）
		if (TryHandleTabHeaderClick(mousePos))
			return true;

		// 设计器里的 Menu：需要“可交互”但也要选中。
		// 注意：不能让 Menu 抢走 Form::Selected，否则 Delete/方向键等设计器快捷键会失效。
		auto findDesignedMenu = [&]() -> Menu* {
			for (auto& dc : _designerControls)
			{
				if (!dc || !dc->ControlInstance) continue;
				if (dc->Type != UIClass::UI_Menu) continue;
				return dynamic_cast<Menu*>(dc->ControlInstance);
			}
			return nullptr;
		};
		auto findDesignerByControl = [&](Control* c) -> std::shared_ptr<DesignerControl> {
			if (!c) return nullptr;
			for (auto it = _designerControls.rbegin(); it != _designerControls.rend(); ++it)
			{
				auto& dc = *it;
				if (dc && dc->ControlInstance == c)
					return dc;
			}
			return nullptr;
		};

		if (auto* menu = findDesignedMenu())
		{
			auto r = GetControlRectInCanvas(menu);
			if (mousePos.x >= r.left && mousePos.x <= r.right && mousePos.y >= r.top && mousePos.y <= r.bottom)
			{
				auto dc = findDesignerByControl(menu);
				if (dc)
				{
					ClearSelection();
					AddToSelection(dc, true, true);
				}

				// 转发鼠标消息给 Menu 执行展开/点击等交互
				POINT local{ mousePos.x - r.left, mousePos.y - r.top };
				auto* oldSelected = this->ParentForm ? this->ParentForm->Selected : nullptr;
				menu->ProcessMessage(message, wParam, lParam, local.x, local.y);
				// 恢复：让键盘快捷键仍由画布处理
				if (this->ParentForm) this->ParentForm->Selected = this;
				(void)oldSelected;
				return true;
			}
		}
		
		// 检查是否点击主选中手柄（仅单选/主选中可调整大小）
		if (_selectedControl && _selectedControls.size() == 1)
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
		
		bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
		ClearAlignmentGuides();
		// 选中控件
		auto hitControl = HitTestControl(mousePos);
		if (hitControl)
		{
			if (shift)
			{
				ToggleSelection(hitControl, true);
			}
			else
			{
				// 单击：如果点在已选中集合内，则保留多选并切换主选中；否则选中单个
				if (IsSelected(hitControl) && _selectedControls.size() > 1)
				{
					SetPrimarySelection(hitControl, true);
				}
				else
				{
					ClearSelection();
					AddToSelection(hitControl, true, true);
				}
			}
			BeginDragFromCurrentSelection(mousePos);
			return true;
		}
		else
		{
			// 空白处：开始框选
			_boxSelectAddToSelection = shift;
			if (!shift)
			{
				ClearSelection();
				OnControlSelected(nullptr);
			}
			if (IsPointInDesignSurface(mousePos))
			{
				_isBoxSelecting = true;
				_boxSelectStart = mousePos;
				_boxSelectRect = { mousePos.x, mousePos.y, mousePos.x, mousePos.y };
				return true;
			}
		}
		break;
	}
	case WM_MOUSEMOVE:
	{
		// 框选更新
		if (_isBoxSelecting)
		{
			RECT r;
			r.left = (std::min)(_boxSelectStart.x, mousePos.x);
			r.top = (std::min)(_boxSelectStart.y, mousePos.y);
			r.right = (std::max)(_boxSelectStart.x, mousePos.x);
			r.bottom = (std::max)(_boxSelectStart.y, mousePos.y);
			_boxSelectRect = r;
			this->Cursor = CursorKind::Arrow;
			this->PostRender();
			return true;
		}

		// 拖拽控件
		if (_isDragging && !_dragStartItems.empty())
		{
			int rawDx = mousePos.x - _dragStartPoint.x;
			int rawDy = mousePos.y - _dragStartPoint.y;
			if (!_dragHasMoved && (std::abs(rawDx) >= _dragStartThreshold || std::abs(rawDy) >= _dragStartThreshold))
			{
				_dragHasMoved = true;
				// 若原先在布局容器内，先抬升到根设计面，才能拖出容器边界
				if (_selectedControls.size() == 1)
					LiftSelectedToRootForDrag();
			}
			Control* refParent = (_selectedControl && _selectedControl->ControlInstance) ? _selectedControl->ControlInstance->Parent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
			RECT desired = _dragStartRectInCanvas;
			desired.left += rawDx; desired.right += rawDx;
			desired.top += rawDy; desired.bottom += rawDy;
			desired = ApplyMoveSnap(desired, refParent);
			int dx = desired.left - _dragStartRectInCanvas.left;
			int dy = desired.top - _dragStartRectInCanvas.top;
			ApplyMoveDeltaToSelection(dx, dy);
			NotifySelectionChangedThrottled();
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
			if (newRect.right - newRect.left < minSize) newRect.right = newRect.left + minSize;
			if (newRect.bottom - newRect.top < minSize) newRect.bottom = newRect.top + minSize;

			Control* refParent = _selectedControl->ControlInstance->Parent ? _selectedControl->ControlInstance->Parent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
			newRect = ApplyResizeSnap(newRect, refParent, _resizeHandle);

			// 再次最小尺寸限制（吸附后可能破坏）
			if (newRect.right - newRect.left < minSize) newRect.right = newRect.left + minSize;
			if (newRect.bottom - newRect.top < minSize) newRect.bottom = newRect.top + minSize;
			// 约束到客户区（不允许进入标题栏）
			auto bounds = GetClientSurfaceRectInCanvas();
			newRect = ClampRectToBounds(newRect, bounds, false);

			ApplyRectToControl(_selectedControl->ControlInstance, newRect);
			NotifySelectionChangedThrottled();
			
			this->Cursor = GetResizeCursor(_resizeHandle);
			return true;
		}
		
		// 更新鼠标样式（仅单选时显示 resize cursor）
		if (_selectedControl && _selectedControls.size() == 1)
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
		// 框选结束：按矩形选中（限制：同一父容器）
		if (_isBoxSelecting)
		{
			_isBoxSelecting = false;
			RECT sel = _boxSelectRect;
			auto intersects = [](const RECT& a, const RECT& b) {
				RECT r;
				r.left = (std::max)(a.left, b.left);
				r.top = (std::max)(a.top, b.top);
				r.right = (std::min)(a.right, b.right);
				r.bottom = (std::min)(a.bottom, b.bottom);
				return (r.right > r.left) && (r.bottom > r.top);
			};

			std::shared_ptr<DesignerControl> firstPick = nullptr;
			Control* requiredParent = nullptr;
			if (_boxSelectAddToSelection && _selectedControl && _selectedControl->ControlInstance)
				requiredParent = _selectedControl->ControlInstance->Parent;

			bool primarySet = (_selectedControl != nullptr);
			for (auto& dc : _designerControls)
			{
				if (!dc || !dc->ControlInstance) continue;
				if (dc->Type == UIClass::UI_TabPage) continue;
				auto r = GetControlRectInCanvas(dc->ControlInstance);
				if (!intersects(sel, r)) continue;

				if (!requiredParent)
					requiredParent = dc->ControlInstance->Parent;
				if (dc->ControlInstance->Parent != requiredParent) continue;

				if (!firstPick) firstPick = dc;

				// Shift+框选：追加；普通框选：此时已在 LBUTTONDOWN 清空过
				if (!primarySet)
				{
					AddToSelection(dc, true, false);
					primarySet = true;
				}
				else
				{
					AddToSelection(dc, false, false);
				}
			}
			_boxSelectAddToSelection = false;
			OnControlSelected(_selectedControl);
			this->PostRender();
			return true;
		}

		// 拖拽结束：单选时尝试放入容器
		if (_isDragging && _selectedControls.size() == 1 && (_dragHasMoved || _dragLiftedToRoot))
		{
			TryReparentSelectedAfterDrag();
		}
		_isDragging = false;
		_dragHasMoved = false;
		_dragLiftedToRoot = false;
		_dragStartItems.clear();
		_isResizing = false;
		_resizeHandle = DesignerControl::ResizeHandle::None;
		ClearAlignmentGuides();
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
	if (!_designSurface || !_clientSurface) return;
	if (!IsPointInDesignSurface(canvasPos)) return;
	
	// 在点击位置创建控件（左上角对齐，稍微偏移避免手感奇怪）
	int centerX = (int)canvasPos.x - 30;
	int centerY = (int)canvasPos.y - 12;
	
	switch (type)
	{
	case UIClass::UI_Label:
		newControl = new Label(L"标签", centerX, centerY);
		typeName = L"Label";
		break;
	case UIClass::UI_LinkLabel:
		newControl = new LinkLabel(L"链接标签", centerX, centerY);
		typeName = L"LinkLabel";
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
	case UIClass::UI_Menu:
	{
		// Menu 始终为窗体根级控件：放在客户区顶部并拉伸宽度
		int w = _clientSurface ? _clientSurface->Width : 360;
		if (w < 80) w = 80;
		newControl = new Menu(0, 0, w, 28);
		typeName = L"Menu";
		break;
	}
	case UIClass::UI_StatusBar:
	{
		// StatusBar 始终为窗体根级控件：放在客户区底部并拉伸宽度
		int w = _clientSurface ? _clientSurface->Width : 360;
		if (w < 80) w = 80;
		int h = 26;
		int y = _clientSurface ? (_clientSurface->Height - h) : (centerY);
		if (y < 0) y = 0;
		newControl = new StatusBar(0, y, w, h);
		typeName = L"StatusBar";
		break;
	}
	case UIClass::UI_WebBrowser:
		newControl = new FakeWebBrowser(centerX, centerY, 500, 360);
		typeName = L"WebBrowser";
		break;
	case UIClass::UI_MediaPlayer:
		newControl = new MediaPlayer(centerX, centerY, 640, 360);
		typeName = L"MediaPlayer";
		break;
	default:
		return;
	}
	
	if (newControl)
	{
		// Menu/StatusBar 不参与容器命中：强制根级（窗体客户区）
		if (type == UIClass::UI_Menu || type == UIClass::UI_StatusBar)
		{
			_clientSurface->AddControl(newControl);
			std::wstring name = GenerateDefaultControlName(type, typeName);
			auto dc = std::make_shared<DesignerControl>(newControl, name, type, nullptr);
			_designerControls.push_back(dc);
			UpdateDefaultNameCounterFromName(type, name);
			ClearSelection();
			AddToSelection(dc, true, true);
			this->PostRender();
			return;
		}

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
			// 根级：属于窗体客户区
			_clientSurface->AddControl(newControl);
			POINT local = CanvasToContainerPoint({ centerX, centerY }, _clientSurface);
			newControl->Location = local;
			// 约束初始位置到客户区
			ClampControlToDesignSurface(newControl);
		}
		
		std::wstring name = GenerateDefaultControlName(type, typeName);
		
		// 创建设计器控件包装
		auto dc = std::make_shared<DesignerControl>(newControl, name, type, designerParent);
		_designerControls.push_back(dc);
		UpdateDefaultNameCounterFromName(type, name);
		
		// 自动选中新添加的控件
		ClearSelection();
		AddToSelection(dc, true, true);
	}
}

void DesignerCanvas::DeleteSelectedControl()
{
	if (_selectedControls.empty()) return;

	// 复制要删除的实例列表（避免删除过程中修改 _selectedControls）
	std::vector<Control*> toDelete;
	toDelete.reserve(_selectedControls.size());
	for (auto& dc : _selectedControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		// 安全：不允许删除设计面板本身
		if (dc->ControlInstance == _designSurface || dc->ControlInstance == _clientSurface) continue;
		toDelete.push_back(dc->ControlInstance);
	}

	ClearSelection();
	OnControlSelected(nullptr);

	for (auto* inst : toDelete)
	{
		if (!inst) continue;
		// 删除控件前：先移除该子树下所有 DesignerControl，避免悬挂指针
		RemoveDesignerControlsInSubtree(inst);
		if (inst->Parent)
			inst->Parent->RemoveControl(inst);
		DeleteControlRecursive(inst);
	}
}

void DesignerCanvas::ClearCanvas()
{
	if (_clientSurface)
	{
		// 清空客户区内的所有控件（递归释放）
		while (_clientSurface->Count > 0)
		{
			auto c = _clientSurface->operator[](_clientSurface->Count - 1);
			_clientSurface->RemoveControl(c);
			DeleteControlRecursive(c);
		}
	}
	_designerControls.clear();
	_selectedControl = nullptr;
	_controlTypeCounters.clear();
	_designedFormName = L"MainForm";
	_designedFormEventHandlers.clear();
	
	OnControlSelected(nullptr);
}

static bool IsExportableDesignType(UIClass t)
{
	switch (t)
	{
	case UIClass::UI_Label:
	case UIClass::UI_LinkLabel:
	case UIClass::UI_Button:
	case UIClass::UI_TextBox:
	case UIClass::UI_RichTextBox:
	case UIClass::UI_PasswordBox:
	case UIClass::UI_Panel:
	case UIClass::UI_StackPanel:
	case UIClass::UI_GridPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_CheckBox:
	case UIClass::UI_RadioBox:
	case UIClass::UI_ComboBox:
	case UIClass::UI_GridView:
	case UIClass::UI_TreeView:
	case UIClass::UI_ProgressBar:
	case UIClass::UI_Slider:
	case UIClass::UI_PictureBox:
	case UIClass::UI_Switch:
	case UIClass::UI_TabControl:
	case UIClass::UI_TabPage:
	case UIClass::UI_ToolBar:
	case UIClass::UI_Menu:
	case UIClass::UI_StatusBar:
	case UIClass::UI_WebBrowser:
	case UIClass::UI_MediaPlayer:
		return true;
	default:
		return false;
	}
}

static std::wstring ExportTypeName(UIClass t)
{
	switch (t)
	{
	case UIClass::UI_Label: return L"Label";
	case UIClass::UI_LinkLabel: return L"LinkLabel";
	case UIClass::UI_Button: return L"Button";
	case UIClass::UI_TextBox: return L"TextBox";
	case UIClass::UI_RichTextBox: return L"RichTextBox";
	case UIClass::UI_PasswordBox: return L"PasswordBox";
	case UIClass::UI_ComboBox: return L"ComboBox";
	case UIClass::UI_GridView: return L"GridView";
	case UIClass::UI_CheckBox: return L"CheckBox";
	case UIClass::UI_RadioBox: return L"RadioBox";
	case UIClass::UI_ProgressBar: return L"ProgressBar";
	case UIClass::UI_TreeView: return L"TreeView";
	case UIClass::UI_Panel: return L"Panel";
	case UIClass::UI_TabPage: return L"TabPage";
	case UIClass::UI_TabControl: return L"TabControl";
	case UIClass::UI_Switch: return L"Switch";
	case UIClass::UI_Menu: return L"Menu";
	case UIClass::UI_ToolBar: return L"ToolBar";
	case UIClass::UI_StatusBar: return L"StatusBar";
	case UIClass::UI_Slider: return L"Slider";
	case UIClass::UI_WebBrowser: return L"WebBrowser";
	case UIClass::UI_StackPanel: return L"StackPanel";
	case UIClass::UI_GridPanel: return L"GridPanel";
	case UIClass::UI_DockPanel: return L"DockPanel";
	case UIClass::UI_WrapPanel: return L"WrapPanel";
	case UIClass::UI_RelativePanel: return L"RelativePanel";
	case UIClass::UI_PictureBox: return L"PictureBox";
	case UIClass::UI_MediaPlayer: return L"MediaPlayer";
	default: return L"Control";
	}
}

namespace
{
	static bool TryParseNumericSuffix(const std::wstring& name, const std::wstring& prefix, int& outSuffix);
}

std::vector<std::shared_ptr<DesignerControl>> DesignerCanvas::GetAllControlsForExport() const
{
	std::vector<std::shared_ptr<DesignerControl>> out;
	out.reserve(_designerControls.size() + 64);

	std::unordered_map<Control*, std::shared_ptr<DesignerControl>> dcOf;
	dcOf.reserve(_designerControls.size() * 2 + 16);

	std::unordered_set<std::wstring> usedNames;
	usedNames.reserve(_designerControls.size() * 2 + 16);

	for (auto& dc : _designerControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		out.push_back(dc);
		dcOf[dc->ControlInstance] = dc;
		if (!dc->Name.empty()) usedNames.insert(dc->Name);
	}

	std::unordered_map<std::wstring, int> nextSuffixOf;
	nextSuffixOf.reserve(64);

	auto computeMaxSuffix = [&](const std::wstring& base) -> int {
		int maxSuf = 0;
		for (const auto& n : usedNames)
		{
			int suf = 0;
			if (TryParseNumericSuffix(n, base, suf))
				maxSuf = (std::max)(maxSuf, suf);
		}
		return maxSuf;
	};

	auto makeUniqueName = [&](UIClass t) -> std::wstring {
		std::wstring base = ExportTypeName(t);
		if (base.empty()) base = L"Control";
		auto it = nextSuffixOf.find(base);
		if (it == nextSuffixOf.end())
			it = nextSuffixOf.emplace(base, computeMaxSuffix(base)).first;

		for (int guard = 0; guard < 1000000; guard++)
		{
			it->second++;
			std::wstring cand = base + std::to_wstring(it->second);
			if (usedNames.insert(cand).second)
				return cand;
		}

		std::wstring cand = base + L"_auto";
		usedNames.insert(cand);
		return cand;
	};

	auto isInternalSurface = [&](Control* c) -> bool {
		return c == (Control*)_designSurface || c == (Control*)_clientSurface || c == (Control*)this;
	};

	Control* root = _clientSurface ? (Control*)_clientSurface : (_designSurface ? (Control*)_designSurface : (Control*)this);
	if (!root) return out;

	std::function<void(Control*)> walk;
	walk = [&](Control* parent)
	{
		if (!parent) return;
		for (int i = 0; i < parent->Count; i++)
		{
			auto* c = parent->operator[](i);
			if (!c) continue;
			if (isInternalSurface(c)) { walk(c); continue; }

			UIClass t = c->Type();
			if (IsExportableDesignType(t))
			{
				if (dcOf.find(c) == dcOf.end())
				{
					Control* designerParent = nullptr;
					auto* rp = c->Parent;
					if (rp && !isInternalSurface(rp) && rp != root)
						designerParent = rp;
					std::wstring name = makeUniqueName(t);
					auto dc = std::make_shared<DesignerControl>(c, name, t, designerParent);
					out.push_back(dc);
					dcOf[c] = dc;
				}
			}

			walk(c);
		}
	};

	walk(root);
	return out;
}

namespace
{
	static std::wstring TrimWs(const std::wstring& s)
	{
		size_t b = 0;
		while (b < s.size() && iswspace(s[b])) b++;
		size_t e = s.size();
		while (e > b && iswspace(s[e - 1])) e--;
		return s.substr(b, e - b);
	}

	static std::string ToUtf8(const std::wstring& s)
	{
		return Convert::UnicodeToUtf8(s);
	}

	static std::wstring FromUtf8(const std::string& s)
	{
		return Convert::Utf8ToUnicode(s);
	}

	static std::string UIClassToString(UIClass t)
	{
		switch (t)
		{
			case UIClass::UI_Label: return "Label";
			case UIClass::UI_LinkLabel: return "LinkLabel";
			case UIClass::UI_Button: return "Button";
		case UIClass::UI_TextBox: return "TextBox";
		case UIClass::UI_RichTextBox: return "RichTextBox";
		case UIClass::UI_PasswordBox: return "PasswordBox";
		case UIClass::UI_Panel: return "Panel";
		case UIClass::UI_StackPanel: return "StackPanel";
		case UIClass::UI_GridPanel: return "GridPanel";
		case UIClass::UI_DockPanel: return "DockPanel";
		case UIClass::UI_WrapPanel: return "WrapPanel";
		case UIClass::UI_RelativePanel: return "RelativePanel";
		case UIClass::UI_CheckBox: return "CheckBox";
		case UIClass::UI_RadioBox: return "RadioBox";
		case UIClass::UI_ComboBox: return "ComboBox";
		case UIClass::UI_GridView: return "GridView";
		case UIClass::UI_TreeView: return "TreeView";
		case UIClass::UI_ProgressBar: return "ProgressBar";
		case UIClass::UI_Slider: return "Slider";
		case UIClass::UI_PictureBox: return "PictureBox";
		case UIClass::UI_Switch: return "Switch";
		case UIClass::UI_TabControl: return "TabControl";
		case UIClass::UI_ToolBar: return "ToolBar";
		case UIClass::UI_Menu: return "Menu";
		case UIClass::UI_StatusBar: return "StatusBar";
		case UIClass::UI_WebBrowser: return "WebBrowser";
		case UIClass::UI_MediaPlayer: return "MediaPlayer";
		case UIClass::UI_TabPage: return "TabPage";
		default: return "Control";
		}
	}

	static bool TryParseUIClass(const std::string& s, UIClass& out)
	{
		if (s == "Label") { out = UIClass::UI_Label; return true; }
		if (s == "LinkLabel") { out = UIClass::UI_LinkLabel; return true; }
		if (s == "Button") { out = UIClass::UI_Button; return true; }
		if (s == "TextBox") { out = UIClass::UI_TextBox; return true; }
		if (s == "RichTextBox") { out = UIClass::UI_RichTextBox; return true; }
		if (s == "PasswordBox") { out = UIClass::UI_PasswordBox; return true; }
		if (s == "Panel") { out = UIClass::UI_Panel; return true; }
		if (s == "StackPanel") { out = UIClass::UI_StackPanel; return true; }
		if (s == "GridPanel") { out = UIClass::UI_GridPanel; return true; }
		if (s == "DockPanel") { out = UIClass::UI_DockPanel; return true; }
		if (s == "WrapPanel") { out = UIClass::UI_WrapPanel; return true; }
		if (s == "RelativePanel") { out = UIClass::UI_RelativePanel; return true; }
		if (s == "CheckBox") { out = UIClass::UI_CheckBox; return true; }
		if (s == "RadioBox") { out = UIClass::UI_RadioBox; return true; }
		if (s == "ComboBox") { out = UIClass::UI_ComboBox; return true; }
		if (s == "GridView") { out = UIClass::UI_GridView; return true; }
		if (s == "TreeView") { out = UIClass::UI_TreeView; return true; }
		if (s == "ProgressBar") { out = UIClass::UI_ProgressBar; return true; }
		if (s == "Slider") { out = UIClass::UI_Slider; return true; }
		if (s == "PictureBox") { out = UIClass::UI_PictureBox; return true; }
		if (s == "Switch") { out = UIClass::UI_Switch; return true; }
		if (s == "TabControl") { out = UIClass::UI_TabControl; return true; }
		if (s == "ToolBar") { out = UIClass::UI_ToolBar; return true; }
		if (s == "Menu") { out = UIClass::UI_Menu; return true; }
		if (s == "StatusBar") { out = UIClass::UI_StatusBar; return true; }
		if (s == "WebBrowser") { out = UIClass::UI_WebBrowser; return true; }
		if (s == "MediaPlayer") { out = UIClass::UI_MediaPlayer; return true; }
		if (s == "TabPage") { out = UIClass::UI_TabPage; return true; }
		return false;
	}

	static Json MenuItemToJson(MenuItem* it)
	{
		if (!it) return Json();
		Json j;
		j["text"] = ToUtf8(it->Text);
		j["id"] = it->Id;
		j["shortcut"] = ToUtf8(it->Shortcut);
		j["separator"] = it->Separator;
		j["enable"] = it->Enable;
		Json subs = Json::array();
		for (auto* s : it->SubItems)
		{
			if (!s) continue;
			subs.push_back(MenuItemToJson(s));
		}
		j["subItems"] = subs;
		return j;
	}

	static void JsonToMenuSubItems(const Json& arr, std::vector<MenuItem*>& out, MenuItem* owner)
	{
		if (!owner) return;
		if (!arr.is_array()) return;
		for (auto& j : arr)
		{
			if (!j.is_object()) continue;
			bool sep = j.value("separator", false);
			if (sep)
			{
				auto* s = owner->AddSeparator();
				if (!s) continue;
				continue;
			}
			auto text = FromUtf8(j.value("text", std::string()));
			int id = j.value("id", 0);
			auto* s = owner->AddSubItem(text, id);
			if (!s) continue;
			s->Shortcut = FromUtf8(j.value("shortcut", std::string()));
			s->Enable = j.value("enable", true);
			if (j.contains("subItems"))
			{
				JsonToMenuSubItems(j["subItems"], out, s);
			}
		}
	}

	static Json ColorToJson(const D2D1_COLOR_F& c)
	{
		return Json{ {"r", c.r}, {"g", c.g}, {"b", c.b}, {"a", c.a} };
	}
	static D2D1_COLOR_F ColorFromJson(const Json& j, const D2D1_COLOR_F& def)
	{
		D2D1_COLOR_F c = def;
		if (j.is_object())
		{
			c.r = j.value("r", def.r);
			c.g = j.value("g", def.g);
			c.b = j.value("b", def.b);
			c.a = j.value("a", def.a);
		}
		return c;
	}

	static Json ThicknessToJson(const Thickness& t)
	{
		return Json{ {"l", t.Left}, {"t", t.Top}, {"r", t.Right}, {"b", t.Bottom} };
	}
	static Thickness ThicknessFromJson(const Json& j, const Thickness& def)
	{
		Thickness t = def;
		if (j.is_object())
		{
			t.Left = j.value("l", def.Left);
			t.Top = j.value("t", def.Top);
			t.Right = j.value("r", def.Right);
			t.Bottom = j.value("b", def.Bottom);
		}
		return t;
	}

	static std::string HAlignToString(HorizontalAlignment a)
	{
		switch (a)
		{
		case HorizontalAlignment::Left: return "Left";
		case HorizontalAlignment::Center: return "Center";
		case HorizontalAlignment::Right: return "Right";
		case HorizontalAlignment::Stretch: return "Stretch";
		default: return "Left";
		}
	}
	static bool TryParseHAlign(const std::string& s, HorizontalAlignment& out)
	{
		if (s == "Left") { out = HorizontalAlignment::Left; return true; }
		if (s == "Center") { out = HorizontalAlignment::Center; return true; }
		if (s == "Right") { out = HorizontalAlignment::Right; return true; }
		if (s == "Stretch") { out = HorizontalAlignment::Stretch; return true; }
		return false;
	}
	static std::string VAlignToString(VerticalAlignment a)
	{
		switch (a)
		{
		case VerticalAlignment::Top: return "Top";
		case VerticalAlignment::Center: return "Center";
		case VerticalAlignment::Bottom: return "Bottom";
		case VerticalAlignment::Stretch: return "Stretch";
		default: return "Top";
		}
	}
	static bool TryParseVAlign(const std::string& s, VerticalAlignment& out)
	{
		if (s == "Top") { out = VerticalAlignment::Top; return true; }
		if (s == "Center") { out = VerticalAlignment::Center; return true; }
		if (s == "Bottom") { out = VerticalAlignment::Bottom; return true; }
		if (s == "Stretch") { out = VerticalAlignment::Stretch; return true; }
		return false;
	}
	static std::string DockToString(Dock d)
	{
		switch (d)
		{
		case Dock::Left: return "Left";
		case Dock::Top: return "Top";
		case Dock::Right: return "Right";
		case Dock::Bottom: return "Bottom";
		case Dock::Fill: return "Fill";
		default: return "Fill";
		}
	}
	static bool TryParseDock(const std::string& s, Dock& out)
	{
		if (s == "Left") { out = Dock::Left; return true; }
		if (s == "Top") { out = Dock::Top; return true; }
		if (s == "Right") { out = Dock::Right; return true; }
		if (s == "Bottom") { out = Dock::Bottom; return true; }
		if (s == "Fill") { out = Dock::Fill; return true; }
		return false;
	}
	static std::string OrientationToString(Orientation o)
	{
		switch (o)
		{
		case Orientation::Horizontal: return "Horizontal";
		case Orientation::Vertical: return "Vertical";
		default: return "Vertical";
		}
	}
	static bool TryParseOrientation(const std::string& s, Orientation& out)
	{
		if (s == "Horizontal") { out = Orientation::Horizontal; return true; }
		if (s == "Vertical") { out = Orientation::Vertical; return true; }
		return false;
	}

	static std::string SizeUnitToString(SizeUnit u)
	{
		switch (u)
		{
		case SizeUnit::Pixel: return "Pixel";
		case SizeUnit::Percent: return "Percent";
		case SizeUnit::Auto: return "Auto";
		case SizeUnit::Star: return "Star";
		default: return "Pixel";
		}
	}
	static bool TryParseSizeUnit(const std::string& s, SizeUnit& out)
	{
		if (s == "Pixel") { out = SizeUnit::Pixel; return true; }
		if (s == "Percent") { out = SizeUnit::Percent; return true; }
		if (s == "Auto") { out = SizeUnit::Auto; return true; }
		if (s == "Star") { out = SizeUnit::Star; return true; }
		return false;
	}
	static Json GridLengthToJson(const GridLength& gl)
	{
		return Json{ {"value", gl.Value}, {"unit", SizeUnitToString(gl.Unit)} };
	}
	static GridLength GridLengthFromJson(const Json& j, const GridLength& def)
	{
		GridLength gl = def;
		if (!j.is_object()) return gl;
		gl.Value = j.value("value", def.Value);
		SizeUnit u = def.Unit;
		if (j.contains("unit") && j["unit"].is_string())
		{
			TryParseSizeUnit(j["unit"].get<std::string>(), u);
		}
		gl.Unit = u;
		return gl;
	}

	static int GetChildIndex(Control* parent, Control* child)
	{
		if (!parent || !child) return -1;
		for (int i = 0; i < parent->Count; i++)
		{
			if (parent->operator[](i) == child) return i;
		}
		return -1;
	}

	static Json TreeNodesToJson(List<TreeNode*>& nodes)
	{
		Json arr = Json::array();
		for (auto* n : nodes)
		{
			if (!n) continue;
			Json one;
			one["text"] = ToUtf8(n->Text);
			one["expand"] = n->Expand;
			if (n->Children.Count > 0)
				one["children"] = TreeNodesToJson(n->Children);
			arr.push_back(one);
		}
		return arr;
	}

	static void JsonToTreeNodes(const Json& j, List<TreeNode*>& outNodes)
	{
		if (!j.is_array()) return;
		for (auto& it : j)
		{
			if (!it.is_object()) continue;
			auto text = FromUtf8(it.value("text", std::string()));
			auto* node = new TreeNode(text);
			node->Expand = it.value("expand", false);
			if (it.contains("children"))
				JsonToTreeNodes(it["children"], node->Children);
			outNodes.push_back(node);
		}
	}

	static int ParseTrailingIntOrZero(const std::wstring& s)
	{
		int i = (int)s.size() - 1;
		while (i >= 0 && iswdigit(s[(size_t)i])) i--;
		if (i == (int)s.size() - 1) return 0;
		try
		{
			return std::stoi(s.substr((size_t)i + 1));
		}
		catch (...) { return 0; }
	}

	static bool StartsWith(const std::wstring& s, const std::wstring& prefix)
	{
		if (s.size() < prefix.size()) return false;
		return s.compare(0, prefix.size(), prefix) == 0;
	}

	static bool TryParseNumericSuffix(const std::wstring& name, const std::wstring& prefix, int& outSuffix)
	{
		outSuffix = 0;
		if (!StartsWith(name, prefix)) return false;
		std::wstring rest = name.substr(prefix.size());
		if (rest.empty()) return false;
		for (wchar_t ch : rest)
		{
			if (!iswdigit(ch)) return false;
		}
		try
		{
			outSuffix = std::stoi(rest);
			return outSuffix > 0;
		}
		catch (...) { return false; }
	}
}

std::wstring DesignerCanvas::MakeUniqueControlName(const std::shared_ptr<DesignerControl>& target, const std::wstring& desired) const
{
	std::wstring base = TrimWs(desired);
	if (base.empty()) base = L"Control";

	auto isUsed = [&](const std::wstring& n) -> bool
	{
		for (auto& dc : _designerControls)
		{
			if (!dc) continue;
			if (dc == target) continue;
			if (dc->Name == n) return true;
		}
		return false;
	};

	if (!isUsed(base)) return base;

	int suffix = 2;
	while (suffix < 1000000)
	{
		std::wstring candidate = base + std::to_wstring(suffix);
		if (!isUsed(candidate)) return candidate;
		suffix++;
	}
	// 极端情况下兜底：保持可用但不保证美观
	return base + L"_";
}

std::wstring DesignerCanvas::GenerateDefaultControlName(UIClass type, const std::wstring& typeName)
{
	std::wstring base = typeName;
	if (base.empty()) base = L"Control";

	int maxExisting = 0;
	for (auto& dc : _designerControls)
	{
		if (!dc) continue;
		if (dc->Type != type) continue;
		int suf = 0;
		if (TryParseNumericSuffix(dc->Name, base, suf))
			maxExisting = (std::max)(maxExisting, suf);
	}

	int& counter = _controlTypeCounters[(int)type];
	counter = (std::max)(counter, maxExisting);

	auto isUsed = [&](const std::wstring& n) -> bool
	{
		for (auto& dc : _designerControls)
		{
			if (!dc) continue;
			if (dc->Name == n) return true;
		}
		return false;
	};

	for (int guard = 0; guard < 1000000; guard++)
	{
		counter++;
		std::wstring candidate = base + std::to_wstring(counter);
		if (!isUsed(candidate)) return candidate;
	}

	return base + L"_";
}

void DesignerCanvas::UpdateDefaultNameCounterFromName(UIClass type, const std::wstring& name)
{
	std::wstring base = ExportTypeName(type);
	if (base.empty()) base = L"Control";
	int suf = 0;
	if (!TryParseNumericSuffix(name, base, suf)) return;
	int& counter = _controlTypeCounters[(int)type];
	counter = (std::max)(counter, suf);
}

bool DesignerCanvas::SaveDesignFile(const std::wstring& filePath, std::wstring* outError) const
{
	try
	{
		if (filePath.empty())
		{
			if (outError) *outError = L"文件路径为空。";
			return false;
		}

		Json root;
		root["schema"] = "cui.designer";
		root["version"] = 1;
		Json formObj = Json{
			{"name", ToUtf8(_designedFormName)},
			{"text", ToUtf8(_designedFormText)},
			{"font", Json{{"name", ToUtf8(_designedFormFontName)}, {"size", _designedFormFontSize}}},
			{"size", Json{{"w", _designedFormSize.cx}, {"h", _designedFormSize.cy}}},
			{"location", Json{{"x", _designedFormLocation.x}, {"y", _designedFormLocation.y}}},
			{"backColor", Json{{"r", _designedFormBackColor.r}, {"g", _designedFormBackColor.g}, {"b", _designedFormBackColor.b}, {"a", _designedFormBackColor.a}}},
			{"foreColor", Json{{"r", _designedFormForeColor.r}, {"g", _designedFormForeColor.g}, {"b", _designedFormForeColor.b}, {"a", _designedFormForeColor.a}}},
			{"showInTaskBar", _designedFormShowInTaskBar},
			{"topMost", _designedFormTopMost},
			{"enable", _designedFormEnable},
			{"visible", _designedFormVisible},
			{"visibleHead", _designedFormVisibleHead},
			{"headHeight", _designedFormHeadHeight},
			{"minBox", _designedFormMinBox},
			{"maxBox", _designedFormMaxBox},
			{"closeBox", _designedFormCloseBox},
			{"centerTitle", _designedFormCenterTitle},
			{"allowResize", _designedFormAllowResize}
		};
		{
			Json ev = Json::object();
			for (const auto& kv : _designedFormEventHandlers)
			{
				if (kv.first.empty()) continue;
				if (kv.second.empty()) continue;
				ev[ToUtf8(kv.first)] = true;
			}
			if (!ev.empty()) formObj["events"] = ev;
		}
		root["form"] = formObj;

		// 防御：Name 必须唯一，否则 parent 引用会歧义，文件将无法可靠加载
		{
			std::unordered_set<std::wstring> used;
			used.reserve(_designerControls.size());
			for (auto& dc : _designerControls)
			{
				if (!dc) continue;
				if (dc->Name.empty())
				{
					if (outError) *outError = L"存在空的控件 Name，请先为控件命名。";
					return false;
				}
				if (used.find(dc->Name) != used.end())
				{
					if (outError) *outError = L"存在重复的控件 Name: " + dc->Name;
					return false;
				}
				used.insert(dc->Name);
			}
		}

		// Control* -> name
		std::unordered_map<Control*, std::wstring> nameOf;
		nameOf.reserve(_designerControls.size());
		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			nameOf[dc->ControlInstance] = dc->Name;
		}

		// TabPage* -> pageId
		std::unordered_map<Control*, std::string> tabPageIdOf;
		tabPageIdOf.reserve(32);
		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Type != UIClass::UI_TabControl) continue;
			auto* tc = (TabControl*)dc->ControlInstance;
			for (int i = 0; i < tc->Count; i++)
			{
				auto* page = tc->operator[](i);
				if (!page) continue;
				std::wstring wid = dc->Name + L"#page" + std::to_wstring(i);
				tabPageIdOf[page] = ToUtf8(wid);
			}
		}

		Json arr = Json::array();
		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Type == UIClass::UI_TabPage) continue;
			auto* c = dc->ControlInstance;

			Json item;
			item["name"] = ToUtf8(dc->Name);
			item["type"] = UIClassToString(dc->Type);

			// parent reference
			if (!dc->DesignerParent)
			{
				item["parent"] = nullptr;
			}
			else
			{
				auto itName = nameOf.find(dc->DesignerParent);
				if (itName != nameOf.end())
					item["parent"] = ToUtf8(itName->second);
				else
				{
					auto itPage = tabPageIdOf.find(dc->DesignerParent);
					if (itPage != tabPageIdOf.end()) item["parent"] = itPage->second;
					else item["parent"] = nullptr;
				}
			}

			Control* runtimeParent = dc->DesignerParent ? dc->DesignerParent : (_clientSurface ? (Control*)_clientSurface : (Control*)_designSurface);
			item["order"] = GetChildIndex(runtimeParent, c);

			Json props;
			props["text"] = ToUtf8(c->Text);
			props["location"] = Json{ {"x", c->Location.x}, {"y", c->Location.y} };
			props["size"] = Json{ {"w", c->Size.cx}, {"h", c->Size.cy} };
			// 字体：默认（跟随窗体/框架）不保存，显式字体才保存
			{
				::Font* f = c->Font;
				bool inherited = false;
				if (_designedFormSharedFont)
					inherited = (f == _designedFormSharedFont);
				else
					inherited = (f == GetDefaultFontObject());
				if (!inherited && f)
				{
					props["font"] = Json{ {"name", ToUtf8(f->FontName)}, {"size", f->FontSize} };
				}
			}
			props["enable"] = c->Enable;
			props["visible"] = c->Visible;
			props["backColor"] = ColorToJson(c->BackColor);
			props["foreColor"] = ColorToJson(c->ForeColor);
			props["bolderColor"] = ColorToJson(c->BolderColor);
			props["margin"] = ThicknessToJson(c->Margin);
			props["padding"] = ThicknessToJson(c->Padding);
			props["anchor"] = (int)c->AnchorStyles;
			props["hAlign"] = HAlignToString(c->HAlign);
			props["vAlign"] = VAlignToString(c->VAlign);
			props["dock"] = DockToString(c->DockPosition);
			props["gridRow"] = c->GridRow;
			props["gridColumn"] = c->GridColumn;
			props["gridRowSpan"] = c->GridRowSpan;
			props["gridColumnSpan"] = c->GridColumnSpan;
			props["sizeMode"] = (int)c->SizeMode;
			item["props"] = props;

			Json extra;
			if (dc->Type == UIClass::UI_ComboBox)
			{
				auto* cb = (ComboBox*)c;
				Json items = Json::array();
				for (int i = 0; i < cb->Items.Count; i++)
					items.push_back(ToUtf8(cb->Items[i]));
				extra["items"] = items;
				extra["selectedIndex"] = cb->SelectedIndex;
			}
			else if (dc->Type == UIClass::UI_ProgressBar)
			{
				extra["percentageValue"] = ((ProgressBar*)c)->PercentageValue;
			}
			else if (dc->Type == UIClass::UI_Slider)
			{
				auto* s = (Slider*)c;
				extra["min"] = s->Min;
				extra["max"] = s->Max;
				extra["value"] = s->Value;
				extra["step"] = s->Step;
				extra["snapToStep"] = s->SnapToStep;
			}
			else if (dc->Type == UIClass::UI_GridView)
			{
				auto* gv = (GridView*)c;
				Json cols = Json::array();
				for (int i = 0; i < gv->Columns.Count; i++)
				{
					auto& col = gv->Columns[i];
					Json cj;
					cj["name"] = ToUtf8(col.Name);
					cj["width"] = col.Width;
					cj["type"] = (int)col.Type;
					cj["canEdit"] = col.CanEdit;
					cols.push_back(cj);
				}
				extra["columns"] = cols;
			}
			else if (dc->Type == UIClass::UI_TreeView)
			{
				auto* tv = (TreeView*)c;
				if (tv->Root)
					extra["nodes"] = TreeNodesToJson(tv->Root->Children);
				extra["selectedBackColor"] = ColorToJson(tv->SelectedBackColor);
				extra["underMouseItemBackColor"] = ColorToJson(tv->UnderMouseItemBackColor);
				extra["selectedForeColor"] = ColorToJson(tv->SelectedForeColor);
			}
			else if (dc->Type == UIClass::UI_TabControl)
			{
				auto* tc = (TabControl*)c;
				extra["selectedIndex"] = tc->SelectedIndex;
				extra["titleHeight"] = tc->TitleHeight;
				extra["titleWidth"] = tc->TitleWidth;
				Json pages = Json::array();
				for (int i = 0; i < tc->Count; i++)
				{
					auto* page = tc->operator[](i);
					if (!page) continue;
					Json pj;
					std::wstring wid = dc->Name + L"#page" + std::to_wstring(i);
					pj["id"] = ToUtf8(wid);
					pj["text"] = ToUtf8(page->Text);
					pages.push_back(pj);
				}
				extra["pages"] = pages;
			}
			else if (dc->Type == UIClass::UI_ToolBar)
			{
				auto* tb = (ToolBar*)c;
				extra["padding"] = tb->Padding;
				extra["gap"] = tb->Gap;
				extra["itemHeight"] = tb->ItemHeight;
			}
			else if (dc->Type == UIClass::UI_GridPanel)
			{
				auto* gp = (GridPanel*)c;
				Json rows = Json::array();
				for (auto& r : gp->GetRows())
				{
					rows.push_back(Json{
						{"height", GridLengthToJson(r.Height)},
						{"min", r.MinHeight},
						{"max", r.MaxHeight}
					});
				}
				Json cols = Json::array();
				for (auto& col : gp->GetColumns())
				{
					cols.push_back(Json{
						{"width", GridLengthToJson(col.Width)},
						{"min", col.MinWidth},
						{"max", col.MaxWidth}
					});
				}
				extra["rows"] = rows;
				extra["columns"] = cols;
			}
			else if (dc->Type == UIClass::UI_StackPanel)
			{
				auto* sp = (StackPanel*)c;
				extra["orientation"] = OrientationToString(sp->GetOrientation());
				extra["spacing"] = sp->GetSpacing();
			}
			else if (dc->Type == UIClass::UI_WrapPanel)
			{
				auto* wp = (WrapPanel*)c;
				extra["orientation"] = OrientationToString(wp->GetOrientation());
				extra["itemWidth"] = wp->GetItemWidth();
				extra["itemHeight"] = wp->GetItemHeight();
			}
			else if (dc->Type == UIClass::UI_DockPanel)
			{
				auto* dp = (DockPanel*)c;
				extra["lastChildFill"] = dp->GetLastChildFill();
			}
			else if (dc->Type == UIClass::UI_StatusBar)
			{
				auto* sb = (StatusBar*)c;
				extra["topMost"] = sb->TopMost;
				Json parts = Json::array();
				for (int i = 0; i < sb->PartCount(); i++)
				{
					Json pj;
					pj["text"] = ToUtf8(sb->GetPartText(i));
					pj["width"] = sb->GetPartWidth(i);
					parts.push_back(pj);
				}
				extra["parts"] = parts;
			}
			else if (dc->Type == UIClass::UI_Menu)
			{
				auto* m = (Menu*)c;
				Json tops = Json::array();
				for (int i = 0; i < m->Count; i++)
				{
					auto* it = dynamic_cast<MenuItem*>(m->operator[](i));
					if (!it) continue;
					tops.push_back(MenuItemToJson(it));
				}
				extra["items"] = tops;
			}
			else if (dc->Type == UIClass::UI_MediaPlayer)
			{
				auto* mp = (MediaPlayer*)c;
				// 设计期保存：媒体源路径放在 DesignStrings 中，避免加载文件也能保持可往返。
				auto it = dc->DesignStrings.find(L"mediaFile");
				std::wstring mediaFile = (it != dc->DesignStrings.end()) ? it->second : mp->MediaFile;
				if (!mediaFile.empty()) extra["mediaFile"] = ToUtf8(mediaFile);
				extra["autoPlay"] = mp->AutoPlay;
				extra["loop"] = mp->Loop;
				extra["volume"] = mp->Volume;
				extra["playbackRate"] = mp->PlaybackRate;
				extra["renderMode"] = (int)mp->RenderMode;
			}

			if (!extra.empty()) item["extra"] = extra;

			// events: { "OnMouseClick": true, ... }（兼容旧格式：string handlerName）
			if (!dc->EventHandlers.empty())
			{
				Json ev = Json::object();
				for (const auto& kv : dc->EventHandlers)
				{
					if (kv.first.empty()) continue;
					// 现在只保存“是否启用”，handler 名在导出时按规则生成
					ev[ToUtf8(kv.first)] = true;
				}
				if (!ev.empty()) item["events"] = ev;
			}
			arr.push_back(item);
		}

		root["controls"] = arr;
		std::string out = root.dump(2);
		std::ofstream f(filePath, std::ios::binary);
		if (!f.is_open())
		{
			if (outError) *outError = L"无法打开文件写入。";
			return false;
		}
		f.write(out.data(), (std::streamsize)out.size());
		return true;
	}
	catch (const std::exception& ex)
	{
		if (outError) *outError = L"保存失败: " + FromUtf8(ex.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"保存失败：未知错误。";
		return false;
	}
}

bool DesignerCanvas::LoadDesignFile(const std::wstring& filePath, std::wstring* outError)
{
	try
	{
		if (filePath.empty())
		{
			if (outError) *outError = L"文件路径为空。";
			return false;
		}

		std::ifstream f(filePath, std::ios::binary);
		if (!f.is_open())
		{
			if (outError) *outError = L"无法打开文件读取。";
			return false;
		}
		std::stringstream ss;
		ss << f.rdbuf();
		Json root = Json::parse(ss.str(), nullptr, true, true);

		if (root.value("schema", std::string()) != "cui.designer")
		{
			if (outError) *outError = L"不是有效的 CUI Designer 文件（schema 不匹配）。";
			return false;
		}
		int ver = root.value("version", 0);
		if (ver != 1)
		{
			if (outError) *outError = L"不支持的设计文件版本。";
			return false;
		}
		if (!root.contains("controls") || !root["controls"].is_array())
		{
			if (outError) *outError = L"设计文件缺少 controls 数组。";
			return false;
		}

		ClearCanvas();
		_controlTypeCounters.clear();
		_designedFormEventHandlers.clear();

		if (root.contains("form") && root["form"].is_object())
		{
			auto& form = root["form"];
			_designedFormName = FromUtf8(form.value("name", std::string()));
			if (_designedFormName.empty()) _designedFormName = L"MainForm";
			_designedFormText = FromUtf8(form.value("text", std::string()));
			// Font（name 允许为空，表示框架默认字体名）
			if (form.contains("font") && form["font"].is_object())
			{
				auto& fj = form["font"];
				_designedFormFontName = FromUtf8(fj.value("name", std::string()));
				_designedFormFontSize = (float)fj.value("size", (double)_designedFormFontSize);
				if (_designedFormFontSize < 1.0f) _designedFormFontSize = 1.0f;
				if (_designedFormFontSize > 200.0f) _designedFormFontSize = 200.0f;
			}
			else
			{
				_designedFormFontName.clear();
				if (auto* def = GetDefaultFontObject()) _designedFormFontSize = def->FontSize;
			}
			_designedFormShowInTaskBar = form.value("showInTaskBar", _designedFormShowInTaskBar);
			_designedFormTopMost = form.value("topMost", _designedFormTopMost);
			_designedFormEnable = form.value("enable", _designedFormEnable);
			_designedFormVisible = form.value("visible", _designedFormVisible);
			_designedFormVisibleHead = form.value("visibleHead", _designedFormVisibleHead);
			_designedFormHeadHeight = form.value("headHeight", _designedFormHeadHeight);
			if (_designedFormHeadHeight < 0) _designedFormHeadHeight = 0;
			_designedFormMinBox = form.value("minBox", _designedFormMinBox);
			_designedFormMaxBox = form.value("maxBox", _designedFormMaxBox);
			_designedFormCloseBox = form.value("closeBox", _designedFormCloseBox);
			_designedFormCenterTitle = form.value("centerTitle", _designedFormCenterTitle);
			_designedFormAllowResize = form.value("allowResize", _designedFormAllowResize);
			if (form.contains("backColor") && form["backColor"].is_object())
			{
				auto& c = form["backColor"];
				_designedFormBackColor = D2D1::ColorF(
					(float)c.value("r", (double)_designedFormBackColor.r),
					(float)c.value("g", (double)_designedFormBackColor.g),
					(float)c.value("b", (double)_designedFormBackColor.b),
					(float)c.value("a", (double)_designedFormBackColor.a));
			}
			if (form.contains("foreColor") && form["foreColor"].is_object())
			{
				auto& c = form["foreColor"];
				_designedFormForeColor = D2D1::ColorF(
					(float)c.value("r", (double)_designedFormForeColor.r),
					(float)c.value("g", (double)_designedFormForeColor.g),
					(float)c.value("b", (double)_designedFormForeColor.b),
					(float)c.value("a", (double)_designedFormForeColor.a));
			}
			if (_clientSurface) _clientSurface->BackColor = _designedFormBackColor;
			if (form.contains("events") && form["events"].is_object())
			{
				for (auto it = form["events"].begin(); it != form["events"].end(); ++it)
				{
					std::wstring name = FromUtf8(it.key());
					if (name.empty()) continue;
					if (it.value().is_boolean())
					{
						if (it.value().get<bool>()) _designedFormEventHandlers[name] = L"1";
					}
					else if (it.value().is_string())
					{
						auto v = FromUtf8(it.value().get<std::string>());
						if (!v.empty()) _designedFormEventHandlers[name] = v;
						else _designedFormEventHandlers[name] = L"1";
					}
				}
			}
			if (form.contains("size") && form["size"].is_object())
			{
				SIZE s;
				s.cx = form["size"].value("w", 800);
				s.cy = form["size"].value("h", 600);
				SetDesignedFormSize(s);
			}
			if (form.contains("location") && form["location"].is_object())
			{
				_designedFormLocation.x = form["location"].value("x", 100);
				_designedFormLocation.y = form["location"].value("y", 100);
			}
			UpdateClientSurfaceLayout();
			RebuildDesignedFormSharedFont();
		}

		struct Pending
		{
			std::wstring name;
			UIClass type = UIClass::UI_Base;
			Json parent;
			int order = -1;
			Json props;
			Json extra;
			Json events;
		};
		std::vector<Pending> items;
		items.reserve(root["controls"].size());

		std::unordered_set<std::wstring> nameSet;
		for (auto& j : root["controls"])
		{
			if (!j.is_object()) continue;
			Pending p;
			p.name = FromUtf8(j.value("name", std::string()));
			std::string typeStr = j.value("type", std::string());
			UIClass t;
			if (p.name.empty() || !TryParseUIClass(typeStr, t))
			{
				if (outError) *outError = L"控件条目缺少 name/type 或 type 不支持。";
				return false;
			}
			p.type = t;
			if (nameSet.find(p.name) != nameSet.end())
			{
				if (outError) *outError = L"控件 Name 重复: " + p.name;
				return false;
			}
			nameSet.insert(p.name);
			p.parent = j.contains("parent") ? j["parent"] : Json();
			p.order = j.value("order", -1);
			p.props = j.contains("props") ? j["props"] : Json::object();
			p.extra = j.contains("extra") ? j["extra"] : Json::object();
			p.events = j.contains("events") ? j["events"] : Json::object();
			items.push_back(std::move(p));
		}

		auto createControl = [&](UIClass type) -> Control*
		{
			switch (type)
			{
	case UIClass::UI_Label: return new Label(L"标签", 0, 0);
			case UIClass::UI_LinkLabel: return new LinkLabel(L"链接标签", 0, 0);
			case UIClass::UI_Button: return new Button(L"按钮", 0, 0, 120, 30);
			case UIClass::UI_TextBox: return new TextBox(L"", 0, 0, 200, 25);
			case UIClass::UI_RichTextBox: return new RichTextBox(L"", 0, 0, 300, 160);
			case UIClass::UI_PasswordBox: return new PasswordBox(L"", 0, 0, 200, 25);
			case UIClass::UI_Panel: return new Panel(0, 0, 200, 200);
			case UIClass::UI_StackPanel: return new StackPanel(0, 0, 200, 200);
			case UIClass::UI_GridPanel: return new GridPanel(0, 0, 200, 200);
			case UIClass::UI_DockPanel: return new DockPanel(0, 0, 200, 200);
			case UIClass::UI_WrapPanel: return new WrapPanel(0, 0, 200, 200);
			case UIClass::UI_RelativePanel: return new RelativePanel(0, 0, 200, 200);
			case UIClass::UI_CheckBox: return new CheckBox(L"复选框", 0, 0);
			case UIClass::UI_RadioBox: return new RadioBox(L"单选框", 0, 0);
			case UIClass::UI_ComboBox: return new ComboBox(L"", 0, 0, 150, 25);
			case UIClass::UI_GridView: return new GridView(0, 0, 360, 200);
			case UIClass::UI_TreeView: return new TreeView(0, 0, 220, 220);
			case UIClass::UI_ProgressBar: return new ProgressBar(0, 0, 200, 20);
			case UIClass::UI_Slider: return new Slider(0, 0, 200, 30);
			case UIClass::UI_PictureBox: return new PictureBox(0, 0, 150, 150);
			case UIClass::UI_Switch: return new Switch(0, 0, 60, 30);
			case UIClass::UI_TabControl: return new TabControl(0, 0, 360, 240);
			case UIClass::UI_ToolBar: return new ToolBar(0, 0, 360, 34);
			case UIClass::UI_Menu: return new Menu(0, 0, 600, 28);
			case UIClass::UI_StatusBar: return new StatusBar(0, 0, 600, 26);
			case UIClass::UI_WebBrowser: return new FakeWebBrowser(0, 0, 500, 360);
			case UIClass::UI_MediaPlayer: return new MediaPlayer(0, 0, 640, 360);
			default: return nullptr;
			}
		};

		std::unordered_map<std::wstring, std::shared_ptr<DesignerControl>> dcOf;
		dcOf.reserve(items.size());
		std::unordered_map<std::wstring, Control*> instOf;
		instOf.reserve(items.size());

		std::unordered_map<std::wstring, Control*> tabPageOf;
		tabPageOf.reserve(64);

		for (auto& it : items)
		{
			Control* c = createControl(it.type);
			if (!c)
			{
				if (outError) *outError = L"无法创建控件实例: " + it.name;
				return false;
			}
			auto dc = std::make_shared<DesignerControl>(c, it.name, it.type, nullptr);
			dcOf[it.name] = dc;
			instOf[it.name] = c;
			UpdateDefaultNameCounterFromName(it.type, it.name);
		}

		for (auto& it : items)
		{
			auto dcIt = dcOf.find(it.name);
			if (dcIt == dcOf.end()) continue;
			auto dc = dcIt->second;
			auto* c = dc->ControlInstance;
			if (!c) continue;

			if (it.events.is_object())
			{
				dc->EventHandlers.clear();
				for (auto evIt = it.events.begin(); evIt != it.events.end(); ++evIt)
				{
					std::wstring k = FromUtf8(evIt.key());
					if (k.empty()) continue;
					if (evIt.value().is_boolean())
					{
						if (evIt.value().get<bool>())
							dc->EventHandlers[k] = L"1";
					}
					else if (evIt.value().is_string())
					{
						std::wstring v = FromUtf8(evIt.value().get<std::string>());
						if (!v.empty()) dc->EventHandlers[k] = v;
					}
				}
			}

			if (it.props.is_object())
			{
				c->Text = FromUtf8(it.props.value("text", std::string()));
				if (it.props.contains("location"))
				{
					auto& l = it.props["location"];
					if (l.is_object())
						c->Location = { l.value("x", 0), l.value("y", 0) };
				}
				if (it.props.contains("size"))
				{
					auto& s = it.props["size"];
					if (s.is_object())
						c->Size = { s.value("w", c->Size.cx), s.value("h", c->Size.cy) };
				}
				c->Enable = it.props.value("enable", true);
				c->Visible = it.props.value("visible", true);
				c->BackColor = ColorFromJson(it.props.value("backColor", Json()), c->BackColor);
				c->ForeColor = ColorFromJson(it.props.value("foreColor", Json()), c->ForeColor);
				c->BolderColor = ColorFromJson(it.props.value("bolderColor", Json()), c->BolderColor);
				c->Margin = ThicknessFromJson(it.props.value("margin", Json()), c->Margin);
				c->Padding = ThicknessFromJson(it.props.value("padding", Json()), c->Padding);
				c->AnchorStyles = (uint8_t)it.props.value("anchor", (int)c->AnchorStyles);
				HorizontalAlignment ha = c->HAlign;
				VerticalAlignment va = c->VAlign;
				Dock dk = c->DockPosition;
				if (it.props.contains("hAlign") && it.props["hAlign"].is_string())
					TryParseHAlign(it.props["hAlign"].get<std::string>(), ha);
				if (it.props.contains("vAlign") && it.props["vAlign"].is_string())
					TryParseVAlign(it.props["vAlign"].get<std::string>(), va);
				if (it.props.contains("dock") && it.props["dock"].is_string())
					TryParseDock(it.props["dock"].get<std::string>(), dk);
				c->HAlign = ha;
				c->VAlign = va;
				c->DockPosition = dk;
				c->GridRow = it.props.value("gridRow", c->GridRow);
				c->GridColumn = it.props.value("gridColumn", c->GridColumn);
				c->GridRowSpan = it.props.value("gridRowSpan", c->GridRowSpan);
				c->GridColumnSpan = it.props.value("gridColumnSpan", c->GridColumnSpan);
				c->SizeMode = (ImageSizeMode)it.props.value("sizeMode", (int)c->SizeMode);

				// Font：有显式设置则创建新对象，否则跟随窗体字体/框架默认
				if (it.props.contains("font") && it.props["font"].is_object())
				{
					auto& fj = it.props["font"];
					std::wstring fn = FromUtf8(fj.value("name", std::string()));
					float fs = (float)fj.value("size", (double)GetDefaultFontObject()->FontSize);
					if (fs < 1.0f) fs = 1.0f;
					if (fs > 200.0f) fs = 200.0f;
					if (fn.empty()) fn = GetDefaultFontObject()->FontName;
					c->Font = new ::Font(fn, fs);
				}
				else
				{
					if (_designedFormSharedFont) c->SetFontEx(_designedFormSharedFont, false);
					else c->SetFontEx(nullptr, false);
				}
			}

			if (it.extra.is_object())
			{
				if (it.type == UIClass::UI_GridPanel)
				{
					auto* gp = (GridPanel*)c;
					gp->ClearRows();
					gp->ClearColumns();
					if (it.extra.contains("rows") && it.extra["rows"].is_array())
					{
						for (auto& r : it.extra["rows"])
						{
							if (!r.is_object()) continue;
							GridLength h = GridLengthFromJson(r.value("height", Json()), GridLength::Auto());
							float minH = r.value("min", 0.0f);
							float maxH = r.value("max", FLT_MAX);
							gp->AddRow(h, minH, maxH);
						}
					}
					if (it.extra.contains("columns") && it.extra["columns"].is_array())
					{
						for (auto& col : it.extra["columns"])
						{
							if (!col.is_object()) continue;
							GridLength w = GridLengthFromJson(col.value("width", Json()), GridLength::Auto());
							float minW = col.value("min", 0.0f);
							float maxW = col.value("max", FLT_MAX);
							gp->AddColumn(w, minW, maxW);
						}
					}
				}
				else if (it.type == UIClass::UI_TabControl)
				{
					auto* tc = (TabControl*)c;
					tc->SelectedIndex = it.extra.value("selectedIndex", tc->SelectedIndex);
					tc->TitleHeight = it.extra.value("titleHeight", tc->TitleHeight);
					tc->TitleWidth = it.extra.value("titleWidth", tc->TitleWidth);
					if (it.extra.contains("pages") && it.extra["pages"].is_array())
					{
						for (auto& pj : it.extra["pages"])
						{
							if (!pj.is_object()) continue;
							std::wstring id = FromUtf8(pj.value("id", std::string()));
							auto text = FromUtf8(pj.value("text", std::string("Page")));
							auto* page = tc->AddPage(text);
							if (page)
								tabPageOf[id] = page;
						}
					}
				}
				else if (it.type == UIClass::UI_StackPanel)
				{
					auto* sp = (StackPanel*)c;
					Orientation o;
					if (it.extra.contains("orientation") && it.extra["orientation"].is_string() && TryParseOrientation(it.extra["orientation"].get<std::string>(), o))
						sp->SetOrientation(o);
					sp->SetSpacing(it.extra.value("spacing", sp->GetSpacing()));
				}
				else if (it.type == UIClass::UI_WrapPanel)
				{
					auto* wp = (WrapPanel*)c;
					Orientation o;
					if (it.extra.contains("orientation") && it.extra["orientation"].is_string() && TryParseOrientation(it.extra["orientation"].get<std::string>(), o))
						wp->SetOrientation(o);
					wp->SetItemWidth(it.extra.value("itemWidth", wp->GetItemWidth()));
					wp->SetItemHeight(it.extra.value("itemHeight", wp->GetItemHeight()));
				}
				else if (it.type == UIClass::UI_DockPanel)
				{
					auto* dp = (DockPanel*)c;
					dp->SetLastChildFill(it.extra.value("lastChildFill", dp->GetLastChildFill()));
				}
				else if (it.type == UIClass::UI_ToolBar)
				{
					auto* tb = (ToolBar*)c;
					tb->Padding = it.extra.value("padding", tb->Padding);
					tb->Gap = it.extra.value("gap", tb->Gap);
					tb->ItemHeight = it.extra.value("itemHeight", tb->ItemHeight);
				}
				else if (it.type == UIClass::UI_ComboBox)
				{
					auto* cb = (ComboBox*)c;
					cb->Items.Clear();
					if (it.extra.contains("items") && it.extra["items"].is_array())
					{
						for (auto& sj : it.extra["items"])
							if (sj.is_string()) cb->Items.Add(FromUtf8(sj.get<std::string>()));
					}
					cb->SelectedIndex = it.extra.value("selectedIndex", cb->SelectedIndex);
					if (cb->Items.Count > 0 && cb->SelectedIndex >= 0 && cb->SelectedIndex < cb->Items.Count)
						cb->Text = cb->Items[cb->SelectedIndex];
				}
				else if (it.type == UIClass::UI_GridView)
				{
					auto* gv = (GridView*)c;
					gv->Columns.Clear();
					if (it.extra.contains("columns") && it.extra["columns"].is_array())
					{
						for (auto& cj : it.extra["columns"])
						{
							if (!cj.is_object()) continue;
							GridViewColumn col;
							col.Name = FromUtf8(cj.value("name", std::string()));
							col.Width = cj.value("width", col.Width);
							col.Type = (ColumnType)cj.value("type", (int)col.Type);
							col.CanEdit = cj.value("canEdit", col.CanEdit);
							gv->Columns.Add(col);
						}
					}
				}
				else if (it.type == UIClass::UI_TreeView)
				{
					auto* tv = (TreeView*)c;
					if (tv->Root)
					{
						for (auto n : tv->Root->Children) delete n;
						tv->Root->Children.Clear();
						if (it.extra.contains("nodes"))
							JsonToTreeNodes(it.extra["nodes"], tv->Root->Children);
					}
					tv->SelectedBackColor = ColorFromJson(it.extra.value("selectedBackColor", Json()), tv->SelectedBackColor);
					tv->UnderMouseItemBackColor = ColorFromJson(it.extra.value("underMouseItemBackColor", Json()), tv->UnderMouseItemBackColor);
					tv->SelectedForeColor = ColorFromJson(it.extra.value("selectedForeColor", Json()), tv->SelectedForeColor);
				}
				else if (it.type == UIClass::UI_ProgressBar)
				{
					((ProgressBar*)c)->PercentageValue = it.extra.value("percentageValue", ((ProgressBar*)c)->PercentageValue);
				}
				else if (it.type == UIClass::UI_Slider)
				{
					auto* s = (Slider*)c;
					s->Min = it.extra.value("min", s->Min);
					s->Max = it.extra.value("max", s->Max);
					s->Value = it.extra.value("value", s->Value);
					s->Step = it.extra.value("step", s->Step);
					s->SnapToStep = it.extra.value("snapToStep", s->SnapToStep);
				}
				else if (it.type == UIClass::UI_StatusBar)
				{
					auto* sb = (StatusBar*)c;
					sb->TopMost = it.extra.value("topMost", sb->TopMost);
					sb->ClearParts();
					if (it.extra.contains("parts") && it.extra["parts"].is_array())
					{
						for (auto& pj : it.extra["parts"])
						{
							if (!pj.is_object()) continue;
							std::wstring text = FromUtf8(pj.value("text", std::string()));
							int w = pj.value("width", 0);
							sb->AddPart(text, w);
						}
					}
				}
				else if (it.type == UIClass::UI_MediaPlayer)
				{
					auto* mp = (MediaPlayer*)c;
					// 仅恢复属性与“媒体源路径”字段；不在设计器中自动加载/播放媒体。
					mp->AutoPlay = it.extra.value("autoPlay", mp->AutoPlay);
					mp->Loop = it.extra.value("loop", mp->Loop);
					mp->Volume = it.extra.value("volume", mp->Volume);
					mp->PlaybackRate = (float)it.extra.value("playbackRate", (double)mp->PlaybackRate);
					mp->RenderMode = (MediaPlayer::VideoRenderMode)it.extra.value("renderMode", (int)mp->RenderMode);
					if (it.extra.contains("mediaFile") && it.extra["mediaFile"].is_string())
						dc->DesignStrings[L"mediaFile"] = FromUtf8(it.extra["mediaFile"].get<std::string>());
					else
						dc->DesignStrings.erase(L"mediaFile");
				}
				else if (it.type == UIClass::UI_Menu)
				{
					auto* m = (Menu*)c;
					// 清空现有顶层项
					while (m->Count > 0)
					{
						auto* cc = m->operator[](m->Count - 1);
						m->RemoveControl(cc);
						delete cc;
					}
					if (it.extra.contains("items") && it.extra["items"].is_array())
					{
						for (auto& ij : it.extra["items"])
						{
							if (!ij.is_object()) continue;
							bool sep = ij.value("separator", false);
							if (sep) continue; // 顶层不支持 separator
							auto text = FromUtf8(ij.value("text", std::string()));
							if (text.empty()) continue;
							auto* top = m->AddItem(text);
							if (!top) continue;
							top->Id = ij.value("id", 0);
							top->Shortcut = FromUtf8(ij.value("shortcut", std::string()));
							top->Enable = ij.value("enable", true);
							if (ij.contains("subItems"))
							{
								std::vector<MenuItem*> tmp;
								JsonToMenuSubItems(ij["subItems"], tmp, top);
							}
						}
					}
				}
			}
		}

		std::unordered_map<std::wstring, std::vector<Pending*>> childrenByParent;
		childrenByParent.reserve(items.size());
		std::vector<Pending*> roots;
		roots.reserve(items.size());
		for (auto& it : items)
		{
			if (!it.parent.is_string())
			{
				roots.push_back(&it);
				continue;
			}
			childrenByParent[FromUtf8(it.parent.get<std::string>())].push_back(&it);
		}

		auto sortByOrder = [](std::vector<Pending*>& v) {
			std::stable_sort(v.begin(), v.end(), [](const Pending* a, const Pending* b) {
				return a->order < b->order;
			});
		};
		sortByOrder(roots);
		for (auto& kv : childrenByParent) sortByOrder(kv.second);

		std::unordered_set<std::wstring> attached;
		attached.reserve(items.size());

		auto attachOne = [&](Pending* it, Control* runtimeParent, Control* designerParent)
		{
			if (!it) return;
			auto dc = dcOf[it->name];
			if (!dc || !dc->ControlInstance) return;
			auto* c = dc->ControlInstance;
			if (!runtimeParent) runtimeParent = _clientSurface ? (Control*)_clientSurface : (Control*)_designSurface;
			if (!runtimeParent) return;
			if (runtimeParent->Type() == UIClass::UI_ToolBar)
			{
				if (c->Type() == UIClass::UI_Button)
					((ToolBar*)runtimeParent)->AddToolButton((Button*)c);
				else
					runtimeParent->AddControl(c);
			}
			else
			{
				runtimeParent->AddControl(c);
			}
			dc->DesignerParent = designerParent;
			_designerControls.push_back(dc);
			attached.insert(it->name);
		};

		std::function<void(const std::wstring& parentKey, Control* runtimeParent, Control* designerParent)> attachChildren;
		attachChildren = [&](const std::wstring& parentKey, Control* runtimeParent, Control* designerParent)
		{
			auto it = childrenByParent.find(parentKey);
			if (it == childrenByParent.end()) return;
			for (auto* ch : it->second)
			{
				attachOne(ch, runtimeParent, designerParent);
				attachChildren(ch->name, dcOf[ch->name]->ControlInstance, dcOf[ch->name]->ControlInstance);
				if (ch->type == UIClass::UI_TabControl)
				{
					auto* tc = (TabControl*)dcOf[ch->name]->ControlInstance;
					(void)tc;
					for (auto& kv : tabPageOf)
					{
						std::wstring prefix = ch->name + L"#page";
						if (kv.first.rfind(prefix, 0) != 0) continue;
						attachChildren(kv.first, kv.second, kv.second);
					}
				}
			}
		};

		for (auto* it : roots)
		{
			attachOne(it, _clientSurface ? (Control*)_clientSurface : (Control*)_designSurface, nullptr);
			attachChildren(it->name, dcOf[it->name]->ControlInstance, dcOf[it->name]->ControlInstance);
			if (it->type == UIClass::UI_TabControl)
			{
				for (auto& kv : tabPageOf)
				{
					std::wstring prefix = it->name + L"#page";
					if (kv.first.rfind(prefix, 0) != 0) continue;
					attachChildren(kv.first, kv.second, kv.second);
				}
			}
		}

		if (attached.size() != items.size())
		{
			for (auto& it : items)
			{
				if (attached.find(it.name) == attached.end())
				{
					if (outError) *outError = L"无法解析控件父级引用，未能挂载控件: " + it.name;
					return false;
				}
			}
		}

		if (_designSurface)
		{
			if (auto* p = dynamic_cast<Panel*>(_designSurface))
			{
				p->InvalidateLayout();
				p->PerformLayout();
			}
		}
		UpdateClientSurfaceLayout();
		for (auto& dc : _designerControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (auto* p = dynamic_cast<Panel*>(dc->ControlInstance))
			{
				p->InvalidateLayout();
				p->PerformLayout();
			}
		}

		ClearSelection();
		OnControlSelected(nullptr);
		this->PostRender();
		return true;
	}
	catch (const std::exception& ex)
	{
		if (outError) *outError = L"加载失败: " + FromUtf8(ex.what());
		return false;
	}
	catch (...)
	{
		if (outError) *outError = L"加载失败：未知错误。";
		return false;
	}
}
