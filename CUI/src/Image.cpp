#include "Image.h"
#include "Window.h"
#include <algorithm>

UIClass Image::Type() { return UIClass::UI_Image; }

const DependencyProperty& Image::SourceProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<Image,
			std::shared_ptr<BitmapSource>> options;
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Equals = [](
			const std::shared_ptr<BitmapSource>& left,
			const std::shared_ptr<BitmapSource>& right)
			{ return left == right; };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<Image,
			std::shared_ptr<BitmapSource>>(
				DependencyPropertyRegistrationLiteral(L"Source"),
				[](Image& target) { return target.Source; },
				[](Image& target, const std::shared_ptr<BitmapSource>& value)
				{ target.Source = value; }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& Image::StretchProperty()
{
	static const auto registration = []
	{
		DependencyPropertyOptions<Image,
			cui::drawing::ImageBrushStretch> options;
		options.DefaultValue = cui::drawing::ImageBrushStretch::Uniform;
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Appearance";
		options.Design.CategoryOrder = 200;
		options.Design.Order = 20;
		options.Design.Editor = DependencyPropertyEditorKind::Choice;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		options.Design.Choices = {
			{ L"None", BindingValue(cui::drawing::ImageBrushStretch::None) },
			{ L"Fill", BindingValue(cui::drawing::ImageBrushStretch::Fill) },
			{ L"Uniform", BindingValue(
				cui::drawing::ImageBrushStretch::Uniform) },
			{ L"UniformToFill", BindingValue(
				cui::drawing::ImageBrushStretch::UniformToFill) }
		};
		)
		return DependencyPropertyRegistry::RegisterStatic<Image,
			cui::drawing::ImageBrushStretch>(
				DependencyPropertyRegistrationLiteral(L"Stretch"),
				[](Image& target) { return target.Stretch; },
				[](Image& target,
					const cui::drawing::ImageBrushStretch& value)
				{ target.Stretch = value; }, {}, std::move(options));
	}();
	return *registration;
}

void Image::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)SourceProperty();
	(void)StretchProperty();
#endif
}

GET_CPP(Image, std::shared_ptr<BitmapSource>, Source)
{
	return _source;
}

SET_CPP(Image, std::shared_ptr<BitmapSource>, Source)
{
	if (!SetPropertyField(SourceProperty(), _source, std::move(value))) return;
	_sourceCache.Reset();
	_sourceCacheTarget = nullptr;
}

GET_CPP(Image, cui::drawing::ImageBrushStretch, Stretch)
{
	return _stretch;
}

SET_CPP(Image, cui::drawing::ImageBrushStretch, Stretch)
{
	(void)SetPropertyField(StretchProperty(), _stretch, value);
}

Image::Image()
{
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
}

cui::core::Size Image::MeasureCore(
	const cui::core::Constraints& available)
{
	(void)available;
	if (!_source) return {};
	const auto pixels = _source->GetPixelSize();
	const auto dpiX = _source->GetDpiX() > 0.0f ? _source->GetDpiX() : 96.0f;
	const auto dpiY = _source->GetDpiY() > 0.0f ? _source->GetDpiY() : 96.0f;
	return { static_cast<float>(pixels.width) * 96.0f / dpiX,
		static_cast<float>(pixels.height) * 96.0f / dpiY };
}

ID2D1Bitmap* Image::EnsureSourceCache()
{
	if (!_source || !GetPresentationWindow() || !GetDrawingContext()) return nullptr;
	auto* target = GetDrawingContext()->GetRenderTargetRaw();
	if (!target) return nullptr;
	if (_sourceCache && _sourceCacheTarget == target)
		return _sourceCache.Get();
	_sourceCache.Reset();
	_sourceCacheTarget = target;
	_sourceCache.Attach(GetDrawingContext()->CreateBitmap(_source));
	return _sourceCache.Get();
}

void Image::RenderSource()
{
	auto* bitmap = EnsureSourceCache();
	if (!bitmap || !GetPresentationWindow() || !GetDrawingContext()) return;
	const auto sourceSize = bitmap->GetSize();
	const auto targetSize = GetActualSizeDip();
	if (sourceSize.width <= 0.0f || sourceSize.height <= 0.0f
		|| targetSize.width <= 0.0f || targetSize.height <= 0.0f) return;

	float width = sourceSize.width;
	float height = sourceSize.height;
	if (_stretch == cui::drawing::ImageBrushStretch::Fill)
	{
		width = targetSize.width;
		height = targetSize.height;
	}
	else if (_stretch == cui::drawing::ImageBrushStretch::Uniform
		|| _stretch == cui::drawing::ImageBrushStretch::UniformToFill)
	{
		const float scaleX = targetSize.width / sourceSize.width;
		const float scaleY = targetSize.height / sourceSize.height;
		const float scale = _stretch
			== cui::drawing::ImageBrushStretch::Uniform
			? (std::min)(scaleX, scaleY) : (std::max)(scaleX, scaleY);
		width *= scale;
		height *= scale;
	}
	GetDrawingContext()->DrawBitmap(bitmap,
		(targetSize.width - width) * 0.5f,
		(targetSize.height - height) * 0.5f,
		width, height);
}

void Image::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	this->BeginRender();
	RenderSource();
	this->EndRender();
}

void Image::NotifyDeviceResourcesInvalidated() noexcept
{
	_sourceCache.Reset();
	_sourceCacheTarget = nullptr;
	Control::NotifyDeviceResourcesInvalidated();
}
