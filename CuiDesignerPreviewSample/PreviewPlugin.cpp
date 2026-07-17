#define CUI_DESIGNER_PREVIEW_PLUGIN_EXPORTS
#include "../CuiDesigner/DesignerPreviewPluginAbi.h"

#include <algorithm>
#include <cstddef>
#include <new>
#include <string>
#include <vector>

namespace
{
	struct PreviewSession
	{
		std::string Text = "Ready";
		int64_t Severity = 1;
		bool Checked = false;
		std::vector<CuiDesignerPreviewPrimitiveV1> Primitives;
	};

	struct PluginState
	{
		CuiDesignerPreviewHostV1 Host{};
		uint32_t ActiveSessions = 0;
	};

	PluginState State;

	bool Equals(CuiDesignerUtf8ViewV1 value, const char* expected)
	{
		if (!expected) return false;
		const size_t size = std::char_traits<char>::length(expected);
		return value.Size == size && (size == 0
			|| (value.Data && std::equal(
				value.Data, value.Data + value.Size, expected)));
	}

	void Log(const char* message)
	{
		if (!State.Host.Log || !message) return;
		State.Host.Log(
			State.Host.HostContext,
			CUI_DESIGNER_PREVIEW_LOG_INFO_V1,
			{ message, static_cast<uint32_t>(
				std::char_traits<char>::length(message)) });
	}

	CuiDesignerPreviewPrimitiveV1 Primitive(
		uint32_t kind,
		float x,
		float y,
		float width,
		float height)
	{
		CuiDesignerPreviewPrimitiveV1 result{};
		result.StructSize = sizeof(result);
		result.Kind = kind;
		result.X = x;
		result.Y = y;
		result.Width = width;
		result.Height = height;
		return result;
	}

	CuiDesignerPreviewStatusV1 CUI_DESIGNER_PREVIEW_CALL CreateSession(
		void*,
		CuiDesignerUtf8ViewV1 xamlNamespace,
		CuiDesignerUtf8ViewV1 xamlName,
		void** output)
	{
		if (!output) return CUI_DESIGNER_PREVIEW_INVALID_ARGUMENT_V1;
		*output = nullptr;
		if (!Equals(xamlNamespace, "urn:cui:samples")
			|| !Equals(xamlName, "StatusBadge"))
			return CUI_DESIGNER_PREVIEW_UNSUPPORTED_V1;
		try
		{
			*output = new PreviewSession();
			++State.ActiveSessions;
			return CUI_DESIGNER_PREVIEW_OK_V1;
		}
		catch (...)
		{
			return CUI_DESIGNER_PREVIEW_FAILED_V1;
		}
	}

	void CUI_DESIGNER_PREVIEW_CALL DestroySession(void*, void* value)
	{
		if (!value) return;
		delete static_cast<PreviewSession*>(value);
		if (State.ActiveSessions > 0) --State.ActiveSessions;
	}

	CuiDesignerPreviewStatusV1 CUI_DESIGNER_PREVIEW_CALL SetValue(
		void*,
		void* value,
		CuiDesignerUtf8ViewV1 propertyName,
		const CuiDesignerPreviewValueV1* input)
	{
		if (!value || !input || input->StructSize < sizeof(*input))
			return CUI_DESIGNER_PREVIEW_INVALID_ARGUMENT_V1;
		auto& session = *static_cast<PreviewSession*>(value);
		try
		{
			if (Equals(propertyName, "Text")
				&& input->Kind == CUI_DESIGNER_PREVIEW_VALUE_UTF8_V1)
			{
				const auto text = input->Data.Utf8Value;
				if (text.Size > CUI_DESIGNER_PREVIEW_MAX_UTF8_BYTES_V1
					|| (text.Size != 0 && !text.Data))
					return CUI_DESIGNER_PREVIEW_LIMIT_EXCEEDED_V1;
				session.Text.assign(text.Data ? text.Data : "", text.Size);
				return CUI_DESIGNER_PREVIEW_OK_V1;
			}
			if (Equals(propertyName, "Severity")
				&& input->Kind == CUI_DESIGNER_PREVIEW_VALUE_INT64_V1)
			{
				session.Severity = input->Data.Int64Value;
				return CUI_DESIGNER_PREVIEW_OK_V1;
			}
			if (Equals(propertyName, "Checked")
				&& input->Kind == CUI_DESIGNER_PREVIEW_VALUE_BOOL_V1)
			{
				session.Checked = input->Data.BoolValue != 0;
				return CUI_DESIGNER_PREVIEW_OK_V1;
			}
			return CUI_DESIGNER_PREVIEW_UNSUPPORTED_V1;
		}
		catch (...)
		{
			return CUI_DESIGNER_PREVIEW_FAILED_V1;
		}
	}

	CuiDesignerPreviewStatusV1 CUI_DESIGNER_PREVIEW_CALL Render(
		void*,
		void* value,
		const CuiDesignerPreviewFrameInputV1* input,
		CuiDesignerPreviewFrameV1* output)
	{
		if (!value || !input || !output
			|| input->StructSize < sizeof(*input)
			|| output->StructSize < sizeof(*output))
			return CUI_DESIGNER_PREVIEW_INVALID_ARGUMENT_V1;
		auto& session = *static_cast<PreviewSession*>(value);
		try
		{
			session.Primitives.clear();
			auto background = Primitive(
				CUI_DESIGNER_PREVIEW_PRIMITIVE_ROUNDED_RECT_V1,
				0.0f, 0.0f, input->Width, input->Height);
			background.RadiusX = 8.0f;
			background.RadiusY = 8.0f;
			background.FillArgb32 = session.Severity >= 2
				? 0xFFFFE6E6u : 0xFFE8F5E9u;
			background.StrokeArgb32 = session.Severity >= 2
				? 0xFFC62828u : 0xFF2E7D32u;
			background.StrokeWidth = session.Checked ? 2.5f : 1.5f;
			session.Primitives.push_back(background);

			auto indicator = Primitive(
				CUI_DESIGNER_PREVIEW_PRIMITIVE_ELLIPSE_V1,
				10.0f, input->Height * 0.5f - 4.0f, 8.0f, 8.0f);
			indicator.FillArgb32 = session.Severity >= 2
				? 0xFFD32F2Fu : 0xFF43A047u;
			session.Primitives.push_back(indicator);

			auto text = Primitive(
				CUI_DESIGNER_PREVIEW_PRIMITIVE_TEXT_V1,
				26.0f, 0.0f, (std::max)(0.0f, input->Width - 32.0f),
				input->Height);
			text.FillArgb32 = input->Enabled ? 0xFF1F2937u : 0xFF808080u;
			text.FontSize = 13.0f;
			text.Text = { session.Text.data(),
				static_cast<uint32_t>(session.Text.size()) };
			session.Primitives.push_back(text);

			output->Primitives = session.Primitives.data();
			output->PrimitiveCount =
				static_cast<uint32_t>(session.Primitives.size());
			return CUI_DESIGNER_PREVIEW_OK_V1;
		}
		catch (...)
		{
			return CUI_DESIGNER_PREVIEW_FAILED_V1;
		}
	}

	void CUI_DESIGNER_PREVIEW_CALL Shutdown(void*)
	{
		Log(State.ActiveSessions == 0
			? "CUI sample preview plugin shutdown"
			: "CUI sample preview plugin shutdown with live sessions");
		State.Host = {};
	}

	CuiDesignerPreviewPluginV1 Plugin = {
		sizeof(CuiDesignerPreviewPluginV1),
		CUI_DESIGNER_PREVIEW_ABI_V1,
		&State,
		&CreateSession,
		&DestroySession,
		&SetValue,
		&Render,
		&Shutdown
	};
}

extern "C" CUI_DESIGNER_PREVIEW_EXPORT CuiDesignerPreviewStatusV1
	CUI_DESIGNER_PREVIEW_CALL CuiDesignerGetPreviewPluginV1(
		const CuiDesignerPreviewHostV1* host,
		const CuiDesignerPreviewPluginV1** output)
{
	if (!host || !output
		|| host->StructSize < sizeof(CuiDesignerPreviewHostV1)
		|| host->AbiVersion != CUI_DESIGNER_PREVIEW_ABI_V1)
		return CUI_DESIGNER_PREVIEW_INVALID_ARGUMENT_V1;
	State.Host = *host;
	*output = &Plugin;
	Log("CUI sample preview plugin loaded");
	return CUI_DESIGNER_PREVIEW_OK_V1;
}

