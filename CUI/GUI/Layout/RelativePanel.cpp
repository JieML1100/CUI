#include "RelativePanel.h"
#include "../Form.h"
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
		const RelativeConstraints& c = it->second;
		
		// 检查所有依赖的控件
		if (c.AlignLeftWith && HasCycle(start, c.AlignLeftWith, visited)) return true;
		if (c.AlignRightWith && HasCycle(start, c.AlignRightWith, visited)) return true;
		if (c.AlignTopWith && HasCycle(start, c.AlignTopWith, visited)) return true;
		if (c.AlignBottomWith && HasCycle(start, c.AlignBottomWith, visited)) return true;
		if (c.LeftOf && HasCycle(start, c.LeftOf, visited)) return true;
		if (c.RightOf && HasCycle(start, c.RightOf, visited)) return true;
		if (c.Above && HasCycle(start, c.Above, visited)) return true;
		if (c.Below && HasCycle(start, c.Below, visited)) return true;
	}
	
	visited[current] = 2; // 标记为已访问完成
	return false;
}

std::vector<Control*> RelativeLayoutEngine::TopologicalSort(Control* container)
{
	std::vector<Control*> result;
	if (!container) return result;
	
	std::set<Control*> processed;
	std::map<Control*, int> visited; // 0=未访问, 1=正在访问, 2=已完成
	
	// 初始化
	for (int i = 0; i < container->Count; i++)
	{
		auto child = container->operator[](i);
		if (child && child->Visible)
			visited[child] = 0;
	}
	
	// 简化版：按依赖深度排序
	// 没有依赖的控件先排列，有依赖的后排列
	std::vector<Control*> noDeps;
	std::vector<Control*> hasDeps;
	
	for (int i = 0; i < container->Count; i++)
	{
		auto child = container->operator[](i);
		if (!child || !child->Visible) continue;
		
		auto it = _constraints.find(child);
		bool hasDependency = false;
		
		if (it != _constraints.end())
		{
			const RelativeConstraints& c = it->second;
			if (c.AlignLeftWith || c.AlignRightWith || c.AlignTopWith || c.AlignBottomWith ||
				c.LeftOf || c.RightOf || c.Above || c.Below)
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
	for (auto c : noDeps)
		result.push_back(c);
	
	// 再添加有依赖的
	for (auto c : hasDeps)
		result.push_back(c);
	
	return result;
}

SIZE RelativeLayoutEngine::Measure(Control* container, SIZE availableSize)
{
	if (!container) return {0, 0};
	
	// 简单测量：返回容器期望的最大尺寸
	SIZE desiredSize = {0, 0};
	
	for (int i = 0; i < container->Count; i++)
	{
		auto child = container->operator[](i);
		if (!child || !child->Visible) continue;
		
		SIZE childSize = child->MeasureCore(availableSize);
		Thickness margin = child->Margin;
		
		LONG totalWidth = childSize.cx + (LONG)(margin.Left + margin.Right);
		LONG totalHeight = childSize.cy + (LONG)(margin.Top + margin.Bottom);
		
		if (totalWidth > desiredSize.cx)
			desiredSize.cx = totalWidth;
		if (totalHeight > desiredSize.cy)
			desiredSize.cy = totalHeight;
	}
	
	_needsLayout = false;
	return desiredSize;
}

void RelativeLayoutEngine::Arrange(Control* container, D2D1_RECT_F finalRect)
{
	if (!container) return;
	
	const float originX = finalRect.left;
	const float originY = finalRect.top;
	float containerWidth = finalRect.right - finalRect.left;
	float containerHeight = finalRect.bottom - finalRect.top;
	
	// 拓扑排序
	std::vector<Control*> sorted = TopologicalSort(container);
	
	// 存储每个控件的计算位置
	std::map<Control*, D2D1_RECT_F> positions;
	
	// 遍历排序后的控件
	for (auto child : sorted)
	{
		if (!child) continue;
		
		SIZE childSize = child->Size;
		Thickness margin = child->Margin;
		
		float left = 0.0f, top = 0.0f, right = 0.0f, bottom = 0.0f;
		bool leftSet = false, topSet = false, rightSet = false, bottomSet = false;
		
		auto it = _constraints.find(child);
		if (it != _constraints.end())
		{
			const RelativeConstraints& c = it->second;
			
			// 相对于面板对齐
			if (c.AlignLeftWithPanel)
			{
				left = originX + margin.Left;
				leftSet = true;
			}
			if (c.AlignRightWithPanel)
			{
				right = originX + containerWidth - margin.Right;
				rightSet = true;
			}
			if (c.AlignTopWithPanel)
			{
				top = originY + margin.Top;
				topSet = true;
			}
			if (c.AlignBottomWithPanel)
			{
				bottom = originY + containerHeight - margin.Bottom;
				bottomSet = true;
			}
			
			// 相对于其他控件对齐
			if (c.AlignLeftWith && positions.find(c.AlignLeftWith) != positions.end())
			{
				left = positions[c.AlignLeftWith].left + margin.Left;
				leftSet = true;
			}
			if (c.AlignRightWith && positions.find(c.AlignRightWith) != positions.end())
			{
				right = positions[c.AlignRightWith].right - margin.Right;
				rightSet = true;
			}
			if (c.AlignTopWith && positions.find(c.AlignTopWith) != positions.end())
			{
				top = positions[c.AlignTopWith].top + margin.Top;
				topSet = true;
			}
			if (c.AlignBottomWith && positions.find(c.AlignBottomWith) != positions.end())
			{
				bottom = positions[c.AlignBottomWith].bottom - margin.Bottom;
				bottomSet = true;
			}
			
			// 相对位置关系
			if (c.LeftOf && positions.find(c.LeftOf) != positions.end())
			{
				right = positions[c.LeftOf].left - margin.Right;
				rightSet = true;
			}
			if (c.RightOf && positions.find(c.RightOf) != positions.end())
			{
				left = positions[c.RightOf].right + margin.Left;
				leftSet = true;
			}
			if (c.Above && positions.find(c.Above) != positions.end())
			{
				bottom = positions[c.Above].top - margin.Bottom;
				bottomSet = true;
			}
			if (c.Below && positions.find(c.Below) != positions.end())
			{
				top = positions[c.Below].bottom + margin.Top;
				topSet = true;
			}
			
			// 居中
			if (c.CenterHorizontal && !leftSet && !rightSet)
			{
				left = originX + (containerWidth - childSize.cx) / 2.0f;
				leftSet = true;
			}
			if (c.CenterVertical && !topSet && !bottomSet)
			{
				top = originY + (containerHeight - childSize.cy) / 2.0f;
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
		float finalWidth = (float)childSize.cx;
		float finalHeight = (float)childSize.cy;
		
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
		
		// 应用布局
		POINT loc = { (LONG)finalLeft, (LONG)finalTop };
		SIZE size = { (LONG)finalWidth, (LONG)finalHeight };
		child->ApplyLayout(loc, size);
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
