#define NOMINMAX
#include "RichTextClipboard.h"
#include "RichTextRtf.h"

#include <Windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <unordered_map>
#include <utility>

namespace
{
	constexpr wchar_t PortableFormatName[] =
		L"CUI.RichTextDocumentFragment.Binary.v8";
	constexpr wchar_t Version7PortableFormatName[] =
		L"CUI.RichTextDocumentFragment.Binary.v7";
	constexpr wchar_t Version6PortableFormatName[] =
		L"CUI.RichTextDocumentFragment.Binary.v6";
	constexpr wchar_t Version5PortableFormatName[] =
		L"CUI.RichTextDocumentFragment.Binary.v5";
	constexpr wchar_t Version4PortableFormatName[] =
		L"CUI.RichTextDocumentFragment.Binary.v4";
	constexpr wchar_t Version3PortableFormatName[] =
		L"CUI.RichTextDocumentFragment.Binary.v3";
	constexpr wchar_t Version2PortableFormatName[] =
		L"CUI.RichTextDocumentFragment.Binary.v2";
	constexpr wchar_t LegacyPortableFormatName[] =
		L"CUI.RichTextDocumentFragment.Binary.v1";
	constexpr std::uint8_t PortableMagic[] = {
		'C', 'U', 'I', 'A', 'T', 'B', '0', '1' };
	constexpr std::uint32_t PortableVersion = 8;
	constexpr std::uint32_t GradientInterpolationPortableVersion = 8;
	constexpr std::uint32_t LanguagePortableVersion = 7;
	constexpr std::uint32_t FontStretchPortableVersion = 6;
	constexpr std::uint32_t DirectionPortableVersion = 5;
	constexpr std::uint32_t ParagraphPortableVersion = 4;
	constexpr std::uint32_t MarkerPortableVersion = 3;
	constexpr std::uint32_t StructuredPortableVersion = 2;
	constexpr std::uint32_t LegacyPortableVersion = 1;
	constexpr std::size_t HeaderSize = 28;
	constexpr std::size_t MaxBytes = 64u * 1024u * 1024u;
	constexpr std::uint64_t MaxTextUnits = 16u * 1024u * 1024u;
	constexpr std::uint64_t MaxSpans = 1024u * 1024u;
	constexpr std::uint64_t MaxCollection = 65536u;
	constexpr std::uint64_t MaxStructureDepth = 1024u;

	UINT AttributedFormat() noexcept
	{
		static const UINT format = RegisterClipboardFormatW(PortableFormatName);
		return format;
	}

	UINT LegacyAttributedFormat() noexcept
	{
		static const UINT format =
			RegisterClipboardFormatW(LegacyPortableFormatName);
		return format;
	}

	UINT Version7AttributedFormat() noexcept
	{
		static const UINT format =
			RegisterClipboardFormatW(Version7PortableFormatName);
		return format;
	}

	UINT Version6AttributedFormat() noexcept
	{
		static const UINT format =
			RegisterClipboardFormatW(Version6PortableFormatName);
		return format;
	}

	UINT Version5AttributedFormat() noexcept
	{
		static const UINT format =
			RegisterClipboardFormatW(Version5PortableFormatName);
		return format;
	}

	UINT Version4AttributedFormat() noexcept
	{
		static const UINT format =
			RegisterClipboardFormatW(Version4PortableFormatName);
		return format;
	}

	UINT Version3AttributedFormat() noexcept
	{
		static const UINT format =
			RegisterClipboardFormatW(Version3PortableFormatName);
		return format;
	}

	UINT Version2AttributedFormat() noexcept
	{
		static const UINT format =
			RegisterClipboardFormatW(Version2PortableFormatName);
		return format;
	}

	UINT RtfFormat() noexcept
	{
		static const UINT format =
			RegisterClipboardFormatW(L"Rich Text Format");
		return format;
	}

	class Writer final
	{
	public:
		void U8(std::uint8_t value) { Data.push_back(value); }
		void U16(std::uint16_t value)
		{
			U8(static_cast<std::uint8_t>(value));
			U8(static_cast<std::uint8_t>(value >> 8));
		}
		void U32(std::uint32_t value)
		{
			for (int shift = 0; shift < 32; shift += 8)
				U8(static_cast<std::uint8_t>(value >> shift));
		}
		void U64(std::uint64_t value)
		{
			for (int shift = 0; shift < 64; shift += 8)
				U8(static_cast<std::uint8_t>(value >> shift));
		}
		void Float(float value)
		{
			std::uint32_t bits = 0;
			static_assert(sizeof(bits) == sizeof(value));
			std::memcpy(&bits, &value, sizeof(bits));
			U32(bits);
		}
		void Bytes(const void* source, std::size_t size)
		{
			if (size == 0) return;
			const auto* first = static_cast<const std::uint8_t*>(source);
			Data.insert(Data.end(), first, first + size);
		}
		bool String(const std::wstring& value)
		{
			if (value.size() > MaxTextUnits) return false;
			U64(static_cast<std::uint64_t>(value.size()));
			for (const wchar_t character : value)
				U16(static_cast<std::uint16_t>(character));
			return true;
		}

		std::vector<std::uint8_t> Data;
	};

	class Reader final
	{
	public:
		Reader(const std::uint8_t* data, std::size_t size) noexcept
			: _data(data), _size(size) {}

		bool U8(std::uint8_t& value) noexcept
		{
			if (_position >= _size) return false;
			value = _data[_position++];
			return true;
		}
		bool U16(std::uint16_t& value) noexcept
		{
			std::uint8_t low = 0;
			std::uint8_t high = 0;
			if (!U8(low) || !U8(high)) return false;
			value = static_cast<std::uint16_t>(
				low | (static_cast<std::uint16_t>(high) << 8));
			return true;
		}
		bool U32(std::uint32_t& value) noexcept
		{
			value = 0;
			for (int shift = 0; shift < 32; shift += 8)
			{
				std::uint8_t byte = 0;
				if (!U8(byte)) return false;
				value |= static_cast<std::uint32_t>(byte) << shift;
			}
			return true;
		}
		bool U64(std::uint64_t& value) noexcept
		{
			value = 0;
			for (int shift = 0; shift < 64; shift += 8)
			{
				std::uint8_t byte = 0;
				if (!U8(byte)) return false;
				value |= static_cast<std::uint64_t>(byte) << shift;
			}
			return true;
		}
		bool Float(float& value) noexcept
		{
			std::uint32_t bits = 0;
			if (!U32(bits)) return false;
			std::memcpy(&value, &bits, sizeof(value));
			return std::isfinite(value);
		}
		bool String(std::wstring& value)
		{
			std::uint64_t length = 0;
			if (!U64(length) || length > MaxTextUnits
				|| length > Remaining() / 2) return false;
			value.resize(static_cast<std::size_t>(length));
			for (auto& character : value)
			{
				std::uint16_t encoded = 0;
				if (!U16(encoded)) return false;
				character = static_cast<wchar_t>(encoded);
			}
			return true;
		}
		std::size_t Remaining() const noexcept { return _size - _position; }

	private:
		const std::uint8_t* _data = nullptr;
		std::size_t _size = 0;
		std::size_t _position = 0;
	};

	std::uint32_t Checksum(
		const std::uint8_t* data, std::size_t size) noexcept
	{
		std::uint32_t result = 2166136261u;
		for (std::size_t index = 0; index < size; ++index)
		{
			result ^= data[index];
			result *= 16777619u;
		}
		return result;
	}

	bool WriteTransform(
		Writer& writer,
		const std::optional<cui::drawing::Transform>& transform)
	{
		writer.U8(transform ? 1 : 0);
		if (!transform) return true;
		if (transform->Operations.size() > MaxCollection) return false;
		writer.U64(static_cast<std::uint64_t>(transform->Operations.size()));
		for (const auto& operation : transform->Operations)
		{
			const auto kind = static_cast<std::uint32_t>(operation.Kind);
			if (kind > static_cast<std::uint32_t>(
				cui::drawing::TransformKind::Skew)) return false;
			writer.U32(kind);
			const float values[] = {
				operation.Matrix._11, operation.Matrix._12,
				operation.Matrix._21, operation.Matrix._22,
				operation.Matrix._31, operation.Matrix._32,
				operation.X, operation.Y,
				operation.ScaleX, operation.ScaleY,
				operation.Angle, operation.AngleX, operation.AngleY,
				operation.CenterX, operation.CenterY };
			for (const float value : values)
			{
				if (!std::isfinite(value)) return false;
				writer.Float(value);
			}
		}
		return true;
	}

	bool ReadTransform(
		Reader& reader,
		std::optional<cui::drawing::Transform>& transform)
	{
		std::uint8_t present = 0;
		if (!reader.U8(present) || present > 1) return false;
		if (present == 0)
		{
			transform.reset();
			return true;
		}
		std::uint64_t count = 0;
		if (!reader.U64(count) || count > MaxCollection) return false;
		cui::drawing::Transform result;
		result.Operations.reserve(static_cast<std::size_t>(count));
		for (std::uint64_t index = 0; index < count; ++index)
		{
			std::uint32_t kind = 0;
			if (!reader.U32(kind) || kind > static_cast<std::uint32_t>(
				cui::drawing::TransformKind::Skew)) return false;
			cui::drawing::TransformOperation operation;
			operation.Kind = static_cast<cui::drawing::TransformKind>(kind);
			float* values[] = {
				&operation.Matrix._11, &operation.Matrix._12,
				&operation.Matrix._21, &operation.Matrix._22,
				&operation.Matrix._31, &operation.Matrix._32,
				&operation.X, &operation.Y,
				&operation.ScaleX, &operation.ScaleY,
				&operation.Angle, &operation.AngleX, &operation.AngleY,
				&operation.CenterX, &operation.CenterY };
			for (auto* value : values)
				if (!reader.Float(*value)) return false;
			result.Operations.push_back(operation);
		}
		transform = std::move(result);
		return true;
	}

	bool WriteBrush(Writer& writer, const cui::drawing::Brush& brush)
	{
		if (brush.Kind == cui::drawing::BrushKind::Image) return false;
		const auto kind = static_cast<std::uint32_t>(brush.Kind);
		const auto mapping = static_cast<std::uint32_t>(brush.MappingMode);
		if (kind > static_cast<std::uint32_t>(
			cui::drawing::BrushKind::RadialGradient)
			|| mapping > static_cast<std::uint32_t>(
				cui::drawing::BrushMappingMode::RelativeToBoundingBox)) return false;
		writer.U32(kind);
		writer.U32(mapping);
		writer.U32(static_cast<std::uint32_t>(brush.ColorInterpolationMode));
		const float values[] = {
			brush.Color.r, brush.Color.g, brush.Color.b, brush.Color.a,
			brush.Opacity,
			brush.StartPoint.x, brush.StartPoint.y,
			brush.EndPoint.x, brush.EndPoint.y,
			brush.Center.x, brush.Center.y,
			brush.GradientOrigin.x, brush.GradientOrigin.y,
			brush.RadiusX, brush.RadiusY };
		for (const float value : values)
		{
			if (!std::isfinite(value)) return false;
			writer.Float(value);
		}
		if (brush.GradientStops.size() > MaxCollection) return false;
		writer.U64(static_cast<std::uint64_t>(brush.GradientStops.size()));
		for (const auto& stop : brush.GradientStops)
		{
			const float stopValues[] = {
				stop.Offset, stop.Color.r, stop.Color.g,
				stop.Color.b, stop.Color.a };
			for (const float value : stopValues)
			{
				if (!std::isfinite(value)) return false;
				writer.Float(value);
			}
		}
		if (!WriteTransform(writer, brush.Transform)
			|| !WriteTransform(writer, brush.RelativeTransform)) return false;
		writer.U32(static_cast<std::uint32_t>(brush.Stretch));
		writer.U32(static_cast<std::uint32_t>(brush.AlignmentX));
		writer.U32(static_cast<std::uint32_t>(brush.AlignmentY));
		return true;
	}

	bool ReadBrush(
		Reader& reader,
		cui::drawing::Brush& brush,
		bool allowGradientInterpolation)
	{
		std::uint32_t kind = 0;
		std::uint32_t mapping = 0;
		if (!reader.U32(kind) || !reader.U32(mapping)
			|| kind > static_cast<std::uint32_t>(
				cui::drawing::BrushKind::RadialGradient)
			|| mapping > static_cast<std::uint32_t>(
				cui::drawing::BrushMappingMode::RelativeToBoundingBox)) return false;
		brush.Kind = static_cast<cui::drawing::BrushKind>(kind);
		brush.MappingMode = static_cast<cui::drawing::BrushMappingMode>(mapping);
		if (allowGradientInterpolation)
		{
			std::uint32_t interpolation = 0;
			if (!reader.U32(interpolation)
				|| interpolation > static_cast<std::uint32_t>(
					cui::drawing::GradientColorInterpolationMode::SRgbLinearInterpolation))
				return false;
			brush.ColorInterpolationMode = static_cast<
				cui::drawing::GradientColorInterpolationMode>(interpolation);
		}
		float* values[] = {
			&brush.Color.r, &brush.Color.g, &brush.Color.b, &brush.Color.a,
			&brush.Opacity,
			&brush.StartPoint.x, &brush.StartPoint.y,
			&brush.EndPoint.x, &brush.EndPoint.y,
			&brush.Center.x, &brush.Center.y,
			&brush.GradientOrigin.x, &brush.GradientOrigin.y,
			&brush.RadiusX, &brush.RadiusY };
		for (auto* value : values)
			if (!reader.Float(*value)) return false;
		std::uint64_t stopCount = 0;
		if (!reader.U64(stopCount) || stopCount > MaxCollection) return false;
		brush.GradientStops.reserve(static_cast<std::size_t>(stopCount));
		for (std::uint64_t index = 0; index < stopCount; ++index)
		{
			cui::drawing::GradientStop stop;
			float* stopValues[] = {
				&stop.Offset, &stop.Color.r, &stop.Color.g,
				&stop.Color.b, &stop.Color.a };
			for (auto* value : stopValues)
				if (!reader.Float(*value)) return false;
			brush.GradientStops.push_back(stop);
		}
		if (!ReadTransform(reader, brush.Transform)
			|| !ReadTransform(reader, brush.RelativeTransform)) return false;
		std::uint32_t stretch = 0;
		std::uint32_t alignmentX = 0;
		std::uint32_t alignmentY = 0;
		if (!reader.U32(stretch) || !reader.U32(alignmentX)
			|| !reader.U32(alignmentY)
			|| stretch > static_cast<std::uint32_t>(Stretch::UniformToFill)
			|| alignmentX > static_cast<std::uint32_t>(
				cui::drawing::ImageBrushAlignmentX::Right)
			|| alignmentY > static_cast<std::uint32_t>(
				cui::drawing::ImageBrushAlignmentY::Bottom)) return false;
		brush.Stretch = static_cast<Stretch>(stretch);
		brush.AlignmentX =
			static_cast<cui::drawing::ImageBrushAlignmentX>(alignmentX);
		brush.AlignmentY =
			static_cast<cui::drawing::ImageBrushAlignmentY>(alignmentY);
		return true;
	}

	enum StyleMask : std::uint32_t
	{
		Foreground = 1u << 0,
		Background = 1u << 1,
		FontFamily = 1u << 2,
		FontSize = 1u << 3,
		FontWeight = 1u << 4,
		FontStyle = 1u << 5,
		Underline = 1u << 6,
		Strikethrough = 1u << 7,
		FontStretch = 1u << 8,
		Language = 1u << 9,
		KnownStyleMask = (1u << 10) - 1,
	};

	bool WriteStyle(Writer& writer, const RichTextCharacterStyle& style)
	{
		if (!style.Validate()) return false;
		std::uint32_t mask = 0;
		if (style.Foreground) mask |= Foreground;
		if (style.Background) mask |= Background;
		if (style.FontFamily) mask |= FontFamily;
		if (style.FontSize) mask |= FontSize;
		if (style.FontWeight) mask |= FontWeight;
		if (style.FontStyle) mask |= FontStyle;
		if (style.Underline) mask |= Underline;
		if (style.Strikethrough) mask |= Strikethrough;
		if (style.FontStretch) mask |= FontStretch;
		if (style.Language) mask |= Language;
		writer.U32(mask);
		if (style.Foreground && !WriteBrush(writer, *style.Foreground)) return false;
		if (style.Background && !WriteBrush(writer, *style.Background)) return false;
		if (style.FontFamily && !writer.String(*style.FontFamily)) return false;
		if (style.FontSize) writer.Float(*style.FontSize);
		if (style.FontWeight)
			writer.U32(static_cast<std::uint32_t>(*style.FontWeight));
		if (style.FontStyle)
			writer.U32(static_cast<std::uint32_t>(*style.FontStyle));
		if (style.Underline) writer.U8(*style.Underline ? 1 : 0);
		if (style.Strikethrough) writer.U8(*style.Strikethrough ? 1 : 0);
		if (style.FontStretch)
			writer.U32(static_cast<std::uint32_t>(*style.FontStretch));
		if (style.Language && !writer.String(*style.Language)) return false;
		return true;
	}

	bool ReadStyle(
		Reader& reader,
		RichTextCharacterStyle& style,
		bool allowFontStretch,
		bool allowLanguage,
		bool allowGradientInterpolation)
	{
		std::uint32_t mask = 0;
		std::uint32_t allowedMask = KnownStyleMask;
		if (!allowFontStretch) allowedMask &= ~FontStretch;
		if (!allowLanguage) allowedMask &= ~Language;
		if (!reader.U32(mask) || (mask & ~allowedMask) != 0) return false;
		if ((mask & Foreground) != 0)
		{
			cui::drawing::Brush value;
			if (!ReadBrush(reader, value, allowGradientInterpolation)) return false;
			style.Foreground = std::move(value);
		}
		if ((mask & Background) != 0)
		{
			cui::drawing::Brush value;
			if (!ReadBrush(reader, value, allowGradientInterpolation)) return false;
			style.Background = std::move(value);
		}
		if ((mask & FontFamily) != 0)
		{
			std::wstring value;
			if (!reader.String(value)) return false;
			style.FontFamily = std::move(value);
		}
		if ((mask & FontSize) != 0)
		{
			float value = 0.0f;
			if (!reader.Float(value)) return false;
			style.FontSize = value;
		}
		if ((mask & FontWeight) != 0)
		{
			std::uint32_t value = 0;
			if (!reader.U32(value) || value < 1 || value > 999) return false;
			style.FontWeight = static_cast<DWRITE_FONT_WEIGHT>(value);
		}
		if ((mask & FontStyle) != 0)
		{
			std::uint32_t value = 0;
			if (!reader.U32(value) || value > static_cast<std::uint32_t>(
				DWRITE_FONT_STYLE_ITALIC)) return false;
			style.FontStyle = static_cast<DWRITE_FONT_STYLE>(value);
		}
		auto readBoolean = [&reader](std::optional<bool>& target)
		{
			std::uint8_t value = 0;
			if (!reader.U8(value) || value > 1) return false;
			target = value != 0;
			return true;
		};
		if ((mask & Underline) != 0 && !readBoolean(style.Underline)) return false;
		if ((mask & Strikethrough) != 0
			&& !readBoolean(style.Strikethrough)) return false;
		if ((mask & FontStretch) != 0)
		{
			std::uint32_t value = 0;
			if (!reader.U32(value)
				|| value < static_cast<std::uint32_t>(
					DWRITE_FONT_STRETCH_ULTRA_CONDENSED)
				|| value > static_cast<std::uint32_t>(
					DWRITE_FONT_STRETCH_ULTRA_EXPANDED))
				return false;
			style.FontStretch = static_cast<DWRITE_FONT_STRETCH>(value);
		}
		if ((mask & Language) != 0)
		{
			std::wstring value;
			if (!reader.String(value)
				|| !IsCanonicalRichTextLanguageTag(value)) return false;
			style.Language = std::move(value);
		}
		return style.Validate();
	}

	template<typename TValue>
	void OverlayValue(
		std::optional<TValue>& target,
		const std::optional<TValue>& value)
	{
		if (value) target = value;
	}

	void OverlayStyle(
		RichTextCharacterStyle& target,
		const RichTextCharacterStyle& value)
	{
		OverlayValue(target.Foreground, value.Foreground);
		OverlayValue(target.Background, value.Background);
		OverlayValue(target.FontFamily, value.FontFamily);
		OverlayValue(target.Language, value.Language);
		OverlayValue(target.FontSize, value.FontSize);
		OverlayValue(target.FontWeight, value.FontWeight);
		OverlayValue(target.FontStretch, value.FontStretch);
		OverlayValue(target.FontStyle, value.FontStyle);
		OverlayValue(target.Underline, value.Underline);
		OverlayValue(target.Strikethrough, value.Strikethrough);
	}

	void ApplyIntrinsicStyle(
		RichTextStructureKind kind,
		RichTextCharacterStyle& style)
	{
		switch (kind)
		{
		case RichTextStructureKind::Bold:
			style.FontWeight = DWRITE_FONT_WEIGHT_BOLD;
			break;
		case RichTextStructureKind::Italic:
			style.FontStyle = DWRITE_FONT_STYLE_ITALIC;
			break;
		case RichTextStructureKind::Underline:
			style.Underline = true;
			break;
		default:
			break;
		}
	}

	RichTextCharacterStyle EffectivePathStyle(
		const RichTextCharacterStyle& root,
		const std::vector<RichTextStructureNode>& path)
	{
		auto result = root;
		for (const auto& node : path)
		{
			ApplyIntrinsicStyle(node.Kind, result);
			OverlayStyle(result, node.LocalStyle);
		}
		return result;
	}

	RichTextParagraphStyle EffectiveParagraphStyle(
		const RichTextParagraphStyle& root,
		const std::vector<RichTextStructureNode>& path)
	{
		auto result = root;
		if (!path.empty() && path.front().LocalParagraphStyle.TextAlignment)
			result.TextAlignment =
				path.front().LocalParagraphStyle.TextAlignment;
		if (!path.empty() && path.front().LocalParagraphStyle.FlowDirection)
			result.FlowDirection =
				path.front().LocalParagraphStyle.FlowDirection;
		return result;
	}

	void PreserveParagraphStyle(
		RichTextStructureNode& paragraph,
		const RichTextParagraphStyle& desired,
		const RichTextParagraphStyle& targetRoot)
	{
		paragraph.LocalParagraphStyle = {};
		if (desired.TextAlignment.value_or(::TextAlignment::Left)
			!= targetRoot.TextAlignment.value_or(::TextAlignment::Left))
		{
			paragraph.LocalParagraphStyle.TextAlignment =
				desired.TextAlignment.value_or(::TextAlignment::Left);
		}
		if (desired.FlowDirection.value_or(::FlowDirection::LeftToRight)
			!= targetRoot.FlowDirection.value_or(
				::FlowDirection::LeftToRight))
		{
			paragraph.LocalParagraphStyle.FlowDirection =
				desired.FlowDirection.value_or(
					::FlowDirection::LeftToRight);
		}
	}

	template<typename TValue>
	void PreserveDesiredValue(
		std::optional<TValue>& local,
		const std::optional<TValue>& effective,
		const std::optional<TValue>& desired)
	{
		if (desired && effective != desired) local = desired;
	}

	void PreserveDesiredStyle(
		RichTextCharacterStyle& terminal,
		const RichTextCharacterStyle& effective,
		const RichTextCharacterStyle& desired)
	{
		PreserveDesiredValue(
			terminal.Foreground, effective.Foreground, desired.Foreground);
		PreserveDesiredValue(
			terminal.Background, effective.Background, desired.Background);
		PreserveDesiredValue(
			terminal.FontFamily, effective.FontFamily, desired.FontFamily);
		PreserveDesiredValue(
			terminal.Language, effective.Language, desired.Language);
		PreserveDesiredValue(
			terminal.FontSize, effective.FontSize, desired.FontSize);
		PreserveDesiredValue(
			terminal.FontWeight, effective.FontWeight, desired.FontWeight);
		PreserveDesiredValue(
			terminal.FontStretch, effective.FontStretch, desired.FontStretch);
		PreserveDesiredValue(
			terminal.FontStyle, effective.FontStyle, desired.FontStyle);
		PreserveDesiredValue(
			terminal.Underline, effective.Underline, desired.Underline);
		PreserveDesiredValue(terminal.Strikethrough,
			effective.Strikethrough, desired.Strikethrough);
	}

	void AppendStyleSpan(
		std::vector<RichTextStyleSpan>& spans,
		RichTextStyleSpan span)
	{
		if (span.Length == 0) return;
		if (!spans.empty() && spans.back().End() == span.Start
			&& spans.back().Style == span.Style)
		{
			spans.back().Length += span.Length;
			return;
		}
		spans.push_back(std::move(span));
	}

	void AppendStructureSpan(
		std::vector<RichTextStructureSpan>& spans,
		RichTextStructureSpan span)
	{
		if (span.Length == 0) return;
		if (!spans.empty() && spans.back().End() == span.Start
			&& spans.back().Path == span.Path)
		{
			spans.back().Length += span.Length;
			return;
		}
		spans.push_back(std::move(span));
	}

	std::optional<RichTextDocumentFragment> RewritePortableStructure(
		const RichTextDocumentFragment& source,
		std::vector<RichTextStyleSpan> desiredSpans,
		const RichTextCharacterStyle& targetRootStyle,
		const RichTextParagraphStyle& targetRootParagraphStyle,
		std::uint64_t targetRootId)
	{
		if (!source.ValidateCanonical() || !targetRootStyle.Validate()
			|| !targetRootParagraphStyle.Validate()
			|| targetRootId == 0) return std::nullopt;
		RichTextDocumentFragment portableCheck;
		portableCheck.Text = source.Text;
		portableCheck.Spans = desiredSpans;
		if (!portableCheck.ValidateCanonical()) return std::nullopt;
		if (source.StructureSpans.empty()
			&& source.StructureMarkers.empty()) return portableCheck;

		RichTextDocumentFragment result;
		result.Text = source.Text;
		result.RootStyle = targetRootStyle;
		result.RootParagraphStyle = targetRootParagraphStyle;
		result.StructureRootId = targetRootId;
		const auto sourceRootParagraphStyle =
			source.RootParagraphStyle.value_or(RichTextParagraphStyle{});
		std::unordered_map<std::uint64_t, std::uint64_t> remappedIds;
		remappedIds.reserve((source.StructureSpans.size()
			+ source.StructureMarkers.size()) * 2);
		auto remapContainerId = [&](std::uint64_t sourceId)
		{
			const auto [found, inserted] = remappedIds.emplace(
				sourceId, 0);
			if (inserted) found->second = AllocateRichTextStructureId();
			return found->second;
		};

		std::size_t styleIndex = 0;
		for (const auto& sourceSpan : source.StructureSpans)
		{
			while (styleIndex < desiredSpans.size()
				&& desiredSpans[styleIndex].End() <= sourceSpan.Start)
				++styleIndex;
			std::size_t overlapIndex = styleIndex;
			while (overlapIndex < desiredSpans.size()
				&& desiredSpans[overlapIndex].Start < sourceSpan.End())
			{
				const auto& desiredSpan = desiredSpans[overlapIndex];
				const auto start = (std::max)(
					sourceSpan.Start, desiredSpan.Start);
				const auto end = (std::min)(
					sourceSpan.End(), desiredSpan.End());
				if (end > start)
				{
					auto path = sourceSpan.Path;
					const auto desiredParagraphStyle = EffectiveParagraphStyle(
						sourceRootParagraphStyle, sourceSpan.Path);
					for (std::size_t depth = 0; depth < path.size(); ++depth)
					{
						path[depth].Id = depth + 1 == path.size()
							? AllocateRichTextStructureId()
							: remapContainerId(sourceSpan.Path[depth].Id);
					}
					PreserveParagraphStyle(
						path.front(), desiredParagraphStyle,
						targetRootParagraphStyle);
					const auto before = EffectivePathStyle(
						targetRootStyle, path);
					PreserveDesiredStyle(
						path.back().LocalStyle, before, desiredSpan.Style);
					const auto effective = EffectivePathStyle(
						targetRootStyle, path);
					AppendStyleSpan(result.Spans,
						RichTextStyleSpan{ start, end - start, effective });
					AppendStructureSpan(result.StructureSpans,
						RichTextStructureSpan{
							start, end - start, std::move(path) });
				}
				++overlapIndex;
			}
		}
		const auto sourceRootStyle =
			source.RootStyle.value_or(RichTextCharacterStyle{});
		for (const auto& sourceMarker : source.StructureMarkers)
		{
			if (sourceMarker.Path.empty()) return std::nullopt;
			auto path = sourceMarker.Path;
			const auto desiredParagraphStyle = EffectiveParagraphStyle(
				sourceRootParagraphStyle, sourceMarker.Path);
			for (std::size_t depth = 0; depth < path.size(); ++depth)
			{
				path[depth].Id = depth + 1 == path.size()
					? AllocateRichTextStructureId()
					: remapContainerId(sourceMarker.Path[depth].Id);
			}
			PreserveParagraphStyle(
				path.front(), desiredParagraphStyle, targetRootParagraphStyle);
			const auto desired = EffectivePathStyle(
				sourceRootStyle, sourceMarker.Path);
			const auto before = EffectivePathStyle(targetRootStyle, path);
			PreserveDesiredStyle(
				path.back().LocalStyle, before, desired);
			result.StructureMarkers.push_back(RichTextStructureMarker{
				sourceMarker.Position, std::move(path) });
		}
		if (!result.ValidateCanonical()) return std::nullopt;
		return result;
	}

	class ClipboardScope final
	{
	public:
		explicit ClipboardScope(HWND owner) noexcept
			: Opened(OpenClipboard(owner) != FALSE) {}
		~ClipboardScope() { if (Opened) CloseClipboard(); }
		bool Opened = false;
	};

	HGLOBAL MakeGlobalMemory(const void* data, std::size_t size) noexcept
	{
		if (!data || size == 0) return nullptr;
		HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
		if (!memory) return nullptr;
		void* target = GlobalLock(memory);
		if (!target)
		{
			GlobalFree(memory);
			return nullptr;
		}
		std::memcpy(target, data, size);
		GlobalUnlock(memory);
		return memory;
	}

	std::optional<std::vector<std::uint8_t>> ReadBytes(UINT format) noexcept
	{
		if (format == 0 || !IsClipboardFormatAvailable(format))
			return std::nullopt;
		HGLOBAL memory = static_cast<HGLOBAL>(GetClipboardData(format));
		if (!memory) return std::nullopt;
		const SIZE_T size = GlobalSize(memory);
		if (size == 0 || size > MaxBytes) return std::nullopt;
		const auto* data = static_cast<const std::uint8_t*>(GlobalLock(memory));
		if (!data) return std::nullopt;
		try
		{
			std::vector<std::uint8_t> result(data, data + size);
			GlobalUnlock(memory);
			return result;
		}
		catch (...)
		{
			GlobalUnlock(memory);
			return std::nullopt;
		}
	}

	std::optional<std::wstring> ReadUnicodeText() noexcept
	{
		if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) return std::nullopt;
		HGLOBAL memory = static_cast<HGLOBAL>(GetClipboardData(CF_UNICODETEXT));
		if (!memory) return std::nullopt;
		const SIZE_T bytes = GlobalSize(memory);
		if (bytes < sizeof(wchar_t)
			|| bytes / sizeof(wchar_t) > MaxTextUnits + 1) return std::nullopt;
		const auto* data = static_cast<const wchar_t*>(GlobalLock(memory));
		if (!data) return std::nullopt;
		const std::size_t capacity = bytes / sizeof(wchar_t);
		std::size_t length = 0;
		while (length < capacity && data[length] != L'\0') ++length;
		if (length == capacity)
		{
			GlobalUnlock(memory);
			return std::nullopt;
		}
		try
		{
			std::wstring result(data, length);
			GlobalUnlock(memory);
			return result;
		}
		catch (...)
		{
			GlobalUnlock(memory);
			return std::nullopt;
		}
	}

	std::optional<std::wstring> ReadAnsiText() noexcept
	{
		if (!IsClipboardFormatAvailable(CF_TEXT)) return std::nullopt;
		HGLOBAL memory = static_cast<HGLOBAL>(GetClipboardData(CF_TEXT));
		if (!memory) return std::nullopt;
		const SIZE_T bytes = GlobalSize(memory);
		if (bytes == 0 || bytes > MaxBytes) return std::nullopt;
		const auto* data = static_cast<const char*>(GlobalLock(memory));
		if (!data) return std::nullopt;
		std::size_t length = 0;
		while (length < bytes && data[length] != '\0') ++length;
		if (length == bytes || length > static_cast<std::size_t>(
			(std::numeric_limits<int>::max)()))
		{
			GlobalUnlock(memory);
			return std::nullopt;
		}
		const int wideLength = MultiByteToWideChar(
			CP_ACP, 0, data, static_cast<int>(length), nullptr, 0);
		if (wideLength == 0 && length != 0)
		{
			GlobalUnlock(memory);
			return std::nullopt;
		}
		try
		{
			std::wstring result(static_cast<std::size_t>(wideLength), L'\0');
			if (wideLength > 0 && MultiByteToWideChar(
				CP_ACP, 0, data, static_cast<int>(length),
				result.data(), wideLength) == 0)
			{
				GlobalUnlock(memory);
				return std::nullopt;
			}
			GlobalUnlock(memory);
			return result;
		}
		catch (...)
		{
			GlobalUnlock(memory);
			return std::nullopt;
		}
	}

	std::optional<std::string> ReadRtfText() noexcept
	{
		auto bytes = ReadBytes(RtfFormat());
		if (!bytes || bytes->empty()) return std::nullopt;
		const auto terminator = std::find(bytes->begin(), bytes->end(), 0);
		const auto length = static_cast<std::size_t>(
			std::distance(bytes->begin(), terminator));
		if (length == 0 || length > MaxBytes) return std::nullopt;
		try
		{
			return std::string(
				reinterpret_cast<const char*>(bytes->data()), length);
		}
		catch (...)
		{
			return std::nullopt;
		}
	}

	bool PublishWin32(
		void* owner,
		const cui::richtext::clipboard::DataObject& data) noexcept
	{
		if (!data.PlainText || data.PlainText->empty()
			|| data.PlainText->size() > MaxTextUnits) return false;
		const auto textBytes = (data.PlainText->size() + 1) * sizeof(wchar_t);
		HGLOBAL textMemory = MakeGlobalMemory(
			data.PlainText->c_str(), textBytes);
		if (!textMemory) return false;
		const UINT attributedFormat = AttributedFormat();
		HGLOBAL attributedMemory = nullptr;
		if (data.Attributed && !data.Attributed->empty()
			&& data.Attributed->size() <= MaxBytes
			&& attributedFormat != 0)
		{
			attributedMemory = MakeGlobalMemory(
				data.Attributed->data(), data.Attributed->size());
		}
		const UINT rtfFormat = RtfFormat();
		HGLOBAL rtfMemory = nullptr;
		if (data.Rtf && !data.Rtf->empty()
			&& data.Rtf->size() < MaxBytes && rtfFormat != 0)
		{
			rtfMemory = MakeGlobalMemory(
				data.Rtf->c_str(), data.Rtf->size() + 1);
		}

		ClipboardScope clipboard(static_cast<HWND>(owner));
		if (!clipboard.Opened || !EmptyClipboard())
		{
			GlobalFree(textMemory);
			if (attributedMemory) GlobalFree(attributedMemory);
			if (rtfMemory) GlobalFree(rtfMemory);
			return false;
		}
		if (!SetClipboardData(CF_UNICODETEXT, textMemory))
		{
			GlobalFree(textMemory);
			if (attributedMemory) GlobalFree(attributedMemory);
			if (rtfMemory) GlobalFree(rtfMemory);
			return false;
		}
		textMemory = nullptr;
		if (attributedMemory)
		{
			if (SetClipboardData(attributedFormat, attributedMemory))
				attributedMemory = nullptr;
			else
				GlobalFree(attributedMemory);
		}
		if (rtfMemory)
		{
			if (SetClipboardData(rtfFormat, rtfMemory)) rtfMemory = nullptr;
			else GlobalFree(rtfMemory);
		}
		return true;
	}

	std::optional<cui::richtext::clipboard::DataObject> ReadWin32(
		void* owner) noexcept
	{
		ClipboardScope clipboard(static_cast<HWND>(owner));
		if (!clipboard.Opened) return std::nullopt;
		cui::richtext::clipboard::DataObject result;
		result.Attributed = ReadBytes(AttributedFormat());
		if (!result.Attributed)
			result.Attributed = ReadBytes(Version7AttributedFormat());
		if (!result.Attributed)
			result.Attributed = ReadBytes(Version6AttributedFormat());
		if (!result.Attributed)
			result.Attributed = ReadBytes(Version5AttributedFormat());
		if (!result.Attributed)
			result.Attributed = ReadBytes(Version4AttributedFormat());
		if (!result.Attributed)
			result.Attributed = ReadBytes(Version3AttributedFormat());
		if (!result.Attributed)
			result.Attributed = ReadBytes(Version2AttributedFormat());
		if (!result.Attributed)
			result.Attributed = ReadBytes(LegacyAttributedFormat());
		result.Rtf = ReadRtfText();
		result.PlainText = ReadUnicodeText();
		if (!result.PlainText) result.PlainText = ReadAnsiText();
		return result;
	}

	bool CanPasteWin32() noexcept
	{
		const UINT attributed = AttributedFormat();
		const UINT version7Attributed = Version7AttributedFormat();
		const UINT version6Attributed = Version6AttributedFormat();
		const UINT version5Attributed = Version5AttributedFormat();
		const UINT version4Attributed = Version4AttributedFormat();
		const UINT version3Attributed = Version3AttributedFormat();
		const UINT version2Attributed = Version2AttributedFormat();
		const UINT legacyAttributed = LegacyAttributedFormat();
		const UINT rtf = RtfFormat();
		return (attributed != 0
			&& IsClipboardFormatAvailable(attributed) != FALSE)
			|| (version7Attributed != 0
				&& IsClipboardFormatAvailable(version7Attributed) != FALSE)
			|| (version6Attributed != 0
				&& IsClipboardFormatAvailable(version6Attributed) != FALSE)
			|| (version5Attributed != 0
				&& IsClipboardFormatAvailable(version5Attributed) != FALSE)
			|| (version4Attributed != 0
				&& IsClipboardFormatAvailable(version4Attributed) != FALSE)
			|| (version3Attributed != 0
				&& IsClipboardFormatAvailable(version3Attributed) != FALSE)
			|| (version2Attributed != 0
				&& IsClipboardFormatAvailable(version2Attributed) != FALSE)
			|| (legacyAttributed != 0
				&& IsClipboardFormatAvailable(legacyAttributed) != FALSE)
			|| (rtf != 0 && IsClipboardFormatAvailable(rtf) != FALSE)
			|| IsClipboardFormatAvailable(CF_UNICODETEXT) != FALSE
			|| IsClipboardFormatAvailable(CF_TEXT) != FALSE;
	}

	class Win32Backend final
		: public cui::richtext::clipboard::Backend
	{
	public:
		bool Publish(void* owner,
			const cui::richtext::clipboard::DataObject& data) noexcept override
		{
			return PublishWin32(owner, data);
		}
		std::optional<cui::richtext::clipboard::DataObject> Read(
			void* owner) noexcept override
		{
			return ReadWin32(owner);
		}
		bool CanPaste() noexcept override { return CanPasteWin32(); }
	};

	thread_local cui::richtext::clipboard::Backend* OverrideBackend = nullptr;

	cui::richtext::clipboard::Backend& ActiveBackend() noexcept
	{
		if (OverrideBackend) return *OverrideBackend;
		static Win32Backend backend;
		return backend;
	}
}

namespace cui::richtext::clipboard
{
ScopedBackendOverride::ScopedBackendOverride(Backend& backend) noexcept
	: _installed(&backend), _previous(OverrideBackend)
{
	OverrideBackend = _installed;
}

ScopedBackendOverride::~ScopedBackendOverride()
{
	if (OverrideBackend != _installed) std::terminate();
	OverrideBackend = _previous;
}

std::optional<RichTextDocumentFragment> MakePortableStructuredFragment(
	const RichTextDocumentFragment& source,
	std::vector<RichTextStyleSpan> effectiveSpans) noexcept
{
	try
	{
		return RewritePortableStructure(
			source, std::move(effectiveSpans),
			RichTextCharacterStyle{}, RichTextParagraphStyle{},
			AllocateRichTextStructureId());
	}
	catch (...)
	{
		return std::nullopt;
	}
}

std::optional<RichTextDocumentFragment> RebaseStructureForInsertion(
	const RichTextDocumentFragment& source,
	const RichTextCharacterStyle& targetRootStyle,
	const RichTextParagraphStyle& targetRootParagraphStyle,
	std::uint64_t targetRootId) noexcept
{
	try
	{
		return RewritePortableStructure(
			source, source.Spans, targetRootStyle,
			targetRootParagraphStyle, targetRootId);
	}
	catch (...)
	{
		return std::nullopt;
	}
}

std::optional<std::vector<std::uint8_t>> Encode(
	const RichTextDocumentFragment& fragment) noexcept
{
	try
	{
		auto portable = MakePortableStructuredFragment(
			fragment, fragment.Spans);
		if (!portable) return std::nullopt;
		Writer payload;
		if (!payload.String(portable->Text)
			|| portable->Spans.size() > MaxSpans
			|| portable->StructureSpans.size() > MaxSpans
			|| portable->StructureMarkers.size() > MaxSpans)
			return std::nullopt;
		payload.U64(static_cast<std::uint64_t>(portable->Spans.size()));
		for (const auto& span : portable->Spans)
		{
			payload.U64(static_cast<std::uint64_t>(span.Start));
			payload.U64(static_cast<std::uint64_t>(span.Length));
			if (!WriteStyle(payload, span.Style)) return std::nullopt;
		}
		payload.U64(static_cast<std::uint64_t>(
			portable->StructureSpans.size()));
		std::unordered_map<std::uint64_t, std::uint64_t> portableIds;
		portableIds.reserve((portable->StructureSpans.size()
			+ portable->StructureMarkers.size()) * 2);
		std::vector<std::pair<std::uint64_t, ::TextAlignment>>
			paragraphStyles;
		std::vector<std::pair<std::uint64_t, ::FlowDirection>>
			paragraphDirections;
		std::uint64_t nextPortableId = 1;
		auto writePath = [&](const std::vector<RichTextStructureNode>& path)
		{
			if (path.empty() || path.size() > MaxStructureDepth)
				return false;
			payload.U64(static_cast<std::uint64_t>(path.size()));
			for (const auto& node : path)
			{
				const auto [found, inserted] = portableIds.emplace(
					node.Id, 0);
				if (inserted)
				{
					found->second = nextPortableId++;
					if (node.LocalParagraphStyle.TextAlignment)
					{
						paragraphStyles.emplace_back(found->second,
							*node.LocalParagraphStyle.TextAlignment);
					}
					if (node.LocalParagraphStyle.FlowDirection)
					{
						paragraphDirections.emplace_back(found->second,
							*node.LocalParagraphStyle.FlowDirection);
					}
				}
				payload.U64(found->second);
				payload.U8(static_cast<std::uint8_t>(node.Kind));
				if (!WriteStyle(payload, node.LocalStyle)) return false;
			}
			return true;
		};
		for (const auto& span : portable->StructureSpans)
		{
			payload.U64(static_cast<std::uint64_t>(span.Start));
			payload.U64(static_cast<std::uint64_t>(span.Length));
			if (!writePath(span.Path)) return std::nullopt;
		}
		payload.U64(static_cast<std::uint64_t>(
			portable->StructureMarkers.size()));
		for (const auto& marker : portable->StructureMarkers)
		{
			payload.U64(static_cast<std::uint64_t>(marker.Position));
			if (!writePath(marker.Path)) return std::nullopt;
		}
		payload.U64(static_cast<std::uint64_t>(paragraphStyles.size()));
		for (const auto& [id, alignment] : paragraphStyles)
		{
			payload.U64(id);
			payload.U8(static_cast<std::uint8_t>(alignment));
		}
		payload.U64(static_cast<std::uint64_t>(paragraphDirections.size()));
		for (const auto& [id, direction] : paragraphDirections)
		{
			payload.U64(id);
			payload.U8(static_cast<std::uint8_t>(direction));
		}
		if (payload.Data.size() > MaxBytes - HeaderSize) return std::nullopt;

		Writer result;
		result.Bytes(PortableMagic, sizeof(PortableMagic));
		result.U32(PortableVersion);
		result.U64(static_cast<std::uint64_t>(payload.Data.size()));
		result.U32(Checksum(payload.Data.data(), payload.Data.size()));
		result.U32(0);
		result.Bytes(payload.Data.data(), payload.Data.size());
		return std::move(result.Data);
	}
	catch (...)
	{
		return std::nullopt;
	}
}

std::optional<RichTextDocumentFragment> Decode(
	const std::vector<std::uint8_t>& bytes) noexcept
{
	if (bytes.size() < HeaderSize || bytes.size() > MaxBytes
		|| std::memcmp(bytes.data(), PortableMagic, sizeof(PortableMagic)) != 0)
		return std::nullopt;
	try
	{
		Reader header(bytes.data() + sizeof(PortableMagic),
			bytes.size() - sizeof(PortableMagic));
		std::uint32_t version = 0;
		std::uint64_t payloadSize = 0;
		std::uint32_t checksum = 0;
		std::uint32_t reserved = 0;
		if (!header.U32(version) || !header.U64(payloadSize)
			|| !header.U32(checksum) || !header.U32(reserved)
			|| (version != PortableVersion
				&& version != LanguagePortableVersion
				&& version != FontStretchPortableVersion
				&& version != DirectionPortableVersion
				&& version != ParagraphPortableVersion
				&& version != MarkerPortableVersion
				&& version != StructuredPortableVersion
				&& version != LegacyPortableVersion)
			|| reserved != 0
			|| payloadSize != bytes.size() - HeaderSize) return std::nullopt;
		const auto* payload = bytes.data() + HeaderSize;
		if (Checksum(payload, static_cast<std::size_t>(payloadSize)) != checksum)
			return std::nullopt;

		Reader reader(payload, static_cast<std::size_t>(payloadSize));
		// Serialized node identities are payload-local only. Decode always maps
		// them to fresh process-local identities.
		RichTextDocumentFragment fragment;
		if (!reader.String(fragment.Text)) return std::nullopt;
		std::uint64_t spanCount = 0;
		if (!reader.U64(spanCount) || spanCount > MaxSpans) return std::nullopt;
		fragment.Spans.reserve(static_cast<std::size_t>(spanCount));
		for (std::uint64_t index = 0; index < spanCount; ++index)
		{
			std::uint64_t start = 0;
			std::uint64_t length = 0;
			RichTextCharacterStyle style;
			if (!reader.U64(start) || !reader.U64(length)
				|| start > (std::numeric_limits<std::size_t>::max)()
				|| length > (std::numeric_limits<std::size_t>::max)()
				|| !ReadStyle(reader, style,
					version >= FontStretchPortableVersion,
					version >= LanguagePortableVersion,
					version >= GradientInterpolationPortableVersion))
				return std::nullopt;
			fragment.Spans.push_back(RichTextStyleSpan{
				static_cast<std::size_t>(start),
				static_cast<std::size_t>(length), std::move(style) });
		}
		if (version >= StructuredPortableVersion)
		{
			std::uint64_t structureCount = 0;
			if (!reader.U64(structureCount)
				|| structureCount > MaxSpans) return std::nullopt;
			fragment.StructureSpans.reserve(
				static_cast<std::size_t>(structureCount));
			std::unordered_map<std::uint64_t, std::uint64_t> decodedIds;
			decodedIds.reserve(static_cast<std::size_t>(structureCount) * 2);
			auto readPath = [&](std::vector<RichTextStructureNode>& path)
			{
				std::uint64_t pathCount = 0;
				if (!reader.U64(pathCount) || pathCount == 0
					|| pathCount > MaxStructureDepth) return false;
				path.reserve(static_cast<std::size_t>(pathCount));
				for (std::uint64_t depth = 0; depth < pathCount; ++depth)
				{
					std::uint64_t encodedId = 0;
					std::uint8_t kind = 0;
					RichTextCharacterStyle localStyle;
					if (!reader.U64(encodedId) || encodedId == 0
						|| !reader.U8(kind)
						|| kind > static_cast<std::uint8_t>(
							RichTextStructureKind::ParagraphBreak)
						|| !ReadStyle(reader, localStyle,
							version >= FontStretchPortableVersion,
							version >= LanguagePortableVersion,
							version >= GradientInterpolationPortableVersion))
						return false;
					const auto [found, inserted] = decodedIds.emplace(
						encodedId, 0);
					if (inserted)
						found->second = AllocateRichTextStructureId();
					path.push_back(RichTextStructureNode{
						found->second,
						static_cast<RichTextStructureKind>(kind),
						std::move(localStyle) });
				}
				return true;
			};
			for (std::uint64_t index = 0; index < structureCount; ++index)
			{
				std::uint64_t start = 0;
				std::uint64_t length = 0;
				if (!reader.U64(start) || !reader.U64(length)
					|| start > (std::numeric_limits<std::size_t>::max)()
					|| length > (std::numeric_limits<std::size_t>::max)())
					return std::nullopt;
				RichTextStructureSpan span;
				span.Start = static_cast<std::size_t>(start);
				span.Length = static_cast<std::size_t>(length);
				if (!readPath(span.Path)) return std::nullopt;
				fragment.StructureSpans.push_back(std::move(span));
			}
			std::uint64_t markerCount = 0;
			if (version >= MarkerPortableVersion)
			{
				if (!reader.U64(markerCount) || markerCount > MaxSpans)
					return std::nullopt;
				fragment.StructureMarkers.reserve(
					static_cast<std::size_t>(markerCount));
				for (std::uint64_t index = 0; index < markerCount; ++index)
				{
					std::uint64_t position = 0;
					if (!reader.U64(position)
						|| position > (std::numeric_limits<std::size_t>::max)())
					{
						return std::nullopt;
					}
					RichTextStructureMarker marker;
					marker.Position = static_cast<std::size_t>(position);
					if (!readPath(marker.Path)) return std::nullopt;
					fragment.StructureMarkers.push_back(std::move(marker));
				}
			}
			if (version >= ParagraphPortableVersion)
			{
				std::uint64_t paragraphStyleCount = 0;
				if (!reader.U64(paragraphStyleCount)
					|| paragraphStyleCount > MaxSpans)
					return std::nullopt;
				std::unordered_map<std::uint64_t, ::TextAlignment>
					paragraphStyles;
				paragraphStyles.reserve(
					static_cast<std::size_t>(paragraphStyleCount));
				for (std::uint64_t index = 0;
					index < paragraphStyleCount; ++index)
				{
					std::uint64_t encodedId = 0;
					std::uint8_t alignment = 0;
					if (!reader.U64(encodedId) || encodedId == 0
						|| !reader.U8(alignment)
						|| alignment > static_cast<std::uint8_t>(
							::TextAlignment::Justify))
						return std::nullopt;
					const auto id = decodedIds.find(encodedId);
					if (id == decodedIds.end()
						|| !paragraphStyles.emplace(
							id->second,
							static_cast<::TextAlignment>(alignment)).second)
						return std::nullopt;
				}
				std::unordered_map<std::uint64_t, bool> seen;
				seen.reserve(paragraphStyles.size());
				auto applyParagraphStyles = [&](auto& values)
				{
					for (auto& value : values)
					{
						for (auto& node : value.Path)
						{
							const auto style = paragraphStyles.find(node.Id);
							if (style == paragraphStyles.end()) continue;
							if (node.Kind != RichTextStructureKind::Paragraph)
								return false;
							node.LocalParagraphStyle.TextAlignment = style->second;
							seen[node.Id] = true;
						}
					}
					return true;
				};
				if (!applyParagraphStyles(fragment.StructureSpans)
					|| !applyParagraphStyles(fragment.StructureMarkers)
					|| seen.size() != paragraphStyles.size())
					return std::nullopt;
			}
			if (version >= DirectionPortableVersion)
			{
				std::uint64_t directionCount = 0;
				if (!reader.U64(directionCount) || directionCount > MaxSpans)
					return std::nullopt;
				std::unordered_map<std::uint64_t, ::FlowDirection> directions;
				directions.reserve(static_cast<std::size_t>(directionCount));
				for (std::uint64_t index = 0; index < directionCount; ++index)
				{
					std::uint64_t encodedId = 0;
					std::uint8_t direction = 0;
					if (!reader.U64(encodedId) || encodedId == 0
						|| !reader.U8(direction)
						|| direction > static_cast<std::uint8_t>(
							::FlowDirection::RightToLeft))
						return std::nullopt;
					const auto id = decodedIds.find(encodedId);
					if (id == decodedIds.end()
						|| !directions.emplace(id->second,
							static_cast<::FlowDirection>(direction)).second)
						return std::nullopt;
				}
				std::unordered_map<std::uint64_t, bool> seenDirections;
				seenDirections.reserve(directions.size());
				auto applyDirections = [&](auto& values)
				{
					for (auto& value : values)
					{
						for (auto& node : value.Path)
						{
							const auto direction = directions.find(node.Id);
							if (direction == directions.end()) continue;
							if (node.Kind != RichTextStructureKind::Paragraph)
								return false;
							node.LocalParagraphStyle.FlowDirection =
								direction->second;
							seenDirections[node.Id] = true;
						}
					}
					return true;
				};
				if (!applyDirections(fragment.StructureSpans)
					|| !applyDirections(fragment.StructureMarkers)
					|| seenDirections.size() != directions.size())
					return std::nullopt;
			}
			if (structureCount != 0 || markerCount != 0)
			{
				fragment.RootStyle = RichTextCharacterStyle{};
				fragment.RootParagraphStyle = RichTextParagraphStyle{};
				fragment.StructureRootId = AllocateRichTextStructureId();
			}
		}
		if (reader.Remaining() != 0 || !fragment.ValidateCanonical())
			return std::nullopt;
		return fragment;
	}
	catch (...)
	{
		return std::nullopt;
	}
}

bool Publish(void* owner, const RichTextDocumentFragment& fragment) noexcept
{
	try
	{
		DataObject data;
		data.PlainText = fragment.Text;
		data.Attributed = Encode(fragment);
		data.Rtf = cui::richtext::rtf::Encode(fragment);
		return Publish(owner, data);
	}
	catch (...)
	{
		return false;
	}
}

bool Publish(void* owner, const DataObject& data) noexcept
{
	return ActiveBackend().Publish(owner, data);
}

std::optional<DataObject> Read(void* owner) noexcept
{
	return ActiveBackend().Read(owner);
}

bool CanPaste() noexcept
{
	return ActiveBackend().CanPaste();
}
}
