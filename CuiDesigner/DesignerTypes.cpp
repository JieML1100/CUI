#include "DesignerTypes.h"

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
	
	auto location = ControlInstance->ActualLocation;
	auto size = ControlInstance->Size;
	int half = handleSize / 2;
	
	// TopLeft, Top, TopRight, Right, BottomRight, Bottom, BottomLeft, Left
	rects.push_back({location.x - half, location.y - half, location.x + half, location.y + half});
	rects.push_back({location.x + size.cx / 2 - half, location.y - half, location.x + size.cx / 2 + half, location.y + half});
	rects.push_back({location.x + size.cx - half, location.y - half, location.x + size.cx + half, location.y + half});
	rects.push_back({location.x + size.cx - half, location.y + size.cy / 2 - half, location.x + size.cx + half, location.y + size.cy / 2 + half});
	rects.push_back({location.x + size.cx - half, location.y + size.cy - half, location.x + size.cx + half, location.y + size.cy + half});
	rects.push_back({location.x + size.cx / 2 - half, location.y + size.cy - half, location.x + size.cx / 2 + half, location.y + size.cy + half});
	rects.push_back({location.x - half, location.y + size.cy - half, location.x + half, location.y + size.cy + half});
	rects.push_back({location.x - half, location.y + size.cy / 2 - half, location.x + half, location.y + size.cy / 2 + half});
	
	return rects;
}
