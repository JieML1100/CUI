#include "Layout/RelativePanel.h"
#include "Form.h"
#include <algorithm>
#include <set>

// RelativeLayoutEngine 实现

bool RelativeLayoutEngine::HasCycle(Control* start, Control* current, std::map<Control*, int>& visited)
{
	if (visited[current] == 1) // 正在访问中
		return true;
	if (visited[current] == 2) // 已访问完成
		return false;
	
	visited[current] = 1; // 标记为正在访问
	
	auto it = _constraints.find(current);
	if (it != _constraints.end())
	{
		const RelativeConstraints& constraints = it->second;
		
		// 检查所有依赖的控件
		if (constraints.AlignLeftWith && HasCycle(start, constraints.AlignLeftWith, visited)) return true;
		if (constraints.AlignRightWith && HasCycle(start, constraints.AlignRightWith, visited)) return true;
		if (constraints.AlignTopWith && HasCycle(start, constraints.AlignTopWith, visited)) return true;
		if (constraints.AlignBottomWith && HasCycle(start, constraints.AlignBottomWith, visited)) return true;
		if (constraints.LeftOf && HasCycle(start, constraints.LeftOf, visited)) return true;
		if (constraints.RightOf && HasCycle(start, constraints.RightOf, visited)) return true;
		if (constraints.Above && HasCycle(start, constraints.Above, visited)) return true;
		if (constraints.Below && HasCycle(start, constraints.Below, visited)) return true;
	}
	
	visited[current] = 2; // 标记为已访问完成
	return false;
}

std::vector<Control*> RelativeLayoutEngine::TopologicalSort(LayoutContext& context)
{
	std::vector<Control*> result;
	
	std::set<Control*> processed;
	std::map<Control*, int> visited; // 0=未访问, 1=正在访问, 2=已完成
	
	// 初始化
	for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
	{
		auto child = context.ChildAt(childIndex);
		if (child && child->Visible)
			visited[child] = 0;
	}
	
	// 简化版：按依赖深度排序
	// 没有依赖的控件先排列，有依赖的后排列
	std::vector<Control*> noDeps;
	std::vector<Control*> hasDeps;
	
	for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
	{
		auto child = context.ChildAt(childIndex);
		if (!child || !child->Visible) continue;
		
		auto it = _constraints.find(child);
		bool hasDependency = false;
		
		if (it != _constraints.end())
		{
			const RelativeConstraints& constraints = it->second;
			if (constraints.AlignLeftWith || constraints.AlignRightWith || constraints.AlignTopWith || constraints.AlignBottomWith ||
				constraints.LeftOf || constraints.RightOf || constraints.Above || constraints.Below)
			{
				hasDependency = true;
			}
		}
		
		if (hasDependency)
			hasDeps.push_back(child);
		else
			noDeps.push_back(child);
	}
	
	// 先添加没有依赖的
	for (auto child : noDeps)
		result.push_back(child);
	
	// 再添加有依赖的
	for (auto child : hasDeps)
		result.push_back(child);
	
	return result;
}

SIZE RelativeLayoutEngine::Measure(Control* container, SIZE availableSize)
{
	if (!container) return SIZE{ 0, 0 };
	LayoutContext context(container);
	const auto desired = Measure(context, cui::core::Constraints{ cui::core::Size{
		static_cast<float>((std::max)(0L, availableSize.cx)),
		static_cast<float>((std::max)(0L, availableSize.cy)) } });
	return SIZE{ static_cast<LONG>(std::ceil(desired.width)), static_cast<LONG>(std::ceil(desired.height)) };
}

cui::core::Size RelativeLayoutEngine::Measure(LayoutContext& context, const cui::core::Constraints& available)
{
	
	// 简单测量：返回容器期望的最大尺寸
	cui::core::Size desiredSize{};
	
	for (int childIndex = 0; childIndex < context.ChildCount(); childIndex++)
	{
		auto child = context.ChildAt(childIndex);
		if (!child || !child->Visible) continue;
		
		const auto childSize = child->Measure(available);
		Thickness margin = child->Margin;
		
		const float totalWidth = childSize.width + margin.Left + margin.Right;
		const float totalHeight = childSize.height + margin.Top + margin.Bottom;
		
		if (totalWidth > desiredSize.width)
			desiredSize.width = totalWidth;
		if (totalHeight > desiredSize.height)
			desiredSize.height = totalHeight;
	}
	
	_needsLayout = false;
	return desiredSize;
}

void RelativeLayoutEngine::Arrange(Control* container, D2D1_RECT_F finalRect)
{
	if (!container) return;
	LayoutContext context(container);
	Arrange(context, finalRect);
}

void RelativeLayoutEngine::Arrange(LayoutContext& context, D2D1_RECT_F finalRect)
{
	
	const float originX = finalRect.left;
	const float originY = finalRect.top;
	float containerWidth = finalRect.right - finalRect.left;
	float containerHeight = finalRect.bottom - finalRect.top;
	
	// 拓扑排序
	std::vector<Control*> sorted = TopologicalSort(context);
	
	// 存储每个控件的计算位置
	std::map<Control*, D2D1_RECT_F> positions;
	
	// 遍历排序后的控件
	for (auto child : sorted)
	{
		if (!child) continue;
		
		const auto childSize = child->Measure(cui::core::Constraints{ cui::core::Size{
			containerWidth, containerHeight } });
		Thickness margin = child->Margin;
		
		float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
		bool leftSet = false, topSet = false, rightSet = false, bottomSet = false;
		
		auto it = _constraints.find(child);
		if (it != _constraints.end())
		{
			const RelativeConstraints& constraints = it->second;
			
			// 相对于面板对齐
			if (constraints.AlignLeftWithPanel)
			{
				left = originX + margin.Left;
				leftSet = true;
			}
			if (constraints.AlignRightWithPanel)
			{
				right = originX + containerWidth - margin.Right;
				rightSet = true;
			}
			if (constraints.AlignTopWithPanel)
			{
				top = originY + margin.Top;
				topSet = true;
			}
			if (constraints.AlignBottomWithPanel)
			{
				bottom = originY + containerHeight - margin.Bottom;
				bottomSet = true;
			}
			
			// 相对于其他控件对齐
			if (constraints.AlignLeftWith && positions.find(constraints.AlignLeftWith) != positions.end())
			{
				left = positions[constraints.AlignLeftWith].left + margin.Left;
				leftSet = true;
			}
			if (constraints.AlignRightWith && positions.find(constraints.AlignRightWith) != positions.end())
			{
				right = positions[constraints.AlignRightWith].right - margin.Right;
				rightSet = true;
			}
			if (constraints.AlignTopWith && positions.find(constraints.AlignTopWith) != positions.end())
			{
				top = positions[constraints.AlignTopWith].top + margin.Top;
				topSet = true;
			}
			if (constraints.AlignBottomWith && positions.find(constraints.AlignBottomWith) != positions.end())
			{
				bottom = positions[constraints.AlignBottomWith].bottom - margin.Bottom;
				bottomSet = true;
			}
			
			// 相对位置关系
			if (constraints.LeftOf && positions.find(constraints.LeftOf) != positions.end())
			{
				right = positions[constraints.LeftOf].left - margin.Right;
				rightSet = true;
			}
			if (constraints.RightOf && positions.find(constraints.RightOf) != positions.end())
			{
				left = positions[constraints.RightOf].right + margin.Left;
				leftSet = true;
			}
			if (constraints.Above && positions.find(constraints.Above) != positions.end())
			{
				bottom = positions[constraints.Above].top - margin.Bottom;
				bottomSet = true;
			}
			if (constraints.Below && positions.find(constraints.Below) != positions.end())
			{
				top = positions[constraints.Below].bottom + margin.Top;
				topSet = true;
			}
			
			// 居中
			if (constraints.CenterHorizontal && !leftSet && !rightSet)
			{
				float availableWidth = containerWidth - margin.Left - margin.Right;
				if (availableWidth < 0) availableWidth = 0;
				left = originX + margin.Left + (availableWidth - childSize.width) / 2.0f;
				leftSet = true;
			}
			if (constraints.CenterVertical && !topSet && !bottomSet)
			{
				float availableHeight = containerHeight - margin.Top - margin.Bottom;
				if (availableHeight < 0) availableHeight = 0;
				top = originY + margin.Top + (availableHeight - childSize.height) / 2.0f;
				topSet = true;
			}
		}
		
		// 如果没有设置位置，使用默认位置
		if (!leftSet && !rightSet)
		{
			left = originX + margin.Left;
			leftSet = true;
		}
		if (!topSet && !bottomSet)
		{
			top = originY + margin.Top;
			topSet = true;
		}
		
		// 计算最终位置和尺寸
		float finalLeft = left;
		float finalTop = top;
		float finalWidth = childSize.width;
		float finalHeight = childSize.height;
		
		// 如果同时设置了左右或上下，则拉伸
		if (leftSet && rightSet)
		{
			finalWidth = right - left;
			if (finalWidth < 0) finalWidth = 0;
		}
		else if (rightSet)
		{
			finalLeft = right - finalWidth;
		}
		
		if (topSet && bottomSet)
		{
			finalHeight = bottom - top;
			if (finalHeight < 0) finalHeight = 0;
		}
		else if (bottomSet)
		{
			finalTop = bottom - finalHeight;
		}
		
		// 保存位置
		D2D1_RECT_F rect = {
			finalLeft,
			finalTop,
			finalLeft + finalWidth,
			finalTop + finalHeight
		};
		positions[child] = rect;
		
		child->ApplyLayout(cui::core::Rect{
			finalLeft, finalTop, finalWidth, finalHeight });
	}
	
	_needsLayout = false;
}

// RelativePanel 实现

RelativePanel::RelativePanel()
{
	_relativeEngine = new RelativeLayoutEngine();
	SetLayoutEngine(_relativeEngine);
}

RelativePanel::RelativePanel(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	_relativeEngine = new RelativeLayoutEngine();
	SetLayoutEngine(_relativeEngine);
}

RelativePanel::~RelativePanel()
{
	// _relativeEngine 会被 Panel 的析构函数通过 _layoutEngine 删除
}
