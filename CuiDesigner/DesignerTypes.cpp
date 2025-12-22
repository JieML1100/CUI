#include "DesignerTypes.h"

DesignerControl::ResizeHandle DesignerControl::HitTestHandle(POINT pt, int handleSize)
{
	if (!ControlInstance) return ResizeHandle::None;
	
	auto rects = GetHandleRects(handleSize);
	int idx = 0;
	for (const auto& rect : rects)
	{
		if (pt.x >= rect.left && pt.x <= rect.right &&
			pt.y >= rect.top && pt.y <= rect.bottom)
		{
			return static_cast<ResizeHandle>(idx + 1);
		}
		idx++;
	}
	return ResizeHandle::None;
}

std::vector<RECT> DesignerControl::GetHandleRects(int handleSize)
{
	std::vector<RECT> rects;
	if (!ControlInstance) return rects;
	
	auto loc = ControlInstance->Location;
	auto size = ControlInstance->Size;
	int half = handleSize / 2;
	
	// TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left
	rects.push_back({loc.x - half, loc.y - half, loc.x + half, loc.y + half});
	rects.push_back({loc.x + size.cx / 2 - half, loc.y - half, loc.x + size.cx / 2 + half, loc.y + half});
	rects.push_back({loc.x + size.cx - half, loc.y - half, loc.x + size.cx + half, loc.y + half});
	rects.push_back({loc.x + size.cx - half, loc.y + size.cy / 2 - half, loc.x + size.cx + half, loc.y + size.cy / 2 + half});
	rects.push_back({loc.x + size.cx - half, loc.y + size.cy - half, loc.x + size.cx + half, loc.y + size.cy + half});
	rects.push_back({loc.x + size.cx / 2 - half, loc.y + size.cy - half, loc.x + size.cx / 2 + half, loc.y + size.cy + half});
	rects.push_back({loc.x - half, loc.y + size.cy - half, loc.x + half, loc.y + size.cy + half});
	rects.push_back({loc.x - half, loc.y + size.cy / 2 - half, loc.x + half, loc.y + size.cy / 2 + half});
	
	return rects;
}
