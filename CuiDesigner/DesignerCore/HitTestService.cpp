#include "HitTestService.h"

#include "../../CUI/include/Control.h"
#include "../../CUI/include/ItemsControl.h"

#include <algorithm>
#include <climits>
#include <cmath>

bool HitTestService::IsDescendantOf(Control* ancestor, Control* node)
{
	if (!ancestor || !node) return false;
	auto* parent = node->GetVisualParent();
	while (parent)
	{
		if (parent == ancestor) return true;
		parent = parent->GetVisualParent();
	}
	return false;
}

bool HitTestService::IsContainerControl(Control* control)
{
	if (!control) return false;
	if (auto* items = dynamic_cast<ItemsControl*>(control))
		return !items->GetItemsSource();
	switch (control->Type())
	{
	case UIClass::UI_Canvas:
	case UIClass::UI_GroupBox:
	case UIClass::UI_Expander:
	case UIClass::UI_ScrollViewer:
	case UIClass::UI_StackPanel:
	case UIClass::UI_Grid:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_TabControl:
	case UIClass::UI_TabItem:
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
	std::function<Control*(Control*)> hitDeepest = [&](Control* parent) -> Control* {
		if (!parent) return nullptr;
		auto children = parent->GetVisualChildrenInReverseZOrder();
		for (auto* child : children)
		{
			if (!child || !child->IsVisible) continue;
			D2D1_POINT_2F local{};
			if (!child->TryTransformRenderPointToLocal(
				D2D1::Point2F(
					static_cast<float>(pt.x), static_cast<float>(pt.y)), local)
				|| !child->IsRenderPointInsideClip(D2D1::Point2F(
					static_cast<float>(pt.x), static_cast<float>(pt.y)))
				|| !child->ContainsPoint(
					static_cast<int>(std::floor(local.x)),
					static_cast<int>(std::floor(local.y))))
				continue;
			if (child->HitTestChildren() && child->VisualChildCount() > 0)
			{
				auto* deeper = hitDeepest(child);
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
			control = control->GetVisualParent();
		}
		return nullptr;
	};

	Control* hit = hitDeepest(root);
	if (!hit) return nullptr;

	if (preferParentContainer)
	{
		Control* parent = hit->GetVisualParent();
		while (parent && parent != root)
		{
			auto dc = findDesigner(parent);
			if (dc) return dc;
			parent = parent->GetVisualParent();
		}
	}

	return findDesigner(hit);
}

Control* HitTestService::FindBestContainerAtPoint(
	const std::vector<std::shared_ptr<DesignerControl>>& designerControls,
	POINT ptCanvas,
	Control* ignore,
	const std::function<RECT(Control*)>& getControlRectInCanvas,
	const std::function<bool(Control*)>& containsPoint)
{
	Control* best = nullptr;
	int bestArea = INT_MAX;

	for (const auto& dc : designerControls)
	{
		if (!dc || !dc->ControlInstance) continue;
		auto* control = dc->ControlInstance;
		if (!control->IsVisible || !control->IsEnabled) continue;
		if (!IsContainerControl(control)) continue;
		if (ignore && (control == ignore || IsDescendantOf(ignore, control))) continue;

		auto rect = getControlRectInCanvas(control);
		if (ptCanvas.x >= rect.left && ptCanvas.x <= rect.right
			&& ptCanvas.y >= rect.top && ptCanvas.y <= rect.bottom
			&& (!containsPoint || containsPoint(control)))
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
