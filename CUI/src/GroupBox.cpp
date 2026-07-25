#include "GroupBox.h"
#include "Window.h"

#include <algorithm>
#include <cmath>
#include <utility>

UIClass GroupBox::Type() { return UIClass::UI_GroupBox; }

void GroupBox::RegisterDependencyProperties()
{
	HeaderedContentControl::RegisterDependencyProperties();
	RegisterControlBorderThicknessMetadata<GroupBox>(1.0f, 70);
}

GroupBox::GroupBox()
	: HeaderedContentControl()
{
	RegisterDependencyProperties();
	InitializeControlBorderThicknessDefault(1.0f);
	this->RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
}

float GroupBox::GetCaptionBandHeight()
{
	if (auto* header = GetHeaderVisual())
	{
		const auto desired = header->Measure(cui::core::Constraints::Unbounded());
		return (std::max)(20.0f, desired.height + _captionPaddingY * 2.0f);
	}
	auto font = this->GetRenderFont();
	if (!font) return 20.0f;
	auto textSize = font->GetTextSize(GetDisplayText());
	return (std::max)(20.0f, textSize.height + _captionPaddingY * 2.0f);
}

cui::core::Insets GroupBox::GetHeaderPresentationInsets() const noexcept
{
	return cui::core::Insets{
		_captionMarginLeft + _captionPaddingX,
		0.0f,
		_captionPaddingX,
		0.0f };
}

float GroupBox::GetHeaderSlotHeightDip(float availableWidth)
{
	(void)availableWidth;
	return GetCaptionBandHeight();
}

void GroupBox::PerformGroupLayoutIfNeeded()
{
	HeaderedContentControl::PerformPendingLayout();
}

void GroupBox::PerformPendingLayout()
{
	PerformGroupLayoutIfNeeded();
}

void GroupBox::OnRender()
{
	if (this->IsVisible == false) return;

	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	auto font = this->GetRenderFont();
	float textWidth = 0.0f;
	if (font)
	{
		auto textSize = font->GetTextSize(GetDisplayText());
		textWidth = textSize.width;
	}
	float captionBandHeight = GetCaptionBandHeight();
	const float border = BorderThickness.MaxEdge();
	const float radius = (std::clamp)(7.0f, 0.0f,
		(std::min)(actualWidth, actualHeight) * 0.5f);

	this->BeginRender();
	if (GetControlTemplateRoot())
	{
		this->EndRender();
		return;
	}
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, this->RendererBackgroundColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, this->RendererBackgroundColor);
		if (border > 0.0f && this->RendererBorderColor.a > 0.0f)
		{
			const float drawW = (std::max)(0.0f, actualWidth - border);
			const float drawH = (std::max)(0.0f, actualHeight - border);
			if (radius > 0.0f)
				d2d->DrawRoundRect(border * 0.5f, border * 0.5f, drawW, drawH, this->RendererBorderColor, border, radius);
			else
				d2d->DrawRect(border * 0.5f, border * 0.5f, drawW, drawH, this->RendererBorderColor, border);
		}

		auto* header = GetHeaderVisual();
		if (header || !GetDisplayText().empty())
		{
			const float captionH = (std::min)(captionBandHeight, actualHeight);
			const float maxCaptionW = (std::max)(0.0f,
				actualWidth - _captionMarginLeft * 2.0f);
			const auto headerSize = header
				? header->GetActualSizeDip() : cui::core::Size{ textWidth, captionH };
			const auto headerLocation = header
				? header->GetActualLocationDip()
				: cui::core::Point{ _captionMarginLeft, _captionPaddingY };
			const float captionW = (std::min)(maxCaptionW,
				headerSize.width + _captionPaddingX * 2.0f);
			const float captionX = (std::clamp)(
				headerLocation.x - _captionPaddingX,
				0.0f, (std::max)(0.0f, actualWidth - captionW));
			const float captionY = (std::clamp)(
				headerLocation.y - _captionPaddingY,
				0.0f, (std::max)(0.0f, actualHeight - captionH));
			const float captionRadius = (std::clamp)(
				_captionCornerRadius, 0.0f, captionH * 0.5f);
			const auto captionBack = _captionBackColor.a > 0.0f
				? _captionBackColor : this->RendererBackgroundColor;
			if (captionBack.a > 0.0f)
				d2d->FillRoundRect(captionX, captionY, captionW, captionH, captionBack, captionRadius);
			if (_captionBorderColor.a > 0.0f)
				d2d->DrawRoundRect(captionX + 0.5f, captionY + 0.5f,
					(std::max)(0.0f, captionW - 1.0f),
					(std::max)(0.0f, captionH - 1.0f),
					_captionBorderColor, 1.0f, captionRadius);
			if (!header)
				d2d->DrawString(GetDisplayText(), captionX + _captionPaddingX,
					captionY + _captionPaddingY, this->RendererForegroundColor, font);
		}
	}
	if (!this->IsEnabled)
	{
		const auto disabledOverlay = cui::theme::palette::DisabledOverlay;
		if (radius > 0.0f)
			d2d->FillRoundRect(
				0, 0, actualWidth, actualHeight, disabledOverlay, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, disabledOverlay);
	}
	this->EndRender();
}
