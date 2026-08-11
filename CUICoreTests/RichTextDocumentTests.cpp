#include "RichTextDocumentTests.h"

#include "TestRunner.h"
#include <RichTextDocument.h>
#include <TextBoundary.h>

#include <array>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
	RichTextCharacterStyle WeightStyle(DWRITE_FONT_WEIGHT weight)
	{
		RichTextCharacterStyle style;
		style.FontWeight = weight;
		return style;
	}

	RichTextDocumentFragment CanonicalFixture()
	{
		const auto normal = WeightStyle(DWRITE_FONT_WEIGHT_NORMAL);
		const auto bold = WeightStyle(DWRITE_FONT_WEIGHT_BOLD);
		RichTextDocumentFragment fragment;
		fragment.Text = L"abcdef";
		fragment.Spans = {
			{ 0, 2, normal }, { 2, 2, bold }, { 4, 2, normal }
		};
		return fragment;
	}

	void ExpectSpan(
		const RichTextStyleSpan& span,
		std::size_t start,
		std::size_t length,
		const RichTextCharacterStyle& style)
	{
		CUI_EXPECT_EQ(start, span.Start);
		CUI_EXPECT_EQ(length, span.Length);
		CUI_EXPECT_EQ(style, span.Style);
	}

	template<typename TException, typename TAction>
	bool Throws(TAction&& action)
	{
		try
		{
			std::forward<TAction>(action)();
		}
		catch (const TException&)
		{
			return true;
		}
		return false;
	}

	void ExpectSingleTextElement(const std::wstring& text)
	{
		CUI_EXPECT_TRUE(!text.empty());
		CUI_EXPECT_TRUE(CuiTextBoundary::IsTextElementBoundary(
			text, 0, true));
		CUI_EXPECT_TRUE(CuiTextBoundary::IsTextElementBoundary(
			text, text.size(), true));
		for (std::size_t position = 1; position < text.size(); ++position)
		{
			CUI_EXPECT_FALSE(CuiTextBoundary::IsTextElementBoundary(
				text, position, true));
		}
		CUI_EXPECT_EQ(
			static_cast<unsigned long long>(text.size()),
			static_cast<unsigned long long>(
				CuiTextBoundary::GetNextTextElementBoundary(
					text, 0, true)));
		CUI_EXPECT_EQ(0ULL, static_cast<unsigned long long>(
			CuiTextBoundary::GetPreviousTextElementBoundary(
				text, text.size(), true)));
	}
}

void RegisterRichTextDocumentTests(cui::test::Runner& runner)
{
	runner.Add(
		"RichText document replacement splits shifts and merges canonical spans",
		[]
	{
		const auto normal = WeightStyle(DWRITE_FONT_WEIGHT_NORMAL);
		const auto bold = WeightStyle(DWRITE_FONT_WEIGHT_BOLD);
		const auto light = WeightStyle(DWRITE_FONT_WEIGHT_LIGHT);
		RichTextDocument document(CanonicalFixture());

		const auto change = document.Replace(1, 4,
			RichTextDocumentFragment::FromPlainText(L"XY", light));
		CUI_EXPECT_EQ(1ULL,
			static_cast<unsigned long long>(change.Start));
		CUI_EXPECT_EQ(std::wstring(L"bcde"), change.Before.Text);
		CUI_EXPECT_EQ(3ULL,
			static_cast<unsigned long long>(change.Before.Spans.size()));
		ExpectSpan(change.Before.Spans[0], 0, 1, normal);
		ExpectSpan(change.Before.Spans[1], 1, 2, bold);
		ExpectSpan(change.Before.Spans[2], 3, 1, normal);
		CUI_EXPECT_EQ(std::wstring(L"XY"), change.After.Text);
		CUI_EXPECT_TRUE(change.TextChanged());

		CUI_EXPECT_EQ(std::wstring(L"aXYf"), document.GetText());
		CUI_EXPECT_EQ(3ULL,
			static_cast<unsigned long long>(document.GetSpans().size()));
		ExpectSpan(document.GetSpans()[0], 0, 1, normal);
		ExpectSpan(document.GetSpans()[1], 1, 2, light);
		ExpectSpan(document.GetSpans()[2], 3, 1, normal);
		CUI_EXPECT_TRUE(document.ValidateCanonical());

		RichTextDocument deletion(CanonicalFixture());
		(void)deletion.Replace(1, 4, RichTextDocumentFragment{});
		CUI_EXPECT_EQ(std::wstring(L"af"), deletion.GetText());
		CUI_EXPECT_EQ(1ULL,
			static_cast<unsigned long long>(deletion.GetSpans().size()));
		ExpectSpan(deletion.GetSpans()[0], 0, 2, normal);
		CUI_EXPECT_TRUE(deletion.ValidateCanonical());
	});

	runner.Add(
		"RichText document extraction preserves relative attributed spans",
		[]
	{
		const auto normal = WeightStyle(DWRITE_FONT_WEIGHT_NORMAL);
		const auto bold = WeightStyle(DWRITE_FONT_WEIGHT_BOLD);
		RichTextDocument document(CanonicalFixture());
		const auto extracted = document.Extract(1, 4);

		CUI_EXPECT_EQ(std::wstring(L"bcde"), extracted.Text);
		CUI_EXPECT_EQ(3ULL,
			static_cast<unsigned long long>(extracted.Spans.size()));
		ExpectSpan(extracted.Spans[0], 0, 1, normal);
		ExpectSpan(extracted.Spans[1], 1, 2, bold);
		ExpectSpan(extracted.Spans[2], 3, 1, normal);
		CUI_EXPECT_TRUE(extracted.ValidateCanonical());

		const auto empty = document.Extract(3, 0);
		CUI_EXPECT_TRUE(empty.Empty());
		CUI_EXPECT_TRUE(empty.Spans.empty());
		CUI_EXPECT_TRUE(empty.ValidateCanonical());
	});

	runner.Add(
		"RichText document formatting applies set and clear deltas canonically",
		[]
	{
		RichTextDocument document(L"abcdef");
		const auto foreground = cui::drawing::MakeSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::CornflowerBlue));
		const auto background = cui::drawing::MakeSolidColorBrush(
			D2D1::ColorF(D2D1::ColorF::LightGoldenrodYellow));
		RichTextFormatDelta set;
		set.Foreground =
			RichTextFormatChange<cui::drawing::Brush>::Set(foreground);
		set.Background =
			RichTextFormatChange<cui::drawing::Brush>::Set(background);
		set.FontFamily =
			RichTextFormatChange<std::wstring>::Set(L"Consolas");
		set.Language =
			RichTextFormatChange<std::wstring>::Set(L"ja-jp");
		set.FontSize = RichTextFormatChange<float>::Set(18.0f);
		set.FontWeight = RichTextFormatChange<DWRITE_FONT_WEIGHT>::Set(
			DWRITE_FONT_WEIGHT_BOLD);
		set.FontStretch = RichTextFormatChange<DWRITE_FONT_STRETCH>::Set(
			DWRITE_FONT_STRETCH_EXPANDED);
		set.FontStyle = RichTextFormatChange<DWRITE_FONT_STYLE>::Set(
			DWRITE_FONT_STYLE_ITALIC);
		set.Underline = RichTextFormatChange<bool>::Set(true);
		set.Strikethrough = RichTextFormatChange<bool>::Set(true);

		const auto applied = document.ApplyFormat(1, 4, set);
		CUI_EXPECT_TRUE(applied.Changed());
		CUI_EXPECT_FALSE(applied.TextChanged());
		CUI_EXPECT_TRUE(applied.FormattingChanged());
		CUI_EXPECT_EQ(std::wstring(L"abcdef"), document.GetText());
		CUI_EXPECT_EQ(3ULL,
			static_cast<unsigned long long>(document.GetSpans().size()));
		const auto styled = document.StyleAt(2);
		CUI_EXPECT_EQ(std::optional<cui::drawing::Brush>(foreground),
			styled.Foreground);
		CUI_EXPECT_EQ(std::optional<cui::drawing::Brush>(background),
			styled.Background);
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"Consolas"),
			styled.FontFamily);
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"ja-jp"),
			styled.Language);
		CUI_EXPECT_EQ(std::optional<float>(18.0f), styled.FontSize);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_WEIGHT>(
			DWRITE_FONT_WEIGHT_BOLD), styled.FontWeight);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_STRETCH>(
			DWRITE_FONT_STRETCH_EXPANDED), styled.FontStretch);
		CUI_EXPECT_EQ(std::optional<DWRITE_FONT_STYLE>(
			DWRITE_FONT_STYLE_ITALIC), styled.FontStyle);
		CUI_EXPECT_EQ(std::optional<bool>(true), styled.Underline);
		CUI_EXPECT_EQ(std::optional<bool>(true), styled.Strikethrough);

		RichTextFormatDelta clear;
		clear.Foreground =
			RichTextFormatChange<cui::drawing::Brush>::Clear();
		clear.Background =
			RichTextFormatChange<cui::drawing::Brush>::Clear();
		clear.FontFamily = RichTextFormatChange<std::wstring>::Clear();
		clear.Language = RichTextFormatChange<std::wstring>::Clear();
		clear.FontSize = RichTextFormatChange<float>::Clear();
		clear.FontWeight =
			RichTextFormatChange<DWRITE_FONT_WEIGHT>::Clear();
		clear.FontStretch =
			RichTextFormatChange<DWRITE_FONT_STRETCH>::Clear();
		clear.FontStyle =
			RichTextFormatChange<DWRITE_FONT_STYLE>::Clear();
		clear.Underline = RichTextFormatChange<bool>::Clear();
		clear.Strikethrough = RichTextFormatChange<bool>::Clear();
		const auto cleared = document.ApplyFormat(1, 4, clear);
		CUI_EXPECT_TRUE(cleared.FormattingChanged());
		CUI_EXPECT_EQ(1ULL,
			static_cast<unsigned long long>(document.GetSpans().size()));
		CUI_EXPECT_EQ(RichTextCharacterStyle{}, document.StyleAt(2));
		CUI_EXPECT_TRUE(document.ValidateCanonical());

		RichTextFormatDelta largeFont;
		largeFont.FontSize = RichTextFormatChange<float>::Set(500.0f);
		CUI_EXPECT_TRUE(largeFont.Validate());
		const auto enlarged = document.ApplyFormat(0, 1, largeFont);
		CUI_EXPECT_TRUE(enlarged.FormattingChanged());
		CUI_EXPECT_EQ(std::optional<float>(500.0f),
			document.StyleAt(0).FontSize);
	});

	runner.Add(
		"RichText document formatting does not freeze an inherited effective value",
		[]
	{
		RichTextCharacterStyle boldStyle;
		boldStyle.FontWeight = DWRITE_FONT_WEIGHT_BOLD;
		RichTextDocumentFragment fragment;
		fragment.Text = L"A";
		fragment.Spans = { { 0, 1, boldStyle } };
		fragment.RootStyle = RichTextCharacterStyle{};
		fragment.StructureRootId = AllocateRichTextStructureId();
		fragment.StructureSpans = { {
			0, 1,
			{
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Paragraph, {} },
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Bold, {} },
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Run, {} }
			}
		} };
		CUI_EXPECT_TRUE(fragment.ValidateCanonical());
		RichTextDocument document(fragment);
		RichTextFormatDelta setBold;
		setBold.FontWeight =
			RichTextFormatChange<DWRITE_FONT_WEIGHT>::Set(
				DWRITE_FONT_WEIGHT_BOLD);
		const auto change = document.ApplyFormat(0, 1, setBold);
		CUI_EXPECT_FALSE(change.Changed());
		CUI_EXPECT_EQ(fragment, document.ToFragment());
		CUI_EXPECT_FALSE(document.ToFragment()
			.StructureSpans.front().Path.back().LocalStyle.FontWeight
			.has_value());

		RichTextFormatDelta setBaselineSize;
		setBaselineSize.FontSize =
			RichTextFormatChange<float>::Set(30.0f);
		RichTextCharacterStyle baseline;
		baseline.FontSize = 30.0f;
		const auto baselineChange = document.ApplyFormat(
			0, 1, setBaselineSize, baseline);
		CUI_EXPECT_FALSE(baselineChange.Changed());
		CUI_EXPECT_EQ(fragment, document.ToFragment());
		CUI_EXPECT_FALSE(document.ToFragment()
			.StructureSpans.front().Path.back().LocalStyle.FontSize
			.has_value());
	});

	runner.Add(
		"RichText document relative font sizing preserves run differences and clamps",
		[]
	{
		RichTextCharacterStyle ten;
		ten.FontSize = 10.0f;
		RichTextCharacterStyle twenty;
		twenty.FontSize = 20.0f;
		RichTextDocumentFragment fragment;
		fragment.Text = L"ab";
		fragment.Spans = { { 0, 1, ten }, { 1, 1, twenty } };
		RichTextDocument document(fragment);
		RichTextCharacterStyle baseline;
		baseline.FontSize = 12.0f;
		const auto increased = document.AdjustFontSize(
			0, 2, 0.75f, 0.75f, 1638.0f, baseline);
		CUI_EXPECT_TRUE(increased.Changed());
		CUI_EXPECT_FALSE(increased.TextChanged());
		CUI_EXPECT_NEAR(10.75f,
			*document.StyleAt(0).FontSize, 0.0001);
		CUI_EXPECT_NEAR(20.75f,
			*document.StyleAt(1).FontSize, 0.0001);

		(void)document.AdjustFontSize(
			0, 2, -100.0f, 0.75f, 1638.0f, baseline);
		CUI_EXPECT_NEAR(0.75f,
			*document.StyleAt(0).FontSize, 0.0001);
		CUI_EXPECT_NEAR(0.75f,
			*document.StyleAt(1).FontSize, 0.0001);
		const auto unchanged = document.AdjustFontSize(
			0, 2, -0.75f, 0.75f, 1638.0f, baseline);
		CUI_EXPECT_FALSE(unchanged.Changed());
		CUI_EXPECT_TRUE(document.ValidateCanonical());
		CUI_EXPECT_TRUE(Throws<std::invalid_argument>([&]
		{
			(void)document.AdjustFontSize(
				0, 1, 0.0f, 0.75f, 1638.0f, baseline);
		}));
	});

	runner.Add(
		"RichText document structure root provenance survives edits and rejects foreign roots",
		[]
	{
		RichTextDocumentFragment original;
		original.Text = L"A";
		original.Spans = { { 0, 1, {} } };
		original.RootStyle = RichTextCharacterStyle{};
		original.StructureRootId = AllocateRichTextStructureId();
		original.StructureSpans = { {
			0, 1,
			{
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Paragraph, {} },
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Run, {} }
			}
		} };
		CUI_EXPECT_TRUE(original.ValidateCanonical());
		RichTextDocument document(original);

		auto foreign = original;
		foreign.StructureRootId = AllocateRichTextStructureId();
		CUI_EXPECT_TRUE(foreign.ValidateCanonical());
		CUI_EXPECT_TRUE(Throws<std::invalid_argument>([&]
		{
			(void)document.Replace(0, 1, foreign);
		}));
		CUI_EXPECT_EQ(original, document.ToFragment());

		RichTextDocument portable(L"xy");
		(void)portable.Replace(1, 0, original);
		CUI_EXPECT_EQ(std::wstring(L"xAy"), portable.GetText());
		CUI_EXPECT_TRUE(portable.ToFragment().StructureSpans.empty());
		CUI_EXPECT_FALSE(portable.ToFragment().RootStyle.has_value());
		CUI_EXPECT_FALSE(
			portable.ToFragment().StructureRootId.has_value());
		CUI_EXPECT_TRUE(portable.ValidateCanonical());

		(void)document.Replace(1, 0,
			RichTextDocumentFragment::FromPlainText(L"B"));
		CUI_EXPECT_EQ(original.StructureRootId,
			document.ToFragment().StructureRootId);
		CUI_EXPECT_EQ(original.StructureRootId,
			document.Extract(0, document.Length()).StructureRootId);

		const auto deletion = document.Replace(
			0, document.Length(), RichTextDocumentFragment{});
		CUI_EXPECT_TRUE(document.Empty());
		CUI_EXPECT_FALSE(document.ToFragment().RootStyle.has_value());
		CUI_EXPECT_FALSE(
			document.ToFragment().StructureRootId.has_value());
		(void)document.Replace(
			deletion.Start, deletion.After.Text.size(), deletion.Before);
		CUI_EXPECT_EQ(original.StructureRootId,
			document.ToFragment().StructureRootId);
		CUI_EXPECT_EQ(std::wstring(L"AB"), document.GetText());
		CUI_EXPECT_TRUE(document.ValidateCanonical());
	});

	runner.Add(
		"RichText document ranges preserve CRLF and surrogate pairs",
		[]
	{
		const std::wstring text =
			L"A\r\n\xD83D\xDE00" L"B";
		RichTextDocument document(text);

		const auto crlf = document.NormalizeRange(2, 1);
		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(crlf.Start));
		CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(crlf.Length));
		CUI_EXPECT_EQ(std::wstring(L"\r\n"), document.Extract(2, 1).Text);
		const auto surrogate = document.NormalizeRange(4, 1);
		CUI_EXPECT_EQ(3ULL,
			static_cast<unsigned long long>(surrogate.Start));
		CUI_EXPECT_EQ(2ULL,
			static_cast<unsigned long long>(surrogate.Length));

		CUI_EXPECT_EQ(1ULL, static_cast<unsigned long long>(
			document.SnapToBoundary(
				2, RichTextBoundaryAffinity::Backward)));
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			document.SnapToBoundary(
				2, RichTextBoundaryAffinity::Forward)));
		CUI_EXPECT_EQ(3ULL, static_cast<unsigned long long>(
			document.SnapToBoundary(
				4, RichTextBoundaryAffinity::Backward)));
		CUI_EXPECT_EQ(5ULL, static_cast<unsigned long long>(
			document.SnapToBoundary(
				4, RichTextBoundaryAffinity::Forward)));

		(void)document.Replace(4, 1, RichTextDocumentFragment{});
		CUI_EXPECT_EQ(std::wstring(L"A\r\nB"), document.GetText());
		CUI_EXPECT_TRUE(document.ValidateCanonical());

		RichTextDocument caret(text);
		(void)caret.Replace(2, 0,
			RichTextDocumentFragment::FromPlainText(L"X"));
		CUI_EXPECT_EQ(
			std::wstring(L"A\r\nX\xD83D\xDE00" L"B"),
			caret.GetText());
		CUI_EXPECT_TRUE(caret.ValidateCanonical());
		CUI_EXPECT_EQ(std::wstring(L"A\r\nB"),
			RichTextDocumentFragment::FromPlainText(L"A\nB").Text);
		});

	runner.Add(
		"RichText document edit boundaries preserve composed Unicode text elements",
		[]
		{
			const std::wstring combining = L"e\u0301";
			const std::wstring emojiModifier =
				L"\xD83D\xDC4D\xD83C\xDFFD";
			const std::wstring family =
				L"\xD83D\xDC69\u200D\xD83D\xDC69\u200D"
				L"\xD83D\xDC67\u200D\xD83D\xDC66";
			const std::wstring flag =
				L"\xD83C\xDDFA\xD83C\xDDF8";
			const std::wstring keycap = L"1\uFE0F\u20E3";
			const std::wstring hangul = L"\u1100\u1161\u11A8";
			const std::wstring indic = L"\u0915\u094D\u0937";

			ExpectSingleTextElement(combining);
			ExpectSingleTextElement(emojiModifier);
			ExpectSingleTextElement(family);
			ExpectSingleTextElement(flag);
			ExpectSingleTextElement(keycap);
			ExpectSingleTextElement(hangul);
			ExpectSingleTextElement(indic);
			ExpectSingleTextElement(L"\r\n");

			const std::wstring threeIndicators =
				L"\xD83C\xDDFA\xD83C\xDDF8\xD83C\xDDE8";
			CUI_EXPECT_FALSE(CuiTextBoundary::IsTextElementBoundary(
				threeIndicators, 2, true));
			CUI_EXPECT_TRUE(CuiTextBoundary::IsTextElementBoundary(
				threeIndicators, 4, true));

			const auto normal = WeightStyle(DWRITE_FONT_WEIGHT_NORMAL);
			const auto bold = WeightStyle(DWRITE_FONT_WEIGHT_BOLD);
			RichTextDocumentFragment splitStyle;
			splitStyle.Text = combining;
			splitStyle.Spans = {
				{ 0, 1, normal }, { 1, 1, bold }
			};
			// Formatting boundaries are independent from editable caret
			// positions, matching WPF's ability to split Runs inside a
			// composed sequence.
			CUI_EXPECT_TRUE(splitStyle.ValidateCanonical());
			RichTextDocument document(splitStyle);
			CUI_EXPECT_EQ(0ULL, static_cast<unsigned long long>(
				document.SnapToBoundary(
					1, RichTextBoundaryAffinity::Backward)));
			CUI_EXPECT_EQ(2ULL, static_cast<unsigned long long>(
				document.SnapToBoundary(
					1, RichTextBoundaryAffinity::Forward)));
			const auto normalized = document.NormalizeRange(1, 1);
			CUI_EXPECT_EQ(0ULL,
				static_cast<unsigned long long>(normalized.Start));
			CUI_EXPECT_EQ(2ULL,
				static_cast<unsigned long long>(normalized.Length));
			CUI_EXPECT_EQ(combining, document.Extract(1, 1).Text);

			(void)document.Replace(1, 1, RichTextDocumentFragment{});
			CUI_EXPECT_TRUE(document.Empty());
			CUI_EXPECT_TRUE(document.ValidateCanonical());
		});

	runner.Add(
		"RichText document zero-width structure markers survive edit undo and redo",
		[]
		{
			const auto rootId = AllocateRichTextStructureId();
			const auto paragraphId = AllocateRichTextStructureId();
			const auto spanId = AllocateRichTextStructureId();
			const auto runId = AllocateRichTextStructureId();
			RichTextDocumentFragment emptyTree;
			emptyTree.RootStyle = RichTextCharacterStyle{};
			emptyTree.StructureRootId = rootId;
			emptyTree.StructureMarkers = { {
				0,
				{
					{ paragraphId, RichTextStructureKind::Paragraph, {} },
					{ spanId, RichTextStructureKind::Span, {} },
					{ runId, RichTextStructureKind::Run, {} }
				}
			} };
			CUI_EXPECT_TRUE(emptyTree.ValidateCanonical());

			RichTextDocument document(emptyTree);
			const auto insertion = document.Replace(0, 0,
				RichTextDocumentFragment::FromPlainText(L"X"));
			CUI_EXPECT_EQ(emptyTree, insertion.Before);
			CUI_EXPECT_EQ(std::wstring(L"X"), document.GetText());
			CUI_EXPECT_TRUE(document.ToFragment().StructureMarkers.empty());
			CUI_EXPECT_EQ(runId, document.ToFragment()
				.StructureSpans.front().Path.back().Id);

			const auto undo = document.Replace(
				insertion.Start, insertion.After.Text.size(), insertion.Before);
			CUI_EXPECT_EQ(insertion.After, undo.Before);
			CUI_EXPECT_EQ(insertion.Before, undo.After);
			CUI_EXPECT_EQ(emptyTree, document.ToFragment());

			const auto redo = document.Replace(
				insertion.Start, insertion.Before.Text.size(), insertion.After);
			CUI_EXPECT_EQ(insertion.Before, redo.Before);
			CUI_EXPECT_EQ(insertion.After, redo.After);
			CUI_EXPECT_EQ(std::wstring(L"X"), document.GetText());
			CUI_EXPECT_TRUE(document.ValidateCanonical());
		});

	runner.Add(
		"RichText document insertion style honors boundary affinity",
		[]
		{
		const auto normal = WeightStyle(DWRITE_FONT_WEIGHT_NORMAL);
		const auto bold = WeightStyle(DWRITE_FONT_WEIGHT_BOLD);
		RichTextDocumentFragment fragment;
		fragment.Text = L"abCD";
		fragment.Spans = { { 0, 2, normal }, { 2, 2, bold } };
		RichTextDocument document(fragment);

		CUI_EXPECT_EQ(normal, document.InsertionStyleAt(
			0, RichTextBoundaryAffinity::Backward));
		CUI_EXPECT_EQ(normal, document.InsertionStyleAt(
			2, RichTextBoundaryAffinity::Backward));
		CUI_EXPECT_EQ(bold, document.InsertionStyleAt(
			2, RichTextBoundaryAffinity::Forward));
		CUI_EXPECT_EQ(bold, document.InsertionStyleAt(
			4, RichTextBoundaryAffinity::Forward));

		const auto inherited = document.InsertionStyleAt(2);
		(void)document.Replace(2, 0,
			RichTextDocumentFragment::FromPlainText(L"x", inherited));
		CUI_EXPECT_EQ(std::wstring(L"abxCD"), document.GetText());
		CUI_EXPECT_EQ(normal, document.StyleAt(2));
		CUI_EXPECT_TRUE(document.ValidateCanonical());
	});

	runner.Add(
		"RichText document rejects malformed fragments and deltas",
		[]
	{
		const auto normal = WeightStyle(DWRITE_FONT_WEIGHT_NORMAL);
		RichTextDocumentFragment gap;
		gap.Text = L"abcd";
		gap.Spans = { { 0, 1, normal }, { 2, 2, normal } };
		CUI_EXPECT_FALSE(gap.ValidateCanonical());

		RichTextDocumentFragment unmerged;
		unmerged.Text = L"abcd";
		unmerged.Spans = { { 0, 2, normal }, { 2, 2, normal } };
		CUI_EXPECT_FALSE(unmerged.ValidateCanonical());

		RichTextDocumentFragment splitCrLf;
		splitCrLf.Text = L"\r\n";
		splitCrLf.Spans = {
			{ 0, 1, normal },
			{ 1, 1, WeightStyle(DWRITE_FONT_WEIGHT_BOLD) }
		};
		CUI_EXPECT_FALSE(splitCrLf.ValidateCanonical());

		RichTextDocumentFragment invalidUtf16;
		invalidUtf16.Text = std::wstring(1, static_cast<wchar_t>(0xD83D));
		invalidUtf16.Spans = { { 0, 1, normal } };
		CUI_EXPECT_FALSE(invalidUtf16.ValidateCanonical());

		auto portableWithRoot =
			RichTextDocumentFragment::FromPlainText(L"portable", normal);
		portableWithRoot.RootStyle = RichTextCharacterStyle{};
		CUI_EXPECT_FALSE(portableWithRoot.ValidateCanonical());
		auto portableWithRootId =
			RichTextDocumentFragment::FromPlainText(L"portable", normal);
		portableWithRootId.StructureRootId =
			AllocateRichTextStructureId();
		CUI_EXPECT_FALSE(portableWithRootId.ValidateCanonical());

		RichTextDocumentFragment markerOnly;
		markerOnly.RootStyle = RichTextCharacterStyle{};
		markerOnly.StructureRootId = AllocateRichTextStructureId();
		markerOnly.StructureMarkers = { {
			0,
			{
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Paragraph, {} },
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Run, {} }
			}
		} };
		CUI_EXPECT_TRUE(markerOnly.ValidateCanonical());
		auto markerWithoutRoot = markerOnly;
		markerWithoutRoot.StructureRootId.reset();
		CUI_EXPECT_FALSE(markerWithoutRoot.ValidateCanonical());
		auto zeroLineBreak = markerOnly;
		zeroLineBreak.StructureMarkers[0].Path.back().Kind =
			RichTextStructureKind::LineBreak;
		CUI_EXPECT_FALSE(zeroLineBreak.ValidateCanonical());
		auto duplicateMarker = markerOnly;
		duplicateMarker.StructureMarkers.push_back(
			duplicateMarker.StructureMarkers.front());
		CUI_EXPECT_FALSE(duplicateMarker.ValidateCanonical());
		auto disconnectedParagraph = markerOnly;
		disconnectedParagraph.StructureMarkers.push_back({
			0,
			{
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Paragraph, {} },
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Run, {} }
			}
		});
		CUI_EXPECT_FALSE(disconnectedParagraph.ValidateCanonical());

		RichTextDocumentFragment explicitLineBreak;
		explicitLineBreak.Text = L"\r\n";
		explicitLineBreak.Spans = { { 0, 2, normal } };
		explicitLineBreak.RootStyle = RichTextCharacterStyle{};
		explicitLineBreak.StructureRootId =
			AllocateRichTextStructureId();
		explicitLineBreak.StructureSpans = { {
			0, 2,
			{
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Paragraph, {} },
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::LineBreak, normal }
			}
		} };
		CUI_EXPECT_TRUE(explicitLineBreak.ValidateCanonical());
		auto malformedLineBreak = explicitLineBreak;
		malformedLineBreak.Text = L"xx";
		CUI_EXPECT_FALSE(malformedLineBreak.ValidateCanonical());
		auto markerInsideCrLf = explicitLineBreak;
		markerInsideCrLf.StructureMarkers.push_back({
			1,
			{
				markerInsideCrLf.StructureSpans.front().Path.front(),
				{ AllocateRichTextStructureId(),
					RichTextStructureKind::Run, {} }
			}
		});
		CUI_EXPECT_FALSE(markerInsideCrLf.ValidateCanonical());
		auto oversizedLineBreak = explicitLineBreak;
		oversizedLineBreak.Text = L"\r\n\r\n";
		oversizedLineBreak.Spans[0].Length = 4;
		oversizedLineBreak.StructureSpans[0].Length = 4;
		CUI_EXPECT_FALSE(oversizedLineBreak.ValidateCanonical());
		auto inlineParagraphFormatting = explicitLineBreak;
		inlineParagraphFormatting.StructureSpans[0].Path.back()
			.LocalParagraphStyle.FlowDirection =
				::FlowDirection::RightToLeft;
		CUI_EXPECT_FALSE(inlineParagraphFormatting.ValidateCanonical());

		RichTextParagraphStyle invalidParagraphStyle;
		invalidParagraphStyle.FlowDirection =
			static_cast<::FlowDirection>(255);
		CUI_EXPECT_FALSE(invalidParagraphStyle.Validate());
		RichTextParagraphFormatDelta invalidParagraphDelta;
		invalidParagraphDelta.FlowDirection =
			RichTextFormatChange<::FlowDirection>::Set(
				static_cast<::FlowDirection>(255));
		CUI_EXPECT_FALSE(invalidParagraphDelta.Validate());

		RichTextFormatDelta invalidDelta;
		invalidDelta.FontSize.Mode = RichTextFormatDeltaMode::Set;
		CUI_EXPECT_FALSE(invalidDelta.Validate());
		RichTextFormatDelta tooLargeFont;
		tooLargeFont.FontSize =
			RichTextFormatChange<float>::Set(160001.0f);
		CUI_EXPECT_FALSE(tooLargeFont.Validate());
		RichTextFormatDelta tooSmallFont;
		tooSmallFont.FontSize =
			RichTextFormatChange<float>::Set(0.001f);
		CUI_EXPECT_FALSE(tooSmallFont.Validate());
		RichTextCharacterStyle invalidStretchStyle;
		invalidStretchStyle.FontStretch =
			static_cast<DWRITE_FONT_STRETCH>(0);
		CUI_EXPECT_FALSE(invalidStretchStyle.Validate());
		RichTextFormatDelta invalidStretchDelta;
		invalidStretchDelta.FontStretch =
			RichTextFormatChange<DWRITE_FONT_STRETCH>::Set(
				static_cast<DWRITE_FONT_STRETCH>(10));
		CUI_EXPECT_FALSE(invalidStretchDelta.Validate());
		CUI_EXPECT_EQ(std::optional<std::wstring>(L"zh-hans-cn"),
			NormalizeRichTextLanguageTag(L"ZH-Hans-CN"));
		CUI_EXPECT_TRUE(IsCanonicalRichTextLanguageTag(L"en-us"));
		CUI_EXPECT_TRUE(IsCanonicalRichTextLanguageTag(L""));
		CUI_EXPECT_FALSE(IsCanonicalRichTextLanguageTag(L"en-US"));
		CUI_EXPECT_FALSE(NormalizeRichTextLanguageTag(L"9-en"));
		CUI_EXPECT_FALSE(NormalizeRichTextLanguageTag(L"en--us"));
		CUI_EXPECT_FALSE(NormalizeRichTextLanguageTag(L"abcdefghij"));
		CUI_EXPECT_FALSE(NormalizeRichTextLanguageTag(L"中文"));
		RichTextCharacterStyle invalidLanguageStyle;
		invalidLanguageStyle.Language = L"en-US";
		CUI_EXPECT_FALSE(invalidLanguageStyle.Validate());
		RichTextFormatDelta invalidLanguageDelta;
		invalidLanguageDelta.Language =
			RichTextFormatChange<std::wstring>::Set(L"en--us");
		CUI_EXPECT_FALSE(invalidLanguageDelta.Validate());
		RichTextDocument document(L"text");
		CUI_EXPECT_TRUE(Throws<std::invalid_argument>([&]
		{
			(void)document.ApplyFormat(0, 1, invalidDelta);
		}));
		CUI_EXPECT_TRUE(Throws<std::invalid_argument>([&]
		{
			(void)document.Replace(0, 1, gap);
		}));
	});

	runner.Add(
		"RichText document randomized replacements undo and redo exactly",
		[]
	{
		std::mt19937 random(0xC01D5EEDu);
		const std::array<RichTextCharacterStyle, 4> styles = {
			RichTextCharacterStyle{},
			WeightStyle(DWRITE_FONT_WEIGHT_LIGHT),
			WeightStyle(DWRITE_FONT_WEIGHT_BOLD),
			[]
			{
				RichTextCharacterStyle style;
				style.Underline = true;
				return style;
			}()
		};
		const std::array<std::wstring, 6> replacements = {
			L"", L"x", L"YZ", L"\r\n",
			L"\xD83D\xDE00", L"q\r\nr"
		};
		RichTextDocument document(
			L"start\r\n\xD83D\xDE00" L" end");
		const auto initial = document.ToFragment();
		std::vector<RichTextDocumentFragment> beforeStates;
		std::vector<RichTextDocumentChange> changes;
		beforeStates.reserve(128);
		changes.reserve(128);

		for (int iteration = 0; iteration < 128; ++iteration)
		{
			beforeStates.push_back(document.ToFragment());
			const std::size_t start = static_cast<std::size_t>(random())
				% (document.Length() + 1);
			const std::size_t remaining = document.Length() - start;
			const std::size_t length = static_cast<std::size_t>(random())
				% (remaining + 1);
			const auto& text = replacements[static_cast<std::size_t>(random())
				% replacements.size()];
			const auto& style = styles[static_cast<std::size_t>(random())
				% styles.size()];
			changes.push_back(document.Replace(start, length,
				RichTextDocumentFragment::FromPlainText(text, style)));
			CUI_EXPECT_TRUE(document.ValidateCanonical());
		}
		const auto finalState = document.ToFragment();

		for (std::size_t index = changes.size(); index-- > 0;)
		{
			const auto& change = changes[index];
			const auto inverse = document.Replace(
				change.Start, change.After.Text.size(), change.Before);
			CUI_EXPECT_EQ(change.After, inverse.Before);
			CUI_EXPECT_EQ(change.Before, inverse.After);
			CUI_EXPECT_EQ(beforeStates[index], document.ToFragment());
			CUI_EXPECT_TRUE(document.ValidateCanonical());
		}
		CUI_EXPECT_EQ(initial, document.ToFragment());

		for (const auto& change : changes)
		{
			(void)document.Replace(
				change.Start, change.Before.Text.size(), change.After);
			CUI_EXPECT_TRUE(document.ValidateCanonical());
		}
		CUI_EXPECT_EQ(finalState, document.ToFragment());
	});
}
