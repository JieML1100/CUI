#pragma once

#include "RichTextDocument.h"

#include <optional>
#include <string>
#include <string_view>

namespace cui::richtext::rtf
{
/**
 * Encodes the portable character/paragraph subset understood by CUI.
 * It preserves font family/size, bold/italic, underline/strike, solid RGB
 * foreground/background, explicit line/paragraph breaks and paragraph
 * alignment. Images, embedded objects, alpha/gradient paint and process-local
 * text-tree identities are never emitted. UTF-16 text uses \uN fallback
 * escapes.
 */
std::optional<std::string> Encode(
	const RichTextDocumentFragment& fragment) noexcept;

/**
 * Decodes a bounded, non-executable RTF subset into a detached attributed
 * fragment. Unknown destinations are ignored; malformed input is rejected.
 */
std::optional<RichTextDocumentFragment> Decode(
	std::string_view value) noexcept;
}
