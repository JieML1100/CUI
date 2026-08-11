#pragma once

#include "RoutedCommand.h"

/**
 * WPF-compatible routed command identities for rich-text editing.
 *
 * The command objects do not own an editor. RichTextBox registers class
 * handlers and the standard class input gestures for the supported subset.
 */
class EditingCommands final
{
public:
	EditingCommands() = delete;

	static const RoutedCommand& ToggleBold();
	static const RoutedCommand& ToggleItalic();
	static const RoutedCommand& ToggleUnderline();
	static const RoutedCommand& ResetFormat();
	static const RoutedCommand& IncreaseFontSize();
	static const RoutedCommand& DecreaseFontSize();
	static const RoutedCommand& DeleteNextWord();
	static const RoutedCommand& DeletePreviousWord();
	static const RoutedCommand& MoveRightByCharacter();
	static const RoutedCommand& MoveLeftByCharacter();
	static const RoutedCommand& SelectRightByCharacter();
	static const RoutedCommand& SelectLeftByCharacter();
	static const RoutedCommand& MoveRightByWord();
	static const RoutedCommand& MoveLeftByWord();
	static const RoutedCommand& SelectRightByWord();
	static const RoutedCommand& SelectLeftByWord();
	static const RoutedCommand& MoveUpByLine();
	static const RoutedCommand& MoveDownByLine();
	static const RoutedCommand& SelectUpByLine();
	static const RoutedCommand& SelectDownByLine();
	static const RoutedCommand& MoveUpByPage();
	static const RoutedCommand& MoveDownByPage();
	static const RoutedCommand& SelectUpByPage();
	static const RoutedCommand& SelectDownByPage();
	static const RoutedCommand& MoveUpByParagraph();
	static const RoutedCommand& MoveDownByParagraph();
	static const RoutedCommand& SelectUpByParagraph();
	static const RoutedCommand& SelectDownByParagraph();
	static const RoutedCommand& MoveToLineStart();
	static const RoutedCommand& MoveToLineEnd();
	static const RoutedCommand& MoveToDocumentStart();
	static const RoutedCommand& MoveToDocumentEnd();
	static const RoutedCommand& SelectToLineStart();
	static const RoutedCommand& SelectToLineEnd();
	static const RoutedCommand& SelectToDocumentStart();
	static const RoutedCommand& SelectToDocumentEnd();
	static const RoutedCommand& AlignLeft();
	static const RoutedCommand& AlignCenter();
	static const RoutedCommand& AlignRight();
	static const RoutedCommand& AlignJustify();
};
