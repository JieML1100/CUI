#pragma once

#include "../Control.h"
#include "LayoutEngine.h"

#include <algorithm>
#include <cmath>
#include <span>

namespace cui::layout
{
	/** Measures children that share one content slot without implying Grid/Panel semantics. */
	inline core::Size MeasureOverlayChildren(
		std::span<Control* const> children,
		const core::Constraints& available,
		core::Insets padding = {})
	{
		const auto contentConstraints = available.Deflate(padding);
		core::Size desired{};
		for (auto* child : children)
		{
			if (!child || child->IsCollapsed()) continue;
			const auto margin = child->GetSpecifiedLayout().margin;
			const auto childDesired = child->Measure(
				contentConstraints.Deflate(margin));
			desired.width = (std::max)(desired.width,
				childDesired.width + margin.Horizontal());
			desired.height = (std::max)(desired.height,
				childDesired.height + margin.Vertical());
		}
		return {
			desired.width + padding.Horizontal(),
			desired.height + padding.Vertical()
		};
	}

	/** Arranges overlay/content children in one slot while honoring margin/alignment. */
	inline void ArrangeOverlayChildren(
		std::span<Control* const> children,
		core::Rect slot)
	{
		slot = slot.Normalized();
		for (auto* child : children)
		{
			if (!child || child->IsCollapsed()) continue;
			const auto margin = child->GetSpecifiedLayout().margin;
			const core::Rect content{
				slot.x + margin.left,
				slot.y + margin.top,
				(std::max)(0.0f,
					slot.width - margin.Horizontal()),
				(std::max)(0.0f,
					slot.height - margin.Vertical())
			};
			const auto desired = child->Measure(
				core::Constraints{ content.Extent() });
			const auto horizontalAlignment =
				ResolveHorizontalArrangeAlignment(*child);
			const auto verticalAlignment =
				ResolveVerticalArrangeAlignment(*child);

			float width = (std::min)(desired.width, content.width);
			float height = (std::min)(desired.height, content.height);
			float x = content.x;
			float y = content.y;
			if (horizontalAlignment == HorizontalAlignment::Stretch)
				width = content.width;
			else if (horizontalAlignment == HorizontalAlignment::Center)
				x += (content.width - width) * 0.5f;
			else if (horizontalAlignment == HorizontalAlignment::Right)
				x += content.width - width;
			if (verticalAlignment == VerticalAlignment::Stretch)
				height = content.height;
			else if (verticalAlignment == VerticalAlignment::Center)
				y += (content.height - height) * 0.5f;
			else if (verticalAlignment == VerticalAlignment::Bottom)
				y += content.height - height;

			child->Arrange(core::Rect{ x, y, width, height });
		}
	}

}
