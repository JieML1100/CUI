#include "GroupBox.h"
#include "Form.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	template<typename TValue>
	ControlPropertyOptions<GroupBox, TValue> GroupBoxPropertyOptions(
		TValue defaultValue,
		const wchar_t* category,
		int categoryOrder,
		int order,
		ControlPropertyEditorKind editor,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		ControlPropertyOptions<GroupBox, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags | ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = category;
		options.Design.CategoryOrder = categoryOrder;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		return options;
	}

	auto GroupBoxPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			GroupBox& target,
			BindingPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					Control*, const ControlPropertyChangedEventArgs& args)
				{
					if (_wcsicmp(args.PropertyName.c_str(), propertyName.c_str()) == 0)
						handler();
				});
		};
	}

	ControlPropertyOptions<GroupBox, float> GroupBoxMetricOptions(
		float defaultValue,
		int order,
		ControlPropertyFlags flags = ControlPropertyFlags::AffectsRender)
	{
		auto options = GroupBoxPropertyOptions(
			defaultValue, L"Caption", 150, order,
			ControlPropertyEditorKind::Number, flags);
		options.Coerce = [](
			GroupBox&, const float& proposed) -> std::optional<float>
		{
			return std::isfinite(proposed)
				? std::optional<float>{ (std::max)(0.0f, proposed) }
				: std::nullopt;
		};
		options.Design.Minimum = 0.0;
		options.Design.Step = 0.5;
		return options;
	}

	bool GroupBoxColorsEqual(
		const D2D1_COLOR_F& left,
		const D2D1_COLOR_F& right)
	{
		return left.r == right.r && left.g == right.g
			&& left.b == right.b && left.a == right.a;
	}

	ControlPropertyOptions<GroupBox, D2D1_COLOR_F> GroupBoxColorOptions(
		D2D1_COLOR_F defaultValue,
		int order)
	{
		auto options = GroupBoxPropertyOptions(
			defaultValue, L"Caption", 150, order,
			ControlPropertyEditorKind::Color);
		options.Equals = GroupBoxColorsEqual;
		return options;
	}
}

UIClass GroupBox::Type() { return UIClass::UI_GroupBox; }

void GroupBox::EnsureBindingPropertiesRegistered()
{
	HeaderedContentControl::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		BindingPropertyRegistry::Register<GroupBox, float>(L"CaptionMarginLeft",
			[](GroupBox& target) { return target.CaptionMarginLeft; },
			[](GroupBox& target, const float& value) { target.CaptionMarginLeft = value; },
			GroupBoxPropertySubscriber(L"CaptionMarginLeft"),
			GroupBoxMetricOptions(12.0f, 10));
		BindingPropertyRegistry::Register<GroupBox, float>(L"CaptionPaddingX",
			[](GroupBox& target) { return target.CaptionPaddingX; },
			[](GroupBox& target, const float& value) { target.CaptionPaddingX = value; },
			GroupBoxPropertySubscriber(L"CaptionPaddingX"),
			GroupBoxMetricOptions(6.0f, 20));
		BindingPropertyRegistry::Register<GroupBox, float>(L"CaptionPaddingY",
			[](GroupBox& target) { return target.CaptionPaddingY; },
			[](GroupBox& target, const float& value) { target.CaptionPaddingY = value; },
			GroupBoxPropertySubscriber(L"CaptionPaddingY"),
			GroupBoxMetricOptions(2.0f, 30,
				ControlPropertyFlags::AffectsMeasure
				| ControlPropertyFlags::AffectsRender));
		BindingPropertyRegistry::Register<GroupBox, float>(L"CaptionCornerRadius",
			[](GroupBox& target) { return target.CaptionCornerRadius; },
			[](GroupBox& target, const float& value) { target.CaptionCornerRadius = value; },
			GroupBoxPropertySubscriber(L"CaptionCornerRadius"),
			GroupBoxMetricOptions(6.0f, 40));
		BindingPropertyRegistry::Register<GroupBox, D2D1_COLOR_F>(L"CaptionBackColor",
			[](GroupBox& target) { return target.CaptionBackColor; },
			[](GroupBox& target, const D2D1_COLOR_F& value) { target.CaptionBackColor = value; },
			GroupBoxPropertySubscriber(L"CaptionBackColor"),
			GroupBoxColorOptions(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f }, 50));
		BindingPropertyRegistry::Register<GroupBox, D2D1_COLOR_F>(L"CaptionBorderColor",
			[](GroupBox& target) { return target.CaptionBorderColor; },
			[](GroupBox& target, const D2D1_COLOR_F& value) { target.CaptionBorderColor = value; },
			GroupBoxPropertySubscriber(L"CaptionBorderColor"),
			GroupBoxColorOptions(D2D1_COLOR_F{ 0.0f, 0.0f, 0.0f, 0.0f }, 60));
		return true;
	}();
	(void)registered;
}

GroupBox::GroupBox()
	: HeaderedContentControl()
{
	ClearRows();
	AddRow(GridLength::Pixels(20.0f));
	AddRow(GridLength::Star(1.0f));
	this->Text = L"GroupBox";
	this->BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	this->OnTextChanged += [&](Control* sender, std::wstring oldText, std::wstring newText)
		{
			(void)sender;
			(void)oldText;
			(void)newText;
			this->InvalidateLayout();
		};
}

GroupBox::GroupBox(std::wstring text, int x, int y, int width, int height)
	: GroupBox()
{
	this->Text = text;
	this->Location = POINT{ x, y };
	this->Size = SIZE{ width, height };
}

#define CUI_GROUP_BOX_PROPERTY_IMPL(type, name, field, propertyName) \
	GET_CPP(GroupBox, type, name) { return field; } \
	SET_CPP(GroupBox, type, name) { (void)SetPropertyField(propertyName, field, value); }

CUI_GROUP_BOX_PROPERTY_IMPL(float, CaptionMarginLeft, _captionMarginLeft, L"CaptionMarginLeft")
CUI_GROUP_BOX_PROPERTY_IMPL(float, CaptionPaddingX, _captionPaddingX, L"CaptionPaddingX")
CUI_GROUP_BOX_PROPERTY_IMPL(float, CaptionPaddingY, _captionPaddingY, L"CaptionPaddingY")
CUI_GROUP_BOX_PROPERTY_IMPL(float, CaptionCornerRadius, _captionCornerRadius, L"CaptionCornerRadius")
CUI_GROUP_BOX_PROPERTY_IMPL(D2D1_COLOR_F, CaptionBackColor, _captionBackColor, L"CaptionBackColor")
CUI_GROUP_BOX_PROPERTY_IMPL(D2D1_COLOR_F, CaptionBorderColor, _captionBorderColor, L"CaptionBorderColor")

#undef CUI_GROUP_BOX_PROPERTY_IMPL

float GroupBox::GetCaptionBandHeight()
{
	if (auto* header = GetHeaderVisual())
	{
		const auto desired = header->Measure(cui::core::Constraints::Unbounded());
		return (std::max)(20.0f, desired.height + CaptionPaddingY * 2.0f);
	}
	auto font = this->Font;
	if (!font) return 20.0f;
	auto textSize = font->GetTextSize(this->Text);
	return (std::max)(20.0f, textSize.height + CaptionPaddingY * 2.0f);
}

void GroupBox::ConfigureHeaderVisual(Control& child)
{
	HeaderedContentControl::ConfigureHeaderVisual(child);
	child.HAlign = HorizontalAlignment::Left;
	child.VAlign = VerticalAlignment::Center;
	child.Margin = Thickness{
		CaptionMarginLeft + CaptionPaddingX,
		0.0f,
		CaptionPaddingX,
		0.0f };
}

void GroupBox::UpdateHeaderLayout()
{
	const float height = GetCaptionBandHeight();
	const auto& rows = GetRows();
	if (rows.size() != 2 || !rows[0].Height.IsPixel()
		|| std::fabs(rows[0].Height.Value - height) > 0.01f
		|| !rows[1].Height.IsStar())
	{
		ClearRows();
		AddRow(GridLength::Pixels(height));
		AddRow(GridLength::Star(1.0f));
	}
	if (auto* header = GetHeaderVisual()) ConfigureHeaderVisual(*header);
}

cui::core::Point GroupBox::GetChildrenLayoutOriginDip()
{
	return {
		Padding.Left,
		Padding.Top + GetCaptionBandHeight()
	};
}

void GroupBox::PerformGroupLayoutIfNeeded()
{
	UpdateHeaderLayout();
	if (this->IsLayoutSuspended()) return;
	if (!_needsLayout && !(_layoutEngine && _layoutEngine->NeedsLayout()))
		return;
	PerformLayout();
}

void GroupBox::PerformPendingLayout()
{
	PerformGroupLayoutIfNeeded();
}

void GroupBox::Update()
{
	if (this->IsVisual == false) return;

	PerformGroupLayoutIfNeeded();

	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	auto font = this->Font;
	float textWidth = 0.0f;
	if (font)
	{
		auto textSize = font->GetTextSize(this->Text);
		textWidth = textSize.width;
	}
	float captionBandHeight = GetCaptionBandHeight();
	const float border = (std::max)(0.0f, this->BorderThickness);
	const float radius = (std::clamp)(this->CornerRadius, 0.0f, (std::min)(actualWidth, actualHeight) * 0.5f);

	this->BeginRender();
	if (GetControlTemplateRoot())
	{
		if (!this->ParentForm->IsDCompSceneRenderActive())
		{
			for (auto* child : this->GetChildrenInZOrder())
				if (child && child->Visible) child->Update();
		}
		this->EndRender();
		return;
	}
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, this->BackColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage(radius);
		}
		if (!this->ParentForm || !this->ParentForm->IsDCompSceneRenderActive())
		{
			auto* content = GetVisualContent();
			if (!content) content = GetGeneratedPresenter();
			if (content && content->Visible) content->Update();
		}

		if (border > 0.0f && this->BorderColor.a > 0.0f)
		{
			const float drawW = (std::max)(0.0f, actualWidth - border);
			const float drawH = (std::max)(0.0f, actualHeight - border);
			if (radius > 0.0f)
				d2d->DrawRoundRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border, radius);
			else
				d2d->DrawRect(border * 0.5f, border * 0.5f, drawW, drawH, this->BorderColor, border);
		}

		auto* header = GetHeaderVisual();
		if (header || !this->Text.empty())
		{
			const float captionH = (std::min)(captionBandHeight, actualHeight);
			const float maxCaptionW = (std::max)(0.0f, actualWidth - CaptionMarginLeft * 2.0f);
			const auto headerSize = header
				? header->GetActualSizeDip() : cui::core::Size{ textWidth, captionH };
			const auto headerLocation = header
				? header->GetActualLocationDip() : cui::core::Point{ CaptionMarginLeft, CaptionPaddingY };
			const float captionW = (std::min)(maxCaptionW,
				headerSize.width + CaptionPaddingX * 2.0f);
			const float captionX = (std::clamp)(
				headerLocation.x - CaptionPaddingX,
				0.0f, (std::max)(0.0f, actualWidth - captionW));
			const float captionY = (std::clamp)(
				headerLocation.y - CaptionPaddingY,
				0.0f, (std::max)(0.0f, actualHeight - captionH));
			const float captionRadius = (std::clamp)(CaptionCornerRadius, 0.0f, captionH * 0.5f);
			const auto captionBack = CaptionBackColor.a > 0.0f ? CaptionBackColor : this->BackColor;
			if (captionBack.a > 0.0f)
				d2d->FillRoundRect(captionX, captionY, captionW, captionH, captionBack, captionRadius);
			if (CaptionBorderColor.a > 0.0f)
				d2d->DrawRoundRect(captionX + 0.5f, captionY + 0.5f, (std::max)(0.0f, captionW - 1.0f), (std::max)(0.0f, captionH - 1.0f), CaptionBorderColor, 1.0f, captionRadius);
			if (header)
			{
				if ((!this->ParentForm
					|| !this->ParentForm->IsDCompSceneRenderActive())
					&& header->Visible)
					header->Update();
			}
			else
				d2d->DrawString(this->Text, captionX + CaptionPaddingX,
					captionY + CaptionPaddingY, this->ForeColor, font);
		}
	}
	if (!this->Enable)
	{
		if (radius > 0.0f)
			d2d->FillRoundRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor, radius);
		else
			d2d->FillRect(0, 0, actualWidth, actualHeight, DisabledOverlayColor);
	}
	this->EndRender();
}

bool GroupBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	PerformGroupLayoutIfNeeded();
	return HeaderedContentControl::ProcessMessage(
		message, wParam, lParam, localX, localY);
}
