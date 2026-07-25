#include "DesignerTypes.h"
#include <cmath>

DesignerControl::ResizeHandle DesignerControl::HitTestHandle(POINT pt, int handleSize)
{
	if (!ControlInstance) return ResizeHandle::None;
	
	auto rects = GetHandleRects(handleSize);
	int handleIndex = 0;
	for (const auto& rect : rects)
	{
		if (pt.x >= rect.left && pt.x <= rect.right &&
			pt.y >= rect.top && pt.y <= rect.bottom)
		{
			return static_cast<ResizeHandle>(handleIndex + 1);
		}
		handleIndex++;
	}
	return ResizeHandle::None;
}

std::vector<RECT> DesignerControl::GetHandleRects(int handleSize)
{
	std::vector<RECT> rects;
	if (!ControlInstance) return rects;
	
	const auto location = ControlInstance->GetActualLocationDip();
	const auto size = ControlInstance->GetActualSizeDip();
	const LONG x = static_cast<LONG>(std::lround(location.x));
	const LONG y = static_cast<LONG>(std::lround(location.y));
	const LONG width = static_cast<LONG>(std::lround(size.width));
	const LONG height = static_cast<LONG>(std::lround(size.height));
	int half = handleSize / 2;
	
	// TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left
	rects.push_back({x - half, y - half, x + half, y + half});
	rects.push_back({x + width / 2 - half, y - half, x + width / 2 + half, y + half});
	rects.push_back({x + width - half, y - half, x + width + half, y + half});
	rects.push_back({x + width - half, y + height / 2 - half, x + width + half, y + height / 2 + half});
	rects.push_back({x + width - half, y + height - half, x + width + half, y + height + half});
	rects.push_back({x + width / 2 - half, y + height - half, x + width / 2 + half, y + height + half});
	rects.push_back({x - half, y + height - half, x + half, y + height + half});
	rects.push_back({x - half, y + height / 2 - half, x + half, y + height / 2 + half});
	
	return rects;
}
