#include "HitTestService.h"

#include "../../CUI_Legacy/GUI/Control.h"

#include <climits>

bool HitTestService::IsDescendantOf(Control* ancestor, Control* node)
{
	if (!ancestor || !node) return false;
	auto* parent = node->Parent;
	while (parent)
	{
		if (parent == ancestor) return true;
		parent = parent->Parent;
	}
	return false;
}

bool HitTestService::IsContainerControl(Control* control)
{
	if (!control) return false;
	switch (control->Type())
	{
	case UIClass::UI_Panel:
	case UIClass::UI_GroupBox:
	case UIClass::UI_ScrollView:
	case UIClass::UI_StackPanel:
	case UIClass::UI_GridPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_SplitContainer:
	case UIClass::UI_TabControl:
	case UIClass::UI_ToolBar:
	case UIClass::UI_TabPage:
		return true;
	default:
		return false;
	}
}

std::shared_ptr<DesignerControl> HitTestService::HitTestControl(
	Control* root,
	const std::vector<std::shared_ptr<DesignerControl>>& designerControls,
	POINT pt,
	bool preferParentContainer)
{
	auto pointInRect = [](POINT p, POINT loc, SIZE sz) -> bool {
		return p.x >= loc.x && p.y >= loc.y && p.x <= (loc.x + sz.cx) && p.y <= (loc.y + sz.cy);
	};

	std::function<Control*(Control*, POINT)> hitDeepest = [&](Control* parent, POINT ptLocal) -> Control* {
		if (!parent) return nullptr;
		for (int i = parent->Count - 1; i >= 0; i--)
		{
			auto* child = parent->operator[](i);
			if (!child || !child->Visible) continue;

			auto loc = child->ActualLocation;
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

	auto findDesigner = [&](Control* control) -> std::shared_ptr<DesignerControl> {
		while (control && control != root)
		{
			for (auto it = designerControls.rbegin(); it != designerControls.rend(); ++it)
			{
				auto& dc = *it;
				if (dc && dc->ControlInstance == control)
					return dc;
			}
			control = control->Parent;
		}
		return nullptr;
	};

	Control* hit = hitDeepest(root, pt);
	if (!hit) return nullptr;

	if (preferParentContainer)
	{
		Control* parent = hit->Parent;
		while (parent && parent != root)
		{
			auto dc = findDesigner(parent);
			if (dc) return dc;
			parent = parent->Parent;
		}
	}

	return findDesigner(hit);
}

Control* HitTestService::FindBestContainerAtPoint(
	const std::vector<std::shared_ptr<DesignerControl>>& designerControls,
	POINT ptCanvas,
	Control* ignore,
	const std::function<RECT(Control*)>& getControlRectInCanvas)
{
	Control* best = nullptr;
	int bestArea = INT_MAX;

	for (const auto& dc : designerControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		auto* control = dc->ControlInstance;
		if (!control->IsVisual || !control->Visible || !control->Enable) continue;
		if (!IsContainerControl(control)) continue;
		if (ignore && (control == ignore || IsDescendantOf(ignore, control))) continue;

		auto rect = getControlRectInCanvas(control);
		if (ptCanvas.x >= rect.left && ptCanvas.x <= rect.right && ptCanvas.y >= rect.top && ptCanvas.y <= rect.bottom)
		{
			int area = (rect.right - rect.left) * (rect.bottom - rect.top);
			if (area < bestArea)
			{
				best = control;
				bestArea = area;
			}
		}
	}

	return best;
}
