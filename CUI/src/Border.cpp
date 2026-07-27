#include "Border.h"
#include "Panel.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	std::optional<cui::drawing::Brush> ConvertBorderBrush(
		const BindingValue& value)
	{
		cui::drawing::Brush brush;
		if (value.TryGet(brush)) return brush;
		D2D1_COLOR_F color{};
		if (value.TryGet(color))
			return cui::drawing::MakeSolidColorBrush(color);
		return std::nullopt;
	}

	DependencyPropertyDesignMetadata BorderBrushDesign(
		int order,
		const wchar_t* displayName)
	{
		DependencyPropertyDesignMetadata design;
		design.Browsable = false;
		design.DisplayName = displayName;
		design.Category = L"Appearance";
		design.CategoryOrder = 200;
		design.Order = order;
		design.Editor = DependencyPropertyEditorKind::Text;
		design.Persistence = DependencyPropertyPersistence::Metadata;
		return design;
	}

	bool IsFiniteNonNegativeThickness(const Thickness& value) noexcept
	{
		return std::isfinite(value.Left) && value.Left >= 0.0f
			&& std::isfinite(value.Top) && value.Top >= 0.0f
			&& std::isfinite(value.Right) && value.Right >= 0.0f
			&& std::isfinite(value.Bottom) && value.Bottom >= 0.0f;
	}
}

const DependencyProperty& Border::BorderBrushProperty()
{
	static const auto* property = []
	{
		DependencyPropertyOptions<Border, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::NoBrush();
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertBorderBrush;
		options.Design = BorderBrushDesign(30, L"BorderBrush");
		return DependencyPropertyRegistry::Register<
			Border, cui::drawing::Brush>(
				L"BorderBrush", std::move(options));
	}();
	return *property;
}

const DependencyProperty& Border::BackgroundProperty()
{
	static const auto* property = []
	{
		DependencyPropertyOptions<Border, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::NoBrush();
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertBorderBrush;
		options.Design = BorderBrushDesign(10, L"Background");
		return DependencyPropertyRegistry::AddOwner<
			Border, cui::drawing::Brush>(
				Panel::BackgroundProperty(), std::move(options));
	}();
	return *property;
}

const DependencyProperty& Border::BorderThicknessProperty()
{
	static const auto* property = []
	{
		DependencyPropertyOptions<Border, Thickness> options;
		options.DefaultValue = Thickness{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Validate = IsFiniteNonNegativeThickness;
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = 40;
		options.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		options.Design.Editor =
			DependencyPropertyEditorKind::Thickness;
		return DependencyPropertyRegistry::Register<Border, Thickness>(
			L"BorderThickness", std::move(options));
	}();
	return *property;
}

const DependencyProperty& Border::PaddingProperty()
{
	static const auto* property = []
	{
		DependencyPropertyOptions<Border, Thickness> options;
		options.DefaultValue = Thickness{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Validate = IsFiniteNonNegativeThickness;
		options.Design.Category = L"Layout";
		options.Design.CategoryOrder = 100;
		options.Design.Order = 80;
		options.Design.Persistence =
			DependencyPropertyPersistence::Native;
		options.Design.Editor =
			DependencyPropertyEditorKind::Thickness;
		return DependencyPropertyRegistry::Register<Border, Thickness>(
			L"Padding", std::move(options));
	}();
	return *property;
}

void Border::RegisterDependencyProperties()
{
	Decorator::RegisterDependencyProperties();
	(void)BorderBrushProperty();
	(void)BackgroundProperty();
	(void)BorderThicknessProperty();
	(void)PaddingProperty();
}

cui::core::Insets Border::GetDecoratorInsets() const noexcept
{
	const auto padding = GetSpecifiedLayout().padding;
	const auto borderThickness = const_cast<Border*>(this)->BorderThickness;
	return {
		padding.left + borderThickness.Left,
		padding.top + borderThickness.Top,
		padding.right + borderThickness.Right,
		padding.bottom + borderThickness.Bottom };
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
		const auto borderThickness = BorderThickness;
		const float left = (std::min)(borderThickness.Left, width);
		const float top = (std::min)(borderThickness.Top, height);
		const float right = (std::min)(borderThickness.Right, width);
		const float bottom = (std::min)(borderThickness.Bottom, height);
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
