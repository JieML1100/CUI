#include "CppUserCodeIndex.h"

#include <algorithm>
#include <cctype>
#include <exception>
#include <map>
#include <set>
#include <utility>

namespace DesignerModel
{
namespace
{
	struct Token
	{
		std::string Text;
		size_t Position = 0;
	};

	bool StartsWithAt(
		std::string_view text,
		size_t position,
		std::string_view value) noexcept
	{
		return position <= text.size()
			&& value.size() <= text.size() - position
			&& text.compare(position, value.size(), value) == 0;
	}

	bool IsIdentifierStart(unsigned char value) noexcept
	{
		return std::isalpha(value) || value == '_';
	}

	bool IsIdentifierPart(unsigned char value) noexcept
	{
		return std::isalnum(value) || value == '_';
	}

	struct PreprocessorDirective
	{
		size_t Begin = 0;
		size_t End = 0;
		std::string Keyword;
		std::string Argument;
	};

	void UpdateLineStateForSkippedToken(
		std::string_view source,
		size_t begin,
		size_t end,
		size_t& lineStart,
		bool& onlyWhitespace)
	{
		const auto newline = source.rfind('\n', end == 0 ? 0 : end - 1);
		if (newline != std::string_view::npos && newline >= begin)
		{
			lineStart = newline + 1;
			onlyWhitespace = false;
		}
	}

	std::vector<PreprocessorDirective> FindPreprocessorDirectives(
		std::string_view source)
	{
		std::vector<PreprocessorDirective> directives;
		size_t lineStart = 0;
		bool onlyWhitespace = true;
		for (size_t position = 0; position < source.size();)
		{
			const auto current = static_cast<unsigned char>(source[position]);
			if (current == '\n')
			{
				lineStart = ++position;
				onlyWhitespace = true;
				continue;
			}
			if (std::isspace(current))
			{
				++position;
				continue;
			}
			if (StartsWithAt(source, position, "//"))
			{
				const auto end = source.find('\n', position + 2);
				position = end == std::string_view::npos
					? source.size() : end;
				continue;
			}
			if (StartsWithAt(source, position, "/*"))
			{
				const auto endMarker = source.find("*/", position + 2);
				const auto end = endMarker == std::string_view::npos
					? source.size() : endMarker + 2;
				const auto newline = source.rfind(
					'\n', end == 0 ? 0 : end - 1);
				if (newline != std::string_view::npos && newline >= position)
				{
					lineStart = newline + 1;
					onlyWhitespace = true;
				}
				position = end;
				continue;
			}

			constexpr std::string_view rawPrefixes[]{
				"R\"", "u8R\"", "uR\"", "UR\"", "LR\"" };
			bool raw = false;
			for (const auto prefix : rawPrefixes)
			{
				if (!StartsWithAt(source, position, prefix)) continue;
				if (position > 0 && IsIdentifierPart(
					static_cast<unsigned char>(source[position - 1]))) continue;
				const auto delimiterBegin = position + prefix.size();
				const auto open = source.find('(', delimiterBegin);
				if (open == std::string_view::npos
					|| open - delimiterBegin > 16) continue;
				const std::string delimiter(
					source.substr(delimiterBegin, open - delimiterBegin));
				const auto terminator = ")" + delimiter + "\"";
				const auto endMarker = source.find(terminator, open + 1);
				const auto end = endMarker == std::string_view::npos
					? source.size() : endMarker + terminator.size();
				onlyWhitespace = false;
				UpdateLineStateForSkippedToken(
					source, position, end, lineStart, onlyWhitespace);
				position = end;
				raw = true;
				break;
			}
			if (raw) continue;

			if (current == '"' || current == '\'')
			{
				const auto quote = current;
				const auto begin = position++;
				while (position < source.size())
				{
					if (source[position] == '\\')
					{
						position += position + 1 < source.size() ? 2 : 1;
						continue;
					}
					if (static_cast<unsigned char>(source[position]) == quote)
					{
						++position;
						break;
					}
					++position;
				}
				onlyWhitespace = false;
				UpdateLineStateForSkippedToken(
					source, begin, position, lineStart, onlyWhitespace);
				continue;
			}

			if (current == '#' && onlyWhitespace)
			{
				const auto directiveBegin = lineStart;
				size_t logicalEnd = position;
				size_t firstLineEnd = source.size();
				for (;;)
				{
					const auto newline = source.find('\n', logicalEnd);
					const auto lineEnd = newline == std::string_view::npos
						? source.size() : newline;
					if (firstLineEnd == source.size()) firstLineEnd = lineEnd;
					size_t last = lineEnd;
					if (last > logicalEnd && source[last - 1] == '\r') --last;
					const bool continued = last > logicalEnd
						&& source[last - 1] == '\\';
					logicalEnd = newline == std::string_view::npos
						? source.size() : newline + 1;
					if (!continued || logicalEnd >= source.size()) break;
				}

				size_t wordBegin = position + 1;
				while (wordBegin < firstLineEnd && std::isspace(
					static_cast<unsigned char>(source[wordBegin]))) ++wordBegin;
				size_t wordEnd = wordBegin;
				while (wordEnd < firstLineEnd && IsIdentifierPart(
					static_cast<unsigned char>(source[wordEnd]))) ++wordEnd;
				size_t argumentBegin = wordEnd;
				while (argumentBegin < firstLineEnd && std::isspace(
					static_cast<unsigned char>(source[argumentBegin])))
					++argumentBegin;
				directives.push_back({ directiveBegin, logicalEnd,
					std::string(source.substr(wordBegin, wordEnd - wordBegin)),
					std::string(source.substr(
						argumentBegin, firstLineEnd - argumentBegin)) });
				position = logicalEnd;
				lineStart = logicalEnd;
				onlyWhitespace = true;
				continue;
			}

			onlyWhitespace = false;
			++position;
		}
		return directives;
	}

	enum class LiteralCondition : unsigned char
	{
		False,
		True,
		Unknown,
	};

	LiteralCondition ParseLiteralCondition(std::string argument)
	{
		if (const auto comment = argument.find("//");
			comment != std::string::npos) argument.erase(comment);
		if (const auto comment = argument.find("/*");
			comment != std::string::npos) argument.erase(comment);
		auto trim = [](std::string& value)
		{
			while (!value.empty() && std::isspace(
				static_cast<unsigned char>(value.front()))) value.erase(value.begin());
			while (!value.empty() && std::isspace(
				static_cast<unsigned char>(value.back()))) value.pop_back();
		};
		trim(argument);
		while (argument.size() >= 2 && argument.front() == '('
			&& argument.back() == ')')
		{
			argument = argument.substr(1, argument.size() - 2);
			trim(argument);
		}
		std::transform(argument.begin(), argument.end(), argument.begin(),
			[](unsigned char value)
			{
				return static_cast<char>(std::tolower(value));
			});
		if (argument == "0" || argument == "0u" || argument == "0l"
			|| argument == "0ul" || argument == "0lu"
			|| argument == "0x0") return LiteralCondition::False;
		if (argument == "1" || argument == "1u" || argument == "1l"
			|| argument == "1ul" || argument == "1lu")
			return LiteralCondition::True;
		return LiteralCondition::Unknown;
	}

	void MaskRange(std::string& text, size_t begin, size_t end)
	{
		end = std::min(end, text.size());
		for (size_t index = std::min(begin, end); index < end; ++index)
			if (text[index] != '\r' && text[index] != '\n') text[index] = ' ';
	}

	std::string MaskPreprocessor(std::string_view source)
	{
		struct ConditionalFrame
		{
			bool ParentActive = true;
			bool Active = true;
			bool Known = false;
			bool BranchTaken = false;
			bool ElseSeen = false;
		};
		std::string masked(source);
		std::vector<ConditionalFrame> stack;
		auto active = [&]() { return stack.empty() || stack.back().Active; };
		size_t cursor = 0;
		for (const auto& directive : FindPreprocessorDirectives(source))
		{
			if (!active()) MaskRange(masked, cursor, directive.Begin);
			MaskRange(masked, directive.Begin, directive.End);
			const auto keyword = directive.Keyword;
			if (keyword == "if" || keyword == "ifdef" || keyword == "ifndef")
			{
				const auto condition = keyword == "if"
					? ParseLiteralCondition(directive.Argument)
					: LiteralCondition::Unknown;
				ConditionalFrame frame;
				frame.ParentActive = active();
				frame.Known = condition != LiteralCondition::Unknown;
				frame.BranchTaken = condition == LiteralCondition::True;
				frame.Active = frame.ParentActive
					&& condition != LiteralCondition::False;
				stack.push_back(frame);
			}
			else if (keyword == "elif" && !stack.empty()
				&& !stack.back().ElseSeen)
			{
				auto& frame = stack.back();
				if (!frame.Known)
					frame.Active = frame.ParentActive;
				else if (frame.BranchTaken)
					frame.Active = false;
				else
				{
					const auto condition =
						ParseLiteralCondition(directive.Argument);
					if (condition == LiteralCondition::Unknown)
					{
						frame.Known = false;
						frame.Active = frame.ParentActive;
					}
					else
					{
						frame.Active = frame.ParentActive
							&& condition == LiteralCondition::True;
						frame.BranchTaken =
							condition == LiteralCondition::True;
					}
				}
			}
			else if (keyword == "else" && !stack.empty()
				&& !stack.back().ElseSeen)
			{
				auto& frame = stack.back();
				frame.ElseSeen = true;
				frame.Active = frame.ParentActive
					&& (!frame.Known || !frame.BranchTaken);
				frame.BranchTaken = true;
			}
			else if (keyword == "endif" && !stack.empty())
			{
				stack.pop_back();
			}
			cursor = directive.End;
		}
		if (!active()) MaskRange(masked, cursor, masked.size());
		return masked;
	}

	std::vector<Token> Tokenize(std::string_view source)
	{
		const auto preprocessed = MaskPreprocessor(source);
		source = preprocessed;
		std::vector<Token> tokens;
		for (size_t position = 0; position < source.size();)
		{
			const auto current = static_cast<unsigned char>(source[position]);
			if (std::isspace(current))
			{
				++position;
				continue;
			}
			if (StartsWithAt(source, position, "//"))
			{
				const auto end = source.find('\n', position + 2);
				position = end == std::string_view::npos
					? source.size() : end + 1;
				continue;
			}
			if (StartsWithAt(source, position, "/*"))
			{
				const auto end = source.find("*/", position + 2);
				position = end == std::string_view::npos
					? source.size() : end + 2;
				continue;
			}

			constexpr std::string_view rawPrefixes[]{
				"R\"", "u8R\"", "uR\"", "UR\"", "LR\"" };
			bool raw = false;
			for (const auto prefix : rawPrefixes)
			{
				if (!StartsWithAt(source, position, prefix)) continue;
				if (position > 0 && IsIdentifierPart(
					static_cast<unsigned char>(source[position - 1]))) continue;
				const auto delimiterBegin = position + prefix.size();
				const auto open = source.find('(', delimiterBegin);
				if (open == std::string_view::npos
					|| open - delimiterBegin > 16) continue;
				const std::string delimiter(
					source.substr(delimiterBegin, open - delimiterBegin));
				const auto terminator = ")" + delimiter + "\"";
				const auto end = source.find(terminator, open + 1);
				position = end == std::string_view::npos
					? source.size() : end + terminator.size();
				raw = true;
				break;
			}
			if (raw) continue;

			if (current == '"' || current == '\'')
			{
				const auto quote = current;
				++position;
				while (position < source.size())
				{
					if (source[position] == '\\')
					{
						position += position + 1 < source.size() ? 2 : 1;
						continue;
					}
					if (static_cast<unsigned char>(source[position]) == quote)
					{
						++position;
						break;
					}
					++position;
				}
				continue;
			}
			if (IsIdentifierStart(current))
			{
				const auto begin = position++;
				while (position < source.size() && IsIdentifierPart(
					static_cast<unsigned char>(source[position]))) ++position;
				tokens.push_back({
					std::string(source.substr(begin, position - begin)), begin });
				continue;
			}
			if (StartsWithAt(source, position, "::"))
			{
				tokens.push_back({ "::", position });
				position += 2;
				continue;
			}
			tokens.push_back({ std::string(1, source[position]), position });
			++position;
		}
		return tokens;
	}

	std::vector<std::string> SplitQualifiedName(std::string_view value)
	{
		std::vector<std::string> result;
		if (value.empty()) return result;
		for (size_t begin = 0; begin <= value.size();)
		{
			const auto end = value.find("::", begin);
			const auto segment = value.substr(begin,
				end == std::string_view::npos
					? std::string_view::npos : end - begin);
			if (segment.empty()
				|| !IsIdentifierStart(static_cast<unsigned char>(segment.front()))
				|| !std::all_of(segment.begin() + 1, segment.end(),
					[](unsigned char value) { return IsIdentifierPart(value); }))
				return {};
			result.emplace_back(segment);
			if (end == std::string_view::npos) break;
			begin = end + 2;
		}
		return result;
	}


	struct NamespaceOpening
	{
		std::vector<std::string> Segments;
	};

	std::map<size_t, NamespaceOpening> FindNamespaceOpenings(
		const std::vector<Token>& tokens)
	{
		std::map<size_t, NamespaceOpening> result;
		for (size_t index = 0; index < tokens.size(); ++index)
		{
			if (tokens[index].Text != "namespace") continue;
			size_t cursor = index + 1;
			NamespaceOpening opening;
			if (cursor < tokens.size() && tokens[cursor].Text == "{")
			{
				// Anonymous namespaces cannot enclose the exported x:Class.
				opening.Segments.push_back("<anonymous>");
				result.emplace(cursor, std::move(opening));
				continue;
			}
			if (cursor >= tokens.size()
				|| !IsIdentifierStart(static_cast<unsigned char>(
					tokens[cursor].Text.front())))
				continue;
			opening.Segments.push_back(tokens[cursor++].Text);
			while (cursor + 1 < tokens.size()
				&& tokens[cursor].Text == "::"
				&& IsIdentifierStart(static_cast<unsigned char>(
					tokens[cursor + 1].Text.front())))
			{
				opening.Segments.push_back(tokens[cursor + 1].Text);
				cursor += 2;
			}
			if (cursor < tokens.size() && tokens[cursor].Text == "{")
				result.emplace(cursor, std::move(opening));
		}
		return result;
	}

	struct MemberQualifier
	{
		std::vector<std::string> Segments;
		bool Global = false;
		size_t FirstToken = 0;
	};

	MemberQualifier ReadMemberQualifier(
		const std::vector<Token>& tokens,
		size_t handlerIndex)
	{
		MemberQualifier result;
		if (handlerIndex < 2 || tokens[handlerIndex - 1].Text != "::")
			return result;
		size_t first = handlerIndex - 2;
		if (tokens[first].Text.empty()
			|| !IsIdentifierStart(static_cast<unsigned char>(
				tokens[first].Text.front())))
			return result;
		while (first >= 2 && tokens[first - 1].Text == "::"
			&& !tokens[first - 2].Text.empty()
			&& IsIdentifierStart(static_cast<unsigned char>(
				tokens[first - 2].Text.front())))
			first -= 2;
		result.Global = first > 0 && tokens[first - 1].Text == "::";
		result.FirstToken = first;
		for (size_t cursor = first; cursor < handlerIndex; cursor += 2)
			result.Segments.push_back(tokens[cursor].Text);
		return result;
	}

	bool IsPrefix(
		const std::vector<std::string>& prefix,
		const std::vector<std::string>& value)
	{
		return prefix.size() <= value.size()
			&& std::equal(prefix.begin(), prefix.end(), value.begin());
	}

	bool MatchesClassScope(
		const MemberQualifier& qualifier,
		const std::vector<std::string>& activeNamespace,
		const std::vector<std::string>& classSegments)
	{
		if (qualifier.Segments.empty()) return false;
		if (classSegments.empty()) return true;
		std::vector<std::string> classNamespace(
			classSegments.begin(), classSegments.end() - 1);
		if (qualifier.Segments == classSegments)
			return IsPrefix(activeNamespace, classNamespace);
		if (qualifier.Global) return false;
		std::vector<std::string> resolved = activeNamespace;
		resolved.insert(resolved.end(), qualifier.Segments.begin(),
			qualifier.Segments.end());
		return resolved == classSegments;
	}

	bool MatchesGeneratedBaseSpecifier(
		const std::vector<Token>& tokens,
		size_t begin,
		size_t end,
		const std::vector<std::string>& activeNamespace,
		const std::vector<std::string>& generatedClassSegments)
	{
		while (begin < end)
		{
			const auto& token = tokens[begin].Text;
			if (token == "public" || token == "protected"
				|| token == "private" || token == "virtual")
			{
				++begin;
				continue;
			}
			if (token == "[" && begin + 1 < end
				&& tokens[begin + 1].Text == "[")
			{
				begin += 2;
				int depth = 1;
				while (begin + 1 < end && depth > 0)
				{
					if (tokens[begin].Text == "["
						&& tokens[begin + 1].Text == "[")
					{
						++depth;
						begin += 2;
						continue;
					}
					if (tokens[begin].Text == "]"
						&& tokens[begin + 1].Text == "]")
					{
						--depth;
						begin += 2;
						continue;
					}
					++begin;
				}
				continue;
			}
			break;
		}

		bool global = false;
		if (begin < end && tokens[begin].Text == "::")
		{
			global = true;
			++begin;
		}
		std::vector<std::string> segments;
		if (begin >= end || tokens[begin].Text.empty()
			|| !IsIdentifierStart(static_cast<unsigned char>(
				tokens[begin].Text.front()))) return false;
		segments.push_back(tokens[begin++].Text);
		while (begin + 1 < end && tokens[begin].Text == "::"
			&& !tokens[begin + 1].Text.empty()
			&& IsIdentifierStart(static_cast<unsigned char>(
				tokens[begin + 1].Text.front())))
		{
			segments.push_back(tokens[begin + 1].Text);
			begin += 2;
		}
		if (segments == generatedClassSegments) return true;
		if (global) return false;
		auto resolved = activeNamespace;
		resolved.insert(resolved.end(), segments.begin(), segments.end());
		return resolved == generatedClassSegments;
	}

	bool ClassDerivesGeneratedBase(
		const std::vector<Token>& tokens,
		size_t begin,
		size_t end,
		const std::vector<std::string>& activeNamespace,
		const std::vector<std::string>& generatedClassSegments)
	{
		size_t specifierBegin = begin;
		int angles = 0;
		int parentheses = 0;
		int brackets = 0;
		for (size_t index = begin; index <= end; ++index)
		{
			const bool atEnd = index == end;
			const auto token = atEnd ? std::string_view{}
				: std::string_view(tokens[index].Text);
			if (!atEnd)
			{
				if (token == "<") ++angles;
				else if (token == ">" && angles > 0) --angles;
				else if (token == "(") ++parentheses;
				else if (token == ")" && parentheses > 0) --parentheses;
				else if (token == "[") ++brackets;
				else if (token == "]" && brackets > 0) --brackets;
			}
			if (!atEnd && (token != "," || angles != 0
				|| parentheses != 0 || brackets != 0)) continue;
			if (MatchesGeneratedBaseSpecifier(
				tokens, specifierBegin, index, activeNamespace,
				generatedClassSegments)) return true;
			specifierBegin = index + 1;
		}
		return false;
	}

	using ParameterTokens = std::vector<std::vector<std::string>>;

	ParameterTokens SplitParameters(
		const std::vector<Token>& tokens,
		size_t begin,
		size_t end)
	{
		ParameterTokens parameters;
		std::vector<std::string> current;
		int parentheses = 0;
		int angles = 0;
		int brackets = 0;
		for (size_t index = begin; index < end; ++index)
		{
			const auto& token = tokens[index].Text;
			if (token == "(") ++parentheses;
			else if (token == ")" && parentheses > 0) --parentheses;
			else if (token == "<") ++angles;
			else if (token == ">" && angles > 0) --angles;
			else if (token == "[") ++brackets;
			else if (token == "]" && brackets > 0) --brackets;
			if (token == "," && parentheses == 0
				&& angles == 0 && brackets == 0)
			{
				parameters.push_back(std::move(current));
				current.clear();
				continue;
			}
			current.push_back(token);
		}
		if (!current.empty()) parameters.push_back(std::move(current));
		if (parameters.size() == 1 && parameters.front().size() == 1
			&& parameters.front().front() == "void")
			parameters.clear();
		return parameters;
	}

	ParameterTokens ExpectedParameterTypes(std::string_view parameterList)
	{
		const auto tokens = Tokenize(parameterList);
		auto parameters = SplitParameters(tokens, 0, tokens.size());
		for (auto& parameter : parameters)
			if (!parameter.empty()) parameter.pop_back();
		return parameters;
	}

	bool Matches(
		const ParameterTokens& definition,
		const ParameterTokens& expected)
	{
		if (definition.size() != expected.size()) return false;
		for (size_t index = 0; index < definition.size(); ++index)
		{
			const auto& actual = definition[index];
			const auto& required = expected[index];
			if (actual == required) continue;
			if (actual.size() != required.size() + 1 || actual.empty())
				return false;
			const auto& name = actual.back();
			if (name.empty()
				|| !IsIdentifierStart(static_cast<unsigned char>(name.front()))
				|| !std::equal(required.begin(), required.end(), actual.begin()))
				return false;
		}
		return true;
	}

	void SetError(std::wstring* outError, const wchar_t* value)
	{
		if (outError) *outError = value;
	}
}

bool CppUserCodeIndex::Build(
	std::string_view source,
	std::string_view qualifiedClassName,
	CppUserCodeIndex& index,
	std::wstring* outError)
{
	index = {};
	if (outError) outError->clear();
	try
	{
		const auto classSegments = SplitQualifiedName(qualifiedClassName);
		if (!qualifiedClassName.empty() && classSegments.empty())
		{
			SetError(outError, L"C++ code-behind 类名无效。");
			return false;
		}
		index._qualifiedClassName = qualifiedClassName;
		const auto tokens = Tokenize(source);
		const auto namespaceOpenings = FindNamespaceOpenings(tokens);
		struct ScopeFrame
		{
			bool IsNamespace = false;
			size_t AddedNamespaceSegments = 0;
		};
		std::vector<ScopeFrame> scopes;
		std::vector<std::string> activeNamespace;
		size_t nonNamespaceDepth = 0;
		std::vector<std::string> classNamespace;
		std::vector<std::string> generatedClassSegments;
		if (!classSegments.empty())
		{
			classNamespace.assign(
				classSegments.begin(), classSegments.end() - 1);
			generatedClassSegments = classNamespace;
			generatedClassSegments.push_back(
				classSegments.back() + "Generated");
		}
		for (size_t tokenIndex = 0; tokenIndex < tokens.size(); ++tokenIndex)
		{
			const auto& token = tokens[tokenIndex].Text;
			if (nonNamespaceDepth == 0 && !classSegments.empty()
				&& (token == "class" || token == "struct")
				&& (tokenIndex == 0 || tokens[tokenIndex - 1].Text != "enum")
				&& activeNamespace == classNamespace)
			{
				size_t bodyOpen = tokens.size();
				size_t colon = tokens.size();
				size_t classNameIndex = tokens.size();
				int parentheses = 0;
				int brackets = 0;
				for (size_t cursor = tokenIndex + 1;
					cursor < tokens.size(); ++cursor)
				{
					const auto& headerToken = tokens[cursor].Text;
					if (headerToken == "(") ++parentheses;
					else if (headerToken == ")" && parentheses > 0)
						--parentheses;
					else if (headerToken == "[") ++brackets;
					else if (headerToken == "]" && brackets > 0)
						--brackets;
					if (parentheses != 0 || brackets != 0) continue;
					if (headerToken == classSegments.back()
						&& colon == tokens.size())
						classNameIndex = cursor;
					if (headerToken == ":" && colon == tokens.size())
					{
						colon = cursor;
						continue;
					}
					if (headerToken == "{" || headerToken == ";")
					{
						if (headerToken == "{") bodyOpen = cursor;
						break;
					}
				}
				if (bodyOpen != tokens.size()
					&& classNameIndex != tokens.size()
					&& classNameIndex < (colon == tokens.size()
						? bodyOpen : colon))
				{
					ClassDefinition definition;
					definition.DerivesGeneratedBase = colon != tokens.size()
						&& colon < bodyOpen
						&& ClassDerivesGeneratedBase(
							tokens, colon + 1, bodyOpen, activeNamespace,
							generatedClassSegments);
					definition.Line = 1 + static_cast<size_t>(std::count(
						source.begin(), source.begin()
							+ tokens[classNameIndex].Position, '\n'));
					index._classDefinitions.push_back(std::move(definition));

					size_t classBodyClose = bodyOpen + 1;
					int classBraceDepth = 1;
					for (; classBodyClose < tokens.size()
						&& classBraceDepth > 0; ++classBodyClose)
					{
						if (tokens[classBodyClose].Text == "{")
							++classBraceDepth;
						else if (tokens[classBodyClose].Text == "}")
							--classBraceDepth;
					}
					if (classBraceDepth == 0)
					{
						const auto classBodyEnd = classBodyClose - 1;
						int memberBraceDepth = 0;
						for (size_t cursor = bodyOpen + 1;
							cursor < classBodyEnd; ++cursor)
						{
							const auto& memberToken = tokens[cursor].Text;
							if (memberToken == "{")
							{
								++memberBraceDepth;
								continue;
							}
							if (memberToken == "}")
							{
								if (memberBraceDepth > 0) --memberBraceDepth;
								continue;
							}
							const bool isConstructor =
								memberToken == classSegments.back()
								&& (cursor == bodyOpen + 1
									|| (tokens[cursor - 1].Text != "~"
										&& tokens[cursor - 1].Text != "::"));
							const bool isMember =
								memberToken != "operator"
								&& !memberToken.empty()
								&& IsIdentifierStart(static_cast<unsigned char>(
									memberToken.front()))
								&& (cursor == bodyOpen + 1
									|| (tokens[cursor - 1].Text != "~"
										&& tokens[cursor - 1].Text != "::"));
							if (memberBraceDepth != 0
								|| (!isConstructor && !isMember)
								|| cursor + 1 >= classBodyEnd
								|| tokens[cursor + 1].Text != "(")
								continue;

							const auto parameterBegin = cursor + 2;
							int parameterDepth = 1;
							size_t afterParameters = parameterBegin;
							for (; afterParameters < classBodyEnd
								&& parameterDepth > 0; ++afterParameters)
							{
								if (tokens[afterParameters].Text == "(")
									++parameterDepth;
								else if (tokens[afterParameters].Text == ")")
									--parameterDepth;
							}
							if (parameterDepth != 0) continue;

							bool defined = false;
							bool deleted = false;
							bool staticMember = false;
							bool incompatibleSuffix = false;
							bool trailingVoid = false;
							const bool directVoid = !isConstructor
								&& cursor > bodyOpen + 1
								&& tokens[cursor - 1].Text == "void";
							const bool trailingReturn = !isConstructor
								&& cursor > bodyOpen + 1
								&& tokens[cursor - 1].Text == "auto";
							if (!isConstructor)
							{
								size_t declarationBegin = cursor;
								while (declarationBegin > bodyOpen + 1)
								{
									const auto& previous =
										tokens[declarationBegin - 1].Text;
									if (previous == ";" || previous == "{"
										|| previous == "}" || previous == ":")
										break;
									--declarationBegin;
								}
								for (size_t prefix = declarationBegin;
									prefix < cursor; ++prefix)
									if (tokens[prefix].Text == "static")
										staticMember = true;
							}
							size_t definitionEnd = cursor;
							for (size_t tail = afterParameters;
								tail < classBodyEnd; ++tail)
							{
								if (tokens[tail].Text == "=")
								{
									if (isConstructor && tail + 1 < classBodyEnd
										&& tokens[tail + 1].Text == "default")
									{
										defined = true;
										definitionEnd = tail + 1;
									}
									else if (tail + 1 < classBodyEnd
										&& tokens[tail + 1].Text == "delete")
									{
										deleted = true;
										definitionEnd = tail + 1;
									}
								}
								if (!isConstructor)
								{
									const auto& suffix = tokens[tail].Text;
									if (suffix == "const" || suffix == "volatile"
										|| suffix == "&")
										incompatibleSuffix = true;
									if (suffix == "-" && tail + 2 < classBodyEnd
										&& tokens[tail + 1].Text == ">"
										&& tokens[tail + 2].Text == "void")
										trailingVoid = true;
								}
								if (tokens[tail].Text == "{")
								{
									int bodyDepth = 1;
									size_t bodyClose = tail + 1;
									for (; bodyClose < classBodyEnd
										&& bodyDepth > 0; ++bodyClose)
									{
										if (tokens[bodyClose].Text == "{")
											++bodyDepth;
										else if (tokens[bodyClose].Text == "}")
											--bodyDepth;
									}
									if (bodyDepth != 0) break;
									const auto afterBody = bodyClose;
									if (isConstructor && afterBody < classBodyEnd
										&& (tokens[afterBody].Text == ","
											|| tokens[afterBody].Text == "{"))
									{
										tail = bodyClose - 1;
										continue;
									}
									defined = true;
									definitionEnd = bodyClose - 1;
									break;
								}
								if (tokens[tail].Text == ";") break;
							}
							if (!defined && !deleted) continue;

							Definition definition;
							definition.Name = memberToken;
							definition.Parameters = SplitParameters(
								tokens, parameterBegin, afterParameters - 1);
							definition.CompatibleHandlerShape = isConstructor
								|| ((directVoid || (trailingReturn && trailingVoid))
									&& !staticMember && !incompatibleSuffix);
							definition.Deleted = deleted;
							definition.Line = 1 + static_cast<size_t>(std::count(
								source.begin(), source.begin()
									+ tokens[cursor].Position, '\n'));
							definition.Position = tokens[cursor].Position;
							if (isConstructor)
								index._constructors.push_back(std::move(definition));
							else
								index._definitions.push_back(std::move(definition));
							cursor = std::max(cursor, definitionEnd);
						}
					}
				}
			}
			if (token == "{")
			{
				const auto opening = namespaceOpenings.find(tokenIndex);
				if (opening != namespaceOpenings.end())
				{
					activeNamespace.insert(activeNamespace.end(),
						opening->second.Segments.begin(),
						opening->second.Segments.end());
					scopes.push_back({ true,
						opening->second.Segments.size() });
				}
				else
				{
					++nonNamespaceDepth;
					scopes.push_back({ false, 0 });
				}
				continue;
			}
			if (token == "}")
			{
				if (!scopes.empty())
				{
					const auto scope = scopes.back();
					scopes.pop_back();
					if (scope.IsNamespace)
						activeNamespace.resize(activeNamespace.size()
							- scope.AddedNamespaceSegments);
					else if (nonNamespaceDepth > 0)
						--nonNamespaceDepth;
				}
				continue;
			}
			if (nonNamespaceDepth != 0 || tokenIndex + 1 >= tokens.size()
				|| tokens[tokenIndex + 1].Text != "("
				|| token.empty()
				|| !IsIdentifierStart(static_cast<unsigned char>(
					token.front())))
				continue;
			const auto qualifier = ReadMemberQualifier(tokens, tokenIndex);
			if (!MatchesClassScope(
				qualifier, activeNamespace, classSegments)) continue;
			const bool isDestructor = tokenIndex > 0
				&& tokens[tokenIndex - 1].Text == "~";
			if (isDestructor) continue;
			const bool isConstructor = !qualifier.Segments.empty()
				&& token == qualifier.Segments.back();

			const auto parameterBegin = tokenIndex + 2;
			int depth = 1;
			size_t afterParameters = parameterBegin;
			for (; afterParameters < tokens.size() && depth > 0;
				++afterParameters)
			{
				if (tokens[afterParameters].Text == "(") ++depth;
				else if (tokens[afterParameters].Text == ")") --depth;
			}
			if (depth != 0 || afterParameters >= tokens.size()) continue;
			bool deleted = false;
			bool staticMember = false;
			bool incompatibleSuffix = false;
			bool trailingVoid = false;
			size_t returnEnd = qualifier.FirstToken;
			if (qualifier.Global && returnEnd > 0
				&& tokens[returnEnd - 1].Text == "::")
				--returnEnd;
			const bool directVoid = !isConstructor && returnEnd > 0
				&& tokens[returnEnd - 1].Text == "void";
			const bool trailingReturn = !isConstructor && returnEnd > 0
				&& tokens[returnEnd - 1].Text == "auto";
			if (!isConstructor && returnEnd > 0)
			{
				size_t declarationBegin = returnEnd - 1;
				while (declarationBegin > 0)
				{
					const auto& previous = tokens[declarationBegin - 1].Text;
					if (previous == ";" || previous == "{"
						|| previous == "}")
						break;
					--declarationBegin;
				}
				for (size_t prefix = declarationBegin;
					prefix < returnEnd; ++prefix)
					if (tokens[prefix].Text == "static") staticMember = true;
			}

			size_t bodyOpen = afterParameters;
			if (isConstructor && tokens[bodyOpen].Text != "{")
			{
				// Constructors may have an initializer list between `)` and
				// their body. A declaration still terminates at `;` and is not
				// accepted as an owning class definition.
				while (bodyOpen < tokens.size()
					&& tokens[bodyOpen].Text != "{"
					&& tokens[bodyOpen].Text != ";")
					++bodyOpen;
			}
			if (!isConstructor)
			{
				for (; bodyOpen < tokens.size(); ++bodyOpen)
				{
					const auto& suffix = tokens[bodyOpen].Text;
					if (suffix == ";") break;
					if (suffix == "=" && bodyOpen + 1 < tokens.size()
						&& tokens[bodyOpen + 1].Text == "delete")
					{
						deleted = true;
						break;
					}
					if (suffix == "{") break;
					if (suffix == "const" || suffix == "volatile"
						|| suffix == "&" || suffix == "noexcept"
						|| suffix == "override" || suffix == "final")
						incompatibleSuffix = true;
					if (suffix == "-" && bodyOpen + 2 < tokens.size()
						&& tokens[bodyOpen + 1].Text == ">"
						&& tokens[bodyOpen + 2].Text == "void")
						trailingVoid = true;
				}
			}
			if (!deleted && (bodyOpen >= tokens.size()
				|| tokens[bodyOpen].Text != "{")) continue;
			Definition definition;
			definition.Name = token;
			if (definition.Name == "operator") continue;
			definition.Parameters = SplitParameters(
				tokens, parameterBegin, afterParameters - 1);
			definition.CompatibleHandlerShape = isConstructor
				|| ((directVoid || (trailingReturn && trailingVoid))
					&& !staticMember && !incompatibleSuffix);
			definition.Deleted = deleted;
			definition.Line = 1 + static_cast<size_t>(std::count(
				source.begin(), source.begin() + tokens[tokenIndex].Position, '\n'));
			definition.Position = tokens[tokenIndex].Position;
			if (isConstructor)
				index._constructors.push_back(std::move(definition));
			else
				index._definitions.push_back(std::move(definition));
		}
		return true;
	}
	catch (const std::exception&)
	{
		SetError(outError, L"建立 C++ 用户代码索引时资源分配失败。");
	}
	catch (...)
	{
		SetError(outError, L"建立 C++ 用户代码索引时发生未知异常。");
	}
	index = {};
	return false;
}

CppUserHandlerDefinitionInspection CppUserCodeIndex::InspectHandler(
	std::string_view handlerName,
	std::string_view generatedParameterList) const
{
	CppUserHandlerDefinitionInspection result;
	const auto expected = ExpectedParameterTypes(generatedParameterList);
	for (const auto& definition : _definitions)
	{
		if (definition.Name != handlerName) continue;
		++result.DefinitionCount;
		if (result.FirstDefinitionLine == 0)
			result.FirstDefinitionLine = definition.Line;
		if (!Matches(definition.Parameters, expected)) continue;
		if (!definition.CompatibleHandlerShape)
		{
			++result.IncompatibleShapeDefinitionCount;
			continue;
		}
		if (definition.Deleted)
		{
			++result.DeletedCompatibleDefinitionCount;
			continue;
		}
		++result.CompatibleDefinitionCount;
		if (result.FirstCompatibleDefinitionLine == 0)
			result.FirstCompatibleDefinitionLine = definition.Line;
	}
	return result;
}

CppUserHandlerDefinitionInspection CppUserCodeIndex::InspectConstructor(
	std::string_view generatedParameterList) const
{
	CppUserHandlerDefinitionInspection result;
	const auto expected = ExpectedParameterTypes(generatedParameterList);
	for (const auto& definition : _constructors)
	{
		++result.DefinitionCount;
		if (result.FirstDefinitionLine == 0)
			result.FirstDefinitionLine = definition.Line;
		if (!Matches(definition.Parameters, expected)) continue;
		if (definition.Deleted)
		{
			++result.DeletedCompatibleDefinitionCount;
			continue;
		}
		++result.CompatibleDefinitionCount;
		if (result.FirstCompatibleDefinitionLine == 0)
			result.FirstCompatibleDefinitionLine = definition.Line;
	}
	return result;
}

CppUserClassDefinitionInspection
CppUserCodeIndex::InspectGeneratedClassDefinition() const
{
	CppUserClassDefinitionInspection result;
	for (const auto& definition : _classDefinitions)
	{
		++result.DefinitionCount;
		if (result.FirstDefinitionLine == 0)
			result.FirstDefinitionLine = definition.Line;
		if (!definition.DerivesGeneratedBase) continue;
		++result.CompatibleGeneratedBaseCount;
		if (result.FirstCompatibleDefinitionLine == 0)
			result.FirstCompatibleDefinitionLine = definition.Line;
	}
	return result;
}

std::vector<std::string> CppUserCodeIndex::FindCompatibleHandlerNames(
	std::string_view generatedParameterList) const
{
	const auto expected = ExpectedParameterTypes(generatedParameterList);
	std::map<std::string, size_t> compatibleCounts;
	std::set<std::string> unusableNames;
	for (const auto& definition : _definitions)
	{
		if (!Matches(definition.Parameters, expected)) continue;
		if (definition.CompatibleHandlerShape && !definition.Deleted)
			++compatibleCounts[definition.Name];
		else
			unusableNames.insert(definition.Name);
	}

	std::vector<std::string> result;
	std::set<std::string> appended;
	for (const auto& definition : _definitions)
	{
		const auto count = compatibleCounts.find(definition.Name);
		if (count == compatibleCounts.end() || count->second != 1
			|| unusableNames.contains(definition.Name)
			|| !appended.insert(definition.Name).second)
			continue;
		result.push_back(definition.Name);
	}
	return result;
}

bool CppUserCodeIndex::TryRenameUniqueCompatibleHandler(
	std::string_view source,
	std::string_view oldName,
	std::string_view newName,
	std::string_view generatedParameterList,
	std::string& rewrittenSource,
	std::wstring* outError) const
{
	rewrittenSource.clear();
	if (outError) outError->clear();
	if (oldName.empty() || newName.empty() || oldName == newName)
	{
		SetError(outError, L"处理函数源码迁移名称无效。");
		return false;
	}
	const auto expected = ExpectedParameterTypes(generatedParameterList);
	const Definition* sourceDefinition = nullptr;
	size_t sourceCompatibleCount = 0;
	for (const auto& definition : _definitions)
	{
		if (definition.Name == oldName
			&& definition.CompatibleHandlerShape
			&& !definition.Deleted
			&& Matches(definition.Parameters, expected))
		{
			sourceDefinition = &definition;
			++sourceCompatibleCount;
		}
		if (definition.Name == newName
			&& Matches(definition.Parameters, expected))
		{
			SetError(outError, L"目标处理函数已经存在同签名定义，不能迁移函数体。");
			return false;
		}
	}
	if (sourceCompatibleCount == 0 || !sourceDefinition)
	{
		SetError(outError,
			L"用户代码文件中不存在可迁移的兼容 void 成员处理函数定义。");
		return false;
	}
	if (sourceCompatibleCount != 1)
	{
		SetError(outError,
			L"用户代码文件中存在多个兼容定义，不能安全迁移函数体。");
		return false;
	}
	if (sourceDefinition->Position > source.size()
		|| oldName.size() > source.size() - sourceDefinition->Position
		|| source.substr(sourceDefinition->Position, oldName.size()) != oldName)
	{
		SetError(outError, L"处理函数源码位置已失效，未修改用户文件。");
		return false;
	}
	rewrittenSource.assign(source);
	rewrittenSource.replace(
		sourceDefinition->Position, oldName.size(), newName);

	CppUserCodeIndex verification;
	std::wstring verificationError;
	if (!Build(rewrittenSource, _qualifiedClassName,
		verification, &verificationError)
		|| verification.InspectHandler(
			newName, generatedParameterList).CompatibleDefinitionCount != 1)
	{
		rewrittenSource.clear();
		if (outError) *outError = verificationError.empty()
			? L"处理函数源码迁移后的定义校验失败。"
			: std::move(verificationError);
		return false;
	}
	return true;
}
}
