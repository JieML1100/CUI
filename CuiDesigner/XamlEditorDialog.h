#pragma once

#include "../CUI/include/Form.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/Button.h"
#include "DesignerModel/XamlDocumentParser.h"

#include <string>

class DesignerCanvas;

/**
 * Thin source editor for one complete CUI XAML document.
 *
 * The dialog deliberately owns no language-service features.  It only edits
 * text, validates after a short idle delay, and transactionally synchronizes
 * valid documents to the design surface.  A future Visual Studio host can
 * replace this shell without replacing the XAML frontend.
 */
class XamlEditorDialog final : public Form
{
	friend bool RunDesignerSelfTest(std::wstring& report);

public:
	bool Applied = false;

	XamlEditorDialog(DesignerCanvas* canvas, std::wstring initialXaml);
	~XamlEditorDialog();

	bool ProcessMessage(
		UINT message,
		WPARAM wParam,
		LPARAM lParam,
		int localX,
		int localY) override;

private:
	DesignerCanvas* _canvas = nullptr;
	RichTextBox* _editor = nullptr;
	Label* _status = nullptr;
	Button* _locateError = nullptr;
	Button* _restorePreview = nullptr;
	bool _loading = false;
	wchar_t _suppressedCharacter = L'\0';
	std::size_t _diagnosticOffset =
		DesignerModel::XamlDocumentDiagnostic::UnknownOffset;
	std::wstring _lastValidXaml;

	static constexpr UINT_PTR PreviewTimerId = 0xC0D4;
	static constexpr UINT PreviewDelayMilliseconds = 300;

	void SchedulePreview();
	bool ValidateAndPreview();
	void ClearDiagnostic();
	void LocateDiagnostic();
	void RefreshRestorePreviewState();
	void RestoreLastValidPreview();
	void Accept();
	void Cancel();
	void ShowStatus(std::wstring message, bool isError);
};
