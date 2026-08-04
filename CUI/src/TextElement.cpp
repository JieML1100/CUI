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

#if CUI_ENABLE_DESIGN_METADATA
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
#endif
}

const DependencyProperty& TextElement::ForegroundProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::MakeSolidColorBrush(
			D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 1.0f });
		options.Flags = DependencyPropertyFlags::Inherits
			| DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertTextBrush;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextBrushDesign(21, L"Foreground");
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"Foreground"),
				std::move(options));
	}();
	return *registration;
}

const DependencyProperty& TextElement::BackgroundProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<TextElement, cui::drawing::Brush> options;
		options.DefaultValue = cui::drawing::NoBrush();
		options.Flags = DependencyPropertyFlags::AffectsRender;
		options.Convert = ConvertTextBrush;
		CUI_DESIGN_METADATA_ONLY(
		options.Design = TextBrushDesign(10, L"Background");
		)
		return DependencyPropertyRegistry::RegisterStatic<
			TextElement, cui::drawing::Brush>(
				DependencyPropertyRegistrationLiteral(L"Background"),
				std::move(options));
	}();
	return *registration;
}

void TextElement::RegisterDependencyProperties()
{
#if CUI_ENABLE_DYNAMIC_XAML
	(void)ForegroundProperty();
	(void)BackgroundProperty();
#endif
}
