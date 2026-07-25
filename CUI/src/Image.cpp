#include "Image.h"
#include "Window.h"
#include <algorithm>
UIClass Image::Type() { return UIClass::UI_Image; }

void Image::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		DependencyPropertyOptions<Image,
			std::shared_ptr<BitmapSource>> sourceOptions;
		sourceOptions.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		sourceOptions.Equals = [](
			const std::shared_ptr<BitmapSource>& left,
			const std::shared_ptr<BitmapSource>& right)
			{ return left == right; };
		sourceOptions.Design.Category = L"Appearance";
		sourceOptions.Design.CategoryOrder = 200;
		sourceOptions.Design.Order = 10;
		sourceOptions.Design.Editor = DependencyPropertyEditorKind::Text;
		sourceOptions.Design.Persistence = DependencyPropertyPersistence::Metadata;
		DependencyPropertyRegistry::Register<Image,
			std::shared_ptr<BitmapSource>>(L"Source",
			[](Image& target) { return target.Source; },
			[](Image& target, const std::shared_ptr<BitmapSource>& value)
			{ target.Source = value; }, {}, std::move(sourceOptions));

		DependencyPropertyOptions<Image,
			cui::drawing::ImageBrushStretch> stretchOptions;
		stretchOptions.DefaultValue =
			cui::drawing::ImageBrushStretch::Uniform;
		stretchOptions.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		stretchOptions.Design.Category = L"Appearance";
		stretchOptions.Design.CategoryOrder = 200;
		stretchOptions.Design.Order = 20;
		stretchOptions.Design.Editor = DependencyPropertyEditorKind::Choice;
		stretchOptions.Design.Persistence = DependencyPropertyPersistence::Native;
		stretchOptions.Design.Choices = {
			{ L"None", BindingValue(cui::drawing::ImageBrushStretch::None) },
			{ L"Fill", BindingValue(cui::drawing::ImageBrushStretch::Fill) },
			{ L"Uniform", BindingValue(cui::drawing::ImageBrushStretch::Uniform) },
			{ L"UniformToFill",
				BindingValue(cui::drawing::ImageBrushStretch::UniformToFill) }
		};
		DependencyPropertyRegistry::Register<Image,
			cui::drawing::ImageBrushStretch>(L"Stretch",
			[](Image& target) { return target.Stretch; },
			[](Image& target,
				const cui::drawing::ImageBrushStretch& value)
			{ target.Stretch = value; }, {}, std::move(stretchOptions));
		return true;
	}();
	(void)registered;
}

GET_CPP(Image, std::shared_ptr<BitmapSource>, Source)
{
	return _source;
}

SET_CPP(Image, std::shared_ptr<BitmapSource>, Source)
{
	if (!SetPropertyField(L"Source", _source, std::move(value))) return;
	_sourceCache.Reset();
	_sourceCacheTarget = nullptr;
}

GET_CPP(Image, cui::drawing::ImageBrushStretch, Stretch)
{
	return _stretch;
}

SET_CPP(Image, cui::drawing::ImageBrushStretch, Stretch)
{
	(void)SetPropertyField(L"Stretch", _stretch, value);
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
