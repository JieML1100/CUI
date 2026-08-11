#include "RichTextBoxDocumentTests.h"

#include "TestRunner.h"
#include <EditingCommands.h>
#include <InputInfrastructure.h>
#include <RichTextBox.h>
#include <RichTextClipboard.h>
#include <RichTextRtf.h>
#include <TextEditCore.h>
#include <TextRange.h>
#include <Window.h>
#include <Factory.h>
#include <Font.h>
#include <Graphics.h>

#include <Richedit.h>

#include <wrl/client.h>

#include <cfloat>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

struct RichTextBoxDocumentTestAccess final
{
	struct VisualLine
	{
		std::size_t Start = 0;
		std::size_t End = 0;
		float Top = 0.0f;
		float Height = 0.0f;
	};

	struct LayoutChunk
	{
		std::size_t Start = 0;
		std::size_t Length = 0;
		std::size_t LayoutLength = 0;
		float Height = -1.0f;
		bool HasSentinel = false;
		bool IsSingleVisualLine = false;
		::TextAlignment Alignment = ::TextAlignment::Left;
		::FlowDirection Direction = ::FlowDirection::LeftToRight;
	};

	static std::vector<LayoutChunk> RebuildLayoutChunks(
		RichTextBox& box, const std::wstring& text)
	{
		box.Text = text;
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = 256;
		box.RebuildBlocks();
		std::vector<LayoutChunk> result;
		result.reserve(box.blocks.size());
		for (const auto& block : box.blocks)
			result.push_back({ block.start, block.len, block.layoutLen,
				block.height, block.appendSentinel,
				block.singleVisualLine, block.textAlignment,
				block.flowDirection });
		return result;
	}

	static std::vector<LayoutChunk> ProfileVisualLines(
		RichTextBox& box,
		const std::wstring& text,
		float width,
		float height,
		std::size_t windowLength = 256)
	{
		box.Text = text;
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = windowLength;
		box.RebuildBlocks();
		box.EnsureAllBlockMetrics(width, height);
		std::vector<LayoutChunk> result;
		result.reserve(box.blocks.size());
		for (const auto& block : box.blocks)
			result.push_back({ block.start, block.len, block.layoutLen,
				block.height, block.appendSentinel,
				block.singleVisualLine, block.textAlignment,
				block.flowDirection });
		return result;
	}

	static std::vector<LayoutChunk> RebuildDocumentLayoutChunks(
		RichTextBox& box, std::size_t windowLength = 256)
	{
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = windowLength;
		box.RebuildBlocks();
		std::vector<LayoutChunk> result;
		result.reserve(box.blocks.size());
		for (const auto& block : box.blocks)
			result.push_back({ block.start, block.len, block.layoutLen,
				block.height, block.appendSentinel,
				block.singleVisualLine, block.textAlignment,
				block.flowDirection });
		return result;
	}

	static std::vector<LayoutChunk> ProfileDocumentVisualLines(
		RichTextBox& box,
		float width,
		float height,
		std::size_t windowLength = 256)
	{
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = windowLength;
		box.RebuildBlocks();
		box.EnsureAllBlockMetrics(width, height);
		std::vector<LayoutChunk> result;
		result.reserve(box.blocks.size());
		for (const auto& block : box.blocks)
			result.push_back({ block.start, block.len, block.layoutLen,
				block.height, block.appendSentinel,
				block.singleVisualLine, block.textAlignment,
				block.flowDirection });
		return result;
	}

	static std::pair<float, float> MeasureVirtualAndContinuousHeight(
		RichTextBox& box, float width)
	{
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = 256;
		box.RebuildBlocks();
		box.EnsureAllBlockMetrics(width, FLT_MAX);
		const float virtualHeight = box.virtualTotalHeight;

		auto* font = box.GetRenderFont();
		CUI_EXPECT_TRUE(font != nullptr);
		if (!font || !font->FontObject) return { virtualHeight, 0.0f };
		Microsoft::WRL::ComPtr<IDWriteTextLayout> continuous;
		continuous.Attach(Factory::CreateStringLayout(
			box.buffer, width, FLT_MAX, font->FontObject));
		CUI_EXPECT_TRUE(continuous != nullptr);
		if (!continuous) return { virtualHeight, 0.0f };
		(void)continuous->SetWordWrapping(DWRITE_WORD_WRAPPING_CHARACTER);
		box.ApplyTextLayoutFormatting(
			continuous.Get(), 0, static_cast<int>(box.buffer.size()));
		DWRITE_TEXT_METRICS metrics{};
		CUI_EXPECT_TRUE(SUCCEEDED(continuous->GetMetrics(&metrics)));
		return { virtualHeight, metrics.height };
	}

	static std::pair<float, std::size_t> MeasureAllBlockMetrics(
		RichTextBox& box, float width, float height)
	{
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = 256;
		box.RebuildBlocks();
		box.EnsureAllBlockMetrics(width, height);
		std::size_t resident = 0;
		for (const auto& block : box.blocks)
			if (block.layout) ++resident;
		return { box.virtualTotalHeight, resident };
	}

	static std::pair<bool, int> RoundTripVirtualCaret(
		RichTextBox& box,
		const std::wstring& text,
		float width,
		float height,
		int textIndex)
	{
		box.Text = text;
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = 256;
		box._isVirtualized = true;
		box.Arrange(cui::core::Rect{ 0.0f, 0.0f, width, height });
		const float renderWidth = box.TextViewportWidth();
		const float renderHeight = box.TextViewportHeight();
		box.RebuildBlocks();
		box.EnsureAllBlockMetrics(renderWidth, renderHeight);
		float x = 0.0f;
		float y = 0.0f;
		float caretHeight = 0.0f;
		if (!box.GetCaretMetrics(textIndex, x, y, caretHeight))
			return { false, -1 };
		return { true, box.HitTestGlobalIndex(
			x, y + caretHeight * 0.5f) };
	}

	static std::pair<int, int> TextPoint(
		RichTextBox& box, int textIndex)
	{
		box.UpdateLayout();
		CUI_EXPECT_FALSE(box._isVirtualized);
		CUI_EXPECT_TRUE(box._textLayoutCache != nullptr);
		auto* font = box.GetRenderFont();
		CUI_EXPECT_TRUE(font != nullptr);
		if (!font || !box._textLayoutCache) return {};
		const auto ranges = font->HitTestTextRange(
			box._textLayoutCache,
			static_cast<UINT32>(textIndex), 1);
		CUI_EXPECT_TRUE(!ranges.empty());
		if (ranges.empty()) return {};
		const auto& range = ranges.front();
		return {
			static_cast<int>(range.left + range.width * 0.5f
				+ box.Padding.Left),
			static_cast<int>(range.top + range.height * 0.5f
				- box._verticalScrollOffset + box.Padding.Top)
		};
	}

	static std::vector<VisualLine> ContinuousVisualLines(RichTextBox& box)
	{
		box.UpdateLayout();
		CUI_EXPECT_FALSE(box._isVirtualized);
		CUI_EXPECT_TRUE(box._textLayoutCache != nullptr);
		std::vector<VisualLine> result;
		if (!box._textLayoutCache) return result;
		UINT32 required = 0;
		(void)box._textLayoutCache->GetLineMetrics(nullptr, 0, &required);
		if (required == 0) return result;
		std::vector<DWRITE_LINE_METRICS> metrics(required);
		UINT32 written = 0;
		CUI_EXPECT_TRUE(SUCCEEDED(box._textLayoutCache->GetLineMetrics(
			metrics.data(), required, &written)));
		metrics.resize((std::min)(required, written));
		std::size_t start = 0;
		float top = 0.0f;
		for (const auto& line : metrics)
		{
			const auto visibleLength = line.length >= line.newlineLength
				? line.length - line.newlineLength : 0;
			result.push_back({ start, start + visibleLength,
				top, line.height });
			start += line.length;
			top += line.height;
		}
		return result;
	}

	static float CaretTop(RichTextBox& box)
	{
		box.UpdateLayout();
		float x = 0.0f;
		float y = 0.0f;
		float height = 0.0f;
		CUI_EXPECT_TRUE(box.GetCaretMetrics(
			box._selectionEnd, x, y, height));
		return y;
	}

	static void ForceVirtualizedLayout(
		RichTextBox& box, std::size_t blockLength = 256)
	{
		box._enableVirtualization = true;
		box._virtualizeThreshold = 1;
		box._blockCharCount = blockLength;
		box._textLayoutDirty = true;
		box.blocksDirty = true;
		box.blockMetricsDirty = true;
	}

	static std::vector<VisualLine> VirtualVisualLines(RichTextBox& box)
	{
		box.UpdateLayout();
		CUI_EXPECT_TRUE(box._isVirtualized);
		std::vector<VisualLine> result;
		for (std::size_t index = 0; index < box.blocks.size(); ++index)
		{
			const auto& block = box.blocks[index];
			if (!block.singleVisualLine) continue;
			result.push_back({ block.start,
				block.start + block.layoutLen,
				box.blockTops[index], block.height });
		}
		return result;
	}

	static int VisualLineBoundary(
		RichTextBox& box, int textIndex, bool lineEnd)
	{
		return box.GetVisualLineBoundary(textIndex, lineEnd);
	}

	static bool ExecuteCommand(
		RichTextBox& box, const RoutedCommand& command)
	{
		return box.ExecuteEditingCommand(command);
	}

	static std::optional<DWRITE_READING_DIRECTION> FirstBlockReadingDirection(
		RichTextBox& box)
	{
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = 256;
		box.RebuildBlocks();
		if (box.blocks.empty()) return std::nullopt;
		box.EnsureBlockLayout(0, 300.0f, 1000.0f);
		if (!box.blocks.front().layout) return std::nullopt;
		return box.blocks.front().layout->GetReadingDirection();
	}

	static std::optional<DWRITE_TEXT_ALIGNMENT> FirstBlockTextAlignment(
		RichTextBox& box)
	{
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = 256;
		box.RebuildBlocks();
		if (box.blocks.empty()) return std::nullopt;
		box.EnsureBlockLayout(0, 300.0f, 1000.0f);
		if (!box.blocks.front().layout) return std::nullopt;
		return box.blocks.front().layout->GetTextAlignment();
	}

	static std::optional<DWRITE_FONT_STRETCH> FirstBlockFontStretch(
		RichTextBox& box)
	{
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = 256;
		box.RebuildBlocks();
		if (box.blocks.empty()) return std::nullopt;
		box.EnsureBlockLayout(0, 300.0f, 1000.0f);
		if (!box.blocks.front().layout) return std::nullopt;
		DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_UNDEFINED;
		DWRITE_TEXT_RANGE range{};
		if (FAILED(box.blocks.front().layout->GetFontStretch(
			0, &stretch, &range))) return std::nullopt;
		return stretch;
	}

	static std::optional<std::wstring> FirstBlockLanguage(
		RichTextBox& box)
	{
		box.SyncBufferFromControlIfNeeded();
		box._blockCharCount = 256;
		box.RebuildBlocks();
		if (box.blocks.empty()) return std::nullopt;
		box.EnsureBlockLayout(0, 300.0f, 1000.0f);
		if (!box.blocks.front().layout) return std::nullopt;
		UINT32 length = 0;
		DWRITE_TEXT_RANGE range{};
		if (FAILED(box.blocks.front().layout->GetLocaleNameLength(
			0, &length, &range))) return std::nullopt;
		std::wstring value(static_cast<std::size_t>(length), L'\0');
		std::vector<wchar_t> buffer(static_cast<std::size_t>(length) + 1);
		if (FAILED(box.blocks.front().layout->GetLocaleName(
			0, buffer.data(), static_cast<UINT32>(buffer.size()), &range)))
			return std::nullopt;
		value.assign(buffer.data(), static_cast<std::size_t>(length));
		return value;
	}
};

namespace
{
	class MemoryClipboardBackend final
		: public cui::richtext::clipboard::Backend
	{
	public:
		bool Publish(
			void*,
			const cui::richtext::clipboard::DataObject& data) noexcept override
		{
			++PublishCount;
			if (RejectPublish || !data.PlainText
				|| data.PlainText->empty()) return false;
			try
			{
				Stored = data;
				return true;
			}
			catch (...)
			{
				return false;
			}
		}

		std::optional<cui::richtext::clipboard::DataObject> Read(
			void*) noexcept override
		{
			try
			{
				return Stored;
			}
			catch (...)
			{
				return std::nullopt;
			}
		}

		bool CanPaste() noexcept override
		{
			return Stored.has_value()
				&& (Stored->Attributed.has_value()
					|| (Stored->Rtf && !Stored->Rtf->empty())
					|| (Stored->PlainText && !Stored->PlainText->empty()));
		}

		std::optional<cui::richtext::clipboard::DataObject> Stored;
		bool RejectPublish = false;
		int PublishCount = 0;
	};

	struct NativeRtfReadState
	{
		std::string_view Value;
		std::size_t Position = 0;
	};

	DWORD CALLBACK ReadNativeRtf(
		DWORD_PTR cookie, LPBYTE buffer, LONG capacity, LONG* written)
	{
		if (!buffer || !written || capacity < 0) return 1;
		auto& state = *reinterpret_cast<NativeRtfReadState*>(cookie);
		const auto remaining = state.Value.size() - state.Position;
		const auto count = (std::min)(remaining,
			static_cast<std::size_t>(capacity));
		if (count != 0)
			std::memcpy(buffer, state.Value.data() + state.Position, count);
		state.Position += count;
		*written = static_cast<LONG>(count);
		return 0;
	}

	RichTextDocumentFragment Flatten(RichTextBox& box)
	{
		auto fragment = box.GetDocument().Flatten();
		CUI_EXPECT_TRUE(fragment.ValidateCanonical());
		CUI_EXPECT_EQ(box.Text, fragment.Text);
		return fragment;
	}

	void ExpectWeight(
		const RichTextDocumentFragment& fragment,
		std::size_t index,
		std::optional<DWRITE_FONT_WEIGHT> expected)
	{
		const RichTextDocument document(fragment);
		CUI_EXPECT_EQ(expected, document.StyleAt(index).FontWeight);
	}

	void ExpectUnderline(
		const RichTextDocumentFragment& fragment,
		std::size_t index,
		std::optional<bool> expected)
	{
		const RichTextDocument document(fragment);
		CUI_EXPECT_EQ(expected, document.StyleAt(index).Underline);
	}

	void ExpectFontSize(
		const RichTextDocumentFragment& fragment,
		std::size_t index,
		float expected)
	{
		const RichTextDocument document(fragment);
		const auto actual = document.StyleAt(index).FontSize;
		CUI_EXPECT_TRUE(actual.has_value());
		if (actual) CUI_EXPECT_NEAR(expected, *actual, 0.0001);
	}

	void ExpectSelectionWeight(
		const RichTextBox& box,
		TextSelectionPropertyValueKind expectedKind,
		DWRITE_FONT_WEIGHT expectedValue = DWRITE_FONT_WEIGHT_NORMAL)
	{
		const auto result = box.GetSelection().GetPropertyValue(
			TextElement::FontWeightProperty());
		CUI_EXPECT_EQ(expectedKind, result.Kind);
		if (expectedKind != TextSelectionPropertyValueKind::Value) return;
		DWRITE_FONT_WEIGHT value = DWRITE_FONT_WEIGHT_NORMAL;
		CUI_EXPECT_TRUE(result.Value.TryGet(value));
		CUI_EXPECT_EQ(expectedValue, value);
	}

	void ExpectSelectionUnderline(
		const RichTextBox& box,
		TextSelectionPropertyValueKind expectedKind,
		bool expectedValue = false)
	{
		const auto result = box.GetSelection().GetPropertyValue(
			TextElement::UnderlineProperty());
		CUI_EXPECT_EQ(expectedKind, result.Kind);
		if (expectedKind != TextSelectionPropertyValueKind::Value) return;
		bool value = false;
		CUI_EXPECT_TRUE(result.Value.TryGet(value));
		CUI_EXPECT_EQ(expectedValue, value);
	}

	void ExpectSelectionFontStretch(
		const RichTextBox& box,
		TextSelectionPropertyValueKind expectedKind,
		DWRITE_FONT_STRETCH expectedValue = DWRITE_FONT_STRETCH_NORMAL)
	{
		const auto result = box.GetSelection().GetPropertyValue(
			TextElement::FontStretchProperty());
		CUI_EXPECT_EQ(expectedKind, result.Kind);
		if (expectedKind != TextSelectionPropertyValueKind::Value) return;
		DWRITE_FONT_STRETCH value = DWRITE_FONT_STRETCH_NORMAL;
		CUI_EXPECT_TRUE(result.Value.TryGet(value));
		CUI_EXPECT_EQ(expectedValue, value);
	}

	void ExpectSelectionLanguage(
		const RichTextBox& box,
		TextSelectionPropertyValueKind expectedKind,
		std::wstring expectedValue = L"en-us")
	{
		const auto result = box.GetSelection().GetPropertyValue(
			TextElement::LanguageProperty());
		CUI_EXPECT_EQ(expectedKind, result.Kind);
		if (expectedKind != TextSelectionPropertyValueKind::Value) return;
		std::wstring value;
		CUI_EXPECT_TRUE(result.Value.TryGet(value));
		CUI_EXPECT_EQ(expectedValue, value);
	}

	void ExpectSelectionFontSize(
		const RichTextBox& box,
		TextSelectionPropertyValueKind expectedKind,
		float expectedValue = 0.0f)
	{
		const auto result = box.GetSelection().GetPropertyValue(
			TextElement::FontSizeProperty());
		CUI_EXPECT_EQ(expectedKind, result.Kind);
		if (expectedKind != TextSelectionPropertyValueKind::Value) return;
		float value = 0.0f;
		CUI_EXPECT_TRUE(result.Value.TryGet(value));
		CUI_EXPECT_EQ(expectedValue, value);
	}

	void ExpectSelectionAlignment(
		const RichTextBox& box,
		TextSelectionPropertyValueKind expectedKind,
		::TextAlignment expectedValue = ::TextAlignment::Left)
	{
		const auto result = box.GetSelection().GetPropertyValue(
			Block::TextAlignmentProperty());
		CUI_EXPECT_EQ(expectedKind, result.Kind);
		if (expectedKind != TextSelectionPropertyValueKind::Value) return;
		::TextAlignment value = ::TextAlignment::Left;
		CUI_EXPECT_TRUE(result.Value.TryGet(value));
		CUI_EXPECT_EQ(expectedValue, value);
	}

	void ExpectSelectionFlowDirection(
		const RichTextBox& box,
		TextSelectionPropertyValueKind expectedKind,
		::FlowDirection expectedValue = ::FlowDirection::LeftToRight)
	{
		const auto result = box.GetSelection().GetPropertyValue(
			Block::FlowDirectionProperty());
		CUI_EXPECT_EQ(expectedKind, result.Kind);
		if (expectedKind != TextSelectionPropertyValueKind::Value) return;
		::FlowDirection value = ::FlowDirection::LeftToRight;
		CUI_EXPECT_TRUE(result.Value.TryGet(value));
		CUI_EXPECT_EQ(expectedValue, value);
	}

	bool ProcessCommandGesture(
		Control& target,
		Key key,
		ModifierKeys modifiers = ModifierKeys::Control)
	{
		InputReport report;
		report.Kind = InputReportKind::KeyDown;
		report.Key = key;
		report.Modifiers = modifiers;
		auto args = report.CreateKeyEventArgs();
		return cui::framework::InputAccess::ProcessCommandInput(
			target, args);
	}
}

void RegisterRichTextBoxDocumentTests(cui::test::Runner& runner)
{
	runner.Add(
		"RichTextBox document has one owner and external Text resets formatting history",
		[]
	{
		RichTextBox first;
		RichTextBox second;
		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		paragraph.Inlines.AddRun(L"owned");
		auto* documentIdentity = document.get();
		first.SetDocument(std::move(document));

		CUI_EXPECT_TRUE(documentIdentity->GetOwner() == &first);
		CUI_EXPECT_FALSE(documentIdentity->TryAttachOwner(&second));
		CUI_EXPECT_TRUE(second.GetDocument().GetOwner() == &second);
		CUI_EXPECT_EQ(std::wstring(L"owned"), first.Text);

		first.Select(0, 5);
		CUI_EXPECT_TRUE(first.GetSelection().ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_BOLD)));
		CUI_EXPECT_TRUE(first.CanUndo());
		ExpectWeight(Flatten(first), 2, DWRITE_FONT_WEIGHT_BOLD);

		first.Undo();
		CUI_EXPECT_TRUE(first.CanRedo());
		first.Redo();
		CUI_EXPECT_TRUE(first.CanUndo());
		first.Text = L"plain";

		CUI_EXPECT_EQ(std::wstring(L"plain"), first.Text);
		CUI_EXPECT_FALSE(first.CanUndo());
		CUI_EXPECT_FALSE(first.CanRedo());
		const auto plain = Flatten(first);
		CUI_EXPECT_EQ(1ULL,
			static_cast<unsigned long long>(plain.Spans.size()));
		CUI_EXPECT_EQ(RichTextCharacterStyle{}, plain.Spans[0].Style);
		first.SelectAll();
		ExpectSelectionWeight(
			first, TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_NORMAL);
	});

	runner.Add(
		"RichTextBox document Run and Paragraph mutations project live Text and styles",
		[]
	{
		RichTextBox box;
		auto& blocks = box.GetDocument().Blocks;
		blocks.Clear();
		auto& firstParagraph = blocks.AddParagraph();
		auto& firstRun = firstParagraph.Inlines.AddRun(L"Alpha");
		auto& secondParagraph = blocks.AddParagraph();
		auto& secondRun = secondParagraph.Inlines.AddRun(L"Beta");

		CUI_EXPECT_EQ(std::wstring(L"Alpha\r\nBeta"), box.Text);
		firstParagraph.SetFontWeight(DWRITE_FONT_WEIGHT_BOLD);
		firstRun.SetUnderline(true);
		secondRun.SetFontStyle(DWRITE_FONT_STYLE_ITALIC);
		const auto styled = Flatten(box);
		ExpectWeight(styled, 0, DWRITE_FONT_WEIGHT_BOLD);
		ExpectUnderline(styled, 0, true);
		ExpectWeight(styled, 5, DWRITE_FONT_WEIGHT_BOLD);
		CUI_EXPECT_EQ(
			std::optional<DWRITE_FONT_STYLE>(DWRITE_FONT_STYLE_ITALIC),
			RichTextDocument(styled).StyleAt(7).FontStyle);

		firstRun.Text = L"A\nA";
		CUI_EXPECT_EQ(std::wstring(L"A\r\nA\r\nBeta"), box.Text);
		CUI_EXPECT_EQ(std::wstring(L"A\r\nA\r\nBeta"), Flatten(box).Text);
		CUI_EXPECT_FALSE(box.CanUndo());

		auto removed = blocks.Remove(secondParagraph);
		CUI_EXPECT_TRUE(removed != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"A\r\nA"), box.Text);
		CUI_EXPECT_TRUE(removed->GetParent() == nullptr);
		CUI_EXPECT_TRUE(removed->GetFlowDocument() == nullptr);
		CUI_EXPECT_EQ(std::wstring(L"A\r\nA"), Flatten(box).Text);
	});

	runner.Add(
		"RichTextBox document selection formatting splits spans and typing style formats insertion",
		[]
	{
		RichTextBox selectionBox;
		selectionBox.Text = L"abcdef";
		selectionBox.Select(2, 2);
		CUI_EXPECT_TRUE(selectionBox.GetSelection().ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_BOLD)));

		const auto split = Flatten(selectionBox);
		CUI_EXPECT_EQ(3ULL,
			static_cast<unsigned long long>(split.Spans.size()));
		CUI_EXPECT_EQ(0ULL,
			static_cast<unsigned long long>(split.Spans[0].Start));
		CUI_EXPECT_EQ(2ULL,
			static_cast<unsigned long long>(split.Spans[0].Length));
		CUI_EXPECT_EQ(2ULL,
			static_cast<unsigned long long>(split.Spans[1].Start));
		CUI_EXPECT_EQ(2ULL,
			static_cast<unsigned long long>(split.Spans[1].Length));
		ExpectWeight(split, 1, std::nullopt);
		ExpectWeight(split, 2, DWRITE_FONT_WEIGHT_BOLD);
		ExpectWeight(split, 4, std::nullopt);
		ExpectSelectionWeight(selectionBox,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_BOLD);
		selectionBox.Select(1, 3);
		ExpectSelectionWeight(
			selectionBox, TextSelectionPropertyValueKind::Mixed);

		RichTextBox typingBox;
		typingBox.Text = L"abc";
		typingBox.Select(0, 0);
		CUI_EXPECT_TRUE(typingBox.GetSelection().ApplyPropertyValue(
			TextElement::UnderlineProperty(), BindingValue(true)));
		ExpectSelectionUnderline(typingBox,
			TextSelectionPropertyValueKind::Value, true);
		CUI_EXPECT_FALSE(typingBox.CanUndo());
		typingBox.InsertText(L"X");

		CUI_EXPECT_EQ(std::wstring(L"Xabc"), typingBox.Text);
		ExpectUnderline(Flatten(typingBox), 0, true);
		CUI_EXPECT_TRUE(typingBox.CanUndo());
		typingBox.Undo();
		CUI_EXPECT_EQ(std::wstring(L"abc"), typingBox.Text);
		CUI_EXPECT_EQ(0, typingBox.GetCaretIndex());
		ExpectSelectionUnderline(typingBox,
			TextSelectionPropertyValueKind::Value, true);
		typingBox.Redo();
		CUI_EXPECT_EQ(std::wstring(L"Xabc"), typingBox.Text);
		ExpectUnderline(Flatten(typingBox), 0, true);
	});

	runner.Add(
		"RichTextBox document collapsed format inside word applies word and preserves caret",
		[]
	{
		RichTextBox box;
		box.Text = L"abc def";
		box.Select(1, 0);
		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_BOLD)));

		CUI_EXPECT_EQ(std::wstring(L"abc def"), box.Text);
		CUI_EXPECT_EQ(1, box.GetSelectionStart());
		CUI_EXPECT_EQ(0, box.GetSelectionLength());
		const auto formatted = Flatten(box);
		ExpectWeight(formatted, 0, DWRITE_FONT_WEIGHT_BOLD);
		ExpectWeight(formatted, 2, DWRITE_FONT_WEIGHT_BOLD);
		ExpectWeight(formatted, 3, std::nullopt);
		CUI_EXPECT_TRUE(box.CanUndo());

		box.Undo();
		CUI_EXPECT_EQ(1, box.GetCaretIndex());
		CUI_EXPECT_EQ(RichTextCharacterStyle{}, Flatten(box).Spans[0].Style);
		CUI_EXPECT_TRUE(box.CanRedo());
		box.Redo();
		CUI_EXPECT_EQ(1, box.GetCaretIndex());
		ExpectWeight(Flatten(box), 1, DWRITE_FONT_WEIGHT_BOLD);
	});

	runner.Add(
		"RichTextBox document collapsed ClearAll only clears typing style",
		[]
	{
		RichTextBox word;
		word.Text = L"abc";
		word.SelectAll();
		CUI_EXPECT_TRUE(word.GetSelection().ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_BOLD)));
		word.Select(1, 0);
		word.GetSelection().ClearAllProperties();
		CUI_EXPECT_EQ(1, word.GetCaretIndex());
		ExpectWeight(Flatten(word), 1, DWRITE_FONT_WEIGHT_BOLD);
		word.Undo();
		CUI_EXPECT_EQ(RichTextCharacterStyle{}, Flatten(word).Spans[0].Style);

		RichTextBox springload;
		springload.Text = L"abc";
		springload.Select(0, 0);
		CUI_EXPECT_TRUE(springload.GetSelection().ApplyPropertyValue(
			TextElement::UnderlineProperty(), BindingValue(true)));
		ExpectSelectionUnderline(springload,
			TextSelectionPropertyValueKind::Value, true);
		springload.GetSelection().ClearAllProperties();
		ExpectSelectionUnderline(springload,
			TextSelectionPropertyValueKind::Value, false);
		CUI_EXPECT_FALSE(springload.CanUndo());
		springload.InsertText(L"X");
		ExpectUnderline(Flatten(springload), 0, std::nullopt);
	});

	runner.Add(
		"RichTextBox document property query resolves implicit and explicit defaults",
		[]
	{
		RichTextBox implicit;
		implicit.SetFontSize(30.0);
		implicit.Text = L"abc";
		implicit.SelectAll();
		ExpectSelectionFontSize(implicit,
			TextSelectionPropertyValueKind::Value, 30.0f);
		ExpectSelectionWeight(implicit,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_NORMAL);
		ExpectSelectionUnderline(implicit,
			TextSelectionPropertyValueKind::Value, false);
		CUI_EXPECT_TRUE(implicit.GetSelection().ApplyPropertyValue(
			TextElement::FontSizeProperty(), BindingValue(30.0)));
		CUI_EXPECT_FALSE(implicit.CanUndo());
		implicit.SetFontSize(40.0);
		ExpectSelectionFontSize(implicit,
			TextSelectionPropertyValueKind::Value, 40.0f);

		auto defaultDocument = std::make_unique<FlowDocument>();
		auto* defaultDocumentIdentity = defaultDocument.get();
		defaultDocument->Blocks.AddParagraph().Inlines.AddRun(L"abc");
		RichTextBox explicitDefault;
		explicitDefault.SetFontSize(30.0);
		explicitDefault.SetDocument(std::move(defaultDocument));
		explicitDefault.SelectAll();
		ExpectSelectionFontSize(explicitDefault,
			TextSelectionPropertyValueKind::Value, 12.0f);
		CUI_EXPECT_TRUE(explicitDefault.GetSelection().ApplyPropertyValue(
			TextElement::FontSizeProperty(), BindingValue(12.0)));
		CUI_EXPECT_FALSE(explicitDefault.CanUndo());
		defaultDocumentIdentity->SetFontSize(18.0);
		ExpectSelectionFontSize(explicitDefault,
			TextSelectionPropertyValueKind::Value, 18.0f);

		auto localDocument = std::make_unique<FlowDocument>();
		localDocument->SetFontSize(18.0);
		auto& paragraph = localDocument->Blocks.AddParagraph();
		paragraph.SetFontWeight(DWRITE_FONT_WEIGHT_BOLD);
		paragraph.Inlines.AddRun(L"abc");
		RichTextBox explicitLocal;
		explicitLocal.SetFontSize(30.0);
		explicitLocal.SetDocument(std::move(localDocument));
		explicitLocal.SelectAll();
		ExpectSelectionFontSize(explicitLocal,
			TextSelectionPropertyValueKind::Value, 18.0f);
		ExpectSelectionWeight(explicitLocal,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_BOLD);

		explicitLocal.Select(1, 0);
		ExpectSelectionFontSize(explicitLocal,
			TextSelectionPropertyValueKind::Value, 18.0f);

		RichTextBox largeFont;
		largeFont.Text = L"large";
		largeFont.SelectAll();
		CUI_EXPECT_TRUE(largeFont.GetSelection().ApplyPropertyValue(
			TextElement::FontSizeProperty(), BindingValue(500.0)));
		ExpectSelectionFontSize(largeFont,
			TextSelectionPropertyValueKind::Value, 500.0f);
		CUI_EXPECT_TRUE(largeFont.CanUndo());
	});

	runner.Add(
		"RichTextBox FontStretch inherits formats lays out and undoes",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		document->SetFontStretch(DWRITE_FONT_STRETCH_CONDENSED);
		auto* documentIdentity = document.get();
		auto& paragraph = document->Blocks.AddParagraph();
		auto& run = paragraph.Inlines.AddRun(L"stretch");
		RichTextBox box;
		box.SetDocument(std::move(document));
		box.SelectAll();

		CUI_EXPECT_EQ(DWRITE_FONT_STRETCH_CONDENSED,
			run.GetFontStretch());
		ExpectSelectionFontStretch(box,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_STRETCH_CONDENSED);
		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			TextElement::FontStretchProperty(), BindingValue(L"Condensed")));
		CUI_EXPECT_FALSE(box.CanUndo());

		documentIdentity->SetFontStretch(DWRITE_FONT_STRETCH_SEMI_CONDENSED);
		ExpectSelectionFontStretch(box,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_STRETCH_SEMI_CONDENSED);
		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			TextElement::FontStretchProperty(),
			BindingValue(DWRITE_FONT_STRETCH_EXPANDED)));
		ExpectSelectionFontStretch(box,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_STRETCH_EXPANDED);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_STRETCH>(
			DWRITE_FONT_STRETCH_EXPANDED),
			RichTextBoxDocumentTestAccess::FirstBlockFontStretch(box));
		CUI_EXPECT_TRUE(box.CanUndo());

		box.Undo();
		ExpectSelectionFontStretch(box,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_STRETCH_SEMI_CONDENSED);
		box.Redo();
		ExpectSelectionFontStretch(box,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_STRETCH_EXPANDED);
		box.GetSelection().ClearAllProperties();
		ExpectSelectionFontStretch(box,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_STRETCH_SEMI_CONDENSED);
		box.Undo();
		ExpectSelectionFontStretch(box,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_STRETCH_EXPANDED);

		CUI_EXPECT_FALSE(box.GetSelection().ApplyPropertyValue(
			TextElement::FontStretchProperty(), BindingValue(L"Wide")));
	});

	runner.Add(
		"RichTextBox Language inherits normalizes lays out and undoes",
		[]
	{
		RichTextBox implicit;
		implicit.SetLanguage(L"JA-JP");
		CUI_EXPECT_EQ(std::wstring(L"ja-jp"), implicit.GetLanguage());
		implicit.Text = L"implicit";
		implicit.SelectAll();
		ExpectSelectionLanguage(implicit,
			TextSelectionPropertyValueKind::Value, L"ja-jp");
		CUI_EXPECT_TRUE(implicit.GetSelection().ApplyPropertyValue(
			TextElement::LanguageProperty(), BindingValue(L"ja-JP")));
		CUI_EXPECT_FALSE(implicit.CanUndo());
		implicit.SetLanguage(L"zh-Hans-CN");
		ExpectSelectionLanguage(implicit,
			TextSelectionPropertyValueKind::Value, L"zh-hans-cn");

		auto document = std::make_unique<FlowDocument>();
		document->SetLanguage(L"fr-FR");
		auto* documentIdentity = document.get();
		auto& paragraph = document->Blocks.AddParagraph();
		auto& run = paragraph.Inlines.AddRun(L"langue");
		RichTextBox box;
		box.SetLanguage(L"de-DE");
		box.SetDocument(std::move(document));
		box.SelectAll();
		CUI_EXPECT_EQ(std::wstring(L"fr-fr"), run.GetLanguage());
		ExpectSelectionLanguage(box,
			TextSelectionPropertyValueKind::Value, L"fr-fr");
		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			TextElement::LanguageProperty(), BindingValue(L"FR-fr")));
		CUI_EXPECT_FALSE(box.CanUndo());

		documentIdentity->SetLanguage(L"es-ES");
		ExpectSelectionLanguage(box,
			TextSelectionPropertyValueKind::Value, L"es-es");
		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			TextElement::LanguageProperty(), BindingValue(L"ko-KR")));
		ExpectSelectionLanguage(box,
			TextSelectionPropertyValueKind::Value, L"ko-kr");
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"ko-kr"),
			RichTextBoxDocumentTestAccess::FirstBlockLanguage(box));
		CUI_EXPECT_TRUE(box.CanUndo());

		box.Undo();
		ExpectSelectionLanguage(box,
			TextSelectionPropertyValueKind::Value, L"es-es");
		box.Redo();
		ExpectSelectionLanguage(box,
			TextSelectionPropertyValueKind::Value, L"ko-kr");
		box.GetSelection().ClearAllProperties();
		ExpectSelectionLanguage(box,
			TextSelectionPropertyValueKind::Value, L"es-es");
		box.Undo();
		ExpectSelectionLanguage(box,
			TextSelectionPropertyValueKind::Value, L"ko-kr");

		CUI_EXPECT_FALSE(box.GetSelection().ApplyPropertyValue(
			TextElement::LanguageProperty(), BindingValue(L"9-invalid")));
		const auto before = box.GetLanguage();
		box.SetLanguage(L"not valid");
		CUI_EXPECT_EQ(before, box.GetLanguage());
	});

	runner.Add(
		"RichTextBox document rich formatting and attributed replacement undo redo exactly",
		[]
	{
		RichTextBox box;
		box.Text = L"abcdef";
		box.Select(2, 2);
		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_BOLD)));
		const auto formatted = Flatten(box);
		ExpectWeight(formatted, 2, DWRITE_FONT_WEIGHT_BOLD);

		box.Undo();
		CUI_EXPECT_EQ(std::wstring(L"abcdef"), box.Text);
		CUI_EXPECT_EQ(2, box.GetSelectionStart());
		CUI_EXPECT_EQ(2, box.GetSelectionLength());
		CUI_EXPECT_EQ(RichTextCharacterStyle{}, Flatten(box).Spans[0].Style);
		box.Redo();
		CUI_EXPECT_EQ(formatted, Flatten(box));
		CUI_EXPECT_EQ(2, box.GetSelectionStart());
		CUI_EXPECT_EQ(2, box.GetSelectionLength());

		box.GetSelection().SetText(L"Q");
		CUI_EXPECT_EQ(std::wstring(L"abQef"), box.Text);
		ExpectWeight(Flatten(box), 2, DWRITE_FONT_WEIGHT_BOLD);
		CUI_EXPECT_EQ(3, box.GetCaretIndex());
		box.Undo();
		CUI_EXPECT_EQ(formatted, Flatten(box));
		CUI_EXPECT_EQ(2, box.GetSelectionStart());
		CUI_EXPECT_EQ(2, box.GetSelectionLength());
		box.Redo();
		CUI_EXPECT_EQ(std::wstring(L"abQef"), box.Text);
		ExpectWeight(Flatten(box), 2, DWRITE_FONT_WEIGHT_BOLD);
		CUI_EXPECT_EQ(3, box.GetCaretIndex());
	});

	runner.Add(
		"RichTextBox document formatting expands a partial CRLF selection",
		[]
	{
		const std::wstring original =
			L"A\r\n\xD83D\xDE00" L"B";
		RichTextBox crlfBox;
		crlfBox.Text = original;
		crlfBox.Select(2, 1);
		CUI_EXPECT_EQ(std::wstring(L"\r\n"),
			crlfBox.GetSelection().GetText());
		CUI_EXPECT_TRUE(crlfBox.GetSelection().ApplyPropertyValue(
			TextElement::UnderlineProperty(), BindingValue(true)));
		const auto crlfStyled = Flatten(crlfBox);
		const RichTextDocument styledDocument(crlfStyled);
		CUI_EXPECT_EQ(std::optional<bool>{},
			styledDocument.StyleAt(0).Underline);
		CUI_EXPECT_EQ(std::optional<bool>(true),
			styledDocument.StyleAt(1).Underline);
		CUI_EXPECT_EQ(std::optional<bool>(true),
			styledDocument.StyleAt(2).Underline);
		CUI_EXPECT_EQ(std::optional<bool>{},
			styledDocument.StyleAt(3).Underline);
		crlfBox.Undo();
		CUI_EXPECT_EQ(original, crlfBox.Text);
		CUI_EXPECT_EQ(RichTextCharacterStyle{},
			Flatten(crlfBox).Spans[0].Style);
		crlfBox.Redo();
		CUI_EXPECT_EQ(crlfStyled, Flatten(crlfBox));
	});

	runner.Add(
		"RichTextBox document replacement expands a partial surrogate selection",
		[]
	{
		const std::wstring original =
			L"A\r\n\xD83D\xDE00" L"B";
		RichTextBox surrogateBox;
		surrogateBox.Text = original;
		surrogateBox.Select(4, 1);
		CUI_EXPECT_EQ(std::wstring(L"\xD83D\xDE00"),
			surrogateBox.GetSelection().GetText());
		surrogateBox.GetSelection().SetText(L"X");
		CUI_EXPECT_EQ(std::wstring(L"A\r\nXB"), surrogateBox.Text);
		CUI_EXPECT_EQ(4, surrogateBox.GetCaretIndex());
		CUI_EXPECT_TRUE(Flatten(surrogateBox).ValidateCanonical());
		surrogateBox.Undo();
		CUI_EXPECT_EQ(original, surrogateBox.Text);
		CUI_EXPECT_EQ(std::wstring(L"\xD83D\xDE00"),
			surrogateBox.GetSelection().GetText());
		surrogateBox.Redo();
		CUI_EXPECT_EQ(std::wstring(L"A\r\nXB"), surrogateBox.Text);
	});

	runner.Add(
		"RichTextBox document insertion snaps a caret out of CRLF",
		[]
	{
		const std::wstring original =
			L"A\r\n\xD83D\xDE00" L"B";
		RichTextBox insertionBox;
		insertionBox.Text = original;
		insertionBox.Select(2, 0);
		CUI_EXPECT_TRUE(insertionBox.GetSelection().ApplyPropertyValue(
			TextElement::UnderlineProperty(), BindingValue(true)));
		insertionBox.InsertText(L"X");
		CUI_EXPECT_EQ(
			std::wstring(L"A\r\nX\xD83D\xDE00" L"B"),
			insertionBox.Text);
		CUI_EXPECT_EQ(4, insertionBox.GetCaretIndex());
		ExpectUnderline(Flatten(insertionBox), 3, true);
	});

	runner.Add(
		"RichTextBox document reconstruction keeps ancestor formatting live",
		[]
	{
		RichTextBox box;
		box.Text = L"A\r\nB";
		auto* firstParagraph = dynamic_cast<Paragraph*>(
			box.GetDocument().Blocks.At(0));
		CUI_EXPECT_TRUE(firstParagraph != nullptr);
		firstParagraph->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD);
		auto paragraphStyled = Flatten(box);
		ExpectWeight(paragraphStyled, 0, DWRITE_FONT_WEIGHT_BOLD);
		ExpectWeight(paragraphStyled, 1, DWRITE_FONT_WEIGHT_BOLD);
		ExpectWeight(paragraphStyled, 2, DWRITE_FONT_WEIGHT_BOLD);
		ExpectWeight(paragraphStyled, 3, std::nullopt);

		// A touched branch may be reconstructed while untouched structured
		// subtrees retain identity. In either case inheritance must stay live
		// instead of being frozen as Run-local formatting.
		box.Select(static_cast<int>(box.Text.size()), 0);
		box.InsertText(L"!");
		firstParagraph = dynamic_cast<Paragraph*>(
			box.GetDocument().Blocks.At(0));
		CUI_EXPECT_TRUE(firstParagraph != nullptr);
		firstParagraph->SetFontWeight(DWRITE_FONT_WEIGHT_NORMAL);
		auto paragraphUpdated = Flatten(box);
		ExpectWeight(paragraphUpdated, 0, DWRITE_FONT_WEIGHT_NORMAL);
		ExpectWeight(paragraphUpdated, 1, DWRITE_FONT_WEIGHT_NORMAL);
		ExpectWeight(paragraphUpdated, 2, DWRITE_FONT_WEIGHT_NORMAL);
		ExpectWeight(paragraphUpdated, 3, std::nullopt);

		box.GetDocument().SetUnderline(true);
		box.Select(static_cast<int>(box.Text.size()), 0);
		box.InsertText(L"?");
		box.GetDocument().SetUnderline(false);
		const auto documentUpdated = Flatten(box);
		const RichTextDocument updatedDocument(documentUpdated);
		for (std::size_t index = 0; index < documentUpdated.Text.size(); ++index)
		{
			CUI_EXPECT_EQ(std::optional<bool>(false),
				updatedDocument.StyleAt(index).Underline);
		}
	});

	runner.Add(
		"FlowDocument structured reconcile preserves untouched object identity",
		[]
	{
		FlowDocument document;
		auto& first = document.Blocks.AddParagraph();
		auto& left = first.Inlines.AddRun(L"A");
		auto nested = std::make_unique<Span>();
		auto* originalNested = nested.get();
		auto& edited = nested->Inlines.AddRun(L"B");
		auto& right = nested->Inlines.AddRun(L"C");
		first.Inlines.Add(std::move(nested));

		auto& second = document.Blocks.AddParagraph();
		auto bold = std::make_unique<Bold>();
		auto* originalBold = bold.get();
		auto& secondRun = bold->Inlines.AddRun(L"D");
		second.Inlines.Add(std::move(bold));

		auto* originalFirst = &first;
		auto* originalEdited = &edited;
		auto* originalSecond = &second;
		auto changed = document.Flatten();
		CUI_EXPECT_TRUE(changed.ValidateCanonical());
		changed.Text[1] = L'X';
		CUI_EXPECT_TRUE(changed.ValidateCanonical());
		CUI_EXPECT_TRUE(document.ReplaceFromFragment(changed));

		auto* currentFirst = dynamic_cast<Paragraph*>(
			document.Blocks.At(0));
		auto* currentNested = currentFirst
			? dynamic_cast<Span*>(currentFirst->Inlines.At(1)) : nullptr;
		CUI_EXPECT_TRUE(currentFirst != nullptr);
		CUI_EXPECT_TRUE(currentFirst == originalFirst);
		CUI_EXPECT_TRUE(currentFirst->Inlines.At(0) == &left);
		CUI_EXPECT_TRUE(currentNested != nullptr);
		CUI_EXPECT_TRUE(currentNested == originalNested);
		CUI_EXPECT_TRUE(currentNested->Inlines.At(0) != originalEdited);
		CUI_EXPECT_TRUE(currentNested->Inlines.At(1) == &right);
		CUI_EXPECT_TRUE(document.Blocks.At(1) == originalSecond);
		CUI_EXPECT_TRUE(originalSecond->Inlines.At(0) == originalBold);
		CUI_EXPECT_TRUE(originalBold->Inlines.At(0) == &secondRun);
		CUI_EXPECT_TRUE(left.GetParent() == currentFirst);
		CUI_EXPECT_TRUE(right.GetParent() == currentNested);
		CUI_EXPECT_TRUE(left.GetFlowDocument() == &document);
		CUI_EXPECT_TRUE(right.GetFlowDocument() == &document);

		// A no-op structured replacement reuses both complete Paragraph trees.
		auto* noOpFirst = document.Blocks.At(0);
		auto* noOpSecond = document.Blocks.At(1);
		CUI_EXPECT_TRUE(document.ReplaceFromFragment(document.Flatten()));
		CUI_EXPECT_TRUE(document.Blocks.At(0) == noOpFirst);
		CUI_EXPECT_TRUE(document.Blocks.At(1) == noOpSecond);
	});

	runner.Add(
		"RichTextBox effective formatting no-op keeps intrinsic inheritance live",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		auto bold = std::make_unique<Bold>();
		auto* boldIdentity = bold.get();
		auto& run = bold->Inlines.AddRun(L"A");
		paragraph.Inlines.Add(std::move(bold));
		RichTextBox box;
		box.SetDocument(std::move(document));
		box.SelectAll();
		const auto before = box.GetDocument().Flatten();
		CUI_EXPECT_FALSE(box.CanUndo());
		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_BOLD)));
		CUI_EXPECT_EQ(before, box.GetDocument().Flatten());
		CUI_EXPECT_FALSE(box.CanUndo());
		CUI_EXPECT_TRUE(box.GetDocument().Blocks.At(0) == &paragraph);
		CUI_EXPECT_TRUE(paragraph.Inlines.At(0) == boldIdentity);
		CUI_EXPECT_TRUE(boldIdentity->Inlines.At(0) == &run);

		boldIdentity->SetFontWeight(DWRITE_FONT_WEIGHT_NORMAL);
		ExpectWeight(box.GetDocument().Flatten(), 0,
			DWRITE_FONT_WEIGHT_NORMAL);
		box.SelectAll();
		ExpectSelectionWeight(box,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_NORMAL);
		});

	runner.Add(
		"RichTextBox zero-width Inline markers preserve untouched object identity",
		[]
		{
			auto document = std::make_unique<FlowDocument>();
			auto& first = document->Blocks.AddParagraph();
			auto& editedRun = first.Inlines.AddRun(L"A");
			auto emptySpan = std::make_unique<Span>();
			auto* emptySpanIdentity = emptySpan.get();
			auto& nestedEmptyRun = emptySpan->Inlines.AddRun();
			first.Inlines.Add(std::move(emptySpan));
			auto& rightRun = first.Inlines.AddRun(L"B");

			auto& trailing = document->Blocks.AddParagraph();
			auto emptyBold = std::make_unique<Bold>();
			auto* emptyBoldIdentity = emptyBold.get();
			trailing.Inlines.Add(std::move(emptyBold));
			auto* firstIdentity = &first;
			auto* trailingIdentity = &trailing;

			RichTextBox box;
			box.SetDocument(std::move(document));
			const auto initial = Flatten(box);
			CUI_EXPECT_EQ(std::wstring(L"AB\r\n"), initial.Text);
			CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
				initial.StructureMarkers.size()));
			CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
				initial.StructureMarkers[0].Position));
			CUI_EXPECT_EQ(4ULL, static_cast<unsigned long long>(
				initial.StructureMarkers[1].Position));
			CUI_EXPECT_EQ(nestedEmptyRun.GetRichTextStructureId(),
				initial.StructureMarkers[0].Path.back().Id);
			CUI_EXPECT_EQ(emptyBoldIdentity->GetRichTextStructureId(),
				initial.StructureMarkers[1].Path.back().Id);
			int noOpChanged = 0;
			auto noOpSubscription = box.GetDocument().Changed.Subscribe(
				[&](FlowDocument*) { ++noOpChanged; });
			CUI_EXPECT_TRUE(box.GetDocument().ReplaceFromFragment(initial));
			CUI_EXPECT_EQ(0, noOpChanged);
			CUI_EXPECT_TRUE(box.GetDocument().Blocks.At(0) == firstIdentity);
			CUI_EXPECT_TRUE(firstIdentity->Inlines.At(1)
				== emptySpanIdentity);
			CUI_EXPECT_TRUE(emptySpanIdentity->Inlines.At(0)
				== &nestedEmptyRun);

			box.Select(0, 0);
			box.InsertText(L"Z");
			CUI_EXPECT_EQ(std::wstring(L"ZAB\r\n"), box.Text);
			CUI_EXPECT_TRUE(box.GetDocument().Blocks.At(0) == firstIdentity);
			CUI_EXPECT_TRUE(box.GetDocument().Blocks.At(1) == trailingIdentity);
			CUI_EXPECT_TRUE(firstIdentity->Inlines.At(1)
				== emptySpanIdentity);
			CUI_EXPECT_TRUE(emptySpanIdentity->Inlines.At(0)
				== &nestedEmptyRun);
			CUI_EXPECT_TRUE(trailingIdentity->Inlines.At(0)
				== emptyBoldIdentity);
			CUI_EXPECT_TRUE(firstIdentity->Inlines.At(2) == &rightRun);
			CUI_EXPECT_TRUE(firstIdentity->Inlines.At(0) != &editedRun);
			const auto inserted = Flatten(box);
			CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
				inserted.StructureMarkers[0].Position));
			CUI_EXPECT_EQ(5ULL, static_cast<unsigned long long>(
				inserted.StructureMarkers[1].Position));

			box.Undo();
			CUI_EXPECT_EQ(std::wstring(L"AB\r\n"), box.Text);
			CUI_EXPECT_TRUE(firstIdentity->Inlines.At(1)
				== emptySpanIdentity);
			CUI_EXPECT_TRUE(emptySpanIdentity->Inlines.At(0)
				== &nestedEmptyRun);
			CUI_EXPECT_TRUE(trailingIdentity->Inlines.At(0)
				== emptyBoldIdentity);
			box.Redo();
			CUI_EXPECT_EQ(std::wstring(L"ZAB\r\n"), box.Text);
			CUI_EXPECT_TRUE(firstIdentity->Inlines.At(1)
				== emptySpanIdentity);
			CUI_EXPECT_TRUE(trailingIdentity->Inlines.At(0)
				== emptyBoldIdentity);
		});

	runner.Add(
		"FlowDocument nested spans reject cycles and propagate live inheritance",
		[]
	{
		auto outer = std::make_unique<Span>();
		auto inner = std::make_unique<Span>();
		auto* innerIdentity = inner.get();
		outer->Inlines.Add(std::move(inner));
		bool cycleRejected = false;
		try
		{
			innerIdentity->Inlines.Add(std::move(outer));
		}
		catch (const std::invalid_argument&)
		{
			cycleRejected = true;
		}
		CUI_EXPECT_TRUE(cycleRejected);
		CUI_EXPECT_TRUE(outer != nullptr);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			outer->Inlines.Count()));
		CUI_EXPECT_TRUE(outer->Inlines.At(0) == innerIdentity);
		CUI_EXPECT_TRUE(innerIdentity->GetParent() == outer.get());
		CUI_EXPECT_TRUE(innerIdentity->Inlines.Empty());

		FlowDocument document;
		auto& paragraph = document.Blocks.AddParagraph();
		auto styledOuter = std::make_unique<Span>();
		auto* styledOuterIdentity = styledOuter.get();
		auto bold = std::make_unique<Bold>();
		auto* boldIdentity = bold.get();
		auto& run = bold->Inlines.AddRun(L"A");
		styledOuter->Inlines.Add(std::move(bold));
		paragraph.Inlines.Add(std::move(styledOuter));
		styledOuterIdentity->SetFontSize(24.0);
		CUI_EXPECT_EQ(24.0, run.GetFontSize());
		CUI_EXPECT_EQ(DWRITE_FONT_WEIGHT_BOLD, run.GetFontWeight());
		styledOuterIdentity->SetFontSize(36.0);
		CUI_EXPECT_EQ(36.0, run.GetFontSize());
		boldIdentity->SetFontWeight(DWRITE_FONT_WEIGHT_NORMAL);
		CUI_EXPECT_EQ(DWRITE_FONT_WEIGHT_NORMAL, run.GetFontWeight());
	});

	runner.Add(
		"FlowDocument structured transaction rejects callback reentry and restores pointers",
		[]
	{
		FlowDocument document;
		auto& paragraph = document.Blocks.AddParagraph();
		auto& first = paragraph.Inlines.AddRun(L"A");
		paragraph.Inlines.AddRun(L"B");
		auto& third = paragraph.Inlines.AddRun(L"C");
		third.SetUnderline(true);

		bool collectionRejected = false;
		bool textRejected = false;
		bool formatRejected = false;
		bool rawSetRejected = false;
		bool rawClearRejected = false;
		bool rawCoerceRejected = false;
		bool scopeRejected = false;
		auto guarding = document.Changed.Subscribe(
			[&](FlowDocument*)
			{
				try { document.Blocks.Clear(); }
				catch (const std::logic_error&) { collectionRejected = true; }
				try { third.SetText(L"!"); }
				catch (const std::logic_error&) { textRejected = true; }
				try { third.SetFontSize(42.0); }
				catch (const std::logic_error&) { formatRejected = true; }
				try
				{
					(void)third.TrySetPropertyValue(
						TextElement::FontWeightProperty(),
						BindingValue(DWRITE_FONT_WEIGHT_BOLD));
				}
				catch (const std::logic_error&) { rawSetRejected = true; }
				try
				{
					(void)third.ClearPropertyValue(
						TextElement::UnderlineProperty());
				}
				catch (const std::logic_error&) { rawClearRejected = true; }
				try
				{
					(void)third.CoerceValue(
						TextElement::UnderlineProperty());
				}
				catch (const std::logic_error&) { rawCoerceRejected = true; }
				try { document.BeginChange(); }
				catch (const std::logic_error&) { scopeRejected = true; }
			});

		auto successful = document.Flatten();
		successful.Text[1] = L'X';
		CUI_EXPECT_TRUE(document.ReplaceFromFragment(successful));
		CUI_EXPECT_TRUE(collectionRejected);
		CUI_EXPECT_TRUE(textRejected);
		CUI_EXPECT_TRUE(formatRejected);
		CUI_EXPECT_TRUE(rawSetRejected);
		CUI_EXPECT_TRUE(rawClearRejected);
		CUI_EXPECT_TRUE(rawCoerceRejected);
		CUI_EXPECT_TRUE(scopeRejected);
		CUI_EXPECT_EQ(std::wstring(L"AXC"), document.Flatten().Text);
		CUI_EXPECT_EQ(std::wstring(L"C"), third.GetText());
		CUI_EXPECT_TRUE(third.GetUnderline());
		CUI_EXPECT_FALSE(third.HasPropertyValue(
			TextElement::FontWeightProperty(),
			DependencyPropertyValueSource::Local));

		auto* committedParagraph = document.Blocks.At(0);
		auto* committedFirst = &first;
		auto* committedThird = &third;
		const auto committed = document.Flatten();
		auto failing = committed;
		failing.Text[1] = L'Y';
		auto throwing = document.Changed.Subscribe(
			[](FlowDocument*)
			{
				throw std::runtime_error(
					"expected structured callback failure");
			});
		bool threw = false;
		try { (void)document.ReplaceFromFragment(failing); }
		catch (const std::runtime_error&) { threw = true; }
		CUI_EXPECT_TRUE(threw);
		CUI_EXPECT_FALSE(document.IsChanging());
		CUI_EXPECT_EQ(committed, document.Flatten());
		CUI_EXPECT_TRUE(document.Blocks.At(0) == committedParagraph);
		auto* restoredParagraph = dynamic_cast<Paragraph*>(
			document.Blocks.At(0));
		CUI_EXPECT_TRUE(restoredParagraph != nullptr);
		CUI_EXPECT_TRUE(restoredParagraph->Inlines.At(0) == committedFirst);
		CUI_EXPECT_TRUE(restoredParagraph->Inlines.At(2) == committedThird);
		CUI_EXPECT_TRUE(committedFirst->GetParent() == restoredParagraph);
		CUI_EXPECT_TRUE(committedThird->GetParent() == restoredParagraph);
		CUI_EXPECT_TRUE(committedFirst->GetFlowDocument() == &document);
		CUI_EXPECT_TRUE(committedThird->GetFlowDocument() == &document);
	});

	runner.Add(
		"FlowDocument owner projection precedes public Changed and follows rollback",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		auto& run = paragraph.Inlines.AddRun(L"old");
		auto throwing = document->Changed.Subscribe(
			[](FlowDocument*)
			{
				throw std::runtime_error(
					"expected pre-subscribed callback failure");
			});

		RichTextBox owner;
		owner.SetDocument(std::move(document));
		bool textThrew = false;
		try { run.SetText(L"new"); }
		catch (const std::runtime_error&) { textThrew = true; }
		CUI_EXPECT_TRUE(textThrew);
		CUI_EXPECT_EQ(std::wstring(L"new"), run.GetText());
		CUI_EXPECT_EQ(std::wstring(L"new"), owner.Text);

		bool styleThrew = false;
		try { run.SetFontWeight(DWRITE_FONT_WEIGHT_BOLD); }
		catch (const std::runtime_error&) { styleThrew = true; }
		CUI_EXPECT_TRUE(styleThrew);
		owner.SelectAll();
		ExpectSelectionWeight(owner,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_BOLD);

		bool collectionThrew = false;
		try { paragraph.Inlines.AddRun(L"!"); }
		catch (const std::runtime_error&) { collectionThrew = true; }
		CUI_EXPECT_TRUE(collectionThrew);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			paragraph.Inlines.Count()));
		CUI_EXPECT_EQ(std::wstring(L"new"), owner.Text);
		CUI_EXPECT_EQ(std::wstring(L"new"),
			owner.GetDocument().Flatten().Text);

		const auto original = owner.GetDocument().Flatten();
		auto replacement = original;
		replacement.Text = L"NEW";
		bool replaceThrew = false;
		try
		{
			(void)owner.GetDocument().ReplaceFromFragment(replacement);
		}
		catch (const std::runtime_error&) { replaceThrew = true; }
		CUI_EXPECT_TRUE(replaceThrew);
		CUI_EXPECT_EQ(original, owner.GetDocument().Flatten());
		CUI_EXPECT_TRUE(owner.GetDocument().Blocks.At(0) == &paragraph);
		CUI_EXPECT_TRUE(paragraph.Inlines.At(0) == &run);
		CUI_EXPECT_EQ(std::wstring(L"new"), owner.Text);
	});

	runner.Add(
		"FlowDocument owner transaction restores editor state and suppresses failed notifications",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		paragraph.Inlines.AddRun(L"old");
		RichTextBox owner;
		owner.SetDocument(std::move(document));
		owner.Select(3, 0);
		owner.InsertText(L"1");
		owner.InsertText(L"2");
		owner.Undo();
		CUI_EXPECT_EQ(std::wstring(L"old1"), owner.Text);
		CUI_EXPECT_TRUE(owner.CanUndo());
		CUI_EXPECT_TRUE(owner.CanRedo());
		owner.Select(4, 0);
		CUI_EXPECT_TRUE(owner.GetSelection().ApplyPropertyValue(
			TextElement::UnderlineProperty(), BindingValue(true)));
		ExpectSelectionUnderline(owner,
			TextSelectionPropertyValueKind::Value, true);
		owner.SetHighlightRanges({ RichTextBoxTextRange{ 0, 2 } });
		const int selectionStart = owner.GetSelectionStart();
		const int selectionLength = owner.GetSelectionLength();
		const auto before = owner.GetDocument().Flatten();
		const auto blockCount = owner.GetDocument().Blocks.Count();

		int textEvents = 0;
		int selectionEvents = 0;
		auto textChanged = owner.OnTextChanged.Subscribe(
			[&](Control*, TextChangedEventArgs&) { ++textEvents; });
		auto selectionChanged = owner.SelectionChanged.Subscribe(
			[&](Control*, SelectionChangedEventArgs&) { ++selectionEvents; });
		bool sawProjectedText = false;
		bool sawResetHistory = false;
		auto throwing = owner.GetDocument().Changed.Subscribe(
			[&](FlowDocument*)
			{
				sawProjectedText = owner.Text == L"old1\r\n";
				sawResetHistory = !owner.CanUndo() && !owner.CanRedo();
				throw std::runtime_error(
					"expected owner transaction rejection");
			});
		bool threw = false;
		try { owner.GetDocument().Blocks.AddParagraph(); }
		catch (const std::runtime_error&) { threw = true; }
		CUI_EXPECT_TRUE(threw);
		CUI_EXPECT_TRUE(sawProjectedText);
		CUI_EXPECT_TRUE(sawResetHistory);
		CUI_EXPECT_EQ(0, textEvents);
		CUI_EXPECT_EQ(0, selectionEvents);
		CUI_EXPECT_EQ(before, owner.GetDocument().Flatten());
		CUI_EXPECT_EQ(blockCount, owner.GetDocument().Blocks.Count());
		CUI_EXPECT_EQ(std::wstring(L"old1"), owner.Text);
		CUI_EXPECT_EQ(selectionStart, owner.GetSelectionStart());
		CUI_EXPECT_EQ(selectionLength, owner.GetSelectionLength());
		CUI_EXPECT_TRUE(owner.CanUndo());
		CUI_EXPECT_TRUE(owner.CanRedo());
		ExpectSelectionUnderline(owner,
			TextSelectionPropertyValueKind::Value, true);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			owner.GetHighlightRanges().size()));
		CUI_EXPECT_EQ(0, owner.GetHighlightRanges()[0].Start);
		CUI_EXPECT_EQ(2, owner.GetHighlightRanges()[0].Length);

		throwing = {};
		int exactChanged = 0;
		auto exactConnection = owner.GetDocument().Changed.Subscribe(
			[&](FlowDocument*) { ++exactChanged; });
		CUI_EXPECT_TRUE(owner.GetDocument().ReplaceFromFragment(
			owner.GetDocument().Flatten()));
		CUI_EXPECT_EQ(0, exactChanged);
		CUI_EXPECT_TRUE(owner.CanUndo());
		CUI_EXPECT_TRUE(owner.CanRedo());
		CUI_EXPECT_EQ(selectionStart, owner.GetSelectionStart());
		CUI_EXPECT_EQ(selectionLength, owner.GetSelectionLength());
		ExpectSelectionUnderline(owner,
			TextSelectionPropertyValueKind::Value, true);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			owner.GetHighlightRanges().size()));
	});

	runner.Add(
		"FlowDocument owner notification exceptions occur after document commit",
		[]
	{
		RichTextBox owner;
		owner.Text = L"A";
		owner.Select(1, 0);
		owner.InsertText(L"B");
		CUI_EXPECT_TRUE(owner.CanUndo());
		int documentEvents = 0;
		auto documentChanged = owner.GetDocument().Changed.Subscribe(
			[&](FlowDocument*) { ++documentEvents; });
		auto textChanged = owner.OnTextChanged.Subscribe(
			[](Control*, TextChangedEventArgs&)
			{
				throw std::runtime_error(
					"expected downstream owner notification failure");
			});

		bool threw = false;
		try { owner.GetDocument().Blocks.AddParagraph(); }
		catch (const std::runtime_error&) { threw = true; }
		CUI_EXPECT_TRUE(threw);
		CUI_EXPECT_EQ(1, documentEvents);
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			owner.GetDocument().Blocks.Count()));
		CUI_EXPECT_EQ(std::wstring(L"AB\r\n"),
			owner.GetDocument().Flatten().Text);
		CUI_EXPECT_EQ(std::wstring(L"AB\r\n"), owner.Text);
		CUI_EXPECT_FALSE(owner.CanUndo());
		CUI_EXPECT_FALSE(owner.CanRedo());
		CUI_EXPECT_FALSE(owner.GetDocument().IsChanging());
	});

	runner.Add(
		"FlowDocument structured roots and terminal CRLF retain their contracts",
		[]
	{
		FlowDocument source;
		auto& sourceParagraph = source.Blocks.AddParagraph();
		sourceParagraph.Inlines.AddRun(L"A");
		const auto foreign = source.Flatten();
		CUI_EXPECT_TRUE(foreign.StructureRootId.has_value());

		FlowDocument target;
		auto& targetParagraph = target.Blocks.AddParagraph();
		targetParagraph.Inlines.AddRun(L"B");
		auto* targetIdentity = target.Blocks.At(0);
		const auto targetBefore = target.Flatten();
		std::wstring error;
		CUI_EXPECT_FALSE(target.ReplaceFromFragment(foreign, &error));
		CUI_EXPECT_FALSE(error.empty());
		CUI_EXPECT_TRUE(target.Blocks.At(0) == targetIdentity);
		CUI_EXPECT_EQ(targetBefore, target.Flatten());

		auto foreignStyle = targetBefore;
		RichTextCharacterStyle rootStyle;
		rootStyle.FontSize = 18.0f;
		foreignStyle.RootStyle = rootStyle;
		for (auto& span : foreignStyle.Spans)
			span.Style.FontSize = 18.0f;
		CUI_EXPECT_TRUE(foreignStyle.ValidateCanonical());
		error.clear();
		CUI_EXPECT_FALSE(target.ReplaceFromFragment(foreignStyle, &error));
		CUI_EXPECT_FALSE(error.empty());
		CUI_EXPECT_TRUE(target.Blocks.At(0) == targetIdentity);
		CUI_EXPECT_EQ(targetBefore, target.Flatten());

		RichTextDocumentFragment foreignEmpty;
		foreignEmpty.RootStyle = foreign.RootStyle;
		foreignEmpty.StructureRootId = foreign.StructureRootId;
		CUI_EXPECT_FALSE(foreignEmpty.ValidateCanonical());
		error.clear();
		CUI_EXPECT_FALSE(target.ReplaceFromFragment(foreignEmpty, &error));
		CUI_EXPECT_FALSE(error.empty());
		CUI_EXPECT_TRUE(target.Blocks.At(0) == targetIdentity);
		CUI_EXPECT_EQ(targetBefore, target.Flatten());

		FlowDocument runSource;
		auto& runParagraph = runSource.Blocks.AddParagraph();
		runParagraph.Inlines.AddRun(L"A\r\n");
		const auto runFragment = runSource.Flatten();
		CUI_EXPECT_TRUE(runFragment.ValidateCanonical());
		CUI_EXPECT_EQ(RichTextStructureKind::Run,
			runFragment.StructureSpans.back().Path.back().Kind);
		auto runReplacement = runFragment;
		runReplacement.Text[0] = L'B';
		CUI_EXPECT_TRUE(runReplacement.ValidateCanonical());
		CUI_EXPECT_TRUE(runSource.ReplaceFromFragment(runReplacement));
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			runSource.Blocks.Count()));
		auto* rebuiltRunParagraph = dynamic_cast<Paragraph*>(
			runSource.Blocks.At(0));
		auto* rebuiltRun = rebuiltRunParagraph
			? dynamic_cast<Run*>(rebuiltRunParagraph->Inlines.At(0)) : nullptr;
		CUI_EXPECT_TRUE(rebuiltRun != nullptr);
		CUI_EXPECT_EQ(std::wstring(L"B\r\n"), rebuiltRun->GetText());

		FlowDocument lineBreakSource;
		auto& lineBreakParagraph = lineBreakSource.Blocks.AddParagraph();
		lineBreakParagraph.Inlines.AddRun(L"A");
		auto boldBreak = std::make_unique<Bold>();
		auto* boldBreakIdentity = boldBreak.get();
		auto& lineBreak = boldBreak->Inlines.AddLineBreak();
		auto* lineBreakIdentity = &lineBreak;
		lineBreakParagraph.Inlines.Add(std::move(boldBreak));
		lineBreakParagraph.Inlines.AddRun(L"B");
		const auto lineBreakFragment = lineBreakSource.Flatten();
		CUI_EXPECT_TRUE(lineBreakFragment.ValidateCanonical());
		CUI_EXPECT_EQ(std::wstring(L"A\r\nB"), lineBreakFragment.Text);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			lineBreakSource.Blocks.Count()));
		CUI_EXPECT_EQ(RichTextStructureKind::LineBreak,
			lineBreakFragment.StructureSpans[1].Path.back().Kind);
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			lineBreakFragment.StructureSpans[1].Path.size()));
		auto lineBreakReplacement = lineBreakFragment;
		lineBreakReplacement.Text[0] = L'C';
		CUI_EXPECT_TRUE(lineBreakReplacement.ValidateCanonical());
		CUI_EXPECT_TRUE(lineBreakSource.ReplaceFromFragment(
			lineBreakReplacement));
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			lineBreakSource.Blocks.Count()));
		auto* rebuiltLineBreakParagraph = dynamic_cast<Paragraph*>(
			lineBreakSource.Blocks.At(0));
		CUI_EXPECT_TRUE(rebuiltLineBreakParagraph != nullptr);
		if (rebuiltLineBreakParagraph)
		{
			CUI_EXPECT_TRUE(rebuiltLineBreakParagraph->Inlines.At(1)
				== boldBreakIdentity);
			auto* rebuiltBold = dynamic_cast<Bold*>(
				rebuiltLineBreakParagraph->Inlines.At(1));
			CUI_EXPECT_TRUE(rebuiltBold != nullptr);
			if (rebuiltBold)
				CUI_EXPECT_TRUE(rebuiltBold->Inlines.At(0)
					== lineBreakIdentity);
		}
		CUI_EXPECT_EQ(std::wstring(L"C\r\nB"),
			lineBreakSource.Flatten().Text);

		FlowDocument breakSource;
		auto& breakParagraph = breakSource.Blocks.AddParagraph();
		breakParagraph.Inlines.AddRun(L"A");
		breakSource.Blocks.AddParagraph();
		const auto breakFragment = breakSource.Flatten();
		CUI_EXPECT_TRUE(breakFragment.ValidateCanonical());
		CUI_EXPECT_EQ(RichTextStructureKind::ParagraphBreak,
			breakFragment.StructureSpans.back().Path.back().Kind);
		auto breakReplacement = breakFragment;
		breakReplacement.Text[0] = L'B';
		CUI_EXPECT_TRUE(breakReplacement.ValidateCanonical());
		CUI_EXPECT_TRUE(breakSource.ReplaceFromFragment(breakReplacement));
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			breakSource.Blocks.Count()));
	});

	runner.Add(
		"RichText TextRange tracks edits replaces paragraphs and preserves atomic boundaries",
		[]
	{
		FlowDocument document;
		auto& paragraph = document.Blocks.AddParagraph();
		paragraph.Inlines.AddRun(L"abcdef");
		TextRange range(
			document.CreateTextPointerAtTextOffset(
				4, LogicalDirection::Forward),
			document.CreateTextPointerAtTextOffset(
				1, LogicalDirection::Backward));
		CUI_EXPECT_FALSE(range.GetIsEmpty());
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			range.GetStart().GetTextOffset()));
		CUI_EXPECT_EQ(4ULL, static_cast<unsigned long long>(
			range.GetEnd().GetTextOffset()));
		CUI_EXPECT_EQ(std::wstring(L"bcd"), range.GetText());
		CUI_EXPECT_TRUE(range.Contains(
			document.CreateTextPointerAtTextOffset(1)));
		CUI_EXPECT_TRUE(range.Contains(
			document.CreateTextPointerAtTextOffset(4)));
		CUI_EXPECT_FALSE(range.Contains(
			document.CreateTextPointerAtTextOffset(0)));

		paragraph.Inlines.Insert(0, std::make_unique<Run>(L"X"));
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			range.GetStart().GetTextOffset()));
		CUI_EXPECT_EQ(5ULL, static_cast<unsigned long long>(
			range.GetEnd().GetTextOffset()));
		CUI_EXPECT_EQ(std::wstring(L"bcd"), range.GetText());

		int changedCount = 0;
		auto changed = document.Changed.Subscribe(
			[&](FlowDocument*) { ++changedCount; });
		range.SetText(L"Q\nR");
		CUI_EXPECT_EQ(1, changedCount);
		CUI_EXPECT_EQ(std::wstring(L"XaQ\r\nRef"),
			document.Flatten().Text);
		CUI_EXPECT_EQ(std::wstring(L"Q\r\nR"), range.GetText());
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			range.GetStart().GetTextOffset()));
		CUI_EXPECT_EQ(6ULL, static_cast<unsigned long long>(
			range.GetEnd().GetTextOffset()));
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			document.Blocks.Count()));

		CUI_EXPECT_TRUE(range.ApplyPropertyValue(
			TextElement::UnderlineProperty(), BindingValue(true)));
		auto underline = range.GetPropertyValue(
			TextElement::UnderlineProperty());
		CUI_EXPECT_EQ(TextRangePropertyValueKind::Value, underline.Kind);
		bool underlineValue = false;
		CUI_EXPECT_TRUE(underline.Value.TryGet(underlineValue));
		CUI_EXPECT_TRUE(underlineValue);
		range.ClearAllProperties();
		underline = range.GetPropertyValue(
			TextElement::UnderlineProperty());
		CUI_EXPECT_EQ(TextRangePropertyValueKind::Value, underline.Kind);
		CUI_EXPECT_TRUE(underline.Value.TryGet(underlineValue));
		CUI_EXPECT_FALSE(underlineValue);

		const auto beforeThrow = document.Flatten();
		const auto startBeforeThrow = range.GetStart().GetTextOffset();
		const auto endBeforeThrow = range.GetEnd().GetTextOffset();
		auto throwing = document.Changed.Subscribe(
			[](FlowDocument*)
			{
				throw std::runtime_error("expected TextRange callback failure");
			});
		bool threw = false;
		try { range.SetText(L"Z"); }
		catch (const std::runtime_error&) { threw = true; }
		CUI_EXPECT_TRUE(threw);
		CUI_EXPECT_EQ(beforeThrow, document.Flatten());
		CUI_EXPECT_EQ(startBeforeThrow, range.GetStart().GetTextOffset());
		CUI_EXPECT_EQ(endBeforeThrow, range.GetEnd().GetTextOffset());

		FlowDocument atomic;
		auto& atomicParagraph = atomic.Blocks.AddParagraph();
		atomicParagraph.Inlines.AddRun(
			L"A\r\n\xD83D\xDE00" L"B");
		TextRange atomicRange(
			atomic.CreateTextPointerAtTextOffset(
				2, LogicalDirection::Forward),
			atomic.CreateTextPointerAtTextOffset(
				4, LogicalDirection::Backward));
		CUI_EXPECT_EQ(std::wstring(L"\r\n\xD83D\xDE00"),
			atomicRange.GetText());
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			atomicRange.GetStart().GetTextOffset()));
		CUI_EXPECT_EQ(5ULL, static_cast<unsigned long long>(
			atomicRange.GetEnd().GetTextOffset()));
		const auto crlfBackward = atomic.CreateTextPointerAtTextOffset(
			2, LogicalDirection::Backward);
		const auto crlfForward = atomic.CreateTextPointerAtTextOffset(
			2, LogicalDirection::Forward);
		TextRange collapsedBackward(crlfBackward, crlfBackward);
		TextRange collapsedForward(crlfForward, crlfForward);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			collapsedBackward.GetStart().GetTextOffset()));
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			collapsedForward.GetStart().GetTextOffset()));

		FlowDocument foreign;
		foreign.Blocks.AddParagraph().Inlines.AddRun(L"foreign");
		bool foreignRejected = false;
		try
		{
			TextRange invalid(
				document.GetContentStart(), foreign.GetContentStart());
		}
		catch (const std::invalid_argument&)
		{
			foreignRejected = true;
		}
		CUI_EXPECT_TRUE(foreignRejected);
	});

	runner.Add(
		"RichText TextRange owned edits are undoable without hijacking selection",
		[]
	{
		RichTextBox box;
		box.Text = L"abcdef";
		box.Select(0, 0);
		TextRange range(
			box.GetDocument().CreateTextPointerAtTextOffset(
				2, LogicalDirection::Backward),
			box.GetDocument().CreateTextPointerAtTextOffset(
				4, LogicalDirection::Forward));
		range.SetText(L"Q");
		CUI_EXPECT_EQ(std::wstring(L"abQef"), box.Text);
		CUI_EXPECT_EQ(std::wstring(L"Q"), range.GetText());
		CUI_EXPECT_EQ(0, box.GetSelectionStart());
		CUI_EXPECT_EQ(0, box.GetSelectionLength());
		CUI_EXPECT_TRUE(box.CanUndo());
		box.Undo();
		CUI_EXPECT_EQ(std::wstring(L"abcdef"), box.Text);
		CUI_EXPECT_EQ(std::wstring(L"cd"), range.GetText());
		CUI_EXPECT_EQ(0, box.GetSelectionStart());
		box.Redo();
		CUI_EXPECT_EQ(std::wstring(L"abQef"), box.Text);
		CUI_EXPECT_EQ(std::wstring(L"Q"), range.GetText());

		CUI_EXPECT_TRUE(range.ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_BOLD)));
		CUI_EXPECT_TRUE(box.CanUndo());
		auto weight = range.GetPropertyValue(
			TextElement::FontWeightProperty());
		CUI_EXPECT_EQ(TextRangePropertyValueKind::Value, weight.Kind);
		DWRITE_FONT_WEIGHT weightValue = DWRITE_FONT_WEIGHT_NORMAL;
		CUI_EXPECT_TRUE(weight.Value.TryGet(weightValue));
		CUI_EXPECT_EQ(DWRITE_FONT_WEIGHT_BOLD, weightValue);
		box.Undo();
		weight = range.GetPropertyValue(
			TextElement::FontWeightProperty());
		CUI_EXPECT_TRUE(weight.Value.TryGet(weightValue));
		CUI_EXPECT_EQ(DWRITE_FONT_WEIGHT_NORMAL, weightValue);

		RichTextBox selectionBox;
		selectionBox.Text = L"abcdef";
		selectionBox.Select(2, 2);
		TextRange& selectionRange = selectionBox.GetSelection();
		selectionRange.SetText(L"XY");
		CUI_EXPECT_EQ(std::wstring(L"abXYef"), selectionBox.Text);
		CUI_EXPECT_EQ(2, selectionBox.GetSelectionStart());
		CUI_EXPECT_EQ(2, selectionBox.GetSelectionLength());
		CUI_EXPECT_EQ(std::wstring(L"XY"),
			selectionBox.GetSelection().GetText());
		selectionBox.Undo();
		CUI_EXPECT_EQ(std::wstring(L"abcdef"), selectionBox.Text);
		CUI_EXPECT_EQ(2, selectionBox.GetSelectionStart());
		CUI_EXPECT_EQ(2, selectionBox.GetSelectionLength());
		selectionBox.Redo();
		CUI_EXPECT_EQ(std::wstring(L"XY"),
			selectionBox.GetSelection().GetText());

		selectionBox.IsReadOnly = true;
		selectionBox.Select(2, 2);
		selectionBox.GetSelection().SetText(L"Z");
		CUI_EXPECT_EQ(std::wstring(L"abZef"), selectionBox.Text);
		CUI_EXPECT_EQ(std::wstring(L"Z"),
			selectionBox.GetSelection().GetText());
		CUI_EXPECT_TRUE(selectionBox.GetSelection().ApplyPropertyValue(
			TextElement::UnderlineProperty(), BindingValue(true)));

		RichTextBox rollback;
		rollback.Text = L"old";
		rollback.Select(0, 0);
		TextRange rollbackRange(
			rollback.GetDocument().CreateTextPointerAtTextOffset(1),
			rollback.GetDocument().CreateTextPointerAtTextOffset(2));
		auto throwing = rollback.GetDocument().Changed.Subscribe(
			[](FlowDocument*)
			{
				throw std::runtime_error("expected owner range failure");
			});
		bool threw = false;
		try { rollbackRange.SetText(L"X"); }
		catch (const std::runtime_error&) { threw = true; }
		CUI_EXPECT_TRUE(threw);
		CUI_EXPECT_EQ(std::wstring(L"old"), rollback.Text);
		CUI_EXPECT_EQ(std::wstring(L"l"), rollbackRange.GetText());
		CUI_EXPECT_FALSE(rollback.CanUndo());
		CUI_EXPECT_FALSE(rollback.CanRedo());
		CUI_EXPECT_EQ(0, rollback.GetSelectionStart());
	});

	runner.Add(
		"RichText TextRange effective queries preserve implicit and explicit inheritance",
		[]
	{
		RichTextBox implicit;
		implicit.SetFontSize(30.0);
		implicit.Text = L"abc";
		TextRange implicitRange(
			implicit.GetDocument().GetContentStart(),
			implicit.GetDocument().GetContentEnd());
		auto size = implicitRange.GetPropertyValue(
			TextElement::FontSizeProperty());
		CUI_EXPECT_EQ(TextRangePropertyValueKind::Value, size.Kind);
		float sizeValue = 0.0f;
		CUI_EXPECT_TRUE(size.Value.TryGet(sizeValue));
		CUI_EXPECT_EQ(30.0f, sizeValue);
		CUI_EXPECT_TRUE(implicitRange.ApplyPropertyValue(
			TextElement::FontSizeProperty(), BindingValue(30.0)));
		CUI_EXPECT_FALSE(implicit.CanUndo());
		implicit.SetFontSize(40.0);
		size = implicitRange.GetPropertyValue(
			TextElement::FontSizeProperty());
		CUI_EXPECT_TRUE(size.Value.TryGet(sizeValue));
		CUI_EXPECT_EQ(40.0f, sizeValue);

		TextRange middle(
			implicit.GetDocument().CreateTextPointerAtTextOffset(1),
			implicit.GetDocument().CreateTextPointerAtTextOffset(2));
		CUI_EXPECT_TRUE(middle.ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_BOLD)));
		const auto mixed = implicitRange.GetPropertyValue(
			TextElement::FontWeightProperty());
		CUI_EXPECT_EQ(TextRangePropertyValueKind::Mixed, mixed.Kind);

		auto explicitDocument = std::make_unique<FlowDocument>();
		explicitDocument->FontSize = 18.0;
		explicitDocument->SetFlowDirection(
			::FlowDirection::RightToLeft);
		explicitDocument->Blocks.AddParagraph().Inlines.AddRun(L"plain");
		RichTextBox explicitBox;
		explicitBox.SetFontSize(60.0);
		explicitBox.SetDocument(std::move(explicitDocument));
		TextRange explicitRange(
			explicitBox.GetDocument().GetContentStart(),
			explicitBox.GetDocument().GetContentEnd());
		size = explicitRange.GetPropertyValue(
			TextElement::FontSizeProperty());
		CUI_EXPECT_TRUE(size.Value.TryGet(sizeValue));
		CUI_EXPECT_EQ(18.0f, sizeValue);
		auto direction = explicitRange.GetPropertyValue(
			Block::FlowDirectionProperty());
		CUI_EXPECT_EQ(TextRangePropertyValueKind::Value, direction.Kind);
		::FlowDirection directionValue = ::FlowDirection::LeftToRight;
		CUI_EXPECT_TRUE(direction.Value.TryGet(directionValue));
		CUI_EXPECT_EQ(::FlowDirection::RightToLeft, directionValue);
		CUI_EXPECT_TRUE(explicitRange.ApplyPropertyValue(
			Block::FlowDirectionProperty(),
			BindingValue(::FlowDirection::RightToLeft)));
		CUI_EXPECT_FALSE(explicitBox.CanUndo());
		CUI_EXPECT_TRUE(explicitRange.ApplyPropertyValue(
			TextElement::FontSizeProperty(), BindingValue(18.0)));
		CUI_EXPECT_FALSE(explicitBox.CanUndo());
		explicitBox.SetFontSize(72.0);
		size = explicitRange.GetPropertyValue(
			TextElement::FontSizeProperty());
		CUI_EXPECT_TRUE(size.Value.TryGet(sizeValue));
		CUI_EXPECT_EQ(18.0f, sizeValue);
		explicitBox.GetDocument().FontSize = 20.0;
		explicitBox.GetDocument().SetFlowDirection(
			::FlowDirection::LeftToRight);
		size = explicitRange.GetPropertyValue(
			TextElement::FontSizeProperty());
		CUI_EXPECT_TRUE(size.Value.TryGet(sizeValue));
		CUI_EXPECT_EQ(20.0f, sizeValue);
		direction = explicitRange.GetPropertyValue(
			Block::FlowDirectionProperty());
		CUI_EXPECT_TRUE(direction.Value.TryGet(directionValue));
		CUI_EXPECT_EQ(::FlowDirection::LeftToRight, directionValue);
	});

	runner.Add(
		"RichText TextPointer symbol navigation preserves empty element edges and projection gaps",
		[]
	{
		RichTextBox box;
		auto document = std::make_unique<FlowDocument>();
		auto& first = document->Blocks.AddParagraph();
		auto& left = first.Inlines.AddRun(L"A");
		auto emptySpan = std::make_unique<Span>();
		auto* emptySpanIdentity = emptySpan.get();
		auto emptyItalic = std::make_unique<Italic>();
		auto* emptyItalicIdentity = emptyItalic.get();
		emptySpan->Inlines.Add(std::move(emptyItalic));
		auto& emptyRun = emptySpan->Inlines.AddRun();
		first.Inlines.Add(std::move(emptySpan));
		first.Inlines.AddRun(L"B");
		auto& second = document->Blocks.AddParagraph();
		auto emptyBold = std::make_unique<Bold>();
		auto* emptyBoldIdentity = emptyBold.get();
		second.Inlines.Add(std::move(emptyBold));
		box.SetDocument(std::move(document));
		auto& live = box.GetDocument();

		CUI_EXPECT_EQ(std::wstring(L"AB\r\n"), box.Text);
		CUI_EXPECT_EQ(18ULL, static_cast<unsigned long long>(
			live.GetSymbolCount()));
		const auto documentStart = live.GetContentStart();
		const auto documentEnd = live.GetContentEnd();
		CUI_EXPECT_EQ(0ULL, static_cast<unsigned long long>(
			documentStart.GetSymbolOffset()));
		CUI_EXPECT_EQ(18ULL, static_cast<unsigned long long>(
			documentEnd.GetSymbolOffset()));
		CUI_EXPECT_EQ(TextPointerContext::ElementStart,
			documentStart.GetPointerContext(LogicalDirection::Forward));
		CUI_EXPECT_TRUE(documentStart.GetAdjacentElement(
			LogicalDirection::Forward) == &first);
		CUI_EXPECT_EQ(TextPointerContext::None,
			documentStart.GetPointerContext(LogicalDirection::Backward));

		const auto italicElementStart =
			emptyItalicIdentity->GetElementStart();
		const auto italicContentStart =
			emptyItalicIdentity->GetContentStart();
		const auto italicContentEnd =
			emptyItalicIdentity->GetContentEnd();
		const auto italicElementEnd =
			emptyItalicIdentity->GetElementEnd();
		CUI_EXPECT_EQ(5ULL, static_cast<unsigned long long>(
			italicElementStart.GetSymbolOffset()));
		CUI_EXPECT_EQ(6ULL, static_cast<unsigned long long>(
			italicContentStart.GetSymbolOffset()));
		CUI_EXPECT_EQ(6ULL, static_cast<unsigned long long>(
			italicContentEnd.GetSymbolOffset()));
		CUI_EXPECT_EQ(7ULL, static_cast<unsigned long long>(
			italicElementEnd.GetSymbolOffset()));
		CUI_EXPECT_EQ(0, italicContentStart.CompareSymbolPositionTo(
			italicContentEnd));
		CUI_EXPECT_EQ(2, italicElementStart.GetSymbolOffsetToPosition(
			italicElementEnd));
		CUI_EXPECT_EQ(0, italicElementStart.GetTextOffsetToPosition(
			italicElementEnd));
		CUI_EXPECT_EQ(TextPointerContext::ElementStart,
			italicContentStart.GetPointerContext(
				LogicalDirection::Backward));
		CUI_EXPECT_EQ(TextPointerContext::ElementEnd,
			italicContentStart.GetPointerContext(
				LogicalDirection::Forward));
		CUI_EXPECT_TRUE(italicContentStart.GetAdjacentElement(
			LogicalDirection::Backward) == emptyItalicIdentity);
		CUI_EXPECT_TRUE(italicContentStart.GetAdjacentElement(
			LogicalDirection::Forward) == emptyItalicIdentity);

		const auto spanContentStart = emptySpanIdentity->GetContentStart();
		const auto spanContentEnd = emptySpanIdentity->GetContentEnd();
		CUI_EXPECT_EQ(4, spanContentStart.GetSymbolOffsetToPosition(
			spanContentEnd));
		CUI_EXPECT_EQ(0, spanContentStart.GetTextOffsetToPosition(
			spanContentEnd));
		CUI_EXPECT_EQ(8ULL, static_cast<unsigned long long>(
			emptyRun.GetContentStart().GetSymbolOffset()));
		CUI_EXPECT_EQ(8ULL, static_cast<unsigned long long>(
			emptyRun.GetContentEnd().GetSymbolOffset()));
		CUI_EXPECT_EQ(16ULL, static_cast<unsigned long long>(
			emptyBoldIdentity->GetContentStart().GetSymbolOffset()));

		const auto leftContent = left.GetContentStart();
		CUI_EXPECT_EQ(TextPointerContext::Text,
			leftContent.GetPointerContext(LogicalDirection::Forward));
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			leftContent.GetTextRunLength(LogicalDirection::Forward)));
		CUI_EXPECT_EQ(std::wstring(L"A"),
			leftContent.GetTextInRun(LogicalDirection::Forward));
		const auto afterLeftText = leftContent.GetNextContextPosition(
			LogicalDirection::Forward);
		CUI_EXPECT_TRUE(afterLeftText.has_value());
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			afterLeftText ? afterLeftText->GetSymbolOffset() : 0));
		CUI_EXPECT_EQ(std::wstring(L"A"),
			afterLeftText
				? afterLeftText->GetTextInRun(LogicalDirection::Backward)
				: std::wstring{});

		const auto firstContentEnd = first.GetContentEnd();
		const auto firstElementEnd = first.GetElementEnd();
		CUI_EXPECT_EQ(13ULL, static_cast<unsigned long long>(
			firstContentEnd.GetSymbolOffset()));
		CUI_EXPECT_EQ(14ULL, static_cast<unsigned long long>(
			firstElementEnd.GetSymbolOffset()));
		CUI_EXPECT_EQ(1, firstContentEnd.GetSymbolOffsetToPosition(
			firstElementEnd));
		CUI_EXPECT_EQ(2, firstContentEnd.GetTextOffsetToPosition(
			firstElementEnd));
		CUI_EXPECT_EQ(TextPointerContext::ElementEnd,
			firstContentEnd.GetPointerContext(LogicalDirection::Forward));
		CUI_EXPECT_EQ(TextPointerContext::ElementStart,
			firstElementEnd.GetPointerContext(LogicalDirection::Forward));
		CUI_EXPECT_TRUE(firstElementEnd.GetAdjacentElement(
			LogicalDirection::Forward) == &second);

		auto projectedBackward = live.CreateTextPointerAtTextOffset(
			1, LogicalDirection::Backward);
		auto projectedForward = live.CreateTextPointerAtTextOffset(
			1, LogicalDirection::Forward);
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			projectedBackward.GetSymbolOffset()));
		CUI_EXPECT_EQ(11ULL, static_cast<unsigned long long>(
			projectedForward.GetSymbolOffset()));
		CUI_EXPECT_EQ(-1, projectedBackward.CompareTo(projectedForward));
		CUI_EXPECT_EQ(-1,
			projectedBackward.CompareSymbolPositionTo(projectedForward));

		FlowDocument hardBreakDocument;
		auto& hardBreakParagraph =
			hardBreakDocument.Blocks.AddParagraph();
		auto& hardBreakRun =
			hardBreakParagraph.Inlines.AddRun(L"A\r\nB");
		FlowDocument lineBreakDocument;
		auto& lineBreakParagraph =
			lineBreakDocument.Blocks.AddParagraph();
		lineBreakParagraph.Inlines.AddRun(L"A");
		auto& lineBreak = lineBreakParagraph.Inlines.AddLineBreak();
		lineBreakParagraph.Inlines.AddRun(L"B");
		CUI_EXPECT_EQ(hardBreakDocument.Flatten().Text,
			lineBreakDocument.Flatten().Text);
		CUI_EXPECT_EQ(8ULL, static_cast<unsigned long long>(
			hardBreakDocument.GetSymbolCount()));
		CUI_EXPECT_EQ(10ULL, static_cast<unsigned long long>(
			lineBreakDocument.GetSymbolCount()));
		CUI_EXPECT_EQ(4ULL, static_cast<unsigned long long>(
			hardBreakRun.GetContentStart().GetTextRunLength(
				LogicalDirection::Forward)));
		CUI_EXPECT_EQ(std::wstring(L"A\r\nB"),
			hardBreakRun.GetContentStart().GetTextInRun(
				LogicalDirection::Forward));
		CUI_EXPECT_EQ(0, lineBreak.GetContentStart()
			.GetSymbolOffsetToPosition(lineBreak.GetContentEnd()));
		CUI_EXPECT_EQ(2, lineBreak.GetContentStart()
			.GetTextOffsetToPosition(lineBreak.GetElementEnd()));
		CUI_EXPECT_EQ(TextPointerContext::ElementEnd,
			lineBreak.GetContentStart().GetPointerContext(
				LogicalDirection::Forward));

		auto liveEmptyEdge = emptyItalicIdentity->GetContentStart();
		box.Select(0, 0);
		box.InsertText(L"Z");
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			liveEmptyEdge.GetTextOffset()));
		CUI_EXPECT_EQ(7ULL, static_cast<unsigned long long>(
			liveEmptyEdge.GetSymbolOffset()));
		CUI_EXPECT_EQ(TextPointerContext::ElementEnd,
			liveEmptyEdge.GetPointerContext(LogicalDirection::Forward));
		box.Undo();
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			liveEmptyEdge.GetTextOffset()));
		CUI_EXPECT_EQ(6ULL, static_cast<unsigned long long>(
			liveEmptyEdge.GetSymbolOffset()));
		box.Redo();
		CUI_EXPECT_EQ(7ULL, static_cast<unsigned long long>(
			liveEmptyEdge.GetSymbolOffset()));

		std::unique_ptr<Inline> insertedEmpty =
			std::make_unique<Span>();
		auto* insertedEmptyIdentity = insertedEmpty.get();
		first.Inlines.Insert(1, std::move(insertedEmpty));
		CUI_EXPECT_EQ(9ULL, static_cast<unsigned long long>(
			liveEmptyEdge.GetSymbolOffset()));
		insertedEmpty = first.Inlines.Remove(*insertedEmptyIdentity);
		CUI_EXPECT_TRUE(insertedEmpty != nullptr);
		CUI_EXPECT_EQ(7ULL, static_cast<unsigned long long>(
			liveEmptyEdge.GetSymbolOffset()));

		bool rollbackThrew = false;
		{
			auto throwing = live.Changed.Subscribe(
				[](FlowDocument*)
				{
					throw std::runtime_error(
						"expected symbol-anchor rollback");
				});
			try { first.Inlines.Insert(1, std::move(insertedEmpty)); }
			catch (const std::runtime_error&) { rollbackThrew = true; }
		}
		CUI_EXPECT_TRUE(rollbackThrew);
		CUI_EXPECT_TRUE(insertedEmpty != nullptr);
		CUI_EXPECT_EQ(7ULL, static_cast<unsigned long long>(
			liveEmptyEdge.GetSymbolOffset()));
		CUI_EXPECT_EQ(TextPointerContext::ElementEnd,
			liveEmptyEdge.GetPointerContext(LogicalDirection::Forward));
	});

	runner.Add(
		"TextPointer live anchors honor gravity atomic boundaries and rollback",
		[]
	{
		RichTextBox box;
		box.Text = L"AB";
		auto backward = box.GetDocument().CreateTextPointerAtTextOffset(
			1, LogicalDirection::Backward);
		auto forward = box.GetDocument().CreateTextPointerAtTextOffset(
			1, LogicalDirection::Forward);
		int observedForward = -1;
		auto changed = box.GetDocument().Changed.Subscribe(
			[&](FlowDocument*)
			{
				observedForward = static_cast<int>(
					forward.GetTextOffset());
			});
		box.Select(1, 0);
		box.InsertText(L"X");
		CUI_EXPECT_EQ(std::wstring(L"AXB"), box.Text);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			backward.GetTextOffset()));
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			forward.GetTextOffset()));

		FlowDocument directRun;
		auto& directParagraph = directRun.Blocks.AddParagraph();
		auto& directText = directParagraph.Inlines.AddRun(L"aaaa");
		auto directBackward = directRun.CreateTextPointerAtTextOffset(
			1, LogicalDirection::Backward);
		auto directForward = directRun.CreateTextPointerAtTextOffset(
			1, LogicalDirection::Forward);
		directRun.BeginChange();
		directText.SetText(L"aaaaa");
		CUI_EXPECT_EQ(0ULL, static_cast<unsigned long long>(
			directBackward.GetTextOffset()));
		CUI_EXPECT_EQ(5ULL, static_cast<unsigned long long>(
			directForward.GetTextOffset()));
		directRun.EndChange();
		CUI_EXPECT_EQ(std::wstring(L"aaaaa"), directRun.Flatten().Text);

		FlowDocument repeated;
		auto& repeatedParagraph = repeated.Blocks.AddParagraph();
		repeatedParagraph.Inlines.AddRun(L"aaaa");
		auto repeatedPointer = repeated.CreateTextPointerAtTextOffset(
			1, LogicalDirection::Forward);
		repeatedParagraph.Inlines.Insert(
			0, std::make_unique<Run>(L"a"));
		CUI_EXPECT_EQ(std::wstring(L"aaaaa"), repeated.Flatten().Text);
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			repeatedPointer.GetTextOffset()));
		CUI_EXPECT_EQ(2, observedForward);
		CUI_EXPECT_EQ(-1, backward.CompareTo(forward));
		CUI_EXPECT_EQ(1, backward.GetTextOffsetToPosition(forward));

		box.Undo();
		CUI_EXPECT_EQ(std::wstring(L"AB"), box.Text);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			backward.GetTextOffset()));
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			forward.GetTextOffset()));
		box.Redo();
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			backward.GetTextOffset()));
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			forward.GetTextOffset()));

		FlowDocument atomic;
		auto& atomicParagraph = atomic.Blocks.AddParagraph();
		atomicParagraph.Inlines.AddRun(
			L"A\r\n\xD83D\xDE00" L"B");
		auto crlfInterior = atomic.CreateTextPointerAtTextOffset(
			2, LogicalDirection::Forward);
		CUI_EXPECT_FALSE(crlfInterior.IsAtInsertionPosition());
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			crlfInterior.GetInsertionPosition(
				LogicalDirection::Backward).GetTextOffset()));
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			crlfInterior.GetInsertionPosition(
				LogicalDirection::Forward).GetTextOffset()));
		auto surrogateInterior = atomic.CreateTextPointerAtTextOffset(
			4, LogicalDirection::Backward);
		CUI_EXPECT_FALSE(surrogateInterior.IsAtInsertionPosition());
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			surrogateInterior.GetInsertionPosition(
				LogicalDirection::Backward).GetTextOffset()));
		CUI_EXPECT_EQ(5ULL, static_cast<unsigned long long>(
			surrogateInterior.GetInsertionPosition(
				LogicalDirection::Forward).GetTextOffset()));
		auto afterCrLf = atomic.CreateTextPointerAtTextOffset(
			1, LogicalDirection::Forward)
			.GetNextInsertionPosition(LogicalDirection::Forward);
		CUI_EXPECT_TRUE(afterCrLf.has_value());
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			afterCrLf ? afterCrLf->GetTextOffset() : 0));

		FlowDocument rollback;
		auto& rollbackParagraph = rollback.Blocks.AddParagraph();
		rollbackParagraph.Inlines.AddRun(L"A");
		auto retained = rollback.CreateTextPointerAtTextOffset(
			1, LogicalDirection::Forward);
		TextPointer callbackPointer;
		std::size_t callbackOffset = 0;
		auto throwing = rollback.Changed.Subscribe(
			[&](FlowDocument* document)
			{
				callbackOffset = retained.GetTextOffset();
				callbackPointer = document->GetContentEnd();
				throw std::runtime_error("expected pointer rollback");
			});
		bool threw = false;
		try { rollbackParagraph.Inlines.AddRun(L"B"); }
		catch (const std::runtime_error&) { threw = true; }
		CUI_EXPECT_TRUE(threw);
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(callbackOffset));
		CUI_EXPECT_EQ(std::wstring(L"A"), rollback.Flatten().Text);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			retained.GetTextOffset()));
		CUI_EXPECT_TRUE(callbackPointer.IsValid());
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			callbackPointer.GetTextOffset()));

		bool crossDocumentRejected = false;
		try { (void)retained.CompareTo(atomic.GetContentStart()); }
		catch (const std::invalid_argument&)
		{
			crossDocumentRejected = true;
		}
		CUI_EXPECT_TRUE(crossDocumentRejected);

		TextPointer expired;
		{
			auto document = std::make_unique<FlowDocument>();
			expired = document->GetContentStart();
			CUI_EXPECT_TRUE(expired.IsValid());
		}
		CUI_EXPECT_FALSE(expired.IsValid());
		bool invalidRejected = false;
		try { (void)expired.GetTextOffset(); }
		catch (const std::logic_error&) { invalidRejected = true; }
		CUI_EXPECT_TRUE(invalidRejected);
	});

	runner.Add(
		"TextSelection pointer endpoints preserve direction and rebase externally",
		[]
	{
		RichTextBox box;
		box.Text = L"abcd";
		auto anchor = box.GetDocument().CreateTextPointerAtTextOffset(
			4, LogicalDirection::Forward);
		auto moving = box.GetDocument().CreateTextPointerAtTextOffset(
			1, LogicalDirection::Backward);
		box.GetSelection().Select(anchor, moving);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			box.GetSelection().GetStart().GetTextOffset()));
		CUI_EXPECT_EQ(4ULL, static_cast<unsigned long long>(
			box.GetSelection().GetEnd().GetTextOffset()));
		CUI_EXPECT_EQ(4ULL, static_cast<unsigned long long>(
			box.GetSelection().GetAnchorPosition().GetTextOffset()));
		CUI_EXPECT_EQ(LogicalDirection::Forward,
			box.GetSelection().GetAnchorPosition().GetLogicalDirection());
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			box.GetSelection().GetMovingPosition().GetTextOffset()));
		CUI_EXPECT_EQ(LogicalDirection::Backward,
			box.GetSelection().GetMovingPosition().GetLogicalDirection());
		CUI_EXPECT_EQ(std::wstring(L"bcd"),
			box.GetSelection().GetText());

		auto* paragraph = dynamic_cast<Paragraph*>(
			box.GetDocument().Blocks.At(0));
		CUI_EXPECT_TRUE(paragraph != nullptr);
		if (paragraph)
			paragraph->Inlines.Insert(0, std::make_unique<Run>(L"X"));
		CUI_EXPECT_EQ(std::wstring(L"Xabcd"), box.Text);
		CUI_EXPECT_EQ(2, box.GetSelectionStart());
		CUI_EXPECT_EQ(3, box.GetSelectionLength());
		CUI_EXPECT_EQ(5ULL, static_cast<unsigned long long>(
			box.GetSelection().GetAnchorPosition().GetTextOffset()));
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			box.GetSelection().GetMovingPosition().GetTextOffset()));
		CUI_EXPECT_EQ(std::wstring(L"bcd"),
			box.GetSelection().GetText());

		RichTextBox foreign;
		bool foreignRejected = false;
		try
		{
			box.GetSelection().Select(
				foreign.GetDocument().GetContentStart(), moving);
		}
		catch (const std::invalid_argument&)
		{
			foreignRejected = true;
		}
		CUI_EXPECT_TRUE(foreignRejected);

		RichTextBox undoDirection;
		undoDirection.Text = L"AB";
		auto backwardCaret =
			undoDirection.GetDocument().CreateTextPointerAtTextOffset(
				1, LogicalDirection::Backward);
		undoDirection.GetSelection().Select(
			backwardCaret, backwardCaret);
		undoDirection.InsertText(L"X");
		CUI_EXPECT_EQ(LogicalDirection::Forward,
			undoDirection.GetSelection().GetMovingPosition()
				.GetLogicalDirection());
		undoDirection.Undo();
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			undoDirection.GetSelection().GetMovingPosition().GetTextOffset()));
		CUI_EXPECT_EQ(LogicalDirection::Backward,
			undoDirection.GetSelection().GetMovingPosition()
				.GetLogicalDirection());
		undoDirection.Redo();
		CUI_EXPECT_EQ(LogicalDirection::Forward,
			undoDirection.GetSelection().GetMovingPosition()
				.GetLogicalDirection());
	});

	runner.Add(
		"RichTextBox document clear formatting records the projected inherited result",
		[]
	{
		RichTextBox box;
		box.GetDocument().SetFontWeight(DWRITE_FONT_WEIGHT_BOLD);
		box.Text = L"abc";
		box.SelectAll();
		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_NORMAL)));
		ExpectWeight(Flatten(box), 1, DWRITE_FONT_WEIGHT_NORMAL);

		box.GetSelection().ClearAllProperties();
		ExpectWeight(Flatten(box), 1, DWRITE_FONT_WEIGHT_BOLD);
		CUI_EXPECT_TRUE(box.CanUndo());
		box.Undo();
		ExpectWeight(Flatten(box), 1, DWRITE_FONT_WEIGHT_NORMAL);
		box.Redo();
		ExpectWeight(Flatten(box), 1, DWRITE_FONT_WEIGHT_BOLD);
	});

	runner.Add(
		"RichTextBox paragraph alignment inherits queries edits and undoes",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		document->SetTextAlignment(::TextAlignment::Center);
		auto& first = document->Blocks.AddParagraph();
		first.Inlines.AddRun(L"A");
		auto& second = document->Blocks.AddParagraph();
		second.SetTextAlignment(::TextAlignment::Right);
		second.Inlines.AddRun(L"B");
		auto* firstIdentity = &first;

		RichTextBox box;
		box.SetDocument(std::move(document));
		CUI_EXPECT_EQ(::TextAlignment::Center,
			box.GetDocument().GetTextAlignment());
		CUI_EXPECT_EQ(::TextAlignment::Center, first.GetTextAlignment());
		CUI_EXPECT_EQ(::TextAlignment::Right, second.GetTextAlignment());
		box.SelectAll();
		ExpectSelectionAlignment(
			box, TextSelectionPropertyValueKind::Mixed);
		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			Block::TextAlignmentProperty(),
			BindingValue(::TextAlignment::Center)));
		CUI_EXPECT_TRUE(box.GetDocument().Blocks.At(0) == firstIdentity);
		ExpectSelectionAlignment(box,
			TextSelectionPropertyValueKind::Value,
			::TextAlignment::Center);
		CUI_EXPECT_TRUE(box.CanUndo());
		box.Undo();
		ExpectSelectionAlignment(
			box, TextSelectionPropertyValueKind::Mixed);
		CUI_EXPECT_TRUE(box.CanRedo());
		box.Redo();
		ExpectSelectionAlignment(box,
			TextSelectionPropertyValueKind::Value,
			::TextAlignment::Center);

		auto inheritedDocument = std::make_unique<FlowDocument>();
		inheritedDocument->SetTextAlignment(::TextAlignment::Center);
		auto& inheritedParagraph =
			inheritedDocument->Blocks.AddParagraph();
		inheritedParagraph.Inlines.AddRun(L"same");
		RichTextBox inherited;
		inherited.SetDocument(std::move(inheritedDocument));
		inherited.Select(2, 0);
		CUI_EXPECT_TRUE(inherited.GetSelection().ApplyPropertyValue(
			Block::TextAlignmentProperty(),
			BindingValue(::TextAlignment::Center)));
		CUI_EXPECT_FALSE(inherited.CanUndo());
		CUI_EXPECT_FALSE(inheritedParagraph.HasPropertyValue(
			Block::TextAlignmentProperty(),
			DependencyPropertyValueSource::Local));
		inherited.GetDocument().SetTextAlignment(::TextAlignment::Right);
		CUI_EXPECT_EQ(::TextAlignment::Right,
			inheritedParagraph.GetTextAlignment());
	});

	runner.Add(
		"RichTextBox empty paragraph alignment is undoable and ClearAll keeps it",
		[]
	{
		RichTextBox empty;
		empty.Select(0, 0);
		CUI_EXPECT_TRUE(empty.GetSelection().ApplyPropertyValue(
			Block::TextAlignmentProperty(),
			BindingValue(::TextAlignment::Center)));
		auto* centered = dynamic_cast<Paragraph*>(
			empty.GetDocument().Blocks.At(0));
		CUI_EXPECT_TRUE(centered != nullptr);
		CUI_EXPECT_TRUE(centered
			&& centered->GetTextAlignment() == ::TextAlignment::Center);
		CUI_EXPECT_TRUE(empty.CanUndo());
		empty.Undo();
		auto* left = dynamic_cast<Paragraph*>(
			empty.GetDocument().Blocks.At(0));
		CUI_EXPECT_TRUE(left
			&& left->GetTextAlignment() == ::TextAlignment::Left);
		empty.Redo();
		auto* redone = dynamic_cast<Paragraph*>(
			empty.GetDocument().Blocks.At(0));
		CUI_EXPECT_TRUE(redone
			&& redone->GetTextAlignment() == ::TextAlignment::Center);

		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		paragraph.SetTextAlignment(::TextAlignment::Right);
		auto bold = std::make_unique<Bold>();
		bold->Inlines.AddRun(L"formatted");
		paragraph.Inlines.Add(std::move(bold));
		RichTextBox clear;
		clear.SetDocument(std::move(document));
		clear.SelectAll();
		clear.GetSelection().ClearAllProperties();
		CUI_EXPECT_EQ(::TextAlignment::Right,
			dynamic_cast<Paragraph*>(
				clear.GetDocument().Blocks.At(0))->GetTextAlignment());
	});

	runner.Add(
		"RichTextBox paragraph flow direction drives queries layout navigation and undo",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		document->SetFlowDirection(::FlowDirection::RightToLeft);
		auto& first = document->Blocks.AddParagraph();
		first.Inlines.AddRun(L"ABCD");
		auto& second = document->Blocks.AddParagraph();
		second.SetFlowDirection(::FlowDirection::LeftToRight);
		second.Inlines.AddRun(L"EF");
		auto* firstIdentity = &first;

		RichTextBox box;
		box.SetDocument(std::move(document));
		CUI_EXPECT_EQ(::FlowDirection::RightToLeft,
			box.GetDocument().GetFlowDirection());
		CUI_EXPECT_EQ(::FlowDirection::RightToLeft,
			first.GetFlowDirection());
		CUI_EXPECT_EQ(::FlowDirection::LeftToRight,
			second.GetFlowDirection());
		box.SelectAll();
		ExpectSelectionFlowDirection(
			box, TextSelectionPropertyValueKind::Mixed);

		const auto chunks =
			RichTextBoxDocumentTestAccess::RebuildDocumentLayoutChunks(box);
		CUI_EXPECT_EQ(2ULL,
			static_cast<unsigned long long>(chunks.size()));
		if (chunks.size() == 2)
		{
			CUI_EXPECT_EQ(::FlowDirection::RightToLeft,
				chunks[0].Direction);
			CUI_EXPECT_EQ(::FlowDirection::LeftToRight,
				chunks[1].Direction);
		}
		const auto readingDirection =
			RichTextBoxDocumentTestAccess::FirstBlockReadingDirection(box);
		CUI_EXPECT_TRUE(readingDirection.has_value());
		if (readingDirection)
			CUI_EXPECT_EQ(DWRITE_READING_DIRECTION_RIGHT_TO_LEFT,
				*readingDirection);
		const auto physicalAlignment =
			RichTextBoxDocumentTestAccess::FirstBlockTextAlignment(box);
		CUI_EXPECT_TRUE(physicalAlignment.has_value());
		if (physicalAlignment)
			CUI_EXPECT_EQ(DWRITE_TEXT_ALIGNMENT_TRAILING,
				*physicalAlignment);

		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			Block::FlowDirectionProperty(),
			BindingValue(::FlowDirection::RightToLeft)));
		CUI_EXPECT_TRUE(box.GetDocument().Blocks.At(0) == firstIdentity);
		ExpectSelectionFlowDirection(box,
			TextSelectionPropertyValueKind::Value,
			::FlowDirection::RightToLeft);
		CUI_EXPECT_TRUE(box.CanUndo());
		box.Undo();
		ExpectSelectionFlowDirection(
			box, TextSelectionPropertyValueKind::Mixed);
		box.Redo();
		ExpectSelectionFlowDirection(box,
			TextSelectionPropertyValueKind::Value,
			::FlowDirection::RightToLeft);

		box.Select(2, 0);
		CUI_EXPECT_TRUE(RichTextBoxDocumentTestAccess::ExecuteCommand(
			box, EditingCommands::MoveRightByCharacter()));
		CUI_EXPECT_EQ(1, box.GetCaretIndex());
		CUI_EXPECT_TRUE(RichTextBoxDocumentTestAccess::ExecuteCommand(
			box, EditingCommands::MoveLeftByCharacter()));
		CUI_EXPECT_EQ(2, box.GetCaretIndex());
		CUI_EXPECT_TRUE(RichTextBoxDocumentTestAccess::ExecuteCommand(
			box, EditingCommands::SelectRightByCharacter()));
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			box.GetSelection().GetAnchorPosition().GetTextOffset()));
		CUI_EXPECT_EQ(1, box.GetCaretIndex());
		box.Select(2, 0);
		CUI_EXPECT_TRUE(RichTextBoxDocumentTestAccess::ExecuteCommand(
			box, EditingCommands::MoveRightByWord()));
		CUI_EXPECT_EQ(CuiTextEdit::GetPreviousWordCaretIndex(
			box.Text, 2, true), box.GetCaretIndex());
		box.Select(2, 0);
		CUI_EXPECT_TRUE(RichTextBoxDocumentTestAccess::ExecuteCommand(
			box, EditingCommands::MoveLeftByWord()));
		CUI_EXPECT_EQ(CuiTextEdit::GetNextWordCaretIndex(
			box.Text, 2, true), box.GetCaretIndex());

		auto inheritedDocument = std::make_unique<FlowDocument>();
		inheritedDocument->SetFlowDirection(
			::FlowDirection::RightToLeft);
		auto& inheritedParagraph =
			inheritedDocument->Blocks.AddParagraph();
		inheritedParagraph.Inlines.AddRun(L"same");
		RichTextBox inherited;
		inherited.SetDocument(std::move(inheritedDocument));
		inherited.Select(2, 0);
		CUI_EXPECT_TRUE(inherited.GetSelection().ApplyPropertyValue(
			Block::FlowDirectionProperty(),
			BindingValue(::FlowDirection::RightToLeft)));
		CUI_EXPECT_FALSE(inherited.CanUndo());
		CUI_EXPECT_FALSE(inheritedParagraph.HasPropertyValue(
			Block::FlowDirectionProperty(),
			DependencyPropertyValueSource::Local));
		inherited.GetDocument().SetFlowDirection(
			::FlowDirection::LeftToRight);
		CUI_EXPECT_EQ(::FlowDirection::LeftToRight,
			inheritedParagraph.GetFlowDirection());
	});

	runner.Add(
		"RichTextBox Shift Enter inserts LineBreak while Enter splits Paragraph",
		[]
	{
		class InputRichTextBox final : public RichTextBox
		{
		public:
			using RichTextBox::ProcessInput;
		};
		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		auto bold = std::make_unique<Bold>();
		bold->Inlines.AddRun(L"AB");
		paragraph.Inlines.Add(std::move(bold));
		InputRichTextBox inlineBreak;
		inlineBreak.SetDocument(std::move(document));
		inlineBreak.Select(1, 0);
		InputReport shiftEnter;
		shiftEnter.Kind = InputReportKind::KeyDown;
		shiftEnter.Key = Key::Return;
		shiftEnter.Modifiers = ModifierKeys::Shift;
		CUI_EXPECT_TRUE(inlineBreak.ProcessInput(shiftEnter));
		CUI_EXPECT_EQ(std::wstring(L"A\r\nB"), inlineBreak.Text);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			inlineBreak.GetDocument().Blocks.Count()));
		auto* inlineParagraph = dynamic_cast<Paragraph*>(
			inlineBreak.GetDocument().Blocks.At(0));
		auto* inlineBold = inlineParagraph
			? dynamic_cast<Bold*>(inlineParagraph->Inlines.At(0)) : nullptr;
		CUI_EXPECT_TRUE(inlineBold != nullptr);
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			inlineBold ? inlineBold->Inlines.Count() : 0));
		CUI_EXPECT_TRUE(inlineBold
			&& dynamic_cast<LineBreak*>(inlineBold->Inlines.At(1)) != nullptr);
		CUI_EXPECT_TRUE(inlineBreak.CanUndo());
		inlineBreak.Undo();
		CUI_EXPECT_EQ(std::wstring(L"AB"), inlineBreak.Text);

		InputRichTextBox paragraphBreak;
		paragraphBreak.Text = L"AB";
		paragraphBreak.Select(1, 0);
		InputReport enter;
		enter.Kind = InputReportKind::KeyDown;
		enter.Key = Key::Return;
		CUI_EXPECT_TRUE(paragraphBreak.ProcessInput(enter));
		CUI_EXPECT_EQ(std::wstring(L"A\r\nB"), paragraphBreak.Text);
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			paragraphBreak.GetDocument().Blocks.Count()));
		const auto paragraphFragment = paragraphBreak.GetDocument().Flatten();
		const auto separator = std::find_if(
			paragraphFragment.StructureSpans.begin(),
			paragraphFragment.StructureSpans.end(), [](const auto& span)
			{
				return span.Path.back().Kind
					== RichTextStructureKind::ParagraphBreak;
			});
		CUI_EXPECT_TRUE(separator
			!= paragraphFragment.StructureSpans.end());

		InputRichTextBox empty;
		empty.SetFontSize(30.0);
		CUI_EXPECT_TRUE(empty.ProcessInput(shiftEnter));
		CUI_EXPECT_EQ(std::wstring(L"\r\n"), empty.Text);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			empty.GetDocument().Blocks.Count()));
		auto* emptyParagraph = dynamic_cast<Paragraph*>(
			empty.GetDocument().Blocks.At(0));
		CUI_EXPECT_TRUE(emptyParagraph != nullptr);
		auto* emptyLineBreak = emptyParagraph
			? dynamic_cast<LineBreak*>(emptyParagraph->Inlines.At(0)) : nullptr;
		CUI_EXPECT_TRUE(emptyLineBreak != nullptr);
		empty.Select(0, 0);
		empty.InsertText(L"A");
		CUI_EXPECT_EQ(std::wstring(L"A\r\n"), empty.Text);
		CUI_EXPECT_TRUE(empty.GetDocument().Blocks.At(0) == emptyParagraph);
		CUI_EXPECT_TRUE(emptyParagraph
			&& emptyParagraph->Inlines.At(1) == emptyLineBreak);
	});

	runner.Add(
		"RichTextBox document public selected text expands atomic UTF16 ranges",
		[]
	{
		RichTextBox box;
		box.Text = L"A\r\n\xD83D\xDE00" L"B";
		box.Select(2, 1);
		CUI_EXPECT_EQ(std::wstring(L"\r\n"), box.GetSelectedString());
		box.Select(4, 1);
		CUI_EXPECT_EQ(
			std::wstring(L"\xD83D\xDE00"), box.GetSelectedString());
	});

	runner.Add(
		"RichTextBox document surrogate interior caret uses snapped insertion affinity",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		paragraph.Inlines.AddRun(L"A");
		auto& emoji = paragraph.Inlines.AddRun(L"\xD83D\xDE00");
		emoji.SetFontWeight(DWRITE_FONT_WEIGHT_BOLD);
		paragraph.Inlines.AddRun(L"B");

		RichTextBox box;
		box.SetDocument(std::move(document));
		box.Select(2, 0); // Between the surrogate code units.
		box.InsertText(L"X");
		CUI_EXPECT_EQ(
			std::wstring(L"A\xD83D\xDE00" L"XB"), box.Text);
		ExpectWeight(Flatten(box), 3, DWRITE_FONT_WEIGHT_BOLD);
	});

	runner.Add(
		"RichTextBox document caret selection deletion and limits preserve composed Unicode",
		[]
		{
			class InputRichTextBox final : public RichTextBox
			{
			public:
				using RichTextBox::ProcessInput;
			};

			const std::wstring combining = L"e\u0301";
			const std::wstring combiningText = L"A" + combining + L"B";
			InputReport right;
			right.Kind = InputReportKind::KeyDown;
			right.Key = Key::Right;
			InputReport left;
			left.Kind = InputReportKind::KeyDown;
			left.Key = Key::Left;
			InputReport back;
			back.Kind = InputReportKind::KeyDown;
			back.Key = Key::Back;
			InputReport deleteKey;
			deleteKey.Kind = InputReportKind::KeyDown;
			deleteKey.Key = Key::Delete;

			InputRichTextBox editor;
			editor.Text = combiningText;
			int keyboardSelectionChanges = 0;
			auto selectionChanged = editor.SelectionChanged.Subscribe(
				[&](Control*, SelectionChangedEventArgs&)
				{ ++keyboardSelectionChanges; });
			editor.Select(1, 1); // The moving edge is inside e + combining acute.
			const int beforeRight = keyboardSelectionChanges;
			CUI_EXPECT_TRUE(editor.ProcessInput(right));
			CUI_EXPECT_EQ(3, editor.GetCaretIndex());
			CUI_EXPECT_EQ(0, editor.GetSelectionLength());
			CUI_EXPECT_EQ(beforeRight + 1, keyboardSelectionChanges);
			editor.Select(2, 1); // The anchor edge is inside the same text element.
			const int beforeLeft = keyboardSelectionChanges;
			CUI_EXPECT_TRUE(editor.ProcessInput(left));
			CUI_EXPECT_EQ(1, editor.GetCaretIndex());
			CUI_EXPECT_EQ(0, editor.GetSelectionLength());
			CUI_EXPECT_EQ(beforeLeft + 1, keyboardSelectionChanges);
			editor.SetCaretIndex(1);
			CUI_EXPECT_TRUE(editor.ProcessInput(right));
			CUI_EXPECT_EQ(3, editor.GetCaretIndex());
			CUI_EXPECT_TRUE(editor.ProcessInput(back));
			CUI_EXPECT_EQ(std::wstring(L"AB"), editor.Text);
			CUI_EXPECT_EQ(1, editor.GetCaretIndex());
			CUI_EXPECT_TRUE(editor.CanUndo());
			editor.Undo();
			CUI_EXPECT_EQ(combiningText, editor.Text);
			CUI_EXPECT_EQ(3, editor.GetCaretIndex());

			editor.SetCaretIndex(1);
			CUI_EXPECT_TRUE(editor.ProcessInput(deleteKey));
			CUI_EXPECT_EQ(std::wstring(L"AB"), editor.Text);
			editor.Undo();
			editor.Select(2, 1); // Select only the combining mark.
			CUI_EXPECT_EQ(combining, editor.GetSelectedString());
			editor.InsertText(L"X");
			CUI_EXPECT_EQ(std::wstring(L"AXB"), editor.Text);
			editor.Undo();
			CUI_EXPECT_EQ(combiningText, editor.Text);

			const std::wstring family =
				L"\xD83D\xDC69\u200D\xD83D\xDC69\u200D"
				L"\xD83D\xDC67\u200D\xD83D\xDC66";
			std::wstring familyText = L"A";
			familyText += family;
			familyText += L"B";
			InputRichTextBox emoji;
			emoji.Text = familyText;
			emoji.SetCaretIndex(1);
			CUI_EXPECT_TRUE(emoji.ProcessInput(right));
			CUI_EXPECT_EQ(12, emoji.GetCaretIndex());
			CUI_EXPECT_TRUE(emoji.ProcessInput(back));
			CUI_EXPECT_EQ(std::wstring(L"AB"), emoji.Text);
			CUI_EXPECT_TRUE(emoji.CanUndo());
			emoji.Undo();
			CUI_EXPECT_EQ(familyText, emoji.Text);

			RichTextBox limited;
			limited.MaxLength = 1;
			limited.InsertText(combining);
			CUI_EXPECT_TRUE(limited.Text.empty());
			CUI_EXPECT_FALSE(limited.CanUndo());
			limited.MaxLength = 2;
			limited.InsertText(combining);
			CUI_EXPECT_EQ(combining, limited.Text);

			const std::wstring emojiModifier =
				L"\xD83D\xDC4D\xD83C\xDFFD";
			RichTextBox emojiLimit;
			emojiLimit.MaxLength = 3;
			emojiLimit.InsertText(emojiModifier);
			CUI_EXPECT_TRUE(emojiLimit.Text.empty());
			emojiLimit.MaxLength = 4;
			emojiLimit.InsertText(emojiModifier);
			CUI_EXPECT_EQ(emojiModifier, emojiLimit.Text);

			FlowDocument splitRun;
			auto& paragraph = splitRun.Blocks.AddParagraph();
			auto& base = paragraph.Inlines.AddRun(L"Ae");
			auto& mark = paragraph.Inlines.AddRun(L"\u0301");
			mark.SetFontWeight(DWRITE_FONT_WEIGHT_BOLD);
			paragraph.Inlines.AddRun(L"B");
			auto interior = splitRun.CreateTextPointerAtTextOffset(
				2, LogicalDirection::Forward);
			CUI_EXPECT_FALSE(interior.IsAtInsertionPosition());
			CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
				interior.GetInsertionPosition(
					LogicalDirection::Backward).GetTextOffset()));
			CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
				interior.GetInsertionPosition(
					LogicalDirection::Forward).GetTextOffset()));
			auto next = splitRun.CreateTextPointerAtTextOffset(
				1, LogicalDirection::Forward)
				.GetNextInsertionPosition(LogicalDirection::Forward);
			CUI_EXPECT_TRUE(next.has_value());
			CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
				next ? next->GetTextOffset() : 0));
			auto interiorBackward =
				splitRun.CreateTextPointerAtTextOffset(
					2, LogicalDirection::Backward);
			auto interiorForward =
				splitRun.CreateTextPointerAtTextOffset(
					2, LogicalDirection::Forward);
			base.SetText(L"Ax");
			CUI_EXPECT_EQ(std::wstring(L"Ax\u0301B"),
				splitRun.Flatten().Text);
			CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
				interiorBackward.GetTextOffset()));
			CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
				interiorForward.GetTextOffset()));

			RichTextBox virtualCaret;
			const auto roundTrip =
				RichTextBoxDocumentTestAccess::RoundTripVirtualCaret(
					virtualCaret, combiningText,
					220.0f, 80.0f, 2);
			CUI_EXPECT_TRUE(roundTrip.first);
			CUI_EXPECT_EQ(3, roundTrip.second);
		});

	runner.Add(
		"RichTextBox double click selects a Unicode word instead of its whole line",
		[]
		{
			class InputRichTextBox final : public RichTextBox
			{
			public:
				using RichTextBox::ProcessInput;
			};

			Window host;
			auto owned = std::make_unique<InputRichTextBox>();
			auto* box = owned.get();
			box->Text = L"one e\u0301lan!\r\nnext";
			box->Width = 320.0;
			box->Height = 100.0;
			CUI_EXPECT_TRUE(host.SetVisualContent(
				std::move(owned)) == box);
			box->Arrange(cui::core::Rect{
				0.0f, 0.0f, 320.0f, 100.0f });

			const auto wordPoint =
				RichTextBoxDocumentTestAccess::TextPoint(*box, 6);
			InputReport doubleClick;
			doubleClick.Kind = InputReportKind::PointerDoubleClick;
			doubleClick.ChangedButton = MouseButton::Left;
			doubleClick.X = wordPoint.first;
			doubleClick.Y = wordPoint.second;
			CUI_EXPECT_TRUE(box->ProcessInput(doubleClick));
			CUI_EXPECT_EQ(std::wstring(L"e\u0301lan"),
				box->GetSelectedString());
			CUI_EXPECT_EQ(4, box->GetSelectionStart());
			CUI_EXPECT_EQ(5, box->GetSelectionLength());

			const auto punctuationPoint =
				RichTextBoxDocumentTestAccess::TextPoint(*box, 9);
			doubleClick.X = punctuationPoint.first;
			doubleClick.Y = punctuationPoint.second;
			CUI_EXPECT_TRUE(box->ProcessInput(doubleClick));
			CUI_EXPECT_EQ(std::wstring(L"!"), box->GetSelectedString());
		});

	runner.Add(
		"RichTextBox document preserves trailing empty Run typing style across edit",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& first = document->Blocks.AddParagraph();
		first.Inlines.AddRun(L"A");
		auto& trailing = document->Blocks.AddParagraph();
		auto& emptyRun = trailing.Inlines.AddRun();
		emptyRun.SetUnderline(true);

		RichTextBox box;
		box.SetDocument(std::move(document));
		box.Select(0, 0);
		box.InsertText(L"Z");
		CUI_EXPECT_EQ(std::wstring(L"ZA\r\n"), box.Text);
		box.Select(static_cast<int>(box.Text.size()), 0);
		ExpectSelectionUnderline(
			box, TextSelectionPropertyValueKind::Value, true);
		box.InsertText(L"X");
		CUI_EXPECT_EQ(std::wstring(L"ZA\r\nX"), box.Text);
		ExpectUnderline(Flatten(box), 4, true);
	});

	runner.Add(
		"RichTextBox document inherited ClearAll omits ineffective undo",
		[]
	{
		RichTextBox box;
		box.GetDocument().SetFontWeight(DWRITE_FONT_WEIGHT_BOLD);
		box.Text = L"abc";
		box.SelectAll();
		box.GetSelection().ClearAllProperties();
		ExpectWeight(Flatten(box), 1, DWRITE_FONT_WEIGHT_BOLD);
		CUI_EXPECT_FALSE(box.CanUndo());
		CUI_EXPECT_FALSE(box.CanRedo());

		CUI_EXPECT_TRUE(box.GetSelection().ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_NORMAL)));
		box.Undo();
		CUI_EXPECT_TRUE(box.CanRedo());
		box.GetSelection().ClearAllProperties();
		CUI_EXPECT_TRUE(box.CanRedo());
	});

	runner.Add(
		"RichTextBox document history is committed for callbacks and reentrant undo",
		[]
	{
		RichTextBox box;
		box.Text = L"abc";
		box.Select(3, 0);
		int textEvents = 0;
		bool insertSawUndo = false;
		bool undoSawRedo = false;
		bool redoSawUndo = false;
		bool reenterOnce = true;
		auto textChanged = box.OnTextChanged.Subscribe(
			[&](Control*, TextChangedEventArgs& args)
			{
				++textEvents;
				if (args.NewText == L"abcX" && reenterOnce)
				{
					reenterOnce = false;
					insertSawUndo = box.CanUndo() && !box.CanRedo();
					box.Undo();
				}
				else if (args.NewText == L"abc")
					undoSawRedo = !box.CanUndo() && box.CanRedo();
				else if (args.NewText == L"abcX")
					redoSawUndo = box.CanUndo() && !box.CanRedo();
			});

		box.InsertText(L"X");
		CUI_EXPECT_EQ(std::wstring(L"abc"), box.Text);
		CUI_EXPECT_TRUE(insertSawUndo);
		CUI_EXPECT_TRUE(undoSawRedo);
		CUI_EXPECT_FALSE(box.CanUndo());
		CUI_EXPECT_TRUE(box.CanRedo());
		box.Redo();
		CUI_EXPECT_EQ(std::wstring(L"abcX"), box.Text);
		CUI_EXPECT_TRUE(redoSawUndo);
		CUI_EXPECT_EQ(3, textEvents);

		RichTextBox formatBox;
		formatBox.Text = L"abc";
		formatBox.SelectAll();
		bool formatSawUndo = false;
		bool reentrantUndoAttempted = false;
		auto documentChanged = formatBox.GetDocument().Changed.Subscribe(
			[&](FlowDocument*)
			{
				formatSawUndo = formatBox.CanUndo();
				reentrantUndoAttempted = true;
				formatBox.Undo();
			});
		CUI_EXPECT_TRUE(formatBox.GetSelection().ApplyPropertyValue(
			TextElement::FontWeightProperty(),
			BindingValue(DWRITE_FONT_WEIGHT_BOLD)));
		CUI_EXPECT_TRUE(formatSawUndo);
		CUI_EXPECT_TRUE(reentrantUndoAttempted);
		ExpectWeight(Flatten(formatBox), 1, DWRITE_FONT_WEIGHT_BOLD);
		CUI_EXPECT_TRUE(formatBox.CanUndo());
	});

	runner.Add(
		"RichTextBox document tree transactions roll back throwing callbacks",
		[]
	{
		FlowDocument document;
		auto& paragraph = document.Blocks.AddParagraph();
		paragraph.Inlines.AddRun(L"old");
		auto* originalParagraph = document.Blocks.At(0);
		const auto original = document.Flatten();
		auto throwing = document.Changed.Subscribe(
			[](FlowDocument*)
			{
				throw std::runtime_error("expected document callback failure");
			});

		bool replaceThrew = false;
		try
		{
			(void)document.ReplaceFromFragment(
				RichTextDocumentFragment::FromPlainText(L"new"));
		}
		catch (const std::runtime_error&)
		{
			replaceThrew = true;
		}
		CUI_EXPECT_TRUE(replaceThrew);
		CUI_EXPECT_FALSE(document.IsChanging());
		CUI_EXPECT_EQ(original, document.Flatten());
		CUI_EXPECT_TRUE(document.Blocks.At(0) == originalParagraph);
		CUI_EXPECT_TRUE(originalParagraph->GetParent() == &document);
		CUI_EXPECT_TRUE(originalParagraph->GetFlowDocument() == &document);

		bool removeThrew = false;
		try
		{
			(void)document.Blocks.RemoveAt(0);
		}
		catch (const std::runtime_error&)
		{
			removeThrew = true;
		}
		CUI_EXPECT_TRUE(removeThrew);
		CUI_EXPECT_EQ(1ULL,
			static_cast<unsigned long long>(document.Blocks.Count()));
		CUI_EXPECT_TRUE(document.Blocks.At(0) == originalParagraph);
		CUI_EXPECT_FALSE(document.IsChanging());

		throwing = {};
		std::wstring error;
		CUI_EXPECT_TRUE(document.ReplaceFromFragment(
			RichTextDocumentFragment::FromPlainText(L"new"), &error));
		CUI_EXPECT_TRUE(error.empty());
		CUI_EXPECT_EQ(std::wstring(L"new"), document.Flatten().Text);

		RichTextBox owner;
		owner.Text = L"abc";
		owner.Select(1, 1);
		auto ownerThrowing = owner.GetDocument().Changed.Subscribe(
			[](FlowDocument*)
			{
				throw std::runtime_error("expected owner callback failure");
			});
		bool editorThrew = false;
		try
		{
			owner.InsertText(L"X");
		}
		catch (const std::runtime_error&)
		{
			editorThrew = true;
		}
		CUI_EXPECT_TRUE(editorThrew);
		CUI_EXPECT_EQ(std::wstring(L"abc"), owner.Text);
		CUI_EXPECT_EQ(std::wstring(L"abc"), owner.GetDocument().Flatten().Text);
		CUI_EXPECT_EQ(1, owner.GetSelectionStart());
		CUI_EXPECT_EQ(1, owner.GetSelectionLength());
		CUI_EXPECT_FALSE(owner.CanUndo());
		CUI_EXPECT_FALSE(owner.CanRedo());

		RichTextBox compatibilityOwner;
		compatibilityOwner.Text = L"abc";
		compatibilityOwner.Select(3, 0);
		compatibilityOwner.InsertText(L"X");
		CUI_EXPECT_TRUE(compatibilityOwner.CanUndo());
		const int compatibilityCaret =
			compatibilityOwner.GetCaretIndex();
		auto compatibilityThrowing =
			compatibilityOwner.GetDocument().Changed.Subscribe(
				[](FlowDocument*)
				{
					throw std::runtime_error(
						"expected compatibility callback failure");
				});
		bool compatibilityThrew = false;
		try
		{
			compatibilityOwner.Text = L"replacement";
		}
		catch (const std::runtime_error&)
		{
			compatibilityThrew = true;
		}
		CUI_EXPECT_TRUE(compatibilityThrew);
		CUI_EXPECT_EQ(std::wstring(L"abcX"), compatibilityOwner.Text);
		CUI_EXPECT_EQ(std::wstring(L"abcX"),
			compatibilityOwner.GetDocument().Flatten().Text);
		CUI_EXPECT_EQ(compatibilityCaret,
			compatibilityOwner.GetCaretIndex());
		CUI_EXPECT_TRUE(compatibilityOwner.CanUndo());
		CUI_EXPECT_FALSE(compatibilityOwner.CanRedo());
	});

	runner.Add(
		"RichText clipboard codec is portable and rejects corrupt payloads",
		[]
	{
		FlowDocument document;
		document.SetFontFamily(L"Consolas");
		document.SetLanguage(L"ja-JP");
		document.SetFontStretch(DWRITE_FONT_STRETCH_SEMI_CONDENSED);
		document.SetTextAlignment(::TextAlignment::Center);
		document.SetFlowDirection(::FlowDirection::RightToLeft);
		auto& paragraph = document.Blocks.AddParagraph();
		paragraph.SetTextAlignment(::TextAlignment::Right);
		paragraph.SetFlowDirection(::FlowDirection::LeftToRight);
		paragraph.Inlines.AddRun(L"A");
		auto outer = std::make_unique<Span>();
		outer->SetUnderline(true);
		outer->SetLanguage(L"ja-JP");
		auto bold = std::make_unique<Bold>();
		bold->SetLanguage(L"ko-KR");
		bold->SetFontStretch(DWRITE_FONT_STRETCH_EXPANDED);
		bold->Inlines.AddRun(L"BC");
		bold->Inlines.AddLineBreak();
		bold->Inlines.AddRun(L"EF");
		outer->Inlines.Add(std::move(bold));
		auto emptyItalic = std::make_unique<Italic>();
		auto* emptyItalicIdentity = emptyItalic.get();
		outer->Inlines.Add(std::move(emptyItalic));
		auto& emptyRun = outer->Inlines.AddRun();
		paragraph.Inlines.Add(std::move(outer));
		paragraph.Inlines.AddRun(L"D");

		const auto source = document.Flatten();
		CUI_EXPECT_TRUE(source.ValidateCanonical());
		CUI_EXPECT_FALSE(source.StructureSpans.empty());
		const auto encoded = cui::richtext::clipboard::Encode(source);
		CUI_EXPECT_TRUE(encoded.has_value());
		const auto decoded = cui::richtext::clipboard::Decode(*encoded);
		CUI_EXPECT_TRUE(decoded.has_value());
		CUI_EXPECT_EQ(source.Text, decoded->Text);
		CUI_EXPECT_EQ(source.Spans, decoded->Spans);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_STRETCH>(
			DWRITE_FONT_STRETCH_SEMI_CONDENSED),
			RichTextDocument(*decoded).StyleAt(0).FontStretch);
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"ja-jp"),
			RichTextDocument(*decoded).StyleAt(0).Language);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_STRETCH>(
			DWRITE_FONT_STRETCH_EXPANDED),
			RichTextDocument(*decoded).StyleAt(1).FontStretch);
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"ko-kr"),
			RichTextDocument(*decoded).StyleAt(1).Language);
		CUI_EXPECT_FALSE(decoded->StructureSpans.empty());
		CUI_EXPECT_TRUE(decoded->RootStyle.has_value());
		CUI_EXPECT_TRUE(decoded->RootParagraphStyle.has_value());
		CUI_EXPECT_TRUE(decoded->StructureRootId.has_value());
		CUI_EXPECT_TRUE(source.StructureRootId
			!= decoded->StructureRootId);
		CUI_EXPECT_TRUE(source.StructureSpans.front().Path.front().Id
			!= decoded->StructureSpans.front().Path.front().Id);
		CUI_EXPECT_EQ(RichTextStructureKind::LineBreak,
			decoded->StructureSpans[2].Path.back().Kind);
		CUI_EXPECT_EQ(std::optional<::TextAlignment>(
			::TextAlignment::Right),
			decoded->StructureSpans.front().Path.front()
				.LocalParagraphStyle.TextAlignment);
		RichTextParagraphStyle defaultParagraphStyle;
		defaultParagraphStyle.FlowDirection =
			::FlowDirection::LeftToRight;
		const auto decodedParagraphStyles = RichTextDocument(*decoded)
			.ParagraphStylesInRange(0, decoded->Text.size(),
				defaultParagraphStyle);
		CUI_EXPECT_TRUE(!decodedParagraphStyles.empty());
		if (!decodedParagraphStyles.empty())
			CUI_EXPECT_EQ(::FlowDirection::LeftToRight,
				decodedParagraphStyles.front().FlowDirection.value_or(
					::FlowDirection::LeftToRight));
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			decoded->StructureMarkers.size()));
		CUI_EXPECT_EQ(RichTextStructureKind::Italic,
			decoded->StructureMarkers[0].Path.back().Kind);
		CUI_EXPECT_EQ(RichTextStructureKind::Run,
			decoded->StructureMarkers[1].Path.back().Kind);
		CUI_EXPECT_TRUE(decoded->StructureMarkers[0].Path.back().Id
			!= emptyItalicIdentity->GetRichTextStructureId());
		CUI_EXPECT_TRUE(decoded->StructureMarkers[1].Path.back().Id
			!= emptyRun.GetRichTextStructureId());
		CUI_EXPECT_EQ(decoded->StructureMarkers[0].Path[1].Id,
			decoded->StructureMarkers[1].Path[1].Id);

		// v7 adds Language and v6 adds FontStretch as optional style-mask fields.
		// A payload that does not use either is byte-compatible with v5; v5 appends a direction
		// table after v4's alignment table.
		// This plain fixture has empty eight-byte extension counts, so removing
		// each table in turn creates real v4, v3, v2, and v1 payloads.
		const auto plainEncoded = cui::richtext::clipboard::Encode(
			RichTextDocumentFragment::FromPlainText(L"legacy"));
		CUI_EXPECT_TRUE(plainEncoded.has_value());
		auto writeU32 = [](std::vector<std::uint8_t>& bytes,
			std::size_t offset, std::uint32_t value)
		{
			for (int shift = 0; shift < 32; shift += 8)
				bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
		};
		auto writeU64 = [](std::vector<std::uint8_t>& bytes,
			std::size_t offset, std::uint64_t value)
		{
			for (int shift = 0; shift < 64; shift += 8)
				bytes[offset++] = static_cast<std::uint8_t>(value >> shift);
		};
		auto rewriteHeader = [&](std::vector<std::uint8_t>& bytes,
			std::uint32_t version)
		{
			writeU32(bytes, 8, version);
			writeU64(bytes, 12,
				static_cast<std::uint64_t>(bytes.size() - 28));
			std::uint32_t checksum = 2166136261u;
			for (std::size_t index = 28; index < bytes.size(); ++index)
			{
				checksum ^= bytes[index];
				checksum *= 16777619u;
			}
			writeU32(bytes, 20, checksum);
		};
		auto legacyV6 = *plainEncoded;
		rewriteHeader(legacyV6, 6);
		const auto version6Decoded =
			cui::richtext::clipboard::Decode(legacyV6);
		CUI_EXPECT_TRUE(version6Decoded.has_value());
		CUI_EXPECT_EQ(std::wstring(L"legacy"), version6Decoded->Text);
		auto languageAsV6 = *encoded;
		rewriteHeader(languageAsV6, 6);
		CUI_EXPECT_FALSE(cui::richtext::clipboard::Decode(
			languageAsV6).has_value());

		auto legacyV5 = legacyV6;
		rewriteHeader(legacyV5, 5);
		const auto version5Decoded =
			cui::richtext::clipboard::Decode(legacyV5);
		CUI_EXPECT_TRUE(version5Decoded.has_value());
		CUI_EXPECT_EQ(std::wstring(L"legacy"), version5Decoded->Text);
		RichTextCharacterStyle stretchOnlyStyle;
		stretchOnlyStyle.FontStretch = DWRITE_FONT_STRETCH_EXPANDED;
		const auto stretchOnlyEncoded = cui::richtext::clipboard::Encode(
			RichTextDocumentFragment::FromPlainText(
				L"stretch", stretchOnlyStyle));
		CUI_EXPECT_TRUE(stretchOnlyEncoded.has_value());
		auto stretchedAsV5 = *stretchOnlyEncoded;
		rewriteHeader(stretchedAsV5, 5);
		CUI_EXPECT_FALSE(cui::richtext::clipboard::Decode(
			stretchedAsV5).has_value());

		auto legacyV4 = legacyV5;
		CUI_EXPECT_TRUE(legacyV4.size() > 60);
		legacyV4.resize(legacyV4.size() - 8);
		rewriteHeader(legacyV4, 4);
		const auto version4Decoded =
			cui::richtext::clipboard::Decode(legacyV4);
		CUI_EXPECT_TRUE(version4Decoded.has_value());
		CUI_EXPECT_EQ(std::wstring(L"legacy"), version4Decoded->Text);

		auto legacyV3 = legacyV4;
		legacyV3.resize(legacyV3.size() - 8);
		rewriteHeader(legacyV3, 3);
		const auto version3Decoded =
			cui::richtext::clipboard::Decode(legacyV3);
		CUI_EXPECT_TRUE(version3Decoded.has_value());
		CUI_EXPECT_EQ(std::wstring(L"legacy"), version3Decoded->Text);
		CUI_EXPECT_TRUE(version3Decoded->StructureMarkers.empty());

		auto legacyV2 = legacyV3;
		legacyV2.resize(legacyV2.size() - 8);
		rewriteHeader(legacyV2, 2);
		const auto version2Decoded =
			cui::richtext::clipboard::Decode(legacyV2);
		CUI_EXPECT_TRUE(version2Decoded.has_value());
		CUI_EXPECT_EQ(std::wstring(L"legacy"), version2Decoded->Text);
		CUI_EXPECT_TRUE(version2Decoded->StructureMarkers.empty());
		FlowDocument version2Source;
		auto& version2Paragraph = version2Source.Blocks.AddParagraph();
		auto version2Bold = std::make_unique<Bold>();
		version2Bold->Inlines.AddRun(L"v2");
		version2Paragraph.Inlines.Add(std::move(version2Bold));
		auto structuredV2 = cui::richtext::clipboard::Encode(
			version2Source.Flatten());
		CUI_EXPECT_TRUE(structuredV2.has_value());
		if (structuredV2)
		{
			structuredV2->resize(structuredV2->size() - 8);
			structuredV2->resize(structuredV2->size() - 8);
			structuredV2->resize(structuredV2->size() - 8);
			rewriteHeader(*structuredV2, 2);
			const auto decodedV2 =
				cui::richtext::clipboard::Decode(*structuredV2);
			CUI_EXPECT_TRUE(decodedV2.has_value());
			CUI_EXPECT_TRUE(decodedV2
				&& !decodedV2->StructureSpans.empty());
			CUI_EXPECT_TRUE(decodedV2
				&& decodedV2->StructureMarkers.empty());
			CUI_EXPECT_TRUE(decodedV2
				&& decodedV2->StructureSpans.front().Path.size() == 3
				&& decodedV2->StructureSpans.front().Path[1].Kind
					== RichTextStructureKind::Bold);
		}

		auto legacyV1 = legacyV2;
		legacyV1.resize(legacyV1.size() - 8);
		rewriteHeader(legacyV1, 1);
		const auto legacyDecoded =
			cui::richtext::clipboard::Decode(legacyV1);
		CUI_EXPECT_TRUE(legacyDecoded.has_value());
		CUI_EXPECT_EQ(std::wstring(L"legacy"), legacyDecoded->Text);
		CUI_EXPECT_TRUE(legacyDecoded->StructureSpans.empty());

		auto unknownVersion = *encoded;
		CUI_EXPECT_TRUE(unknownVersion.size() > 8);
		unknownVersion[8] = 8;
		CUI_EXPECT_FALSE(cui::richtext::clipboard::Decode(
			unknownVersion).has_value());

		auto badChecksum = *encoded;
		badChecksum.back() ^= 0x5a;
		CUI_EXPECT_FALSE(cui::richtext::clipboard::Decode(
			badChecksum).has_value());

		auto truncated = *encoded;
		truncated.pop_back();
		CUI_EXPECT_FALSE(cui::richtext::clipboard::Decode(
			truncated).has_value());

		RichTextDocumentFragment malformed;
		malformed.Text = L"ab";
		malformed.Spans.push_back(RichTextStyleSpan{ 0, 1, {} });
		CUI_EXPECT_FALSE(malformed.ValidateCanonical());
		CUI_EXPECT_FALSE(cui::richtext::clipboard::Encode(
			malformed).has_value());
		auto malformedMarker = source;
		malformedMarker.StructureMarkers[0].Path.back().Kind =
			RichTextStructureKind::LineBreak;
		CUI_EXPECT_FALSE(malformedMarker.ValidateCanonical());
		CUI_EXPECT_FALSE(cui::richtext::clipboard::Encode(
			malformedMarker).has_value());
	});

	runner.Add(
		"RichText RTF decodes standard formatting Unicode breaks and alignment",
		[]
	{
		const std::string value =
			"{\\rtf1\\ansi\\ansicpg1252\\deff0\\deflang1033\\uc1"
			"{\\fonttbl{\\f0\\fnil Consolas;}}"
			"{\\colortbl;\\red255\\green0\\blue0;"
			"\\red255\\green255\\blue0;}"
			"\\pard\\rtlpar\\qc\\f0\\fs30\\b\\i\\ul\\strike\\lang1041"
			"\\cf1\\highlight2 A\\u20013?\\line B\\par"
			"\\pard\\ltrpar\\qr\\plain\\f0\\fs24 C"
			"{\\*\\object hidden-object}" "}";
		const auto decoded = cui::richtext::rtf::Decode(value);
		CUI_EXPECT_TRUE(decoded.has_value());
		if (!decoded) return;
		CUI_EXPECT_TRUE(decoded->ValidateCanonical());
		CUI_EXPECT_EQ(std::wstring(L"A\u4e2d\r\nB\r\nC"), decoded->Text);
		RichTextDocument document(*decoded);
		const auto first = document.StyleAt(0);
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"Consolas"),
			first.FontFamily);
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"ja-jp"),
			first.Language);
		CUI_EXPECT_TRUE(first.FontSize.has_value());
		if (first.FontSize) CUI_EXPECT_NEAR(20.0, *first.FontSize, 0.001);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_WEIGHT>(
			DWRITE_FONT_WEIGHT_BOLD), first.FontWeight);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_STYLE>(
			DWRITE_FONT_STYLE_ITALIC), first.FontStyle);
		CUI_EXPECT_EQ(std::optional<bool>(true), first.Underline);
		CUI_EXPECT_EQ(std::optional<bool>(true), first.Strikethrough);
		CUI_EXPECT_TRUE(first.Foreground
			&& first.Foreground->Kind == cui::drawing::BrushKind::Solid);
		CUI_EXPECT_TRUE(first.Background
			&& first.Background->Kind == cui::drawing::BrushKind::Solid);
		if (first.Foreground)
		{
			CUI_EXPECT_NEAR(1.0, first.Foreground->Color.r, 0.001);
			CUI_EXPECT_NEAR(0.0, first.Foreground->Color.g, 0.001);
		}
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"en-us"),
			document.StyleAt(document.Length() - 1).Language);
		if (first.Background)
		{
			CUI_EXPECT_NEAR(1.0, first.Background->Color.r, 0.001);
			CUI_EXPECT_NEAR(1.0, first.Background->Color.g, 0.001);
		}
		const auto paragraphStyles = document.ParagraphStylesInRange(
			0, document.Length());
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			paragraphStyles.size()));
		if (paragraphStyles.size() == 2)
		{
			CUI_EXPECT_EQ(std::optional<::TextAlignment>(
				::TextAlignment::Center), paragraphStyles[0].TextAlignment);
			CUI_EXPECT_EQ(std::optional<::TextAlignment>(
				::TextAlignment::Right), paragraphStyles[1].TextAlignment);
			CUI_EXPECT_EQ(std::optional<::FlowDirection>(
				::FlowDirection::RightToLeft),
				paragraphStyles[0].FlowDirection);
			CUI_EXPECT_EQ(::FlowDirection::LeftToRight,
				paragraphStyles[1].FlowDirection.value_or(
					::FlowDirection::LeftToRight));
		}
		const bool hasLineBreak = std::any_of(
			decoded->StructureSpans.begin(), decoded->StructureSpans.end(),
			[](const auto& span)
			{
				return !span.Path.empty()
					&& span.Path.back().Kind
						== RichTextStructureKind::LineBreak;
			});
		CUI_EXPECT_TRUE(hasLineBreak);
		const auto defaultFont = cui::richtext::rtf::Decode(
			"{\\rtf1\\ansi\\deff1"
			"{\\fonttbl{\\f0 Arial;}{\\f1 Times New Roman;}}Default}");
		CUI_EXPECT_TRUE(defaultFont.has_value());
		CUI_EXPECT_TRUE(defaultFont
			&& RichTextDocument(*defaultFont).StyleAt(0).FontFamily
				== std::optional<std::wstring>(L"Times New Roman"));
		CUI_EXPECT_FALSE(cui::richtext::rtf::Decode(
			"{\\rtf1\\ansi broken").has_value());
		CUI_EXPECT_FALSE(cui::richtext::rtf::Decode(
			"{\\ansi text{\\rtf1}}").has_value());
		CUI_EXPECT_FALSE(cui::richtext::rtf::Decode(
			"{\\rtf1{\\*\\object ignored}}").has_value());
	});

	runner.Add(
		"RichText RTF parser bounds groups binary destinations and mutations",
		[]
	{
		const auto hidden = cui::richtext::rtf::Decode(
			"{\\rtf1\\ansi A{\\pict ignored-image}"
			"{\\*\\unknown ignored-metadata}{\\v hidden-text}B}");
		CUI_EXPECT_TRUE(hidden.has_value());
		CUI_EXPECT_TRUE(hidden && hidden->Text == L"AB");
		CUI_EXPECT_FALSE(cui::richtext::rtf::Decode(
			"{\\rtf1\\ansi A{\\pict\\bin999 x}B}").has_value());
		CUI_EXPECT_FALSE(cui::richtext::rtf::Decode(
			"{\\rtf1\\ansicpg99999 A}").has_value());

		std::string tooDeep = "{\\rtf1 ";
		tooDeep.append(257, '{');
		tooDeep += 'x';
		tooDeep.append(257, '}');
		tooDeep += '}';
		CUI_EXPECT_FALSE(cui::richtext::rtf::Decode(tooDeep).has_value());

		const std::string seed =
			"{\\rtf1\\ansi\\uc1{\\fonttbl{\\f0 Arial;}}"
			"\\pard\\qc\\f0\\fs24 A\\u20013?\\line B\\par C}";
		std::mt19937 random(0x435549u);
		for (int iteration = 0; iteration < 256; ++iteration)
		{
			auto mutated = seed;
			const int edits = 1 + static_cast<int>(random() % 8);
			for (int edit = 0; edit < edits; ++edit)
			{
				const auto index = static_cast<std::size_t>(
					random() % mutated.size());
				static constexpr char alphabet[] = {
					'{', '}', '\\', '\'', '-', '0', '9', 'A', '\0' };
				mutated[index] = alphabet[random()
					% (sizeof(alphabet) / sizeof(alphabet[0]))];
			}
			const auto decoded = cui::richtext::rtf::Decode(mutated);
			CUI_EXPECT_TRUE(!decoded || decoded->ValidateCanonical());
		}
	});

	runner.Add(
		"RichText RTF copy round trips cross application formatting",
		[]
	{
		MemoryClipboardBackend clipboardBackend;
		cui::richtext::clipboard::ScopedBackendOverride clipboardScope(
			clipboardBackend);
		auto sourceDocument = std::make_unique<FlowDocument>();
		auto& first = sourceDocument->Blocks.AddParagraph();
		first.SetTextAlignment(::TextAlignment::Center);
		first.SetFlowDirection(::FlowDirection::RightToLeft);
		auto styled = std::make_unique<Run>(
			L"A{\u4e2d\xD83D\xDE00}");
		styled->SetFontFamily(L"\u5fae\u8f6f\u96c5\u9ed1");
		styled->SetLanguage(L"ja-JP");
		styled->SetFontSize(20.0);
		styled->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD);
		styled->SetFontStyle(DWRITE_FONT_STYLE_ITALIC);
		styled->SetUnderline(true);
		styled->SetStrikethrough(true);
		styled->SetForeground(cui::drawing::MakeSolidColorBrush(
			D2D1::ColorF(0.2f, 0.4f, 0.8f, 1.0f)));
		styled->SetBackground(cui::drawing::MakeSolidColorBrush(
			D2D1::ColorF(1.0f, 0.9f, 0.2f, 1.0f)));
		first.Inlines.Add(std::move(styled));
		first.Inlines.AddLineBreak();
		first.Inlines.AddRun(L"B");
		auto& second = sourceDocument->Blocks.AddParagraph();
		second.SetTextAlignment(::TextAlignment::Right);
		second.SetFlowDirection(::FlowDirection::LeftToRight);
		second.Inlines.AddRun(L"C");
		RichTextBox source;
		source.SetDocument(std::move(sourceDocument));
		source.SelectAll();
		CUI_EXPECT_TRUE(source.Copy());
		CUI_EXPECT_TRUE(clipboardBackend.Stored.has_value());
		CUI_EXPECT_TRUE(clipboardBackend.Stored
			&& clipboardBackend.Stored->Attributed.has_value());
		CUI_EXPECT_TRUE(clipboardBackend.Stored
			&& clipboardBackend.Stored->Rtf.has_value());
		CUI_EXPECT_TRUE(clipboardBackend.Stored
			&& clipboardBackend.Stored->PlainText == source.Text);
		if (!clipboardBackend.Stored || !clipboardBackend.Stored->Rtf) return;
		CUI_EXPECT_TRUE(clipboardBackend.Stored->Rtf->starts_with("{\\rtf1"));
		CUI_EXPECT_TRUE(clipboardBackend.Stored->Rtf->find("\\rtlpar")
			!= std::string::npos);
		CUI_EXPECT_TRUE(clipboardBackend.Stored->Rtf->find("\\lang1041")
			!= std::string::npos);

		const auto decoded = cui::richtext::rtf::Decode(
			*clipboardBackend.Stored->Rtf);
		CUI_EXPECT_TRUE(decoded.has_value());
		if (!decoded) return;
		CUI_EXPECT_EQ(source.Text, decoded->Text);
		const RichTextDocument result(*decoded);
		const auto firstStyle = result.StyleAt(0);
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"\u5fae\u8f6f\u96c5\u9ed1"),
			firstStyle.FontFamily);
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"ja-jp"),
			firstStyle.Language);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_WEIGHT>(
			DWRITE_FONT_WEIGHT_BOLD), firstStyle.FontWeight);
		CUI_EXPECT_EQ(std::optional<bool>(true), firstStyle.Underline);
		CUI_EXPECT_EQ(std::optional<bool>(true), firstStyle.Strikethrough);
		const auto alignments = result.ParagraphStylesInRange(
			0, result.Length());
		CUI_EXPECT_EQ(2ULL,
			static_cast<unsigned long long>(alignments.size()));
		if (alignments.size() == 2)
		{
			CUI_EXPECT_EQ(std::optional<::TextAlignment>(
				::TextAlignment::Center), alignments[0].TextAlignment);
			CUI_EXPECT_EQ(std::optional<::TextAlignment>(
				::TextAlignment::Right), alignments[1].TextAlignment);
			CUI_EXPECT_EQ(std::optional<::FlowDirection>(
				::FlowDirection::RightToLeft),
				alignments[0].FlowDirection);
			CUI_EXPECT_EQ(::FlowDirection::LeftToRight,
				alignments[1].FlowDirection.value_or(
					::FlowDirection::LeftToRight));
		}
	});

	runner.Add(
		"RichText RTF output is accepted by Windows RichEdit",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		paragraph.SetTextAlignment(::TextAlignment::Center);
		auto run = std::make_unique<Run>(L"A\u4e2d");
		run->SetFontFamily(L"Segoe UI");
		run->SetFontSize(20.0);
		run->SetFontWeight(DWRITE_FONT_WEIGHT_BOLD);
		run->SetFontStyle(DWRITE_FONT_STYLE_ITALIC);
		run->SetUnderline(true);
		run->SetStrikethrough(true);
		run->SetForeground(cui::drawing::MakeSolidColorBrush(
			D2D1::ColorF(1.0f, 0.0f, 0.0f, 1.0f)));
		paragraph.Inlines.Add(std::move(run));
		auto& second = document->Blocks.AddParagraph();
		second.Inlines.AddRun(L"B");
		const auto expectedText = document->Flatten().Text;
		const auto encoded = cui::richtext::rtf::Encode(document->Flatten());
		CUI_EXPECT_TRUE(encoded.has_value());
		if (!encoded) return;

		HMODULE richEditModule = LoadLibraryW(L"Msftedit.dll");
		CUI_EXPECT_TRUE(richEditModule != nullptr);
		if (!richEditModule) return;
		HWND native = CreateWindowExW(0, MSFTEDIT_CLASS, L"",
			WS_POPUP | ES_MULTILINE, 0, 0, 320, 120,
			nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
		CUI_EXPECT_TRUE(native != nullptr);
		if (!native)
		{
			FreeLibrary(richEditModule);
			return;
		}

		NativeRtfReadState state{ *encoded };
		EDITSTREAM stream{};
		stream.dwCookie = reinterpret_cast<DWORD_PTR>(&state);
		stream.pfnCallback = &ReadNativeRtf;
		const auto streamed = SendMessageW(
			native, EM_STREAMIN, SF_RTF,
			reinterpret_cast<LPARAM>(&stream));
		CUI_EXPECT_TRUE(streamed > 0);
		CUI_EXPECT_EQ(0UL, stream.dwError);

		const int length = GetWindowTextLengthW(native);
		std::wstring nativeText(static_cast<std::size_t>(
			(std::max)(0, length)) + 1, L'\0');
		const int copied = GetWindowTextW(
			native, nativeText.data(), static_cast<int>(nativeText.size()));
		nativeText.resize(static_cast<std::size_t>((std::max)(0, copied)));
		CUI_EXPECT_EQ(expectedText,
			RichTextDocumentFragment::FromPlainText(nativeText).Text);

		CHARRANGE selection{ 0, 1 };
		(void)SendMessageW(native, EM_EXSETSEL, 0,
			reinterpret_cast<LPARAM>(&selection));
		CHARFORMAT2W character{};
		character.cbSize = sizeof(character);
		(void)SendMessageW(native, EM_GETCHARFORMAT, SCF_SELECTION,
			reinterpret_cast<LPARAM>(&character));
		CUI_EXPECT_TRUE((character.dwEffects & CFE_BOLD) != 0);
		CUI_EXPECT_TRUE((character.dwEffects & CFE_ITALIC) != 0);
		CUI_EXPECT_TRUE((character.dwEffects & CFE_UNDERLINE) != 0);
		CUI_EXPECT_TRUE((character.dwEffects & CFE_STRIKEOUT) != 0);
		CUI_EXPECT_EQ(RGB(255, 0, 0), character.crTextColor);
		CUI_EXPECT_NEAR(300.0, static_cast<double>(character.yHeight), 1.0);

		PARAFORMAT2 paragraphFormat{};
		paragraphFormat.cbSize = sizeof(paragraphFormat);
		(void)SendMessageW(native, EM_GETPARAFORMAT, 0,
			reinterpret_cast<LPARAM>(&paragraphFormat));
		CUI_EXPECT_EQ(static_cast<WORD>(PFA_CENTER),
			paragraphFormat.wAlignment);

		DestroyWindow(native);
		FreeLibrary(richEditModule);
	});

	runner.Add(
		"RichText paste prefers CUI rich then RTF then Unicode fallback",
		[]
	{
		MemoryClipboardBackend clipboardBackend;
		cui::richtext::clipboard::ScopedBackendOverride clipboardScope(
			clipboardBackend);
		RichTextCharacterStyle rtfStyle;
		rtfStyle.FontWeight = DWRITE_FONT_WEIGHT_BOLD;
		rtfStyle.Underline = true;
		const auto rtfFragment = RichTextDocumentFragment::FromPlainText(
			L"rtf", rtfStyle);
		const auto rtf = cui::richtext::rtf::Encode(rtfFragment);
		CUI_EXPECT_TRUE(rtf.has_value());
		clipboardBackend.Stored.emplace();
		clipboardBackend.Stored->Rtf = rtf;
		RichTextBox rtfOnly;
		CUI_EXPECT_TRUE(rtfOnly.CanPaste());
		CUI_EXPECT_TRUE(rtfOnly.Paste());
		CUI_EXPECT_EQ(std::wstring(L"rtf"), rtfOnly.Text);
		ExpectUnderline(Flatten(rtfOnly), 0, true);

		cui::richtext::clipboard::DataObject data;
		data.Attributed = std::vector<std::uint8_t>{ 1, 2, 3 };
		data.Rtf = rtf;
		data.PlainText = L"plain";
		CUI_EXPECT_TRUE(cui::richtext::clipboard::Publish(nullptr, data));
		RichTextBox rtfPreferred;
		rtfPreferred.SetFontSize(30.0);
		CUI_EXPECT_TRUE(rtfPreferred.Paste());
		CUI_EXPECT_EQ(std::wstring(L"rtf"), rtfPreferred.Text);
		ExpectWeight(Flatten(rtfPreferred), 0, DWRITE_FONT_WEIGHT_BOLD);
		ExpectUnderline(Flatten(rtfPreferred), 0, true);
		CUI_EXPECT_EQ(std::optional<float>(16.0f),
			RichTextDocument(Flatten(rtfPreferred)).StyleAt(0).FontSize);
		CUI_EXPECT_TRUE(rtfPreferred.CanUndo());
		rtfPreferred.Undo();
		CUI_EXPECT_TRUE(rtfPreferred.Text.empty());

		data.Rtf = "{\\rtf1\\ansi malformed";
		data.PlainText = L"unicode fallback";
		CUI_EXPECT_TRUE(cui::richtext::clipboard::Publish(nullptr, data));
		RichTextBox unicodeFallback;
		CUI_EXPECT_TRUE(unicodeFallback.Paste());
		CUI_EXPECT_EQ(std::wstring(L"unicode fallback"),
			unicodeFallback.Text);

		RichTextCharacterStyle privateStyle;
		privateStyle.Strikethrough = true;
		const auto privatePayload = cui::richtext::clipboard::Encode(
			RichTextDocumentFragment::FromPlainText(L"private", privateStyle));
		CUI_EXPECT_TRUE(privatePayload.has_value());
		data.Attributed = privatePayload;
		data.Rtf = rtf;
		data.PlainText = L"plain";
		CUI_EXPECT_TRUE(cui::richtext::clipboard::Publish(nullptr, data));
		RichTextBox privatePreferred;
		CUI_EXPECT_TRUE(privatePreferred.Paste());
		CUI_EXPECT_EQ(std::wstring(L"private"), privatePreferred.Text);
		CUI_EXPECT_EQ(std::optional<bool>(true),
			Flatten(privatePreferred).Spans.front().Style.Strikethrough);
	});

	runner.Add(
		"RichText clipboard copy paste keeps nested effective formatting",
		[]
	{
		MemoryClipboardBackend clipboardBackend;
		cui::richtext::clipboard::ScopedBackendOverride clipboardScope(
			clipboardBackend);
		{
			MemoryClipboardBackend nestedBackend;
			cui::richtext::clipboard::ScopedBackendOverride nestedScope(
				nestedBackend);
			cui::richtext::clipboard::DataObject probe;
			probe.PlainText = L"nested";
			CUI_EXPECT_TRUE(cui::richtext::clipboard::Publish(
				nullptr, probe));
			CUI_EXPECT_EQ(1, nestedBackend.PublishCount);
			CUI_EXPECT_EQ(0, clipboardBackend.PublishCount);
		}
		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		paragraph.Inlines.AddRun(L"A");
		auto outer = std::make_unique<Span>();
		outer->SetUnderline(true);
		outer->SetLanguage(L"ja-JP");
		auto bold = std::make_unique<Bold>();
		bold->Inlines.AddRun(L"BC");
		bold->Inlines.AddLineBreak();
		bold->Inlines.AddRun(L"EF");
		outer->Inlines.Add(std::move(bold));
		paragraph.Inlines.Add(std::move(outer));
		paragraph.Inlines.AddRun(L"D");

		RichTextBox source;
		source.SetDocument(std::move(document));
		source.Select(1, 6);
		CUI_EXPECT_TRUE(source.Copy());
		CUI_EXPECT_EQ(1, clipboardBackend.PublishCount);
		auto clipboard = cui::richtext::clipboard::Read(nullptr);
		CUI_EXPECT_TRUE(clipboard.has_value());
		CUI_EXPECT_TRUE(clipboard->Attributed.has_value());
		auto portable = cui::richtext::clipboard::Decode(
			*clipboard->Attributed);
		CUI_EXPECT_TRUE(portable.has_value());
		CUI_EXPECT_EQ(std::wstring(L"BC\r\nEF"), portable->Text);
		CUI_EXPECT_FALSE(portable->StructureSpans.empty());
		CUI_EXPECT_TRUE(portable->RootStyle.has_value());
		CUI_EXPECT_TRUE(portable->StructureRootId.has_value());
		CUI_EXPECT_EQ(RichTextStructureKind::LineBreak,
			portable->StructureSpans[1].Path.back().Kind);
		const RichTextDocument portableDocument(*portable);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_WEIGHT>(
			DWRITE_FONT_WEIGHT_BOLD),
			portableDocument.StyleAt(0).FontWeight);
		CUI_EXPECT_EQ(std::optional<bool>(true),
			portableDocument.StyleAt(0).Underline);
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"ja-jp"),
			portableDocument.StyleAt(0).Language);

		RichTextBox target;
		target.SetFontSize(30.0);
		CUI_EXPECT_TRUE(target.Paste());
		CUI_EXPECT_EQ(std::wstring(L"BC\r\nEF"), target.Text);
		CUI_EXPECT_EQ(6, target.GetCaretIndex());
		const auto pasted = Flatten(target);
		ExpectWeight(pasted, 0, DWRITE_FONT_WEIGHT_BOLD);
		ExpectUnderline(pasted, 0, true);
		CUI_EXPECT_EQ(std::optional<float>(12.0f),
			RichTextDocument(pasted).StyleAt(0).FontSize);
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"ja-jp"),
			RichTextDocument(pasted).StyleAt(0).Language);
		auto* pastedParagraph = dynamic_cast<Paragraph*>(
			target.GetDocument().Blocks.At(0));
		CUI_EXPECT_TRUE(pastedParagraph != nullptr);
		auto* pastedSpan = pastedParagraph
			? dynamic_cast<Span*>(pastedParagraph->Inlines.At(0)) : nullptr;
		CUI_EXPECT_TRUE(pastedSpan != nullptr);
		auto* pastedBold = pastedSpan
			? dynamic_cast<Bold*>(pastedSpan->Inlines.At(0)) : nullptr;
		CUI_EXPECT_TRUE(pastedBold != nullptr);
		CUI_EXPECT_TRUE(pastedBold
			&& dynamic_cast<LineBreak*>(pastedBold->Inlines.At(1)) != nullptr);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			target.GetDocument().Blocks.Count()));
		CUI_EXPECT_TRUE(target.CanUndo());
		target.Undo();
		CUI_EXPECT_TRUE(target.Text.empty());
		CUI_EXPECT_FALSE(target.CanUndo());
		CUI_EXPECT_TRUE(target.CanRedo());
		target.Redo();
		CUI_EXPECT_EQ(std::wstring(L"BC\r\nEF"), target.Text);

		CUI_EXPECT_TRUE(source.Cut());
		CUI_EXPECT_EQ(2, clipboardBackend.PublishCount);
		CUI_EXPECT_EQ(std::wstring(L"AD"), source.Text);
		CUI_EXPECT_TRUE(source.CanUndo());
		source.Undo();
		CUI_EXPECT_EQ(std::wstring(L"ABC\r\nEFD"), source.Text);
		ExpectWeight(Flatten(source), 1, DWRITE_FONT_WEIGHT_BOLD);
		CUI_EXPECT_TRUE(source.CanRedo());
		const auto beforeFailedCut = Flatten(source);
		source.Select(1, 6);
		clipboardBackend.RejectPublish = true;
		CUI_EXPECT_FALSE(source.Cut());
		CUI_EXPECT_EQ(beforeFailedCut, Flatten(source));
		CUI_EXPECT_TRUE(source.CanRedo());
	});

	runner.Add(
		"RichText clipboard v7 preserves empty Inline and Paragraph topology",
		[]
	{
		MemoryClipboardBackend clipboardBackend;
		cui::richtext::clipboard::ScopedBackendOverride clipboardScope(
			clipboardBackend);
		auto document = std::make_unique<FlowDocument>();
		document->SetFontFamily(L"Consolas");
		auto& first = document->Blocks.AddParagraph();
		first.Inlines.AddRun(L"A");
		auto emptySpan = std::make_unique<Span>();
		auto* sourceSpan = emptySpan.get();
		auto emptyItalic = std::make_unique<Italic>();
		auto* sourceItalic = emptyItalic.get();
		emptyItalic->SetFontSize(24.0);
		emptySpan->Inlines.Add(std::move(emptyItalic));
		auto& sourceEmptyRun = emptySpan->Inlines.AddRun();
		first.Inlines.Add(std::move(emptySpan));
		first.Inlines.AddRun(L"B");
		auto& second = document->Blocks.AddParagraph();
		auto emptyBold = std::make_unique<Bold>();
		auto* sourceBold = emptyBold.get();
		second.Inlines.Add(std::move(emptyBold));
		auto& sourceEmptyParagraph = document->Blocks.AddParagraph();

		RichTextBox source;
		source.SetDocument(std::move(document));
		source.SelectAll();
		CUI_EXPECT_TRUE(source.Copy());
		auto data = cui::richtext::clipboard::Read(nullptr);
		CUI_EXPECT_TRUE(data.has_value());
		CUI_EXPECT_TRUE(data && data->Attributed.has_value());
		if (!data || !data->Attributed) return;
		auto portable = cui::richtext::clipboard::Decode(*data->Attributed);
		CUI_EXPECT_TRUE(portable.has_value());
		if (!portable) return;
		CUI_EXPECT_EQ(std::wstring(L"AB\r\n\r\n"), portable->Text);
		CUI_EXPECT_EQ(4ULL, static_cast<unsigned long long>(
			portable->StructureMarkers.size()));
		CUI_EXPECT_EQ(RichTextStructureKind::Italic,
			portable->StructureMarkers[0].Path.back().Kind);
		CUI_EXPECT_EQ(RichTextStructureKind::Run,
			portable->StructureMarkers[1].Path.back().Kind);
		CUI_EXPECT_EQ(RichTextStructureKind::Bold,
			portable->StructureMarkers[2].Path.back().Kind);
		CUI_EXPECT_EQ(RichTextStructureKind::Paragraph,
			portable->StructureMarkers[3].Path.back().Kind);
		CUI_EXPECT_TRUE(portable->StructureMarkers[0].Path.back().Id
			!= sourceItalic->GetRichTextStructureId());
		CUI_EXPECT_TRUE(portable->StructureMarkers[1].Path.back().Id
			!= sourceEmptyRun.GetRichTextStructureId());
		CUI_EXPECT_TRUE(portable->StructureMarkers[2].Path.back().Id
			!= sourceBold->GetRichTextStructureId());
		CUI_EXPECT_TRUE(portable->StructureMarkers[3].Path.back().Id
			!= sourceEmptyParagraph.GetRichTextStructureId());

		RichTextBox target;
		const auto targetInitial = target.GetDocument().Flatten();
		CUI_EXPECT_TRUE(target.Paste());
		CUI_EXPECT_EQ(std::wstring(L"AB\r\n\r\n"), target.Text);
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			target.GetDocument().Blocks.Count()));
		auto* pastedFirst = dynamic_cast<Paragraph*>(
			target.GetDocument().Blocks.At(0));
		auto* pastedSecond = dynamic_cast<Paragraph*>(
			target.GetDocument().Blocks.At(1));
		auto* pastedThird = dynamic_cast<Paragraph*>(
			target.GetDocument().Blocks.At(2));
		CUI_EXPECT_TRUE(pastedFirst != nullptr);
		CUI_EXPECT_TRUE(pastedSecond != nullptr);
		CUI_EXPECT_TRUE(pastedThird != nullptr);
		if (!pastedFirst || !pastedSecond || !pastedThird) return;
		CUI_EXPECT_TRUE(pastedThird->Inlines.Empty());
		auto* pastedSpan = dynamic_cast<Span*>(
			pastedFirst->Inlines.At(1));
		auto* pastedBold = dynamic_cast<Bold*>(
			pastedSecond->Inlines.At(0));
		CUI_EXPECT_TRUE(pastedSpan != nullptr);
		CUI_EXPECT_TRUE(pastedBold != nullptr);
		if (!pastedSpan || !pastedBold) return;
		auto* pastedItalic = dynamic_cast<Italic*>(
			pastedSpan->Inlines.At(0));
		auto* pastedEmptyRun = dynamic_cast<Run*>(
			pastedSpan->Inlines.At(1));
		CUI_EXPECT_TRUE(pastedItalic != nullptr);
		CUI_EXPECT_TRUE(pastedEmptyRun != nullptr);
		CUI_EXPECT_TRUE(pastedEmptyRun
			&& pastedEmptyRun->GetText().empty());
		CUI_EXPECT_TRUE(pastedItalic
			&& pastedItalic->GetFontFamily() == L"Consolas");
		CUI_EXPECT_TRUE(pastedItalic
			&& pastedItalic->GetFontSize() == 24.0);
		CUI_EXPECT_TRUE(pastedSpan->GetRichTextStructureId()
			!= sourceSpan->GetRichTextStructureId());
		CUI_EXPECT_TRUE(pastedItalic
			&& pastedItalic->GetRichTextStructureId()
				!= sourceItalic->GetRichTextStructureId());
		CUI_EXPECT_TRUE(pastedBold->GetRichTextStructureId()
			!= sourceBold->GetRichTextStructureId());
		CUI_EXPECT_TRUE(pastedThird->GetRichTextStructureId()
			!= sourceEmptyParagraph.GetRichTextStructureId());
		CUI_EXPECT_TRUE(target.CanUndo());
		target.Undo();
		CUI_EXPECT_TRUE(target.Text.empty());
		CUI_EXPECT_EQ(targetInitial, target.GetDocument().Flatten());
		CUI_EXPECT_TRUE(target.CanRedo());
		target.Redo();
		CUI_EXPECT_EQ(std::wstring(L"AB\r\n\r\n"), target.Text);
		const auto redone = target.GetDocument().Flatten();
		CUI_EXPECT_EQ(4ULL, static_cast<unsigned long long>(
			redone.StructureMarkers.size()));
	});

	runner.Add(
		"RichText clipboard v7 preserves effective paragraph formatting across roots",
		[]
	{
		MemoryClipboardBackend clipboardBackend;
		cui::richtext::clipboard::ScopedBackendOverride clipboardScope(
			clipboardBackend);
		auto sourceDocument = std::make_unique<FlowDocument>();
		sourceDocument->SetTextAlignment(::TextAlignment::Center);
		sourceDocument->SetFlowDirection(::FlowDirection::RightToLeft);
		auto& first = sourceDocument->Blocks.AddParagraph();
		first.Inlines.AddRun(L"A");
		auto& second = sourceDocument->Blocks.AddParagraph();
		second.SetTextAlignment(::TextAlignment::Right);
		second.SetFlowDirection(::FlowDirection::LeftToRight);
		second.Inlines.AddRun(L"B");
		RichTextBox source;
		source.SetDocument(std::move(sourceDocument));
		source.SelectAll();
		CUI_EXPECT_TRUE(source.Copy());

		auto targetDocument = std::make_unique<FlowDocument>();
		targetDocument->SetTextAlignment(::TextAlignment::Justify);
		targetDocument->SetFlowDirection(::FlowDirection::LeftToRight);
		targetDocument->Blocks.AddParagraph();
		RichTextBox target;
		target.SetDocument(std::move(targetDocument));
		CUI_EXPECT_TRUE(target.Paste());
		CUI_EXPECT_EQ(std::wstring(L"A\r\nB"), target.Text);
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			target.GetDocument().Blocks.Count()));
		auto* pastedFirst = dynamic_cast<Paragraph*>(
			target.GetDocument().Blocks.At(0));
		auto* pastedSecond = dynamic_cast<Paragraph*>(
			target.GetDocument().Blocks.At(1));
		CUI_EXPECT_TRUE(pastedFirst != nullptr);
		CUI_EXPECT_TRUE(pastedSecond != nullptr);
		CUI_EXPECT_TRUE(pastedFirst
			&& pastedFirst->GetTextAlignment() == ::TextAlignment::Center);
		CUI_EXPECT_TRUE(pastedFirst
			&& pastedFirst->GetFlowDirection()
				== ::FlowDirection::RightToLeft);
		CUI_EXPECT_TRUE(pastedSecond
			&& pastedSecond->GetTextAlignment() == ::TextAlignment::Right);
		CUI_EXPECT_TRUE(pastedSecond
			&& pastedSecond->GetFlowDirection()
				== ::FlowDirection::LeftToRight);
		CUI_EXPECT_TRUE(target.CanUndo());
		target.Undo();
		CUI_EXPECT_TRUE(target.Text.empty());
		CUI_EXPECT_EQ(::TextAlignment::Justify,
			target.GetDocument().GetTextAlignment());
		CUI_EXPECT_EQ(::FlowDirection::LeftToRight,
			target.GetDocument().GetFlowDirection());
	});

	runner.Add(
		"RichText clipboard prefers valid rich and falls back from invalid rich",
		[]
	{
		MemoryClipboardBackend clipboardBackend;
		cui::richtext::clipboard::ScopedBackendOverride clipboardScope(
			clipboardBackend);
		RichTextCharacterStyle richStyle;
		richStyle.FontWeight = DWRITE_FONT_WEIGHT_BOLD;
		const auto rich = RichTextDocumentFragment::FromPlainText(
			L"rich", richStyle);
		const auto encoded = cui::richtext::clipboard::Encode(rich);
		CUI_EXPECT_TRUE(encoded.has_value());

		cui::richtext::clipboard::DataObject data;
		data.PlainText = L"plain";
		data.Attributed = encoded;
		CUI_EXPECT_TRUE(cui::richtext::clipboard::Publish(nullptr, data));
		RichTextBox preferred;
		CUI_EXPECT_TRUE(preferred.Paste());
		CUI_EXPECT_EQ(std::wstring(L"rich"), preferred.Text);
		ExpectWeight(Flatten(preferred), 0, DWRITE_FONT_WEIGHT_BOLD);

		auto unknownVersion = *encoded;
		unknownVersion[8] = 8;
		data.PlainText = L"version fallback";
		data.Attributed = std::move(unknownVersion);
		CUI_EXPECT_TRUE(cui::richtext::clipboard::Publish(nullptr, data));
		RichTextBox versionFallback;
		CUI_EXPECT_TRUE(versionFallback.Paste());
		CUI_EXPECT_EQ(std::wstring(L"version fallback"),
			versionFallback.Text);

		auto badChecksum = *encoded;
		badChecksum.back() ^= 0x33;
		data.PlainText = L"checksum fallback";
		data.Attributed = std::move(badChecksum);
		CUI_EXPECT_TRUE(cui::richtext::clipboard::Publish(nullptr, data));
		RichTextBox checksumFallback;
		CUI_EXPECT_TRUE(checksumFallback.Paste());
		CUI_EXPECT_EQ(std::wstring(L"checksum fallback"),
			checksumFallback.Text);
	});

	runner.Add(
		"RichText clipboard imports LineBreak and paragraph topology with fresh identities",
		[]
	{
		MemoryClipboardBackend clipboardBackend;
		cui::richtext::clipboard::ScopedBackendOverride clipboardScope(
			clipboardBackend);
		auto sourceDocument = std::make_unique<FlowDocument>();
		auto& first = sourceDocument->Blocks.AddParagraph();
		first.Inlines.AddRun(L"L");
		auto& sourceLineBreak = first.Inlines.AddLineBreak();
		first.Inlines.AddRun(L"M");
		auto& second = sourceDocument->Blocks.AddParagraph();
		auto bold = std::make_unique<Bold>();
		bold->Inlines.AddRun(L"N");
		second.Inlines.Add(std::move(bold));

		RichTextBox source;
		source.SetDocument(std::move(sourceDocument));
		CUI_EXPECT_EQ(std::wstring(L"L\r\nM\r\nN"), source.Text);
		source.SelectAll();
		CUI_EXPECT_TRUE(source.Copy());

		RichTextBox target;
		target.Text = L"xy";
		target.Select(1, 0);
		CUI_EXPECT_TRUE(target.Paste());
		CUI_EXPECT_EQ(std::wstring(L"xL\r\nM\r\nNy"), target.Text);
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
			target.GetDocument().Blocks.Count()));
		auto* pastedFirst = dynamic_cast<Paragraph*>(
			target.GetDocument().Blocks.At(0));
		auto* pastedSecond = dynamic_cast<Paragraph*>(
			target.GetDocument().Blocks.At(1));
		CUI_EXPECT_TRUE(pastedFirst != nullptr);
		CUI_EXPECT_TRUE(pastedSecond != nullptr);
		bool foundLineBreak = false;
		if (pastedFirst)
		{
			for (const auto& child : pastedFirst->Inlines.Items())
				if (dynamic_cast<LineBreak*>(child.get()))
					foundLineBreak = true;
		}
		CUI_EXPECT_TRUE(foundLineBreak);
		CUI_EXPECT_TRUE(pastedSecond
			&& dynamic_cast<Bold*>(pastedSecond->Inlines.At(0)) != nullptr);
		const auto pasted = target.GetDocument().Flatten();
		const auto lineBreakSpan = std::find_if(
			pasted.StructureSpans.begin(), pasted.StructureSpans.end(),
			[](const auto& span)
			{
				return span.Path.back().Kind
					== RichTextStructureKind::LineBreak;
			});
		CUI_EXPECT_TRUE(lineBreakSpan != pasted.StructureSpans.end());
		if (lineBreakSpan != pasted.StructureSpans.end())
			CUI_EXPECT_TRUE(lineBreakSpan->Path.back().Id
				!= sourceLineBreak.GetRichTextStructureId());
		CUI_EXPECT_TRUE(target.CanUndo());
		target.Undo();
		CUI_EXPECT_EQ(std::wstring(L"xy"), target.Text);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			target.GetDocument().Blocks.Count()));
		target.Redo();
		CUI_EXPECT_EQ(std::wstring(L"xL\r\nM\r\nNy"), target.Text);

		RichTextBox limited;
		limited.MaxLength = 3;
		CUI_EXPECT_TRUE(limited.Paste());
		CUI_EXPECT_EQ(std::wstring(L"L\r\n"), limited.Text);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			limited.GetDocument().Blocks.Count()));
		auto* limitedParagraph = dynamic_cast<Paragraph*>(
			limited.GetDocument().Blocks.At(0));
		CUI_EXPECT_TRUE(limitedParagraph != nullptr);
		CUI_EXPECT_TRUE(limitedParagraph
			&& dynamic_cast<LineBreak*>(
				limitedParagraph->Inlines.At(1)) != nullptr);
	});

	runner.Add(
		"RichText clipboard paste truncates only at complete text element boundaries",
		[]
	{
		MemoryClipboardBackend clipboardBackend;
		cui::richtext::clipboard::ScopedBackendOverride clipboardScope(
			clipboardBackend);
		RichTextCharacterStyle style;
		style.Underline = true;
		const auto fragment = RichTextDocumentFragment::FromPlainText(
			L"A\r\n\xD83D\xDE00" L"B", style);
		CUI_EXPECT_TRUE(cui::richtext::clipboard::Publish(nullptr, fragment));

		RichTextBox limited;
		limited.MaxLength = 4;
		CUI_EXPECT_TRUE(limited.Paste());
		CUI_EXPECT_EQ(std::wstring(L"A\r\n"), limited.Text);
		CUI_EXPECT_EQ(3, limited.GetCaretIndex());
		ExpectUnderline(Flatten(limited), 2, true);
		CUI_EXPECT_TRUE(limited.CanUndo());
		limited.Undo();
		CUI_EXPECT_TRUE(limited.Text.empty());
		CUI_EXPECT_FALSE(limited.CanUndo());
		CUI_EXPECT_TRUE(limited.CanRedo());
		limited.Redo();
		CUI_EXPECT_EQ(std::wstring(L"A\r\n"), limited.Text);

		RichTextBox includesSurrogate;
		includesSurrogate.MaxLength = 5;
		CUI_EXPECT_TRUE(includesSurrogate.Paste());
		CUI_EXPECT_EQ(std::wstring(
			L"A\r\n\xD83D\xDE00"), includesSurrogate.Text);
		CUI_EXPECT_EQ(5, includesSurrogate.GetCaretIndex());
		ExpectUnderline(Flatten(includesSurrogate), 4, true);

		const auto composed = RichTextDocumentFragment::FromPlainText(
			L"Ae\u0301B", style);
		CUI_EXPECT_TRUE(cui::richtext::clipboard::Publish(
			nullptr, composed));
		RichTextBox excludesPartialComposition;
		excludesPartialComposition.MaxLength = 2;
		CUI_EXPECT_TRUE(excludesPartialComposition.Paste());
		CUI_EXPECT_EQ(std::wstring(L"A"),
			excludesPartialComposition.Text);
		RichTextBox includesComposition;
		includesComposition.MaxLength = 3;
		CUI_EXPECT_TRUE(includesComposition.Paste());
		CUI_EXPECT_EQ(std::wstring(L"Ae\u0301"),
			includesComposition.Text);
		ExpectUnderline(Flatten(includesComposition), 2, true);
	});

	runner.Add(
		"RichText clipboard paste rolls back when document callback throws",
		[]
	{
		MemoryClipboardBackend clipboardBackend;
		cui::richtext::clipboard::ScopedBackendOverride clipboardScope(
			clipboardBackend);
		RichTextCharacterStyle style;
		style.FontWeight = DWRITE_FONT_WEIGHT_BOLD;
		const auto fragment = RichTextDocumentFragment::FromPlainText(
			L"new", style);
		CUI_EXPECT_TRUE(cui::richtext::clipboard::Publish(nullptr, fragment));

		RichTextBox box;
		box.Text = L"old";
		box.SelectAll();
		auto throwing = box.GetDocument().Changed.Subscribe(
			[](FlowDocument*)
			{
				throw std::runtime_error("expected paste callback failure");
			});
		bool threw = false;
		try
		{
			(void)box.Paste();
		}
		catch (const std::runtime_error&)
		{
			threw = true;
		}
		CUI_EXPECT_TRUE(threw);
		CUI_EXPECT_EQ(std::wstring(L"old"), box.Text);
		CUI_EXPECT_EQ(0, box.GetSelectionStart());
		CUI_EXPECT_EQ(3, box.GetSelectionLength());
		CUI_EXPECT_FALSE(box.CanUndo());
		CUI_EXPECT_FALSE(box.CanRedo());
	});

	runner.Add(
		"RichTextBox document virtual chunks preserve complete Unicode text elements",
		[]
	{
		std::wstring crlf(255, L'a');
		crlf.append(L"\r\nrest");
		CUI_EXPECT_EQ(257ULL, static_cast<unsigned long long>(
			CuiTextEdit::ExpandChunkToTextElementBoundary(
				crlf, 0, 256)));

		std::wstring surrogate(255, L'a');
		surrogate.append(L"\xD83D\xDE00" L"rest");
		CUI_EXPECT_EQ(257ULL, static_cast<unsigned long long>(
			CuiTextEdit::ExpandChunkToTextElementBoundary(
				surrogate, 0, 256)));
		CUI_EXPECT_EQ(4ULL, static_cast<unsigned long long>(
			CuiTextEdit::ExpandChunkToTextElementBoundary(
				surrogate, 257, 4)));

		std::wstring combining(255, L'a');
		combining.append(L"e\u0301rest");
		CUI_EXPECT_EQ(257ULL, static_cast<unsigned long long>(
			CuiTextEdit::ExpandChunkToTextElementBoundary(
				combining, 0, 256)));

		std::wstring family(255, L'a');
		family.append(
			L"\xD83D\xDC69\u200D\xD83D\xDC69\u200D"
			L"\xD83D\xDC67\u200D\xD83D\xDC66rest");
		CUI_EXPECT_EQ(266ULL, static_cast<unsigned long long>(
			CuiTextEdit::ExpandChunkToTextElementBoundary(
				family, 0, 256)));
	});

	runner.Add(
		"RichText layout keeps paragraph alignment boundaries and continuous Justify",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& left = document->Blocks.AddParagraph();
		left.Inlines.AddRun(L"A");
		auto& center = document->Blocks.AddParagraph();
		center.SetTextAlignment(::TextAlignment::Center);
		center.Inlines.AddRun(L"B");
		auto& right = document->Blocks.AddParagraph();
		right.SetTextAlignment(::TextAlignment::Right);
		right.Inlines.AddRun(L"C");
		RichTextBox box;
		box.SetDocument(std::move(document));
		const auto blocks = RichTextBoxDocumentTestAccess::
			RebuildDocumentLayoutChunks(box);
		CUI_EXPECT_EQ(3ULL,
			static_cast<unsigned long long>(blocks.size()));
		if (blocks.size() == 3)
		{
			CUI_EXPECT_EQ(::TextAlignment::Left, blocks[0].Alignment);
			CUI_EXPECT_EQ(::TextAlignment::Center, blocks[1].Alignment);
			CUI_EXPECT_EQ(::TextAlignment::Right, blocks[2].Alignment);
			CUI_EXPECT_EQ(0ULL,
				static_cast<unsigned long long>(blocks[0].Start));
			CUI_EXPECT_EQ(3ULL,
				static_cast<unsigned long long>(blocks[1].Start));
			CUI_EXPECT_EQ(6ULL,
				static_cast<unsigned long long>(blocks[2].Start));
		}

		auto justifiedDocument = std::make_unique<FlowDocument>();
		justifiedDocument->SetTextAlignment(::TextAlignment::Justify);
		auto& justifiedParagraph =
			justifiedDocument->Blocks.AddParagraph();
		justifiedParagraph.Inlines.AddRun(std::wstring(12000, L'x'));
		RichTextBox justified;
		justified.SetDocument(std::move(justifiedDocument));
		const auto justifiedBlocks = RichTextBoxDocumentTestAccess::
			ProfileDocumentVisualLines(justified, 180.0f, FLT_MAX);
		CUI_EXPECT_EQ(1ULL,
			static_cast<unsigned long long>(justifiedBlocks.size()));
		if (!justifiedBlocks.empty())
		{
			CUI_EXPECT_EQ(::TextAlignment::Justify,
				justifiedBlocks.front().Alignment);
			CUI_EXPECT_FALSE(justifiedBlocks.front().IsSingleVisualLine);
		}
	});

	runner.Add(
		"RichText virtual layout chunks preserve paragraph formatting context",
		[]
	{
		const std::wstring paragraphs = L"A\r\nB\r\nC";
		const auto firstChunk = CuiTextEdit::FindSafeTextLayoutChunk(
			paragraphs, 0, 3);
		const auto secondChunk = CuiTextEdit::FindSafeTextLayoutChunk(
			paragraphs, firstChunk.Length, 3);
		const auto finalChunk = CuiTextEdit::FindSafeTextLayoutChunk(
			paragraphs, firstChunk.Length + secondChunk.Length, 3);
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			firstChunk.Length));
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			firstChunk.LayoutLength));
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			secondChunk.Length));
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			secondChunk.LayoutLength));
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			finalChunk.Length));
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			finalChunk.LayoutLength));

		const auto longParagraph = CuiTextEdit::FindSafeTextLayoutChunk(
			L"abcdef", 0, 2);
		CUI_EXPECT_EQ(6ULL, static_cast<unsigned long long>(
			longParagraph.Length));
		CUI_EXPECT_EQ(6ULL, static_cast<unsigned long long>(
			longParagraph.LayoutLength));
		const auto expandedParagraph = CuiTextEdit::FindSafeTextLayoutChunk(
			L"abcd\r\nef", 0, 2);
		CUI_EXPECT_EQ(6ULL, static_cast<unsigned long long>(
			expandedParagraph.Length));
		CUI_EXPECT_EQ(4ULL, static_cast<unsigned long long>(
			expandedParagraph.LayoutLength));
		const auto trailingBreak = CuiTextEdit::FindSafeTextLayoutChunk(
			L"A\r\n", 0, 1);
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			trailingBreak.Length));
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			trailingBreak.LayoutLength));

		RichTextBox box;
		std::wstring blockText(255, L'a');
		blockText.append(L"\r\n");
		blockText.append(300, L'b');
		const auto blocks =
			RichTextBoxDocumentTestAccess::RebuildLayoutChunks(box, blockText);
		CUI_EXPECT_EQ(2ULL,
			static_cast<unsigned long long>(blocks.size()));
		if (blocks.size() == 2)
		{
			CUI_EXPECT_EQ(0ULL, static_cast<unsigned long long>(
				blocks[0].Start));
			CUI_EXPECT_EQ(257ULL, static_cast<unsigned long long>(
				blocks[0].Length));
			CUI_EXPECT_EQ(255ULL, static_cast<unsigned long long>(
				blocks[0].LayoutLength));
			CUI_EXPECT_EQ(257ULL, static_cast<unsigned long long>(
				blocks[1].Start));
			CUI_EXPECT_EQ(300ULL, static_cast<unsigned long long>(
				blocks[1].Length));
			CUI_EXPECT_EQ(300ULL, static_cast<unsigned long long>(
				blocks[1].LayoutLength));
		}
		const auto unbroken =
			RichTextBoxDocumentTestAccess::RebuildLayoutChunks(
				box, std::wstring(1024, L'x'));
		CUI_EXPECT_EQ(1ULL,
			static_cast<unsigned long long>(unbroken.size()));
		if (!unbroken.empty())
			CUI_EXPECT_EQ(1024ULL, static_cast<unsigned long long>(
				unbroken.front().LayoutLength));

		auto styledDocument = std::make_unique<FlowDocument>();
		auto& styledFirst = styledDocument->Blocks.AddParagraph();
		styledFirst.SetFontSize(40.0);
		auto styledFirstRun = std::make_unique<Run>(
			std::wstring(255, L'a'));
		styledFirstRun->SetFontSize(12.0);
		styledFirst.Inlines.Add(std::move(styledFirstRun));
		auto& styledSecond = styledDocument->Blocks.AddParagraph();
		styledSecond.Inlines.Add(std::make_unique<Run>(
			std::wstring(300, L'b')));
		RichTextBox styledBox;
		styledBox.SetDocument(std::move(styledDocument));
		const auto measured = RichTextBoxDocumentTestAccess::
			MeasureVirtualAndContinuousHeight(styledBox, 10000.0f);
		CUI_EXPECT_NEAR(measured.second, measured.first, 0.001);
		const auto cachedMetrics = RichTextBoxDocumentTestAccess::
			MeasureAllBlockMetrics(styledBox, 10000.0f, 20.0f);
		CUI_EXPECT_TRUE(cachedMetrics.first > 0.0f);
		CUI_EXPECT_EQ(0ULL, static_cast<unsigned long long>(
			cachedMetrics.second));

		D2DGraphics::InitOptions options;
		options.kind = D2DGraphics::SurfaceKind::Offscreen;
		options.width = 320;
		options.height = 160;
		D2DGraphics graphics(options);
		auto metrics = [&](const std::wstring& text,
			std::optional<DWRITE_TEXT_RANGE> largeStyle = std::nullopt)
		{
			Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
			layout.Attach(graphics.CreateStringLayout(text, 300.0f, 1000.0f));
			DWRITE_TEXT_METRICS value{};
			CUI_EXPECT_TRUE(layout != nullptr);
			if (layout && largeStyle)
				CUI_EXPECT_TRUE(SUCCEEDED(
					layout->SetFontSize(40.0f, *largeStyle)));
			CUI_EXPECT_TRUE(layout && SUCCEEDED(layout->GetMetrics(&value)));
			UINT32 lineCount = 0;
			if (layout)
				(void)layout->GetLineMetrics(nullptr, 0, &lineCount);
			return std::pair<float, UINT32>{ value.height, lineCount };
		};
		const auto aWithSentinel = metrics(
			L"A\u200B", DWRITE_TEXT_RANGE{ 1, 1 });
		const auto emptyWithSentinel = metrics(
			L"\u200B", DWRITE_TEXT_RANGE{ 0, 1 });
		const auto b = metrics(L"B");
		const auto whole = metrics(
			L"A\r\nB", DWRITE_TEXT_RANGE{ 1, 2 });
		const auto emptyWhole = metrics(
			L"\r\nB", DWRITE_TEXT_RANGE{ 0, 2 });
		CUI_EXPECT_NEAR(whole.first,
			aWithSentinel.first + b.first, 0.001);
		CUI_EXPECT_EQ(whole.second,
			aWithSentinel.second + b.second);
		CUI_EXPECT_NEAR(emptyWhole.first,
			emptyWithSentinel.first + b.first, 0.001);
		CUI_EXPECT_EQ(emptyWhole.second,
			emptyWithSentinel.second + b.second);
	});

	runner.Add(
		"RichTextBox editing commands expose routed identities and class gestures",
		[]
	{
		CUI_EXPECT_EQ(std::wstring(L"ToggleBold"),
			EditingCommands::ToggleBold().Name());
		CUI_EXPECT_EQ(std::wstring(L"AlignJustify"),
			EditingCommands::AlignJustify().Name());
		CUI_EXPECT_EQ(std::wstring(L"ResetFormat"),
			EditingCommands::ResetFormat().Name());
		CUI_EXPECT_EQ(std::wstring(L"DeletePreviousWord"),
			EditingCommands::DeletePreviousWord().Name());
		CUI_EXPECT_EQ(std::wstring(L"MoveRightByCharacter"),
			EditingCommands::MoveRightByCharacter().Name());
		CUI_EXPECT_EQ(std::wstring(L"SelectLeftByCharacter"),
			EditingCommands::SelectLeftByCharacter().Name());
		CUI_EXPECT_EQ(std::wstring(L"MoveToLineStart"),
			EditingCommands::MoveToLineStart().Name());
		CUI_EXPECT_EQ(std::wstring(L"MoveDownByLine"),
			EditingCommands::MoveDownByLine().Name());
		CUI_EXPECT_EQ(std::wstring(L"MoveDownByPage"),
			EditingCommands::MoveDownByPage().Name());
		CUI_EXPECT_EQ(std::wstring(L"SelectUpByPage"),
			EditingCommands::SelectUpByPage().Name());
		CUI_EXPECT_EQ(std::wstring(L"MoveDownByParagraph"),
			EditingCommands::MoveDownByParagraph().Name());
		CUI_EXPECT_EQ(std::wstring(L"SelectToDocumentEnd"),
			EditingCommands::SelectToDocumentEnd().Name());
		KeyGesture bracketGesture;
		std::wstring gestureError;
		CUI_EXPECT_TRUE(TryParseKeyGesture(
			L"Ctrl+OemOpenBrackets", bracketGesture, &gestureError));
		CUI_EXPECT_EQ(Key::OemOpenBrackets, bracketGesture.Key);
		CUI_EXPECT_EQ(ModifierKeys::Control, bracketGesture.Modifiers);
		CUI_EXPECT_EQ(std::wstring(L"Ctrl+OemOpenBrackets"),
			FormatKeyGesture(bracketGesture));

		RichTextBox editor;
		editor.Text = L"plain";
		editor.Select(0, 5);
		CUI_EXPECT_TRUE(RoutedCommandManager::CanExecute(
			EditingCommands::ToggleBold(), editor));
		CUI_EXPECT_TRUE(ProcessCommandGesture(editor, Key::B));
		ExpectSelectionWeight(editor,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_BOLD);
		CUI_EXPECT_TRUE(editor.CanUndo());
		editor.Undo();
		CUI_EXPECT_FALSE(editor.CanUndo());
		ExpectSelectionWeight(editor,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_NORMAL);

		Window host;
		auto hostedOwner = std::make_unique<RichTextBox>();
		auto* hosted = hostedOwner.get();
		hosted->Text = L"window route";
		hosted->SelectAll();
		hosted->Focusable = true;
		CUI_EXPECT_TRUE(host.SetVisualContent(
			std::move(hostedOwner)) == hosted);
		host.SetKeyboardFocus(hosted, false);
		CUI_EXPECT_TRUE(host.GetKeyboardFocusedElement() == hosted);
		InputReport routedInput;
		routedInput.Kind = InputReportKind::KeyDown;
		routedInput.Key = Key::U;
		routedInput.Modifiers = ModifierKeys::Control;
		CUI_EXPECT_TRUE(cui::framework::InputAccess::DispatchInput(
			host, routedInput));
		ExpectSelectionUnderline(*hosted,
			TextSelectionPropertyValueKind::Value, true);

		CUI_EXPECT_FALSE(ProcessCommandGesture(
			editor, Key::B,
			ModifierKeys::Control | ModifierKeys::Shift));
		ExpectSelectionWeight(editor,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_NORMAL);

		RichTextBox overridden;
		overridden.Text = L"x";
		overridden.SelectAll();
		const RoutedCommand localCommand(L"LocalBoldOverride");
		int localExecutions = 0;
		CommandBinding localHandler;
		localHandler.Command = localCommand;
		localHandler.Executed = [&](Control*, ExecutedRoutedEventArgs&)
		{
			++localExecutions;
		};
		auto localLifetime = overridden.AddCommandBinding(
			std::move(localHandler));
		CUI_EXPECT_TRUE(localLifetime.Connected());
		KeyBinding localInput;
		localInput.Command = localCommand;
		localInput.Gesture.Key = Key::B;
		localInput.Gesture.Modifiers = ModifierKeys::Control;
		CUI_EXPECT_TRUE(overridden.AddInputBinding(std::move(localInput)));
		CUI_EXPECT_TRUE(ProcessCommandGesture(overridden, Key::B));
		CUI_EXPECT_EQ(1, localExecutions);
		ExpectSelectionWeight(overridden,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_NORMAL);

		RichTextBox commandOverride;
		commandOverride.Text = L"local command";
		commandOverride.SelectAll();
		int overriddenExecutions = 0;
		CommandBinding sameCommand;
		sameCommand.Command = EditingCommands::ToggleItalic();
		sameCommand.CanExecute = [](
			Control*, CanExecuteRoutedEventArgs& args)
		{
			args.CanExecute = true;
		};
		sameCommand.Executed = [&out = overriddenExecutions](
			Control*, ExecutedRoutedEventArgs&)
		{
			++out;
		};
		auto sameCommandLifetime = commandOverride.AddCommandBinding(
			std::move(sameCommand));
		CUI_EXPECT_TRUE(sameCommandLifetime.Connected());
		CUI_EXPECT_TRUE(ProcessCommandGesture(commandOverride, Key::I));
		CUI_EXPECT_EQ(1, overriddenExecutions);
		const auto italicResult = commandOverride.GetSelection().GetPropertyValue(
			TextElement::FontStyleProperty());
		CUI_EXPECT_EQ(TextSelectionPropertyValueKind::Value,
			italicResult.Kind);
		DWRITE_FONT_STYLE italicValue = DWRITE_FONT_STYLE_ITALIC;
		CUI_EXPECT_TRUE(italicResult.Value.TryGet(italicValue));
		CUI_EXPECT_EQ(DWRITE_FONT_STYLE_NORMAL, italicValue);
		CUI_EXPECT_FALSE(commandOverride.CanUndo());
	});

	runner.Add(
		"RichTextBox Home End navigate visual lines through WPF routed commands",
		[]
	{
		Window host;
		auto owned = std::make_unique<RichTextBox>();
		auto* box = owned.get();
		box->Text = L"alpha beta gamma e\u0301lan delta epsilon zeta eta theta";
		box->Width = 96.0;
		box->Height = 160.0;
		box->Focusable = true;
		CUI_EXPECT_TRUE(host.SetVisualContent(std::move(owned)) == box);
		box->Arrange(cui::core::Rect{ 0.0f, 0.0f, 96.0f, 160.0f });
		host.SetKeyboardFocus(box, false);

		int probe = -1;
		int visualStart = -1;
		int visualEnd = -1;
		for (int index = 1; index + 1 < static_cast<int>(box->Text.size());
			++index)
		{
			const int start = RichTextBoxDocumentTestAccess::VisualLineBoundary(
				*box, index, false);
			const int end = RichTextBoxDocumentTestAccess::VisualLineBoundary(
				*box, index, true);
			if (start > 0 && start < index
				&& end > index && end < static_cast<int>(box->Text.size()))
			{
				probe = index;
				visualStart = start;
				visualEnd = end;
				break;
			}
		}
		CUI_EXPECT_TRUE(probe > 0);
		if (probe <= 0) return;

		int selectionEvents = 0;
		auto selectionChanged = box->SelectionChanged.Subscribe(
			[&](Control*, SelectionChangedEventArgs&) { ++selectionEvents; });
		box->SetCaretIndex(probe);
		int before = selectionEvents;
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Home, ModifierKeys::None));
		CUI_EXPECT_EQ(visualStart, box->GetCaretIndex());
		CUI_EXPECT_EQ(before + 1, selectionEvents);

		box->SetCaretIndex(probe);
		before = selectionEvents;
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::End, ModifierKeys::None));
		CUI_EXPECT_EQ(visualEnd, box->GetCaretIndex());
		CUI_EXPECT_EQ(before + 1, selectionEvents);

		box->SetCaretIndex(probe);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Home, ModifierKeys::Shift));
		CUI_EXPECT_EQ(visualStart, box->GetCaretIndex());
		CUI_EXPECT_EQ(probe - visualStart, box->GetSelectionLength());

		box->SetCaretIndex(probe);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::End,
			ModifierKeys::Control | ModifierKeys::Shift));
		CUI_EXPECT_EQ(static_cast<int>(box->Text.size()),
			box->GetCaretIndex());
		CUI_EXPECT_EQ(static_cast<int>(box->Text.size()) - probe,
			box->GetSelectionLength());

		box->SetCaretIndex(probe);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Home, ModifierKeys::Control));
		CUI_EXPECT_EQ(0, box->GetCaretIndex());
		CUI_EXPECT_EQ(0, box->GetSelectionLength());

		RichTextBox logicalFallback;
		logicalFallback.Text = L"first\r\nsecond";
		logicalFallback.SetCaretIndex(10);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			logicalFallback, Key::Home, ModifierKeys::None));
		CUI_EXPECT_EQ(7, logicalFallback.GetCaretIndex());
	});

	runner.Add(
		"RichTextBox paragraph commands distinguish Blocks from inline hard breaks",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& first = document->Blocks.AddParagraph();
		first.Inlines.AddRun(L"one\r\ntwo");
		first.Inlines.Add(std::make_unique<LineBreak>());
		first.Inlines.AddRun(L"three");
		auto& second = document->Blocks.AddParagraph();
		second.Inlines.AddRun(L"second");
		document->Blocks.AddParagraph();
		auto& fourth = document->Blocks.AddParagraph();
		fourth.Inlines.AddRun(L"fourth");

		RichTextBox box;
		box.SetDocument(std::move(document));
		CUI_EXPECT_EQ(std::wstring(
			L"one\r\ntwo\r\nthree\r\nsecond\r\n\r\nfourth"), box.Text);
		CUI_EXPECT_EQ(4ULL, static_cast<unsigned long long>(
			box.GetDocument().Blocks.Count()));

		box.SetCaretIndex(4); // CRLF inside Run, not a Paragraph boundary.
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Down, ModifierKeys::Control));
		CUI_EXPECT_EQ(17, box.GetCaretIndex());
		box.SetCaretIndex(10); // After explicit LineBreak, still first Paragraph.
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Up, ModifierKeys::Control));
		CUI_EXPECT_EQ(0, box.GetCaretIndex());

		box.SetCaretIndex(17);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Down, ModifierKeys::Control));
		CUI_EXPECT_EQ(25, box.GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Down, ModifierKeys::Control));
		CUI_EXPECT_EQ(27, box.GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Down, ModifierKeys::Control));
		CUI_EXPECT_EQ(33, box.GetCaretIndex());

		box.SetCaretIndex(27);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Up,
			ModifierKeys::Control | ModifierKeys::Shift));
		CUI_EXPECT_EQ(25, box.GetCaretIndex());
		CUI_EXPECT_EQ(2, box.GetSelectionLength());

		box.Select(0, 17);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Down, ModifierKeys::Control));
		CUI_EXPECT_EQ(17, box.GetCaretIndex());
		CUI_EXPECT_EQ(0, box.GetSelectionLength());
	});

	runner.Add(
		"RichTextBox vertical commands use actual rich visual line heights",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& largeParagraph = document->Blocks.AddParagraph();
		auto& large = largeParagraph.Inlines.AddRun(L"A");
		large.SetFontSize(48.0);
		auto& ordinaryParagraph = document->Blocks.AddParagraph();
		ordinaryParagraph.Inlines.AddRun(L"B");
		auto& finalParagraph = document->Blocks.AddParagraph();
		finalParagraph.Inlines.AddRun(L"C");

		Window host;
		auto owned = std::make_unique<RichTextBox>();
		auto* box = owned.get();
		box->SetDocument(std::move(document));
		box->Width = 240.0;
		box->Height = 180.0;
		box->Focusable = true;
		CUI_EXPECT_TRUE(host.SetVisualContent(std::move(owned)) == box);
		box->Arrange(cui::core::Rect{ 0.0f, 0.0f, 240.0f, 180.0f });
		host.SetKeyboardFocus(box, false);
		CUI_EXPECT_EQ(std::wstring(L"A\r\nB\r\nC"), box->Text);

		box->SetCaretIndex(0);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_EQ(3, box->GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_EQ(6, box->GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Up, ModifierKeys::None));
		CUI_EXPECT_EQ(3, box->GetCaretIndex());

		box->SetCaretIndex(6);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Up, ModifierKeys::Shift));
		CUI_EXPECT_EQ(3, box->GetCaretIndex());
		CUI_EXPECT_EQ(3, box->GetSelectionLength());

		box->Select(0, 4);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_EQ(7, box->GetCaretIndex());
		CUI_EXPECT_EQ(0, box->GetSelectionLength());
	});

	runner.Add(
		"RichTextBox vertical commands preserve and reset WPF suggested X",
		[]
	{
		Window host;
		auto owned = std::make_unique<RichTextBox>();
		auto* box = owned.get();
		box->Text = L"abcdefghij\r\nx\r\nabcdefghij";
		box->Width = 320.0;
		box->Height = 160.0;
		box->Focusable = true;
		CUI_EXPECT_TRUE(host.SetVisualContent(std::move(owned)) == box);
		box->Arrange(cui::core::Rect{ 0.0f, 0.0f, 320.0f, 160.0f });
		host.SetKeyboardFocus(box, false);

		box->SetCaretIndex(8);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_EQ(13, box->GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_EQ(23, box->GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Up, ModifierKeys::None));
		CUI_EXPECT_EQ(13, box->GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Up, ModifierKeys::None));
		CUI_EXPECT_EQ(8, box->GetCaretIndex());

		// End does not move this caret, but WPF still discards the old desired
		// column. The next Down therefore starts at the short line's own end.
		box->SetCaretIndex(8);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_EQ(13, box->GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::End, ModifierKeys::None));
		CUI_EXPECT_EQ(13, box->GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_EQ(16, box->GetCaretIndex());
	});

	runner.Add(
		"RichTextBox vertical commands honor wrapped line caret affinity",
		[]
	{
		Window host;
		auto owned = std::make_unique<RichTextBox>();
		auto* box = owned.get();
		box->Text = std::wstring(96, L'W');
		box->Width = 96.0;
		box->Height = 600.0;
		box->Focusable = true;
		CUI_EXPECT_TRUE(host.SetVisualContent(std::move(owned)) == box);
		box->Arrange(cui::core::Rect{ 0.0f, 0.0f, 96.0f, 600.0f });
		host.SetKeyboardFocus(box, false);

		const auto lines =
			RichTextBoxDocumentTestAccess::ContinuousVisualLines(*box);
		CUI_EXPECT_TRUE(lines.size() >= 3);
		if (lines.size() < 3) return;
		const int firstWrap = static_cast<int>(lines[0].End);
		CUI_EXPECT_TRUE(firstWrap > 0
			&& firstWrap + 1 < static_cast<int>(box->Text.size()));

		// Moving Left onto a wrap boundary gives the caret backward
		// affinity: it is visually at the end of the first line, not the
		// start of the second line, despite sharing the same UTF-16 offset.
		box->SetCaretIndex(firstWrap + 1);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Left, ModifierKeys::None));
		CUI_EXPECT_EQ(firstWrap, box->GetCaretIndex());
		CUI_EXPECT_NEAR(lines[0].Top + box->Padding.Top,
			RichTextBoxDocumentTestAccess::CaretTop(*box), 0.5f);
		D2D1_RECT_F textInputCaret{};
		CUI_EXPECT_TRUE(box->TryGetTextInputCaretRect(textInputCaret));
		CUI_EXPECT_NEAR(lines[0].Top + box->Padding.Top,
			textInputCaret.top, 0.5f);

		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_NEAR(lines[1].Top + box->Padding.Top,
			RichTextBoxDocumentTestAccess::CaretTop(*box), 0.5f);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Up, ModifierKeys::None));
		CUI_EXPECT_EQ(firstWrap, box->GetCaretIndex());
		CUI_EXPECT_NEAR(lines[0].Top + box->Padding.Top,
			RichTextBoxDocumentTestAccess::CaretTop(*box), 0.5f);
	});

	runner.Add(
		"RichTextBox virtual vertical commands cross one visual block seam",
		[]
	{
		Window host;
		auto owned = std::make_unique<RichTextBox>();
		auto* box = owned.get();
		box->Text = std::wstring(1400, L'W');
		box->Width = 96.0;
		box->Height = 600.0;
		box->Focusable = true;
		RichTextBoxDocumentTestAccess::ForceVirtualizedLayout(*box);
		CUI_EXPECT_TRUE(host.SetVisualContent(std::move(owned)) == box);
		box->Arrange(cui::core::Rect{ 0.0f, 0.0f, 96.0f, 600.0f });
		host.SetKeyboardFocus(box, false);

		const auto lines =
			RichTextBoxDocumentTestAccess::VirtualVisualLines(*box);
		CUI_EXPECT_TRUE(lines.size() >= 3);
		if (lines.size() < 3) return;
		const int firstSeam = static_cast<int>(lines[0].End);
		CUI_EXPECT_EQ(lines[1].Start, lines[0].End);

		box->SetCaretIndex(firstSeam + 1);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Left, ModifierKeys::None));
		CUI_EXPECT_EQ(firstSeam, box->GetCaretIndex());
		CUI_EXPECT_NEAR(lines[0].Top + box->Padding.Top,
			RichTextBoxDocumentTestAccess::CaretTop(*box), 0.5f);

		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_NEAR(lines[1].Top + box->Padding.Top,
			RichTextBoxDocumentTestAccess::CaretTop(*box), 0.5f);
	});

	runner.Add(
		"RichTextBox vertical commands traverse empty visual lines",
		[]
	{
		Window host;
		auto owned = std::make_unique<RichTextBox>();
		auto* box = owned.get();
		box->Text = L"A\r\n\r\nB";
		box->Width = 180.0;
		box->Height = 160.0;
		box->Focusable = true;
		CUI_EXPECT_TRUE(host.SetVisualContent(std::move(owned)) == box);
		box->Arrange(cui::core::Rect{ 0.0f, 0.0f, 180.0f, 160.0f });
		host.SetKeyboardFocus(box, false);

		box->SetCaretIndex(0);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_EQ(3, box->GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Down, ModifierKeys::None));
		CUI_EXPECT_EQ(5, box->GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Up, ModifierKeys::None));
		CUI_EXPECT_EQ(3, box->GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::Up, ModifierKeys::None));
		CUI_EXPECT_EQ(0, box->GetCaretIndex());
	});

	runner.Add(
		"RichTextBox character commands navigate complete Unicode text elements",
		[]
	{
		RichTextBox box;
		box.Text = L"Ae\u0301\xD83D\xDE00" L"B";
		box.SetCaretIndex(1);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Right, ModifierKeys::None));
		CUI_EXPECT_EQ(3, box.GetCaretIndex());
		CUI_EXPECT_EQ(0, box.GetSelectionLength());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Right, ModifierKeys::Shift));
		CUI_EXPECT_EQ(5, box.GetCaretIndex());
		CUI_EXPECT_EQ(2, box.GetSelectionLength());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Left, ModifierKeys::None));
		CUI_EXPECT_EQ(3, box.GetCaretIndex());
		CUI_EXPECT_EQ(0, box.GetSelectionLength());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Left, ModifierKeys::None));
		CUI_EXPECT_EQ(1, box.GetCaretIndex());

		box.Select(2, 1); // Raw endpoint inside e + combining acute.
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			box, Key::Left, ModifierKeys::None));
		CUI_EXPECT_EQ(1, box.GetCaretIndex());
		CUI_EXPECT_EQ(0, box.GetSelectionLength());
	});

	runner.Add(
		"RichTextBox Page commands share routed direct and selection semantics",
		[]
	{
		std::wstring text;
		for (int line = 0; line < 20; ++line)
		{
			if (!text.empty()) text += L"\r\n";
			text += L"abcdefghij";
		}

		Window host;
		auto owned = std::make_unique<RichTextBox>();
		auto* box = owned.get();
		box->Text = text;
		box->Width = 320.0;
		box->Height = 72.0;
		box->Focusable = true;
		CUI_EXPECT_TRUE(host.SetVisualContent(std::move(owned)) == box);
		box->Arrange(cui::core::Rect{ 0.0f, 0.0f, 320.0f, 72.0f });
		host.SetKeyboardFocus(box, false);

		box->SetCaretIndex(8);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::PageDown, ModifierKeys::None));
		const int routedTarget = box->GetCaretIndex();
		CUI_EXPECT_TRUE(routedTarget > 8);
		CUI_EXPECT_TRUE(routedTarget < static_cast<int>(text.size()));
		CUI_EXPECT_EQ(8, routedTarget % 12);

		box->SetCaretIndex(8);
		InputReport direct;
		direct.Kind = InputReportKind::KeyDown;
		direct.Key = Key::PageDown;
		direct.Modifiers = ModifierKeys::None;
		CUI_EXPECT_TRUE(cui::framework::InputAccess::DispatchInput(
			*box, direct));
		CUI_EXPECT_EQ(routedTarget, box->GetCaretIndex());

		box->SetCaretIndex(8);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::PageDown, ModifierKeys::Shift));
		CUI_EXPECT_EQ(routedTarget, box->GetCaretIndex());
		CUI_EXPECT_EQ(routedTarget - 8, box->GetSelectionLength());

		CUI_EXPECT_TRUE(ProcessCommandGesture(
			*box, Key::PageDown, ModifierKeys::Shift));
		CUI_EXPECT_TRUE(box->GetCaretIndex() > routedTarget);
		CUI_EXPECT_EQ(8, box->GetCaretIndex() % 12);
		CUI_EXPECT_EQ(box->GetCaretIndex() - 8,
			box->GetSelectionLength());
	});

	runner.Add(
		"RichTextBox editing commands toggle first character auto word and springload",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		auto bold = std::make_unique<Bold>();
		bold->Inlines.AddRun(L"a");
		paragraph.Inlines.Add(std::move(bold));
		paragraph.Inlines.AddRun(L"b");
		RichTextBox mixed;
		mixed.SetDocument(std::move(document));
		mixed.Select(0, 2);
		CUI_EXPECT_TRUE(ProcessCommandGesture(mixed, Key::B));
		ExpectWeight(Flatten(mixed), 0, DWRITE_FONT_WEIGHT_NORMAL);
		// The second Run already inherits the effective Normal value, so the
		// toggle must not freeze that value as redundant local formatting.
		ExpectWeight(Flatten(mixed), 1, std::nullopt);
		ExpectSelectionWeight(mixed,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_NORMAL);
		CUI_EXPECT_TRUE(mixed.CanUndo());
		mixed.Undo();
		CUI_EXPECT_FALSE(mixed.CanUndo());
		ExpectWeight(Flatten(mixed), 0, DWRITE_FONT_WEIGHT_BOLD);
		ExpectWeight(Flatten(mixed), 1, std::nullopt);

		RichTextBox word;
		word.Text = L"word rest";
		word.SetCaretIndex(2);
		CUI_EXPECT_TRUE(ProcessCommandGesture(word, Key::I));
		CUI_EXPECT_EQ(2, word.GetCaretIndex());
		CUI_EXPECT_EQ(0, word.GetSelectionLength());
		const auto italic = Flatten(word);
		const RichTextDocument italicDocument(italic);
		CUI_EXPECT_EQ(
			std::optional<DWRITE_FONT_STYLE>(DWRITE_FONT_STYLE_ITALIC),
			italicDocument.StyleAt(0).FontStyle);
		CUI_EXPECT_EQ(
			std::optional<DWRITE_FONT_STYLE>(DWRITE_FONT_STYLE_ITALIC),
			italicDocument.StyleAt(3).FontStyle);
		CUI_EXPECT_EQ(
			std::optional<DWRITE_FONT_STYLE>{},
			italicDocument.StyleAt(5).FontStyle);
		CUI_EXPECT_TRUE(word.CanUndo());
		word.Undo();
		CUI_EXPECT_FALSE(word.CanUndo());

		RichTextBox springload;
		springload.Text = L"word rest";
		springload.SetCaretIndex(4);
		CUI_EXPECT_TRUE(ProcessCommandGesture(springload, Key::U));
		CUI_EXPECT_FALSE(springload.CanUndo());
		springload.InsertText(L"X");
		CUI_EXPECT_EQ(std::wstring(L"wordX rest"), springload.Text);
		ExpectUnderline(Flatten(springload), 4, true);
		CUI_EXPECT_TRUE(springload.CanUndo());
		springload.Undo();
		CUI_EXPECT_EQ(std::wstring(L"word rest"), springload.Text);
	});

	runner.Add(
		"RichTextBox editing commands align touched paragraphs and honor read only",
		[]
	{
		RichTextBox editor;
		editor.Text = L"a\r\nb\r\nc";
		editor.Select(0, 4);
		CUI_EXPECT_TRUE(ProcessCommandGesture(editor, Key::E));
		CUI_EXPECT_EQ(::TextAlignment::Center,
			dynamic_cast<Paragraph*>(
				editor.GetDocument().Blocks.At(0))->GetTextAlignment());
		CUI_EXPECT_EQ(::TextAlignment::Center,
			dynamic_cast<Paragraph*>(
				editor.GetDocument().Blocks.At(1))->GetTextAlignment());
		CUI_EXPECT_EQ(::TextAlignment::Left,
			dynamic_cast<Paragraph*>(
				editor.GetDocument().Blocks.At(2))->GetTextAlignment());
		CUI_EXPECT_TRUE(editor.CanUndo());
		editor.Undo();
		CUI_EXPECT_FALSE(editor.CanUndo());
		CUI_EXPECT_EQ(::TextAlignment::Left,
			dynamic_cast<Paragraph*>(
				editor.GetDocument().Blocks.At(0))->GetTextAlignment());
		CUI_EXPECT_EQ(::TextAlignment::Left,
			dynamic_cast<Paragraph*>(
				editor.GetDocument().Blocks.At(1))->GetTextAlignment());
		editor.Redo();
		CUI_EXPECT_EQ(::TextAlignment::Center,
			dynamic_cast<Paragraph*>(
				editor.GetDocument().Blocks.At(0))->GetTextAlignment());

		RichTextBox readOnly;
		readOnly.Text = L"locked";
		readOnly.SelectAll();
		readOnly.IsReadOnly = true;
		CUI_EXPECT_TRUE(ProcessCommandGesture(readOnly, Key::B));
		CUI_EXPECT_TRUE(ProcessCommandGesture(readOnly, Key::J));
		ExpectSelectionWeight(readOnly,
			TextSelectionPropertyValueKind::Value,
			DWRITE_FONT_WEIGHT_NORMAL);
		ExpectSelectionAlignment(readOnly,
			TextSelectionPropertyValueKind::Value,
			::TextAlignment::Left);
		CUI_EXPECT_FALSE(readOnly.CanUndo());
	});

	runner.Add(
		"RichTextBox reset format and font size commands match WPF selection semantics",
		[]
	{
		auto document = std::make_unique<FlowDocument>();
		auto& paragraph = document->Blocks.AddParagraph();
		paragraph.SetFontWeight(DWRITE_FONT_WEIGHT_BOLD);
		auto underlined = std::make_unique<::Underline>();
		auto formattedRun = std::make_unique<Run>(L"word");
		formattedRun->SetFontStyle(DWRITE_FONT_STYLE_ITALIC);
		underlined->Inlines.Add(std::move(formattedRun));
		paragraph.Inlines.Add(std::move(underlined));
		paragraph.Inlines.AddRun(L" rest");
		auto* paragraphIdentity = &paragraph;

		RichTextBox reset;
		reset.SetDocument(std::move(document));
		reset.SetCaretIndex(2);
		CUI_EXPECT_TRUE(ProcessCommandGesture(reset, Key::Space));
		CUI_EXPECT_EQ(2, reset.GetCaretIndex());
		CUI_EXPECT_EQ(0, reset.GetSelectionLength());
		CUI_EXPECT_TRUE(reset.GetDocument().Blocks.At(0)
			== paragraphIdentity);
		const auto cleared = Flatten(reset);
		ExpectWeight(cleared, 0, DWRITE_FONT_WEIGHT_BOLD);
		ExpectUnderline(cleared, 0, std::nullopt);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_STYLE>{},
			RichTextDocument(cleared).StyleAt(0).FontStyle);
		ExpectSelectionUnderline(reset,
			TextSelectionPropertyValueKind::Value, false);
		CUI_EXPECT_TRUE(reset.CanUndo());
		reset.Undo();
		const auto restored = Flatten(reset);
		ExpectUnderline(restored, 0, true);
		CUI_EXPECT_EQ(
			std::optional<DWRITE_FONT_STYLE>(DWRITE_FONT_STYLE_ITALIC),
			RichTextDocument(restored).StyleAt(0).FontStyle);

		RichTextBox springloadReset;
		springloadReset.Text = L"word rest";
		springloadReset.SetCaretIndex(4);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			springloadReset, Key::I));
		CUI_EXPECT_FALSE(springloadReset.CanUndo());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			springloadReset, Key::Space));
		CUI_EXPECT_FALSE(springloadReset.CanUndo());
		springloadReset.InsertText(L"X");
		CUI_EXPECT_EQ(std::wstring(L"wordX rest"),
			springloadReset.Text);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_STYLE>{},
			RichTextDocument(Flatten(springloadReset))
				.StyleAt(4).FontStyle);

		auto sizedDocument = std::make_unique<FlowDocument>();
		auto& sizedParagraph = sizedDocument->Blocks.AddParagraph();
		auto first = std::make_unique<Run>(L"a");
		first->SetFontSize(10.0);
		sizedParagraph.Inlines.Add(std::move(first));
		auto second = std::make_unique<Run>(L"b");
		second->SetFontSize(20.0);
		sizedParagraph.Inlines.Add(std::move(second));
		RichTextBox sized;
		sized.SetDocument(std::move(sizedDocument));
		sized.SelectAll();
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			sized, Key::OemCloseBrackets));
		ExpectFontSize(Flatten(sized), 0, 10.75f);
		ExpectFontSize(Flatten(sized), 1, 20.75f);
		CUI_EXPECT_TRUE(sized.CanUndo());
		sized.Undo();
		ExpectFontSize(Flatten(sized), 0, 10.0f);
		ExpectFontSize(Flatten(sized), 1, 20.0f);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			sized, Key::OemOpenBrackets));
		ExpectFontSize(Flatten(sized), 0, 9.25f);
		ExpectFontSize(Flatten(sized), 1, 19.25f);

		RichTextBox autoWordSize;
		autoWordSize.Text = L"word rest";
		autoWordSize.SetCaretIndex(2);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			autoWordSize, Key::OemCloseBrackets));
		ExpectFontSize(Flatten(autoWordSize), 0, 12.75f);
		ExpectFontSize(Flatten(autoWordSize), 3, 12.75f);
		CUI_EXPECT_EQ(std::optional<float>{},
			RichTextDocument(Flatten(autoWordSize)).StyleAt(5).FontSize);
		CUI_EXPECT_TRUE(autoWordSize.CanUndo());

		RichTextBox springloadSize;
		springloadSize.Text = L"word rest";
		springloadSize.SetCaretIndex(4);
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			springloadSize, Key::OemCloseBrackets));
		CUI_EXPECT_FALSE(springloadSize.CanUndo());
		springloadSize.InsertText(L"X");
		ExpectFontSize(Flatten(springloadSize), 4, 12.75f);

		auto minimumDocument = std::make_unique<FlowDocument>();
		auto& minimumParagraph = minimumDocument->Blocks.AddParagraph();
		auto minimumRun = std::make_unique<Run>(L"x");
		minimumRun->SetFontSize(0.75);
		minimumParagraph.Inlines.Add(std::move(minimumRun));
		RichTextBox minimum;
		minimum.SetDocument(std::move(minimumDocument));
		minimum.SelectAll();
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			minimum, Key::OemOpenBrackets));
		ExpectFontSize(Flatten(minimum), 0, 0.75f);
		CUI_EXPECT_FALSE(minimum.CanUndo());
	});

	runner.Add(
		"RichTextBox word commands navigate and delete atomic text with one undo",
		[]
	{
		const std::wstring text =
			L"one  \xD83D\xDE00 two\r\nthree";
		CUI_EXPECT_EQ(5, CuiTextEdit::GetNextWordCaretIndex(
			text, 0, true));
		CUI_EXPECT_EQ(8, CuiTextEdit::GetNextWordCaretIndex(
			text, 5, true));
		CUI_EXPECT_EQ(13, CuiTextEdit::GetNextWordCaretIndex(
			text, 8, true));
		CUI_EXPECT_EQ(5, CuiTextEdit::GetPreviousWordCaretIndex(
			text, 8, true));
		CUI_EXPECT_EQ(8, CuiTextEdit::GetPreviousWordCaretIndex(
			text, 13, true));
		// Invalid mid-pair positions snap through the complete UTF-16 element.
		CUI_EXPECT_EQ(8, CuiTextEdit::GetNextWordCaretIndex(
			text, 6, true));
		CUI_EXPECT_EQ(5, CuiTextEdit::GetPreviousWordCaretIndex(
			text, 6, true));
		CUI_EXPECT_EQ(13, CuiTextEdit::GetNextWordCaretIndex(
			text, 12, true));
		CUI_EXPECT_EQ(8, CuiTextEdit::GetPreviousWordCaretIndex(
			text, 12, true));

		RichTextBox navigation;
		navigation.Text = text;
		navigation.SetCaretIndex(0);
		CUI_EXPECT_TRUE(ProcessCommandGesture(navigation, Key::Right));
		CUI_EXPECT_EQ(5, navigation.GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(navigation, Key::Right));
		CUI_EXPECT_EQ(8, navigation.GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(
			navigation, Key::Right,
			ModifierKeys::Control | ModifierKeys::Shift));
		CUI_EXPECT_EQ(13, navigation.GetCaretIndex());
		CUI_EXPECT_EQ(5, navigation.GetSelectionLength());
		CUI_EXPECT_TRUE(ProcessCommandGesture(navigation, Key::Left));
		CUI_EXPECT_EQ(8, navigation.GetCaretIndex());
		CUI_EXPECT_EQ(0, navigation.GetSelectionLength());

		navigation.SetCaretIndex(13);
		CUI_EXPECT_TRUE(ProcessCommandGesture(navigation, Key::Back));
		CUI_EXPECT_EQ(std::wstring(
			L"one  \xD83D\xDE00 three"), navigation.Text);
		CUI_EXPECT_EQ(8, navigation.GetCaretIndex());
		CUI_EXPECT_TRUE(navigation.CanUndo());
		navigation.Undo();
		CUI_EXPECT_EQ(text, navigation.Text);
		CUI_EXPECT_EQ(13, navigation.GetCaretIndex());

		navigation.SetCaretIndex(5);
		CUI_EXPECT_TRUE(ProcessCommandGesture(navigation, Key::Delete));
		CUI_EXPECT_EQ(std::wstring(L"one  two\r\nthree"),
			navigation.Text);
		CUI_EXPECT_EQ(5, navigation.GetCaretIndex());
		CUI_EXPECT_TRUE(navigation.CanUndo());
		navigation.Undo();
		CUI_EXPECT_EQ(text, navigation.Text);

		Window host;
		auto hostedOwner = std::make_unique<RichTextBox>();
		auto* hosted = hostedOwner.get();
		hosted->Text = L"one two";
		hosted->SetCaretIndex(4);
		hosted->Focusable = true;
		CUI_EXPECT_TRUE(host.SetVisualContent(
			std::move(hostedOwner)) == hosted);
		host.SetKeyboardFocus(hosted, false);
		InputReport deleteWord;
		deleteWord.Kind = InputReportKind::KeyDown;
		deleteWord.Key = Key::Delete;
		deleteWord.Modifiers = ModifierKeys::Control;
		CUI_EXPECT_TRUE(cui::framework::InputAccess::DispatchInput(
			host, deleteWord));
		CUI_EXPECT_EQ(std::wstring(L"one "), hosted->Text);
		CUI_EXPECT_TRUE(hosted->CanUndo());

		RichTextBox readOnly;
		readOnly.Text = text;
		readOnly.SetCaretIndex(0);
		readOnly.IsReadOnly = true;
		CUI_EXPECT_TRUE(ProcessCommandGesture(readOnly, Key::Right));
		CUI_EXPECT_EQ(5, readOnly.GetCaretIndex());
		CUI_EXPECT_TRUE(ProcessCommandGesture(readOnly, Key::Delete));
		CUI_EXPECT_EQ(text, readOnly.Text);
		CUI_EXPECT_FALSE(readOnly.CanUndo());
	});

	runner.Add(
		"RichText virtual layout incrementally profiles long visual lines",
		[]
	{
		std::wstring longParagraph;
		longParagraph.reserve(12000);
		for (int index = 0; index < 2000; ++index)
			longParagraph.append(L"office");

		RichTextBox box;
		const auto lines = RichTextBoxDocumentTestAccess::ProfileVisualLines(
			box, longParagraph, 180.0f, FLT_MAX);
		CUI_EXPECT_TRUE(lines.size() > 100);
		std::size_t cursor = 0;
		for (const auto& line : lines)
		{
			CUI_EXPECT_EQ(static_cast<unsigned long long>(cursor),
				static_cast<unsigned long long>(line.Start));
			CUI_EXPECT_TRUE(line.IsSingleVisualLine);
			CUI_EXPECT_TRUE(line.Height > 0.0f);
			CUI_EXPECT_TRUE(line.LayoutLength <= line.Length);
			CUI_EXPECT_TRUE(line.LayoutLength <= 256);
			cursor += line.Length;
		}
		CUI_EXPECT_EQ(
			static_cast<unsigned long long>(longParagraph.size()),
			static_cast<unsigned long long>(cursor));
		std::wstring unicodeParagraph;
		unicodeParagraph.reserve(6000);
		for (int index = 0; index < 2000; ++index)
			unicodeParagraph.append(L"\xD83D\xDE00" L"x");
		RichTextBox unicodeBox;
		const auto unicodeLines =
			RichTextBoxDocumentTestAccess::ProfileVisualLines(
				unicodeBox, unicodeParagraph, 180.0f, FLT_MAX);
		CUI_EXPECT_TRUE(unicodeLines.size() > 20);
		for (const auto& line : unicodeLines)
		{
			const auto boundary = line.Start + line.Length;
			if (boundary > 0 && boundary < unicodeParagraph.size())
				CUI_EXPECT_FALSE(CuiTextEdit::IsHighSurrogate(
					unicodeParagraph[boundary - 1])
					&& CuiTextEdit::IsLowSurrogate(
						unicodeParagraph[boundary]));
		}
		std::wstring bidiParagraph;
		bidiParagraph.reserve(5000);
		for (int index = 0; index < 1000; ++index)
			bidiParagraph.append(L"\u0633\u0644\u0627\u0645 ");
		RichTextBox bidiBox;
		const auto bidiBlocks =
			RichTextBoxDocumentTestAccess::ProfileVisualLines(
				bidiBox, bidiParagraph, 180.0f, FLT_MAX);
		CUI_EXPECT_EQ(1ULL,
			static_cast<unsigned long long>(bidiBlocks.size()));
		if (!bidiBlocks.empty())
		{
			CUI_EXPECT_FALSE(bidiBlocks.front().IsSingleVisualLine);
			CUI_EXPECT_EQ(
				static_cast<unsigned long long>(bidiParagraph.size()),
				static_cast<unsigned long long>(bidiBlocks.front().Length));
		}
		const auto measured = RichTextBoxDocumentTestAccess::
			MeasureVirtualAndContinuousHeight(box, 180.0f);
		CUI_EXPECT_NEAR(measured.second, measured.first, 0.01);
		const auto resident = RichTextBoxDocumentTestAccess::
			MeasureAllBlockMetrics(box, 180.0f, FLT_MAX);
		CUI_EXPECT_EQ(0ULL,
			static_cast<unsigned long long>(resident.second));

		auto styledDocument = std::make_unique<FlowDocument>();
		auto& paragraph = styledDocument->Blocks.AddParagraph();
		paragraph.Inlines.Add(std::make_unique<Run>(
			std::wstring(5600, L'a')));
		auto emphasized = std::make_unique<Run>(
			std::wstring(800, L'W'));
		emphasized->SetFontSize(40.0);
		paragraph.Inlines.Add(std::move(emphasized));
		paragraph.Inlines.Add(std::make_unique<Run>(
			std::wstring(5600, L'b')));
		RichTextBox styledBox;
		styledBox.SetDocument(std::move(styledDocument));
		const auto styledMeasured = RichTextBoxDocumentTestAccess::
			MeasureVirtualAndContinuousHeight(styledBox, 220.0f);
		CUI_EXPECT_NEAR(
			styledMeasured.second, styledMeasured.first, 0.01);

		std::wstring trailingBreak(6000, L'x');
		trailingBreak.append(L"\r\n");
		RichTextBox trailingBox;
		const auto trailingLines =
			RichTextBoxDocumentTestAccess::ProfileVisualLines(
				trailingBox, trailingBreak, 180.0f, FLT_MAX);
		CUI_EXPECT_TRUE(trailingLines.size() > 2);
		if (!trailingLines.empty())
		{
			const auto& finalLine = trailingLines.back();
			CUI_EXPECT_EQ(
				static_cast<unsigned long long>(trailingBreak.size()),
				static_cast<unsigned long long>(finalLine.Start));
			CUI_EXPECT_EQ(0ULL,
				static_cast<unsigned long long>(finalLine.Length));
			CUI_EXPECT_EQ(0ULL,
				static_cast<unsigned long long>(finalLine.LayoutLength));
			CUI_EXPECT_TRUE(finalLine.HasSentinel);
			CUI_EXPECT_TRUE(finalLine.IsSingleVisualLine);
		}
		const auto trailingMeasured = RichTextBoxDocumentTestAccess::
			MeasureVirtualAndContinuousHeight(trailingBox, 180.0f);
		CUI_EXPECT_NEAR(
			trailingMeasured.second, trailingMeasured.first, 0.01);

		RichTextBox hitBox;
		const int caretIndex = 7777;
		const auto caretRoundTrip =
			RichTextBoxDocumentTestAccess::RoundTripVirtualCaret(
				hitBox, longParagraph, 220.0f, 140.0f, caretIndex);
		CUI_EXPECT_TRUE(caretRoundTrip.first);
		CUI_EXPECT_EQ(caretIndex, caretRoundTrip.second);
	});
}
