#include "Brush.h"

#include <BitmapSource.h>
#include <Graphics.h>
#include <algorithm>
#include <cmath>
#include <wrl/client.h>

namespace cui::drawing
{
namespace
{
	float FiniteOr(float value, float fallback)
	{
		return std::isfinite(value) ? value : fallback;
	}

	D2D1_POINT_2F ResolvePoint(
		D2D1_POINT_2F point,
		BrushMappingMode mode,
		D2D1_SIZE_F bounds)
	{
		point.x = FiniteOr(point.x, 0.0f);
		point.y = FiniteOr(point.y, 0.0f);
		if (mode == BrushMappingMode::RelativeToBoundingBox)
		{
			point.x *= (std::max)(0.0f, bounds.width);
			point.y *= (std::max)(0.0f, bounds.height);
		}
		return point;
	}

	std::vector<D2D1_GRADIENT_STOP> ResolveStops(
		const std::vector<GradientStop>& source)
	{
		std::vector<D2D1_GRADIENT_STOP> result;
		result.reserve(source.size());
		for (const auto& stop : source)
		{
			result.push_back(D2D1_GRADIENT_STOP{
				(std::clamp)(FiniteOr(stop.Offset, 0.0f), 0.0f, 1.0f),
				stop.Color });
		}
		std::stable_sort(result.begin(), result.end(),
			[](const auto& left, const auto& right)
			{ return left.position < right.position; });
		return result;
	}
}

ID2D1Brush* Brush::CreateBrush(
	D2DGraphics& graphics,
	D2D1_SIZE_F bounds) const
{
	ID2D1Brush* result = nullptr;
	if (Kind == BrushKind::Solid)
	{
		result = graphics.CreateSolidColorBrush(Color);
	}
	else if (Kind == BrushKind::Image)
	{
		if (!ImageSource) return nullptr;
		Microsoft::WRL::ComPtr<ID2D1Bitmap> sourceBitmap;
		sourceBitmap.Attach(graphics.CreateBitmap(ImageSource));
		if (!sourceBitmap) return nullptr;

		const auto source = sourceBitmap->GetSize();
		const float sourceWidth = (std::max)(source.width, 0.0001f);
		const float sourceHeight = (std::max)(source.height, 0.0001f);
		const float targetWidth = (std::max)(0.0f, FiniteOr(bounds.width, 0.0f));
		const float targetHeight = (std::max)(0.0f, FiniteOr(bounds.height, 0.0f));
		if (targetWidth <= 0.0f || targetHeight <= 0.0f) return nullptr;
		float scaleX = 1.0f;
		float scaleY = 1.0f;
		if (Stretch == ImageBrushStretch::Fill)
		{
			scaleX = targetWidth / sourceWidth;
			scaleY = targetHeight / sourceHeight;
		}
		else if (Stretch == ImageBrushStretch::Uniform
			|| Stretch == ImageBrushStretch::UniformToFill)
		{
			const float uniform = Stretch == ImageBrushStretch::Uniform
				? (std::min)(targetWidth / sourceWidth, targetHeight / sourceHeight)
				: (std::max)(targetWidth / sourceWidth, targetHeight / sourceHeight);
			scaleX = scaleY = uniform;
		}
		scaleX = (std::max)(0.0001f, FiniteOr(scaleX, 1.0f));
		scaleY = (std::max)(0.0001f, FiniteOr(scaleY, 1.0f));
		const float renderedWidth = sourceWidth * scaleX;
		const float renderedHeight = sourceHeight * scaleY;
		const float remainingX = targetWidth - renderedWidth;
		const float remainingY = targetHeight - renderedHeight;
		const float offsetX = AlignmentX == ImageBrushAlignmentX::Left ? 0.0f
			: AlignmentX == ImageBrushAlignmentX::Right ? remainingX
			: remainingX * 0.5f;
		const float offsetY = AlignmentY == ImageBrushAlignmentY::Top ? 0.0f
			: AlignmentY == ImageBrushAlignmentY::Bottom ? remainingY
			: remainingY * 0.5f;

		// Compose into a target-sized transparent bitmap. A transformed bitmap
		// brush with CLAMP would smear its edge pixels into the unused band for
		// None/Uniform; the intermediate surface preserves WPF-like transparency.
		Microsoft::WRL::ComPtr<ID2D1BitmapRenderTarget> imageTarget;
		if (!graphics.GetRenderTargetRaw()
			|| FAILED(graphics.GetRenderTargetRaw()->CreateCompatibleRenderTarget(
				D2D1::SizeF(targetWidth, targetHeight), &imageTarget))
			|| !imageTarget) return nullptr;
		imageTarget->BeginDraw();
		imageTarget->Clear(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f });
		imageTarget->DrawBitmap(
			sourceBitmap.Get(),
			D2D1::RectF(offsetX, offsetY,
				offsetX + renderedWidth, offsetY + renderedHeight),
			1.0f,
			D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
			D2D1::RectF(0.0f, 0.0f, sourceWidth, sourceHeight));
		if (FAILED(imageTarget->EndDraw())) return nullptr;
		Microsoft::WRL::ComPtr<ID2D1Bitmap> composedBitmap;
		if (FAILED(imageTarget->GetBitmap(&composedBitmap)) || !composedBitmap)
			return nullptr;

		Microsoft::WRL::ComPtr<ID2D1BitmapBrush> brush;
		brush.Attach(graphics.CreateBitmapBrush(composedBitmap.Get()));
		if (!brush) return nullptr;
		brush->SetExtendModeX(D2D1_EXTEND_MODE_CLAMP);
		brush->SetExtendModeY(D2D1_EXTEND_MODE_CLAMP);
		brush->SetInterpolationMode(D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
		result = brush.Detach();
	}
	else
	{
		auto stops = ResolveStops(GradientStops);
		if (stops.empty()) return nullptr;
		if (Kind == BrushKind::LinearGradient)
		{
			auto* brush = graphics.CreateLinearGradientBrush(
				stops.data(), static_cast<unsigned int>(stops.size()));
			if (!brush) return nullptr;
			brush->SetStartPoint(ResolvePoint(
				StartPoint, MappingMode, bounds));
			brush->SetEndPoint(ResolvePoint(
				EndPoint, MappingMode, bounds));
			result = brush;
		}
		else if (Kind == BrushKind::RadialGradient)
		{
			const auto center = ResolvePoint(Center, MappingMode, bounds);
			auto* brush = graphics.CreateRadialGradientBrush(
				stops.data(), static_cast<unsigned int>(stops.size()), center);
			if (!brush) return nullptr;
			const auto origin = ResolvePoint(
				GradientOrigin, MappingMode, bounds);
			brush->SetGradientOriginOffset(D2D1::Point2F(
				origin.x - center.x, origin.y - center.y));
			brush->SetRadiusX((std::max)(0.0f, FiniteOr(RadiusX, 0.5f))
				* (MappingMode == BrushMappingMode::RelativeToBoundingBox
					? (std::max)(0.0f, bounds.width) : 1.0f));
			brush->SetRadiusY((std::max)(0.0f, FiniteOr(RadiusY, 0.5f))
				* (MappingMode == BrushMappingMode::RelativeToBoundingBox
					? (std::max)(0.0f, bounds.height) : 1.0f));
			result = brush;
		}
	}
	if (result)
		result->SetOpacity((std::clamp)(FiniteOr(Opacity, 1.0f), 0.0f, 1.0f));
	return result;
}
}
