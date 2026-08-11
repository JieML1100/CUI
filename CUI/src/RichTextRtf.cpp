#define NOMINMAX
#include "RichTextRtf.h"

#include "FlowDocument.h"
#include "RichTextClipboard.h"

#include <Windows.h>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cwctype>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
	constexpr std::size_t MaxRtfBytes = 64u * 1024u * 1024u;
	constexpr std::size_t MaxTextUnits = 16u * 1024u * 1024u;
	// Keep recursion comfortably below the default Windows thread stack. Real
	// word-processing payloads are normally only a few dozen groups deep.
	constexpr std::size_t MaxGroupDepth = 256u;
	constexpr std::size_t MaxTableEntries = 65536u;
	constexpr std::size_t MaxParagraphs = 65536u;
	constexpr std::size_t MaxInlineNodes = 1024u * 1024u;
	constexpr std::size_t MaxFontNameUnits = 1024u;

	using cui::drawing::Brush;
	using cui::drawing::BrushKind;

	std::optional<LCID> LocaleIdForLanguage(
		const std::optional<std::wstring>& language) noexcept
	{
		if (!language || language->empty()) return std::nullopt;
		const auto value = LocaleNameToLCID(
			language->c_str(), LOCALE_ALLOW_NEUTRAL_NAMES);
		return value == 0 ? std::nullopt : std::optional<LCID>(value);
	}

	std::optional<std::wstring> LanguageForLocaleId(int value)
	{
		if (value == 0) return std::wstring{};
		if (value < 0) return std::nullopt;
		wchar_t buffer[LOCALE_NAME_MAX_LENGTH]{};
		if (LCIDToLocaleName(static_cast<LCID>(value), buffer,
			LOCALE_NAME_MAX_LENGTH, 0) <= 0) return std::nullopt;
		return NormalizeRichTextLanguageTag(buffer);
	}

	Brush SolidColor(float red, float green, float blue)
	{
		return cui::drawing::MakeSolidColorBrush(
			D2D1::ColorF(red, green, blue, 1.0f));
	}

	RichTextCharacterStyle DefaultRtfStyle()
	{
		RichTextCharacterStyle result;
		result.Foreground = SolidColor(0.0f, 0.0f, 0.0f);
		result.Background = cui::drawing::NoBrush();
		result.FontFamily = L"Segoe UI";
		result.Language = L"en-us";
		// RTF's conventional default is 12 points; CUI FontSize is in DIPs.
		result.FontSize = 16.0f;
		result.FontWeight = DWRITE_FONT_WEIGHT_NORMAL;
		result.FontStretch = DWRITE_FONT_STRETCH_NORMAL;
		result.FontStyle = DWRITE_FONT_STYLE_NORMAL;
		result.Underline = false;
		result.Strikethrough = false;
		return result;
	}

	void ApplyStyle(TextElement& target, const RichTextCharacterStyle& style)
	{
		if (style.Foreground) target.SetForeground(*style.Foreground);
		if (style.Background) target.SetBackground(*style.Background);
		if (style.FontFamily) target.SetFontFamily(*style.FontFamily);
		if (style.Language) target.SetLanguage(*style.Language);
		if (style.FontSize) target.SetFontSize(*style.FontSize);
		if (style.FontWeight) target.SetFontWeight(*style.FontWeight);
		if (style.FontStretch) target.SetFontStretch(*style.FontStretch);
		if (style.FontStyle) target.SetFontStyle(*style.FontStyle);
		if (style.Underline) target.SetUnderline(*style.Underline);
		if (style.Strikethrough)
			target.SetStrikethrough(*style.Strikethrough);
	}

	std::uint8_t ColorByte(float value) noexcept
	{
		return static_cast<std::uint8_t>(std::lround(
			(std::clamp)(value, 0.0f, 1.0f) * 255.0f));
	}

	std::optional<std::uint32_t> SolidRgb(const std::optional<Brush>& brush)
	{
		if (!brush || brush->Kind != BrushKind::Solid) return std::nullopt;
		return (static_cast<std::uint32_t>(ColorByte(brush->Color.r)) << 16)
			| (static_cast<std::uint32_t>(ColorByte(brush->Color.g)) << 8)
			| static_cast<std::uint32_t>(ColorByte(brush->Color.b));
	}

	void AppendSignedUnicode(std::string& output, wchar_t value)
	{
		const auto encoded = static_cast<std::int16_t>(
			static_cast<std::uint16_t>(value));
		output += "\\u";
		output += std::to_string(static_cast<int>(encoded));
		output += '?';
	}

	void AppendEscapedText(std::string& output, std::wstring_view text)
	{
		for (std::size_t index = 0; index < text.size(); ++index)
		{
			const wchar_t value = text[index];
			if (value == L'\r' && index + 1 < text.size()
				&& text[index + 1] == L'\n')
			{
				output += "\\line ";
				++index;
				continue;
			}
			if (value == L'\t')
			{
				output += "\\tab ";
				continue;
			}
			if (value == L'\\' || value == L'{' || value == L'}')
			{
				output.push_back('\\');
				output.push_back(static_cast<char>(value));
				continue;
			}
			if (value >= 0x20 && value <= 0x7e)
			{
				output.push_back(static_cast<char>(value));
				continue;
			}
			AppendSignedUnicode(output, value);
		}
	}

	::TextAlignment EffectiveParagraphAlignment(
		const RichTextDocumentFragment& fragment,
		std::size_t position)
	{
		auto result = fragment.RootParagraphStyle
			&& fragment.RootParagraphStyle->TextAlignment
			? *fragment.RootParagraphStyle->TextAlignment
			: ::TextAlignment::Left;
		auto applyPath = [&](const std::vector<RichTextStructureNode>& path)
		{
			for (const auto& node : path)
				if (node.Kind == RichTextStructureKind::Paragraph
					&& node.LocalParagraphStyle.TextAlignment)
				{
					result = *node.LocalParagraphStyle.TextAlignment;
				}
		};
		for (const auto& marker : fragment.StructureMarkers)
		{
			if (marker.Position == position)
			{
				applyPath(marker.Path);
				return result;
			}
		}
		for (const auto& span : fragment.StructureSpans)
		{
			if ((span.Start <= position && position < span.End())
				|| (span.Start >= position))
			{
				applyPath(span.Path);
				return result;
			}
		}
		return result;
	}

	::FlowDirection EffectiveParagraphDirection(
		const RichTextDocumentFragment& fragment,
		std::size_t position)
	{
		auto result = fragment.RootParagraphStyle
			&& fragment.RootParagraphStyle->FlowDirection
			? *fragment.RootParagraphStyle->FlowDirection
			: ::FlowDirection::LeftToRight;
		auto applyPath = [&](const std::vector<RichTextStructureNode>& path)
		{
			for (const auto& node : path)
				if (node.Kind == RichTextStructureKind::Paragraph
					&& node.LocalParagraphStyle.FlowDirection)
				{
					result = *node.LocalParagraphStyle.FlowDirection;
				}
		};
		for (const auto& marker : fragment.StructureMarkers)
		{
			if (marker.Position == position)
			{
				applyPath(marker.Path);
				return result;
			}
		}
		for (const auto& span : fragment.StructureSpans)
		{
			if ((span.Start <= position && position < span.End())
				|| span.Start >= position)
			{
				applyPath(span.Path);
				return result;
			}
		}
		return result;
	}

	const RichTextStructureSpan* StructureAt(
		const RichTextDocumentFragment& fragment, std::size_t position)
	{
		for (const auto& span : fragment.StructureSpans)
			if (span.Start <= position && position < span.End()) return &span;
		return nullptr;
	}

	const RichTextStyleSpan* StyleAt(
		const RichTextDocumentFragment& fragment, std::size_t position)
	{
		for (const auto& span : fragment.Spans)
			if (span.Start <= position && position < span.End()) return &span;
		return nullptr;
	}

	void AppendParagraphControl(
		std::string& output, ::TextAlignment alignment,
		::FlowDirection direction)
	{
		output += "\\pard";
		output += direction == ::FlowDirection::RightToLeft
			? "\\rtlpar" : "\\ltrpar";
		switch (alignment)
		{
		case ::TextAlignment::Right: output += "\\qr "; break;
		case ::TextAlignment::Center: output += "\\qc "; break;
		case ::TextAlignment::Justify: output += "\\qj "; break;
		case ::TextAlignment::Left:
		default: output += "\\ql "; break;
		}
	}

	struct ParsedInline
	{
		bool LineBreak = false;
		std::wstring Text;
		RichTextCharacterStyle Style;
	};

	struct ParsedParagraph
	{
		::TextAlignment Alignment = ::TextAlignment::Left;
		::FlowDirection Direction = ::FlowDirection::LeftToRight;
		std::vector<ParsedInline> Inlines;
	};

	struct DecodeState
	{
		RichTextCharacterStyle Style = DefaultRtfStyle();
		::TextAlignment Alignment = ::TextAlignment::Left;
		::FlowDirection Direction = ::FlowDirection::LeftToRight;
		int FontIndex = 0;
		int UnicodeFallbackCount = 1;
		int SkipFallback = 0;
		bool Hidden = false;
	};

	class RtfDecoder final
	{
	public:
		explicit RtfDecoder(std::string_view input) : _input(input)
		{
			_paragraphs.emplace_back();
		}

		std::optional<RichTextDocumentFragment> Run()
		{
			if (_input.size() < 7 || _input.size() > MaxRtfBytes
				|| _input.front() != '{') return std::nullopt;
			if (PeekDestination(1).Name != "rtf") return std::nullopt;
			DecodeState state;
			++_position;
			if (!ParseGroup(state, 0)) return std::nullopt;
			while (_position < _input.size()
				&& std::isspace(static_cast<unsigned char>(_input[_position])))
			{
				++_position;
			}
			if (_position != _input.size() || !_sawRtfHeader
				|| !_hasContent) return std::nullopt;
			return BuildFragment();
		}

	private:
		static bool IsControlLetter(char value) noexcept
		{
			return (value >= 'a' && value <= 'z')
				|| (value >= 'A' && value <= 'Z');
		}

		static int HexValue(char value) noexcept
		{
			if (value >= '0' && value <= '9') return value - '0';
			if (value >= 'a' && value <= 'f') return value - 'a' + 10;
			if (value >= 'A' && value <= 'F') return value - 'A' + 10;
			return -1;
		}

		static std::wstring Trim(std::wstring value)
		{
			auto first = std::find_if_not(value.begin(), value.end(),
				[](wchar_t character) { return std::iswspace(character); });
			auto last = std::find_if_not(value.rbegin(), value.rend(),
				[](wchar_t character) { return std::iswspace(character); }).base();
			if (first >= last) return {};
			return std::wstring(first, last);
		}

		bool ConvertBytes(
			const std::string& bytes, std::wstring& output) const
		{
			if (bytes.empty()) return true;
			if (bytes.size() > static_cast<std::size_t>(
				(std::numeric_limits<int>::max)())) return false;
			const UINT page = _codePage > 0
				? static_cast<UINT>(_codePage) : 1252u;
			const int required = MultiByteToWideChar(page, 0,
				bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
			if (required <= 0) return false;
			const auto oldSize = output.size();
			if (oldSize + static_cast<std::size_t>(required) > MaxTextUnits)
				return false;
			output.resize(oldSize + static_cast<std::size_t>(required));
			return MultiByteToWideChar(page, 0,
				bytes.data(), static_cast<int>(bytes.size()),
				output.data() + oldSize, required) == required;
		}

		bool AppendText(std::wstring text, const DecodeState& state)
		{
			if (text.empty()) return true;
			if (state.Hidden) return true;
			if (text.find(L'\0') != std::wstring::npos) return false;
			if (_outputUnits + text.size() > MaxTextUnits) return false;
			_outputUnits += text.size();
			auto& paragraph = _paragraphs.back();
			paragraph.Alignment = state.Alignment;
			paragraph.Direction = state.Direction;
			auto resolvedStyle = state.Style;
			if (const auto font = _fonts.find(state.FontIndex);
				font != _fonts.end()) resolvedStyle.FontFamily = font->second;
			if (!paragraph.Inlines.empty()
				&& !paragraph.Inlines.back().LineBreak
				&& paragraph.Inlines.back().Style == resolvedStyle)
			{
				paragraph.Inlines.back().Text += text;
			}
			else
			{
				if (_inlineNodes >= MaxInlineNodes) return false;
				++_inlineNodes;
				paragraph.Inlines.push_back(ParsedInline{
					false, std::move(text), std::move(resolvedStyle) });
			}
			_hasContent = true;
			return true;
		}

		bool AppendCharacter(wchar_t value, const DecodeState& state)
		{
			return AppendText(std::wstring(1, value), state);
		}

		bool AppendLineBreak(const DecodeState& state)
		{
			if (state.Hidden) return true;
			if (_outputUnits + 2 > MaxTextUnits) return false;
			if (_inlineNodes >= MaxInlineNodes) return false;
			_outputUnits += 2;
			++_inlineNodes;
			auto& paragraph = _paragraphs.back();
			paragraph.Alignment = state.Alignment;
			paragraph.Direction = state.Direction;
			auto resolvedStyle = state.Style;
			if (const auto font = _fonts.find(state.FontIndex);
				font != _fonts.end()) resolvedStyle.FontFamily = font->second;
			paragraph.Inlines.push_back(ParsedInline{
				true, {}, std::move(resolvedStyle) });
			_hasContent = true;
			return true;
		}

		bool AppendParagraphBreak(const DecodeState& state)
		{
			if (state.Hidden) return true;
			if (_outputUnits + 2 > MaxTextUnits) return false;
			if (_paragraphs.size() >= MaxParagraphs) return false;
			_outputUnits += 2;
			_paragraphs.back().Alignment = state.Alignment;
			_paragraphs.back().Direction = state.Direction;
			_paragraphs.push_back(ParsedParagraph{
				state.Alignment, state.Direction, {} });
			_hasContent = true;
			return true;
		}

		std::optional<std::size_t> FindGroupEnd(std::size_t start) const
		{
			std::size_t depth = 1;
			for (std::size_t current = start; current < _input.size(); ++current)
			{
				const char value = _input[current];
				if (value == '\\')
				{
					if (current + 1 >= _input.size()) return std::nullopt;
					const char next = _input[current + 1];
					if (next == '\\' || next == '{' || next == '}')
					{
						++current;
						continue;
					}
					std::size_t probe = current + 1;
					std::string name;
					while (probe < _input.size()
						&& IsControlLetter(_input[probe]))
					{
						name.push_back(static_cast<char>(std::tolower(
							static_cast<unsigned char>(_input[probe++]))));
					}
					bool negative = false;
					if (probe < _input.size() && _input[probe] == '-')
					{
						negative = true;
						++probe;
					}
					std::size_t digits = probe;
					long long parameter = 0;
					while (probe < _input.size()
						&& std::isdigit(static_cast<unsigned char>(_input[probe])))
					{
						parameter = parameter * 10 + (_input[probe++] - '0');
						if (parameter > static_cast<long long>(MaxRtfBytes))
							return std::nullopt;
					}
					if (negative) parameter = -parameter;
					if (probe < _input.size() && _input[probe] == ' ') ++probe;
					if (name == "bin" && probe > digits && parameter >= 0)
					{
						const auto count = static_cast<std::size_t>(parameter);
						if (count > _input.size() - probe) return std::nullopt;
						current = probe + count - 1;
					}
					continue;
				}
				if (value == '{') ++depth;
				else if (value == '}' && --depth == 0) return current;
			}
			return std::nullopt;
		}

		struct Destination
		{
			std::string Name;
			bool Starred = false;
		};

		Destination PeekDestination(std::size_t position) const
		{
			Destination result;
			while (position < _input.size()
				&& (_input[position] == '\r' || _input[position] == '\n'))
			{
				++position;
			}
			if (position + 1 < _input.size()
				&& _input[position] == '\\' && _input[position + 1] == '*')
			{
				result.Starred = true;
				position += 2;
				while (position < _input.size()
					&& std::isspace(static_cast<unsigned char>(_input[position])))
					++position;
			}
			if (position >= _input.size() || _input[position] != '\\')
				return result;
			++position;
			while (position < _input.size()
				&& IsControlLetter(_input[position]))
			{
				result.Name.push_back(static_cast<char>(std::tolower(
					static_cast<unsigned char>(_input[position++]))));
			}
			return result;
		}

		static bool IsSkippedDestination(std::string_view name)
		{
			static constexpr std::string_view values[] = {
				"stylesheet", "info", "pict", "object", "header",
				"headerl", "headerr", "headerf", "footer", "footerl",
				"footerr", "footerf", "generator", "xmlnstbl",
				"listtable", "listoverridetable", "latentstyles",
				"colorschememapping", "themedata", "datastore",
				"datafield", "fldinst", "nonshppict", "shppict",
				"deleted", "annotation", "atnauthor", "atnid", "atnref",
				"footnote", "xe", "tc" };
			return std::find(std::begin(values), std::end(values), name)
				!= std::end(values);
		}

		std::wstring DecodeFontName(std::string_view value) const
		{
			std::string bytes;
			std::wstring result;
			int unicodeFallback = 1;
			int skipFallback = 0;
			auto flush = [&]()
			{
				if (!ConvertBytes(bytes, result)) return false;
				bytes.clear();
				return true;
			};
			for (std::size_t position = 0; position < value.size();)
			{
				const char current = value[position++];
				if (current == '{' || current == '}'
					|| current == '\r' || current == '\n') continue;
				if (current != '\\')
				{
					if (skipFallback > 0) --skipFallback;
					else bytes.push_back(current);
					continue;
				}
				if (position >= value.size()) return {};
				const char symbol = value[position];
				if (symbol == '\\' || symbol == '{' || symbol == '}')
				{
					if (skipFallback > 0) --skipFallback;
					else bytes.push_back(symbol);
					++position;
					continue;
				}
				if (symbol == '\'' && position + 2 < value.size())
				{
					const int high = HexValue(value[position + 1]);
					const int low = HexValue(value[position + 2]);
					if (high < 0 || low < 0) return {};
					if (skipFallback > 0) --skipFallback;
					else bytes.push_back(static_cast<char>((high << 4) | low));
					position += 3;
					continue;
				}
				if (!flush()) return {};
				std::string name;
				while (position < value.size() && IsControlLetter(value[position]))
					name.push_back(static_cast<char>(std::tolower(
						static_cast<unsigned char>(value[position++]))));
				bool negative = false;
				if (position < value.size() && value[position] == '-')
				{
					negative = true;
					++position;
				}
				int parameter = 0;
				bool hasParameter = false;
				while (position < value.size()
					&& std::isdigit(static_cast<unsigned char>(value[position])))
				{
					hasParameter = true;
					parameter = parameter * 10 + (value[position++] - '0');
				}
				if (negative) parameter = -parameter;
				if (position < value.size() && value[position] == ' ') ++position;
				if (name == "uc" && hasParameter
					&& parameter >= 0 && parameter <= 16)
				{
					unicodeFallback = parameter;
				}
				else if (name == "u" && hasParameter)
				{
					result.push_back(static_cast<wchar_t>(
						static_cast<std::uint16_t>(parameter)));
					skipFallback = unicodeFallback;
				}
			}
			if (!flush()) return {};
			result = Trim(std::move(result));
			return result.size() <= MaxFontNameUnits
				? std::move(result) : std::wstring{};
		}

		bool ParseFontTable(std::string_view group, DecodeState& state)
		{
			for (std::size_t position = 0; position + 2 < group.size(); ++position)
			{
				if (group[position] != '\\'
					|| (group[position + 1] != 'f'
						&& group[position + 1] != 'F')
					|| !std::isdigit(static_cast<unsigned char>(group[position + 2])))
					continue;
				std::size_t digits = position + 2;
				int id = 0;
				while (digits < group.size()
					&& std::isdigit(static_cast<unsigned char>(group[digits])))
				{
					if (id > 1000000) return false;
					id = id * 10 + (group[digits++] - '0');
				}
				const auto semicolon = group.find(';', digits);
				if (semicolon == std::string_view::npos) return false;
				auto name = DecodeFontName(group.substr(
					digits, semicolon - digits));
				if (!name.empty())
				{
					if (_fonts.size() >= MaxTableEntries
						&& !_fonts.contains(id)) return false;
					_fonts[id] = std::move(name);
				}
				position = semicolon;
			}
			if (const auto found = _fonts.find(_defaultFont);
				found != _fonts.end()) state.Style.FontFamily = found->second;
			return true;
		}

		bool ParseColorTable(std::string_view group)
		{
			int red = 0;
			int green = 0;
			int blue = 0;
			bool hasColor = false;
			for (std::size_t position = 0; position < group.size();)
			{
				if (group[position] == ';')
				{
					if (_colors.size() >= MaxTableEntries) return false;
					if (hasColor)
					{
						_colors.push_back(SolidColor(
							static_cast<float>(red) / 255.0f,
							static_cast<float>(green) / 255.0f,
							static_cast<float>(blue) / 255.0f));
					}
					else _colors.push_back(std::nullopt);
					red = green = blue = 0;
					hasColor = false;
					++position;
					continue;
				}
				if (group[position++] != '\\') continue;
				std::string name;
				while (position < group.size()
					&& IsControlLetter(group[position]))
				{
					name.push_back(static_cast<char>(std::tolower(
						static_cast<unsigned char>(group[position++]))));
				}
				int value = 0;
				bool hasValue = false;
				while (position < group.size()
					&& std::isdigit(static_cast<unsigned char>(group[position])))
				{
					hasValue = true;
					value = value * 10 + (group[position++] - '0');
					if (value > 255) return false;
				}
				if (!hasValue) continue;
				if (name == "red") { red = value; hasColor = true; }
				else if (name == "green") { green = value; hasColor = true; }
				else if (name == "blue") { blue = value; hasColor = true; }
			}
			return !_colors.empty();
		}

		bool HandleControl(
			std::string_view name, bool hasParameter, int parameter,
			DecodeState& state)
		{
			auto enabled = [&]() { return !hasParameter || parameter != 0; };
			if (name == "rtf")
			{
				_sawRtfHeader = hasParameter && parameter == 1;
				return _sawRtfHeader;
			}
			if (name == "ansi") { _codePage = 1252; return true; }
			if (name == "ansicpg")
			{
				if (!hasParameter || parameter <= 0 || parameter > 65535)
					return false;
				_codePage = parameter;
				return true;
			}
			if (name == "deff")
			{
				if (!hasParameter || parameter < 0) return false;
				_defaultFont = parameter;
				state.FontIndex = parameter;
				if (const auto found = _fonts.find(parameter);
					found != _fonts.end()) state.Style.FontFamily = found->second;
				return true;
			}
			if (name == "uc")
			{
				if (!hasParameter || parameter < 0 || parameter > 16)
					return false;
				state.UnicodeFallbackCount = parameter;
				return true;
			}
			if (name == "u")
			{
				if (!hasParameter || parameter < -32768 || parameter > 65535)
					return false;
				if (!AppendCharacter(static_cast<wchar_t>(
					static_cast<std::uint16_t>(parameter)), state)) return false;
				state.SkipFallback = state.UnicodeFallbackCount;
				return true;
			}
			if (name == "plain")
			{
				state.Style = DefaultRtfStyle();
				state.Style.Language = _defaultLanguage;
				state.FontIndex = _defaultFont;
				if (const auto found = _fonts.find(_defaultFont);
					found != _fonts.end()) state.Style.FontFamily = found->second;
				return true;
			}
			if (name == "pard")
			{
				state.Alignment = ::TextAlignment::Left;
				state.Direction = ::FlowDirection::LeftToRight;
				return true;
			}
			if (name == "deflang")
			{
				if (!hasParameter) return true;
				const auto language = LanguageForLocaleId(parameter);
				if (language)
				{
					_defaultLanguage = *language;
					state.Style.Language = *language;
				}
				return true;
			}
			if (name == "lang")
			{
				if (!hasParameter) return true;
				const auto language = LanguageForLocaleId(parameter);
				if (language) state.Style.Language = *language;
				return true;
			}
			if (name == "ql") { state.Alignment = ::TextAlignment::Left; return true; }
			if (name == "qr") { state.Alignment = ::TextAlignment::Right; return true; }
			if (name == "qc") { state.Alignment = ::TextAlignment::Center; return true; }
			if (name == "qj") { state.Alignment = ::TextAlignment::Justify; return true; }
			if (name == "ltrpar")
			{
				state.Direction = ::FlowDirection::LeftToRight;
				return true;
			}
			if (name == "rtlpar")
			{
				state.Direction = ::FlowDirection::RightToLeft;
				return true;
			}
			if (name == "par") return AppendParagraphBreak(state);
			if (name == "line") return AppendLineBreak(state);
			if (name == "tab") return AppendCharacter(L'\t', state);
			if (name == "f")
			{
				if (hasParameter && parameter >= 0)
				{
					state.FontIndex = parameter;
					if (const auto found = _fonts.find(parameter);
						found != _fonts.end()) state.Style.FontFamily = found->second;
				}
				return true;
			}
			if (name == "fs")
			{
				if (hasParameter && parameter > 0)
				{
					const float size = static_cast<float>(parameter) * (2.0f / 3.0f);
					if (size >= 1.0f / 300.0f && size <= 160000.0f)
						state.Style.FontSize = size;
				}
				return true;
			}
			if (name == "b")
			{
				state.Style.FontWeight = enabled()
					? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
				return true;
			}
			if (name == "i")
			{
				state.Style.FontStyle = enabled()
					? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;
				return true;
			}
			if (name == "ul") { state.Style.Underline = enabled(); return true; }
			if (name == "ulnone") { state.Style.Underline = false; return true; }
			if (name == "strike") { state.Style.Strikethrough = enabled(); return true; }
			if (name == "v") { state.Hidden = enabled(); return true; }
			if (name == "cf")
			{
				if (!hasParameter || parameter <= 0
					|| static_cast<std::size_t>(parameter) >= _colors.size()
					|| !_colors[static_cast<std::size_t>(parameter)])
					state.Style.Foreground = SolidColor(0.0f, 0.0f, 0.0f);
				else state.Style.Foreground =
					*_colors[static_cast<std::size_t>(parameter)];
				return true;
			}
			if (name == "highlight" || name == "cb")
			{
				if (!hasParameter || parameter <= 0
					|| static_cast<std::size_t>(parameter) >= _colors.size()
					|| !_colors[static_cast<std::size_t>(parameter)])
					state.Style.Background = cui::drawing::NoBrush();
				else state.Style.Background =
					*_colors[static_cast<std::size_t>(parameter)];
				return true;
			}
			if (name == "emdash") return AppendCharacter(0x2014, state);
			if (name == "endash") return AppendCharacter(0x2013, state);
			if (name == "lquote") return AppendCharacter(0x2018, state);
			if (name == "rquote") return AppendCharacter(0x2019, state);
			if (name == "ldblquote") return AppendCharacter(0x201c, state);
			if (name == "rdblquote") return AppendCharacter(0x201d, state);
			if (name == "bullet") return AppendCharacter(0x2022, state);
			return true;
		}

		bool ParseGroup(DecodeState state, std::size_t depth)
		{
			if (depth > MaxGroupDepth) return false;
			const auto groupStart = _position;
			const auto destination = PeekDestination(groupStart);
			if (destination.Name == "fonttbl" || destination.Name == "colortbl"
				|| destination.Starred || IsSkippedDestination(destination.Name))
			{
				const auto end = FindGroupEnd(groupStart);
				if (!end) return false;
				const auto group = _input.substr(groupStart, *end - groupStart);
				if (destination.Name == "fonttbl")
				{
					if (!ParseFontTable(group, state)) return false;
				}
				else if (destination.Name == "colortbl")
				{
					if (!ParseColorTable(group)) return false;
				}
				_position = *end + 1;
				return true;
			}

			std::string ansiBytes;
			auto flush = [&]()
			{
				if (ansiBytes.empty()) return true;
				std::wstring text;
				if (!ConvertBytes(ansiBytes, text)) return false;
				ansiBytes.clear();
				return AppendText(std::move(text), state);
			};
			while (_position < _input.size())
			{
				const char current = _input[_position++];
				if (current == '}') return flush();
				if (current == '{')
				{
					if (!flush() || !ParseGroup(state, depth + 1)) return false;
					continue;
				}
				if (current == '\r' || current == '\n') continue;
				if (current != '\\')
				{
					if (state.SkipFallback > 0) --state.SkipFallback;
					else ansiBytes.push_back(current);
					continue;
				}
				if (_position >= _input.size()) return false;
				const char symbol = _input[_position];
				if (symbol == '\\' || symbol == '{' || symbol == '}')
				{
					++_position;
					if (state.SkipFallback > 0) --state.SkipFallback;
					else ansiBytes.push_back(symbol);
					continue;
				}
				if (symbol == '\'' && _position + 2 < _input.size())
				{
					const int high = HexValue(_input[_position + 1]);
					const int low = HexValue(_input[_position + 2]);
					if (high < 0 || low < 0) return false;
					_position += 3;
					if (state.SkipFallback > 0) --state.SkipFallback;
					else ansiBytes.push_back(static_cast<char>((high << 4) | low));
					continue;
				}
				if (!flush()) return false;
				if (symbol == '~')
				{
					++_position;
					if (!AppendCharacter(0x00a0, state)) return false;
					continue;
				}
				if (symbol == '-') { ++_position; continue; }
				if (symbol == '_')
				{
					++_position;
					if (!AppendCharacter(0x2011, state)) return false;
					continue;
				}
				if (symbol == '*' || symbol == '\r' || symbol == '\n')
				{
					++_position;
					continue;
				}
				std::string name;
				while (_position < _input.size()
					&& IsControlLetter(_input[_position]))
				{
					name.push_back(static_cast<char>(std::tolower(
						static_cast<unsigned char>(_input[_position++]))));
				}
				if (name.empty())
				{
					++_position;
					continue;
				}
				bool negative = false;
				if (_position < _input.size() && _input[_position] == '-')
				{
					negative = true;
					++_position;
				}
				long long numeric = 0;
				bool hasParameter = false;
				while (_position < _input.size()
					&& std::isdigit(static_cast<unsigned char>(_input[_position])))
				{
					hasParameter = true;
					numeric = numeric * 10 + (_input[_position++] - '0');
					if (numeric > (std::numeric_limits<int>::max)()) return false;
				}
				if (negative) numeric = -numeric;
				if (_position < _input.size() && _input[_position] == ' ')
					++_position;
				if (name == "bin")
				{
					if (!hasParameter || numeric < 0
						|| static_cast<std::size_t>(numeric)
							> _input.size() - _position) return false;
					_position += static_cast<std::size_t>(numeric);
					continue;
				}
				if (!HandleControl(name, hasParameter,
					static_cast<int>(numeric), state)) return false;
			}
			return false;
		}

		std::optional<RichTextDocumentFragment> BuildFragment()
		{
			try
			{
				FlowDocument document;
				for (auto& parsed : _paragraphs)
				{
					auto& paragraph = document.Blocks.AddParagraph();
					paragraph.SetTextAlignment(parsed.Alignment);
					paragraph.SetFlowDirection(parsed.Direction);
					for (auto& inlineValue : parsed.Inlines)
					{
						if (inlineValue.LineBreak)
						{
							auto line = std::make_unique<LineBreak>();
							ApplyStyle(*line, inlineValue.Style);
							paragraph.Inlines.Add(std::move(line));
						}
						else
						{
							auto run = std::make_unique<::Run>(
								std::move(inlineValue.Text));
							ApplyStyle(*run, inlineValue.Style);
							paragraph.Inlines.Add(std::move(run));
						}
					}
				}
				auto flattened = document.Flatten();
				return cui::richtext::clipboard::MakePortableStructuredFragment(
					flattened, flattened.Spans);
			}
			catch (...)
			{
				return std::nullopt;
			}
		}

		std::string_view _input;
		std::size_t _position = 0;
		std::size_t _outputUnits = 0;
		std::size_t _inlineNodes = 0;
		int _codePage = 1252;
		int _defaultFont = 0;
		std::wstring _defaultLanguage = L"en-us";
		bool _sawRtfHeader = false;
		bool _hasContent = false;
		std::unordered_map<int, std::wstring> _fonts;
		std::vector<std::optional<Brush>> _colors;
		std::vector<ParsedParagraph> _paragraphs;
	};
}

namespace cui::richtext::rtf
{
std::optional<std::string> Encode(
	const RichTextDocumentFragment& fragment) noexcept
{
	try
	{
		if (fragment.Empty() || !fragment.ValidateCanonical())
			return std::nullopt;
		if (fragment.Text.size() > MaxTextUnits
			|| fragment.Spans.size() > MaxInlineNodes) return std::nullopt;
		std::vector<std::wstring> fonts{ L"Segoe UI" };
		std::map<std::wstring, int> fontIndexes{ { fonts.front(), 0 } };
		std::vector<std::uint32_t> colors;
		std::map<std::uint32_t, int> colorIndexes;
		bool tablesValid = true;
		auto addFont = [&](const std::optional<std::wstring>& font)
		{
			if (!font || font->empty() || fontIndexes.contains(*font)) return;
			if (font->size() > MaxFontNameUnits
				|| fonts.size() >= MaxTableEntries
				|| std::any_of(font->begin(), font->end(), [](wchar_t value)
				{
					return value < 0x20 || value == L';';
				}))
			{
				tablesValid = false;
				return;
			}
			const int index = static_cast<int>(fonts.size());
			fonts.push_back(*font);
			fontIndexes.emplace(*font, index);
		};
		auto addColor = [&](const std::optional<Brush>& brush)
		{
			const auto value = SolidRgb(brush);
			if (!value || colorIndexes.contains(*value)) return;
			if (colors.size() >= MaxTableEntries)
			{
				tablesValid = false;
				return;
			}
			const int index = static_cast<int>(colors.size()) + 1;
			colors.push_back(*value);
			colorIndexes.emplace(*value, index);
		};
		for (const auto& span : fragment.Spans)
		{
			addFont(span.Style.FontFamily);
			addColor(span.Style.Foreground);
			addColor(span.Style.Background);
		}
		if (!tablesValid) return std::nullopt;

		std::string output =
			"{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033\\uc1\n{\\fonttbl";
		for (std::size_t index = 0; index < fonts.size(); ++index)
		{
			output += "{\\f" + std::to_string(index) + "\\fnil ";
			AppendEscapedText(output, fonts[index]);
			output += ";}";
			if (output.size() > MaxRtfBytes) return std::nullopt;
		}
		output += "}\n{\\colortbl;";
		for (const auto value : colors)
		{
			output += "\\red" + std::to_string((value >> 16) & 0xff)
				+ "\\green" + std::to_string((value >> 8) & 0xff)
				+ "\\blue" + std::to_string(value & 0xff) + ";";
			if (output.size() > MaxRtfBytes) return std::nullopt;
		}
		output += "}\n";
		AppendParagraphControl(output,
			EffectiveParagraphAlignment(fragment, 0),
			EffectiveParagraphDirection(fragment, 0));

		std::size_t position = 0;
		while (position < fragment.Text.size())
		{
			const auto* structure = StructureAt(fragment, position);
			const auto kind = structure && !structure->Path.empty()
				? structure->Path.back().Kind : RichTextStructureKind::Run;
			const bool canonicalBreak = position + 1 < fragment.Text.size()
				&& fragment.Text[position] == L'\r'
				&& fragment.Text[position + 1] == L'\n';
			if ((kind == RichTextStructureKind::ParagraphBreak
					|| (!structure && canonicalBreak)) && canonicalBreak)
			{
				output += "\\par\n";
				position += 2;
				if (position < fragment.Text.size())
					AppendParagraphControl(output,
						EffectiveParagraphAlignment(fragment, position),
						EffectiveParagraphDirection(fragment, position));
				continue;
			}
			if (kind == RichTextStructureKind::LineBreak && canonicalBreak)
			{
				output += "\\line ";
				position += 2;
				continue;
			}

			const auto* styleSpan = StyleAt(fragment, position);
			if (!styleSpan) return std::nullopt;
			std::size_t end = styleSpan->End();
			if (structure) end = (std::min)(end, structure->End());
			if (end <= position) return std::nullopt;
			const auto& style = styleSpan->Style;
			const auto family = style.FontFamily.value_or(fonts.front());
			const auto font = fontIndexes.find(family);
			const int fontIndex = font != fontIndexes.end() ? font->second : 0;
			const float dipSize = style.FontSize.value_or(16.0f);
			const int halfPoints = (std::clamp)(
				static_cast<int>(std::lround(dipSize * 1.5f)), 1, 32767);
			const bool bold = style.FontWeight.value_or(
				DWRITE_FONT_WEIGHT_NORMAL) >= DWRITE_FONT_WEIGHT_SEMI_BOLD;
			const bool italic = style.FontStyle.value_or(
				DWRITE_FONT_STYLE_NORMAL) != DWRITE_FONT_STYLE_NORMAL;
			const bool underline = style.Underline.value_or(false);
			const bool strike = style.Strikethrough.value_or(false);
			const auto localeId = LocaleIdForLanguage(style.Language);
			const auto foreground = SolidRgb(style.Foreground);
			const auto background = SolidRgb(style.Background);
			const int foregroundIndex = foreground
				&& colorIndexes.contains(*foreground)
				? colorIndexes.at(*foreground) : 0;
			const int backgroundIndex = background
				&& colorIndexes.contains(*background)
				? colorIndexes.at(*background) : 0;
			output += "{\\f" + std::to_string(fontIndex)
				+ "\\fs" + std::to_string(halfPoints)
				+ (bold ? "\\b" : "\\b0")
				+ (italic ? "\\i" : "\\i0")
				+ (underline ? "\\ul" : "\\ulnone")
				+ (strike ? "\\strike" : "\\strike0")
				+ (localeId
					? "\\lang" + std::to_string(*localeId) : std::string{})
				+ "\\cf" + std::to_string(foregroundIndex)
				+ "\\highlight" + std::to_string(backgroundIndex) + " ";
			AppendEscapedText(output, std::wstring_view(fragment.Text).substr(
				position, end - position));
			output += '}';
			position = end;
			if (output.size() > MaxRtfBytes) return std::nullopt;
		}
		output += '}';
		if (output.size() > MaxRtfBytes) return std::nullopt;
		return output;
	}
	catch (...)
	{
		return std::nullopt;
	}
}

std::optional<RichTextDocumentFragment> Decode(
	std::string_view value) noexcept
{
	try
	{
		return RtfDecoder(value).Run();
	}
	catch (...)
	{
		return std::nullopt;
	}
}
}
