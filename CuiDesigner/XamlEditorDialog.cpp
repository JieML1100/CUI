#include "XamlEditorDialog.h"
#include "DesignerCanvas.h"
#include "DesignerModel/XamlDocumentParser.h"
#include "../CUI/include/TextEditCore.h"

#include <algorithm>
#include <cmath>
#include <limits>

using namespace DesignerModel::XamlEditorAssist;

namespace
{
	D2D1_COLOR_F SyntaxForeColor(XamlSyntaxKind kind)
	{
		switch (kind)
		{
		case XamlSyntaxKind::ElementName: return Colors::DodgerBlue4;
		case XamlSyntaxKind::AttributeName: return Colors::Firebrick4;
		case XamlSyntaxKind::AttributeValue: return Colors::Sienna4;
		case XamlSyntaxKind::Comment: return Colors::DarkGreen;
		case XamlSyntaxKind::CData: return Colors::DarkSlateGray;
		case XamlSyntaxKind::ProcessingInstruction: return Colors::Purple4;
		case XamlSyntaxKind::Declaration: return Colors::Purple4;
		case XamlSyntaxKind::EntityReference: return Colors::DarkOrange4;
		case XamlSyntaxKind::MarkupDelimiter:
		default:
			return Colors::DimGrey;
		}
	}
}

XamlEditorDialog::XamlEditorDialog(
	DesignerCanvas* canvas,
	std::wstring initialXaml,
	std::map<std::string, std::vector<std::wstring>> compatibleUserHandlers)
	: Form(L"实时编辑 CUI XAML", POINT{ 180, 70 }, SIZE{ 1080, 800 }),
	  _canvas(canvas),
	  _compatibleUserHandlers(std::move(compatibleUserHandlers)),
	  _lastValidXaml(initialXaml),
	  _transactionOpen(canvas && canvas->HasActiveDocumentTransaction())
{
	VisibleHead = true;
	MinBox = false;
	MaxBox = true;
	AllowResize = true;
	BackColor = Colors::WhiteSmoke;

	auto* tip = AddControl(new Label(
		L"有效 XAML 会在停止输入 300ms 后更新画布；Ctrl+S 应用检查点，Tab / Shift+Tab 调整缩进。",
		20, 12));
	tip->Size = { 1035, 24 };
	tip->AnchorStyles = AnchorStyles::Left | AnchorStyles::Right | AnchorStyles::Top;
	tip->Margin = { 0, 0, 25, 0 };

	auto* findLabel = AddControl(new Label(L"查找", 20, 48));
	findLabel->Size = { 36, 26 };
	_findText = AddControl(new TextBox(L"", 60, 43, 220, 30));
	_findText->AccessibleName = L"查找文本";
	auto* previous = AddControl(new Button(L"上一个", 288, 42, 74, 32));
	auto* next = AddControl(new Button(L"下一个", 370, 42, 74, 32));
	auto* replaceLabel = AddControl(new Label(L"替换", 456, 48));
	replaceLabel->Size = { 38, 26 };
	_replaceText = AddControl(new TextBox(L"", 500, 43, 178, 30));
	_replaceText->AccessibleName = L"替换文本";
	auto* replace = AddControl(new Button(L"替换", 686, 42, 72, 32));
	auto* replaceAll = AddControl(new Button(L"全部替换", 766, 42, 82, 32));
	_matchCase = AddControl(new CheckBox(L"大小写", 856, 48));
	_matchCase->AccessibleName = L"区分大小写";
	_position = AddControl(new Label(L"行 1，列 1", 952, 48));
	_position->Size = { 103, 26 };
	_position->AnchorStyles = AnchorStyles::Right | AnchorStyles::Top;
	_position->Margin = { 0, 0, 25, 0 };

	_editor = AddControl(new RichTextBox(
		std::move(initialXaml), 20, 80, 1035, 614));
	_editor->AllowMultiLine = true;
	_editor->AllowTabInput = true;
	_editor->EnableVirtualization = true;
	_editor->VirtualizeThreshold = 12000;
	_editor->BackColor = Colors::White;
	_editor->FocusedColor = Colors::White;
	_editor->AnchorStyles = AnchorStyles::Left | AnchorStyles::Right
		| AnchorStyles::Top | AnchorStyles::Bottom;
	_editor->Margin = { 0, 0, 25, 106 };

	_status = AddControl(new Label(L"XAML 已与当前画布同步。", 20, 704));
	_status->Size = { 310, 42 };
	_status->AnchorStyles = AnchorStyles::Left | AnchorStyles::Right
		| AnchorStyles::Bottom;
	_status->Margin = { 0, 0, 750, 54 };

	auto* locateSelection = AddControl(new Button(
		L"定位选区", 344, 710, 80, 36));
	locateSelection->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	locateSelection->Margin = { 0, 0, 656, 54 };
	locateSelection->AccessibleName = L"在 XAML 中定位设计选区";
	locateSelection->AccessibleDescription =
		L"在源码中选中当前画布控件的元素名称。";
	auto* selectControl = AddControl(new Button(
		L"选中控件", 432, 710, 80, 36));
	selectControl->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	selectControl->Margin = { 0, 0, 568, 54 };
	selectControl->AccessibleName = L"在画布中选中 XAML 控件";
	selectControl->AccessibleDescription =
		L"根据光标所在元素的稳定 ID 或名称选中设计控件。";

	_locateError = AddControl(new Button(L"定位错误", 520, 710, 80, 36));
	_locateError->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	_locateError->Margin = { 0, 0, 480, 54 };
	_locateError->Enable = false;
	_locateError->AccessibleName = L"定位 XAML 错误";
	_locateError->AccessibleDescription = L"当前没有可定位的 XAML 错误。";
	_restorePreview = AddControl(new Button(
		L"还原预览", 608, 710, 80, 36));
	_restorePreview->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	_restorePreview->Margin = { 0, 0, 392, 54 };
	_restorePreview->Enable = false;
	_restorePreview->AccessibleName = L"还原最后一次有效 XAML 预览";
	_restorePreview->AccessibleDescription =
		L"当前源码已经是最后一次有效预览。";
	auto* format = AddControl(new Button(L"格式化", 696, 710, 80, 36));
	format->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	format->Margin = { 0, 0, 304, 54 };
	_applyButton = AddControl(new Button(L"应用", 784, 710, 80, 36));
	_applyButton->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	_applyButton->Margin = { 0, 0, 216, 54 };
	_applyButton->AccessibleName = L"应用 XAML 但保持编辑器打开";
	_applyButton->AccessibleDescription =
		L"把当前有效 XAML 保存为独立撤销步骤并继续编辑。快捷键 Ctrl+S。";
	auto* ok = AddControl(new Button(L"确定", 872, 710, 80, 36));
	ok->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	ok->Margin = { 0, 0, 128, 54 };
	auto* cancel = AddControl(new Button(L"取消", 960, 710, 80, 36));
	cancel->AnchorStyles = AnchorStyles::Right | AnchorStyles::Bottom;
	cancel->Margin = { 0, 0, 40, 54 };

	_editorMenu = AddControl(new ContextMenu());
	_editorMenu->AddItem(L"撤销", EditorUndo)->Shortcut = L"Ctrl+Z";
	_editorMenu->AddItem(L"重做", EditorRedo)->Shortcut = L"Ctrl+Y";
	_editorMenu->AddSeparator();
	_editorMenu->AddItem(L"剪切", EditorCut)->Shortcut = L"Ctrl+X";
	_editorMenu->AddItem(L"复制", EditorCopy)->Shortcut = L"Ctrl+C";
	_editorMenu->AddItem(L"粘贴", EditorPaste)->Shortcut = L"Ctrl+V";
	_editorMenu->AddItem(L"删除", EditorDelete)->Shortcut = L"Delete";
	_editorMenu->AddSeparator();
	_editorMenu->AddItem(L"全选", EditorSelectAll)->Shortcut = L"Ctrl+A";
	_editorMenu->AddSeparator();
	_editorMenu->AddItem(L"增加缩进", EditorIndent)->Shortcut = L"Tab";
	_editorMenu->AddItem(L"减少缩进", EditorOutdent)->Shortcut = L"Shift+Tab";
	_editorMenu->AddItem(L"格式化文档", EditorFormat)
		->Shortcut = L"Ctrl+Shift+F";
	_editorMenu->OnMenuCommand += [this](Control*, int commandId)
		{ OnEditorMenuCommand(commandId); };

	_editor->OnTextChanged +=
		[this](Control*, std::wstring, std::wstring)
		{
			if (!_loading)
			{
				SchedulePreview();
				RefreshRestorePreviewState();
				UpdateSyntaxHighlighting();
				UpdateTagMatch();
				if (!_applyingCompletion) RefreshCompletion(false);
			}
		};
	_editor->OnSelectionChanged +=
		[this](Control*)
		{
			UpdateCaretPosition();
			UpdateTagMatch();
			if (_completionPopup && _completionPopup->IsOpen()
				&& static_cast<size_t>((std::max)(0, _editor->SelectionEnd))
					!= _completionCaret)
				CloseCompletionPopup();
		};
	_editor->OnMouseUp +=
		[this](Control*, MouseEventArgs args)
		{
			if (args.Buttons == MouseButtons::Right)
				ShowEditorContextMenu(args.X, args.Y, true);
		};
	previous->OnMouseClick +=
		[this](Control*, MouseEventArgs) { (void)FindNext(true); };
	next->OnMouseClick +=
		[this](Control*, MouseEventArgs) { (void)FindNext(false); };
	replace->OnMouseClick +=
		[this](Control*, MouseEventArgs) { ReplaceCurrent(); };
	replaceAll->OnMouseClick +=
		[this](Control*, MouseEventArgs) { ReplaceAll(); };
	format->OnMouseClick +=
		[this](Control*, MouseEventArgs) { FormatDocument(); };
	_applyButton->OnMouseClick +=
		[this](Control*, MouseEventArgs) { (void)ApplyCheckpoint(); };
	locateSelection->OnMouseClick +=
		[this](Control*, MouseEventArgs)
		{ LocateCanvasSelectionInSource(); };
	selectControl->OnMouseClick +=
		[this](Control*, MouseEventArgs) { SelectCanvasControlAtCaret(); };
	_locateError->OnMouseClick +=
		[this](Control*, MouseEventArgs) { LocateDiagnostic(); };
	_restorePreview->OnMouseClick +=
		[this](Control*, MouseEventArgs) { RestoreLastValidPreview(); };
	ok->OnMouseClick +=
		[this](Control*, MouseEventArgs) { Accept(); };
	cancel->OnMouseClick +=
		[this](Control*, MouseEventArgs) { Cancel(); };
	// WM_NCDESTROY releases the Form's child controls before the derived
	// destructor runs. The completion popup is owned by this dialog but keeps
	// the editor as its positioning parent, so dispose it while that parent is
	// still alive.
	OnFormClosed += [this](Form*) { DestroyCompletionPopup(); };
	UpdateCaretPosition();
	UpdateSyntaxHighlighting();
	UpdateTagMatch();
	RefreshRestorePreviewState();
	LocateCanvasSelectionInSource(false, false);
}

XamlEditorDialog::~XamlEditorDialog()
{
	if (Handle) (void)::KillTimer(Handle, PreviewTimerId);
	DestroyCompletionPopup();
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
		RefreshRestorePreviewState();
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
		std::wstring status = error.empty() ? L"XAML 无法应用。" : std::move(error);
		if (diagnostic.HasLocation())
			status = L"第 " + std::to_wstring(diagnostic.Line)
				+ L" 行，第 " + std::to_wstring(diagnostic.Column)
				+ L" 列：" + status + L" （F8 定位）";
		ShowStatus(std::move(status), true);
		RefreshRestorePreviewState();
		return false;
	}
	_lastValidXaml = _editor->Text;
	ClearDiagnostic();
	RefreshRestorePreviewState();
	ShowStatus(L"预览已更新："
		+ std::to_wstring(_canvas->GetAllControls().size())
		+ L" 个控件。", false);
	return true;
}

void XamlEditorDialog::ClearDiagnostic()
{
	_diagnosticOffset = static_cast<std::size_t>(-1);
	if (!_locateError) return;
	_locateError->Enable = false;
	_locateError->AccessibleDescription = L"当前没有可定位的 XAML 错误。";
	_locateError->InvalidateVisual();
}

void XamlEditorDialog::LocateDiagnostic()
{
	if (!_editor || _diagnosticOffset == static_cast<std::size_t>(-1)) return;
	const auto offset = (std::min)(_diagnosticOffset, _editor->Text.size());
	const int length = offset < _editor->Text.size() ? 1 : 0;
	_editor->Select(static_cast<int>(offset), length);
	(void)_editor->Focus();
	_editor->ScrollSelectionIntoView();
}

void XamlEditorDialog::RefreshRestorePreviewState()
{
	if (!_restorePreview) return;
	const bool canRestore = _editor && _editor->Text != _lastValidXaml;
	_restorePreview->Enable = canRestore;
	_restorePreview->AccessibleDescription = canRestore
		? L"放弃当前无效或尚未预览的源码，并恢复最后一次有效 XAML；可用 Ctrl+Z 撤销恢复。"
		: L"当前源码已经是最后一次有效预览。";
	_restorePreview->InvalidateVisual();
}

void XamlEditorDialog::RestoreLastValidPreview()
{
	if (!_editor || _editor->Text == _lastValidXaml) return;
	CloseCompletionPopup();
	const int caret = static_cast<int>((std::min)(
		static_cast<size_t>((std::max)(0, _editor->SelectionEnd)),
		_lastValidXaml.size()));
	_editor->ReplaceAllTextAndSelect(_lastValidXaml, caret, 0);
	_editor->ScrollSelectionIntoView();
	if (_canvas && !ValidateAndPreview()) return;
	ClearDiagnostic();
	RefreshRestorePreviewState();
	ShowStatus(L"已还原到最后一次有效预览；可用 Ctrl+Z 恢复刚才的源码。", false);
	(void)_editor->Focus();
}

void XamlEditorDialog::FocusFind(bool replace)
{
	if (!_editor || !_findText || !_replaceText) return;
	if (!replace)
	{
		const std::wstring selected = _editor->GetSelectedString();
		if (!selected.empty() && selected.size() <= 128
			&& selected.find_first_of(L"\r\n") == std::wstring::npos)
		{
			_findText->Text = selected;
		}
		_findText->SelectAll();
		(void)_findText->Focus();
		return;
	}
	_replaceText->SelectAll();
	(void)_replaceText->Focus();
}

bool XamlEditorDialog::FindNext(bool backwards)
{
	if (!_editor || !_findText || _findText->Text.empty())
	{
		ShowStatus(L"请输入要查找的文本。", true);
		FocusFind(false);
		return false;
	}

	const auto span = CuiTextEdit::NormalizeSelection(
		_editor->SelectionStart,
		_editor->SelectionEnd,
		_editor->Text.size());
	const int start = backwards ? span.start - 1 : span.end;
	CuiTextEdit::FindOptions options;
	options.matchCase = _matchCase && _matchCase->Checked;
	const int match = CuiTextEdit::FindText(
		_editor->Text, _findText->Text, start, backwards, options);
	if (match < 0)
	{
		ShowStatus(L"未找到“" + _findText->Text + L"”。", true);
		return false;
	}

	_editor->Select(match, static_cast<int>(_findText->Text.size()));
	(void)_editor->Focus();
	_editor->ScrollSelectionIntoView();
	ShowStatus(L"已定位匹配项；F3 / Shift+F3 可继续导航。", false);
	return true;
}

void XamlEditorDialog::ReplaceCurrent()
{
	if (!_editor || !_findText || !_replaceText || _findText->Text.empty())
	{
		ShowStatus(L"请输入要替换的文本。", true);
		FocusFind(false);
		return;
	}

	const auto span = CuiTextEdit::NormalizeSelection(
		_editor->SelectionStart,
		_editor->SelectionEnd,
		_editor->Text.size());
	const bool matches = static_cast<size_t>(span.Length()) == _findText->Text.size()
		&& CuiTextEdit::MatchesAt(
			_editor->Text,
			_findText->Text,
			static_cast<size_t>(span.start),
			_matchCase && _matchCase->Checked);
	if (!matches)
	{
		(void)FindNext(false);
		return;
	}

	_editor->InsertText(_replaceText->Text);
	_editor->ScrollSelectionIntoView();
	ShowStatus(L"已替换当前匹配项。", false);
}

void XamlEditorDialog::ReplaceAll()
{
	if (!_editor || !_findText || !_replaceText || _findText->Text.empty())
	{
		ShowStatus(L"请输入要替换的文本。", true);
		FocusFind(false);
		return;
	}

	auto result = CuiTextEdit::ReplaceAllText(
		_editor->Text,
		_findText->Text,
		_replaceText->Text,
		_matchCase && _matchCase->Checked);
	if (result.replacements == 0)
	{
		ShowStatus(L"没有可替换的匹配项。", true);
		return;
	}

	_editor->SelectAll();
	_editor->InsertText(result.text);
	_editor->ScrollSelectionIntoView();
	ShowStatus(L"已替换 " + std::to_wstring(result.replacements)
		+ L" 处；可用 Ctrl+Z 一次撤销。", false);
}

void XamlEditorDialog::UpdateCaretPosition()
{
	if (!_editor || !_position) return;
	const auto position = CuiTextEdit::GetTextPosition(
		_editor->Text,
		static_cast<size_t>((std::max)(0, _editor->SelectionEnd)));
	_position->Text = L"行 " + std::to_wstring(position.line)
		+ L"，列 " + std::to_wstring(position.column);
	_position->AccessibleDescription = _position->Text;
	_position->InvalidateVisual();
}

void XamlEditorDialog::UpdateSyntaxHighlighting()
{
	if (!_editor) return;
	const auto syntax = ScanXamlSyntax(_editor->Text);
	std::vector<RichTextBoxTextStyleRange> ranges;
	ranges.reserve(syntax.size());
	for (const auto& span : syntax)
	{
		if (span.Start > static_cast<size_t>((std::numeric_limits<int>::max)())
			|| span.Length > static_cast<size_t>((std::numeric_limits<int>::max)()))
			continue;
		ranges.push_back({
			static_cast<int>(span.Start),
			static_cast<int>(span.Length),
			SyntaxForeColor(span.Kind) });
	}
	_editor->SetTextStyleRanges(std::move(ranges));
}

void XamlEditorDialog::UpdateTagMatch()
{
	if (!_editor) return;
	const auto match = FindTagMatch(_editor->Text,
		static_cast<size_t>((std::max)(0, _editor->SelectionEnd)));
	std::vector<RichTextBoxTextRange> ranges;
	for (const auto& range : match.Ranges)
	{
		ranges.push_back({ static_cast<int>(range.Start),
			static_cast<int>(range.Length) });
	}
	_editor->SetHighlightRanges(std::move(ranges));
}

void XamlEditorDialog::SelectCanvasControlAtCaret()
{
	if (!_canvas || !_editor) return;
	if (!ValidateAndPreview()) return;
	const auto caret = static_cast<size_t>((std::max)(
		0, _editor->SelectionEnd));
	const auto element = FindElementAtPosition(_editor->Text, caret);
	if (!element)
	{
		ShowStatus(L"光标不在可映射的 Form 或设计控件元素内。", true);
		return;
	}
	if (element->IsForm())
	{
		_canvas->RestorePrimarySelectionByName(L"", true);
		(void)_editor->Focus();
		ShowStatus(L"已从 XAML 选中设计窗体。", false);
		return;
	}

	std::shared_ptr<DesignerControl> target;
	if (element->StableId > 0)
		for (const auto& control : _canvas->GetAllControls())
			if (control && control->StableId == element->StableId)
			{
				target = control;
				break;
			}
	if (!target && !element->Name.empty())
		for (const auto& control : _canvas->GetAllControls())
			if (control && _wcsicmp(
				control->Name.c_str(), element->Name.c_str()) == 0)
			{
				target = control;
				break;
			}
	if (!target)
	{
		ShowStatus(L"当前有效预览中找不到该 XAML 元素对应的控件。", true);
		return;
	}

	_canvas->RestorePrimarySelectionByName(target->Name, true);
	(void)_editor->Focus();
	ShowStatus(L"已从 XAML 选中控件 " + target->Name + L"。", false);
}

void XamlEditorDialog::LocateCanvasSelectionInSource(
	bool validatePreview,
	bool reportStatus)
{
	if (!_canvas || !_editor) return;
	if (validatePreview && !ValidateAndPreview()) return;
	const auto selected = _canvas->GetSelectedControl();
	const int stableId = selected ? selected->StableId : 0;
	const std::wstring name = selected
		? selected->Name : _canvas->GetDesignedFormName();
	const auto element = FindElementByDesignIdentity(
		_editor->Text, stableId, name);
	if (!element)
	{
		if (reportStatus)
			ShowStatus(selected
				? L"当前 XAML 中找不到设计选区对应的稳定 ID 或名称。"
				: L"当前 XAML 中找不到设计窗体元素。", true);
		return;
	}

	_editor->Select(
		static_cast<int>(element->ElementNameRange.Start),
		static_cast<int>(element->ElementNameRange.Length));
	_editor->ScrollSelectionIntoView();
	(void)_editor->Focus();
	UpdateCaretPosition();
	UpdateTagMatch();
	if (reportStatus)
		ShowStatus(selected
			? L"已在 XAML 中定位控件 " + selected->Name + L"。"
			: L"已在 XAML 中定位设计窗体。", false);
}

void XamlEditorDialog::CloseCompletionPopup()
{
	_completionContext = {};
	_completionCaret = static_cast<size_t>(-1);
	if (_completionPopup) _completionPopup->Hide(false, true);
}

void XamlEditorDialog::DestroyCompletionPopup() noexcept
{
	auto* popup = _completionPopup;
	_completionPopup = nullptr;
	if (!popup) return;

	// Do not route visibility/layout changes through a positioning parent that
	// may already be in teardown. OnFormClosed normally runs before child
	// cleanup; the direct detach also makes the destructor fallback idempotent.
	if (auto* form = popup->ParentForm)
	{
		if (form->ForegroundControl == popup)
			form->ForegroundControl = nullptr;
		if (form->Selected == popup)
			form->Selected = nullptr;
		if (form->UnderMouse == popup)
			form->UnderMouse = nullptr;
	}
	popup->Parent = nullptr;
	popup->ParentForm = nullptr;
	delete popup;
}

void XamlEditorDialog::RefreshCompletion(bool manual)
{
	if (!_editor || !_canvas || Selected != _editor)
	{
		CloseCompletionPopup();
		return;
	}
	const size_t caret = static_cast<size_t>((std::max)(0,
		_editor->SelectionEnd));
	auto context = GetCompletionContext(_editor->Text, caret);
	if (!context.IsValid())
	{
		CloseCompletionPopup();
		return;
	}

	std::vector<std::wstring> candidates;
	switch (context.Kind)
	{
	case CompletionKind::Element:
		candidates = _canvas->GetXamlCompletionElementNames();
		break;
	case CompletionKind::ClosingElement:
		candidates = OpenElementNames(_editor->Text, caret);
		break;
	case CompletionKind::Attribute:
		candidates = _canvas->GetXamlCompletionAttributeNames(
			context.ElementName);
		break;
	case CompletionKind::AttributeValue:
		candidates = _canvas->GetXamlCompletionAttributeValues(
			context.ElementName, context.AttributeName,
			_compatibleUserHandlers);
		break;
	default:
		break;
	}
	auto suggestions = FilterSuggestions(
		std::move(candidates), context, manual ? 120 : 80);
	if (suggestions.empty())
	{
		CloseCompletionPopup();
		return;
	}

	if (!_completionPopup)
	{
		_completionPopup = new DropDownPopup();
		_completionPopup->AccessibleName = L"XAML 补全候选";
		_completionPopup->SelectionChanged +=
			[this](DropDownPopup*, int, std::wstring selectedText)
			{ ApplyCompletion(selectedText); };
	}
	D2D1_RECT_F caretRect{};
	if (!_editor->TryGetCaretViewportRect(caretRect))
	{
		CloseCompletionPopup();
		return;
	}
	_completionContext = std::move(context);
	_completionCaret = caret;
	_completionPopup->ShowAt(this, _editor, caretRect, suggestions, 0,
		(std::max)(220.0f, static_cast<float>(_editor->Width) * 0.36f),
		27.0f, 9);
	// Keep character input in the editor while the foreground popup owns hit-testing.
	SetSelectedControl(_editor, false);
}

void XamlEditorDialog::ApplyCompletion(const std::wstring& selectedText)
{
	if (!_editor || selectedText.empty() || !_completionContext.IsValid()) return;
	const auto context = _completionContext;
	CloseCompletionPopup();
	const int start = static_cast<int>((std::min)(
		context.ReplaceStart, _editor->Text.size()));
	const int end = static_cast<int>((std::min)(
		context.ReplaceEnd, _editor->Text.size()));
	std::wstring insertion = selectedText;
	int caret = start + static_cast<int>(insertion.size());
	if (context.Kind == CompletionKind::Attribute)
	{
		insertion += L"=\"\"";
		caret = start + static_cast<int>(selectedText.size()) + 2;
	}
	_applyingCompletion = true;
	_editor->Select(start, (std::max)(0, end - start));
	_editor->InsertText(insertion);
	_editor->Select(caret, 0);
	_editor->ScrollSelectionIntoView();
	(void)_editor->Focus();
	_applyingCompletion = false;
	UpdateTagMatch();
}

void XamlEditorDialog::InsertSmartNewLine()
{
	if (!_editor) return;
	const auto span = CuiTextEdit::NormalizeSelection(
		_editor->SelectionStart, _editor->SelectionEnd, _editor->Text.size());
	const auto edit = BuildNewLineEdit(_editor->Text,
		static_cast<size_t>(span.start), static_cast<size_t>(span.end));
	_editor->InsertText(edit.Text);
	const int caret = span.start + static_cast<int>(edit.CaretOffset);
	if (caret != _editor->SelectionEnd) _editor->Select(caret, 0);
	_editor->ScrollSelectionIntoView();
}

void XamlEditorDialog::IndentSelection(bool outdent)
{
	if (!_editor) return;
	if (!outdent && !_editor->HasSelection())
	{
		_editor->InsertText(L"\t");
		_editor->ScrollSelectionIntoView();
		ShowStatus(L"已增加当前行缩进。", false);
		return;
	}

	const auto edit = BuildLineIndentEdit(
		_editor->Text,
		static_cast<size_t>((std::max)(0, _editor->SelectionStart)),
		static_cast<size_t>((std::max)(0, _editor->SelectionEnd)),
		outdent);
	if (!edit.Changed)
	{
		ShowStatus(L"当前行已没有可减少的缩进。", false);
		(void)_editor->Focus();
		return;
	}

	_editor->Select(
		static_cast<int>(edit.ReplaceStart),
		static_cast<int>(edit.ReplaceLength));
	const size_t selectionStart = (std::min)(
		edit.SelectionStart, edit.SelectionEnd);
	const size_t selectionEnd = (std::max)(
		edit.SelectionStart, edit.SelectionEnd);
	_editor->InsertTextAndSelect(
		edit.Text,
		static_cast<int>(selectionStart),
		static_cast<int>(selectionEnd - selectionStart));
	_editor->ScrollSelectionIntoView();
	(void)_editor->Focus();
	ShowStatus(outdent
		? L"已减少所选行缩进；可用 Ctrl+Z 一次撤销。"
		: L"已增加所选行缩进；可用 Ctrl+Z 一次撤销。", false);
}

void XamlEditorDialog::RefreshEditorContextMenu()
{
	if (!_editorMenu || !_editor) return;
	auto setEnabled = [&](int id, bool enabled)
		{
			if (auto* item = _editorMenu->FindItemById(id))
				item->Enable = enabled;
		};
	const bool hasSelection = _editor->HasSelection();
	setEnabled(EditorUndo, _editor->CanUndo());
	setEnabled(EditorRedo, _editor->CanRedo());
	setEnabled(EditorCut, hasSelection && !_editor->ReadOnly);
	setEnabled(EditorCopy, hasSelection);
	setEnabled(EditorPaste, _editor->CanPaste());
	setEnabled(EditorDelete, hasSelection && !_editor->ReadOnly);
	setEnabled(EditorSelectAll, !_editor->Text.empty());
	setEnabled(EditorIndent, !_editor->ReadOnly);
	setEnabled(EditorOutdent, !_editor->ReadOnly);
	setEnabled(EditorFormat, _canvas != nullptr);
}

void XamlEditorDialog::ShowEditorContextMenu(
	int x, int y, bool relativeToEditor)
{
	if (!_editorMenu || !_editor) return;
	CloseCompletionPopup();
	RefreshEditorContextMenu();
	if (relativeToEditor) _editorMenu->ShowAt(_editor, x, y);
	else _editorMenu->ShowAt(x, y);
}

void XamlEditorDialog::ShowEditorContextMenuAtCaret()
{
	if (!_editor || !_editorMenu) return;
	D2D1_RECT_F caret{};
	if (_editor->TryGetCaretViewportRect(caret))
	{
		ShowEditorContextMenu(
			static_cast<int>(std::lround(caret.left)),
			static_cast<int>(std::lround(caret.bottom)), false);
	}
	else
	{
		ShowEditorContextMenu(8, 8, true);
	}
}

void XamlEditorDialog::OnEditorMenuCommand(int commandId)
{
	if (!_editor) return;
	switch (commandId)
	{
	case EditorUndo:
		if (_editor->CanUndo()) _editor->Undo();
		break;
	case EditorRedo:
		if (_editor->CanRedo()) _editor->Redo();
		break;
	case EditorCut:
		if (!_editor->Cut()) ShowStatus(L"当前没有可剪切的文本。", true);
		break;
	case EditorCopy:
		if (!_editor->Copy()) ShowStatus(L"当前没有可复制的文本。", true);
		break;
	case EditorPaste:
		if (!_editor->Paste()) ShowStatus(L"剪贴板中没有可粘贴的文本。", true);
		break;
	case EditorDelete:
		if (_editor->HasSelection()) _editor->InsertText(L"");
		break;
	case EditorSelectAll:
		_editor->SelectAll();
		break;
	case EditorIndent:
		IndentSelection(false);
		return;
	case EditorOutdent:
		IndentSelection(true);
		return;
	case EditorFormat:
		FormatDocument();
		return;
	default:
		return;
	}
	_editor->ScrollSelectionIntoView();
	(void)_editor->Focus();
}

void XamlEditorDialog::FormatDocument()
{
	if (!ValidateAndPreview()) return;
	const auto oldCaret = static_cast<size_t>((std::max)(
		0, _editor->SelectionEnd));
	const auto oldElement = FindElementAtPosition(_editor->Text, oldCaret);
	std::wstring canonical;
	std::wstring error;
	if (!_canvas->BuildXamlDocumentText(canonical, &error))
	{
		ShowStatus(error.empty() ? L"无法格式化当前 XAML。" : error, true);
		return;
	}
	std::optional<ElementIdentitySpan> formattedElement;
	if (oldElement)
		formattedElement = FindElementByDesignIdentity(
			canonical, oldElement->StableId, oldElement->Name);
	int selectionStart = static_cast<int>((std::min)(
		oldCaret, canonical.size()));
	int selectionLength = 0;
	if (formattedElement)
	{
		selectionStart = static_cast<int>(
			formattedElement->ElementNameRange.Start);
		selectionLength = static_cast<int>(
			formattedElement->ElementNameRange.Length);
	}
	_loading = true;
	_editor->ReplaceAllTextAndSelect(
		canonical, selectionStart, selectionLength);
	_loading = false;
	_lastValidXaml = _editor->Text;
	RefreshRestorePreviewState();
	_editor->ScrollSelectionIntoView();
	UpdateCaretPosition();
	UpdateSyntaxHighlighting();
	UpdateTagMatch();
	ShowStatus(L"已格式化并保持当前元素与实时预览。", false);
}

bool XamlEditorDialog::ApplyCheckpoint()
{
	if (!_canvas || !_transactionOpen
		|| !_canvas->HasActiveDocumentTransaction())
	{
		ShowStatus(L"当前没有可应用的 XAML 文档事务。", true);
		return false;
	}
	if (!ValidateAndPreview()) return false;

	auto committed = _canvas->CommitDocumentEditTransaction();
	_transactionOpen = _canvas->HasActiveDocumentTransaction();
	if (!committed)
	{
		ShowStatus(committed.Error.empty()
			? L"无法应用当前 XAML。" : committed.Error, true);
		return false;
	}
	if (committed.HasChanges()) ++_appliedCheckpointCount;

	auto begun = _canvas->BeginDocumentEditTransaction(L"EditXaml");
	_transactionOpen = begun.Succeeded()
		&& _canvas->HasActiveDocumentTransaction();
	if (!_transactionOpen)
	{
		ShowStatus(L"当前 XAML 已应用，但无法建立继续编辑的恢复点："
			+ (begun.Error.empty() ? L"未知错误。" : begun.Error), true);
		return false;
	}

	_lastValidXaml = _editor ? _editor->Text : _lastValidXaml;
	ClearDiagnostic();
	RefreshRestorePreviewState();
	if (committed.HasChanges())
	{
		ShowStatus(L"已应用为第 "
			+ std::to_wstring(_appliedCheckpointCount)
			+ L" 个独立撤销步骤；可继续编辑，Ctrl+S 再次应用。", false);
	}
	else
	{
		ShowStatus(L"当前 XAML 已经应用，没有产生新的撤销步骤。", false);
	}
	if (_editor) (void)_editor->Focus();
	return true;
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
	if (message == WM_KEYDOWN && wParam == 'S'
		&& (::GetKeyState(VK_CONTROL) & 0x8000) != 0
		&& (::GetKeyState(VK_MENU) & 0x8000) == 0)
	{
		(void)ApplyCheckpoint();
		return true;
	}
	if (message == WM_KEYDOWN
		&& (::GetKeyState(VK_CONTROL) & 0x8000) != 0
		&& wParam == VK_SPACE)
	{
		RefreshCompletion(true);
		return true;
	}
	if (message == WM_KEYDOWN && _completionPopup
		&& _completionPopup->IsOpen())
	{
		if (wParam == VK_ESCAPE)
		{
			CloseCompletionPopup();
			return true;
		}
		if (wParam == VK_TAB || wParam == VK_RETURN
			|| wParam == VK_UP || wParam == VK_DOWN
			|| wParam == VK_PRIOR || wParam == VK_NEXT
			|| wParam == VK_HOME || wParam == VK_END)
		{
			if (wParam == VK_TAB) _suppressedCharacter = L'\t';
			else if (wParam == VK_RETURN) _suppressedCharacter = L'\r';
			_completionPopup->ProcessMessage(WM_KEYDOWN,
				wParam == VK_TAB ? VK_RETURN : wParam, lParam, 0, 0);
			return true;
		}
	}
	if (message == WM_KEYDOWN && Selected == _editor
		&& wParam == VK_TAB
		&& (::GetKeyState(VK_CONTROL) & 0x8000) == 0
		&& (::GetKeyState(VK_MENU) & 0x8000) == 0)
	{
		_suppressedCharacter = L'\t';
		IndentSelection((::GetKeyState(VK_SHIFT) & 0x8000) != 0);
		return true;
	}
	if (message == WM_KEYDOWN && Selected == _editor
		&& (wParam == VK_APPS
			|| (wParam == VK_F10
				&& (::GetKeyState(VK_SHIFT) & 0x8000) != 0)))
	{
		ShowEditorContextMenuAtCaret();
		return true;
	}
	if (message == WM_KEYDOWN && Selected == _editor
		&& wParam == 'F'
		&& (::GetKeyState(VK_CONTROL) & 0x8000) != 0
		&& (::GetKeyState(VK_SHIFT) & 0x8000) != 0)
	{
		FormatDocument();
		return true;
	}
	if (message == WM_KEYDOWN
		&& (::GetKeyState(VK_CONTROL) & 0x8000) != 0
		&& (wParam == 'F' || wParam == 'H'))
	{
		FocusFind(wParam == 'H');
		return true;
	}
	if (message == WM_KEYDOWN && wParam == VK_F3)
	{
		(void)FindNext((::GetKeyState(VK_SHIFT) & 0x8000) != 0);
		return true;
	}
	if (message == WM_KEYDOWN && wParam == VK_RETURN
		&& Selected == _findText)
	{
		_suppressedCharacter = L'\r';
		(void)FindNext((::GetKeyState(VK_SHIFT) & 0x8000) != 0);
		return true;
	}
	if (message == WM_KEYDOWN && wParam == VK_RETURN
		&& Selected == _replaceText)
	{
		_suppressedCharacter = L'\r';
		ReplaceCurrent();
		return true;
	}
	if (message == WM_KEYDOWN && wParam == VK_RETURN
		&& Selected == _editor
		&& (::GetKeyState(VK_CONTROL) & 0x8000) == 0
		&& (::GetKeyState(VK_MENU) & 0x8000) == 0)
	{
		_suppressedCharacter = L'\r';
		InsertSmartNewLine();
		return true;
	}
	if (message == WM_KEYDOWN && wParam == VK_F8
		&& _diagnosticOffset != static_cast<std::size_t>(-1))
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
