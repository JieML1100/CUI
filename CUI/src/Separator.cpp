#include "Separator.h"
#include "Window.h"

Separator::Separator()
{
	(void)TrySetPropertyValue(
		L"Height", BindingValue(cui::layout::Length::Fixed(8.0f)),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"VerticalAlignment", BindingValue(VerticalAlignment::Center),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"IsTabStop", BindingValue(false),
		DependencyPropertyValueSource::Theme);
	(void)TrySetPropertyValue(
		L"Focusable", BindingValue(false),
		DependencyPropertyValueSource::Theme);
	RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	RendererBorderColor = cui::theme::palette::Border;
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(Thickness(1.0f)),
		DependencyPropertyValueSource::Theme);
}

void Separator::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
}

void Separator::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	const auto size = GetActualSizeDip();
	BeginRender();
	const float border = BorderThickness.MaxEdge();
	if (border > 0.0f && RendererBorderColor.a > 0.0f)
	{
		if (size.width >= size.height)
			GetDrawingContext()->DrawLine(
				0.0f, size.height * 0.5f, size.width, size.height * 0.5f,
				RendererBorderColor, border);
		else
			GetDrawingContext()->DrawLine(
				size.width * 0.5f, 0.0f, size.width * 0.5f, size.height,
				RendererBorderColor, border);
	}
	EndRender();
}
