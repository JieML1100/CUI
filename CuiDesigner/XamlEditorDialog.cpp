#include "XamlEditorDialog.h"
#include "ProgrammaticControlFactory.h"
#include "DesignerCanvas.h"
#include "../CUI/include/WindowInfrastructure.h"

#include <algorithm>

XamlEditorDialog::XamlEditorDialog(
	DesignerCanvas* canvas,
	std::wstring initialXaml)
	: Window(),
	  _canvas(canvas),
	  _lastValidXaml(initialXaml)
{
	Title = L"编辑 CUI XAML";
	Left = 180.0f;
	Top = 70.0f;
	Width = 1080.0f;
	Height = 800.0f;
	ResizeMode = ::ResizeMode::CanResize;
	Background = Colors::WhiteSmoke;
	auto contentOwner = std::make_unique<Panel>();
	contentOwner->BorderThickness = 0.0f;
	contentOwner->Background = D2D1_COLOR_F{ 0, 0, 0, 0 };
	auto* contentRoot = static_cast<Panel*>(SetVisualContent(std::move(contentOwner)));
	auto addContent = [contentRoot](auto* child) { return contentRoot->AdoptVisualChild(child); };

	auto* tip = addContent(cui::designer::NewControl<Label>(
		L"停止输入 300ms 后验证；有效 XAML 会立即同步到设计画布。",
		20, 14));
	tip->Width = 1035.0f;
	tip->Height = 24.0f;

	_editor = addContent(cui::designer::NewControl<RichTextBox>(
		std::move(initialXaml), 20, 46, 1035, 648));
	_editor->Focusable = true;
	_editor->AcceptsTab = true;
	_editor->Background = Colors::White;
	_editor->BorderBrush = Colors::White;

	_status = addContent(cui::designer::NewControl<Label>(L"XAML 已与当前画布同步。", 20, 708));
	_status->Width = 690.0f;
	_status->Height = 42.0f;

	_locateError = addContent(cui::designer::NewControl<Button>(L"定位错误", 720, 710, 80, 36));
	_locateError->IsEnabled = false;
	_locateError->AutomationFullDescription = L"当前没有可定位的 XAML 错误。";

	_restorePreview = addContent(cui::designer::NewControl<Button>(L"恢复有效版本", 808, 710, 104, 36));
	_restorePreview->IsEnabled = false;
	_restorePreview->AutomationFullDescription = L"当前源码已经是最后一次有效版本。";

	auto* ok = addContent(cui::designer::NewControl<Button>(L"确定", 920, 710, 56, 36));
	auto* cancel = addContent(cui::designer::NewControl<Button>(L"取消", 984, 710, 56, 36));

	_editor->OnTextChanged +=
		[this](Control*, TextChangedEventArgs&)
		{
			if (_loading) return;
			SchedulePreview();
			RefreshRestorePreviewState();
		};
	_locateError->Click +=
		[this](Control*, RoutedEventArgs&) { LocateDiagnostic(); };
	_restorePreview->Click +=
		[this](Control*, RoutedEventArgs&) { RestoreLastValidPreview(); };
	ok->Click +=
		[this](Control*, RoutedEventArgs&) { Accept(); };
	cancel->Click +=
		[this](Control*, RoutedEventArgs&) { Cancel(); };

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
			_locateError->IsEnabled = true;
			_locateError->AutomationFullDescription = L"定位到第 "
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
	_locateError->IsEnabled = false;
	_locateError->AutomationFullDescription = L"当前没有可定位的 XAML 错误。";
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
	_restorePreview->IsEnabled = canRestore;
	_restorePreview->AutomationFullDescription = canRestore
		? L"放弃当前草稿并恢复最后一次通过验证的 XAML；可用 Ctrl+Z 撤销恢复。"
		: L"当前源码已经是最后一次有效版本。";
	_restorePreview->InvalidateVisual();
}

void XamlEditorDialog::RestoreLastValidPreview()
{
	if (!_editor || _editor->Text == _lastValidXaml) return;
	const int caret = static_cast<int>((std::min)(
		static_cast<size_t>((std::max)(0, _editor->CaretIndex)),
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
	_status->Foreground = isError ? Colors::IndianRed : Colors::DarkGreen;
	_status->AutomationFullDescription = _status->Text;
	_status->InvalidateVisual();
}

bool XamlEditorDialog::OnPreviewInputReport(const InputReport& input)
{
	if (input.Kind != InputReportKind::KeyDown) return false;
	const Key key = input.Key;
	if (key == Key::Return && input.HasModifier(ModifierKeys::Control))
	{
		cui::framework::WindowAccess::TextComposition(
			*this).SuppressNextCharacter(L'\r');
		Accept();
		return true;
	}
	if (key == Key::F8
		&& _diagnosticOffset
			!= DesignerModel::XamlDocumentDiagnostic::UnknownOffset)
	{
		LocateDiagnostic();
		return true;
	}
	if (key == Key::Escape)
	{
		Cancel();
		return true;
	}
	return false;
}

std::optional<LRESULT> XamlEditorDialog::OnPlatformMessage(
	UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_TIMER && wParam == PreviewTimerId)
	{
		(void)ValidateAndPreview();
		return LRESULT{ 0 };
	}
	(void)lParam;
	return std::nullopt;
}
