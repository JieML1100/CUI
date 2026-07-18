#include "XamlEditorDialog.h"
#include "DesignerCanvas.h"

#include <algorithm>

XamlEditorDialog::XamlEditorDialog(
	DesignerCanvas* canvas,
	std::wstring initialXaml)
	: Form(L"编辑 CUI XAML", POINT{ 180, 70 }, SIZE{ 1080, 800 }),
	  _canvas(canvas),
	  _lastValidXaml(initialXaml)
{
	VisibleHead = true;
	MinBox = false;
	MaxBox = true;
	AllowResize = true;
	BackColor = Colors::WhiteSmoke;

	auto* tip = AddControl(new Label(
		L"停止输入 300ms 后验证；有效 XAML 会立即同步到设计画布。",
		20, 14));
	tip->Size = { 1035, 24 };
	tip->AnchorStyles = AnchorStyles::Left | AnchorStyles::Right
		| AnchorStyles::Top;
	tip->Margin = { 0, 0, 25, 0 };

	_editor = AddControl(new RichTextBox(
		std::move(initialXaml), 20, 46, 1035, 648));
	_editor->AllowMultiLine = true;
	_editor->AllowTabInput = true;
	_editor->EnableVirtualization = true;
	_editor->VirtualizeThreshold = 12000;
	_editor->BackColor = Colors::White;
	_editor->FocusedColor = Colors::White;
	_editor->AnchorStyles = AnchorStyles::Left | AnchorStyles::Right
		| AnchorStyles::Top | AnchorStyles::Bottom;
	_editor->Margin = { 0, 0, 25, 106 };

	_status = AddControl(new Label(L"XAML 已与当前画布同步。", 20, 708));
	_status->Size = { 690, 42 };
	_status->AnchorStyles = AnchorStyles::Left | AnchorStyles::Right
		| AnchorStyles::Bottom;
	_status->Margin = { 0, 0, 370, 54 };

	_locateError = AddControl(new Button(L"定位错误", 720, 710, 80, 36));
	_locateError->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	_locateError->Margin = { 0, 0, 260, 54 };
	_locateError->Enable = false;
	_locateError->AccessibleDescription = L"当前没有可定位的 XAML 错误。";

	_restorePreview = AddControl(new Button(L"恢复有效版本", 808, 710, 104, 36));
	_restorePreview->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	_restorePreview->Margin = { 0, 0, 148, 54 };
	_restorePreview->Enable = false;
	_restorePreview->AccessibleDescription = L"当前源码已经是最后一次有效版本。";

	auto* ok = AddControl(new Button(L"确定", 920, 710, 56, 36));
	ok->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	ok->Margin = { 0, 0, 84, 54 };
	auto* cancel = AddControl(new Button(L"取消", 984, 710, 56, 36));
	cancel->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	cancel->Margin = { 0, 0, 20, 54 };

	_editor->OnTextChanged +=
		[this](Control*, std::wstring, std::wstring)
		{
			if (_loading) return;
			SchedulePreview();
			RefreshRestorePreviewState();
		};
	_locateError->OnMouseClick +=
		[this](Control*, MouseEventArgs) { LocateDiagnostic(); };
	_restorePreview->OnMouseClick +=
		[this](Control*, MouseEventArgs) { RestoreLastValidPreview(); };
	ok->OnMouseClick +=
		[this](Control*, MouseEventArgs) { Accept(); };
	cancel->OnMouseClick +=
		[this](Control*, MouseEventArgs) { Cancel(); };

	RefreshRestorePreviewState();
}

XamlEditorDialog::~XamlEditorDialog()
{
	if (Handle) (void)::KillTimer(Handle, PreviewTimerId);
}

void XamlEditorDialog::SchedulePreview()
{
	ClearDiagnostic();
	ShowStatus(L"正在等待输入完成…", false);
	if (!Handle) return;
	(void)::KillTimer(Handle, PreviewTimerId);
	(void)::SetTimer(
		Handle, PreviewTimerId, PreviewDelayMilliseconds, nullptr);
}

bool XamlEditorDialog::ValidateAndPreview()
{
	if (Handle) (void)::KillTimer(Handle, PreviewTimerId);
	if (!_canvas || !_editor)
	{
		ShowStatus(L"设计画布不可用。", true);
		return false;
	}

	std::wstring error;
	DesignerModel::XamlDocumentDiagnostic diagnostic;
	if (!_canvas->PreviewXamlDocumentText(
		_editor->Text, &error, &diagnostic))
	{
		if (diagnostic.HasSourceOffset())
		{
			_diagnosticOffset = diagnostic.Utf16Offset;
			_locateError->Enable = true;
			_locateError->AccessibleDescription = L"定位到第 "
				+ std::to_wstring(diagnostic.Line) + L" 行，第 "
				+ std::to_wstring(diagnostic.Column) + L" 列的 XAML 错误。";
			_locateError->InvalidateVisual();
		}
		auto status = error.empty() ? L"XAML 无法应用。" : std::move(error);
		if (diagnostic.HasLocation())
			status = L"第 " + std::to_wstring(diagnostic.Line)
				+ L" 行，第 " + std::to_wstring(diagnostic.Column)
				+ L" 列：" + status + L"（F8 定位）";
		ShowStatus(std::move(status), true);
		RefreshRestorePreviewState();
		return false;
	}

	_lastValidXaml = _editor->Text;
	ClearDiagnostic();
	RefreshRestorePreviewState();
	ShowStatus(L"验证通过，已同步到设计画布："
		+ std::to_wstring(_canvas->GetAllControls().size())
		+ L" 个控件。", false);
	return true;
}

void XamlEditorDialog::ClearDiagnostic()
{
	_diagnosticOffset = DesignerModel::XamlDocumentDiagnostic::UnknownOffset;
	if (!_locateError) return;
	_locateError->Enable = false;
	_locateError->AccessibleDescription = L"当前没有可定位的 XAML 错误。";
	_locateError->InvalidateVisual();
}

void XamlEditorDialog::LocateDiagnostic()
{
	if (!_editor
		|| _diagnosticOffset == DesignerModel::XamlDocumentDiagnostic::UnknownOffset)
		return;
	const auto offset = (std::min)(_diagnosticOffset, _editor->Text.size());
	_editor->Select(
		static_cast<int>(offset), offset < _editor->Text.size() ? 1 : 0);
	(void)_editor->Focus();
	_editor->ScrollSelectionIntoView();
}

void XamlEditorDialog::RefreshRestorePreviewState()
{
	if (!_restorePreview) return;
	const bool canRestore = _editor && _editor->Text != _lastValidXaml;
	_restorePreview->Enable = canRestore;
	_restorePreview->AccessibleDescription = canRestore
		? L"放弃当前草稿并恢复最后一次通过验证的 XAML；可用 Ctrl+Z 撤销恢复。"
		: L"当前源码已经是最后一次有效版本。";
	_restorePreview->InvalidateVisual();
}

void XamlEditorDialog::RestoreLastValidPreview()
{
	if (!_editor || _editor->Text == _lastValidXaml) return;
	const int caret = static_cast<int>((std::min)(
		static_cast<size_t>((std::max)(0, _editor->SelectionEnd)),
		_lastValidXaml.size()));
	_loading = true;
	_editor->ReplaceAllTextAndSelect(_lastValidXaml, caret, 0);
	_loading = false;
	_editor->ScrollSelectionIntoView();
	ClearDiagnostic();
	RefreshRestorePreviewState();
	ShowStatus(L"已恢复最后一次有效 XAML；可用 Ctrl+Z 恢复草稿。", false);
	(void)_editor->Focus();
}

void XamlEditorDialog::Accept()
{
	if (!ValidateAndPreview()) return;
	Applied = true;
	Close();
}

void XamlEditorDialog::Cancel()
{
	Applied = false;
	Close();
}

void XamlEditorDialog::ShowStatus(std::wstring message, bool isError)
{
	if (!_status) return;
	_status->Text = std::move(message);
	_status->ForeColor = isError ? Colors::IndianRed : Colors::DarkGreen;
	_status->AccessibleDescription = _status->Text;
	_status->InvalidateVisual();
}

bool XamlEditorDialog::ProcessMessage(
	UINT message,
	WPARAM wParam,
	LPARAM lParam,
	int localX,
	int localY)
{
	if (message == WM_CHAR && _suppressedCharacter != L'\0'
		&& static_cast<wchar_t>(wParam) == _suppressedCharacter)
	{
		_suppressedCharacter = L'\0';
		return true;
	}
	if (message == WM_CHAR) _suppressedCharacter = L'\0';
	if (message == WM_TIMER && wParam == PreviewTimerId)
	{
		(void)ValidateAndPreview();
		return true;
	}
	if (message == WM_KEYDOWN && wParam == VK_RETURN
		&& (::GetKeyState(VK_CONTROL) & 0x8000) != 0)
	{
		_suppressedCharacter = L'\r';
		Accept();
		return true;
	}
	if (message == WM_KEYDOWN && wParam == VK_F8
		&& _diagnosticOffset
			!= DesignerModel::XamlDocumentDiagnostic::UnknownOffset)
	{
		LocateDiagnostic();
		return true;
	}
	if (message == WM_KEYDOWN && wParam == VK_ESCAPE)
	{
		Cancel();
		return true;
	}
	return Form::ProcessMessage(
		message, wParam, lParam, localX, localY);
}
