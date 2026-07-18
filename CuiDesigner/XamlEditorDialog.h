#pragma once

#include "../CUI/include/Form.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/CheckBox.h"
#include "../CUI/include/DropDownPopup.h"
#include "../CUI/include/ContextMenu.h"
#include "DesignerModel/XamlEditorAssist.h"

#include <map>
#include <string>
#include <vector>

class DesignerCanvas;

/** Debounced live editor for the complete current CUI XAML document. */
class XamlEditorDialog final : public Form
{
friend bool RunDesignerSelfTest(std::wstring& report);

public:
	bool Applied = false;
	std::size_t GetAppliedCheckpointCount() const noexcept
	{
		return _appliedCheckpointCount;
	}

	XamlEditorDialog(
		DesignerCanvas* canvas,
		std::wstring initialXaml,
		std::map<std::string, std::vector<std::wstring>>
			compatibleUserHandlers = {});
	~XamlEditorDialog();

	bool ProcessMessage(
		UINT message,
		WPARAM wParam,
		LPARAM lParam,
		int localX,
		int localY) override;

private:
	DesignerCanvas* _canvas = nullptr;
	std::map<std::string, std::vector<std::wstring>>
		_compatibleUserHandlers;
	RichTextBox* _editor = nullptr;
	TextBox* _findText = nullptr;
	TextBox* _replaceText = nullptr;
	CheckBox* _matchCase = nullptr;
	Label* _position = nullptr;
	Label* _status = nullptr;
	Button* _locateError = nullptr;
	Button* _restorePreview = nullptr;
	Button* _applyButton = nullptr;
	DropDownPopup* _completionPopup = nullptr;
	ContextMenu* _editorMenu = nullptr;
	DesignerModel::XamlEditorAssist::CompletionContext _completionContext;
	std::size_t _completionCaret = static_cast<std::size_t>(-1);
	bool _loading = false;
	bool _applyingCompletion = false;
	wchar_t _suppressedCharacter = L'\0';
	std::size_t _diagnosticOffset = static_cast<std::size_t>(-1);
	std::wstring _lastValidXaml;
	bool _transactionOpen = false;
	std::size_t _appliedCheckpointCount = 0;
	static constexpr UINT_PTR PreviewTimerId = 0xC0D4;
	static constexpr UINT PreviewDelayMilliseconds = 300;
	enum EditorMenuCommandId
	{
		EditorUndo = 32101,
		EditorRedo,
		EditorCut,
		EditorCopy,
		EditorPaste,
		EditorDelete,
		EditorSelectAll,
		EditorIndent,
		EditorOutdent,
		EditorFormat,
	};

	void SchedulePreview();
	bool ValidateAndPreview();
	void ClearDiagnostic();
	void LocateDiagnostic();
	void RefreshRestorePreviewState();
	void RestoreLastValidPreview();
	void FocusFind(bool replace);
	bool FindNext(bool backwards);
	void ReplaceCurrent();
	void ReplaceAll();
	void UpdateCaretPosition();
	void UpdateSyntaxHighlighting();
	void UpdateTagMatch();
	void SelectCanvasControlAtCaret();
	void LocateCanvasSelectionInSource(
		bool validatePreview = true,
		bool reportStatus = true);
	void RefreshCompletion(bool manual);
	void ApplyCompletion(const std::wstring& selectedText);
	void CloseCompletionPopup();
	void DestroyCompletionPopup() noexcept;
	void InsertSmartNewLine();
	void IndentSelection(bool outdent);
	void RefreshEditorContextMenu();
	void ShowEditorContextMenu(int x, int y, bool relativeToEditor);
	void ShowEditorContextMenuAtCaret();
	void OnEditorMenuCommand(int commandId);
	void FormatDocument();
	bool ApplyCheckpoint();
	void Accept();
	void Cancel();
	void ShowStatus(std::wstring message, bool isError);
};
