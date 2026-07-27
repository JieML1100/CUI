#include "TextElement.h"
#include "DependencyProperty.h"

#include <utility>

namespace
{
	std::optional<cui::drawing::Brush> ConvertTextBrush(
		const BindingValue& value)
	{
		cui::drawing::Brush brush;
		if (value.TryGet(brush)) return brush;
		D2D1_COLOR_F color{};
		if (value.TryGet(color))
			return cui::drawing::MakeSolidColorBrush(color);
		return std::nullopt;
	}

	DependencyPropertyDesignMetadata TextBrushDesign(
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
}

const DependencyProperty& TextElement::ForegroundProperty()
{
	static const auto* property = []
	{
		DependencyPropertyOptions<TextElement, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::MakeSolidColorBrush(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertTextBrush;
		options.Design = TextBrushDesign(21, L"Foreground");
		return DependencyPropertyRegistry::Register<
			TextElement, cui::drawing::Brush>(
				L"Foreground", std::move(options));
	}();
	return *property;
}

const DependencyProperty& TextElement::BackgroundProperty()
{
	static const auto* property = []
	{
		DependencyPropertyOptions<TextElement, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::NoBrush();
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertTextBrush;
		options.Design = TextBrushDesign(10, L"Background");
		return DependencyPropertyRegistry::Register<
			TextElement, cui::drawing::Brush>(
				L"Background", std::move(options));
	}();
	return *property;
}

void TextElement::RegisterDependencyProperties()
{
	(void)ForegroundProperty();
	(void)BackgroundProperty();
}
