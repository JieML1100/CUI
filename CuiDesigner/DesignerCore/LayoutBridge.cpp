#include "LayoutBridge.h"

#include "../FakeWebBrowser.h"
#include "../../CUI/GUI/Button.h"
#include "../../CUI/GUI/Control.h"
#include "../../CUI/GUI/Menu.h"
#include "../../CUI/GUI/StatusBar.h"
#include "../../CUI/GUI/TabControl.h"
#include "../../CUI/GUI/ToolBar.h"
#include "../../CUI/GUI/Layout/DockPanel.h"
#include "../../CUI/GUI/Layout/GridPanel.h"
#include "../../CUI/GUI/Layout/RelativePanel.h"
#include "../../CUI/GUI/Layout/StackPanel.h"
#include "../../CUI/GUI/Layout/WrapPanel.h"
#include "../../CUI/GUI/Panel.h"

#include <algorithm>
#include <cmath>

Control* LayoutBridge::NormalizeContainerForDrop(Control* container)
{
	if (!container) return nullptr;
	if (container->Type() == UIClass::UI_TabControl)
	{
		auto* tabControl = (TabControl*)container;
		if (tabControl->Count <= 0)
		{
			tabControl->AddPage(L"Page 1");
		}
		if (tabControl->Count <= 0) return tabControl;
		if (tabControl->SelectedIndex < 0) tabControl->SelectedIndex = 0;
		if (tabControl->SelectedIndex >= tabControl->Count) tabControl->SelectedIndex = tabControl->Count - 1;
		return tabControl->operator[](tabControl->SelectedIndex);
	}
	return container;
}

bool LayoutBridge::CanAcceptChild(Control* container, UIClass childType)
{
	if (!container) return false;
	if (container->Type() == UIClass::UI_ToolBar)
	{
		return childType == UIClass::UI_Button;
	}
	return true;
}

void LayoutBridge::AttachChild(Control* container, Control* child)
{
	if (!container || !child) return;
	if (container->Type() == UIClass::UI_ToolBar)
	{
		auto* toolBar = (ToolBar*)container;
		toolBar->AddToolButton((Button*)child);
		return;
	}
	container->AddControl(child);
}

void LayoutBridge::ApplyNewChildLayout(Control* container, Control* child, POINT local, POINT dropLocal)
{
	if (!container || !child) return;

	if (container->Type() == UIClass::UI_GridPanel)
	{
		auto* gridPanel = (GridPanel*)container;
		int row = 0;
		int col = 0;
		if (gridPanel->TryGetCellAtPoint(dropLocal, row, col))
		{
			child->GridRow = row;
			child->GridColumn = col;
		}
		child->HAlign = HorizontalAlignment::Stretch;
		child->VAlign = VerticalAlignment::Stretch;
		child->Location = { 0, 0 };
		return;
	}

	if (container->Type() == UIClass::UI_StackPanel)
	{
		child->Location = { 0, 0 };
		return;
	}

	if (container->Type() == UIClass::UI_DockPanel)
	{
		auto containerSize = container->Size;
		float w = (float)containerSize.cx;
		float h = (float)containerSize.cy;
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
		child->DockPosition = dock;
		child->Location = { 0, 0 };
		return;
	}

	if (container->Type() == UIClass::UI_WrapPanel)
	{
		auto* wrapPanel = (WrapPanel*)container;
		int insertIndex = wrapPanel->Count - 1;
		Orientation orient = wrapPanel->GetOrientation();
		const float lineTol = 10.0f;
		for (int i = 0; i < wrapPanel->Count; i++)
		{
			auto* current = wrapPanel->operator[](i);
			if (!current || current == child || !current->Visible) continue;
			auto currentLocation = current->ActualLocation;
			auto currentSize = current->ActualSize();
			float childPrimary = (orient == Orientation::Horizontal) ? (float)currentLocation.y : (float)currentLocation.x;
			float childSecondaryMid = (orient == Orientation::Horizontal)
				? (currentLocation.x + currentSize.cx * 0.5f)
				: (currentLocation.y + currentSize.cy * 0.5f);
			float dropPrimary = (orient == Orientation::Horizontal) ? (float)dropLocal.y : (float)dropLocal.x;
			float dropSecondary = (orient == Orientation::Horizontal) ? (float)dropLocal.x : (float)dropLocal.y;
			if (childPrimary > dropPrimary + lineTol || (std::fabs(childPrimary - dropPrimary) <= lineTol && dropSecondary < childSecondaryMid))
			{
				insertIndex = i;
				break;
			}
		}
		auto found = std::find(wrapPanel->Children.begin(), wrapPanel->Children.end(), child);
		int curIndex = (found != wrapPanel->Children.end()) ? std::distance(wrapPanel->Children.begin(), found) : -1;
		if (curIndex >= 0)
		{
			while (curIndex > insertIndex)
			{
				std::swap(wrapPanel->Children[curIndex], wrapPanel->Children[curIndex - 1]);
				curIndex--;
			}
			while (curIndex < insertIndex)
			{
				std::swap(wrapPanel->Children[curIndex], wrapPanel->Children[curIndex + 1]);
				curIndex++;
			}
		}
		child->Location = { 0, 0 };
		return;
	}

	if (container->Type() == UIClass::UI_RelativePanel)
	{
		auto margin = child->Margin;
		margin.Left = (float)local.x;
		margin.Top = (float)local.y;
		margin.Right = 0.0f;
		margin.Bottom = 0.0f;
		child->Margin = margin;
		child->Location = { 0, 0 };
		return;
	}

	child->Location = local;
}

void LayoutBridge::ApplyExistingChildLayout(
	Control* container,
	Control* child,
	POINT local,
	POINT dropLocalCenter,
	bool containerChanged,
	const RECT& originalRectInCanvas,
	const std::function<void(const RECT&)>& applyRectToControl)
{
	if (!container || !child) return;

	if (container->Type() == UIClass::UI_GridPanel)
	{
		auto* gridPanel = (GridPanel*)container;
		int row = 0;
		int col = 0;
		if (gridPanel->TryGetCellAtPoint(dropLocalCenter, row, col))
		{
			child->GridRow = row;
			child->GridColumn = col;
		}
		child->HAlign = HorizontalAlignment::Stretch;
		child->VAlign = VerticalAlignment::Stretch;
		child->Location = { 0, 0 };
		return;
	}

	if (container->Type() == UIClass::UI_StackPanel)
	{
		auto* stackPanel = (StackPanel*)container;
		int insertIndex = stackPanel->Count - 1;
		Orientation orient = stackPanel->GetOrientation();
		for (int i = 0; i < stackPanel->Count; i++)
		{
			auto* current = stackPanel->operator[](i);
			if (!current || current == child || !current->Visible) continue;
			auto currentLocation = current->ActualLocation;
			auto currentSize = current->ActualSize();
			float mid = (orient == Orientation::Vertical)
				? (currentLocation.y + currentSize.cy * 0.5f)
				: (currentLocation.x + currentSize.cx * 0.5f);
			float dropAxis = (orient == Orientation::Vertical) ? (float)dropLocalCenter.y : (float)dropLocalCenter.x;
			if (dropAxis < mid)
			{
				insertIndex = i;
				break;
			}
		}
		auto found = std::find(stackPanel->Children.begin(), stackPanel->Children.end(), child);
		int curIndex = (found != stackPanel->Children.end()) ? std::distance(stackPanel->Children.begin(), found) : -1;
		if (curIndex >= 0)
		{
			while (curIndex > insertIndex)
			{
				std::swap(stackPanel->Children[curIndex], stackPanel->Children[curIndex - 1]);
				curIndex--;
			}
			while (curIndex < insertIndex)
			{
				std::swap(stackPanel->Children[curIndex], stackPanel->Children[curIndex + 1]);
				curIndex++;
			}
		}
		child->Location = { 0, 0 };
		return;
	}

	if (container->Type() == UIClass::UI_DockPanel)
	{
		ApplyNewChildLayout(container, child, local, dropLocalCenter);
		return;
	}

	if (container->Type() == UIClass::UI_WrapPanel)
	{
		ApplyNewChildLayout(container, child, local, dropLocalCenter);
		return;
	}

	if (container->Type() == UIClass::UI_RelativePanel)
	{
		ApplyNewChildLayout(container, child, local, dropLocalCenter);
		return;
	}

	if (containerChanged && applyRectToControl)
	{
		applyRectToControl(originalRectInCanvas);
	}
}

void LayoutBridge::RefreshContainerLayout(Control* container)
{
	if (auto* panel = dynamic_cast<Panel*>(container))
	{
		panel->InvalidateLayout();
		panel->PerformLayout();
	}
}