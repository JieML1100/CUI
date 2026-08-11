#pragma once
#include "Control.h"
#include "TextAlignment.h"

/** WPF TextBlock line-breaking policy. */
enum class TextWrapping : uint8_t
{
	NoWrap,
	Wrap,
	WrapWithOverflow
};

/** WPF TextBlock overflow indication policy. */
enum class TextTrimming : uint8_t
{
	None,
	CharacterEllipsis,
	WordEllipsis
};

/**
 * @file Label.h
 * @brief Label：TextBlock 的原生实现（只读文本）。
 *
 * 测量与绘制共享同一套受约束的 DirectWrite 布局。无显式 Width/Height
 * 时由文本贡献 DesiredSize；父级约束、换行与裁剪决定最终 RenderSize。
 */
class Label : public Control
{
private:
	static const DependencyPropertyMetadataRegistration&
		ForegroundPropertyMetadataRelation();
	static const DependencyPropertyMetadataRegistration&
		BackgroundPropertyMetadataRelation();
	::TextAlignment _textAlignment = TextAlignment::Left;
	::TextWrapping _textWrapping = TextWrapping::NoWrap;
	::TextTrimming _textTrimming = TextTrimming::None;

	Microsoft::WRL::ComPtr<IDWriteTextLayout> _formattedText;
	std::wstring _formattedTextValue;
	IDWriteTextFormat* _formattedTextFormat = nullptr;
	std::wstring _formattedTextFontFamily;
	float _formattedTextFontSize = 0.0f;
	cui::core::Size _formattedTextBounds{
		-1.0f, -1.0f };
	::TextAlignment _formattedTextAlignment = TextAlignment::Left;
	::TextWrapping _formattedTextWrapping = TextWrapping::NoWrap;
	::TextTrimming _formattedTextTrimming = TextTrimming::None;
	bool _formattedTextForMeasure = false;

	void InvalidateFormattedText() noexcept;
	IDWriteTextLayout* EnsureFormattedText(
		cui::core::Size bounds,
		bool forMeasure);

protected:
	const DependencyPropertyMetadata* ResolveExactDependencyPropertyMetadata(
		const DependencyProperty& property) const override;
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Text, L"TextBlock");
	}
	void VisitDeclaredInheritedProperties(
		void* context, InheritedPropertyVisitor visitor) const override;
public:
	PROPERTY(std::wstring, Text);
	GET(std::wstring, Text);
	SET(std::wstring, Text);
	PROPERTY(::TextAlignment, TextAlignment);
	GET(::TextAlignment, TextAlignment);
	SET(::TextAlignment, TextAlignment);
	PROPERTY(::TextWrapping, TextWrapping);
	GET(::TextWrapping, TextWrapping);
	SET(::TextWrapping, TextWrapping);
	PROPERTY(::TextTrimming, TextTrimming);
	GET(::TextTrimming, TextTrimming);
	SET(::TextTrimming, TextTrimming);
	PROPERTY(cui::drawing::Brush, Background);
	GET(cui::drawing::Brush, Background);
	SET(cui::drawing::Brush, Background);
	virtual UIClass Type();
	/** WPF TextBlock.Text property identity. */
	static const DependencyProperty& TextProperty();
	static const DependencyProperty& ForegroundProperty();
	static const DependencyProperty& BackgroundProperty();
	static const DependencyProperty& TextAlignmentProperty();
	static const DependencyProperty& TextWrappingProperty();
	static const DependencyProperty& TextTrimmingProperty();
	static void RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	void EnsureBindingPropertiesRegistered() override
	{
		RegisterDependencyProperties();
	}
#endif
	std::wstring GetSemanticText() const override;
	/** @brief 创建 Label。 */
	Label();
	cui::core::Size MeasureCore(const cui::core::Constraints& available) override;
protected:
	void OnRender() override;
};
