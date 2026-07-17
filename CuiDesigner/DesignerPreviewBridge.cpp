#include "DesignerPreviewBridge.h"

#include "DesignerControlFactory.h"
#include "DesignerStyleSheetUtils.h"
#include "../CUI/include/Form.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <utility>

namespace
{
	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	bool TryWideToUtf8(const std::wstring& value, std::string& output)
	{
		output.clear();
		if (value.empty()) return true;
		if (value.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
			return false;
		const int count = ::WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
		if (count <= 0) return false;
		output.resize(static_cast<size_t>(count));
		return ::WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), output.data(), count,
			nullptr, nullptr) == count;
	}

	D2D1_COLOR_F FromArgb32(uint32_t value) noexcept
	{
		constexpr float scale = 1.0f / 255.0f;
		return D2D1_COLOR_F{
			static_cast<float>((value >> 16) & 0xffu) * scale,
			static_cast<float>((value >> 8) & 0xffu) * scale,
			static_cast<float>(value & 0xffu) * scale,
			static_cast<float>((value >> 24) & 0xffu) * scale
		};
	}

	uint32_t ToArgb32(const D2D1_COLOR_F& value) noexcept
	{
		auto byte = [](float component)
		{
			return static_cast<uint32_t>(std::lround(
				(std::clamp)(component, 0.0f, 1.0f) * 255.0f));
		};
		return (byte(value.a) << 24) | (byte(value.r) << 16)
			| (byte(value.g) << 8) | byte(value.b);
	}

	bool HasAlpha(uint32_t value) noexcept
	{
		return (value >> 24) != 0;
	}

	void FillAndStrokeRect(
		D2DGraphics& graphics,
		const DesignerPreviewPrimitive& primitive,
		bool rounded)
	{
		const D2D1_RECT_F rect{
			primitive.X,
			primitive.Y,
			primitive.X + primitive.Width,
			primitive.Y + primitive.Height
		};
		const float radius = (std::min)(
			primitive.RadiusX, primitive.RadiusY);
		if (HasAlpha(primitive.FillArgb32))
		{
			if (rounded)
				graphics.FillRoundRect(rect, FromArgb32(primitive.FillArgb32), radius);
			else
				graphics.FillRect(rect, FromArgb32(primitive.FillArgb32));
		}
		if (primitive.StrokeWidth > 0.0f
			&& HasAlpha(primitive.StrokeArgb32))
		{
			if (rounded)
				graphics.DrawRoundRect(
					rect, FromArgb32(primitive.StrokeArgb32),
					primitive.StrokeWidth, radius);
			else
				graphics.DrawRect(
					rect, FromArgb32(primitive.StrokeArgb32),
					primitive.StrokeWidth);
		}
	}

	void DrawPrimitive(
		D2DGraphics& graphics,
		const DesignerPreviewPrimitive& primitive)
	{
		switch (primitive.Kind)
		{
		case CUI_DESIGNER_PREVIEW_PRIMITIVE_RECT_V1:
			FillAndStrokeRect(graphics, primitive, false);
			break;
		case CUI_DESIGNER_PREVIEW_PRIMITIVE_ROUNDED_RECT_V1:
			FillAndStrokeRect(graphics, primitive, true);
			break;
		case CUI_DESIGNER_PREVIEW_PRIMITIVE_ELLIPSE_V1:
		{
			const float radiusX = primitive.Width * 0.5f;
			const float radiusY = primitive.Height * 0.5f;
			const float centerX = primitive.X + radiusX;
			const float centerY = primitive.Y + radiusY;
			if (HasAlpha(primitive.FillArgb32))
				graphics.FillEllipse(centerX, centerY, radiusX, radiusY,
					FromArgb32(primitive.FillArgb32));
			if (primitive.StrokeWidth > 0.0f
				&& HasAlpha(primitive.StrokeArgb32))
				graphics.DrawEllipse(centerX, centerY, radiusX, radiusY,
					FromArgb32(primitive.StrokeArgb32),
					primitive.StrokeWidth);
			break;
		}
		case CUI_DESIGNER_PREVIEW_PRIMITIVE_LINE_V1:
			if (primitive.StrokeWidth > 0.0f
				&& HasAlpha(primitive.StrokeArgb32))
				graphics.DrawLine(
					primitive.X, primitive.Y,
					primitive.X + primitive.Width,
					primitive.Y + primitive.Height,
					FromArgb32(primitive.StrokeArgb32),
					primitive.StrokeWidth);
			break;
		case CUI_DESIGNER_PREVIEW_PRIMITIVE_TEXT_V1:
			if (HasAlpha(primitive.FillArgb32) && !primitive.Text.empty())
			{
				Font font(L"Arial", primitive.FontSize);
				graphics.DrawString(
					primitive.Text, primitive.X, primitive.Y,
					primitive.Width, primitive.Height,
					FromArgb32(primitive.FillArgb32), &font);
			}
			break;
		default:
			break;
		}
	}

	class PreviewAttachment final
	{
	public:
		explicit PreviewAttachment(DesignerPreviewPluginSession session)
			: _session(std::move(session))
		{
		}

		void Render(Control& control, D2DGraphics& graphics) noexcept
		{
			try
			{
				SynchronizeCommonProperties(control);
				const auto size = control.GetActualSizeDip();
				if (!std::isfinite(size.width) || size.width <= 0.0f
					|| !std::isfinite(size.height) || size.height <= 0.0f)
					return;
				CuiDesignerPreviewFrameInputV1 input{};
				input.StructSize = sizeof(input);
				input.Width = size.width;
				input.Height = size.height;
				input.DpiScale = control.ParentForm
					? control.ParentForm->GetDpiScale() : 1.0f;
				input.Enabled = control.Enable ? 1u : 0u;
				input.Checked = control.Checked ? 1u : 0u;
				input.FrameNumber = _frameNumber++;
				std::vector<DesignerPreviewPrimitive> primitives;
				if (!_session.Render(input, primitives)) return;
				for (const auto& primitive : primitives)
					DrawPrimitive(graphics, primitive);
			}
			catch (...)
			{
			}
		}

		bool SetValue(
			const std::wstring& propertyName,
			const DesignerStyleValue& value,
			std::wstring* outError)
		{
			BindingValue converted;
			if (!DesignerStyleSheetUtils::TryConvertValue(
				value, converted, outError)) return false;
			switch (value.Kind)
			{
			case DesignerStyleValueKind::Bool:
			{
				bool typed = false;
				return converted.TryGet(typed)
					&& _session.SetBool(propertyName, typed, outError);
			}
			case DesignerStyleValueKind::Int:
			case DesignerStyleValueKind::Int64:
			{
				long long typed = 0;
				return converted.TryGet(typed)
					&& _session.SetInt64(propertyName, typed, outError);
			}
			case DesignerStyleValueKind::Float:
			case DesignerStyleValueKind::Double:
			{
				double typed = 0.0;
				return converted.TryGet(typed)
					&& _session.SetDouble(propertyName, typed, outError);
			}
			case DesignerStyleValueKind::Color:
			{
				D2D1_COLOR_F typed{};
				return converted.TryGet(typed)
					&& _session.SetArgb32(
						propertyName, ToArgb32(typed), outError);
			}
			case DesignerStyleValueKind::String:
			case DesignerStyleValueKind::Thickness:
			case DesignerStyleValueKind::Size:
			case DesignerStyleValueKind::Length:
			default:
			{
				std::string utf8;
				if (!TryWideToUtf8(value.Text, utf8))
					return Fail(L"预览属性文本无法转换为 UTF-8。",
						outError);
				return _session.SetUtf8(propertyName, utf8, outError);
			}
			}
		}

	private:
		void SynchronizeCommonProperties(Control& control)
		{
			const auto text = control.Text;
			if (!_hasText || text != _text)
			{
				_text = text;
				_hasText = true;
				std::string utf8;
				if (TryWideToUtf8(text, utf8))
					(void)_session.SetUtf8(L"Text", utf8);
			}
			if (!_hasChecked || control.Checked != _checked)
			{
				_checked = control.Checked;
				_hasChecked = true;
				(void)_session.SetBool(L"Checked", _checked);
			}
		}

		DesignerPreviewPluginSession _session;
		uint64_t _frameNumber = 0;
		std::wstring _text;
		bool _hasText = false;
		bool _checked = false;
		bool _hasChecked = false;
	};

	thread_local std::unordered_map<Control*, std::weak_ptr<PreviewAttachment>>
		Attachments;

	class PreviewAttachmentRegistration final
	{
	public:
		PreviewAttachmentRegistration(
			Control& control,
			std::shared_ptr<PreviewAttachment> attachment)
			: _control(&control), _attachment(std::move(attachment))
		{
			Attachments[_control] = _attachment;
		}

		~PreviewAttachmentRegistration()
		{
			const auto found = Attachments.find(_control);
			if (found == Attachments.end()) return;
			const auto current = found->second.lock();
			if (!current || current == _attachment) Attachments.erase(found);
		}

		void Render(Control& control, D2DGraphics& graphics) noexcept
		{
			_attachment->Render(control, graphics);
		}

	private:
		Control* _control = nullptr;
		std::shared_ptr<PreviewAttachment> _attachment;
	};
}

bool DesignerPreviewBridge::Attach(
	Control& control,
	const Module& module,
	const DesignerCustomControlType& customType,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (!module || !module->IsLoaded())
		return Fail(L"预览插件模块未加载。", outError);
	if (customType.XamlNamespace.empty() || customType.XamlName.empty())
		return Fail(L"自定义控件缺少 XAML identity。", outError);
	DesignerPreviewPluginSession session;
	if (!module->CreateSession(
		customType.XamlNamespace, customType.XamlName, session, outError))
		return false;
	auto registration = std::make_shared<PreviewAttachmentRegistration>(
		control, std::make_shared<PreviewAttachment>(std::move(session)));
	control.SetRenderDecorator(
		[registration = std::move(registration)](
			Control& target, D2DGraphics& graphics)
		{
			registration->Render(target, graphics);
		});
	return true;
}

bool DesignerPreviewBridge::SetValue(
	Control& control,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	std::wstring* outError)
{
	if (outError) outError->clear();
	const auto found = Attachments.find(&control);
	if (found == Attachments.end())
		return Fail(L"控件没有插件预览 session。", outError);
	const auto attachment = found->second.lock();
	if (!attachment)
	{
		Attachments.erase(found);
		return Fail(L"控件的插件预览 session 已失效。", outError);
	}
	return attachment->SetValue(propertyName, value, outError);
}

size_t DesignerPreviewBridge::AttachFactories(
	std::vector<DesignerControlDescriptor>& descriptors,
	const std::vector<Module>& modules)
{
	size_t attached = 0;
	for (auto& descriptor : descriptors)
	{
		if (!descriptor.IsCustom() || descriptor.PreviewFactory) continue;
		for (const auto& module : modules)
		{
			if (!module || !module->IsLoaded()) continue;
			DesignerPreviewPluginSession probe;
			if (!module->CreateSession(
				descriptor.CustomType.XamlNamespace,
				descriptor.CustomType.XamlName,
				probe))
				continue;

			const auto baseType = descriptor.Type;
			const auto customType = descriptor.CustomType;
			const auto customProperties = descriptor.CustomProperties;
			descriptor.PreviewFactory =
				[module, baseType, customType, customProperties](int x, int y)
			{
				auto control = DesignerControlFactory::Create(baseType, x, y);
				if (control && DesignerPreviewBridge::Attach(
					*control, module, customType))
				{
					for (const auto& property : customProperties)
						(void)DesignerPreviewBridge::SetValue(
							*control, property.Name, property.DefaultValue);
				}
				return control;
			};
			++attached;
			break;
		}
	}
	return attached;
}
