#include "Border.h"

#include <algorithm>

void Border::RegisterDependencyProperties()
{
	Decorator::RegisterDependencyProperties();
}

cui::core::Insets Border::GetDecoratorInsets() const noexcept
{
	const auto padding = GetSpecifiedLayout().padding;
	return {
		padding.left + _borderThickness.Left,
		padding.top + _borderThickness.Top,
		padding.right + _borderThickness.Right,
		padding.bottom + _borderThickness.Bottom };
}

void Border::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	auto& graphics = *GetDrawingContext();
	const auto size = GetActualSizeDip();
	const float width = size.width;
	const float height = size.height;

	BeginRender();
	if (auto* background = CreateBackgroundBrush(
		graphics, D2D1_SIZE_F{ width, height }))
	{
		graphics.FillRect(0.0f, 0.0f, width, height, background);
		background->Release();
	}
	if (auto* border = CreateBorderBrush(
		graphics, D2D1_SIZE_F{ width, height }))
	{
		const float left = (std::min)(_borderThickness.Left, width);
		const float top = (std::min)(_borderThickness.Top, height);
		const float right = (std::min)(_borderThickness.Right, width);
		const float bottom = (std::min)(_borderThickness.Bottom, height);
		if (left > 0.0f)
			graphics.FillRect(0.0f, 0.0f, left, height, border);
		if (top > 0.0f)
			graphics.FillRect(0.0f, 0.0f, width, top, border);
		if (right > 0.0f)
			graphics.FillRect(
				(std::max)(0.0f, width - right), 0.0f,
				right, height, border);
		if (bottom > 0.0f)
			graphics.FillRect(
				0.0f, (std::max)(0.0f, height - bottom),
				width, bottom, border);
		border->Release();
	}
	EndRender();
}
