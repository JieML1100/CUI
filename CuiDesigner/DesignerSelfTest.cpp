#include "DesignerSelfTest.h"
#include "../CUI/include/Canvas.h"
#include "../CUI/include/EventInfrastructure.h"
#include "../CUI/include/StyleInfrastructure.h"
#include "ProgrammaticControlFactory.h"
#include "CodeGenerator.h"
#include "CodeBehindExportDialog.h"
#include "Designer.h"
#include "DesignerCanvas.h"
#include "DesignerControlCatalog.h"
#include "DesignerDataContextSchemaUtils.h"
#include "DataResourcesEditorDialog.h"
#include "DesignerCore/Commands/ControlPlacementCommand.h"
#include "DesignerCore/Commands/ControlStructureCommand.h"
#include "DesignerCore/Commands/DocumentSnapshotCommand.h"
#include "DesignerCore/Commands/EventHandlerCommand.h"
#include "DesignerCore/PropertyGridBinder.h"
#include "DesignerStructureEdit.h"
#include "DesignerModel/AtomicFile.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerModel/DesignCodeGenerationService.h"
#include "DesignerModel/DesignDocumentEventIndex.h"
#include "DesignerModel/DesignDocumentSerializer.h"
#include "DesignerModel/RuntimeDocument.h"
#include "DesignerModel/XamlDocumentParser.h"
#include "DesignerModel/XamlDocumentSerializer.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerPropertyRowCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include "PropertyGrid.h"
#include "SourceCodeNavigator.h"
#include "ToolBox.h"
#include "XamlEditorDialog.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/ContentControl.h"
#include "../CUI/include/ContentPresenter.h"
#include "../CUI/include/ItemsControl.h"
#include "../CUI/include/InputInfrastructure.h"
#include "../CUI/include/TemplateInfrastructure.h"
#include "../CUI/include/ItemsPresenter.h"
#include "../CUI/include/ListBox.h"
#include "../CUI/include/ScrollViewer.h"
#include "../CUI/include/GroupBox.h"
#include "../CUI/include/Expander.h"
#include "../CUI/include/Window.h"
#include "../CUI/include/ChartView.h"
#include "../CUI/include/ListView.h"
#include "../CUI/include/Menu.h"
#include "../CUI/include/Image.h"
#include "../CUI/include/PresentationInfrastructure.h"
#include "../CUI/include/WindowInfrastructure.h"
#include "../CUI/include/CuiGeneratedFrameworkTheme.h"
#include "../CUI/include/RoutedEventInfrastructure.h"
#include "../CUI/include/ProgressBar.h"
#include "../CUI/include/StatusBar.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/ToolBar.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/WebBrowser.h"
#include "../CUI/include/Layout/Grid.h"
#include "../CUI/include/Layout/StackPanel.h"
#include "../D2DGraphics/include/BitmapSource.h"
#include <Convert.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <set>
#include <limits>
#include <vector>
#include <Windows.h>

namespace
{
	POINT RoundedPoint(float x, float y) noexcept
	{
		return { static_cast<LONG>(std::lround(x)),
			static_cast<LONG>(std::lround(y)) };
	}

	InputReport PointerInput(
		InputReportKind kind,
		MouseButton changedButton,
		int x,
		int y,
		MouseButton pressedButton = MouseButton::None,
		ModifierKeys modifiers = ModifierKeys::None)
	{
		InputReport input;
		input.Kind = kind;
		input.X = x;
		input.Y = y;
		input.ChangedButton = changedButton;
		input.ButtonStates = MouseButtonStates::WithPressed(pressedButton);
		input.Modifiers = modifiers;
		input.ClickCount = kind == InputReportKind::PointerDoubleClick ? 2
			: kind == InputReportKind::PointerDown ? 1 : 0;
		return input;
	}

	InputReport KeyInput(
		InputReportKind kind,
		Key key,
		ModifierKeys modifiers = ModifierKeys::None)
	{
		InputReport input;
		input.Kind = kind;
		input.Key = key;
		input.Modifiers = modifiers;
		return input;
	}

	InputReport LifecycleInput(InputReportKind kind, int x = 0, int y = 0)
	{
		InputReport input;
		input.Kind = kind;
		input.X = x;
		input.Y = y;
		return input;
	}

	std::wstring ReadNodeString(
		const DesignerModel::DesignNode& node,
		const wchar_t* propertyName)
	{
		BindingValue value;
		std::wstring result;
		return DesignerPropertyCatalog::ReadNodeValue(
			node, propertyName, value) && value.TryGet(result)
			? result : std::wstring{};
	}

	std::shared_ptr<DesignerControl> FindControl(
		DesignerCanvas& canvas,
		const std::wstring& name)
	{
		const auto& controls = canvas.GetAllControls();
		const auto found = std::find_if(
			controls.begin(), controls.end(), [&](const auto& control)
			{
				return control && control->Name == name;
			});
		return found == controls.end() ? nullptr : *found;
	}

	std::wstring ReadControlStringProperty(
		Control* control, const wchar_t* propertyName)
	{
		if (!control || !propertyName) return {};
		BindingValue value;
		std::wstring text;
		return control->TryGetPropertyValue(propertyName, value)
			&& value.TryGet(text) ? text : std::wstring{};
	}

	bool WriteControlStringProperty(
		Control* control,
		const wchar_t* propertyName,
		std::wstring value)
	{
		return control && propertyName
			&& control->TrySetPropertyValue(
				propertyName, BindingValue(std::move(value)));
	}

	std::wstring ControlAutomationName(
		DesignerCanvas& canvas,
		const std::wstring& name)
	{
		auto control = FindControl(canvas, name);
		return control
			? ReadControlStringProperty(
				control->ControlInstance, L"AutomationProperties.Name")
			: std::wstring{};
	}

	Control* FindDescendantByAutomationName(
		Control* root,
		const std::wstring& accessibleName)
	{
		if (!root) return nullptr;
		if (root->AutomationName == accessibleName) return root;
		for (int index = 0; index < root->VisualChildCount(); ++index)
			if (auto* found = FindDescendantByAutomationName(
				root->GetVisualChild(index), accessibleName))
				return found;
		return nullptr;
	}

	void AppendFailure(
		std::vector<std::wstring>& failures,
		bool condition,
		std::wstring message)
	{
		if (!condition) failures.push_back(std::move(message));
	}

	void AppendFailure(
		std::vector<std::wstring>& failures,
		const DesignerDocumentTransactionResult& result,
		std::wstring message)
	{
		if (result.HasChanges()) return;
		if (!result.Error.empty()) message += L": " + result.Error;
		failures.push_back(std::move(message));
	}

	bool IsUnchanged(
		const DesignerDocumentTransactionResult& result)
	{
		return result.State
			== DesignerDocumentTransactionState::Unchanged;
	}

	bool ReplaceClipboardTextForSelfTest(const std::wstring& text)
	{
		const auto byteCount = (text.size() + 1) * sizeof(wchar_t);
		auto memory = ::GlobalAlloc(GMEM_MOVEABLE, byteCount);
		if (!memory) return false;
		auto* destination = static_cast<wchar_t*>(::GlobalLock(memory));
		if (!destination)
		{
			::GlobalFree(memory);
			return false;
		}
		std::copy(text.begin(), text.end(), destination);
		destination[text.size()] = L'\0';
		::GlobalUnlock(memory);
		bool clipboardOpened = false;
		for (int attempt = 0; attempt < 10 && !clipboardOpened; ++attempt)
		{
			clipboardOpened = ::OpenClipboard(nullptr) != FALSE;
			if (!clipboardOpened) ::Sleep(10);
		}
		if (!clipboardOpened)
		{
			::GlobalFree(memory);
			return false;
		}
		const bool published = ::EmptyClipboard()
			&& ::SetClipboardData(CF_UNICODETEXT, memory) != nullptr;
		::CloseClipboard();
		if (!published) ::GlobalFree(memory);
		return published;
	}

	void NormalizeRuntimeColorValues(
		DesignerModel::DesignDocument& document)
	{
		(void)document;
	}

	int NodePropertyInt(
		const DesignerModel::DesignNode* node,
		const wchar_t* propertyName,
		int fallback = 0) noexcept
	{
		const auto* assignment = node
			? node->Properties.Find(propertyName) : nullptr;
		if (!assignment) return fallback;
		try
		{
			size_t consumed = 0;
			const auto value = std::stoi(assignment->Value.Text, &consumed);
			return consumed == assignment->Value.Text.size()
				? value : fallback;
		}
		catch (...) { return fallback; }
	}

	float NodePropertyFloat(
		const DesignerModel::DesignNode* node,
		const wchar_t* propertyName,
		float fallback = 0.0f) noexcept
	{
		const auto* assignment = node
			? node->Properties.Find(propertyName) : nullptr;
		if (!assignment) return fallback;
		try
		{
			size_t consumed = 0;
			const auto value = std::stof(assignment->Value.Text, &consumed);
			return consumed == assignment->Value.Text.size()
				&& std::isfinite(value) ? value : fallback;
		}
		catch (...) { return fallback; }
	}

	Thickness NodePropertyThickness(
		const DesignerModel::DesignNode* node,
		const wchar_t* propertyName)
	{
		const auto* assignment = node
			? node->Properties.Find(propertyName) : nullptr;
		if (!assignment) return {};
		BindingValue converted;
		Thickness result{};
		return DesignerStyleSheetUtils::TryConvertValue(
			assignment->Value, converted) && converted.TryGet(result)
			? result : Thickness{};
	}

	bool EquivalentDocumentContent(
		DesignerModel::DesignDocument left,
		DesignerModel::DesignDocument right)
	{
		// nextId is an allocation high-water mark, not visual/document content.
		left.NextStableId = 1;
		right.NextStableId = 1;
		// Runtime colors are float-valued. Text persistence is read as double, so
		// compare the values after the same float conversion used by controls.
		NormalizeRuntimeColorValues(left);
		NormalizeRuntimeColorValues(right);
		return left == right;
	}

	bool EquivalentXamlContent(
		const DesignerModel::DesignDocument& left,
		const DesignerModel::DesignDocument& right)
	{
		try
		{
			return DesignerModel::XamlDocumentSerializer::ToXaml(left)
				== DesignerModel::XamlDocumentSerializer::ToXaml(right);
		}
		catch (...)
		{
			return false;
		}
	}

	std::wstring DescribeXamlDifference(
		const DesignerModel::DesignDocument& left,
		const DesignerModel::DesignDocument& right)
	{
		try
		{
			const auto a = DesignerModel::XamlDocumentSerializer::ToXaml(left);
			const auto b = DesignerModel::XamlDocumentSerializer::ToXaml(right);
			const auto count = (std::min)(a.size(), b.size());
			size_t offset = 0;
			while (offset < count && a[offset] == b[offset]) ++offset;
			if (offset == a.size() && offset == b.size()) return L"equal";
			const auto start = offset > 40 ? offset - 40 : 0;
			return L"offset=" + std::to_wstring(offset)
				+ L", left=" + Convert::Utf8ToUnicode(
					a.substr(start, (std::min)(size_t{ 100 }, a.size() - start)))
				+ L", right=" + Convert::Utf8ToUnicode(
					b.substr(start, (std::min)(size_t{ 100 }, b.size() - start)));
		}
		catch (...)
		{
			return L"serialization failed";
		}
	}

	const wchar_t* SelfTestFlag(bool value) noexcept
	{
		return value ? L"1" : L"0";
	}

	std::wstring DescribeDocumentDifference(
		const DesignerModel::DesignDocument& left,
		const DesignerModel::DesignDocument& right)
	{
		std::wstring result;
		auto add = [&](const std::wstring& value)
		{
			if (!result.empty()) result += L",";
			result += value;
		};
		if (left.Schema != right.Schema) add(L"schema");
		if (left.SchemaVersion != right.SchemaVersion) add(L"version");
		if (left.Window != right.Window) add(L"form");
		if (left.CodeBehind != right.CodeBehind) add(L"codeBehind");
		if (left.DataContextSchema != right.DataContextSchema) add(L"dataContext");
		if (left.StyleSheet != right.StyleSheet) add(L"styleSheet");
		if (left.Components != right.Components) add(L"components");
		if (left.Nodes.size() != right.Nodes.size()) add(L"nodeCount");
		const auto count = (std::min)(left.Nodes.size(), right.Nodes.size());
		for (size_t index = 0; index < count; ++index)
		{
			const auto& a = left.Nodes[index];
			const auto& b = right.Nodes[index];
			if (a == b) continue;
			std::wstring fields;
			auto field = [&](const wchar_t* value)
			{
				if (!fields.empty()) fields += L"+";
				fields += value;
			};
			if (a.Id != b.Id) field(L"id");
			if (a.ParentId != b.ParentId) field(L"parentId");
			if (a.ParentRef != b.ParentRef) field(L"parentRef");
			if (a.Name != b.Name) field(L"name");
			if (a.Type != b.Type) field(L"type");
			if (a.ComponentType != b.ComponentType) field(L"componentType");
			if (a.Order != b.Order) field(L"order");
			if (a.Properties != b.Properties)
			{
				std::wstring keys;
				std::set<std::wstring> names;
				for (const auto& [name, ignored] : a.Properties.Values)
				{
					(void)ignored;
					names.insert(name);
				}
				for (const auto& [name, ignored] : b.Properties.Values)
				{
					(void)ignored;
					names.insert(name);
				}
				for (const auto& name : names)
				{
					const auto* aValue = a.Properties.Find(name);
					const auto* bValue = b.Properties.Find(name);
					if (aValue && bValue && *aValue == *bValue) continue;
					if (!keys.empty()) keys += L"|";
					keys += name;
				}
				field((L"properties{" + keys + L"}").c_str());
			}
			if (a.Structure != b.Structure) field(L"structure");
			if (a.TemplateState != b.TemplateState) field(L"templateState");
			if (a.Events != b.Events) field(L"events");
			if (a.Bindings != b.Bindings) field(L"bindings");
			add(L"node" + std::to_wstring(index) + L":" + fields);
		}
		return result.empty() ? L"nextId" : result;
	}

	std::wstring CreateTemporarySelfTestFile()
	{
		wchar_t directory[MAX_PATH]{};
		wchar_t path[MAX_PATH]{};
		if (::GetTempPathW(MAX_PATH, directory) == 0
			|| ::GetTempFileNameW(
				directory, L"cui", 0, path) == 0)
			return {};
		return path;
	}

	bool HasAtomicSaveTemporaryFile(const std::wstring& filePath)
	{
		WIN32_FIND_DATAW data{};
		const auto pattern = filePath + L".~cui-*.tmp";
		const HANDLE found = ::FindFirstFileW(pattern.c_str(), &data);
		if (found == INVALID_HANDLE_VALUE) return false;
		(void)::FindClose(found);
		return true;
	}

	struct TemporarySelfTestFiles
	{
		~TemporarySelfTestFiles()
		{
			for (const auto& path : Paths)
				if (!path.empty()) (void)::DeleteFileW(path.c_str());
		}

		std::vector<std::wstring> Paths;
	};

	void ReloadCurrentSelection(
		PropertyGrid& propertyGrid,
		DesignerCanvas& canvas)
	{
		propertyGrid.LoadControls(
			canvas.GetSelectedControls(), canvas.GetSelectedControl());
	}
}

bool RunDesignerSelfTest(std::wstring& report)
{
	std::vector<std::wstring> failures;
	ToolBox toolBox(0, 0, 260, 640);
	const auto toolboxDescriptors =
		DesignerControlCatalog::BuiltInDescriptors();
	std::unique_ptr<ToolBoxItem> toolboxPresentationProbe;
	if (!toolboxDescriptors.empty())
		toolboxPresentationProbe = std::make_unique<ToolBoxItem>(
			toolboxDescriptors.front(), 0, 0, 220, 40);
	AppendFailure(failures,
		toolBox.GetItemCount()
			== toolboxDescriptors.size()
			&& toolBox.GetVisibleItemCount() == toolBox.GetItemCount()
			&& toolBox.GetVisibleCategoryCount() == 7,
		L"toolbox: controls were not grouped into the expected native categories");
	AppendFailure(failures,
		toolboxPresentationProbe
			&& toolboxPresentationProbe->GetContent().Empty()
			&& !toolboxPresentationProbe->GetTemplate(),
		L"toolbox: custom card retained a Button content/template projection and would render its name twice");
	toolBox.SetFilterText(L"媒体");
	AppendFailure(failures,
		toolBox.GetVisibleItemCount() == 2
		&& toolBox.GetVisibleCategoryCount() == 1,
		L"toolbox: category-aware filtering did not isolate media controls");
	toolBox.SetFilterText(L"Button");
	AppendFailure(failures,
		toolBox.GetVisibleItemCount() == 2
		&& toolBox.GetVisibleCategoryCount() == 1,
		L"toolbox: type-name filtering did not isolate Button and RadioButton");
	toolBox.SetFilterText(L"不存在的控件");
	AppendFailure(failures,
		toolBox.GetVisibleItemCount() == 0
			&& toolBox.GetVisibleCategoryCount() == 0,
		L"toolbox: empty filter results retained stale visible rows");
	{
		DesignerModel::DesignDocument dataDocument;
		DesignerModel::DesignDataTypeDefinition type;
		type.Name = L"Person";
		type.Properties = {
			{ L"Name", BindingValueKind::String, true, true, true }
		};
		dataDocument.DataTypes.push_back(type);
		DesignerModel::DesignDataList list;
		list.Key = L"People";
		list.ItemType = L"Person";
		dataDocument.DataLists.push_back(list);
		DataResourcesEditorDialog editor(dataDocument);
		AppendFailure(failures,
			!editor.Applied && editor.ResultDocument == dataDocument
				&& editor.Width.IsFixed() && editor.Width.value == 940.0f
				&& editor.Height.IsFixed() && editor.Height.value == 760.0f,
			L"data resources editor: construction changed the document or layout contract");
	}
	{
		const std::wstring source =
			L"<Window xmlns=\"urn:cui\" Name=\"MainWindow\">"
			L"<Button Name=\"existingButton\" Content=\"Existing\"/>"
			L"</Window>";
		XamlEditorDialog recoveryEditor(nullptr, source);
		const std::wstring invalidDraft =
			L"<Window><Button Visibility=\"Vanished\"/></Window>";
		recoveryEditor._editor->SelectAll();
		recoveryEditor._editor->InsertText(invalidDraft);
		recoveryEditor.RefreshRestorePreviewState();
		const bool recoveryEnabled = recoveryEditor._restorePreview
			&& recoveryEditor._restorePreview->IsEnabled;
		recoveryEditor.RestoreLastValidPreview();
		const bool recoveryRestored = recoveryEditor._editor->Text == source
			&& recoveryEditor._restorePreview
			&& !recoveryEditor._restorePreview->IsEnabled
			&& recoveryEditor._editor->CanUndo();
		recoveryEditor._editor->Undo();
		const bool recoveryUndo = recoveryEditor._editor->Text == invalidDraft
			&& recoveryEditor._restorePreview->IsEnabled;

		DesignerCanvas previewCanvas(0, 0, 900, 640);
		std::wstring initial;
		std::wstring previewError;
		const bool sourceBuilt =
			previewCanvas.BuildXamlDocumentText(initial, &previewError);
		const auto transaction =
			previewCanvas.BeginDocumentEditTransaction(L"EditXaml");
		XamlEditorDialog previewEditor(&previewCanvas, initial);
		auto valid = initial;
		const auto contentEnd = valid.rfind(L"</Canvas>");
		if (contentEnd != std::wstring::npos)
			valid.insert(contentEnd,
				L"  <Button Name=\"liveButton\" Content=\"Live\" />\r\n");
		else
		{
			const auto canvasStart = valid.rfind(L"<Canvas");
			const auto close = canvasStart == std::wstring::npos
				? std::wstring::npos : valid.find(L"/>", canvasStart);
			if (close != std::wstring::npos)
				valid.replace(close, 2,
					L">\r\n  <Button Name=\"liveButton\" Content=\"Live\" />"
					L"\r\n</Canvas>");
		}
		previewEditor._editor->ReplaceAllTextAndSelect(
			valid, static_cast<int>(valid.size()), 0);
		const bool validSynchronized = previewEditor.ValidateAndPreview()
			&& previewEditor._lastValidXaml == valid
			&& FindControl(previewCanvas, L"liveButton");
		const auto validPreviewStatus = previewEditor._status
			? previewEditor._status->Text : std::wstring{};
		const auto validControlCount = previewCanvas.GetAllControls().size();
		auto invalid = valid;
		const auto visibility = invalid.find(L"Content=\"Live\"");
		if (visibility != std::wstring::npos)
			invalid.replace(
				visibility, std::wstring(L"Content=\"Live\"").size(),
				L"Visibility=\"Vanished\"");
		previewEditor._editor->ReplaceAllTextAndSelect(
			invalid, static_cast<int>(invalid.size()), 0);
		const bool invalidRejected = !previewEditor.ValidateAndPreview()
			&& previewEditor._diagnosticOffset
				!= DesignerModel::XamlDocumentDiagnostic::UnknownOffset
			&& previewCanvas.GetAllControls().size() == validControlCount
			&& FindControl(previewCanvas, L"liveButton");
		previewEditor.RestoreLastValidPreview();
		const bool validRecovered = previewEditor._editor->Text == valid
			&& previewEditor._diagnosticOffset
				== DesignerModel::XamlDocumentDiagnostic::UnknownOffset;
		const auto rollback = previewCanvas.RollbackDocumentEditTransaction();
		const bool rollbackRestored = rollback.Succeeded()
			&& !FindControl(previewCanvas, L"liveButton");

		AppendFailure(failures,
			recoveryEditor._editor && recoveryEnabled
				&& recoveryRestored && recoveryUndo
				&& sourceBuilt && transaction.Succeeded()
			&& validSynchronized && invalidRejected
				&& validRecovered && rollbackRestored
				&& initial.find(L"<Panel") == std::wstring::npos,
			std::wstring(L"XAML editor thin shell: recovery, validation, synchronization, or rollback failed")
				+ L" [recoveryEnabled=" + std::to_wstring(recoveryEnabled)
				+ L", recoveryRestored=" + std::to_wstring(recoveryRestored)
				+ L", recoveryUndo=" + std::to_wstring(recoveryUndo)
				+ L", sourceBuilt=" + std::to_wstring(sourceBuilt)
				+ L", canvasTag=" + std::to_wstring(
					initial.find(L"<Canvas") != std::wstring::npos)
				+ L", forbiddenPanelTag=" + std::to_wstring(
					initial.find(L"<Panel") != std::wstring::npos)
				+ L", transaction=" + std::to_wstring(transaction.Succeeded())
				+ L", synchronized=" + std::to_wstring(validSynchronized)
				+ L", rejected=" + std::to_wstring(invalidRejected)
				+ L", recovered=" + std::to_wstring(validRecovered)
				+ L", rollback=" + std::to_wstring(rollbackRestored)
				+ L", validStatus=" + validPreviewStatus
				+ L", status=" + (previewEditor._status
					? previewEditor._status->Text : std::wstring{}) + L"]"
				+ (previewError.empty()
					? std::wstring{} : L": " + previewError));
	}

	// The XAML editor is modal, so its Designer owner is disabled while live
	// previews invalidate the canvas. Disabled owners must still consume paint
	// requests; otherwise BeginPaint validates the region and the applied XAML
	// remains visually stale after the editor closes.
	bool disabledOwnerRepainted = false;
	{
	Window previewOwner;
	previewOwner.Title = L"Designer live XAML repaint probe";
		previewOwner.Left = -30000.0f;
		previewOwner.Top = -30000.0f;
		previewOwner.Width = 220.0f;
		previewOwner.Height = 140.0f;
		previewOwner.WindowStyle = ::WindowStyle::None;
		auto* previewSurface = previewOwner.AdoptVisualChild(
			cui::designer::NewControl<Panel>(0, 0, 200, 120));
		previewOwner.Show();
		(void)::UpdateWindow(previewOwner.Handle);
		const auto committedBefore = cui::framework::WindowAccess::
			PresentationCommittedFrameCount(previewOwner);
		(void)::EnableWindow(previewOwner.Handle, FALSE);
		previewSurface->Background = Colors::DodgerBlue;
		previewSurface->InvalidateVisual();
		(void)::UpdateWindow(previewOwner.Handle);
		const auto committedWhileDisabled =
			cui::framework::WindowAccess::PresentationCommittedFrameCount(
				previewOwner);
		(void)::EnableWindow(previewOwner.Handle, TRUE);
		(void)::UpdateWindow(previewOwner.Handle);
		const auto committedAfterEnable =
			cui::framework::WindowAccess::PresentationCommittedFrameCount(
				previewOwner);
		disabledOwnerRepainted =
			committedWhileDisabled == committedBefore
			&& committedAfterEnable > committedBefore;
		(void)::DestroyWindow(previewOwner.Handle);
	}
	AppendFailure(failures, disabledOwnerRepainted,
		L"live XAML: a disabled modal owner did not retain and resume the canvas repaint");

	auto catalogDescriptors = DesignerControlCatalog::BuiltInDescriptors();
	auto findBuiltInDescriptor = [&](UIClass type)
		-> const DesignerControlDescriptor*
	{
		const auto found = std::find_if(
			catalogDescriptors.begin(), catalogDescriptors.end(),
			[type](const DesignerControlDescriptor& descriptor)
			{
				return descriptor.Type == type;
			});
		return found == catalogDescriptors.end() ? nullptr : &*found;
	};
	const auto* buttonDropDescriptor = findBuiltInDescriptor(UIClass::UI_Button);
	const auto* panelDropDescriptor = findBuiltInDescriptor(UIClass::UI_Canvas);
	AppendFailure(failures,
		buttonDropDescriptor && panelDropDescriptor,
		L"toolbox drag: required built-in descriptors were not available");
	if (buttonDropDescriptor && panelDropDescriptor)
	{
		DesignerCanvas dropPreviewCanvas(0, 0, 900, 640);
		std::wstring dropTarget;
		const bool rootPreview = dropPreviewCanvas.UpdateControlDropPreview(
			*buttonDropDescriptor, POINT{ 100, 100 }, &dropTarget);
		const auto rootGhost = dropPreviewCanvas.GetControlDropPreviewRect();
		AppendFailure(failures,
			rootPreview && dropPreviewCanvas.HasControlDropPreview()
			&& dropTarget == L"contentRoot"
			&& rootGhost.right - rootGhost.left
				== buttonDropDescriptor->DefaultSize.width
			&& rootGhost.bottom - rootGhost.top
				== buttonDropDescriptor->DefaultSize.height,
			L"toolbox drag: root preview did not expose the default-size ghost");
		dropPreviewCanvas.ClearControlDropPreview();
		AppendFailure(failures,
			!dropPreviewCanvas.HasControlDropPreview(),
			L"toolbox drag: clearing a preview retained stale view state");

		const auto panelAdd = dropPreviewCanvas.AdoptVisualChildToCanvas(
			*panelDropDescriptor, POINT{ 300, 240 });
		auto panelWrapper = dropPreviewCanvas.GetSelectedControl();
		const POINT panelPoint = panelWrapper && panelWrapper->ControlInstance
			? RoundedPoint(
				panelWrapper->ControlInstance->GetAbsoluteLocationDip().x
					- dropPreviewCanvas.GetAbsoluteLocationDip().x + 80,
				panelWrapper->ControlInstance->GetAbsoluteLocationDip().y
					- dropPreviewCanvas.GetAbsoluteLocationDip().y + 80)
			: POINT{ 0, 0 };
		const bool panelPreview = panelWrapper
			&& dropPreviewCanvas.UpdateControlDropPreview(
				*buttonDropDescriptor, panelPoint, &dropTarget);
		const auto buttonAdd = panelPreview
			? dropPreviewCanvas.AdoptVisualChildToCanvas(
				*buttonDropDescriptor, panelPoint)
			: DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"panel preview unavailable");
		auto nestedButton = dropPreviewCanvas.GetSelectedControl();
		AppendFailure(failures,
			panelAdd.HasChanges() && panelPreview
			&& dropTarget == panelWrapper->Name
			&& buttonAdd.HasChanges() && nestedButton
			&& nestedButton->DesignerParent == panelWrapper->ControlInstance
			&& dropPreviewCanvas.UndoCommand().HasChanges()
			&& dropPreviewCanvas.GetAllControls().size() == 2,
			L"toolbox drag: container preview, placement, or single-step undo failed");

		Designer dragDesigner;
		dragDesigner.InitializeComponents();
		auto* toolboxCard = dynamic_cast<Button*>(
			FindDescendantByAutomationName(
				dragDesigner._toolBox, L"文本块"));
		const bool designerChromeStructure = toolboxCard
			&& toolboxCard->GetContent().Empty()
			&& !toolboxCard->GetTemplate()
			&& dragDesigner._canvas && dragDesigner._canvas->ClipToBounds;
		AppendFailure(failures, designerChromeStructure,
			L"designer chrome: toolbox card projection or canvas viewport clipping was not installed");
		const auto canvasOrigin = dragDesigner._canvas->GetAbsoluteLocationDip();
		const auto canvasViewPoint = dragDesigner._canvas->CanvasToViewPoint(
			POINT{ 140, 140 });
		const POINT formDropPoint{
			static_cast<LONG>(std::lround(canvasOrigin.x)) + canvasViewPoint.x,
			static_cast<LONG>(std::lround(canvasOrigin.y)) + canvasViewPoint.y };
		dragDesigner.BeginToolBoxDrag(
			*buttonDropDescriptor, POINT{ 30, 160 });
		dragDesigner.UpdateToolBoxDrag(31, 161);
		const bool thresholdPreserved = dragDesigner._toolBoxPointerDown
			&& !dragDesigner._toolBoxDragging
			&& !dragDesigner._canvas->HasControlDropPreview()
			&& dragDesigner._canvas->GetAllControls().size() == 1;
		dragDesigner.UpdateToolBoxDrag(formDropPoint.x, formDropPoint.y);
		const bool activePreview = dragDesigner._toolBoxDragging
			&& dragDesigner._toolBoxDropAccepted
			&& dragDesigner._canvas->HasControlDropPreview();
		dragDesigner.EndToolBoxDrag(formDropPoint.x, formDropPoint.y);
		auto draggedControl = dragDesigner._canvas->GetSelectedControl();
		const bool dropCommitted = !dragDesigner._toolBoxPointerDown
			&& !dragDesigner._canvas->HasControlDropPreview()
			&& dragDesigner._canvas->GetAllControls().size() == 2
			&& draggedControl && draggedControl->Type == UIClass::UI_Button
			&& dragDesigner._canvas->GetUndoCommandLabel() == L"AdoptVisualChild";
		const bool dropUndone = dragDesigner._canvas->UndoCommand().HasChanges()
			&& dragDesigner._canvas->GetAllControls().size() == 1;
		AppendFailure(failures,
			thresholdPreserved && activePreview && dropCommitted && dropUndone,
			L"toolbox drag: Designer threshold, captured drop, or undo lifecycle failed");

		DesignerCanvas tabOrderModeCanvas(0, 0, 900, 640);
		const auto tabModeAdd = tabOrderModeCanvas.AdoptVisualChildToCanvas(
			*buttonDropDescriptor, POINT{ 180, 160 });
		auto tabModeButton = tabOrderModeCanvas.GetSelectedControl();
		const auto tabModeReset = tabOrderModeCanvas.ResetDocumentHistoryAsSaved();
		const bool tabModeEntered = tabOrderModeCanvas.SetTabOrderMode(true)
			&& tabOrderModeCanvas.IsTabOrderMode()
			&& tabOrderModeCanvas.GetNextTabOrderIndex() == 0
			&& tabOrderModeCanvas.GetTabOrderCandidateCount() == 1
			&& !tabOrderModeCanvas.IsDocumentDirty()
			&& tabOrderModeCanvas.GetUndoCommandCount() == 0;
		POINT tabModePoint{ 0, 0 };
		if (tabModeButton && tabModeButton->ControlInstance)
		{
			const auto size = tabModeButton->ControlInstance->GetActualSizeDip();
			tabModePoint = RoundedPoint(
				tabModeButton->ControlInstance->GetAbsoluteLocationDip().x
					- tabOrderModeCanvas.GetAbsoluteLocationDip().x
					+ (std::max)(1.0f, size.width * 0.5f),
				tabModeButton->ControlInstance->GetAbsoluteLocationDip().y
					- tabOrderModeCanvas.GetAbsoluteLocationDip().y
					+ (std::max)(1.0f, size.height * 0.5f));
			(void)cui::framework::InputAccess::DispatchInput(tabOrderModeCanvas, PointerInput(
				InputReportKind::PointerDown, MouseButton::Left,
				tabModePoint.x, tabModePoint.y, MouseButton::Left));
		}
		const bool tabModeAssigned = tabModeButton
			&& tabModeButton->ControlInstance
			&& tabModeButton->ControlInstance->TabIndex == 0
			&& tabModeButton->MetadataProperties.contains(L"TabIndex")
			&& tabOrderModeCanvas.GetNextTabOrderIndex() == 1
			&& tabOrderModeCanvas.GetUndoCommandCount() == 1
			&& tabOrderModeCanvas.GetUndoCommandLabel() == L"SetTabOrder";
		const auto tabModeUndo = tabOrderModeCanvas.UndoCommand();
		const bool tabModeUndone = tabModeUndo.HasChanges()
			&& tabModeButton
			&& !tabModeButton->MetadataProperties.contains(L"TabIndex");
		(void)cui::framework::InputAccess::DispatchInput(tabOrderModeCanvas, KeyInput(
			InputReportKind::KeyDown, Key::Escape));
		AppendFailure(failures,
			tabModeAdd.HasChanges() && tabModeReset
			&& tabModeEntered && tabModeAssigned && tabModeUndone
			&& !tabOrderModeCanvas.IsTabOrderMode()
			&& !tabOrderModeCanvas.IsDocumentDirty(),
			L"tab order: view-only mode, click assignment, Escape, or delta Undo failed");

		DesignerCanvas autoTabCanvas(0, 0, 900, 640);
		(void)autoTabCanvas.AdoptVisualChildToCanvas(
			*buttonDropDescriptor, POINT{ 330, 310 });
		auto lowerButton = autoTabCanvas.GetSelectedControl();
		(void)autoTabCanvas.AdoptVisualChildToCanvas(
			*buttonDropDescriptor, POINT{ 140, 130 });
		auto upperButton = autoTabCanvas.GetSelectedControl();
		(void)autoTabCanvas.AdoptVisualChildToCanvas(
			*buttonDropDescriptor, POINT{ 440, 130 });
		auto excludedButton = autoTabCanvas.GetSelectedControl();
		const auto lowerSeed = autoTabCanvas.AssignTabOrderIndex(lowerButton, 8);
		const auto upperSeed = autoTabCanvas.AssignTabOrderIndex(upperButton, 9);
		if (excludedButton && excludedButton->ControlInstance)
			excludedButton->ControlInstance->IsTabStop = false;
		const auto autoReset = autoTabCanvas.ResetDocumentHistoryAsSaved();
		const auto autoResult = autoTabCanvas.AutoArrangeTabOrder();
		const bool autoApplied = lowerSeed.HasChanges() && upperSeed.HasChanges()
			&& autoReset && autoResult.HasChanges()
			&& upperButton && lowerButton && excludedButton
			&& upperButton->ControlInstance->TabIndex == 0
			&& lowerButton->ControlInstance->TabIndex == 1
			&& excludedButton->ControlInstance->TabIndex == 0
			&& autoTabCanvas.GetTabOrderCandidateCount() == 2
			&& autoTabCanvas.GetUndoCommandCount() == 1
			&& autoTabCanvas.GetUndoCommandLabel() == L"AutoTabOrder";
		const auto autoUndo = autoTabCanvas.UndoCommand();
		const bool autoUndone = autoUndo.HasChanges()
			&& lowerButton->ControlInstance->TabIndex == 8
			&& upperButton->ControlInstance->TabIndex == 9;
		const auto autoRedo = autoTabCanvas.RedoCommand();
		DesignerModel::DesignDocument autoTabDocument;
		std::wstring autoTabXaml;
		std::wstring autoTabError;
		const bool autoPersisted = autoRedo.HasChanges()
			&& autoTabCanvas.BuildDesignDocument(
				autoTabDocument, &autoTabError)
			&& autoTabCanvas.BuildXamlDocumentText(
				autoTabXaml, &autoTabError)
			&& lowerButton->MetadataProperties.contains(L"TabIndex")
			&& upperButton->MetadataProperties.contains(L"TabIndex")
			&& autoTabXaml.find(L"TabIndex=\"0\"") != std::wstring::npos
			&& autoTabXaml.find(L"TabIndex=\"1\"") != std::wstring::npos;
		AppendFailure(failures,
			autoApplied && autoUndone && autoPersisted,
			L"tab order: focusable filtering, visual auto-sort, batch Undo/Redo, or persistence failed");
	}
	DesignerCanvas structureCanvas(0, 0, 900, 640);
	std::wstring structureError;
	structureCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Grid, POINT{ 520, 80 });
	auto gridPanelControl = structureCanvas.GetAllControls().back();
	auto* structurePanel = dynamic_cast<Grid*>(
		gridPanelControl->ControlInstance);
	structurePanel->ClearRows();
	structurePanel->ClearColumns();
	structurePanel->AddRow(GridLength::Pixels(40.0f), 5.0f, 200.0f);
	structurePanel->AddColumn(GridLength::Star(2.0f), 10.0f, 300.0f);
	DesignerStructureSnapshot definitionsBefore;
	DesignerStructureSnapshot definitionsCurrent;
	AppendFailure(failures,
		DesignerStructureEdit::Capture(
			*gridPanelControl,
			DesignerCustomEditorKind::GridDefinitions,
			definitionsBefore, &structureError),
		L"structure delta: failed to capture Grid definitions");
	structurePanel->ClearRows();
	structurePanel->ClearColumns();
	structurePanel->AddRow(GridLength::Auto());
	structurePanel->AddColumn(GridLength::Pixels(50.0f));
	AppendFailure(failures,
		DesignerStructureEdit::Restore(
			*gridPanelControl, definitionsBefore, &structureError)
		&& DesignerStructureEdit::Capture(
			*gridPanelControl,
			DesignerCustomEditorKind::GridDefinitions,
			definitionsCurrent, &structureError)
		&& definitionsCurrent == definitionsBefore,
		L"structure delta: Grid definitions did not restore exactly");

	DesignerCanvas canvas(0, 0, 1000, 760);
	canvas.AdoptVisualChildToCanvasCore(UIClass::UI_Button, POINT{ 100, 100 });
	canvas.AdoptVisualChildToCanvasCore(UIClass::UI_Button, POINT{ 280, 100 });

	AppendFailure(failures, canvas.GetAllControls().size() == 3,
		L"setup: expected two controls");
	if (canvas.GetAllControls().size() != 3)
	{
		report = failures.front();
		return false;
	}

	const auto firstName = canvas.GetAllControls()[1]->Name;
	const auto secondName = canvas.GetAllControls()[2]->Name;
	(void)WriteControlStringProperty(
		canvas.GetAllControls()[2]->ControlInstance,
		L"AutomationProperties.Name", L"不同名称");
	canvas.RestoreSelectionByNames(
		{ firstName, secondName }, firstName, false);

	PropertyGrid propertyGrid(0, 0, 360, 700);
	propertyGrid.SetDesignerCanvas(&canvas);
	ReloadCurrentSelection(propertyGrid, canvas);

	const auto* mixedAutomationName = DesignerPropertyRowCatalog::Find(
		propertyGrid.GetPresentedPropertyRows(), L"AutomationProperties.Name");
	AppendFailure(failures, mixedAutomationName != nullptr,
		L"mixed selection: AutomationProperties.Name row missing");
	AppendFailure(failures,
		mixedAutomationName && mixedAutomationName->HasMixedValue,
		L"mixed selection: AutomationProperties.Name did not report mixed values");

	const auto batchEdit = propertyGrid.ApplyPropertyValue(
		L"AutomationProperties.Name", L"批量名称");
	AppendFailure(failures, batchEdit.Succeeded && batchEdit.AppliedCount == 2,
		L"batch edit: expected both targets to update");
	AppendFailure(failures,
		ControlAutomationName(canvas, firstName) == L"批量名称"
			&& ControlAutomationName(canvas, secondName) == L"批量名称",
		L"batch edit: runtime values differ");
	AppendFailure(failures, !propertyGrid.HasPropertyEditError(),
		L"batch edit: stale error remained visible");

	AppendFailure(failures, canvas.UndoCommand(),
		L"undo: command was not available");
	AppendFailure(failures,
		ControlAutomationName(canvas, firstName) != L"批量名称"
			&& ControlAutomationName(canvas, secondName) == L"不同名称",
		L"undo: pre-edit values were not restored");
	AppendFailure(failures, canvas.GetSelectedControls().size() == 2
		&& canvas.GetSelectedControl()
		&& canvas.GetSelectedControl()->Name == firstName,
		L"undo: complete selection was not restored");

	AppendFailure(failures, canvas.RedoCommand(),
		L"redo: command was not available");
	AppendFailure(failures,
		ControlAutomationName(canvas, firstName) == L"批量名称"
			&& ControlAutomationName(canvas, secondName) == L"批量名称",
		L"redo: edited values were not restored");
	AppendFailure(failures, canvas.GetSelectedControls().size() == 2,
		L"redo: complete selection was not restored");

	ReloadCurrentSelection(propertyGrid, canvas);
	const auto invalidEdit = propertyGrid.ApplyPropertyValue(
		L"FontSize", L"not-a-number");
	AppendFailure(failures, !invalidEdit.Succeeded,
		L"error state: invalid numeric text was accepted");
	AppendFailure(failures, propertyGrid.HasPropertyEditError()
		&& propertyGrid.GetPropertyEditErrorProperty() == L"FontSize"
		&& !propertyGrid.GetPropertyEditErrorMessage().empty(),
		L"error state: failure was not exposed by PropertyGrid");
	AppendFailure(failures,
		ControlAutomationName(canvas, firstName) == L"批量名称"
			&& ControlAutomationName(canvas, secondName) == L"批量名称",
		L"error state: rejected edit mutated unrelated values");
	AppendFailure(failures, canvas.UndoCommand(),
		L"error state: prior valid command was no longer undoable");
	AppendFailure(failures,
		ControlAutomationName(canvas, firstName) != L"批量名称"
			&& ControlAutomationName(canvas, secondName) == L"不同名称",
		L"error state: rejected edit entered the undo history");
	AppendFailure(failures, canvas.RedoCommand(),
		L"error state: prior valid command was no longer redoable");
	AppendFailure(failures,
		ControlAutomationName(canvas, firstName) == L"批量名称"
			&& ControlAutomationName(canvas, secondName) == L"批量名称",
		L"error state: redo did not restore the prior valid edit");

	ReloadCurrentSelection(propertyGrid, canvas);
	const auto reset = propertyGrid.ResetPropertyValue(
		L"AutomationProperties.Name");
	AppendFailure(failures, reset.Succeeded && reset.AppliedCount == 2,
		L"reset: expected both targets to reset");
	AppendFailure(failures,
		ControlAutomationName(canvas, firstName).empty()
			&& ControlAutomationName(canvas, secondName).empty(),
		L"reset: default values were not applied (first='"
			+ ControlAutomationName(canvas, firstName) + L"', second='"
			+ ControlAutomationName(canvas, secondName) + L"')");
	AppendFailure(failures, !propertyGrid.HasPropertyEditError(),
		L"reset: successful edit did not clear the error state");

	AppendFailure(failures, canvas.UndoCommand(),
		L"reset undo: command was not available");
	AppendFailure(failures,
		ControlAutomationName(canvas, firstName) == L"批量名称"
			&& ControlAutomationName(canvas, secondName) == L"批量名称",
		L"reset undo: edited values were not restored");
	AppendFailure(failures, canvas.RedoCommand(),
		L"reset redo: command was not available");
	AppendFailure(failures,
		ControlAutomationName(canvas, firstName).empty()
			&& ControlAutomationName(canvas, secondName).empty(),
		L"reset redo: defaults were not restored (first='"
			+ ControlAutomationName(canvas, firstName) + L"', second='"
			+ ControlAutomationName(canvas, secondName) + L"')");

	// A design-time Binding is the Local expression itself. Removing it reveals
	// metadata/style state; it must not resurrect the Local value it replaced.
	auto boundControl = FindControl(canvas, firstName);
	AppendFailure(failures, boundControl && boundControl->ControlInstance,
		L"binding preview: target control missing");
	if (boundControl && boundControl->ControlInstance)
	{
		(void)WriteControlStringProperty(
			boundControl->ControlInstance,
			L"AutomationProperties.Name", L"本地后备值");
		boundControl->DataBindings[L"AutomationProperties.Name"] = {
			L"Caption",
			BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged,
			L"StringTrim"
		};
		auto dataContext = std::make_shared<ObservableObject>();
		dataContext->SetValue(L"Caption", std::wstring(L"  绑定预览  "));
		canvas.SetDesignDataContext(dataContext);
		AppendFailure(failures,
			ReadControlStringProperty(
				boundControl->ControlInstance,
				L"AutomationProperties.Name") == L"绑定预览",
			L"binding preview: source value did not become effective");
		AppendFailure(failures,
			boundControl->ControlInstance->GetPropertyValueSource(
				L"AutomationProperties.Name")
				== DependencyPropertyValueSource::Local
			&& boundControl->ControlInstance->GetPropertyExpressionKind(
				L"AutomationProperties.Name")
				== DependencyPropertyExpressionKind::Binding,
			L"binding preview: Binding did not own the effective value");
		const auto rows = DesignerPropertyRowCatalog::GetControlRows(
			*boundControl, DesignerControlPropertyContext{});
		const auto* automationNameRow = DesignerPropertyRowCatalog::Find(
			rows, L"AutomationProperties.Name");
		AppendFailure(failures,
			automationNameRow && automationNameRow->HasConfiguredBinding
				&& automationNameRow->IsReadOnly
				&& !automationNameRow->Diagnostics.empty(),
			L"binding preview: PropertyGrid row did not expose diagnostics");

		canvas.SetDesignDataContext(nullptr);
		AppendFailure(failures,
			ReadControlStringProperty(
				boundControl->ControlInstance,
				L"AutomationProperties.Name").empty()
				&& boundControl->ControlInstance->GetPropertyExpressionKind(
					L"AutomationProperties.Name")
					== DependencyPropertyExpressionKind::None,
			L"binding preview: removing DataContext resurrected the replaced Local value");
		AppendFailure(failures,
			boundControl->ControlInstance->DataBindings.Count() == 0,
			L"binding preview: transient runtime binding was not removed");
		const auto detached = boundControl->BindingPreviewStates.find(
			L"AutomationProperties.Name");
		AppendFailure(failures,
			detached != boundControl->BindingPreviewStates.end()
				&& detached->second.Status
					== DesignerBindingPreviewStatus::Detached,
			L"binding preview: detached state was not reported");
	}

	DesignerCanvas invisibleControlCanvas(0, 0, 800, 640);
	invisibleControlCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 180, 170 });
	const auto invisibleControl =
		invisibleControlCanvas.GetSelectedControl();
	auto* const invisibleRuntime = invisibleControl
		? invisibleControl->ControlInstance : nullptr;
	PropertyGrid invisiblePropertyGrid(0, 0, 360, 620);
	invisiblePropertyGrid.SetDesignerCanvas(&invisibleControlCanvas);
	ReloadCurrentSelection(
		invisiblePropertyGrid, invisibleControlCanvas);
	const auto hideControlResult =
		invisiblePropertyGrid.ApplyPropertyValue(L"Visibility", L"Hidden");
	cui::framework::PresentationAccess::Prepare(invisibleControlCanvas);
	AppendFailure(failures,
		hideControlResult.Succeeded
		&& invisibleRuntime
		&& invisibleRuntime->Visibility == Visibility::Hidden
		&& invisibleControlCanvas.GetSelectedControl() == invisibleControl,
		L"designer visibility: Hidden discarded the selected control");
	if (invisibleRuntime)
	{
		const auto runtimeLocation = invisibleRuntime->GetAbsoluteLocationDip();
		const auto runtimeSize = invisibleRuntime->GetActualSizeDip();
		const auto canvasLocation = invisibleControlCanvas.GetAbsoluteLocationDip();
		const POINT hiddenCenter{
			static_cast<LONG>(std::lround(
				runtimeLocation.x - canvasLocation.x + runtimeSize.width / 2)),
			static_cast<LONG>(std::lround(
				runtimeLocation.y - canvasLocation.y + runtimeSize.height / 2))
		};
		invisibleControlCanvas.RestoreSelectionByNames({}, L"", false);
		(void)cui::framework::InputAccess::DispatchInput(invisibleControlCanvas, PointerInput(
			InputReportKind::PointerDown, MouseButton::Left,
			hiddenCenter.x, hiddenCenter.y, MouseButton::Left));
		AppendFailure(failures,
			invisibleControlCanvas.GetSelectedControl() == invisibleControl,
			L"designer visibility: hidden placeholder was not hit-testable");
		(void)invisibleControlCanvas.CancelActivePointerInteraction(
			L"self-test cleanup");
	}

	DesignerCanvas hiddenAncestorCanvas(0, 0, 800, 640);
	hiddenAncestorCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Canvas, POINT{ 230, 200 });
	const auto hiddenAncestor = hiddenAncestorCanvas.GetSelectedControl();
	auto* const hiddenAncestorRuntime = hiddenAncestor
		? hiddenAncestor->ControlInstance : nullptr;
	if (hiddenAncestorRuntime)
	{
		const POINT inside = RoundedPoint(hiddenAncestorRuntime->GetAbsoluteLocationDip().x
				- hiddenAncestorCanvas.GetAbsoluteLocationDip().x + 60, hiddenAncestorRuntime->GetAbsoluteLocationDip().y
				- hiddenAncestorCanvas.GetAbsoluteLocationDip().y + 55);
		hiddenAncestorCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, inside);
	}
	const auto hiddenDescendant = hiddenAncestorCanvas.GetSelectedControl();
	if (hiddenAncestorRuntime)
		hiddenAncestorRuntime->Visibility = Visibility::Hidden;
	cui::framework::PresentationAccess::Prepare(hiddenAncestorCanvas);
	AppendFailure(failures,
		hiddenAncestor && hiddenDescendant
		&& hiddenAncestorCanvas.GetSelectedControl() == nullptr,
		L"designer visibility: a hidden ancestor retained a concealed descendant selection");
	if (hiddenAncestor)
	{
		hiddenAncestorCanvas.RestoreSelectionByNames(
			{ hiddenAncestor->Name }, hiddenAncestor->Name, false);
		cui::framework::PresentationAccess::Prepare(hiddenAncestorCanvas);
		AppendFailure(failures,
			hiddenAncestorCanvas.GetSelectedControl() == hiddenAncestor,
			L"designer visibility: a self-hidden container was not retained");
	}

	DesignerCanvas multiEventCanvas(0, 0, 800, 640);
	multiEventCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 150, 140 });
	multiEventCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_CheckBox, POINT{ 330, 140 });
	const auto multiEventButton = multiEventCanvas.GetAllControls().size() > 1
		? multiEventCanvas.GetAllControls()[1] : nullptr;
	const auto multiEventCheck = multiEventCanvas.GetAllControls().size() > 2
		? multiEventCanvas.GetAllControls()[2] : nullptr;
	if (multiEventButton && multiEventCheck)
	{
		multiEventButton->EventHandlers[L"Click"] = L"FirstOnlyClick";
		multiEventCheck->EventHandlers[L"MouseMove"] = L"ConflictingShared";
		multiEventCanvas.RestoreSelectionByNames(
			{ multiEventButton->Name, multiEventCheck->Name },
			multiEventCheck->Name, false);
	}
	PropertyGrid multiEventGrid(0, 0, 380, 520);
	multiEventGrid.SetDesignerCanvas(&multiEventCanvas);
	multiEventGrid.SetViewMode(DesignerPropertyGridViewMode::Events);
	ReloadCurrentSelection(multiEventGrid, multiEventCanvas);
	auto* multiEventNativeGrid = multiEventGrid.GetNativePropertyGrid();
	const PropertyGridItem* mixedCommonEventItem = nullptr;
	bool multiSelectionExposedNonCommonEvent = false;
	if (multiEventNativeGrid)
	{
		for (const auto& item : multiEventNativeGrid->Items)
		{
			if (item.Name.rfind(L"Click", 0) == 0)
				mixedCommonEventItem = &item;
			if (item.Name.rfind(L"Checked", 0) == 0)
				multiSelectionExposedNonCommonEvent = true;
		}
	}
	AppendFailure(failures,
		multiEventButton && multiEventCheck && mixedCommonEventItem
		&& mixedCommonEventItem->IsMixed
		&& mixedCommonEventItem->Value == L"<多个值>"
		&& mixedCommonEventItem->CanReset
		&& mixedCommonEventItem->ValueType
			== PropertyGridValueType::EditableEnum
		&& mixedCommonEventItem->Category.find(L"公共事件")
			!= std::wstring::npos
		&& mixedCommonEventItem->Description.find(L"2 个选中控件")
			!= std::wstring::npos
		&& !multiSelectionExposedNonCommonEvent,
		L"multi-selection events: common intersection or mixed value was not presented");

	const auto multiEventUndoBefore = multiEventCanvas.GetUndoCommandCount();
	const auto multiEventEdit = multiEventGrid.ApplyPropertyValue(
		L"Click", L"HandleMultiSelectionClick");
	const bool multiEventApplied = multiEventButton && multiEventCheck
		&& multiEventButton->EventHandlers[L"Click"]
			== L"HandleMultiSelectionClick"
		&& multiEventCheck->EventHandlers[L"Click"]
			== L"HandleMultiSelectionClick"
		&& multiEventCheck->EventHandlers[L"MouseMove"]
			== L"ConflictingShared";
	const auto multiEventUndoAfter = multiEventCanvas.GetUndoCommandCount();
	const auto undoMultiEvent = multiEventCanvas.UndoCommand();
	const bool multiEventRestored = multiEventButton && multiEventCheck
		&& multiEventButton->EventHandlers[L"Click"] == L"FirstOnlyClick"
		&& !multiEventCheck->EventHandlers.contains(L"Click")
		&& multiEventCheck->EventHandlers[L"MouseMove"] == L"ConflictingShared";
	const auto conflictingMultiEvent = multiEventGrid.ApplyPropertyValue(
		L"Click", L"ConflictingShared");
	const bool multiEventConflictPreserved = multiEventButton && multiEventCheck
		&& multiEventButton->EventHandlers[L"Click"] == L"FirstOnlyClick"
		&& !multiEventCheck->EventHandlers.contains(L"Click")
		&& multiEventCheck->EventHandlers[L"MouseMove"] == L"ConflictingShared";
	AppendFailure(failures,
		multiEventEdit && multiEventEdit.AppliedCount == 2
		&& multiEventApplied
		&& multiEventUndoAfter == multiEventUndoBefore + 1
		&& undoMultiEvent.HasChanges() && multiEventRestored
		&& multiEventCanvas.GetSelectedControls().size() == 2
		&& multiEventCanvas.GetSelectedControl() == multiEventCheck
		&& !conflictingMultiEvent && !conflictingMultiEvent.Error.empty()
		&& multiEventConflictPreserved,
		L"multi-selection events: atomic edit, undo, or signature rejection failed");

	std::wstring multiActivatedHandler;
	int multiActivationCount = 0;
	multiEventGrid.OnEventHandlerActivated +=
		[&](PropertyGrid*, const std::wstring& handler)
		{
			multiActivatedHandler = handler;
			++multiActivationCount;
		};
	std::wstring multiDefaultHandler;
	const auto activateMultiEvent = multiEventGrid.ActivateEventHandler(
		L"Click", &multiDefaultHandler);
	const auto expectedMultiDefault = multiEventCheck
		? multiEventCheck->Name + L"_Click" : std::wstring{};
	const bool multiActivationApplied = multiEventButton && multiEventCheck
		&& multiEventButton->EventHandlers[L"Click"]
			== expectedMultiDefault
		&& multiEventCheck->EventHandlers[L"Click"]
			== expectedMultiDefault;
	const auto undoMultiActivation = multiEventCanvas.UndoCommand();
	AppendFailure(failures,
		activateMultiEvent && activateMultiEvent.AppliedCount == 2
		&& !expectedMultiDefault.empty()
		&& multiDefaultHandler == expectedMultiDefault
		&& multiActivatedHandler == expectedMultiDefault
		&& multiActivationCount == 1
		&& multiActivationApplied
		&& undoMultiActivation.HasChanges()
		&& multiEventButton
		&& multiEventButton->EventHandlers[L"Click"] == L"FirstOnlyClick"
		&& multiEventCheck
		&& !multiEventCheck->EventHandlers.contains(L"Click"),
		L"multi-selection events: activation did not create one shared default handler");

	ReloadCurrentSelection(multiEventGrid, multiEventCanvas);
	int multiEventResetIndex = -1;
	if (multiEventNativeGrid)
		for (int index = 0;
			index < static_cast<int>(multiEventNativeGrid->Items.size()); ++index)
			if (multiEventNativeGrid->Items[static_cast<size_t>(index)].Name.rfind(
				L"Click", 0) == 0)
			{
				multiEventResetIndex = index;
				break;
			}
	const auto multiResetUndoBefore = multiEventCanvas.GetUndoCommandCount();
	const bool requestedMultiEventReset = multiEventNativeGrid
		&& multiEventResetIndex >= 0
		&& multiEventNativeGrid->RequestReset(multiEventResetIndex);
	const bool multiEventResetApplied = multiEventButton && multiEventCheck
		&& !multiEventButton->EventHandlers.contains(L"Click")
		&& !multiEventCheck->EventHandlers.contains(L"Click")
		&& multiEventCheck->EventHandlers[L"MouseMove"] == L"ConflictingShared";
	const auto undoMultiEventReset = multiEventCanvas.UndoCommand();
	AppendFailure(failures,
		requestedMultiEventReset && multiEventResetApplied
		&& multiEventCanvas.GetUndoCommandCount() == multiResetUndoBefore
		&& undoMultiEventReset.HasChanges()
		&& multiEventButton
		&& multiEventButton->EventHandlers[L"Click"] == L"FirstOnlyClick"
		&& multiEventCheck
		&& !multiEventCheck->EventHandlers.contains(L"Click"),
		L"multi-selection events: reset affordance was not atomic or undoable");

	DesignerCanvas falseBooleanCanvas(0, 0, 800, 640);
	falseBooleanCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_CheckBox, POINT{ 170, 160 });
	if (auto eventControl = falseBooleanCanvas.GetSelectedControl())
		eventControl->EventHandlers[L"Click"] = L"ExistingCheckClick";
	PropertyGrid falseBooleanGrid(0, 0, 360, 360);
	falseBooleanGrid.SetDesignerCanvas(&falseBooleanCanvas);
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const auto checkedRow = std::find_if(
		falseBooleanGrid.GetPresentedPropertyRows().begin(),
		falseBooleanGrid.GetPresentedPropertyRows().end(),
		[](const DesignerPropertyRow& row)
		{ return row.Name == L"IsChecked"; });
	const auto checkedDisplayName =
		checkedRow != falseBooleanGrid.GetPresentedPropertyRows().end()
		? checkedRow->DisplayName : std::wstring{};
	auto* nativeFalseGrid = falseBooleanGrid.GetNativePropertyGrid();
	const PropertyGridItem* nativeCheckedItem = nullptr;
	bool exposedRawColorEditor = false;
	bool propertyViewContainedEvent = false;
	int nativeCheckedIndex = -1;
	if (nativeFalseGrid)
	{
		for (int index = 0;
			index < static_cast<int>(nativeFalseGrid->Items.size()); ++index)
		{
			const auto& item = nativeFalseGrid->Items[static_cast<size_t>(index)];
			if (checkedRow != falseBooleanGrid.GetPresentedPropertyRows().end()
				&& item.Name.rfind(checkedRow->DisplayName, 0) == 0)
			{
				nativeCheckedItem = &item;
				nativeCheckedIndex = index;
			}
			exposedRawColorEditor = exposedRawColorEditor
				|| item.ValueType == PropertyGridValueType::Color;
			if (item.Name == L"Click" || item.Name == L"Checked")
				propertyViewContainedEvent = true;
		}
	}
	AppendFailure(failures,
		checkedRow != falseBooleanGrid.GetPresentedPropertyRows().end()
		&& checkedRow->Value.Text == L"false"
		&& nativeFalseGrid && nativeFalseGrid->IsVisible
		&& nativeCheckedItem
		&& nativeCheckedItem->ValueType == PropertyGridValueType::Enum
		&& nativeCheckedItem->Value == L"False"
		&& nativeCheckedItem->Options
			== std::vector<std::wstring>{
				L"False", L"True", L"Indeterminate" }
		&& !nativeCheckedItem->IsMixed,
		L"nullable boolean editor: IsChecked did not expose the WPF three-state choices");
	AppendFailure(failures,
		DesignerPropertyRowCatalog::Find(
			falseBooleanGrid.GetPresentedPropertyRows(), L"Background") == nullptr
		&& !exposedRawColorEditor,
		L"native property grid: non-browsable WPF object properties leaked through a raw color editor");
	AppendFailure(failures,
		falseBooleanGrid.GetViewMode()
			== DesignerPropertyGridViewMode::Properties
		&& nativeFalseGrid
		&& nativeFalseGrid->GetNameHeaderLabel() == L"属性"
		&& nativeFalseGrid->GetValueHeaderLabel() == L"值"
		&& !propertyViewContainedEvent,
		L"property/event views: property mode still mixed event rows into properties");
	std::wstring propertyModeCategory = nativeCheckedItem
		? nativeCheckedItem->Category : std::wstring{};
	if (nativeFalseGrid && !propertyModeCategory.empty())
	{
		nativeFalseGrid->CollapseCategory(propertyModeCategory, true);
		nativeFalseGrid->SetScrollOffset(80.0f);
	}
	const float propertyModeScroll = nativeFalseGrid
		? nativeFalseGrid->ScrollYOffset : 0.0f;
	falseBooleanGrid.SetViewMode(DesignerPropertyGridViewMode::Events);
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const PropertyGridItem* nativeEventItem = nullptr;
	const PropertyGridItem* nativeDefaultEventItem = nullptr;
	const PropertyGridItem* nativeEventActivationItem = nullptr;
	const PropertyGridItem* nativeEventManagerItem = nullptr;
	bool eventViewContainedProperty = false;
	if (nativeFalseGrid)
	{
		for (const auto& item : nativeFalseGrid->Items)
		{
			if (item.Name.rfind(L"Click", 0) == 0)
				nativeEventItem = &item;
			if (item.Name.rfind(L"Checked", 0) == 0)
				nativeDefaultEventItem = &item;
			if (item.Name == L"生成/定位处理函数")
				nativeEventActivationItem = &item;
			if (item.Name == L"重命名处理函数")
				nativeEventManagerItem = &item;
			if (!checkedDisplayName.empty()
				&& item.Name.rfind(checkedDisplayName, 0) == 0)
				eventViewContainedProperty = true;
		}
	}
	const auto eventControl = falseBooleanCanvas.GetSelectedControl();
	const auto defaultEventName = eventControl
		? eventControl->Name + L"_Click" : std::wstring{};
	AppendFailure(failures,
		nativeEventItem
		&& nativeEventItem->ValueType == PropertyGridValueType::EditableEnum
		&& nativeEventItem->CanReset
		&& nativeEventItem->Value == L"ExistingCheckClick"
		&& nativeEventItem->Name.find(L"[未关联代码]") != std::wstring::npos
		&& std::find(nativeEventItem->Options.begin(), nativeEventItem->Options.end(),
			defaultEventName) != nativeEventItem->Options.end(),
		L"native property grid: existing event mapping did not expose an editable handler");
	AppendFailure(failures,
		nativeDefaultEventItem
		&& nativeDefaultEventItem->Category.find(L"值变化") != std::wstring::npos
		&& nativeDefaultEventItem->Description.find(L"默认事件") != std::wstring::npos,
		L"native property grid: event category or default-event metadata was not presented");
	AppendFailure(failures,
		nativeEventActivationItem
		&& nativeEventActivationItem->ValueType
			== PropertyGridValueType::Action
		&& nativeEventActivationItem->Value.find(L"F12")
			!= std::wstring::npos
		&& nativeEventActivationItem->Value.find(L"Checked")
			!= std::wstring::npos
		&& nativeEventActivationItem->Description.find(L"不会覆盖")
			!= std::wstring::npos,
		L"native property grid: explicit event generation/location action was not exposed");
	AppendFailure(failures,
		nativeEventManagerItem
		&& nativeEventManagerItem->ValueType == PropertyGridValueType::Action
		&& nativeEventManagerItem->Value.find(L"1") != std::wstring::npos,
		L"native property grid: document-wide event handler manager was not exposed");
	AppendFailure(failures,
		falseBooleanGrid.GetViewMode() == DesignerPropertyGridViewMode::Events
		&& nativeFalseGrid
		&& nativeFalseGrid->GetNameHeaderLabel() == L"事件"
		&& nativeFalseGrid->GetValueHeaderLabel() == L"处理函数"
		&& !eventViewContainedProperty,
		L"property/event views: event mode still mixed property rows into events");
	const auto eventControlBeforeDelta = falseBooleanCanvas.GetSelectedControl();
	const auto eventHistoryMemoryBefore =
		falseBooleanCanvas.GetCommandHistoryMemoryUsage();
	const auto eventUndoCountBefore = falseBooleanCanvas.GetUndoCommandCount();
	const auto namedEventEdit = falseBooleanGrid.ApplyPropertyValue(
		L"Click", L"HandleCheckClick");
	const auto eventHistoryMemoryAfter =
		falseBooleanCanvas.GetCommandHistoryMemoryUsage();
	const auto eventUndoCountAfter = falseBooleanCanvas.GetUndoCommandCount();
	const auto undoNamedEvent = falseBooleanCanvas.UndoCommand();
	const bool restoredExistingEvent = eventControlBeforeDelta
		&& eventControlBeforeDelta->EventHandlers[L"Click"]
			== L"ExistingCheckClick";
	const auto redoNamedEvent = falseBooleanCanvas.RedoCommand();
	const bool restoredNamedEvent = eventControlBeforeDelta
		&& eventControlBeforeDelta->EventHandlers[L"Click"]
			== L"HandleCheckClick";
	const auto conflictingEventEdit = falseBooleanGrid.ApplyPropertyValue(
		L"MouseDoubleClick", L"HandleCheckClick");
	const auto invalidEventEdit = falseBooleanGrid.ApplyPropertyValue(
		L"Click", L"bad::handler");
	const auto currentEventControl = falseBooleanCanvas.GetSelectedControl();
	std::wstring activatedHandler;
	int activatedHandlerCount = 0;
	falseBooleanGrid.OnEventHandlerActivated +=
		[&](PropertyGrid*, const std::wstring& handler)
		{
			activatedHandler = handler;
			++activatedHandlerCount;
		};
	std::wstring existingActivatedHandler;
	const auto existingEventActivation = falseBooleanGrid.ActivateEventHandler(
		L"Click", &existingActivatedHandler);
	std::wstring defaultActivatedHandler;
	const auto defaultEventActivation = falseBooleanGrid.ActivateEventHandler(
		L"MouseDoubleClick", &defaultActivatedHandler);
	std::wstring catalogDefaultActivatedHandler;
	const auto catalogDefaultActivation =
		falseBooleanGrid.ActivateDefaultEventHandler(
			&catalogDefaultActivatedHandler);
	const auto expectedDoubleClickHandler = currentEventControl
		? currentEventControl->Name + L"_MouseDoubleClick"
		: std::wstring{};
	const auto expectedCheckedHandler = currentEventControl
		? currentEventControl->Name + L"_Checked"
		: std::wstring{};
	AppendFailure(failures,
		namedEventEdit
		&& eventUndoCountAfter == eventUndoCountBefore + 1
		&& eventHistoryMemoryAfter > eventHistoryMemoryBefore
		&& eventHistoryMemoryAfter - eventHistoryMemoryBefore < 32 * 1024
		&& undoNamedEvent.HasChanges()
		&& restoredExistingEvent
		&& redoNamedEvent.HasChanges()
		&& restoredNamedEvent
		&& falseBooleanCanvas.GetSelectedControl() == eventControlBeforeDelta
		&& !conflictingEventEdit
		&& !conflictingEventEdit.Error.empty()
		&& !invalidEventEdit
		&& currentEventControl
		&& currentEventControl->EventHandlers[L"Click"]
			== L"HandleCheckClick",
		L"native property grid: event delta, identity, or signature validation failed");
	AppendFailure(failures,
		existingEventActivation
		&& existingEventActivation.AppliedCount == 0
		&& existingActivatedHandler == L"HandleCheckClick"
		&& defaultEventActivation
		&& defaultEventActivation.AppliedCount == 1
		&& !expectedDoubleClickHandler.empty()
		&& defaultActivatedHandler == expectedDoubleClickHandler
		&& catalogDefaultActivation
		&& catalogDefaultActivation.AppliedCount == 1
		&& catalogDefaultActivatedHandler == expectedCheckedHandler
		&& activatedHandler == expectedCheckedHandler
		&& activatedHandlerCount == 3
		&& currentEventControl
		&& currentEventControl->EventHandlers[L"MouseDoubleClick"]
			== expectedDoubleClickHandler
		&& currentEventControl->EventHandlers[L"Checked"]
			== expectedCheckedHandler,
		L"native property grid: event activation did not reuse or create an undoable handler");

	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	int doubleClickEventIndex = -1;
	int checkedEventIndex = -1;
	int eventActivationIndex = -1;
	if (nativeFalseGrid)
	{
		for (size_t i = 0; i < nativeFalseGrid->Items.size(); ++i)
		{
			const auto& item = nativeFalseGrid->Items[i];
			if (item.Name.rfind(L"MouseDoubleClick", 0) == 0)
				doubleClickEventIndex = static_cast<int>(i);
			if (item.Name.rfind(L"Checked", 0) == 0)
				checkedEventIndex = static_cast<int>(i);
			if (item.Name == L"生成/定位处理函数")
				eventActivationIndex = static_cast<int>(i);
		}
	}
	const auto explicitActivationUndoBefore =
		falseBooleanCanvas.GetUndoCommandCount();
	const int explicitActivationCountBefore = activatedHandlerCount;
	bool explicitActionActivated = false;
	if (nativeFalseGrid && doubleClickEventIndex >= 0
		&& eventActivationIndex >= 0)
	{
		nativeFalseGrid->SelectItem(doubleClickEventIndex);
		explicitActionActivated =
			nativeFalseGrid->ActivateItem(eventActivationIndex);
	}
	const bool explicitActionReusedExpected = activatedHandler
		== expectedDoubleClickHandler
		&& activatedHandlerCount == explicitActivationCountBefore + 1;
	const bool f12Activated = nativeFalseGrid && checkedEventIndex >= 0;
	if (f12Activated)
	{
		nativeFalseGrid->SelectItem(checkedEventIndex);
		nativeFalseGrid->OnKeyDown(
			nativeFalseGrid, KeyEventArgs(Key::F12));
	}
	AppendFailure(failures,
		explicitActionActivated
		&& explicitActionReusedExpected
		&& f12Activated
		&& activatedHandler == expectedCheckedHandler
		&& activatedHandlerCount == explicitActivationCountBefore + 2
		&& falseBooleanCanvas.GetUndoCommandCount()
			== explicitActivationUndoBefore,
		L"native property grid: action row and F12 did not share the safe event activation path");

	DesignerModel::DesignEventHandlerCodeInspection eventCodeInspection;
	eventCodeInspection.Associated = true;
	DesignerModel::DesignEventHandlerCodeEntry eventCodeEntry;
	eventCodeEntry.HandlerName = L"HandleCheckClick";
	eventCodeEntry.ParameterList = L"Control* sender, RoutedEventArgs& e";
	eventCodeEntry.State =
		DesignerModel::DesignEventHandlerCodeState::SignatureMismatch;
	eventCodeEntry.Diagnostic = L"现有定义参数签名不匹配；双击定位后修正。";
	eventCodeInspection.Handlers.emplace(
		eventCodeEntry.HandlerName, std::move(eventCodeEntry));
	eventCodeInspection.CompatibleUserHandlers[
		"Control* sender, RoutedEventArgs& e"] = {
			L"ReusableMouseHandler", expectedCheckedHandler };
	falseBooleanGrid.SetEventHandlerCodeInspection(
		std::move(eventCodeInspection));
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const PropertyGridItem* signatureDiagnosticItem = nullptr;
	if (nativeFalseGrid)
		for (const auto& item : nativeFalseGrid->Items)
			if (item.Name.rfind(L"Click", 0) == 0)
			{
				signatureDiagnosticItem = &item;
				break;
			}
	AppendFailure(failures,
		signatureDiagnosticItem
		&& signatureDiagnosticItem->Name.find(L"[签名错误]")
			!= std::wstring::npos
		&& signatureDiagnosticItem->Description.find(L"参数签名不匹配")
			!= std::wstring::npos
		&& std::find(signatureDiagnosticItem->Options.begin(),
			signatureDiagnosticItem->Options.end(), L"ReusableMouseHandler")
			!= signatureDiagnosticItem->Options.end()
		&& std::find(signatureDiagnosticItem->Options.begin(),
			signatureDiagnosticItem->Options.end(), expectedCheckedHandler)
			!= signatureDiagnosticItem->Options.end(),
		L"native property grid: event source diagnostic was not visible on the row");
	std::wstring eventModeCategory;
	if (nativeFalseGrid)
	{
		for (const auto& item : nativeFalseGrid->Items)
		{
			if (item.Name.rfind(L"Click", 0) == 0)
			{
				eventModeCategory = item.Category;
				break;
			}
		}
		if (!eventModeCategory.empty())
			nativeFalseGrid->CollapseCategory(eventModeCategory, true);
		nativeFalseGrid->SetScrollOffset(70.0f);
	}
	const float eventModeScroll = nativeFalseGrid
		? nativeFalseGrid->ScrollYOffset : 0.0f;
	falseBooleanGrid.SetViewMode(DesignerPropertyGridViewMode::Properties);
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const bool restoredPropertyModeState = nativeFalseGrid
		&& !propertyModeCategory.empty()
		&& nativeFalseGrid->IsCategoryCollapsed(propertyModeCategory)
		&& propertyModeScroll > 0.0f
		&& std::fabs(nativeFalseGrid->ScrollYOffset - propertyModeScroll) < 0.01f;
	falseBooleanGrid.SetViewMode(DesignerPropertyGridViewMode::Events);
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const bool restoredEventModeState = nativeFalseGrid
		&& !eventModeCategory.empty()
		&& nativeFalseGrid->IsCategoryCollapsed(eventModeCategory)
		&& eventModeScroll > 0.0f
		&& std::fabs(nativeFalseGrid->ScrollYOffset - eventModeScroll) < 0.01f;
	AppendFailure(failures,
		restoredPropertyModeState && restoredEventModeState,
		L"property/event views: collapse or scroll state leaked across view modes");

	falseBooleanGrid.SetFilterText(L"Mouse");
	falseBooleanGrid.SetViewMode(DesignerPropertyGridViewMode::Properties);
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const bool propertyFilterInitiallyIndependent =
		falseBooleanGrid.GetFilterText().empty();
	falseBooleanGrid.SetFilterText(L"IsChecked");
	falseBooleanGrid.SetViewMode(DesignerPropertyGridViewMode::Events);
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const bool restoredEventFilter =
		falseBooleanGrid.GetFilterText() == L"Mouse";
	falseBooleanGrid.SetViewMode(DesignerPropertyGridViewMode::Properties);
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const bool restoredPropertyFilter =
		falseBooleanGrid.GetFilterText() == L"IsChecked";
	AppendFailure(failures,
		propertyFilterInitiallyIndependent
		&& restoredEventFilter && restoredPropertyFilter,
		L"property/event views: filters were not retained independently");
	falseBooleanGrid.SetFilterText(L"");
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	nativeCheckedIndex = -1;
	if (nativeFalseGrid)
	{
		for (int index = 0;
			index < static_cast<int>(nativeFalseGrid->Items.size()); ++index)
		{
			const auto& item = nativeFalseGrid->Items[static_cast<size_t>(index)];
			if (!checkedDisplayName.empty()
				&& item.Name.rfind(checkedDisplayName, 0) == 0)
			{
				nativeCheckedIndex = index;
				break;
			}
		}
	}

	const std::wstring navigationSource =
		L"D:\\Project Folder\\Window.cpp";
	const auto codeNavigationPlan = SourceCodeNavigator::BuildPlan(
		SourceCodeEditorKind::VisualStudioCode,
		L"C:\\Editor Folder\\Code.exe", navigationSource, 42);
	const auto visualStudioNavigationPlan = SourceCodeNavigator::BuildPlan(
		SourceCodeEditorKind::VisualStudio,
		L"C:\\Visual Studio\\devenv.exe", navigationSource, 42);
	const auto customNavigationPlan = SourceCodeNavigator::BuildPlan(
		SourceCodeEditorKind::Custom,
		L"C:\\Custom Editor\\editor.exe", navigationSource, 42,
		L"--file {file} --line {line} --column {column}");
	const auto customAppendFilePlan = SourceCodeNavigator::BuildPlan(
		SourceCodeEditorKind::Custom,
		L"C:\\Custom Editor\\editor.exe", navigationSource, 0,
		L"--reuse-window");
	AppendFailure(failures,
		codeNavigationPlan.RequestsExactLine
		&& codeNavigationPlan.Arguments
			== L"--goto \"D:\\Project Folder\\Window.cpp:42:1\""
		&& visualStudioNavigationPlan.RequestsExactLine
		&& visualStudioNavigationPlan.Arguments
			== L"/Edit \"D:\\Project Folder\\Window.cpp\" /Command \"Edit.Goto 42\""
		&& customNavigationPlan.RequestsExactLine
		&& customNavigationPlan.Arguments
			== L"--file \"D:\\Project Folder\\Window.cpp\" --line 42 --column 1"
		&& !customAppendFilePlan.RequestsExactLine
		&& customAppendFilePlan.Arguments
			== L"--reuse-window \"D:\\Project Folder\\Window.cpp\""
		&& SourceCodeNavigator::QuoteArgument(L"C:\\Folder With Space\\")
			== L"\"C:\\Folder With Space\\\\\"",
		L"source navigation: editor plans did not quote paths or request exact lines safely");
	constexpr std::string_view locatorSource =
		"// void Fake::HandleClick() {}\n"
		"const char* text = \"Fake::HandleClick() {\";\n"
		"auto raw = R\"tag(Fake::HandleClick() {})tag\";\n"
		"void Fake::HandleClick();\n"
		"/* void Fake::HandleClick() {} */\n"
		"void Other::HandleClick() {}\n"
		"void Acme::Window::HandleClick(\n"
		"    Control*, RoutedEventArgs&)\n"
		"{\n"
		"}\n";
	AppendFailure(failures,
		SourceCodeNavigator::FindMemberDefinitionLineInText(
			locatorSource, "HandleClick", "Acme::Window") == 7
		&& SourceCodeNavigator::FindMemberDefinitionLineInText(
			locatorSource, "HandleClick", "Missing::Window") == 0
		&& SourceCodeNavigator::FindMemberDefinitionLineInText(
			locatorSource, "MissingHandler", "Acme::Window") == 0,
		L"source navigation: comments, literals, declarations, or another class produced a false target line");
	constexpr std::string_view overloadedLocatorSource =
		"void Acme::Window::HandleClick(Control*, KeyEventArgs&) {}\n"
		"void Acme::Window::HandleClick(Control*, RoutedEventArgs&) {}\n";
	constexpr std::string_view namespaceLocatorSource =
		"namespace Acme::Views {\n"
		"void Window::HandleClick(Control*, RoutedEventArgs&) {}\n"
		"}\n";
	constexpr std::string_view conditionalLocatorSource =
		"#define FAKE_SCOPE { ignored }\n"
		"#if 0\n"
		"void Acme::Window::HandleClick(Control*, RoutedEventArgs&) {}\n"
		"#endif\n"
		"void Acme::Window::HandleClick(Control*, RoutedEventArgs&) {}\n";
	constexpr std::string_view inlineLocatorSource =
		"namespace Acme {\n"
		"class Window {\n"
		"public:\n"
		"    void HandleClick(Control*, RoutedEventArgs&) {}\n"
		"};\n"
		"}\n";
	AppendFailure(failures,
		SourceCodeNavigator::FindMemberDefinitionLineInText(
			overloadedLocatorSource, "HandleClick", "Acme::Window",
			"Control* sender, RoutedEventArgs& e") == 2
		&& SourceCodeNavigator::FindMemberDefinitionLineInText(
			overloadedLocatorSource, "HandleClick", "Acme::Window") == 1,
		L"source navigation: signature-aware lookup did not select the compatible overload");
	AppendFailure(failures,
		SourceCodeNavigator::FindMemberDefinitionLineInText(
			namespaceLocatorSource, "HandleClick", "Acme::Views::Window",
			"Control* sender, RoutedEventArgs& e") == 2
		&& SourceCodeNavigator::FindMemberDefinitionLineInText(
			namespaceLocatorSource, "HandleClick", "Acme::Other::Window") == 0,
		L"source navigation: namespace-scoped member definition was not resolved exactly");
	AppendFailure(failures,
		SourceCodeNavigator::FindMemberDefinitionLineInText(
			conditionalLocatorSource, "HandleClick", "Acme::Window",
			"Control* sender, RoutedEventArgs& e") == 5,
		L"source navigation: disabled preprocessor branch or macro body produced a false definition");
	AppendFailure(failures,
		SourceCodeNavigator::FindMemberDefinitionLineInText(
			inlineLocatorSource, "HandleClick", "Acme::Window",
			"Control* sender, RoutedEventArgs& e") == 4,
		L"source navigation: inline class member definition was not located");

	DesignerCanvas canvasDefaultEventCanvas(0, 0, 800, 640);
	canvasDefaultEventCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 180, 160 });
	PropertyGrid canvasDefaultEventGrid(0, 0, 360, 360);
	canvasDefaultEventGrid.SetDesignerCanvas(&canvasDefaultEventCanvas);
	ReloadCurrentSelection(canvasDefaultEventGrid, canvasDefaultEventCanvas);
	int canvasDefaultRequestCount = 0;
	DesignerPropertyEditResult canvasDefaultResult;
	canvasDefaultEventCanvas.OnControlSelected +=
		[&](std::shared_ptr<DesignerControl> selected)
		{
			canvasDefaultEventGrid.LoadControls(
				canvasDefaultEventCanvas.GetSelectedControls(), selected);
		};
	canvasDefaultEventCanvas.OnDefaultEventRequested +=
		[&](std::shared_ptr<DesignerControl>)
		{
			++canvasDefaultRequestCount;
			canvasDefaultResult =
				canvasDefaultEventGrid.ActivateDefaultEventHandler();
		};
	const auto canvasDefaultControl =
		canvasDefaultEventCanvas.GetSelectedControl();
	if (canvasDefaultControl && canvasDefaultControl->ControlInstance)
	{
		auto* runtime = canvasDefaultControl->ControlInstance;
		const auto size = runtime->GetActualSizeDip();
		const POINT center = RoundedPoint(runtime->GetAbsoluteLocationDip().x - canvasDefaultEventCanvas.GetAbsoluteLocationDip().x
				+ size.width / 2, runtime->GetAbsoluteLocationDip().y - canvasDefaultEventCanvas.GetAbsoluteLocationDip().y
				+ size.height / 2);
		(void)cui::framework::InputAccess::DispatchInput(canvasDefaultEventCanvas, PointerInput(
			InputReportKind::PointerDoubleClick, MouseButton::Left,
			center.x, center.y, MouseButton::Left));
	}
	const auto expectedCanvasHandler = canvasDefaultControl
		? canvasDefaultControl->Name + L"_Click" : std::wstring{};
	AppendFailure(failures,
		canvasDefaultControl
		&& canvasDefaultRequestCount == 1
		&& canvasDefaultResult
		&& canvasDefaultControl->EventHandlers[L"Click"]
			== expectedCanvasHandler,
		L"canvas default event: control double-click did not create the catalog default handler");

	DesignerCanvas formDefaultEventCanvas(0, 0, 800, 640);
	PropertyGrid formDefaultEventGrid(0, 0, 360, 360);
	formDefaultEventGrid.SetDesignerCanvas(&formDefaultEventCanvas);
	formDefaultEventGrid.LoadControls({}, nullptr);
	int formDefaultRequestCount = 0;
	DesignerPropertyEditResult formDefaultResult;
	formDefaultEventCanvas.OnControlSelected +=
		[&](std::shared_ptr<DesignerControl> selected)
		{
			formDefaultEventGrid.LoadControls(
				formDefaultEventCanvas.GetSelectedControls(), selected);
		};
	formDefaultEventCanvas.OnDefaultEventRequested +=
		[&](std::shared_ptr<DesignerControl>)
		{
			++formDefaultRequestCount;
			formDefaultResult =
				formDefaultEventGrid.ActivateDefaultEventHandler();
		};
	(void)cui::framework::InputAccess::DispatchInput(formDefaultEventCanvas, PointerInput(
		InputReportKind::PointerDoubleClick, MouseButton::Left,
		30, 28, MouseButton::Left));
	const auto& formDefaultHandlers =
		formDefaultEventCanvas.GetDesignedWindowEventHandlers();
	const auto shownHandler = formDefaultHandlers.find(L"ContentRendered");
	const bool createdWindowDefault = shownHandler != formDefaultHandlers.end()
		&& shownHandler->second == L"MainWindow_ContentRendered";
	const auto formEventMemory =
		formDefaultEventCanvas.GetCommandHistoryMemoryUsage();
	const auto undoWindowDefault = formDefaultEventCanvas.UndoCommand();
	const bool removedWindowDefault =
		formDefaultEventCanvas.GetDesignedWindowEventHandlers().find(L"ContentRendered")
			== formDefaultEventCanvas.GetDesignedWindowEventHandlers().end();
	const auto redoWindowDefault = formDefaultEventCanvas.RedoCommand();
	const auto restoredWindowDefault =
		formDefaultEventCanvas.GetDesignedWindowEventHandlers().find(L"ContentRendered");
	AppendFailure(failures,
		formDefaultRequestCount == 1
		&& formDefaultResult
		&& createdWindowDefault
		&& formEventMemory > 0 && formEventMemory < 32 * 1024
		&& undoWindowDefault.HasChanges() && removedWindowDefault
		&& redoWindowDefault.HasChanges()
		&& restoredWindowDefault
			!= formDefaultEventCanvas.GetDesignedWindowEventHandlers().end()
		&& restoredWindowDefault->second == L"MainWindow_ContentRendered",
		L"canvas default event: Window double-click did not create ContentRendered");

	DesignerCanvas eventRenameCanvas(0, 0, 800, 640);
	eventRenameCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 120, 120 });
	eventRenameCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 280, 120 });
	const auto& renameControls = eventRenameCanvas.GetAllControls();
	if (renameControls.size() == 3)
	{
		renameControls[1]->EventHandlers[L"Click"] = L"HandleSharedClick";
		renameControls[2]->EventHandlers[L"Click"] = L"HandleSharedClick";
	}
	eventRenameCanvas.SetDesignedWindowEventHandler(
		L"ContentRendered", L"HandleWindowContentRendered");
	const auto renameHistoryMemoryBefore =
		eventRenameCanvas.GetCommandHistoryMemoryUsage();
	const auto renameUndoCountBefore = eventRenameCanvas.GetUndoCommandCount();
	const auto firstRenameInstance = renameControls.size() == 3
		? renameControls[1]->ControlInstance : nullptr;
	const auto secondRenameInstance = renameControls.size() == 3
		? renameControls[2]->ControlInstance : nullptr;
	size_t renamedEventReferences = 0;
	std::wstring renameCommandError;
	auto renamedEventTransaction = eventRenameCanvas.RenameEventHandler(
		L"HandleSharedClick", L"HandleClick",
		&renamedEventReferences, &renameCommandError);
	const bool renamedLiveReferences = renameControls.size() == 3
		&& renameControls[1]->EventHandlers[L"Click"] == L"HandleClick"
		&& renameControls[2]->EventHandlers[L"Click"] == L"HandleClick";
	auto undoEventRename = eventRenameCanvas.UndoCommand();
	const bool restoredOldEventReferences = renameControls.size() == 3
		&& renameControls[1]->EventHandlers[L"Click"] == L"HandleSharedClick"
		&& renameControls[2]->EventHandlers[L"Click"] == L"HandleSharedClick";
	auto redoEventRename = eventRenameCanvas.RedoCommand();
	const auto renameHistoryMemoryAfter =
		eventRenameCanvas.GetCommandHistoryMemoryUsage();
	const bool renameUsedCompactDelta =
		eventRenameCanvas.GetUndoCommandCount() == renameUndoCountBefore + 1
		&& renameHistoryMemoryAfter > renameHistoryMemoryBefore
		&& renameHistoryMemoryAfter - renameHistoryMemoryBefore < 32 * 1024
		&& renameControls.size() == 3
		&& renameControls[1]->ControlInstance == firstRenameInstance
		&& renameControls[2]->ControlInstance == secondRenameInstance;

	DesignerEventHandlerDelta staleEventDelta;
	if (renameControls.size() == 3)
	{
		staleEventDelta.StableId = renameControls[1]->StableId;
		staleEventDelta.ControlType = renameControls[1]->Type;
		staleEventDelta.SubjectName = renameControls[1]->Name;
		staleEventDelta.EventName = L"Click";
		staleEventDelta.Before = { true, L"HandleClick" };
		staleEventDelta.After = { true, L"GuardedClick" };
	}
	const auto staleHistoryCount = eventRenameCanvas.GetUndoCommandCount();
	if (renameControls.size() == 3)
		renameControls[1]->EventHandlers[L"Click"] = L"ExternalClick";
	auto staleEventResult = eventRenameCanvas.ExecuteCommand(
		std::make_unique<EventHandlerCommand>(
			&eventRenameCanvas,
			std::vector<DesignerEventHandlerDelta>{ staleEventDelta },
			std::vector<std::wstring>{}, L"", L"GuardedEventEdit"));
	const bool staleEventWasRejected = !staleEventResult
		&& eventRenameCanvas.GetUndoCommandCount() == staleHistoryCount
		&& renameControls.size() == 3
		&& renameControls[1]->EventHandlers[L"Click"]
			== L"ExternalClick";
	if (renameControls.size() == 3)
		renameControls[1]->EventHandlers[L"Click"] = L"HandleClick";

	DesignerModel::DesignDocument renamedEventDocument;
	std::wstring renamedEventError;
	const bool capturedRenamedEvents = eventRenameCanvas.BuildDesignDocument(
		renamedEventDocument, &renamedEventError);
	DesignerModel::DesignDocumentEventIndex renamedEventIndex;
	const bool indexedRenamedEvents = capturedRenamedEvents
		&& DesignerModel::DesignDocumentEventIndex::Build(
			renamedEventDocument, renamedEventIndex, &renamedEventError);
	DesignerModel::DesignDocument xmlRenamedEvents;
	DesignerModel::DesignDocumentEventIndex xmlRenamedEventIndex;
	const bool xmlRenamedEventRoundTrip = indexedRenamedEvents
		&& DesignerModel::DesignDocumentSerializer::FromXml(
			DesignerModel::DesignDocumentSerializer::ToXml(renamedEventDocument),
			xmlRenamedEvents, &renamedEventError)
		&& DesignerModel::DesignDocumentEventIndex::Build(
			xmlRenamedEvents, xmlRenamedEventIndex, &renamedEventError)
		&& xmlRenamedEventIndex.FindHandler(L"HandleClick")
		&& xmlRenamedEventIndex.FindHandler(L"HandleClick")
			->ReferenceIndices.size() == 2
		&& !xmlRenamedEventIndex.FindHandler(L"HandleSharedClick");
	DesignerModel::DesignDocument xamlRenamedEvents;
	DesignerModel::DesignDocumentEventIndex xamlRenamedEventIndex;
	const bool xamlRenamedEventRoundTrip = indexedRenamedEvents
		&& DesignerModel::XamlDocumentParser::FromXaml(
			DesignerModel::XamlDocumentSerializer::ToXaml(renamedEventDocument),
			xamlRenamedEvents, &renamedEventError)
		&& DesignerModel::DesignDocumentEventIndex::Build(
			xamlRenamedEvents, xamlRenamedEventIndex, &renamedEventError)
		&& xamlRenamedEventIndex.FindHandler(L"HandleClick")
		&& xamlRenamedEventIndex.FindHandler(L"HandleClick")
			->ReferenceIndices.size() == 2
		&& !xamlRenamedEventIndex.FindHandler(L"HandleSharedClick");
	const bool builtRenamedEventCodeInput = indexedRenamedEvents
		&& CodeGenerator::ValidateDocument(
			renamedEventDocument, &renamedEventError);
	CodeGenerator renamedEventGenerator(
		L"RenamedEventWindow", renamedEventDocument);
	const auto renamedEventHeader = builtRenamedEventCodeInput
		? renamedEventGenerator.GenerateHeader() : std::string{};
	const auto renamedEventCpp = builtRenamedEventCodeInput
		? renamedEventGenerator.GenerateCpp() : std::string{};
	const bool generatedRenamedEventCode = builtRenamedEventCodeInput
		&& renamedEventHeader.find("virtual void HandleClick(") != std::string::npos
		&& renamedEventHeader.find("HandleSharedClick") == std::string::npos
		&& renamedEventCpp.find("::HandleClick, this)") != std::string::npos
		&& renamedEventCpp.find("HandleSharedClick") == std::string::npos;
	const auto beforeConflictingRename = renamedEventDocument;
	auto conflictingRenameTransaction = eventRenameCanvas.RenameEventHandler(
		L"HandleClick", L"HandleWindowContentRendered", nullptr, &renameCommandError);
	DesignerModel::DesignDocument afterConflictingRename;
	const bool capturedAfterConflict = eventRenameCanvas.BuildDesignDocument(
		afterConflictingRename, &renamedEventError);
	AppendFailure(failures,
		renamedEventTransaction.HasChanges()
		&& renamedEventReferences == 2
		&& renamedLiveReferences
		&& undoEventRename.HasChanges()
		&& restoredOldEventReferences
		&& redoEventRename.HasChanges()
		&& renameUsedCompactDelta
		&& staleEventWasRejected
		&& indexedRenamedEvents
		&& renamedEventIndex.FindHandler(L"HandleClick")
		&& renamedEventIndex.FindHandler(L"HandleClick")
			->ReferenceIndices.size() == 2
		&& xmlRenamedEventRoundTrip
		&& xamlRenamedEventRoundTrip
		&& generatedRenamedEventCode
		&& !conflictingRenameTransaction
		&& !conflictingRenameTransaction.Error.empty()
		&& capturedAfterConflict
		&& afterConflictingRename == beforeConflictingRename,
		L"event handler rename: shared references, undo/redo, persistence, or conflict rollback failed"
		+ std::wstring(L" [renameState=")
		+ std::to_wstring(static_cast<int>(renamedEventTransaction.State))
		+ L", count=" + std::to_wstring(renamedEventReferences)
		+ L", live=" + (renamedLiveReferences ? L"1" : L"0")
		+ L", undo=" + (undoEventRename.HasChanges() ? L"1" : L"0")
		+ L", old=" + (restoredOldEventReferences ? L"1" : L"0")
		+ L", redo=" + (redoEventRename.HasChanges() ? L"1" : L"0")
		+ L", index=" + (indexedRenamedEvents ? L"1" : L"0")
		+ L", xml=" + (xmlRenamedEventRoundTrip ? L"1" : L"0")
		+ L", xaml=" + (xamlRenamedEventRoundTrip ? L"1" : L"0")
		+ L", codegen=" + (generatedRenamedEventCode ? L"1" : L"0")
		+ L", conflictState="
		+ std::to_wstring(static_cast<int>(conflictingRenameTransaction.State))
		+ L", restored=" + (capturedAfterConflict
			&& afterConflictingRename == beforeConflictingRename ? L"1" : L"0")
		+ L", error=" + renamedEventError + L"]");

	namespace fs = std::filesystem;
	const auto migrationDirectory = fs::temp_directory_path()
		/ (L"cui-handler-migration-"
			+ std::to_wstring(::GetCurrentProcessId())
			+ L"-" + std::to_wstring(::GetTickCount64()));
	fs::create_directories(migrationDirectory);
	const auto migrationBase = migrationDirectory / L"MigrationWindow";
	auto readMigrationText = [](const fs::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(stream),
			std::istreambuf_iterator<char>());
	};
	DesignerCanvas migrationCanvas(0, 0, 800, 640);
	migrationCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 140, 140 });
	const auto migrationControl = migrationCanvas.GetSelectedControl();
	if (migrationControl)
		migrationControl->EventHandlers[L"Click"] = L"HandleOriginal";
	DesignerModel::DesignCodeBehindModel migrationAssociation;
	migrationAssociation.ClassName = L"Acme::MigrationWindow";
	migrationAssociation.RelativeBasePath = L"MigrationWindow";
	std::wstring migrationError;
	const bool setMigrationAssociation = migrationCanvas.SetCodeBehind(
		migrationAssociation, &migrationError);
	DesignerModel::DesignDocument migrationDocument;
	const bool capturedMigrationDocument = setMigrationAssociation
		&& migrationCanvas.BuildDesignDocument(
			migrationDocument, &migrationError);
	DesignerModel::DesignCodeGenerationOptions migrationOptions;
	migrationOptions.OutputBasePath = migrationBase.wstring();
	migrationOptions.ClassName = migrationAssociation.ClassName;
	DesignerModel::DesignCodeGenerationResult migrationGenerated;
	const bool generatedMigrationFiles = capturedMigrationDocument
		&& DesignerModel::DesignCodeGenerationService::Generate(
			migrationDocument, L"", migrationOptions,
			&migrationGenerated, &migrationError);
	const fs::path migrationSource = migrationBase.wstring() + L".cpp";
	const fs::path migrationGeneratedSource =
		migrationBase.wstring() + L".g.cpp";
	const fs::path migrationUserHeader = migrationBase.wstring() + L".h";
	std::string originalMigrationSource = generatedMigrationFiles
		? readMigrationText(migrationSource) : std::string{};
	const auto unusedLine = originalMigrationSource.find("\t(void)e;");
	if (unusedLine != std::string::npos)
		originalMigrationSource.insert(unusedLine + std::string("\t(void)e;").size(),
			"\n\tint preservedBody = 7; (void)preservedBody;");
	const bool customizedMigrationBody = generatedMigrationFiles
		&& unusedLine != std::string::npos
		&& DesignerModel::AtomicFile::Write(
			migrationSource.wstring(), originalMigrationSource, &migrationError);

	DesignerEventHandlerCodeMigration migrationRequest;
	migrationRequest.OutputBasePath = migrationBase.wstring();
	migrationRequest.ClassName = migrationAssociation.ClassName;
	migrationRequest.UserCodePath = migrationSource.wstring();
	migrationRequest.ParameterList = "Control* sender, RoutedEventArgs& e";
	migrationRequest.OldName = L"HandleOriginal";
	migrationRequest.NewName = L"HandleRenamed";
	const auto migrationMemoryBefore =
		migrationCanvas.GetCommandHistoryMemoryUsage();
	size_t migratedReferenceCount = 0;
	auto migrationResult = customizedMigrationBody
		? migrationCanvas.RenameEventHandler(
			L"HandleOriginal", L"HandleRenamed", &migratedReferenceCount,
			&migrationError, &migrationRequest)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"migration setup failed");
	const auto migratedSource = generatedMigrationFiles
		? readMigrationText(migrationSource) : std::string{};
	const auto migratedGeneratedSource = generatedMigrationFiles
		? readMigrationText(migrationGeneratedSource) : std::string{};
	const bool migratedBodyAndCode = migrationResult.HasChanges()
		&& migratedReferenceCount == 1
		&& migrationControl
		&& migrationControl->EventHandlers[L"Click"] == L"HandleRenamed"
		&& migratedSource.find("::HandleRenamed(") != std::string::npos
		&& migratedSource.find("::HandleOriginal(") == std::string::npos
		&& migratedSource.find("preservedBody = 7") != std::string::npos
		&& migratedGeneratedSource.find("::HandleRenamed, this)")
			!= std::string::npos
		&& migrationCanvas.GetCommandHistoryMemoryUsage()
			- migrationMemoryBefore < 32 * 1024;

	const auto undoMigration = migrationCanvas.UndoCommand();
	const auto undoMigrationSource = readMigrationText(migrationSource);
	const auto undoMigrationGeneratedSource =
		readMigrationText(migrationGeneratedSource);
	const bool undidBodyAndCode = undoMigration.HasChanges()
		&& migrationControl
		&& migrationControl->EventHandlers[L"Click"] == L"HandleOriginal"
		&& undoMigrationSource.find("::HandleOriginal(") != std::string::npos
		&& undoMigrationSource.find("::HandleRenamed(") == std::string::npos
		&& undoMigrationSource.find("preservedBody = 7") != std::string::npos
		&& undoMigrationGeneratedSource.find("::HandleOriginal, this)")
			!= std::string::npos;
	const auto redoMigration = migrationCanvas.RedoCommand();
	const auto redoMigrationSource = readMigrationText(migrationSource);
	const bool redidBodyAndCode = redoMigration.HasChanges()
		&& migrationControl
		&& migrationControl->EventHandlers[L"Click"] == L"HandleRenamed"
		&& redoMigrationSource.find("::HandleRenamed(") != std::string::npos;

	auto externallyChangedMigrationSource = redoMigrationSource;
	const auto externalName = externallyChangedMigrationSource.find(
		"::HandleRenamed(");
	if (externalName != std::string::npos)
		externallyChangedMigrationSource.replace(
			externalName, std::string("::HandleRenamed(").size(),
			"::ExternallyChanged(");
	const bool wroteExternalMigrationConflict = externalName != std::string::npos
		&& DesignerModel::AtomicFile::Write(
			migrationSource.wstring(), externallyChangedMigrationSource,
			&migrationError);
	const auto failedUndoCount = migrationCanvas.GetUndoCommandCount();
	const auto rejectedMigrationUndo = migrationCanvas.UndoCommand();
	const bool rejectedExternalMigration = wroteExternalMigrationConflict
		&& !rejectedMigrationUndo
		&& migrationCanvas.GetUndoCommandCount() == failedUndoCount
		&& migrationControl
		&& migrationControl->EventHandlers[L"Click"] == L"HandleRenamed"
		&& readMigrationText(migrationSource) == externallyChangedMigrationSource;
	const bool restoredRedoSource = DesignerModel::AtomicFile::Write(
		migrationSource.wstring(), redoMigrationSource, &migrationError);
	const auto retriedMigrationUndo = restoredRedoSource
		? migrationCanvas.UndoCommand()
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed, L"restore failed");
	const bool retriedExternalMigration = retriedMigrationUndo.HasChanges()
		&& migrationControl
		&& migrationControl->EventHandlers[L"Click"] == L"HandleOriginal"
		&& readMigrationText(migrationSource).find("::HandleOriginal(")
			!= std::string::npos;

	const auto validMigrationHeader = readMigrationText(migrationUserHeader);
	auto invalidMigrationHeader = validMigrationHeader;
	const auto migrationIdentity = invalidMigrationHeader.find(
		"Acme::MigrationWindow");
	if (migrationIdentity != std::string::npos)
		invalidMigrationHeader.replace(migrationIdentity,
			std::string("Acme::MigrationWindow").size(),
			"Other::MigrationWindow");
	const bool wroteInvalidMigrationHeader =
		migrationIdentity != std::string::npos
		&& DesignerModel::AtomicFile::Write(
			migrationUserHeader.wstring(), invalidMigrationHeader,
			&migrationError);
	const auto beforeFailedMigrationSource = readMigrationText(migrationSource);
	const auto beforeFailedGeneratedSource =
		readMigrationText(migrationGeneratedSource);
	DesignerEventHandlerCodeMigration failingMigrationRequest = migrationRequest;
	failingMigrationRequest.NewName = L"HandleRollback";
	const auto failedMigrationHistoryCount =
		migrationCanvas.GetUndoCommandCount();
	auto failedMigration = wroteInvalidMigrationHeader
		? migrationCanvas.RenameEventHandler(
			L"HandleOriginal", L"HandleRollback", nullptr,
			&migrationError, &failingMigrationRequest)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed, L"setup failed");
	const bool rolledBackFailedMigration = wroteInvalidMigrationHeader
		&& !failedMigration && failedMigration.DocumentRestored
		&& migrationCanvas.GetUndoCommandCount() == failedMigrationHistoryCount
		&& migrationControl
		&& migrationControl->EventHandlers[L"Click"] == L"HandleOriginal"
		&& readMigrationText(migrationSource) == beforeFailedMigrationSource
		&& readMigrationText(migrationGeneratedSource)
			== beforeFailedGeneratedSource
		&& readMigrationText(migrationUserHeader) == invalidMigrationHeader;
	const bool restoredValidMigrationHeader = DesignerModel::AtomicFile::Write(
		migrationUserHeader.wstring(), validMigrationHeader, &migrationError);
	auto inlineMigrationHeader = validMigrationHeader;
	const auto inlineMigrationInclude = inlineMigrationHeader.find(
		"#include \"MigrationWindow.handlers.g.inc\"");
	if (inlineMigrationInclude != std::string::npos)
		inlineMigrationHeader.insert(inlineMigrationInclude,
			"\tvoid HandleOriginal(Control* sender, RoutedEventArgs& e)\n"
			"\t{\n"
			"\t\t(void)sender; (void)e;\n"
			"\t\tint inlineBody = 11; (void)inlineBody;\n"
			"\t}\n");
	auto inlineMigrationSource = beforeFailedMigrationSource;
	const auto inlineSourceBegin = inlineMigrationSource.find(
		"void Acme::MigrationWindow::HandleOriginal(");
	const auto inlineSourceEnd = inlineSourceBegin == std::string::npos
		? std::string::npos
		: inlineMigrationSource.find("\n}\n", inlineSourceBegin);
	if (inlineSourceEnd != std::string::npos)
		inlineMigrationSource.erase(inlineSourceBegin,
			inlineSourceEnd + 3 - inlineSourceBegin);
	const bool wroteInlineMigrationFiles = restoredValidMigrationHeader
		&& inlineMigrationInclude != std::string::npos
		&& inlineSourceBegin != std::string::npos
		&& inlineSourceEnd != std::string::npos
		&& DesignerModel::AtomicFile::Write(
			migrationUserHeader.wstring(), inlineMigrationHeader,
			&migrationError)
		&& DesignerModel::AtomicFile::Write(
			migrationSource.wstring(), inlineMigrationSource,
			&migrationError);
	const bool generatedInlineMigration = wroteInlineMigrationFiles
		&& DesignerModel::DesignCodeGenerationService::Generate(
			migrationDocument, L"", migrationOptions,
			&migrationGenerated, &migrationError);
	DesignerEventHandlerCodeMigration inlineMigrationRequest = migrationRequest;
	inlineMigrationRequest.UserCodePath = migrationUserHeader.wstring();
	inlineMigrationRequest.NewName = L"HandleInlineRenamed";
	size_t inlineMigrationReferenceCount = 0;
	const auto inlineMigrationResult = generatedInlineMigration
		? migrationCanvas.RenameEventHandler(
			L"HandleOriginal", L"HandleInlineRenamed",
			&inlineMigrationReferenceCount, &migrationError,
			&inlineMigrationRequest)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"inline migration setup failed");
	const auto renamedInlineHeader = readMigrationText(migrationUserHeader);
	const auto renamedInlineSource = readMigrationText(migrationSource);
	const auto renamedInlineGenerated = readMigrationText(
		migrationGeneratedSource);
	const auto renamedInlineDeclarations = readMigrationText(
		fs::path(migrationBase.wstring() + L".handlers.g.inc"));
	const bool migratedInlineBody = inlineMigrationResult.HasChanges()
		&& inlineMigrationReferenceCount == 1
		&& migrationControl
		&& migrationControl->EventHandlers[L"Click"]
			== L"HandleInlineRenamed"
		&& renamedInlineHeader.find("void HandleInlineRenamed(")
			!= std::string::npos
		&& renamedInlineHeader.find("inlineBody = 11")
			!= std::string::npos
		&& renamedInlineHeader.find("void HandleOriginal(")
			== std::string::npos
		&& renamedInlineSource.find("::HandleOriginal(")
			== std::string::npos
		&& renamedInlineSource.find("::HandleInlineRenamed(")
			== std::string::npos
		&& renamedInlineGenerated.find("::HandleInlineRenamed, this)")
			!= std::string::npos
		&& renamedInlineDeclarations.find("HandleInlineRenamed")
			== std::string::npos;
	const auto undoInlineMigration = migrationCanvas.UndoCommand();
	const auto undoInlineHeader = readMigrationText(migrationUserHeader);
	const bool undidInlineBody = undoInlineMigration.HasChanges()
		&& migrationControl
		&& migrationControl->EventHandlers[L"Click"]
			== L"HandleOriginal"
		&& undoInlineHeader.find("void HandleOriginal(")
			!= std::string::npos
		&& undoInlineHeader.find("void HandleInlineRenamed(")
			== std::string::npos
		&& undoInlineHeader.find("inlineBody = 11") != std::string::npos;
	const auto redoInlineMigration = migrationCanvas.RedoCommand();
	const auto redoInlineHeader = readMigrationText(migrationUserHeader);
	const bool redidInlineBody = redoInlineMigration.HasChanges()
		&& migrationControl
		&& migrationControl->EventHandlers[L"Click"]
			== L"HandleInlineRenamed"
		&& redoInlineHeader.find("void HandleInlineRenamed(")
			!= std::string::npos
		&& redoInlineHeader.find("inlineBody = 11") != std::string::npos;
	fs::remove_all(migrationDirectory);
	AppendFailure(failures,
		migratedBodyAndCode && undidBodyAndCode && redidBodyAndCode
		&& rejectedExternalMigration && retriedExternalMigration
		&& rolledBackFailedMigration && migratedInlineBody
		&& undidInlineBody && redidInlineBody,
		L"event handler code migration: body, generation, Undo/Redo, conflict, or rollback failed"
		+ std::wstring(L" [generated=")
		+ (generatedMigrationFiles ? L"1" : L"0")
		+ L", customized=" + (customizedMigrationBody ? L"1" : L"0")
		+ L", migrated=" + (migratedBodyAndCode ? L"1" : L"0")
		+ L", undo=" + (undidBodyAndCode ? L"1" : L"0")
		+ L", redo=" + (redidBodyAndCode ? L"1" : L"0")
		+ L", conflict=" + (rejectedExternalMigration ? L"1" : L"0")
		+ L", retry=" + (retriedExternalMigration ? L"1" : L"0")
		+ L", rollback=" + (rolledBackFailedMigration ? L"1" : L"0")
		+ L", inline=" + (migratedInlineBody ? L"1" : L"0")
		+ L", inlineUndo=" + (undidInlineBody ? L"1" : L"0")
		+ L", inlineRedo=" + (redidInlineBody ? L"1" : L"0")
		+ L", error=" + migrationError + L"]");
	std::wstring preservedCategory;
	if (nativeFalseGrid)
	{
		for (const auto& item : nativeFalseGrid->Items)
		{
			if (!item.Category.empty())
			{
				preservedCategory = item.Category;
				break;
			}
		}
		if (!preservedCategory.empty())
			nativeFalseGrid->CollapseCategory(preservedCategory, true);
		nativeFalseGrid->SetScrollOffset(120.0f);
	}
	const float preservedScroll = nativeFalseGrid
		? nativeFalseGrid->ScrollYOffset : 0.0f;
	const bool statePreservingEdit = nativeFalseGrid
		&& nativeCheckedIndex >= 0
		&& nativeFalseGrid->SetValue(nativeCheckedIndex, L"True");
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	AppendFailure(failures,
		statePreservingEdit
		&& nativeFalseGrid
		&& !preservedCategory.empty()
		&& nativeFalseGrid->IsCategoryCollapsed(preservedCategory)
		&& preservedScroll > 0.0f
		&& std::fabs(nativeFalseGrid->ScrollYOffset - preservedScroll) < 0.01f,
		L"native property grid: value reload lost category collapse or scroll state");

	DesignerCanvas nativeSliderCanvas(0, 0, 800, 640);
	nativeSliderCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_MediaPlayer, POINT{ 170, 160 });
	PropertyGrid nativeSliderGrid(0, 0, 360, 620);
	nativeSliderGrid.SetDesignerCanvas(&nativeSliderCanvas);
	ReloadCurrentSelection(nativeSliderGrid, nativeSliderCanvas);
	// RangeBase.Value is bounded by the live Minimum/Maximum dependency
	// properties, not by fixed design metadata. Exercise the native bounded
	// editor with MediaPlayer.Volume, whose metadata owns a fixed 0..1 interval.
	const auto* boundedRow = DesignerPropertyRowCatalog::Find(
		nativeSliderGrid.GetPresentedPropertyRows(), L"Volume");
	auto* nativeSliderView = nativeSliderGrid.GetNativePropertyGrid();
	int nativeSliderIndex = -1;
	if (boundedRow && nativeSliderView)
	{
		for (int index = 0;
			index < static_cast<int>(nativeSliderView->Items.size()); ++index)
		{
			const auto& item = nativeSliderView->Items[static_cast<size_t>(index)];
			if (item.Name.rfind(boundedRow->DisplayName, 0) == 0)
			{
				nativeSliderIndex = index;
				break;
			}
		}
	}
	AppendFailure(failures,
		nativeSliderIndex >= 0
		&& nativeSliderView->Items[static_cast<size_t>(nativeSliderIndex)].ValueType
			== PropertyGridValueType::Slider,
		L"native property grid: bounded float metadata did not use Slider");
	if (nativeSliderIndex >= 0)
	{
		const size_t commandCountBeforeSlider =
			nativeSliderCanvas.GetUndoCommandCount();
		nativeSliderView->EnsureVisible(nativeSliderIndex);
		int sliderTop = -1;
		int sliderBottom = -1;
		for (int y = 0; y < static_cast<int>(
			std::ceil(nativeSliderView->ActualHeight)); ++y)
		{
			if (nativeSliderView->HitTestItem(230, y) == nativeSliderIndex)
			{
				if (sliderTop < 0) sliderTop = y;
				sliderBottom = y;
			}
		}
		const int sliderY = sliderTop >= 0
			? (sliderTop + sliderBottom) / 2 : -1;
		if (sliderY >= 0)
		{
			(void)cui::framework::InputAccess::DispatchInput(*nativeSliderView, PointerInput(
				InputReportKind::PointerDown, MouseButton::Left,
				215, sliderY, MouseButton::Left));
			(void)cui::framework::InputAccess::DispatchInput(*nativeSliderView, PointerInput(
				InputReportKind::PointerMove, MouseButton::None,
				285, sliderY, MouseButton::Left));
			(void)cui::framework::InputAccess::DispatchInput(*nativeSliderView, PointerInput(
				InputReportKind::PointerUp, MouseButton::Left,
				285, sliderY));
		}
		AppendFailure(failures,
			sliderY >= 0
			&& nativeSliderCanvas.GetUndoCommandCount()
				== commandCountBeforeSlider + 1
			&& !nativeSliderGrid.HasPropertyEditError(),
			L"native property grid: Slider drag was not committed as one command"
			+ std::wstring(L" (y=") + std::to_wstring(sliderY)
			+ L", before=" + std::to_wstring(commandCountBeforeSlider)
			+ L", after=" + std::to_wstring(
				nativeSliderCanvas.GetUndoCommandCount())
			+ L", error=" + nativeSliderGrid.GetPropertyEditErrorMessage() + L")");
	}

	auto captureGridDefinitions = [](
		DesignerCanvas& targetCanvas,
		const std::wstring& targetName,
		DesignerStructureSnapshot& snapshot,
		std::wstring* error = nullptr)
	{
		const auto control = FindControl(targetCanvas, targetName);
		return control && control->ControlInstance
			&& dynamic_cast<Grid*>(control->ControlInstance)
			&& DesignerStructureEdit::Capture(
				*control, DesignerCustomEditorKind::GridDefinitions,
				snapshot, error);
	};
	auto setCommittedGridDefinitions = [](Grid& grid)
	{
		grid.ClearRows();
		grid.ClearColumns();
		grid.AddRow(GridLength::Pixels(40.0f), 5.0f, 200.0f);
		grid.AddRow(GridLength::Star(2.0f));
		grid.AddColumn(GridLength::Star(3.0f), 10.0f, 300.0f);
	};

	DesignerCanvas structuralCanvas(0, 0, 800, 640);
	structuralCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Grid, POINT{ 140, 140 });
	AppendFailure(failures, structuralCanvas.GetAllControls().size() == 2,
		L"structure transaction: setup control missing");
	if (structuralCanvas.GetAllControls().size() == 2)
	{
		const auto structuralName =
			structuralCanvas.GetAllControls()[1]->Name;
		auto getGrid = [&structuralCanvas, &structuralName]() -> Grid*
		{
			auto control = FindControl(structuralCanvas, structuralName);
			return control && control->ControlInstance
				? dynamic_cast<Grid*>(control->ControlInstance) : nullptr;
		};
		auto* grid = getGrid();
		AppendFailure(failures, grid != nullptr,
			L"structure transaction: target is not Grid");
		if (grid)
		{
			DesignerStructureSnapshot originalDefinitions;
			DesignerStructureSnapshot committedDefinitions;
			DesignerStructureSnapshot currentDefinitions;
			std::wstring definitionError;
			const bool capturedOriginalDefinitions = captureGridDefinitions(
				structuralCanvas, structuralName,
				originalDefinitions, &definitionError);
			auto structureBegin = structuralCanvas.BeginDocumentEditTransaction(
				L"SelfTest:GridDefinitions");
			AppendFailure(failures,
				capturedOriginalDefinitions && structureBegin.State
					== DesignerDocumentTransactionState::Begun,
				L"structure transaction: could not capture before document");
			auto nestedBegin = structuralCanvas.BeginDocumentEditTransaction(
				L"SelfTest:Nested");
			AppendFailure(failures,
				nestedBegin.State
					== DesignerDocumentTransactionState::Rejected,
				L"structure transaction: nested transaction was accepted");
			setCommittedGridDefinitions(*grid);
			const bool capturedCommittedDefinitions = captureGridDefinitions(
				structuralCanvas, structuralName,
				committedDefinitions, &definitionError);
			auto structureCommit =
				structuralCanvas.CommitDocumentEditTransaction();
			AppendFailure(failures,
				capturedCommittedDefinitions && structureCommit.State
					== DesignerDocumentTransactionState::Committed,
				L"structure transaction: commit failed");
			AppendFailure(failures,
				captureGridDefinitions(
					structuralCanvas, structuralName,
					currentDefinitions, &definitionError)
				&& currentDefinitions == committedDefinitions,
				L"structure transaction: committed values differ");

			AppendFailure(failures, structuralCanvas.UndoCommand(),
				L"structure transaction: undo unavailable");
			AppendFailure(failures,
				captureGridDefinitions(
					structuralCanvas, structuralName,
					currentDefinitions, &definitionError)
				&& currentDefinitions == originalDefinitions,
				L"structure transaction: undo did not restore collection");
			AppendFailure(failures, structuralCanvas.RedoCommand(),
				L"structure transaction: redo unavailable");
			AppendFailure(failures,
				captureGridDefinitions(
					structuralCanvas, structuralName,
					currentDefinitions, &definitionError)
				&& currentDefinitions == committedDefinitions,
				L"structure transaction: redo did not restore collection");

			auto rollbackBegin = structuralCanvas.BeginDocumentEditTransaction(
				L"SelfTest:Rollback");
			AppendFailure(failures,
				rollbackBegin.State
					== DesignerDocumentTransactionState::Begun,
				L"structure transaction: rollback snapshot unavailable");
			if (auto* current = getGrid())
			{
				current->ClearRows();
				current->ClearColumns();
				current->AddRow(GridLength::Auto());
				current->AddColumn(GridLength::Pixels(17.0f));
			}
			auto rollbackResult =
				structuralCanvas.RollbackDocumentEditTransaction();
			AppendFailure(failures,
				rollbackResult.State
					== DesignerDocumentTransactionState::RolledBack,
				L"structure transaction: explicit rollback failed");
			AppendFailure(failures,
				captureGridDefinitions(
					structuralCanvas, structuralName,
					currentDefinitions, &definitionError)
				&& currentDefinitions == committedDefinitions,
				L"structure transaction: rollback retained transient values");
		}
	}
	DesignerCanvas emptyTransactionCanvas(0, 0, 800, 640);
	emptyTransactionCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Grid, POINT{ 140, 140 });
	const auto emptyGridName = emptyTransactionCanvas.GetSelectedControl()
		? emptyTransactionCanvas.GetSelectedControl()->Name : std::wstring{};
	auto getEmptyGrid = [&emptyTransactionCanvas,
		&emptyGridName]() -> Grid*
	{
		auto control = FindControl(emptyTransactionCanvas, emptyGridName);
		return control && control->ControlInstance
			? dynamic_cast<Grid*>(control->ControlInstance) : nullptr;
	};
	DesignerStructureSnapshot emptyOriginalDefinitions;
	DesignerStructureSnapshot emptyCurrentDefinitions;
	std::wstring emptyDefinitionError;
	const bool capturedEmptyOriginal = captureGridDefinitions(
		emptyTransactionCanvas, emptyGridName,
		emptyOriginalDefinitions, &emptyDefinitionError);
	auto noChangeBegin = emptyTransactionCanvas.BeginDocumentEditTransaction(
		L"SelfTest:NoChange");
	auto noChangeCommit = emptyTransactionCanvas.CommitDocumentEditTransaction();
	AppendFailure(failures,
		capturedEmptyOriginal
		&& noChangeBegin.State == DesignerDocumentTransactionState::Begun
		&& noChangeCommit.State
			== DesignerDocumentTransactionState::Unchanged,
		L"structure transaction: no-change commit failed");
	AppendFailure(failures,
		IsUnchanged(emptyTransactionCanvas.UndoCommand()),
		L"structure transaction: no-change commit entered history");
	auto cancelBegin = emptyTransactionCanvas.BeginDocumentEditTransaction(
		L"SelfTest:Cancel");
	AppendFailure(failures,
		cancelBegin.State == DesignerDocumentTransactionState::Begun,
		L"structure transaction: cancel snapshot unavailable");
	auto cancelResult = emptyTransactionCanvas.CancelDocumentEditTransaction();
	AppendFailure(failures,
		cancelResult.State == DesignerDocumentTransactionState::Canceled,
		L"structure transaction: unchanged cancel state was not reported");
	AppendFailure(failures,
		IsUnchanged(emptyTransactionCanvas.UndoCommand()),
		L"structure transaction: canceled edit entered history");

	(void)emptyTransactionCanvas.BeginDocumentEditTransaction(
		L"SelfTest:CancelMutation");
	if (auto* current = getEmptyGrid())
		current->AddRow(GridLength::Pixels(11.0f));
	auto cancelMutation =
		emptyTransactionCanvas.CancelDocumentEditTransaction();
	AppendFailure(failures,
		cancelMutation.State
			== DesignerDocumentTransactionState::RolledBack
		&& captureGridDefinitions(
			emptyTransactionCanvas, emptyGridName,
			emptyCurrentDefinitions, &emptyDefinitionError)
		&& emptyCurrentDefinitions == emptyOriginalDefinitions,
		L"structure transaction: cancel did not restore leaked mutation");
	AppendFailure(failures,
		IsUnchanged(emptyTransactionCanvas.UndoCommand()),
		L"structure transaction: restored cancel entered history");

	auto abortedTransaction =
		emptyTransactionCanvas.ExecuteDocumentEditTransaction(
			L"SelfTest:Abort",
			[&getEmptyGrid](std::wstring& error)
			{
				if (auto* current = getEmptyGrid())
					current->AddColumn(GridLength::Pixels(19.0f));
				error = L"expected rejection";
				return false;
			});
	AppendFailure(failures,
		abortedTransaction.State
			== DesignerDocumentTransactionState::Aborted
		&& abortedTransaction.DocumentRestored
		&& captureGridDefinitions(
			emptyTransactionCanvas, emptyGridName,
			emptyCurrentDefinitions, &emptyDefinitionError)
		&& emptyCurrentDefinitions == emptyOriginalDefinitions,
		L"document transaction: rejected operation was not restored");
	AppendFailure(failures,
		IsUnchanged(emptyTransactionCanvas.UndoCommand()),
		L"document transaction: rejected operation entered history");

	auto throwingTransaction =
		emptyTransactionCanvas.ExecuteDocumentEditTransaction(
			L"SelfTest:Exception",
			[&getEmptyGrid](std::wstring&) -> bool
			{
				if (auto* current = getEmptyGrid())
					current->AddRow(GridLength::Star(4.0f));
				throw 1;
			});
	AppendFailure(failures,
		throwingTransaction.State
			== DesignerDocumentTransactionState::Failed
		&& throwingTransaction.DocumentRestored
		&& captureGridDefinitions(
			emptyTransactionCanvas, emptyGridName,
			emptyCurrentDefinitions, &emptyDefinitionError)
		&& emptyCurrentDefinitions == emptyOriginalDefinitions,
		L"document transaction: exception was not restored");
	AppendFailure(failures,
		IsUnchanged(emptyTransactionCanvas.UndoCommand()),
		L"document transaction: exception entered history");

	auto executedTransaction =
		emptyTransactionCanvas.ExecuteDocumentEditTransaction(
			L"SelfTest:Execute",
			[&getEmptyGrid](std::wstring& error)
			{
				auto* current = getEmptyGrid();
				if (!current)
				{
					error = L"Grid unavailable";
					return false;
				}
				current->ClearRows();
				current->ClearColumns();
				current->AddRow(GridLength::Pixels(77.0f));
				current->AddColumn(GridLength::Star(2.0f));
				return true;
			});
	AppendFailure(failures,
		executedTransaction.State
			== DesignerDocumentTransactionState::Committed
		&& getEmptyGrid()
		&& getEmptyGrid()->GetRows().size() == 1
		&& getEmptyGrid()->GetRows().front().Height.IsPixel()
		&& getEmptyGrid()->GetRows().front().Height.Value == 77.0f
		&& getEmptyGrid()->GetColumns().size() == 1
		&& getEmptyGrid()->GetColumns().front().Width.IsStar(),
		L"document transaction: execute did not commit");
	AppendFailure(failures, emptyTransactionCanvas.UndoCommand()
		&& captureGridDefinitions(
			emptyTransactionCanvas, emptyGridName,
			emptyCurrentDefinitions, &emptyDefinitionError)
		&& emptyCurrentDefinitions == emptyOriginalDefinitions,
		L"document transaction: execute undo did not restore state");

	DesignerCanvas codeBehindCanvas(0, 0, 800, 640);
	DesignerModel::DesignCodeBehindModel codeBehindAssociation;
	codeBehindAssociation.ClassName = L"PersistentWindow";
	codeBehindAssociation.RelativeBasePath = L"generated/PersistentWindow";
	auto codeBehindTransaction =
		codeBehindCanvas.ExecuteDocumentEditTransaction(
			L"SelfTest:CodeBehind",
			[&](std::wstring& error)
			{
				return codeBehindCanvas.SetCodeBehind(
					codeBehindAssociation, &error);
			});
	DesignerModel::DesignDocument capturedCodeBehind;
	std::wstring codeBehindError;
	const bool capturedAssociatedCodeBehind =
		codeBehindCanvas.BuildDesignDocument(
			capturedCodeBehind, &codeBehindError);
	auto undoCodeBehind = codeBehindCanvas.UndoCommand();
	const bool clearedAssociatedCodeBehind =
		codeBehindCanvas.GetCodeBehind().Empty();
	auto redoCodeBehind = codeBehindCanvas.RedoCommand();
	DesignerModel::DesignCodeBehindModel invalidCodeBehind;
	invalidCodeBehind.ClassName = L"PersistentWindow";
	invalidCodeBehind.RelativeBasePath = L"C:/outside/PersistentWindow";
	const bool rejectedInvalidCodeBehind =
		!codeBehindCanvas.SetCodeBehind(
			invalidCodeBehind, &codeBehindError);
	AppendFailure(failures,
		codeBehindTransaction.HasChanges()
		&& capturedAssociatedCodeBehind
		&& capturedCodeBehind.CodeBehind == codeBehindAssociation
		&& undoCodeBehind.HasChanges()
		&& clearedAssociatedCodeBehind
		&& redoCodeBehind.HasChanges()
		&& codeBehindCanvas.GetCodeBehind() == codeBehindAssociation
		&& rejectedInvalidCodeBehind
		&& codeBehindCanvas.GetCodeBehind() == codeBehindAssociation,
		L"document transaction: code-behind association did not validate or round-trip through undo/redo");

	const std::wstring exportDesignPath =
		L"C:\\CuiDesignerSelfTest\\document\\PersistentWindow.cui.xaml";
	const std::wstring preservedExportBase =
		L"C:\\CuiDesignerSelfTest\\document\\generated\\PersistentWindow";
	CodeBehindExportDialog preservedExportDialog(
		codeBehindAssociation, L"PersistentWindow",
		preservedExportBase, exportDesignPath);
	CodeBehindExportDialog migratedExportDialog(
		codeBehindAssociation, L"Acme.Views.RenamedWindow",
		L"C:\\CuiDesignerSelfTest\\document\\generated\\RenamedWindow",
		exportDesignPath);
	CodeBehindExportDialog invalidExportDialog(
		codeBehindAssociation, L"bad::class",
		preservedExportBase, exportDesignPath);
	AppendFailure(failures,
		preservedExportDialog.CanApply()
		&& !preservedExportDialog.Plan.CreatesAssociation
		&& !preservedExportDialog.Plan.MigratesClass
		&& !preservedExportDialog.Plan.ChangesRelativeOutput
		&& migratedExportDialog.CanApply()
		&& migratedExportDialog.Plan.MigratesClass
		&& migratedExportDialog.Plan.ChangesRelativeOutput
		&& migratedExportDialog.ValidationMessage().find(L"迁移")
			!= std::wstring::npos
		&& !invalidExportDialog.CanApply()
		&& invalidExportDialog.Plan.Association.Empty()
		&& !invalidExportDialog.ValidationMessage().empty(),
		L"code-behind export dialog: preserve, migrate, or invalid class state was not projected");

	DesignerCanvas coalescedPropertyCanvas(0, 0, 800, 640);
	coalescedPropertyCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 160, 160 });
	PropertyGrid coalescedPropertyGrid(0, 0, 360, 620);
	coalescedPropertyGrid.SetDesignerCanvas(&coalescedPropertyCanvas);
	ReloadCurrentSelection(
		coalescedPropertyGrid, coalescedPropertyCanvas);
	const auto coalescedPropertyName =
		coalescedPropertyCanvas.GetSelectedControl()
			? coalescedPropertyCanvas.GetSelectedControl()->Name
			: std::wstring{};
	const auto originalCoalescedAutomationName =
		ControlAutomationName(coalescedPropertyCanvas, coalescedPropertyName);
	const auto coalescedPropertyIdentity =
		coalescedPropertyCanvas.GetSelectedControl();
	const auto firstCoalescedProperty =
		coalescedPropertyGrid.ApplyPropertyValue(
			L"AutomationProperties.Name", L"A");
	const auto secondCoalescedProperty =
		coalescedPropertyGrid.ApplyPropertyValue(
			L"AutomationProperties.Name", L"AB");
	AppendFailure(failures,
		firstCoalescedProperty.Succeeded
		&& secondCoalescedProperty.Succeeded
		&& coalescedPropertyCanvas.GetUndoCommandCount() == 1
		&& ControlAutomationName(coalescedPropertyCanvas, coalescedPropertyName)
			== L"AB"
		&& coalescedPropertyCanvas.UndoCommand().HasChanges()
		&& ControlAutomationName(coalescedPropertyCanvas, coalescedPropertyName)
			== originalCoalescedAutomationName
		&& coalescedPropertyCanvas.RedoCommand().HasChanges()
		&& ControlAutomationName(coalescedPropertyCanvas, coalescedPropertyName)
			== L"AB"
		&& FindControl(coalescedPropertyCanvas, coalescedPropertyName)
			== coalescedPropertyIdentity
		&& coalescedPropertyCanvas.GetCommandHistoryMemoryUsage() < 32768,
		L"history coalescing: property edits did not merge end to end");
	ReloadCurrentSelection(
		coalescedPropertyGrid, coalescedPropertyCanvas);
	(void)coalescedPropertyCanvas.MarkDocumentSaved();
	const auto afterSaveProperty =
		coalescedPropertyGrid.ApplyPropertyValue(
			L"AutomationProperties.Name", L"ABC");
	AppendFailure(failures,
		afterSaveProperty.Succeeded
		&& coalescedPropertyCanvas.GetUndoCommandCount() == 2
		&& coalescedPropertyCanvas.IsDocumentDirty()
		&& coalescedPropertyCanvas.UndoCommand().HasChanges()
		&& !coalescedPropertyCanvas.IsDocumentDirty()
		&& ControlAutomationName(coalescedPropertyCanvas, coalescedPropertyName)
			== L"AB",
		L"history coalescing: merge crossed an exact save point");
	const auto identityBeforeSubtreeDelta =
		FindControl(coalescedPropertyCanvas, coalescedPropertyName);
	const auto addAfterPropertyDelta = coalescedPropertyCanvas.AdoptVisualChildToCanvas(
		UIClass::UI_Label, POINT{ 360, 240 });
	const auto undoSnapshotAfterDelta = coalescedPropertyCanvas.UndoCommand();
	const auto identityAfterSubtreeDelta =
		FindControl(coalescedPropertyCanvas, coalescedPropertyName);
	AppendFailure(failures,
		addAfterPropertyDelta.HasChanges()
		&& undoSnapshotAfterDelta.HasChanges()
		&& identityAfterSubtreeDelta
		&& identityAfterSubtreeDelta == identityBeforeSubtreeDelta
		&& coalescedPropertyCanvas.UndoCommand().HasChanges()
		&& ControlAutomationName(coalescedPropertyCanvas, coalescedPropertyName)
			== originalCoalescedAutomationName
		&& coalescedPropertyCanvas.RedoCommand().HasChanges()
		&& ControlAutomationName(coalescedPropertyCanvas, coalescedPropertyName)
			== L"AB",
		L"property delta: target resolution failed after Add subtree undo");
	coalescedPropertyCanvas.SetCommandHistoryMemoryLimit(1);
	AppendFailure(failures,
		coalescedPropertyCanvas.GetCommandHistoryMemoryLimit() == 1
		&& coalescedPropertyCanvas.GetUndoCommandCount() == 1
		&& coalescedPropertyCanvas.GetRedoCommandCount() == 0
		&& coalescedPropertyCanvas.GetCommandHistoryMemoryUsage() > 1,
		L"history budget: did not retain exactly one nearest oversized command");

	DesignerCanvas renameCanvas(0, 0, 800, 640);
	renameCanvas.AdoptVisualChildToCanvasCore(UIClass::UI_Button, POINT{ 160, 160 });
	PropertyGrid renameGrid(0, 0, 360, 620);
	renameGrid.SetDesignerCanvas(&renameCanvas);
	ReloadCurrentSelection(renameGrid, renameCanvas);
	const auto renameIdentity = renameCanvas.GetSelectedControl();
	const auto originalName = renameIdentity ? renameIdentity->Name : std::wstring{};
	const int originalStableId = renameIdentity ? renameIdentity->StableId : 0;
	const auto renameResult = renameGrid.ApplyPropertyValue(
		L"Name", L"RenamedButton");
	const bool renamedInitially = renameResult.Succeeded
		&& renameCanvas.GetSelectedControl()
		&& renameCanvas.GetSelectedControl()->Name == L"RenamedButton";
	const auto renameUndo = renamedInitially
		? renameCanvas.UndoCommand() : DesignerDocumentTransactionResult{};
	const bool renameUndone = renameUndo.HasChanges()
		&& renameCanvas.GetSelectedControl()
		&& renameCanvas.GetSelectedControl()->Name == originalName
		&& renameCanvas.GetSelectedControl() == renameIdentity;
	const auto renameRedo = renameUndone
		? renameCanvas.RedoCommand() : DesignerDocumentTransactionResult{};
	const bool renameRedone = renameRedo.HasChanges()
		&& renameCanvas.GetSelectedControl()
		&& renameCanvas.GetSelectedControl()->Name == L"RenamedButton"
		&& renameCanvas.GetSelectedControl() == renameIdentity
		&& renameCanvas.GetSelectedControl()->StableId == originalStableId
		&& renameCanvas.GetSelectedControl()->ControlInstance->GetDesignId()
			== originalStableId;
	AppendFailure(failures,
		renamedInitially && renameUndone && renameRedone,
		std::wstring(L"property delta: Name undo/redo lost identity or selection [apply=")
			+ (renamedInitially ? L"1" : L"0") + L", undo="
			+ (renameUndone ? L"1" : L"0") + L", redo="
			+ (renameRedone ? L"1" : L"0") + L", error="
			+ renameResult.Error + L", undoChanged="
			+ (renameUndo.HasChanges() ? L"1" : L"0") + L", selected="
			+ (renameCanvas.GetSelectedControl()
				? renameCanvas.GetSelectedControl()->Name : L"<none>") + L"]");

	DesignerModel::DesignDocument renamedDocument;
	std::wstring stableIdError;
	const bool capturedRenamedDocument = renameCanvas.BuildDesignDocument(
		renamedDocument, &stableIdError);
	DesignerModel::DesignDocument reloadedRenamedDocument;
	const auto renamedXml = capturedRenamedDocument
		? DesignerModel::DesignDocumentSerializer::ToXml(renamedDocument)
		: std::string{};
	const bool parsedRenamedDocument = capturedRenamedDocument
		&& DesignerModel::DesignDocumentSerializer::FromXml(
			renamedXml, reloadedRenamedDocument, &stableIdError);
	DesignerCanvas reloadedIdentityCanvas(0, 0, 800, 640);
	const bool appliedRenamedDocument = parsedRenamedDocument
		&& reloadedIdentityCanvas.ApplyDesignDocument(
			reloadedRenamedDocument, &stableIdError);
	auto reloadedIdentity = FindControl(
		reloadedIdentityCanvas, L"RenamedButton");
	AppendFailure(failures,
		appliedRenamedDocument
		&& reloadedIdentity
		&& reloadedIdentity->StableId == originalStableId
		&& reloadedIdentity->ControlInstance->GetDesignId() == originalStableId
		&& reloadedRenamedDocument.NextStableId > originalStableId,
		L"stable identity: save/load or rename changed the control id");

	if (reloadedIdentity)
	{
		const int expectedNewId = reloadedRenamedDocument.NextStableId;
		reloadedIdentityCanvas.RestoreSelectionByNames(
			{ reloadedIdentity->Name }, reloadedIdentity->Name, false);
		const auto removedIdentity =
			reloadedIdentityCanvas.DeleteSelectedControl();
		reloadedIdentityCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, POINT{ 300, 180 });
		auto newIdentity = reloadedIdentityCanvas.GetSelectedControl();
		const int newStableId = newIdentity ? newIdentity->StableId : 0;
		const auto restoredIdentity = reloadedIdentityCanvas.UndoCommand();
		auto restoredOriginal = FindControl(
			reloadedIdentityCanvas, L"RenamedButton");
		AppendFailure(failures,
			removedIdentity.HasChanges()
			&& newIdentity
			&& newStableId == expectedNewId
			&& restoredIdentity.HasChanges()
			&& restoredOriginal
			&& restoredOriginal->StableId == originalStableId
			&& restoredOriginal->StableId != newStableId,
			L"stable identity: delete/add reused an id or undo changed identity");
	}

	// The public dynamic loader must consume the same complete materialized tree
	// as code generation: identity, style, bindings, and named events all travel
	// through one RuntimeDocument, and failed replacements leave it unchanged.
	DesignerCanvas runtimeSourceCanvas(0, 0, 800, 640);
	runtimeSourceCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 180, 170 });
	runtimeSourceCanvas.SetDesignedWindowEventHandler(
		L"ContentRendered", L"HandleRuntimeContentRendered");
	auto runtimeSourceControl = runtimeSourceCanvas.GetSelectedControl();
	DesignerModel::DesignDocument runtimeSourceDocument;
	std::wstring runtimeDocumentError;
	if (runtimeSourceControl && runtimeSourceControl->ControlInstance)
	{
		auto* runtimeButton = dynamic_cast<Button*>(
			runtimeSourceControl->ControlInstance);
		if (runtimeButton)
			runtimeButton->SetContent(
				BindingValue(std::wstring(L"运行时本地后备")));
		runtimeSourceControl->EventHandlers[L"Click"] =
			L"HandleRuntimeClick";
		runtimeSourceControl->DataBindings[L"Content"] = {
			L"Caption",
			BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged,
			{}
		};
		DesignerStyleSheet runtimeStyle;
		DesignerStyleRule runtimeRule;
		runtimeRule.HasType = true;
		runtimeRule.Type = UIClass::UI_Button;
		runtimeRule.Setters.push_back({
			L"BorderThickness", false, {},
			{ DesignerStyleValueKind::Thickness, L"7" } });
		runtimeStyle.Rules.push_back(std::move(runtimeRule));
		(void)runtimeSourceCanvas.SetDocumentStyleSheet(
			std::move(runtimeStyle), &runtimeDocumentError);
		(void)runtimeSourceCanvas.SetDataContextSchema({
			{ L"Caption", BindingValueKind::String, true, true, true }
		}, &runtimeDocumentError);
	}
	const bool capturedRuntimeDocument = runtimeSourceControl
		&& runtimeSourceCanvas.BuildDesignDocument(
			runtimeSourceDocument, &runtimeDocumentError);
	auto runtimeDataContext = std::make_shared<ObservableObject>();
	runtimeDataContext->SetValue(
		L"Caption", std::wstring(L"动态绑定值"));
	runtimeDataContext->SetValue(
		L"Status", std::wstring(L"Pending"));
	runtimeDataContext->SetValue(L"IsAdmin", false);
	int runtimeClickCount = 0;
	DesignerModel::RuntimeDocumentLoadOptions runtimeOptions;
	runtimeOptions.DataContext = runtimeDataContext;
	runtimeOptions.RequireControlEventResolver = true;
	runtimeOptions.ControlEventResolver =
		[&runtimeClickCount](
			const DesignerModel::RuntimeControlEventRequest& request,
			EventConnection& connection,
			std::wstring& error)
		{
			if (request.HandlerName != L"HandleRuntimeClick"
				|| request.Event.Name != L"Click")
			{
				error = L"unexpected runtime event request";
				return false;
			}
			connection = cui::framework::RoutedEventAccess::SubscribeClick(
				request.Target,
				[&runtimeClickCount](Control*, RoutedEventArgs&)
				{
					++runtimeClickCount;
				});
			return true;
		};
	DesignerModel::RuntimeDocument runtimeDocument;
	const bool loadedRuntimeDocument = capturedRuntimeDocument
		&& DesignerModel::RuntimeDocumentLoader::Load(
			runtimeSourceDocument,
			runtimeDocument,
			runtimeOptions,
			&runtimeDocumentError);
	auto* runtimeLoadedControl = runtimeSourceControl
		? runtimeDocument.FindControlByDesignId(runtimeSourceControl->StableId)
		: nullptr;
	if (runtimeLoadedControl)
	{
		cui::framework::RoutedEventAccess::RaiseClick(
			*runtimeLoadedControl, runtimeLoadedControl);
	}
	Window runtimeHostWindow;
	runtimeHostWindow.Title = L"runtime host";
	runtimeHostWindow.Width = 320.0f;
	runtimeHostWindow.Height = 200.0f;
	int runtimeContentRenderedCount = 0;
	const bool appliedRuntimeWindow = loadedRuntimeDocument
		&& runtimeDocument.ApplyWindowProperties(
			runtimeHostWindow, &runtimeDocumentError);
	const bool boundRuntimeWindowEvents = appliedRuntimeWindow
		&& runtimeDocument.BindWindowEvents(
			runtimeHostWindow,
			[&runtimeContentRenderedCount](
				const DesignerModel::RuntimeWindowEventRequest& request,
				EventConnection& connection,
				std::wstring& error)
			{
				if (request.HandlerName != L"HandleRuntimeContentRendered"
					|| request.Event.Name != L"ContentRendered")
				{
					error = L"unexpected runtime form event request";
					return false;
				}
				connection = request.Target.ContentRendered.Subscribe(
					[&runtimeContentRenderedCount](Window*)
					{
						++runtimeContentRenderedCount;
					});
				return true;
			},
			&runtimeDocumentError);
	if (boundRuntimeWindowEvents)
		cui::framework::EventAccess::Raise(runtimeHostWindow.ContentRendered, &runtimeHostWindow);
	AppendFailure(failures,
		loadedRuntimeDocument
		&& runtimeLoadedControl
		&& runtimeDocument.FindControlByName(runtimeSourceControl->Name)
			== runtimeLoadedControl
		&& runtimeDocument.ContentRoot()
		&& runtimeDocument.OwnsContentRoot()
		&& runtimeDocument.Controls().size() == 2
		&& runtimeDocument.BoundControlEventCount() == 1
		&& runtimeLoadedControl->GetDisplayText() == L"动态绑定值"
		&& cui::framework::StyleAccess::DocumentStyles(
			*runtimeLoadedControl) != nullptr
		&& runtimeClickCount == 1
		&& appliedRuntimeWindow
		&& boundRuntimeWindowEvents
		&& runtimeDocument.BoundWindowEventCount() == 1
		&& runtimeHostWindow.Title == ReadNodeString(
			runtimeSourceDocument.Window, L"Title")
		&& runtimeContentRenderedCount == 1,
		L"runtime document: normalized model did not materialize identity, style, binding, and event state"
		+ std::wstring(L" [loaded=") + (loadedRuntimeDocument ? L"1" : L"0")
		+ L", content=" + std::to_wstring(runtimeDocument.ContentRoot() != nullptr)
		+ L", controls=" + std::to_wstring(runtimeDocument.Controls().size())
		+ L", events=" + std::to_wstring(runtimeDocument.BoundControlEventCount())
		+ L", click=" + std::to_wstring(runtimeClickCount)
		+ L", formEvents=" + std::to_wstring(runtimeDocument.BoundWindowEventCount())
		+ L", shown=" + std::to_wstring(runtimeContentRenderedCount)
		+ L", error=" + runtimeDocumentError + L"]");
	if (runtimeLoadedControl)
	{
		runtimeDataContext->SetValue(
			L"Caption", std::wstring(L"动态更新值"));
		AppendFailure(failures,
			ReadControlStringProperty(
				runtimeLoadedControl, L"Content") == L"动态更新值",
			L"runtime document: live data-context update did not reach the target");
		runtimeDocument.ClearDataBindings();
		AppendFailure(failures,
			ReadControlStringProperty(
				runtimeLoadedControl, L"Content").empty()
			&& runtimeLoadedControl->GetPropertyExpressionKind(L"Content")
				== DependencyPropertyExpressionKind::None
			&& runtimeLoadedControl->DataBindings.Count() == 0,
			L"runtime document: clearing bindings resurrected the replaced Local value");
	}
	Control* const runtimeBeforeRejectedLoad = runtimeLoadedControl;
	auto invalidRuntimeDocument = runtimeSourceDocument;
	if (runtimeSourceControl)
	{
		for (auto& node : invalidRuntimeDocument.Nodes)
			if (node.Id == runtimeSourceControl->StableId) node.Id = 0;
	}
	const bool rejectedRuntimeReplacement =
		!DesignerModel::RuntimeDocumentLoader::Load(
			invalidRuntimeDocument,
			runtimeDocument,
			{},
			&runtimeDocumentError);
	AppendFailure(failures,
		rejectedRuntimeReplacement
		&& !runtimeDocumentError.empty()
		&& runtimeDocument.FindControlByDesignId(
			runtimeSourceControl ? runtimeSourceControl->StableId : 0)
			== runtimeBeforeRejectedLoad,
		L"runtime document: rejected normalized model corrupted the previously loaded tree");

	const std::string runtimeXaml = R"xaml(
<Window xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      xmlns:d="urn:cui:designer"
      x:Name="XamlRuntimeWindow" x:Class="Runtime.Views.RuntimeWindow"
      d:CodeBehind="generated/RuntimeWindow" Title="Readable runtime form"
      Width="520" Height="280" ContentRendered="HandleRuntimeContentRendered">
  <Window.Resources>
    <Color x:Key="Accent">#FF0067C0</Color>
    <Thickness x:Key="StaticHoverBorder">7</Thickness>
    <ControlTemplate x:Key="StaticCodeTemplate" TargetType="Button">
      <Border x:Name="StaticCodeChrome"
              Background="{StaticResource Accent}"
              Padding="{TemplateBinding Padding}">
        <Border.RenderTransform>
          <TransformGroup>
            <TranslateTransform X="0" />
          </TransformGroup>
        </Border.RenderTransform>
        <Border.Triggers>
          <EventTrigger RoutedEvent="Click">
            <BeginStoryboard x:Name="StaticClickPulse">
              <Storyboard>
                <DoubleAnimation Storyboard.TargetName="StaticCodeChrome"
                  Storyboard.TargetProperty="(Canvas.Left)"
                  From="0" To="12" Duration="0:0:0.200"/>
                <DoubleAnimation Storyboard.TargetName="StaticCodeChrome"
                  Storyboard.TargetProperty="(Control.RenderTransform).(TransformGroup.Children)[0].(TranslateTransform.X)"
                  From="0" To="8" Duration="0:0:0.200"/>
              </Storyboard>
            </BeginStoryboard>
          </EventTrigger>
        </Border.Triggers>
        <VisualStateManager.VisualStateGroups>
          <VisualStateGroup x:Name="StaticCommonStates">
            <VisualStateGroup.Transitions>
              <VisualTransition From="Normal" To="PointerOver"
                GeneratedDuration="0:0:0.100"/>
            </VisualStateGroup.Transitions>
            <VisualState x:Name="PointerOver">
              <VisualState.StateTriggers>
                <StateTrigger Property="IsMouseOver" Value="true"/>
              </VisualState.StateTriggers>
              <VisualState.Setters>
                <Setter TargetName="StaticCodeChrome"
                  Property="Background" Value="#FFEAF2FF"/>
              </VisualState.Setters>
            </VisualState>
            <VisualState x:Name="Normal"/>
          </VisualStateGroup>
        </VisualStateManager.VisualStateGroups>
        <ContentPresenter x:Name="StaticCodePresenter"
                          ContentSource="Content" />
      </Border>
    </ControlTemplate>
    <Style TargetType="Button">
      <Setter Property="IsDefault" Value="false" />
      <Style.Triggers>
        <Trigger Property="IsMouseOver" Value="true">
          <Setter Property="BorderThickness" Value="5.5" />
          <Trigger.EnterActions>
            <BeginStoryboard x:Name="StaticStyleHoverClock">
              <Storyboard>
                <ThicknessAnimation Storyboard.TargetProperty="BorderThickness"
                  From="5.5" To="{StaticResource StaticHoverBorder}"
                  Duration="0:0:0.080" />
                <DoubleAnimationUsingKeyFrames
                  Storyboard.TargetProperty="(Control.RenderTransform).(TranslateTransform.X)"
                  Duration="0:0:0.080">
                  <LinearDoubleKeyFrame KeyTime="0:0:0.080" Value="6" />
                </DoubleAnimationUsingKeyFrames>
              </Storyboard>
            </BeginStoryboard>
          </Trigger.EnterActions>
          <Trigger.ExitActions>
            <StopStoryboard BeginStoryboardName="StaticStyleHoverClock" />
          </Trigger.ExitActions>
        </Trigger>
        <MultiTrigger>
          <MultiTrigger.Conditions>
            <Condition Property="IsMouseOver" Value="true" />
            <Condition Property="IsDefault" Value="true" />
          </MultiTrigger.Conditions>
          <Setter Property="FontSize" Value="18" />
        </MultiTrigger>
        <DataTrigger Binding="{Binding Status}" Value="Ready">
          <Setter Property="Visibility" Value="Collapsed" />
        </DataTrigger>
        <MultiDataTrigger>
          <MultiDataTrigger.Conditions>
            <Condition Binding="{Binding Status}" Value="Ready" />
            <Condition Binding="{Binding IsAdmin}" Value="true" />
          </MultiDataTrigger.Conditions>
          <Setter Property="IsDefault" Value="true" />
        </MultiDataTrigger>
      </Style.Triggers>
    </Style>
    <Style x:Key="BaseButton" TargetType="Button"
           BasedOn="{StaticResource {x:Type Button}}">
      <Setter Property="BorderThickness" Value="2.5" />
      <Setter Property="FontSize" Value="14" />
    </Style>
    <Style x:Key="PrimaryButton" BasedOn="{StaticResource BaseButton}">
      <Setter Property="FontSize" Value="16" />
      <Setter Property="Background" Value="{StaticResource Accent}" />
    </Style>
  </Window.Resources>
  <StackPanel x:Name="xamlRoot" DesignId="500"
              Width="Auto" Height="Auto"
              Orientation="Vertical">
    <Button x:Name="xamlAction" DesignId="501"
            Style="{StaticResource PrimaryButton}"
            Template="{StaticResource StaticCodeTemplate}"
            Width="180.5" Height="36"
            Content="{Binding Caption, Mode=OneWay}"
            Click="HandleRuntimeClick">
      <Control.RenderTransform>
        <TranslateTransform X="0" />
      </Control.RenderTransform>
    </Button>
  </StackPanel>
</Window>)xaml";
	DesignerModel::DesignDocument parsedXamlDocument;
	std::wstring xamlError;
	const bool parsedRuntimeXaml =
		DesignerModel::XamlDocumentParser::FromXaml(
			runtimeXaml, parsedXamlDocument, &xamlError);
	const auto parsedRuntimeXamlError = parsedRuntimeXaml
		? std::wstring{} : xamlError;
	DesignerModel::DesignDocument xamlRoundTrip;
	const bool roundTrippedRuntimeXaml = parsedRuntimeXaml
		&& DesignerModel::DesignDocumentSerializer::FromXml(
			DesignerModel::DesignDocumentSerializer::ToXml(parsedXamlDocument),
			xamlRoundTrip,
			&xamlError)
		&& xamlRoundTrip == parsedXamlDocument;
	const auto roundTrippedRuntimeXamlError = roundTrippedRuntimeXaml
		? std::wstring{} : xamlError;
	const auto canonicalRuntimeXaml = parsedRuntimeXaml
		? DesignerModel::XamlDocumentSerializer::ToXaml(parsedXamlDocument)
		: std::string{};
	DesignerModel::DesignDocument canonicalXamlRoundTrip;
	const bool roundTrippedCanonicalXaml = parsedRuntimeXaml
		&& DesignerModel::XamlDocumentParser::FromXaml(
			canonicalRuntimeXaml,
			canonicalXamlRoundTrip,
			&xamlError)
		&& EquivalentXamlContent(
			canonicalXamlRoundTrip, parsedXamlDocument);
	const auto roundTrippedCanonicalXamlError = roundTrippedCanonicalXaml
		? std::wstring{} : xamlError;
	const bool builtXamlStyleCodeInput = parsedRuntimeXaml
		&& CodeGenerator::ValidateDocument(parsedXamlDocument, &xamlError);
	const auto xamlStyleGeneratedCpp = builtXamlStyleCodeInput
		? CodeGenerator(
			L"XamlStyleWindow",
			parsedXamlDocument,
			CodeGeneratorOutputKind::StaticWindow).GenerateCpp()
		: std::string{};
	const auto firstInheritedBorder = xamlStyleGeneratedCpp.find(
		"Thickness(2.5f, 2.5f, 2.5f, 2.5f)");
	const bool generatedExpandedStyleInheritance = firstInheritedBorder
		!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"Thickness(2.5f, 2.5f, 2.5f, 2.5f)",
			firstInheritedBorder + 1)
			!= std::string::npos;
	DesignerModel::RuntimeDocument xamlRuntimeDocument;
	int xamlClickCount = 0;
	auto xamlOptions = runtimeOptions;
	xamlOptions.ControlEventResolver =
		[&xamlClickCount](
			const DesignerModel::RuntimeControlEventRequest& request,
			EventConnection& connection,
			std::wstring& error)
		{
			if (request.HandlerName != L"HandleRuntimeClick"
				|| request.Event.Name != L"Click")
			{
				error = L"unexpected XAML event request";
				return false;
			}
			connection = cui::framework::RoutedEventAccess::SubscribeClick(
				request.Target,
				[&xamlClickCount](Control*, RoutedEventArgs&) { ++xamlClickCount; });
			return true;
		};
	const bool loadedRuntimeXaml =
		DesignerModel::RuntimeDocumentLoader::LoadXaml(
			runtimeXaml, xamlRuntimeDocument, xamlOptions, &xamlError);
	const auto loadedRuntimeXamlError = loadedRuntimeXaml
		? std::wstring{} : xamlError;
	const auto runtimeXamlFrontendError = !parsedRuntimeXamlError.empty()
		? parsedRuntimeXamlError
		: (!roundTrippedRuntimeXamlError.empty()
			? roundTrippedRuntimeXamlError
			: (!roundTrippedCanonicalXamlError.empty()
				? roundTrippedCanonicalXamlError
				: loadedRuntimeXamlError));
	auto* xamlAction = xamlRuntimeDocument.FindControlByDesignId(501);
	auto* xamlButton = dynamic_cast<Button*>(xamlAction);
	bool xamlTriggerApplied = false;
	bool xamlTriggerRestored = false;
	bool xamlMultiTriggerInactive = false;
	bool xamlMultiTriggerApplied = false;
	bool xamlMultiTriggerRestored = false;
	bool xamlDataTriggerInactive = false;
	bool xamlDataTriggerApplied = false;
	bool xamlDataTriggerRestored = false;
	bool xamlMultiDataTriggerInactive = false;
	bool xamlMultiDataTriggerApplied = false;
	bool xamlMultiDataTriggerRestored = false;
	if (xamlButton)
	{
		xamlDataTriggerInactive = xamlButton->IsVisible;
		xamlMultiDataTriggerInactive = !xamlButton->IsDefault;
		runtimeDataContext->SetValue(
			L"Status", std::wstring(L"Ready"));
		xamlDataTriggerApplied = !xamlButton->IsVisible;
		runtimeDataContext->SetValue(L"IsAdmin", true);
		xamlMultiDataTriggerApplied = xamlButton->IsDefault;
		runtimeDataContext->SetValue(
			L"Status", std::wstring(L"Pending"));
		xamlDataTriggerRestored = xamlButton->IsVisible;
		xamlMultiDataTriggerRestored = !xamlButton->IsDefault;
		runtimeDataContext->SetValue(L"IsAdmin", false);
		cui::framework::InputAccess::PublishPointerOverState(
			*xamlButton, true, true);
		xamlTriggerApplied = std::fabs(
			xamlButton->BorderThickness.MaxEdge() - 5.5f) < 0.001f;
		xamlMultiTriggerInactive = std::fabs(xamlButton->FontSize - 16.0) < 0.001;
		xamlButton->IsDefault = true;
		xamlMultiTriggerApplied = std::fabs(xamlButton->FontSize - 18.0) < 0.001;
		cui::framework::InputAccess::PublishPointerOverState(
			*xamlButton, false, false);
		xamlTriggerRestored = std::fabs(
			xamlButton->BorderThickness.MaxEdge() - 2.5f) < 0.001f;
		xamlMultiTriggerRestored = std::fabs(xamlButton->FontSize - 16.0) < 0.001;
		xamlButton->IsDefault = false;
	}
	if (xamlAction)
		cui::framework::RoutedEventAccess::RaiseClick(*xamlAction, xamlAction);
	const auto* xamlActionBeforeFailure = xamlAction;
	const bool rejectedXamlReplacement =
		!DesignerModel::RuntimeDocumentLoader::LoadXaml(
			"<Window><Button Name=\"bad\" UnknownProperty=\"1\" /></Window>",
			xamlRuntimeDocument,
			{},
			&xamlError);
	auto unchangedParsedDocument = parsedXamlDocument;
	const bool rejectedParserReplacement =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Window><Unknown /></Window>", unchangedParsedDocument, &xamlError);
	auto unchangedConflictDocument = parsedXamlDocument;
	const bool rejectedSignatureConflictXaml =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Window x:Name=\"ConflictWindow\" "
			"xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
			"Command=\"HandleShared\">"
			"<Button x:Name=\"action\" Click=\"HandleShared\" />"
			"</Window>", unchangedConflictDocument, &xamlError);
	auto unchangedInvalidNameDocument = parsedXamlDocument;
	const bool rejectedInvalidControlNameXaml =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Window xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
			"<Button x:Name=\"bad-name\" /></Window>",
			unchangedInvalidNameDocument, &xamlError);
	auto unchangedDuplicateNameDocument = parsedXamlDocument;
	const bool rejectedDuplicateControlNameXaml =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Window xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
			"<Button x:Name=\"saveButton\" />"
			"<Button x:Name=\"SaveButton\" /></Window>",
			unchangedDuplicateNameDocument, &xamlError);
	auto unchangedCodeBehindDocument = parsedXamlDocument;
	const bool rejectedAbsoluteCodeBehindXaml =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Window xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
			"xmlns:d=\"urn:cui:designer\" x:Name=\"Unsafe\" "
			"x:Class=\"UnsafeWindow\" d:CodeBehind=\"C:/outside/UnsafeWindow\" />",
			unchangedCodeBehindDocument, &xamlError);
	auto unchangedDataTriggerDocument = parsedXamlDocument;
	const bool rejectedConfiguredDataTriggerBinding =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Window><Window.Resources><Style TargetType=\"Button\"><Style.Triggers>"
			"<DataTrigger Binding=\"{Binding Status, Mode=OneWay}\" Value=\"Ready\">"
			"<Setter Property=\"Visibility\" Value=\"Collapsed\" /></DataTrigger>"
			"</Style.Triggers></Style></Window.Resources></Window>",
			unchangedDataTriggerDocument, &xamlError);
	auto unchangedDataTriggerResourceDocument = parsedXamlDocument;
	const bool rejectedDataTriggerResourceValue =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Window><Window.Resources><Style TargetType=\"Button\"><Style.Triggers>"
			"<DataTrigger Binding=\"{Binding Status}\" Value=\"{StaticResource Ready}\">"
			"<Setter Property=\"Visibility\" Value=\"Collapsed\" /></DataTrigger>"
			"</Style.Triggers></Style></Window.Resources></Window>",
			unchangedDataTriggerResourceDocument, &xamlError);
	auto unchangedMultiDataTriggerDocument = parsedXamlDocument;
	const bool rejectedMultiDataTrigger =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Window><Window.Resources><Style TargetType=\"Button\"><Style.Triggers>"
			"<MultiDataTrigger><MultiDataTrigger.Conditions>"
			"<Condition Binding=\"{Binding Status}\" Value=\"Ready\" />"
			"</MultiDataTrigger.Conditions><Setter Property=\"Visibility\" Value=\"Collapsed\" />"
			"</MultiDataTrigger></Style.Triggers></Style></Window.Resources></Window>",
			unchangedMultiDataTriggerDocument, &xamlError);
	const bool runtimeXamlFrontendReady = parsedRuntimeXaml
		&& roundTrippedRuntimeXaml
		&& roundTrippedCanonicalXaml
		&& loadedRuntimeXaml && xamlAction && xamlButton;
	const bool runtimeXamlEffectiveValues = xamlAction && xamlButton
		&& ReadControlStringProperty(xamlAction, L"Content") == L"动态更新值"
		&& !xamlButton->IsDefault
		&& std::fabs(
			xamlButton->BorderThickness.MaxEdge() - 2.5f) < 0.001f
		&& std::fabs(xamlButton->FontSize - 16.0) < 0.001
		&& xamlAction->Width.IsFixed()
		&& std::fabs(xamlAction->Width.value - 180.5f) < 0.001f
		&& cui::framework::StyleAccess::DocumentStyles(*xamlAction) != nullptr;
	const bool runtimeXamlTriggersReady = xamlTriggerApplied
		&& xamlTriggerRestored
		&& xamlMultiTriggerInactive
		&& xamlMultiTriggerApplied
		&& xamlMultiTriggerRestored
		&& xamlDataTriggerInactive
		&& xamlDataTriggerApplied
		&& xamlDataTriggerRestored
		&& xamlMultiDataTriggerInactive
		&& xamlMultiDataTriggerApplied
		&& xamlMultiDataTriggerRestored;
	const bool runtimeXamlCanonicalStyles =
		canonicalRuntimeXaml.find("<Style TargetType=\"Button\">")
			!= std::string::npos
		&& canonicalRuntimeXaml.find("<Style.Triggers>")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"<Trigger Property=\"IsMouseOver\" Value=\"true\">")
			!= std::string::npos
		&& canonicalRuntimeXaml.find("<Trigger.EnterActions>")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"<BeginStoryboard x:Name=\"StaticStyleHoverClock\">")
			!= std::string::npos
		&& canonicalRuntimeXaml.find("<Trigger.ExitActions>")
			!= std::string::npos
		&& canonicalRuntimeXaml.find("<MultiTrigger>") != std::string::npos
		&& canonicalRuntimeXaml.find("<MultiTrigger.Conditions>")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"<Condition Property=\"IsDefault\" Value=\"true\"")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"<DataTrigger Binding=\"{Binding Status}\" Value=\"Ready\">")
			!= std::string::npos
		&& canonicalRuntimeXaml.find("<MultiDataTrigger>")
			!= std::string::npos
		&& canonicalRuntimeXaml.find("<MultiDataTrigger.Conditions>")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"<Condition Binding=\"{Binding IsAdmin}\" Value=\"true\"")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"BasedOn=\"{StaticResource {x:Type Button}}\"")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"BasedOn=\"{StaticResource BaseButton}\"")
			!= std::string::npos;
	const auto runtimeXamlCompiledTargetParent = xamlStyleGeneratedCpp.find(
		"TreeAccess::SetTemplatedParent(*__cuiStaticTemplateOwner");
	const auto runtimeXamlCompiledTargetRegistration = xamlStyleGeneratedCpp.find(
		"TemplateAccess::RegisterTemplatePart(__templateOwner");
	const auto runtimeXamlCompiledInteractionInstall = xamlStyleGeneratedCpp.find(
		"TemplateAccess::InstallCompiledInteractions(__templateOwner");
	const bool runtimeXamlCompiledTargetOrder =
		runtimeXamlCompiledTargetParent != std::string::npos
		&& runtimeXamlCompiledTargetRegistration != std::string::npos
		&& runtimeXamlCompiledInteractionInstall != std::string::npos
		&& runtimeXamlCompiledTargetParent
			< runtimeXamlCompiledTargetRegistration
		&& runtimeXamlCompiledTargetRegistration
			< runtimeXamlCompiledInteractionInstall;
	const bool runtimeXamlGeneratedContract =
		xamlStyleGeneratedCpp.find(
			"static constexpr CompiledStyleDataConditionOp "
			"__styleSheet_program_data_conditions")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"::BindData(BindingSourceReference dataContext)")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"SetDataContext(std::move(dataContext))") != std::string::npos
		&& xamlStyleGeneratedCpp.find("->DataContextSource()")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find("__styles->SetDataContext")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"xamlAction->ClearPropertyValues()")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"xamlAction->TrySetPropertyValue(L\"Background\"")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find("xamlAction->Background =")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"class CuiGeneratedControlTemplate final")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"__controlTemplate_StaticCodeTemplate")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"->SetApplyCallback([this")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"ControlTemplateReference(__controlTemplate_StaticCodeTemplate")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static const CompiledInteractionProgramView "
			"__cuiInteractionProgram") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static const BindingValue __cuiInteraction_values")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"const BindingValue __cuiInteraction_values")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledInteractionGroupOp "
			"__cuiInteraction_groups") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledInteractionStateOp "
			"__cuiInteraction_states") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledInteractionTransitionOp "
			"__cuiInteraction_transitions") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static const CompiledInteractionPropertyOperand "
			"__cuiInteraction_property_operands") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledInteractionConditionOp "
			"__cuiInteraction_conditions") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledInteractionSetterOp "
			"__cuiInteraction_setters") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledStoryboardObjectPathOp "
			"__cuiInteraction_object_paths") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"CompiledStoryboardObjectPathKind::Transform")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"CompiledStoryboardObjectPathMember::TransformX")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static_cast<uint8_t>(cui::drawing::TransformKind::Translate)")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"std::array<Control*, 2> __cuiInteractionTargets")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find("setter.TargetName")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"{ nullptr, static_cast<RoutedEventId>(11),")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find("trigger.EventName")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"DependencyPropertyReference(Control::CanvasLeftProperty())")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find("animation.ObjectPath = L\"")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"DeclarativeVisualStateGroupDefinition group")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"group.Name = L\"StaticCommonStates\"")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"TemplateAccess::InstallCompiledInteractions(__templateOwner")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"__cuiInteractionProgram, std::span<const BindingValue>{ "
			"__cuiInteraction_values }") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"TemplateAccess::DefineInteractions(__templateOwner")
			== std::string::npos
		&& runtimeXamlCompiledTargetOrder
		&& xamlStyleGeneratedCpp.find(
			"static const CompiledInteractionPropertyOperand "
			"__styleSheet_program_property_operands") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"DependencyPropertyReference(Control::BorderThicknessProperty())")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledStoryboardObjectPathOp "
			"__styleSheet_program_object_paths") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledInteractionKeyFrameOp "
			"__styleSheet_program_key_frames") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledInteractionAnimationOp "
			"__styleSheet_program_animations") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledInteractionStoryboardOp "
			"__styleSheet_program_storyboards") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledInteractionActionOp "
			"__styleSheet_program_actions") != std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"{ DeclarativeStoryboardActionKind::Begin, 0u }")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"{ DeclarativeStoryboardActionKind::Stop, 0u }")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"BindingValue(Thickness(7.f, 7.f, 7.f, 7.f))")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"std::vector<DeclarativeEventTriggerActionDefinition>")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find("action.StoryboardName =")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find("animation.Property =")
			== std::string::npos
		&& generatedExpandedStyleInheritance;
	const bool runtimeXamlIdentityReady = xamlClickCount == 1
		&& xamlRuntimeDocument.WindowNode().Name == L"XamlRuntimeWindow"
		&& parsedXamlDocument.CodeBehind.ClassName
			== L"Runtime::Views::RuntimeWindow"
		&& parsedXamlDocument.CodeBehind.RelativeBasePath
			== L"generated/RuntimeWindow"
		&& xamlRuntimeDocument.ContentRoot()
		&& xamlRuntimeDocument.Controls().size() == 2;
	const bool runtimeXamlRollbackReady = rejectedXamlReplacement
		&& xamlRuntimeDocument.FindControlByDesignId(501)
			== xamlActionBeforeFailure
		&& rejectedParserReplacement
		&& unchangedParsedDocument == parsedXamlDocument
		&& rejectedSignatureConflictXaml
		&& unchangedConflictDocument == parsedXamlDocument
		&& rejectedInvalidControlNameXaml
		&& unchangedInvalidNameDocument == parsedXamlDocument
		&& rejectedDuplicateControlNameXaml
		&& unchangedDuplicateNameDocument == parsedXamlDocument
		&& rejectedAbsoluteCodeBehindXaml
		&& unchangedCodeBehindDocument == parsedXamlDocument
		&& rejectedConfiguredDataTriggerBinding
		&& unchangedDataTriggerDocument == parsedXamlDocument
		&& rejectedDataTriggerResourceValue
		&& unchangedDataTriggerResourceDocument == parsedXamlDocument
		&& rejectedMultiDataTrigger
		&& unchangedMultiDataTriggerDocument == parsedXamlDocument;
	AppendFailure(failures,
		parsedRuntimeXaml
		&& roundTrippedRuntimeXaml
		&& roundTrippedCanonicalXaml
		&& loadedRuntimeXaml
		&& xamlAction
		&& xamlButton
		&& ReadControlStringProperty(xamlAction, L"Content") == L"动态更新值"
		&& !xamlButton->IsDefault
		&& std::fabs(
			xamlButton->BorderThickness.MaxEdge() - 2.5f) < 0.001f
		&& std::fabs(xamlButton->FontSize - 16.0) < 0.001
		&& xamlTriggerApplied
		&& xamlTriggerRestored
		&& xamlMultiTriggerInactive
		&& xamlMultiTriggerApplied
		&& xamlMultiTriggerRestored
		&& xamlDataTriggerInactive
		&& xamlDataTriggerApplied
		&& xamlDataTriggerRestored
		&& xamlMultiDataTriggerInactive
		&& xamlMultiDataTriggerApplied
		&& xamlMultiDataTriggerRestored
		&& xamlAction->Width.IsFixed()
		&& std::fabs(xamlAction->Width.value - 180.5f) < 0.001f
		&& cui::framework::StyleAccess::DocumentStyles(*xamlAction) != nullptr
		&& canonicalRuntimeXaml.find("<Style TargetType=\"Button\">")
			!= std::string::npos
		&& canonicalRuntimeXaml.find("<Style.Triggers>")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"<Trigger Property=\"IsMouseOver\" Value=\"true\">")
			!= std::string::npos
		&& canonicalRuntimeXaml.find("<MultiTrigger>") != std::string::npos
		&& canonicalRuntimeXaml.find("<MultiTrigger.Conditions>")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"<Condition Property=\"IsDefault\" Value=\"true\"")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"<DataTrigger Binding=\"{Binding Status}\" Value=\"Ready\">")
			!= std::string::npos
		&& canonicalRuntimeXaml.find("<MultiDataTrigger>") != std::string::npos
		&& canonicalRuntimeXaml.find("<MultiDataTrigger.Conditions>")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"<Condition Binding=\"{Binding IsAdmin}\" Value=\"true\"")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"static constexpr CompiledStyleDataConditionOp "
			"__styleSheet_program_data_conditions")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"::BindData(BindingSourceReference dataContext)")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"SetDataContext(std::move(dataContext))") != std::string::npos
		&& xamlStyleGeneratedCpp.find("->DataContextSource()")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find("__styles->SetDataContext")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"xamlAction->ClearPropertyValues()")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"xamlAction->TrySetPropertyValue(L\"Background\"")
			== std::string::npos
		&& xamlStyleGeneratedCpp.find("xamlAction->Background =")
			== std::string::npos
		&& canonicalRuntimeXaml.find(
			"BasedOn=\"{StaticResource {x:Type Button}}\"")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"BasedOn=\"{StaticResource BaseButton}\"")
			!= std::string::npos
		&& generatedExpandedStyleInheritance
		&& runtimeXamlGeneratedContract
		&& xamlClickCount == 1
		&& xamlRuntimeDocument.WindowNode().Name == L"XamlRuntimeWindow"
		&& parsedXamlDocument.CodeBehind.ClassName
			== L"Runtime::Views::RuntimeWindow"
		&& parsedXamlDocument.CodeBehind.RelativeBasePath
			== L"generated/RuntimeWindow"
		&& xamlRuntimeDocument.ContentRoot()
		&& xamlRuntimeDocument.Controls().size() == 2
		&& rejectedXamlReplacement
		&& xamlRuntimeDocument.FindControlByDesignId(501)
			== xamlActionBeforeFailure
		&& rejectedParserReplacement
		&& unchangedParsedDocument == parsedXamlDocument
		&& rejectedSignatureConflictXaml
		&& unchangedConflictDocument == parsedXamlDocument
		&& rejectedInvalidControlNameXaml
		&& unchangedInvalidNameDocument == parsedXamlDocument
		&& rejectedDuplicateControlNameXaml
		&& unchangedDuplicateNameDocument == parsedXamlDocument
		&& rejectedAbsoluteCodeBehindXaml
		&& unchangedCodeBehindDocument == parsedXamlDocument
		&& rejectedConfiguredDataTriggerBinding
		&& unchangedDataTriggerDocument == parsedXamlDocument
		&& rejectedDataTriggerResourceValue
		&& unchangedDataTriggerResourceDocument == parsedXamlDocument
		&& rejectedMultiDataTrigger
		&& unchangedMultiDataTriggerDocument == parsedXamlDocument,
		L"runtime XAML: frontend, floating/Auto layout, binding, style, event, "
		L"round-trip, or transactional rollback failed"
		+ std::wstring(L" [frontend=") + SelfTestFlag(runtimeXamlFrontendReady)
		+ L", parsed=" + SelfTestFlag(parsedRuntimeXaml)
		+ L", xmlRound=" + SelfTestFlag(roundTrippedRuntimeXaml)
		+ L", canonicalRound=" + SelfTestFlag(roundTrippedCanonicalXaml)
		+ L", loaded=" + SelfTestFlag(loadedRuntimeXaml)
		+ L", values=" + SelfTestFlag(runtimeXamlEffectiveValues)
		+ L", triggers=" + SelfTestFlag(runtimeXamlTriggersReady)
		+ L", canonical=" + SelfTestFlag(runtimeXamlCanonicalStyles)
		+ L", generated=" + SelfTestFlag(runtimeXamlGeneratedContract)
		+ L", codeInput=" + SelfTestFlag(builtXamlStyleCodeInput)
		+ L", cppBytes=" + std::to_wstring(xamlStyleGeneratedCpp.size())
		+ L", inherited=" + SelfTestFlag(generatedExpandedStyleInheritance)
		+ L", identity=" + SelfTestFlag(runtimeXamlIdentityReady)
		+ L", rollback=" + SelfTestFlag(runtimeXamlRollbackReady)
		+ L", error=" + runtimeXamlFrontendError + L"]");
	DesignerStyleSheet cyclicStyles;
	DesignerStyleRule cyclicA;
	cyclicA.Id = L"CycleA";
	cyclicA.BasedOn = L"CycleB";
	cyclicA.Setters.push_back({
		L"Visibility", false, {},
		{ DesignerStyleValueKind::String, L"Visible" } });
	DesignerStyleRule cyclicB;
	cyclicB.Id = L"CycleB";
	cyclicB.BasedOn = L"CycleA";
	cyclicB.Setters.push_back({
		L"IsEnabled", false, {}, { DesignerStyleValueKind::Bool, L"true" } });
	cyclicStyles.Rules = { std::move(cyclicA), std::move(cyclicB) };
	std::wstring cyclicStyleError;
	AppendFailure(failures,
		!DesignerStyleSheetUtils::Validate(cyclicStyles, &cyclicStyleError)
		&& cyclicStyleError.find(L"循环") != std::wstring::npos,
		L"style inheritance: a cyclic BasedOn chain was accepted");
	auto releasedRuntimeContent = runtimeDocument.ReleaseContentRoot();
	AppendFailure(failures,
		releasedRuntimeContent
		&& !runtimeDocument.OwnsContentRoot()
		&& !runtimeDocument.ReleaseContentRoot(),
		L"runtime document: Content ownership transfer was inconsistent"
		+ std::wstring(L" [released=")
		+ std::to_wstring(releasedRuntimeContent != nullptr)
		+ L", owns=" + (runtimeDocument.OwnsContentRoot() ? L"1" : L"0")
		+ L"]");

	DesignerModel::DesignDocument runtimeWebDocument;
	DesignerModel::DesignNode runtimeWebNode;
	runtimeWebNode.Id = runtimeWebDocument.AllocateNodeId();
	runtimeWebNode.Name = L"runtimeBrowser";
	runtimeWebNode.Type = UIClass::UI_WebBrowser;
	runtimeWebDocument.Nodes.push_back(std::move(runtimeWebNode));
	DesignerModel::RuntimeDocument runtimeWebResult;
	const bool loadedProductionWebBrowser =
		DesignerModel::RuntimeDocumentLoader::Load(
			runtimeWebDocument, runtimeWebResult, {}, &runtimeDocumentError);
	auto* runtimeBrowser = runtimeWebResult.FindControlByName(L"runtimeBrowser");
	AppendFailure(failures,
		loadedProductionWebBrowser
		&& runtimeBrowser
		&& typeid(*runtimeBrowser) == typeid(WebBrowser),
		L"runtime document: default factory created a Designer WebBrowser placeholder"
		+ std::wstring(L" [loaded=")
		+ (loadedProductionWebBrowser ? L"1" : L"0")
		+ L", error=" + runtimeDocumentError + L"]");

	std::unique_ptr<Control> externallyOwnedRuntimeContent;
	Control* externallyOwnedRuntimeControl = nullptr;
	bool loadedExternalOwnershipBinding = false;
	{
		DesignerModel::RuntimeDocument externallyOwnedDocument;
		DesignerModel::RuntimeDocumentLoadOptions externalOptions;
		externalOptions.DataContext = runtimeDataContext;
		loadedExternalOwnershipBinding = capturedRuntimeDocument
			&& DesignerModel::RuntimeDocumentLoader::Load(
				runtimeSourceDocument,
				externallyOwnedDocument,
				externalOptions,
				&runtimeDocumentError);
		externallyOwnedRuntimeControl = runtimeSourceControl
			? externallyOwnedDocument.FindControlByDesignId(
				runtimeSourceControl->StableId)
			: nullptr;
		externallyOwnedRuntimeContent =
			externallyOwnedDocument.ReleaseContentRoot();
	}
	AppendFailure(failures,
		loadedExternalOwnershipBinding
		&& externallyOwnedRuntimeControl
		&& ReadControlStringProperty(
			externallyOwnedRuntimeControl, L"Content").empty()
		&& externallyOwnedRuntimeControl->GetPropertyExpressionKind(L"Content")
			== DependencyPropertyExpressionKind::None
		&& externallyOwnedRuntimeControl->DataBindings.Count() == 0
		&& externallyOwnedRuntimeContent,
		L"runtime document: raw Content transfer did not detach managed bindings"
		+ std::wstring(L" [loaded=")
		+ (loadedExternalOwnershipBinding ? L"1" : L"0")
		+ L", content=" + std::to_wstring(
			externallyOwnedRuntimeContent != nullptr)
		+ L", error=" + runtimeDocumentError + L"]");

	DesignerCanvas largePropertyCanvas(0, 0, 1000, 720);
	for (int index = 0; index < 160; ++index)
		largePropertyCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button,
			POINT{ 30 + (index % 16) * 55, 40 + (index / 16) * 45 });
	const auto largePropertyTarget = largePropertyCanvas.GetAllControls()[1];
	const auto largePropertyName = largePropertyTarget->Name;
	const auto largeOriginalAutomationName =
		ControlAutomationName(largePropertyCanvas, largePropertyName);
	largePropertyCanvas.RestoreSelectionByNames(
		{ largePropertyName }, largePropertyName, false);
	(void)largePropertyCanvas.ResetDocumentHistoryAsSaved();
	PropertyGrid largePropertyGrid(0, 0, 360, 620);
	largePropertyGrid.SetDesignerCanvas(&largePropertyCanvas);
	ReloadCurrentSelection(largePropertyGrid, largePropertyCanvas);
	const auto largePropertyResult = largePropertyGrid.ApplyPropertyValue(
		L"AutomationProperties.Name", L"LargeDelta");
	AppendFailure(failures,
		largePropertyResult.Succeeded
		&& largePropertyCanvas.GetUndoCommandCount() == 1
		&& largePropertyCanvas.GetCommandHistoryMemoryUsage() < 32768
		&& largePropertyCanvas.UndoCommand().HasChanges()
		&& ControlAutomationName(largePropertyCanvas, largePropertyName)
			== largeOriginalAutomationName
		&& FindControl(largePropertyCanvas, largePropertyName)
			== largePropertyTarget
		&& largePropertyCanvas.RedoCommand().HasChanges()
		&& ControlAutomationName(largePropertyCanvas, largePropertyName)
			== L"LargeDelta"
		&& FindControl(largePropertyCanvas, largePropertyName)
			== largePropertyTarget,
		L"property delta: large document retained a full snapshot or rebuilt controls");

	DesignerCanvas guardedPropertyCanvas(0, 0, 800, 640);
	guardedPropertyCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 160, 160 });
	(void)guardedPropertyCanvas.ResetDocumentHistoryAsSaved();
	PropertyGrid guardedPropertyGrid(0, 0, 360, 620);
	guardedPropertyGrid.SetDesignerCanvas(&guardedPropertyCanvas);
	ReloadCurrentSelection(guardedPropertyGrid, guardedPropertyCanvas);
	const auto guardedPropertyControl =
		guardedPropertyCanvas.GetSelectedControl();
	const auto guardedOriginalAutomationName = guardedPropertyControl
		? ReadControlStringProperty(
			guardedPropertyControl->ControlInstance,
			L"AutomationProperties.Name")
		: std::wstring{};
	const auto guardedPropertyEdit = guardedPropertyGrid.ApplyPropertyValue(
		L"AutomationProperties.Name", L"GuardedDelta");
	const auto guardedPropertyMemory =
		guardedPropertyCanvas.GetCommandHistoryMemoryUsage();
	if (guardedPropertyControl && guardedPropertyControl->ControlInstance)
		(void)WriteControlStringProperty(
			guardedPropertyControl->ControlInstance,
			L"AutomationProperties.Name", L"ExternalMutation");
	const auto rejectedPropertyUndo = guardedPropertyCanvas.UndoCommand();
	AppendFailure(failures,
		guardedPropertyEdit.Succeeded
		&& !rejectedPropertyUndo
		&& guardedPropertyCanvas.GetUndoCommandCount() == 1
		&& guardedPropertyCanvas.GetCommandHistoryMemoryUsage()
			== guardedPropertyMemory
		&& guardedPropertyControl
		&& ReadControlStringProperty(
			guardedPropertyControl->ControlInstance,
			L"AutomationProperties.Name")
			== L"ExternalMutation",
		L"property delta: mismatched start did not preserve failed undo history");
	if (guardedPropertyControl && guardedPropertyControl->ControlInstance)
		(void)WriteControlStringProperty(
			guardedPropertyControl->ControlInstance,
			L"AutomationProperties.Name", L"GuardedDelta");
	AppendFailure(failures,
		guardedPropertyCanvas.UndoCommand().HasChanges()
		&& guardedPropertyControl
		&& ReadControlStringProperty(
			guardedPropertyControl->ControlInstance,
			L"AutomationProperties.Name")
			== guardedOriginalAutomationName,
		L"property delta: guarded undo did not recover after start was repaired");

	DesignerCanvas interactionCanvas(0, 0, 800, 640);
	interactionCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 160, 160 });
	const auto interactionControlName = interactionCanvas.GetSelectedControl()
		? interactionCanvas.GetSelectedControl()->Name : std::wstring{};
	auto getInteractionControl = [&interactionCanvas,
		&interactionControlName]() -> std::shared_ptr<DesignerControl>
	{
		return FindControl(interactionCanvas, interactionControlName);
	};
	const auto interactionIdentityBeforeNudge = getInteractionControl();
	DesignerModel::DesignDocument interactionBaseline;
	std::wstring interactionCaptureError;
	AppendFailure(failures,
		interactionCanvas.BuildDesignDocument(
			interactionBaseline, &interactionCaptureError),
		L"canvas interaction: baseline capture failed");
	size_t interactionEventCount = 0;
	DesignerCanvasInteractionTransactionEventArgs lastInteractionEvent;
	interactionCanvas.OnInteractionTransactionCompleted +=
		[&interactionEventCount, &lastInteractionEvent](
			const DesignerCanvasInteractionTransactionEventArgs& args)
		{
			++interactionEventCount;
			lastInteractionEvent = args;
		};

	auto nudgeResult = interactionCanvas.NudgeSelectionBy(1, 0);
	auto secondNudgeResult = interactionCanvas.NudgeSelectionBy(1, 0);
	DesignerModel::DesignDocument nudgedDocument;
	std::wstring nudgedCaptureError;
	const bool nudgedCaptured = interactionCanvas.BuildDesignDocument(
		nudgedDocument, &nudgedCaptureError);
	AppendFailure(failures,
		nudgeResult.State == DesignerDocumentTransactionState::Committed
		&& secondNudgeResult.State == DesignerDocumentTransactionState::Committed
		&& interactionCanvas.HasInteractionTransactionResult()
		&& interactionCanvas.GetLastInteractionTransaction()
			== L"MoveSelection"
		&& interactionCanvas.GetLastInteractionTransactionResult().State
			== DesignerDocumentTransactionState::Committed
		&& interactionEventCount == 2
		&& lastInteractionEvent.Operation == L"MoveSelection"
		&& nudgedCaptured
		&& nudgedDocument != interactionBaseline
		&& interactionCanvas.GetUndoCommandCount() == 1
		&& interactionCanvas.GetCommandHistoryMemoryUsage() > 0
		&& interactionCanvas.GetCommandHistoryMemoryUsage() < 32768,
		L"canvas interaction: consecutive nudges were not merged and published");
	AppendFailure(failures,
		interactionCanvas.UndoCommand(),
		L"canvas interaction: nudge undo unavailable");
	DesignerModel::DesignDocument restoredNudgeDocument;
	std::wstring restoredNudgeError;
	AppendFailure(failures,
		interactionCanvas.BuildDesignDocument(
			restoredNudgeDocument, &restoredNudgeError)
		&& restoredNudgeDocument == interactionBaseline
		&& getInteractionControl() == interactionIdentityBeforeNudge,
		L"canvas interaction: nudge undo did not restore baseline");
	AppendFailure(failures,
		interactionCanvas.RedoCommand(),
		L"canvas interaction: merged nudge redo unavailable");
	DesignerModel::DesignDocument redoneNudgeDocument;
	std::wstring redoneNudgeError;
	AppendFailure(failures,
		interactionCanvas.BuildDesignDocument(
			redoneNudgeDocument, &redoneNudgeError)
		&& redoneNudgeDocument == nudgedDocument
		&& getInteractionControl() == interactionIdentityBeforeNudge,
		L"canvas interaction: merged nudge redo did not restore final state");
	AppendFailure(failures,
		interactionCanvas.UndoCommand(),
		L"canvas interaction: second merged nudge undo unavailable");
	AppendFailure(failures,
		interactionCanvas.RedoCommand().HasChanges(),
		L"placement delta: setup redo unavailable");
	const auto interactionIdentityBeforeSubtreeDelta =
		getInteractionControl();
	const auto addAfterPlacementDelta = interactionCanvas.AdoptVisualChildToCanvas(
		UIClass::UI_Label, POINT{ 420, 260 });
	const auto undoSnapshotAfterPlacement = interactionCanvas.UndoCommand();
	const auto interactionIdentityAfterSubtreeDelta =
		getInteractionControl();
	const auto undoPlacementAfterSubtree = interactionCanvas.UndoCommand();
	AppendFailure(failures,
		addAfterPlacementDelta.HasChanges()
		&& undoSnapshotAfterPlacement.HasChanges()
		&& interactionIdentityAfterSubtreeDelta
		&& interactionIdentityAfterSubtreeDelta
			== interactionIdentityBeforeSubtreeDelta
		&& undoPlacementAfterSubtree.HasChanges(),
		L"placement delta: target resolution failed after Add subtree undo"
			+ std::wstring(L" [add=")
			+ addAfterPlacementDelta.Error
			+ L", subtreeUndo=" + undoSnapshotAfterPlacement.Error
			+ L", placementUndo=" + undoPlacementAfterSubtree.Error
			+ L"]");
	DesignerModel::DesignDocument placementAfterRebuildUndo;
	std::wstring placementAfterRebuildError;
	const bool capturedPlacementAfterRebuild =
		interactionCanvas.BuildDesignDocument(
			placementAfterRebuildUndo, &placementAfterRebuildError);
	AppendFailure(failures,
		capturedPlacementAfterRebuild
		&& EquivalentDocumentContent(
			placementAfterRebuildUndo, interactionBaseline)
		&& placementAfterRebuildUndo.NextStableId
			> interactionBaseline.NextStableId,
		L"placement delta: rebuilt target did not return to baseline");
	if (capturedPlacementAfterRebuild
		&& EquivalentDocumentContent(
			placementAfterRebuildUndo, interactionBaseline))
	{
		// The temporary control consumed an ID even though its addition was undone.
		interactionBaseline.NextStableId =
			placementAfterRebuildUndo.NextStableId;
	}
	const auto guardedPlacementRedo = interactionCanvas.RedoCommand();
	auto guardedPlacementControl = getInteractionControl();
	const cui::core::Point guardedPlacementLocation = guardedPlacementControl
		? cui::core::Point{
			Canvas::GetLeft(*(guardedPlacementControl->ControlInstance)),
			Canvas::GetTop(*(guardedPlacementControl->ControlInstance)) }
		: cui::core::Point{};
	const auto guardedPlacementMemory =
		interactionCanvas.GetCommandHistoryMemoryUsage();
	if (guardedPlacementControl && guardedPlacementControl->ControlInstance)
	{
		Canvas::SetLeft(*(guardedPlacementControl->ControlInstance), guardedPlacementLocation.x + 7.0f);
	}
	const auto rejectedPlacementUndo = interactionCanvas.UndoCommand();
	AppendFailure(failures,
		guardedPlacementRedo.HasChanges()
		&& !rejectedPlacementUndo
		&& interactionCanvas.GetUndoCommandCount() == 1
		&& interactionCanvas.GetCommandHistoryMemoryUsage()
			== guardedPlacementMemory,
		L"placement delta: mismatched start did not preserve failed undo history");
	if (guardedPlacementControl && guardedPlacementControl->ControlInstance)
	{
		Canvas::SetLeft(*(guardedPlacementControl->ControlInstance), guardedPlacementLocation.x);
		Canvas::SetTop(*(guardedPlacementControl->ControlInstance), guardedPlacementLocation.y);
	}
	AppendFailure(failures,
		interactionCanvas.UndoCommand().HasChanges(),
		L"placement delta: guarded undo did not recover after start was repaired");

	auto dragControl = getInteractionControl();
	AppendFailure(failures,
		dragControl && dragControl->ControlInstance,
		L"canvas interaction: drag target unavailable after undo");
	if (dragControl && dragControl->ControlInstance)
	{
		auto* runtime = dragControl->ControlInstance;
		const auto size = runtime->GetActualSizeDip();
		const POINT center = RoundedPoint(runtime->GetAbsoluteLocationDip().x - interactionCanvas.GetAbsoluteLocationDip().x
				+ size.width / 2, runtime->GetAbsoluteLocationDip().y - interactionCanvas.GetAbsoluteLocationDip().y
				+ size.height / 2);
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, PointerInput(
			InputReportKind::PointerDown, MouseButton::Left,
			center.x, center.y, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, PointerInput(
			InputReportKind::PointerMove, MouseButton::None,
			center.x + 15, center.y + 9, MouseButton::Left));
		DesignerModel::DesignDocument dragPreviewDocument;
		std::wstring dragPreviewError;
		AppendFailure(failures,
			interactionCanvas.BuildDesignDocument(
				dragPreviewDocument, &dragPreviewError)
			&& dragPreviewDocument != interactionBaseline,
			L"canvas interaction: drag preview did not mutate document");
		const auto previewRedoCount = interactionCanvas.GetRedoCommandCount();
		const auto blockedPreviewUndo = interactionCanvas.UndoCommand();
		const auto blockedPreviewSavePoint =
			interactionCanvas.MarkDocumentSaved();
		AppendFailure(failures,
			interactionCanvas.HasActiveDocumentTransaction()
			&& blockedPreviewUndo.State
				== DesignerDocumentTransactionState::Rejected
			&& blockedPreviewSavePoint.State
				== DesignerDocumentTransactionState::Rejected
			&& interactionCanvas.GetRedoCommandCount() == previewRedoCount,
			L"placement preview: history/save-point operations were not rejected");
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, LifecycleInput(
			InputReportKind::Cancel, center.x + 15, center.y + 9));
		DesignerModel::DesignDocument canceledDragDocument;
		std::wstring canceledDragError;
		AppendFailure(failures,
			interactionCanvas.BuildDesignDocument(
				canceledDragDocument, &canceledDragError)
			&& canceledDragDocument == interactionBaseline
			&& interactionCanvas.GetLastInteractionTransaction()
				== L"MoveSelection"
			&& interactionCanvas.GetLastInteractionTransactionResult().State
				== DesignerDocumentTransactionState::RolledBack
			&& interactionEventCount == 3
			&& lastInteractionEvent.Result.State
				== DesignerDocumentTransactionState::RolledBack
			&& !lastInteractionEvent.Message.empty(),
			L"canvas interaction: canceled drag was not restored and reported");
	}
	AppendFailure(failures,
		IsUnchanged(interactionCanvas.UndoCommand()),
		L"canvas interaction: canceled drag entered undo history");
	AppendFailure(failures, interactionCanvas.RedoCommand(),
		L"canvas interaction: canceled drag destroyed prior redo history");
	AppendFailure(failures, interactionCanvas.UndoCommand(),
		L"canvas interaction: restored redo could not be undone");

	auto resizeControl = getInteractionControl();
	if (resizeControl && resizeControl->ControlInstance)
	{
		auto* runtime = resizeControl->ControlInstance;
		const auto size = runtime->GetActualSizeDip();
		const POINT bottomRight = RoundedPoint(runtime->GetAbsoluteLocationDip().x - interactionCanvas.GetAbsoluteLocationDip().x
				+ size.width, runtime->GetAbsoluteLocationDip().y - interactionCanvas.GetAbsoluteLocationDip().y
				+ size.height);
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, PointerInput(
			InputReportKind::PointerDown, MouseButton::Left,
			bottomRight.x, bottomRight.y, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, PointerInput(
			InputReportKind::PointerMove, MouseButton::None,
			bottomRight.x + 12, bottomRight.y + 8, MouseButton::Left));
		DesignerModel::DesignDocument resizePreviewDocument;
		std::wstring resizePreviewError;
		AppendFailure(failures,
			interactionCanvas.BuildDesignDocument(
				resizePreviewDocument, &resizePreviewError)
			&& resizePreviewDocument != interactionBaseline,
			L"canvas interaction: resize preview did not mutate document");
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, KeyInput(
			InputReportKind::KeyDown, Key::Escape));
		DesignerModel::DesignDocument canceledResizeDocument;
		std::wstring canceledResizeError;
		AppendFailure(failures,
			interactionCanvas.BuildDesignDocument(
				canceledResizeDocument, &canceledResizeError)
			&& canceledResizeDocument == interactionBaseline
			&& interactionCanvas.GetLastInteractionTransaction()
				== L"ResizeSelection"
			&& interactionCanvas.GetLastInteractionTransactionResult().State
				== DesignerDocumentTransactionState::RolledBack
			&& interactionEventCount == 4
			&& !lastInteractionEvent.Message.empty(),
			L"canvas interaction: Escape did not restore resize preview");
	}
	AppendFailure(failures,
		IsUnchanged(interactionCanvas.UndoCommand()),
		L"canvas interaction: canceled resize entered undo history");

	auto committedDragControl = getInteractionControl();
	if (committedDragControl && committedDragControl->ControlInstance)
	{
		auto* runtime = committedDragControl->ControlInstance;
		const auto size = runtime->GetActualSizeDip();
		const POINT center = RoundedPoint(runtime->GetAbsoluteLocationDip().x - interactionCanvas.GetAbsoluteLocationDip().x
				+ size.width / 2, runtime->GetAbsoluteLocationDip().y - interactionCanvas.GetAbsoluteLocationDip().y
				+ size.height / 2);
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, PointerInput(
			InputReportKind::PointerDown, MouseButton::Left,
			center.x, center.y, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, PointerInput(
			InputReportKind::PointerMove, MouseButton::None,
			center.x + 8, center.y + 6, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, PointerInput(
			InputReportKind::PointerUp, MouseButton::Left,
			center.x + 8, center.y + 6));
		DesignerModel::DesignDocument committedDragDocument;
		std::wstring committedDragError;
		AppendFailure(failures,
			interactionCanvas.BuildDesignDocument(
				committedDragDocument, &committedDragError)
			&& committedDragDocument != interactionBaseline
			&& getInteractionControl() == committedDragControl
			&& interactionCanvas.GetCommandHistoryMemoryUsage() > 0
			&& interactionCanvas.GetCommandHistoryMemoryUsage() < 32768
			&& interactionCanvas.GetLastInteractionTransaction()
				== L"MoveSelection"
			&& interactionCanvas.GetLastInteractionTransactionResult().State
				== DesignerDocumentTransactionState::Committed
			&& interactionEventCount == 5,
			L"canvas interaction: mouse-up did not commit drag transaction");
		AppendFailure(failures,
			interactionCanvas.UndoCommand(),
			L"canvas interaction: committed drag undo unavailable");
		DesignerModel::DesignDocument undoneDragDocument;
		std::wstring undoneDragError;
		AppendFailure(failures,
			interactionCanvas.BuildDesignDocument(
				undoneDragDocument, &undoneDragError)
			&& undoneDragDocument == interactionBaseline
			&& getInteractionControl() == committedDragControl,
			L"canvas interaction: committed drag undo missed baseline");
	}
	else
	{
		AppendFailure(failures, false,
			L"canvas interaction: committed drag target unavailable");
	}

	auto committedResizeControl = getInteractionControl();
	if (committedResizeControl && committedResizeControl->ControlInstance)
	{
		auto* runtime = committedResizeControl->ControlInstance;
		const auto size = runtime->GetActualSizeDip();
		const POINT bottomRight = RoundedPoint(runtime->GetAbsoluteLocationDip().x - interactionCanvas.GetAbsoluteLocationDip().x
				+ size.width, runtime->GetAbsoluteLocationDip().y - interactionCanvas.GetAbsoluteLocationDip().y
				+ size.height);
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, PointerInput(
			InputReportKind::PointerDown, MouseButton::Left,
			bottomRight.x, bottomRight.y, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, PointerInput(
			InputReportKind::PointerMove, MouseButton::None,
			bottomRight.x + 14, bottomRight.y + 10, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(interactionCanvas, PointerInput(
			InputReportKind::PointerUp, MouseButton::Left,
			bottomRight.x + 14, bottomRight.y + 10));
		DesignerModel::DesignDocument committedResizeDocument;
		std::wstring committedResizeError;
		AppendFailure(failures,
			interactionCanvas.BuildDesignDocument(
				committedResizeDocument, &committedResizeError)
			&& committedResizeDocument != interactionBaseline
			&& getInteractionControl() == committedResizeControl
			&& interactionCanvas.GetCommandHistoryMemoryUsage() > 0
			&& interactionCanvas.GetCommandHistoryMemoryUsage() < 32768
			&& interactionCanvas.GetLastInteractionTransaction()
				== L"ResizeSelection"
			&& interactionCanvas.GetLastInteractionTransactionResult().State
				== DesignerDocumentTransactionState::Committed,
			L"canvas interaction: committed resize did not use a small delta");
		AppendFailure(failures,
			interactionCanvas.UndoCommand().HasChanges(),
			L"canvas interaction: committed resize undo unavailable");
		DesignerModel::DesignDocument undoneResizeDocument;
		std::wstring undoneResizeError;
		AppendFailure(failures,
			interactionCanvas.BuildDesignDocument(
				undoneResizeDocument, &undoneResizeError)
			&& undoneResizeDocument == interactionBaseline
			&& getInteractionControl() == committedResizeControl,
			L"canvas interaction: resize delta did not restore baseline identity");
	}
	else
	{
		AppendFailure(failures, false,
			L"canvas interaction: committed resize target unavailable");
	}

	DesignerCanvas reparentCanvas(0, 0, 800, 640);
	reparentCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 220, 190 });
	auto reparentContainer = reparentCanvas.GetSelectedControl();
	const auto reparentContainerName = reparentContainer
		? reparentContainer->Name : std::wstring{};
	if (reparentContainer && reparentContainer->ControlInstance)
	{
		auto* containerRuntime = reparentContainer->ControlInstance;
		const POINT insideContainer = RoundedPoint(containerRuntime->GetAbsoluteLocationDip().x - reparentCanvas.GetAbsoluteLocationDip().x + 70, containerRuntime->GetAbsoluteLocationDip().y - reparentCanvas.GetAbsoluteLocationDip().y + 60);
		reparentCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, insideContainer);
	}
	auto reparentTarget = reparentCanvas.GetSelectedControl();
	auto reparentContentRoot = FindControl(reparentCanvas, L"contentRoot");
	const auto reparentTargetName = reparentTarget
		? reparentTarget->Name : std::wstring{};
	DesignerModel::DesignDocument reparentBaseline;
	std::wstring reparentBaselineError;
	const bool reparentSetup = reparentContainer
		&& reparentContainer->ControlInstance
		&& reparentTarget && reparentTarget->ControlInstance
		&& reparentTarget->DesignerParent
			== reparentContainer->ControlInstance
		&& reparentCanvas.BuildDesignDocument(
			reparentBaseline, &reparentBaselineError);
	AppendFailure(failures, reparentSetup,
		L"placement tree delta: nested setup failed");
	DesignerModel::DesignDocument reparentedDocument;
	std::shared_ptr<DesignerControl> reparentIdentity = reparentTarget;
	if (reparentSetup)
	{
		auto* runtime = reparentTarget->ControlInstance;
		const auto size = runtime->GetActualSizeDip();
		const POINT center = RoundedPoint(runtime->GetAbsoluteLocationDip().x - reparentCanvas.GetAbsoluteLocationDip().x
				+ size.width / 2, runtime->GetAbsoluteLocationDip().y - reparentCanvas.GetAbsoluteLocationDip().y
				+ size.height / 2);
		const POINT rootDrop{ 650, 500 };
		(void)cui::framework::InputAccess::DispatchInput(reparentCanvas, PointerInput(
			InputReportKind::PointerDown, MouseButton::Left,
			center.x, center.y, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(reparentCanvas, PointerInput(
			InputReportKind::PointerMove, MouseButton::None,
			rootDrop.x, rootDrop.y, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(reparentCanvas, PointerInput(
			InputReportKind::PointerUp, MouseButton::Left,
			rootDrop.x, rootDrop.y));
		std::wstring reparentedError;
		auto moved = FindControl(reparentCanvas, reparentTargetName);
		AppendFailure(failures,
			reparentCanvas.BuildDesignDocument(
				reparentedDocument, &reparentedError)
			&& reparentedDocument != reparentBaseline
			&& moved == reparentIdentity
			&& moved && reparentContentRoot
			&& moved->DesignerParent
				== reparentContentRoot->ControlInstance
			&& moved->ControlInstance->GetVisualParent()
				!= reparentContainer->ControlInstance
			&& reparentCanvas.GetUndoCommandCount() == 1
			&& reparentCanvas.GetCommandHistoryMemoryUsage() > 0
			&& reparentCanvas.GetCommandHistoryMemoryUsage() < 32768,
			L"placement tree delta: drag reparent retained a full snapshot");

		Control* rootParent = moved && moved->ControlInstance
			? moved->ControlInstance->GetVisualParent() : nullptr;
		if (moved && moved->ControlInstance && rootParent
			&& reparentContainer->ControlInstance)
		{
			auto owner = rootParent->DetachVisualChild(moved->ControlInstance);
			if (owner)
				reparentContainer->ControlInstance->AddOwned(std::move(owner));
			moved->DesignerParent = reparentContainer->ControlInstance;
		}
		const size_t guardedTreeMemory =
			reparentCanvas.GetCommandHistoryMemoryUsage();
		const auto rejectedTreeUndo = reparentCanvas.UndoCommand();
		AppendFailure(failures,
			!rejectedTreeUndo
			&& reparentCanvas.GetUndoCommandCount() == 1
			&& reparentCanvas.GetCommandHistoryMemoryUsage()
				== guardedTreeMemory,
			L"placement tree delta: mismatched parent lost undo history");
		moved = FindControl(reparentCanvas, reparentTargetName);
		if (moved && moved->ControlInstance && rootParent
			&& moved->ControlInstance->GetVisualParent()
				== reparentContainer->ControlInstance)
		{
			auto owner = reparentContainer->ControlInstance->DetachVisualChild(
				moved->ControlInstance);
			if (owner) rootParent->AddOwned(std::move(owner));
			moved->DesignerParent = reparentContentRoot
				? reparentContentRoot->ControlInstance : nullptr;
		}
		AppendFailure(failures,
			reparentCanvas.UndoCommand().HasChanges(),
			L"placement tree delta: guarded undo did not recover after repair");
		DesignerModel::DesignDocument reparentUndone;
		std::wstring reparentUndoneError;
		moved = FindControl(reparentCanvas, reparentTargetName);
		AppendFailure(failures,
			reparentCanvas.BuildDesignDocument(
				reparentUndone, &reparentUndoneError)
			&& reparentUndone == reparentBaseline
			&& moved == reparentIdentity
			&& moved && moved->DesignerParent
				== reparentContainer->ControlInstance,
			L"placement tree delta: undo did not restore parent and order");
		AppendFailure(failures,
			reparentCanvas.RedoCommand().HasChanges(),
			L"placement tree delta: redo unavailable");

		const auto identityBeforeTreeSubtreeDelta =
			FindControl(reparentCanvas, reparentTargetName);
		const auto addAfterTreeDelta = reparentCanvas.AdoptVisualChildToCanvas(
			UIClass::UI_Label, POINT{ 700, 180 });
		const auto undoAfterTreeSnapshot = reparentCanvas.UndoCommand();
		const auto identityAfterTreeSubtreeDelta =
			FindControl(reparentCanvas, reparentTargetName);
		const auto undoTreeAfterSubtree = reparentCanvas.UndoCommand();
		AppendFailure(failures,
			addAfterTreeDelta.HasChanges()
			&& undoAfterTreeSnapshot.HasChanges()
			&& identityAfterTreeSubtreeDelta
			&& identityAfterTreeSubtreeDelta == identityBeforeTreeSubtreeDelta
			&& undoTreeAfterSubtree.HasChanges(),
			L"placement tree delta: target resolution failed after Add subtree undo"
				+ std::wstring(L" [add=") + addAfterTreeDelta.Error
				+ L", subtreeUndo=" + undoAfterTreeSnapshot.Error
				+ L", placementUndo=" + undoTreeAfterSubtree.Error
				+ L"]");
		DesignerModel::DesignDocument treeAfterRebuildUndo;
		std::wstring treeAfterRebuildError;
		auto rebuiltTarget = FindControl(reparentCanvas, reparentTargetName);
		auto rebuiltContainer = FindControl(
			reparentCanvas, reparentContainerName);
		AppendFailure(failures,
			reparentCanvas.BuildDesignDocument(
				treeAfterRebuildUndo, &treeAfterRebuildError)
			&& EquivalentDocumentContent(
				treeAfterRebuildUndo, reparentBaseline)
			&& treeAfterRebuildUndo.NextStableId
				> reparentBaseline.NextStableId
			&& rebuiltTarget && rebuiltContainer
			&& rebuiltTarget->DesignerParent
				== rebuiltContainer->ControlInstance,
			L"placement tree delta: rebuilt undo missed original hierarchy");
		AppendFailure(failures,
			reparentCanvas.RedoCommand().HasChanges(),
			L"placement tree delta: rebuilt redo unavailable");
		DesignerModel::DesignDocument treeAfterRebuildRedo;
		std::wstring treeAfterRebuildRedoError;
		AppendFailure(failures,
			reparentCanvas.BuildDesignDocument(
				treeAfterRebuildRedo, &treeAfterRebuildRedoError)
			&& EquivalentDocumentContent(
				treeAfterRebuildRedo, reparentedDocument)
			&& treeAfterRebuildRedo.NextStableId
				== treeAfterRebuildUndo.NextStableId,
			L"placement tree delta: rebuilt redo missed reparented hierarchy");
	}

	DesignerCanvas reorderCanvas(0, 0, 800, 640);
	reorderCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 230, 190 });
	auto reorderContainer = reorderCanvas.GetSelectedControl();
	std::vector<std::wstring> reorderButtonNames;
	if (reorderContainer && reorderContainer->ControlInstance)
	{
		auto* runtime = reorderContainer->ControlInstance;
		const POINT firstDrop = RoundedPoint(runtime->GetAbsoluteLocationDip().x - reorderCanvas.GetAbsoluteLocationDip().x + 70, runtime->GetAbsoluteLocationDip().y - reorderCanvas.GetAbsoluteLocationDip().y + 45);
		const POINT secondDrop{ firstDrop.x, firstDrop.y + 55 };
		reorderCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, firstDrop);
		reorderCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, secondDrop);
		for (const auto& control : reorderCanvas.GetAllControls())
			if (control && control->Type == UIClass::UI_Button)
				reorderButtonNames.push_back(control->Name);
	}
	DesignerModel::DesignDocument reorderBaseline;
	std::wstring reorderBaselineError;
	auto firstReorderButton = reorderButtonNames.size() >= 2
		? FindControl(reorderCanvas, reorderButtonNames[0]) : nullptr;
	auto secondReorderButton = reorderButtonNames.size() >= 2
		? FindControl(reorderCanvas, reorderButtonNames[1]) : nullptr;
	const bool reorderSetup = reorderContainer
		&& reorderContainer->ControlInstance
		&& firstReorderButton && firstReorderButton->ControlInstance
		&& secondReorderButton && secondReorderButton->ControlInstance
		&& firstReorderButton->ControlInstance->GetVisualParent()
			== reorderContainer->ControlInstance
		&& firstReorderButton->ControlInstance->GetVisualParent()->IndexOfVisualChild(
			firstReorderButton->ControlInstance) == 0
		&& reorderCanvas.BuildDesignDocument(
			reorderBaseline, &reorderBaselineError);
	AppendFailure(failures, reorderSetup,
		L"placement tree delta: reorder setup failed");
	if (reorderSetup)
	{
		auto* firstRuntime = firstReorderButton->ControlInstance;
		auto* secondRuntime = secondReorderButton->ControlInstance;
		const auto firstSize = firstRuntime->GetActualSizeDip();
		const auto secondSize = secondRuntime->GetActualSizeDip();
		const POINT firstCenter = RoundedPoint(firstRuntime->GetAbsoluteLocationDip().x - reorderCanvas.GetAbsoluteLocationDip().x
				+ firstSize.width / 2, firstRuntime->GetAbsoluteLocationDip().y - reorderCanvas.GetAbsoluteLocationDip().y
				+ firstSize.height / 2);
		const POINT afterSecond = RoundedPoint(secondRuntime->GetAbsoluteLocationDip().x - reorderCanvas.GetAbsoluteLocationDip().x
				+ secondSize.width / 2, secondRuntime->GetAbsoluteLocationDip().y - reorderCanvas.GetAbsoluteLocationDip().y
				+ secondSize.height + 12);
		(void)cui::framework::InputAccess::DispatchInput(reorderCanvas, PointerInput(
			InputReportKind::PointerDown, MouseButton::Left,
			firstCenter.x, firstCenter.y, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(reorderCanvas, PointerInput(
			InputReportKind::PointerMove, MouseButton::None,
			afterSecond.x, afterSecond.y, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(reorderCanvas, PointerInput(
			InputReportKind::PointerUp, MouseButton::Left,
			afterSecond.x, afterSecond.y));
		DesignerModel::DesignDocument reorderedDocument;
		std::wstring reorderedError;
		AppendFailure(failures,
			reorderCanvas.BuildDesignDocument(
				reorderedDocument, &reorderedError)
			&& reorderedDocument != reorderBaseline
			&& firstRuntime->GetVisualParent() == reorderContainer->ControlInstance
			&& firstRuntime->GetVisualParent()->IndexOfVisualChild(firstRuntime) == 1
			&& reorderCanvas.GetUndoCommandCount() == 1
			&& reorderCanvas.GetCommandHistoryMemoryUsage() > 0
			&& reorderCanvas.GetCommandHistoryMemoryUsage() < 32768,
			L"placement tree delta: container reorder was not a small delta");
		AppendFailure(failures,
			reorderCanvas.UndoCommand().HasChanges()
			&& firstRuntime->GetVisualParent() == reorderContainer->ControlInstance
			&& firstRuntime->GetVisualParent()->IndexOfVisualChild(firstRuntime) == 0,
			L"placement tree delta: reorder undo missed sibling order");
		DesignerModel::DesignDocument reorderUndone;
		std::wstring reorderUndoneError;
		AppendFailure(failures,
			reorderCanvas.BuildDesignDocument(
				reorderUndone, &reorderUndoneError)
			&& reorderUndone == reorderBaseline
			&& firstReorderButton
				== FindControl(reorderCanvas, reorderButtonNames[0]),
			L"placement tree delta: reorder undo rebuilt instances");
		AppendFailure(failures,
			reorderCanvas.RedoCommand().HasChanges()
			&& firstRuntime->GetVisualParent()->IndexOfVisualChild(firstRuntime) == 1,
			L"placement tree delta: reorder redo missed sibling order");
	}

	DesignerCanvas commandCanvas(0, 0, 800, 640);
	size_t commandEventCount = 0;
	DesignerCanvasCommandEventArgs lastCommandEvent;
	commandCanvas.OnCommandCompleted +=
		[&commandEventCount, &lastCommandEvent](
			const DesignerCanvasCommandEventArgs& args)
		{
			++commandEventCount;
			lastCommandEvent = args;
		};
	auto addCommandResult = commandCanvas.AdoptVisualChildToCanvas(
		UIClass::UI_Button, POINT{ 120, 120 });
	AppendFailure(failures,
		addCommandResult.State
			== DesignerDocumentTransactionState::Committed
		&& commandCanvas.GetAllControls().size() == 2
		&& commandCanvas.HasCommandResult()
		&& commandCanvas.GetLastCommandOperation() == L"AdoptVisualChild"
		&& commandCanvas.GetLastCommandLabel() == L"AdoptVisualChild"
		&& commandCanvas.GetLastCommandResult().State
			== DesignerDocumentTransactionState::Committed
		&& commandEventCount == 1
		&& lastCommandEvent.Operation == L"AdoptVisualChild"
		&& lastCommandEvent.Label == L"AdoptVisualChild",
		L"add command: commit result or event was not published");
	const auto addedName = commandCanvas.GetSelectedControl()
		? commandCanvas.GetSelectedControl()->Name : std::wstring{};
	const auto addedIdentity = commandCanvas.GetSelectedControl();
	auto* const addedRuntimeIdentity = addedIdentity
		? addedIdentity->ControlInstance : nullptr;
	const auto addCommandMemory =
		commandCanvas.GetCommandHistoryMemoryUsage();
	AppendFailure(failures,
		addCommandMemory > 0 && addCommandMemory < 64 * 1024,
		L"add command: simple subtree retained document-sized history (bytes="
			+ std::to_wstring(addCommandMemory) + L")");
	auto undoAddResult = commandCanvas.UndoCommand();
	AppendFailure(failures,
		undoAddResult.HasChanges()
		&& commandEventCount == 2
		&& lastCommandEvent.Operation == L"Undo"
		&& lastCommandEvent.Label == L"AdoptVisualChild",
		L"add command: undo result or label was not published");
	AppendFailure(failures, commandCanvas.GetAllControls().size() == 1,
		L"add command: undo did not remove the control");
	auto redoAddResult = commandCanvas.RedoCommand();
	AppendFailure(failures,
		redoAddResult.HasChanges()
		&& commandEventCount == 3
		&& lastCommandEvent.Operation == L"Redo"
		&& lastCommandEvent.Label == L"AdoptVisualChild",
		L"add command: redo result or label was not published");
	AppendFailure(failures, commandCanvas.GetAllControls().size() == 2
		&& commandCanvas.GetSelectedControl()
		&& commandCanvas.GetSelectedControl()->Name == addedName
		&& commandCanvas.GetSelectedControl() == addedIdentity
		&& commandCanvas.GetSelectedControl()->ControlInstance
			== addedRuntimeIdentity,
		L"add command: redo did not restore identity and selection");

	const auto beforeDeleteMemory =
		commandCanvas.GetCommandHistoryMemoryUsage();
	auto deleteCommandResult = commandCanvas.DeleteSelectedControl();
	const auto deleteCommandMemory =
		commandCanvas.GetCommandHistoryMemoryUsage() - beforeDeleteMemory;
	AppendFailure(failures,
		deleteCommandResult.HasChanges()
		&& commandCanvas.GetAllControls().size() == 1
		&& deleteCommandMemory > 0 && deleteCommandMemory < 64 * 1024
		&& commandEventCount == 4
		&& lastCommandEvent.Operation == L"DeleteSelection"
		&& lastCommandEvent.Label == L"DeleteSelection",
		L"delete command: result or event was not published [error="
			+ deleteCommandResult.Error + L", bytes="
			+ std::to_wstring(deleteCommandMemory) + L", events="
			+ std::to_wstring(commandEventCount) + L"]");
	auto undoDeleteResult = commandCanvas.UndoCommand();
	AppendFailure(failures,
		undoDeleteResult.HasChanges()
		&& commandEventCount == 5
		&& lastCommandEvent.Operation == L"Undo"
		&& lastCommandEvent.Label == L"DeleteSelection",
		L"delete command: undo result or label was not published");
	AppendFailure(failures, commandCanvas.GetAllControls().size() == 2
		&& commandCanvas.GetSelectedControl()
		&& commandCanvas.GetSelectedControl()->Name == addedName
		&& commandCanvas.GetSelectedControl() == addedIdentity
		&& commandCanvas.GetSelectedControl()->ControlInstance
			== addedRuntimeIdentity,
		L"delete command: undo did not restore identity and selection");
	auto redoDeleteResult = commandCanvas.RedoCommand();
	AppendFailure(failures,
		redoDeleteResult.HasChanges()
		&& commandEventCount == 6
		&& lastCommandEvent.Operation == L"Redo"
		&& lastCommandEvent.Label == L"DeleteSelection",
		L"delete command: redo result or label was not published");
	AppendFailure(failures, commandCanvas.GetAllControls().size() == 1,
		L"delete command: redo did not remove the control");

	auto blockUndoBegin = commandCanvas.BeginDocumentEditTransaction(
		L"SelfTest:BlockUndo");
	auto blockedUndo = commandCanvas.UndoCommand();
	AppendFailure(failures,
		blockUndoBegin.State == DesignerDocumentTransactionState::Begun
		&& blockedUndo.State == DesignerDocumentTransactionState::Rejected
		&& !blockedUndo.Error.empty()
		&& commandEventCount == 7
		&& lastCommandEvent.Operation == L"Undo"
		&& lastCommandEvent.Label == L"DeleteSelection"
		&& commandCanvas.GetAllControls().size() == 1,
		L"active transaction: undo was not rejected without mutation");
	auto blockUndoCancel = commandCanvas.CancelDocumentEditTransaction();
	auto undoAfterBlock = commandCanvas.UndoCommand();
	AppendFailure(failures,
		blockUndoCancel.State == DesignerDocumentTransactionState::Canceled
		&& undoAfterBlock.HasChanges()
		&& commandEventCount == 8
		&& lastCommandEvent.Operation == L"Undo"
		&& lastCommandEvent.Label == L"DeleteSelection"
		&& commandCanvas.GetAllControls().size() == 2,
		L"active transaction: rejected undo damaged transaction or history");

	auto blockRedoBegin = commandCanvas.BeginDocumentEditTransaction(
		L"SelfTest:BlockRedo");
	auto blockedRedo = commandCanvas.RedoCommand();
	AppendFailure(failures,
		blockRedoBegin.State == DesignerDocumentTransactionState::Begun
		&& blockedRedo.State == DesignerDocumentTransactionState::Rejected
		&& !blockedRedo.Error.empty()
		&& commandEventCount == 9
		&& lastCommandEvent.Operation == L"Redo"
		&& lastCommandEvent.Label == L"DeleteSelection"
		&& commandCanvas.GetAllControls().size() == 2,
		L"active transaction: redo was not rejected without mutation");
	auto blockRedoCancel = commandCanvas.CancelDocumentEditTransaction();
	auto redoAfterBlock = commandCanvas.RedoCommand();
	AppendFailure(failures,
		blockRedoCancel.State == DesignerDocumentTransactionState::Canceled
		&& redoAfterBlock.HasChanges()
		&& commandEventCount == 10
		&& lastCommandEvent.Operation == L"Redo"
		&& lastCommandEvent.Label == L"DeleteSelection"
		&& commandCanvas.GetAllControls().size() == 1,
		L"active transaction: rejected redo damaged transaction or history");

	DesignerCanvas clipboardCanvas(0, 0, 900, 680);
	(void)clipboardCanvas.ResetDocumentHistoryAsSaved();
	const std::wstring clipboardXaml = LR"xaml(
		<Window xmlns="urn:cui"
		      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
		      x:Name="ClipboardWindow">
		  <Canvas x:Name="panel1" DesignId="1" Canvas.Left="40" Canvas.Top="50"
		         Width="240" Height="160">
		    <Button x:Name="button1" DesignId="2" Canvas.Left="10" Canvas.Top="12"
		            Width="100" Height="30" Content="Paste" />
		  </Canvas>
		</Window>)xaml";
	const auto pasteXaml = clipboardCanvas.PasteControlsFromXamlText(
		clipboardXaml);
	DesignerModel::DesignDocument pastedXamlDocument;
	std::wstring pastedXamlError;
	AppendFailure(failures,
		pasteXaml.HasChanges()
		&& clipboardCanvas.BuildDesignDocument(
			pastedXamlDocument, &pastedXamlError)
		&& pastedXamlDocument.Nodes.size() == 3
		&& clipboardCanvas.GetSelectedControls().size() == 1
		&& clipboardCanvas.GetSelectedControl()
		&& clipboardCanvas.GetSelectedControl()->Name == L"panel1"
		&& clipboardCanvas.GetUndoCommandCount() == 1
		&& clipboardCanvas.GetLastCommandOperation() == L"PasteSelection",
		L"clipboard XAML: paste did not commit one selected subtree");
	const auto pasteHistory = clipboardCanvas.GetUndoCommandCount();
	const auto rejectedClipboardXaml = clipboardCanvas.PasteControlsFromXamlText(
		L"<Button x:Name=\"broken\">");
	DesignerModel::DesignDocument afterRejectedClipboard;
	std::wstring afterRejectedClipboardError;
	AppendFailure(failures,
		!rejectedClipboardXaml.Succeeded()
		&& clipboardCanvas.GetUndoCommandCount() == pasteHistory
		&& clipboardCanvas.BuildDesignDocument(
			afterRejectedClipboard, &afterRejectedClipboardError)
		&& afterRejectedClipboard == pastedXamlDocument,
		L"clipboard XAML: invalid text mutated document or history");
	const auto undoPasteXaml = clipboardCanvas.UndoCommand();
	AppendFailure(failures,
		undoPasteXaml.HasChanges()
		&& clipboardCanvas.GetAllControls().size() == 1
		&& clipboardCanvas.GetSelectedControls().empty(),
		L"clipboard XAML: undo did not remove pasted subtree and selection");
	const auto redoPasteXaml = clipboardCanvas.RedoCommand();
	AppendFailure(failures,
		redoPasteXaml.HasChanges()
		&& clipboardCanvas.GetAllControls().size() == 3
		&& clipboardCanvas.GetSelectedControl()
		&& clipboardCanvas.GetSelectedControl()->Name == L"panel1",
		L"clipboard XAML: redo did not restore subtree selection");

	DesignerCanvas bindingClipboardSource(0, 0, 900, 680);
	std::wstring bindingClipboardSchemaError;
	const bool bindingClipboardSchemaReady =
		bindingClipboardSource.SetDataContextSchema({
			{ L"Profile", BindingValueKind::Object, true, false, true },
			{ L"Profile.DisplayName", BindingValueKind::String,
				true, false, true }
		}, &bindingClipboardSchemaError);
	bindingClipboardSource.AdoptVisualChildToCanvasCore(
		UIClass::UI_TextBox, POINT{ 210, 170 });
	const auto bindingClipboardSourceControl =
		bindingClipboardSource.GetSelectedControl();
	if (bindingClipboardSourceControl)
		bindingClipboardSourceControl->DataBindings[L"Text"] = {
			L"Profile.DisplayName", BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged, L"" };
	const auto bindingClipboardCopy = bindingClipboardSource.CopySelectedControls();

	DesignerCanvas bindingClipboardTarget(0, 0, 900, 680);
	bindingClipboardTarget.AdoptVisualChildToCanvasCore(
		UIClass::UI_Label, POINT{ 140, 120 });
	const auto bindingClipboardExisting = bindingClipboardTarget.GetSelectedControl();
	if (bindingClipboardExisting)
		bindingClipboardExisting->DataBindings[L"Text"] = {
			L"Existing.Caption", BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged, L"" };
	(void)bindingClipboardTarget.ResetDocumentHistoryAsSaved();
	const auto bindingClipboardPaste =
		bindingClipboardTarget.PasteControlsFromClipboardInPlace();
	DesignerModel::DesignDocument bindingClipboardMerged;
	std::wstring bindingClipboardMergeError;
	const bool bindingClipboardMergedCaptured =
		bindingClipboardTarget.BuildDesignDocument(
			bindingClipboardMerged, &bindingClipboardMergeError);
	const auto* bindingClipboardExistingPath =
		DesignerDataContextSchemaUtils::Find(
			bindingClipboardMerged.DataContextSchema, L"Existing.Caption");
	const auto* bindingClipboardImportedPath =
		DesignerDataContextSchemaUtils::Find(
			bindingClipboardMerged.DataContextSchema, L"Profile.DisplayName");
	const auto undoBindingClipboard = bindingClipboardTarget.UndoCommand();
	DesignerModel::DesignDocument bindingClipboardUndone;
	std::wstring bindingClipboardUndoError;
	const bool bindingClipboardUndoCaptured =
		bindingClipboardTarget.BuildDesignDocument(
			bindingClipboardUndone, &bindingClipboardUndoError);
	const auto redoBindingClipboard = bindingClipboardTarget.RedoCommand();
	DesignerModel::DesignDocument bindingClipboardRedone;
	std::wstring bindingClipboardRedoError;
	const bool bindingClipboardRedoCaptured =
		bindingClipboardTarget.BuildDesignDocument(
			bindingClipboardRedone, &bindingClipboardRedoError);
	AppendFailure(failures,
		bindingClipboardSchemaReady && bindingClipboardSourceControl
		&& bindingClipboardCopy.Succeeded()
		&& bindingClipboardExisting && bindingClipboardPaste.HasChanges()
		&& bindingClipboardMergedCaptured
		&& bindingClipboardMerged.DataContextSchema.size() == 4
		&& bindingClipboardExistingPath
		&& bindingClipboardExistingPath->ValueKind == BindingValueKind::Empty
		&& bindingClipboardImportedPath
		&& bindingClipboardImportedPath->ValueKind == BindingValueKind::String
		&& !bindingClipboardImportedPath->CanWrite
		&& bindingClipboardTarget.GetUndoCommandCount() == 1
		&& undoBindingClipboard.HasChanges() && bindingClipboardUndoCaptured
		&& bindingClipboardUndone.Nodes.size() == 2
		&& bindingClipboardUndone.DataContextSchema.empty()
		&& redoBindingClipboard.HasChanges() && bindingClipboardRedoCaptured
		&& bindingClipboardRedone == bindingClipboardMerged,
		L"clipboard bindings: schema dependencies did not survive cross-canvas copy/paste and Undo/Redo"
		+ std::wstring(L" [schema=") + SelfTestFlag(bindingClipboardSchemaReady)
		+ L", copy=" + SelfTestFlag(bindingClipboardCopy.Succeeded())
		+ L", paste=" + SelfTestFlag(bindingClipboardPaste.HasChanges())
		+ L", capture=" + SelfTestFlag(bindingClipboardMergedCaptured)
		+ L", schemaCount=" + std::to_wstring(
			bindingClipboardMerged.DataContextSchema.size())
		+ L", undo=" + SelfTestFlag(undoBindingClipboard.HasChanges())
		+ L", redo=" + SelfTestFlag(redoBindingClipboard.HasChanges())
		+ L", schemaError=" + bindingClipboardSchemaError
		+ L", mergeError=" + bindingClipboardMergeError
		+ L", undoError=" + bindingClipboardUndoError
		+ L", redoError=" + bindingClipboardRedoError + L"]");

	DesignerCanvas styleClipboardSource(0, 0, 900, 680);
	styleClipboardSource.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 210, 170 });
	const auto styleClipboardSourceControl =
		styleClipboardSource.GetSelectedControl();
	if (styleClipboardSourceControl && styleClipboardSourceControl->ControlInstance)
	{
		cui::framework::StyleAccess::SetResourceKey(
			*styleClipboardSourceControl->ControlInstance, L"SourceButton");
	}
	DesignerStyleSheet sourceClipboardStyle;
	sourceClipboardStyle.Resources = {
		{ L"Accent", { DesignerStyleValueKind::Color, L"#FFFF0000" } }
	};
	DesignerStyleRule sourceClipboardTyped;
	sourceClipboardTyped.HasType = true;
	sourceClipboardTyped.Type = UIClass::UI_Button;
	sourceClipboardTyped.Setters.push_back({
		L"BorderThickness", false, {},
		{ DesignerStyleValueKind::Thickness, L"2" } });
	DesignerStyleRule sourceClipboardId;
	sourceClipboardId.HasType = true;
	sourceClipboardId.Type = UIClass::UI_Button;
	sourceClipboardId.Id = L"SourceButton";
	sourceClipboardId.BasedOn = L"{x:Type Button}";
	sourceClipboardId.Setters.push_back({
		L"BorderThickness", false, {},
		{ DesignerStyleValueKind::Thickness, L"7" } });
	sourceClipboardId.Setters.push_back({
		L"Background", true, L"Accent", {} });
	sourceClipboardStyle.Rules = {
		sourceClipboardTyped, sourceClipboardId };
	std::wstring styleClipboardSourceError;
	const bool sourceClipboardStyleReady =
		styleClipboardSource.SetDocumentStyleSheet(
			sourceClipboardStyle, &styleClipboardSourceError);
	const auto styleClipboardCopy =
		styleClipboardSource.CopySelectedControls();

	DesignerCanvas styleClipboardTarget(0, 0, 900, 680);
	styleClipboardTarget.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 140, 120 });
	const auto styleClipboardExisting = styleClipboardTarget.GetSelectedControl();
	const auto styleClipboardExistingName = styleClipboardExisting
		? styleClipboardExisting->Name : std::wstring{};
	if (styleClipboardExisting && styleClipboardExisting->ControlInstance)
	{
		cui::framework::StyleAccess::SetResourceKey(
			*styleClipboardExisting->ControlInstance, L"SourceButton");
	}
	DesignerStyleSheet targetClipboardStyle;
	targetClipboardStyle.Resources = {
		{ L"Accent", { DesignerStyleValueKind::Color, L"#FF0000FF" } }
	};
	DesignerStyleRule targetClipboardId = sourceClipboardId;
	targetClipboardId.Setters.front().Literal.Text = L"99";
	targetClipboardStyle.Rules = {
		sourceClipboardTyped, targetClipboardId };
	std::wstring styleClipboardTargetError;
	const bool targetClipboardStyleReady =
		styleClipboardTarget.SetDocumentStyleSheet(
			targetClipboardStyle, &styleClipboardTargetError);
	DesignerModel::DesignDocument styleClipboardBaseline;
	std::wstring styleClipboardBaselineError;
	const bool styleClipboardBaselineCaptured =
		styleClipboardTarget.BuildDesignDocument(
			styleClipboardBaseline, &styleClipboardBaselineError);
	(void)styleClipboardTarget.ResetDocumentHistoryAsSaved();
	const auto styleClipboardPaste =
		styleClipboardTarget.PasteControlsFromClipboardInPlace();
	const auto styleClipboardPasted = styleClipboardTarget.GetSelectedControl();
	const auto styleClipboardExistingAfterPaste =
		FindControl(styleClipboardTarget, styleClipboardExistingName);
	const auto pastedPreviewButton = styleClipboardPasted
		? dynamic_cast<Button*>(styleClipboardPasted->ControlInstance)
		: nullptr;
	const auto existingPreviewButton = styleClipboardExistingAfterPaste
		? dynamic_cast<Button*>(
			styleClipboardExistingAfterPaste->ControlInstance)
		: nullptr;
	const auto pastedPreviewBorder = pastedPreviewButton
		? pastedPreviewButton->BorderThickness.MaxEdge() : -1.0f;
	const auto pastedPreviewRed = pastedPreviewButton
		? pastedPreviewButton->Background.Color.r : -1.0f;
	const auto pastedPreviewBlue = pastedPreviewButton
		? pastedPreviewButton->Background.Color.b : -1.0f;
	const auto existingPreviewBorder = existingPreviewButton
		? existingPreviewButton->BorderThickness.MaxEdge() : -1.0f;
	const auto existingPreviewRed = existingPreviewButton
		? existingPreviewButton->Background.Color.r : -1.0f;
	const auto existingPreviewBlue = existingPreviewButton
		? existingPreviewButton->Background.Color.b : -1.0f;
	const bool styleClipboardPreviewCorrect =
		styleClipboardPasted
		&& styleClipboardPasted != styleClipboardExistingAfterPaste
		&& styleClipboardPasted->ControlInstance
		&& cui::framework::StyleAccess::ResourceKey(
			*styleClipboardPasted->ControlInstance).starts_with(L"CuiPasteStyle_")
		&& pastedPreviewButton
		&& pastedPreviewButton->BorderThickness == Thickness(7.0f)
		&& pastedPreviewButton->Background.Color.r == 1.0f
		&& pastedPreviewButton->Background.Color.b == 0.0f
		&& existingPreviewButton
		&& existingPreviewButton->BorderThickness == Thickness(99.0f)
		&& existingPreviewButton->Background.Color.r == 0.0f
		&& existingPreviewButton->Background.Color.b == 1.0f;
	DesignerModel::DesignDocument styleClipboardMerged;
	std::wstring styleClipboardMergeError;
	const bool styleClipboardMergedCaptured =
		styleClipboardTarget.BuildDesignDocument(
			styleClipboardMerged, &styleClipboardMergeError);
	const auto undoStyleClipboard = styleClipboardTarget.UndoCommand();
	DesignerModel::DesignDocument styleClipboardUndone;
	std::wstring styleClipboardUndoError;
	const bool styleClipboardUndoCaptured =
		styleClipboardTarget.BuildDesignDocument(
			styleClipboardUndone, &styleClipboardUndoError);
	const auto redoStyleClipboard = styleClipboardTarget.RedoCommand();
	DesignerModel::DesignDocument styleClipboardRedone;
	std::wstring styleClipboardRedoError;
	const bool styleClipboardRedoCaptured =
		styleClipboardTarget.BuildDesignDocument(
			styleClipboardRedone, &styleClipboardRedoError);
	AppendFailure(failures,
		styleClipboardSourceControl && sourceClipboardStyleReady
		&& styleClipboardCopy.Succeeded()
		&& styleClipboardExisting && targetClipboardStyleReady
		&& styleClipboardBaselineCaptured
		&& styleClipboardPaste.HasChanges() && styleClipboardPreviewCorrect
		&& styleClipboardMergedCaptured
		&& styleClipboardMerged.Nodes.size() == 3
		&& styleClipboardMerged.StyleSheet.Resources.size() == 2
		&& styleClipboardMerged.StyleSheet.Rules.size() == 3
		&& styleClipboardTarget.GetUndoCommandCount() == 1
		&& undoStyleClipboard.HasChanges() && styleClipboardUndoCaptured
		&& styleClipboardUndone == styleClipboardBaseline
		&& redoStyleClipboard.HasChanges() && styleClipboardRedoCaptured
		&& styleClipboardRedone == styleClipboardMerged,
		L"clipboard styles: conflicting resources/selectors were not isolated across canvases or Undo/Redo"
		+ std::wstring(L" [sourceStyle=")
		+ SelfTestFlag(sourceClipboardStyleReady)
		+ L", copy=" + SelfTestFlag(styleClipboardCopy.Succeeded())
		+ L", targetStyle=" + SelfTestFlag(targetClipboardStyleReady)
		+ L", paste=" + SelfTestFlag(styleClipboardPaste.HasChanges())
		+ L", capture=" + SelfTestFlag(styleClipboardMergedCaptured)
		+ L", resources=" + std::to_wstring(
			styleClipboardMerged.StyleSheet.Resources.size())
		+ L", rules=" + std::to_wstring(
			styleClipboardMerged.StyleSheet.Rules.size())
		+ L", preview=" + SelfTestFlag(styleClipboardPreviewCorrect)
		+ L", pastedBorder=" + std::to_wstring(pastedPreviewBorder)
		+ L", pastedRed=" + std::to_wstring(pastedPreviewRed)
		+ L", pastedBlue=" + std::to_wstring(pastedPreviewBlue)
		+ L", existingBorder=" + std::to_wstring(existingPreviewBorder)
		+ L", existingRed=" + std::to_wstring(existingPreviewRed)
		+ L", existingBlue=" + std::to_wstring(existingPreviewBlue)
		+ L", undo=" + SelfTestFlag(undoStyleClipboard.HasChanges())
		+ L", undoEqual=" + SelfTestFlag(
			styleClipboardUndone == styleClipboardBaseline)
		+ L", redo=" + SelfTestFlag(redoStyleClipboard.HasChanges())
		+ L", redoEqual=" + SelfTestFlag(
			styleClipboardRedone == styleClipboardMerged)
		+ L", sourceError=" + styleClipboardSourceError
		+ L", targetError=" + styleClipboardTargetError
		+ L", mergeError=" + styleClipboardMergeError
		+ L", undoError=" + styleClipboardUndoError
		+ L", redoError=" + styleClipboardRedoError + L"]");
	DesignerModel::DesignDocument liveXamlBaseline;
	std::wstring liveXamlBaselineError;
	std::wstring liveXamlText;
	const bool liveXamlSetup = clipboardCanvas.BuildDesignDocument(
		liveXamlBaseline, &liveXamlBaselineError)
		&& clipboardCanvas.BuildXamlDocumentText(
			liveXamlText, &liveXamlBaselineError)
		&& clipboardCanvas.ResetDocumentHistoryAsSaved().Succeeded();
	const auto liveTextPosition = liveXamlText.find(L"Content=\"Paste\"");
	if (liveTextPosition != std::wstring::npos)
		liveXamlText.replace(
			liveTextPosition, std::wstring(L"Content=\"Paste\"").size(),
			L"Content=\"Live Preview\"");
	const auto liveNamePosition = liveXamlText.find(L"x:Name=\"panel1\"");
	if (liveNamePosition != std::wstring::npos)
		liveXamlText.replace(
			liveNamePosition, std::wstring(L"x:Name=\"panel1\"").size(),
			L"x:Name=\"renamedPanel\"");
	const auto beginLivePreview = clipboardCanvas.BeginDocumentEditTransaction(
		L"EditXaml");
	std::wstring livePreviewError;
	const bool appliedLivePreview = beginLivePreview.Succeeded()
		&& liveTextPosition != std::wstring::npos
		&& liveNamePosition != std::wstring::npos
		&& clipboardCanvas.PreviewXamlDocumentText(
			liveXamlText, &livePreviewError);
	const bool renamedSelectionPreserved =
		clipboardCanvas.GetSelectedControl()
		&& clipboardCanvas.GetSelectedControl()->Name == L"renamedPanel";
	DesignerModel::DesignDocument validLivePreview;
	std::wstring validLivePreviewError;
	const bool capturedLivePreview = clipboardCanvas.BuildDesignDocument(
		validLivePreview, &validLivePreviewError);
	DesignerModel::XamlDocumentDiagnostic invalidLiveDiagnostic;
	std::wstring invalidSyntaxError;
	const bool rejectedInvalidLivePreview =
		!clipboardCanvas.PreviewXamlDocumentText(
			L"<Window xmlns=\"urn:cui\">\n  <Broken>",
			&invalidSyntaxError, &invalidLiveDiagnostic);
	auto semanticLiveXaml = liveXamlText;
	auto semanticInsert = semanticLiveXaml.find(L"Width=\"240\"");
	const std::wstring semanticOriginal = L"Width=\"240\"";
	if (semanticInsert != std::wstring::npos)
		semanticLiveXaml.replace(
			semanticInsert, semanticOriginal.size(),
			L"Width=\"Vanished\"");
	DesignerModel::XamlDocumentDiagnostic semanticLiveDiagnostic;
	std::wstring semanticLiveError;
	const bool rejectedSemanticLivePreview =
		semanticInsert != std::wstring::npos
		&& !clipboardCanvas.PreviewXamlDocumentText(
			semanticLiveXaml, &semanticLiveError, &semanticLiveDiagnostic);
	const auto semanticExpectedOffset = semanticInsert;
	DesignerModel::DesignDocument afterInvalidLivePreview;
	std::wstring afterInvalidLivePreviewError;
	const bool invalidPreviewPreserved = clipboardCanvas.BuildDesignDocument(
		afterInvalidLivePreview, &afterInvalidLivePreviewError)
		&& afterInvalidLivePreview == validLivePreview;
	const auto rollbackLivePreview = clipboardCanvas.RollbackDocumentEditTransaction();
	DesignerModel::DesignDocument rolledBackLivePreview;
	std::wstring rolledBackLivePreviewError;
	AppendFailure(failures,
		liveXamlSetup && appliedLivePreview && renamedSelectionPreserved
		&& capturedLivePreview
		&& validLivePreview != liveXamlBaseline
		&& rejectedInvalidLivePreview
		&& invalidLiveDiagnostic.HasLocation()
		&& invalidLiveDiagnostic.HasSourceOffset()
		&& invalidLiveDiagnostic.Message == invalidSyntaxError
		&& rejectedSemanticLivePreview
		&& semanticLiveDiagnostic.HasLocation()
		&& semanticLiveDiagnostic.HasSourceOffset()
		&& semanticLiveDiagnostic.Utf16Offset == semanticExpectedOffset
		&& semanticLiveDiagnostic.Message == semanticLiveError
		&& invalidPreviewPreserved
		&& rollbackLivePreview.State
			== DesignerDocumentTransactionState::RolledBack
		&& clipboardCanvas.BuildDesignDocument(
			rolledBackLivePreview, &rolledBackLivePreviewError)
		&& rolledBackLivePreview == liveXamlBaseline,
		L"live XAML: invalid preview or cancel did not preserve the session baseline"
		+ std::wstring(L" [setup=") + SelfTestFlag(liveXamlSetup)
		+ L", apply=" + SelfTestFlag(appliedLivePreview)
		+ L", stableSelection=" + SelfTestFlag(renamedSelectionPreserved)
		+ L", capture=" + SelfTestFlag(capturedLivePreview)
		+ L", changed=" + SelfTestFlag(validLivePreview != liveXamlBaseline)
		+ L", reject=" + SelfTestFlag(rejectedInvalidLivePreview)
		+ L", located=" + SelfTestFlag(invalidLiveDiagnostic.HasLocation()
			&& invalidLiveDiagnostic.HasSourceOffset())
		+ L", semantic=" + SelfTestFlag(rejectedSemanticLivePreview
			&& semanticLiveDiagnostic.HasSourceOffset()
			&& semanticLiveDiagnostic.Utf16Offset == semanticExpectedOffset)
		+ L", preserve=" + SelfTestFlag(invalidPreviewPreserved)
		+ L", rollback=" + std::to_wstring(static_cast<int>(rollbackLivePreview.State))
		+ L", syntaxError=" + invalidSyntaxError
		+ L", semanticError=" + semanticLiveError
		+ L", rollbackError=" + rollbackLivePreview.Error + L"]");

	const auto beginCommittedLive = clipboardCanvas.BeginDocumentEditTransaction(
		L"EditXaml");
	const bool appliedCommittedLive = beginCommittedLive.Succeeded()
		&& clipboardCanvas.PreviewXamlDocumentText(
			liveXamlText, &livePreviewError);
	const auto commitLivePreview = clipboardCanvas.CommitDocumentEditTransaction();
	const auto liveCommitUndoCount = clipboardCanvas.GetUndoCommandCount();
	const auto undoLivePreview = clipboardCanvas.UndoCommand();
	DesignerModel::DesignDocument undoneLivePreview;
	std::wstring undoneLivePreviewError;
	const bool capturedUndoneLive = clipboardCanvas.BuildDesignDocument(
		undoneLivePreview, &undoneLivePreviewError);
	const auto redoLivePreview = clipboardCanvas.RedoCommand();
	DesignerModel::DesignDocument redoneLivePreview;
	std::wstring redoneLivePreviewError;
	AppendFailure(failures,
		appliedCommittedLive && commitLivePreview.HasChanges()
		&& liveCommitUndoCount == 1
		&& undoLivePreview.HasChanges() && capturedUndoneLive
		&& undoneLivePreview == liveXamlBaseline
		&& redoLivePreview.HasChanges()
		&& clipboardCanvas.BuildDesignDocument(
			redoneLivePreview, &redoneLivePreviewError)
		&& redoneLivePreview == validLivePreview,
		L"live XAML: one edit session did not commit as one undoable command"
		+ std::wstring(L" [begin=") + SelfTestFlag(beginCommittedLive.Succeeded())
		+ L", apply=" + SelfTestFlag(appliedCommittedLive)
		+ L", commit=" + std::to_wstring(static_cast<int>(commitLivePreview.State))
		+ L", undoCount=" + std::to_wstring(liveCommitUndoCount)
		+ L", undo=" + std::to_wstring(static_cast<int>(undoLivePreview.State))
		+ L", redo=" + std::to_wstring(static_cast<int>(redoLivePreview.State))
		+ L", error=" + livePreviewError + L"]");

	DesignerCanvas duplicateCanvas(0, 0, 900, 680);
	duplicateCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 180, 160 });
	const auto duplicateSource = duplicateCanvas.GetSelectedControl();
	const auto duplicateSourceName = duplicateSource
		? duplicateSource->Name : std::wstring{};
	const cui::core::Point duplicateSourceLocation = duplicateSource
		&& duplicateSource->ControlInstance
		? cui::core::Point{
			Canvas::GetLeft(*(duplicateSource->ControlInstance)),
			Canvas::GetTop(*(duplicateSource->ControlInstance)) }
		: cui::core::Point{};
	if (duplicateSource)
	{
		duplicateSource->EventHandlers[L"Click"] =
			duplicateSourceName + L"_Click";
		duplicateSource->EventHandlers[L"MouseDoubleClick"] =
			L"KeepSharedMouseHandler";
	}
	PropertyGrid duplicatePropertyGrid(0, 0, 360, 360);
	duplicatePropertyGrid.SetDesignerCanvas(&duplicateCanvas);
	ReloadCurrentSelection(duplicatePropertyGrid, duplicateCanvas);
	int duplicateSelectionNotifications = 0;
	duplicateCanvas.OnControlSelected +=
		[&](std::shared_ptr<DesignerControl> selected)
		{
			++duplicateSelectionNotifications;
			duplicatePropertyGrid.LoadControls(
				duplicateCanvas.GetSelectedControls(), selected);
		};
	(void)duplicateCanvas.ResetDocumentHistoryAsSaved();
	const auto duplicateResult = duplicateCanvas.DuplicateSelectedControls();
	const auto duplicatedControl = duplicateCanvas.GetSelectedControl();
	const auto duplicatedName = duplicatedControl
		? duplicatedControl->Name : std::wstring{};
	AppendFailure(failures,
		duplicateResult.HasChanges()
		&& duplicateCanvas.GetAllControls().size() == 3
		&& duplicateSource && duplicatedControl
		&& duplicatedControl != duplicateSource
		&& duplicatedName != duplicateSourceName
		&& duplicatedControl->StableId != duplicateSource->StableId
		&& Canvas::GetLeft(*(duplicatedControl->ControlInstance))
			== duplicateSourceLocation.x + 12
		&& Canvas::GetTop(*(duplicatedControl->ControlInstance))
			== duplicateSourceLocation.y + 12
		&& duplicateSource->EventHandlers[L"Click"]
			== duplicateSourceName + L"_Click"
		&& duplicatedControl->EventHandlers[L"Click"]
			== duplicatedName + L"_Click"
		&& duplicatedControl->EventHandlers[L"MouseDoubleClick"]
			== L"KeepSharedMouseHandler"
		&& duplicateSelectionNotifications >= 2
		&& duplicateCanvas.GetUndoCommandCount() == 1
		&& duplicateCanvas.GetLastCommandOperation() == L"DuplicateSelection",
		L"duplicate: offset copy, identity, property-grid reload, selection, or one-command history failed");
	const auto undoDuplicate = duplicateCanvas.UndoCommand();
	const auto redoDuplicate = duplicateCanvas.RedoCommand();
	AppendFailure(failures,
		undoDuplicate.HasChanges() && redoDuplicate.HasChanges()
		&& duplicateCanvas.GetAllControls().size() == 3
		&& duplicateCanvas.GetSelectedControl()
		&& duplicateCanvas.GetSelectedControl()->Name == duplicatedName
		&& duplicateCanvas.GetSelectedControl()->EventHandlers[L"Click"]
			== duplicatedName + L"_Click"
		&& duplicateCanvas.GetSelectedControl()->EventHandlers[
			L"MouseDoubleClick"] == L"KeepSharedMouseHandler",
		L"duplicate: undo/redo did not restore the copied selection");

	DesignerCanvas stackDuplicateCanvas(0, 0, 900, 680);
	stackDuplicateCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 300, 240 });
	const auto stackDuplicateParent = stackDuplicateCanvas.GetSelectedControl();
	if (stackDuplicateParent && stackDuplicateParent->ControlInstance)
	{
		const POINT inside = RoundedPoint(stackDuplicateParent->ControlInstance->GetAbsoluteLocationDip().x
				- stackDuplicateCanvas.GetAbsoluteLocationDip().x + 35, stackDuplicateParent->ControlInstance->GetAbsoluteLocationDip().y
				- stackDuplicateCanvas.GetAbsoluteLocationDip().y + 25);
		for (int index = 0; index < 3; ++index)
			stackDuplicateCanvas.AdoptVisualChildToCanvasCore(
				UIClass::UI_Button,
				POINT{ inside.x, inside.y + index * 45 });
	}
	const auto stackDuplicateControls = stackDuplicateCanvas.GetAllControls();
	const bool stackDuplicateReady = stackDuplicateParent
		&& stackDuplicateControls.size() >= 5
		&& stackDuplicateControls[2] && stackDuplicateControls[3]
		&& stackDuplicateControls[4];
	const int stackFirstId = stackDuplicateReady
		? stackDuplicateControls[2]->StableId : 0;
	const int stackSourceId = stackDuplicateReady
		? stackDuplicateControls[3]->StableId : 0;
	const int stackLastId = stackDuplicateReady
		? stackDuplicateControls[4]->StableId : 0;
	if (stackDuplicateReady)
		stackDuplicateCanvas.RestoreSelectionByNames(
			{ stackDuplicateControls[3]->Name },
			stackDuplicateControls[3]->Name, false);
	(void)stackDuplicateCanvas.ResetDocumentHistoryAsSaved();
	const auto duplicateStackMiddle = stackDuplicateReady
		? stackDuplicateCanvas.DuplicateSelectedControls()
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing stack duplicate setup");
	const auto stackDuplicateCopy = stackDuplicateCanvas.GetSelectedControl();
	const int stackCopyId = stackDuplicateCopy
		? stackDuplicateCopy->StableId : 0;
	DesignerModel::DesignDocument stackDuplicateDocument;
	std::wstring stackDuplicateError;
	const bool stackDuplicateCaptured =
		stackDuplicateCanvas.BuildDesignDocument(
			stackDuplicateDocument, &stackDuplicateError);
	std::vector<const DesignerModel::DesignNode*> stackDuplicateChildren;
	for (const auto& node : stackDuplicateDocument.Nodes)
		if (stackDuplicateParent
			&& node.ParentId == stackDuplicateParent->StableId)
			stackDuplicateChildren.push_back(&node);
	std::stable_sort(stackDuplicateChildren.begin(),
		stackDuplicateChildren.end(),
		[](const auto* left, const auto* right)
		{
			return left->Order < right->Order;
		});
	const bool stackDuplicateAdjacent = stackDuplicateChildren.size() == 4
		&& stackDuplicateChildren[0]->Id == stackFirstId
		&& stackDuplicateChildren[1]->Id == stackSourceId
		&& stackDuplicateChildren[2]->Id == stackCopyId
		&& stackDuplicateChildren[3]->Id == stackLastId
		&& !stackDuplicateChildren[2]->Properties.Find(L"Canvas.Left")
		&& !stackDuplicateChildren[2]->Properties.Find(L"Canvas.Top");
	const auto undoStackDuplicate = stackDuplicateCanvas.UndoCommand();
	AppendFailure(failures,
		stackDuplicateReady && duplicateStackMiddle.HasChanges()
		&& stackDuplicateCaptured && stackDuplicateAdjacent
		&& stackDuplicateCanvas.GetUndoCommandCount() == 0
		&& undoStackDuplicate.HasChanges(),
		L"duplicate layout: StackPanel copy was not inserted beside its source or undone once"
		+ std::wstring(L" [ready=") + SelfTestFlag(stackDuplicateReady)
		+ L", duplicate=" + SelfTestFlag(duplicateStackMiddle.HasChanges())
		+ L", capture=" + SelfTestFlag(stackDuplicateCaptured)
		+ L", adjacent=" + SelfTestFlag(stackDuplicateAdjacent)
		+ L", error=" + stackDuplicateError + L"]");

	DesignerCanvas relativeDuplicateCanvas(0, 0, 900, 680);
	relativeDuplicateCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_RelativePanel, POINT{ 320, 250 });
	const auto relativeDuplicateParent =
		relativeDuplicateCanvas.GetSelectedControl();
	if (relativeDuplicateParent && relativeDuplicateParent->ControlInstance)
	{
		const POINT inside = RoundedPoint(relativeDuplicateParent->ControlInstance->GetAbsoluteLocationDip().x
				- relativeDuplicateCanvas.GetAbsoluteLocationDip().x + 70, relativeDuplicateParent->ControlInstance->GetAbsoluteLocationDip().y
				- relativeDuplicateCanvas.GetAbsoluteLocationDip().y + 80);
		relativeDuplicateCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, inside);
	}
	const auto relativeDuplicateSource =
		relativeDuplicateCanvas.GetSelectedControl();
	const auto relativeSourceMargin = relativeDuplicateSource
		&& relativeDuplicateSource->ControlInstance
		? relativeDuplicateSource->ControlInstance->Margin : Thickness{};
	(void)relativeDuplicateCanvas.ResetDocumentHistoryAsSaved();
	const auto duplicateRelative = relativeDuplicateSource
		&& relativeDuplicateSource != relativeDuplicateParent
		? relativeDuplicateCanvas.DuplicateSelectedControls()
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing relative duplicate setup");
	const auto relativeDuplicateCopy =
		relativeDuplicateCanvas.GetSelectedControl();
	DesignerModel::DesignDocument relativeDuplicateDocument;
	std::wstring relativeDuplicateError;
	const bool relativeDuplicateCaptured =
		relativeDuplicateCanvas.BuildDesignDocument(
			relativeDuplicateDocument, &relativeDuplicateError);
	const auto relativeDuplicateNode = std::find_if(
		relativeDuplicateDocument.Nodes.begin(),
		relativeDuplicateDocument.Nodes.end(),
		[&relativeDuplicateCopy](const auto& node)
		{
			return relativeDuplicateCopy
				&& node.Id == relativeDuplicateCopy->StableId;
		});
	const bool relativeDuplicateOffset = relativeDuplicateNode
		!= relativeDuplicateDocument.Nodes.end()
		&& std::fabs(NodePropertyThickness(&*relativeDuplicateNode, L"Margin").Left
			- (relativeSourceMargin.Left + 12.0)) < 0.01
		&& std::fabs(NodePropertyThickness(&*relativeDuplicateNode, L"Margin").Top
			- (relativeSourceMargin.Top + 12.0)) < 0.01
		&& !relativeDuplicateNode->Properties.Find(L"Canvas.Left")
		&& !relativeDuplicateNode->Properties.Find(L"Canvas.Top");
	AppendFailure(failures,
		duplicateRelative.HasChanges() && relativeDuplicateCaptured
		&& relativeDuplicateCopy && relativeDuplicateOffset
		&& relativeDuplicateCanvas.GetUndoCommandCount() == 1,
		L"duplicate layout: RelativePanel copy did not offset Margin by 12 DIP"
		+ std::wstring(L" [duplicate=")
		+ SelfTestFlag(duplicateRelative.HasChanges())
		+ L", capture=" + SelfTestFlag(relativeDuplicateCaptured)
		+ L", offset=" + SelfTestFlag(relativeDuplicateOffset)
		+ L", error=" + relativeDuplicateError + L"]");

	DesignerCanvas nestedClipboardCanvas(0, 0, 900, 680);
	const auto nestedSetup = nestedClipboardCanvas.PasteControlsFromXamlText(
		clipboardXaml);
	(void)nestedClipboardCanvas.ResetDocumentHistoryAsSaved();
	const std::wstring nestedChildXaml = LR"xaml(
		<Window xmlns="urn:cui"
		      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
		      x:Name="ClipboardChild">
		  <TextBlock x:Name="insertLabel1" DesignId="10"
		         Canvas.Left="8" Canvas.Top="9" Width="80" Height="24"
		         Text="Inside" />
		</Window>)xaml";
	const auto pasteIntoSelectedPanel =
		nestedClipboardCanvas.PasteControlsFromXamlText(nestedChildXaml);
	DesignerModel::DesignDocument nestedAfterPaste;
	std::wstring nestedClipboardError;
	const bool nestedPasteCaptured = nestedClipboardCanvas.BuildDesignDocument(
		nestedAfterPaste, &nestedClipboardError);
	auto findNestedNode = [](const DesignerModel::DesignDocument& document,
		const std::wstring& name) -> const DesignerModel::DesignNode*
	{
		const auto found = std::find_if(
			document.Nodes.begin(), document.Nodes.end(),
			[&](const auto& node) { return node.Name == name; });
		return found == document.Nodes.end() ? nullptr : &*found;
	};
	const auto* nestedPanel = findNestedNode(nestedAfterPaste, L"panel1");
	const auto* insertedLabel = findNestedNode(
		nestedAfterPaste, L"insertLabel1");
	const bool pastedIntoPanel = nestedSetup.HasChanges()
		&& pasteIntoSelectedPanel.HasChanges()
		&& nestedPasteCaptured && nestedPanel && insertedLabel
		&& insertedLabel->ParentId == nestedPanel->Id
		&& insertedLabel->ParentRef == nestedPanel->Name
		&& nestedClipboardCanvas.GetUndoCommandCount() == 1;
	const auto undoNestedPaste = nestedClipboardCanvas.UndoCommand();
	nestedClipboardCanvas.RestoreSelectionByNames(
		{ L"button1" }, L"button1", true);
	(void)nestedClipboardCanvas.ResetDocumentHistoryAsSaved();
	const auto duplicateNestedChild =
		nestedClipboardCanvas.DuplicateSelectedControls();
	DesignerModel::DesignDocument nestedAfterDuplicate;
	const bool nestedDuplicateCaptured =
		nestedClipboardCanvas.BuildDesignDocument(
			nestedAfterDuplicate, &nestedClipboardError);
	const auto* duplicatePanel = findNestedNode(
		nestedAfterDuplicate, L"panel1");
	const auto* duplicatedNestedButton = findNestedNode(
		nestedAfterDuplicate, L"button2");
	AppendFailure(failures,
		pastedIntoPanel && undoNestedPaste.HasChanges()
		&& duplicateNestedChild.HasChanges() && nestedDuplicateCaptured
		&& duplicatePanel && duplicatedNestedButton
		&& duplicatedNestedButton->ParentId == duplicatePanel->Id
		&& duplicatedNestedButton->ParentRef == duplicatePanel->Name
		&& nestedClipboardCanvas.GetSelectedControl()
		&& nestedClipboardCanvas.GetSelectedControl()->Name == L"button2"
		&& nestedClipboardCanvas.GetUndoCommandCount() == 1,
		L"nested clipboard: paste target or duplicate parent was not preserved"
		+ std::wstring(L" [setup=") + SelfTestFlag(nestedSetup.HasChanges())
		+ L", paste=" + SelfTestFlag(pasteIntoSelectedPanel.HasChanges())
		+ L", captured=" + SelfTestFlag(nestedPasteCaptured)
		+ L", parent=" + SelfTestFlag(pastedIntoPanel)
		+ L", undo=" + SelfTestFlag(undoNestedPaste.HasChanges())
		+ L", duplicate=" + SelfTestFlag(duplicateNestedChild.HasChanges())
		+ L", error=" + nestedClipboardError + L"]");

	DesignerCanvas repeatedPasteCanvas(0, 0, 900, 680);
	const auto repeatedPaste1 = repeatedPasteCanvas.PasteControlsFromXamlText(
		clipboardXaml);
	const auto repeatedPaste2 = repeatedPasteCanvas.PasteControlsFromXamlText(
		clipboardXaml);
	const auto repeatedPaste3 = repeatedPasteCanvas.PasteControlsFromXamlText(
		clipboardXaml);
	DesignerModel::DesignDocument repeatedPasteDocument;
	std::wstring repeatedPasteError;
	const bool repeatedPasteCaptured = repeatedPasteCanvas.BuildDesignDocument(
		repeatedPasteDocument, &repeatedPasteError);
	const auto* repeatedPanel1 = findNestedNode(
		repeatedPasteDocument, L"panel1");
	const auto* repeatedPanel2 = findNestedNode(
		repeatedPasteDocument, L"panel2");
	const auto* repeatedPanel3 = findNestedNode(
		repeatedPasteDocument, L"panel3");
	const auto* repeatedContentRoot = findNestedNode(
		repeatedPasteDocument, L"contentRoot");
	AppendFailure(failures,
		repeatedPaste1.HasChanges() && repeatedPaste2.HasChanges()
		&& repeatedPaste3.HasChanges() && repeatedPasteCaptured
		&& repeatedContentRoot
		&& repeatedPanel1 && repeatedPanel2 && repeatedPanel3
		&& repeatedPanel1->ParentId == repeatedContentRoot->Id
		&& repeatedPanel2->ParentId == repeatedContentRoot->Id
		&& repeatedPanel3->ParentId == repeatedContentRoot->Id
		&& repeatedPanel1->ParentRef == repeatedContentRoot->Name
		&& repeatedPanel2->ParentRef == repeatedContentRoot->Name
		&& repeatedPanel3->ParentRef == repeatedContentRoot->Name
		&& repeatedPasteCanvas.GetSelectedControl()
		&& repeatedPasteCanvas.GetSelectedControl()->Name == L"panel3",
		L"repeated clipboard: a copied container was nested into its prior paste"
		+ std::wstring(L" [first=") + SelfTestFlag(repeatedPaste1.HasChanges())
		+ L", second=" + SelfTestFlag(repeatedPaste2.HasChanges())
		+ L", third=" + SelfTestFlag(repeatedPaste3.HasChanges())
		+ L", captured=" + SelfTestFlag(repeatedPasteCaptured)
		+ L", error=" + repeatedPasteError + L"]");

	auto clipboardNodeLocation = [](
		const DesignerModel::DesignNode* node) -> POINT
	{
		return POINT{
			static_cast<LONG>(std::lround(NodePropertyFloat(
				node, L"Canvas.Left"))),
			static_cast<LONG>(std::lround(NodePropertyFloat(
				node, L"Canvas.Top"))) };
	};
	DesignerCanvas inPlacePasteCanvas(0, 0, 900, 680);
	const auto inPlacePaste1 =
		inPlacePasteCanvas.PasteControlsFromXamlTextInPlace(clipboardXaml);
	const auto inPlacePaste2 =
		inPlacePasteCanvas.PasteControlsFromXamlTextInPlace(clipboardXaml);
	const auto cascadeAfterInPlace =
		inPlacePasteCanvas.PasteControlsFromXamlText(clipboardXaml);
	DesignerModel::DesignDocument inPlacePasteDocument;
	std::wstring inPlacePasteError;
	const bool inPlacePasteCaptured = inPlacePasteCanvas.BuildDesignDocument(
		inPlacePasteDocument, &inPlacePasteError);
	const auto* inPlacePanel1 = findNestedNode(
		inPlacePasteDocument, L"panel1");
	const auto* inPlacePanel2 = findNestedNode(
		inPlacePasteDocument, L"panel2");
	const auto* inPlacePanel3 = findNestedNode(
		inPlacePasteDocument, L"panel3");
	const auto inPlaceLocation1 = clipboardNodeLocation(inPlacePanel1);
	const auto inPlaceLocation2 = clipboardNodeLocation(inPlacePanel2);
	const auto cascadeLocation = clipboardNodeLocation(inPlacePanel3);
	const auto undoCascadeAfterInPlace = inPlacePasteCanvas.UndoCommand();
	AppendFailure(failures,
		inPlacePaste1.HasChanges() && inPlacePaste2.HasChanges()
		&& cascadeAfterInPlace.HasChanges() && inPlacePasteCaptured
		&& inPlacePanel1 && inPlacePanel2 && inPlacePanel3
		&& inPlaceLocation1.x == 40 && inPlaceLocation1.y == 50
		&& inPlaceLocation2.x == 40 && inPlaceLocation2.y == 50
		&& cascadeLocation.x == 52 && cascadeLocation.y == 62
		&& inPlacePasteCanvas.GetUndoCommandCount() == 2
		&& undoCascadeAfterInPlace.HasChanges(),
		L"clipboard placement: in-place paste moved roots, consumed the cascade sequence, or lost one-command Undo"
		+ std::wstring(L" [first=") + SelfTestFlag(inPlacePaste1.HasChanges())
		+ L", second=" + SelfTestFlag(inPlacePaste2.HasChanges())
		+ L", cascade=" + SelfTestFlag(cascadeAfterInPlace.HasChanges())
		+ L", capture=" + SelfTestFlag(inPlacePasteCaptured)
		+ L", p1=" + std::to_wstring(inPlaceLocation1.x) + L","
		+ std::to_wstring(inPlaceLocation1.y)
		+ L", p2=" + std::to_wstring(inPlaceLocation2.x) + L","
		+ std::to_wstring(inPlaceLocation2.y)
		+ L", p3=" + std::to_wstring(cascadeLocation.x) + L","
		+ std::to_wstring(cascadeLocation.y)
		+ L", undoCount="
		+ std::to_wstring(inPlacePasteCanvas.GetUndoCommandCount())
		+ L", error=" + inPlacePasteError + L"]");

	DesignerCanvas pointPasteCanvas(0, 0, 900, 680);
	const auto pointPasteSetup =
		pointPasteCanvas.PasteControlsFromXamlTextInPlace(clipboardXaml);
	const auto pointPanel = std::find_if(
		pointPasteCanvas.GetAllControls().begin(),
		pointPasteCanvas.GetAllControls().end(),
		[](const auto& candidate)
		{
			return candidate && candidate->Name == L"panel1";
		});
	const bool pointPanelReady = pointPanel
		!= pointPasteCanvas.GetAllControls().end()
		&& *pointPanel && (*pointPanel)->ControlInstance;
	POINT pointInsidePanel{};
	if (pointPanelReady)
	{
		pointInsidePanel = RoundedPoint(
			(*pointPanel)->ControlInstance->GetAbsoluteLocationDip().x
				- pointPasteCanvas.GetAbsoluteLocationDip().x + 73,
			(*pointPanel)->ControlInstance->GetAbsoluteLocationDip().y
				- pointPasteCanvas.GetAbsoluteLocationDip().y + 81);
	}
	(void)pointPasteCanvas.ResetDocumentHistoryAsSaved();
	const auto pasteAtPanelPoint = pointPanelReady
		? pointPasteCanvas.PasteControlsFromXamlTextAt(
			nestedChildXaml, pointInsidePanel)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing point-paste panel");
	DesignerModel::DesignDocument pointPasteDocument;
	std::wstring pointPasteError;
	const bool pointPasteCaptured = pointPasteCanvas.BuildDesignDocument(
		pointPasteDocument, &pointPasteError);
	const auto* pointPanelNode = findNestedNode(
		pointPasteDocument, L"panel1");
	const auto* pointLabelNode = findNestedNode(
		pointPasteDocument, L"insertLabel1");
	const auto pointLabelLocation = clipboardNodeLocation(pointLabelNode);
	const auto pointPasteHistory = pointPasteCanvas.GetUndoCommandCount();
	const auto rejectedOutsidePoint =
		pointPasteCanvas.PasteControlsFromXamlTextAt(
			nestedChildXaml, POINT{ -1000, -1000 });
	DesignerModel::DesignDocument afterRejectedPointPaste;
	const bool rejectedPointPreserved = pointPasteCanvas.BuildDesignDocument(
		afterRejectedPointPaste, &pointPasteError)
		&& afterRejectedPointPaste == pointPasteDocument;
	const auto undoPointPaste = pointPasteCanvas.UndoCommand();
	AppendFailure(failures,
		pointPasteSetup.HasChanges() && pointPanelReady
		&& pasteAtPanelPoint.HasChanges() && pointPasteCaptured
		&& pointPanelNode && pointLabelNode
		&& pointLabelNode->ParentId == pointPanelNode->Id
		&& pointLabelLocation.x == 73 && pointLabelLocation.y == 81
		&& pointPasteHistory == 1
		&& rejectedOutsidePoint.State
			== DesignerDocumentTransactionState::Rejected
		&& pointPasteCanvas.GetUndoCommandCount() == 0
		&& rejectedPointPreserved && undoPointPaste.HasChanges(),
		L"clipboard placement: paste-here missed the pointed container/location or invalid target changed history"
		+ std::wstring(L" [setup=") + SelfTestFlag(pointPasteSetup.HasChanges())
		+ L", panel=" + SelfTestFlag(pointPanelReady)
		+ L", paste=" + SelfTestFlag(pasteAtPanelPoint.HasChanges())
		+ L", capture=" + SelfTestFlag(pointPasteCaptured)
		+ L", preserve=" + SelfTestFlag(rejectedPointPreserved)
		+ L", location=" + std::to_wstring(pointLabelLocation.x) + L","
		+ std::to_wstring(pointLabelLocation.y)
		+ L", parent=" + (pointLabelNode
			? std::to_wstring(pointLabelNode->ParentId) : L"missing")
		+ L", expectedParent=" + (pointPanelNode
			? std::to_wstring(pointPanelNode->Id) : L"missing")
		+ L", history=" + std::to_wstring(pointPasteHistory)
		+ L", error=" + pointPasteError + L"]");

	DesignerCanvas stackPointPasteCanvas(0, 0, 900, 680);
	stackPointPasteCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 300, 240 });
	const auto stackPointTarget = stackPointPasteCanvas.GetSelectedControl();
	auto* stackPointControl = stackPointTarget
		? dynamic_cast<StackPanel*>(stackPointTarget->ControlInstance) : nullptr;
	if (stackPointControl)
	{
		const POINT inside = RoundedPoint(stackPointControl->GetAbsoluteLocationDip().x
				- stackPointPasteCanvas.GetAbsoluteLocationDip().x + 30, stackPointControl->GetAbsoluteLocationDip().y
				- stackPointPasteCanvas.GetAbsoluteLocationDip().y + 25);
		stackPointPasteCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, inside);
		stackPointPasteCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, POINT{ inside.x, inside.y + 60 });
	}
	POINT beforeStackSecond{};
	if (stackPointControl && stackPointControl->VisualChildCount() >= 2)
	{
		auto* second = stackPointControl->GetVisualChild(1);
		beforeStackSecond = RoundedPoint(
			second->GetAbsoluteLocationDip().x - stackPointPasteCanvas.GetAbsoluteLocationDip().x + 5,
			second->GetAbsoluteLocationDip().y - stackPointPasteCanvas.GetAbsoluteLocationDip().y + 1);
	}
	(void)stackPointPasteCanvas.ResetDocumentHistoryAsSaved();
	const auto pasteIntoStackMiddle = stackPointControl
		&& stackPointControl->VisualChildCount() >= 2
		? stackPointPasteCanvas.PasteControlsFromXamlTextAt(
			nestedChildXaml, beforeStackSecond)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing stack point target");
	DesignerModel::DesignDocument stackPointDocument;
	std::wstring stackPointError;
	const bool stackPointCaptured = stackPointPasteCanvas.BuildDesignDocument(
		stackPointDocument, &stackPointError);
	std::vector<const DesignerModel::DesignNode*> stackPointChildren;
	for (const auto& node : stackPointDocument.Nodes)
		if (stackPointTarget && node.ParentId == stackPointTarget->StableId)
			stackPointChildren.push_back(&node);
	std::stable_sort(stackPointChildren.begin(), stackPointChildren.end(),
		[](const auto* left, const auto* right)
		{
			return left->Order < right->Order;
		});
	const auto* stackPointLabel = findNestedNode(
		stackPointDocument, L"insertLabel1");
	const auto stackPointLabelLocation = clipboardNodeLocation(stackPointLabel);
	const bool stackOrderCorrect = stackPointChildren.size() == 3
		&& stackPointChildren[0]->Type == UIClass::UI_Button
		&& stackPointChildren[1]->Name == L"insertLabel1"
		&& stackPointChildren[2]->Type == UIClass::UI_Button;
	const auto undoStackPointPaste = stackPointPasteCanvas.UndoCommand();
	const auto stackAfterUndo = stackPointTarget
		? FindControl(stackPointPasteCanvas, stackPointTarget->Name) : nullptr;
	AppendFailure(failures,
		pasteIntoStackMiddle.HasChanges() && stackPointCaptured
		&& stackPointTarget && stackPointLabel && stackOrderCorrect
		&& stackPointLabelLocation.x == 0 && stackPointLabelLocation.y == 0
		&& stackPointPasteCanvas.GetUndoCommandCount() == 0
		&& undoStackPointPaste.HasChanges()
		&& stackAfterUndo && stackAfterUndo->ControlInstance
		&& stackAfterUndo->ControlInstance->VisualChildCount() == 2,
		L"clipboard placement: StackPanel paste-here did not insert at the pointed boundary or undo atomically"
		+ std::wstring(L" [paste=")
		+ SelfTestFlag(pasteIntoStackMiddle.HasChanges())
		+ L", capture=" + SelfTestFlag(stackPointCaptured)
		+ L", order=" + SelfTestFlag(stackOrderCorrect)
		+ L", location=" + std::to_wstring(stackPointLabelLocation.x)
		+ L"," + std::to_wstring(stackPointLabelLocation.y)
		+ L", error=" + stackPointError + L"]");

	const std::wstring gridPasteTargetXaml = LR"xaml(
		<Window xmlns="urn:cui"
		      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
		      x:Name="GridPasteWindow">
		  <Grid x:Name="grid1" DesignId="1" Canvas.Left="80"
		             Canvas.Top="70" Width="240" Height="180">
		    <Grid.RowDefinitions>
		      <RowDefinition Height="*" />
		      <RowDefinition Height="*" />
		    </Grid.RowDefinitions>
		    <Grid.ColumnDefinitions>
		      <ColumnDefinition Width="*" />
		      <ColumnDefinition Width="*" />
		    </Grid.ColumnDefinitions>
		  </Grid>
		</Window>)xaml";
	DesignerCanvas gridPointPasteCanvas(0, 0, 900, 680);
	const auto gridPointSetup =
		gridPointPasteCanvas.PasteControlsFromXamlTextInPlace(
			gridPasteTargetXaml);
	const auto gridPointTarget = gridPointPasteCanvas.GetSelectedControl();
	auto* gridPointControl = gridPointTarget
		? dynamic_cast<Grid*>(gridPointTarget->ControlInstance) : nullptr;
	POINT gridSecondCellPoint{};
	if (gridPointControl)
	{
		gridPointControl->UpdateLayout();
		gridSecondCellPoint = RoundedPoint(
			gridPointControl->GetAbsoluteLocationDip().x
				- gridPointPasteCanvas.GetAbsoluteLocationDip().x + 180,
			gridPointControl->GetAbsoluteLocationDip().y
				- gridPointPasteCanvas.GetAbsoluteLocationDip().y + 130);
	}
	(void)gridPointPasteCanvas.ResetDocumentHistoryAsSaved();
	const auto pasteIntoGridCell = gridPointControl
		? gridPointPasteCanvas.PasteControlsFromXamlTextAt(
			nestedChildXaml, gridSecondCellPoint)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing grid point target");
	DesignerModel::DesignDocument gridPointDocument;
	std::wstring gridPointError;
	const bool gridPointCaptured = gridPointPasteCanvas.BuildDesignDocument(
		gridPointDocument, &gridPointError);
	const auto* gridPointLabel = findNestedNode(
		gridPointDocument, L"insertLabel1");
	const auto gridPointLocation = clipboardNodeLocation(gridPointLabel);
	AppendFailure(failures,
		gridPointSetup.HasChanges() && pasteIntoGridCell.HasChanges()
		&& gridPointCaptured && gridPointTarget && gridPointLabel
		&& gridPointLabel->ParentId == gridPointTarget->StableId
		&& NodePropertyInt(gridPointLabel, L"Grid.Row", -1) == 1
		&& NodePropertyInt(gridPointLabel, L"Grid.Column", -1) == 1
		&& NodePropertyInt(gridPointLabel, L"Grid.RowSpan", -1) == 1
		&& NodePropertyInt(gridPointLabel, L"Grid.ColumnSpan", -1) == 1
		&& NodePropertyInt(gridPointLabel, L"HorizontalAlignment", -1)
			== static_cast<int>(HorizontalAlignment::Stretch)
		&& NodePropertyInt(gridPointLabel, L"VerticalAlignment", -1)
			== static_cast<int>(VerticalAlignment::Stretch)
		&& gridPointLocation.x == 0 && gridPointLocation.y == 0
		&& gridPointPasteCanvas.GetUndoCommandCount() == 1,
		L"clipboard placement: Grid paste-here did not target the pointed cell"
		+ std::wstring(L" [setup=") + SelfTestFlag(gridPointSetup.HasChanges())
		+ L", paste=" + SelfTestFlag(pasteIntoGridCell.HasChanges())
		+ L", capture=" + SelfTestFlag(gridPointCaptured)
		+ L", row=" + (gridPointLabel
			? std::to_wstring(NodePropertyInt(gridPointLabel, L"Grid.Row", -1))
			: L"missing")
		+ L", column=" + (gridPointLabel
			? std::to_wstring(NodePropertyInt(gridPointLabel, L"Grid.Column", -1))
			: L"missing")
		+ L", error=" + gridPointError + L"]");

	DesignerCanvas relativePointPasteCanvas(0, 0, 900, 680);
	relativePointPasteCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_RelativePanel, POINT{ 320, 250 });
	const auto relativePointTarget =
		relativePointPasteCanvas.GetSelectedControl();
	POINT relativePastePoint{};
	if (relativePointTarget && relativePointTarget->ControlInstance)
	{
		relativePastePoint = RoundedPoint(
			relativePointTarget->ControlInstance->GetAbsoluteLocationDip().x
				- relativePointPasteCanvas.GetAbsoluteLocationDip().x + 65,
			relativePointTarget->ControlInstance->GetAbsoluteLocationDip().y
				- relativePointPasteCanvas.GetAbsoluteLocationDip().y + 75);
	}
	(void)relativePointPasteCanvas.ResetDocumentHistoryAsSaved();
	const auto pasteIntoRelativePoint = relativePointTarget
		? relativePointPasteCanvas.PasteControlsFromXamlTextAt(
			nestedChildXaml, relativePastePoint)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing relative point target");
	DesignerModel::DesignDocument relativePointDocument;
	std::wstring relativePointError;
	const bool relativePointCaptured =
		relativePointPasteCanvas.BuildDesignDocument(
			relativePointDocument, &relativePointError);
	const auto* relativePointLabel = findNestedNode(
		relativePointDocument, L"insertLabel1");
	const auto relativePointLocation = clipboardNodeLocation(relativePointLabel);
	AppendFailure(failures,
		pasteIntoRelativePoint.HasChanges() && relativePointCaptured
		&& relativePointTarget && relativePointLabel
		&& relativePointLabel->ParentId == relativePointTarget->StableId
		&& NodePropertyThickness(relativePointLabel, L"Margin").Left == 65
		&& NodePropertyThickness(relativePointLabel, L"Margin").Top == 75
		&& relativePointLocation.x == 0 && relativePointLocation.y == 0
		&& relativePointPasteCanvas.GetUndoCommandCount() == 1,
		L"clipboard placement: RelativePanel paste-here did not convert the point to Margin"
		+ std::wstring(L" [paste=")
		+ SelfTestFlag(pasteIntoRelativePoint.HasChanges())
		+ L", capture=" + SelfTestFlag(relativePointCaptured)
		+ L", error=" + relativePointError + L"]");

	DesignerCanvas arrangeCanvas(0, 0, 900, 680);
	arrangeCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 140, 130 });
	arrangeCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 330, 200 });
	arrangeCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 570, 290 });
	const auto arrangeFirst = arrangeCanvas.GetAllControls().size() > 1
		? arrangeCanvas.GetAllControls()[1] : nullptr;
	const auto arrangeSecond = arrangeCanvas.GetAllControls().size() > 2
		? arrangeCanvas.GetAllControls()[2] : nullptr;
	const auto arrangeThird = arrangeCanvas.GetAllControls().size() > 3
		? arrangeCanvas.GetAllControls()[3] : nullptr;
	const bool arrangeSetup = arrangeFirst && arrangeSecond && arrangeThird
		&& arrangeFirst->ControlInstance && arrangeSecond->ControlInstance
		&& arrangeThird->ControlInstance;
	AppendFailure(failures, arrangeSetup,
		L"arrange: three-control setup failed");
	if (arrangeSetup)
	{
		auto* first = arrangeFirst->ControlInstance;
		auto* second = arrangeSecond->ControlInstance;
		auto* third = arrangeThird->ControlInstance;
		first->Width = 80.0f;
		first->Height = 30.0f;
		second->Width = 110.0f;
		second->Height = 42.0f;
		third->Width = 140.0f;
		third->Height = 54.0f;
		first->ZIndex = 2;
		second->ZIndex = 7;
		third->ZIndex = 12;
		arrangeCanvas.RestoreSelectionByNames(
			{ arrangeFirst->Name, arrangeSecond->Name, arrangeThird->Name },
			arrangeSecond->Name, false);
		(void)arrangeCanvas.ResetDocumentHistoryAsSaved();
		DesignerModel::DesignDocument arrangeBaseline;
		std::wstring arrangeError;
		const bool arrangeBaselineCaptured = arrangeCanvas.BuildDesignDocument(
			arrangeBaseline, &arrangeError);

		const auto alignLeft = arrangeCanvas.ArrangeSelection(
			DesignerSelectionArrangeAction::AlignLeft);
		const bool aligned = alignLeft.HasChanges()
			&& Canvas::GetLeft(*(first)) == Canvas::GetLeft(*(second))
			&& Canvas::GetLeft(*(third)) == Canvas::GetLeft(*(second))
			&& arrangeCanvas.GetUndoCommandCount() == 1
			&& arrangeCanvas.GetLastCommandOperation() == L"AlignLeft";
		const auto undoAlign = arrangeCanvas.UndoCommand();
		DesignerModel::DesignDocument afterUndoAlign;
		const bool alignRestored = undoAlign.HasChanges()
			&& arrangeCanvas.BuildDesignDocument(afterUndoAlign, &arrangeError)
			&& afterUndoAlign == arrangeBaseline;

		const auto sameSize = arrangeCanvas.ArrangeSelection(
			DesignerSelectionArrangeAction::MakeSameSize);
		const bool sizesMatched = sameSize.HasChanges()
			&& first->Width == second->Width
			&& first->Height == second->Height
			&& third->Width == second->Width
			&& third->Height == second->Height;
		const bool sameSizeRestored = arrangeCanvas.UndoCommand().HasChanges()
			&& first->Width.IsFixed() && first->Width.value == 80.0f
			&& first->Height.IsFixed() && first->Height.value == 30.0f
			&& second->Width.IsFixed() && second->Width.value == 110.0f
			&& second->Height.IsFixed() && second->Height.value == 42.0f
			&& third->Width.IsFixed() && third->Width.value == 140.0f
			&& third->Height.IsFixed() && third->Height.value == 54.0f;

		const auto distribute = arrangeCanvas.ArrangeSelection(
			DesignerSelectionArrangeAction::DistributeHorizontally);
		const float firstGap = Canvas::GetLeft(*(second))
			- (Canvas::GetLeft(*(first)) + first->Width.value);
		const float secondGap = Canvas::GetLeft(*(third))
			- (Canvas::GetLeft(*(second)) + second->Width.value);
		const bool distributed = distribute.HasChanges()
			&& std::abs(firstGap - secondGap) <= 1;
		const bool distributionRestored =
			arrangeCanvas.UndoCommand().HasChanges();
		AppendFailure(failures,
			arrangeBaselineCaptured && aligned && alignRestored
			&& sizesMatched && sameSizeRestored
			&& distributed && distributionRestored,
			L"arrange geometry: align, same-size, distribution, or undo failed"
			+ std::wstring(L" [baseline=")
			+ SelfTestFlag(arrangeBaselineCaptured)
			+ L", aligned=" + SelfTestFlag(aligned)
			+ L", restored=" + SelfTestFlag(alignRestored)
			+ L", size=" + SelfTestFlag(sizesMatched)
			+ L", sizeUndo=" + SelfTestFlag(sameSizeRestored)
			+ L", distribute=" + SelfTestFlag(distributed)
			+ L", distributeUndo=" + SelfTestFlag(distributionRestored)
			+ L", error=" + arrangeError + L"]");

		auto peerOrder = [&]()
		{
			std::vector<Control*> result;
			for (auto* control : first->GetVisualParent()->GetVisualChildrenInZOrder())
				if (control == first || control == second || control == third)
					result.push_back(control);
			return result;
		};
		auto originalLayerState = [&]()
		{
			return peerOrder() == std::vector<Control*>{ first, second, third }
				&& first->ZIndex == 2 && second->ZIndex == 7
				&& third->ZIndex == 12;
		};
		arrangeCanvas.RestoreSelectionByNames(
			{ arrangeFirst->Name }, arrangeFirst->Name, false);
		const auto bringForward = arrangeCanvas.ArrangeSelection(
			DesignerSelectionArrangeAction::BringForward);
		const bool broughtForward = bringForward.HasChanges()
			&& peerOrder() == std::vector<Control*>{ second, first, third }
			&& first->ZIndex == 7;
		const bool forwardUndone = arrangeCanvas.UndoCommand().HasChanges()
			&& originalLayerState();
		const auto bringFront = arrangeCanvas.ArrangeSelection(
			DesignerSelectionArrangeAction::BringToFront);
		const bool broughtFront = bringFront.HasChanges()
			&& peerOrder() == std::vector<Control*>{ second, third, first }
			&& first->ZIndex == 12;
		const bool frontUndone = arrangeCanvas.UndoCommand().HasChanges()
			&& originalLayerState();

		arrangeCanvas.RestoreSelectionByNames(
			{ arrangeThird->Name }, arrangeThird->Name, false);
		const auto sendBackward = arrangeCanvas.ArrangeSelection(
			DesignerSelectionArrangeAction::SendBackward);
		const bool sentBackward = sendBackward.HasChanges()
			&& peerOrder() == std::vector<Control*>{ first, third, second }
			&& third->ZIndex == 7;
		const bool backwardUndone = arrangeCanvas.UndoCommand().HasChanges()
			&& originalLayerState();
		const auto sendBack = arrangeCanvas.ArrangeSelection(
			DesignerSelectionArrangeAction::SendToBack);
		const bool sentBack = sendBack.HasChanges()
			&& peerOrder() == std::vector<Control*>{ third, first, second }
			&& third->ZIndex == 2;
		const bool backUndone = arrangeCanvas.UndoCommand().HasChanges()
			&& originalLayerState();
		AppendFailure(failures,
			broughtForward && forwardUndone && broughtFront && frontUndone
			&& sentBackward && backwardUndone && sentBack && backUndone,
			L"arrange layer: explicit ZIndex ordering or exact undo failed");
	}

	DesignerCanvas managedArrangeCanvas(0, 0, 900, 680);
	managedArrangeCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 260, 220 });
	const auto managedParent = managedArrangeCanvas.GetSelectedControl();
	if (managedParent && managedParent->ControlInstance)
	{
		const POINT inside = RoundedPoint(managedParent->ControlInstance->GetAbsoluteLocationDip().x
				- managedArrangeCanvas.GetAbsoluteLocationDip().x + 40, managedParent->ControlInstance->GetAbsoluteLocationDip().y
				- managedArrangeCanvas.GetAbsoluteLocationDip().y + 35);
		managedArrangeCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, inside);
		managedArrangeCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, POINT{ inside.x + 20, inside.y + 55 });
	}
	if (managedArrangeCanvas.GetAllControls().size() >= 4)
	{
		const auto managedFirst = managedArrangeCanvas.GetAllControls()[2];
		const auto managedSecond = managedArrangeCanvas.GetAllControls()[3];
		managedArrangeCanvas.RestoreSelectionByNames(
			{ managedFirst->Name, managedSecond->Name },
			managedFirst->Name, false);
		(void)managedArrangeCanvas.ResetDocumentHistoryAsSaved();
		const auto rejectedManagedArrange = managedArrangeCanvas.ArrangeSelection(
			DesignerSelectionArrangeAction::AlignLeft);
		AppendFailure(failures,
			rejectedManagedArrange.State
				== DesignerDocumentTransactionState::Rejected
			&& managedArrangeCanvas.GetUndoCommandCount() == 0,
			L"arrange layout guard: geometry operation mutated a managed container");
	}
	else
	{
		failures.push_back(
			L"arrange layout guard: managed-container setup failed");
	}

	DesignerCanvas lockCanvas(0, 0, 800, 640);
	lockCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 210, 180 });
	auto lockedControl = lockCanvas.GetSelectedControl();
	const auto lockedIdentity = lockedControl;
	const cui::core::Point unlockedLocation = lockedControl
		&& lockedControl->ControlInstance
		? cui::core::Point{ Canvas::GetLeft(*(lockedControl->ControlInstance)),
			Canvas::GetTop(*(lockedControl->ControlInstance)) }
		: cui::core::Point{};
	(void)lockCanvas.ResetDocumentHistoryAsSaved();
	const auto lockResult = lockCanvas.SetSelectedControlsLocked(true);
	DesignerModel::DesignDocument lockedDocument;
	std::wstring lockedError;
	std::wstring lockedXaml;
	const bool lockedCaptured = lockCanvas.BuildDesignDocument(
		lockedDocument, &lockedError);
	const bool lockedXamlCaptured = lockCanvas.BuildXamlDocumentText(
		lockedXaml, &lockedError);
	const auto lockedUndoCount = lockCanvas.GetUndoCommandCount();
	const auto rejectedLockedNudge = lockCanvas.NudgeSelectionBy(8, 4);
	const auto rejectedLockedArrange = lockCanvas.ArrangeSelection(
		DesignerSelectionArrangeAction::BringToFront);
	const auto rejectedLockedHierarchy = lockedControl
		? lockCanvas.MoveControlInHierarchy(
			lockedControl->StableId, std::nullopt,
			DesignerHierarchyDropPosition::Inside)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing locked control");
	if (lockedControl && lockedControl->ControlInstance)
	{
		auto* runtime = lockedControl->ControlInstance;
		const auto size = runtime->GetActualSizeDip();
		const POINT center = RoundedPoint(runtime->GetAbsoluteLocationDip().x - lockCanvas.GetAbsoluteLocationDip().x + size.width / 2, runtime->GetAbsoluteLocationDip().y - lockCanvas.GetAbsoluteLocationDip().y + size.height / 2);
		(void)cui::framework::InputAccess::DispatchInput(lockCanvas, PointerInput(
			InputReportKind::PointerDown, MouseButton::Left,
			center.x, center.y, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(lockCanvas, PointerInput(
			InputReportKind::PointerMove, MouseButton::None,
			center.x + 25, center.y + 15, MouseButton::Left));
		(void)cui::framework::InputAccess::DispatchInput(lockCanvas, PointerInput(
			InputReportKind::PointerUp, MouseButton::Left,
			center.x + 25, center.y + 15));
	}
	const bool lockedPlacementUnchanged = lockedControl
		&& lockedControl->ControlInstance
		&& Canvas::GetLeft(*(lockedControl->ControlInstance)) == unlockedLocation.x
		&& Canvas::GetTop(*(lockedControl->ControlInstance)) == unlockedLocation.y
		&& lockCanvas.GetUndoCommandCount() == lockedUndoCount;
	AppendFailure(failures,
		lockedControl && lockResult.HasChanges()
		&& lockedControl->IsLocked
		&& lockedCaptured && lockedDocument.Nodes.size() == 2
		&& std::any_of(lockedDocument.Nodes.begin(), lockedDocument.Nodes.end(),
			[&](const auto& node)
			{
				return node.Id == lockedControl->StableId && node.Locked;
			})
		&& lockedXamlCaptured
		&& lockedXaml.find(L"d:Locked=\"true\"") != std::wstring::npos
		&& lockedUndoCount == 1
		&& rejectedLockedNudge.State
			== DesignerDocumentTransactionState::Rejected
		&& rejectedLockedArrange.State
			== DesignerDocumentTransactionState::Rejected
		&& rejectedLockedHierarchy.State
			== DesignerDocumentTransactionState::Rejected
		&& lockedPlacementUnchanged,
		L"design lock: persistence or placement guards failed"
		+ std::wstring(L" [capture=") + SelfTestFlag(lockedCaptured)
		+ L", xaml=" + SelfTestFlag(lockedXamlCaptured)
		+ L", placement=" + SelfTestFlag(lockedPlacementUnchanged)
		+ L", error=" + lockedError + L"]");
	const auto unlockUndo = lockCanvas.UndoCommand();
	const bool unlockedByUndo = unlockUndo.HasChanges()
		&& lockedControl && !lockedControl->IsLocked
		&& lockCanvas.GetSelectedControl() == lockedIdentity;
	const auto lockRedo = lockCanvas.RedoCommand();
	const bool relockedByRedo = lockRedo.HasChanges()
		&& lockedControl && lockedControl->IsLocked
		&& lockCanvas.GetSelectedControl() == lockedIdentity;
	DesignerModel::DesignDocument parsedLocked;
	const bool parsedLockedXaml = lockedCaptured
		&& DesignerModel::XamlDocumentParser::FromXaml(
			DesignerModel::XamlDocumentSerializer::ToXaml(lockedDocument),
			parsedLocked, &lockedError);
	DesignerCanvas restoredLockCanvas(0, 0, 800, 640);
	const bool restoredLocked = parsedLockedXaml
		&& restoredLockCanvas.ApplyDesignDocument(parsedLocked, &lockedError)
		&& restoredLockCanvas.GetAllControls().size() == 2
		&& FindControl(restoredLockCanvas, lockedControl->Name)
		&& FindControl(restoredLockCanvas, lockedControl->Name)->IsLocked;
	AppendFailure(failures,
		unlockedByUndo && relockedByRedo && restoredLocked,
		L"design lock: undo/redo or XAML materialization lost lock metadata"
		+ std::wstring(L" [undo=") + SelfTestFlag(unlockedByUndo)
		+ L", redo=" + SelfTestFlag(relockedByRedo)
		+ L", restore=" + SelfTestFlag(restoredLocked)
		+ L", error=" + lockedError + L"]");

	DesignerCanvas viewCanvas(0, 0, 400, 300);
	viewCanvas.Arrange(
		cui::core::Rect{ 0.0f, 0.0f, 400.0f, 300.0f });
	viewCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 180, 140 });
	const auto transformedViewControl = viewCanvas.GetSelectedControl();
	size_t viewChangeCount = 0;
	viewCanvas.OnViewChanged +=
		[&](const DesignerCanvasViewChangedEventArgs&) { ++viewChangeCount; };
	(void)viewCanvas.ResetDocumentHistoryAsSaved();
	viewCanvas.FitDesignSurfaceToViewport();
	const float fittedZoom = viewCanvas.GetViewZoom();
	const POINT focalPoint{ 200, 150 };
	const POINT logicalAtFocal = viewCanvas.ViewToCanvasPoint(focalPoint);
	viewCanvas.SetViewZoom(fittedZoom * 1.2f, focalPoint);
	const POINT logicalAfterZoom = viewCanvas.ViewToCanvasPoint(focalPoint);
	D2D1_RECT_F logicalRenderRect{};
	D2D1_RECT_F transformedRenderRect{};
	D2D1_RECT_F expectedRenderRect{};
	bool descendantRenderTransformApplied = false;
	if (transformedViewControl && transformedViewControl->ControlInstance)
	{
		auto* runtimeControl = transformedViewControl->ControlInstance;
		const auto logicalRect = runtimeControl->GetAbsoluteRectDip();
		logicalRenderRect = D2D1_RECT_F{
			logicalRect.Left(), logicalRect.Top(),
			logicalRect.Right(), logicalRect.Bottom() };
		transformedRenderRect = runtimeControl->GetRenderedAbsoluteRectDip();
		const auto canvasAbs = viewCanvas.GetAbsoluteLocationDip();
		const auto viewOffset = viewCanvas.GetViewOffset();
		const float viewZoom = viewCanvas.GetViewZoom();
		expectedRenderRect = D2D1_RECT_F{
			canvasAbs.x + viewOffset.x
				+ (logicalRenderRect.left - canvasAbs.x) * viewZoom,
			canvasAbs.y + viewOffset.y
				+ (logicalRenderRect.top - canvasAbs.y) * viewZoom,
			canvasAbs.x + viewOffset.x
				+ (logicalRenderRect.right - canvasAbs.x) * viewZoom,
			canvasAbs.y + viewOffset.y
				+ (logicalRenderRect.bottom - canvasAbs.y) * viewZoom };
		descendantRenderTransformApplied =
			std::fabs(transformedRenderRect.left
				- expectedRenderRect.left) < 0.01f
			&& std::fabs(transformedRenderRect.top
				- expectedRenderRect.top) < 0.01f
			&& std::fabs(transformedRenderRect.right
				- expectedRenderRect.right) < 0.01f
			&& std::fabs(transformedRenderRect.bottom
				- expectedRenderRect.bottom) < 0.01f;
	}
	const auto offsetBeforePan = viewCanvas.GetViewOffset();
	const bool panDown = cui::framework::InputAccess::DispatchInput(viewCanvas, PointerInput(
		InputReportKind::PointerDown, MouseButton::Middle,
		100, 100, MouseButton::Middle));
	const bool panMove = cui::framework::InputAccess::DispatchInput(viewCanvas, PointerInput(
		InputReportKind::PointerMove, MouseButton::None,
		135, 125, MouseButton::Middle));
	const bool panUp = cui::framework::InputAccess::DispatchInput(viewCanvas, PointerInput(
		InputReportKind::PointerUp, MouseButton::Middle, 135, 125));
	const auto offsetAfterPan = viewCanvas.GetViewOffset();
	viewCanvas.ResetView();
	const POINT resetMapped = viewCanvas.CanvasToViewPoint(POINT{ 73, 91 });
	AppendFailure(failures,
		fittedZoom > 0.42f && fittedZoom < 0.45f
		&& logicalAtFocal.x == logicalAfterZoom.x
		&& logicalAtFocal.y == logicalAfterZoom.y
		&& descendantRenderTransformApplied
		&& panDown && panMove && panUp
		&& (offsetBeforePan.x != offsetAfterPan.x
			|| offsetBeforePan.y != offsetAfterPan.y)
		&& resetMapped.x == 73 && resetMapped.y == 91
		&& viewChangeCount >= 4
		&& viewCanvas.GetUndoCommandCount() == 0
		&& !viewCanvas.IsDocumentDirty(),
		L"canvas view: fit, focal zoom, descendant rendering, pan, reset, or non-document state failed"
		+ std::wstring(L" [fit=") + std::to_wstring(fittedZoom)
		+ L", focal=" + SelfTestFlag(
			logicalAtFocal.x == logicalAfterZoom.x
			&& logicalAtFocal.y == logicalAfterZoom.y)
		+ L", render=" + SelfTestFlag(descendantRenderTransformApplied)
		+ L", pan=" + SelfTestFlag(panDown && panMove && panUp)
		+ L", moved=" + SelfTestFlag(
			offsetBeforePan.x != offsetAfterPan.x
			|| offsetBeforePan.y != offsetAfterPan.y)
		+ L", reset=" + SelfTestFlag(
			resetMapped.x == 73 && resetMapped.y == 91)
		+ L", events=" + std::to_wstring(viewChangeCount)
		+ L", undo=" + std::to_wstring(viewCanvas.GetUndoCommandCount())
		+ L", dirty=" + SelfTestFlag(viewCanvas.IsDocumentDirty()) + L"]");

	DesignerCanvas contextCanvas(0, 0, 900, 680);
	contextCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 170, 150 });
	contextCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Label, POINT{ 390, 240 });
	const auto contextFirst = contextCanvas.GetAllControls().size() > 1
		? contextCanvas.GetAllControls()[1] : nullptr;
	const auto contextSecond = contextCanvas.GetAllControls().size() > 2
		? contextCanvas.GetAllControls()[2] : nullptr;
	size_t contextRequestCount = 0;
	DesignerCanvasContextMenuEventArgs lastContextRequest;
	contextCanvas.OnContextMenuRequested +=
		[&](const DesignerCanvasContextMenuEventArgs& args)
		{
			++contextRequestCount;
			lastContextRequest = args;
		};
	bool contextHitHandled = false;
	if (contextFirst && contextFirst->ControlInstance && contextSecond)
	{
		contextCanvas.RestoreSelectionByNames(
			{ contextSecond->Name }, contextSecond->Name, false);
		const auto size = contextFirst->ControlInstance->GetActualSizeDip();
		const POINT center = RoundedPoint(contextFirst->ControlInstance->GetAbsoluteLocationDip().x
				- contextCanvas.GetAbsoluteLocationDip().x + size.width / 2, contextFirst->ControlInstance->GetAbsoluteLocationDip().y
				- contextCanvas.GetAbsoluteLocationDip().y + size.height / 2);
		contextHitHandled = cui::framework::InputAccess::DispatchInput(contextCanvas, PointerInput(
			InputReportKind::PointerUp, MouseButton::Right,
			center.x, center.y));
	}
	const bool contextHitSelected = contextHitHandled
		&& contextRequestCount == 1 && lastContextRequest.HasSelection
		&& contextCanvas.GetSelectedControl() == contextFirst;
	const bool contextBlankHandled = cui::framework::InputAccess::DispatchInput(contextCanvas, PointerInput(
		InputReportKind::PointerUp, MouseButton::Right, 700, 28));
	const bool blankClearedSelection = contextBlankHandled
		&& contextRequestCount == 2 && !lastContextRequest.HasSelection
		&& contextCanvas.GetSelectedControls().empty();
	const bool selectedAll = contextCanvas.SelectAllInCurrentContainer(false)
		&& contextCanvas.GetSelectedControls().size() == 2;
	const bool keyboardMenuHandled = cui::framework::InputAccess::DispatchInput(contextCanvas, KeyInput(
		InputReportKind::KeyDown, Key::Apps));
	AppendFailure(failures,
		contextHitSelected && blankClearedSelection && selectedAll
		&& keyboardMenuHandled && contextRequestCount == 3
		&& lastContextRequest.HasSelection,
		L"canvas context menu: hit selection, blank request, select-all, or keyboard request failed"
			+ std::wstring(L" [hit=") + SelfTestFlag(contextHitSelected)
			+ L", blank=" + SelfTestFlag(blankClearedSelection)
			+ L", all=" + SelfTestFlag(selectedAll)
			+ L", keyboard=" + SelfTestFlag(keyboardMenuHandled)
			+ L", requests=" + std::to_wstring(contextRequestCount) + L"]");

	Designer commandSurfaceDesigner;
	commandSurfaceDesigner.InitializeComponents();
	const bool commandSurfaceInitial = commandSurfaceDesigner._canvas
		&& commandSurfaceDesigner._btnUndo
		&& commandSurfaceDesigner._btnRedo
		&& commandSurfaceDesigner._btnZoomOut
		&& commandSurfaceDesigner._btnZoomIn
		&& commandSurfaceDesigner._btnFitView
		&& commandSurfaceDesigner._btnGridSettings
		&& commandSurfaceDesigner._lblZoom
		&& commandSurfaceDesigner._canvasMenu
		&& commandSurfaceDesigner._gridMenu
		&& !commandSurfaceDesigner._btnUndo->IsEnabled
		&& !commandSurfaceDesigner._btnRedo->IsEnabled
		&& commandSurfaceDesigner._canvasMenu->ItemCount() == 18
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"原位粘贴", false) != nullptr
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"原位粘贴", false)->InputGestureText == L"Ctrl+Shift+V"
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"粘贴到此处", false) != nullptr
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"锁定控件", false) != nullptr
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"锁定控件", false)->InputGestureText == L"Ctrl+L"
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"视图", false) != nullptr
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"网格与吸附") != nullptr
		&& commandSurfaceDesigner._lblZoom->Text.find(L"%")
			!= std::wstring::npos;
	const auto gridStateBefore = commandSurfaceDesigner._canvas
		? commandSurfaceDesigner._canvas->GetCurrentDocumentStateId() : 0;
	commandSurfaceDesigner._canvas->SetGridVisible(false);
	commandSurfaceDesigner._canvas->SetSnapToGridEnabled(false);
	commandSurfaceDesigner._canvas->SetSnapToGuidesEnabled(false);
	commandSurfaceDesigner._canvas->SetGridSize(20);
	commandSurfaceDesigner.RefreshGridSettingsPresentation();
	auto* gridVisibleItem = commandSurfaceDesigner._gridMenu
		? commandSurfaceDesigner._gridMenu->FindItemByText(L"显示网格")
		: nullptr;
	auto* gridSizeItem = commandSurfaceDesigner._gridMenu
		? commandSurfaceDesigner._gridMenu->FindItemByText(L"网格间距 20 DIP")
		: nullptr;
	const bool gridSettingsReady = commandSurfaceDesigner._canvas
		&& !commandSurfaceDesigner._canvas->IsGridVisible()
		&& !commandSurfaceDesigner._canvas->IsSnapToGridEnabled()
		&& !commandSurfaceDesigner._canvas->IsSnapToGuidesEnabled()
		&& commandSurfaceDesigner._canvas->GetGridSize() == 20
		&& commandSurfaceDesigner._canvas->GetCurrentDocumentStateId()
			== gridStateBefore
		&& gridVisibleItem && !gridVisibleItem->IsChecked
		&& gridSizeItem && gridSizeItem->IsChecked
		&& ReadControlStringProperty(
			commandSurfaceDesigner._btnGridSettings, L"Content") == L"网格 20";
	const auto commandSurfaceAdd = commandSurfaceDesigner._canvas
		? commandSurfaceDesigner._canvas->AdoptVisualChildToCanvas(
			UIClass::UI_Button, POINT{ 220, 180 })
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing command-surface canvas");
	commandSurfaceDesigner.OnCanvasContextMenuRequested(
		DesignerCanvasContextMenuEventArgs{ POINT{ 220, 180 }, true });
	auto* commandSurfaceUndo = commandSurfaceDesigner._canvasMenu
		? commandSurfaceDesigner._canvasMenu->GetItem(0) : nullptr;
	auto* commandSurfaceRedo = commandSurfaceDesigner._canvasMenu
		? commandSurfaceDesigner._canvasMenu->GetItem(1) : nullptr;
	auto* commandSurfaceCut = commandSurfaceDesigner._canvasMenu
		? commandSurfaceDesigner._canvasMenu->FindItemByText(L"剪切", false)
		: nullptr;
	const bool commandSurfaceAfterAdd = commandSurfaceAdd.HasChanges()
		&& commandSurfaceDesigner._btnUndo->IsEnabled
		&& !commandSurfaceDesigner._btnRedo->IsEnabled
		&& commandSurfaceUndo && commandSurfaceUndo->IsEnabled
		&& ReadControlStringProperty(
			commandSurfaceUndo, L"Header").find(L"添加控件")
			!= std::wstring::npos
		&& commandSurfaceRedo && !commandSurfaceRedo->IsEnabled
		&& commandSurfaceCut && commandSurfaceCut->IsEnabled
		&& commandSurfaceDesigner._canvasMenu->IsOpen;
	commandSurfaceDesigner._canvasMenu->Hide();
	commandSurfaceDesigner.OnUndoClick();
	const bool commandSurfaceAfterUndo =
		!commandSurfaceDesigner._btnUndo->IsEnabled
		&& commandSurfaceDesigner._btnRedo->IsEnabled
		&& commandSurfaceDesigner._canvas->GetAllControls().size() == 1;
	commandSurfaceDesigner.OnRedoClick();
	const bool commandSurfaceAfterRedo =
		commandSurfaceDesigner._btnUndo->IsEnabled
		&& !commandSurfaceDesigner._btnRedo->IsEnabled
		&& commandSurfaceDesigner._canvas->GetAllControls().size() == 2;
	AppendFailure(failures,
		commandSurfaceInitial && commandSurfaceAfterAdd
		&& commandSurfaceAfterUndo && commandSurfaceAfterRedo
		&& gridSettingsReady,
		L"designer command surface: toolbar, context menu, history, or session-only grid settings failed");

	Designer contextPasteDesigner;
	contextPasteDesigner.InitializeComponents();
	const auto contextPasteAdd = contextPasteDesigner._canvas
		? contextPasteDesigner._canvas->AdoptVisualChildToCanvas(
			UIClass::UI_Button, POINT{ 220, 180 })
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing context-paste canvas");
	const auto contextPasteCopy = contextPasteDesigner._canvas
		? contextPasteDesigner._canvas->CopySelectedControls()
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing context-paste canvas");
	const bool contextPasteSourceAvailable = contextPasteDesigner._canvas
		&& contextPasteDesigner._canvas->CanPasteControlsFromClipboard()
		&& contextPasteDesigner._btnPaste
		&& contextPasteDesigner._btnPaste->IsEnabled;
	if (contextPasteDesigner._canvas)
		(void)contextPasteDesigner._canvas->ResetDocumentHistoryAsSaved();
	const POINT requestedPastePoint{ 520, 360 };
	const auto requestedPasteViewPoint = contextPasteDesigner._canvas
		? contextPasteDesigner._canvas->CanvasToViewPoint(requestedPastePoint)
		: POINT{};
	const auto contextPasteBlock = contextPasteDesigner._canvas
		? contextPasteDesigner._canvas->BeginDocumentEditTransaction(
			L"ClipboardAvailabilityTest")
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing context-paste canvas");
	contextPasteDesigner.RefreshCommandAvailability();
	contextPasteDesigner.OnCanvasContextMenuRequested(
		DesignerCanvasContextMenuEventArgs{
			requestedPasteViewPoint, true });
	auto* blockedContextPaste = contextPasteDesigner._canvasMenu
		? contextPasteDesigner._canvasMenu->FindItemByText(L"粘贴", false)
		: nullptr;
	auto* blockedContextPasteInPlace = contextPasteDesigner._canvasMenu
		? contextPasteDesigner._canvasMenu->FindItemByText(
			L"原位粘贴", false) : nullptr;
	auto* blockedContextPasteHere = contextPasteDesigner._canvasMenu
		? contextPasteDesigner._canvasMenu->FindItemByText(
			L"粘贴到此处", false) : nullptr;
	const bool contextPasteBlockedDuringTransaction = contextPasteBlock.Succeeded()
		&& contextPasteDesigner._btnPaste
		&& !contextPasteDesigner._btnPaste->IsEnabled
		&& blockedContextPaste && !blockedContextPaste->IsEnabled
		&& blockedContextPasteInPlace && !blockedContextPasteInPlace->IsEnabled
		&& blockedContextPasteHere && !blockedContextPasteHere->IsEnabled;
	if (contextPasteDesigner._canvasMenu)
		contextPasteDesigner._canvasMenu->Hide();
	if (contextPasteDesigner._canvas)
		(void)contextPasteDesigner._canvas->RollbackDocumentEditTransaction();
	contextPasteDesigner.RefreshCommandAvailability();
	if (contextPasteDesigner._btnPaste)
		contextPasteDesigner._btnPaste->IsEnabled = false;
	const bool contextPasteClipboardUpdateHandled =
		contextPasteDesigner.OnPlatformMessage(
			WM_CLIPBOARDUPDATE, 0, 0).has_value()
		&& contextPasteDesigner._btnPaste
		&& contextPasteDesigner._btnPaste->IsEnabled;
	const bool contextPasteEmptyTextPublished =
		ReplaceClipboardTextForSelfTest(L"");
	bool contextPasteEmptyTextDisabled = false;
	for (int attempt = 0;
		attempt < 20 && !contextPasteEmptyTextDisabled; ++attempt)
	{
		const bool handled = contextPasteDesigner.OnPlatformMessage(
			WM_CLIPBOARDUPDATE, 0, 0).has_value();
		contextPasteEmptyTextDisabled = handled
			&& contextPasteDesigner._btnPaste
			&& !contextPasteDesigner._btnPaste->IsEnabled
			&& !contextPasteDesigner._canvas->CanPasteControlsFromClipboard();
		if (!contextPasteEmptyTextDisabled) ::Sleep(10);
	}
	const auto contextPasteRestoreCopy = contextPasteDesigner._canvas
		? contextPasteDesigner._canvas->CopySelectedControls()
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing context-paste canvas");
	const bool contextPasteRestored = contextPasteRestoreCopy.Succeeded()
		&& contextPasteDesigner._btnPaste
		&& contextPasteDesigner._btnPaste->IsEnabled;
	contextPasteDesigner.OnCanvasContextMenuRequested(
		DesignerCanvasContextMenuEventArgs{
			requestedPasteViewPoint, true });
	auto* pasteHereCommand = contextPasteDesigner._canvasMenu
		? contextPasteDesigner._canvasMenu->FindItemByText(
			L"粘贴到此处", false) : nullptr;
	if (pasteHereCommand)
		(void)pasteHereCommand->Invoke();
	if (contextPasteDesigner._canvasMenu)
		contextPasteDesigner._canvasMenu->Hide();
	const auto contextPastedControl = contextPasteDesigner._canvas
		? contextPasteDesigner._canvas->GetSelectedControl() : nullptr;
	POINT contextPastedCanvasLocation{};
	if (contextPastedControl && contextPastedControl->ControlInstance
		&& contextPasteDesigner._canvas)
	{
		contextPastedCanvasLocation = RoundedPoint(
			contextPastedControl->ControlInstance->GetAbsoluteLocationDip().x
				- contextPasteDesigner._canvas->GetAbsoluteLocationDip().x,
			contextPastedControl->ControlInstance->GetAbsoluteLocationDip().y
				- contextPasteDesigner._canvas->GetAbsoluteLocationDip().y);
	}
	const auto undoContextPaste = contextPasteDesigner._canvas
		? contextPasteDesigner._canvas->UndoCommand()
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing context-paste canvas");
	AppendFailure(failures,
		contextPasteAdd.HasChanges() && contextPasteCopy.Succeeded()
		&& contextPasteSourceAvailable
		&& contextPasteBlockedDuringTransaction
		&& contextPasteClipboardUpdateHandled
		&& contextPasteEmptyTextPublished
		&& contextPasteEmptyTextDisabled
		&& contextPasteRestored,
		L"designer paste availability: source, transaction gate, clipboard update, empty text, or restore failed"
			+ std::wstring(L" [add=") + SelfTestFlag(contextPasteAdd.HasChanges())
			+ L", copy=" + SelfTestFlag(contextPasteCopy.Succeeded())
			+ L", source=" + SelfTestFlag(contextPasteSourceAvailable)
			+ L", blocked=" + SelfTestFlag(contextPasteBlockedDuringTransaction)
			+ L", update=" + SelfTestFlag(contextPasteClipboardUpdateHandled)
			+ L", emptyPublished=" + SelfTestFlag(contextPasteEmptyTextPublished)
			+ L", emptyDisabled=" + SelfTestFlag(contextPasteEmptyTextDisabled)
			+ L", restored=" + SelfTestFlag(contextPasteRestored) + L"]");
	AppendFailure(failures,
		pasteHereCommand && pasteHereCommand->IsEnabled
		&& contextPasteDesigner._hasCanvasContextPastePoint
		&& contextPasteDesigner._canvasContextPastePoint.x
			== requestedPastePoint.x
		&& contextPasteDesigner._canvasContextPastePoint.y
			== requestedPastePoint.y
		&& contextPastedControl
		&& contextPastedCanvasLocation.x == requestedPastePoint.x
		&& contextPastedCanvasLocation.y == requestedPastePoint.y
		&& undoContextPaste.HasChanges()
		&& contextPasteDesigner._canvas->GetAllControls().size() == 2,
		L"designer context paste: view-to-canvas point, command route, placement, or one-step Undo failed");

	Designer outlineDesigner;
	outlineDesigner.InitializeComponents();
	outlineDesigner._canvas->AdoptVisualChildToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 280, 210 });
	const auto outlineParent = outlineDesigner._canvas->GetSelectedControl();
	if (outlineParent && outlineParent->ControlInstance)
	{
		const POINT inside = RoundedPoint(outlineParent->ControlInstance->GetAbsoluteLocationDip().x
				- outlineDesigner._canvas->GetAbsoluteLocationDip().x + 30, outlineParent->ControlInstance->GetAbsoluteLocationDip().y
				- outlineDesigner._canvas->GetAbsoluteLocationDip().y + 30);
		outlineDesigner._canvas->AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, inside);
	}
	const auto outlineChild = outlineDesigner._canvas->GetSelectedControl();
	const auto outlineContentRoot = FindControl(
		*outlineDesigner._canvas, L"contentRoot");
	if (outlineChild && outlineChild->ControlInstance)
		outlineChild->ControlInstance->Visibility = Visibility::Hidden;
	if (outlineParent) outlineParent->IsLocked = true;
	outlineDesigner.RebuildDocumentOutline();
	outlineDesigner.SetSidebarView(true);
	TreeViewItem* outlineParentNode = outlineParent
		? outlineDesigner._outlineNodesByStableId[outlineParent->StableId]
		: nullptr;
	TreeViewItem* outlineChildNode = outlineChild
		? outlineDesigner._outlineNodesByStableId[outlineChild->StableId]
		: nullptr;
	bool outlineNested = false;
	if (outlineParentNode && outlineChildNode)
		for (size_t index = 0;
			index < outlineParentNode->AuthoredItemCount(); ++index)
			outlineNested = outlineNested
				|| outlineParentNode->GetAuthoredItem(index) == outlineChildNode;
	std::wstring outlineChildHeader;
	std::wstring outlineParentHeader;
	const bool outlineHiddenMarked = outlineChildNode
		&& outlineChildNode->GetHeader().TryGetString(outlineChildHeader)
		&& outlineChildHeader.find(L"[Hidden]") != std::wstring::npos;
	const bool outlineLockedMarked = outlineParentNode
		&& outlineParentNode->GetHeader().TryGetString(outlineParentHeader)
		&& outlineParentHeader.rfind(L"[锁定]", 0) == 0;
	if (outlineChildNode)
	{
		(void)outlineDesigner._outlineTree->SelectItem(
			outlineChildNode, false);
		outlineDesigner.OnDocumentOutlineSelectionChanged();
	}
	const bool outlineSelectedHidden = outlineChild
		&& outlineDesigner._canvas->GetSelectedControl() == outlineChild;
	if (outlineDesigner._outlineWindowNode)
	{
		(void)outlineDesigner._outlineTree->SelectItem(
			outlineDesigner._outlineWindowNode, false);
		outlineDesigner.OnDocumentOutlineSelectionChanged();
	}
	const bool outlineSelectedWindow =
		outlineDesigner._canvas->GetSelectedControls().empty();
	outlineDesigner._canvas->AdoptVisualChildToCanvasCore(
		UIClass::UI_Label, POINT{ 760, 520 });
	const auto outlineRootSibling =
		outlineDesigner._canvas->GetSelectedControl();
	if (outlineChild)
		outlineDesigner._canvas->RestoreSelectionByNames(
			{ outlineChild->Name }, outlineChild->Name, false);
	const auto outlineMove = outlineChild && outlineRootSibling
		? outlineDesigner._canvas->MoveControlInHierarchy(
			outlineChild->StableId, outlineRootSibling->StableId,
			DesignerHierarchyDropPosition::Before)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing outline hierarchy controls");
	const bool outlineMovedToRoot = outlineMove.HasChanges()
		&& outlineChild && outlineRootSibling
		&& outlineContentRoot
		&& outlineChild->DesignerParent
			== outlineContentRoot->ControlInstance
		&& outlineChild->ControlInstance->GetVisualParent()
			== outlineRootSibling->ControlInstance->GetVisualParent()
		&& outlineChild->ControlInstance->GetVisualParent()->IndexOfVisualChild(
			outlineChild->ControlInstance)
			< outlineChild->ControlInstance->GetVisualParent()->IndexOfVisualChild(
				outlineRootSibling->ControlInstance);
	const auto outlineMoveUndo = outlineDesigner._canvas->UndoCommand();
	const bool outlineMoveUndone = outlineMoveUndo.HasChanges()
		&& outlineChild && outlineParent
		&& outlineChild->DesignerParent == outlineParent->ControlInstance
		&& outlineChild->ControlInstance->GetVisualParent()
			== outlineParent->ControlInstance;
	const auto outlineMoveRedo = outlineDesigner._canvas->RedoCommand();
	const bool outlineMoveRedone = outlineMoveRedo.HasChanges()
		&& outlineChild && outlineRootSibling
		&& outlineContentRoot
		&& outlineChild->DesignerParent
			== outlineContentRoot->ControlInstance
		&& outlineChild->ControlInstance->GetVisualParent()
			== outlineRootSibling->ControlInstance->GetVisualParent();
	const auto latestParentNode = outlineParent
		? outlineDesigner._outlineNodesByStableId[outlineParent->StableId]
		: nullptr;
	if (latestParentNode) latestParentNode->SetIsExpanded(false);
	outlineDesigner.RebuildDocumentOutline();
	const auto rebuiltParentNode = outlineParent
		? outlineDesigner._outlineNodesByStableId[outlineParent->StableId]
		: nullptr;
	AppendFailure(failures,
		outlineDesigner._btnToolboxView
		&& outlineDesigner._btnOutlineView
		&& outlineDesigner._outlineTree
		&& outlineDesigner._outlineTree->IsVisible
		&& !outlineDesigner._toolBox->IsVisible
		&& outlineNested && outlineHiddenMarked && outlineLockedMarked
		&& outlineSelectedHidden && outlineSelectedWindow
		&& outlineMovedToRoot && outlineMoveUndone && outlineMoveRedone
		&& rebuiltParentNode && !rebuiltParentNode->GetIsExpanded(),
		L"document outline: selection, drag hierarchy delta, Undo/Redo, view switch, or expansion persistence failed");

	Designer outlineShortcutDesigner;
	outlineShortcutDesigner.InitializeComponents();
	const auto outlineShortcutSetup =
		outlineShortcutDesigner._canvas->PasteControlsFromXamlText(
			clipboardXaml);
	(void)outlineShortcutDesigner._canvas->ResetDocumentHistoryAsSaved();
	outlineShortcutDesigner.RebuildDocumentOutline();
	outlineShortcutDesigner.SetSidebarView(true);
	auto selectOutlineNode = [&](const std::wstring& name) -> bool
	{
		const auto control = std::find_if(
			outlineShortcutDesigner._canvas->GetAllControls().begin(),
			outlineShortcutDesigner._canvas->GetAllControls().end(),
			[&](const auto& candidate)
			{
				return candidate && candidate->Name == name;
			});
		if (control == outlineShortcutDesigner._canvas->GetAllControls().end())
			return false;
		const auto node = outlineShortcutDesigner._outlineNodesByStableId.find(
			(*control)->StableId);
		if (node == outlineShortcutDesigner._outlineNodesByStableId.end())
			return false;
		(void)outlineShortcutDesigner._outlineTree->SelectItem(
			node->second, false);
		outlineShortcutDesigner.OnDocumentOutlineSelectionChanged();
		return true;
	};
	const bool outlineShortcutChildSelected = selectOutlineNode(L"button1");
	const bool outlineShortcutTreeFocused =
		outlineShortcutDesigner.GetKeyboardFocusedElement() == outlineShortcutDesigner._outlineTree;
	const bool outlineShortcutCopied = outlineShortcutDesigner.QueueOutlineShortcut(
		Key::C, true, false);
	const bool outlineShortcutParentSelected = selectOutlineNode(L"panel1");
	outlineShortcutDesigner.SetKeyboardFocus(
		outlineShortcutDesigner._outlineTree, false);
	const bool outlineShortcutPasted =
		outlineShortcutDesigner.OnPreviewInputReport(KeyInput(
			InputReportKind::KeyDown, Key::V, ModifierKeys::Control));
	const size_t outlineShortcutCountAfterPaste =
		outlineShortcutDesigner._canvas->GetAllControls().size();
	const bool outlineShortcutCharacterSuppressed =
		cui::framework::WindowAccess::TextComposition(outlineShortcutDesigner)
			.ProcessWindowMessage(WM_CHAR, L'\x16', 0).Recognized
		&& outlineShortcutDesigner._canvas->GetAllControls().size()
			== outlineShortcutCountAfterPaste;
	const bool outlineShortcutParentReselected = selectOutlineNode(L"panel1");
	const bool outlineShortcutInPlacePasted =
		outlineShortcutDesigner.QueueOutlineShortcut(Key::V, true, true);
	DesignerModel::DesignDocument outlineShortcutPasteDocument;
	std::wstring outlineShortcutError;
	const bool outlineShortcutPasteCaptured =
		outlineShortcutDesigner._canvas->BuildDesignDocument(
			outlineShortcutPasteDocument, &outlineShortcutError);
	auto findOutlineShortcutNode = [](
		const DesignerModel::DesignDocument& document,
		const std::wstring& name) -> const DesignerModel::DesignNode*
	{
		const auto found = std::find_if(
			document.Nodes.begin(), document.Nodes.end(),
			[&](const auto& node) { return node.Name == name; });
		return found == document.Nodes.end() ? nullptr : &*found;
	};
	const auto* outlineShortcutPanel = findOutlineShortcutNode(
		outlineShortcutPasteDocument, L"panel1");
	const auto* outlineShortcutButton = findOutlineShortcutNode(
		outlineShortcutPasteDocument, L"button2");
	const auto* outlineShortcutInPlaceButton = findOutlineShortcutNode(
		outlineShortcutPasteDocument, L"button3");
	const auto outlineShortcutInPlaceLocation =
		clipboardNodeLocation(outlineShortcutInPlaceButton);
	const bool outlineShortcutParentPreserved = outlineShortcutPanel
		&& outlineShortcutButton
		&& outlineShortcutInPlaceButton
		&& outlineShortcutButton->ParentId == outlineShortcutPanel->Id
		&& outlineShortcutInPlaceButton->ParentId == outlineShortcutPanel->Id
		&& outlineShortcutInPlaceLocation.x == 10
		&& outlineShortcutInPlaceLocation.y == 12;
	const bool outlineShortcutInPlaceUndone =
		outlineShortcutDesigner.QueueOutlineShortcut(Key::Z, true, false)
		&& outlineShortcutDesigner._canvas->GetAllControls().size() == 4;
	const bool outlineShortcutUndone = outlineShortcutDesigner.QueueOutlineShortcut(
		Key::Z, true, false)
		&& outlineShortcutDesigner._canvas->GetAllControls().size() == 3;
	const bool outlineShortcutReselected = selectOutlineNode(L"button1");
	const auto outlineShortcutLockTarget =
		outlineShortcutDesigner._canvas->GetSelectedControl();
	const bool outlineShortcutLocked =
		outlineShortcutDesigner.QueueOutlineShortcut(Key::L, true, false)
		&& outlineShortcutLockTarget && outlineShortcutLockTarget->IsLocked;
	const bool outlineShortcutUnlocked =
		outlineShortcutDesigner.QueueOutlineShortcut(Key::L, true, false)
		&& outlineShortcutLockTarget && !outlineShortcutLockTarget->IsLocked;
	const bool outlineShortcutDuplicated =
		outlineShortcutDesigner.QueueOutlineShortcut(Key::D, true, false)
		&& outlineShortcutDesigner._canvas->GetAllControls().size() == 4;
	const bool outlineShortcutDeleted =
		outlineShortcutDesigner.QueueOutlineShortcut(
			Key::Delete, false, false)
		&& outlineShortcutDesigner._canvas->GetAllControls().size() == 3;
	AppendFailure(failures,
		outlineShortcutSetup.HasChanges()
		&& outlineShortcutChildSelected && outlineShortcutTreeFocused
		&& outlineShortcutCopied
		&& outlineShortcutParentSelected && outlineShortcutPasted
		&& outlineShortcutCharacterSuppressed
		&& outlineShortcutParentReselected && outlineShortcutInPlacePasted
		&& outlineShortcutPasteCaptured && outlineShortcutParentPreserved
		&& outlineShortcutInPlaceUndone
		&& outlineShortcutUndone && outlineShortcutReselected
		&& outlineShortcutLocked && outlineShortcutUnlocked
		&& outlineShortcutDuplicated && outlineShortcutDeleted,
		L"document outline: window-level edit shortcuts did not preserve target semantics"
		+ std::wstring(L" [setup=")
		+ SelfTestFlag(outlineShortcutSetup.HasChanges())
		+ L", treeFocus=" + SelfTestFlag(outlineShortcutTreeFocused)
		+ L", copied=" + SelfTestFlag(outlineShortcutCopied)
		+ L", pasted=" + SelfTestFlag(outlineShortcutPasted)
		+ L", inPlace=" + SelfTestFlag(outlineShortcutInPlacePasted)
		+ L", charSuppressed="
		+ SelfTestFlag(outlineShortcutCharacterSuppressed)
		+ L", parent=" + SelfTestFlag(outlineShortcutParentPreserved)
		+ L", undo=" + SelfTestFlag(outlineShortcutUndone)
		+ L", lock=" + SelfTestFlag(outlineShortcutLocked)
		+ L", unlock=" + SelfTestFlag(outlineShortcutUnlocked)
		+ L", duplicate=" + SelfTestFlag(outlineShortcutDuplicated)
		+ L", delete=" + SelfTestFlag(outlineShortcutDeleted)
		+ L", error=" + outlineShortcutError + L"]");

	DesignerCanvas subtreeCanvas(0, 0, 900, 680);
	subtreeCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 260, 220 });
	const auto subtreeRoot = subtreeCanvas.GetSelectedControl();
	auto* const subtreeRootRuntime = subtreeRoot
		? subtreeRoot->ControlInstance : nullptr;
	if (subtreeRootRuntime)
	{
		const POINT inside = RoundedPoint(subtreeRootRuntime->GetAbsoluteLocationDip().x - subtreeCanvas.GetAbsoluteLocationDip().x + 60, subtreeRootRuntime->GetAbsoluteLocationDip().y - subtreeCanvas.GetAbsoluteLocationDip().y + 50);
		subtreeCanvas.AdoptVisualChildToCanvasCore(UIClass::UI_Button, inside);
		subtreeCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Label, POINT{ inside.x + 20, inside.y + 70 });
	}
	const auto subtreeFirstChild = subtreeCanvas.GetAllControls().size() > 2
		? subtreeCanvas.GetAllControls()[2] : nullptr;
	const auto subtreeSecondChild = subtreeCanvas.GetAllControls().size() > 3
		? subtreeCanvas.GetAllControls()[3] : nullptr;
	auto* const subtreeFirstRuntime = subtreeFirstChild
		? subtreeFirstChild->ControlInstance : nullptr;
	auto* const subtreeSecondRuntime = subtreeSecondChild
		? subtreeSecondChild->ControlInstance : nullptr;
	DesignerModel::DesignDocument subtreeBaseline;
	std::wstring subtreeBaselineError;
	const bool subtreeSetup = subtreeRoot && subtreeRootRuntime
		&& subtreeFirstChild && subtreeFirstRuntime
		&& subtreeSecondChild && subtreeSecondRuntime
		&& subtreeRootRuntime->VisualChildCount() == 2
		&& subtreeCanvas.BuildDesignDocument(
			subtreeBaseline, &subtreeBaselineError);
	AppendFailure(failures, subtreeSetup,
		L"subtree delta: nested setup failed");
	if (subtreeSetup)
	{
		(void)subtreeCanvas.ResetDocumentHistoryAsSaved();
		subtreeCanvas.RestoreSelectionByNames(
			{ subtreeRoot->Name }, subtreeRoot->Name, false);
		const auto deleteSubtree = subtreeCanvas.DeleteSelectedControl();
		const auto deleteSubtreeMemory =
			subtreeCanvas.GetCommandHistoryMemoryUsage();
		AppendFailure(failures,
			deleteSubtree.HasChanges()
			&& subtreeCanvas.GetAllControls().size() == 1
			&& deleteSubtreeMemory > 0
			&& deleteSubtreeMemory < 160 * 1024,
			L"subtree delta: nested delete retained a full document or left wrappers (bytes="
				+ std::to_wstring(deleteSubtreeMemory) + L")");
		const auto undoSubtree = subtreeCanvas.UndoCommand();
		DesignerModel::DesignDocument restoredSubtree;
		std::wstring restoredSubtreeError;
		AppendFailure(failures,
			undoSubtree.HasChanges()
			&& subtreeCanvas.BuildDesignDocument(
				restoredSubtree, &restoredSubtreeError)
			&& restoredSubtree == subtreeBaseline
			&& FindControl(subtreeCanvas, subtreeRoot->Name) == subtreeRoot
			&& FindControl(subtreeCanvas, subtreeFirstChild->Name)
				== subtreeFirstChild
			&& FindControl(subtreeCanvas, subtreeSecondChild->Name)
				== subtreeSecondChild
			&& subtreeRoot->ControlInstance == subtreeRootRuntime
			&& subtreeFirstChild->ControlInstance == subtreeFirstRuntime
			&& subtreeSecondChild->ControlInstance == subtreeSecondRuntime
			&& subtreeRootRuntime->VisualChildCount() == 2
			&& subtreeRootRuntime->GetVisualChild(0) == subtreeFirstRuntime
			&& subtreeRootRuntime->GetVisualChild(1) == subtreeSecondRuntime
			&& subtreeCanvas.GetSelectedControl() == subtreeRoot,
			L"subtree delta: nested undo lost document, order, identity, or selection");
		AppendFailure(failures,
			subtreeCanvas.RedoCommand().HasChanges()
			&& subtreeCanvas.GetAllControls().size() == 1,
			L"subtree delta: nested redo did not detach the full subtree");
	}

	DesignerCanvas siblingDeleteCanvas(0, 0, 900, 680);
	siblingDeleteCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Canvas, POINT{ 300, 240 });
	const auto siblingParent = siblingDeleteCanvas.GetSelectedControl();
	auto* const siblingParentRuntime = siblingParent
		? siblingParent->ControlInstance : nullptr;
	if (siblingParentRuntime)
	{
		const POINT inside = RoundedPoint(siblingParentRuntime->GetAbsoluteLocationDip().x
				- siblingDeleteCanvas.GetAbsoluteLocationDip().x + 55, siblingParentRuntime->GetAbsoluteLocationDip().y
				- siblingDeleteCanvas.GetAbsoluteLocationDip().y + 45);
		for (int index = 0; index < 3; ++index)
			siblingDeleteCanvas.AdoptVisualChildToCanvasCore(
				UIClass::UI_Button,
				POINT{ inside.x + index * 45, inside.y + index * 45 });
	}
	const auto siblingFirst = siblingDeleteCanvas.GetAllControls().size() > 2
		? siblingDeleteCanvas.GetAllControls()[2] : nullptr;
	const auto siblingMiddle = siblingDeleteCanvas.GetAllControls().size() > 3
		? siblingDeleteCanvas.GetAllControls()[3] : nullptr;
	const auto siblingLast = siblingDeleteCanvas.GetAllControls().size() > 4
		? siblingDeleteCanvas.GetAllControls()[4] : nullptr;
	auto* const siblingFirstRuntime = siblingFirst
		? siblingFirst->ControlInstance : nullptr;
	auto* const siblingMiddleRuntime = siblingMiddle
		? siblingMiddle->ControlInstance : nullptr;
	auto* const siblingLastRuntime = siblingLast
		? siblingLast->ControlInstance : nullptr;
	const bool siblingSetup = siblingParentRuntime
		&& siblingFirstRuntime && siblingMiddleRuntime && siblingLastRuntime
		&& siblingParentRuntime->VisualChildCount() == 3;
	AppendFailure(failures, siblingSetup,
		L"subtree delta: multi-root sibling setup failed");
	if (siblingSetup)
	{
		(void)siblingDeleteCanvas.ResetDocumentHistoryAsSaved();
		siblingDeleteCanvas.RestoreSelectionByNames(
			{ siblingFirst->Name, siblingLast->Name },
			siblingLast->Name,
			false);
		const auto deleteSiblings =
			siblingDeleteCanvas.DeleteSelectedControl();
		AppendFailure(failures,
			deleteSiblings.HasChanges()
			&& siblingParentRuntime->VisualChildCount() == 1
			&& siblingParentRuntime->GetVisualChild(0) == siblingMiddleRuntime,
			L"subtree delta: multi-root delete damaged the remaining sibling");
		const auto undoSiblings = siblingDeleteCanvas.UndoCommand();
		AppendFailure(failures,
			undoSiblings.HasChanges()
			&& siblingParentRuntime->VisualChildCount() == 3
			&& siblingParentRuntime->GetVisualChild(0) == siblingFirstRuntime
			&& siblingParentRuntime->GetVisualChild(1) == siblingMiddleRuntime
			&& siblingParentRuntime->GetVisualChild(2) == siblingLastRuntime
			&& FindControl(siblingDeleteCanvas, siblingFirst->Name)
				== siblingFirst
			&& FindControl(siblingDeleteCanvas, siblingLast->Name)
				== siblingLast,
			L"subtree delta: multi-root undo lost sibling order or identity");
	}

	DesignerCanvas guardedAddCanvas(0, 0, 800, 640);
	const auto guardedAdd = guardedAddCanvas.AdoptVisualChildToCanvas(
		UIClass::UI_Button, POINT{ 150, 150 });
	const auto guardedAddIdentity = guardedAddCanvas.GetSelectedControl();
	auto* const guardedAddRuntime = guardedAddIdentity
		? guardedAddIdentity->ControlInstance : nullptr;
	const auto guardedAddName = guardedAddIdentity
		? guardedAddIdentity->Name : std::wstring{};
	const auto guardedAddText = guardedAddRuntime
		? ReadControlStringProperty(guardedAddRuntime, L"Content")
		: std::wstring{};
	const bool guardedAddHadLocalContent = guardedAddRuntime
		&& guardedAddRuntime->HasPropertyValue(
			L"Content", DependencyPropertyValueSource::Local);
	if (guardedAddRuntime)
		(void)WriteControlStringProperty(
			guardedAddRuntime, L"Content", L"ExternalMutation");
	const auto guardedAddUndo = guardedAddCanvas.UndoCommand();
	AppendFailure(failures,
		guardedAdd.HasChanges()
		&& guardedAddUndo.State == DesignerDocumentTransactionState::Failed
		&& !guardedAddUndo.DocumentRestored
		&& guardedAddCanvas.GetUndoCommandCount() == 1
		&& guardedAddCanvas.GetAllControls().size() == 2
		&& guardedAddCanvas.GetSelectedControl() == guardedAddIdentity
		&& guardedAddRuntime
		&& ReadControlStringProperty(
			guardedAddRuntime, L"Content") == L"ExternalMutation",
		L"subtree delta: mismatched Add endpoint damaged state or history"
		+ std::wstring(L" [add=") + SelfTestFlag(guardedAdd.HasChanges())
		+ L", undoState=" + std::to_wstring(
			static_cast<int>(guardedAddUndo.State))
		+ L", restored=" + SelfTestFlag(guardedAddUndo.DocumentRestored)
		+ L", undoCount=" + std::to_wstring(
			guardedAddCanvas.GetUndoCommandCount())
		+ L", controls=" + std::to_wstring(
			guardedAddCanvas.GetAllControls().size())
		+ L", selected=" + SelfTestFlag(
			guardedAddCanvas.GetSelectedControl() == guardedAddIdentity)
		+ L", content=" + ReadControlStringProperty(
			guardedAddRuntime, L"Content")
		+ L", error=" + guardedAddUndo.Error + L"]");
	if (guardedAddRuntime)
	{
		if (guardedAddHadLocalContent)
			(void)WriteControlStringProperty(
				guardedAddRuntime, L"Content", guardedAddText);
		else
			(void)guardedAddRuntime->ResetPropertyValue(L"Content");
	}
	const auto repairedAddUndo = guardedAddCanvas.UndoCommand();
	guardedAddCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 310, 190 });
	const auto conflictingAdd = guardedAddCanvas.GetSelectedControl();
	if (conflictingAdd) conflictingAdd->Name = guardedAddName;
	const auto conflictingRedo = guardedAddCanvas.RedoCommand();
	AppendFailure(failures,
		repairedAddUndo.HasChanges()
		&& conflictingRedo.State == DesignerDocumentTransactionState::Failed
		&& guardedAddCanvas.GetRedoCommandCount() == 1
		&& guardedAddCanvas.GetAllControls().size() == 2,
		L"subtree delta: absent-name conflict did not preserve redo history"
		+ std::wstring(L" [undo=") + SelfTestFlag(repairedAddUndo.HasChanges())
		+ L", redoState=" + std::to_wstring(
			static_cast<int>(conflictingRedo.State))
		+ L", redoCount=" + std::to_wstring(
			guardedAddCanvas.GetRedoCommandCount())
		+ L", controls=" + std::to_wstring(
			guardedAddCanvas.GetAllControls().size())
		+ L", error=" + conflictingRedo.Error + L"]");
	guardedAddCanvas.DeleteSelectedControlCore();
	AppendFailure(failures,
		guardedAddCanvas.RedoCommand().HasChanges()
		&& guardedAddCanvas.GetSelectedControl() == guardedAddIdentity
		&& guardedAddIdentity
		&& guardedAddIdentity->ControlInstance == guardedAddRuntime,
		L"subtree delta: Add redo did not recover after name conflict repair");

	DesignerCanvas rebuiltDeleteCanvas(0, 0, 900, 680);
	rebuiltDeleteCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_ToolBar, POINT{ 300, 180 });
	const auto originalToolBar = rebuiltDeleteCanvas.GetSelectedControl();
	const auto originalToolBarName = originalToolBar
		? originalToolBar->Name : std::wstring{};
	auto* const originalToolBarRuntime = originalToolBar
		? dynamic_cast<ToolBar*>(originalToolBar->ControlInstance) : nullptr;
	if (originalToolBarRuntime)
	{
		const POINT inside = RoundedPoint(originalToolBarRuntime->GetAbsoluteLocationDip().x
				- rebuiltDeleteCanvas.GetAbsoluteLocationDip().x + 50, originalToolBarRuntime->GetAbsoluteLocationDip().y
				- rebuiltDeleteCanvas.GetAbsoluteLocationDip().y + 16);
		rebuiltDeleteCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, inside);
	}
	const auto rebuiltDeleteChild = rebuiltDeleteCanvas.GetSelectedControl();
	auto* const rebuiltDeleteChildRuntime = rebuiltDeleteChild
		? rebuiltDeleteChild->ControlInstance : nullptr;
	DesignerModel::DesignDocument rebuiltDeleteBaseline;
	std::wstring rebuiltDeleteBaselineError;
	const bool rebuiltDeleteSetup = originalToolBar
		&& originalToolBarRuntime && rebuiltDeleteChild
		&& rebuiltDeleteChildRuntime
		&& rebuiltDeleteCanvas.BuildDesignDocument(
			rebuiltDeleteBaseline, &rebuiltDeleteBaselineError);
	AppendFailure(failures, rebuiltDeleteSetup,
		L"subtree delta: rebuild/ToolBar setup failed");
	if (rebuiltDeleteSetup)
	{
		(void)rebuiltDeleteCanvas.ResetDocumentHistoryAsSaved();
		rebuiltDeleteCanvas.RestoreSelectionByNames(
			{ rebuiltDeleteChild->Name }, rebuiltDeleteChild->Name, false);
		const auto deleteBeforeRebuild =
			rebuiltDeleteCanvas.DeleteSelectedControl();
		const auto rebuildTransaction =
			rebuiltDeleteCanvas.ExecuteDocumentEditTransaction(
				L"SelfTest:RebuildWhileSubtreeAbsent",
				[&rebuiltDeleteCanvas](std::wstring& error)
				{
					return rebuiltDeleteCanvas.ApplyDesignedWindowProperty(
						L"Title",
						{ DesignerStyleValueKind::String,
							L"Temporary rebuild title" },
						nullptr, &error);
				});
		const auto undoRebuild = rebuiltDeleteCanvas.UndoCommand();
		const auto rebuiltToolBarWrapper = FindControl(
			rebuiltDeleteCanvas, originalToolBarName);
		auto* const rebuiltToolBarRuntime = rebuiltToolBarWrapper
			? dynamic_cast<ToolBar*>(
				rebuiltToolBarWrapper->ControlInstance) : nullptr;
		const auto undoDeleteAfterRebuild =
			rebuiltDeleteCanvas.UndoCommand();
		DesignerModel::DesignDocument rebuiltDeleteRestored;
		std::wstring rebuiltDeleteRestoredError;
		AppendFailure(failures,
			deleteBeforeRebuild.HasChanges()
			&& rebuildTransaction.HasChanges()
			&& undoRebuild.HasChanges()
			&& rebuiltToolBarWrapper
			&& rebuiltToolBarWrapper != originalToolBar
			&& rebuiltToolBarRuntime
			&& undoDeleteAfterRebuild.HasChanges()
			&& rebuiltDeleteCanvas.BuildDesignDocument(
				rebuiltDeleteRestored, &rebuiltDeleteRestoredError)
			&& rebuiltDeleteRestored == rebuiltDeleteBaseline
			&& FindControl(
				rebuiltDeleteCanvas, rebuiltDeleteChild->Name)
				== rebuiltDeleteChild
			&& rebuiltDeleteChild->ControlInstance
				== rebuiltDeleteChildRuntime
			&& rebuiltDeleteChild->DesignerParent
				== rebuiltToolBarRuntime,
			L"subtree delta: undo after rebuild lost parent, identity, or document state"
			+ std::wstring(L" [delete=")
			+ SelfTestFlag(deleteBeforeRebuild.HasChanges())
			+ L", rebuild=" + SelfTestFlag(rebuildTransaction.HasChanges())
			+ L", undoRebuild=" + SelfTestFlag(undoRebuild.HasChanges())
			+ L", parentRebuilt=" + SelfTestFlag(
				rebuiltToolBarWrapper
				&& rebuiltToolBarWrapper != originalToolBar
				&& rebuiltToolBarRuntime)
			+ L", undoDelete=" + SelfTestFlag(
				undoDeleteAfterRebuild.HasChanges())
			+ L", capture=" + SelfTestFlag(
				!rebuiltDeleteRestored.Nodes.empty())
			+ L", equal=" + SelfTestFlag(
				rebuiltDeleteRestored == rebuiltDeleteBaseline)
			+ L", childIdentity=" + SelfTestFlag(
				FindControl(rebuiltDeleteCanvas, rebuiltDeleteChild->Name)
					== rebuiltDeleteChild
				&& rebuiltDeleteChild->ControlInstance
					== rebuiltDeleteChildRuntime)
			+ L", parent=" + SelfTestFlag(
				rebuiltDeleteChild->DesignerParent == rebuiltToolBarRuntime)
			+ L", error=" + undoDeleteAfterRebuild.Error
			+ L", diff=" + DescribeDocumentDifference(
				rebuiltDeleteRestored, rebuiltDeleteBaseline) + L"]");
	}

	auto verifySubtreeSpecialParent = [&failures](
		UIClass parentType,
		const std::wstring& label)
	{
		DesignerCanvas specialCanvas(0, 0, 850, 660);
		specialCanvas.AdoptVisualChildToCanvasCore(
			parentType, POINT{ 280, 220 });
		const auto parent = specialCanvas.GetSelectedControl();
		POINT childDrop{ 330, 280 };
		if (parent && parent->ControlInstance)
		{
			childDrop = RoundedPoint(
				parent->ControlInstance->GetAbsoluteLocationDip().x
					- specialCanvas.GetAbsoluteLocationDip().x + 80,
				parent->ControlInstance->GetAbsoluteLocationDip().y
					- specialCanvas.GetAbsoluteLocationDip().y + 80
			);
		}
		specialCanvas.AdoptVisualChildToCanvasCore(
			UIClass::UI_Button, childDrop);
		const auto child = specialCanvas.GetSelectedControl();
		auto* const childRuntime = child
			? child->ControlInstance : nullptr;
		auto* const runtimeParent = childRuntime
			? childRuntime->GetVisualParent() : nullptr;
		auto* const designerParent = child
			? child->DesignerParent : nullptr;
		bool parentKindMatches = false;
		if (parent && parent->ControlInstance && childRuntime)
		{
			if (auto* tabs = dynamic_cast<TabControl*>(
				parent->ControlInstance))
				parentKindMatches = static_cast<int>(tabs->ItemCount()) > 0
					&& runtimeParent == tabs->GetItem(0)
					&& designerParent == runtimeParent;
		}
		const bool setup = parent && child && childRuntime
			&& runtimeParent && parentKindMatches;
		AppendFailure(failures, setup,
			L"subtree delta: " + label + L" setup failed");
		if (!setup) return;
		(void)specialCanvas.ResetDocumentHistoryAsSaved();
		specialCanvas.RestoreSelectionByNames(
			{ child->Name }, child->Name, false);
		const auto removed = specialCanvas.DeleteSelectedControl();
		const auto restored = specialCanvas.UndoCommand();
		AppendFailure(failures,
			removed.HasChanges() && restored.HasChanges()
			&& FindControl(specialCanvas, child->Name) == child
			&& child->ControlInstance == childRuntime
			&& childRuntime->GetVisualParent() == runtimeParent
			&& child->DesignerParent == designerParent,
			L"subtree delta: " + label
				+ L" locator did not restore identity and parent"
				+ L" [remove=" + SelfTestFlag(removed.HasChanges())
				+ L", undo=" + SelfTestFlag(restored.HasChanges())
				+ L", identity=" + SelfTestFlag(
					FindControl(specialCanvas, child->Name) == child
					&& child->ControlInstance == childRuntime)
				+ L", visualParent=" + SelfTestFlag(
					childRuntime->GetVisualParent() == runtimeParent)
				+ L", designerParent=" + SelfTestFlag(
					child->DesignerParent == designerParent)
				+ L", removeError=" + removed.Error
				+ L", undoError=" + restored.Error + L"]");
	};
	verifySubtreeSpecialParent(
		UIClass::UI_TabControl, L"TabItem");

	DesignerCanvas emptyCommandCanvas(0, 0, 800, 640);
	size_t emptyCommandEventCount = 0;
	DesignerCanvasCommandEventArgs lastEmptyCommandEvent;
	emptyCommandCanvas.OnCommandCompleted +=
		[&emptyCommandEventCount, &lastEmptyCommandEvent](
			const DesignerCanvasCommandEventArgs& args)
		{
			++emptyCommandEventCount;
			lastEmptyCommandEvent = args;
		};
	auto emptyUndoResult = emptyCommandCanvas.UndoCommand();
	AppendFailure(failures,
		IsUnchanged(emptyUndoResult)
		&& emptyCommandEventCount == 1
		&& lastEmptyCommandEvent.Operation == L"Undo"
		&& lastEmptyCommandEvent.Label.empty(),
		L"empty command history: undo state was not reported");
	auto emptyDeleteResult = emptyCommandCanvas.DeleteSelectedControl();
	AppendFailure(failures,
		IsUnchanged(emptyDeleteResult)
		&& emptyCommandEventCount == 2
		&& lastEmptyCommandEvent.Operation == L"DeleteSelection"
		&& !lastEmptyCommandEvent.Message.empty(),
		L"empty delete: unchanged result or message was not reported");
	auto rejectedAddResult = emptyCommandCanvas.AdoptVisualChildToCanvas(
		UIClass::UI_Button, POINT{ 0, 0 });
	AppendFailure(failures,
		rejectedAddResult.State
			== DesignerDocumentTransactionState::Rejected
		&& !rejectedAddResult.Error.empty()
		&& emptyCommandEventCount == 3
		&& lastEmptyCommandEvent.Operation == L"AdoptVisualChild"
		&& lastEmptyCommandEvent.Result.State
			== DesignerDocumentTransactionState::Rejected
		&& IsUnchanged(emptyCommandCanvas.UndoCommand()),
		L"rejected add: failure entered history or was not reported");

	TemporarySelfTestFiles lifecycleFiles;
	const auto lifecyclePath = CreateTemporarySelfTestFile();
	const auto invalidLifecyclePath = CreateTemporarySelfTestFile();
	const auto xamlLifecycleBase = CreateTemporarySelfTestFile();
	const auto xamlLifecyclePath = xamlLifecycleBase.empty()
		? std::wstring{} : xamlLifecycleBase + L".cui.xaml";
	const auto invalidXamlBase = CreateTemporarySelfTestFile();
	const auto invalidXamlPath = invalidXamlBase.empty()
		? std::wstring{} : invalidXamlBase + L".cui.xaml";
	lifecycleFiles.Paths = {
		lifecyclePath, invalidLifecyclePath,
		xamlLifecycleBase, xamlLifecyclePath,
		invalidXamlBase, invalidXamlPath };
	AppendFailure(failures,
		!lifecyclePath.empty() && !invalidLifecyclePath.empty()
		&& !xamlLifecyclePath.empty() && !invalidXamlPath.empty(),
		L"document lifecycle: temporary files unavailable");

	DesignerCanvas lifecycleCanvas(0, 0, 800, 640);
	size_t documentStateEventCount = 0;
	DesignerCanvasDocumentStateEventArgs lastDocumentState;
	lifecycleCanvas.OnDocumentStateChanged +=
		[&documentStateEventCount, &lastDocumentState](
			const DesignerCanvasDocumentStateEventArgs& args)
		{
			++documentStateEventCount;
			lastDocumentState = args;
		};
	AppendFailure(failures,
		!lifecycleCanvas.IsDocumentDirty()
		&& lifecycleCanvas.GetCurrentDocumentStateId()
			== lifecycleCanvas.GetSavedDocumentStateId(),
		L"document lifecycle: initial canvas was not clean");
	auto lifecycleAdd = lifecycleCanvas.AdoptVisualChildToCanvas(
		UIClass::UI_Button, POINT{ 120, 120 });
	AppendFailure(failures,
		lifecycleAdd.HasChanges()
		&& lifecycleCanvas.IsDocumentDirty()
		&& documentStateEventCount > 0
		&& lastDocumentState.IsDirty,
		L"document lifecycle: committed edit did not become dirty");

	if (!lifecyclePath.empty())
	{
		std::wstring lifecycleError;
		auto firstSave = lifecycleCanvas.SaveDesignFile(
			lifecyclePath, &lifecycleError);
		const auto savedStateId =
			lifecycleCanvas.GetSavedDocumentStateId();
		AppendFailure(failures,
			firstSave.State == DesignerDocumentTransactionState::Unchanged
			&& !lifecycleCanvas.IsDocumentDirty()
			&& lifecycleCanvas.GetCurrentDocumentStateId() == savedStateId
			&& lifecycleCanvas.GetLastCommandOperation() == L"SaveDocument"
			&& !lastDocumentState.IsDirty,
			L"document lifecycle: successful save did not establish save point");
		AppendFailure(failures,
			lifecycleCanvas.UndoCommand().HasChanges()
			&& lifecycleCanvas.IsDocumentDirty(),
			L"document lifecycle: undo across save point was not dirty");
		AppendFailure(failures,
			lifecycleCanvas.RedoCommand().HasChanges()
			&& !lifecycleCanvas.IsDocumentDirty()
			&& lifecycleCanvas.GetCurrentDocumentStateId() == savedStateId,
			L"document lifecycle: redo to save point was not clean");
		AppendFailure(failures,
			lifecycleCanvas.UndoCommand().HasChanges(),
			L"document lifecycle: branch setup undo unavailable");
		auto branchEdit = lifecycleCanvas.AdoptVisualChildToCanvas(
			UIClass::UI_Label, POINT{ 180, 180 });
		const auto branchRedo = lifecycleCanvas.RedoCommand();
		AppendFailure(failures,
			branchEdit.HasChanges()
			&& lifecycleCanvas.IsDocumentDirty()
			&& lifecycleCanvas.GetCurrentDocumentStateId() != savedStateId
			&& IsUnchanged(branchRedo),
			L"document lifecycle: branched history matched stale save point"
				+ std::wstring(L" [edit=") + branchEdit.Error
				+ L", dirty=" + SelfTestFlag(lifecycleCanvas.IsDocumentDirty())
				+ L", current=" + std::to_wstring(
					lifecycleCanvas.GetCurrentDocumentStateId())
				+ L", saved=" + std::to_wstring(savedStateId)
				+ L", redoState=" + std::to_wstring(
					static_cast<int>(branchRedo.State))
				+ L", redoError=" + branchRedo.Error + L"]");

		auto branchSave = lifecycleCanvas.SaveDesignFile(
			lifecyclePath, &lifecycleError);
		DesignerModel::DesignDocument cleanBranchDocument;
		std::wstring cleanBranchError;
		AppendFailure(failures,
			branchSave.State == DesignerDocumentTransactionState::Unchanged
			&& !lifecycleCanvas.IsDocumentDirty()
			&& lifecycleCanvas.BuildDesignDocument(
				cleanBranchDocument, &cleanBranchError),
			L"document lifecycle: branch save failed");

		if (!xamlLifecyclePath.empty())
		{
			std::wstring xamlSaveError;
			auto xamlSave = lifecycleCanvas.SaveDesignFile(
				xamlLifecyclePath, &xamlSaveError);
			DesignerModel::DesignDocument persistedXamlDocument;
			std::wstring persistedXamlError;
			DesignerCanvas xamlOpenCanvas(0, 0, 800, 640);
			std::wstring xamlOpenError;
			auto xamlOpen = xamlOpenCanvas.LoadDesignFile(
				xamlLifecyclePath, &xamlOpenError);
			DesignerModel::DesignDocument openedXamlDocument;
			std::wstring openedXamlError;
			const bool persistedXamlLoaded =
				DesignerModel::XamlDocumentParser::LoadFromFile(
					xamlLifecyclePath, persistedXamlDocument,
					&persistedXamlError);
			const bool persistedXamlEquivalent = persistedXamlLoaded
				&& EquivalentXamlContent(
					persistedXamlDocument, cleanBranchDocument);
			const bool openedXamlBuilt = xamlOpenCanvas.BuildDesignDocument(
				openedXamlDocument, &openedXamlError);
			const bool openedXamlEquivalent = openedXamlBuilt
				&& EquivalentXamlContent(
					openedXamlDocument, cleanBranchDocument);
			const bool noXamlTemporaryFile =
				!HasAtomicSaveTemporaryFile(xamlLifecyclePath);
			AppendFailure(failures,
				xamlSave.State == DesignerDocumentTransactionState::Unchanged
				&& !lifecycleCanvas.IsDocumentDirty()
				&& persistedXamlEquivalent
				&& xamlOpen.Succeeded()
				&& !xamlOpenCanvas.IsDocumentDirty()
				&& openedXamlEquivalent
				&& noXamlTemporaryFile,
				L"document lifecycle: XAML Save As/open did not preserve content or clean state"
				+ std::wstring(L" [save=") + xamlSaveError
				+ L", open=" + xamlOpenError
				+ L", parse=" + persistedXamlError
				+ L", build=" + openedXamlError
				+ L", saveState="
				+ std::to_wstring(static_cast<int>(xamlSave.State))
				+ L", dirty=" + SelfTestFlag(lifecycleCanvas.IsDocumentDirty())
				+ L", persisted=" + SelfTestFlag(persistedXamlEquivalent)
				+ L", openState="
				+ std::to_wstring(static_cast<int>(xamlOpen.State))
				+ L", openDirty=" + SelfTestFlag(xamlOpenCanvas.IsDocumentDirty())
				+ L", opened=" + SelfTestFlag(openedXamlEquivalent)
				+ L", persistedDiff=" + DescribeDocumentDifference(
					persistedXamlDocument, cleanBranchDocument)
				+ L", xamlDiff=" + DescribeXamlDifference(
					persistedXamlDocument, cleanBranchDocument)
				+ L", openedXamlDiff=" + DescribeXamlDifference(
					openedXamlDocument, cleanBranchDocument)
				+ L", temp=" + SelfTestFlag(!noXamlTemporaryFile) + L"]");

			if (!invalidXamlPath.empty())
			{
				std::wstring malformedWriteError;
				const bool malformedWritten = DesignerModel::AtomicFile::Write(
					invalidXamlPath,
					"<Window><Unknown /></Window>",
					&malformedWriteError);
				const auto beforeRejectedXaml = openedXamlDocument;
				std::wstring rejectedXamlError;
				auto rejectedXaml = xamlOpenCanvas.LoadDesignFile(
					invalidXamlPath, &rejectedXamlError);
				DesignerModel::DesignDocument afterRejectedXaml;
				std::wstring afterRejectedXamlError;
				const bool rejectedXamlBuilt = xamlOpenCanvas.BuildDesignDocument(
					afterRejectedXaml, &afterRejectedXamlError);
				const bool rejectedXamlPreserved = rejectedXamlBuilt
					&& EquivalentDocumentContent(
						afterRejectedXaml, beforeRejectedXaml);
				AppendFailure(failures,
					malformedWritten
					&& rejectedXaml.State
						== DesignerDocumentTransactionState::Failed
					&& !rejectedXamlError.empty()
					&& rejectedXamlPreserved
					&& !xamlOpenCanvas.IsDocumentDirty(),
					L"document lifecycle: rejected XAML replacement mutated the open document"
					+ std::wstring(L" [write=") + malformedWriteError
					+ L", load=" + rejectedXamlError
					+ L", build=" + afterRejectedXamlError
					+ L", state="
					+ std::to_wstring(static_cast<int>(rejectedXaml.State))
					+ L", preserved=" + SelfTestFlag(rejectedXamlPreserved)
					+ L", dirty=" + SelfTestFlag(xamlOpenCanvas.IsDocumentDirty())
					+ L"]");
			}
		}

		const auto lockedSaveSetupEdit = lifecycleCanvas.AdoptVisualChildToCanvas(
			UIClass::UI_Button, POINT{ 260, 220 });
		const auto dirtyBeforeLockedSave =
			lifecycleCanvas.GetCurrentDocumentStateId();
		const HANDLE lockedDesignFile = ::CreateFileW(
			lifecyclePath.c_str(), GENERIC_READ, 0, nullptr,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
		AppendFailure(failures,
			lockedDesignFile != INVALID_HANDLE_VALUE,
			L"document lifecycle: could not lock existing save file");
		if (lockedDesignFile != INVALID_HANDLE_VALUE)
		{
			std::wstring lockedSaveError;
			auto lockedSave = lifecycleCanvas.SaveDesignFile(
				lifecyclePath, &lockedSaveError);
			(void)::CloseHandle(lockedDesignFile);
			DesignerModel::DesignDocument persistedAfterLockedSave;
			std::wstring persistedError;
			const bool persistedAfterLockedSaveLoaded =
				DesignerModel::DesignDocumentSerializer::LoadFromFile(
					lifecyclePath, persistedAfterLockedSave,
					&persistedError);
			const bool lockedSavePreserved =
				persistedAfterLockedSaveLoaded
				&& EquivalentDocumentContent(
					persistedAfterLockedSave, cleanBranchDocument);
			const bool noLockedSaveTemporaryFile =
				!HasAtomicSaveTemporaryFile(lifecyclePath);
			AppendFailure(failures,
				lockedSave.State == DesignerDocumentTransactionState::Failed
				&& !lockedSaveError.empty()
				&& lifecycleCanvas.IsDocumentDirty()
				&& lifecycleCanvas.GetCurrentDocumentStateId()
					== dirtyBeforeLockedSave
				&& lockedSavePreserved
				&& noLockedSaveTemporaryFile,
				L"document lifecycle: failed atomic replacement damaged the old file or save point"
				+ std::wstring(L" [error=") + lockedSaveError
				+ L", parse=" + persistedError
				+ L", state="
				+ std::to_wstring(static_cast<int>(lockedSave.State))
				+ L", setupState=" + std::to_wstring(
					static_cast<int>(lockedSaveSetupEdit.State))
				+ L", setupError=" + lockedSaveSetupEdit.Error
				+ L", dirty=" + SelfTestFlag(lifecycleCanvas.IsDocumentDirty())
				+ L", stateId=" + SelfTestFlag(
					lifecycleCanvas.GetCurrentDocumentStateId()
						== dirtyBeforeLockedSave)
				+ L", content=" + SelfTestFlag(lockedSavePreserved)
				+ L", diff=" + DescribeDocumentDifference(
					persistedAfterLockedSave, cleanBranchDocument)
				+ L", temp=" + SelfTestFlag(!noLockedSaveTemporaryFile) + L"]");
		}
		DesignerModel::DesignDocument beforeInvalidLoad;
		std::wstring beforeInvalidError;
		const bool beforeInvalidCaptured =
			lifecycleCanvas.BuildDesignDocument(
				beforeInvalidLoad, &beforeInvalidError);
		const auto beforeInvalidStateId =
			lifecycleCanvas.GetCurrentDocumentStateId();
		const auto beforeInvalidPrimary = lifecycleCanvas.GetSelectedControl()
			? lifecycleCanvas.GetSelectedControl()->Name : std::wstring{};

		if (!invalidLifecyclePath.empty()
			&& !cleanBranchDocument.Nodes.empty())
		{
			auto invalidDocument = cleanBranchDocument;
			invalidDocument.Nodes.front().ParentRef = L"MissingParent";
			std::wstring invalidSaveError;
			const bool invalidSaved =
				DesignerModel::DesignDocumentSerializer::SaveToFile(
					invalidDocument, invalidLifecyclePath,
					&invalidSaveError);
			AppendFailure(failures, invalidSaved,
				L"document lifecycle: invalid apply probe file unavailable");
			if (invalidSaved)
			{
				std::wstring invalidLoadError;
				auto invalidLoad = lifecycleCanvas.LoadDesignFile(
					invalidLifecyclePath, &invalidLoadError);
				DesignerModel::DesignDocument afterInvalidLoad;
				std::wstring afterInvalidError;
				AppendFailure(failures,
					invalidLoad.State
						== DesignerDocumentTransactionState::Failed
					&& invalidLoad.DocumentRestored
					&& !invalidLoadError.empty()
					&& beforeInvalidCaptured
					&& lifecycleCanvas.BuildDesignDocument(
						afterInvalidLoad, &afterInvalidError)
					&& afterInvalidLoad == beforeInvalidLoad
					&& lifecycleCanvas.GetCurrentDocumentStateId()
						== beforeInvalidStateId
					&& lifecycleCanvas.IsDocumentDirty()
					&& lifecycleCanvas.GetSelectedControl()
					&& lifecycleCanvas.GetSelectedControl()->Name
						== beforeInvalidPrimary,
					L"document lifecycle: failed open did not restore document, selection, and dirty state"
						+ std::wstring(L" [loadState=")
						+ std::to_wstring(static_cast<int>(invalidLoad.State))
						+ L", restored=" + SelfTestFlag(invalidLoad.DocumentRestored)
						+ L", loadError=" + invalidLoadError
						+ L", buildError=" + afterInvalidError
						+ L", content=" + DescribeDocumentDifference(
							afterInvalidLoad, beforeInvalidLoad)
						+ L", state=" + SelfTestFlag(
							lifecycleCanvas.GetCurrentDocumentStateId()
								== beforeInvalidStateId)
						+ L", dirty=" + SelfTestFlag(
							lifecycleCanvas.IsDocumentDirty())
						+ L", selected=" + (lifecycleCanvas.GetSelectedControl()
							? lifecycleCanvas.GetSelectedControl()->Name
							: std::wstring(L"<none>")) + L"]");
			}
		}

		auto lifecycleBegin = lifecycleCanvas.BeginDocumentEditTransaction(
			L"SelfTest:LifecycleGuard");
		auto blockedSave = lifecycleCanvas.SaveDesignFile(
			lifecyclePath, &lifecycleError);
		auto blockedNew = lifecycleCanvas.CreateNewDocument();
		auto blockedOpen = lifecycleCanvas.LoadDesignFile(
			lifecyclePath, &lifecycleError);
		auto blockedRecovery = lifecycleCanvas.RestoreRecoveredDocument(
			cleanBranchDocument);
		auto lifecycleCancel =
			lifecycleCanvas.CancelDocumentEditTransaction();
		AppendFailure(failures,
			lifecycleBegin.State == DesignerDocumentTransactionState::Begun
			&& blockedSave.State
				== DesignerDocumentTransactionState::Rejected
			&& blockedNew.State
				== DesignerDocumentTransactionState::Rejected
			&& blockedOpen.State
				== DesignerDocumentTransactionState::Rejected
			&& blockedRecovery.State
				== DesignerDocumentTransactionState::Rejected
			&& lifecycleCancel.State
				== DesignerDocumentTransactionState::Canceled
			&& lifecycleCanvas.GetCurrentDocumentStateId()
				== beforeInvalidStateId,
			L"document lifecycle: active transaction did not reject replacement operations");

		auto successfulOpen = lifecycleCanvas.LoadDesignFile(
			lifecyclePath, &lifecycleError);
		DesignerModel::DesignDocument openedDocument;
		std::wstring openedError;
		AppendFailure(failures,
			successfulOpen.Succeeded()
			&& !lifecycleCanvas.IsDocumentDirty()
			&& lifecycleCanvas.GetCurrentDocumentStateId()
				== lifecycleCanvas.GetSavedDocumentStateId()
			&& lifecycleCanvas.BuildDesignDocument(
				openedDocument, &openedError)
			&& openedDocument == cleanBranchDocument
			&& IsUnchanged(lifecycleCanvas.UndoCommand()),
			L"document lifecycle: successful open did not reset clean history");

		auto recoveredDocument = lifecycleCanvas.RestoreRecoveredDocument(
			cleanBranchDocument);
		DesignerModel::DesignDocument recoveredDocumentModel;
		std::wstring recoveredDocumentError;
		AppendFailure(failures,
			recoveredDocument.Succeeded()
			&& lifecycleCanvas.IsDocumentDirty()
			&& lifecycleCanvas.GetCurrentDocumentStateId()
				!= lifecycleCanvas.GetSavedDocumentStateId()
			&& lifecycleCanvas.BuildDesignDocument(
				recoveredDocumentModel, &recoveredDocumentError)
			&& recoveredDocumentModel == cleanBranchDocument
			&& IsUnchanged(lifecycleCanvas.UndoCommand())
			&& lifecycleCanvas.GetLastCommandOperation() == L"Undo",
			L"document lifecycle: recovered document was not an undo-free dirty baseline");

		(void)lifecycleCanvas.AdoptVisualChildToCanvas(
			UIClass::UI_Button, POINT{ 300, 260 });
		const auto dirtyBeforeFailedSave =
			lifecycleCanvas.GetCurrentDocumentStateId();
		wchar_t tempDirectory[MAX_PATH]{};
		(void)::GetTempPathW(MAX_PATH, tempDirectory);
		std::wstring failedSaveError;
		auto failedSave = lifecycleCanvas.SaveDesignFile(
			tempDirectory, &failedSaveError);
		AppendFailure(failures,
			failedSave.State == DesignerDocumentTransactionState::Failed
			&& !failedSaveError.empty()
			&& lifecycleCanvas.IsDocumentDirty()
			&& lifecycleCanvas.GetCurrentDocumentStateId()
				== dirtyBeforeFailedSave,
			L"document lifecycle: failed save cleared dirty state");

		auto newDocument = lifecycleCanvas.CreateNewDocument();
		DesignerModel::DesignDocument newDocumentModel;
		std::wstring newDocumentError;
		AppendFailure(failures,
			newDocument.Succeeded()
			&& !lifecycleCanvas.IsDocumentDirty()
			&& lifecycleCanvas.GetAllControls().size() == 1
			&& lifecycleCanvas.BuildDesignDocument(
				newDocumentModel, &newDocumentError)
			&& newDocumentModel.Nodes.size() == 1
			&& newDocumentModel.Nodes.front().Name == L"contentRoot"
			&& newDocumentModel.Nodes.front().ParentId == 0
			&& newDocumentModel.Nodes.front().ParentRef.empty()
			&& newDocumentModel.Window.Name == L"MainWindow"
			&& ReadNodeString(newDocumentModel.Window, L"Title") == L"Window"
			&& IsUnchanged(lifecycleCanvas.UndoCommand()),
			L"document lifecycle: new document did not restore defaults and clean history"
				+ std::wstring(L" [state=")
				+ std::to_wstring(static_cast<int>(newDocument.State))
				+ L", error=" + newDocument.Error
				+ L", build=" + newDocumentError
				+ L", controls=" + std::to_wstring(
					lifecycleCanvas.GetAllControls().size())
				+ L", nodes=" + std::to_wstring(newDocumentModel.Nodes.size())
				+ L"]");
	}

	DesignerCanvas failedRestoreCanvas(0, 0, 800, 640);
	size_t failedCommandEventCount = 0;
	DesignerCanvasCommandEventArgs lastFailedCommandEvent;
	failedRestoreCanvas.OnCommandCompleted +=
		[&failedCommandEventCount, &lastFailedCommandEvent](
			const DesignerCanvasCommandEventArgs& args)
		{
			++failedCommandEventCount;
			lastFailedCommandEvent = args;
		};
	failedRestoreCanvas.AdoptVisualChildToCanvasCore(
		UIClass::UI_Button, POINT{ 120, 120 });
	DesignerModel::DesignDocument validDocument;
	std::wstring captureError;
	const bool captured = failedRestoreCanvas.BuildDesignDocument(
		validDocument, &captureError);
	const auto validProbeControl = failedRestoreCanvas.GetSelectedControl();
	AppendFailure(failures, captured && validDocument.Nodes.size() == 2
		&& validProbeControl,
		L"failed undo: could not capture valid setup document");
	if (captured && validDocument.Nodes.size() == 2 && validProbeControl)
	{
		auto invalidDocument = validDocument;
		invalidDocument.Nodes.push_back(validDocument.Nodes.front());
		const auto controlName = validProbeControl->Name;
		auto command = std::make_unique<DocumentSnapshotCommand>(
			&failedRestoreCanvas,
			std::move(invalidDocument),
			validDocument,
			std::vector<std::wstring>{ controlName },
			std::vector<std::wstring>{ controlName },
			controlName,
			controlName,
			L"InvalidUndoProbe",
			true);
		auto probeResult =
			failedRestoreCanvas.ExecuteCommand(std::move(command));
		AppendFailure(failures,
			probeResult.HasChanges()
			&& failedCommandEventCount == 1
			&& lastFailedCommandEvent.Operation == L"ExecuteCommand"
			&& lastFailedCommandEvent.Label == L"InvalidUndoProbe",
			L"failed undo: probe command did not enter history");
		auto failedUndoResult = failedRestoreCanvas.UndoCommand();
		AppendFailure(failures,
			failedUndoResult.State
				== DesignerDocumentTransactionState::Failed
			&& failedUndoResult.DocumentRestored
			&& !failedUndoResult.Error.empty()
			&& failedCommandEventCount == 2
			&& lastFailedCommandEvent.Operation == L"Undo"
			&& lastFailedCommandEvent.Label == L"InvalidUndoProbe"
			&& lastFailedCommandEvent.Result.DocumentRestored,
			L"failed undo: failure details were not retained and published");
		AppendFailure(failures,
			failedRestoreCanvas.GetAllControls().size() == 2
				&& FindControl(failedRestoreCanvas, controlName)
				&& failedRestoreCanvas.GetSelectedControl()
				&& failedRestoreCanvas.GetSelectedControl()->Name == controlName,
			L"failed undo: current document or selection was not rolled back");
		auto redoAfterFailedUndo = failedRestoreCanvas.RedoCommand();
		AppendFailure(failures,
			IsUnchanged(redoAfterFailedUndo)
			&& failedCommandEventCount == 3
			&& lastFailedCommandEvent.Operation == L"Redo"
			&& lastFailedCommandEvent.Label.empty(),
			L"failed undo: command was incorrectly moved to redo history");
		auto repeatedFailedUndo = failedRestoreCanvas.UndoCommand();
		AppendFailure(failures,
			repeatedFailedUndo.State
				== DesignerDocumentTransactionState::Failed
			&& repeatedFailedUndo.DocumentRestored
			&& failedCommandEventCount == 4
			&& lastFailedCommandEvent.Operation == L"Undo"
			&& lastFailedCommandEvent.Label == L"InvalidUndoProbe",
			L"failed undo: failed command was not retained on undo history");
		AppendFailure(failures,
			failedRestoreCanvas.GetAllControls().size() == 2
				&& FindControl(failedRestoreCanvas, controlName),
			L"failed undo: repeated failure did not preserve current document");
	}

	{
		namespace fs = std::filesystem;
		const auto freshnessRoot = fs::temp_directory_path()
			/ (L"cui-designer-freshness-"
				+ std::to_wstring(::GetCurrentProcessId()) + L"-"
				+ std::to_wstring(::GetTickCount64()));
		const auto freshnessBase = freshnessRoot / L"FreshDesignerWindow";
		auto readText = [](const fs::path& path)
		{
			std::ifstream stream(path, std::ios::binary);
			return std::string(
				std::istreambuf_iterator<char>(stream),
				std::istreambuf_iterator<char>());
		};

		Designer freshnessDesigner;
		freshnessDesigner.InitializeComponents();
		const auto* duplicateArrangeItem = freshnessDesigner._arrangeMenu
			? freshnessDesigner._arrangeMenu->FindItemByText(L"重复") : nullptr;
		const auto* layerArrangeItem = freshnessDesigner._arrangeMenu
			? freshnessDesigner._arrangeMenu->FindItemByText(L"层级") : nullptr;
		const auto* lockArrangeItem = freshnessDesigner._arrangeMenu
			? freshnessDesigner._arrangeMenu->FindItemByText(
				L"锁定控件", false) : nullptr;
		const auto* frontArrangeItem = freshnessDesigner._arrangeMenu
			? freshnessDesigner._arrangeMenu->FindItemByText(L"置于顶层") : nullptr;
		const bool arrangeUiReady = freshnessDesigner._btnArrange
			&& !freshnessDesigner._btnArrange->IsEnabled
			&& freshnessDesigner._arrangeMenu
			&& freshnessDesigner._arrangeMenu->ItemCount() == 7
			&& duplicateArrangeItem
			&& duplicateArrangeItem->InputGestureText == L"Ctrl+D"
			&& lockArrangeItem
			&& lockArrangeItem->InputGestureText == L"Ctrl+L"
			&& layerArrangeItem && layerArrangeItem->ItemCount() == 4
			&& frontArrangeItem
			&& frontArrangeItem->InputGestureText == L"Ctrl+Shift+]";
		DesignerModel::DesignCodeBehindModel association;
		association.ClassName = L"Acme::FreshDesignerWindow";
		std::wstring freshnessError;
		const bool associated = freshnessDesigner._canvas
			&& freshnessDesigner._canvas->SetCodeBehind(
				association, &freshnessError);
		freshnessDesigner._lastExportBasePath = freshnessBase.wstring();
		const bool initiallyGenerated = associated
			&& freshnessDesigner.GenerateCodeFiles(
				freshnessBase.wstring(), &freshnessError);
		const bool initiallyCurrent = initiallyGenerated
			&& freshnessDesigner._codeFreshness.State
				== DesignerModel::DesignCodeFreshnessState::Current
			&& freshnessDesigner._btnRegenerate
			&& freshnessDesigner._btnRegenerate->IsEnabled
			&& ReadControlStringProperty(
				freshnessDesigner._btnRegenerate, L"Content") == L"重新生成";

		auto eventEdit = freshnessDesigner._canvas
			? freshnessDesigner._canvas->UpdateEventHandler(
				nullptr, L"ContentRendered", L"HandleContentRendered", &freshnessError)
			: DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"missing freshness canvas");
		const bool eventMarkedStale = eventEdit.HasChanges()
			&& freshnessDesigner._codeFreshness.State
				== DesignerModel::DesignCodeFreshnessState::Stale
			&& ReadControlStringProperty(
				freshnessDesigner._btnRegenerate, L"Content") == L"重新生成 *";
		auto freshnessUndo = freshnessDesigner._canvas
			? freshnessDesigner._canvas->UndoCommand()
			: DesignerDocumentTransactionResult{};
		const bool undoRestoredCurrent = freshnessUndo.HasChanges()
			&& freshnessDesigner._codeFreshness.State
				== DesignerModel::DesignCodeFreshnessState::Current
			&& ReadControlStringProperty(
				freshnessDesigner._btnRegenerate, L"Content") == L"重新生成";
		auto freshnessRedo = freshnessDesigner._canvas
			? freshnessDesigner._canvas->RedoCommand()
			: DesignerDocumentTransactionResult{};
		const bool redoRestoredStale = freshnessRedo.HasChanges()
			&& freshnessDesigner._codeFreshness.State
				== DesignerModel::DesignCodeFreshnessState::Stale;
		const bool regenerated = freshnessDesigner.GenerateCodeFiles(
			freshnessBase.wstring(), &freshnessError);

		const auto generatedHeader = fs::path(
			freshnessBase.wstring() + L".g.h");
		auto driftedHeader = readText(generatedHeader);
		driftedHeader += "\n// EXTERNAL_DRIFT\n";
		const bool driftWritten = DesignerModel::AtomicFile::Write(
			generatedHeader.wstring(), driftedHeader, &freshnessError);
		freshnessDesigner.RefreshCodeFreshnessFromFiles();
		freshnessDesigner.UpdateDocumentPresentation();
		const bool externalDriftDetected = driftWritten
			&& freshnessDesigner._codeFreshness.State
				== DesignerModel::DesignCodeFreshnessState::Stale
			&& ReadControlStringProperty(
				freshnessDesigner._btnRegenerate, L"Content") == L"重新生成 *";

		const bool repairedDrift = freshnessDesigner.GenerateCodeFiles(
			freshnessBase.wstring(), &freshnessError);
		const auto generatedSource = fs::path(
			freshnessBase.wstring() + L".g.cpp");
		std::error_code removeError;
		const bool sourceRemoved = fs::remove(generatedSource, removeError);
		freshnessDesigner.RefreshCodeFreshnessFromFiles();
		freshnessDesigner.UpdateDocumentPresentation();
		const bool missingDetected = sourceRemoved && !removeError
			&& freshnessDesigner._codeFreshness.State
				== DesignerModel::DesignCodeFreshnessState::Missing
			&& freshnessDesigner._codeFreshness.MissingFiles.size() == 1
			&& ReadControlStringProperty(
				freshnessDesigner._btnRegenerate, L"Content") == L"重新生成 !";

		const bool repairedMissing = freshnessDesigner.GenerateCodeFiles(
			freshnessBase.wstring(), &freshnessError);
		const auto userHeaderPath = fs::path(
			freshnessBase.wstring() + L".h");
		const auto validUserHeader = readText(userHeaderPath);
		auto wrongUserHeader = validUserHeader;
		const auto classMarker = wrongUserHeader.find(
			"Acme::FreshDesignerWindow");
		if (classMarker != std::string::npos)
			wrongUserHeader.replace(
				classMarker,
				std::string("Acme::FreshDesignerWindow").size(),
				"Other::FreshDesignerWindow");
		const bool wrongHeaderWritten = classMarker != std::string::npos
			&& DesignerModel::AtomicFile::Write(
				userHeaderPath.wstring(), wrongUserHeader, &freshnessError);
		freshnessDesigner.RefreshCodeFreshnessFromFiles();
		freshnessDesigner.UpdateDocumentPresentation();
		const bool blockedDetected = wrongHeaderWritten
			&& freshnessDesigner._codeFreshness.State
				== DesignerModel::DesignCodeFreshnessState::Blocked
			&& ReadControlStringProperty(
				freshnessDesigner._btnRegenerate, L"Content") == L"生成受阻 !"
			&& !freshnessDesigner._btnRegenerate->AutomationFullDescription.empty();
		const bool validHeaderRestored = DesignerModel::AtomicFile::Write(
			userHeaderPath.wstring(), validUserHeader, &freshnessError);
		freshnessDesigner.RefreshCodeFreshnessFromFiles();
		freshnessDesigner.UpdateDocumentPresentation();
		const bool restoredCurrent = validHeaderRestored
			&& freshnessDesigner._codeFreshness.State
				== DesignerModel::DesignCodeFreshnessState::Current;

		AppendFailure(failures,
			arrangeUiReady && initiallyCurrent && eventMarkedStale
			&& undoRestoredCurrent && redoRestoredStale
			&& regenerated && externalDriftDetected && repairedDrift
			&& missingDetected && repairedMissing
			&& blockedDetected && restoredCurrent,
			L"designer toolbar/code freshness: arrange menu, Undo/Redo, drift, missing, or blocked detection failed");
	if (::GetEnvironmentVariableW(
			L"CUI_KEEP_CODEGEN_TEST_OUTPUT", nullptr, 0) == 0)
			fs::remove_all(freshnessRoot, removeError);
	}

	// The runtime gallery is the public XAML conformance fixture. The Designer
	// must consume the same complete built-in surface; unsupported native or
	// custom implementations are represented by design-safe proxies rather than
	// rejecting the document.
	{
		namespace fs = std::filesystem;
		const auto demoPath = fs::current_path()
			/ L"CUITest" / L"DemoWindow.cui.xaml";
		DesignerModel::DesignDocument demoDocument;
		std::wstring demoParseError;
		const bool demoParsed = DesignerModel::XamlDocumentParser::LoadFromFile(
			demoPath.wstring(), demoDocument, &demoParseError);
		DesignerCanvas demoCanvas(0, 0, 1440, 900);
		std::wstring demoApplyError;
		const bool demoApplied = demoParsed
			&& demoCanvas.ApplyDesignDocument(demoDocument, &demoApplyError);
		auto demoStatus = demoApplied
			? FindControl(demoCanvas, L"mainStatusBar") : nullptr;
		const auto declaredStatusWidth = demoStatus
			&& demoStatus->ControlInstance
			? demoStatus->ControlInstance->Width : cui::layout::Length::Auto();
		const auto declaredStatusHeight = demoStatus
			&& demoStatus->ControlInstance
			? demoStatus->ControlInstance->Height : cui::layout::Length::Auto();
		if (demoApplied)
			cui::framework::PresentationAccess::Prepare(demoCanvas);
		auto demoRoot = demoApplied
			? FindControl(demoCanvas, L"windowContent") : nullptr;
		const auto demoRootSize = demoRoot && demoRoot->ControlInstance
			? demoRoot->ControlInstance->GetActualSizeDip()
			: cui::core::Size{};
		const bool previewRootFillsWindow = demoRoot
			&& std::fabs(demoRootSize.width
				- demoCanvas.GetDesignedWindowSize().width) < 0.01f;
		bool statusPreviewMatchesRuntime = false;
		if (demoStatus && demoStatus->ControlInstance
			&& demoStatus->ControlInstance->GetVisualParent())
		{
			auto* statusParent = dynamic_cast<Grid*>(
				demoStatus->ControlInstance->GetVisualParent());
			const auto actual = demoStatus->ControlInstance
				->GetActualLocationDip();
			const auto actualSize = demoStatus->ControlInstance
				->GetActualSizeDip();
			const auto parentSize = statusParent
				? statusParent->GetActualSizeDip() : cui::core::Size{};
			statusPreviewMatchesRuntime =
				statusParent
				&& Grid::GetRow(*(demoStatus->ControlInstance)) == 4
				&& actual.x >= -0.01f && actual.y >= -0.01f
				&& actualSize.width > 0.0f && actualSize.height > 0.0f
				&& actual.x + actualSize.width
					<= parentSize.width + 0.01f
				&& actual.y + actualSize.height
					<= parentSize.height + 0.01f
				&& demoStatus->ControlInstance->Width == declaredStatusWidth
				&& demoStatus->ControlInstance->Height == declaredStatusHeight;
		}
		DesignerModel::DesignDocument recapturedDemo;
		std::wstring demoCaptureError;
		const bool demoRecaptured = demoApplied
			&& demoCanvas.BuildDesignDocument(
				recapturedDemo, &demoCaptureError);
		std::wstring demoCodeGenError;
		const bool staticCodeGenerationReady = demoRecaptured
			&& CodeGenerator::ValidateDocument(
				recapturedDemo, &demoCodeGenError);
		std::string compactDemo;
		bool demoCompact = false;
		if (demoRecaptured)
		{
			try
			{
				compactDemo = DesignerModel::XamlDocumentSerializer::ToXaml(
					recapturedDemo);
				demoCompact = compactDemo.find("d:ProjectedProperties")
					== std::string::npos
					&& compactDemo.find("d:DesignProps") == std::string::npos
					&& compactDemo.find("d:DesignBindings") == std::string::npos
					&& compactDemo.find("d:DesignExtra") == std::string::npos
					&& compactDemo.find("<PathGeometry")
						!= std::string::npos
					&& compactDemo.find("x:Key=\"GradientLabelClip\"")
						!= std::string::npos
					&& compactDemo.find("<Geometry.Transform>")
						!= std::string::npos
					&& compactDemo.find("<ArcSegment")
						!= std::string::npos
					&& compactDemo.find("x:Key=\"GradientLabelTransform\"")
						!= std::string::npos
					&& compactDemo.find("Property=\"Clip\"") != std::string::npos
					&& compactDemo.find("Property=\"RenderTransform\"")
						!= std::string::npos
					&& compactDemo.find("RenderTransformOrigin=\"0.5, 0.5\"")
						!= std::string::npos
					&& compactDemo.find("x:Name=\"sideNavigationList\"")
						!= std::string::npos
					&& compactDemo.find("x:Key=\"AnalyticsRows\"")
						!= std::string::npos
					&& compactDemo.find("x:Key=\"AnalyticsRowTemplate\"")
						!= std::string::npos
					&& compactDemo.find("<ChartView.Series>") != std::string::npos
					&& compactDemo.find("x:Name=\"analyticsRows\"")
						!= std::string::npos
					&& compactDemo.find("x:Name=\"notificationPanel\"")
						!= std::string::npos
					&& compactDemo.find("<BitmapImage") != std::string::npos
					&& compactDemo.find(
						"UriSource=\"Assets/nav-overview.svg\"") != std::string::npos
					&& compactDemo.find("<LinearGradientBrush") != std::string::npos
					&& compactDemo.find(
						"<ResourceDictionary.MergedDictionaries>")
						!= std::string::npos
					&& compactDemo.find(
						"Source=\"Assets/DemoTheme.xaml\"")
						!= std::string::npos;
			}
			catch (const std::exception& exception)
			{
				demoCaptureError = Convert::Utf8ToUnicode(exception.what());
			}
		}
		std::wstring demoCompactDiagnostic;
		const auto appendCompactDiagnostic =
			[&](const std::string& diagnostic)
			{
				if (!demoCompactDiagnostic.empty())
					demoCompactDiagnostic += L", ";
				demoCompactDiagnostic += Convert::Utf8ToUnicode(diagnostic);
			};
		for (const char* forbidden : {
			"d:ProjectedProperties", "d:DesignProps", "d:DesignBindings",
			"d:DesignExtra" })
		{
			if (compactDemo.find(forbidden) != std::string::npos)
				appendCompactDiagnostic(std::string("unexpected ") + forbidden);
		}
		for (const char* required : {
			"<PathGeometry", "x:Key=\"GradientLabelClip\"",
			"<Geometry.Transform>", "<ArcSegment",
			"x:Key=\"GradientLabelTransform\"", "Property=\"Clip\"",
			"Property=\"RenderTransform\"", "RenderTransformOrigin=\"0.5, 0.5\"",
			"x:Name=\"sideNavigationList\"", "x:Key=\"AnalyticsRows\"",
			"x:Key=\"AnalyticsRowTemplate\"", "<ChartView.Series>",
			"x:Name=\"analyticsRows\"", "x:Name=\"notificationPanel\"", "<BitmapImage",
			"UriSource=\"Assets/nav-overview.svg\"", "<LinearGradientBrush",
			"<ResourceDictionary.MergedDictionaries>",
			"Source=\"Assets/DemoTheme.xaml\"" })
		{
			if (compactDemo.find(required) == std::string::npos)
				appendCompactDiagnostic(std::string("missing ") + required);
		}
		const auto hasType = [&](UIClass type)
		{
			return std::any_of(
				demoDocument.Nodes.begin(), demoDocument.Nodes.end(),
				[type](const auto& node) { return node.Type == type; });
		};
		bool composedDataMaterialized = false;
		bool objectResourcesMaterialized = false;
		bool imageResourceMaterialized = false;
		bool gradientBrushResourceMaterialized = false;
		bool imageBrushResourceMaterialized = false;
		bool drawingResourcesMaterialized = false;
		if (demoApplied)
		{
			auto navigationWrapper = FindControl(demoCanvas, L"sideNavigationList");
			auto analyticsRowsWrapper = FindControl(demoCanvas, L"analyticsRows");
			auto analyticsReportWrapper = FindControl(demoCanvas, L"analyticsReport");
			auto chartWrapper = FindControl(demoCanvas, L"salesChart");
			auto titleWrapper = FindControl(demoCanvas, L"basicTitle");
			auto badgeWrapper = FindControl(demoCanvas, L"runtimeBadge");
			auto imageWrapper = FindControl(demoCanvas, L"demoImage");
			auto gradientWrapper = FindControl(demoCanvas, L"gradientLabel");
			auto* navigation = navigationWrapper
				? dynamic_cast<ListBox*>(navigationWrapper->ControlInstance) : nullptr;
			auto* analyticsRows = analyticsRowsWrapper
				? dynamic_cast<ListView*>(analyticsRowsWrapper->ControlInstance) : nullptr;
			auto* chart = chartWrapper
				? dynamic_cast<ChartView*>(chartWrapper->ControlInstance) : nullptr;
			auto* title = titleWrapper ? titleWrapper->ControlInstance : nullptr;
			auto* badge = badgeWrapper ? badgeWrapper->ControlInstance : nullptr;
			auto* image = imageWrapper
				? dynamic_cast<Image*>(imageWrapper->ControlInstance) : nullptr;
			auto* gradient = gradientWrapper
				? gradientWrapper->ControlInstance : nullptr;
			composedDataMaterialized = navigation
				&& navigation->GetItemsSource()
				&& navigation->SelectedIndex == 1
				&& analyticsRows && analyticsRows->GetItemsSource()
				&& analyticsReportWrapper
				&& analyticsReportWrapper->ControlInstance
				&& analyticsReportWrapper->ControlInstance->Type()
					== UIClass::UI_GroupBox
				&& chart && chart->Title == L"成交趋势"
				&& chart->GetSeries().size() == 3
				&& chart->GetSeries()[0].Points.size() == 8;
			const auto& titleBrush = title
				? title->GetForegroundBrush()
				: std::optional<cui::drawing::Brush>{};
			const auto& badgeBrush = badge
				? badge->GetForegroundBrush()
				: std::optional<cui::drawing::Brush>{};
			imageResourceMaterialized = image && image->Source
				&& image->Source->GetSourceUri() == L"Assets/nav-overview.svg"
				;
			gradientBrushResourceMaterialized = titleBrush
				&& titleBrush->Kind == cui::drawing::BrushKind::LinearGradient
				&& titleBrush->GradientStops.size() == 2;
			imageBrushResourceMaterialized = badgeBrush
				&& badgeBrush->Kind == cui::drawing::BrushKind::Image
				&& badgeBrush->ImageSource
				&& badgeBrush->ImageSource->GetSourceUri()
					== L"Assets/nav-overview.svg"
				&& badgeBrush->Stretch
					== cui::drawing::ImageBrushStretch::UniformToFill
				&& std::fabs(badgeBrush->Opacity - 0.9f) < 0.01f;
			objectResourcesMaterialized = imageResourceMaterialized
				&& gradientBrushResourceMaterialized
				&& imageBrushResourceMaterialized;
			const auto& clip = gradient
				? gradient->GetClip()
				: std::optional<cui::drawing::Geometry>{};
			const auto& transform = gradient
				? gradient->GetRenderTransform()
				: std::optional<cui::drawing::Transform>{};
			drawingResourcesMaterialized = clip
				&& clip->Kind == cui::drawing::GeometryKind::Path
				&& clip->Figures.size() == 1
				&& clip->Figures[0].Segments.size() == 8
				&& clip->LocalTransform
				&& clip->LocalTransform->Operations.size() == 1
				&& transform && transform->Operations.size() == 2
				&& transform->Operations[0].Kind
					== cui::drawing::TransformKind::Rotate
				&& transform->Operations[1].Kind
					== cui::drawing::TransformKind::Scale
				&& gradient->GetPropertyValueSource(L"Clip")
					== DependencyPropertyValueSource::Style
				&& gradient->GetPropertyValueSource(L"RenderTransform")
					== DependencyPropertyValueSource::Style;
		}
		bool surfaceChildMoveStable = false;
		if (auto basicButton = demoApplied
			? FindControl(demoCanvas, L"basicButton") : nullptr;
			basicButton && basicButton->ControlInstance)
		{
			auto* control = basicButton->ControlInstance;
			const cui::core::Point beforeLocation{
				Canvas::GetLeft(*(control)), Canvas::GetTop(*(control)) };
			const auto beforeWidth = control->Width;
			const auto beforeHeight = control->Height;
			const auto beforeAbsolute = control->GetAbsoluteLocationDip();
			demoCanvas.RestoreSelectionByNames(
				{ basicButton->Name }, basicButton->Name, false);
			const auto moved = demoCanvas.NudgeSelectionBy(1, 1);
			const auto afterAbsolute = control->GetAbsoluteLocationDip();
			surfaceChildMoveStable = moved.HasChanges()
				&& Canvas::GetLeft(*(control)) == beforeLocation.x + 1.0f
				&& Canvas::GetTop(*(control)) == beforeLocation.y + 1.0f
				&& control->Width == beforeWidth
				&& control->Height == beforeHeight
				&& std::fabs(afterAbsolute.x - beforeAbsolute.x - 1.0f) < 0.01f
				&& std::fabs(afterAbsolute.y - beforeAbsolute.y - 1.0f) < 0.01f;
		}
		bool transformedChildMoveStable = false;
		if (auto gradientLabel = demoApplied
			? FindControl(demoCanvas, L"gradientLabel") : nullptr;
			gradientLabel && gradientLabel->ControlInstance)
		{
			auto* control = gradientLabel->ControlInstance;
			const cui::core::Point beforeLocation{
				Canvas::GetLeft(*(control)), Canvas::GetTop(*(control)) };
			const auto beforeWidth = control->Width;
			const auto beforeHeight = control->Height;
			demoCanvas.RestoreSelectionByNames(
				{ gradientLabel->Name }, gradientLabel->Name, false);
			const auto moved = demoCanvas.NudgeSelectionBy(1, 0);
			transformedChildMoveStable = moved.HasChanges()
				&& Canvas::GetLeft(*(control)) == beforeLocation.x + 1.0f
				&& Canvas::GetTop(*(control)) == beforeLocation.y
				&& control->Width == beforeWidth
				&& control->Height == beforeHeight;
		}
		std::wstring groupMoveDetail;
		std::wstring expanderMoveDetail;
		const auto nestedContainerChildMoveStable =
			[&](const std::wstring& name, std::wstring& detail)
			{
				auto wrapper = demoApplied
					? FindControl(demoCanvas, name) : nullptr;
				if (!wrapper || !wrapper->ControlInstance) return false;
				auto* control = wrapper->ControlInstance;
				const cui::core::Point beforeLocation{
					Canvas::GetLeft(*(control)), Canvas::GetTop(*(control)) };
				const auto beforeWidth = control->Width;
				const auto beforeHeight = control->Height;
				const auto beforeAbsolute = control->GetAbsoluteLocationDip();
				demoCanvas.RestoreSelectionByNames(
					{ wrapper->Name }, wrapper->Name, false);
				const auto moved = demoCanvas.NudgeSelectionBy(1, 1);
				const auto afterAbsolute = control->GetAbsoluteLocationDip();
				const bool stable = moved.HasChanges()
					&& Canvas::GetLeft(*(control)) == beforeLocation.x + 1.0f
					&& Canvas::GetTop(*(control)) == beforeLocation.y + 1.0f
					&& control->Width == beforeWidth
					&& control->Height == beforeHeight
					&& std::fabs(afterAbsolute.x
						- beforeAbsolute.x - 1.0f) < 0.01f
					&& std::fabs(afterAbsolute.y
						- beforeAbsolute.y - 1.0f) < 0.01f;
				if (!stable)
					detail = L"changed="
						+ std::wstring(moved.HasChanges() ? L"1" : L"0")
						+ L"; loc="
						+ std::to_wstring(beforeLocation.x) + L","
						+ std::to_wstring(beforeLocation.y) + L"->"
						+ std::to_wstring(Canvas::GetLeft(*(control))) + L","
						+ std::to_wstring(Canvas::GetTop(*(control))) + L"; size="
						+ std::to_wstring(beforeWidth.value) + L","
						+ std::to_wstring(beforeHeight.value) + L"->"
						+ std::to_wstring(control->Width.value) + L","
						+ std::to_wstring(control->Height.value) + L"; abs="
						+ std::to_wstring(beforeAbsolute.x) + L","
						+ std::to_wstring(beforeAbsolute.y) + L"->"
						+ std::to_wstring(afterAbsolute.x) + L","
						+ std::to_wstring(afterAbsolute.y);
				return stable;
			};
		const bool groupChildMoveStable =
			nestedContainerChildMoveStable(
				L"groupName", groupMoveDetail);
		const bool expanderChildMoveStable =
			nestedContainerChildMoveStable(
				L"expanderText", expanderMoveDetail);
		const bool specialContainerMovesStable =
			groupChildMoveStable && expanderChildMoveStable;
		const bool navigationCompositionMaterialized = demoApplied
			&& FindControl(demoCanvas, L"navigationComposition")
			&& FindControl(demoCanvas, L"detailComposition");
		const bool notificationCompositionMaterialized = demoApplied
			&& FindControl(demoCanvas, L"notificationPanel")
			&& FindControl(demoCanvas, L"toastMessage");
		const bool webBrowserMaterialized = hasType(UIClass::UI_WebBrowser);
		const bool mediaPlayerMaterialized = hasType(UIClass::UI_MediaPlayer);
		AppendFailure(failures,
			demoParsed && demoApplied && demoRecaptured && demoCompact
			&& staticCodeGenerationReady
			&& composedDataMaterialized
			&& objectResourcesMaterialized
			&& drawingResourcesMaterialized
			&& previewRootFillsWindow
			&& statusPreviewMatchesRuntime
			&& surfaceChildMoveStable
			&& transformedChildMoveStable
			&& specialContainerMovesStable
			&& navigationCompositionMaterialized
			&& notificationCompositionMaterialized
			&& webBrowserMaterialized
			&& mediaPlayerMaterialized,
			L"public XAML gallery: DemoWindow preview geometry, movement, or compact serialization regressed"
			+ std::wstring(L" [path=") + demoPath.wstring()
			+ L", parse=" + demoParseError
			+ L", apply=" + demoApplyError
			+ L", capture=" + demoCaptureError
			+ L", status=" + (statusPreviewMatchesRuntime ? L"1" : L"0")
			+ L", rootWidth=" + std::to_wstring(demoRootSize.width)
			+ L"/" + std::to_wstring(
				demoCanvas.GetDesignedWindowSize().width)
			+ L", surface=" + (surfaceChildMoveStable ? L"1" : L"0")
			+ L", transform=" + (transformedChildMoveStable ? L"1" : L"0")
			+ L", group=" + (groupChildMoveStable ? L"1" : L"0")
			+ L", groupDetail=" + groupMoveDetail
			+ L", expander=" + (expanderChildMoveStable ? L"1" : L"0")
			+ L", expanderDetail=" + expanderMoveDetail
			+ L", composed=" + (composedDataMaterialized ? L"1" : L"0")
			+ L", objects=" + (objectResourcesMaterialized ? L"1" : L"0")
			+ L", bitmap=" + SelfTestFlag(imageResourceMaterialized)
			+ L", gradientBrush="
				+ SelfTestFlag(gradientBrushResourceMaterialized)
			+ L", imageBrush="
				+ SelfTestFlag(imageBrushResourceMaterialized)
			+ L", drawing=" + (drawingResourcesMaterialized ? L"1" : L"0")
			+ L", navigationComposition="
				+ (navigationCompositionMaterialized ? L"1" : L"0")
			+ L", notificationComposition="
				+ (notificationCompositionMaterialized ? L"1" : L"0")
			+ L", web=" + (webBrowserMaterialized ? L"1" : L"0")
			+ L", media=" + (mediaPlayerMaterialized ? L"1" : L"0")
			+ L", compact=" + (demoCompact ? L"1" : L"0")
			+ L", compactDetail=" + demoCompactDiagnostic
			+ L", codegenReady=" + (staticCodeGenerationReady ? L"1" : L"0")
			+ L", codegen=" + demoCodeGenError
			+ L"]");
	}

	{
		const std::string elementBindingXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="ElementPreviewWindow">
	<Canvas x:Name="elementScope">
	  <TextBox x:Name="elementSource" Text="Designer source" />
	  <TextBlock x:Name="elementTarget"
	         Text="{Binding Text, ElementName=elementSource}" />
	</Canvas>
</Window>)XAML";
		DesignerModel::DesignDocument elementDocument;
		std::wstring elementError;
		const bool elementParsed = DesignerModel::XamlDocumentParser::FromXaml(
			elementBindingXaml, elementDocument, &elementError);
		DesignerCanvas elementCanvas(0, 0, 640, 480);
		const bool elementApplied = elementParsed
			&& elementCanvas.ApplyDesignDocument(elementDocument, &elementError);
		auto elementSource = elementApplied
			? FindControl(elementCanvas, L"elementSource") : nullptr;
		auto elementTarget = elementApplied
			? FindControl(elementCanvas, L"elementTarget") : nullptr;
		const auto previewState = elementTarget
			? elementTarget->BindingPreviewStates.find(L"Text")
			: std::map<std::wstring, DesignerBindingPreviewState>::const_iterator{};
		const bool elementPreviewInitial = elementSource && elementTarget
			&& ReadControlStringProperty(
				elementTarget->ControlInstance, L"Text") == L"Designer source"
			&& previewState != elementTarget->BindingPreviewStates.end()
			&& previewState->second.Status == DesignerBindingPreviewStatus::Active;
		if (elementSource)
			(void)WriteControlStringProperty(
				elementSource->ControlInstance, L"Text", L"Designer updated");
		const bool elementPreviewUpdated = elementTarget
			&& ReadControlStringProperty(
				elementTarget->ControlInstance, L"Text") == L"Designer updated";
		DesignerModel::DesignDocument elementCaptured;
		const bool elementSaved = elementCanvas.BuildDesignDocument(
			elementCaptured, &elementError);
		const auto elementCanonical = elementSaved
			? DesignerModel::XamlDocumentSerializer::ToXaml(elementCaptured)
			: std::string{};
		AppendFailure(failures,
			elementParsed && elementApplied && elementPreviewInitial
			&& elementPreviewUpdated && elementSaved
			&& elementCanonical.find("ElementName=elementSource")
				!= std::string::npos,
			L"ElementName binding did not preview or persist in Designer: "
				+ elementError
				+ L" [parsed=" + SelfTestFlag(elementParsed)
				+ L", applied=" + SelfTestFlag(elementApplied)
				+ L", initial=" + SelfTestFlag(elementPreviewInitial)
				+ L", updated=" + SelfTestFlag(elementPreviewUpdated)
				+ L", saved=" + SelfTestFlag(elementSaved) + L"]");
	}

	{
		const std::string inheritedBindingXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="InheritedPreviewWindow">
  <Window.DataContextSchema>
    <Property Path="Profile" Kind="Object" ObjectType="BindingSource" />
    <Property Path="Profile.Name" Kind="String" />
  </Window.DataContextSchema>
  <StackPanel x:Name="profileScope" DataContext="{Binding Profile}">
    <TextBlock x:Name="profileName" Text="{Binding Name}" />
    <TextBlock x:Name="selfPreview" Text="Self preview"
           AutomationProperties.Name="{Binding Text, RelativeSource={RelativeSource Self}}" />
  </StackPanel>
</Window>)XAML";
		DesignerModel::DesignDocument inheritedDocument;
		std::wstring inheritedError;
		const bool inheritedParsed = DesignerModel::XamlDocumentParser::FromXaml(
			inheritedBindingXaml, inheritedDocument, &inheritedError);
		DesignerCanvas inheritedCanvas(0, 0, 640, 480);
		const bool inheritedApplied = inheritedParsed
			&& inheritedCanvas.ApplyDesignDocument(
				inheritedDocument, &inheritedError);
		auto profile = std::make_shared<ObservableObject>();
		profile->SetValue(L"Name", std::wstring(L"Designer profile"));
		auto root = std::make_shared<ObservableObject>();
		root->SetValue(L"Profile", BindingSourceReference(profile));
		if (inheritedApplied) inheritedCanvas.SetDesignDataContext(root);
		auto inheritedName = inheritedApplied
			? FindControl(inheritedCanvas, L"profileName") : nullptr;
		auto selfPreview = inheritedApplied
			? FindControl(inheritedCanvas, L"selfPreview") : nullptr;
		const bool inheritedInitial = inheritedName && selfPreview
			&& ReadControlStringProperty(
				inheritedName->ControlInstance, L"Text") == L"Designer profile"
			&& selfPreview->ControlInstance->AutomationName == L"Self preview";
		auto replacement = std::make_shared<ObservableObject>();
		replacement->SetValue(L"Name", std::wstring(L"Replacement profile"));
		root->SetValue(L"Profile", BindingSourceReference(replacement));
		if (selfPreview)
			(void)WriteControlStringProperty(
				selfPreview->ControlInstance, L"Text", L"Changed self preview");
		const bool inheritedUpdated = inheritedName && selfPreview
			&& ReadControlStringProperty(
				inheritedName->ControlInstance, L"Text") == L"Replacement profile"
			&& selfPreview->ControlInstance->AutomationName
				== L"Changed self preview";
		DesignerModel::DesignDocument inheritedCaptured;
		const bool inheritedSaved = inheritedApplied
			&& inheritedCanvas.BuildDesignDocument(
				inheritedCaptured, &inheritedError);
		const auto inheritedCanonical = inheritedSaved
			? DesignerModel::XamlDocumentSerializer::ToXaml(inheritedCaptured)
			: std::string{};
		AppendFailure(failures,
			inheritedParsed && inheritedApplied && inheritedInitial
			&& inheritedUpdated && inheritedSaved
			&& inheritedCanonical.find("DataContext=\"{Binding Profile")
				!= std::string::npos
			&& inheritedCanonical.find(
				"RelativeSource={RelativeSource Self}") != std::string::npos,
			L"inherited DataContext or RelativeSource Self did not preview/persist: "
				+ inheritedError);
	}

	{
		const std::string contentPresenterXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="ContentPreviewWindow">
  <Window.Resources>
    <DataType x:Key="Person"><DataType.Properties>
      <Property Path="Name" Kind="String" />
    </DataType.Properties></DataType>
    <DataTemplate DataType="Person">
      <TextBlock x:Name="personPreview" Text="{Binding Name}"
             AutomationProperties.Name="Typed person preview" />
    </DataTemplate>
  </Window.Resources>
  <Window.DataContextSchema>
    <Property Path="CurrentPerson" Kind="Object"
              ObjectType="BindingSource" DataType="Person" />
  </Window.DataContextSchema>
	<StackPanel x:Name="contentRoot">
	  <ContentPresenter x:Name="personPresenter"
	                    Content="{Binding CurrentPerson}" />
	  <ContentControl x:Name="personContent"
	                  Content="{Binding CurrentPerson}" />
	  <ContentControl x:Name="visualContent">
	    <TextBlock x:Name="authoredContent" Text="Authored content" />
	  </ContentControl>
	  <Button x:Name="buttonContent" Content="Designer action" />
	  <GroupBox x:Name="personGroup" Header="{Binding CurrentPerson}"
	            Content="Group body" />
	  <Expander x:Name="visualHeaderExpander">
	    <Expander.Header>
	      <TextBlock x:Name="authoredHeader" Text="Authored header" />
	    </Expander.Header>
	    <Canvas x:Name="expanderBody">
	      <TextBlock x:Name="expanderBodyText" Text="Expander body" />
	    </Canvas>
	  </Expander>
	</StackPanel>
</Window>)XAML";
		DesignerModel::DesignDocument contentDocument;
		std::wstring contentError;
		const bool contentParsed = DesignerModel::XamlDocumentParser::FromXaml(
			contentPresenterXaml, contentDocument, &contentError);
		DesignerCanvas contentCanvas(0, 0, 640, 480);
		const bool contentApplied = contentParsed
			&& contentCanvas.ApplyDesignDocument(contentDocument, &contentError);
		auto person = std::make_shared<ObservableObject>();
		person->SetValue(L"Name", std::wstring(L"Designer Alice"));
		auto contentRoot = std::make_shared<ObservableObject>();
		contentRoot->SetValue(L"CurrentPerson", BindingSourceReference(person));
		if (contentApplied) contentCanvas.SetDesignDataContext(contentRoot);
		auto presenterControl = contentApplied
			? FindControl(contentCanvas, L"personPresenter") : nullptr;
		auto* presenter = presenterControl
			? dynamic_cast<ContentPresenter*>(
				presenterControl->ControlInstance) : nullptr;
		auto* preview = presenter
			? dynamic_cast<Label*>(presenter->GetGeneratedContent()) : nullptr;
		auto contentControlRecord = contentApplied
			? FindControl(contentCanvas, L"personContent") : nullptr;
		auto* contentControl = contentControlRecord
			? dynamic_cast<ContentControl*>(
				contentControlRecord->ControlInstance) : nullptr;
		auto* contentPreview = contentControl
			? dynamic_cast<Label*>(
				cui::framework::TemplateAccess::GetGeneratedContent(
					*contentControl)) : nullptr;
		auto visualControlRecord = contentApplied
			? FindControl(contentCanvas, L"visualContent") : nullptr;
		auto* visualControl = visualControlRecord
			? dynamic_cast<ContentControl*>(
				visualControlRecord->ControlInstance) : nullptr;
		auto* authoredContent = visualControl
			? dynamic_cast<Label*>(visualControl->GetVisualContent()) : nullptr;
		auto buttonControlRecord = contentApplied
			? FindControl(contentCanvas, L"buttonContent") : nullptr;
		auto* buttonContent = buttonControlRecord
			? dynamic_cast<Button*>(buttonControlRecord->ControlInstance) : nullptr;
		auto* buttonPreview = buttonContent
			? dynamic_cast<Label*>(
				cui::framework::TemplateAccess::GetGeneratedContent(
					*buttonContent)) : nullptr;
		auto groupRecord = contentApplied
			? FindControl(contentCanvas, L"personGroup") : nullptr;
		auto* group = groupRecord
			? dynamic_cast<GroupBox*>(groupRecord->ControlInstance) : nullptr;
		auto* groupHeader = group
			? dynamic_cast<Label*>(group->GetGeneratedHeaderContent()) : nullptr;
		auto* groupBody = group
			? dynamic_cast<Label*>(
				cui::framework::TemplateAccess::GetGeneratedContent(*group)) : nullptr;
		auto expanderRecord = contentApplied
			? FindControl(contentCanvas, L"visualHeaderExpander") : nullptr;
		auto* expander = expanderRecord
			? dynamic_cast<Expander*>(expanderRecord->ControlInstance) : nullptr;
		auto* authoredHeader = expander
			? dynamic_cast<Label*>(expander->GetVisualHeader()) : nullptr;
		auto* expanderBody = expander
			? dynamic_cast<Panel*>(expander->GetVisualContent()) : nullptr;
		const bool initialPreview = preview
			&& preview->Text == L"Designer Alice"
			&& preview->AutomationName == L"Typed person preview"
			&& contentPreview
			&& contentPreview->Text == L"Designer Alice"
			&& authoredContent
			&& authoredContent->Text == L"Authored content"
			&& buttonPreview
			&& buttonPreview->GetDisplayText() == L"Designer action"
			&& groupHeader && groupHeader->Text == L"Designer Alice"
			&& groupBody && groupBody->Text == L"Group body"
			&& authoredHeader && authoredHeader->Text == L"Authored header"
			&& expanderBody;
		auto replacement = std::make_shared<ObservableObject>();
		replacement->SetValue(L"Name", std::wstring(L"Designer Bob"));
		contentRoot->SetValue(
			L"CurrentPerson", BindingSourceReference(replacement));
		preview = presenter
			? dynamic_cast<Label*>(presenter->GetGeneratedContent()) : nullptr;
		contentPreview = contentControl
			? dynamic_cast<Label*>(
				cui::framework::TemplateAccess::GetGeneratedContent(
					*contentControl)) : nullptr;
		groupHeader = group
			? dynamic_cast<Label*>(group->GetGeneratedHeaderContent()) : nullptr;
		DesignerModel::DesignDocument capturedContent;
		const bool contentSaved = contentApplied
			&& contentCanvas.BuildDesignDocument(capturedContent, &contentError);
		const auto canonical = contentSaved
			? DesignerModel::XamlDocumentSerializer::ToXaml(capturedContent)
			: std::string{};
		const bool contentCanonicalDataType =
			canonical.find("DataType=\"Person\"") != std::string::npos;
		const bool contentCanonicalPresenter =
			canonical.find("<ContentPresenter") != std::string::npos;
		const bool contentCanonicalControl =
			canonical.find("<ContentControl") != std::string::npos;
		const bool contentCanonicalButton =
			canonical.find("<Button") != std::string::npos
			&& canonical.find("x:Name=\"buttonContent\"") != std::string::npos
			&& canonical.find("Content=\"Designer action\"")
				!= std::string::npos;
		const bool contentCanonicalHeader =
			canonical.find("Header=\"{Binding CurrentPerson}")
				!= std::string::npos;
		const bool contentCanonicalVisuals =
			canonical.find("<Expander.Header>") != std::string::npos
			&& canonical.find("Authored content") != std::string::npos;
		AppendFailure(failures,
			contentParsed && contentApplied && initialPreview
			&& preview && preview->Text == L"Designer Bob"
			&& contentPreview && contentPreview->Text == L"Designer Bob"
			&& groupHeader && groupHeader->Text == L"Designer Bob"
			&& contentSaved
			&& canonical.find("DataType=\"Person\"") != std::string::npos
			&& canonical.find("<ContentPresenter") != std::string::npos
			&& canonical.find("<ContentControl") != std::string::npos
			&& canonical.find("<Button") != std::string::npos
			&& canonical.find("x:Name=\"buttonContent\"") != std::string::npos
			&& canonical.find("Content=\"Designer action\"") != std::string::npos
			&& canonical.find("Header=\"{Binding CurrentPerson}")
				!= std::string::npos
			&& canonical.find("<Expander.Header>") != std::string::npos
			&& canonical.find("Authored content") != std::string::npos,
			L"ContentPresenter/ContentControl/HeaderedContentControl did not preview or persist in Designer: "
				+ contentError
				+ L" [dataType=" + SelfTestFlag(contentCanonicalDataType)
				+ L", presenter=" + SelfTestFlag(contentCanonicalPresenter)
				+ L", contentControl=" + SelfTestFlag(contentCanonicalControl)
				+ L", button=" + SelfTestFlag(contentCanonicalButton)
				+ L", header=" + SelfTestFlag(contentCanonicalHeader)
				+ L", visuals=" + SelfTestFlag(contentCanonicalVisuals)
				+ L"]");
	}

	{
		const std::string resourcePropertyXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml" x:Name="ResourceWindow">
  <Window.Resources>
    <Color x:Key="TextMuted">#FF748399</Color>
  </Window.Resources>
  <TextBlock x:Name="resourceLabel" Foreground="{StaticResource TextMuted}" />
</Window>)XAML";
		DesignerModel::DesignDocument resourceDocument;
		std::wstring resourceError;
		const bool resourceParsed = DesignerModel::XamlDocumentParser::FromXaml(
			resourcePropertyXaml, resourceDocument, &resourceError);
		DesignerCanvas resourceCanvas(0, 0, 640, 480);
		const bool resourceApplied = resourceParsed
			&& resourceCanvas.ApplyDesignDocument(resourceDocument, &resourceError);
		auto resourceControl = resourceApplied
			? FindControl(resourceCanvas, L"resourceLabel") : nullptr;
		DesignerModel::DesignDocument capturedResourceDocument;
		const bool resourceCaptured = resourceControl
			&& resourceCanvas.BuildDesignDocument(
				capturedResourceDocument, &resourceError);
		const auto* capturedResourceProperty = resourceCaptured
			&& !capturedResourceDocument.Nodes.empty()
			? capturedResourceDocument.Nodes.front().Properties.Find(L"Foreground")
			: nullptr;
		const bool initialReferencePreserved = capturedResourceProperty
			&& capturedResourceProperty->ResourceKey == L"TextMuted";

		auto updatedResources = resourceCanvas.GetDocumentStyleSheet();
		if (!updatedResources.Resources.empty())
			updatedResources.Resources.front().Value.Text = L"#FF112233";
		const bool resourceValueUpdated = resourceApplied
			&& resourceCanvas.SetDocumentStyleSheet(
				updatedResources, &resourceError);
		const bool previewResourceUpdated = resourceControl
			&& std::fabs(resourceControl->ControlInstance->Foreground.Color.r
				- 0x11 / 255.0f) < 0.0001f;

		auto renamedResources = resourceCanvas.GetDocumentStyleSheet();
		if (!renamedResources.Resources.empty())
			renamedResources.Resources.front().Key = L"SecondaryText";
		const bool resourceRenamed = resourceValueUpdated
			&& resourceCanvas.SetDocumentStyleSheet(
				renamedResources, &resourceError,
				{ { L"TextMuted", L"SecondaryText" } });
		DesignerModel::DesignDocument renamedResourceDocument;
		const bool renamedResourceCaptured = resourceRenamed
			&& resourceCanvas.BuildDesignDocument(
				renamedResourceDocument, &resourceError);
		const auto renamedResourceXaml = renamedResourceCaptured
			? DesignerModel::XamlDocumentSerializer::ToXaml(
				renamedResourceDocument) : std::string{};
		const bool renamedReferencePreserved = renamedResourceCaptured
			&& renamedResourceXaml.find(
				"Foreground=\"{StaticResource SecondaryText}\"")
				!= std::string::npos;

		auto missingResources = resourceCanvas.GetDocumentStyleSheet();
		missingResources.Resources.clear();
		const bool missingResourceUpdateRejected = !resourceCanvas.SetDocumentStyleSheet(
			missingResources, &resourceError);
		const auto preservedResourceReference = resourceControl
			? resourceControl->MetadataPropertyResourceKeys.find(L"Foreground")
			: std::map<std::wstring, std::wstring>::const_iterator{};
		const bool missingResourceRejected = missingResourceUpdateRejected
			&& resourceControl
			&& preservedResourceReference
				!= resourceControl->MetadataPropertyResourceKeys.end()
			&& preservedResourceReference->second == L"SecondaryText"
			&& std::fabs(resourceControl->ControlInstance->Foreground.Color.r
				- 0x11 / 255.0f) < 0.0001f;
		AppendFailure(failures,
			resourceParsed && resourceApplied && resourceCaptured
			&& initialReferencePreserved && resourceValueUpdated
			&& previewResourceUpdated && resourceRenamed
			&& renamedReferencePreserved && missingResourceRejected,
			L"direct property StaticResource was not preserved, refreshed, renamed, or rejected transactionally: "
				+ resourceError
				+ L" [parsed=" + (resourceParsed ? L"1" : L"0")
				+ L", applied=" + (resourceApplied ? L"1" : L"0")
				+ L", captured=" + (resourceCaptured ? L"1" : L"0")
				+ L", initial=" + (initialReferencePreserved ? L"1" : L"0")
				+ L", updated=" + (resourceValueUpdated ? L"1" : L"0")
				+ L", preview=" + (previewResourceUpdated ? L"1" : L"0")
				+ L", renamed=" + (resourceRenamed ? L"1" : L"0")
				+ L", renamedRef=" + (renamedReferencePreserved ? L"1" : L"0")
				+ L", missingRejected=" + (missingResourceRejected ? L"1" : L"0")
				+ L", capturedKey=" + (capturedResourceProperty
					? capturedResourceProperty->ResourceKey
					: L"<none>")
				+ L", renamedXamlHasLabel=" + (renamedResourceXaml.find(
					"resourceLabel") != std::string::npos ? L"1" : L"0")
				+ L", liveKey=" + (resourceControl
					&& resourceControl->MetadataPropertyResourceKeys.contains(L"Foreground")
					? resourceControl->MetadataPropertyResourceKeys.at(L"Foreground")
					: L"<none>")
				+ L", tracked=" + (resourceControl
					? std::to_wstring(resourceControl->MetadataProperties.size())
					: L"0")
				+ L"]");
	}

	{
		const std::string controlTemplatePropertyXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  x:Name="TemplatePropertyWindow">
  <Window.Resources>
	<Color x:Key="TemplateAccent">#FF336699</Color>
    <ControlTemplate x:Key="FirstButtonTemplate" TargetType="Button">
	  <TextBlock x:Name="firstChrome" Text="{TemplateBinding Content}"
	         Foreground="{StaticResource TemplateAccent}" />
    </ControlTemplate>
    <ControlTemplate x:Key="SecondButtonTemplate" TargetType="Button">
	  <TextBlock x:Name="secondChrome" Text="{TemplateBinding Content}"
	         Foreground="{StaticResource TemplateAccent}" />
    </ControlTemplate>
  </Window.Resources>
  <Button x:Name="templateButton" Content="Designer template"
          Template="{StaticResource FirstButtonTemplate}" />
</Window>)XAML";
		DesignerModel::DesignDocument templateDocument;
		std::wstring templateError;
		const bool templateParsed = DesignerModel::XamlDocumentParser::FromXaml(
			controlTemplatePropertyXaml, templateDocument, &templateError);
		DesignerCanvas templateCanvas(0, 0, 640, 480);
		const bool templateApplied = templateParsed
			&& templateCanvas.ApplyDesignDocument(templateDocument, &templateError);
		if (templateApplied)
			templateCanvas.RestoreSelectionByNames(
				{ L"templateButton" }, L"templateButton", true);
		PropertyGrid templateGrid(0, 0, 360, 620);
		templateGrid.SetDesignerCanvas(&templateCanvas);
		ReloadCurrentSelection(templateGrid, templateCanvas);
		const auto* templateRow = DesignerPropertyRowCatalog::Find(
			templateGrid.GetPresentedPropertyRows(), L"Template");
		auto hasChoice = [&](const wchar_t* key)
		{
			return templateRow && std::any_of(
				templateRow->Choices.begin(), templateRow->Choices.end(),
				[&](const auto& choice) { return choice.ValueText == key; });
		};
		const bool choicesVisible = templateRow
			&& templateRow->Value.Text == L"FirstButtonTemplate"
			&& hasChoice(L"FirstButtonTemplate")
			&& hasChoice(L"SecondButtonTemplate");
		const auto templateChanged = templateGrid.ApplyPropertyValue(
			L"Template", L"SecondButtonTemplate");
		auto changedButton = templateChanged
			? FindControl(templateCanvas, L"templateButton") : nullptr;
		const bool changedPreview = changedButton
			&& changedButton->ControlInstance
			&& changedButton->ControlInstance->FindDeclarativeTemplatePart(
				L"secondChrome") != nullptr
			&& changedButton->ControlInstance->FindDeclarativeTemplatePart(
				L"firstChrome") == nullptr;
		DesignerModel::DesignDocument capturedTemplateDocument;
		const bool templateCaptured = templateChanged
			&& templateCanvas.BuildDesignDocument(
				capturedTemplateDocument, &templateError);
		const auto templateCanonical = templateCaptured
			? DesignerModel::XamlDocumentSerializer::ToXaml(
				capturedTemplateDocument) : std::string{};
		const bool templatePersisted = templateCaptured
			&& capturedTemplateDocument.ControlTemplates.size() == 2
			&& capturedTemplateDocument.Nodes.size() == 1
			&& capturedTemplateDocument.Nodes.front().Structure.ControlTemplate
				== L"SecondButtonTemplate"
			&& templateCanonical.find(
				"Template=\"{StaticResource SecondButtonTemplate}\"")
				!= std::string::npos;
		const bool templateUndone = templateChanged
			&& templateCanvas.UndoCommand();
		auto undoneButton = templateUndone
			? FindControl(templateCanvas, L"templateButton") : nullptr;
		const bool undoPreview = undoneButton && undoneButton->ControlInstance
			&& undoneButton->ControlInstance->FindDeclarativeTemplatePart(
				L"firstChrome") != nullptr
			&& templateCanvas.GetControlTemplates().size() == 2;
		const bool templateRedone = templateUndone
			&& templateCanvas.RedoCommand();
		auto redoneButton = templateRedone
			? FindControl(templateCanvas, L"templateButton") : nullptr;
		const bool redoPreview = redoneButton && redoneButton->ControlInstance
			&& redoneButton->ControlInstance->FindDeclarativeTemplatePart(
				L"secondChrome") != nullptr
			&& templateCanvas.GetControlTemplates().size() == 2;
		auto renamedTemplateResources = templateCanvas.GetDocumentStyleSheet();
		if (!renamedTemplateResources.Resources.empty())
		{
			renamedTemplateResources.Resources.front().Key = L"TemplateAccent2";
			renamedTemplateResources.Resources.front().Value.Text = L"#FF112233";
		}
		const bool templateResourceRenamed = redoPreview
			&& templateCanvas.SetDocumentStyleSheet(
				std::move(renamedTemplateResources), &templateError,
				{ { L"TemplateAccent", L"TemplateAccent2" } });
		auto renamedButton = templateResourceRenamed
			? FindControl(templateCanvas, L"templateButton") : nullptr;
		auto* renamedChrome = renamedButton && renamedButton->ControlInstance
			? dynamic_cast<Label*>(renamedButton->ControlInstance
				->FindDeclarativeTemplatePart(L"secondChrome")) : nullptr;
		DesignerModel::DesignDocument renamedTemplateDocument;
		const bool renamedTemplateCaptured = templateResourceRenamed
			&& templateCanvas.BuildDesignDocument(
				renamedTemplateDocument, &templateError);
		const auto renamedTemplateXaml = renamedTemplateCaptured
			? DesignerModel::XamlDocumentSerializer::ToXaml(
				renamedTemplateDocument) : std::string{};
		const bool renamedTemplatePreview = renamedChrome
			&& std::fabs(renamedChrome->Foreground.Color.r - 0x11 / 255.0f) < 0.0001f
			&& renamedTemplateXaml.find(
				"Foreground=\"{StaticResource TemplateAccent2}\"")
				!= std::string::npos;
		AppendFailure(failures,
			templateParsed && templateApplied && choicesVisible
			&& templateChanged.Succeeded && changedPreview
			&& templatePersisted && templateUndone && undoPreview
			&& templateRedone && redoPreview
			&& templateResourceRenamed && renamedTemplatePreview,
			L"ControlTemplate property-grid selection did not preview, persist, or undo: "
				+ templateError
				+ L" [parsed=" + (templateParsed ? L"1" : L"0")
				+ L", applied=" + (templateApplied ? L"1" : L"0")
				+ L", choices=" + (choicesVisible ? L"1" : L"0")
				+ L", changed=" + (templateChanged.Succeeded ? L"1" : L"0")
				+ L", preview=" + (changedPreview ? L"1" : L"0")
				+ L", persisted=" + (templatePersisted ? L"1" : L"0")
				+ L", undo=" + (undoPreview ? L"1" : L"0")
				+ L", redo=" + (redoPreview ? L"1" : L"0")
				+ L", resourceRename="
					+ (templateResourceRenamed ? L"1" : L"0")
				+ L", renamedPreview="
					+ (renamedTemplatePreview ? L"1" : L"0")
				+ L"]");
	}

	{
		const std::string templateSlotXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  x:Name="TemplateSlotDesignerWindow">
  <Window.Resources>
    <ControlTemplate x:Key="GroupSlots" TargetType="GroupBox">
      <Canvas x:Name="slotChrome">
        <ContentPresenter x:Name="slotHeaderPresenter"
                          ContentSource="Header" />
        <ContentPresenter x:Name="slotContentPresenter"
                          ContentSource="Content" />
      </Canvas>
    </ControlTemplate>
  </Window.Resources>
  <GroupBox x:Name="slotHost" DesignId="1"
            Template="{StaticResource GroupSlots}">
    <GroupBox.Header>
      <TextBlock x:Name="slotHeader" DesignId="2" Text="Header before" />
    </GroupBox.Header>
    <TextBlock x:Name="slotBody" DesignId="3" Text="Body before" />
  </GroupBox>
</Window>)XAML";
		DesignerModel::DesignDocument slotDocument;
		std::wstring slotError;
		const bool slotParsed = DesignerModel::XamlDocumentParser::FromXaml(
			templateSlotXaml, slotDocument, &slotError);
		DesignerCanvas slotCanvas(0, 0, 640, 480);
		const bool slotApplied = slotParsed
			&& slotCanvas.ApplyDesignDocument(slotDocument, &slotError);
		if (slotApplied)
			slotCanvas.RestoreSelectionByNames(
				{ L"slotBody" }, L"slotBody", true);
		auto slotHostRecord = slotApplied
			? FindControl(slotCanvas, L"slotHost") : nullptr;
		auto slotHeaderRecord = slotApplied
			? FindControl(slotCanvas, L"slotHeader") : nullptr;
		auto slotBodyRecord = slotApplied
			? FindControl(slotCanvas, L"slotBody") : nullptr;
		auto* slotHost = slotHostRecord
			? dynamic_cast<GroupBox*>(slotHostRecord->ControlInstance) : nullptr;
		auto* slotHeaderPresenter = slotHost
			? slotHost->GetTemplateHeaderPresenter() : nullptr;
		auto* slotContentPresenter = slotHost
			? cui::framework::TemplateAccess::GetContentPresenter(*slotHost)
			: nullptr;
		const bool slotOwnershipReady = slotHost && slotHeaderRecord
			&& slotBodyRecord && slotHeaderPresenter && slotContentPresenter
			&& slotHost->VisualChildCount() == 1
			&& slotHeaderRecord->DesignerParent == slotHost
			&& slotBodyRecord->DesignerParent == slotHost
			&& slotHeaderRecord->ControlInstance->GetVisualParent() == slotHeaderPresenter
			&& slotBodyRecord->ControlInstance->GetVisualParent() == slotContentPresenter
			&& slotHeaderRecord->ControlInstance->GetLogicalParent() == slotHost
			&& slotBodyRecord->ControlInstance->GetLogicalParent() == slotHost
			&& slotHeaderRecord->ControlInstance->GetTemplatedParent() == nullptr
			&& slotBodyRecord->ControlInstance->GetTemplatedParent() == nullptr
			&& slotHeaderPresenter->GetTemplatedParent() == slotHost
			&& slotContentPresenter->GetTemplatedParent() == slotHost
			&& slotHost->GetVisualHeader() == slotHeaderRecord->ControlInstance
			&& slotHost->GetVisualContent() == slotBodyRecord->ControlInstance;
		DesignerModel::DesignDocument capturedSlotDocument;
		const bool slotCaptured = slotApplied
			&& slotCanvas.BuildDesignDocument(
				capturedSlotDocument, &slotError);
		const auto slotCanonical = slotCaptured
			? DesignerModel::XamlDocumentSerializer::ToXaml(
				capturedSlotDocument) : std::string{};
		auto changedSlotXaml = Convert::Utf8ToUnicode(slotCanonical);
		const auto bodyTextAt = changedSlotXaml.find(L"Text=\"Body before\"");
		if (bodyTextAt != std::wstring::npos)
			changedSlotXaml.replace(bodyTextAt,
				std::wstring(L"Text=\"Body before\"").size(),
				L"Text=\"Body after\"");
		(void)slotCanvas.ResetDocumentHistoryAsSaved();
		const auto beginSlotEdit = slotCanvas.BeginDocumentEditTransaction(
			L"EditXaml");
		const bool slotPreviewed = beginSlotEdit.Succeeded()
			&& bodyTextAt != std::wstring::npos
			&& slotCanvas.PreviewXamlDocumentText(
				changedSlotXaml, &slotError);
		auto changedSlotBody = slotPreviewed
			? FindControl(slotCanvas, L"slotBody") : nullptr;
		auto changedSlotHostRecord = slotPreviewed
			? FindControl(slotCanvas, L"slotHost") : nullptr;
		auto* changedSlotHost = changedSlotHostRecord
			? dynamic_cast<GroupBox*>(changedSlotHostRecord->ControlInstance) : nullptr;
		const bool slotPreviewReady = changedSlotBody && changedSlotHost
			&& ReadControlStringProperty(
				changedSlotBody->ControlInstance, L"Text") == L"Body after"
			&& slotCanvas.GetSelectedControl()
			&& slotCanvas.GetSelectedControl()->Name == L"slotBody"
			&& cui::framework::TemplateAccess::GetContentPresenter(
				*changedSlotHost)
			&& changedSlotBody->ControlInstance->GetVisualParent()
				== cui::framework::TemplateAccess::GetContentPresenter(
					*changedSlotHost)
			&& changedSlotBody->DesignerParent == changedSlotHost;
		const auto commitSlotEdit = slotCanvas.CommitDocumentEditTransaction();
		const auto undoSlotEdit = commitSlotEdit.HasChanges()
			? slotCanvas.UndoCommand() : DesignerDocumentTransactionResult{};
		auto undoneSlotBody = undoSlotEdit.HasChanges()
			? FindControl(slotCanvas, L"slotBody") : nullptr;
		const bool slotUndoReady = undoneSlotBody
			&& ReadControlStringProperty(
				undoneSlotBody->ControlInstance, L"Text") == L"Body before";
		const auto redoSlotEdit = slotUndoReady
			? slotCanvas.RedoCommand() : DesignerDocumentTransactionResult{};
		auto redoneSlotBody = redoSlotEdit.HasChanges()
			? FindControl(slotCanvas, L"slotBody") : nullptr;
		auto redoneSlotHostRecord = redoSlotEdit.HasChanges()
			? FindControl(slotCanvas, L"slotHost") : nullptr;
		auto* redoneSlotHost = redoneSlotHostRecord
			? dynamic_cast<GroupBox*>(redoneSlotHostRecord->ControlInstance) : nullptr;
		const bool slotRedoReady = redoneSlotBody && redoneSlotHost
			&& ReadControlStringProperty(
				redoneSlotBody->ControlInstance, L"Text") == L"Body after"
			&& cui::framework::TemplateAccess::GetContentPresenter(
				*redoneSlotHost)
			&& redoneSlotBody->ControlInstance->GetVisualParent()
				== cui::framework::TemplateAccess::GetContentPresenter(
					*redoneSlotHost);
		AppendFailure(failures,
			slotParsed && slotApplied && slotOwnershipReady && slotCaptured
			&& slotCanonical.find("ContentSource=\"Header\"")
				!= std::string::npos
			&& slotCanonical.find("ContentSource=\"Content\"")
				!= std::string::npos
			&& slotPreviewed && slotPreviewReady
			&& commitSlotEdit.HasChanges() && slotUndoReady && slotRedoReady,
			L"ControlTemplate ContentSource did not preserve logical ownership, "
			L"selection, XAML preview, or undo/redo: " + slotError
				+ L" [parsed=" + SelfTestFlag(slotParsed)
				+ L", applied=" + SelfTestFlag(slotApplied)
				+ L", ownership=" + SelfTestFlag(slotOwnershipReady)
				+ L", captured=" + SelfTestFlag(slotCaptured)
				+ L", preview=" + SelfTestFlag(slotPreviewReady)
				+ L", undo=" + SelfTestFlag(slotUndoReady)
				+ L", redo=" + SelfTestFlag(slotRedoReady) + L"]");
	}

	{
		const std::string itemsPresenterXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  x:Name="ItemsPresenterDesignerWindow">
  <Window.Resources>
    <ControlTemplate x:Key="ListChrome" TargetType="ListBox">
      <Grid x:Name="listChrome">
        <ScrollViewer x:Name="listScroll">
          <ItemsPresenter x:Name="itemsSlot" />
        </ScrollViewer>
      </Grid>
    </ControlTemplate>
  </Window.Resources>
  <ListBox x:Name="templatedList" DesignId="1"
           Template="{StaticResource ListChrome}" />
</Window>)XAML";
		DesignerModel::DesignDocument itemsPresenterDocument;
		std::wstring itemsPresenterError;
		const bool itemsPresenterParsed =
			DesignerModel::XamlDocumentParser::FromXaml(itemsPresenterXaml,
				itemsPresenterDocument, &itemsPresenterError);
		DesignerCanvas itemsPresenterCanvas(0, 0, 640, 480);
		const bool itemsPresenterApplied = itemsPresenterParsed
			&& itemsPresenterCanvas.ApplyDesignDocument(
				itemsPresenterDocument, &itemsPresenterError);
		if (itemsPresenterApplied)
			itemsPresenterCanvas.RestoreSelectionByNames(
				{ L"templatedList" }, L"templatedList", true);
		auto listRecord = itemsPresenterApplied
			? FindControl(itemsPresenterCanvas, L"templatedList") : nullptr;
		auto* list = listRecord
			? dynamic_cast<ListBox*>(listRecord->ControlInstance) : nullptr;
		auto* presenter = list
			? cui::framework::TemplateAccess::GetItemsPresenter(*list) : nullptr;
		const bool itemsPresenterOwnership = list && presenter
			&& cui::framework::TemplateAccess::GetItemsHost(*presenter)
				== cui::framework::TemplateAccess::GetItemsHost(*list)
			&& cui::framework::TemplateAccess::GetItemsHost(*list)
				&& cui::framework::TemplateAccess::GetItemsHost(*list)
				->GetVisualParent() == presenter
			&& cui::framework::TemplateAccess::GetItemsHost(*list)
				->GetLogicalParent() == nullptr
			&& cui::framework::TemplateAccess::GetItemsHost(*list)
				->GetTemplatedParent() == list
			&& presenter->GetTemplatedParent() == list
			&& dynamic_cast<ScrollViewer*>(presenter->GetVisualParent()) != nullptr
			&& list->FindDeclarativeTemplatePart(L"itemsSlot") == presenter;
		DesignerModel::DesignDocument capturedItemsPresenter;
		const bool itemsPresenterCaptured = itemsPresenterApplied
			&& itemsPresenterCanvas.BuildDesignDocument(
				capturedItemsPresenter, &itemsPresenterError);
		const auto itemsPresenterCanonical = itemsPresenterCaptured
			? DesignerModel::XamlDocumentSerializer::ToXaml(
				capturedItemsPresenter) : std::string{};
		auto changedItemsPresenterXaml =
			Convert::Utf8ToUnicode(itemsPresenterCanonical);
		const auto slotAt = changedItemsPresenterXaml.find(
			L"x:Name=\"itemsSlot\"");
		if (slotAt != std::wstring::npos)
			changedItemsPresenterXaml.replace(slotAt,
				std::wstring(L"x:Name=\"itemsSlot\"").size(),
				L"x:Name=\"itemsSlotReloaded\"");
		(void)itemsPresenterCanvas.ResetDocumentHistoryAsSaved();
		const auto beginItemsPresenterEdit =
			itemsPresenterCanvas.BeginDocumentEditTransaction(L"EditXaml");
		const bool itemsPresenterPreviewed = beginItemsPresenterEdit.Succeeded()
			&& slotAt != std::wstring::npos
			&& itemsPresenterCanvas.PreviewXamlDocumentText(
				changedItemsPresenterXaml, &itemsPresenterError);
		auto changedListRecord = itemsPresenterPreviewed
			? FindControl(itemsPresenterCanvas, L"templatedList") : nullptr;
		auto* changedList = changedListRecord
			? dynamic_cast<ListBox*>(changedListRecord->ControlInstance) : nullptr;
		const bool itemsPresenterPreviewReady = changedList
			&& cui::framework::TemplateAccess::GetItemsPresenter(*changedList)
			&& changedList->FindDeclarativeTemplatePart(L"itemsSlotReloaded")
				== cui::framework::TemplateAccess::GetItemsPresenter(*changedList)
			&& cui::framework::TemplateAccess::GetItemsHost(*changedList)
				->GetVisualParent()
				== cui::framework::TemplateAccess::GetItemsPresenter(*changedList)
			&& itemsPresenterCanvas.GetSelectedControl()
			&& itemsPresenterCanvas.GetSelectedControl()->Name == L"templatedList";
		const auto commitItemsPresenterEdit =
			itemsPresenterCanvas.CommitDocumentEditTransaction();
		const auto undoItemsPresenterEdit = commitItemsPresenterEdit.HasChanges()
			? itemsPresenterCanvas.UndoCommand()
			: DesignerDocumentTransactionResult{};
		auto undoListRecord = undoItemsPresenterEdit.HasChanges()
			? FindControl(itemsPresenterCanvas, L"templatedList") : nullptr;
		auto* undoList = undoListRecord
			? dynamic_cast<ListBox*>(undoListRecord->ControlInstance) : nullptr;
		const bool itemsPresenterUndoReady = undoList
			&& undoList->FindDeclarativeTemplatePart(L"itemsSlot")
				== cui::framework::TemplateAccess::GetItemsPresenter(*undoList);
		const auto redoItemsPresenterEdit = itemsPresenterUndoReady
			? itemsPresenterCanvas.RedoCommand()
			: DesignerDocumentTransactionResult{};
		auto redoListRecord = redoItemsPresenterEdit.HasChanges()
			? FindControl(itemsPresenterCanvas, L"templatedList") : nullptr;
		auto* redoList = redoListRecord
			? dynamic_cast<ListBox*>(redoListRecord->ControlInstance) : nullptr;
		const bool itemsPresenterRedoReady = redoList
			&& redoList->FindDeclarativeTemplatePart(L"itemsSlotReloaded")
				== cui::framework::TemplateAccess::GetItemsPresenter(*redoList);
		AppendFailure(failures,
			itemsPresenterParsed && itemsPresenterApplied
			&& itemsPresenterOwnership && itemsPresenterCaptured
			&& itemsPresenterCanonical.find("<ItemsPresenter")
				!= std::string::npos
			&& itemsPresenterPreviewed && itemsPresenterPreviewReady
			&& commitItemsPresenterEdit.HasChanges()
			&& itemsPresenterUndoReady && itemsPresenterRedoReady,
			L"ItemsPresenter Designer preview did not preserve ItemsHost ownership, "
			L"selection, canonical XAML, or undo/redo: " + itemsPresenterError
				+ L" [parsed=" + SelfTestFlag(itemsPresenterParsed)
				+ L", applied=" + SelfTestFlag(itemsPresenterApplied)
				+ L", ownership=" + SelfTestFlag(itemsPresenterOwnership)
				+ L", captured=" + SelfTestFlag(itemsPresenterCaptured)
				+ L", preview=" + SelfTestFlag(itemsPresenterPreviewReady)
				+ L", undo=" + SelfTestFlag(itemsPresenterUndoReady)
				+ L", redo=" + SelfTestFlag(itemsPresenterRedoReady) + L"]");
	}

	{
		const std::string listBoxItemXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  x:Name="ListBoxItemDesignerWindow">
  <Window.Resources>
    <DataType x:Key="DesignerRow">
      <DataType.Properties>
        <Property Path="Name" Kind="String" />
      </DataType.Properties>
    </DataType>
    <DataList x:Key="DesignerRows" ItemType="DesignerRow">
      <DataRecord Name="One" />
      <DataRecord Name="Two" />
    </DataList>
    <DataTemplate x:Key="DesignerRowTemplate" DataType="DesignerRow">
      <TextBlock Text="{Binding Name}" />
    </DataTemplate>
    <ControlTemplate x:Key="DesignerItemChrome" TargetType="ListBoxItem">
      <Canvas x:Name="itemChrome">
        <ContentPresenter ContentSource="Content" />
      </Canvas>
    </ControlTemplate>
    <Style x:Key="DesignerContainerStyle" TargetType="ListBoxItem">
      <Setter Property="Padding" Value="7,3" />
      <Setter Property="Template"
              Value="{StaticResource DesignerItemChrome}" />
    </Style>
  </Window.Resources>
  <ListBox x:Name="designerList" DesignId="1"
           ItemsSource="{StaticResource DesignerRows}"
           ItemTemplate="{StaticResource DesignerRowTemplate}"
           ItemContainerStyle="{StaticResource DesignerContainerStyle}" />
</Window>)XAML";
		DesignerModel::DesignDocument listBoxItemDocument;
		std::wstring listBoxItemError;
		const bool listBoxItemParsed =
			DesignerModel::XamlDocumentParser::FromXaml(listBoxItemXaml,
				listBoxItemDocument, &listBoxItemError);
		DesignerCanvas listBoxItemCanvas(0, 0, 640, 480);
		const bool listBoxItemApplied = listBoxItemParsed
			&& listBoxItemCanvas.ApplyDesignDocument(
				listBoxItemDocument, &listBoxItemError);
		if (listBoxItemApplied)
			listBoxItemCanvas.RestoreSelectionByNames(
				{ L"designerList" }, L"designerList", true);
		auto listBoxItemRecord = listBoxItemApplied
			? FindControl(listBoxItemCanvas, L"designerList") : nullptr;
		auto* designerList = listBoxItemRecord
			? dynamic_cast<ListBox*>(listBoxItemRecord->ControlInstance) : nullptr;
		auto* designerItem = designerList
			? dynamic_cast<ListBoxItem*>(designerList->GetGeneratedItem(0)) : nullptr;
		const bool listBoxItemPreviewReady = designerItem
			&& designerItem->FindDeclarativeTemplatePart(L"itemChrome")
			&& dynamic_cast<Label*>(designerItem->Content())
			&& dynamic_cast<Label*>(designerItem->Content())->Text == L"One";
		DesignerModel::DesignDocument capturedListBoxItem;
		const bool listBoxItemCaptured = listBoxItemApplied
			&& listBoxItemCanvas.BuildDesignDocument(
				capturedListBoxItem, &listBoxItemError);
		const auto listBoxItemCanonical = listBoxItemCaptured
			? DesignerModel::XamlDocumentSerializer::ToXaml(capturedListBoxItem)
			: std::string{};
		auto changedListBoxItemXaml =
			Convert::Utf8ToUnicode(listBoxItemCanonical);
		const auto itemChromeAt = changedListBoxItemXaml.find(L"itemChrome");
		if (itemChromeAt != std::wstring::npos)
			changedListBoxItemXaml.replace(itemChromeAt,
				std::wstring(L"itemChrome").size(), L"itemChromeReloaded");
		(void)listBoxItemCanvas.ResetDocumentHistoryAsSaved();
		const auto beginListBoxItemEdit =
			listBoxItemCanvas.BeginDocumentEditTransaction(L"EditXaml");
		const bool listBoxItemPreviewed = beginListBoxItemEdit.Succeeded()
			&& itemChromeAt != std::wstring::npos
			&& listBoxItemCanvas.PreviewXamlDocumentText(
				changedListBoxItemXaml, &listBoxItemError);
		auto changedListBoxItemRecord = listBoxItemPreviewed
			? FindControl(listBoxItemCanvas, L"designerList") : nullptr;
		auto* changedDesignerList = changedListBoxItemRecord
			? dynamic_cast<ListBox*>(changedListBoxItemRecord->ControlInstance) : nullptr;
		auto* changedDesignerItem = changedDesignerList
			? dynamic_cast<ListBoxItem*>(changedDesignerList->GetGeneratedItem(0))
			: nullptr;
		const bool listBoxItemChangedReady = changedDesignerItem
			&& changedDesignerItem->FindDeclarativeTemplatePart(
				L"itemChromeReloaded")
			&& listBoxItemCanvas.GetSelectedControl()
			&& listBoxItemCanvas.GetSelectedControl()->Name == L"designerList";
		const auto commitListBoxItemEdit =
			listBoxItemCanvas.CommitDocumentEditTransaction();
		const auto undoListBoxItemEdit = commitListBoxItemEdit.HasChanges()
			? listBoxItemCanvas.UndoCommand()
			: DesignerDocumentTransactionResult{};
		auto undoListBoxItemRecord = undoListBoxItemEdit.HasChanges()
			? FindControl(listBoxItemCanvas, L"designerList") : nullptr;
		auto* undoDesignerList = undoListBoxItemRecord
			? dynamic_cast<ListBox*>(undoListBoxItemRecord->ControlInstance) : nullptr;
		auto* undoDesignerItem = undoDesignerList
			? dynamic_cast<ListBoxItem*>(undoDesignerList->GetGeneratedItem(0))
			: nullptr;
		const bool listBoxItemUndoReady = undoDesignerItem
			&& undoDesignerItem->FindDeclarativeTemplatePart(L"itemChrome");
		const auto redoListBoxItemEdit = listBoxItemUndoReady
			? listBoxItemCanvas.RedoCommand()
			: DesignerDocumentTransactionResult{};
		auto redoListBoxItemRecord = redoListBoxItemEdit.HasChanges()
			? FindControl(listBoxItemCanvas, L"designerList") : nullptr;
		auto* redoDesignerList = redoListBoxItemRecord
			? dynamic_cast<ListBox*>(redoListBoxItemRecord->ControlInstance) : nullptr;
		auto* redoDesignerItem = redoDesignerList
			? dynamic_cast<ListBoxItem*>(redoDesignerList->GetGeneratedItem(0))
			: nullptr;
		const bool listBoxItemRedoReady = redoDesignerItem
			&& redoDesignerItem->FindDeclarativeTemplatePart(
				L"itemChromeReloaded");
		AppendFailure(failures,
			listBoxItemParsed && listBoxItemApplied && listBoxItemPreviewReady
			&& listBoxItemCaptured
			&& listBoxItemCanonical.find("TargetType=\"ListBoxItem\"")
				!= std::string::npos
			&& listBoxItemPreviewed && listBoxItemChangedReady
			&& commitListBoxItemEdit.HasChanges()
			&& listBoxItemUndoReady && listBoxItemRedoReady,
			L"ListBoxItem Designer preview did not preserve generated content, "
			L"selection, canonical XAML, or undo/redo: " + listBoxItemError
				+ L" [parsed=" + SelfTestFlag(listBoxItemParsed)
				+ L", applied=" + SelfTestFlag(listBoxItemApplied)
				+ L", initial=" + SelfTestFlag(listBoxItemPreviewReady)
				+ L", captured=" + SelfTestFlag(listBoxItemCaptured)
				+ L", preview=" + SelfTestFlag(listBoxItemChangedReady)
				+ L", undo=" + SelfTestFlag(listBoxItemUndoReady)
				+ L", redo=" + SelfTestFlag(listBoxItemRedoReady) + L"]");
	}

	{
		const std::string comboBoxItemXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  x:Name="ComboBoxItemDesignerWindow">
  <Window.Resources>
    <DataType x:Key="DesignerChoice">
      <DataType.Properties>
        <Property Path="Name" Kind="String" />
      </DataType.Properties>
    </DataType>
    <DataList x:Key="DesignerChoices" ItemType="DesignerChoice">
      <DataRecord Name="One" />
      <DataRecord Name="Two" />
    </DataList>
    <DataTemplate x:Key="DesignerChoiceTemplate" DataType="DesignerChoice">
      <TextBlock Text="{Binding Name}" />
    </DataTemplate>
    <ItemsPanelTemplate x:Key="DesignerChoiceItemsPanel">
      <StackPanel />
    </ItemsPanelTemplate>
    <ControlTemplate x:Key="DesignerChoiceChrome"
                     TargetType="ComboBoxItem">
      <Canvas x:Name="choiceChrome">
        <ContentPresenter ContentSource="Content" />
      </Canvas>
    </ControlTemplate>
    <Style x:Key="DesignerChoiceContainer" TargetType="ComboBoxItem">
      <Setter Property="Template"
              Value="{StaticResource DesignerChoiceChrome}" />
    </Style>
  </Window.Resources>
  <ComboBox x:Name="designerCombo" DesignId="1"
            ItemsSource="{StaticResource DesignerChoices}"
            ItemsPanel="{StaticResource DesignerChoiceItemsPanel}"
            ItemTemplate="{StaticResource DesignerChoiceTemplate}"
            ItemContainerStyle="{StaticResource DesignerChoiceContainer}" />
</Window>)XAML";
		DesignerModel::DesignDocument comboBoxItemDocument;
		std::wstring comboBoxItemError;
		const bool comboBoxItemParsed =
			DesignerModel::XamlDocumentParser::FromXaml(comboBoxItemXaml,
				comboBoxItemDocument, &comboBoxItemError);
		DesignerCanvas comboBoxItemCanvas(0, 0, 640, 480);
		const bool comboBoxItemApplied = comboBoxItemParsed
			&& comboBoxItemCanvas.ApplyDesignDocument(
				comboBoxItemDocument, &comboBoxItemError);
		if (comboBoxItemApplied)
			comboBoxItemCanvas.RestoreSelectionByNames(
				{ L"designerCombo" }, L"designerCombo", true);
		auto comboBoxItemRecord = comboBoxItemApplied
			? FindControl(comboBoxItemCanvas, L"designerCombo") : nullptr;
		auto* designerCombo = comboBoxItemRecord
			? dynamic_cast<ComboBox*>(comboBoxItemRecord->ControlInstance) : nullptr;
		auto* designerChoice = designerCombo
			? designerCombo->GetGeneratedItem(0) : nullptr;
		const bool comboBoxItemPreviewReady = designerChoice
			&& designerChoice->FindDeclarativeTemplatePart(L"choiceChrome")
			&& dynamic_cast<Label*>(designerChoice->Content())
			&& dynamic_cast<Label*>(designerChoice->Content())->Text == L"One";
		DesignerModel::DesignDocument capturedComboBoxItem;
		const bool comboBoxItemCaptured = comboBoxItemApplied
			&& comboBoxItemCanvas.BuildDesignDocument(
				capturedComboBoxItem, &comboBoxItemError);
		const auto comboBoxItemCanonical = comboBoxItemCaptured
			? DesignerModel::XamlDocumentSerializer::ToXaml(capturedComboBoxItem)
			: std::string{};
		auto changedComboBoxItemXaml =
			Convert::Utf8ToUnicode(comboBoxItemCanonical);
		const auto choiceChromeAt = changedComboBoxItemXaml.find(L"choiceChrome");
		if (choiceChromeAt != std::wstring::npos)
			changedComboBoxItemXaml.replace(choiceChromeAt,
				std::wstring(L"choiceChrome").size(), L"choiceChromeReloaded");
		(void)comboBoxItemCanvas.ResetDocumentHistoryAsSaved();
		const auto beginComboBoxItemEdit =
			comboBoxItemCanvas.BeginDocumentEditTransaction(L"EditXaml");
		const bool comboBoxItemPreviewed = beginComboBoxItemEdit.Succeeded()
			&& choiceChromeAt != std::wstring::npos
			&& comboBoxItemCanvas.PreviewXamlDocumentText(
				changedComboBoxItemXaml, &comboBoxItemError);
		auto changedComboBoxItemRecord = comboBoxItemPreviewed
			? FindControl(comboBoxItemCanvas, L"designerCombo") : nullptr;
		auto* changedDesignerCombo = changedComboBoxItemRecord
			? dynamic_cast<ComboBox*>(changedComboBoxItemRecord->ControlInstance)
			: nullptr;
		auto* changedDesignerChoice = changedDesignerCombo
			? changedDesignerCombo->GetGeneratedItem(0) : nullptr;
		const bool comboBoxItemChangedReady = changedDesignerChoice
			&& changedDesignerChoice->FindDeclarativeTemplatePart(
				L"choiceChromeReloaded")
			&& comboBoxItemCanvas.GetSelectedControl()
			&& comboBoxItemCanvas.GetSelectedControl()->Name == L"designerCombo";
		const auto commitComboBoxItemEdit =
			comboBoxItemCanvas.CommitDocumentEditTransaction();
		const auto undoComboBoxItemEdit = commitComboBoxItemEdit.HasChanges()
			? comboBoxItemCanvas.UndoCommand()
			: DesignerDocumentTransactionResult{};
		auto undoComboBoxItemRecord = undoComboBoxItemEdit.HasChanges()
			? FindControl(comboBoxItemCanvas, L"designerCombo") : nullptr;
		auto* undoDesignerCombo = undoComboBoxItemRecord
			? dynamic_cast<ComboBox*>(undoComboBoxItemRecord->ControlInstance)
			: nullptr;
		auto* undoDesignerChoice = undoDesignerCombo
			? undoDesignerCombo->GetGeneratedItem(0) : nullptr;
		const bool comboBoxItemUndoReady = undoDesignerChoice
			&& undoDesignerChoice->FindDeclarativeTemplatePart(L"choiceChrome");
		const auto redoComboBoxItemEdit = comboBoxItemUndoReady
			? comboBoxItemCanvas.RedoCommand()
			: DesignerDocumentTransactionResult{};
		auto redoComboBoxItemRecord = redoComboBoxItemEdit.HasChanges()
			? FindControl(comboBoxItemCanvas, L"designerCombo") : nullptr;
		auto* redoDesignerCombo = redoComboBoxItemRecord
			? dynamic_cast<ComboBox*>(redoComboBoxItemRecord->ControlInstance)
			: nullptr;
		auto* redoDesignerChoice = redoDesignerCombo
			? redoDesignerCombo->GetGeneratedItem(0) : nullptr;
		const bool comboBoxItemRedoReady = redoDesignerChoice
			&& redoDesignerChoice->FindDeclarativeTemplatePart(
				L"choiceChromeReloaded");
		AppendFailure(failures,
			comboBoxItemParsed && comboBoxItemApplied
			&& comboBoxItemPreviewReady && comboBoxItemCaptured
			&& comboBoxItemCanonical.find("TargetType=\"ComboBoxItem\"")
				!= std::string::npos
			&& comboBoxItemPreviewed && comboBoxItemChangedReady
			&& commitComboBoxItemEdit.HasChanges()
			&& comboBoxItemUndoReady && comboBoxItemRedoReady,
			L"ComboBoxItem Designer preview did not preserve generated content, "
			L"selection, canonical XAML, or undo/redo: " + comboBoxItemError
				+ L" [parsed=" + SelfTestFlag(comboBoxItemParsed)
				+ L", applied=" + SelfTestFlag(comboBoxItemApplied)
				+ L", initial=" + SelfTestFlag(comboBoxItemPreviewReady)
				+ L", captured=" + SelfTestFlag(comboBoxItemCaptured)
				+ L", preview=" + SelfTestFlag(comboBoxItemChangedReady)
				+ L", undo=" + SelfTestFlag(comboBoxItemUndoReady)
				+ L", redo=" + SelfTestFlag(comboBoxItemRedoReady) + L"]");
	}

	{
		const std::string treeViewItemXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  x:Name="TreeViewItemDesignerWindow">
  <Window.Resources>
    <ControlTemplate x:Key="DesignerTreeChrome"
                     TargetType="TreeViewItem">
      <Canvas x:Name="treeChrome">
        <ContentPresenter ContentSource="Header" />
      </Canvas>
    </ControlTemplate>
    <Style x:Key="DesignerTreeContainer" TargetType="TreeViewItem">
      <Setter Property="Template"
              Value="{StaticResource DesignerTreeChrome}" />
    </Style>
  </Window.Resources>
  <TreeView x:Name="designerTree" DesignId="1"
            ItemContainerStyle="{StaticResource DesignerTreeContainer}">
    <TreeView.Items>
      <TreeViewItem Header="Root" IsExpanded="True">
        <TreeViewItem Header="Child" />
      </TreeViewItem>
    </TreeView.Items>
  </TreeView>
</Window>)XAML";
		DesignerModel::DesignDocument treeViewItemDocument;
		std::wstring treeViewItemError;
		const bool treeViewItemParsed =
			DesignerModel::XamlDocumentParser::FromXaml(treeViewItemXaml,
				treeViewItemDocument, &treeViewItemError);
		DesignerCanvas treeViewItemCanvas(0, 0, 640, 480);
		const bool treeViewItemApplied = treeViewItemParsed
			&& treeViewItemCanvas.ApplyDesignDocument(
				treeViewItemDocument, &treeViewItemError);
		if (treeViewItemApplied)
			treeViewItemCanvas.RestoreSelectionByNames(
				{ L"designerTree" }, L"designerTree", true);
		auto treeViewItemRecord = treeViewItemApplied
			? FindControl(treeViewItemCanvas, L"designerTree") : nullptr;
		auto* designerTree = treeViewItemRecord
			? dynamic_cast<TreeView*>(treeViewItemRecord->ControlInstance) : nullptr;
		auto* designerRoot = designerTree
			? designerTree->ContainerFromIndex(0) : nullptr;
		const bool designerRootApplied = designerRoot
			&& (designerRoot->ApplyTemplate()
				|| cui::framework::TemplateAccess::GetTemplateRoot(
					*designerRoot));
		std::wstring designerRootHeader;
		const bool designerRootPart = designerRoot
			&& designerRoot->FindDeclarativeTemplatePart(L"treeChrome");
		const bool designerRootHeaderReady = designerRoot
			&& designerRoot->GetHeader().TryGetString(designerRootHeader)
			&& designerRootHeader == L"Root";
		const bool designerRootPresent = designerRoot != nullptr;
		const bool designerRootExpanded =
			designerRoot && designerRoot->IsExpanded;
		const bool designerRootHasItems =
			designerRoot && designerRoot->HasItems;
		const bool designerRootHasTemplate =
			designerRoot && designerRoot->GetTemplate();
		const std::wstring designerRootTemplateError = designerRoot
			? static_cast<Control*>(designerRoot)->LastTemplateError()
			: std::wstring{};
		const bool treeViewItemPreviewReady = designerRootApplied
			&& designerRootPart && designerRootHeaderReady
			&& designerRootExpanded && designerRootHasItems;
		DesignerModel::DesignDocument capturedTreeViewItem;
		const bool treeViewItemCaptured = treeViewItemApplied
			&& treeViewItemCanvas.BuildDesignDocument(
				capturedTreeViewItem, &treeViewItemError);
		const auto treeViewItemCanonical = treeViewItemCaptured
			? DesignerModel::XamlDocumentSerializer::ToXaml(capturedTreeViewItem)
			: std::string{};
		auto changedTreeViewItemXaml =
			Convert::Utf8ToUnicode(treeViewItemCanonical);
		const auto treeChromeAt = changedTreeViewItemXaml.find(L"treeChrome");
		if (treeChromeAt != std::wstring::npos)
			changedTreeViewItemXaml.replace(treeChromeAt,
				std::wstring(L"treeChrome").size(), L"treeChromeReloaded");
		(void)treeViewItemCanvas.ResetDocumentHistoryAsSaved();
		const auto beginTreeViewItemEdit =
			treeViewItemCanvas.BeginDocumentEditTransaction(L"EditXaml");
		const bool treeViewItemPreviewed = beginTreeViewItemEdit.Succeeded()
			&& treeChromeAt != std::wstring::npos
			&& treeViewItemCanvas.PreviewXamlDocumentText(
				changedTreeViewItemXaml, &treeViewItemError);
		auto changedTreeViewItemRecord = treeViewItemPreviewed
			? FindControl(treeViewItemCanvas, L"designerTree") : nullptr;
		auto* changedDesignerTree = changedTreeViewItemRecord
			? dynamic_cast<TreeView*>(changedTreeViewItemRecord->ControlInstance)
			: nullptr;
		auto* changedDesignerRoot = changedDesignerTree
			? changedDesignerTree->ContainerFromIndex(0) : nullptr;
		if (changedDesignerRoot) (void)changedDesignerRoot->ApplyTemplate();
		const bool treeViewItemChangedReady = changedDesignerRoot
			&& changedDesignerRoot->FindDeclarativeTemplatePart(
				L"treeChromeReloaded")
			&& treeViewItemCanvas.GetSelectedControl()
			&& treeViewItemCanvas.GetSelectedControl()->Name == L"designerTree";
		const auto commitTreeViewItemEdit =
			treeViewItemCanvas.CommitDocumentEditTransaction();
		const auto undoTreeViewItemEdit = commitTreeViewItemEdit.HasChanges()
			? treeViewItemCanvas.UndoCommand()
			: DesignerDocumentTransactionResult{};
		auto undoTreeViewItemRecord = undoTreeViewItemEdit.HasChanges()
			? FindControl(treeViewItemCanvas, L"designerTree") : nullptr;
		auto* undoDesignerTree = undoTreeViewItemRecord
			? dynamic_cast<TreeView*>(undoTreeViewItemRecord->ControlInstance)
			: nullptr;
		auto* undoDesignerRoot = undoDesignerTree
			? undoDesignerTree->ContainerFromIndex(0) : nullptr;
		if (undoDesignerRoot) (void)undoDesignerRoot->ApplyTemplate();
		const bool treeViewItemUndoReady = undoDesignerRoot
			&& undoDesignerRoot->FindDeclarativeTemplatePart(L"treeChrome");
		const auto redoTreeViewItemEdit = treeViewItemUndoReady
			? treeViewItemCanvas.RedoCommand()
			: DesignerDocumentTransactionResult{};
		auto redoTreeViewItemRecord = redoTreeViewItemEdit.HasChanges()
			? FindControl(treeViewItemCanvas, L"designerTree") : nullptr;
		auto* redoDesignerTree = redoTreeViewItemRecord
			? dynamic_cast<TreeView*>(redoTreeViewItemRecord->ControlInstance)
			: nullptr;
		auto* redoDesignerRoot = redoDesignerTree
			? redoDesignerTree->ContainerFromIndex(0) : nullptr;
		if (redoDesignerRoot) (void)redoDesignerRoot->ApplyTemplate();
		const bool treeViewItemRedoReady = redoDesignerRoot
			&& redoDesignerRoot->FindDeclarativeTemplatePart(
				L"treeChromeReloaded");
		const bool treeViewItemCanonicalTemplate =
			treeViewItemCanonical.find("TargetType=\"TreeViewItem\"")
				!= std::string::npos;
		const bool treeViewItemCanonicalNode =
			treeViewItemCanonical.find("<TreeViewItem")
				!= std::string::npos;
		const bool treeViewItemCanonicalHeader =
			treeViewItemCanonical.find("Header=\"Root\"")
				!= std::string::npos;
		AppendFailure(failures,
			treeViewItemParsed && treeViewItemApplied
			&& treeViewItemPreviewReady && treeViewItemCaptured
			&& treeViewItemCanonical.find("TargetType=\"TreeViewItem\"")
				!= std::string::npos
			&& treeViewItemCanonical.find("<TreeViewItem")
				!= std::string::npos
			&& treeViewItemCanonical.find("Header=\"Root\"")
				!= std::string::npos
			&& treeViewItemPreviewed && treeViewItemChangedReady
			&& commitTreeViewItemEdit.HasChanges()
			&& treeViewItemUndoReady && treeViewItemRedoReady,
			L"TreeViewItem Designer preview did not preserve generated headers, "
			L"hierarchy, selection, canonical XAML, or undo/redo: "
				+ treeViewItemError
				+ L" [parsed=" + SelfTestFlag(treeViewItemParsed)
				+ L", applied=" + SelfTestFlag(treeViewItemApplied)
				+ L", initial=" + SelfTestFlag(treeViewItemPreviewReady)
				+ L", captured=" + SelfTestFlag(treeViewItemCaptured)
				+ L", template=" + SelfTestFlag(
					treeViewItemCanonicalTemplate)
				+ L", node=" + SelfTestFlag(treeViewItemCanonicalNode)
				+ L", header=" + SelfTestFlag(
					treeViewItemCanonicalHeader)
				+ L", root=" + SelfTestFlag(designerRootPresent)
				+ L", applied=" + SelfTestFlag(designerRootApplied)
				+ L", part=" + SelfTestFlag(designerRootPart)
				+ L", rootHeader=" + SelfTestFlag(designerRootHeaderReady)
				+ L", expanded=" + SelfTestFlag(designerRootExpanded)
				+ L", hasItems=" + SelfTestFlag(designerRootHasItems)
				+ L", template=" + SelfTestFlag(designerRootHasTemplate)
				+ L", templateError=" + designerRootTemplateError
				+ L", preview=" + SelfTestFlag(treeViewItemChangedReady)
				+ L", undo=" + SelfTestFlag(treeViewItemUndoReady)
				+ L", redo=" + SelfTestFlag(treeViewItemRedoReady) + L"]");
	}

	{
		const std::string hierarchicalTreeXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  x:Name="HierarchicalTreeDesignerWindow">
  <Window.Resources>
    <DataType x:Key="DesignerFile">
      <DataType.Properties>
        <Property Path="Name" Kind="String" />
      </DataType.Properties>
    </DataType>
    <DataType x:Key="DesignerFolder">
      <DataType.Properties>
        <Property Path="Name" Kind="String" />
        <Property Path="Children" Kind="Object" ObjectType="BindingList"
                  ItemType="DesignerFile" />
      </DataType.Properties>
    </DataType>
    <DataList x:Key="DesignerFolders" ItemType="DesignerFolder">
      <DataRecord Name="Root folder" />
    </DataList>
	    <HierarchicalDataTemplate DataType="DesignerFolder"
	                              ItemsSource="{Binding Children}">
	      <TextBlock x:Name="hierarchyHeader" Text="{Binding Name}"
	             AutomationProperties.Name="Hierarchy header" />
    </HierarchicalDataTemplate>
    <DataTemplate DataType="DesignerFile">
      <TextBlock x:Name="fileHeader" Text="{Binding Name}" />
    </DataTemplate>
  </Window.Resources>
  <TreeView x:Name="hierarchyTree" DesignId="1"
            ItemsSource="{StaticResource DesignerFolders}" />
</Window>)XAML";
		DesignerModel::DesignDocument hierarchicalTreeDocument;
		std::wstring hierarchicalTreeError;
		const bool hierarchicalTreeParsed =
			DesignerModel::XamlDocumentParser::FromXaml(hierarchicalTreeXaml,
				hierarchicalTreeDocument, &hierarchicalTreeError);
		DesignerCanvas hierarchicalTreeCanvas(0, 0, 640, 480);
		const bool hierarchicalTreeApplied = hierarchicalTreeParsed
			&& hierarchicalTreeCanvas.ApplyDesignDocument(
				hierarchicalTreeDocument, &hierarchicalTreeError);
		if (hierarchicalTreeApplied)
			hierarchicalTreeCanvas.RestoreSelectionByNames(
				{ L"hierarchyTree" }, L"hierarchyTree", true);
		auto hierarchyRecord = hierarchicalTreeApplied
			? FindControl(hierarchicalTreeCanvas, L"hierarchyTree") : nullptr;
		auto* hierarchyTree = hierarchyRecord
			? dynamic_cast<TreeView*>(hierarchyRecord->ControlInstance) : nullptr;
		auto* hierarchyRoot = hierarchyTree
			? hierarchyTree->ContainerFromIndex(0) : nullptr;
		auto* hierarchyLabel = hierarchyRoot
			? dynamic_cast<Label*>(hierarchyRoot->GetGeneratedHeaderContent())
			: nullptr;
		const bool hierarchicalTreePreviewReady = hierarchyTree
			&& hierarchyTree->GeneratedItemCount() == 1
			&& hierarchyRoot && !hierarchyRoot->HasItems
			&& hierarchyLabel && hierarchyLabel->Text == L"Root folder";
		DesignerModel::DesignDocument capturedHierarchy;
		const bool hierarchicalTreeCaptured = hierarchicalTreeApplied
			&& hierarchicalTreeCanvas.BuildDesignDocument(
				capturedHierarchy, &hierarchicalTreeError);
		const auto hierarchicalCanonical = hierarchicalTreeCaptured
			? DesignerModel::XamlDocumentSerializer::ToXaml(capturedHierarchy)
			: std::string{};
		auto changedHierarchyXaml =
			Convert::Utf8ToUnicode(hierarchicalCanonical);
		const auto hierarchyHeaderAt = changedHierarchyXaml.find(
			L"Hierarchy header");
		if (hierarchyHeaderAt != std::wstring::npos)
			changedHierarchyXaml.replace(hierarchyHeaderAt,
				std::wstring(L"Hierarchy header").size(),
				L"Hierarchy header reloaded");
		(void)hierarchicalTreeCanvas.ResetDocumentHistoryAsSaved();
		const auto beginHierarchyEdit =
			hierarchicalTreeCanvas.BeginDocumentEditTransaction(L"EditXaml");
		const bool hierarchyPreviewed = beginHierarchyEdit.Succeeded()
			&& hierarchyHeaderAt != std::wstring::npos
			&& hierarchicalTreeCanvas.PreviewXamlDocumentText(
				changedHierarchyXaml, &hierarchicalTreeError);
		auto changedHierarchyRecord = hierarchyPreviewed
			? FindControl(hierarchicalTreeCanvas, L"hierarchyTree") : nullptr;
		auto* changedHierarchyTree = changedHierarchyRecord
			? dynamic_cast<TreeView*>(changedHierarchyRecord->ControlInstance)
			: nullptr;
		auto* changedHierarchyItem = changedHierarchyTree
			? changedHierarchyTree->ContainerFromIndex(0) : nullptr;
		auto* changedHierarchyLabel = changedHierarchyItem
			? dynamic_cast<Label*>(
				changedHierarchyItem->GetGeneratedHeaderContent()) : nullptr;
		const bool hierarchyChangedReady = changedHierarchyLabel
			&& changedHierarchyLabel->AutomationName
				== L"Hierarchy header reloaded"
			&& hierarchicalTreeCanvas.GetSelectedControl()
			&& hierarchicalTreeCanvas.GetSelectedControl()->Name == L"hierarchyTree";
		const auto commitHierarchyEdit =
			hierarchicalTreeCanvas.CommitDocumentEditTransaction();
		const auto undoHierarchyEdit = commitHierarchyEdit.HasChanges()
			? hierarchicalTreeCanvas.UndoCommand()
			: DesignerDocumentTransactionResult{};
		auto undoHierarchyRecord = undoHierarchyEdit.HasChanges()
			? FindControl(hierarchicalTreeCanvas, L"hierarchyTree") : nullptr;
		auto* undoHierarchyTree = undoHierarchyRecord
			? dynamic_cast<TreeView*>(undoHierarchyRecord->ControlInstance) : nullptr;
		auto* undoHierarchyItem = undoHierarchyTree
			? undoHierarchyTree->ContainerFromIndex(0) : nullptr;
		auto* undoHierarchyLabel = undoHierarchyItem
			? dynamic_cast<Label*>(undoHierarchyItem->GetGeneratedHeaderContent())
			: nullptr;
		const bool hierarchyUndoReady = undoHierarchyLabel
			&& undoHierarchyLabel->AutomationName == L"Hierarchy header";
		const auto redoHierarchyEdit = hierarchyUndoReady
			? hierarchicalTreeCanvas.RedoCommand()
			: DesignerDocumentTransactionResult{};
		auto redoHierarchyRecord = redoHierarchyEdit.HasChanges()
			? FindControl(hierarchicalTreeCanvas, L"hierarchyTree") : nullptr;
		auto* redoHierarchyTree = redoHierarchyRecord
			? dynamic_cast<TreeView*>(redoHierarchyRecord->ControlInstance) : nullptr;
		auto* redoHierarchyItem = redoHierarchyTree
			? redoHierarchyTree->ContainerFromIndex(0) : nullptr;
		auto* redoHierarchyLabel = redoHierarchyItem
			? dynamic_cast<Label*>(redoHierarchyItem->GetGeneratedHeaderContent())
			: nullptr;
		const bool hierarchyRedoReady = redoHierarchyLabel
			&& redoHierarchyLabel->AutomationName
				== L"Hierarchy header reloaded";
		AppendFailure(failures,
			hierarchicalTreeParsed && hierarchicalTreeApplied
			&& hierarchicalTreePreviewReady && hierarchicalTreeCaptured
			&& hierarchicalCanonical.find("<HierarchicalDataTemplate")
				!= std::string::npos
			&& hierarchicalCanonical.find("ItemsSource=\"{Binding Children}\"")
				!= std::string::npos
			&& hierarchyPreviewed && hierarchyChangedReady
			&& commitHierarchyEdit.HasChanges()
			&& hierarchyUndoReady && hierarchyRedoReady,
			L"HierarchicalDataTemplate Designer preview did not preserve data "
			L"headers, selection, canonical XAML, or undo/redo: "
				+ hierarchicalTreeError
				+ L" [parsed=" + SelfTestFlag(hierarchicalTreeParsed)
				+ L", applied=" + SelfTestFlag(hierarchicalTreeApplied)
				+ L", initial=" + SelfTestFlag(hierarchicalTreePreviewReady)
				+ L", captured=" + SelfTestFlag(hierarchicalTreeCaptured)
				+ L", preview=" + SelfTestFlag(hierarchyChangedReady)
				+ L", undo=" + SelfTestFlag(hierarchyUndoReady)
				+ L", redo=" + SelfTestFlag(hierarchyRedoReady) + L"]");
	}

	{
		const std::string unusedStateResourceXaml = R"XAML(<Window xmlns="urn:cui"
  xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
  xmlns:local="urn:cui:self-test-states" x:Name="UnusedStateResourceWindow">
  <Window.Resources>
    <Color x:Key="ActiveColor">#FF36A269</Color>
    <Double x:Key="ActiveScale">1.25</Double>
    <Double x:Key="TransitionRadius">18</Double>
    <Double x:Key="StateIncrement">3</Double>
	<Thickness x:Key="ActivePadding">2,3,4,5</Thickness>
    <ComponentDefinition x:Key="local:UnusedStateCard" BaseType="Canvas">
      <ComponentDefinition.Properties>
        <ComponentProperty Name="IsActive" Type="Bool" Default="false" />
      </ComponentDefinition.Properties>
      <ComponentDefinition.Template>
        <Canvas x:Name="root">
          <VisualStateManager.VisualStateGroups>
            <VisualStateGroup x:Name="CommonStates">
              <VisualStateGroup.Transitions>
                <VisualTransition To="Active" GeneratedDuration="0:0:0.100">
                  <VisualTransition.Storyboard>
                    <Storyboard>
                      <DoubleAnimation Storyboard.TargetName="chrome"
                          Storyboard.TargetProperty="(Canvas.Left)"
                          To="{StaticResource TransitionRadius}"
						  Duration="0:0:0.100" RepeatBehavior="0.5x"
						  AutoReverse="true" FillBehavior="Stop" SpeedRatio="2"
						  IsAdditive="true" IsCumulative="true"
						  AccelerationRatio="0.2" DecelerationRatio="0.3" />
                    </Storyboard>
                  </VisualTransition.Storyboard>
                </VisualTransition>
              </VisualStateGroup.Transitions>
              <VisualState x:Name="Normal" />
              <VisualState x:Name="Active">
                <VisualState.StateTriggers>
                  <StateTrigger Property="IsActive" Value="true" />
                </VisualState.StateTriggers>
                <VisualState.Setters>
                  <Setter TargetName="chrome" Property="Background"
                          Value="{StaticResource ActiveColor}" />
                </VisualState.Setters>
                <VisualState.Storyboard>
                  <Storyboard>
					<DoubleAnimationUsingKeyFrames Storyboard.TargetName="chrome"
											 Storyboard.TargetProperty="(UIElement.RenderTransform).(TransformGroup.Children)[0].(ScaleTransform.ScaleX)"
										 RepeatBehavior="Forever" AutoReverse="true"
										 IsAdditive="true" IsCumulative="true"
										 SpeedRatio="0.5">
						<LinearDoubleKeyFrame KeyTime="0:0:0.150"
												  Value="{StaticResource ActiveScale}" />
					</DoubleAnimationUsingKeyFrames>
					<DoubleAnimation Storyboard.TargetName="chrome"
						Storyboard.TargetProperty="(Canvas.Left)"
						By="{StaticResource StateIncrement}"
						IsAdditive="true" IsCumulative="true"
						Duration="0:0:0.100" />
					<ThicknessAnimation Storyboard.TargetName="chrome"
						Storyboard.TargetProperty="Padding"
						To="{StaticResource ActivePadding}"
						Duration="0:0:0.100" />
					<ObjectAnimationUsingKeyFrames Storyboard.TargetName="chrome"
						Storyboard.TargetProperty="BorderBrush" Duration="0:0:0.100">
						<DiscreteObjectKeyFrame KeyTime="0:0:0"
							Value="{StaticResource ActiveColor}" />
					</ObjectAnimationUsingKeyFrames>
                  </Storyboard>
                </VisualState.Storyboard>
              </VisualState>
            </VisualStateGroup>
          </VisualStateManager.VisualStateGroups>
          <Border x:Name="chrome">
            <Control.RenderTransform>
              <ScaleTransform ScaleX="1" ScaleY="1" />
            </Control.RenderTransform>
          </Border>
        </Canvas>
      </ComponentDefinition.Template>
    </ComponentDefinition>
  </Window.Resources>
</Window>)XAML";
		DesignerModel::DesignDocument stateDocument;
		std::wstring stateError;
		const bool stateParsed = DesignerModel::XamlDocumentParser::FromXaml(
			unusedStateResourceXaml, stateDocument, &stateError);
		DesignerCanvas stateCanvas(0, 0, 640, 480);
		const bool stateApplied = stateParsed
			&& stateCanvas.ApplyDesignDocument(stateDocument, &stateError);
		const bool timingPreserved = stateParsed
			&& !stateDocument.Components.empty()
			&& !stateDocument.Components.front().VisualStateGroups.empty()
			&& !stateDocument.Components.front().VisualStateGroups.front()
				.Transitions.empty()
			&& !stateDocument.Components.front().VisualStateGroups.front()
				.Transitions.front().Animations.empty()
			&& stateDocument.Components.front().VisualStateGroups.front()
				.Transitions.front().Animations.front().RepeatCount == 0.5
			&& stateDocument.Components.front().VisualStateGroups.front()
				.Transitions.front().Animations.front().AutoReverse
			&& stateDocument.Components.front().VisualStateGroups.front()
				.Transitions.front().Animations.front().IsAdditive
			&& stateDocument.Components.front().VisualStateGroups.front()
				.Transitions.front().Animations.front().IsCumulative
			&& stateDocument.Components.front().VisualStateGroups.front()
				.Transitions.front().Animations.front().FillBehavior
				== DesignerTimelineFillBehavior::Stop
			&& std::fabs(stateDocument.Components.front().VisualStateGroups.front()
				.Transitions.front().Animations.front().SpeedRatio - 2.0) < 0.0001
			&& std::fabs(stateDocument.Components.front().VisualStateGroups.front()
				.Transitions.front().Animations.front().AccelerationRatio - 0.2)
				< 0.0001
			&& std::fabs(stateDocument.Components.front().VisualStateGroups.front()
				.Transitions.front().Animations.front().DecelerationRatio - 0.3)
				< 0.0001
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States.size() > 1
			&& !stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations.empty()
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations.front().RepeatBehavior
				== DesignerRepeatBehaviorKind::Forever
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations.front().IsAdditive
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations.front().IsCumulative
			&& std::fabs(stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations.front().SpeedRatio - 0.5) < 0.0001
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations.size() == 4
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[1].HasBy
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[1].IsAdditive
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[1].IsCumulative
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[1].ByUsesResource
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[1].ByResourceKey == L"StateIncrement"
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[2].Kind == DesignerAnimationKind::Thickness
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[2].ToUsesResource
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[2].ToResourceKey == L"ActivePadding"
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[3].Kind == DesignerAnimationKind::Object
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[3].KeyFrames.size() == 1
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[3].KeyFrames.front().Kind
					== DesignerKeyFrameKind::Discrete
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[3].KeyFrames.front().UsesResource
			&& stateDocument.Components.front().VisualStateGroups.front()
				.States[1].Animations[3].KeyFrames.front().ResourceKey
					== L"ActiveColor";
		auto invalidResources = stateCanvas.GetDocumentStyleSheet();
		for (auto& resource : invalidResources.Resources)
			if (resource.Key == L"StateIncrement")
			{
				resource.Value.Kind = DesignerStyleValueKind::String;
				resource.Value.Text = L"not-a-number";
			}
		const bool invalidTypeRejected = stateApplied
			&& !stateCanvas.SetDocumentStyleSheet(invalidResources, &stateError);
		const auto preservedResources = stateCanvas.GetDocumentStyleSheet();
		const auto preservedScale = std::find_if(
			preservedResources.Resources.begin(), preservedResources.Resources.end(),
			[](const auto& resource)
			{ return resource.Key == L"StateIncrement"; });
		const bool oldResourcePreserved = preservedResources.Resources.size() == 5
			&& preservedScale != preservedResources.Resources.end()
			&& preservedScale->Value.Kind == DesignerStyleValueKind::Double
			&& preservedScale->Value.Text == L"3";
		AppendFailure(failures,
			stateParsed && stateApplied && timingPreserved && invalidTypeRejected
			&& oldResourcePreserved,
			L"unused component VisualState Setter/Storyboard resources were not validated transactionally: "
				+ stateError);
	}

	{
		const std::string commandTargetXaml = R"XAML(
<Window xmlns="urn:cui"
        xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
        x:Name="CommandCanvasWindow" Width="640" Height="420">
  <StackPanel x:Name="commandRoot">
    <Button x:Name="routeTarget" Content="Target" />
    <Button x:Name="directSource" Content="Run"
            Command="SelfTest.Run"
            CommandTarget="{x:Reference routeTarget}" />
    <Menu x:Name="commandMenu">
      <Menu.Items>
        <MenuItem Header="File" CommandTarget="routeTarget">
          <MenuItem.Items>
            <MenuItem Header="Window"
                      CommandTarget="{x:Reference CommandCanvasWindow}" />
          </MenuItem.Items>
        </MenuItem>
      </Menu.Items>
    </Menu>
  </StackPanel>
</Window>)XAML";
		DesignerModel::DesignDocument source;
		DesignerModel::DesignDocument rebuilt;
		std::wstring error;
		const bool parsed = DesignerModel::XamlDocumentParser::FromXaml(
			commandTargetXaml, source, &error);
		DesignerCanvas canvas(0, 0, 800, 600);
		const bool applied = parsed && canvas.ApplyDesignDocument(source, &error);
		const bool captured = applied && canvas.BuildDesignDocument(rebuilt, &error);
		auto findNode = [](const DesignerModel::DesignDocument& document,
			const wchar_t* name) -> const DesignerModel::DesignNode*
		{
			const auto found = std::find_if(
				document.Nodes.begin(), document.Nodes.end(),
				[name](const auto& node) { return node.Name == name; });
			return found == document.Nodes.end() ? nullptr : &*found;
		};
			auto hasTargets = [&](const DesignerModel::DesignDocument& document,
			const std::wstring& controlTarget)
		{
			const auto* direct = findNode(document, L"directSource");
			bool menuTarget = false;
			bool windowTarget = false;
			for (const auto& node : document.Nodes)
			{
				if (node.Type != UIClass::UI_MenuItem) continue;
				menuTarget = menuTarget
					|| node.Structure.CommandTarget == controlTarget;
				windowTarget = windowTarget
					|| node.Structure.CommandTarget == L"CommandCanvasWindow";
			}
			return direct && direct->Structure.CommandTarget == controlTarget
				&& menuTarget && windowTarget;
		};
		const bool codeInputPreserved = captured
			&& hasTargets(rebuilt, L"routeTarget");

		std::shared_ptr<DesignerControl> target;
		if (applied)
			for (const auto& control : canvas.GetAllControls())
				if (control && control->Name == L"routeTarget")
				{
					target = control;
					break;
				}
		PropertyGridBinder binder;
		binder.SetCanvas(&canvas);
		binder.BindControl(target);
		const auto renameResult = target
			? canvas.ExecuteDocumentEditTransaction(
				L"SelfTestRenameCommandTarget", [&](std::wstring& editError)
				{
					const auto edited = binder.ApplyControlPropertyValue(
						L"Name", L"routeTargetRenamed");
					if (edited) return true;
					editError = edited.Error;
					return false;
				})
			: DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"routeTarget missing");
		DesignerModel::DesignDocument renamed;
		const bool renamedCoherently = renameResult
			&& canvas.BuildDesignDocument(renamed, &error)
			&& hasTargets(renamed, L"routeTargetRenamed");
		const bool undoPreserved = renamedCoherently && canvas.UndoCommand()
			&& canvas.BuildDesignDocument(rebuilt, &error)
			&& hasTargets(rebuilt, L"routeTarget");
		canvas.RestoreSelectionByNames(
			{ L"routeTarget" }, L"routeTarget", false);
		const auto deleteResult = canvas.DeleteSelectedControl(false);
		const bool danglingDeleteRejected = !deleteResult
			&& canvas.BuildDesignDocument(rebuilt, &error)
			&& findNode(rebuilt, L"routeTarget")
			&& hasTargets(rebuilt, L"routeTarget");
		AppendFailure(failures,
			parsed && applied && captured
			&& hasTargets(rebuilt, L"routeTarget")
			&& codeInputPreserved && renamedCoherently
			&& undoPreserved && danglingDeleteRejected,
			L"DesignerCanvas lost or orphaned authored ICommandSource.CommandTarget: "
				+ error
				+ L" [parsed=" + std::to_wstring(parsed)
				+ L", applied=" + std::to_wstring(applied)
				+ L", captured=" + std::to_wstring(captured)
				+ L", initialTargets="
				+ std::to_wstring(hasTargets(rebuilt, L"routeTarget"))
				+ L", codeInput=" + std::to_wstring(codeInputPreserved)
				+ L", renamed=" + std::to_wstring(renamedCoherently)
				+ L", undo=" + std::to_wstring(undoPreserved)
				+ L", deleteRejected="
				+ std::to_wstring(danglingDeleteRejected) + L"]");
	}

	// Install the full framework theme only after the document-operation suite.
	// This mirrors product startup while keeping unrelated headless canvas tests
	// independent from Designer chrome template lifetime.
	{
		Designer themedDesigner;
		themedDesigner.InitializeComponents();
		auto* propertyModeButton = dynamic_cast<Button*>(
			FindDescendantByAutomationName(
				themedDesigner._propertyGrid, L"显示属性"));
		std::wstring themeError;
		auto* themeRoot = themedDesigner.GetVisualContent();
		const bool themeApplied = themeRoot
			&& themedDesigner._btnNew
			&& themedDesigner._btnToolboxView && propertyModeButton
			&& CuiGeneratedFrameworkTheme::Apply(
				*themeRoot, true, &themeError);
		DesignerModel::DesignDocument themedSnapshot;
		std::wstring snapshotError;
		const bool snapshotCaptured = themeApplied
			&& themedDesigner._canvas
			&& themedDesigner._canvas->BuildDesignDocument(
				themedSnapshot, &snapshotError);
		const bool designerChromeThemed = themeApplied
			&& snapshotCaptured
			&& themedDesigner._btnNew->GetTemplate()
			&& themedDesigner._btnNew->HorizontalContentAlignment
				== HorizontalAlignment::Center
			&& themedDesigner._btnNew->VerticalContentAlignment
				== VerticalAlignment::Center
			&& themedDesigner._btnToolboxView
			&& themedDesigner._btnToolboxView->GetTemplate()
			&& propertyModeButton && propertyModeButton->GetTemplate();
		AppendFailure(failures, designerChromeThemed,
			L"designer chrome: Generic.xaml button alignment/states were not installed: "
				+ themeError
				+ (snapshotError.empty()
					? std::wstring{}
					: L"; themed document snapshot: " + snapshotError));
	}

	if (failures.empty())
	{
		report = L"Designer interaction self-test passed.";
		return true;
	}

	report.clear();
	for (const auto& failure : failures)
	{
		if (!report.empty()) report += L"\r\n";
		report += L"- " + failure;
	}
	return false;
}
