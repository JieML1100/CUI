#define NOMINMAX
#include "RichTextBox.h"
#include "RichTextClipboard.h"
#include "RichTextRtf.h"
#include "EditingCommands.h"
#include "Window.h"
#include "TextEditCore.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace
{
	constexpr bool RichTextIsMultiLine = true;
	// Mirrors WPF TextEditorCharacters.OneFontPoint/MaxFontPoint.
	constexpr float RichTextOneFontPoint = 72.0f / 96.0f;
	constexpr float RichTextMaximumFontPoint = 1638.0f;

	RichTextFormatDelta ClearCharacterFormattingDelta()
	{
		RichTextFormatDelta delta;
		delta.Foreground =
			RichTextFormatChange<cui::drawing::Brush>::Clear();
		delta.Background =
			RichTextFormatChange<cui::drawing::Brush>::Clear();
		delta.FontFamily = RichTextFormatChange<std::wstring>::Clear();
		delta.Language = RichTextFormatChange<std::wstring>::Clear();
		delta.FontSize = RichTextFormatChange<float>::Clear();
		delta.FontWeight =
			RichTextFormatChange<DWRITE_FONT_WEIGHT>::Clear();
		delta.FontStretch =
			RichTextFormatChange<DWRITE_FONT_STRETCH>::Clear();
		delta.FontStyle =
			RichTextFormatChange<DWRITE_FONT_STYLE>::Clear();
		delta.Underline = RichTextFormatChange<bool>::Clear();
		delta.Strikethrough = RichTextFormatChange<bool>::Clear();
		return delta;
	}

	CuiTextEdit::EditOptions RichEditOptions()
	{
		CuiTextEdit::EditOptions options;
		options.allowMultiLine = RichTextIsMultiLine;
		return options;
	}

	std::size_t RebaseSelectionOffset(
		std::size_t position,
		LogicalDirection direction,
		const TextPointerTextChange& change) noexcept
	{
		const auto end = change.Start + change.RemovedLength;
		if (position < change.Start) return position;
		if (position > end)
			return position - change.RemovedLength
				+ change.InsertedLength;
		return direction == LogicalDirection::Backward
			? change.Start : change.Start + change.InsertedLength;
	}

	void ApplyRichTextWrapping(IDWriteTextLayout* layout)
	{
		if (layout)
			layout->SetWordWrapping(DWRITE_WORD_WRAPPING_CHARACTER);
	}

	DWRITE_TEXT_ALIGNMENT ToDirectWriteAlignment(
		::TextAlignment value, ::FlowDirection direction) noexcept
	{
		switch (value)
		{
		case ::TextAlignment::Right:
			return direction == ::FlowDirection::RightToLeft
				? DWRITE_TEXT_ALIGNMENT_LEADING
				: DWRITE_TEXT_ALIGNMENT_TRAILING;
		case ::TextAlignment::Center:
			return DWRITE_TEXT_ALIGNMENT_CENTER;
		case ::TextAlignment::Justify:
			return DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
		case ::TextAlignment::Left:
		default:
			return direction == ::FlowDirection::RightToLeft
				? DWRITE_TEXT_ALIGNMENT_TRAILING
				: DWRITE_TEXT_ALIGNMENT_LEADING;
		}
	}

	DWRITE_READING_DIRECTION ToDirectWriteReadingDirection(
		::FlowDirection value) noexcept
	{
		return value == ::FlowDirection::RightToLeft
			? DWRITE_READING_DIRECTION_RIGHT_TO_LEFT
			: DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
	}

	bool TryGetLineMetrics(
		IDWriteTextLayout* layout,
		std::vector<DWRITE_LINE_METRICS>& metrics)
	{
		metrics.clear();
		if (!layout) return false;
		UINT32 required = 0;
		(void)layout->GetLineMetrics(nullptr, 0, &required);
		if (required == 0) return false;
		metrics.resize(required);
		UINT32 written = 0;
		if (FAILED(layout->GetLineMetrics(
			metrics.data(), required, &written)))
		{
			metrics.clear();
			return false;
		}
		metrics.resize((std::min)(required, written));
		return !metrics.empty();
	}

	struct RichTextVisualLineMetrics
	{
		UINT32 RawStart = 0;
		UINT32 RawEnd = 0;
		UINT32 Start = 0;
		UINT32 End = 0;
		float Top = 0.0f;
		float Height = 0.0f;
	};

	bool TryBuildVisualLineMetrics(
		IDWriteTextLayout* layout,
		UINT32 logicalLength,
		std::vector<RichTextVisualLineMetrics>& lines)
	{
		lines.clear();
		std::vector<DWRITE_LINE_METRICS> metrics;
		if (!TryGetLineMetrics(layout, metrics)) return false;
		lines.reserve(metrics.size());
		UINT32 rawStart = 0;
		float top = 0.0f;
		for (const auto& metric : metrics)
		{
			const auto rawEnd64 = static_cast<std::uint64_t>(rawStart)
				+ metric.length;
			const UINT32 rawEnd = static_cast<UINT32>((std::min)(
				rawEnd64,
				static_cast<std::uint64_t>(
					(std::numeric_limits<UINT32>::max)())));
			const UINT32 newlineLength = (std::min)(
				metric.newlineLength, metric.length);
			const UINT32 rawVisibleEnd = rawEnd - newlineLength;
			const UINT32 start = (std::min)(rawStart, logicalLength);
			const UINT32 end = (std::max)(start,
				(std::min)(rawVisibleEnd, logicalLength));
			lines.push_back({ rawStart, rawEnd, start, end,
				top, metric.height });
			rawStart = rawEnd;
			top += metric.height;
		}
		return !lines.empty();
	}

	std::size_t FindVisualLineAtPosition(
		const std::vector<RichTextVisualLineMetrics>& lines,
		UINT32 position,
		LogicalDirection direction) noexcept
	{
		if (lines.empty()) return 0;
		for (std::size_t index = 0; index < lines.size(); ++index)
		{
			const auto& line = lines[index];
			if (position < line.RawEnd) return index;
			if (position == line.RawEnd)
			{
				if (direction == LogicalDirection::Backward
					|| index + 1 == lines.size())
					return index;
			}
		}
		return lines.size() - 1;
	}

	bool TryHitTestVisualLine(
		IDWriteTextLayout* layout,
		const RichTextVisualLineMetrics& line,
		float x,
		UINT32& position,
		LogicalDirection& direction) noexcept
	{
		if (!layout) return false;
		BOOL trailing = FALSE;
		BOOL inside = FALSE;
		DWRITE_HIT_TEST_METRICS hit{};
		const float height = line.Height > 0.0f ? line.Height : 1.0f;
		if (FAILED(layout->HitTestPoint(
			x, line.Top + height * 0.5f,
			&trailing, &inside, &hit)))
		{
			return false;
		}
		(void)inside;
		const auto rawPosition64 = static_cast<std::uint64_t>(
			hit.textPosition) + (trailing ? 1u : 0u);
		const UINT32 rawPosition = static_cast<UINT32>((std::min)(
			rawPosition64,
			static_cast<std::uint64_t>(
				(std::numeric_limits<UINT32>::max)())));
		position = (std::clamp)(rawPosition, line.Start, line.End);
		if (line.Start == line.End || position == line.Start)
			direction = LogicalDirection::Forward;
		else if (position == line.End)
			direction = LogicalDirection::Backward;
		else
			direction = trailing
				? LogicalDirection::Backward : LogicalDirection::Forward;
		return true;
	}

	bool TryGetDirectionalCaretMetrics(
		IDWriteTextLayout* layout,
		UINT32 position,
		LogicalDirection direction,
		DWRITE_HIT_TEST_METRICS& caret) noexcept
	{
		if (!layout) return false;
		FLOAT x = 0.0f;
		FLOAT y = 0.0f;
		DWRITE_HIT_TEST_METRICS hit{};
		HRESULT result = E_FAIL;
		if (direction == LogicalDirection::Backward && position > 0)
		{
			result = layout->HitTestTextPosition(
				position - 1, TRUE, &x, &y, &hit);
		}
		else
		{
			result = layout->HitTestTextPosition(
				position, FALSE, &x, &y, &hit);
		}
		if (FAILED(result) && position > 0)
		{
			result = layout->HitTestTextPosition(
				position - 1, TRUE, &x, &y, &hit);
		}
		if (FAILED(result))
		{
			result = layout->HitTestTextPosition(
				position, FALSE, &x, &y, &hit);
		}
		if (FAILED(result)) return false;
		caret = hit;
		caret.textPosition = position;
		caret.length = 0;
		caret.left = x;
		caret.top = y;
		caret.width = 0.0f;
		return true;
	}

	bool RequiresContinuousBidiLayout(
		const std::wstring& text,
		size_t start,
		size_t length) noexcept
	{
		start = (std::min)(start, text.size());
		const size_t end = start + (std::min)(length, text.size() - start);
		for (size_t index = start; index < end; ++index)
		{
			std::uint32_t codePoint = static_cast<std::uint16_t>(text[index]);
			if (CuiTextEdit::IsHighSurrogate(text[index])
				&& index + 1 < end
				&& CuiTextEdit::IsLowSurrogate(text[index + 1]))
			{
				codePoint = 0x10000u
					+ ((codePoint - 0xD800u) << 10)
					+ (static_cast<std::uint16_t>(text[++index]) - 0xDC00u);
			}

			// Independent line layouts cannot reproduce arbitrary paragraph-level
			// bidi embedding state or joining forms across a soft wrap. Preserve one
			// continuous layout for those scripts until a stateful text analyzer is
			// available; ordinary LTR/CJK/emoji content remains virtualized.
			if ((codePoint >= 0x0590u && codePoint <= 0x08FFu)
				|| (codePoint >= 0x1800u && codePoint <= 0x18AFu)
				|| (codePoint >= 0x200Cu && codePoint <= 0x200Fu)
				|| (codePoint >= 0x202Au && codePoint <= 0x202Eu)
				|| (codePoint >= 0x2066u && codePoint <= 0x2069u)
				|| (codePoint >= 0xFB1Du && codePoint <= 0xFDFFu)
				|| (codePoint >= 0xFE70u && codePoint <= 0xFEFFu)
				|| (codePoint >= 0x10800u && codePoint <= 0x10FFFu)
				|| (codePoint >= 0x1E800u && codePoint <= 0x1EEFFu))
				return true;
		}
		return false;
	}

}

TextPointer TextSelection::GetStart() const
{
	if (!_owner || !_owner->_document) return {};
	_owner->SyncBufferFromControlIfNeeded();
	return _owner->_document->CreateTextPointerAtTextOffset(
		static_cast<std::size_t>((std::min)(
			_owner->_selectionStart, _owner->_selectionEnd)),
		LogicalDirection::Backward);
}

TextPointer TextSelection::GetEnd() const
{
	if (!_owner || !_owner->_document) return {};
	_owner->SyncBufferFromControlIfNeeded();
	return _owner->_document->CreateTextPointerAtTextOffset(
		static_cast<std::size_t>((std::max)(
			_owner->_selectionStart, _owner->_selectionEnd)),
		LogicalDirection::Forward);
}

TextPointer TextSelection::GetAnchorPosition() const
{
	if (!_owner || !_owner->_document) return {};
	_owner->SyncBufferFromControlIfNeeded();
	const auto direction = _owner->_selectionStart < _owner->_selectionEnd
		? LogicalDirection::Backward
		: _owner->_selectionStart > _owner->_selectionEnd
			? LogicalDirection::Forward
			: _owner->_caretLogicalDirection;
	return _owner->_document->CreateTextPointerAtTextOffset(
		static_cast<std::size_t>(_owner->_selectionStart), direction);
}

TextPointer TextSelection::GetMovingPosition() const
{
	if (!_owner || !_owner->_document) return {};
	_owner->SyncBufferFromControlIfNeeded();
	const auto direction = _owner->_selectionEnd > _owner->_selectionStart
		? LogicalDirection::Forward
		: _owner->_selectionEnd < _owner->_selectionStart
			? LogicalDirection::Backward
			: _owner->_caretLogicalDirection;
	return _owner->_document->CreateTextPointerAtTextOffset(
		static_cast<std::size_t>(_owner->_selectionEnd), direction);
}

void TextSelection::Select(
	const TextPointer& anchorPosition,
	const TextPointer& movingPosition)
{
	if (_owner)
		_owner->SelectPointers(anchorPosition, movingPosition);
}

void TextSelection::SetText(std::wstring value)
{
	if (!_owner || !_owner->_document) return;
	_owner->SyncBufferFromControlIfNeeded();
	const auto normalized = _owner->_flatDocument.NormalizeRange(
		static_cast<std::size_t>((std::max)(0,
			(std::min)(_owner->_selectionStart, _owner->_selectionEnd))),
		static_cast<std::size_t>((std::abs)(
			_owner->_selectionEnd - _owner->_selectionStart)));
	(void)_owner->ReplaceTextRangeContent(
		normalized.Start,
		normalized.Length,
		RichTextDocumentFragment::FromPlainText(
			std::move(value), _owner->EffectiveTypingStyle()));
}

bool TextSelection::ApplyPropertyValue(
	const DependencyProperty& property, const BindingValue& value)
{
	if (!_owner) return false;
	RichTextParagraphFormatDelta paragraphDelta;
	if (TextRange::TryBuildParagraphFormatDelta(
		property, value, paragraphDelta))
	{
		return _owner->ApplySelectionParagraphFormat(paragraphDelta);
	}
	RichTextFormatDelta delta;
	return TextRange::TryBuildFormatDelta(property, value, delta)
		&& _owner->ApplySelectionFormat(delta);
}

TextSelectionPropertyValue TextSelection::GetPropertyValue(
	const DependencyProperty& property) const
{
	return _owner
		? _owner->QuerySelectionProperty(property)
		: TextSelectionPropertyValue{};
}

void TextSelection::ClearAllProperties()
{
	if (_owner) _owner->ClearSelectionFormatting();
}

UIClass RichTextBox::Type() { return UIClass::UI_RichTextBox; }

const DependencyProperty& RichTextBox::TextProperty()
{
	static const auto registration = []
	{
		using Handler = DependencyPropertyMetadata::ChangeHandler;
		DependencyPropertyOptions<RichTextBox, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsRender;
		options.Coerce = [](RichTextBox& target,
			const std::wstring& proposed) -> std::optional<std::wstring>
		{
			return target.NormalizeLineBreaks(proposed);
		};
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Common";
		options.Design.CategoryOrder = 0;
		options.Design.Order = 10;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Native;
		)
		options.Changed = [](
			RichTextBox& target,
			const std::wstring& oldValue, const std::wstring& newValue)
		{
			target.OnCompatibilityTextChanged(oldValue, newValue);
		};
		return DependencyPropertyRegistry::RegisterStatic<
			RichTextBox, std::wstring>(
				DependencyPropertyRegistrationLiteral(L"Text"),
				[](RichTextBox& target, Handler handler,
					DataSourceUpdateMode mode)
				{
					if (mode == DataSourceUpdateMode::OnValidation)
					{
						return target.OnLostFocus.Subscribe(
							[handler = std::move(handler)](Control*)
							{ handler(); });
					}
					return target.OnTextChanged.Subscribe(
						[handler = std::move(handler)](
							Control*, TextChangedEventArgs&) { handler(); });
				}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& RichTextBox::MaxLengthProperty()
{
	static const auto registration = []
	{
		auto options = DependencyPropertyOptions<RichTextBox, int>{
			0, DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsRender,
			[](RichTextBox&, const int& proposed) -> std::optional<int>
			{ return (std::max)(0, proposed); } };
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 30;
		options.Design.Editor = DependencyPropertyEditorKind::Number;
		options.Design.Minimum = 0.0;
		options.Design.Step = 1.0;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		)
		return DependencyPropertyRegistry::RegisterStatic<RichTextBox, int>(
			DependencyPropertyRegistrationLiteral(L"MaxLength"),
			[](RichTextBox& target) { return target.MaxLength; },
			[](RichTextBox& target, const int& value)
			{ target.MaxLength = value; }, {}, std::move(options));
	}();
	return *registration;
}

GET_CPP(RichTextBox, std::wstring, Text)
{
	return GetDependencyPropertyValue<std::wstring>(TextProperty());
}

std::wstring RichTextBox::GetSemanticText() const
{
	return GetDependencyPropertyValue<std::wstring>(TextProperty());
}

SET_CPP(RichTextBox, std::wstring, Text)
{
	(void)SetDependencyPropertyValue(TextProperty(), std::move(value));
}

GET_CPP(RichTextBox, int, MaxLength) { return _maxLength; }
SET_CPP(RichTextBox, int, MaxLength)
{
	if (!SetPropertyField(MaxLengthProperty(), _maxLength, value)) return;
	EditorNotificationScope notifications(*this);
	SyncBufferFromControlIfNeeded();
	TrimToMaxLength();
	_textLayoutDirty = true;
	RequestLayout();
	InvalidateVisual();
	notifications.Commit();
}

void RichTextBox::RegisterDependencyProperties()
{
	TextBoxBase::RegisterDependencyProperties();
#if CUI_ENABLE_DYNAMIC_XAML
	(void)TextProperty();
	(void)MaxLengthProperty();
#endif
	CUI_DESIGN_METADATA_ONLY(
	(void)RegisterControlBorderThicknessMetadata<
		RichTextBox, TextBoxBase>(
			1.0f CUI_DESIGN_METADATA_ARGUMENTS(60));
	)
}

const DependencyPropertyMetadata*
RichTextBox::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &Control::BorderThicknessProperty())
	{
		return &RegisterControlBorderThicknessMetadata<
			RichTextBox, TextBoxBase>(
				1.0f CUI_DESIGN_METADATA_ARGUMENTS(60)).Metadata();
	}
	return TextBoxBase::ResolveExactDependencyPropertyMetadata(property);
}

bool RichTextBox::CanHandleMouseWheel(int delta, int localX, int localY)
{
	(void)localX;
	(void)localY;
	if (delta == 0) return false;
	UpdateLayout();
	const float renderHeight = TextViewportHeight();
	const float maxScroll = std::max(0.0f, _textSize.height - renderHeight);
	if (renderHeight <= 0.0f || maxScroll <= 0.0f)
		return false;
	if (this->_verticalScrollOffset < 0.0f) this->_verticalScrollOffset = 0.0f;
	if (this->_verticalScrollOffset > maxScroll) this->_verticalScrollOffset = maxScroll;
	return delta > 0
		? this->_verticalScrollOffset > 0.0f
		: this->_verticalScrollOffset < maxScroll;
}

bool RichTextBox::HandlesNavigationKey(Key key) const
{
	if (key == Key::Tab)
		return this->_acceptsTab;
	switch (key)
	{
	case Key::Left:
	case Key::Right:
	case Key::Up:
	case Key::Down:
	case Key::Home:
	case Key::End:
	case Key::PageUp:
	case Key::PageDown:
		return true;
	default:
		return false;
	}
}

CursorKind RichTextBox::QueryCursor(int localX, int localY)
{
	(void)localY;
	if (!this->IsEnabled) return CursorKind::Arrow;

	const float renderHeight = TextViewportHeight();
	const bool hasVScroll = (renderHeight > 0.0f) && (this->_textSize.height > renderHeight);
	if (hasVScroll && localX >= (this->ActualWidth - 8))
		return CursorKind::SizeNS;

	return CursorKind::IBeam;
}
RichTextBox::RichTextBox()
	: _selection(*this)
{
	RegisterDependencyProperties();
	EnsureEditingCommandBindingsRegistered();
	auto document = std::make_unique<FlowDocument>();
	document->Blocks.AddParagraph();
	SetDocumentCore(std::move(document), true);
	UpdateLayout();
}

void RichTextBox::EnsureEditingCommandBindingsRegistered()
{
	static const auto registrations = []
	{
		std::vector<EventConnection> result;
		result.reserve(32);
		auto registerCommand = [&](
			const RoutedCommand& command, Key key,
			ModifierKeys modifiers = ModifierKeys::Control)
		{
			CommandBinding commandBinding;
			commandBinding.Command = command;
			commandBinding.CanExecute = [](
				Control* target, CanExecuteRoutedEventArgs& args)
			{
				// WPF's rich editing command query remains enabled for an
				// attached text editor. The execution handler itself observes
				// IsEnabled/IsReadOnly and becomes a no-op when editing is barred.
				args.CanExecute = dynamic_cast<RichTextBox*>(target) != nullptr;
			};
			commandBinding.Executed = [command](
				Control* target, ExecutedRoutedEventArgs&)
			{
				if (auto* editor = dynamic_cast<RichTextBox*>(target))
					(void)editor->ExecuteEditingCommand(command);
			};
			result.push_back(
				RoutedCommandManager::RegisterClassCommandBinding(
					UIClass::UI_RichTextBox, std::move(commandBinding)));

			KeyBinding inputBinding;
			inputBinding.Command = command;
			inputBinding.Gesture.Key = key;
			inputBinding.Gesture.Modifiers = modifiers;
			result.push_back(
				RoutedCommandManager::RegisterClassInputBinding(
					UIClass::UI_RichTextBox,
					InputBinding(std::move(inputBinding))));
		};

		registerCommand(EditingCommands::ToggleBold(), Key::B);
		registerCommand(EditingCommands::ToggleItalic(), Key::I);
		registerCommand(EditingCommands::ToggleUnderline(), Key::U);
		registerCommand(EditingCommands::ResetFormat(), Key::Space);
		registerCommand(
			EditingCommands::DecreaseFontSize(), Key::OemOpenBrackets);
		registerCommand(
			EditingCommands::IncreaseFontSize(), Key::OemCloseBrackets);
		registerCommand(EditingCommands::DeleteNextWord(), Key::Delete);
		registerCommand(EditingCommands::DeletePreviousWord(), Key::Back);
		registerCommand(EditingCommands::MoveRightByCharacter(), Key::Right,
			ModifierKeys::None);
		registerCommand(EditingCommands::MoveLeftByCharacter(), Key::Left,
			ModifierKeys::None);
		registerCommand(EditingCommands::SelectRightByCharacter(), Key::Right,
			ModifierKeys::Shift);
		registerCommand(EditingCommands::SelectLeftByCharacter(), Key::Left,
			ModifierKeys::Shift);
		registerCommand(EditingCommands::MoveRightByWord(), Key::Right);
		registerCommand(EditingCommands::MoveLeftByWord(), Key::Left);
		registerCommand(EditingCommands::SelectRightByWord(), Key::Right,
			ModifierKeys::Control | ModifierKeys::Shift);
		registerCommand(EditingCommands::SelectLeftByWord(), Key::Left,
			ModifierKeys::Control | ModifierKeys::Shift);
		registerCommand(EditingCommands::MoveUpByLine(), Key::Up,
			ModifierKeys::None);
		registerCommand(EditingCommands::MoveDownByLine(), Key::Down,
			ModifierKeys::None);
		registerCommand(EditingCommands::SelectUpByLine(), Key::Up,
			ModifierKeys::Shift);
		registerCommand(EditingCommands::SelectDownByLine(), Key::Down,
			ModifierKeys::Shift);
		registerCommand(EditingCommands::MoveUpByPage(), Key::PageUp,
			ModifierKeys::None);
		registerCommand(EditingCommands::MoveDownByPage(), Key::PageDown,
			ModifierKeys::None);
		registerCommand(EditingCommands::SelectUpByPage(), Key::PageUp,
			ModifierKeys::Shift);
		registerCommand(EditingCommands::SelectDownByPage(), Key::PageDown,
			ModifierKeys::Shift);
		registerCommand(EditingCommands::MoveUpByParagraph(), Key::Up);
		registerCommand(EditingCommands::MoveDownByParagraph(), Key::Down);
		registerCommand(EditingCommands::SelectUpByParagraph(), Key::Up,
			ModifierKeys::Control | ModifierKeys::Shift);
		registerCommand(EditingCommands::SelectDownByParagraph(), Key::Down,
			ModifierKeys::Control | ModifierKeys::Shift);
		registerCommand(EditingCommands::MoveToLineStart(), Key::Home,
			ModifierKeys::None);
		registerCommand(EditingCommands::MoveToLineEnd(), Key::End,
			ModifierKeys::None);
		registerCommand(EditingCommands::MoveToDocumentStart(), Key::Home);
		registerCommand(EditingCommands::MoveToDocumentEnd(), Key::End);
		registerCommand(EditingCommands::SelectToLineStart(), Key::Home,
			ModifierKeys::Shift);
		registerCommand(EditingCommands::SelectToLineEnd(), Key::End,
			ModifierKeys::Shift);
		registerCommand(EditingCommands::SelectToDocumentStart(), Key::Home,
			ModifierKeys::Control | ModifierKeys::Shift);
		registerCommand(EditingCommands::SelectToDocumentEnd(), Key::End,
			ModifierKeys::Control | ModifierKeys::Shift);
		registerCommand(EditingCommands::AlignLeft(), Key::L);
		registerCommand(EditingCommands::AlignCenter(), Key::E);
		registerCommand(EditingCommands::AlignRight(), Key::R);
		registerCommand(EditingCommands::AlignJustify(), Key::J);
		return result;
	}();
	(void)registrations;
}

bool RichTextBox::ExecuteEditingCommand(const RoutedCommand& command)
{
	if (!IsEffectivelyEnabled() || _mutatingDocumentFromEditor) return false;
	SyncBufferFromControlIfNeeded();

	if (command == EditingCommands::MoveRightByCharacter())
		return MoveSelectionByCharacter(
			LogicalForwardForHorizontalArrow(true), false);
	if (command == EditingCommands::MoveLeftByCharacter())
		return MoveSelectionByCharacter(
			LogicalForwardForHorizontalArrow(false), false);
	if (command == EditingCommands::SelectRightByCharacter())
		return MoveSelectionByCharacter(
			LogicalForwardForHorizontalArrow(true), true);
	if (command == EditingCommands::SelectLeftByCharacter())
		return MoveSelectionByCharacter(
			LogicalForwardForHorizontalArrow(false), true);
	if (command == EditingCommands::MoveRightByWord())
		return MoveSelectionByWord(
			LogicalForwardForHorizontalArrow(true), false);
	if (command == EditingCommands::MoveLeftByWord())
		return MoveSelectionByWord(
			LogicalForwardForHorizontalArrow(false), false);
	if (command == EditingCommands::SelectRightByWord())
		return MoveSelectionByWord(
			LogicalForwardForHorizontalArrow(true), true);
	if (command == EditingCommands::SelectLeftByWord())
		return MoveSelectionByWord(
			LogicalForwardForHorizontalArrow(false), true);
	if (command == EditingCommands::MoveUpByLine())
		return MoveSelectionVertically(false, false);
	if (command == EditingCommands::MoveDownByLine())
		return MoveSelectionVertically(true, false);
	if (command == EditingCommands::SelectUpByLine())
		return MoveSelectionVertically(false, true);
	if (command == EditingCommands::SelectDownByLine())
		return MoveSelectionVertically(true, true);
	if (command == EditingCommands::MoveUpByPage())
		return MoveSelectionByPage(false, false);
	if (command == EditingCommands::MoveDownByPage())
		return MoveSelectionByPage(true, false);
	if (command == EditingCommands::SelectUpByPage())
		return MoveSelectionByPage(false, true);
	if (command == EditingCommands::SelectDownByPage())
		return MoveSelectionByPage(true, true);
	if (command == EditingCommands::MoveUpByParagraph())
		return MoveSelectionByParagraph(false, false);
	if (command == EditingCommands::MoveDownByParagraph())
		return MoveSelectionByParagraph(true, false);
	if (command == EditingCommands::SelectUpByParagraph())
		return MoveSelectionByParagraph(false, true);
	if (command == EditingCommands::SelectDownByParagraph())
		return MoveSelectionByParagraph(true, true);
	if (command == EditingCommands::MoveToLineStart())
		return MoveSelectionToLineBoundary(false, false);
	if (command == EditingCommands::MoveToLineEnd())
		return MoveSelectionToLineBoundary(true, false);
	if (command == EditingCommands::MoveToDocumentStart())
		return MoveSelectionToDocumentBoundary(false, false);
	if (command == EditingCommands::MoveToDocumentEnd())
		return MoveSelectionToDocumentBoundary(true, false);
	if (command == EditingCommands::SelectToLineStart())
		return MoveSelectionToLineBoundary(false, true);
	if (command == EditingCommands::SelectToLineEnd())
		return MoveSelectionToLineBoundary(true, true);
	if (command == EditingCommands::SelectToDocumentStart())
		return MoveSelectionToDocumentBoundary(false, true);
	if (command == EditingCommands::SelectToDocumentEnd())
		return MoveSelectionToDocumentBoundary(true, true);
	if (_isReadOnly) return false;
	if (command == EditingCommands::DeleteNextWord())
	{
		InputDelete(true);
		return true;
	}
	if (command == EditingCommands::DeletePreviousWord())
	{
		InputBack(true);
		return true;
	}
	if (command == EditingCommands::ResetFormat())
		return ResetSelectionFormattingCommand();
	if (command == EditingCommands::IncreaseFontSize())
		return AdjustSelectionFontSize(RichTextOneFontPoint);
	if (command == EditingCommands::DecreaseFontSize())
		return AdjustSelectionFontSize(-RichTextOneFontPoint);

	if (command == EditingCommands::ToggleBold())
	{
		const auto current = ResolveEffectiveCharacterStyle(
			EffectiveTypingStyle()).FontWeight.value_or(
				DWRITE_FONT_WEIGHT_NORMAL);
		RichTextFormatDelta delta;
		delta.FontWeight = RichTextFormatChange<DWRITE_FONT_WEIGHT>::Set(
			current == DWRITE_FONT_WEIGHT_BOLD
				? DWRITE_FONT_WEIGHT_NORMAL : DWRITE_FONT_WEIGHT_BOLD);
		return ApplySelectionFormat(delta);
	}
	if (command == EditingCommands::ToggleItalic())
	{
		const auto current = ResolveEffectiveCharacterStyle(
			EffectiveTypingStyle()).FontStyle.value_or(
				DWRITE_FONT_STYLE_NORMAL);
		RichTextFormatDelta delta;
		delta.FontStyle = RichTextFormatChange<DWRITE_FONT_STYLE>::Set(
			current == DWRITE_FONT_STYLE_ITALIC
				? DWRITE_FONT_STYLE_NORMAL : DWRITE_FONT_STYLE_ITALIC);
		return ApplySelectionFormat(delta);
	}
	if (command == EditingCommands::ToggleUnderline())
	{
		const bool current = ResolveEffectiveCharacterStyle(
			EffectiveTypingStyle()).Underline.value_or(false);
		RichTextFormatDelta delta;
		delta.Underline = RichTextFormatChange<bool>::Set(!current);
		return ApplySelectionFormat(delta);
	}

	RichTextParagraphFormatDelta paragraphDelta;
	if (command == EditingCommands::AlignLeft())
		paragraphDelta.TextAlignment =
			RichTextFormatChange<::TextAlignment>::Set(
				::TextAlignment::Left);
	else if (command == EditingCommands::AlignCenter())
		paragraphDelta.TextAlignment =
			RichTextFormatChange<::TextAlignment>::Set(
				::TextAlignment::Center);
	else if (command == EditingCommands::AlignRight())
		paragraphDelta.TextAlignment =
			RichTextFormatChange<::TextAlignment>::Set(
				::TextAlignment::Right);
	else if (command == EditingCommands::AlignJustify())
		paragraphDelta.TextAlignment =
			RichTextFormatChange<::TextAlignment>::Set(
				::TextAlignment::Justify);
	else
		return false;
	return ApplySelectionParagraphFormat(paragraphDelta);
}

bool RichTextBox::MoveSelectionByCharacter(
	bool forward, bool extendSelection)
{
	if (_mutatingDocumentFromEditor) return false;
	SyncBufferFromControlIfNeeded();
	ClearSuggestedCaretX();
	_typingStyle.reset();
	const auto selection = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, buffer.size());
	int target = _selectionEnd;
	if (!extendSelection && selection.HasSelection())
	{
		target = SnapCaretIndex(
			forward ? selection.end : selection.start,
			forward ? LogicalDirection::Forward
				: LogicalDirection::Backward);
	}
	else if (forward)
	{
		target = GetNextCaretIndex(_selectionEnd);
	}
	else
	{
		target = GetPreviousCaretIndex(_selectionEnd);
	}

	_selectionEnd = target;
	if (!extendSelection) _selectionStart = target;
	_caretLogicalDirection = forward
		? LogicalDirection::Forward : LogicalDirection::Backward;
	selRangeDirty = true;
	NotifySelectionChanged();
	UpdateScroll();
	InvalidateVisual();
	return true;
}

::FlowDirection RichTextBox::GetParagraphFlowDirectionAt(
	int caretIndex) const
{
	RichTextParagraphStyle baseline;
	baseline.FlowDirection = _document
		? _document->GetFlowDirection() : ::FlowDirection::LeftToRight;
	const auto styles = _flatDocument.ParagraphStylesInRange(
		static_cast<std::size_t>((std::clamp)(
			caretIndex, 0, static_cast<int>(_flatDocument.Length()))),
		0, baseline);
	return styles.empty()
		? baseline.FlowDirection.value_or(::FlowDirection::LeftToRight)
		: styles.front().FlowDirection.value_or(
			::FlowDirection::LeftToRight);
}

bool RichTextBox::LogicalForwardForHorizontalArrow(bool right) const
{
	const bool rightToLeft = GetParagraphFlowDirectionAt(_selectionEnd)
		== ::FlowDirection::RightToLeft;
	return right != rightToLeft;
}

bool RichTextBox::MoveSelectionByWord(
	bool forward, bool extendSelection)
{
	if (_mutatingDocumentFromEditor) return false;
	SyncBufferFromControlIfNeeded();
	ClearSuggestedCaretX();
	_typingStyle.reset();
	const auto selection = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, buffer.size());
	int target = _selectionEnd;
	if (!extendSelection && selection.HasSelection())
		target = SnapCaretIndex(
			forward ? selection.end : selection.start,
			forward ? LogicalDirection::Forward
				: LogicalDirection::Backward);
	else if (forward)
		target = CuiTextEdit::GetNextWordCaretIndex(
			buffer, _selectionEnd, RichTextIsMultiLine);
	else
		target = CuiTextEdit::GetPreviousWordCaretIndex(
			buffer, _selectionEnd, RichTextIsMultiLine);

	_selectionEnd = target;
	if (!extendSelection) _selectionStart = target;
	_caretLogicalDirection = forward
		? LogicalDirection::Forward : LogicalDirection::Backward;
	selRangeDirty = true;
	NotifySelectionChanged();
	UpdateScroll();
	InvalidateVisual();
	return true;
}

bool RichTextBox::MoveSelectionVertically(
	bool down, bool extendSelection)
{
	if (_mutatingDocumentFromEditor) return false;
	SyncBufferFromControlIfNeeded();
	_typingStyle.reset();
	const auto selection = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, buffer.size());
	int source = _selectionEnd;
	if (!extendSelection && selection.HasSelection())
	{
		source = down ? selection.end : selection.start;
		ClearSuggestedCaretX();
	}
	LogicalDirection targetDirection = _caretLogicalDirection;
	const int target = GetVerticalCaretIndex(
		source, down, targetDirection);
	_selectionEnd = target;
	if (!extendSelection) _selectionStart = target;
	_caretLogicalDirection = targetDirection;
	selRangeDirty = true;
	NotifySelectionChanged();
	UpdateScroll();
	InvalidateVisual();
	return true;
}

bool RichTextBox::MoveSelectionByPage(
	bool down, bool extendSelection)
{
	if (_mutatingDocumentFromEditor) return false;
	SyncBufferFromControlIfNeeded();
	_typingStyle.reset();
	const auto selection = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, buffer.size());
	int source = _selectionEnd;
	if (!extendSelection && selection.HasSelection())
	{
		source = down ? selection.end : selection.start;
		ClearSuggestedCaretX();
	}
	const int target = GetPageCaretIndex(source, down);
	_selectionEnd = target;
	if (!extendSelection) _selectionStart = target;
	_caretLogicalDirection = down
		? LogicalDirection::Forward : LogicalDirection::Backward;
	selRangeDirty = true;
	NotifySelectionChanged();
	UpdateScroll(down && target == static_cast<int>(buffer.size()));
	InvalidateVisual();
	return true;
}

bool RichTextBox::MoveSelectionByParagraph(
	bool down, bool extendSelection)
{
	if (_mutatingDocumentFromEditor) return false;
	SyncBufferFromControlIfNeeded();
	ClearSuggestedCaretX();
	_typingStyle.reset();
	const auto selection = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, buffer.size());
	int source = _selectionEnd;
	if (!extendSelection && selection.HasSelection())
		source = down ? selection.end : selection.start;
	const int target = GetParagraphNavigationTarget(
		source, down,
		down && !extendSelection && selection.HasSelection());
	_selectionEnd = target;
	if (!extendSelection) _selectionStart = target;
	_caretLogicalDirection = LogicalDirection::Backward;
	selRangeDirty = true;
	NotifySelectionChanged();
	UpdateScroll(down && target == static_cast<int>(buffer.size()));
	InvalidateVisual();
	return true;
}

bool RichTextBox::MoveSelectionToLineBoundary(
	bool lineEnd, bool extendSelection)
{
	if (_mutatingDocumentFromEditor) return false;
	SyncBufferFromControlIfNeeded();
	ClearSuggestedCaretX();
	_typingStyle.reset();
	const auto selection = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, buffer.size());
	int source = _selectionEnd;
	if (!extendSelection && selection.HasSelection())
		source = lineEnd ? selection.end : selection.start;
	const int target = GetVisualLineBoundary(source, lineEnd);
	_selectionEnd = target;
	if (!extendSelection) _selectionStart = target;
	if (!lineEnd)
		_caretLogicalDirection = LogicalDirection::Forward;
	else
		_caretLogicalDirection = HasCrLfAt(target)
			? LogicalDirection::Forward : LogicalDirection::Backward;
	selRangeDirty = true;
	NotifySelectionChanged();
	UpdateScroll();
	InvalidateVisual();
	return true;
}

bool RichTextBox::MoveSelectionToDocumentBoundary(
	bool documentEnd, bool extendSelection)
{
	if (_mutatingDocumentFromEditor) return false;
	SyncBufferFromControlIfNeeded();
	ClearSuggestedCaretX();
	_typingStyle.reset();
	const int target = documentEnd
		? static_cast<int>(buffer.size()) : 0;
	_selectionEnd = target;
	if (!extendSelection) _selectionStart = target;
	_caretLogicalDirection = documentEnd
		? LogicalDirection::Backward : LogicalDirection::Forward;
	selRangeDirty = true;
	NotifySelectionChanged();
	UpdateScroll(documentEnd);
	InvalidateVisual();
	return true;
}

RichTextBox::~RichTextBox()
{
	if (_document) _document->DetachOwner(this);
	ReleaseTextLayout();
	ReleaseBlocks();
}

void RichTextBox::SetDocument(std::unique_ptr<FlowDocument> document)
{
	SetDocumentCore(std::move(document), false);
}

void RichTextBox::SetDocumentCore(
	std::unique_ptr<FlowDocument> document, bool isImplicit)
{
	if (!document)
		throw std::invalid_argument("RichTextBox.Document cannot be null.");
	if (document.get() == _document.get()) return;
	if (_document && _document->IsMutationDisallowed())
		throw std::logic_error(
			"RichTextBox.Document cannot change during document publication.");
	if (!document->TryAttachOwner(this))
		throw std::invalid_argument(
			"FlowDocument already belongs to another RichTextBox.");

	if (_document) _document->DetachOwner(this);
	_document = std::move(document);
	_documentIsImplicit = isImplicit;
	OnDocumentChanged(true);
}

void RichTextBox::OnFlowDocumentChangedInternal()
{
	if (_documentProjectionTransactionActive)
	{
		OnDocumentChanged(false);
		// The snapshot held by FlowDocument makes this reset reversible if a
		// later public Changed handler rejects the mutation. Public handlers
		// nevertheless observe the committed WPF-style external reset state.
		if (!_mutatingDocumentFromEditor)
		{
			undoStack.clear();
			redoStack.clear();
			_typingStyle.reset();
		}
		return;
	}
	OnDocumentChanged(!_mutatingDocumentFromEditor);
}

std::unique_ptr<RichTextBox::DocumentProjectionTransactionState>
RichTextBox::BeginFlowDocumentProjectionTransaction()
{
	if (_documentProjectionTransactionActive)
		throw std::logic_error(
			"RichTextBox document projection transaction is already active.");
	auto state = std::make_unique<DocumentProjectionTransactionState>();
	state->ExternalReset = !_mutatingDocumentFromEditor;
	state->PreviousProjectionTransactionActive =
		_documentProjectionTransactionActive;
	state->EditorNotificationDepth = _editorNotificationDepth;
	state->PendingTextChanged = _pendingTextChanged;
	state->PendingTextChangedOldValue = _pendingTextChangedOldValue;
	state->PendingTextChangedNewValue = _pendingTextChangedNewValue;
	state->PendingSelectionChanged = _pendingSelectionChanged;
	state->Buffer = buffer;
	state->CompatibilityText = this->Text;
	state->BufferSyncedFromControl = bufferSyncedFromControl;
	state->FlatDocument = _flatDocument;
	state->SelectionStart = _selectionStart;
	state->SelectionEnd = _selectionEnd;
	state->CaretDirection = _caretLogicalDirection;
	state->SuggestedCaretX = _suggestedCaretX;
	state->SuggestedCaretIndex = _suggestedCaretIndex;
	state->LastNotifiedSelectionStart = _lastNotifiedSelectionStart;
	state->LastNotifiedSelectionEnd = _lastNotifiedSelectionEnd;
	state->TypingStyle = _typingStyle;
	state->UndoStack = undoStack;
	state->RedoStack = redoStack;
	state->HighlightRanges = highlightRanges;
	BeginEditorNotificationTransaction();
	_documentProjectionTransactionActive = true;
	return state;
}

void RichTextBox::CommitFlowDocumentProjectionTransaction(
	DocumentProjectionTransactionState& state)
{
	if (state.ExternalReset)
	{
		undoStack.clear();
		redoStack.clear();
		_typingStyle.reset();
	}
	_documentProjectionTransactionActive =
		state.PreviousProjectionTransactionActive;
	// FlowDocument.Changed is the mutation commit boundary. Downstream owner
	// notifications may propagate an exception, but must not leave the owner in
	// a half-active projection transaction or roll the committed document back.
	EndEditorNotificationTransaction(true);
}

void RichTextBox::RollbackFlowDocumentProjectionTransaction(
	DocumentProjectionTransactionState& state) noexcept
{
	// Re-project the restored tree while notifications remain staged. A commit
	// callback may already have decremented the depth, so normalize it first.
	_editorNotificationDepth = state.EditorNotificationDepth + 1;
	_pendingTextChanged = false;
	_pendingTextChangedOldValue.clear();
	_pendingTextChangedNewValue.clear();
	_pendingSelectionChanged = false;
	_documentProjectionTransactionActive = true;
	try
	{
		OnDocumentChanged(false);
	}
	catch (...)
	{
		// The authoritative tree has already rolled back. Restore the cached
		// projection from the allocation-complete snapshot below.
	}

	_projectingDocumentText = true;
	try
	{
		(void)TrySetCurrentPropertyValue(
			TextProperty(), BindingValue(state.CompatibilityText));
	}
	catch (...)
	{
		// Preserve the original document/public callback exception.
	}
	_projectingDocumentText = false;
	buffer = std::move(state.Buffer);
	bufferSyncedFromControl = state.BufferSyncedFromControl;
	_flatDocument = std::move(state.FlatDocument);
	_selectionStart = state.SelectionStart;
	_selectionEnd = state.SelectionEnd;
	_caretLogicalDirection = state.CaretDirection;
	_suggestedCaretX = state.SuggestedCaretX;
	_suggestedCaretIndex = state.SuggestedCaretIndex;
	_lastNotifiedSelectionStart = state.LastNotifiedSelectionStart;
	_lastNotifiedSelectionEnd = state.LastNotifiedSelectionEnd;
	_typingStyle = std::move(state.TypingStyle);
	undoStack = std::move(state.UndoStack);
	redoStack = std::move(state.RedoStack);
	highlightRanges = std::move(state.HighlightRanges);
	_editorNotificationDepth = state.EditorNotificationDepth;
	_pendingTextChanged = state.PendingTextChanged;
	_pendingTextChangedOldValue =
		std::move(state.PendingTextChangedOldValue);
	_pendingTextChangedNewValue =
		std::move(state.PendingTextChangedNewValue);
	_pendingSelectionChanged = state.PendingSelectionChanged;
	_documentProjectionTransactionActive =
		state.PreviousProjectionTransactionActive;
	_textLayoutDirty = true;
	selRangeDirty = true;
	blocksDirty = true;
	blockMetricsDirty = true;
	_caretRectCacheValid = false;
}

void RichTextBox::BeginEditorNotificationTransaction() noexcept
{
	++_editorNotificationDepth;
}

void RichTextBox::EndEditorNotificationTransaction(bool publish)
{
	if (_editorNotificationDepth <= 0)
		throw std::logic_error(
			"RichTextBox editor notification scope is unbalanced.");
	--_editorNotificationDepth;
	if (_editorNotificationDepth != 0) return;

	if (!publish)
	{
		_pendingTextChanged = false;
		_pendingTextChangedOldValue.clear();
		_pendingTextChangedNewValue.clear();
		_pendingSelectionChanged = false;
		return;
	}

	const bool publishText = _pendingTextChanged;
	auto oldText = std::move(_pendingTextChangedOldValue);
	auto newText = std::move(_pendingTextChangedNewValue);
	const bool publishSelection = _pendingSelectionChanged;
	_pendingTextChanged = false;
	_pendingTextChangedOldValue.clear();
	_pendingTextChangedNewValue.clear();
	_pendingSelectionChanged = false;

	if (publishText)
	{
		TextChangedEventArgs args(oldText, newText);
		OnTextChanged(this, args);
	}
	if (publishSelection) PublishSelectionChanged();
}

void RichTextBox::NotifyTextChanged(
	const std::wstring& oldText, const std::wstring& newText)
{
	if (_editorNotificationDepth == 0)
	{
		TextChangedEventArgs args(oldText, newText);
		OnTextChanged(this, args);
		return;
	}
	if (!_pendingTextChanged)
	{
		_pendingTextChanged = true;
		_pendingTextChangedOldValue = oldText;
	}
	_pendingTextChangedNewValue = newText;
	if (_pendingTextChangedOldValue == _pendingTextChangedNewValue)
	{
		_pendingTextChanged = false;
		_pendingTextChangedOldValue.clear();
		_pendingTextChangedNewValue.clear();
	}
}

void RichTextBox::NotifySelectionChanged()
{
	if (_editorNotificationDepth != 0)
	{
		_pendingSelectionChanged = true;
		return;
	}
	PublishSelectionChanged();
}

void RichTextBox::PublishSelectionChanged()
{
	if (_lastNotifiedSelectionStart == _selectionStart
		&& _lastNotifiedSelectionEnd == _selectionEnd)
	{
		return;
	}
	const int oldStart = _lastNotifiedSelectionStart;
	_lastNotifiedSelectionStart = _selectionStart;
	_lastNotifiedSelectionEnd = _selectionEnd;
	SelectionChangedEventArgs args(oldStart, _selectionStart);
	SelectionChanged(this, args);
}

void RichTextBox::SyncBufferFromControlIfNeeded()
{
	if (!this->bufferSyncedFromControl)
	{
		const auto fragment = RichTextDocumentFragment::FromPlainText(
			NormalizeLineBreaks(this->Text));
		ReplaceDocumentContent(fragment, false);
	}
}

void RichTextBox::OnCompatibilityTextChanged(
	const std::wstring& oldText, const std::wstring& newText)
{
	EditorNotificationScope notifications(*this);
	if (!_projectingDocumentText)
	{
		ReplaceDocumentContent(
			RichTextDocumentFragment::FromPlainText(newText), false,
			TextPointerTextChange{
				0, buffer.size(), newText.size() });
	}
	bufferSyncedFromControl = true;
	_textLayoutDirty = true;
	NotifyTextChanged(oldText, newText);
	notifications.Commit();
}

void RichTextBox::OnDocumentChanged(bool resetEditorState)
{
	if (!_document) return;
	ClearSuggestedCaretX();
	const auto fragment = _document->Flatten();
	if (!fragment.ValidateCanonical())
		throw std::logic_error("FlowDocument produced invalid attributed text.");

	const std::wstring previousBuffer = buffer;
	_flatDocument = RichTextDocument(fragment);
	buffer = fragment.Text;
	bufferSyncedFromControl = true;
	const int textLength = static_cast<int>(buffer.size());
	_selectionStart = (std::clamp)(_selectionStart, 0, textLength);
	_selectionEnd = (std::clamp)(_selectionEnd, 0, textLength);

	if (resetEditorState)
	{
		undoStack.clear();
		redoStack.clear();
		_typingStyle.reset();
	}
	if (previousBuffer != buffer)
	{
		highlightRanges.clear();
	}

	_textLayoutDirty = true;
	selRangeDirty = true;
	blocksDirty = true;
	blockMetricsDirty = true;
	_caretRectCacheValid = false;

	_projectingDocumentText = true;
	try
	{
		(void)TrySetCurrentPropertyValue(
			TextProperty(), BindingValue(buffer));
	}
	catch (...)
	{
		_projectingDocumentText = false;
		throw;
	}
	_projectingDocumentText = false;
	NotifySelectionChanged();
	RequestLayout();
	InvalidateVisual();
}

void RichTextBox::RebaseSelectionForDocumentChange(
	const TextPointerTextChange& textChange,
	std::size_t newTextLength) noexcept
{
	const auto anchorDirection = _selectionStart < _selectionEnd
		? LogicalDirection::Backward
		: _selectionStart > _selectionEnd
			? LogicalDirection::Forward
			: _caretLogicalDirection;
	const auto movingDirection = _selectionEnd > _selectionStart
		? LogicalDirection::Forward
		: _selectionEnd < _selectionStart
			? LogicalDirection::Backward
			: _caretLogicalDirection;
	_selectionStart = static_cast<int>((std::min)(
		RebaseSelectionOffset(
			static_cast<std::size_t>((std::max)(0, _selectionStart)),
			anchorDirection, textChange),
		newTextLength));
	_selectionEnd = static_cast<int>((std::min)(
		RebaseSelectionOffset(
			static_cast<std::size_t>((std::max)(0, _selectionEnd)),
			movingDirection, textChange),
		newTextLength));
}

void RichTextBox::ReplaceDocumentContent(
	const RichTextDocumentFragment& fragment,
	bool editorMutation,
	std::optional<TextPointerTextChange> textChange)
{
	if (!_document)
		throw std::logic_error("RichTextBox has no FlowDocument.");
	std::wstring error;
	const bool previousMutation = _mutatingDocumentFromEditor;
	const int previousSelectionStart = _selectionStart;
	const int previousSelectionEnd = _selectionEnd;
	const auto previousCaretDirection = _caretLogicalDirection;
	const auto previousTypingStyle = _typingStyle;
	std::vector<UndoRecord> previousUndoStack;
	std::vector<UndoRecord> previousRedoStack;
	std::vector<RichTextBoxTextRange> previousHighlightRanges;
	if (!editorMutation)
	{
		previousUndoStack = undoStack;
		previousRedoStack = redoStack;
		previousHighlightRanges = highlightRanges;
	}
	_mutatingDocumentFromEditor = editorMutation;
	bool changed = false;
	try
	{
		changed = _document->ReplaceFromFragment(
			fragment, &error, editorMutation, std::move(textChange));
	}
	catch (...)
	{
		_mutatingDocumentFromEditor = previousMutation;
		// FlowDocument rolls its tree back before propagating. Its owner may
		// already have observed the transient Changed event, so restore the
		// compatibility projection before leaving the scope.
		try
		{
			OnDocumentChanged(false);
		}
		catch (...)
		{
			// Preserve the original document-transaction failure.
		}
		_selectionStart = previousSelectionStart;
		_selectionEnd = previousSelectionEnd;
		_caretLogicalDirection = previousCaretDirection;
		_typingStyle = previousTypingStyle;
		if (!editorMutation)
		{
			undoStack = std::move(previousUndoStack);
			redoStack = std::move(previousRedoStack);
			highlightRanges = std::move(previousHighlightRanges);
		}
		NotifySelectionChanged();
		throw;
	}
	_mutatingDocumentFromEditor = previousMutation;
	if (!changed && !error.empty())
		throw std::invalid_argument(
			"RichTextBox rejected an invalid attributed document fragment.");
	if (!changed) OnDocumentChanged(!editorMutation);
}

std::wstring RichTextBox::NormalizeLineBreaks(const std::wstring& text) const
{
	return CuiTextEdit::NormalizeInput(text, RichEditOptions());
}

bool RichTextBox::HasCrLfAt(int index) const
{
	return CuiTextEdit::HasCrLfAt(this->buffer, index);
}

bool RichTextBox::IsCaretBetweenCrLf(int index) const
{
	return CuiTextEdit::IsBetweenCrLf(this->buffer, index);
}

int RichTextBox::SnapCaretIndex(
	int index, LogicalDirection direction) const noexcept
{
	index = (std::clamp)(index, 0, static_cast<int>(buffer.size()));
	return static_cast<int>(CuiTextBoundary::SnapToTextElementBoundary(
		buffer, static_cast<std::size_t>(index),
		direction == LogicalDirection::Forward, RichTextIsMultiLine));
}

int RichTextBox::GetNextCaretIndex(int index) const
{
	return CuiTextEdit::GetNextCaretIndex(this->buffer, index, RichTextIsMultiLine);
}

int RichTextBox::GetPreviousCaretIndex(int index) const
{
	return CuiTextEdit::GetPreviousCaretIndex(this->buffer, index, RichTextIsMultiLine);
}

void RichTextBox::NormalizeSelectionRangeForErase(int& start, int& end) const
{
	CuiTextEdit::NormalizeSelectionForTextElements(this->buffer, start, end, RichTextIsMultiLine);
}

bool RichTextBox::GetBackspaceEraseRange(int caretIndex, int& eraseStart, int& eraseLength) const
{
	return CuiTextEdit::GetBackspaceEraseRange(this->buffer, caretIndex, RichTextIsMultiLine, eraseStart, eraseLength);
}

bool RichTextBox::GetDeleteEraseRange(int caretIndex, int& eraseStart, int& eraseLength) const
{
	return CuiTextEdit::GetDeleteEraseRange(this->buffer, caretIndex, RichTextIsMultiLine, eraseStart, eraseLength);
}

void RichTextBox::TrimToMaxLength()
{
	if (_maxLength == 0) return;
	if (this->buffer.size() <= static_cast<size_t>(_maxLength)) return;

	size_t removeCount = this->buffer.size()
		- static_cast<size_t>(_maxLength);
	removeCount = _flatDocument.SnapToBoundary(
		removeCount, RichTextBoundaryAffinity::Forward);
	if (removeCount == 0) return;

	RichTextDocument next(_flatDocument.ToFragment());
	(void)next.Replace(0, removeCount,
		RichTextDocumentFragment::FromPlainText(L""));

	int nextSelectionStart = std::max(
		0, this->_selectionStart - static_cast<int>(removeCount));
	int nextSelectionEnd = std::max(
		0, this->_selectionEnd - static_cast<int>(removeCount));
	const int nextLength = static_cast<int>(next.Length());
	if (nextSelectionStart > nextLength) nextSelectionStart = nextLength;
	if (nextSelectionEnd > nextLength) nextSelectionEnd = nextLength;
	ReplaceDocumentContent(
		next.ToFragment(), true,
		TextPointerTextChange{ 0, removeCount, 0 });
	this->_selectionStart = nextSelectionStart;
	this->_selectionEnd = nextSelectionEnd;
	this->_caretLogicalDirection = LogicalDirection::Forward;
	NotifySelectionChanged();
}

void RichTextBox::UpdateSelRange()
{
	if (!this->_textLayoutCache)
		return;
	auto font = this->GetRenderFont();
	int sels = _selectionStart <= _selectionEnd ? _selectionStart : _selectionEnd;
	int sele = _selectionEnd >= _selectionStart ? _selectionEnd : _selectionStart;
	if (sels == sele)
	{
		sels = sele = SnapCaretIndex(sels, _caretLogicalDirection);
	}
	else
	{
		NormalizeSelectionRangeForErase(sels, sele);
	}
	int selLen = sele - sels;
	selRange = font->HitTestTextRange(this->_textLayoutCache, (UINT32)sels, (UINT32)selLen);
	this->selRangeDirty = false;
}

void RichTextBox::ApplyTextDrawingEffects(
	IDWriteTextLayout* layout,
	int textStart,
	int textLength,
	bool includeSelection,
	ID2D1Brush* selectionTextBrush)
{
	if (!layout || !GetPresentationWindow() || !GetDrawingContext()) return;
	layout->SetDrawingEffect(nullptr, DWRITE_TEXT_RANGE{ 0, UINT_MAX });
	const int textEnd = textStart + (std::max)(0, textLength);
	for (const auto& span : _flatDocument.GetSpans())
	{
		const auto style = ResolveEffectiveCharacterStyle(span.Style);
		if (!style.Foreground) continue;
		const int rangeStart = (std::max)(
			static_cast<int>(span.Start), textStart);
		const int rangeEnd = (std::min)(
			static_cast<int>(span.End()), textEnd);
		if (rangeEnd <= rangeStart) continue;
		auto brush = CreateRichTextStyleBrush(*style.Foreground);
		if (!brush) continue;
		layout->SetDrawingEffect(
			brush,
			DWRITE_TEXT_RANGE{
				static_cast<UINT32>(rangeStart - textStart),
				static_cast<UINT32>(rangeEnd - rangeStart) });
	}
	if (!includeSelection) return;
	int normalizedStart = (std::min)(_selectionStart, _selectionEnd);
	int normalizedEnd = (std::max)(_selectionStart, _selectionEnd);
	if (normalizedEnd > normalizedStart)
		NormalizeSelectionRangeForErase(normalizedStart, normalizedEnd);
	const int selectionStart = (std::max)(normalizedStart, textStart);
	const int selectionEnd = (std::min)(normalizedEnd, textEnd);
	if (selectionEnd <= selectionStart) return;
	if (!selectionTextBrush) return;
	layout->SetDrawingEffect(
		selectionTextBrush,
		DWRITE_TEXT_RANGE{
			static_cast<UINT32>(selectionStart - textStart),
			static_cast<UINT32>(selectionEnd - selectionStart) });
}

void RichTextBox::ApplyTextLayoutFormatting(
	IDWriteTextLayout* layout, int textStart, int textLength)
{
	if (!layout) return;
	const int textEnd = textStart + (std::max)(0, textLength);
	for (const auto& span : _flatDocument.GetSpans())
	{
		const int rangeStart = (std::max)(
			static_cast<int>(span.Start), textStart);
		const int rangeEnd = (std::min)(
			static_cast<int>(span.End()), textEnd);
		if (rangeEnd <= rangeStart) continue;
		const DWRITE_TEXT_RANGE range{
			static_cast<UINT32>(rangeStart - textStart),
			static_cast<UINT32>(rangeEnd - rangeStart) };
		const auto style = ResolveEffectiveCharacterStyle(span.Style);
		if (style.Language)
			(void)layout->SetLocaleName(style.Language->c_str(), range);
		if (style.FontFamily)
			(void)layout->SetFontFamilyName(style.FontFamily->c_str(), range);
		if (style.FontSize)
			(void)layout->SetFontSize(*style.FontSize, range);
		if (style.FontWeight)
			(void)layout->SetFontWeight(*style.FontWeight, range);
		if (style.FontStretch)
			(void)layout->SetFontStretch(*style.FontStretch, range);
		if (style.FontStyle)
			(void)layout->SetFontStyle(*style.FontStyle, range);
		if (style.Underline)
			(void)layout->SetUnderline(*style.Underline, range);
		if (style.Strikethrough)
			(void)layout->SetStrikethrough(*style.Strikethrough, range);
	}
}

ID2D1Brush* RichTextBox::CreateRichTextStyleBrush(
	const cui::drawing::Brush& definition)
{
	auto drawingContext = GetDrawingContext();
	if (!drawingContext) return nullptr;
	const auto size = GetActualSizeDip();
	Microsoft::WRL::ComPtr<ID2D1Brush> brush;
	brush.Attach(definition.CreateBrush(
		*drawingContext,
		D2D1::SizeF((std::max)(1.0f, size.width),
			(std::max)(1.0f, size.height))));
	if (!brush) return nullptr;
	richTextStyleBrushes.push_back(std::move(brush));
	return richTextStyleBrushes.back().Get();
}

void RichTextBox::DrawTextStyleBackgrounds(
	IDWriteTextLayout* layout, int textStart, int textLength,
	float drawX, float drawY)
{
	if (!layout) return;
	auto font = GetRenderFont();
	auto d2d = GetDrawingContext();
	if (!font || !d2d) return;
	const int textEnd = textStart + (std::max)(0, textLength);
	for (const auto& span : _flatDocument.GetSpans())
	{
		const auto style = ResolveEffectiveCharacterStyle(span.Style);
		if (!style.Background) continue;
		const int rangeStart = (std::max)(
			static_cast<int>(span.Start), textStart);
		const int rangeEnd = (std::min)(
			static_cast<int>(span.End()), textEnd);
		if (rangeEnd <= rangeStart) continue;
		auto brush = CreateRichTextStyleBrush(*style.Background);
		if (!brush) continue;
		const auto ranges = font->HitTestTextRange(
			layout,
			static_cast<UINT32>(rangeStart - textStart),
			static_cast<UINT32>(rangeEnd - rangeStart));
		for (const auto& range : ranges)
		{
			d2d->FillRect(
				range.left + drawX, range.top + drawY,
				range.width, range.height, brush);
		}
	}
}

void RichTextBox::NotifyDeviceResourcesInvalidated() noexcept
{
	richTextStyleBrushes.clear();
	Control::NotifyDeviceResourcesInvalidated();
}

void RichTextBox::UpdateLayout()
{
	auto font = this->GetRenderFont();
	if (font != this->_lastLayoutFont)
	{
		this->_lastLayoutFont = font;
		this->_textLayoutDirty = true;
		this->selRangeDirty = true;
		this->blocksDirty = true;
		this->blockMetricsDirty = true;
		this->_caretRectCacheValid = false;
		ReleaseTextLayout();
		ReleaseBlocks();
	}

	if (!this->GetPresentationWindow())
		return;
	SyncBufferFromControlIfNeeded();
	if (this->_textLayoutDirty) UpdateParagraphLayoutState();

	this->_isVirtualized = _hasMixedParagraphAlignment
		|| _hasMixedParagraphFlowDirection
		|| (_enableVirtualization
			&& this->buffer.size() >= _virtualizeThreshold);
	if (this->_isVirtualized)
	{
		ReleaseTextLayout();

		float renderWidth = TextViewportWidth();
		float renderHeight = TextViewportHeight();

		if (this->_textLayoutDirty || this->lastLayoutSize.width != this->ActualWidth || this->lastLayoutSize.height != this->ActualHeight || this->blocksDirty)
		{
			RebuildBlocks();
			this->lastLayoutSize = { this->ActualWidth, this->ActualHeight };
			this->_textLayoutDirty = false;
		}

		EnsureAllBlockMetrics(renderWidth, renderHeight);
		this->_textSize.height = this->virtualTotalHeight;
		this->_textSize.width = renderWidth;
		this->selRangeDirty = true;
		return;
	}

	ReleaseBlocks();

	if ((this->_textLayoutDirty || this->lastLayoutSize.width != this->ActualWidth || this->lastLayoutSize.height != this->ActualHeight) && this->GetPresentationWindow())
	{
		// Text formatting is retained model state, not a render-target resource.
		// Input, caret and IME transactions may update it outside an active frame;
		// DrawingContext remains frame-only.
		ReleaseTextLayout();
		auto font = this->GetRenderFont();
		if (font && font->FontObject)
		{
			const float renderWidth =
				(std::max)(1.0f, TextViewportWidth());
			const float renderHeight =
				(std::max)(1.0f, TextViewportHeight());

			this->_textLayoutCache = Factory::CreateStringLayout(
				this->buffer, renderWidth, renderHeight, font->FontObject);
			if (this->_textLayoutCache)
			{
				(void)this->_textLayoutCache->SetReadingDirection(
					ToDirectWriteReadingDirection(_uniformFlowDirection));
				(void)this->_textLayoutCache->SetTextAlignment(
					ToDirectWriteAlignment(
						_uniformTextAlignment, _uniformFlowDirection));
			}
			ApplyRichTextWrapping(this->_textLayoutCache);
			ApplyTextLayoutFormatting(
				this->_textLayoutCache, 0,
				static_cast<int>(this->buffer.size()));
			_textSize = font->GetTextSize(_textLayoutCache);
			if (_textSize.height > renderHeight)
			{
				ReleaseTextLayout();
				this->_textLayoutCache = Factory::CreateStringLayout(
					this->buffer, (std::max)(1.0f, renderWidth - 8.0f),
					renderHeight, font->FontObject);
				if (this->_textLayoutCache)
				{
					(void)this->_textLayoutCache->SetReadingDirection(
						ToDirectWriteReadingDirection(_uniformFlowDirection));
					(void)this->_textLayoutCache->SetTextAlignment(
						ToDirectWriteAlignment(
							_uniformTextAlignment, _uniformFlowDirection));
				}
				ApplyRichTextWrapping(this->_textLayoutCache);
				ApplyTextLayoutFormatting(
					this->_textLayoutCache, 0,
					static_cast<int>(this->buffer.size()));
				_textSize = font->GetTextSize(_textLayoutCache);
			}
			if (this->_textLayoutCache)
			{
				_textLayoutDirty = false;
				this->lastLayoutSize = { this->ActualWidth, this->ActualHeight };
				this->selRangeDirty = true;
			}
		}
	}
}

void RichTextBox::ReleaseTextLayout() noexcept
{
	if (!this->_textLayoutCache) return;
	this->_textLayoutCache->Release();
	this->_textLayoutCache = nullptr;
}

void RichTextBox::ReleaseBlocks()
{
	for (auto& b : this->blocks)
	{
		if (b.layout)
		{
			b.layout->Release();
			b.layout = nullptr;
		}
	}
	this->blocks.clear();
	this->blockTops.clear();
	this->blocksDirty = true;
	this->blockMetricsDirty = true;
	this->virtualTotalHeight = 0.0f;
	this->layoutWidthHasScrollBar = false;
	this->_cachedRenderWidth = 0.0f;
}

void RichTextBox::RebuildBlocks()
{
	ReleaseBlocks();
	this->blocksDirty = false;
	this->blockMetricsDirty = true;

	const size_t bufferLength = this->buffer.size();
	const size_t blockSize = (std::max)((size_t)256, _blockCharCount);
	const auto fragment = _flatDocument.ToFragment();
	if (!fragment.StructureSpans.empty()
		|| !fragment.StructureMarkers.empty())
	{
		struct ParagraphSegment
		{
			std::uint64_t Id = 0;
			size_t Start = 0;
			size_t End = 0;
			bool HasBreak = false;
			::TextAlignment Alignment = ::TextAlignment::Left;
			::FlowDirection Direction = ::FlowDirection::LeftToRight;
		};
		std::vector<ParagraphSegment> paragraphs;
		RichTextParagraphStyle root;
		root.TextAlignment = _document
			? _document->GetTextAlignment() : ::TextAlignment::Left;
		root.FlowDirection = _document
			? _document->GetFlowDirection() : ::FlowDirection::LeftToRight;
		if (fragment.RootParagraphStyle
			&& fragment.RootParagraphStyle->TextAlignment)
			root.TextAlignment = fragment.RootParagraphStyle->TextAlignment;
		if (fragment.RootParagraphStyle
			&& fragment.RootParagraphStyle->FlowDirection)
			root.FlowDirection = fragment.RootParagraphStyle->FlowDirection;
		auto appendEvent = [&](const std::vector<RichTextStructureNode>& path,
			size_t start, size_t end, bool paragraphBreak)
		{
			if (path.empty()) return;
			const auto id = path.front().Id;
			if (paragraphs.empty() || paragraphs.back().Id != id)
			{
				auto alignment = root.TextAlignment.value_or(
					::TextAlignment::Left);
				auto direction = root.FlowDirection.value_or(
					::FlowDirection::LeftToRight);
				if (path.front().LocalParagraphStyle.TextAlignment)
					alignment = *path.front().LocalParagraphStyle.TextAlignment;
				if (path.front().LocalParagraphStyle.FlowDirection)
					direction = *path.front().LocalParagraphStyle.FlowDirection;
				paragraphs.push_back(ParagraphSegment{
					id, start, end, paragraphBreak, alignment, direction });
			}
			else
			{
				paragraphs.back().End = (std::max)(paragraphs.back().End, end);
				paragraphs.back().HasBreak = paragraphs.back().HasBreak
					|| paragraphBreak;
			}
		};
		size_t spanIndex = 0;
		size_t markerIndex = 0;
		while (spanIndex < fragment.StructureSpans.size()
			|| markerIndex < fragment.StructureMarkers.size())
		{
			const bool useMarker = markerIndex
				< fragment.StructureMarkers.size()
				&& (spanIndex >= fragment.StructureSpans.size()
					|| fragment.StructureMarkers[markerIndex].Position
						<= fragment.StructureSpans[spanIndex].Start);
			if (useMarker)
			{
				const auto& marker = fragment.StructureMarkers[markerIndex++];
				appendEvent(marker.Path, marker.Position, marker.Position, false);
			}
			else
			{
				const auto& span = fragment.StructureSpans[spanIndex++];
				appendEvent(span.Path, span.Start, span.End(),
					!span.Path.empty() && span.Path.back().Kind
						== RichTextStructureKind::ParagraphBreak);
			}
		}

		for (size_t first = 0; first < paragraphs.size();)
		{
			size_t last = first;
			const auto alignment = paragraphs[first].Alignment;
			const auto direction = paragraphs[first].Direction;
			while (last + 1 < paragraphs.size()
				&& paragraphs[last + 1].Alignment == alignment
				&& paragraphs[last + 1].Direction == direction
				&& paragraphs[last + 1].End - paragraphs[first].Start
					<= blockSize)
				++last;
			TextBlock block;
			block.start = paragraphs[first].Start;
			block.len = paragraphs[last].End - block.start;
			block.layoutLen = block.len;
			block.textAlignment = alignment;
			block.flowDirection = direction;
			const bool hasFollowingParagraph = last + 1 < paragraphs.size();
			if (hasFollowingParagraph && paragraphs[last].HasBreak
				&& block.layoutLen >= 2)
			{
				block.layoutLen -= 2;
				block.appendSentinel = true;
				block.sentinelStyleIndex = block.start + block.layoutLen;
			}
			else if (block.len == 0)
			{
				block.appendSentinel = true;
				block.sentinelStyleIndex = block.start;
				block.singleVisualLine = true;
			}
			blocks.push_back(block);
			first = last + 1;
		}
		if (!blocks.empty()) return;
	}
	if (bufferLength == 0) return;

	size_t blockStart = 0;
	while (blockStart < bufferLength)
	{
		const auto chunk = CuiTextEdit::FindSafeTextLayoutChunk(
			buffer, blockStart, blockSize);
		if (chunk.Length == 0) break;
		TextBlock block;
		block.start = blockStart;
		block.len = chunk.Length;
		block.layoutLen = chunk.LayoutLength;
		block.appendSentinel = chunk.LayoutLength < chunk.Length;
		block.sentinelStyleIndex = block.start + block.layoutLen;
		block.textAlignment = _uniformTextAlignment;
		block.flowDirection = _uniformFlowDirection;
		this->blocks.push_back(block);
		blockStart += chunk.Length;
	}
}

void RichTextBox::UpdateParagraphLayoutState()
{
	_uniformTextAlignment = _document
		? _document->GetTextAlignment() : ::TextAlignment::Left;
	_uniformFlowDirection = _document
		? _document->GetFlowDirection() : ::FlowDirection::LeftToRight;
	_hasMixedParagraphAlignment = false;
	_hasMixedParagraphFlowDirection = false;
	RichTextParagraphStyle baseline;
	baseline.TextAlignment = _uniformTextAlignment;
	baseline.FlowDirection = _uniformFlowDirection;
	auto styles = _flatDocument.ParagraphStylesInRange(
		0, _flatDocument.Length(), baseline);
	const auto trailing = _flatDocument.ParagraphStylesInRange(
		_flatDocument.Length(), 0, baseline);
	styles.insert(styles.end(), trailing.begin(), trailing.end());
	bool initialized = false;
	for (const auto& style : styles)
	{
		const auto alignment = style.TextAlignment.value_or(
			::TextAlignment::Left);
		const auto direction = style.FlowDirection.value_or(
			::FlowDirection::LeftToRight);
		if (!initialized)
		{
			_uniformTextAlignment = alignment;
			_uniformFlowDirection = direction;
			initialized = true;
		}
		else
		{
			if (_uniformTextAlignment != alignment)
				_hasMixedParagraphAlignment = true;
			if (_uniformFlowDirection != direction)
				_hasMixedParagraphFlowDirection = true;
		}
	}
}

void RichTextBox::ApplySentinelLayoutFormatting(
	IDWriteTextLayout* layout,
	UINT32 layoutPosition,
	size_t sourceStyleIndex)
{
	if (!layout) return;
	RichTextCharacterStyle sourceStyle;
	if (!buffer.empty())
	{
		sourceStyleIndex = (std::min)(sourceStyleIndex, buffer.size() - 1);
		sourceStyle = _flatDocument.StyleAt(sourceStyleIndex);
	}
	const auto style = ResolveEffectiveCharacterStyle(std::move(sourceStyle));
	const DWRITE_TEXT_RANGE sentinel{ layoutPosition, 1 };
	if (style.FontFamily)
		(void)layout->SetFontFamilyName(
			style.FontFamily->c_str(), sentinel);
	if (style.Language)
		(void)layout->SetLocaleName(style.Language->c_str(), sentinel);
	if (style.FontSize)
		(void)layout->SetFontSize(*style.FontSize, sentinel);
	if (style.FontWeight)
		(void)layout->SetFontWeight(*style.FontWeight, sentinel);
	if (style.FontStretch)
		(void)layout->SetFontStretch(*style.FontStretch, sentinel);
	if (style.FontStyle)
		(void)layout->SetFontStyle(*style.FontStyle, sentinel);
	if (style.Underline)
		(void)layout->SetUnderline(*style.Underline, sentinel);
	if (style.Strikethrough)
		(void)layout->SetStrikethrough(
			*style.Strikethrough, sentinel);
}

void RichTextBox::SplitLongBlocksIntoVisualLines(float renderWidth)
{
	if (blocks.empty()) return;
	auto font = GetRenderFont();
	if (!font || !font->FontObject) return;

	const size_t windowTarget = (std::max)(size_t{ 256 }, _blockCharCount);
	std::vector<TextBlock> refined;
	refined.reserve(blocks.size());

	for (auto& source : blocks)
	{
		if (source.layout)
		{
			source.layout->Release();
			source.layout = nullptr;
		}
		if (source.layoutLen <= windowTarget
			|| source.textAlignment == ::TextAlignment::Justify
			|| RequiresContinuousBidiLayout(
				buffer, source.start, source.layoutLen))
		{
			source.height = -1.0f;
			refined.push_back(source);
			continue;
		}

		std::vector<TextBlock> split;
		split.reserve(source.layoutLen / windowTarget + 2);
		size_t sourceOffset = 0;
		bool failed = false;
		while (sourceOffset < source.layoutLen && !failed)
		{
			const size_t remaining = source.layoutLen - sourceOffset;
			size_t candidateLength = (std::min)(windowTarget, remaining);
			std::vector<DWRITE_LINE_METRICS> lineMetrics;
			bool atSourceEnd = false;

			for (;;)
			{
				candidateLength = (std::min)(remaining,
					CuiTextEdit::ExpandChunkToTextElementBoundary(
						buffer, source.start + sourceOffset,
						candidateLength));
				atSourceEnd = candidateLength == remaining;
				std::wstring candidate = buffer.substr(
					source.start + sourceOffset, candidateLength);
				const bool appendSourceSentinel =
					atSourceEnd && source.appendSentinel;
				if (appendSourceSentinel)
					candidate.push_back(L'\u200B');

				Microsoft::WRL::ComPtr<IDWriteTextLayout> layout;
				layout.Attach(Factory::CreateStringLayout(
					std::move(candidate),
					(std::max)(1.0f, renderWidth), FLT_MAX,
					font->FontObject));
				if (!layout)
				{
					failed = true;
					break;
				}
				ApplyRichTextWrapping(layout.Get());
				(void)layout->SetReadingDirection(
					ToDirectWriteReadingDirection(source.flowDirection));
				(void)layout->SetTextAlignment(
					ToDirectWriteAlignment(
						source.textAlignment, source.flowDirection));
				ApplyTextLayoutFormatting(
					layout.Get(),
					static_cast<int>(source.start + sourceOffset),
					static_cast<int>(candidateLength));
				if (appendSourceSentinel)
					ApplySentinelLayoutFormatting(
						layout.Get(),
						static_cast<UINT32>(candidateLength),
						source.sentinelStyleIndex);
				if (!TryGetLineMetrics(layout.Get(), lineMetrics))
				{
					failed = true;
					break;
				}
				if (atSourceEnd || lineMetrics.size() >= 2)
					break;

				const size_t growth = (std::min)(
					remaining - candidateLength, candidateLength);
				if (growth == 0)
				{
					failed = true;
					break;
				}
				candidateLength += growth;
			}
			if (failed) break;

			const size_t commitCount = atSourceEnd
				? lineMetrics.size() : lineMetrics.size() - 1;
			if (commitCount == 0)
			{
				failed = true;
				break;
			}

			size_t consumed = 0;
			for (size_t lineIndex = 0;
				lineIndex < commitCount; ++lineIndex)
			{
				const auto& metric = lineMetrics[lineIndex];
				const size_t rawLength = metric.length;
				const bool consumesSourceSentinel =
					atSourceEnd && source.appendSentinel
					&& lineIndex + 1 == lineMetrics.size();

				TextBlock line;
				line.start = source.start + sourceOffset + consumed;
				line.height = metric.height > 0.0f
					? metric.height : font->FontHeight;
				line.singleVisualLine = true;
				line.textAlignment = source.textAlignment;
				if (rawLength == 0)
				{
					if (!atSourceEnd
						|| lineIndex + 1 != lineMetrics.size()
						|| source.len == 0)
					{
						failed = true;
						break;
					}
					line.start = source.start + source.len;
					line.appendSentinel = true;
					line.sentinelStyleIndex =
						source.start + source.len - 1;
					split.push_back(line);
					continue;
				}

				if (consumesSourceSentinel)
				{
					if (rawLength < 1)
					{
						failed = true;
						break;
					}
					line.layoutLen = rawLength - 1;
					line.len = line.layoutLen
						+ (source.len - source.layoutLen);
					line.appendSentinel = true;
					line.sentinelStyleIndex =
						source.sentinelStyleIndex;
					consumed += line.layoutLen;
				}
				else
				{
					line.len = rawLength;
					line.layoutLen = rawLength;
					if (metric.newlineLength > 0)
					{
						if (metric.newlineLength > rawLength)
						{
							failed = true;
							break;
						}
						line.layoutLen -= metric.newlineLength;
						line.appendSentinel = true;
						line.sentinelStyleIndex =
							line.start + line.layoutLen;
					}
					consumed += rawLength;
				}
				split.push_back(line);
			}
			if (failed || consumed == 0)
			{
				failed = true;
				break;
			}
			sourceOffset += consumed;
		}

		size_t claimedEnd = source.start;
		if (!failed)
		{
			for (const auto& line : split)
			{
				if (line.start != claimedEnd)
				{
					failed = true;
					break;
				}
				claimedEnd += line.len;
			}
			if (claimedEnd != source.start + source.len
				|| sourceOffset != source.layoutLen
				|| split.size() < 2)
				failed = true;
		}

		if (failed)
		{
			source.height = -1.0f;
			refined.push_back(source);
		}
		else
		{
			refined.insert(refined.end(),
				split.begin(), split.end());
		}
	}

	blocks = std::move(refined);
}

void RichTextBox::EnsureBlockLayout(int blockIndex, float renderWidth, float renderHeight)
{
	if (blockIndex < 0 || blockIndex >= (int)this->blocks.size()) return;
	auto& block = this->blocks[blockIndex];
	if (block.layout && block.height >= 0.0f) return;

	auto font = this->GetRenderFont();
	if (!font || !font->FontObject)
	{
		block.height = 0.0f;
		return;
	}

	std::wstring blockText = this->buffer.substr(
		block.start, block.layoutLen);
	if (block.appendSentinel)
		blockText.push_back(L'\u200B');
	block.layout = Factory::CreateStringLayout(
		std::move(blockText), (std::max)(1.0f, renderWidth),
		FLT_MAX, font->FontObject);
	if (block.layout)
		block.layout->SetReadingDirection(
			ToDirectWriteReadingDirection(block.flowDirection));
	if (block.layout)
		block.layout->SetTextAlignment(
			ToDirectWriteAlignment(
				block.textAlignment, block.flowDirection));
	if (block.layout)
		block.layout->SetWordWrapping(block.singleVisualLine
			? DWRITE_WORD_WRAPPING_NO_WRAP
			: DWRITE_WORD_WRAPPING_CHARACTER);
	ApplyTextLayoutFormatting(
		block.layout, static_cast<int>(block.start),
		static_cast<int>(block.layoutLen));
	if (block.appendSentinel)
		ApplySentinelLayoutFormatting(
			block.layout,
			static_cast<UINT32>(block.layoutLen),
			block.sentinelStyleIndex);
	float measuredHeight = 0.0f;
	if (block.layout)
	{
		DWRITE_TEXT_METRICS metrics{};
		if (SUCCEEDED(block.layout->GetMetrics(&metrics)))
			measuredHeight = metrics.height;
	}
	if (block.height < 0.0f)
		block.height = measuredHeight > 0.0f
			? measuredHeight : font->FontHeight;
}

void RichTextBox::EnsureAllBlockMetrics(float renderWidth, float renderHeight)
{
	if (!this->blockMetricsDirty && this->_cachedRenderWidth == renderWidth)
		return;

	auto computeTotalHeight = [&](float layoutWidth)
	{
		// Recreate paragraph-safe source blocks for every width pass. The first
		// pass may discover that a scrollbar is required; visual-line boundaries
		// must then be profiled again at the narrower final width.
		RebuildBlocks();
		SplitLongBlocksIntoVisualLines(layoutWidth);
		this->blockTops.resize(this->blocks.size());
		for (auto& block : this->blocks)
		{
			if (block.layout)
			{
				block.layout->Release();
				block.layout = nullptr;
			}
			if (!block.singleVisualLine)
				block.height = -1.0f;
		}
		float blockTop = 0.0f;
		for (int i = 0; i < (int)this->blocks.size(); i++)
		{
			this->blockTops[i] = blockTop;
			EnsureBlockLayout(i, layoutWidth, renderHeight);
			blockTop += this->blocks[i].height;
			// Height is the persistent virtualization metric. The expensive
			// DirectWrite layout is realized again only when this block enters
			// the viewport (or is needed for an explicit caret/hit test).
			if (this->blocks[i].layout)
			{
				this->blocks[i].layout->Release();
				this->blocks[i].layout = nullptr;
			}
		}
		return blockTop;
		};

	float totalHeight = computeTotalHeight(renderWidth);
	bool needsScrollBar = totalHeight > renderHeight;
	if (needsScrollBar)
	{
		totalHeight = computeTotalHeight(std::max(0.0f, renderWidth - 8.0f));
		this->layoutWidthHasScrollBar = true;
	}
	else
	{
		this->layoutWidthHasScrollBar = false;
	}
	this->virtualTotalHeight = totalHeight;
	this->_cachedRenderWidth = renderWidth;
	this->blocksDirty = false;
	this->blockMetricsDirty = false;
}

int RichTextBox::HitTestGlobalIndex(float x, float y)
{
	if (!this->_isVirtualized || this->blocks.empty()) return 0;
	float renderHeight = TextViewportHeight();
	float renderWidth = TextViewportWidth();
	if (this->layoutWidthHasScrollBar) renderWidth -= 8.0f;

	float contentY = (y + this->_verticalScrollOffset) - Padding.Top;
	if (contentY < 0) contentY = 0;

	int blockIndex = 0;
	for (int i = 0; i < (int)this->blockTops.size(); i++)
	{
		if (contentY >= this->blockTops[i])
			blockIndex = i;
		else
			break;
	}
	EnsureBlockLayout(blockIndex, renderWidth, renderHeight);
	float yInBlock = contentY - this->blockTops[blockIndex];
	float xInBlock = x - Padding.Left;
	if (xInBlock < 0) xInBlock = 0;

	int localIndex = this->GetRenderFont()->HitTestTextPosition(
		this->blocks[blockIndex].layout, xInBlock, yInBlock);
	localIndex = (std::clamp)(localIndex, 0,
		static_cast<int>(this->blocks[blockIndex].layoutLen));
	int globalIndex = (int)this->blocks[blockIndex].start + localIndex;
	globalIndex = std::clamp(globalIndex, 0, (int)this->buffer.size());
	return SnapCaretIndex(globalIndex, LogicalDirection::Forward);
}

bool RichTextBox::GetCaretMetrics(
	int caretIndex, float& outX, float& outY, float& outH)
{
	return GetCaretMetrics(
		caretIndex, _caretLogicalDirection, outX, outY, outH);
}

int RichTextBox::GetCaretBlockIndex(
	int caretIndex, LogicalDirection direction) const noexcept
{
	if (blocks.empty()) return -1;
	caretIndex = (std::clamp)(
		caretIndex, 0, static_cast<int>(buffer.size()));
	for (std::size_t index = 0; index < blocks.size(); ++index)
	{
		const auto start = static_cast<int>(blocks[index].start);
		const auto end = static_cast<int>(
			blocks[index].start + blocks[index].len);
		if (caretIndex < start)
			return index == 0 ? 0 : static_cast<int>(index - 1);
		if (caretIndex > end) continue;
		if (caretIndex > start && caretIndex < end)
			return static_cast<int>(index);
		if (caretIndex == start)
		{
			if (direction == LogicalDirection::Forward || index == 0)
				return static_cast<int>(index);
			const auto previousEnd = blocks[index - 1].start
				+ blocks[index - 1].len;
			return previousEnd == blocks[index].start
				? static_cast<int>(index - 1)
				: static_cast<int>(index);
		}
		if (direction == LogicalDirection::Backward
			|| index + 1 == blocks.size()
			|| blocks[index + 1].start
				!= blocks[index].start + blocks[index].len)
		{
			return static_cast<int>(index);
		}
		return static_cast<int>(index + 1);
	}
	return static_cast<int>(blocks.size() - 1);
}

bool RichTextBox::GetCaretMetrics(
	int caretIndex, LogicalDirection direction,
	float& outX, float& outY, float& outH)
{
	outX = outY = outH = 0.0f;
	caretIndex = SnapCaretIndex(caretIndex, direction);
	DWRITE_HIT_TEST_METRICS hit{};
	if (!this->_isVirtualized)
	{
		if (!_textLayoutCache
			|| !TryGetDirectionalCaretMetrics(
				_textLayoutCache, static_cast<UINT32>(caretIndex),
				direction, hit))
		{
			return false;
		}
		outX = hit.left + Padding.Left;
		outY = hit.top - _verticalScrollOffset + Padding.Top;
		outH = hit.height;
		return true;
	}
	if (this->blocks.empty()) return false;

	float renderHeight = TextViewportHeight();
	float renderWidth = TextViewportWidth();
	if (this->layoutWidthHasScrollBar) renderWidth -= 8.0f;
	const int blockIndex = GetCaretBlockIndex(caretIndex, direction);
	if (blockIndex < 0) return false;
	EnsureBlockLayout(blockIndex, renderWidth, renderHeight);
	const auto& block = this->blocks[blockIndex];
	if (!block.layout) return false;
	int localIndex = caretIndex - static_cast<int>(block.start);
	localIndex = (std::clamp)(localIndex, 0,
		static_cast<int>(block.layoutLen));
	if (!TryGetDirectionalCaretMetrics(
		block.layout, static_cast<UINT32>(localIndex), direction, hit))
	{
		return false;
	}
	outX = hit.left + Padding.Left;
	outY = (this->blockTops[blockIndex] + hit.top)
		- this->_verticalScrollOffset + Padding.Top;
	outH = hit.height;
	return true;
}

void RichTextBox::ClearSuggestedCaretX() noexcept
{
	_suggestedCaretX.reset();
	_suggestedCaretIndex = -1;
}

bool RichTextBox::TryGetSuggestedCaretX(
	int caretIndex, float& outX, float& outY, float& outHeight)
{
	float currentX = 0.0f;
	if (!GetCaretMetrics(caretIndex, currentX, outY, outHeight))
		return false;
	if (!_suggestedCaretX || _suggestedCaretIndex != caretIndex)
		_suggestedCaretX = currentX;
	outX = *_suggestedCaretX;
	return true;
}

int RichTextBox::GetVisualLineBoundary(int caretIndex, bool lineEnd)
{
	SyncBufferFromControlIfNeeded();
	caretIndex = SnapCaretIndex(caretIndex,
		lineEnd ? LogicalDirection::Forward : LogicalDirection::Backward);
	const int logicalFallback = lineEnd
		? CuiTextEdit::GetLineEndIndex(buffer, caretIndex)
		: CuiTextEdit::GetLineStartIndex(buffer, caretIndex);
	const bool rightToLeft = GetParagraphFlowDirectionAt(caretIndex)
		== ::FlowDirection::RightToLeft;
	const bool physicalRight = lineEnd ? !rightToLeft : rightToLeft;
	UpdateLayout();
	if (_isVirtualized)
	{
		float x = 0.0f;
		float y = 0.0f;
		float height = 0.0f;
		if (!GetCaretMetrics(caretIndex, x, y, height))
			return logicalFallback;
		return HitTestGlobalIndex(
			physicalRight ? TextViewportWidth() : Padding.Left,
			y + height * 0.5f);
	}
	if (!_textLayoutCache) return logicalFallback;
	auto* font = GetRenderFont();
	if (!font) return logicalFallback;
	auto caret = font->HitTestTextRange(
		_textLayoutCache,
		static_cast<UINT32>(caretIndex),
		0);
	if (caret.empty()) return logicalFallback;
	return SnapCaretIndex(font->HitTestTextPosition(
		_textLayoutCache,
		physicalRight ? TextViewportWidth() : 0.0f,
		caret.front().top + caret.front().height * 0.5f),
		lineEnd ? LogicalDirection::Forward
			: LogicalDirection::Backward);
}

int RichTextBox::GetVerticalCaretIndex(
	int caretIndex, bool down,
	LogicalDirection& targetDirection)
{
	SyncBufferFromControlIfNeeded();
	UpdateLayout();
	const LogicalDirection sourceDirection = _caretLogicalDirection;
	targetDirection = sourceDirection;
	caretIndex = SnapCaretIndex(caretIndex, sourceDirection);
	float x = 0.0f;
	float y = 0.0f;
	float height = 0.0f;
	if (!TryGetSuggestedCaretX(caretIndex, x, y, height))
		return caretIndex;
	(void)y;
	(void)height;
	const float layoutX = (std::max)(0.0f, x - Padding.Left);
	auto logicalLength = [](std::size_t value) noexcept
	{
		return static_cast<UINT32>((std::min)(value,
			static_cast<std::size_t>(
				(std::numeric_limits<UINT32>::max)())));
	};
	auto resolveTarget = [&](
		IDWriteTextLayout* layout,
		const RichTextVisualLineMetrics& line,
		std::size_t globalStart,
		int& target) -> bool
	{
		UINT32 localTarget = 0;
		LogicalDirection hitDirection = sourceDirection;
		if (!TryHitTestVisualLine(
			layout, line, layoutX, localTarget, hitDirection))
		{
			return false;
		}
		const auto globalTarget = (std::min)(
			globalStart + static_cast<std::size_t>(localTarget),
			buffer.size());
		targetDirection = hitDirection;
		target = SnapCaretIndex(
			static_cast<int>(globalTarget), targetDirection);
		return true;
	};

	std::vector<RichTextVisualLineMetrics> lines;
	int target = caretIndex;
	if (!_isVirtualized)
	{
		if (!_textLayoutCache
			|| !TryBuildVisualLineMetrics(
				_textLayoutCache, logicalLength(buffer.size()), lines))
		{
			return caretIndex;
		}
		const auto currentLine = FindVisualLineAtPosition(
			lines, static_cast<UINT32>(caretIndex), sourceDirection);
		if ((down && currentLine + 1 >= lines.size())
			|| (!down && currentLine == 0))
		{
			return caretIndex;
		}
		const auto targetLine = down ? currentLine + 1 : currentLine - 1;
		if (!resolveTarget(
			_textLayoutCache, lines[targetLine], 0, target))
		{
			return caretIndex;
		}
	}
	else
	{
		if (blocks.empty()) return caretIndex;
		float renderWidth = TextViewportWidth();
		const float renderHeight = TextViewportHeight();
		if (layoutWidthHasScrollBar) renderWidth -= 8.0f;
		const int currentBlock = GetCaretBlockIndex(
			caretIndex, sourceDirection);
		if (currentBlock < 0) return caretIndex;
		EnsureBlockLayout(currentBlock, renderWidth, renderHeight);
		auto* currentLayout = blocks[currentBlock].layout;
		if (!currentLayout
			|| !TryBuildVisualLineMetrics(
				currentLayout,
				logicalLength(blocks[currentBlock].layoutLen), lines))
		{
			return caretIndex;
		}
		const int localCaret = (std::clamp)(
			caretIndex - static_cast<int>(blocks[currentBlock].start),
			0, static_cast<int>(blocks[currentBlock].layoutLen));
		const auto currentLine = FindVisualLineAtPosition(
			lines, static_cast<UINT32>(localCaret), sourceDirection);

		int targetBlock = currentBlock;
		std::size_t targetLine = currentLine;
		if (down && currentLine + 1 < lines.size())
		{
			targetLine = currentLine + 1;
		}
		else if (!down && currentLine > 0)
		{
			targetLine = currentLine - 1;
		}
		else
		{
			bool found = false;
			for (int candidate = currentBlock + (down ? 1 : -1);
				candidate >= 0
					&& candidate < static_cast<int>(blocks.size());
				candidate += down ? 1 : -1)
			{
				EnsureBlockLayout(candidate, renderWidth, renderHeight);
				auto* candidateLayout = blocks[candidate].layout;
				std::vector<RichTextVisualLineMetrics> candidateLines;
				if (!candidateLayout
					|| !TryBuildVisualLineMetrics(
						candidateLayout,
						logicalLength(blocks[candidate].layoutLen),
						candidateLines))
				{
					continue;
				}
				targetBlock = candidate;
				targetLine = down ? 0 : candidateLines.size() - 1;
				lines = std::move(candidateLines);
				found = true;
				break;
			}
			if (!found) return caretIndex;
		}
		if (!resolveTarget(
			blocks[targetBlock].layout, lines[targetLine],
			blocks[targetBlock].start, target))
		{
			return caretIndex;
		}
	}
	_suggestedCaretIndex = target;
	return target;
}

int RichTextBox::GetPageCaretIndex(int caretIndex, bool down)
{
	SyncBufferFromControlIfNeeded();
	UpdateLayout();
	caretIndex = SnapCaretIndex(caretIndex,
		down ? LogicalDirection::Forward : LogicalDirection::Backward);
	float x = 0.0f;
	float y = 0.0f;
	float height = 0.0f;
	if (!TryGetSuggestedCaretX(caretIndex, x, y, height))
		return caretIndex;
	const float pageHeight = TextViewportHeight();
	if (pageHeight <= 0.0f) return caretIndex;
	const float targetY = down
		? y + pageHeight : y + height - pageHeight;
	int target = caretIndex;
	if (_isVirtualized)
		target = HitTestGlobalIndex(x, targetY);
	else if (_textLayoutCache)
	{
		auto* font = GetRenderFont();
		if (font)
			target = font->HitTestTextPosition(
				_textLayoutCache,
				(std::max)(0.0f, x - Padding.Left),
				(targetY + _verticalScrollOffset) - Padding.Top);
	}
	target = SnapCaretIndex(target,
		down ? LogicalDirection::Forward : LogicalDirection::Backward);
	if ((down && target <= caretIndex) || (!down && target >= caretIndex))
	{
		target = down ? static_cast<int>(buffer.size()) : 0;
		ClearSuggestedCaretX();
		return target;
	}
	_suggestedCaretIndex = target;
	return target;
}

int RichTextBox::GetParagraphNavigationTarget(
	int caretIndex, bool down,
	bool preferPreviousAtBoundary) const
{
	const auto ranges = _flatDocument.ParagraphRanges();
	caretIndex = (std::clamp)(
		caretIndex, 0, static_cast<int>(buffer.size()));
	if (ranges.empty())
	{
		if (down)
		{
			const int end = CuiTextEdit::GetLineEndIndex(buffer, caretIndex);
			return HasCrLfAt(end) ? end + 2 : end;
		}
		const int start = CuiTextEdit::GetLineStartIndex(buffer, caretIndex);
		return caretIndex > start
			? start : CuiTextEdit::GetLineStartIndex(buffer, (std::max)(0, start - 1));
	}

	std::size_t current = 0;
	for (std::size_t index = 0; index < ranges.size(); ++index)
	{
		if (ranges[index].Start > static_cast<std::size_t>(caretIndex))
			break;
		if (preferPreviousAtBoundary && index > 0
			&& ranges[index].Start == static_cast<std::size_t>(caretIndex))
		{
			current = index - 1;
			break;
		}
		current = index;
	}

	if (down)
	{
		return static_cast<int>(current + 1 < ranges.size()
			? ranges[current + 1].Start : ranges[current].End());
	}
	if (static_cast<std::size_t>(caretIndex) > ranges[current].Start)
		return static_cast<int>(ranges[current].Start);
	return current > 0
		? static_cast<int>(ranges[current - 1].Start) : 0;
}

void RichTextBox::DrawScroll()
{
	if (!IsVerticalScrollBarVisible()) return;
	auto d2d = this->GetDrawingContext();
	float renderHeight = TextViewportHeight();
	float maxScroll = _textSize.height - renderHeight;
	if (this->_verticalScrollOffset > maxScroll)
	{
		this->_verticalScrollOffset = maxScroll;
		if (this->_verticalScrollOffset < 0)this->_verticalScrollOffset = 0;
	}
	if (_textSize.height > renderHeight)
	{
		float scrollThumbHeight = (renderHeight / _textSize.height) * renderHeight;
		if (scrollThumbHeight < this->ActualHeight * 0.1f)scrollThumbHeight = this->ActualHeight * 0.1f;
		float scrollThumbMoveSpace = this->ActualHeight - scrollThumbHeight;
		float scrollRatio = (float)this->_verticalScrollOffset / (float)maxScroll;
		float scrollThumbTop = scrollRatio * scrollThumbMoveSpace;
		// 局部坐标：滚动条 X = Width - 8，Y = 0
		d2d->FillRoundRect(this->ActualWidth - 8.0f, 0, 8.0f,
			static_cast<float>(this->ActualHeight), _scrollBackColor, 4.0f);
		d2d->FillRoundRect(this->ActualWidth - 8.0f, scrollThumbTop,
			8.0f, scrollThumbHeight, _scrollForeColor, 4.0f);
	}
}

void RichTextBox::ScrollToEnd()
{
	this->UpdateLayout();
	float renderHeight = TextViewportHeight();
	float maxScroll = _textSize.height - renderHeight;
	this->_verticalScrollOffset = maxScroll;
	if (this->_verticalScrollOffset < 0)this->_verticalScrollOffset = 0;
	this->_selectionEnd = this->_selectionStart = (int)this->buffer.size();
	NotifySelectionChanged();
	this->InvalidateVisual();
}

void RichTextBox::ScrollSelectionIntoView()
{
	SyncBufferFromControlIfNeeded();
	const int textLength = static_cast<int>(buffer.size());
	_selectionStart = (std::clamp)(_selectionStart, 0, textLength);
	_selectionEnd = (std::clamp)(_selectionEnd, 0, textLength);
	NotifySelectionChanged();
	UpdateScroll(_selectionEnd >= textLength);
	selRangeDirty = true;
	InvalidateVisual();
}

void RichTextBox::SetHighlightRanges(
	std::vector<RichTextBoxTextRange> ranges)
{
	SyncBufferFromControlIfNeeded();
	std::vector<RichTextBoxTextRange> normalized;
	normalized.reserve(ranges.size());
	const int textLength = static_cast<int>(buffer.size());
	for (auto range : ranges)
	{
		range.Start = (std::clamp)(range.Start, 0, textLength);
		range.Length = (std::clamp)(range.Length, 0,
			textLength - range.Start);
		if (range.Length > 0) normalized.push_back(range);
	}
	highlightRanges = std::move(normalized);
	InvalidateVisual();
}

void RichTextBox::ClearHighlightRanges()
{
	if (highlightRanges.empty()) return;
	highlightRanges.clear();
	InvalidateVisual();
}

bool RichTextBox::TryGetTextInputCaretRect(D2D1_RECT_F& outRect)
{
	outRect = D2D1::RectF();
	if (!GetPresentationWindow()) return false;
	UpdateLayout();
	SyncBufferFromControlIfNeeded();
	const auto absolute = GetAbsoluteLocationDip();
	float x = Padding.Left;
	float y = Padding.Top;
	float height = GetRenderFont() ? GetRenderFont()->FontHeight : 16.0f;
	if (!buffer.empty()
		&& !GetCaretMetrics(_selectionEnd, x, y, height))
	{
		return false;
	}
	outRect = TransformAbsoluteRectToRenderSpace(D2D1::RectF(
		static_cast<float>(absolute.x) + x,
		static_cast<float>(absolute.y) + y,
		static_cast<float>(absolute.x) + x + 1.0f,
		static_cast<float>(absolute.y) + y + (std::max)(1.0f, height)));
	return true;
}

bool RichTextBox::ApplyTextInput(const TextCompositionEventArgs& input)
{
	if (_isReadOnly || input.Text.empty()) return false;
	InputText(input.Text);
	UpdateScroll(_selectionEnd >= static_cast<int>(buffer.size()));
	InvalidateVisual();
	return true;
}

void RichTextBox::UpdateScrollDrag(float posY) {
	if (!isDraggingScroll) return;

	float renderHeight = TextViewportHeight();
	float maxScroll = _textSize.height - renderHeight;

	float scrollBlockHeight = (renderHeight / _textSize.height) * renderHeight;
	if (scrollBlockHeight < this->ActualHeight * 0.1f)scrollBlockHeight = this->ActualHeight * 0.1f;

	float scrollHeight = this->ActualHeight - scrollBlockHeight;
	if (scrollHeight <= 0.0f) return;
	float thumbGrabOffset = std::clamp(_verticalScrollThumbGrabOffset, 0.0f, scrollBlockHeight);
	float targetTop = posY - thumbGrabOffset;
	float scrollRatio = targetTop / scrollHeight;
	scrollRatio = std::clamp(scrollRatio, 0.0f, 1.0f);
	float newScroll = scrollRatio * maxScroll;
	{
		this->_verticalScrollOffset = newScroll;
		if (this->_verticalScrollOffset < 0) this->_verticalScrollOffset = 0;
		if (this->_verticalScrollOffset > maxScroll + 1) this->_verticalScrollOffset = maxScroll + 1;
		InvalidateVisual();
	}
}
void RichTextBox::SetScrollByPos(float localY)
{
	const float renderHeight = TextViewportHeight();
	if (renderHeight <= 0.0f || _textSize.height <= 0.0f)
	{
		this->_verticalScrollOffset = 0.0f;
		return;
	}

	if (_textSize.height <= renderHeight)
	{
		this->_verticalScrollOffset = 0.0f;
		return;
	}

	const float maxScroll = std::max(0.0f, _textSize.height - renderHeight);

	float scrollBlockHeight = (renderHeight / _textSize.height) * renderHeight;
	if (scrollBlockHeight < this->ActualHeight * 0.1f) scrollBlockHeight = this->ActualHeight * 0.1f;
	if (scrollBlockHeight > static_cast<float>(this->ActualHeight)) scrollBlockHeight = static_cast<float>(this->ActualHeight);

	const float topPosition = scrollBlockHeight * 0.5f;
	const float bottomPosition = this->ActualHeight - topPosition;
	if (bottomPosition > topPosition)
	{
		const float percent = std::clamp((localY - topPosition) / (bottomPosition - topPosition), 0.0f, 1.0f);
		this->_verticalScrollOffset = maxScroll * percent;
	}
	this->_verticalScrollOffset = std::clamp(this->_verticalScrollOffset, 0.0f, maxScroll);
}

RichTextCharacterStyle RichTextBox::ResolveEffectiveCharacterStyle(
	RichTextCharacterStyle style) const
{
	RichTextCharacterStyle fallback;
	if (_document && !_documentIsImplicit)
	{
		fallback.Foreground = _document->GetForeground();
		fallback.Background = _document->GetBackground();
		fallback.FontFamily = _document->GetFontFamily();
		fallback.Language = _document->GetLanguage();
		fallback.FontSize = static_cast<float>(_document->GetFontSize());
		fallback.FontWeight = _document->GetFontWeight();
		fallback.FontStretch = _document->GetFontStretch();
		fallback.FontStyle = _document->GetFontStyle();
		fallback.Underline = _document->GetUnderline();
		fallback.Strikethrough = _document->GetStrikethrough();
	}
	else
	{
		auto* mutableThis = const_cast<RichTextBox*>(this);
		fallback.Foreground = mutableThis->GetForeground();
		fallback.Background = cui::drawing::NoBrush();
		fallback.FontFamily = mutableThis->GetFontFamily();
		fallback.Language = mutableThis->GetLanguage();
		fallback.FontSize = static_cast<float>(mutableThis->GetFontSize());
		fallback.FontWeight = DWRITE_FONT_WEIGHT_NORMAL;
		fallback.FontStretch = DWRITE_FONT_STRETCH_NORMAL;
		fallback.FontStyle = DWRITE_FONT_STYLE_NORMAL;
		fallback.Underline = false;
		fallback.Strikethrough = false;
	}
	if (!style.Foreground) style.Foreground = std::move(fallback.Foreground);
	if (!style.Background) style.Background = std::move(fallback.Background);
	if (!style.FontFamily) style.FontFamily = std::move(fallback.FontFamily);
	if (!style.Language) style.Language = std::move(fallback.Language);
	if (!style.FontSize) style.FontSize = fallback.FontSize;
	if (!style.FontWeight) style.FontWeight = fallback.FontWeight;
	if (!style.FontStretch) style.FontStretch = fallback.FontStretch;
	if (!style.FontStyle) style.FontStyle = fallback.FontStyle;
	if (!style.Underline) style.Underline = fallback.Underline;
	if (!style.Strikethrough)
		style.Strikethrough = fallback.Strikethrough;
	return style;
}

RichTextDocumentFragment RichTextBox::CreateClipboardFragment() const
{
	const int start = (std::min)(_selectionStart, _selectionEnd);
	const int end = (std::max)(_selectionStart, _selectionEnd);
	auto result = _flatDocument.Extract(
		static_cast<std::size_t>((std::max)(0, start)),
		static_cast<std::size_t>((std::max)(0, end - start)));
	std::vector<RichTextStyleSpan> spans;
	spans.reserve(result.Spans.size());
	for (const auto& source : result.Spans)
	{
		auto style = ResolveEffectiveCharacterStyle(source.Style);
		if (!spans.empty() && spans.back().End() == source.Start
			&& spans.back().Style == style)
		{
			spans.back().Length += source.Length;
		}
		else
		{
			spans.push_back(RichTextStyleSpan{
				source.Start, source.Length, std::move(style) });
		}
	}
	auto portable =
		cui::richtext::clipboard::MakePortableStructuredFragment(
			result, std::move(spans));
	if (!portable)
		throw std::logic_error(
			"RichTextBox clipboard projection is not portable.");
	return std::move(*portable);
}

RichTextCharacterStyle RichTextBox::EffectiveTypingStyle() const
{
	if (_typingStyle) return *_typingStyle;
	const int start = (std::min)(_selectionStart, _selectionEnd);
	const int end = (std::max)(_selectionStart, _selectionEnd);
	if (end > start)
		return _flatDocument.StyleAt(static_cast<std::size_t>(start));
	const auto insertionPosition = _flatDocument.SnapToBoundary(
		static_cast<std::size_t>((std::max)(0, start)),
		RichTextBoundaryAffinity::Forward);
	RichTextCharacterStyle paragraphStyle;
	if (_document && _document->TryGetParagraphInsertionStyleAt(
		insertionPosition, paragraphStyle))
	{
		return paragraphStyle;
	}
	if (_flatDocument.Empty())
	{
		RichTextCharacterStyle documentStyle;
		if (_document) _document->ApplyLocalCharacterStyle(documentStyle);
		return documentStyle;
	}
	return _flatDocument.InsertionStyleAt(
		insertionPosition, RichTextBoundaryAffinity::Backward);
}

RichTextDocumentChange RichTextBox::ReplaceRangeWithFragment(
	int start,
	int end,
	RichTextDocumentFragment fragment,
	bool collapseSelection)
{
	SyncBufferFromControlIfNeeded();
	RichTextDocument next(_flatDocument.ToFragment());
	if (end < start) std::swap(start, end);
	auto change = next.Replace(
		static_cast<std::size_t>((std::max)(0, start)),
		static_cast<std::size_t>((std::max)(0, end - start)), fragment);
	const int caret = static_cast<int>(
		change.Start + change.After.Text.size());
	if (change.Changed())
	{
		HistoryVisibilityScope historyVisibility(
			*this,
			CanUndo() || (_isUndoEnabled && _undoLimit != 0
				&& !isApplyingUndoRedo),
			false);
		ReplaceDocumentContent(
			next.ToFragment(), true,
			change.TextChanged()
				? std::optional<TextPointerTextChange>(
					TextPointerTextChange{
						change.Start,
						change.Before.Text.size(),
						change.After.Text.size() })
				: std::nullopt);
	}
	if (collapseSelection)
	{
		_selectionStart = _selectionEnd = caret;
		_caretLogicalDirection = LogicalDirection::Forward;
		NotifySelectionChanged();
	}
	return change;
}

RichTextDocumentChange RichTextBox::ReplaceTextRangeContent(
	std::size_t start,
	std::size_t length,
	RichTextDocumentFragment fragment)
{
	if (_mutatingDocumentFromEditor)
		throw std::logic_error(
			"TextRange cannot mutate a RichTextBox during document publication.");
	const auto maximum = static_cast<std::size_t>(
		(std::numeric_limits<int>::max)());
	if (start > maximum || length > maximum - start)
		throw std::out_of_range("TextRange exceeds RichTextBox offset limits.");
	if (!fragment.ValidateCanonical())
		throw std::invalid_argument(
			"TextRange replacement fragment is not canonical.");

	SyncBufferFromControlIfNeeded();
	EditorNotificationScope notifications(*this);
	const int selectionStartBefore = _selectionStart;
	const int selectionEndBefore = _selectionEnd;
	const auto caretDirectionBefore = _caretLogicalDirection;
	const auto typingBefore = _typingStyle;
	const bool storeHistory = !isApplyingUndoRedo
		&& _isUndoEnabled && _undoLimit != 0;
	if (storeHistory) undoStack.reserve(undoStack.size() + 1);

	auto change = ReplaceRangeWithFragment(
		static_cast<int>(start),
		static_cast<int>(start + length),
		std::move(fragment), false);
	if (change.Changed())
	{
		UndoRecord record;
		record.pos = static_cast<int>(change.Start);
		record.removedFragment = change.Before;
		record.insertedFragment = _flatDocument.Extract(
			change.Start, change.After.Text.size());
		record.typingStyleBefore = typingBefore;
		record.typingStyleAfter = _typingStyle;
		record.selStartBefore = selectionStartBefore;
		record.selEndBefore = selectionEndBefore;
		record.selStartAfter = _selectionStart;
		record.selEndAfter = _selectionEnd;
		record.caretDirectionBefore = caretDirectionBefore;
		record.caretDirectionAfter = _caretLogicalDirection;
		if (storeHistory) StoreUndoRecord(std::move(record));
		redoStack.clear();
		selRangeDirty = true;
		blocksDirty = true;
		NotifySelectionChanged();
	}
	notifications.Commit();
	return change;
}

bool RichTextBox::ApplyTextRangeFormat(
	std::size_t start,
	std::size_t length,
	const RichTextFormatDelta& delta)
{
	if (_mutatingDocumentFromEditor)
		throw std::logic_error(
			"TextRange cannot format a RichTextBox during document publication.");
	if (!delta.Validate()) return false;
	const auto maximum = static_cast<std::size_t>(
		(std::numeric_limits<int>::max)());
	if (start > maximum || length > maximum - start)
		throw std::out_of_range("TextRange exceeds RichTextBox offset limits.");
	if (length == 0) return true;

	SyncBufferFromControlIfNeeded();
	RichTextDocument next(_flatDocument.ToFragment());
	const auto change = next.ApplyFormat(
		start,
		length,
		delta,
		ResolveEffectiveCharacterStyle({}));
	if (!change.Changed()) return true;

	EditorNotificationScope notifications(*this);
	const bool storeHistory = !isApplyingUndoRedo
		&& _isUndoEnabled && _undoLimit != 0;
	if (storeHistory) undoStack.reserve(undoStack.size() + 1);
	UndoRecord record;
	record.pos = static_cast<int>(change.Start);
	record.removedFragment = change.Before;
	record.typingStyleBefore = _typingStyle;
	record.typingStyleAfter = _typingStyle;
	record.selStartBefore = record.selStartAfter = _selectionStart;
	record.selEndBefore = record.selEndAfter = _selectionEnd;
	record.caretDirectionBefore = record.caretDirectionAfter =
		_caretLogicalDirection;
	{
		HistoryVisibilityScope historyVisibility(
			*this, CanUndo() || storeHistory, false);
		ReplaceDocumentContent(next.ToFragment(), true);
	}
	record.insertedFragment = _flatDocument.Extract(
		change.Start, change.After.Text.size());
	if (record.insertedFragment != record.removedFragment)
	{
		if (storeHistory) StoreUndoRecord(std::move(record));
		redoStack.clear();
	}
	notifications.Commit();
	return true;
}

bool RichTextBox::ApplyTextRangeParagraphFormat(
	std::size_t start,
	std::size_t length,
	const RichTextParagraphFormatDelta& delta)
{
	if (_mutatingDocumentFromEditor)
		throw std::logic_error(
			"TextRange cannot format RichTextBox paragraphs during publication.");
	if (!delta.Validate()) return false;
	const auto maximum = static_cast<std::size_t>(
		(std::numeric_limits<int>::max)());
	if (start > maximum || length > maximum - start)
		throw std::out_of_range("TextRange exceeds RichTextBox offset limits.");

	SyncBufferFromControlIfNeeded();
	RichTextDocument next(_flatDocument.ToFragment());
	RichTextParagraphStyle baseline;
	baseline.TextAlignment = _document->GetTextAlignment();
	baseline.FlowDirection = _document->GetFlowDirection();
	const auto change = next.ApplyParagraphFormat(
		start, length, delta, baseline);
	if (!change.Changed()) return true;

	EditorNotificationScope notifications(*this);
	const bool storeHistory = !isApplyingUndoRedo
		&& _isUndoEnabled && _undoLimit != 0;
	if (storeHistory) undoStack.reserve(undoStack.size() + 1);
	UndoRecord record;
	record.pos = 0;
	record.removedFragment = change.Before;
	record.typingStyleBefore = _typingStyle;
	record.typingStyleAfter = _typingStyle;
	record.selStartBefore = record.selStartAfter = _selectionStart;
	record.selEndBefore = record.selEndAfter = _selectionEnd;
	record.caretDirectionBefore = record.caretDirectionAfter =
		_caretLogicalDirection;
	{
		HistoryVisibilityScope historyVisibility(
			*this, CanUndo() || storeHistory, false);
		ReplaceDocumentContent(next.ToFragment(), true);
	}
	record.insertedFragment = _flatDocument.ToFragment();
	if (record.insertedFragment != record.removedFragment)
	{
		if (storeHistory) StoreUndoRecord(std::move(record));
		redoStack.clear();
	}
	notifications.Commit();
	return true;
}

bool RichTextBox::ApplySelectionParagraphFormat(
	const RichTextParagraphFormatDelta& delta)
{
	if (_mutatingDocumentFromEditor || !delta.Validate()) return false;
	SyncBufferFromControlIfNeeded();
	const int start = (std::min)(_selectionStart, _selectionEnd);
	const int end = (std::max)(_selectionStart, _selectionEnd);
	return ApplyTextRangeParagraphFormat(
		static_cast<std::size_t>((std::max)(0, start)),
		static_cast<std::size_t>((std::max)(0, end - start)), delta);
}

bool RichTextBox::ApplySelectionFormat(const RichTextFormatDelta& delta)
{
	if (_mutatingDocumentFromEditor || !delta.Validate()) return false;
	SyncBufferFromControlIfNeeded();
	int start = (std::min)(_selectionStart, _selectionEnd);
	int end = (std::max)(_selectionStart, _selectionEnd);
	if (start == end)
	{
		bool insideWord = false;
		if (start > 0 && start < static_cast<int>(buffer.size()))
		{
			const int previous = GetPreviousCaretIndex(start);
			insideWord = CuiTextEdit::ClassifyWordCharacter(buffer, previous)
				== CuiTextEdit::WordCharacterClass::Word
				&& CuiTextEdit::ClassifyWordCharacter(buffer, start)
				== CuiTextEdit::WordCharacterClass::Word;
		}
		if (!insideWord)
		{
			_typingStyle = delta.ApplyTo(EffectiveTypingStyle());
			return true;
		}
		const auto word = CuiTextEdit::GetWordSelectionSpan(
			buffer, start, true);
		start = word.start;
		end = word.end;
	}

	RichTextDocument next(_flatDocument.ToFragment());
	const auto effectiveBaseline = ResolveEffectiveCharacterStyle({});
	auto change = next.ApplyFormat(
		static_cast<std::size_t>((std::max)(0, start)),
		static_cast<std::size_t>((std::max)(0, end - start)),
		delta, effectiveBaseline);
	if (!change.Changed()) return true;

	EditorNotificationScope notifications(*this);
	const bool storeHistory = !isApplyingUndoRedo
		&& _isUndoEnabled && _undoLimit != 0;
	if (storeHistory) undoStack.reserve(undoStack.size() + 1);
	UndoRecord record;
	record.pos = static_cast<int>(change.Start);
	record.removedFragment = change.Before;
	record.selStartBefore = record.selStartAfter = _selectionStart;
	record.selEndBefore = record.selEndAfter = _selectionEnd;
	record.caretDirectionBefore = record.caretDirectionAfter =
		_caretLogicalDirection;
	record.typingStyleBefore = _typingStyle;
	record.typingStyleAfter.reset();
	{
		HistoryVisibilityScope historyVisibility(
			*this, CanUndo() || storeHistory, false);
		ReplaceDocumentContent(next.ToFragment(), true);
	}
	_typingStyle.reset();
	record.insertedFragment = _flatDocument.Extract(
		change.Start, change.After.Text.size());
	if (record.insertedFragment != record.removedFragment)
	{
		if (storeHistory) StoreUndoRecord(std::move(record));
		redoStack.clear();
	}
	notifications.Commit();
	return true;
}

bool RichTextBox::ResetSelectionFormattingCommand()
{
	if (_isReadOnly || _mutatingDocumentFromEditor) return false;
	SyncBufferFromControlIfNeeded();
	int start = (std::min)(_selectionStart, _selectionEnd);
	const int end = (std::max)(_selectionStart, _selectionEnd);
	if (start != end)
	{
		ClearSelectionFormatting();
		return true;
	}

	bool insideWord = false;
	if (start > 0 && start < static_cast<int>(buffer.size()))
	{
		const int previous = GetPreviousCaretIndex(start);
		insideWord = CuiTextEdit::ClassifyWordCharacter(buffer, previous)
			== CuiTextEdit::WordCharacterClass::Word
			&& CuiTextEdit::ClassifyWordCharacter(buffer, start)
				== CuiTextEdit::WordCharacterClass::Word;
	}
	if (!insideWord)
	{
		_typingStyle.reset();
		return true;
	}

	const auto word = CuiTextEdit::GetWordSelectionSpan(
		buffer, start, RichTextIsMultiLine);
	return ApplyTextRangeFormat(
		static_cast<std::size_t>(word.start),
		static_cast<std::size_t>(word.Length()),
		ClearCharacterFormattingDelta());
}

bool RichTextBox::AdjustSelectionFontSize(float amount)
{
	if (_isReadOnly || _mutatingDocumentFromEditor
		|| !std::isfinite(amount) || amount == 0.0f)
	{
		return false;
	}
	SyncBufferFromControlIfNeeded();
	const int start = (std::min)(_selectionStart, _selectionEnd);
	const int end = (std::max)(_selectionStart, _selectionEnd);
	if (start == end)
	{
		const auto current = ResolveEffectiveCharacterStyle(
			EffectiveTypingStyle()).FontSize;
		if (!current) return true;
		float target = *current;
		if (amount > 0.0f)
		{
			if (target >= RichTextMaximumFontPoint) return true;
			target = (std::min)(
				target + amount, RichTextMaximumFontPoint);
		}
		else
		{
			if (target <= RichTextOneFontPoint) return true;
			target = (std::max)(
				target + amount, RichTextOneFontPoint);
		}
		RichTextFormatDelta delta;
		delta.FontSize = RichTextFormatChange<float>::Set(target);
		return ApplySelectionFormat(delta);
	}

	RichTextDocument next(_flatDocument.ToFragment());
	const auto change = next.AdjustFontSize(
		static_cast<std::size_t>(start),
		static_cast<std::size_t>(end - start), amount,
		RichTextOneFontPoint, RichTextMaximumFontPoint,
		ResolveEffectiveCharacterStyle({}));
	if (!change.Changed()) return true;

	EditorNotificationScope notifications(*this);
	const bool storeHistory = !isApplyingUndoRedo
		&& _isUndoEnabled && _undoLimit != 0;
	if (storeHistory) undoStack.reserve(undoStack.size() + 1);
	UndoRecord record;
	record.pos = static_cast<int>(change.Start);
	record.removedFragment = change.Before;
	record.selStartBefore = record.selStartAfter = _selectionStart;
	record.selEndBefore = record.selEndAfter = _selectionEnd;
	record.caretDirectionBefore = record.caretDirectionAfter =
		_caretLogicalDirection;
	record.typingStyleBefore = _typingStyle;
	record.typingStyleAfter.reset();
	{
		HistoryVisibilityScope historyVisibility(
			*this, CanUndo() || storeHistory, false);
		ReplaceDocumentContent(next.ToFragment(), true);
	}
	_typingStyle.reset();
	record.insertedFragment = _flatDocument.Extract(
		change.Start, change.After.Text.size());
	if (record.insertedFragment != record.removedFragment)
	{
		if (storeHistory) StoreUndoRecord(std::move(record));
		redoStack.clear();
	}
	notifications.Commit();
	return true;
}

TextSelectionPropertyValue RichTextBox::QuerySelectionProperty(
	const DependencyProperty& property) const
{
	RichTextDocumentFragment fragment;
	const int start = (std::min)(_selectionStart, _selectionEnd);
	const int end = (std::max)(_selectionStart, _selectionEnd);
	if (&property == &Block::TextAlignmentProperty()
		|| &property == &Block::FlowDirectionProperty())
	{
		RichTextParagraphStyle baseline;
		baseline.TextAlignment = _document->GetTextAlignment();
		baseline.FlowDirection = _document->GetFlowDirection();
		const auto styles = _flatDocument.ParagraphStylesInRange(
			static_cast<std::size_t>((std::max)(0, start)),
			static_cast<std::size_t>((std::max)(0, end - start)),
			baseline);
		if (styles.empty()) return {};
		if (&property == &Block::TextAlignmentProperty())
		{
			const auto common = styles.front().TextAlignment;
			for (const auto& style : styles)
				if (style.TextAlignment != common)
					return { TextSelectionPropertyValueKind::Mixed, {} };
			return common
				? TextSelectionPropertyValue{
					TextSelectionPropertyValueKind::Value, BindingValue(*common) }
				: TextSelectionPropertyValue{};
		}
		const auto common = styles.front().FlowDirection;
		for (const auto& style : styles)
			if (style.FlowDirection != common)
				return { TextSelectionPropertyValueKind::Mixed, {} };
		return common
			? TextSelectionPropertyValue{
				TextSelectionPropertyValueKind::Value, BindingValue(*common) }
			: TextSelectionPropertyValue{};
	}
	if (end > start)
	{
		fragment = _flatDocument.Extract(
			static_cast<std::size_t>((std::max)(0, start)),
			static_cast<std::size_t>(end - start));
	}
	else
	{
		const auto style = ResolveEffectiveCharacterStyle(
			EffectiveTypingStyle());
		fragment = RichTextDocumentFragment::FromPlainText(L"x", style);
	}
	for (auto& span : fragment.Spans)
		span.Style = ResolveEffectiveCharacterStyle(std::move(span.Style));

	auto query = [&]<typename TValue>(
		std::optional<TValue> RichTextCharacterStyle::* member)
		-> TextSelectionPropertyValue
	{
		std::optional<TValue> common;
		bool initialized = false;
		for (const auto& span : fragment.Spans)
		{
			const auto& candidate = span.Style.*member;
			if (!initialized)
			{
				common = candidate;
				initialized = true;
			}
			else if (common != candidate)
			{
				return { TextSelectionPropertyValueKind::Mixed, {} };
			}
		}
		if (!initialized || !common)
			return { TextSelectionPropertyValueKind::Unset, {} };
		return { TextSelectionPropertyValueKind::Value,
			BindingValue(*common) };
	};

	if (&property == &TextElement::ForegroundProperty())
		return query(&RichTextCharacterStyle::Foreground);
	if (&property == &TextElement::BackgroundProperty())
		return query(&RichTextCharacterStyle::Background);
	if (&property == &TextElement::FontFamilyProperty())
		return query(&RichTextCharacterStyle::FontFamily);
	if (&property == &TextElement::LanguageProperty())
		return query(&RichTextCharacterStyle::Language);
	if (&property == &TextElement::FontSizeProperty())
		return query(&RichTextCharacterStyle::FontSize);
	if (&property == &TextElement::FontWeightProperty())
		return query(&RichTextCharacterStyle::FontWeight);
	if (&property == &TextElement::FontStretchProperty())
		return query(&RichTextCharacterStyle::FontStretch);
	if (&property == &TextElement::FontStyleProperty())
		return query(&RichTextCharacterStyle::FontStyle);
	if (&property == &TextElement::UnderlineProperty())
		return query(&RichTextCharacterStyle::Underline);
	if (&property == &TextElement::StrikethroughProperty())
		return query(&RichTextCharacterStyle::Strikethrough);
	return {};
}

void RichTextBox::ClearSelectionFormatting()
{
	if (_selectionStart == _selectionEnd)
	{
		_typingStyle.reset();
		return;
	}
	(void)ApplySelectionFormat(ClearCharacterFormattingDelta());
}

void RichTextBox::InputText(std::wstring input)
{
	if (_isReadOnly || _mutatingDocumentFromEditor) return;
	SyncBufferFromControlIfNeeded();
	auto options = RichEditOptions();
	options.acceptsTab = _acceptsTab;
	input = CuiTextEdit::NormalizeInput(input, options);
	InputFragment(RichTextDocumentFragment::FromPlainText(
		std::move(input), EffectiveTypingStyle()));
}

void RichTextBox::InputLineBreak()
{
	if (_isReadOnly || _mutatingDocumentFromEditor) return;
	SyncBufferFromControlIfNeeded();
	const auto style = EffectiveTypingStyle();
	RichTextDocumentFragment fragment;
	if (!_flatDocument.Empty())
	{
		fragment = _flatDocument.CreateLineBreakFragment(
			static_cast<std::size_t>((std::max)(0,
				(std::min)(_selectionStart, _selectionEnd))),
			style);
	}
	else
	{
		RichTextCharacterStyle rootStyle;
		_document->ApplyLocalCharacterStyle(rootStyle);
		RichTextStructureNode paragraphNode{
			AllocateRichTextStructureId(),
			RichTextStructureKind::Paragraph, {} };
		if (auto* paragraph = dynamic_cast<Paragraph*>(
			_document->Blocks.At(0)))
		{
			paragraphNode.Id = paragraph->GetRichTextStructureId();
			paragraph->ApplyLocalCharacterStyle(paragraphNode.LocalStyle);
		}
		fragment.Text = L"\r\n";
		fragment.Spans = { RichTextStyleSpan{ 0, 2, style } };
		fragment.StructureSpans = { RichTextStructureSpan{
			0, 2,
			{ std::move(paragraphNode),
				RichTextStructureNode{
					AllocateRichTextStructureId(),
					RichTextStructureKind::LineBreak, style } } } };
		fragment.RootStyle = rootStyle;
		fragment.StructureRootId = _document->GetRichTextStructureId();
		RichTextParagraphStyle rootParagraphStyle;
		_document->ApplyLocalParagraphStyle(rootParagraphStyle);
		fragment.RootParagraphStyle = rootParagraphStyle;
		if (!fragment.ValidateCanonical())
			throw std::logic_error(
				"Empty-document LineBreak fragment is not canonical.");
	}
	InputFragment(std::move(fragment));
}

void RichTextBox::InputFragment(RichTextDocumentFragment fragment)
{
	if (_isReadOnly || _mutatingDocumentFromEditor) return;
	if (!fragment.ValidateCanonical())
		throw std::invalid_argument(
			"RichTextBox attributed insertion fragment is not canonical.");
	SyncBufferFromControlIfNeeded();
	EditorNotificationScope notifications(*this);
	TrimToMaxLength();
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	const auto caretDirectionBefore = _caretLogicalDirection;
	const auto normalizedSelection = _flatDocument.NormalizeRange(
		static_cast<std::size_t>((std::max)(0,
			(std::min)(_selectionStart, _selectionEnd))),
		static_cast<std::size_t>((std::abs)(_selectionEnd - _selectionStart)));
	const bool hadClipboardContent = !fragment.Empty();
	if (_maxLength > 0)
	{
		const std::size_t retainedLength =
			_flatDocument.Length() - normalizedSelection.Length;
		const std::size_t allowedLength = retainedLength
			>= static_cast<std::size_t>(_maxLength)
			? 0
			: static_cast<std::size_t>(_maxLength) - retainedLength;
		if (fragment.Text.size() > allowedLength)
		{
			RichTextDocument replacement(fragment);
			const auto cutoff = replacement.SnapToBoundary(
				allowedLength, RichTextBoundaryAffinity::Backward);
			fragment = replacement.Extract(0, cutoff);
		}
	}
	if ((hadClipboardContent && fragment.Empty())
		|| (normalizedSelection.Empty() && fragment.Empty()))
	{
		notifications.Commit();
		return;
	}

	const auto typingBefore = _typingStyle;
	const bool storeHistory = !isApplyingUndoRedo
		&& _isUndoEnabled && _undoLimit != 0;
	if (storeHistory) undoStack.reserve(undoStack.size() + 1);
	auto result = ReplaceRangeWithFragment(
		_selectionStart, _selectionEnd, std::move(fragment));
	UndoRecord rec;
	if (result.Changed())
	{
		rec.pos = static_cast<int>(result.Start);
		rec.removedFragment = result.Before;
		rec.insertedFragment = _flatDocument.Extract(
			result.Start, result.After.Text.size());
		rec.typingStyleBefore = typingBefore;
		rec.typingStyleAfter = _typingStyle;
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = this->_selectionStart;
		rec.selEndAfter = this->_selectionEnd;
		rec.caretDirectionBefore = caretDirectionBefore;
		rec.caretDirectionAfter = _caretLogicalDirection;
		if (storeHistory) StoreUndoRecord(std::move(rec));
		this->redoStack.clear();
	}
	this->selRangeDirty = true;
	this->blocksDirty = true;
	NotifySelectionChanged();
	notifications.Commit();
}
void RichTextBox::InputBack(bool byWord)
{
	if (_isReadOnly || _mutatingDocumentFromEditor) return;
	SyncBufferFromControlIfNeeded();
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	const auto caretDirectionBefore = _caretLogicalDirection;
	const auto typingBefore = _typingStyle;
	int eraseStart = (std::min)(_selectionStart, _selectionEnd);
	int eraseEnd = (std::max)(_selectionStart, _selectionEnd);
	if (_selectionStart == _selectionEnd)
	{
		if (byWord)
		{
			eraseStart = CuiTextEdit::GetPreviousWordCaretIndex(
				buffer, _selectionEnd, RichTextIsMultiLine);
			eraseEnd = _selectionEnd;
			if (eraseEnd <= eraseStart) return;
		}
		else
		{
			int eraseLength = 0;
			if (!GetBackspaceEraseRange(
				_selectionEnd, eraseStart, eraseLength)) return;
			eraseEnd = eraseStart + eraseLength;
		}
	}
	EditorNotificationScope notifications(*this);
	const bool storeHistory = !isApplyingUndoRedo
		&& _isUndoEnabled && _undoLimit != 0;
	if (storeHistory) undoStack.reserve(undoStack.size() + 1);
	auto result = ReplaceRangeWithFragment(
		eraseStart, eraseEnd,
		RichTextDocumentFragment::FromPlainText(L""));
	_typingStyle.reset();
	UndoRecord rec;
	if (result.Changed())
	{
		rec.pos = static_cast<int>(result.Start);
		rec.removedFragment = result.Before;
		rec.insertedFragment = _flatDocument.Extract(
			result.Start, result.After.Text.size());
		rec.typingStyleBefore = typingBefore;
		rec.typingStyleAfter = _typingStyle;
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = this->_selectionStart;
		rec.selEndAfter = this->_selectionEnd;
		rec.caretDirectionBefore = caretDirectionBefore;
		rec.caretDirectionAfter = _caretLogicalDirection;
		if (storeHistory) StoreUndoRecord(std::move(rec));
		this->redoStack.clear();
	}
	this->selRangeDirty = true;
	this->blocksDirty = true;
	NotifySelectionChanged();
	notifications.Commit();
}
void RichTextBox::InputDelete(bool byWord)
{
	if (_isReadOnly || _mutatingDocumentFromEditor) return;
	SyncBufferFromControlIfNeeded();
	const int selStartBefore = this->_selectionStart;
	const int selEndBefore = this->_selectionEnd;
	const auto caretDirectionBefore = _caretLogicalDirection;
	const auto typingBefore = _typingStyle;
	int eraseStart = (std::min)(_selectionStart, _selectionEnd);
	int eraseEnd = (std::max)(_selectionStart, _selectionEnd);
	if (_selectionStart == _selectionEnd)
	{
		if (byWord)
		{
			eraseStart = _selectionEnd;
			eraseEnd = CuiTextEdit::GetNextWordCaretIndex(
				buffer, _selectionEnd, RichTextIsMultiLine);
			if (eraseEnd <= eraseStart) return;
		}
		else
		{
			int eraseLength = 0;
			if (!GetDeleteEraseRange(
				_selectionEnd, eraseStart, eraseLength)) return;
			eraseEnd = eraseStart + eraseLength;
		}
	}
	EditorNotificationScope notifications(*this);
	const bool storeHistory = !isApplyingUndoRedo
		&& _isUndoEnabled && _undoLimit != 0;
	if (storeHistory) undoStack.reserve(undoStack.size() + 1);
	auto result = ReplaceRangeWithFragment(
		eraseStart, eraseEnd,
		RichTextDocumentFragment::FromPlainText(L""));
	_typingStyle.reset();
	UndoRecord rec;
	if (result.Changed())
	{
		rec.pos = static_cast<int>(result.Start);
		rec.removedFragment = result.Before;
		rec.insertedFragment = _flatDocument.Extract(
			result.Start, result.After.Text.size());
		rec.typingStyleBefore = typingBefore;
		rec.typingStyleAfter = _typingStyle;
		rec.selStartBefore = selStartBefore;
		rec.selEndBefore = selEndBefore;
		rec.selStartAfter = this->_selectionStart;
		rec.selEndAfter = this->_selectionEnd;
		rec.caretDirectionBefore = caretDirectionBefore;
		rec.caretDirectionAfter = _caretLogicalDirection;
		if (storeHistory) StoreUndoRecord(std::move(rec));
		this->redoStack.clear();
	}
	this->selRangeDirty = true;
	this->blocksDirty = true;
	NotifySelectionChanged();
	notifications.Commit();
}
bool RichTextBox::ApplyUndoRecord(const UndoRecord& rec, bool isUndo)
{
	if (_isReadOnly) return false;
	SyncBufferFromControlIfNeeded();
	this->isApplyingUndoRedo = true;
	try
	{
		const int pos = (std::clamp)(rec.pos, 0,
			static_cast<int>(_flatDocument.Length()));
		const auto& expected = isUndo
			? rec.insertedFragment : rec.removedFragment;
		const auto& replacement = isUndo
			? rec.removedFragment : rec.insertedFragment;
		const auto current = _flatDocument.Extract(
			static_cast<std::size_t>(pos), expected.Text.size());
		bool currentMatches = current == expected;
		if (!currentMatches && expected.Text.empty()
			&& !expected.StructureMarkers.empty())
		{
			const auto full = _flatDocument.ToFragment();
			currentMatches = full.RootStyle == expected.RootStyle
				&& full.RootParagraphStyle == expected.RootParagraphStyle
				&& full.StructureRootId == expected.StructureRootId;
			for (const auto& expectedMarker : expected.StructureMarkers)
			{
				const auto absolutePosition = static_cast<std::size_t>(pos)
					+ expectedMarker.Position;
				const auto found = std::find_if(
					full.StructureMarkers.begin(),
					full.StructureMarkers.end(),
					[&](const RichTextStructureMarker& marker)
					{
						return marker.Position == absolutePosition
							&& marker.Path == expectedMarker.Path;
					});
				if (found == full.StructureMarkers.end())
				{
					currentMatches = false;
					break;
				}
			}
		}
		if (!currentMatches)
		{
			undoStack.clear();
			redoStack.clear();
			isApplyingUndoRedo = false;
			return false;
		}
		const bool replaceWholeZeroWidthStructure = expected.Text.empty()
			&& replacement.Text.empty()
			&& (!expected.StructureMarkers.empty()
				|| !replacement.StructureMarkers.empty());
		RichTextDocument next(replaceWholeZeroWidthStructure
			? replacement : _flatDocument.ToFragment());
		if (!replaceWholeZeroWidthStructure)
		{
			(void)next.Replace(
				static_cast<std::size_t>(pos), expected.Text.size(), replacement);
		}
		const bool canUndoAfter = isUndo
			? undoStack.size() > 1 : true;
		const bool canRedoAfter = isUndo
			? true : redoStack.size() > 1;
		{
			HistoryVisibilityScope historyVisibility(
				*this, canUndoAfter, canRedoAfter);
			ReplaceDocumentContent(
				next.ToFragment(), true,
				TextPointerTextChange{
					static_cast<std::size_t>(pos),
					expected.Text.size(),
					replacement.Text.size() });
		}

		if (isUndo)
		{
			this->_selectionStart = rec.selStartBefore;
			this->_selectionEnd = rec.selEndBefore;
			_caretLogicalDirection = rec.caretDirectionBefore;
			_typingStyle = rec.typingStyleBefore;
		}
		else
		{
			this->_selectionStart = rec.selStartAfter;
			this->_selectionEnd = rec.selEndAfter;
			_caretLogicalDirection = rec.caretDirectionAfter;
			_typingStyle = rec.typingStyleAfter;
		}
		this->_selectionStart = (std::clamp)(this->_selectionStart, 0,
			static_cast<int>(_flatDocument.Length()));
		this->_selectionEnd = (std::clamp)(this->_selectionEnd, 0,
			static_cast<int>(_flatDocument.Length()));
		this->selRangeDirty = true;
		this->blocksDirty = true;
		NotifySelectionChanged();
	}
	catch (...)
	{
		this->isApplyingUndoRedo = false;
		throw;
	}
	this->isApplyingUndoRedo = false;
	return true;
}

void RichTextBox::StoreUndoRecord(UndoRecord record)
{
	if (!_isUndoEnabled || _undoLimit == 0) return;
	undoStack.push_back(std::move(record));
	if (_undoLimit > 0
		&& undoStack.size() > static_cast<size_t>(_undoLimit))
	{
		undoStack.erase(
			undoStack.begin(),
			undoStack.begin()
				+ static_cast<std::ptrdiff_t>(
					undoStack.size() - static_cast<size_t>(_undoLimit)));
	}
}

void RichTextBox::StoreRedoRecord(UndoRecord record)
{
	if (!_isUndoEnabled || _undoLimit == 0) return;
	redoStack.push_back(std::move(record));
	if (_undoLimit > 0
		&& redoStack.size() > static_cast<size_t>(_undoLimit))
	{
		redoStack.erase(
			redoStack.begin(),
			redoStack.begin()
				+ static_cast<std::ptrdiff_t>(
					redoStack.size() - static_cast<size_t>(_undoLimit)));
	}
}

void RichTextBox::OnUndoPolicyChanged()
{
	undoStack.clear();
	redoStack.clear();
}

void RichTextBox::OnScrollPolicyChanged()
{
	if (_verticalScrollBarVisibility == ScrollBarVisibility::Disabled)
		_verticalScrollOffset = 0.0f;
	_textLayoutDirty = true;
	blocksDirty = true;
	blockMetricsDirty = true;
	RequestLayout();
	InvalidateVisual();
}

void RichTextBox::Undo()
{
	if (_mutatingDocumentFromEditor || !CanUndo()) return;
	EditorNotificationScope notifications(*this);
	UndoRecord rec = this->undoStack.back();
	if (_isUndoEnabled && _undoLimit != 0)
		redoStack.reserve(redoStack.size() + 1);
	if (ApplyUndoRecord(rec, true))
	{
		this->undoStack.pop_back();
		StoreRedoRecord(std::move(rec));
	}
	notifications.Commit();
}
void RichTextBox::Redo()
{
	if (_mutatingDocumentFromEditor || !CanRedo()) return;
	EditorNotificationScope notifications(*this);
	UndoRecord rec = this->redoStack.back();
	if (_isUndoEnabled && _undoLimit != 0)
		undoStack.reserve(undoStack.size() + 1);
	if (ApplyUndoRecord(rec, false))
	{
		this->redoStack.pop_back();
		StoreUndoRecord(std::move(rec));
	}
	notifications.Commit();
}
void RichTextBox::UpdateScroll(bool arrival)
{
	if (this->_textLayoutDirty || (this->_isVirtualized && (this->blocksDirty || this->blockMetricsDirty)) || (!this->_isVirtualized && this->_textLayoutCache == nullptr))
	{
		this->UpdateLayout();
	}
	float cx = 0.0f;
	float cy = 0.0f;
	float ch = 0.0f;
	if (!GetCaretMetrics(this->_selectionEnd, cx, cy, ch)) return;
	const float renderHeight = TextViewportHeight();
	if (renderHeight <= 0.0f) return;
	const float caretTopContent = (cy - Padding.Top)
		+ this->_verticalScrollOffset;
	const float caretBottomContent = caretTopContent + ch;
	if (arrival && this->_selectionEnd >= static_cast<int>(this->buffer.size()))
	{
		const float maxScroll = (std::max)(
			0.0f, this->_textSize.height - renderHeight);
		this->_verticalScrollOffset = maxScroll;
	}
	else if (caretBottomContent - this->_verticalScrollOffset > renderHeight)
	{
		this->_verticalScrollOffset = caretBottomContent - renderHeight;
	}
	if (caretTopContent - this->_verticalScrollOffset < 0.0f)
		this->_verticalScrollOffset = caretTopContent;
	if (this->_verticalScrollOffset < 0.0f)
		this->_verticalScrollOffset = 0.0f;
}
void RichTextBox::AppendText(std::wstring str)
{
	SyncBufferFromControlIfNeeded();
	this->_selectionStart = this->_selectionEnd = (int)this->buffer.size();
	this->_caretLogicalDirection = LogicalDirection::Forward;
	this->InputText(str);
	this->selRangeDirty = true;
}
void RichTextBox::AppendLine(std::wstring str)
{
	SyncBufferFromControlIfNeeded();
	this->_selectionStart = this->_selectionEnd = (int)this->buffer.size();
	this->_caretLogicalDirection = LogicalDirection::Forward;
	this->InputText(str + L"\r\n");
	this->selRangeDirty = true;
}
std::wstring RichTextBox::GetSelectedString()
{
	SyncBufferFromControlIfNeeded();
	const int start = (std::min)(_selectionStart, _selectionEnd);
	const int end = (std::max)(_selectionStart, _selectionEnd);
	return _flatDocument.Extract(
		static_cast<std::size_t>((std::max)(0, start)),
		static_cast<std::size_t>((std::max)(0, end - start))).Text;
}

// ---- 公共选择/编辑 API ----
int RichTextBox::GetSelectionStart()
{
	SyncBufferFromControlIfNeeded();
	auto span = CuiTextEdit::NormalizeSelection(
		_selectionStart, _selectionEnd, buffer.size());
	return span.start;
}

int RichTextBox::GetSelectionLength()
{
	auto span = CuiTextEdit::NormalizeSelection(this->_selectionStart, this->_selectionEnd, this->buffer.size());
	return span.HasSelection() ? static_cast<int>(span.Length()) : 0;
}

int RichTextBox::GetCaretIndex()
{
	SyncBufferFromControlIfNeeded();
	return (std::clamp)(
		_selectionEnd, 0, static_cast<int>(buffer.size()));
}

void RichTextBox::SetCaretIndex(int value)
{
	Select(value, 0);
}

bool RichTextBox::HasSelection()
{
	return GetSelectionLength() > 0;
}

void RichTextBox::Select(int start, int length)
{
	if (_mutatingDocumentFromEditor) return;
	SyncBufferFromControlIfNeeded();
	ClearSuggestedCaretX();
	_typingStyle.reset();
	const int textLen = static_cast<int>(this->buffer.size());
	start = (std::clamp)(start, 0, textLen);
	length = (std::clamp)(length, 0, textLen - start);
	this->_selectionStart = start;
	this->_selectionEnd = start + length;
	this->_caretLogicalDirection = LogicalDirection::Forward;
	this->selRangeDirty = true;
	NotifySelectionChanged();
	this->InvalidateVisual();
}

void RichTextBox::SelectPointers(
	const TextPointer& anchorPosition,
	const TextPointer& movingPosition)
{
	if (_mutatingDocumentFromEditor) return;
	SyncBufferFromControlIfNeeded();
	if (!anchorPosition.IsValid() || !movingPosition.IsValid())
		throw std::invalid_argument(
			"TextSelection positions must be attached to a live document.");
	if (anchorPosition.GetDocument() != _document.get()
		|| movingPosition.GetDocument() != _document.get())
	{
		throw std::invalid_argument(
			"TextSelection positions must belong to its RichTextBox.Document.");
	}
	const auto anchor = anchorPosition.GetInsertionPosition(
		anchorPosition.GetLogicalDirection());
	const auto moving = movingPosition.GetInsertionPosition(
		movingPosition.GetLogicalDirection());
	ClearSuggestedCaretX();
	_typingStyle.reset();
	_selectionStart = static_cast<int>(anchor.GetTextOffset());
	_selectionEnd = static_cast<int>(moving.GetTextOffset());
	_caretLogicalDirection = moving.GetLogicalDirection();
	selRangeDirty = true;
	NotifySelectionChanged();
	InvalidateVisual();
}

void RichTextBox::SelectAll()
{
	Select(0, static_cast<int>(this->Text.size()));
}

void RichTextBox::ClearSelection()
{
	if (_mutatingDocumentFromEditor) return;
	ClearSuggestedCaretX();
	this->_selectionEnd = this->_selectionStart;
	this->_caretLogicalDirection = LogicalDirection::Backward;
	this->selRangeDirty = true;
	NotifySelectionChanged();
	this->InvalidateVisual();
}

void RichTextBox::Clear()
{
	if (_isReadOnly) return;
	this->SelectAll();
	this->InputBack();
}

void RichTextBox::InsertText(const std::wstring& text)
{
	if (_isReadOnly || (text.empty() && !HasSelection())) return;
	this->InputText(text);
}

void RichTextBox::InsertTextAndSelect(
	const std::wstring& text, int selectionStart, int selectionLength)
{
	if (_isReadOnly || (text.empty() && !HasSelection())) return;
	EditorNotificationScope notifications(*this);
	const size_t undoCount = this->undoStack.size();
	this->InputText(text);
	this->Select(selectionStart, selectionLength);
	if (this->undoStack.size() > undoCount)
	{
		auto& record = this->undoStack.back();
		record.selStartAfter = this->_selectionStart;
		record.selEndAfter = this->_selectionEnd;
		record.caretDirectionAfter = _caretLogicalDirection;
	}
	notifications.Commit();
}

void RichTextBox::ReplaceAllTextAndSelect(
	const std::wstring& text, int selectionStart, int selectionLength)
{
	if (_isReadOnly) return;
	SyncBufferFromControlIfNeeded();
	EditorNotificationScope notifications(*this);
	const int selectionStartBefore = this->_selectionStart;
	const int selectionEndBefore = this->_selectionEnd;
	const auto caretDirectionBefore = _caretLogicalDirection;
	if (this->buffer == text)
	{
		this->Select(selectionStart, selectionLength);
		notifications.Commit();
		return;
	}

	this->SelectAll();
	const size_t undoCount = this->undoStack.size();
	this->InputText(text);
	this->Select(selectionStart, selectionLength);
	if (this->undoStack.size() > undoCount)
	{
		auto& record = this->undoStack.back();
		record.selStartBefore = selectionStartBefore;
		record.selEndBefore = selectionEndBefore;
		record.selStartAfter = this->_selectionStart;
		record.selEndAfter = this->_selectionEnd;
		record.caretDirectionBefore = caretDirectionBefore;
		record.caretDirectionAfter = _caretLogicalDirection;
	}
	notifications.Commit();
}

bool RichTextBox::Copy()
{
	SyncBufferFromControlIfNeeded();
	const auto fragment = CreateClipboardFragment();
	if (fragment.Empty()) return false;
	return cui::richtext::clipboard::Publish(
		this->GetPresentationWindow()
			? this->GetPresentationWindow()->Handle : nullptr,
		fragment);
}

bool RichTextBox::Cut()
{
	if (_isReadOnly) return false;
	SyncBufferFromControlIfNeeded();
	const auto fragment = CreateClipboardFragment();
	if (fragment.Empty()) return false;
	if (!cui::richtext::clipboard::Publish(
		this->GetPresentationWindow()
			? this->GetPresentationWindow()->Handle : nullptr,
		fragment))
		return false;
	this->InputBack();
	return true;
}

bool RichTextBox::Paste()
{
	if (_isReadOnly) return false;
	auto clipboard = cui::richtext::clipboard::Read(
		this->GetPresentationWindow()
			? this->GetPresentationWindow()->Handle : nullptr);
	if (!clipboard) return false;
	auto insertRichFragment = [&](
		std::optional<RichTextDocumentFragment> fragment)
	{
		if (!fragment || fragment->Empty()) return false;
		RichTextCharacterStyle rootStyle;
		_document->ApplyLocalCharacterStyle(rootStyle);
		RichTextParagraphStyle rootParagraphStyle;
		_document->ApplyLocalParagraphStyle(rootParagraphStyle);
		auto rebased = cui::richtext::clipboard::RebaseStructureForInsertion(
			*fragment, rootStyle, rootParagraphStyle,
			_document->GetRichTextStructureId());
		if (!rebased) return false;
		InputFragment(std::move(*rebased));
		return true;
	};
	if (clipboard->Attributed)
	{
		if (insertRichFragment(cui::richtext::clipboard::Decode(
			*clipboard->Attributed))) return true;
	}
	if (clipboard->Rtf
		&& insertRichFragment(cui::richtext::rtf::Decode(*clipboard->Rtf)))
		return true;
	if (!clipboard->PlainText || clipboard->PlainText->empty()) return false;
	InputText(std::move(*clipboard->PlainText));
	return true;
}

bool RichTextBox::CanPaste() const noexcept
{
	return !_isReadOnly
		&& cui::richtext::clipboard::CanPaste();
}

void RichTextBox::PreparePresentation()
{
	Control::PreparePresentation();
	UpdateLayout();
}

void RichTextBox::OnRender()
{
	if (this->IsVisible == false)return;
	auto d2d = this->GetDrawingContext();
	auto font = this->GetRenderFont();
	if (!d2d || !font) return;
	richTextStyleBrushes.clear();
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	Microsoft::WRL::ComPtr<ID2D1Brush> foreground;
	foreground.Attach(CreateForegroundBrush(
		*d2d, D2D1::SizeF(actualWidth, actualHeight)));
	auto selectionDefinition = SelectionBrush;
	selectionDefinition.Opacity *= static_cast<float>(
		(std::clamp)(SelectionOpacity, 0.0, 1.0));
	Microsoft::WRL::ComPtr<ID2D1Brush> selectionBrush;
	selectionBrush.Attach(selectionDefinition.CreateBrush(
		*d2d, D2D1::SizeF(actualWidth, actualHeight)));
	Microsoft::WRL::ComPtr<ID2D1Brush> selectionTextBrush;
	selectionTextBrush.Attach(SelectionTextBrush.CreateBrush(
		*d2d, D2D1::SizeF(actualWidth, actualHeight)));
	Microsoft::WRL::ComPtr<ID2D1Brush> caretBrush;
	caretBrush.Attach(CaretBrush.CreateBrush(
		*d2d, D2D1::SizeF(actualWidth, actualHeight)));
	const bool focused = GetPresentationWindow()
		&& GetPresentationWindow()->GetKeyboardFocusedElement() == this;
	const bool isSelected = focused
		|| IsInactiveSelectionHighlightEnabled;
	const bool showCaret = focused
		&& (!_isReadOnly || _isReadOnlyCaretVisible);
	this->_caretRectCacheValid = false;
	bool shouldDrawCaret = false;
	D2D1_POINT_2F caretStart{};
	D2D1_POINT_2F caretEnd{};

	this->BeginRender();
	{
		if (this->buffer.size() > 0)
		{
			if (this->_isVirtualized)
			{
				float renderWidth = TextViewportWidth();
				float renderHeight = TextViewportHeight();
				if (this->layoutWidthHasScrollBar) renderWidth -= 8.0f;

				int sels = std::min(_selectionStart, _selectionEnd);
				int sele = std::max(_selectionStart, _selectionEnd);
				if (sels == sele)
					sels = sele = SnapCaretIndex(
						sels, _caretLogicalDirection);
				else
					NormalizeSelectionRangeForErase(sels, sele);
				int selLen = sele - sels;

				float cx, cy, ch;
				if (showCaret && selLen == 0 && GetCaretMetrics(this->_selectionEnd, cx, cy, ch))
				{
					{
						const float ah = (ch > 0.0f) ? ch : font->FontHeight;
						const auto absoluteLocation = this->GetAbsoluteLocationDip();
						this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + cx - 2.0f, static_cast<float>(absoluteLocation.y) + cy - 2.0f, static_cast<float>(absoluteLocation.x) + cx + 2.0f, static_cast<float>(absoluteLocation.y) + cy + ah + 2.0f };
						this->_caretRectCacheValid = true;
					}
					shouldDrawCaret = true;
					caretStart = { cx, cy };
					caretEnd = { cx, cy + ch };
				}

				float viewTop = this->_verticalScrollOffset;
				float viewBottom = this->_verticalScrollOffset + renderHeight;
				for (int i = 0; i < static_cast<int>(this->blocks.size()); ++i)
				{
					const float top = this->blockTops[i];
					const float bottom = top + this->blocks[i].height;
					if (bottom >= viewTop && top <= viewBottom) continue;
					if (this->blocks[i].layout)
					{
						this->blocks[i].layout->Release();
						this->blocks[i].layout = nullptr;
					}
				}

				int first = 0;
				for (int i = 0; i < (int)this->blockTops.size(); i++)
				{
					if (this->blockTops[i] + this->blocks[i].height >= viewTop)
					{
						first = i;
						break;
					}
				}

				for (int i = first; i < (int)this->blocks.size(); i++)
				{
					float top = this->blockTops[i];
					float bottom = top + this->blocks[i].height;
					if (top > viewBottom) break;

					EnsureBlockLayout(i, renderWidth, renderHeight);
					float drawY = Padding.Top
						+ (top - this->_verticalScrollOffset);
					float drawX = Padding.Left;
					DrawTextStyleBackgrounds(
						blocks[i].layout,
						static_cast<int>(blocks[i].start),
						static_cast<int>(blocks[i].layoutLen),
						drawX, drawY);

					if (isSelected && !highlightRanges.empty())
					{
						const int blockStart = static_cast<int>(blocks[i].start);
						const int blockEnd = static_cast<int>(
							blocks[i].start + blocks[i].layoutLen);
						for (const auto& highlight : highlightRanges)
						{
							const int rangeStart = (std::max)(
								highlight.Start, blockStart);
							const int rangeEnd = (std::min)(
								highlight.Start + highlight.Length, blockEnd);
							if (rangeEnd <= rangeStart) continue;
							auto ranges = font->HitTestTextRange(
								blocks[i].layout,
								static_cast<UINT32>(rangeStart - blockStart),
								static_cast<UINT32>(rangeEnd - rangeStart));
							for (const auto& range : ranges)
							{
								d2d->FillRect(range.left + drawX,
									range.top + drawY, range.width, range.height,
									_highlightBackColor);
							}
						}
					}

					if (isSelected && selLen != 0 && selectionBrush)
					{
						int blockStart = (int)this->blocks[i].start;
						int blockEnd = (int)(this->blocks[i].start
							+ this->blocks[i].layoutLen);
						int is = std::max(sels, blockStart);
						int ie = std::min(sele, blockEnd);
						if (ie > is)
						{
							int localStart = is - blockStart;
							int localLen = ie - is;
							auto ranges = font->HitTestTextRange(this->blocks[i].layout, (UINT32)localStart, (UINT32)localLen);
							for (auto r : ranges)
							{
								d2d->FillRect(
									r.left + drawX,
									r.top + drawY,
									r.width,
									r.height,
									selectionBrush.Get());
							}
						}
					}

					ApplyTextDrawingEffects(
						blocks[i].layout,
						static_cast<int>(blocks[i].start),
						static_cast<int>(blocks[i].layoutLen),
						isSelected,
						selectionTextBrush.Get());
					if (foreground)
						d2d->DrawStringLayout(
							this->blocks[i].layout, drawX, drawY,
							foreground.Get());
					else
						d2d->DrawStringLayout(
							this->blocks[i].layout, drawX, drawY,
							this->RendererForegroundColor);
				}
			}
			else if (isSelected)
			{
				ApplyTextDrawingEffects(
					_textLayoutCache, 0,
					static_cast<int>(buffer.size()), true,
					selectionTextBrush.Get());
				DrawTextStyleBackgrounds(
					_textLayoutCache, 0,
					static_cast<int>(buffer.size()),
					Padding.Left,
					Padding.Top - _verticalScrollOffset);
				for (const auto& highlight : highlightRanges)
				{
					auto ranges = font->HitTestTextRange(
						_textLayoutCache,
						static_cast<UINT32>(highlight.Start),
						static_cast<UINT32>(highlight.Length));
					for (const auto& range : ranges)
					{
						d2d->FillRect(range.left + Padding.Left,
							range.top + Padding.Top - _verticalScrollOffset,
							range.width, range.height, _highlightBackColor);
					}
				}
				if (isSelected && this->selRangeDirty)
				{
					UpdateSelRange();
				}
				int sels = _selectionStart <= _selectionEnd ? _selectionStart : _selectionEnd;
				int sele = _selectionEnd >= _selectionStart ? _selectionEnd : _selectionStart;
				int selLen = sele - sels;
				if (selLen != 0 && selectionBrush)
				{
					for (auto sr : selRange)
					{
						d2d->FillRect(
							sr.left + Padding.Left,
							(sr.top + Padding.Top) - this->_verticalScrollOffset,
							sr.width,
							sr.height,
							selectionBrush.Get());
					}
				}
				else
				{
					if (showCaret && selLen == 0 && !selRange.empty())
					{
						const auto caret = selRange[0];
						const float lx = caret.left + Padding.Left;
						const float ly = (caret.top + Padding.Top)
							- this->_verticalScrollOffset;
						const float ah = caret.height > 0 ? caret.height : font->FontHeight;
						const auto absoluteLocation = this->GetAbsoluteLocationDip();
						this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + lx - 2.0f, static_cast<float>(absoluteLocation.y) + ly - 2.0f, static_cast<float>(absoluteLocation.x) + lx + 2.0f, static_cast<float>(absoluteLocation.y) + ly + ah + 2.0f };
						this->_caretRectCacheValid = true;
					}
					if (showCaret && !selRange.empty())
					{
						shouldDrawCaret = true;
						caretStart = { selRange[0].left + Padding.Left,
							(selRange[0].top + Padding.Top) - this->_verticalScrollOffset };
						caretEnd = { selRange[0].left + Padding.Left,
							(selRange[0].top + selRange[0].height + Padding.Top)
								- this->_verticalScrollOffset };
					}
				}
				if (foreground)
					d2d->DrawStringLayout(this->_textLayoutCache,
						Padding.Left, Padding.Top - this->_verticalScrollOffset,
						foreground.Get());
				else
					d2d->DrawStringLayout(this->_textLayoutCache,
						Padding.Left, Padding.Top - this->_verticalScrollOffset,
						this->RendererForegroundColor);
			}
			else
			{
				ApplyTextDrawingEffects(
					_textLayoutCache, 0,
					static_cast<int>(buffer.size()), false, nullptr);
				DrawTextStyleBackgrounds(
					_textLayoutCache, 0,
					static_cast<int>(buffer.size()),
					Padding.Left,
					Padding.Top - _verticalScrollOffset);
				if (foreground)
					d2d->DrawStringLayout(this->_textLayoutCache,
						Padding.Left, Padding.Top - this->_verticalScrollOffset,
						foreground.Get());
				else
					d2d->DrawStringLayout(this->_textLayoutCache,
						Padding.Left, Padding.Top - this->_verticalScrollOffset,
						this->RendererForegroundColor);
			}
		}
		else
		{
			if (showCaret)
			{
				const float lx = Padding.Left;
				const float ly = Padding.Top;
				const float ah = (font->FontHeight > 16.0f) ? font->FontHeight : 16.0f;
				const auto absoluteLocation = this->GetAbsoluteLocationDip();
				this->_caretRectCache = { static_cast<float>(absoluteLocation.x) + lx - 2.0f, static_cast<float>(absoluteLocation.y) + ly - 2.0f, static_cast<float>(absoluteLocation.x) + lx + 2.0f, static_cast<float>(absoluteLocation.y) + ly + ah + 2.0f };
				this->_caretRectCacheValid = true;
				shouldDrawCaret = true;
				caretStart = { lx, ly };
				caretEnd = { lx, ly + 16.0f };
			}
		}
		UpdateCaretBlinkState(showCaret, this->_selectionStart, this->_selectionEnd, this->_caretRectCacheValid, this->_caretRectCacheValid ? &this->_caretRectCache : nullptr);
		if (shouldDrawCaret && IsCaretBlinkVisible())
		{
			if (caretBrush)
				d2d->DrawLine(caretStart, caretEnd, caretBrush.Get());
			else if (foreground)
				d2d->DrawLine(caretStart, caretEnd, foreground.Get());
			else
				d2d->DrawLine(
					caretStart, caretEnd,
					this->RendererForegroundColor);
		}
		this->DrawScroll();
	}
	this->EndRender();
}

bool RichTextBox::GetAnimatedInvalidRect(D2D1_RECT_F& outRect)
{
	return GetCaretBlinkInvalidRect(outRect);
}
bool RichTextBox::ProcessInput(const InputReport& input)
{
	if (!this->IsEnabled || !this->IsVisible) return true;
	SelectionNotificationScope selectionNotification{ this };
	switch (input.Kind)
	{
	case InputReportKind::MouseWheel:
	{
		if (input.WheelDelta > 0)
		{
			if (this->_verticalScrollOffset > 0)
			{
				this->_verticalScrollOffset -= 10;
				if (this->_verticalScrollOffset < 0)this->_verticalScrollOffset = 0;
				this->InvalidateVisual();
			}
		}
		else
		{
			auto font = this->GetRenderFont();
			float renderWidth = TextViewportWidth();
			float renderHeight = TextViewportHeight();
			if (_textSize.height > renderHeight) renderWidth -= 8.0f;
			if (this->_verticalScrollOffset < _textSize.height - renderHeight)
			{
				this->_verticalScrollOffset += 10;
				if (this->_verticalScrollOffset > _textSize.height - renderHeight) this->_verticalScrollOffset = _textSize.height - renderHeight;
				this->InvalidateVisual();
			}
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case InputReportKind::PointerMove:
	{
		if (isDraggingScroll) {
			UpdateScrollDrag(static_cast<float>(input.Y));
		}
		if (input.IsButtonPressed(MouseButton::Left)
			&& this->GetPresentationWindow()->GetKeyboardFocusedElement() == this
			&& !isDraggingScroll)
		{
			ClearSuggestedCaretX();
			auto font = this->GetRenderFont();
			if (this->_isVirtualized)
				_selectionEnd = HitTestGlobalIndex((float)input.X, (float)input.Y);
			else
				_selectionEnd = SnapCaretIndex(font->HitTestTextPosition(
					this->_textLayoutCache, input.X - Padding.Left,
					(input.Y + this->_verticalScrollOffset) - Padding.Top),
					LogicalDirection::Forward);
			_caretLogicalDirection = _selectionEnd < _selectionStart
				? LogicalDirection::Backward : LogicalDirection::Forward;
			UpdateScroll();
			this->InvalidateVisual();
			this->selRangeDirty = true;
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case InputReportKind::PointerDown:
	{
		if (input.ChangedButton == MouseButton::Left
			|| input.ChangedButton == MouseButton::Right)
		{
			if (input.ChangedButton == MouseButton::Left)
				(void)CaptureMouse();
			if (this->GetPresentationWindow()->GetKeyboardFocusedElement() != this)
			{
				auto previousSelection = this->GetPresentationWindow()->GetKeyboardFocusedElement();
				this->GetPresentationWindow()->SetKeyboardFocus(this, false);
				if (previousSelection) previousSelection->InvalidateVisual();
			}
			if (input.ChangedButton == MouseButton::Left
				&& input.X >= ActualWidth - 8.0f && input.X <= ActualWidth)
			{
				// 竖向滚动条：点在滑块上则用按下点锚定；否则用滑块中心（原行为）
				const float renderHeight = TextViewportHeight();
				if (renderHeight > 0.0f && _textSize.height > renderHeight)
				{
					const float maxScroll = std::max(0.0f, _textSize.height - renderHeight);
					float thumbHeight = (renderHeight / _textSize.height) * renderHeight;
					if (thumbHeight < this->ActualHeight * 0.1f) thumbHeight = this->ActualHeight * 0.1f;
					if (thumbHeight > static_cast<float>(this->ActualHeight)) thumbHeight = static_cast<float>(this->ActualHeight);
					const float moveSpace = std::max(0.0f, (float)this->ActualHeight - thumbHeight);
					float scrollRatio = 0.0f;
					if (maxScroll > 0.0f) scrollRatio = std::clamp(this->_verticalScrollOffset / maxScroll, 0.0f, 1.0f);
					const float thumbTop = scrollRatio * moveSpace;
					const float pointerY = (float)input.Y;
					const bool hitThumb = (pointerY >= thumbTop && pointerY <= (thumbTop + thumbHeight));
					_verticalScrollThumbGrabOffset = hitThumb ? (pointerY - thumbTop) : (thumbHeight * 0.5f);
				}
				else
				{
					_verticalScrollThumbGrabOffset = 0.0f;
				}
				isDraggingScroll = true;
				UpdateScrollDrag((float)input.Y);
				this->InvalidateVisual();
			}
			else
			{
				ClearSuggestedCaretX();
				_typingStyle.reset();
				SyncBufferFromControlIfNeeded();
				auto font = this->GetRenderFont();
				const int hit = SnapCaretIndex(this->_isVirtualized
					? HitTestGlobalIndex((float)input.X, (float)input.Y)
					: font->HitTestTextPosition(this->_textLayoutCache,
						input.X - Padding.Left,
						(input.Y + this->_verticalScrollOffset) - Padding.Top),
					LogicalDirection::Forward);
				const auto selection = CuiTextEdit::NormalizeSelection(
					this->_selectionStart, this->_selectionEnd, this->buffer.size());
				if (input.ChangedButton == MouseButton::Left
					|| !selection.HasSelection()
					|| hit < selection.start || hit > selection.end)
				{
					this->_selectionStart = this->_selectionEnd = hit;
					this->_caretLogicalDirection = LogicalDirection::Forward;
					this->selRangeDirty = true;
				}
			}
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseDown(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case InputReportKind::PointerUp:
	{
		if (isDraggingScroll) {
			isDraggingScroll = false;
		}
		else if (input.ChangedButton == MouseButton::Left
			&& this->GetPresentationWindow()->GetKeyboardFocusedElement() == this)
		{
			ClearSuggestedCaretX();
			auto font = this->GetRenderFont();
			if (this->_isVirtualized)
				_selectionEnd = HitTestGlobalIndex((float)input.X, (float)input.Y);
			else
				_selectionEnd = SnapCaretIndex(font->HitTestTextPosition(
					this->_textLayoutCache, input.X - Padding.Left,
					(input.Y + this->_verticalScrollOffset) - Padding.Top),
					LogicalDirection::Forward);
			_caretLogicalDirection = _selectionEnd < _selectionStart
				? LogicalDirection::Backward : LogicalDirection::Forward;
			this->selRangeDirty = true;
		}
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseUp(this, eventArgs);
		this->InvalidateVisual();
		if (input.ChangedButton == MouseButton::Left && IsMouseCaptured())
			(void)ReleaseMouseCapture();
	}
	break;
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
		isDraggingScroll = false;
		if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
			(void)ReleaseMouseCapture();
		return Control::ProcessInput(input);
	case InputReportKind::PointerDoubleClick:
	{
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		this->GetPresentationWindow()->SetKeyboardFocus(this, false);
		ClearSuggestedCaretX();
		_typingStyle.reset();
		SyncBufferFromControlIfNeeded();
		UpdateLayout();
		int hitIndex = 0;
		if (this->_isVirtualized)
			hitIndex = HitTestGlobalIndex((float)input.X, (float)input.Y);
		else
			hitIndex = this->GetRenderFont()->HitTestTextPosition(
				this->_textLayoutCache, input.X - Padding.Left,
				(input.Y + this->_verticalScrollOffset) - Padding.Top);
		hitIndex = SnapCaretIndex(
			hitIndex, LogicalDirection::Forward);
		const auto word = CuiTextEdit::GetWordSelectionSpan(
			this->buffer, hitIndex, RichTextIsMultiLine);
		this->_selectionStart = word.start;
		this->_selectionEnd = word.end;
		this->_caretLogicalDirection = LogicalDirection::Forward;
		this->selRangeDirty = true;
		UpdateScroll();
		auto eventArgs = input.CreateMouseEventArgs();
		this->OnMouseDoubleClick(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	case InputReportKind::KeyDown:
	{
		bool handled = false;
		switch (input.Key)
		{
		case Key::Left:
		case Key::Right:
		case Key::Up:
		case Key::Down:
		case Key::Home:
		case Key::End:
		case Key::PageUp:
		case Key::PageDown:
			_typingStyle.reset();
			break;
		default:
			break;
		}
		if (!_isReadOnly && input.Key == Key::Tab
			&& _acceptsTab)
		{
			this->InputText(L"\t");
			this->selRangeDirty = true;
			UpdateScroll();
			this->InvalidateVisual();
			return true;
		}
		if (input.HasModifier(ModifierKeys::Control))
		{
			if (input.Key == Key::A)
			{
				SelectAll();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (input.Key == Key::C)
			{
				(void)Copy();
				return true;
			}
			if (!_isReadOnly && input.Key == Key::V)
			{
				(void)Paste();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (!_isReadOnly && input.Key == Key::X)
			{
				(void)Cut();
				UpdateScroll();
				InvalidateVisual();
				return true;
			}
			if (!_isReadOnly && input.Key == Key::Z)
			{
				this->Undo();
				UpdateScroll();
				this->InvalidateVisual();
				return true;
			}
			if (!_isReadOnly && input.Key == Key::Y)
			{
				this->Redo();
				UpdateScroll();
				this->InvalidateVisual();
				return true;
			}
		}

		if (input.Key == Key::Back)
		{
			handled = true;
			if (!_isReadOnly)
			{
				InputBack(input.HasModifier(ModifierKeys::Control));
				UpdateScroll();
			}
		}
		else if (input.Key == Key::Return)
		{
			handled = true;
			if (!_isReadOnly)
			{
				if (input.HasModifier(ModifierKeys::Shift))
					InputLineBreak();
				else
					InputText(L"\r\n");
				UpdateScroll(true);
			}
		}
		else if (input.Key == Key::Delete)
		{
			handled = true;
			if (!_isReadOnly)
			{
				this->InputDelete(
					input.HasModifier(ModifierKeys::Control));
				UpdateScroll();
			}
		}
		else if (input.Key == Key::Right)
		{
			const bool forward = LogicalForwardForHorizontalArrow(true);
			handled = input.HasModifier(ModifierKeys::Control)
				? MoveSelectionByWord(forward,
					input.HasModifier(ModifierKeys::Shift))
				: MoveSelectionByCharacter(forward,
					input.HasModifier(ModifierKeys::Shift));
		}
		else if (input.Key == Key::Left)
		{
			const bool forward = LogicalForwardForHorizontalArrow(false);
			handled = input.HasModifier(ModifierKeys::Control)
				? MoveSelectionByWord(forward,
					input.HasModifier(ModifierKeys::Shift))
				: MoveSelectionByCharacter(forward,
					input.HasModifier(ModifierKeys::Shift));
		}
		else if (input.Key == Key::Up)
		{
			handled = input.HasModifier(ModifierKeys::Control)
				? MoveSelectionByParagraph(false,
					input.HasModifier(ModifierKeys::Shift))
				: MoveSelectionVertically(false,
					input.HasModifier(ModifierKeys::Shift));
		}
		else if (input.Key == Key::Down)
		{
			handled = input.HasModifier(ModifierKeys::Control)
				? MoveSelectionByParagraph(true,
					input.HasModifier(ModifierKeys::Shift))
				: MoveSelectionVertically(true,
					input.HasModifier(ModifierKeys::Shift));
		}
		else if (input.Key == Key::Home)
		{
			handled = input.HasModifier(ModifierKeys::Control)
				? MoveSelectionToDocumentBoundary(false,
					input.HasModifier(ModifierKeys::Shift))
				: MoveSelectionToLineBoundary(false,
					input.HasModifier(ModifierKeys::Shift));
		}
		else if (input.Key == Key::End)
		{
			handled = input.HasModifier(ModifierKeys::Control)
				? MoveSelectionToDocumentBoundary(true,
					input.HasModifier(ModifierKeys::Shift))
				: MoveSelectionToLineBoundary(true,
					input.HasModifier(ModifierKeys::Shift));
		}
		else if (input.Key == Key::PageUp)
		{
			handled = MoveSelectionByPage(false,
				input.HasModifier(ModifierKeys::Shift));
		}
		else if (input.Key == Key::PageDown)
		{
			handled = MoveSelectionByPage(true,
				input.HasModifier(ModifierKeys::Shift));
		}
		else if (input.Key == Key::Escape)
		{
			handled = true;
		}
		if (handled) NotifySelectionChanged();
		auto eventArgs = input.CreateKeyEventArgs();
		this->OnKeyDown(this, eventArgs);
		this->InvalidateVisual();
		return handled;
	}
	case InputReportKind::KeyUp:
	{
		auto eventArgs = input.CreateKeyEventArgs();
		this->OnKeyUp(this, eventArgs);
		this->InvalidateVisual();
	}
	break;
	}
	return true;
}
