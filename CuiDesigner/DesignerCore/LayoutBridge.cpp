#include "LayoutBridge.h"

#include "../FakeWebBrowser.h"
#include "../../CUI/include/Button.h"
#include "../../CUI/include/Canvas.h"
#include "../../CUI/include/Control.h"
#include "../../CUI/include/ItemsControl.h"
#include "../../CUI/include/Menu.h"
#include "../../CUI/include/StatusBar.h"
#include "../../CUI/include/TabControl.h"
#include "../../CUI/include/Layout/DockPanel.h"
#include "../../CUI/include/Layout/Grid.h"
#include "../../CUI/include/Layout/RelativePanel.h"
#include "../../CUI/include/Layout/StackPanel.h"
#include "../../CUI/include/Layout/WrapPanel.h"
#include "../../CUI/include/Panel.h"
#include "../../CUI/include/ItemsPresenter.h"
#include "../../CUI/include/TemplateInfrastructure.h"

#include <algorithm>
#include <cmath>

namespace
{
	void ClearCanvasPlacement(Control* child)
	{
		Canvas::SetLeft(*(child), cui::layout::UnsetCanvasOffset);
		Canvas::SetTop(*(child), cui::layout::UnsetCanvasOffset);
		Canvas::SetRight(*(child), cui::layout::UnsetCanvasOffset);
		Canvas::SetBottom(*(child), cui::layout::UnsetCanvasOffset);
	}

	void SetCanvasPlacement(Control* child, POINT local)
	{
		Canvas::SetLeft(*(child), static_cast<float>(local.x));
		Canvas::SetTop(*(child), static_cast<float>(local.y));
		Canvas::SetRight(*(child), cui::layout::UnsetCanvasOffset);
		Canvas::SetBottom(*(child), cui::layout::UnsetCanvasOffset);
	}

	void MoveAuthoredItemToDrop(
		ItemsControl& owner, Control* child, POINT dropLocal)
	{
		auto* host = cui::framework::TemplateAccess::GetItemsHost(owner);
		if (!host || !child) return;
		size_t currentIndex = owner.AuthoredItemCount();
		for (size_t index = 0; index < owner.AuthoredItemCount(); ++index)
			if (owner.GetAuthoredItem(index) == child)
			{
				currentIndex = index;
				break;
			}
		if (currentIndex >= owner.AuthoredItemCount()) return;
		const auto hostLocation = host->GetActualLocationDip();
		const cui::core::Point point{
			static_cast<float>(dropLocal.x) - hostLocation.x,
			static_cast<float>(dropLocal.y) - hostLocation.y };
		size_t insertion = owner.AuthoredItemCount() - 1;
		if (auto* stack = dynamic_cast<StackPanel*>(host))
		{
			const auto orientation = stack->GetOrientation();
			for (size_t index = 0; index < owner.AuthoredItemCount(); ++index)
			{
				auto* candidate = owner.GetAuthoredItem(index);
				if (!candidate || candidate == child || candidate->IsCollapsed()) continue;
				const auto location = candidate->GetActualLocationDip();
				const auto size = candidate->GetActualSizeDip();
				const float midpoint = orientation == Orientation::Vertical
					? location.y + size.height * 0.5f
					: location.x + size.width * 0.5f;
				const float coordinate = orientation == Orientation::Vertical
					? point.y : point.x;
				if (coordinate >= midpoint) continue;
				insertion = index;
				if (currentIndex < insertion) --insertion;
				break;
			}
		}
		(void)owner.MoveItemControl(currentIndex, insertion);
		ClearCanvasPlacement(child);
	}
}

Control* LayoutBridge::NormalizeContainerForDrop(
	Control* container, UIClass childType)
{
	if (!container) return nullptr;
	if (container->Type() == UIClass::UI_TabControl)
	{
		auto* tabControl = (TabControl*)container;
		if (childType == UIClass::UI_TabItem) return tabControl;
		if (static_cast<int>(tabControl->ItemCount()) <= 0) return nullptr;
		if (tabControl->SelectedIndex < 0
			|| tabControl->SelectedIndex >= static_cast<int>(tabControl->ItemCount()))
		{
			tabControl->SelectItem((std::clamp)(
				tabControl->SelectedIndex, 0, static_cast<int>(tabControl->ItemCount()) - 1));
		}
		return tabControl->GetItem(tabControl->SelectedIndex);
	}
	return container;
}

bool LayoutBridge::CanAcceptChild(Control* container, UIClass childType)
{
	if (!container) return false;
	if (dynamic_cast<ContentPresenter*>(container)) return false;
	if (dynamic_cast<ItemsPresenter*>(container)) return false;
	if (auto* tabs = dynamic_cast<TabControl*>(container))
		return childType == UIClass::UI_TabItem && !tabs->GetItemsSource();
	if (auto* content = dynamic_cast<ContentControl*>(container))
	{
		(void)childType;
		return !content->GetVisualContent()
			&& !cui::framework::TemplateAccess::GetGeneratedPresenter(*content)
			&& content->GetContent().Empty()
			&& !content->GetContentTemplate();
	}
	if (auto* items = dynamic_cast<ItemsControl*>(container))
	{
		(void)childType;
		return !items->GetItemsSource();
	}
	return true;
}

void LayoutBridge::AttachChild(Control* container, Control* child)
{
	if (!container || !child) return;
	if (auto* items = dynamic_cast<ItemsControl*>(container))
	{
		items->AdoptItemControl(child);
		return;
	}
	container->AdoptVisualChild(child);
}

Control* LayoutBridge::AttachChild(Control* container, std::unique_ptr<Control> child)
{
	if (!container || !child)
		return nullptr;
	if (auto* items = dynamic_cast<ItemsControl*>(container))
		return items->AddItemControl(std::move(child));
	return container->AddOwned(std::move(child));
}

void LayoutBridge::ApplyNewChildLayout(Control* container, Control* child, POINT local, POINT dropLocal)
{
	if (!container || !child) return;
	if (auto* items = dynamic_cast<ItemsControl*>(container))
	{
		MoveAuthoredItemToDrop(*items, child, dropLocal);
		return;
	}

	if (container->Type() == UIClass::UI_Grid)
	{
		auto* gridPanel = static_cast<Grid*>(container);
		int row = 0;
		int col = 0;
		if (gridPanel->TryGetCellAtPoint(cui::core::Point{
			static_cast<float>(dropLocal.x),
			static_cast<float>(dropLocal.y) }, row, col))
		{
			Grid::SetRow(*(child), row);
			Grid::SetColumn(*(child), col);
		}
		child->HorizontalAlignment = HorizontalAlignment::Stretch;
		child->VerticalAlignment = VerticalAlignment::Stretch;
		ClearCanvasPlacement(child);
		return;
	}

	if (dynamic_cast<ContentControl*>(container))
	{
		child->HorizontalAlignment = HorizontalAlignment::Stretch;
		child->VerticalAlignment = VerticalAlignment::Stretch;
		ClearCanvasPlacement(child);
		return;
	}

	if (container->Type() == UIClass::UI_StackPanel)
	{
		ClearCanvasPlacement(child);
		return;
	}

	if (container->Type() == UIClass::UI_DockPanel)
	{
		const auto containerSize = container->GetActualSizeDip();
		const float w = containerSize.width;
		const float h = containerSize.height;
		float x = (float)dropLocal.x;
		float y = (float)dropLocal.y;
		float left = x;
		float right = w - x;
		float top = y;
		float bottom = h - y;

		Dock dock = Dock::Left;
		float minDist = left;
		if (top < minDist) { minDist = top; dock = Dock::Top; }
		if (right < minDist) { minDist = right; dock = Dock::Right; }
		if (bottom < minDist) { minDist = bottom; dock = Dock::Bottom; }
		DockPanel::SetDock(*(child), dock);
		ClearCanvasPlacement(child);
		return;
	}

	if (container->Type() == UIClass::UI_WrapPanel)
	{
		auto* wrapPanel = (WrapPanel*)container;
		int insertIndex = wrapPanel->VisualChildCount() - 1;
		Orientation orient = wrapPanel->GetOrientation();
		const float lineTol = 10.0f;
		for (int i = 0; i < wrapPanel->VisualChildCount(); i++)
		{
			auto* current = wrapPanel->GetVisualChild(i);
			if (!current || current == child || current->IsCollapsed()) continue;
			auto currentLocation = current->GetActualLocationDip();
			auto currentSize = current->GetActualSizeDip();
			float childPrimary = (orient == Orientation::Horizontal) ? (float)currentLocation.y : (float)currentLocation.x;
			float childSecondaryMid = (orient == Orientation::Horizontal)
				? (currentLocation.x + currentSize.width * 0.5f)
				: (currentLocation.y + currentSize.height * 0.5f);
			float dropPrimary = (orient == Orientation::Horizontal) ? (float)dropLocal.y : (float)dropLocal.x;
			float dropSecondary = (orient == Orientation::Horizontal) ? (float)dropLocal.x : (float)dropLocal.y;
			if (childPrimary > dropPrimary + lineTol || (std::fabs(childPrimary - dropPrimary) <= lineTol && dropSecondary < childSecondaryMid))
			{
				insertIndex = i;
				break;
			}
		}
		auto found = std::find(wrapPanel->GetVisualChildrenView().begin(),
			wrapPanel->GetVisualChildrenView().end(), child);
		int curIndex = (found != wrapPanel->GetVisualChildrenView().end())
			? static_cast<int>(std::distance(
				wrapPanel->GetVisualChildrenView().begin(), found)) : -1;
		if (curIndex >= 0)
		{
			wrapPanel->MoveVisualChild(
				static_cast<size_t>(curIndex),
				static_cast<size_t>(insertIndex));
		}
		ClearCanvasPlacement(child);
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
		ClearCanvasPlacement(child);
		return;
	}

	SetCanvasPlacement(child, local);
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
	if (auto* items = dynamic_cast<ItemsControl*>(container))
	{
		MoveAuthoredItemToDrop(*items, child, dropLocalCenter);
		return;
	}

	if (container->Type() == UIClass::UI_Grid)
	{
		auto* gridPanel = (Grid*)container;
		int row = 0;
		int col = 0;
		if (gridPanel->TryGetCellAtPoint(cui::core::Point{
			static_cast<float>(dropLocalCenter.x),
			static_cast<float>(dropLocalCenter.y) }, row, col))
		{
			Grid::SetRow(*(child), row);
			Grid::SetColumn(*(child), col);
		}
		child->HorizontalAlignment = HorizontalAlignment::Stretch;
		child->VerticalAlignment = VerticalAlignment::Stretch;
		ClearCanvasPlacement(child);
		return;
	}

	if (container->Type() == UIClass::UI_StackPanel)
	{
		auto* stackPanel = (StackPanel*)container;
		int insertIndex = stackPanel->VisualChildCount() - 1;
		Orientation orient = stackPanel->GetOrientation();
		for (int i = 0; i < stackPanel->VisualChildCount(); i++)
		{
			auto* current = stackPanel->GetVisualChild(i);
			if (!current || current == child || current->IsCollapsed()) continue;
			auto currentLocation = current->GetActualLocationDip();
			auto currentSize = current->GetActualSizeDip();
			float mid = (orient == Orientation::Vertical)
				? (currentLocation.y + currentSize.height * 0.5f)
				: (currentLocation.x + currentSize.width * 0.5f);
			float dropAxis = (orient == Orientation::Vertical) ? (float)dropLocalCenter.y : (float)dropLocalCenter.x;
			if (dropAxis < mid)
			{
				insertIndex = i;
				break;
			}
		}
		auto found = std::find(stackPanel->GetVisualChildrenView().begin(),
			stackPanel->GetVisualChildrenView().end(), child);
		int curIndex = (found != stackPanel->GetVisualChildrenView().end())
			? static_cast<int>(std::distance(
				stackPanel->GetVisualChildrenView().begin(), found)) : -1;
		if (curIndex >= 0)
		{
			stackPanel->MoveVisualChild(
				static_cast<size_t>(curIndex),
				static_cast<size_t>(insertIndex));
		}
		ClearCanvasPlacement(child);
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
	if (auto* items = dynamic_cast<ItemsControl*>(container))
		container = cui::framework::TemplateAccess::GetItemsHost(*items);
	if (auto* panel = dynamic_cast<Panel*>(container))
	{
		panel->InvalidateLayout();
		panel->UpdateLayout();
	}
}
