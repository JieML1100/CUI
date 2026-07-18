#include "DesignerSelfTest.h"
#include "CodeGenerator.h"
#include "CodeBehindExportDialog.h"
#include "Designer.h"
#include "DesignerCanvas.h"
#include "DesignerControlCatalog.h"
#include "DesignerDataContextSchemaUtils.h"
#include "DesignerPreviewPlugin.h"
#include "DesignerCore/Commands/ControlPlacementCommand.h"
#include "DesignerCore/Commands/ControlStructureCommand.h"
#include "DesignerCore/Commands/ControlOwnedCollectionCommand.h"
#include "DesignerCore/Commands/DocumentSnapshotCommand.h"
#include "DesignerCore/Commands/EventHandlerCommand.h"
#include "DesignerStructureEdit.h"
#include "DesignerModel/AtomicFile.h"
#include "DesignerModel/DesignDocument.h"
#include "DesignerModel/DesignCodeGenerationService.h"
#include "DesignerModel/DesignDocumentEventIndex.h"
#include "DesignerModel/DesignDocumentCodeGenInputBuilder.h"
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
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/Form.h"
#include "../CUI/include/GridView.h"
#include "../CUI/include/ChartView.h"
#include "../CUI/include/FilterBar.h"
#include "../CUI/include/KpiCard.h"
#include "../CUI/include/Menu.h"
#include "../CUI/include/NavigationView.h"
#include "../CUI/include/PictureBox.h"
#include "../CUI/include/ProgressBar.h"
#include "../CUI/include/ReportView.h"
#include "../CUI/include/SplitContainer.h"
#include "../CUI/include/StatusBar.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/ToolBar.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/WebBrowser.h"
#include "../CUI/include/Layout/GridPanel.h"
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
	CuiDesignerPreviewStatusV1 CUI_DESIGNER_PREVIEW_CALL FakePreviewCreate(
		void*, CuiDesignerUtf8ViewV1, CuiDesignerUtf8ViewV1, void** session)
	{
		if (session) *session = reinterpret_cast<void*>(1);
		return CUI_DESIGNER_PREVIEW_OK_V1;
	}
	void CUI_DESIGNER_PREVIEW_CALL FakePreviewDestroy(void*, void*) {}
	CuiDesignerPreviewStatusV1 CUI_DESIGNER_PREVIEW_CALL FakePreviewSetValue(
		void*, void*, CuiDesignerUtf8ViewV1,
		const CuiDesignerPreviewValueV1*)
	{
		return CUI_DESIGNER_PREVIEW_OK_V1;
	}
	CuiDesignerPreviewStatusV1 CUI_DESIGNER_PREVIEW_CALL FakePreviewRender(
		void*, void*, const CuiDesignerPreviewFrameInputV1*,
		CuiDesignerPreviewFrameV1*)
	{
		return CUI_DESIGNER_PREVIEW_OK_V1;
	}
	void CUI_DESIGNER_PREVIEW_CALL FakePreviewShutdown(void*) {}

	class SelfTestStatusBadge final : public Button
	{
	public:
		SelfTestStatusBadge(int x, int y)
			: Button(L"Preview", x, y, 150, 30) {}
	};

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

	std::wstring ControlText(
		DesignerCanvas& canvas,
		const std::wstring& name)
	{
		auto control = FindControl(canvas, name);
		return control && control->ControlInstance
			? control->ControlInstance->Text : std::wstring{};
	}

	Control* FindDescendantByAccessibleName(
		Control* root,
		const std::wstring& accessibleName)
	{
		if (!root) return nullptr;
		if (root->AccessibleName == accessibleName) return root;
		for (int index = 0; index < root->Count; ++index)
			if (auto* found = FindDescendantByAccessibleName(
				root->operator[](index), accessibleName))
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
		for (auto& node : document.Nodes)
		{
			if (!node.Props.is_object()) continue;
			for (const auto* name : {
				"backColor", "foreColor", "borderColor", "bolderColor" })
			{
				if (!node.Props.contains(name)
					|| !node.Props[name].is_object()) continue;
				auto& color = node.Props[name];
				for (const auto* component : { "r", "g", "b", "a" })
				{
					if (!color.contains(component)
						|| !color[component].is_number()) continue;
					const auto runtimeValue = static_cast<float>(
						color[component].get<double>());
					color[component] = static_cast<double>(runtimeValue);
				}
			}
		}
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
		if (left.Form != right.Form) add(L"form");
		if (left.CodeBehind != right.CodeBehind) add(L"codeBehind");
		if (left.DataContextSchema != right.DataContextSchema) add(L"dataContext");
		if (left.StyleSheet != right.StyleSheet) add(L"styleSheet");
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
			if (a.CustomType != b.CustomType) field(L"customType");
			if (a.CustomEvents != b.CustomEvents) field(L"customEvents");
			if (a.Order != b.Order) field(L"order");
			if (a.Props != b.Props)
			{
				std::wstring keys;
				if (a.Props.is_object() && b.Props.is_object())
				{
					std::set<std::string> names;
					for (const auto& [name, ignored] : a.Props.ObjectItems())
					{
						(void)ignored;
						names.insert(name);
					}
					for (const auto& [name, ignored] : b.Props.ObjectItems())
					{
						(void)ignored;
						names.insert(name);
					}
					for (const auto& name : names)
					{
						const bool aHas = a.Props.contains(name);
						const bool bHas = b.Props.contains(name);
						if (aHas && bHas && a.Props[name] == b.Props[name]) continue;
						if (!keys.empty()) keys += L"|";
						keys.append(name.begin(), name.end());
					}
				}
				field((L"props{" + keys + L"}").c_str());
			}
			if (a.Extra != b.Extra) field(L"extra");
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
	CuiDesignerPreviewPluginV1 previewTable{};
	previewTable.StructSize = sizeof(previewTable);
	previewTable.AbiVersion = CUI_DESIGNER_PREVIEW_ABI_V1;
	previewTable.CreateSession = &FakePreviewCreate;
	previewTable.DestroySession = &FakePreviewDestroy;
	previewTable.SetValue = &FakePreviewSetValue;
	previewTable.Render = &FakePreviewRender;
	previewTable.Shutdown = &FakePreviewShutdown;
	std::wstring previewContractError;
	AppendFailure(failures,
		DesignerPreviewPluginContract::ValidatePluginTable(
			&previewTable, &previewContractError),
		L"preview plugin contract: valid V1 function table was rejected");
	previewTable.AbiVersion = CUI_DESIGNER_PREVIEW_ABI_V1 + 1;
	AppendFailure(failures,
		!DesignerPreviewPluginContract::ValidatePluginTable(
			&previewTable, &previewContractError),
		L"preview plugin contract: incompatible ABI was accepted");
	previewTable.AbiVersion = CUI_DESIGNER_PREVIEW_ABI_V1;

	const char previewText[] = "Badge";
	CuiDesignerPreviewPrimitiveV1 previewPrimitive{};
	previewPrimitive.StructSize = sizeof(previewPrimitive);
	previewPrimitive.Kind = CUI_DESIGNER_PREVIEW_PRIMITIVE_TEXT_V1;
	previewPrimitive.Width = 100.0f;
	previewPrimitive.Height = 24.0f;
	previewPrimitive.FillArgb32 = 0xFF102030u;
	previewPrimitive.Text = { previewText, 5 };
	previewPrimitive.FontSize = 13.0f;
	CuiDesignerPreviewFrameV1 previewFrame{};
	previewFrame.StructSize = sizeof(previewFrame);
	previewFrame.Primitives = &previewPrimitive;
	previewFrame.PrimitiveCount = 1;
	std::vector<DesignerPreviewPrimitive> copiedPreviewFrame;
	AppendFailure(failures,
		DesignerPreviewPluginContract::CopyFrame(
			previewFrame, copiedPreviewFrame, &previewContractError)
		&& copiedPreviewFrame.size() == 1
		&& copiedPreviewFrame.front().Text == L"Badge"
		&& copiedPreviewFrame.front().FillArgb32 == 0xFF102030u,
		L"preview plugin contract: valid plugin-owned frame was not copied");
	const auto retainedPreviewFrame = copiedPreviewFrame;
	previewPrimitive.Width = std::numeric_limits<float>::quiet_NaN();
	AppendFailure(failures,
		!DesignerPreviewPluginContract::CopyFrame(
			previewFrame, copiedPreviewFrame, &previewContractError)
		&& copiedPreviewFrame.size() == retainedPreviewFrame.size()
		&& copiedPreviewFrame.front().Text == retainedPreviewFrame.front().Text,
		L"preview plugin contract: invalid frame was not rejected transactionally");

	ToolBox toolBox(0, 0, 260, 640);
	AppendFailure(failures,
		toolBox.GetItemCount() == ControlRegistry::GetAvailableControls().size()
		&& toolBox.GetVisibleItemCount() == toolBox.GetItemCount()
		&& toolBox.GetVisibleCategoryCount() == 7,
		L"toolbox: controls were not grouped into the expected native categories");
	toolBox.SetFilterText(L"媒体");
	AppendFailure(failures,
		toolBox.GetVisibleItemCount() == 2
		&& toolBox.GetVisibleCategoryCount() == 1,
		L"toolbox: category-aware filtering did not isolate media controls");
	toolBox.SetFilterText(L"Button");
	AppendFailure(failures,
		toolBox.GetVisibleItemCount() == 1
		&& toolBox.GetVisibleCategoryCount() == 1,
		L"toolbox: type-name filtering did not isolate Button");
	toolBox.SetFilterText(L"不存在的控件");
	AppendFailure(failures,
		toolBox.GetVisibleItemCount() == 0
			&& toolBox.GetVisibleCategoryCount() == 0,
		L"toolbox: empty filter results retained stale visible rows");
	{
		const std::wstring source =
			L"<Form xmlns=\"urn:cui\" Name=\"MainForm\">"
			L"<Button Name=\"existingButton\" Text=\"Existing\"/>"
			L"</Form>";
		XamlEditorDialog recoveryEditor(nullptr, source);
		const std::wstring invalidDraft =
			L"<Form><Button Visibility=\"Vanished\"/></Form>";
		recoveryEditor._editor->SelectAll();
		recoveryEditor._editor->InsertText(invalidDraft);
		recoveryEditor.RefreshRestorePreviewState();
		const bool recoveryEnabled = recoveryEditor._restorePreview
			&& recoveryEditor._restorePreview->Enable;
		recoveryEditor.RestoreLastValidPreview();
		const bool recoveryRestored = recoveryEditor._editor->Text == source
			&& recoveryEditor._restorePreview
			&& !recoveryEditor._restorePreview->Enable
			&& recoveryEditor._editor->CanUndo();
		recoveryEditor._editor->Undo();
		const bool recoveryUndo = recoveryEditor._editor->Text == invalidDraft
			&& recoveryEditor._restorePreview->Enable;

		DesignerCanvas previewCanvas(0, 0, 900, 640);
		std::wstring initial;
		std::wstring previewError;
		const bool sourceBuilt =
			previewCanvas.BuildXamlDocumentText(initial, &previewError);
		const auto transaction =
			previewCanvas.BeginDocumentEditTransaction(L"EditXaml");
		XamlEditorDialog previewEditor(&previewCanvas, initial);
		auto valid = initial;
		const auto formEnd = valid.rfind(L"</Form>");
		if (formEnd != std::wstring::npos)
			valid.insert(formEnd,
				L"  <Button Name=\"liveButton\" Text=\"Live\" />\r\n");
		else
		{
			const auto close = valid.rfind(L"/>");
			if (close != std::wstring::npos)
				valid.replace(close, 2,
					L">\r\n  <Button Name=\"liveButton\" Text=\"Live\" />"
					L"\r\n</Form>");
		}
		previewEditor._editor->ReplaceAllTextAndSelect(
			valid, static_cast<int>(valid.size()), 0);
		const bool validSynchronized = previewEditor.ValidateAndPreview()
			&& previewEditor._lastValidXaml == valid
			&& FindControl(previewCanvas, L"liveButton");
		const auto validControlCount = previewCanvas.GetAllControls().size();
		auto invalid = valid;
		const auto visibility = invalid.find(L"Text=\"Live\"");
		if (visibility != std::wstring::npos)
			invalid.replace(
				visibility, std::wstring(L"Text=\"Live\"").size(),
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
				&& validRecovered && rollbackRestored,
			L"XAML editor thin shell: recovery, validation, synchronization, or rollback failed"
				+ (previewError.empty()
					? std::wstring{} : L": " + previewError));
	}

	// The XAML editor is modal, so its Designer owner is disabled while live
	// previews invalidate the canvas. Disabled owners must still consume paint
	// requests; otherwise BeginPaint validates the region and the applied XAML
	// remains visually stale after the editor closes.
	bool disabledOwnerRepainted = false;
	{
		Form previewOwner(
			L"Designer live XAML repaint probe",
			POINT{ -30000, -30000 }, SIZE{ 220, 140 });
		previewOwner.VisibleHead = false;
		auto* previewSurface = previewOwner.AddControl(
			new Panel(0, 0, 200, 120));
		size_t repaintCount = 0;
		previewOwner.OnPaint +=
			[&](Form*) { ++repaintCount; };
		previewOwner.Show();
		(void)::UpdateWindow(previewOwner.Handle);
		repaintCount = 0;
		(void)::EnableWindow(previewOwner.Handle, FALSE);
		previewSurface->BackColor = Colors::DodgerBlue;
		previewSurface->InvalidateVisual();
		(void)::UpdateWindow(previewOwner.Handle);
		disabledOwnerRepainted = repaintCount > 0;
		(void)::EnableWindow(previewOwner.Handle, TRUE);
		(void)::DestroyWindow(previewOwner.Handle);
	}
	AppendFailure(failures, disabledOwnerRepainted,
		L"live XAML: a disabled modal owner discarded the canvas repaint");

	auto catalogDescriptors = DesignerControlCatalog::BuiltInDescriptors();
	const auto builtInDescriptorCount = catalogDescriptors.size();
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
	const auto* panelDropDescriptor = findBuiltInDescriptor(UIClass::UI_Panel);
	const auto* splitDropDescriptor = findBuiltInDescriptor(
		UIClass::UI_SplitContainer);
	AppendFailure(failures,
		buttonDropDescriptor && panelDropDescriptor && splitDropDescriptor,
		L"toolbox drag: required built-in descriptors were not available");
	if (buttonDropDescriptor && panelDropDescriptor && splitDropDescriptor)
	{
		DesignerCanvas dropPreviewCanvas(0, 0, 900, 640);
		std::wstring dropTarget;
		const bool rootPreview = dropPreviewCanvas.UpdateControlDropPreview(
			*buttonDropDescriptor, POINT{ 100, 100 }, &dropTarget);
		const auto rootGhost = dropPreviewCanvas.GetControlDropPreviewRect();
		AppendFailure(failures,
			rootPreview && dropPreviewCanvas.HasControlDropPreview()
			&& dropTarget == L"窗体根"
			&& rootGhost.right - rootGhost.left
				== buttonDropDescriptor->DefaultSize.cx
			&& rootGhost.bottom - rootGhost.top
				== buttonDropDescriptor->DefaultSize.cy,
			L"toolbox drag: root preview did not expose the default-size ghost");
		dropPreviewCanvas.ClearControlDropPreview();
		AppendFailure(failures,
			!dropPreviewCanvas.HasControlDropPreview(),
			L"toolbox drag: clearing a preview retained stale view state");

		const auto panelAdd = dropPreviewCanvas.AddControlToCanvas(
			*panelDropDescriptor, POINT{ 300, 240 });
		auto panelWrapper = dropPreviewCanvas.GetSelectedControl();
		const POINT panelPoint = panelWrapper && panelWrapper->ControlInstance
			? POINT{
				panelWrapper->ControlInstance->AbsLocation.x
					- dropPreviewCanvas.AbsLocation.x + 80,
				panelWrapper->ControlInstance->AbsLocation.y
					- dropPreviewCanvas.AbsLocation.y + 80 }
			: POINT{ 0, 0 };
		const bool panelPreview = panelWrapper
			&& dropPreviewCanvas.UpdateControlDropPreview(
				*buttonDropDescriptor, panelPoint, &dropTarget);
		const auto buttonAdd = panelPreview
			? dropPreviewCanvas.AddControlToCanvas(
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
			&& dropPreviewCanvas.GetAllControls().size() == 1,
			L"toolbox drag: container preview, placement, or single-step undo failed");

		DesignerCanvas splitPreviewCanvas(0, 0, 900, 640);
		const auto splitAdd = splitPreviewCanvas.AddControlToCanvas(
			*splitDropDescriptor, POINT{ 360, 260 });
		auto splitWrapper = splitPreviewCanvas.GetSelectedControl();
		auto* split = splitWrapper
			? dynamic_cast<SplitContainer*>(splitWrapper->ControlInstance)
			: nullptr;
		auto toCanvasPoint = [&](Control* control) -> POINT
		{
			if (!control) return { 0, 0 };
			return {
				control->AbsLocation.x - splitPreviewCanvas.AbsLocation.x
					+ (std::max)(1, control->Width / 2),
				control->AbsLocation.y - splitPreviewCanvas.AbsLocation.y
					+ (std::max)(1, control->Height / 2) };
		};
		const bool firstPreview = split
			&& splitPreviewCanvas.UpdateControlDropPreview(
				*buttonDropDescriptor, toCanvasPoint(split->FirstPanel()),
				&dropTarget)
			&& dropTarget.find(L"First") != std::wstring::npos;
		const bool secondPreview = split
			&& splitPreviewCanvas.UpdateControlDropPreview(
				*buttonDropDescriptor, toCanvasPoint(split->SecondPanel()),
				&dropTarget)
			&& dropTarget.find(L"Second") != std::wstring::npos;
		AppendFailure(failures,
			splitAdd.HasChanges() && firstPreview && secondPreview,
			L"toolbox drag: SplitContainer First/Second target preview was ambiguous");

		Designer dragDesigner;
		dragDesigner.InitializeComponents();
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
			&& dragDesigner._canvas->GetAllControls().empty();
		dragDesigner.UpdateToolBoxDrag(formDropPoint.x, formDropPoint.y);
		const bool activePreview = dragDesigner._toolBoxDragging
			&& dragDesigner._toolBoxDropAccepted
			&& dragDesigner._canvas->HasControlDropPreview();
		dragDesigner.EndToolBoxDrag(formDropPoint.x, formDropPoint.y);
		auto draggedControl = dragDesigner._canvas->GetSelectedControl();
		const bool dropCommitted = !dragDesigner._toolBoxPointerDown
			&& !dragDesigner._canvas->HasControlDropPreview()
			&& dragDesigner._canvas->GetAllControls().size() == 1
			&& draggedControl && draggedControl->Type == UIClass::UI_Button
			&& dragDesigner._canvas->GetUndoCommandLabel() == L"AddControl";
		const bool dropUndone = dragDesigner._canvas->UndoCommand().HasChanges()
			&& dragDesigner._canvas->GetAllControls().empty();
		AppendFailure(failures,
			thresholdPreserved && activePreview && dropCommitted && dropUndone,
			L"toolbox drag: Designer threshold, captured drop, or undo lifecycle failed");

		DesignerCanvas tabOrderModeCanvas(0, 0, 900, 640);
		const auto tabModeAdd = tabOrderModeCanvas.AddControlToCanvas(
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
			tabModePoint = {
				tabModeButton->ControlInstance->AbsLocation.x
					- tabOrderModeCanvas.AbsLocation.x
					+ (std::max)(1, tabModeButton->ControlInstance->Width / 2),
				tabModeButton->ControlInstance->AbsLocation.y
					- tabOrderModeCanvas.AbsLocation.y
					+ (std::max)(1, tabModeButton->ControlInstance->Height / 2) };
			(void)tabOrderModeCanvas.ProcessMessage(
				WM_LBUTTONDOWN, MK_LBUTTON, 0,
				tabModePoint.x, tabModePoint.y);
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
		(void)tabOrderModeCanvas.ProcessMessage(
			WM_KEYDOWN, VK_ESCAPE, 0, 0, 0);
		AppendFailure(failures,
			tabModeAdd.HasChanges() && tabModeReset
			&& tabModeEntered && tabModeAssigned && tabModeUndone
			&& !tabOrderModeCanvas.IsTabOrderMode()
			&& !tabOrderModeCanvas.IsDocumentDirty(),
			L"tab order: view-only mode, click assignment, Escape, or delta Undo failed");

		DesignerCanvas autoTabCanvas(0, 0, 900, 640);
		(void)autoTabCanvas.AddControlToCanvas(
			*buttonDropDescriptor, POINT{ 330, 310 });
		auto lowerButton = autoTabCanvas.GetSelectedControl();
		(void)autoTabCanvas.AddControlToCanvas(
			*buttonDropDescriptor, POINT{ 140, 130 });
		auto upperButton = autoTabCanvas.GetSelectedControl();
		(void)autoTabCanvas.AddControlToCanvas(
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
	const std::string catalogXml = R"xml(<?xml version="1.0" encoding="utf-8"?>
<cuiControlCatalog schema="cui.designer.controls" version="1">
  <control name="StatusBadge" displayName="Status badge" category="Business"
    baseType="Button" xamlPrefix="sample" xamlName="StatusBadge"
    xamlNamespace="urn:cui:selftest" cppType="Acme.Controls.StatusBadge"
    header="Controls/StatusBadge.h" constructor="Bounds"
	width="150" height="30" container="false">
    <property name="Severity" displayName="Severity" category="Appearance"
      categoryOrder="200" order="10" kind="Int64" default="1"
      editor="Choice" minimum="0" maximum="2" bindable="true">
      <choice displayName="Info" value="0" />
      <choice displayName="Normal" value="1" />
      <choice displayName="Warning" value="2" />
    </property>
    <event name="OnSeverityInvoked" displayName="Severity invoked"
      field="OnSeverityInvoked" category="Action" signature="SenderInt"
      order="5" default="true" />
  </control>
</cuiControlCatalog>)xml";
	std::wstring catalogError;
	AppendFailure(failures,
		DesignerControlCatalog::AppendFromXml(
			catalogDescriptors, catalogXml, &catalogError)
		&& catalogDescriptors.size() == builtInDescriptorCount + 1
		&& catalogDescriptors.back().CustomType.CppType
			== L"Acme::Controls::StatusBadge"
		&& catalogDescriptors.back().DefaultSize.cx == 150
		&& catalogDescriptors.back().Category == L"Business"
		&& catalogDescriptors.back().CustomProperties.size() == 1
		&& catalogDescriptors.back().CustomProperties.front().Name == L"Severity"
		&& catalogDescriptors.back().CustomProperties.front().Choices.size() == 3
		&& catalogDescriptors.back().CustomEvents.size() == 1
		&& catalogDescriptors.back().CustomEvents.front().IsDefault,
		L"custom control catalog: valid manifest did not produce a canonical descriptor");
	const auto validCatalogSize = catalogDescriptors.size();
	AppendFailure(failures,
		!DesignerControlCatalog::AppendFromXml(
			catalogDescriptors, catalogXml, &catalogError)
		&& catalogDescriptors.size() == validCatalogSize,
		L"custom control catalog: duplicate append was not rejected transactionally");
	const std::string conflictingCatalogXml = R"xml(
<cuiControlCatalog schema="cui.designer.controls" version="1">
  <control name="OtherBadge" baseType="Button" xamlPrefix="sample"
    xamlName="OtherBadge" xamlNamespace="urn:cui:other"
    cppType="Acme.Controls.OtherBadge" header="Controls/OtherBadge.h" />
</cuiControlCatalog>)xml";
	AppendFailure(failures,
		!DesignerControlCatalog::AppendFromXml(
			catalogDescriptors, conflictingCatalogXml, &catalogError)
		&& catalogDescriptors.size() == validCatalogSize,
		L"custom control catalog: conflicting prefix was not rejected transactionally");
	const std::string nestedCatalogXml = R"xml(
<cuiControlCatalog schema="cui.designer.controls" version="1">
  <control name="NestedBadge" baseType="Button" xamlPrefix="other"
    xamlName="NestedBadge" xamlNamespace="urn:cui:nested"
    cppType="Acme.Controls.NestedBadge" header="Controls/NestedBadge.h">
    <unknown />
  </control>
</cuiControlCatalog>)xml";
	AppendFailure(failures,
		!DesignerControlCatalog::AppendFromXml(
			catalogDescriptors, nestedCatalogXml, &catalogError)
		&& catalogDescriptors.size() == validCatalogSize,
		L"custom control catalog: nested content was not rejected transactionally");
	const std::string invalidPropertyCatalogXml = R"xml(
<cuiControlCatalog schema="cui.designer.controls" version="1">
  <control name="BadBadge" baseType="Button" xamlPrefix="bad"
    xamlName="BadBadge" xamlNamespace="urn:cui:bad"
    cppType="Acme.Controls.BadBadge" header="Controls/BadBadge.h">
    <property name="Text" kind="String" default="duplicate" />
  </control>
</cuiControlCatalog>)xml";
	AppendFailure(failures,
		!DesignerControlCatalog::AppendFromXml(
			catalogDescriptors, invalidPropertyCatalogXml, &catalogError)
		&& catalogDescriptors.size() == validCatalogSize,
		L"custom control catalog: base-property collision was not rejected transactionally");
	const std::string invalidChoiceDefaultCatalogXml = R"xml(
<cuiControlCatalog schema="cui.designer.controls" version="1">
  <control name="BadChoiceBadge" baseType="Button" xamlPrefix="bad"
    xamlName="BadChoiceBadge" xamlNamespace="urn:cui:bad"
    cppType="Acme.Controls.BadChoiceBadge" header="Controls/BadChoiceBadge.h">
    <property name="Severity" kind="Int" default="3" editor="Choice">
      <choice displayName="Normal" value="1" />
      <choice displayName="Warning" value="2" />
    </property>
  </control>
</cuiControlCatalog>)xml";
	AppendFailure(failures,
		!DesignerControlCatalog::AppendFromXml(
			catalogDescriptors, invalidChoiceDefaultCatalogXml, &catalogError)
		&& catalogDescriptors.size() == validCatalogSize,
		L"custom control catalog: choice default outside the set was not rejected");
	const std::string invalidEventSignatureCatalogXml = R"xml(
<cuiControlCatalog schema="cui.designer.controls" version="1">
  <control name="BadEventBadge" baseType="Button" xamlPrefix="bad"
    xamlName="BadEventBadge" xamlNamespace="urn:cui:bad"
    cppType="Acme.Controls.BadEventBadge" header="Controls/BadEventBadge.h">
    <event name="OnInjected" field="OnInjected"
      signature="void(Acme::Payload*)" />
  </control>
</cuiControlCatalog>)xml";
	AppendFailure(failures,
		!DesignerControlCatalog::AppendFromXml(
			catalogDescriptors, invalidEventSignatureCatalogXml, &catalogError)
		&& catalogDescriptors.size() == validCatalogSize,
		L"custom control catalog: arbitrary C++ event signature was accepted");
	const std::string collidingEventCatalogXml = R"xml(
<cuiControlCatalog schema="cui.designer.controls" version="1">
  <control name="CollidingEventBadge" baseType="Button" xamlPrefix="bad"
    xamlName="CollidingEventBadge" xamlNamespace="urn:cui:bad"
    cppType="Acme.Controls.CollidingEventBadge" header="Controls/CollidingEventBadge.h">
    <event name="OnMouseClick" field="OnMouseClick" signature="Sender" />
  </control>
</cuiControlCatalog>)xml";
	AppendFailure(failures,
		!DesignerControlCatalog::AppendFromXml(
			catalogDescriptors, collidingEventCatalogXml, &catalogError)
		&& catalogDescriptors.size() == validCatalogSize,
		L"custom control catalog: inherited event collision was accepted");
	const std::string dtdCatalogXml = R"xml(<!DOCTYPE cuiControlCatalog [
<!ENTITY name "Injected">]>
<cuiControlCatalog schema="cui.designer.controls" version="1">
  <control name="&name;" baseType="Button" xamlPrefix="other"
    xamlName="Injected" xamlNamespace="urn:cui:injected"
    cppType="Acme.Controls.Injected" header="Controls/Injected.h" />
</cuiControlCatalog>)xml";
	AppendFailure(failures,
		!DesignerControlCatalog::AppendFromXml(
			catalogDescriptors, dtdCatalogXml, &catalogError)
		&& catalogDescriptors.size() == validCatalogSize,
		L"custom control catalog: DTD input was not rejected transactionally");
	const std::string oversizedCatalog(4U * 1024U * 1024U + 1U, ' ');
	AppendFailure(failures,
		!DesignerControlCatalog::AppendFromXml(
			catalogDescriptors, oversizedCatalog, &catalogError)
		&& catalogDescriptors.size() == validCatalogSize,
		L"custom control catalog: oversized input was not rejected transactionally");
	AppendFailure(failures,
		DesignerControlCatalog::AttachPreviewFactory(
			catalogDescriptors, L"urn:cui:selftest", L"StatusBadge",
			[](int x, int y)
			{
				return std::make_unique<SelfTestStatusBadge>(x, y);
			}, &catalogError),
		L"custom control catalog: process-local preview factory was not attached");
	const auto customDescriptor = catalogDescriptors.back();
	AppendFailure(failures,
		customDescriptor.PreviewFactory
		&& customDescriptor.PreviewFactory(1, 2)->Type() == UIClass::UI_Button,
		L"custom control catalog: attached preview factory was not usable");

	DesignerControlDescriptor missingPreviewDescriptor = customDescriptor;
	missingPreviewDescriptor.PreviewFactory = {};
	AppendFailure(failures,
		missingPreviewDescriptor.IsValid(),
		L"custom control catalog: portable proxy descriptor unexpectedly requires third-party code");

	ToolBox customToolBox(
		0, 0, 260, 320,
		std::vector<DesignerControlDescriptor>{ customDescriptor });
	const auto preview = customDescriptor.PreviewFactory(0, 0);
	AppendFailure(failures, preview && preview->Type() == UIClass::UI_Button,
		L"custom control catalog: preview factory returned the wrong base type");
	DesignerControlDescriptor mismatchedPreviewDescriptor = customDescriptor;
	mismatchedPreviewDescriptor.PreviewFactory = [](int x, int y)
	{
		return std::make_unique<Label>(L"Wrong type", x, y);
	};
	DesignerCanvas mismatchedPreviewCanvas(0, 0, 400, 300);
	AppendFailure(failures,
		!mismatchedPreviewCanvas.AddControlToCanvas(
			mismatchedPreviewDescriptor, POINT{ 40, 40 })
		&& mismatchedPreviewCanvas.GetAllControls().empty(),
		L"custom control catalog: mismatched preview Type was not rejected");
	AppendFailure(failures,
		customToolBox.GetItemCount() == 1
		&& customToolBox.GetVisibleItemCount() == 1
		&& customToolBox.GetVisibleCategoryCount() == 1,
		L"custom toolbox: descriptor/category was not materialized");
	customToolBox.SetFilterText(L"StatusBadge");
	AppendFailure(failures,
		customToolBox.GetVisibleItemCount() == 1,
		L"custom toolbox: type-name filtering lost the external descriptor");

	DesignerCanvas descriptorCanvas(0, 0, 800, 600);
	const auto descriptorAdd = descriptorCanvas.AddControlToCanvas(
		customDescriptor, POINT{ 120, 120 });
	AppendFailure(failures, descriptorAdd,
		L"custom descriptor: transactional add failed");
	AppendFailure(failures,
		descriptorCanvas.GetAllControls().size() == 1
		&& descriptorCanvas.GetAllControls().front()->CustomType
			== customDescriptor.CustomType
		&& descriptorCanvas.GetAllControls().front()->CustomProperties
			== customDescriptor.CustomProperties
		&& dynamic_cast<SelfTestStatusBadge*>(
			descriptorCanvas.GetAllControls().front()->ControlInstance) != nullptr
		&& descriptorCanvas.GetAllControls().front()->ControlInstance->Size.cx == 150,
		L"custom descriptor: preview/custom identity was not retained");
	PropertyGrid customPropertyGrid(0, 0, 360, 500);
	customPropertyGrid.SetDesignerCanvas(&descriptorCanvas);
	ReloadCurrentSelection(customPropertyGrid, descriptorCanvas);
	const auto* severityRow = DesignerPropertyRowCatalog::Find(
		customPropertyGrid.GetPresentedPropertyRows(), L"Severity");
	AppendFailure(failures,
		severityRow
		&& severityRow->Source == DesignerPropertyRowSource::CustomDescriptor
		&& severityRow->Editor == DesignerPropertyRowEditorKind::Choice
		&& severityRow->Value.Text == L"1",
		L"custom property catalog: schema was not projected into the property grid");
	const auto severityEdit = customPropertyGrid.ApplyPropertyValue(
		L"Severity", L"2");
	AppendFailure(failures,
		severityEdit
		&& descriptorCanvas.GetAllControls().front()->MetadataProperties.at(
			L"Severity").Text == L"2",
		L"custom property catalog: typed edit was not tracked");
	AppendFailure(failures,
		!customPropertyGrid.ApplyPropertyValue(L"Severity", L"4"),
		L"custom property catalog: range/choice validation accepted an invalid value");
	customPropertyGrid.SetViewMode(DesignerPropertyGridViewMode::Events);
	ReloadCurrentSelection(customPropertyGrid, descriptorCanvas);
	std::wstring customDefaultHandler;
	AppendFailure(failures,
		customPropertyGrid.ActivateDefaultEventHandler(&customDefaultHandler)
		&& customDefaultHandler.find(L"OnSeverityInvoked") != std::wstring::npos
		&& descriptorCanvas.GetAllControls().front()->EventHandlers.contains(
			L"OnSeverityInvoked"),
		L"custom event catalog: event page/default activation did not use the manifest contract");
	AppendFailure(failures,
		descriptorCanvas.UndoCommand()
		&& !descriptorCanvas.GetAllControls().front()->EventHandlers.contains(
			L"OnSeverityInvoked"),
		L"custom event catalog: default handler activation was not undoable");
	customPropertyGrid.SetViewMode(DesignerPropertyGridViewMode::Properties);
	ReloadCurrentSelection(customPropertyGrid, descriptorCanvas);
	auto customControl = descriptorCanvas.GetAllControls().front();
	customControl->DataBindings[L"Severity"] = {
		L"View.Severity",
		BindingMode::OneWay,
		DataSourceUpdateMode::OnPropertyChanged,
		L"" };
	std::wstring customBindingError;
	AppendFailure(failures,
		descriptorCanvas.RefreshDesignBindings(
			*customControl, &customBindingError)
		&& customControl->BindingPreviewStates.at(L"Severity").Status
			== DesignerBindingPreviewStatus::Detached,
		L"custom property binding: portable metadata was not validated without a DataContext");
	AppendFailure(failures,
		descriptorCanvas.SetDataContextSchema({
			{ L"View", BindingValueKind::Object, true, false, true },
			{ L"View.Severity", BindingValueKind::Int64, true, false, true } },
			&customBindingError),
		L"custom property binding: schema validation incorrectly required runtime metadata");
	customControl->DataBindings[L"Severity"].Mode = BindingMode::TwoWay;
	AppendFailure(failures,
		!descriptorCanvas.RefreshDesignBindings(
			*customControl, &customBindingError)
		&& customControl->BindingPreviewStates.at(L"Severity").Status
			== DesignerBindingPreviewStatus::Error,
		L"custom property binding: unsupported TwoWay mode passed portable validation");
	customControl->DataBindings[L"Severity"].Mode = BindingMode::OneWay;

	DesignerModel::DesignDocument descriptorDocument;
	std::wstring descriptorError;
	const bool descriptorCaptured = descriptorCanvas.BuildDesignDocument(
		descriptorDocument, &descriptorError);
	AppendFailure(failures,
		descriptorCaptured
		&& descriptorDocument.Nodes.size() == 1
		&& descriptorDocument.Nodes.front().CustomType
			== customDescriptor.CustomType
		&& descriptorDocument.Nodes.front().Props["metadata"]["Severity"]["value"]
			.get<std::string>() == "2"
		&& descriptorDocument.Nodes.front().Bindings.contains("Severity"),
		L"custom descriptor: document capture lost portable identity"
			+ (descriptorError.empty() ? std::wstring() : L": " + descriptorError));
	auto incompatibleEventDocument = descriptorDocument;
	incompatibleEventDocument.Nodes.front().Events["OnSeverityInvoked"] =
		"StatusBadge1_OnSeverityInvoked";
	auto incompatibleDescriptor = customDescriptor;
	incompatibleDescriptor.CustomEvents.front().Signature =
		DesignerCustomEventSignature::SenderBool;
	descriptorCanvas.RegisterControlDescriptor(incompatibleDescriptor);
	std::wstring incompatibilityError;
	AppendFailure(failures,
		!descriptorCanvas.ApplyDesignDocument(
			incompatibleEventDocument, &incompatibilityError)
		&& incompatibilityError.find(L"不兼容") != std::wstring::npos
		&& descriptorCanvas.GetAllControls().size() == 1,
		L"custom event catalog: an incompatible installed signature was accepted");
	descriptorCanvas.RegisterControlDescriptor(customDescriptor);
	customControl->DataBindings.clear();
	(void)descriptorCanvas.RefreshDesignBindings(*customControl, nullptr);
	AppendFailure(failures,
		descriptorCanvas.UndoCommand()
		&& descriptorCanvas.GetAllControls().size() == 1
		&& descriptorCanvas.GetAllControls().front()->MetadataProperties.find(
			L"Severity")
			== descriptorCanvas.GetAllControls().front()->MetadataProperties.end(),
		L"custom descriptor: property undo did not restore the schema default");
	AppendFailure(failures,
		descriptorCanvas.UndoCommand(),
		L"custom descriptor: add undo failed");
	AppendFailure(failures, descriptorCanvas.GetAllControls().empty(),
		L"custom descriptor: undo retained the control");
	AppendFailure(failures, descriptorCanvas.RedoCommand(),
		L"custom descriptor: add redo failed");
	AppendFailure(failures,
		descriptorCanvas.GetAllControls().size() == 1
		&& descriptorCanvas.GetAllControls().front()->CustomType
			== customDescriptor.CustomType
		&& dynamic_cast<SelfTestStatusBadge*>(
			descriptorCanvas.GetAllControls().front()->ControlInstance) != nullptr,
		L"custom descriptor: redo lost the custom identity/real preview factory");

	// High-frequency modal collections keep one small typed delta instead of
	// rebuilding or retaining the complete DesignDocument.
	DesignerCanvas comboStructureCanvas(0, 0, 900, 640);
	comboStructureCanvas.AddControlToCanvasCore(
		UIClass::UI_ComboBox, POINT{ 80, 80 });
	auto comboStructureControl =
		comboStructureCanvas.GetAllControls().back();
	auto* structureCombo = dynamic_cast<ComboBox*>(
		comboStructureControl->ControlInstance);
	structureCombo->Items = {
		L"Zero", L"One", L"Two", L"Three",
		L"Four", L"Five", L"Six", L"Seven" };
	DesignerDataBinding selectedBinding;
	selectedBinding.SourceProperty = L"Profile.ChoiceIndex";
	selectedBinding.Mode = BindingMode::OneWay;
	comboStructureControl->DataBindings[L"SelectedIndex"] = selectedBinding;
	AppendFailure(failures,
		structureCombo->TrySetPropertyValue(
			L"SelectedIndex", 5, ControlPropertyValueSource::Binding),
		L"structure delta: could not prepare ComboBox Binding value");
	comboStructureControl->MetadataProperties.erase(L"SelectedIndex");
	DesignerStructureSnapshot comboBefore;
	DesignerStructureSnapshot comboAfter;
	DesignerStructureSnapshot comboCurrent;
	std::wstring structureError;
	AppendFailure(failures,
		DesignerStructureEdit::Capture(
			*comboStructureControl,
			DesignerCustomEditorKind::ComboBoxItems,
			comboBefore, &structureError)
		&& comboBefore.ComboBox.HasBindingSelectedIndex
		&& comboBefore.ComboBox.BindingSelectedIndex == 5
		&& !comboBefore.ComboBox.HasLocalSelectedIndex
		&& comboBefore.ComboBox.HasConfiguredBinding,
		L"structure delta: failed to capture complete ComboBox before state");
	(void)comboStructureCanvas.ResetDocumentHistoryAsSaved();
	auto* stableComboInstance = comboStructureControl->ControlInstance;
	structureCombo->Items = { L"Alpha", L"Beta" };
	structureCombo->SelectedIndex = 1;
	const bool trackedComboSelectedIndex =
		DesignerPropertyCatalog::TrackCurrentValue(
			*structureCombo,
			comboStructureControl->MetadataProperties,
			L"SelectedIndex");
	const bool capturedComboAfter = DesignerStructureEdit::Capture(
			*comboStructureControl,
			DesignerCustomEditorKind::ComboBoxItems,
			comboAfter, &structureError);
	AppendFailure(failures,
		trackedComboSelectedIndex
		&& capturedComboAfter
		&& comboAfter.ComboBox.HasLocalSelectedIndex
		&& comboAfter.ComboBox.LocalSelectedIndex == 1
		&& comboAfter.ComboBox.HasBindingSelectedIndex
		&& comboAfter.ComboBox.BindingSelectedIndex >= 0
		&& comboAfter.ComboBox.BindingSelectedIndex < 2
		&& comboAfter.ComboBox.HasTrackedSelectedIndex,
		L"structure delta: failed to capture complete ComboBox after state"
		+ std::wstring(L" (tracked=")
		+ (trackedComboSelectedIndex ? L"true" : L"false")
		+ L", captured=" + (capturedComboAfter ? L"true" : L"false")
		+ L", local=" + (comboAfter.ComboBox.HasLocalSelectedIndex ? L"true" : L"false")
		+ L":" + std::to_wstring(comboAfter.ComboBox.LocalSelectedIndex)
		+ L", binding=" + (comboAfter.ComboBox.HasBindingSelectedIndex ? L"true" : L"false")
		+ L":" + std::to_wstring(comboAfter.ComboBox.BindingSelectedIndex)
		+ L", metadata=" + (comboAfter.ComboBox.HasTrackedSelectedIndex ? L"true" : L"false")
		+ L", error=" + structureError + L")");
	auto comboStructureCommand = std::make_unique<ControlStructureCommand>(
		&comboStructureCanvas,
		comboBefore,
		comboAfter,
		std::vector<std::wstring>{ comboStructureControl->Name },
		std::vector<std::wstring>{ comboStructureControl->Name },
		comboStructureControl->Name,
		comboStructureControl->Name,
		L"EditStructure:ComboBoxItems",
		true);
	AppendFailure(failures,
		comboStructureCommand->GetEstimatedMemoryUsage() < 32U * 1024U
		&& comboStructureCanvas.CommitAlreadyAppliedCommand(
			std::move(comboStructureCommand)),
		L"structure delta: ComboBox command was too large or failed to commit");
	AppendFailure(failures,
		comboStructureCanvas.UndoCommand()
		&& comboStructureControl->ControlInstance == stableComboInstance
		&& DesignerStructureEdit::Capture(
			*comboStructureControl,
			DesignerCustomEditorKind::ComboBoxItems,
			comboCurrent, &structureError)
		&& comboCurrent == comboBefore,
		L"structure delta: ComboBox undo lost Items/Local/Binding/metadata state");
	AppendFailure(failures,
		comboStructureCanvas.RedoCommand()
		&& comboStructureControl->ControlInstance == stableComboInstance
		&& DesignerStructureEdit::Capture(
			*comboStructureControl,
			DesignerCustomEditorKind::ComboBoxItems,
			comboCurrent, &structureError)
		&& comboCurrent == comboAfter,
		L"structure delta: ComboBox redo lost Items/Local/Binding/metadata state");
	comboStructureControl->DataBindings[L"SelectedIndex"].SourceProperty =
		L"External.ChangedIndex";
	const auto comboUndoCountBeforeConflict =
		comboStructureCanvas.GetUndoCommandCount();
	AppendFailure(failures,
		!comboStructureCanvas.UndoCommand()
		&& comboStructureCanvas.GetUndoCommandCount()
			== comboUndoCountBeforeConflict
		&& structureCombo->Items
			== std::vector<std::wstring>{ L"Alpha", L"Beta" },
		L"structure delta: ComboBox binding conflict consumed history or mutated state");
	comboStructureControl->DataBindings[L"SelectedIndex"] = selectedBinding;
	AppendFailure(failures,
		comboStructureCanvas.UndoCommand()
		&& DesignerStructureEdit::Capture(
			*comboStructureControl,
			DesignerCustomEditorKind::ComboBoxItems,
			comboCurrent, &structureError)
		&& comboCurrent == comboBefore,
		L"structure delta: ComboBox conflict recovery could not retry undo");

	DesignerCanvas structureCanvas(0, 0, 900, 640);
	structureCanvas.AddControlToCanvasCore(
		UIClass::UI_GridView, POINT{ 80, 80 });
	auto gridControl = structureCanvas.GetAllControls().back();
	auto* structureGrid = dynamic_cast<GridView*>(
		gridControl->ControlInstance);
	structureGrid->ClearColumns();
	structureGrid->AddColumn(GridViewColumn(
		L"Original", 120.0f, ColumnType::Text, true));
	DesignerStructureSnapshot gridBefore;
	AppendFailure(failures,
		DesignerStructureEdit::Capture(
			*gridControl, DesignerCustomEditorKind::GridViewColumns,
			gridBefore, &structureError),
		L"structure delta: failed to capture GridView before state");
	(void)structureCanvas.ResetDocumentHistoryAsSaved();
	auto* stableGridInstance = gridControl->ControlInstance;
	structureGrid->ClearColumns();
	GridViewColumn actionColumn(
		L"Action", 150.0f, ColumnType::Button, false);
	actionColumn.ButtonText = L"Run";
	structureGrid->AddColumn(actionColumn);
	GridViewColumn choiceColumn(
		L"Choice", 180.0f, ColumnType::ComboBox, true);
	choiceColumn.ComboBoxItems = { L"One", L"Two" };
	structureGrid->AddColumn(choiceColumn);
	DesignerStructureSnapshot gridAfter;
	AppendFailure(failures,
		DesignerStructureEdit::Capture(
			*gridControl, DesignerCustomEditorKind::GridViewColumns,
			gridAfter, &structureError),
		L"structure delta: failed to capture GridView after state");
	auto structureCommand = std::make_unique<ControlStructureCommand>(
		&structureCanvas,
		gridBefore,
		gridAfter,
		std::vector<std::wstring>{ gridControl->Name },
		std::vector<std::wstring>{ gridControl->Name },
		gridControl->Name,
		gridControl->Name,
		L"EditStructure:GridViewColumns",
		true);
	const auto structureCommandBytes =
		structureCommand->GetEstimatedMemoryUsage();
	AppendFailure(failures,
		structureCommandBytes < 32U * 1024U,
		L"structure delta: small collection retained document-sized history");
	AppendFailure(failures,
		structureCanvas.CommitAlreadyAppliedCommand(
			std::move(structureCommand)),
		L"structure delta: failed to commit already-applied columns");
	AppendFailure(failures,
		structureCanvas.UndoCommand()
		&& gridControl->ControlInstance == stableGridInstance
		&& DesignerStructureEdit::Capture(
			*gridControl, DesignerCustomEditorKind::GridViewColumns,
			gridAfter, &structureError)
		&& gridAfter == gridBefore,
		L"structure delta: undo replaced the GridView or lost its columns");
	AppendFailure(failures,
		structureCanvas.RedoCommand()
		&& gridControl->ControlInstance == stableGridInstance
		&& DesignerStructureEdit::Capture(
			*gridControl, DesignerCustomEditorKind::GridViewColumns,
			gridBefore, &structureError)
		&& gridBefore.GridViewColumns.size() == 2,
		L"structure delta: redo replaced the GridView or lost its columns");
	const auto expectedAfterConflict = gridBefore;
	structureGrid->AddColumn(GridViewColumn(
		L"External", 90.0f, ColumnType::Text, false));
	const auto undoCountBeforeConflict = structureCanvas.GetUndoCommandCount();
	AppendFailure(failures,
		!structureCanvas.UndoCommand()
		&& structureCanvas.GetUndoCommandCount() == undoCountBeforeConflict
		&& structureGrid->ColumnCount() == 3,
		L"structure delta: conflicting undo consumed history or mutated state");
	AppendFailure(failures,
		DesignerStructureEdit::Restore(
			*gridControl, expectedAfterConflict, &structureError)
		&& structureCanvas.UndoCommand()
		&& gridControl->ControlInstance == stableGridInstance,
		L"structure delta: conflict recovery could not retry undo");

	structureCanvas.AddControlToCanvasCore(
		UIClass::UI_TreeView, POINT{ 300, 80 });
	auto treeControl = structureCanvas.GetAllControls().back();
	auto* structureTree = dynamic_cast<TreeView*>(treeControl->ControlInstance);
	structureTree->Root->ClearChildren();
	auto rootNode = std::make_unique<TreeNode>(L"Root");
	rootNode->Expand = true;
	rootNode->AddChild(std::make_unique<TreeNode>(L"Child"));
	structureTree->Root->AddChild(std::move(rootNode));
	DesignerStructureSnapshot treeBefore;
	DesignerStructureSnapshot treeCurrent;
	AppendFailure(failures,
		DesignerStructureEdit::Capture(
			*treeControl, DesignerCustomEditorKind::TreeViewNodes,
			treeBefore, &structureError),
		L"structure delta: failed to capture TreeView nodes");
	structureTree->Root->ClearChildren();
	structureTree->Root->AddChild(std::make_unique<TreeNode>(L"Changed"));
	AppendFailure(failures,
		DesignerStructureEdit::Restore(
			*treeControl, treeBefore, &structureError)
		&& DesignerStructureEdit::Capture(
			*treeControl, DesignerCustomEditorKind::TreeViewNodes,
			treeCurrent, &structureError)
		&& treeCurrent == treeBefore,
		L"structure delta: TreeView hierarchy did not restore exactly");

	structureCanvas.AddControlToCanvasCore(
		UIClass::UI_Menu, POINT{ 300, 300 });
	auto menuControl = structureCanvas.GetAllControls().back();
	auto* structureMenu = dynamic_cast<Menu*>(menuControl->ControlInstance);
	structureMenu->ClearItems();
	auto* fileItem = structureMenu->AddItem(L"File");
	fileItem->Id = 100;
	fileItem->Shortcut = L"Alt+F";
	auto* openItem = fileItem->AddSubItem(L"Open", 101);
	openItem->Shortcut = L"Ctrl+O";
	auto* recentItem = fileItem->AddSubItem(L"Recent", 102);
	auto* projectItem = recentItem->AddSubItem(L"Project", 103);
	projectItem->Enable = false;
	fileItem->AddSeparator();
	structureMenu->AddSeparator();
	DesignerStructureSnapshot menuBefore;
	DesignerStructureSnapshot menuCurrent;
	AppendFailure(failures,
		DesignerStructureEdit::Capture(
			*menuControl, DesignerCustomEditorKind::MenuItems,
			menuBefore, &structureError),
		L"structure delta: failed to capture recursive Menu items");
	structureMenu->ClearItems();
	structureMenu->AddItem(L"Changed")->Id = 999;
	AppendFailure(failures,
		DesignerStructureEdit::Restore(
			*menuControl, menuBefore, &structureError)
		&& DesignerStructureEdit::Capture(
			*menuControl, DesignerCustomEditorKind::MenuItems,
			menuCurrent, &structureError)
		&& menuCurrent == menuBefore
		&& menuCurrent.MenuItems.size() == 2
		&& menuCurrent.MenuItems[0].Id == 100
		&& menuCurrent.MenuItems[0].Children.size() == 3
		&& menuCurrent.MenuItems[0].Children[1].Children.size() == 1
		&& menuCurrent.MenuItems[0].Children[1].Children[0].Id == 103
		&& !menuCurrent.MenuItems[0].Children[1].Children[0].Enable
		&& menuCurrent.MenuItems[1].Separator,
		L"structure delta: Menu hierarchy/command IDs did not restore exactly");

	structureCanvas.AddControlToCanvasCore(
		UIClass::UI_GridPanel, POINT{ 520, 80 });
	auto gridPanelControl = structureCanvas.GetAllControls().back();
	auto* structurePanel = dynamic_cast<GridPanel*>(
		gridPanelControl->ControlInstance);
	structurePanel->ClearRows();
	structurePanel->ClearColumns();
	structurePanel->AddRow(GridLength::Percent(40.0f), 5.0f, 200.0f);
	structurePanel->AddColumn(GridLength::Star(2.0f), 10.0f, 300.0f);
	DesignerStructureSnapshot definitionsBefore;
	DesignerStructureSnapshot definitionsCurrent;
	AppendFailure(failures,
		DesignerStructureEdit::Capture(
			*gridPanelControl,
			DesignerCustomEditorKind::GridPanelDefinitions,
			definitionsBefore, &structureError),
		L"structure delta: failed to capture GridPanel definitions");
	structurePanel->ClearRows();
	structurePanel->ClearColumns();
	structurePanel->AddRow(GridLength::Auto());
	structurePanel->AddColumn(GridLength::Pixels(50.0f));
	AppendFailure(failures,
		DesignerStructureEdit::Restore(
			*gridPanelControl, definitionsBefore, &structureError)
		&& DesignerStructureEdit::Capture(
			*gridPanelControl,
			DesignerCustomEditorKind::GridPanelDefinitions,
			definitionsCurrent, &structureError)
		&& definitionsCurrent == definitionsBefore,
		L"structure delta: GridPanel definitions did not restore exactly");

	structureCanvas.AddControlToCanvasCore(
		UIClass::UI_StatusBar, POINT{ 80, 360 });
	auto statusControl = structureCanvas.GetAllControls().back();
	auto* structureStatus = dynamic_cast<StatusBar*>(
		statusControl->ControlInstance);
	structureStatus->ClearParts();
	structureStatus->AddPart(L"Ready", -1);
	structureStatus->AddPart(L"Line 1", 120);
	DesignerStructureSnapshot statusBefore;
	DesignerStructureSnapshot statusCurrent;
	AppendFailure(failures,
		DesignerStructureEdit::Capture(
			*statusControl, DesignerCustomEditorKind::StatusBarParts,
			statusBefore, &structureError),
		L"structure delta: failed to capture StatusBar parts");
	structureStatus->ClearParts();
	structureStatus->AddPart(L"Changed", 0);
	AppendFailure(failures,
		DesignerStructureEdit::Restore(
			*statusControl, statusBefore, &structureError)
		&& DesignerStructureEdit::Capture(
			*statusControl, DesignerCustomEditorKind::StatusBarParts,
			statusCurrent, &structureError)
		&& statusCurrent == statusBefore
		&& !DesignerStructureEdit::SupportsDelta(
			DesignerCustomEditorKind::TabControlPages)
		&& !DesignerStructureEdit::SupportsDelta(
			DesignerCustomEditorKind::ToolBarButtons)
		&& DesignerStructureEdit::SupportsDelta(
			DesignerCustomEditorKind::MenuItems),
		L"structure delta: StatusBar restore or ownership fallback boundary failed");

	// Tab pages and ToolBar buttons transfer live child-tree ownership. Their
	// commands must preserve instances and wrappers instead of serializing the
	// complete document or rebuilding the edited subtree.
	DesignerCanvas tabCollectionCanvas(0, 0, 900, 640);
	tabCollectionCanvas.AddControlToCanvasCore(
		UIClass::UI_TabControl, POINT{ 300, 220 });
	auto tabCollectionControl = tabCollectionCanvas.GetSelectedControl();
	auto* collectionTabs = tabCollectionControl
		? dynamic_cast<TabControl*>(tabCollectionControl->ControlInstance)
		: nullptr;
	TabPage* firstPage = collectionTabs
		? collectionTabs->AddPage(L"First") : nullptr;
	TabPage* secondPage = collectionTabs
		? collectionTabs->AddPage(L"Second") : nullptr;
	if (collectionTabs) (void)collectionTabs->SelectPage(0);
	DesignerDataBinding tabSelectedBinding;
	tabSelectedBinding.SourceProperty = L"Profile.TabIndex";
	tabSelectedBinding.Mode = BindingMode::OneWay;
	if (tabCollectionControl)
		tabCollectionControl->DataBindings[L"SelectedIndex"] =
			tabSelectedBinding;
	const bool tabBindingReady = collectionTabs
		&& collectionTabs->TrySetPropertyValue(
			L"SelectedIndex", 0, ControlPropertyValueSource::Binding);
	if (collectionTabs)
	{
		const POINT inside{
			collectionTabs->AbsLocation.x
				- tabCollectionCanvas.AbsLocation.x + 90,
			collectionTabs->AbsLocation.y
				- tabCollectionCanvas.AbsLocation.y + 100
		};
		tabCollectionCanvas.AddControlToCanvasCore(
			UIClass::UI_Button, inside);
	}
	auto pageChildWrapper = tabCollectionCanvas.GetSelectedControl();
	auto* pageChild = pageChildWrapper
		? pageChildWrapper->ControlInstance : nullptr;
	DesignerModel::DesignDocument tabCollectionBefore;
	std::wstring tabCollectionError;
	const bool tabCollectionSetup = tabCollectionControl && collectionTabs
		&& tabBindingReady
		&& firstPage && secondPage && pageChildWrapper && pageChild
		&& pageChild->Parent == firstPage
		&& tabCollectionCanvas.BuildDesignDocument(
			tabCollectionBefore, &tabCollectionError);
	AppendFailure(failures, tabCollectionSetup,
		L"owned collection delta: TabControl setup failed");
	if (tabCollectionSetup)
	{
		(void)tabCollectionCanvas.ResetDocumentHistoryAsSaved();
		auto command = ControlOwnedCollectionCommand::CreateTabPages(
			&tabCollectionCanvas, tabCollectionControl,
			{
				{ secondPage, L"Second edited" },
				{ firstPage, L"First edited" },
				{ nullptr, L"Third" }
			},
			L"EditStructure:TabControlPages", &tabCollectionError);
		const size_t commandBytes = command
			? command->GetEstimatedMemoryUsage() : 0;
		const auto execute = command
			? tabCollectionCanvas.ExecuteCommand(std::move(command))
			: DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				tabCollectionError);
		TabPage* thirdPage = collectionTabs->Count == 3
			? collectionTabs->GetPage(2) : nullptr;
		BindingValue selectedBindingValue;
		int selectedBindingIndex = -1;
		DesignerModel::DesignDocument tabCollectionAfter;
		std::wstring tabAfterError;
		AppendFailure(failures,
			execute.HasChanges()
			&& commandBytes > 0 && commandBytes < 32U * 1024U
			&& collectionTabs->Count == 3
			&& collectionTabs->GetPage(0) == secondPage
			&& collectionTabs->GetPage(1) == firstPage
			&& collectionTabs->SelectedIndex == 1
			&& collectionTabs->TryGetPropertyValue(
				L"SelectedIndex", ControlPropertyValueSource::Binding,
				selectedBindingValue)
			&& selectedBindingValue.TryGetInt(selectedBindingIndex)
			&& selectedBindingIndex == 1
			&& pageChild->Parent == firstPage
			&& FindControl(tabCollectionCanvas, pageChildWrapper->Name)
				== pageChildWrapper
			&& tabCollectionCanvas.BuildDesignDocument(
				tabCollectionAfter, &tabAfterError)
			&& tabCollectionAfter != tabCollectionBefore,
			L"owned collection delta: Tab pages lost order, selection, child identity, or small-history budget"
			+ (execute.Error.empty() ? L"" : L": " + execute.Error));

		const auto undo = tabCollectionCanvas.UndoCommand();
		selectedBindingIndex = -1;
		DesignerModel::DesignDocument tabCollectionUndone;
		std::wstring tabUndoError;
		AppendFailure(failures,
			undo.HasChanges()
			&& collectionTabs->Count == 2
			&& collectionTabs->GetPage(0) == firstPage
			&& collectionTabs->GetPage(1) == secondPage
			&& collectionTabs->SelectedIndex == 0
			&& collectionTabs->TryGetPropertyValue(
				L"SelectedIndex", ControlPropertyValueSource::Binding,
				selectedBindingValue)
			&& selectedBindingValue.TryGetInt(selectedBindingIndex)
			&& selectedBindingIndex == 0
			&& pageChild->Parent == firstPage
			&& tabCollectionCanvas.BuildDesignDocument(
				tabCollectionUndone, &tabUndoError)
			&& tabCollectionUndone == tabCollectionBefore,
			L"owned collection delta: Tab undo rebuilt instances or missed exact state"
			+ (undo.Error.empty() ? L"" : L": " + undo.Error));
		const auto redo = tabCollectionCanvas.RedoCommand();
		AppendFailure(failures,
			redo.HasChanges()
			&& collectionTabs->Count == 3
			&& collectionTabs->GetPage(2) == thirdPage
			&& pageChild->Parent == firstPage,
			L"owned collection delta: Tab redo did not reuse retained pages");

		secondPage->Text = L"External conflict";
		const auto undoCount = tabCollectionCanvas.GetUndoCommandCount();
		const auto guardedUndo = tabCollectionCanvas.UndoCommand();
		AppendFailure(failures,
			!guardedUndo.Succeeded()
			&& tabCollectionCanvas.GetUndoCommandCount() == undoCount
			&& collectionTabs->GetPage(0) == secondPage
			&& secondPage->Text == L"External conflict",
			L"owned collection delta: Tab conflict mutated state or consumed history");
		secondPage->Text = L"Second edited";
		AppendFailure(failures,
			tabCollectionCanvas.UndoCommand().HasChanges()
			&& collectionTabs->GetPage(0) == firstPage,
			L"owned collection delta: Tab conflict repair could not retry undo");
	}

	DesignerCanvas toolCollectionCanvas(0, 0, 900, 640);
	toolCollectionCanvas.AddControlToCanvasCore(
		UIClass::UI_ToolBar, POINT{ 300, 180 });
	auto toolCollectionControl = toolCollectionCanvas.GetSelectedControl();
	auto* collectionToolBar = toolCollectionControl
		? dynamic_cast<ToolBar*>(toolCollectionControl->ControlInstance)
		: nullptr;
	if (collectionToolBar)
	{
		for (int x : { 45, 150 })
		{
			const POINT inside{
				collectionToolBar->AbsLocation.x
					- toolCollectionCanvas.AbsLocation.x + x,
				collectionToolBar->AbsLocation.y
					- toolCollectionCanvas.AbsLocation.y + 16
			};
			toolCollectionCanvas.AddControlToCanvasCore(
				UIClass::UI_Button, inside);
		}
	}
	auto* firstToolButton = collectionToolBar && collectionToolBar->Count >= 2
		? dynamic_cast<Button*>(collectionToolBar->GetChild(0)) : nullptr;
	auto* secondToolButton = collectionToolBar && collectionToolBar->Count >= 2
		? dynamic_cast<Button*>(collectionToolBar->GetChild(1)) : nullptr;
	auto firstToolWrapper = firstToolButton
		? std::find_if(toolCollectionCanvas.GetAllControls().begin(),
			toolCollectionCanvas.GetAllControls().end(),
			[firstToolButton](const auto& wrapper)
			{
				return wrapper && wrapper->ControlInstance == firstToolButton;
			})
		: toolCollectionCanvas.GetAllControls().end();
	DesignerModel::DesignDocument toolCollectionBefore;
	std::wstring toolCollectionError;
	const bool toolCollectionSetup = toolCollectionControl
		&& collectionToolBar && firstToolButton && secondToolButton
		&& firstToolWrapper != toolCollectionCanvas.GetAllControls().end()
		&& toolCollectionCanvas.BuildDesignDocument(
			toolCollectionBefore, &toolCollectionError);
	AppendFailure(failures, toolCollectionSetup,
		L"owned collection delta: ToolBar setup failed");
	if (toolCollectionSetup)
	{
		const auto stableFirstWrapper = *firstToolWrapper;
		(void)toolCollectionCanvas.ResetDocumentHistoryAsSaved();
		auto command = ControlOwnedCollectionCommand::CreateToolBarButtons(
			&toolCollectionCanvas, toolCollectionControl,
			{
				{ secondToolButton, L"Second", 72 },
				{ nullptr, L"New", 88 },
				{ firstToolButton, L"First", 64 }
			},
			L"EditStructure:ToolBarButtons", &toolCollectionError);
		const size_t commandBytes = command
			? command->GetEstimatedMemoryUsage() : 0;
		const auto execute = command
			? toolCollectionCanvas.ExecuteCommand(std::move(command))
			: DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				toolCollectionError);
		auto* newToolButton = collectionToolBar->Count == 3
			? dynamic_cast<Button*>(collectionToolBar->GetChild(1)) : nullptr;
		auto newToolWrapper = newToolButton
			? std::find_if(toolCollectionCanvas.GetAllControls().begin(),
				toolCollectionCanvas.GetAllControls().end(),
				[newToolButton](const auto& wrapper)
				{
					return wrapper && wrapper->ControlInstance == newToolButton;
				})
			: toolCollectionCanvas.GetAllControls().end();
		SIZE firstOverride{ -1, -1 };
		DesignerModel::DesignDocument toolCollectionAfter;
		std::wstring toolAfterError;
		AppendFailure(failures,
			execute.HasChanges()
			&& commandBytes > 0 && commandBytes < 32U * 1024U
			&& collectionToolBar->GetChild(0) == secondToolButton
			&& collectionToolBar->GetChild(2) == firstToolButton
			&& newToolButton && newToolButton->DesignId > 0
			&& newToolWrapper != toolCollectionCanvas.GetAllControls().end()
			&& (*newToolWrapper)->StableId == newToolButton->DesignId
			&& collectionToolBar->TryGetToolItemSizeOverride(
				firstToolButton, firstOverride)
			&& firstOverride.cx == 64
			&& firstOverride.cy == ToolBar::AutoItemHeightOverride
			&& toolCollectionCanvas.BuildDesignDocument(
				toolCollectionAfter, &toolAfterError)
			&& toolCollectionAfter != toolCollectionBefore,
			L"owned collection delta: ToolBar edit lost wrapper, stable ID, size override, or budget"
			+ (execute.Error.empty() ? L"" : L": " + execute.Error));

		const auto undo = toolCollectionCanvas.UndoCommand();
		DesignerModel::DesignDocument toolCollectionUndone;
		std::wstring toolUndoError;
		const bool toolUndoDocumentCaptured =
			toolCollectionCanvas.BuildDesignDocument(
				toolCollectionUndone, &toolUndoError);
		auto toolCollectionComparable = toolCollectionUndone;
		// Stable IDs are monotonic and deliberately not reused after undoing a
		// newly created button; compare the restored document payload separately.
		toolCollectionComparable.NextStableId =
			toolCollectionBefore.NextStableId;
		const bool toolUndoDocumentEqual = toolUndoDocumentCaptured
			&& toolCollectionComparable == toolCollectionBefore;
		std::wstring toolUndoDifference;
		if (toolUndoDocumentCaptured && !toolUndoDocumentEqual)
		{
			const auto beforeXml =
				DesignerModel::DesignDocumentSerializer::ToXml(
					toolCollectionBefore);
			const auto afterXml =
				DesignerModel::DesignDocumentSerializer::ToXml(
					toolCollectionUndone);
			size_t offset = 0;
			while (offset < beforeXml.size() && offset < afterXml.size()
				&& beforeXml[offset] == afterXml[offset]) ++offset;
			const size_t start = offset > 64 ? offset - 64 : 0;
			const auto beforePart = beforeXml.substr(start, 220);
			const auto afterPart = afterXml.substr(start, 220);
			toolUndoDifference = L", offset=" + std::to_wstring(offset)
				+ L", before="
				+ std::wstring(beforePart.begin(), beforePart.end())
				+ L", after="
				+ std::wstring(afterPart.begin(), afterPart.end());
		}
		AppendFailure(failures,
			undo.HasChanges()
			&& collectionToolBar->Count == 2
			&& collectionToolBar->GetChild(0) == firstToolButton
			&& collectionToolBar->GetChild(1) == secondToolButton
			&& FindControl(toolCollectionCanvas, stableFirstWrapper->Name)
				== stableFirstWrapper
			&& toolUndoDocumentEqual,
			std::wstring(L"owned collection delta: ToolBar undo rebuilt buttons or missed document state")
			+ L" (undo=" + (undo.HasChanges() ? std::wstring(L"true") : L"false")
			+ L", count=" + std::to_wstring(collectionToolBar->Count)
			+ L", order0=" + (collectionToolBar->GetChild(0) == firstToolButton ? std::wstring(L"true") : L"false")
			+ L", order1=" + (collectionToolBar->GetChild(1) == secondToolButton ? std::wstring(L"true") : L"false")
			+ L", wrapper=" + (FindControl(toolCollectionCanvas, stableFirstWrapper->Name)
				== stableFirstWrapper ? std::wstring(L"true") : L"false")
			+ L", captured=" + (toolUndoDocumentCaptured ? std::wstring(L"true") : L"false")
			+ L", equal=" + (toolUndoDocumentEqual ? std::wstring(L"true") : L"false")
			+ toolUndoDifference
			+ (undo.Error.empty() ? L"" : L", error=" + undo.Error) + L")");
		AppendFailure(failures,
			toolCollectionCanvas.RedoCommand().HasChanges()
			&& collectionToolBar->GetChild(1) == newToolButton,
			L"owned collection delta: ToolBar redo did not reuse the new button instance");
	}

	DesignerCanvas legacyToolCanvas(0, 0, 700, 480);
	legacyToolCanvas.AddControlToCanvasCore(
		UIClass::UI_ToolBar, POINT{ 250, 150 });
	auto legacyToolWrapper = legacyToolCanvas.GetSelectedControl();
	auto* legacyTool = legacyToolWrapper
		? dynamic_cast<ToolBar*>(legacyToolWrapper->ControlInstance) : nullptr;
	auto* legacyButton = legacyTool
		? legacyTool->AddToolButton(L"Legacy", 90) : nullptr;
	(void)legacyToolCanvas.ResetDocumentHistoryAsSaved();
	std::wstring legacyToolError;
	auto legacyCommand = ControlOwnedCollectionCommand::CreateToolBarButtons(
		&legacyToolCanvas, legacyToolWrapper,
		{ { legacyButton, L"Legacy", 90 } },
		L"EditStructure:ToolBarButtons", &legacyToolError);
	const auto legacyExecute = legacyCommand
		? legacyToolCanvas.ExecuteCommand(std::move(legacyCommand))
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed, legacyToolError);
	auto legacyButtonWrapper = legacyButton
		? std::find_if(legacyToolCanvas.GetAllControls().begin(),
			legacyToolCanvas.GetAllControls().end(),
			[legacyButton](const auto& wrapper)
			{
				return wrapper && wrapper->ControlInstance == legacyButton;
			})
		: legacyToolCanvas.GetAllControls().end();
	const auto retainedLegacyWrapper =
		legacyButtonWrapper != legacyToolCanvas.GetAllControls().end()
			? *legacyButtonWrapper : nullptr;
	AppendFailure(failures,
		legacyExecute.HasChanges() && legacyButton
		&& legacyButton->DesignId > 0 && retainedLegacyWrapper,
		L"owned collection delta: legacy unwrapped ToolBar button was not repaired");
	const auto legacyUndo = legacyToolCanvas.UndoCommand();
	const bool legacyWrapperAbsent = std::none_of(
		legacyToolCanvas.GetAllControls().begin(),
		legacyToolCanvas.GetAllControls().end(),
		[legacyButton](const auto& wrapper)
		{
			return wrapper && wrapper->ControlInstance == legacyButton;
		});
	AppendFailure(failures,
		legacyUndo.HasChanges() && legacyButton
		&& legacyButton->Parent == legacyTool
		&& legacyButton->DesignId == 0 && legacyWrapperAbsent,
		L"owned collection delta: legacy ToolBar undo did not restore unwrapped state");
	AppendFailure(failures,
		legacyToolCanvas.RedoCommand().HasChanges()
		&& retainedLegacyWrapper
		&& FindControl(legacyToolCanvas, retainedLegacyWrapper->Name)
			== retainedLegacyWrapper
		&& legacyButton->Parent == legacyTool,
		L"owned collection delta: legacy ToolBar redo rebuilt its wrapper or button");

	DesignerCanvas canvas(0, 0, 1000, 760);
	canvas.AddControlToCanvasCore(UIClass::UI_Button, POINT{ 100, 100 });
	canvas.AddControlToCanvasCore(UIClass::UI_Button, POINT{ 280, 100 });

	AppendFailure(failures, canvas.GetAllControls().size() == 2,
		L"setup: expected two controls");
	if (canvas.GetAllControls().size() != 2)
	{
		report = failures.front();
		return false;
	}

	const auto firstName = canvas.GetAllControls()[0]->Name;
	const auto secondName = canvas.GetAllControls()[1]->Name;
	canvas.GetAllControls()[1]->ControlInstance->Text = L"不同文本";
	canvas.RestoreSelectionByNames(
		{ firstName, secondName }, firstName, false);

	PropertyGrid propertyGrid(0, 0, 360, 700);
	propertyGrid.SetDesignerCanvas(&canvas);
	ReloadCurrentSelection(propertyGrid, canvas);

	const auto* mixedText = DesignerPropertyRowCatalog::Find(
		propertyGrid.GetPresentedPropertyRows(), L"Text");
	AppendFailure(failures, mixedText != nullptr,
		L"mixed selection: Text row missing");
	AppendFailure(failures, mixedText && mixedText->HasMixedValue,
		L"mixed selection: Text row did not report mixed values");

	const auto batchEdit = propertyGrid.ApplyPropertyValue(
		L"Text", L"批量文本");
	AppendFailure(failures, batchEdit.Succeeded && batchEdit.AppliedCount == 2,
		L"batch edit: expected both targets to update");
	AppendFailure(failures,
		ControlText(canvas, firstName) == L"批量文本"
			&& ControlText(canvas, secondName) == L"批量文本",
		L"batch edit: runtime values differ");
	AppendFailure(failures, !propertyGrid.HasPropertyEditError(),
		L"batch edit: stale error remained visible");

	AppendFailure(failures, canvas.UndoCommand(),
		L"undo: command was not available");
	AppendFailure(failures,
		ControlText(canvas, firstName) != L"批量文本"
			&& ControlText(canvas, secondName) == L"不同文本",
		L"undo: pre-edit values were not restored");
	AppendFailure(failures, canvas.GetSelectedControls().size() == 2
		&& canvas.GetSelectedControl()
		&& canvas.GetSelectedControl()->Name == firstName,
		L"undo: complete selection was not restored");

	AppendFailure(failures, canvas.RedoCommand(),
		L"redo: command was not available");
	AppendFailure(failures,
		ControlText(canvas, firstName) == L"批量文本"
			&& ControlText(canvas, secondName) == L"批量文本",
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
		ControlText(canvas, firstName) == L"批量文本"
			&& ControlText(canvas, secondName) == L"批量文本",
		L"error state: rejected edit mutated unrelated values");
	AppendFailure(failures, canvas.UndoCommand(),
		L"error state: prior valid command was no longer undoable");
	AppendFailure(failures,
		ControlText(canvas, firstName) != L"批量文本"
			&& ControlText(canvas, secondName) == L"不同文本",
		L"error state: rejected edit entered the undo history");
	AppendFailure(failures, canvas.RedoCommand(),
		L"error state: prior valid command was no longer redoable");
	AppendFailure(failures,
		ControlText(canvas, firstName) == L"批量文本"
			&& ControlText(canvas, secondName) == L"批量文本",
		L"error state: redo did not restore the prior valid edit");

	ReloadCurrentSelection(propertyGrid, canvas);
	const auto reset = propertyGrid.ResetPropertyValue(L"Text");
	AppendFailure(failures, reset.Succeeded && reset.AppliedCount == 2,
		L"reset: expected both targets to reset");
	AppendFailure(failures,
		ControlText(canvas, firstName).empty()
			&& ControlText(canvas, secondName).empty(),
		L"reset: default values were not applied (first='"
			+ ControlText(canvas, firstName) + L"', second='"
			+ ControlText(canvas, secondName) + L"')");
	AppendFailure(failures, !propertyGrid.HasPropertyEditError(),
		L"reset: successful edit did not clear the error state");

	AppendFailure(failures, canvas.UndoCommand(),
		L"reset undo: command was not available");
	AppendFailure(failures,
		ControlText(canvas, firstName) == L"批量文本"
			&& ControlText(canvas, secondName) == L"批量文本",
		L"reset undo: edited values were not restored");
	AppendFailure(failures, canvas.RedoCommand(),
		L"reset redo: command was not available");
	AppendFailure(failures,
		ControlText(canvas, firstName).empty()
			&& ControlText(canvas, secondName).empty(),
		L"reset redo: defaults were not restored (first='"
			+ ControlText(canvas, firstName) + L"', second='"
			+ ControlText(canvas, secondName) + L"')");

	// Design-time bindings must temporarily reveal the Binding value layer and
	// restore the persisted Local fallback when the preview context is removed.
	auto boundControl = FindControl(canvas, firstName);
	AppendFailure(failures, boundControl && boundControl->ControlInstance,
		L"binding preview: target control missing");
	if (boundControl && boundControl->ControlInstance)
	{
		boundControl->ControlInstance->Text = L"本地后备值";
		boundControl->DataBindings[L"Text"] = {
			L"Caption",
			BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged,
			L"StringTrim"
		};
		auto dataContext = std::make_shared<ObservableObject>();
		dataContext->SetValue(L"Caption", std::wstring(L"  绑定预览  "));
		canvas.SetDesignDataContext(dataContext);
		AppendFailure(failures,
			boundControl->ControlInstance->Text == L"绑定预览",
			L"binding preview: source value did not become effective");
		AppendFailure(failures,
			boundControl->ControlInstance->GetPropertyValueSource(L"Text")
				== ControlPropertyValueSource::Binding,
			L"binding preview: Binding did not own the effective value");
		const auto rows = DesignerPropertyRowCatalog::GetControlRows(
			*boundControl, DesignerControlPropertyContext{});
		const auto* textRow = DesignerPropertyRowCatalog::Find(rows, L"Text");
		AppendFailure(failures,
			textRow && textRow->HasConfiguredBinding && textRow->IsReadOnly
				&& !textRow->Diagnostics.empty(),
			L"binding preview: PropertyGrid row did not expose diagnostics");

		canvas.SetDesignDataContext(nullptr);
		AppendFailure(failures,
			boundControl->ControlInstance->Text == L"本地后备值",
			L"binding preview: removing DataContext did not restore Local value");
		AppendFailure(failures,
			boundControl->ControlInstance->DataBindings.Count() == 0,
			L"binding preview: transient runtime binding was not removed");
		const auto detached = boundControl->BindingPreviewStates.find(L"Text");
		AppendFailure(failures,
			detached != boundControl->BindingPreviewStates.end()
				&& detached->second.Status
					== DesignerBindingPreviewStatus::Detached,
			L"binding preview: detached state was not reported");
	}

	DesignerCanvas invisibleControlCanvas(0, 0, 800, 640);
	invisibleControlCanvas.AddControlToCanvasCore(
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
		invisiblePropertyGrid.ApplyPropertyValue(L"Visible", L"false");
	invisibleControlCanvas.Update();
	AppendFailure(failures,
		hideControlResult.Succeeded
		&& invisibleRuntime && !invisibleRuntime->Visible
		&& invisibleControlCanvas.GetSelectedControl() == invisibleControl,
		L"designer visibility: Visible=false discarded the selected control");
	if (invisibleRuntime)
	{
		const auto runtimeLocation = invisibleRuntime->AbsLocation;
		const auto runtimeSize = invisibleRuntime->ActualSize();
		const auto canvasLocation = invisibleControlCanvas.AbsLocation;
		const POINT hiddenCenter{
			runtimeLocation.x - canvasLocation.x + runtimeSize.cx / 2,
			runtimeLocation.y - canvasLocation.y + runtimeSize.cy / 2
		};
		invisibleControlCanvas.RestoreSelectionByNames({}, L"", false);
		(void)invisibleControlCanvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0,
			hiddenCenter.x, hiddenCenter.y);
		AppendFailure(failures,
			invisibleControlCanvas.GetSelectedControl() == invisibleControl,
			L"designer visibility: hidden placeholder was not hit-testable");
		(void)invisibleControlCanvas.CancelActivePointerInteraction(
			L"self-test cleanup");
	}

	DesignerCanvas hiddenAncestorCanvas(0, 0, 800, 640);
	hiddenAncestorCanvas.AddControlToCanvasCore(
		UIClass::UI_Panel, POINT{ 230, 200 });
	const auto hiddenAncestor = hiddenAncestorCanvas.GetSelectedControl();
	auto* const hiddenAncestorRuntime = hiddenAncestor
		? hiddenAncestor->ControlInstance : nullptr;
	if (hiddenAncestorRuntime)
	{
		const POINT inside{
			hiddenAncestorRuntime->AbsLocation.x
				- hiddenAncestorCanvas.AbsLocation.x + 60,
			hiddenAncestorRuntime->AbsLocation.y
				- hiddenAncestorCanvas.AbsLocation.y + 55
		};
		hiddenAncestorCanvas.AddControlToCanvasCore(
			UIClass::UI_Button, inside);
	}
	const auto hiddenDescendant = hiddenAncestorCanvas.GetSelectedControl();
	if (hiddenAncestorRuntime) hiddenAncestorRuntime->Visible = false;
	hiddenAncestorCanvas.Update();
	AppendFailure(failures,
		hiddenAncestor && hiddenDescendant
		&& hiddenAncestorCanvas.GetSelectedControl() == nullptr,
		L"designer visibility: a hidden ancestor retained a concealed descendant selection");
	if (hiddenAncestor)
	{
		hiddenAncestorCanvas.RestoreSelectionByNames(
			{ hiddenAncestor->Name }, hiddenAncestor->Name, false);
		hiddenAncestorCanvas.Update();
		AppendFailure(failures,
			hiddenAncestorCanvas.GetSelectedControl() == hiddenAncestor,
			L"designer visibility: a self-hidden container was not retained");
	}

	DesignerCanvas multiEventCanvas(0, 0, 800, 640);
	multiEventCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 150, 140 });
	multiEventCanvas.AddControlToCanvasCore(
		UIClass::UI_CheckBox, POINT{ 330, 140 });
	const auto multiEventButton = multiEventCanvas.GetAllControls().size() > 0
		? multiEventCanvas.GetAllControls()[0] : nullptr;
	const auto multiEventCheck = multiEventCanvas.GetAllControls().size() > 1
		? multiEventCanvas.GetAllControls()[1] : nullptr;
	if (multiEventButton && multiEventCheck)
	{
		multiEventButton->EventHandlers[L"OnMouseClick"] = L"FirstOnlyClick";
		multiEventCheck->EventHandlers[L"OnChecked"] = L"ConflictingShared";
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
			if (item.Name.rfind(L"OnMouseClick", 0) == 0)
				mixedCommonEventItem = &item;
			if (item.Name.rfind(L"OnChecked", 0) == 0)
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
		L"OnMouseClick", L"HandleMultiSelectionClick");
	const bool multiEventApplied = multiEventButton && multiEventCheck
		&& multiEventButton->EventHandlers[L"OnMouseClick"]
			== L"HandleMultiSelectionClick"
		&& multiEventCheck->EventHandlers[L"OnMouseClick"]
			== L"HandleMultiSelectionClick"
		&& multiEventCheck->EventHandlers[L"OnChecked"]
			== L"ConflictingShared";
	const auto multiEventUndoAfter = multiEventCanvas.GetUndoCommandCount();
	const auto undoMultiEvent = multiEventCanvas.UndoCommand();
	const bool multiEventRestored = multiEventButton && multiEventCheck
		&& multiEventButton->EventHandlers[L"OnMouseClick"] == L"FirstOnlyClick"
		&& !multiEventCheck->EventHandlers.contains(L"OnMouseClick")
		&& multiEventCheck->EventHandlers[L"OnChecked"] == L"ConflictingShared";
	const auto conflictingMultiEvent = multiEventGrid.ApplyPropertyValue(
		L"OnMouseClick", L"ConflictingShared");
	const bool multiEventConflictPreserved = multiEventButton && multiEventCheck
		&& multiEventButton->EventHandlers[L"OnMouseClick"] == L"FirstOnlyClick"
		&& !multiEventCheck->EventHandlers.contains(L"OnMouseClick")
		&& multiEventCheck->EventHandlers[L"OnChecked"] == L"ConflictingShared";
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
		L"OnMouseClick", &multiDefaultHandler);
	const auto expectedMultiDefault = multiEventCheck
		? multiEventCheck->Name + L"_OnMouseClick" : std::wstring{};
	const bool multiActivationApplied = multiEventButton && multiEventCheck
		&& multiEventButton->EventHandlers[L"OnMouseClick"]
			== expectedMultiDefault
		&& multiEventCheck->EventHandlers[L"OnMouseClick"]
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
		&& multiEventButton->EventHandlers[L"OnMouseClick"] == L"FirstOnlyClick"
		&& multiEventCheck
		&& !multiEventCheck->EventHandlers.contains(L"OnMouseClick"),
		L"multi-selection events: activation did not create one shared default handler");

	ReloadCurrentSelection(multiEventGrid, multiEventCanvas);
	int multiEventResetIndex = -1;
	if (multiEventNativeGrid)
		for (int index = 0;
			index < static_cast<int>(multiEventNativeGrid->Items.size()); ++index)
			if (multiEventNativeGrid->Items[static_cast<size_t>(index)].Name.rfind(
				L"OnMouseClick", 0) == 0)
			{
				multiEventResetIndex = index;
				break;
			}
	const auto multiResetUndoBefore = multiEventCanvas.GetUndoCommandCount();
	const bool requestedMultiEventReset = multiEventNativeGrid
		&& multiEventResetIndex >= 0
		&& multiEventNativeGrid->RequestReset(multiEventResetIndex);
	const bool multiEventResetApplied = multiEventButton && multiEventCheck
		&& !multiEventButton->EventHandlers.contains(L"OnMouseClick")
		&& !multiEventCheck->EventHandlers.contains(L"OnMouseClick")
		&& multiEventCheck->EventHandlers[L"OnChecked"] == L"ConflictingShared";
	const auto undoMultiEventReset = multiEventCanvas.UndoCommand();
	AppendFailure(failures,
		requestedMultiEventReset && multiEventResetApplied
		&& multiEventCanvas.GetUndoCommandCount() == multiResetUndoBefore
		&& undoMultiEventReset.HasChanges()
		&& multiEventButton
		&& multiEventButton->EventHandlers[L"OnMouseClick"] == L"FirstOnlyClick"
		&& multiEventCheck
		&& !multiEventCheck->EventHandlers.contains(L"OnMouseClick"),
		L"multi-selection events: reset affordance was not atomic or undoable");

	DesignerCanvas falseBooleanCanvas(0, 0, 800, 640);
	falseBooleanCanvas.AddControlToCanvasCore(
		UIClass::UI_CheckBox, POINT{ 170, 160 });
	if (auto eventControl = falseBooleanCanvas.GetSelectedControl())
		eventControl->EventHandlers[L"OnMouseClick"] = L"1";
	PropertyGrid falseBooleanGrid(0, 0, 360, 360);
	falseBooleanGrid.SetDesignerCanvas(&falseBooleanCanvas);
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const auto checkedRow = std::find_if(
		falseBooleanGrid.GetPresentedPropertyRows().begin(),
		falseBooleanGrid.GetPresentedPropertyRows().end(),
		[](const DesignerPropertyRow& row)
		{ return _wcsicmp(row.Name.c_str(), L"Checked") == 0; });
	const auto checkedDisplayName =
		checkedRow != falseBooleanGrid.GetPresentedPropertyRows().end()
		? checkedRow->DisplayName : std::wstring{};
	auto* nativeFalseGrid = falseBooleanGrid.GetNativePropertyGrid();
	const PropertyGridItem* nativeCheckedItem = nullptr;
	const PropertyGridItem* nativeColorItem = nullptr;
	const PropertyGridItem* nativeAnchorItem = nullptr;
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
			if (item.ValueType == PropertyGridValueType::Color)
				nativeColorItem = &item;
			if (item.ValueType == PropertyGridValueType::Anchor)
				nativeAnchorItem = &item;
			if (item.Name == L"OnMouseClick" || item.Name == L"OnChecked")
				propertyViewContainedEvent = true;
		}
	}
	AppendFailure(failures,
		checkedRow != falseBooleanGrid.GetPresentedPropertyRows().end()
		&& checkedRow->Value.Text == L"false"
		&& nativeFalseGrid && nativeFalseGrid->Visible
		&& nativeCheckedItem
		&& nativeCheckedItem->ValueType == PropertyGridValueType::Bool
		&& _wcsicmp(nativeCheckedItem->Value.c_str(), L"false") == 0
		&& !nativeCheckedItem->IsMixed
		&& nativeFalseGrid->CheckBackColor.a >= 0.9f
		&& nativeFalseGrid->CheckBorderColor.a >= 0.9f,
		L"property boolean editor: false Checked value was not visibly represented");
	AppendFailure(failures,
		nativeColorItem != nullptr,
		L"native property grid: color metadata did not use the native color editor");
	AppendFailure(failures,
		nativeAnchorItem != nullptr
		&& nativeAnchorItem->Options.empty()
		&& nativeAnchorItem->Value == L"0",
		L"native property grid: Anchor metadata did not use the visual anchor editor");
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
			if (item.Name.rfind(L"OnMouseClick", 0) == 0)
				nativeEventItem = &item;
			if (item.Name.rfind(L"OnChecked", 0) == 0)
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
		? eventControl->Name + L"_OnMouseClick" : std::wstring{};
	AppendFailure(failures,
		nativeEventItem
		&& nativeEventItem->ValueType == PropertyGridValueType::EditableEnum
		&& nativeEventItem->CanReset
		&& nativeEventItem->Value == defaultEventName
		&& nativeEventItem->Name.find(L"[未关联代码]") != std::wstring::npos
		&& std::find(nativeEventItem->Options.begin(), nativeEventItem->Options.end(),
			defaultEventName) != nativeEventItem->Options.end(),
		L"native property grid: legacy event mapping did not expose an editable default handler");
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
		&& nativeEventActivationItem->Value.find(L"OnChecked")
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
		L"OnMouseClick", L"HandleCheckClick");
	const auto eventHistoryMemoryAfter =
		falseBooleanCanvas.GetCommandHistoryMemoryUsage();
	const auto eventUndoCountAfter = falseBooleanCanvas.GetUndoCommandCount();
	const auto undoNamedEvent = falseBooleanCanvas.UndoCommand();
	const bool restoredLegacyEvent = eventControlBeforeDelta
		&& eventControlBeforeDelta->EventHandlers[L"OnMouseClick"] == L"1";
	const auto redoNamedEvent = falseBooleanCanvas.RedoCommand();
	const bool restoredNamedEvent = eventControlBeforeDelta
		&& eventControlBeforeDelta->EventHandlers[L"OnMouseClick"]
			== L"HandleCheckClick";
	const auto conflictingEventEdit = falseBooleanGrid.ApplyPropertyValue(
		L"OnChecked", L"HandleCheckClick");
	const auto invalidEventEdit = falseBooleanGrid.ApplyPropertyValue(
		L"OnMouseClick", L"bad::handler");
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
		L"OnMouseClick", &existingActivatedHandler);
	std::wstring defaultActivatedHandler;
	const auto defaultEventActivation = falseBooleanGrid.ActivateEventHandler(
		L"OnMouseDoubleClick", &defaultActivatedHandler);
	std::wstring catalogDefaultActivatedHandler;
	const auto catalogDefaultActivation =
		falseBooleanGrid.ActivateDefaultEventHandler(
			&catalogDefaultActivatedHandler);
	const auto expectedDoubleClickHandler = currentEventControl
		? currentEventControl->Name + L"_OnMouseDoubleClick"
		: std::wstring{};
	const auto expectedCheckedHandler = currentEventControl
		? currentEventControl->Name + L"_OnChecked"
		: std::wstring{};
	AppendFailure(failures,
		namedEventEdit
		&& eventUndoCountAfter == eventUndoCountBefore + 1
		&& eventHistoryMemoryAfter > eventHistoryMemoryBefore
		&& eventHistoryMemoryAfter - eventHistoryMemoryBefore < 32 * 1024
		&& undoNamedEvent.HasChanges()
		&& restoredLegacyEvent
		&& redoNamedEvent.HasChanges()
		&& restoredNamedEvent
		&& falseBooleanCanvas.GetSelectedControl() == eventControlBeforeDelta
		&& !conflictingEventEdit
		&& !conflictingEventEdit.Error.empty()
		&& !invalidEventEdit
		&& currentEventControl
		&& currentEventControl->EventHandlers[L"OnMouseClick"]
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
		&& currentEventControl->EventHandlers[L"OnMouseDoubleClick"]
			== expectedDoubleClickHandler
		&& currentEventControl->EventHandlers[L"OnChecked"]
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
			if (item.Name.rfind(L"OnMouseDoubleClick", 0) == 0)
				doubleClickEventIndex = static_cast<int>(i);
			if (item.Name.rfind(L"OnChecked", 0) == 0)
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
			nativeFalseGrid, KeyEventArgs(Keys::F12));
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
	eventCodeEntry.ParameterList = L"Control* sender, MouseEventArgs e";
	eventCodeEntry.State =
		DesignerModel::DesignEventHandlerCodeState::SignatureMismatch;
	eventCodeEntry.Diagnostic = L"现有定义参数签名不匹配；双击定位后修正。";
	eventCodeInspection.Handlers.emplace(
		eventCodeEntry.HandlerName, std::move(eventCodeEntry));
	eventCodeInspection.CompatibleUserHandlers[
		"Control* sender, MouseEventArgs e"] = {
			L"ReusableMouseHandler", expectedCheckedHandler };
	falseBooleanGrid.SetEventHandlerCodeInspection(
		std::move(eventCodeInspection));
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const PropertyGridItem* signatureDiagnosticItem = nullptr;
	if (nativeFalseGrid)
		for (const auto& item : nativeFalseGrid->Items)
			if (item.Name.rfind(L"OnMouseClick", 0) == 0)
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
			== signatureDiagnosticItem->Options.end(),
		L"native property grid: event source diagnostic was not visible on the row");
	std::wstring eventModeCategory;
	if (nativeFalseGrid)
	{
		for (const auto& item : nativeFalseGrid->Items)
		{
			if (item.Name.rfind(L"OnMouseClick", 0) == 0)
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
	falseBooleanGrid.SetFilterText(L"Checked");
	falseBooleanGrid.SetViewMode(DesignerPropertyGridViewMode::Events);
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const bool restoredEventFilter =
		falseBooleanGrid.GetFilterText() == L"Mouse";
	falseBooleanGrid.SetViewMode(DesignerPropertyGridViewMode::Properties);
	ReloadCurrentSelection(falseBooleanGrid, falseBooleanCanvas);
	const bool restoredPropertyFilter =
		falseBooleanGrid.GetFilterText() == L"Checked";
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
		"    Control*, MouseEventArgs)\n"
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
		"void Acme::Window::HandleClick(Control*, KeyEventArgs) {}\n"
		"void Acme::Window::HandleClick(Control*, MouseEventArgs) {}\n";
	constexpr std::string_view namespaceLocatorSource =
		"namespace Acme::Views {\n"
		"void Window::HandleClick(Control*, MouseEventArgs) {}\n"
		"}\n";
	constexpr std::string_view conditionalLocatorSource =
		"#define FAKE_SCOPE { ignored }\n"
		"#if 0\n"
		"void Acme::Window::HandleClick(Control*, MouseEventArgs) {}\n"
		"#endif\n"
		"void Acme::Window::HandleClick(Control*, MouseEventArgs) {}\n";
	constexpr std::string_view inlineLocatorSource =
		"namespace Acme {\n"
		"class Window {\n"
		"public:\n"
		"    void HandleClick(Control*, MouseEventArgs) {}\n"
		"};\n"
		"}\n";
	AppendFailure(failures,
		SourceCodeNavigator::FindMemberDefinitionLineInText(
			overloadedLocatorSource, "HandleClick", "Acme::Window",
			"Control* sender, MouseEventArgs e") == 2
		&& SourceCodeNavigator::FindMemberDefinitionLineInText(
			overloadedLocatorSource, "HandleClick", "Acme::Window") == 1,
		L"source navigation: signature-aware lookup did not select the compatible overload");
	AppendFailure(failures,
		SourceCodeNavigator::FindMemberDefinitionLineInText(
			namespaceLocatorSource, "HandleClick", "Acme::Views::Window",
			"Control* sender, MouseEventArgs e") == 2
		&& SourceCodeNavigator::FindMemberDefinitionLineInText(
			namespaceLocatorSource, "HandleClick", "Acme::Other::Window") == 0,
		L"source navigation: namespace-scoped member definition was not resolved exactly");
	AppendFailure(failures,
		SourceCodeNavigator::FindMemberDefinitionLineInText(
			conditionalLocatorSource, "HandleClick", "Acme::Window",
			"Control* sender, MouseEventArgs e") == 5,
		L"source navigation: disabled preprocessor branch or macro body produced a false definition");
	AppendFailure(failures,
		SourceCodeNavigator::FindMemberDefinitionLineInText(
			inlineLocatorSource, "HandleClick", "Acme::Window",
			"Control* sender, MouseEventArgs e") == 4,
		L"source navigation: inline class member definition was not located");

	DesignerCanvas canvasDefaultEventCanvas(0, 0, 800, 640);
	canvasDefaultEventCanvas.AddControlToCanvasCore(
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
		const auto size = runtime->ActualSize();
		const POINT center{
			runtime->AbsLocation.x - canvasDefaultEventCanvas.AbsLocation.x
				+ size.cx / 2,
			runtime->AbsLocation.y - canvasDefaultEventCanvas.AbsLocation.y
				+ size.cy / 2
		};
		(void)canvasDefaultEventCanvas.ProcessMessage(
			WM_LBUTTONDBLCLK, MK_LBUTTON, 0, center.x, center.y);
	}
	const auto expectedCanvasHandler = canvasDefaultControl
		? canvasDefaultControl->Name + L"_OnMouseClick" : std::wstring{};
	AppendFailure(failures,
		canvasDefaultControl
		&& canvasDefaultRequestCount == 1
		&& canvasDefaultResult
		&& canvasDefaultControl->EventHandlers[L"OnMouseClick"]
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
	(void)formDefaultEventCanvas.ProcessMessage(
		WM_LBUTTONDBLCLK, MK_LBUTTON, 0, 30, 60);
	const auto& formDefaultHandlers =
		formDefaultEventCanvas.GetDesignedFormEventHandlers();
	const auto shownHandler = formDefaultHandlers.find(L"OnShown");
	const bool createdFormDefault = shownHandler != formDefaultHandlers.end()
		&& shownHandler->second == L"MainForm_OnShown";
	const auto formEventMemory =
		formDefaultEventCanvas.GetCommandHistoryMemoryUsage();
	const auto undoFormDefault = formDefaultEventCanvas.UndoCommand();
	const bool removedFormDefault =
		formDefaultEventCanvas.GetDesignedFormEventHandlers().find(L"OnShown")
			== formDefaultEventCanvas.GetDesignedFormEventHandlers().end();
	const auto redoFormDefault = formDefaultEventCanvas.RedoCommand();
	const auto restoredFormDefault =
		formDefaultEventCanvas.GetDesignedFormEventHandlers().find(L"OnShown");
	AppendFailure(failures,
		formDefaultRequestCount == 1
		&& formDefaultResult
		&& createdFormDefault
		&& formEventMemory > 0 && formEventMemory < 32 * 1024
		&& undoFormDefault.HasChanges() && removedFormDefault
		&& redoFormDefault.HasChanges()
		&& restoredFormDefault
			!= formDefaultEventCanvas.GetDesignedFormEventHandlers().end()
		&& restoredFormDefault->second == L"MainForm_OnShown",
		L"canvas default event: form double-click did not create OnShown");

	DesignerCanvas eventRenameCanvas(0, 0, 800, 640);
	eventRenameCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 120, 120 });
	eventRenameCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 280, 120 });
	const auto& renameControls = eventRenameCanvas.GetAllControls();
	if (renameControls.size() == 2)
	{
		renameControls[0]->EventHandlers[L"OnMouseClick"] = L"HandleSharedClick";
		renameControls[0]->EventHandlers[L"OnMouseDoubleClick"] = L"HandleSharedClick";
		renameControls[1]->EventHandlers[L"OnMouseClick"] = L"HandleSharedClick";
	}
	eventRenameCanvas.SetDesignedFormEventHandler(
		L"OnCommand", L"HandleCommand");
	const auto renameHistoryMemoryBefore =
		eventRenameCanvas.GetCommandHistoryMemoryUsage();
	const auto renameUndoCountBefore = eventRenameCanvas.GetUndoCommandCount();
	const auto firstRenameInstance = renameControls.size() == 2
		? renameControls[0]->ControlInstance : nullptr;
	const auto secondRenameInstance = renameControls.size() == 2
		? renameControls[1]->ControlInstance : nullptr;
	size_t renamedEventReferences = 0;
	std::wstring renameCommandError;
	auto renamedEventTransaction = eventRenameCanvas.RenameEventHandler(
		L"HandleSharedClick", L"HandleClick",
		&renamedEventReferences, &renameCommandError);
	const bool renamedLiveReferences = renameControls.size() == 2
		&& renameControls[0]->EventHandlers[L"OnMouseClick"] == L"HandleClick"
		&& renameControls[0]->EventHandlers[L"OnMouseDoubleClick"] == L"HandleClick"
		&& renameControls[1]->EventHandlers[L"OnMouseClick"] == L"HandleClick";
	auto undoEventRename = eventRenameCanvas.UndoCommand();
	const bool restoredOldEventReferences = renameControls.size() == 2
		&& renameControls[0]->EventHandlers[L"OnMouseClick"] == L"HandleSharedClick"
		&& renameControls[1]->EventHandlers[L"OnMouseClick"] == L"HandleSharedClick";
	auto redoEventRename = eventRenameCanvas.RedoCommand();
	const auto renameHistoryMemoryAfter =
		eventRenameCanvas.GetCommandHistoryMemoryUsage();
	const bool renameUsedCompactDelta =
		eventRenameCanvas.GetUndoCommandCount() == renameUndoCountBefore + 1
		&& renameHistoryMemoryAfter > renameHistoryMemoryBefore
		&& renameHistoryMemoryAfter - renameHistoryMemoryBefore < 32 * 1024
		&& renameControls.size() == 2
		&& renameControls[0]->ControlInstance == firstRenameInstance
		&& renameControls[1]->ControlInstance == secondRenameInstance;

	DesignerEventHandlerDelta staleEventDelta;
	if (renameControls.size() == 2)
	{
		staleEventDelta.StableId = renameControls[0]->StableId;
		staleEventDelta.ControlType = renameControls[0]->Type;
		staleEventDelta.SubjectName = renameControls[0]->Name;
		staleEventDelta.EventName = L"OnMouseClick";
		staleEventDelta.Before = { true, L"HandleClick" };
		staleEventDelta.After = { true, L"GuardedClick" };
	}
	const auto staleHistoryCount = eventRenameCanvas.GetUndoCommandCount();
	if (renameControls.size() == 2)
		renameControls[0]->EventHandlers[L"OnMouseClick"] = L"ExternalClick";
	auto staleEventResult = eventRenameCanvas.ExecuteCommand(
		std::make_unique<EventHandlerCommand>(
			&eventRenameCanvas,
			std::vector<DesignerEventHandlerDelta>{ staleEventDelta },
			std::vector<std::wstring>{}, L"", L"GuardedEventEdit"));
	const bool staleEventWasRejected = !staleEventResult
		&& eventRenameCanvas.GetUndoCommandCount() == staleHistoryCount
		&& renameControls.size() == 2
		&& renameControls[0]->EventHandlers[L"OnMouseClick"]
			== L"ExternalClick";
	if (renameControls.size() == 2)
		renameControls[0]->EventHandlers[L"OnMouseClick"] = L"HandleClick";

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
			->ReferenceIndices.size() == 3
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
			->ReferenceIndices.size() == 3
		&& !xamlRenamedEventIndex.FindHandler(L"HandleSharedClick");
	CodeGenInput renamedEventCodeInput;
	const bool builtRenamedEventCodeInput = indexedRenamedEvents
		&& DesignerModel::DesignDocumentCodeGenInputBuilder::Build(
			renamedEventDocument, renamedEventCodeInput, &renamedEventError);
	CodeGenerator renamedEventGenerator(
		L"RenamedEventForm", renamedEventCodeInput);
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
		L"HandleClick", L"HandleCommand", nullptr, &renameCommandError);
	DesignerModel::DesignDocument afterConflictingRename;
	const bool capturedAfterConflict = eventRenameCanvas.BuildDesignDocument(
		afterConflictingRename, &renamedEventError);
	AppendFailure(failures,
		renamedEventTransaction.HasChanges()
		&& renamedEventReferences == 3
		&& renamedLiveReferences
		&& undoEventRename.HasChanges()
		&& restoredOldEventReferences
		&& redoEventRename.HasChanges()
		&& renameUsedCompactDelta
		&& staleEventWasRejected
		&& indexedRenamedEvents
		&& renamedEventIndex.FindHandler(L"HandleClick")
		&& renamedEventIndex.FindHandler(L"HandleClick")
			->ReferenceIndices.size() == 3
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
	const auto migrationBase = migrationDirectory / L"MigrationForm";
	auto readMigrationText = [](const fs::path& path)
	{
		std::ifstream stream(path, std::ios::binary);
		return std::string(std::istreambuf_iterator<char>(stream),
			std::istreambuf_iterator<char>());
	};
	DesignerCanvas migrationCanvas(0, 0, 800, 640);
	migrationCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 140, 140 });
	const auto migrationControl = migrationCanvas.GetAllControls().empty()
		? std::shared_ptr<DesignerControl>{}
		: migrationCanvas.GetAllControls().front();
	if (migrationControl)
		migrationControl->EventHandlers[L"OnMouseClick"] = L"HandleOriginal";
	DesignerModel::DesignCodeBehindModel migrationAssociation;
	migrationAssociation.ClassName = L"Acme::MigrationForm";
	migrationAssociation.RelativeBasePath = L"MigrationForm";
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
	migrationRequest.ParameterList = "Control* sender, MouseEventArgs e";
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
		&& migrationControl->EventHandlers[L"OnMouseClick"] == L"HandleRenamed"
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
		&& migrationControl->EventHandlers[L"OnMouseClick"] == L"HandleOriginal"
		&& undoMigrationSource.find("::HandleOriginal(") != std::string::npos
		&& undoMigrationSource.find("::HandleRenamed(") == std::string::npos
		&& undoMigrationSource.find("preservedBody = 7") != std::string::npos
		&& undoMigrationGeneratedSource.find("::HandleOriginal, this)")
			!= std::string::npos;
	const auto redoMigration = migrationCanvas.RedoCommand();
	const auto redoMigrationSource = readMigrationText(migrationSource);
	const bool redidBodyAndCode = redoMigration.HasChanges()
		&& migrationControl
		&& migrationControl->EventHandlers[L"OnMouseClick"] == L"HandleRenamed"
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
		&& migrationControl->EventHandlers[L"OnMouseClick"] == L"HandleRenamed"
		&& readMigrationText(migrationSource) == externallyChangedMigrationSource;
	const bool restoredRedoSource = DesignerModel::AtomicFile::Write(
		migrationSource.wstring(), redoMigrationSource, &migrationError);
	const auto retriedMigrationUndo = restoredRedoSource
		? migrationCanvas.UndoCommand()
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed, L"restore failed");
	const bool retriedExternalMigration = retriedMigrationUndo.HasChanges()
		&& migrationControl
		&& migrationControl->EventHandlers[L"OnMouseClick"] == L"HandleOriginal"
		&& readMigrationText(migrationSource).find("::HandleOriginal(")
			!= std::string::npos;

	const auto validMigrationHeader = readMigrationText(migrationUserHeader);
	auto invalidMigrationHeader = validMigrationHeader;
	const auto migrationIdentity = invalidMigrationHeader.find(
		"Acme::MigrationForm");
	if (migrationIdentity != std::string::npos)
		invalidMigrationHeader.replace(migrationIdentity,
			std::string("Acme::MigrationForm").size(),
			"Other::MigrationForm");
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
		&& migrationControl->EventHandlers[L"OnMouseClick"] == L"HandleOriginal"
		&& readMigrationText(migrationSource) == beforeFailedMigrationSource
		&& readMigrationText(migrationGeneratedSource)
			== beforeFailedGeneratedSource
		&& readMigrationText(migrationUserHeader) == invalidMigrationHeader;
	const bool restoredValidMigrationHeader = DesignerModel::AtomicFile::Write(
		migrationUserHeader.wstring(), validMigrationHeader, &migrationError);
	auto inlineMigrationHeader = validMigrationHeader;
	const auto inlineMigrationInclude = inlineMigrationHeader.find(
		"#include \"MigrationForm.handlers.g.inc\"");
	if (inlineMigrationInclude != std::string::npos)
		inlineMigrationHeader.insert(inlineMigrationInclude,
			"\tvoid HandleOriginal(Control* sender, MouseEventArgs e)\n"
			"\t{\n"
			"\t\t(void)sender; (void)e;\n"
			"\t\tint inlineBody = 11; (void)inlineBody;\n"
			"\t}\n");
	auto inlineMigrationSource = beforeFailedMigrationSource;
	const auto inlineSourceBegin = inlineMigrationSource.find(
		"void Acme::MigrationForm::HandleOriginal(");
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
		&& migrationControl->EventHandlers[L"OnMouseClick"]
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
		&& migrationControl->EventHandlers[L"OnMouseClick"]
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
		&& migrationControl->EventHandlers[L"OnMouseClick"]
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
	nativeSliderCanvas.AddControlToCanvasCore(
		UIClass::UI_ProgressBar, POINT{ 170, 160 });
	PropertyGrid nativeSliderGrid(0, 0, 360, 620);
	nativeSliderGrid.SetDesignerCanvas(&nativeSliderCanvas);
	ReloadCurrentSelection(nativeSliderGrid, nativeSliderCanvas);
	const auto* percentageRow = DesignerPropertyRowCatalog::Find(
		nativeSliderGrid.GetPresentedPropertyRows(), L"PercentageValue");
	auto* nativeSliderView = nativeSliderGrid.GetNativePropertyGrid();
	int nativeSliderIndex = -1;
	if (percentageRow && nativeSliderView)
	{
		for (int index = 0;
			index < static_cast<int>(nativeSliderView->Items.size()); ++index)
		{
			const auto& item = nativeSliderView->Items[static_cast<size_t>(index)];
			if (item.Name.rfind(percentageRow->DisplayName, 0) == 0)
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
		for (int y = 0; y < nativeSliderView->Height; ++y)
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
			nativeSliderView->ProcessMessage(
				WM_LBUTTONDOWN, 0, 0, 215, sliderY);
			nativeSliderView->ProcessMessage(
				WM_MOUSEMOVE, 0, 0, 285, sliderY);
			nativeSliderView->ProcessMessage(
				WM_LBUTTONUP, 0, 0, 285, sliderY);
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

	DesignerCanvas structuralCanvas(0, 0, 800, 640);
	structuralCanvas.AddControlToCanvasCore(
		UIClass::UI_ComboBox, POINT{ 140, 140 });
	AppendFailure(failures, structuralCanvas.GetAllControls().size() == 1,
		L"structure transaction: setup control missing");
	if (structuralCanvas.GetAllControls().size() == 1)
	{
		const auto structuralName =
			structuralCanvas.GetAllControls().front()->Name;
		auto getCombo = [&structuralCanvas, &structuralName]() -> ComboBox*
		{
			auto control = FindControl(structuralCanvas, structuralName);
			return control && control->ControlInstance
				? dynamic_cast<ComboBox*>(control->ControlInstance) : nullptr;
		};
		auto* combo = getCombo();
		AppendFailure(failures, combo != nullptr,
			L"structure transaction: target is not ComboBox");
		if (combo)
		{
			const auto originalItems = combo->Items;
			const auto originalSelectedIndex = combo->SelectedIndex;
			auto structureBegin = structuralCanvas.BeginDocumentEditTransaction(
				L"SelfTest:ComboBoxItems");
			AppendFailure(failures,
				structureBegin.State
					== DesignerDocumentTransactionState::Begun,
				L"structure transaction: could not capture before document");
			auto nestedBegin = structuralCanvas.BeginDocumentEditTransaction(
				L"SelfTest:Nested");
			AppendFailure(failures,
				nestedBegin.State
					== DesignerDocumentTransactionState::Rejected,
				L"structure transaction: nested transaction was accepted");
			combo->Items = { L"Alpha", L"Beta" };
			combo->SelectedIndex = 1;
			auto structuralControl = FindControl(
				structuralCanvas, structuralName);
			if (structuralControl)
				(void)DesignerPropertyCatalog::TrackCurrentValue(
					*combo,
					structuralControl->MetadataProperties,
					L"SelectedIndex");
			auto structureCommit =
				structuralCanvas.CommitDocumentEditTransaction();
			AppendFailure(failures,
				structureCommit.State
					== DesignerDocumentTransactionState::Committed,
				L"structure transaction: commit failed");
			AppendFailure(failures,
				getCombo() && getCombo()->Items
					== std::vector<std::wstring>{ L"Alpha", L"Beta" }
					&& getCombo()->SelectedIndex == 1,
				L"structure transaction: committed values differ");

			AppendFailure(failures, structuralCanvas.UndoCommand(),
				L"structure transaction: undo unavailable");
			AppendFailure(failures,
				getCombo() && getCombo()->Items == originalItems
					&& getCombo()->SelectedIndex == originalSelectedIndex,
				L"structure transaction: undo did not restore collection");
			AppendFailure(failures, structuralCanvas.RedoCommand(),
				L"structure transaction: redo unavailable");
			AppendFailure(failures,
				getCombo() && getCombo()->Items
					== std::vector<std::wstring>{ L"Alpha", L"Beta" }
					&& getCombo()->SelectedIndex == 1,
				L"structure transaction: redo did not restore collection");

			auto rollbackBegin = structuralCanvas.BeginDocumentEditTransaction(
				L"SelfTest:Rollback");
			AppendFailure(failures,
				rollbackBegin.State
					== DesignerDocumentTransactionState::Begun,
				L"structure transaction: rollback snapshot unavailable");
			if (auto* current = getCombo())
				current->Items = { L"Transient" };
			auto rollbackResult =
				structuralCanvas.RollbackDocumentEditTransaction();
			AppendFailure(failures,
				rollbackResult.State
					== DesignerDocumentTransactionState::RolledBack,
				L"structure transaction: explicit rollback failed");
			AppendFailure(failures,
				getCombo() && getCombo()->Items
					== std::vector<std::wstring>{ L"Alpha", L"Beta" },
				L"structure transaction: rollback retained transient values");
		}
	}
	DesignerCanvas emptyTransactionCanvas(0, 0, 800, 640);
	emptyTransactionCanvas.AddControlToCanvasCore(
		UIClass::UI_ComboBox, POINT{ 140, 140 });
	const auto emptyComboName = emptyTransactionCanvas.GetAllControls().empty()
		? std::wstring{}
		: emptyTransactionCanvas.GetAllControls().front()->Name;
	auto getEmptyCombo = [&emptyTransactionCanvas,
		&emptyComboName]() -> ComboBox*
	{
		auto control = FindControl(emptyTransactionCanvas, emptyComboName);
		return control && control->ControlInstance
			? dynamic_cast<ComboBox*>(control->ControlInstance) : nullptr;
	};
	const auto emptyOriginalItems = getEmptyCombo()
		? getEmptyCombo()->Items : std::vector<std::wstring>{};
	auto noChangeBegin = emptyTransactionCanvas.BeginDocumentEditTransaction(
		L"SelfTest:NoChange");
	auto noChangeCommit = emptyTransactionCanvas.CommitDocumentEditTransaction();
	AppendFailure(failures,
		noChangeBegin.State == DesignerDocumentTransactionState::Begun
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
	if (auto* current = getEmptyCombo())
		current->Items = { L"CancelTransient" };
	auto cancelMutation =
		emptyTransactionCanvas.CancelDocumentEditTransaction();
	AppendFailure(failures,
		cancelMutation.State
			== DesignerDocumentTransactionState::RolledBack
		&& getEmptyCombo()
		&& getEmptyCombo()->Items == emptyOriginalItems,
		L"structure transaction: cancel did not restore leaked mutation");
	AppendFailure(failures,
		IsUnchanged(emptyTransactionCanvas.UndoCommand()),
		L"structure transaction: restored cancel entered history");

	auto abortedTransaction =
		emptyTransactionCanvas.ExecuteDocumentEditTransaction(
			L"SelfTest:Abort",
			[&getEmptyCombo](std::wstring& error)
			{
				if (auto* current = getEmptyCombo())
					current->Items = { L"AbortTransient" };
				error = L"expected rejection";
				return false;
			});
	AppendFailure(failures,
		abortedTransaction.State
			== DesignerDocumentTransactionState::Aborted
		&& abortedTransaction.DocumentRestored
		&& getEmptyCombo()
		&& getEmptyCombo()->Items == emptyOriginalItems,
		L"document transaction: rejected operation was not restored");
	AppendFailure(failures,
		IsUnchanged(emptyTransactionCanvas.UndoCommand()),
		L"document transaction: rejected operation entered history");

	auto throwingTransaction =
		emptyTransactionCanvas.ExecuteDocumentEditTransaction(
			L"SelfTest:Exception",
			[&getEmptyCombo](std::wstring&) -> bool
			{
				if (auto* current = getEmptyCombo())
					current->Items = { L"ExceptionTransient" };
				throw 1;
			});
	AppendFailure(failures,
		throwingTransaction.State
			== DesignerDocumentTransactionState::Failed
		&& throwingTransaction.DocumentRestored
		&& getEmptyCombo()
		&& getEmptyCombo()->Items == emptyOriginalItems,
		L"document transaction: exception was not restored");
	AppendFailure(failures,
		IsUnchanged(emptyTransactionCanvas.UndoCommand()),
		L"document transaction: exception entered history");

	auto executedTransaction =
		emptyTransactionCanvas.ExecuteDocumentEditTransaction(
			L"SelfTest:Execute",
			[&getEmptyCombo](std::wstring& error)
			{
				auto* current = getEmptyCombo();
				if (!current)
				{
					error = L"ComboBox unavailable";
					return false;
				}
				current->Items = { L"Executed" };
				return true;
			});
	AppendFailure(failures,
		executedTransaction.State
			== DesignerDocumentTransactionState::Committed
		&& getEmptyCombo()
		&& getEmptyCombo()->Items
			== std::vector<std::wstring>{ L"Executed" },
		L"document transaction: execute did not commit");
	AppendFailure(failures, emptyTransactionCanvas.UndoCommand()
		&& getEmptyCombo()
		&& getEmptyCombo()->Items == emptyOriginalItems,
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
	coalescedPropertyCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 160, 160 });
	PropertyGrid coalescedPropertyGrid(0, 0, 360, 620);
	coalescedPropertyGrid.SetDesignerCanvas(&coalescedPropertyCanvas);
	ReloadCurrentSelection(
		coalescedPropertyGrid, coalescedPropertyCanvas);
	const auto coalescedPropertyName =
		coalescedPropertyCanvas.GetSelectedControl()
			? coalescedPropertyCanvas.GetSelectedControl()->Name
			: std::wstring{};
	const auto originalCoalescedText =
		ControlText(coalescedPropertyCanvas, coalescedPropertyName);
	const auto coalescedPropertyIdentity =
		coalescedPropertyCanvas.GetSelectedControl();
	const auto firstCoalescedProperty =
		coalescedPropertyGrid.ApplyPropertyValue(L"Text", L"A");
	const auto secondCoalescedProperty =
		coalescedPropertyGrid.ApplyPropertyValue(L"Text", L"AB");
	AppendFailure(failures,
		firstCoalescedProperty.Succeeded
		&& secondCoalescedProperty.Succeeded
		&& coalescedPropertyCanvas.GetUndoCommandCount() == 1
		&& ControlText(coalescedPropertyCanvas, coalescedPropertyName)
			== L"AB"
		&& coalescedPropertyCanvas.UndoCommand().HasChanges()
		&& ControlText(coalescedPropertyCanvas, coalescedPropertyName)
			== originalCoalescedText
		&& coalescedPropertyCanvas.RedoCommand().HasChanges()
		&& ControlText(coalescedPropertyCanvas, coalescedPropertyName)
			== L"AB"
		&& FindControl(coalescedPropertyCanvas, coalescedPropertyName)
			== coalescedPropertyIdentity
		&& coalescedPropertyCanvas.GetCommandHistoryMemoryUsage() < 32768,
		L"history coalescing: property edits did not merge end to end");
	ReloadCurrentSelection(
		coalescedPropertyGrid, coalescedPropertyCanvas);
	(void)coalescedPropertyCanvas.MarkDocumentSaved();
	const auto afterSaveProperty =
		coalescedPropertyGrid.ApplyPropertyValue(L"Text", L"ABC");
	AppendFailure(failures,
		afterSaveProperty.Succeeded
		&& coalescedPropertyCanvas.GetUndoCommandCount() == 2
		&& coalescedPropertyCanvas.IsDocumentDirty()
		&& coalescedPropertyCanvas.UndoCommand().HasChanges()
		&& !coalescedPropertyCanvas.IsDocumentDirty()
		&& ControlText(coalescedPropertyCanvas, coalescedPropertyName)
			== L"AB",
		L"history coalescing: merge crossed an exact save point");
	const auto identityBeforeSubtreeDelta =
		FindControl(coalescedPropertyCanvas, coalescedPropertyName);
	const auto addAfterPropertyDelta = coalescedPropertyCanvas.AddControlToCanvas(
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
		&& ControlText(coalescedPropertyCanvas, coalescedPropertyName)
			== originalCoalescedText
		&& coalescedPropertyCanvas.RedoCommand().HasChanges()
		&& ControlText(coalescedPropertyCanvas, coalescedPropertyName)
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
	renameCanvas.AddControlToCanvasCore(UIClass::UI_Button, POINT{ 160, 160 });
	PropertyGrid renameGrid(0, 0, 360, 620);
	renameGrid.SetDesignerCanvas(&renameCanvas);
	ReloadCurrentSelection(renameGrid, renameCanvas);
	const auto renameIdentity = renameCanvas.GetSelectedControl();
	const auto originalName = renameIdentity ? renameIdentity->Name : std::wstring{};
	const int originalStableId = renameIdentity ? renameIdentity->StableId : 0;
	const auto renameResult = renameGrid.ApplyPropertyValue(
		L"Name", L"RenamedButton");
	AppendFailure(failures,
		renameResult.Succeeded
		&& renameCanvas.GetSelectedControl()
		&& renameCanvas.GetSelectedControl()->Name == L"RenamedButton"
		&& renameCanvas.UndoCommand().HasChanges()
		&& renameCanvas.GetSelectedControl()
		&& renameCanvas.GetSelectedControl()->Name == originalName
		&& renameCanvas.GetSelectedControl() == renameIdentity
		&& renameCanvas.RedoCommand().HasChanges()
		&& renameCanvas.GetSelectedControl()
		&& renameCanvas.GetSelectedControl()->Name == L"RenamedButton"
		&& renameCanvas.GetSelectedControl() == renameIdentity
		&& renameCanvas.GetSelectedControl()->StableId == originalStableId
		&& renameCanvas.GetSelectedControl()->ControlInstance->DesignId
			== originalStableId,
		L"property delta: Name undo/redo lost identity or selection");

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
		&& reloadedIdentity->ControlInstance->DesignId == originalStableId
		&& reloadedRenamedDocument.NextStableId > originalStableId,
		L"stable identity: save/load or rename changed the control id");

	if (reloadedIdentity)
	{
		const int expectedNewId = reloadedRenamedDocument.NextStableId;
		reloadedIdentityCanvas.RestoreSelectionByNames(
			{ reloadedIdentity->Name }, reloadedIdentity->Name, false);
		const auto removedIdentity =
			reloadedIdentityCanvas.DeleteSelectedControl();
		reloadedIdentityCanvas.AddControlToCanvasCore(
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
	runtimeSourceCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 180, 170 });
	runtimeSourceCanvas.SetDesignedFormEventHandler(
		L"OnCommand", L"HandleRuntimeCommand");
	auto runtimeSourceControl = runtimeSourceCanvas.GetSelectedControl();
	DesignerModel::DesignDocument runtimeSourceDocument;
	std::wstring runtimeDocumentError;
	if (runtimeSourceControl && runtimeSourceControl->ControlInstance)
	{
		runtimeSourceControl->ControlInstance->Text = L"运行时本地后备";
		runtimeSourceControl->EventHandlers[L"OnMouseClick"] =
			L"HandleRuntimeClick";
		runtimeSourceControl->DataBindings[L"Text"] = {
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
			L"Round", false, {}, { DesignerStyleValueKind::Float, L"7" } });
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
	const auto runtimeXml = capturedRuntimeDocument
		? DesignerModel::DesignDocumentSerializer::ToXml(runtimeSourceDocument)
		: std::string{};
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
				|| request.Event.Name != L"OnMouseClick")
			{
				error = L"unexpected runtime event request";
				return false;
			}
			connection = request.Target.OnMouseClick.Subscribe(
				[&runtimeClickCount](Control*, MouseEventArgs)
				{
					++runtimeClickCount;
				});
			return true;
		};
	DesignerModel::RuntimeDocument runtimeDocument;
	const bool loadedRuntimeDocument = capturedRuntimeDocument
		&& DesignerModel::RuntimeDocumentLoader::LoadXml(
			runtimeXml,
			runtimeDocument,
			runtimeOptions,
			&runtimeDocumentError);
	auto* runtimeLoadedControl = runtimeSourceControl
		? runtimeDocument.FindControlByDesignId(runtimeSourceControl->StableId)
		: nullptr;
	if (runtimeLoadedControl)
	{
		runtimeLoadedControl->OnMouseClick.Invoke(
			runtimeLoadedControl, MouseEventArgs{});
	}
	Form runtimeHostForm(L"runtime host", POINT{ 0, 0 }, SIZE{ 320, 200 });
	int runtimeCommandCount = 0;
	const bool appliedRuntimeForm = loadedRuntimeDocument
		&& runtimeDocument.ApplyFormProperties(
			runtimeHostForm, &runtimeDocumentError);
	const bool boundRuntimeFormEvents = appliedRuntimeForm
		&& runtimeDocument.BindFormEvents(
			runtimeHostForm,
			[&runtimeCommandCount](
				const DesignerModel::RuntimeFormEventRequest& request,
				EventConnection& connection,
				std::wstring& error)
			{
				if (request.HandlerName != L"HandleRuntimeCommand"
					|| request.Event.Name != L"OnCommand")
				{
					error = L"unexpected runtime form event request";
					return false;
				}
				connection = request.Target.OnCommand.Subscribe(
					[&runtimeCommandCount](Form*, int, int)
					{
						++runtimeCommandCount;
					});
				return true;
			},
			&runtimeDocumentError);
	if (boundRuntimeFormEvents)
		runtimeHostForm.OnCommand.Invoke(&runtimeHostForm, 7, 11);
	AppendFailure(failures,
		loadedRuntimeDocument
		&& runtimeLoadedControl
		&& runtimeDocument.FindControlByName(runtimeSourceControl->Name)
			== runtimeLoadedControl
		&& runtimeDocument.RootControls().size() == 1
		&& runtimeDocument.OwnsRootControls()
		&& runtimeDocument.Controls().size() == 1
		&& runtimeDocument.BoundControlEventCount() == 1
		&& runtimeLoadedControl->Text == L"动态绑定值"
		&& runtimeLoadedControl->GetStyleSheet() != nullptr
		&& runtimeClickCount == 1
		&& appliedRuntimeForm
		&& boundRuntimeFormEvents
		&& runtimeDocument.BoundFormEventCount() == 1
		&& runtimeHostForm.Text == runtimeSourceDocument.Form.Text
		&& runtimeCommandCount == 1,
		L"runtime document: XML did not materialize identity, style, binding, and event state"
		+ std::wstring(L" [loaded=") + (loadedRuntimeDocument ? L"1" : L"0")
		+ L", roots=" + std::to_wstring(runtimeDocument.RootControls().size())
		+ L", controls=" + std::to_wstring(runtimeDocument.Controls().size())
		+ L", events=" + std::to_wstring(runtimeDocument.BoundControlEventCount())
		+ L", click=" + std::to_wstring(runtimeClickCount)
		+ L", formEvents=" + std::to_wstring(runtimeDocument.BoundFormEventCount())
		+ L", command=" + std::to_wstring(runtimeCommandCount)
		+ L", error=" + runtimeDocumentError + L"]");
	if (runtimeLoadedControl)
	{
		runtimeDataContext->SetValue(
			L"Caption", std::wstring(L"动态更新值"));
		AppendFailure(failures,
			runtimeLoadedControl->Text == L"动态更新值",
			L"runtime document: live data-context update did not reach the target");
		runtimeDocument.ClearDataBindings();
		AppendFailure(failures,
			runtimeLoadedControl->Text == L"运行时本地后备"
			&& runtimeLoadedControl->DataBindings.Count() == 0,
			L"runtime document: clearing bindings did not restore the persisted Local value");
	}
	Control* const runtimeBeforeRejectedLoad = runtimeLoadedControl;
	auto invalidRuntimeXml = runtimeXml;
	if (runtimeSourceControl)
	{
		const auto validId = std::string("id=\"")
			+ std::to_string(runtimeSourceControl->StableId) + "\"";
		const auto invalidIdPosition = invalidRuntimeXml.find(validId);
		if (invalidIdPosition != std::string::npos)
			invalidRuntimeXml.replace(
				invalidIdPosition, validId.size(), "id=\"0\"");
	}
	const bool rejectedRuntimeReplacement =
		!DesignerModel::RuntimeDocumentLoader::LoadXml(
			invalidRuntimeXml,
			runtimeDocument,
			{},
			&runtimeDocumentError);
	AppendFailure(failures,
		rejectedRuntimeReplacement
		&& !runtimeDocumentError.empty()
		&& runtimeDocument.FindControlByDesignId(
			runtimeSourceControl ? runtimeSourceControl->StableId : 0)
			== runtimeBeforeRejectedLoad,
		L"runtime document: rejected XML corrupted the previously loaded tree");

	const std::string runtimeXaml = R"xaml(
<Form xmlns="urn:cui"
      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
      xmlns:d="urn:cui:designer"
      x:Name="XamlRuntimeForm" x:Class="Runtime.Views.RuntimeWindow"
      d:CodeBehind="generated/RuntimeWindow" Text="Readable runtime form"
      Width="520" Height="280" Command="HandleRuntimeCommand">
  <Form.Resources>
    <Color x:Key="Accent">#FF0067C0</Color>
    <Style TargetType="Button">
      <Setter Property="Raised" Value="false" />
      <Style.Triggers>
        <Trigger Property="IsMouseOver" Value="true">
          <Setter Property="BorderThickness" Value="5.5" />
        </Trigger>
        <MultiTrigger>
          <MultiTrigger.Conditions>
            <Condition Property="IsMouseOver" Value="true" />
            <Condition Property="IsChecked" Value="true" />
          </MultiTrigger.Conditions>
          <Setter Property="Round" Value="12" />
        </MultiTrigger>
        <DataTrigger Binding="{Binding Status}" Value="Ready">
          <Setter Property="Visible" Value="false" />
        </DataTrigger>
        <MultiDataTrigger>
          <MultiDataTrigger.Conditions>
            <Condition Binding="{Binding Status}" Value="Ready" />
            <Condition Binding="{Binding IsAdmin}" Value="true" />
          </MultiDataTrigger.Conditions>
          <Setter Property="Raised" Value="true" />
        </MultiDataTrigger>
      </Style.Triggers>
    </Style>
    <Style x:Key="BaseButton" TargetType="Button"
           BasedOn="{StaticResource {x:Type Button}}">
      <Setter Property="BorderThickness" Value="2.5" />
      <Setter Property="Round" Value="4" />
    </Style>
    <Style x:Key="PrimaryButton" BasedOn="{StaticResource BaseButton}" Class="primary">
      <Setter Property="Round" Value="9" />
      <Setter Property="BackColor" Value="{StaticResource Accent}" />
    </Style>
  </Form.Resources>
  <StackPanel x:Name="xamlRoot" DesignId="500"
              Width="Auto" Height="Auto"
              Orientation="Vertical" Spacing="6">
    <Button x:Name="xamlAction" DesignId="501"
            Classes="primary" Style="{StaticResource PrimaryButton}"
            Width="180.5" Height="36"
            Text="{Binding Caption, Mode=OneWay}"
            Click="HandleRuntimeClick" />
  </StackPanel>
</Form>)xaml";
	DesignerModel::DesignDocument parsedXamlDocument;
	std::wstring xamlError;
	const bool parsedRuntimeXaml =
		DesignerModel::XamlDocumentParser::FromXaml(
			runtimeXaml, parsedXamlDocument, &xamlError);
	DesignerModel::DesignDocument xamlRoundTrip;
	const bool roundTrippedRuntimeXaml = parsedRuntimeXaml
		&& DesignerModel::DesignDocumentSerializer::FromXml(
			DesignerModel::DesignDocumentSerializer::ToXml(parsedXamlDocument),
			xamlRoundTrip,
			&xamlError)
		&& xamlRoundTrip == parsedXamlDocument;
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
	CodeGenInput xamlStyleCodeInput;
	const bool builtXamlStyleCodeInput = parsedRuntimeXaml
		&& DesignerModel::DesignDocumentCodeGenInputBuilder::Build(
			parsedXamlDocument, xamlStyleCodeInput, &xamlError);
	const auto xamlStyleGeneratedCpp = builtXamlStyleCodeInput
		? CodeGenerator(L"XamlStyleForm", xamlStyleCodeInput).GenerateCpp()
		: std::string{};
	const auto firstInheritedBorder = xamlStyleGeneratedCpp.find(
		"ControlStyleSetter(L\"BorderThickness\"");
	const bool generatedExpandedStyleInheritance = firstInheritedBorder
		!= std::string::npos
		&& xamlStyleGeneratedCpp.find(
			"ControlStyleSetter(L\"BorderThickness\"", firstInheritedBorder + 1)
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
				|| request.Event.Name != L"OnMouseClick")
			{
				error = L"unexpected XAML event request";
				return false;
			}
			connection = request.Target.OnMouseClick.Subscribe(
				[&xamlClickCount](Control*, MouseEventArgs) { ++xamlClickCount; });
			return true;
		};
	const bool loadedRuntimeXaml =
		DesignerModel::RuntimeDocumentLoader::LoadXaml(
			runtimeXaml, xamlRuntimeDocument, xamlOptions, &xamlError);
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
		xamlDataTriggerInactive = xamlButton->Visible;
		xamlMultiDataTriggerInactive = !xamlButton->Raised;
		runtimeDataContext->SetValue(
			L"Status", std::wstring(L"Ready"));
		xamlDataTriggerApplied = !xamlButton->Visible;
		runtimeDataContext->SetValue(L"IsAdmin", true);
		xamlMultiDataTriggerApplied = xamlButton->Raised;
		runtimeDataContext->SetValue(
			L"Status", std::wstring(L"Pending"));
		xamlDataTriggerRestored = xamlButton->Visible;
		xamlMultiDataTriggerRestored = !xamlButton->Raised;
		runtimeDataContext->SetValue(L"IsAdmin", false);
		xamlButton->SetStyleState(ControlStyleState::Hovered);
		xamlTriggerApplied = std::fabs(
			xamlButton->BorderThickness - 5.5f) < 0.001f;
		xamlMultiTriggerInactive = std::fabs(xamlButton->Round - 9.0f) < 0.001f;
		xamlButton->SetStyleState(ControlStyleState::Checked);
		xamlMultiTriggerApplied = std::fabs(xamlButton->Round - 12.0f) < 0.001f;
		xamlButton->SetStyleState(ControlStyleState::Hovered, false);
		xamlTriggerRestored = std::fabs(
			xamlButton->BorderThickness - 2.5f) < 0.001f;
		xamlMultiTriggerRestored = std::fabs(xamlButton->Round - 9.0f) < 0.001f;
		xamlButton->SetStyleState(ControlStyleState::Checked, false);
	}
	if (xamlAction)
		xamlAction->OnMouseClick.Invoke(xamlAction, MouseEventArgs{});
	const auto* xamlActionBeforeFailure = xamlAction;
	const bool rejectedXamlReplacement =
		!DesignerModel::RuntimeDocumentLoader::LoadXaml(
			"<Form><Button Name=\"bad\" UnknownProperty=\"1\" /></Form>",
			xamlRuntimeDocument,
			{},
			&xamlError);
	auto unchangedParsedDocument = parsedXamlDocument;
	const bool rejectedParserReplacement =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Form><Unknown /></Form>", unchangedParsedDocument, &xamlError);
	auto unchangedConflictDocument = parsedXamlDocument;
	const bool rejectedSignatureConflictXaml =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Form x:Name=\"ConflictForm\" "
			"xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
			"Command=\"HandleShared\">"
			"<Button x:Name=\"action\" Click=\"HandleShared\" />"
			"</Form>", unchangedConflictDocument, &xamlError);
	auto unchangedInvalidNameDocument = parsedXamlDocument;
	const bool rejectedInvalidControlNameXaml =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Form xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
			"<Button x:Name=\"bad-name\" /></Form>",
			unchangedInvalidNameDocument, &xamlError);
	auto unchangedDuplicateNameDocument = parsedXamlDocument;
	const bool rejectedDuplicateControlNameXaml =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Form xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\">"
			"<Button x:Name=\"saveButton\" />"
			"<Button x:Name=\"SaveButton\" /></Form>",
			unchangedDuplicateNameDocument, &xamlError);
	auto unchangedCodeBehindDocument = parsedXamlDocument;
	const bool rejectedAbsoluteCodeBehindXaml =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Form xmlns:x=\"http://schemas.microsoft.com/winfx/2006/xaml\" "
			"xmlns:d=\"urn:cui:designer\" x:Name=\"Unsafe\" "
			"x:Class=\"UnsafeWindow\" d:CodeBehind=\"C:/outside/UnsafeWindow\" />",
			unchangedCodeBehindDocument, &xamlError);
	auto unchangedDataTriggerDocument = parsedXamlDocument;
	const bool rejectedConfiguredDataTriggerBinding =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Form><Form.Resources><Style TargetType=\"Button\"><Style.Triggers>"
			"<DataTrigger Binding=\"{Binding Status, Mode=OneWay}\" Value=\"Ready\">"
			"<Setter Property=\"Visible\" Value=\"false\" /></DataTrigger>"
			"</Style.Triggers></Style></Form.Resources></Form>",
			unchangedDataTriggerDocument, &xamlError);
	auto unchangedDataTriggerResourceDocument = parsedXamlDocument;
	const bool rejectedDataTriggerResourceValue =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Form><Form.Resources><Style TargetType=\"Button\"><Style.Triggers>"
			"<DataTrigger Binding=\"{Binding Status}\" Value=\"{StaticResource Ready}\">"
			"<Setter Property=\"Visible\" Value=\"false\" /></DataTrigger>"
			"</Style.Triggers></Style></Form.Resources></Form>",
			unchangedDataTriggerResourceDocument, &xamlError);
	auto unchangedMultiDataTriggerDocument = parsedXamlDocument;
	const bool rejectedMultiDataTrigger =
		!DesignerModel::XamlDocumentParser::FromXaml(
			"<Form><Form.Resources><Style TargetType=\"Button\"><Style.Triggers>"
			"<MultiDataTrigger><MultiDataTrigger.Conditions>"
			"<Condition Binding=\"{Binding Status}\" Value=\"Ready\" />"
			"</MultiDataTrigger.Conditions><Setter Property=\"Visible\" Value=\"false\" />"
			"</MultiDataTrigger></Style.Triggers></Style></Form.Resources></Form>",
			unchangedMultiDataTriggerDocument, &xamlError);
	AppendFailure(failures,
		parsedRuntimeXaml
		&& roundTrippedRuntimeXaml
		&& roundTrippedCanonicalXaml
		&& loadedRuntimeXaml
		&& xamlAction
		&& xamlButton
		&& xamlAction->Text == L"动态更新值"
		&& !xamlButton->Raised
		&& std::fabs(xamlButton->BorderThickness - 2.5f) < 0.001f
		&& std::fabs(xamlButton->Round - 9.0f) < 0.001f
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
		&& xamlAction->GetLayoutWidth().IsFixed()
		&& std::fabs(xamlAction->GetLayoutWidth().value - 180.5f) < 0.001f
		&& xamlAction->GetStyleSheet() != nullptr
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
			"<Condition Property=\"IsChecked\" Value=\"true\"")
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
		&& xamlStyleGeneratedCpp.find(".DataConditions.push_back")
			!= std::string::npos
		&& xamlStyleGeneratedCpp.find("::BindData(IBindingSource& dataContext)")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"BasedOn=\"{StaticResource {x:Type Button}}\"")
			!= std::string::npos
		&& canonicalRuntimeXaml.find(
			"BasedOn=\"{StaticResource BaseButton}\"")
			!= std::string::npos
		&& generatedExpandedStyleInheritance
		&& xamlClickCount == 1
		&& xamlRuntimeDocument.FormModel().Name == L"XamlRuntimeForm"
		&& parsedXamlDocument.CodeBehind.ClassName
			== L"Runtime::Views::RuntimeWindow"
		&& parsedXamlDocument.CodeBehind.RelativeBasePath
			== L"generated/RuntimeWindow"
		&& xamlRuntimeDocument.RootControls().size() == 1
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
		L"round-trip, or transactional rollback failed [error=" + xamlError + L"]");
	DesignerStyleSheet cyclicStyles;
	DesignerStyleRule cyclicA;
	cyclicA.Id = L"CycleA";
	cyclicA.BasedOn = L"CycleB";
	cyclicA.Setters.push_back({
		L"Visible", false, {}, { DesignerStyleValueKind::Bool, L"true" } });
	DesignerStyleRule cyclicB;
	cyclicB.Id = L"CycleB";
	cyclicB.BasedOn = L"CycleA";
	cyclicB.Setters.push_back({
		L"Enable", false, {}, { DesignerStyleValueKind::Bool, L"true" } });
	cyclicStyles.Rules = { std::move(cyclicA), std::move(cyclicB) };
	std::wstring cyclicStyleError;
	AppendFailure(failures,
		!DesignerStyleSheetUtils::Validate(cyclicStyles, &cyclicStyleError)
		&& cyclicStyleError.find(L"循环") != std::wstring::npos,
		L"style inheritance: a cyclic BasedOn chain was accepted");
	const auto runtimeGeneratorInput = runtimeDocument.BuildCodeGenInput();
	auto releasedRuntimeRoots = runtimeDocument.ReleaseRootControls();
	AppendFailure(failures,
		runtimeGeneratorInput.Controls.size() == 1
		&& runtimeGeneratorInput.FormName == runtimeSourceDocument.Form.Name
		&& releasedRuntimeRoots.size() == 1
		&& !runtimeDocument.OwnsRootControls()
		&& runtimeDocument.ReleaseRootControls().empty(),
		L"runtime document: codegen projection or root ownership transfer was inconsistent"
		+ std::wstring(L" [inputControls=")
		+ std::to_wstring(runtimeGeneratorInput.Controls.size())
		+ L", released=" + std::to_wstring(releasedRuntimeRoots.size())
		+ L", owns=" + (runtimeDocument.OwnsRootControls() ? L"1" : L"0")
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

	std::vector<std::unique_ptr<Control>> externallyOwnedRuntimeRoots;
	Control* externallyOwnedRuntimeControl = nullptr;
	bool loadedExternalOwnershipBinding = false;
	{
		DesignerModel::RuntimeDocument externallyOwnedDocument;
		DesignerModel::RuntimeDocumentLoadOptions externalOptions;
		externalOptions.DataContext = runtimeDataContext;
		loadedExternalOwnershipBinding = capturedRuntimeDocument
			&& DesignerModel::RuntimeDocumentLoader::LoadXml(
				runtimeXml,
				externallyOwnedDocument,
				externalOptions,
				&runtimeDocumentError);
		externallyOwnedRuntimeControl = runtimeSourceControl
			? externallyOwnedDocument.FindControlByDesignId(
				runtimeSourceControl->StableId)
			: nullptr;
		externallyOwnedRuntimeRoots =
			externallyOwnedDocument.ReleaseRootControls();
	}
	AppendFailure(failures,
		loadedExternalOwnershipBinding
		&& externallyOwnedRuntimeControl
		&& externallyOwnedRuntimeControl->Text == L"运行时本地后备"
		&& externallyOwnedRuntimeControl->DataBindings.Count() == 0
		&& externallyOwnedRuntimeRoots.size() == 1,
		L"runtime document: destruction after root transfer retained managed bindings"
		+ std::wstring(L" [loaded=")
		+ (loadedExternalOwnershipBinding ? L"1" : L"0")
		+ L", roots=" + std::to_wstring(externallyOwnedRuntimeRoots.size())
		+ L", error=" + runtimeDocumentError + L"]");

	DesignerCanvas largePropertyCanvas(0, 0, 1000, 720);
	for (int index = 0; index < 160; ++index)
		largePropertyCanvas.AddControlToCanvasCore(
			UIClass::UI_Button,
			POINT{ 30 + (index % 16) * 55, 40 + (index / 16) * 45 });
	const auto largePropertyTarget = largePropertyCanvas.GetAllControls().front();
	const auto largePropertyName = largePropertyTarget->Name;
	const auto largeOriginalText =
		ControlText(largePropertyCanvas, largePropertyName);
	largePropertyCanvas.RestoreSelectionByNames(
		{ largePropertyName }, largePropertyName, false);
	(void)largePropertyCanvas.ResetDocumentHistoryAsSaved();
	PropertyGrid largePropertyGrid(0, 0, 360, 620);
	largePropertyGrid.SetDesignerCanvas(&largePropertyCanvas);
	ReloadCurrentSelection(largePropertyGrid, largePropertyCanvas);
	const auto largePropertyResult = largePropertyGrid.ApplyPropertyValue(
		L"Text", L"LargeDelta");
	AppendFailure(failures,
		largePropertyResult.Succeeded
		&& largePropertyCanvas.GetUndoCommandCount() == 1
		&& largePropertyCanvas.GetCommandHistoryMemoryUsage() < 32768
		&& largePropertyCanvas.UndoCommand().HasChanges()
		&& ControlText(largePropertyCanvas, largePropertyName)
			== largeOriginalText
		&& FindControl(largePropertyCanvas, largePropertyName)
			== largePropertyTarget
		&& largePropertyCanvas.RedoCommand().HasChanges()
		&& ControlText(largePropertyCanvas, largePropertyName)
			== L"LargeDelta"
		&& FindControl(largePropertyCanvas, largePropertyName)
			== largePropertyTarget,
		L"property delta: large document retained a full snapshot or rebuilt controls");

	DesignerCanvas guardedPropertyCanvas(0, 0, 800, 640);
	guardedPropertyCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 160, 160 });
	(void)guardedPropertyCanvas.ResetDocumentHistoryAsSaved();
	PropertyGrid guardedPropertyGrid(0, 0, 360, 620);
	guardedPropertyGrid.SetDesignerCanvas(&guardedPropertyCanvas);
	ReloadCurrentSelection(guardedPropertyGrid, guardedPropertyCanvas);
	const auto guardedPropertyControl =
		guardedPropertyCanvas.GetSelectedControl();
	const auto guardedOriginalText = guardedPropertyControl
		? guardedPropertyControl->ControlInstance->Text : std::wstring{};
	const auto guardedPropertyEdit = guardedPropertyGrid.ApplyPropertyValue(
		L"Text", L"GuardedDelta");
	const auto guardedPropertyMemory =
		guardedPropertyCanvas.GetCommandHistoryMemoryUsage();
	if (guardedPropertyControl && guardedPropertyControl->ControlInstance)
		guardedPropertyControl->ControlInstance->Text = L"ExternalMutation";
	const auto rejectedPropertyUndo = guardedPropertyCanvas.UndoCommand();
	AppendFailure(failures,
		guardedPropertyEdit.Succeeded
		&& !rejectedPropertyUndo
		&& guardedPropertyCanvas.GetUndoCommandCount() == 1
		&& guardedPropertyCanvas.GetCommandHistoryMemoryUsage()
			== guardedPropertyMemory
		&& guardedPropertyControl
		&& guardedPropertyControl->ControlInstance->Text
			== L"ExternalMutation",
		L"property delta: mismatched start did not preserve failed undo history");
	if (guardedPropertyControl && guardedPropertyControl->ControlInstance)
		guardedPropertyControl->ControlInstance->Text = L"GuardedDelta";
	AppendFailure(failures,
		guardedPropertyCanvas.UndoCommand().HasChanges()
		&& guardedPropertyControl
		&& guardedPropertyControl->ControlInstance->Text
			== guardedOriginalText,
		L"property delta: guarded undo did not recover after start was repaired");

	DesignerCanvas interactionCanvas(0, 0, 800, 640);
	interactionCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 160, 160 });
	const auto interactionControlName =
		interactionCanvas.GetAllControls().empty()
		? std::wstring{}
		: interactionCanvas.GetAllControls().front()->Name;
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
	const auto addAfterPlacementDelta = interactionCanvas.AddControlToCanvas(
		UIClass::UI_Label, POINT{ 420, 260 });
	const auto undoSnapshotAfterPlacement = interactionCanvas.UndoCommand();
	const auto interactionIdentityAfterSubtreeDelta =
		getInteractionControl();
	AppendFailure(failures,
		addAfterPlacementDelta.HasChanges()
		&& undoSnapshotAfterPlacement.HasChanges()
		&& interactionIdentityAfterSubtreeDelta
		&& interactionIdentityAfterSubtreeDelta
			== interactionIdentityBeforeSubtreeDelta
		&& interactionCanvas.UndoCommand().HasChanges(),
		L"placement delta: target resolution failed after Add subtree undo");
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
	const auto guardedPlacementLocation = guardedPlacementControl
		? guardedPlacementControl->ControlInstance->Location : POINT{ 0, 0 };
	const auto guardedPlacementMemory =
		interactionCanvas.GetCommandHistoryMemoryUsage();
	if (guardedPlacementControl && guardedPlacementControl->ControlInstance)
	{
		auto externalLocation = guardedPlacementLocation;
		externalLocation.x += 7;
		guardedPlacementControl->ControlInstance->Location = externalLocation;
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
		guardedPlacementControl->ControlInstance->Location =
			guardedPlacementLocation;
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
		const auto size = runtime->ActualSize();
		const POINT center{
			runtime->AbsLocation.x - interactionCanvas.AbsLocation.x
				+ size.cx / 2,
			runtime->AbsLocation.y - interactionCanvas.AbsLocation.y
				+ size.cy / 2
		};
		(void)interactionCanvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0, center.x, center.y);
		(void)interactionCanvas.ProcessMessage(
			WM_MOUSEMOVE, MK_LBUTTON, 0, center.x + 15, center.y + 9);
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
		(void)interactionCanvas.ProcessMessage(
			WM_CANCELMODE, 0, 0, center.x + 15, center.y + 9);
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
		const auto size = runtime->ActualSize();
		const POINT bottomRight{
			runtime->AbsLocation.x - interactionCanvas.AbsLocation.x
				+ size.cx,
			runtime->AbsLocation.y - interactionCanvas.AbsLocation.y
				+ size.cy
		};
		(void)interactionCanvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0,
			bottomRight.x, bottomRight.y);
		(void)interactionCanvas.ProcessMessage(
			WM_MOUSEMOVE, MK_LBUTTON, 0,
			bottomRight.x + 12, bottomRight.y + 8);
		DesignerModel::DesignDocument resizePreviewDocument;
		std::wstring resizePreviewError;
		AppendFailure(failures,
			interactionCanvas.BuildDesignDocument(
				resizePreviewDocument, &resizePreviewError)
			&& resizePreviewDocument != interactionBaseline,
			L"canvas interaction: resize preview did not mutate document");
		(void)interactionCanvas.ProcessMessage(
			WM_KEYDOWN, VK_ESCAPE, 0,
			bottomRight.x + 12, bottomRight.y + 8);
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
		const auto size = runtime->ActualSize();
		const POINT center{
			runtime->AbsLocation.x - interactionCanvas.AbsLocation.x
				+ size.cx / 2,
			runtime->AbsLocation.y - interactionCanvas.AbsLocation.y
				+ size.cy / 2
		};
		(void)interactionCanvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0, center.x, center.y);
		(void)interactionCanvas.ProcessMessage(
			WM_MOUSEMOVE, MK_LBUTTON, 0, center.x + 8, center.y + 6);
		(void)interactionCanvas.ProcessMessage(
			WM_LBUTTONUP, 0, 0, center.x + 8, center.y + 6);
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
		const auto size = runtime->ActualSize();
		const POINT bottomRight{
			runtime->AbsLocation.x - interactionCanvas.AbsLocation.x
				+ size.cx,
			runtime->AbsLocation.y - interactionCanvas.AbsLocation.y
				+ size.cy
		};
		(void)interactionCanvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0,
			bottomRight.x, bottomRight.y);
		(void)interactionCanvas.ProcessMessage(
			WM_MOUSEMOVE, MK_LBUTTON, 0,
			bottomRight.x + 14, bottomRight.y + 10);
		(void)interactionCanvas.ProcessMessage(
			WM_LBUTTONUP, 0, 0,
			bottomRight.x + 14, bottomRight.y + 10);
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
	reparentCanvas.AddControlToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 220, 190 });
	auto reparentContainer = reparentCanvas.GetSelectedControl();
	const auto reparentContainerName = reparentContainer
		? reparentContainer->Name : std::wstring{};
	if (reparentContainer && reparentContainer->ControlInstance)
	{
		auto* containerRuntime = reparentContainer->ControlInstance;
		const POINT insideContainer{
			containerRuntime->AbsLocation.x - reparentCanvas.AbsLocation.x + 70,
			containerRuntime->AbsLocation.y - reparentCanvas.AbsLocation.y + 60
		};
		reparentCanvas.AddControlToCanvasCore(
			UIClass::UI_Button, insideContainer);
	}
	auto reparentTarget = reparentCanvas.GetSelectedControl();
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
		const auto size = runtime->ActualSize();
		const POINT center{
			runtime->AbsLocation.x - reparentCanvas.AbsLocation.x
				+ size.cx / 2,
			runtime->AbsLocation.y - reparentCanvas.AbsLocation.y
				+ size.cy / 2
		};
		const POINT rootDrop{ 650, 500 };
		(void)reparentCanvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0, center.x, center.y);
		(void)reparentCanvas.ProcessMessage(
			WM_MOUSEMOVE, MK_LBUTTON, 0, rootDrop.x, rootDrop.y);
		(void)reparentCanvas.ProcessMessage(
			WM_LBUTTONUP, 0, 0, rootDrop.x, rootDrop.y);
		std::wstring reparentedError;
		auto moved = FindControl(reparentCanvas, reparentTargetName);
		AppendFailure(failures,
			reparentCanvas.BuildDesignDocument(
				reparentedDocument, &reparentedError)
			&& reparentedDocument != reparentBaseline
			&& moved == reparentIdentity
			&& moved && !moved->DesignerParent
			&& moved->ControlInstance->Parent
				!= reparentContainer->ControlInstance
			&& reparentCanvas.GetUndoCommandCount() == 1
			&& reparentCanvas.GetCommandHistoryMemoryUsage() > 0
			&& reparentCanvas.GetCommandHistoryMemoryUsage() < 32768,
			L"placement tree delta: drag reparent retained a full snapshot");

		Control* rootParent = moved && moved->ControlInstance
			? moved->ControlInstance->Parent : nullptr;
		if (moved && moved->ControlInstance && rootParent
			&& reparentContainer->ControlInstance)
		{
			auto owner = rootParent->DetachControl(moved->ControlInstance);
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
			&& moved->ControlInstance->Parent
				== reparentContainer->ControlInstance)
		{
			auto owner = reparentContainer->ControlInstance->DetachControl(
				moved->ControlInstance);
			if (owner) rootParent->AddOwned(std::move(owner));
			moved->DesignerParent = nullptr;
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
		const auto addAfterTreeDelta = reparentCanvas.AddControlToCanvas(
			UIClass::UI_Label, POINT{ 700, 180 });
		const auto undoAfterTreeSnapshot = reparentCanvas.UndoCommand();
		const auto identityAfterTreeSubtreeDelta =
			FindControl(reparentCanvas, reparentTargetName);
		AppendFailure(failures,
			addAfterTreeDelta.HasChanges()
			&& undoAfterTreeSnapshot.HasChanges()
			&& identityAfterTreeSubtreeDelta
			&& identityAfterTreeSubtreeDelta == identityBeforeTreeSubtreeDelta
			&& reparentCanvas.UndoCommand().HasChanges(),
			L"placement tree delta: target resolution failed after Add subtree undo");
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
	reorderCanvas.AddControlToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 230, 190 });
	auto reorderContainer = reorderCanvas.GetSelectedControl();
	std::vector<std::wstring> reorderButtonNames;
	if (reorderContainer && reorderContainer->ControlInstance)
	{
		auto* runtime = reorderContainer->ControlInstance;
		const POINT firstDrop{
			runtime->AbsLocation.x - reorderCanvas.AbsLocation.x + 70,
			runtime->AbsLocation.y - reorderCanvas.AbsLocation.y + 45
		};
		const POINT secondDrop{ firstDrop.x, firstDrop.y + 55 };
		reorderCanvas.AddControlToCanvasCore(
			UIClass::UI_Button, firstDrop);
		reorderCanvas.AddControlToCanvasCore(
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
		&& firstReorderButton->ControlInstance->Parent
			== reorderContainer->ControlInstance
		&& firstReorderButton->ControlInstance->Parent->IndexOfControl(
			firstReorderButton->ControlInstance) == 0
		&& reorderCanvas.BuildDesignDocument(
			reorderBaseline, &reorderBaselineError);
	AppendFailure(failures, reorderSetup,
		L"placement tree delta: reorder setup failed");
	if (reorderSetup)
	{
		auto* firstRuntime = firstReorderButton->ControlInstance;
		auto* secondRuntime = secondReorderButton->ControlInstance;
		const auto firstSize = firstRuntime->ActualSize();
		const auto secondSize = secondRuntime->ActualSize();
		const POINT firstCenter{
			firstRuntime->AbsLocation.x - reorderCanvas.AbsLocation.x
				+ firstSize.cx / 2,
			firstRuntime->AbsLocation.y - reorderCanvas.AbsLocation.y
				+ firstSize.cy / 2
		};
		const POINT afterSecond{
			secondRuntime->AbsLocation.x - reorderCanvas.AbsLocation.x
				+ secondSize.cx / 2,
			secondRuntime->AbsLocation.y - reorderCanvas.AbsLocation.y
				+ secondSize.cy + 12
		};
		(void)reorderCanvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0,
			firstCenter.x, firstCenter.y);
		(void)reorderCanvas.ProcessMessage(
			WM_MOUSEMOVE, MK_LBUTTON, 0,
			afterSecond.x, afterSecond.y);
		(void)reorderCanvas.ProcessMessage(
			WM_LBUTTONUP, 0, 0,
			afterSecond.x, afterSecond.y);
		DesignerModel::DesignDocument reorderedDocument;
		std::wstring reorderedError;
		AppendFailure(failures,
			reorderCanvas.BuildDesignDocument(
				reorderedDocument, &reorderedError)
			&& reorderedDocument != reorderBaseline
			&& firstRuntime->Parent == reorderContainer->ControlInstance
			&& firstRuntime->Parent->IndexOfControl(firstRuntime) == 1
			&& reorderCanvas.GetUndoCommandCount() == 1
			&& reorderCanvas.GetCommandHistoryMemoryUsage() > 0
			&& reorderCanvas.GetCommandHistoryMemoryUsage() < 32768,
			L"placement tree delta: container reorder was not a small delta");
		AppendFailure(failures,
			reorderCanvas.UndoCommand().HasChanges()
			&& firstRuntime->Parent == reorderContainer->ControlInstance
			&& firstRuntime->Parent->IndexOfControl(firstRuntime) == 0,
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
			&& firstRuntime->Parent->IndexOfControl(firstRuntime) == 1,
			L"placement tree delta: reorder redo missed sibling order");
	}

	auto verifySpecialParentDelta = [&failures](
		UIClass parentType,
		DesignerPlacementParentKind expectedParentKind,
		const std::wstring& label)
	{
		DesignerCanvas canvas(0, 0, 800, 640);
		canvas.AddControlToCanvasCore(parentType, POINT{ 250, 190 });
		auto parent = canvas.GetSelectedControl();
		POINT childDrop{ 300, 250 };
		if (parent && parent->ControlInstance)
		{
			if (auto* split = dynamic_cast<SplitContainer*>(
				parent->ControlInstance))
			{
				split->RefreshSplitterLayout();
				if (auto* first = split->FirstPanel())
				{
					childDrop = POINT{
						first->AbsLocation.x - canvas.AbsLocation.x + 30,
						first->AbsLocation.y - canvas.AbsLocation.y + 35
					};
				}
			}
			else
			{
				childDrop = POINT{
					parent->ControlInstance->AbsLocation.x
						- canvas.AbsLocation.x + 80,
					parent->ControlInstance->AbsLocation.y
						- canvas.AbsLocation.y + 90
				};
			}
			canvas.AddControlToCanvasCore(UIClass::UI_Button, childDrop);
		}
		auto target = canvas.GetSelectedControl();
		if (target && target->ControlInstance)
		{
			if (auto* layoutParent = dynamic_cast<Panel*>(
				target->ControlInstance->Parent))
			{
				layoutParent->InvalidateLayout();
				layoutParent->PerformLayout();
			}
		}
		DesignerControlPlacementSnapshot captured;
		std::wstring captureError;
		DesignerModel::DesignDocument baseline;
		std::wstring baselineError;
		const bool setup = parent && parent->ControlInstance
			&& target && target->ControlInstance
			&& ControlPlacementCommand::Capture(
				&canvas, { target }, captured, &captureError)
			&& captured.Targets.size() == 1
			&& captured.Targets.front().ParentKind == expectedParentKind
			&& canvas.BuildDesignDocument(baseline, &baselineError);
		AppendFailure(failures, setup,
			L"placement tree delta: " + label
				+ L" parent locator setup failed");
		if (!setup) return;

		auto identity = target;
		auto* runtime = target->ControlInstance;
		const auto size = runtime->ActualSize();
		const POINT center{
			runtime->AbsLocation.x - canvas.AbsLocation.x + size.cx / 2,
			runtime->AbsLocation.y - canvas.AbsLocation.y + size.cy / 2
		};
		const POINT rootDrop{ 690, 500 };
		(void)canvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0, center.x, center.y);
		(void)canvas.ProcessMessage(
			WM_MOUSEMOVE, MK_LBUTTON, 0, rootDrop.x, rootDrop.y);
		(void)canvas.ProcessMessage(
			WM_LBUTTONUP, 0, 0, rootDrop.x, rootDrop.y);
		const auto specialParentUndo = canvas.UndoCommand();
		AppendFailure(failures,
			canvas.GetUndoCommandCount() == 0
			&& canvas.GetCommandHistoryMemoryUsage() > 0
			&& canvas.GetCommandHistoryMemoryUsage() < 32768
			&& specialParentUndo.HasChanges(),
			L"placement tree delta: " + label
				+ L" undo unavailable"
				+ (specialParentUndo.Error.empty() ? L""
					: L": " + specialParentUndo.Error));
		DesignerModel::DesignDocument restored;
		std::wstring restoredError;
		auto restoredTarget = FindControl(canvas, target->Name);
		const bool restoredCaptured =
			canvas.BuildDesignDocument(restored, &restoredError);
		std::wstring difference;
		if (restoredCaptured && restored != baseline)
		{
			const auto beforeXml =
				DesignerModel::DesignDocumentSerializer::ToXml(baseline);
			const auto afterXml =
				DesignerModel::DesignDocumentSerializer::ToXml(restored);
			size_t offset = 0;
			while (offset < beforeXml.size() && offset < afterXml.size()
				&& beforeXml[offset] == afterXml[offset]) ++offset;
			const size_t start = offset > 48 ? offset - 48 : 0;
			const auto beforePart = beforeXml.substr(start, 160);
			const auto afterPart = afterXml.substr(start, 160);
			difference = L" [offset " + std::to_wstring(offset)
				+ L", before="
				+ std::wstring(beforePart.begin(), beforePart.end())
				+ L", after="
				+ std::wstring(afterPart.begin(), afterPart.end()) + L"]";
		}
		AppendFailure(failures,
			restoredCaptured
			&& restored == baseline
			&& restoredTarget == identity,
			L"placement tree delta: " + label
				+ L" parent locator did not restore exactly" + difference);
	};
	verifySpecialParentDelta(
		UIClass::UI_SplitContainer,
		DesignerPlacementParentKind::SplitFirst,
		L"Split panel");
	verifySpecialParentDelta(
		UIClass::UI_TabControl,
		DesignerPlacementParentKind::TabPage,
		L"TabPage");

	DesignerCanvas splitterCanvas(0, 0, 800, 640);
	splitterCanvas.AddControlToCanvasCore(
		UIClass::UI_SplitContainer, POINT{ 140, 140 });
	const auto splitterName = splitterCanvas.GetAllControls().empty()
		? std::wstring{}
		: splitterCanvas.GetAllControls().front()->Name;
	auto getSplitter = [&splitterCanvas, &splitterName]() -> SplitContainer*
	{
		auto control = FindControl(splitterCanvas, splitterName);
		return control && control->ControlInstance
			? dynamic_cast<SplitContainer*>(control->ControlInstance)
			: nullptr;
	};
	DesignerModel::DesignDocument splitterBaseline;
	std::wstring splitterBaselineError;
	AppendFailure(failures,
		splitterCanvas.BuildDesignDocument(
			splitterBaseline, &splitterBaselineError),
		L"canvas interaction: splitter baseline capture failed");
	if (auto* split = getSplitter())
	{
		auto* const splitterIdentity = split;
		const int baselineDistance = split->SplitterDistance;
		const auto size = split->ActualSize();
		POINT splitterPoint{
			split->AbsLocation.x - splitterCanvas.AbsLocation.x,
			split->AbsLocation.y - splitterCanvas.AbsLocation.y
		};
		if (split->SplitOrientation == Orientation::Horizontal)
		{
			splitterPoint.x += split->SplitterDistance
				+ (std::max)(1, split->SplitterWidth) / 2;
			splitterPoint.y += size.cy / 2;
		}
		else
		{
			splitterPoint.x += size.cx / 2;
			splitterPoint.y += split->SplitterDistance
				+ (std::max)(1, split->SplitterWidth) / 2;
		}
		(void)splitterCanvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0,
			splitterPoint.x, splitterPoint.y);
		const int moveX = split->SplitOrientation == Orientation::Horizontal
			? splitterPoint.x + 10 : splitterPoint.x;
		const int moveY = split->SplitOrientation == Orientation::Vertical
			? splitterPoint.y + 10 : splitterPoint.y;
		(void)splitterCanvas.ProcessMessage(
			WM_MOUSEMOVE, MK_LBUTTON, 0, moveX, moveY);
		DesignerModel::DesignDocument splitterPreview;
		std::wstring splitterPreviewError;
		AppendFailure(failures,
			splitterCanvas.BuildDesignDocument(
				splitterPreview, &splitterPreviewError)
			&& splitterPreview != splitterBaseline,
			L"canvas interaction: splitter preview did not mutate metadata");
		const auto previewRedoCount = splitterCanvas.GetRedoCommandCount();
		const auto blockedSplitterUndo = splitterCanvas.UndoCommand();
		const auto blockedSplitterSave = splitterCanvas.MarkDocumentSaved();
		AppendFailure(failures,
			splitterCanvas.HasActiveDocumentTransaction()
			&& blockedSplitterUndo.State
				== DesignerDocumentTransactionState::Rejected
			&& blockedSplitterSave.State
				== DesignerDocumentTransactionState::Rejected
			&& splitterCanvas.GetRedoCommandCount() == previewRedoCount,
			L"property preview: history/save-point operations were not rejected");
		(void)splitterCanvas.ProcessMessage(
			WM_CAPTURECHANGED, 0, 0, moveX, moveY);
		DesignerModel::DesignDocument canceledSplitter;
		std::wstring canceledSplitterError;
		AppendFailure(failures,
			splitterCanvas.BuildDesignDocument(
				canceledSplitter, &canceledSplitterError)
			&& canceledSplitter == splitterBaseline
			&& splitterCanvas.GetLastInteractionTransaction()
				== L"UpdateProperty:SplitterDistance"
			&& splitterCanvas.GetLastInteractionTransactionResult().State
				== DesignerDocumentTransactionState::RolledBack
			&& splitterCanvas.GetUndoCommandCount() == 0,
			L"canvas interaction: capture loss did not restore splitter preview");

		(void)splitterCanvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0,
			splitterPoint.x, splitterPoint.y);
		(void)splitterCanvas.ProcessMessage(
			WM_MOUSEMOVE, MK_LBUTTON, 0, moveX, moveY);
		(void)splitterCanvas.ProcessMessage(
			WM_LBUTTONUP, 0, 0, moveX, moveY);
		DesignerModel::DesignDocument committedSplitter;
		std::wstring committedSplitterError;
		AppendFailure(failures,
			splitterCanvas.BuildDesignDocument(
				committedSplitter, &committedSplitterError)
			&& committedSplitter != splitterBaseline
			&& splitterCanvas.GetUndoCommandCount() == 1
			&& splitterCanvas.GetCommandHistoryMemoryUsage() < 32 * 1024
			&& getSplitter() == splitterIdentity
			&& splitterCanvas.GetLastInteractionTransaction()
				== L"UpdateProperty:SplitterDistance"
			&& splitterCanvas.GetLastInteractionTransactionResult().State
				== DesignerDocumentTransactionState::Committed,
			L"property delta: splitter commit used a snapshot or rebuilt its target");

		if (auto* committed = getSplitter())
		{
			const int committedDistance = committed->SplitterDistance;
			const auto historyBeforeConflict =
				splitterCanvas.GetCommandHistoryMemoryUsage();
			const auto undoCountBeforeConflict =
				splitterCanvas.GetUndoCommandCount();
			const bool changedOutsideHistory = committed->TrySetPropertyValue(
				L"SplitterDistance", BindingValue(baselineDistance),
				ControlPropertyValueSource::Local);
			const auto guardedUndo = splitterCanvas.UndoCommand();
			AppendFailure(failures,
				changedOutsideHistory
				&& !guardedUndo.Succeeded()
				&& splitterCanvas.GetUndoCommandCount()
					== undoCountBeforeConflict
				&& splitterCanvas.GetCommandHistoryMemoryUsage()
					== historyBeforeConflict,
				L"property delta: splitter conflict lost history or was not detected");
			const bool repaired = committed->TrySetPropertyValue(
				L"SplitterDistance", BindingValue(committedDistance),
				ControlPropertyValueSource::Local);
			const auto splitterUndo = splitterCanvas.UndoCommand();
			DesignerModel::DesignDocument undoneSplitter;
			std::wstring undoneSplitterError;
			AppendFailure(failures,
				repaired && splitterUndo.HasChanges()
				&& splitterCanvas.BuildDesignDocument(
					undoneSplitter, &undoneSplitterError)
				&& undoneSplitter == splitterBaseline
				&& getSplitter() == splitterIdentity,
				L"property delta: splitter undo did not restore the exact baseline");
			const auto splitterRedo = splitterCanvas.RedoCommand();
			DesignerModel::DesignDocument redoneSplitter;
			std::wstring redoneSplitterError;
			AppendFailure(failures,
				splitterRedo.HasChanges()
				&& splitterCanvas.BuildDesignDocument(
					redoneSplitter, &redoneSplitterError)
				&& redoneSplitter == committedSplitter
				&& getSplitter() == splitterIdentity,
				L"property delta: splitter redo did not restore the exact endpoint");

			if (auto* redone = getSplitter())
			{
				const auto redoneSize = redone->ActualSize();
				POINT secondPoint{
					redone->AbsLocation.x - splitterCanvas.AbsLocation.x,
					redone->AbsLocation.y - splitterCanvas.AbsLocation.y
				};
				if (redone->SplitOrientation == Orientation::Horizontal)
				{
					secondPoint.x += redone->SplitterDistance
						+ (std::max)(1, redone->SplitterWidth) / 2;
					secondPoint.y += redoneSize.cy / 2;
				}
				else
				{
					secondPoint.x += redoneSize.cx / 2;
					secondPoint.y += redone->SplitterDistance
						+ (std::max)(1, redone->SplitterWidth) / 2;
				}
				const int secondX = redone->SplitOrientation
					== Orientation::Horizontal
					? secondPoint.x - 5 : secondPoint.x;
				const int secondY = redone->SplitOrientation
					== Orientation::Vertical
					? secondPoint.y - 5 : secondPoint.y;
				(void)splitterCanvas.ProcessMessage(
					WM_LBUTTONDOWN, MK_LBUTTON, 0,
					secondPoint.x, secondPoint.y);
				(void)splitterCanvas.ProcessMessage(
					WM_MOUSEMOVE, MK_LBUTTON, 0, secondX, secondY);
				(void)splitterCanvas.ProcessMessage(
					WM_LBUTTONUP, 0, 0, secondX, secondY);
				AppendFailure(failures,
					splitterCanvas.GetUndoCommandCount() == 2
					&& splitterCanvas.GetCommandHistoryMemoryUsage()
						< 64 * 1024,
					L"property delta: adjacent splitter gestures merged or exceeded budget");
				const auto secondUndo = splitterCanvas.UndoCommand();
				DesignerModel::DesignDocument secondUndone;
				std::wstring secondUndoneError;
				AppendFailure(failures,
					secondUndo.HasChanges()
					&& splitterCanvas.BuildDesignDocument(
						secondUndone, &secondUndoneError)
					&& secondUndone == committedSplitter
					&& getSplitter() == splitterIdentity,
					L"property delta: one splitter undo crossed a gesture boundary");
			}
		}
	}
	else
	{
		AppendFailure(failures, false,
			L"canvas interaction: splitter target unavailable");
	}
	AppendFailure(failures,
		getSplitter() != nullptr,
		L"canvas interaction: splitter delta invalidated its target");

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
	auto addCommandResult = commandCanvas.AddControlToCanvas(
		UIClass::UI_Button, POINT{ 120, 120 });
	AppendFailure(failures,
		addCommandResult.State
			== DesignerDocumentTransactionState::Committed
		&& commandCanvas.GetAllControls().size() == 1
		&& commandCanvas.HasCommandResult()
		&& commandCanvas.GetLastCommandOperation() == L"AddControl"
		&& commandCanvas.GetLastCommandLabel() == L"AddControl"
		&& commandCanvas.GetLastCommandResult().State
			== DesignerDocumentTransactionState::Committed
		&& commandEventCount == 1
		&& lastCommandEvent.Operation == L"AddControl"
		&& lastCommandEvent.Label == L"AddControl",
		L"add command: commit result or event was not published");
	const auto addedName = commandCanvas.GetAllControls().empty()
		? std::wstring{} : commandCanvas.GetAllControls().front()->Name;
	const auto addedIdentity = commandCanvas.GetSelectedControl();
	auto* const addedRuntimeIdentity = addedIdentity
		? addedIdentity->ControlInstance : nullptr;
	const auto addCommandMemory =
		commandCanvas.GetCommandHistoryMemoryUsage();
	AppendFailure(failures,
		addCommandMemory > 0 && addCommandMemory < 32 * 1024,
		L"add command: simple subtree retained document-sized history");
	auto undoAddResult = commandCanvas.UndoCommand();
	AppendFailure(failures,
		undoAddResult.HasChanges()
		&& commandEventCount == 2
		&& lastCommandEvent.Operation == L"Undo"
		&& lastCommandEvent.Label == L"AddControl",
		L"add command: undo result or label was not published");
	AppendFailure(failures, commandCanvas.GetAllControls().empty(),
		L"add command: undo did not remove the control");
	auto redoAddResult = commandCanvas.RedoCommand();
	AppendFailure(failures,
		redoAddResult.HasChanges()
		&& commandEventCount == 3
		&& lastCommandEvent.Operation == L"Redo"
		&& lastCommandEvent.Label == L"AddControl",
		L"add command: redo result or label was not published");
	AppendFailure(failures, commandCanvas.GetAllControls().size() == 1
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
		&& commandCanvas.GetAllControls().empty()
		&& deleteCommandMemory > 0 && deleteCommandMemory < 32 * 1024
		&& commandEventCount == 4
		&& lastCommandEvent.Operation == L"DeleteSelection"
		&& lastCommandEvent.Label == L"DeleteSelection",
		L"delete command: result or event was not published");
	auto undoDeleteResult = commandCanvas.UndoCommand();
	AppendFailure(failures,
		undoDeleteResult.HasChanges()
		&& commandEventCount == 5
		&& lastCommandEvent.Operation == L"Undo"
		&& lastCommandEvent.Label == L"DeleteSelection",
		L"delete command: undo result or label was not published");
	AppendFailure(failures, commandCanvas.GetAllControls().size() == 1
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
	AppendFailure(failures, commandCanvas.GetAllControls().empty(),
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
		&& commandCanvas.GetAllControls().empty(),
		L"active transaction: undo was not rejected without mutation");
	auto blockUndoCancel = commandCanvas.CancelDocumentEditTransaction();
	auto undoAfterBlock = commandCanvas.UndoCommand();
	AppendFailure(failures,
		blockUndoCancel.State == DesignerDocumentTransactionState::Canceled
		&& undoAfterBlock.HasChanges()
		&& commandEventCount == 8
		&& lastCommandEvent.Operation == L"Undo"
		&& lastCommandEvent.Label == L"DeleteSelection"
		&& commandCanvas.GetAllControls().size() == 1,
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
		&& commandCanvas.GetAllControls().size() == 1,
		L"active transaction: redo was not rejected without mutation");
	auto blockRedoCancel = commandCanvas.CancelDocumentEditTransaction();
	auto redoAfterBlock = commandCanvas.RedoCommand();
	AppendFailure(failures,
		blockRedoCancel.State == DesignerDocumentTransactionState::Canceled
		&& redoAfterBlock.HasChanges()
		&& commandEventCount == 10
		&& lastCommandEvent.Operation == L"Redo"
		&& lastCommandEvent.Label == L"DeleteSelection"
		&& commandCanvas.GetAllControls().empty(),
		L"active transaction: rejected redo damaged transaction or history");

	DesignerCanvas clipboardCanvas(0, 0, 900, 680);
	(void)clipboardCanvas.ResetDocumentHistoryAsSaved();
	const std::wstring clipboardXaml = LR"xaml(
		<Form xmlns="urn:cui"
		      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
		      x:Name="ClipboardForm">
		  <Panel x:Name="panel1" DesignId="1" Canvas.Left="40" Canvas.Top="50"
		         Width="240" Height="160">
		    <Button x:Name="button1" DesignId="2" Canvas.Left="10" Canvas.Top="12"
		            Width="100" Height="30" Text="Paste" />
		  </Panel>
		</Form>)xaml";
	const auto pasteXaml = clipboardCanvas.PasteControlsFromXamlText(
		clipboardXaml);
	DesignerModel::DesignDocument pastedXamlDocument;
	std::wstring pastedXamlError;
	AppendFailure(failures,
		pasteXaml.HasChanges()
		&& clipboardCanvas.BuildDesignDocument(
			pastedXamlDocument, &pastedXamlError)
		&& pastedXamlDocument.Nodes.size() == 2
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
		&& clipboardCanvas.GetAllControls().empty()
		&& clipboardCanvas.GetSelectedControls().empty(),
		L"clipboard XAML: undo did not remove pasted subtree and selection");
	const auto redoPasteXaml = clipboardCanvas.RedoCommand();
	AppendFailure(failures,
		redoPasteXaml.HasChanges()
		&& clipboardCanvas.GetAllControls().size() == 2
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
	bindingClipboardSource.AddControlToCanvasCore(
		UIClass::UI_TextBox, POINT{ 210, 170 });
	const auto bindingClipboardSourceControl =
		bindingClipboardSource.GetSelectedControl();
	if (bindingClipboardSourceControl)
		bindingClipboardSourceControl->DataBindings[L"Text"] = {
			L"Profile.DisplayName", BindingMode::OneWay,
			DataSourceUpdateMode::OnPropertyChanged, L"" };
	const auto bindingClipboardCopy = bindingClipboardSource.CopySelectedControls();

	DesignerCanvas bindingClipboardTarget(0, 0, 900, 680);
	bindingClipboardTarget.AddControlToCanvasCore(
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
		&& bindingClipboardUndone.Nodes.size() == 1
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
	styleClipboardSource.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 210, 170 });
	const auto styleClipboardSourceControl =
		styleClipboardSource.GetSelectedControl();
	if (styleClipboardSourceControl && styleClipboardSourceControl->ControlInstance)
	{
		styleClipboardSourceControl->ControlInstance->SetStyleId(
			L"SourceButton");
		(void)styleClipboardSourceControl->ControlInstance->AddStyleClass(
			L"primary");
	}
	DesignerStyleSheet sourceClipboardStyle;
	sourceClipboardStyle.Resources = {
		{ L"Accent", { DesignerStyleValueKind::Color, L"#FFFF0000" } }
	};
	DesignerStyleRule sourceClipboardTyped;
	sourceClipboardTyped.HasType = true;
	sourceClipboardTyped.Type = UIClass::UI_Button;
	sourceClipboardTyped.Setters.push_back({
		L"Round", false, {}, { DesignerStyleValueKind::Float, L"2" } });
	DesignerStyleRule sourceClipboardClass;
	sourceClipboardClass.HasType = true;
	sourceClipboardClass.Type = UIClass::UI_Button;
	sourceClipboardClass.Classes = { L"primary" };
	sourceClipboardClass.Setters.push_back({
		L"UnderMouseColor", true, L"Accent", {} });
	DesignerStyleRule sourceClipboardId;
	sourceClipboardId.HasType = true;
	sourceClipboardId.Type = UIClass::UI_Button;
	sourceClipboardId.Id = L"SourceButton";
	sourceClipboardId.Setters.push_back({
		L"Round", false, {}, { DesignerStyleValueKind::Float, L"7" } });
	sourceClipboardStyle.Rules = {
		sourceClipboardTyped, sourceClipboardClass, sourceClipboardId };
	std::wstring styleClipboardSourceError;
	const bool sourceClipboardStyleReady =
		styleClipboardSource.SetDocumentStyleSheet(
			sourceClipboardStyle, &styleClipboardSourceError);
	const auto styleClipboardCopy =
		styleClipboardSource.CopySelectedControls();

	DesignerCanvas styleClipboardTarget(0, 0, 900, 680);
	styleClipboardTarget.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 140, 120 });
	const auto styleClipboardExisting = styleClipboardTarget.GetSelectedControl();
	const auto styleClipboardExistingName = styleClipboardExisting
		? styleClipboardExisting->Name : std::wstring{};
	if (styleClipboardExisting && styleClipboardExisting->ControlInstance)
	{
		styleClipboardExisting->ControlInstance->SetStyleId(L"SourceButton");
		(void)styleClipboardExisting->ControlInstance->AddStyleClass(L"primary");
	}
	DesignerStyleSheet targetClipboardStyle;
	targetClipboardStyle.Resources = {
		{ L"Accent", { DesignerStyleValueKind::Color, L"#FF0000FF" } }
	};
	DesignerStyleRule targetClipboardClass = sourceClipboardClass;
	DesignerStyleRule targetClipboardId = sourceClipboardId;
	targetClipboardId.Setters.front().Literal.Text = L"99";
	targetClipboardStyle.Rules = {
		targetClipboardClass, targetClipboardId };
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
	const auto pastedPreviewRound = pastedPreviewButton
		? pastedPreviewButton->Round : -1.0f;
	const auto pastedPreviewRed = pastedPreviewButton
		? pastedPreviewButton->UnderMouseColor.r : -1.0f;
	const auto pastedPreviewBlue = pastedPreviewButton
		? pastedPreviewButton->UnderMouseColor.b : -1.0f;
	const auto existingPreviewRound = existingPreviewButton
		? existingPreviewButton->Round : -1.0f;
	const auto existingPreviewRed = existingPreviewButton
		? existingPreviewButton->UnderMouseColor.r : -1.0f;
	const auto existingPreviewBlue = existingPreviewButton
		? existingPreviewButton->UnderMouseColor.b : -1.0f;
	const bool styleClipboardPreviewCorrect =
		styleClipboardPasted
		&& styleClipboardPasted != styleClipboardExistingAfterPaste
		&& styleClipboardPasted->ControlInstance
		&& styleClipboardPasted->ControlInstance->GetStyleId().starts_with(
			L"CuiPasteStyle_")
		&& pastedPreviewButton
		&& pastedPreviewButton->Round == 7.0f
		&& pastedPreviewButton->UnderMouseColor.r == 1.0f
		&& pastedPreviewButton->UnderMouseColor.b == 0.0f
		&& existingPreviewButton
		&& existingPreviewButton->Round == 99.0f
		&& existingPreviewButton->UnderMouseColor.r == 0.0f
		&& existingPreviewButton->UnderMouseColor.b == 1.0f;
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
		&& styleClipboardMerged.Nodes.size() == 2
		&& styleClipboardMerged.StyleSheet.Resources.size() == 2
		&& styleClipboardMerged.StyleSheet.Rules.size() == 5
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
		+ L", pastedRound=" + std::to_wstring(pastedPreviewRound)
		+ L", pastedRed=" + std::to_wstring(pastedPreviewRed)
		+ L", pastedBlue=" + std::to_wstring(pastedPreviewBlue)
		+ L", existingRound=" + std::to_wstring(existingPreviewRound)
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
	const auto liveTextPosition = liveXamlText.find(L"Text=\"Paste\"");
	if (liveTextPosition != std::wstring::npos)
		liveXamlText.replace(
			liveTextPosition, std::wstring(L"Text=\"Paste\"").size(),
			L"Text=\"Live Preview\"");
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
			L"<Form xmlns=\"urn:cui\">\n  <Broken>",
			&invalidSyntaxError, &invalidLiveDiagnostic);
	auto semanticLiveXaml = liveXamlText;
	const auto semanticInsert = semanticLiveXaml.find(L"Text=\"Live Preview\"");
	if (semanticInsert != std::wstring::npos)
		semanticLiveXaml.replace(
			semanticInsert, std::wstring(L"Text=\"Live Preview\"").size(),
			L"Visibility=\"Vanished\"");
	DesignerModel::XamlDocumentDiagnostic semanticLiveDiagnostic;
	std::wstring semanticLiveError;
	const bool rejectedSemanticLivePreview =
		semanticInsert != std::wstring::npos
		&& !clipboardCanvas.PreviewXamlDocumentText(
			semanticLiveXaml, &semanticLiveError, &semanticLiveDiagnostic);
	const auto semanticExpectedOffset = semanticLiveXaml.find(L"Visibility");
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
	duplicateCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 180, 160 });
	const auto duplicateSource = duplicateCanvas.GetSelectedControl();
	const auto duplicateSourceName = duplicateSource
		? duplicateSource->Name : std::wstring{};
	const auto duplicateSourceLocation = duplicateSource
		&& duplicateSource->ControlInstance
		? duplicateSource->ControlInstance->Location : POINT{};
	if (duplicateSource)
	{
		duplicateSource->EventHandlers[L"OnMouseClick"] =
			duplicateSourceName + L"_OnMouseClick";
		duplicateSource->EventHandlers[L"OnMouseDoubleClick"] =
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
		&& duplicateCanvas.GetAllControls().size() == 2
		&& duplicateSource && duplicatedControl
		&& duplicatedControl != duplicateSource
		&& duplicatedName != duplicateSourceName
		&& duplicatedControl->StableId != duplicateSource->StableId
		&& duplicatedControl->ControlInstance->Location.x
			== duplicateSourceLocation.x + 12
		&& duplicatedControl->ControlInstance->Location.y
			== duplicateSourceLocation.y + 12
		&& duplicateSource->EventHandlers[L"OnMouseClick"]
			== duplicateSourceName + L"_OnMouseClick"
		&& duplicatedControl->EventHandlers[L"OnMouseClick"]
			== duplicatedName + L"_OnMouseClick"
		&& duplicatedControl->EventHandlers[L"OnMouseDoubleClick"]
			== L"KeepSharedMouseHandler"
		&& duplicateSelectionNotifications >= 2
		&& duplicateCanvas.GetUndoCommandCount() == 1
		&& duplicateCanvas.GetLastCommandOperation() == L"DuplicateSelection",
		L"duplicate: offset copy, identity, property-grid reload, selection, or one-command history failed");
	const auto undoDuplicate = duplicateCanvas.UndoCommand();
	const auto redoDuplicate = duplicateCanvas.RedoCommand();
	AppendFailure(failures,
		undoDuplicate.HasChanges() && redoDuplicate.HasChanges()
		&& duplicateCanvas.GetAllControls().size() == 2
		&& duplicateCanvas.GetSelectedControl()
		&& duplicateCanvas.GetSelectedControl()->Name == duplicatedName
		&& duplicateCanvas.GetSelectedControl()->EventHandlers[L"OnMouseClick"]
			== duplicatedName + L"_OnMouseClick"
		&& duplicateCanvas.GetSelectedControl()->EventHandlers[
			L"OnMouseDoubleClick"] == L"KeepSharedMouseHandler",
		L"duplicate: undo/redo did not restore the copied selection");

	DesignerCanvas stackDuplicateCanvas(0, 0, 900, 680);
	stackDuplicateCanvas.AddControlToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 300, 240 });
	const auto stackDuplicateParent = stackDuplicateCanvas.GetSelectedControl();
	if (stackDuplicateParent && stackDuplicateParent->ControlInstance)
	{
		const POINT inside{
			stackDuplicateParent->ControlInstance->AbsLocation.x
				- stackDuplicateCanvas.AbsLocation.x + 35,
			stackDuplicateParent->ControlInstance->AbsLocation.y
				- stackDuplicateCanvas.AbsLocation.y + 25 };
		for (int index = 0; index < 3; ++index)
			stackDuplicateCanvas.AddControlToCanvasCore(
				UIClass::UI_Button,
				POINT{ inside.x, inside.y + index * 45 });
	}
	const auto stackDuplicateControls = stackDuplicateCanvas.GetAllControls();
	const bool stackDuplicateReady = stackDuplicateParent
		&& stackDuplicateControls.size() >= 4
		&& stackDuplicateControls[1] && stackDuplicateControls[2]
		&& stackDuplicateControls[3];
	const int stackFirstId = stackDuplicateReady
		? stackDuplicateControls[1]->StableId : 0;
	const int stackSourceId = stackDuplicateReady
		? stackDuplicateControls[2]->StableId : 0;
	const int stackLastId = stackDuplicateReady
		? stackDuplicateControls[3]->StableId : 0;
	if (stackDuplicateReady)
		stackDuplicateCanvas.RestoreSelectionByNames(
			{ stackDuplicateControls[2]->Name },
			stackDuplicateControls[2]->Name, false);
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
		&& stackDuplicateChildren[2]->Props["location"].value("x", -1) == 0
		&& stackDuplicateChildren[2]->Props["location"].value("y", -1) == 0;
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
	relativeDuplicateCanvas.AddControlToCanvasCore(
		UIClass::UI_RelativePanel, POINT{ 320, 250 });
	const auto relativeDuplicateParent =
		relativeDuplicateCanvas.GetSelectedControl();
	if (relativeDuplicateParent && relativeDuplicateParent->ControlInstance)
	{
		const POINT inside{
			relativeDuplicateParent->ControlInstance->AbsLocation.x
				- relativeDuplicateCanvas.AbsLocation.x + 70,
			relativeDuplicateParent->ControlInstance->AbsLocation.y
				- relativeDuplicateCanvas.AbsLocation.y + 80 };
		relativeDuplicateCanvas.AddControlToCanvasCore(
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
		&& relativeDuplicateNode->Props.contains("margin")
		&& std::fabs(relativeDuplicateNode->Props["margin"].value(
			"l", -1000.0) - (relativeSourceMargin.Left + 12.0)) < 0.01
		&& std::fabs(relativeDuplicateNode->Props["margin"].value(
			"t", -1000.0) - (relativeSourceMargin.Top + 12.0)) < 0.01
		&& relativeDuplicateNode->Props["location"].value("x", -1) == 0
		&& relativeDuplicateNode->Props["location"].value("y", -1) == 0;
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
		<Form xmlns="urn:cui"
		      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
		      x:Name="ClipboardChild">
		  <Label x:Name="insertLabel1" DesignId="10"
		         Canvas.Left="8" Canvas.Top="9" Width="80" Height="24"
		         Text="Inside" />
		</Form>)xaml";
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
	AppendFailure(failures,
		repeatedPaste1.HasChanges() && repeatedPaste2.HasChanges()
		&& repeatedPaste3.HasChanges() && repeatedPasteCaptured
		&& repeatedPanel1 && repeatedPanel2 && repeatedPanel3
		&& repeatedPanel1->ParentId == 0 && repeatedPanel1->ParentRef.empty()
		&& repeatedPanel2->ParentId == 0 && repeatedPanel2->ParentRef.empty()
		&& repeatedPanel3->ParentId == 0 && repeatedPanel3->ParentRef.empty()
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
		if (!node || !node->Props.is_object()
			|| !node->Props.contains("location")
			|| !node->Props["location"].is_object()) return POINT{};
		const auto& location = node->Props["location"];
		return POINT{
			location.contains("x") && location["x"].is_number()
				? location["x"].get<int>() : 0,
			location.contains("y") && location["y"].is_number()
				? location["y"].get<int>() : 0 };
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
		pointInsidePanel = {
			(*pointPanel)->ControlInstance->AbsLocation.x
				- pointPasteCanvas.AbsLocation.x + 73,
			(*pointPanel)->ControlInstance->AbsLocation.y
				- pointPasteCanvas.AbsLocation.y + 81 };
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

	DesignerCanvas splitPointPasteCanvas(0, 0, 900, 680);
	splitPointPasteCanvas.AddControlToCanvasCore(
		UIClass::UI_SplitContainer, POINT{ 330, 250 });
	const auto splitPointTarget = splitPointPasteCanvas.GetSelectedControl();
	auto* splitPointControl = splitPointTarget
		? dynamic_cast<SplitContainer*>(
			splitPointTarget->ControlInstance) : nullptr;
	POINT pointInsideSecond{};
	if (splitPointControl && splitPointControl->SecondPanel())
	{
		pointInsideSecond = {
			splitPointControl->SecondPanel()->AbsLocation.x
				- splitPointPasteCanvas.AbsLocation.x + 24,
			splitPointControl->SecondPanel()->AbsLocation.y
				- splitPointPasteCanvas.AbsLocation.y + 30 };
	}
	(void)splitPointPasteCanvas.ResetDocumentHistoryAsSaved();
	const auto pasteAtSplitPoint = splitPointControl
		? splitPointPasteCanvas.PasteControlsFromXamlTextAt(
			nestedChildXaml, pointInsideSecond)
		: DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"missing split point target");
	DesignerModel::DesignDocument splitPointDocument;
	std::wstring splitPointError;
	const bool splitPointCaptured = splitPointPasteCanvas.BuildDesignDocument(
		splitPointDocument, &splitPointError);
	const auto* splitPointLabel = findNestedNode(
		splitPointDocument, L"insertLabel1");
	const auto splitPointLocation = clipboardNodeLocation(splitPointLabel);
	AppendFailure(failures,
		pasteAtSplitPoint.HasChanges() && splitPointCaptured
		&& splitPointTarget && splitPointLabel
		&& splitPointLabel->ParentId == splitPointTarget->StableId
		&& splitPointLabel->Extra.is_object()
		&& splitPointLabel->Extra.value(
			"splitRegion", std::string{}) == "panel2"
		&& splitPointLocation.x == 24 && splitPointLocation.y == 30
		&& splitPointPasteCanvas.GetUndoCommandCount() == 1,
		L"clipboard placement: paste-here did not resolve Split Second region"
		+ std::wstring(L" [paste=") + SelfTestFlag(pasteAtSplitPoint.HasChanges())
		+ L", capture=" + SelfTestFlag(splitPointCaptured)
		+ L", location=" + std::to_wstring(splitPointLocation.x) + L","
		+ std::to_wstring(splitPointLocation.y)
		+ L", parent=" + (splitPointLabel
			? std::to_wstring(splitPointLabel->ParentId) : L"missing")
		+ L", expectedParent=" + (splitPointTarget
			? std::to_wstring(splitPointTarget->StableId) : L"missing")
		+ L", region=" + (splitPointLabel && splitPointLabel->Extra.is_object()
			? (splitPointLabel->Extra.value(
				"splitRegion", std::string{}) == "panel2"
				? L"panel2" : L"other") : L"missing")
		+ L", error=" + splitPointError + L"]");

	DesignerCanvas stackPointPasteCanvas(0, 0, 900, 680);
	stackPointPasteCanvas.AddControlToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 300, 240 });
	const auto stackPointTarget = stackPointPasteCanvas.GetSelectedControl();
	auto* stackPointControl = stackPointTarget
		? dynamic_cast<StackPanel*>(stackPointTarget->ControlInstance) : nullptr;
	if (stackPointControl)
	{
		const POINT inside{
			stackPointControl->AbsLocation.x
				- stackPointPasteCanvas.AbsLocation.x + 30,
			stackPointControl->AbsLocation.y
				- stackPointPasteCanvas.AbsLocation.y + 25 };
		stackPointPasteCanvas.AddControlToCanvasCore(
			UIClass::UI_Button, inside);
		stackPointPasteCanvas.AddControlToCanvasCore(
			UIClass::UI_Button, POINT{ inside.x, inside.y + 60 });
	}
	POINT beforeStackSecond{};
	if (stackPointControl && stackPointControl->Count >= 2)
	{
		auto* second = stackPointControl->operator[](1);
		beforeStackSecond = {
			second->AbsLocation.x - stackPointPasteCanvas.AbsLocation.x + 5,
			second->AbsLocation.y - stackPointPasteCanvas.AbsLocation.y + 1 };
	}
	(void)stackPointPasteCanvas.ResetDocumentHistoryAsSaved();
	const auto pasteIntoStackMiddle = stackPointControl
		&& stackPointControl->Count >= 2
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
		&& stackAfterUndo->ControlInstance->Count == 2,
		L"clipboard placement: StackPanel paste-here did not insert at the pointed boundary or undo atomically"
		+ std::wstring(L" [paste=")
		+ SelfTestFlag(pasteIntoStackMiddle.HasChanges())
		+ L", capture=" + SelfTestFlag(stackPointCaptured)
		+ L", order=" + SelfTestFlag(stackOrderCorrect)
		+ L", location=" + std::to_wstring(stackPointLabelLocation.x)
		+ L"," + std::to_wstring(stackPointLabelLocation.y)
		+ L", error=" + stackPointError + L"]");

	const std::wstring gridPasteTargetXaml = LR"xaml(
		<Form xmlns="urn:cui"
		      xmlns:x="http://schemas.microsoft.com/winfx/2006/xaml"
		      x:Name="GridPasteForm">
		  <GridPanel x:Name="grid1" DesignId="1" Canvas.Left="80"
		             Canvas.Top="70" Width="240" Height="180">
		    <Grid.RowDefinitions>
		      <RowDefinition Height="*" />
		      <RowDefinition Height="*" />
		    </Grid.RowDefinitions>
		    <Grid.ColumnDefinitions>
		      <ColumnDefinition Width="*" />
		      <ColumnDefinition Width="*" />
		    </Grid.ColumnDefinitions>
		  </GridPanel>
		</Form>)xaml";
	DesignerCanvas gridPointPasteCanvas(0, 0, 900, 680);
	const auto gridPointSetup =
		gridPointPasteCanvas.PasteControlsFromXamlTextInPlace(
			gridPasteTargetXaml);
	const auto gridPointTarget = gridPointPasteCanvas.GetSelectedControl();
	auto* gridPointControl = gridPointTarget
		? dynamic_cast<GridPanel*>(gridPointTarget->ControlInstance) : nullptr;
	POINT gridSecondCellPoint{};
	if (gridPointControl)
	{
		gridPointControl->PerformLayout();
		gridSecondCellPoint = {
			gridPointControl->AbsLocation.x
				- gridPointPasteCanvas.AbsLocation.x + 180,
			gridPointControl->AbsLocation.y
				- gridPointPasteCanvas.AbsLocation.y + 130 };
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
		&& gridPointLabel->Props.value("gridRow", -1) == 1
		&& gridPointLabel->Props.value("gridColumn", -1) == 1
		&& gridPointLabel->Props.value("gridRowSpan", -1) == 1
		&& gridPointLabel->Props.value("gridColumnSpan", -1) == 1
		&& gridPointLabel->Props.value("hAlign", std::string{}) == "Stretch"
		&& gridPointLabel->Props.value("vAlign", std::string{}) == "Stretch"
		&& gridPointLocation.x == 0 && gridPointLocation.y == 0
		&& gridPointPasteCanvas.GetUndoCommandCount() == 1,
		L"clipboard placement: GridPanel paste-here did not target the pointed cell"
		+ std::wstring(L" [setup=") + SelfTestFlag(gridPointSetup.HasChanges())
		+ L", paste=" + SelfTestFlag(pasteIntoGridCell.HasChanges())
		+ L", capture=" + SelfTestFlag(gridPointCaptured)
		+ L", row=" + (gridPointLabel
			? std::to_wstring(gridPointLabel->Props.value("gridRow", -1))
			: L"missing")
		+ L", column=" + (gridPointLabel
			? std::to_wstring(gridPointLabel->Props.value("gridColumn", -1))
			: L"missing")
		+ L", error=" + gridPointError + L"]");

	DesignerCanvas relativePointPasteCanvas(0, 0, 900, 680);
	relativePointPasteCanvas.AddControlToCanvasCore(
		UIClass::UI_RelativePanel, POINT{ 320, 250 });
	const auto relativePointTarget =
		relativePointPasteCanvas.GetSelectedControl();
	POINT relativePastePoint{};
	if (relativePointTarget && relativePointTarget->ControlInstance)
	{
		relativePastePoint = {
			relativePointTarget->ControlInstance->AbsLocation.x
				- relativePointPasteCanvas.AbsLocation.x + 65,
			relativePointTarget->ControlInstance->AbsLocation.y
				- relativePointPasteCanvas.AbsLocation.y + 75 };
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
		&& relativePointLabel->Props.contains("margin")
		&& relativePointLabel->Props["margin"].value("l", -1) == 65
		&& relativePointLabel->Props["margin"].value("t", -1) == 75
		&& relativePointLocation.x == 0 && relativePointLocation.y == 0
		&& relativePointPasteCanvas.GetUndoCommandCount() == 1,
		L"clipboard placement: RelativePanel paste-here did not convert the point to Margin"
		+ std::wstring(L" [paste=")
		+ SelfTestFlag(pasteIntoRelativePoint.HasChanges())
		+ L", capture=" + SelfTestFlag(relativePointCaptured)
		+ L", error=" + relativePointError + L"]");

	DesignerCanvas arrangeCanvas(0, 0, 900, 680);
	arrangeCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 140, 130 });
	arrangeCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 330, 200 });
	arrangeCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 570, 290 });
	const auto arrangeFirst = arrangeCanvas.GetAllControls().size() > 0
		? arrangeCanvas.GetAllControls()[0] : nullptr;
	const auto arrangeSecond = arrangeCanvas.GetAllControls().size() > 1
		? arrangeCanvas.GetAllControls()[1] : nullptr;
	const auto arrangeThird = arrangeCanvas.GetAllControls().size() > 2
		? arrangeCanvas.GetAllControls()[2] : nullptr;
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
		first->Size = { 80, 30 };
		second->Size = { 110, 42 };
		third->Size = { 140, 54 };
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
			&& first->Location.x == second->Location.x
			&& third->Location.x == second->Location.x
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
			&& first->Size.cx == second->Size.cx
			&& first->Size.cy == second->Size.cy
			&& third->Size.cx == second->Size.cx
			&& third->Size.cy == second->Size.cy;
		const bool sameSizeRestored = arrangeCanvas.UndoCommand().HasChanges()
			&& first->Size.cx == 80 && first->Size.cy == 30
			&& second->Size.cx == 110 && second->Size.cy == 42
			&& third->Size.cx == 140 && third->Size.cy == 54;

		const auto distribute = arrangeCanvas.ArrangeSelection(
			DesignerSelectionArrangeAction::DistributeHorizontally);
		const int firstGap = second->Location.x
			- (first->Location.x + first->Size.cx);
		const int secondGap = third->Location.x
			- (second->Location.x + second->Size.cx);
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
			for (auto* control : first->Parent->GetChildrenInZOrder())
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
	managedArrangeCanvas.AddControlToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 260, 220 });
	const auto managedParent = managedArrangeCanvas.GetSelectedControl();
	if (managedParent && managedParent->ControlInstance)
	{
		const POINT inside{
			managedParent->ControlInstance->AbsLocation.x
				- managedArrangeCanvas.AbsLocation.x + 40,
			managedParent->ControlInstance->AbsLocation.y
				- managedArrangeCanvas.AbsLocation.y + 35
		};
		managedArrangeCanvas.AddControlToCanvasCore(
			UIClass::UI_Button, inside);
		managedArrangeCanvas.AddControlToCanvasCore(
			UIClass::UI_Button, POINT{ inside.x + 20, inside.y + 55 });
	}
	if (managedArrangeCanvas.GetAllControls().size() >= 3)
	{
		const auto managedFirst = managedArrangeCanvas.GetAllControls()[1];
		const auto managedSecond = managedArrangeCanvas.GetAllControls()[2];
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
	lockCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 210, 180 });
	auto lockedControl = lockCanvas.GetSelectedControl();
	const auto lockedIdentity = lockedControl;
	const POINT unlockedLocation = lockedControl && lockedControl->ControlInstance
		? lockedControl->ControlInstance->Location : POINT{ 0, 0 };
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
		const auto size = runtime->ActualSize();
		const POINT center{
			runtime->AbsLocation.x - lockCanvas.AbsLocation.x + size.cx / 2,
			runtime->AbsLocation.y - lockCanvas.AbsLocation.y + size.cy / 2 };
		(void)lockCanvas.ProcessMessage(
			WM_LBUTTONDOWN, MK_LBUTTON, 0, center.x, center.y);
		(void)lockCanvas.ProcessMessage(
			WM_MOUSEMOVE, MK_LBUTTON, 0, center.x + 25, center.y + 15);
		(void)lockCanvas.ProcessMessage(
			WM_LBUTTONUP, 0, 0, center.x + 25, center.y + 15);
	}
	const bool lockedPlacementUnchanged = lockedControl
		&& lockedControl->ControlInstance
		&& lockedControl->ControlInstance->Location.x == unlockedLocation.x
		&& lockedControl->ControlInstance->Location.y == unlockedLocation.y
		&& lockCanvas.GetUndoCommandCount() == lockedUndoCount;
	AppendFailure(failures,
		lockedControl && lockResult.HasChanges()
		&& lockedControl->IsLocked
		&& lockedCaptured && lockedDocument.Nodes.size() == 1
		&& lockedDocument.Nodes.front().Locked
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
		&& restoredLockCanvas.GetAllControls().size() == 1
		&& restoredLockCanvas.GetAllControls().front()->IsLocked;
	AppendFailure(failures,
		unlockedByUndo && relockedByRedo && restoredLocked,
		L"design lock: undo/redo or XAML materialization lost lock metadata"
		+ std::wstring(L" [undo=") + SelfTestFlag(unlockedByUndo)
		+ L", redo=" + SelfTestFlag(relockedByRedo)
		+ L", restore=" + SelfTestFlag(restoredLocked)
		+ L", error=" + lockedError + L"]");

	DesignerCanvas viewCanvas(0, 0, 400, 300);
	viewCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 180, 140 });
	const auto transformedViewControl = viewCanvas.GetAllControls().empty()
		? nullptr : viewCanvas.GetAllControls().front();
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
	const bool panDown = viewCanvas.ProcessMessage(
		WM_MBUTTONDOWN, 0, 0, 100, 100);
	const bool panMove = viewCanvas.ProcessMessage(
		WM_MOUSEMOVE, 0, 0, 135, 125);
	const bool panUp = viewCanvas.ProcessMessage(
		WM_MBUTTONUP, 0, 0, 135, 125);
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
		L"canvas view: fit, focal zoom, descendant rendering, pan, reset, or non-document state failed");

	DesignerCanvas contextCanvas(0, 0, 900, 680);
	contextCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 170, 150 });
	contextCanvas.AddControlToCanvasCore(
		UIClass::UI_Label, POINT{ 390, 240 });
	const auto contextFirst = contextCanvas.GetAllControls().size() > 0
		? contextCanvas.GetAllControls()[0] : nullptr;
	const auto contextSecond = contextCanvas.GetAllControls().size() > 1
		? contextCanvas.GetAllControls()[1] : nullptr;
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
		const auto size = contextFirst->ControlInstance->ActualSize();
		const POINT center{
			contextFirst->ControlInstance->AbsLocation.x
				- contextCanvas.AbsLocation.x + size.cx / 2,
			contextFirst->ControlInstance->AbsLocation.y
				- contextCanvas.AbsLocation.y + size.cy / 2
		};
		contextHitHandled = contextCanvas.ProcessMessage(
			WM_RBUTTONUP, 0, 0, center.x, center.y);
	}
	const bool contextHitSelected = contextHitHandled
		&& contextRequestCount == 1 && lastContextRequest.HasSelection
		&& contextCanvas.GetSelectedControl() == contextFirst;
	const bool contextBlankHandled = contextCanvas.ProcessMessage(
		WM_RBUTTONUP, 0, 0, 700, 540);
	const bool blankClearedSelection = contextBlankHandled
		&& contextRequestCount == 2 && !lastContextRequest.HasSelection
		&& contextCanvas.GetSelectedControls().empty();
	const bool selectedAll = contextCanvas.SelectAllInCurrentContainer(false)
		&& contextCanvas.GetSelectedControls().size() == 2;
	const bool keyboardMenuHandled = contextCanvas.ProcessMessage(
		WM_KEYDOWN, VK_APPS, 0, 0, 0);
	AppendFailure(failures,
		contextHitSelected && blankClearedSelection && selectedAll
		&& keyboardMenuHandled && contextRequestCount == 3
		&& lastContextRequest.HasSelection,
		L"canvas context menu: hit selection, blank request, select-all, or keyboard request failed");

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
		&& !commandSurfaceDesigner._btnUndo->Enable
		&& !commandSurfaceDesigner._btnRedo->Enable
		&& commandSurfaceDesigner._canvasMenu->ItemCount() == 18
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"原位粘贴", false) != nullptr
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"原位粘贴", false)->Shortcut == L"Ctrl+Shift+V"
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"粘贴到此处", false) != nullptr
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"锁定控件", false) != nullptr
		&& commandSurfaceDesigner._canvasMenu->FindItemByText(
			L"锁定控件", false)->Shortcut == L"Ctrl+L"
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
	const auto* gridVisibleItem = commandSurfaceDesigner._gridMenu
		? commandSurfaceDesigner._gridMenu->FindItemByText(L"显示网格")
		: nullptr;
	const auto* gridSizeItem = commandSurfaceDesigner._gridMenu
		? commandSurfaceDesigner._gridMenu->FindItemByText(L"网格间距 20 DIP")
		: nullptr;
	const bool gridSettingsReady = commandSurfaceDesigner._canvas
		&& !commandSurfaceDesigner._canvas->IsGridVisible()
		&& !commandSurfaceDesigner._canvas->IsSnapToGridEnabled()
		&& !commandSurfaceDesigner._canvas->IsSnapToGuidesEnabled()
		&& commandSurfaceDesigner._canvas->GetGridSize() == 20
		&& commandSurfaceDesigner._canvas->GetCurrentDocumentStateId()
			== gridStateBefore
		&& gridVisibleItem && !gridVisibleItem->Checked
		&& gridSizeItem && gridSizeItem->Checked
		&& commandSurfaceDesigner._btnGridSettings->Text == L"网格 20";
	const auto commandSurfaceAdd = commandSurfaceDesigner._canvas
		? commandSurfaceDesigner._canvas->AddControlToCanvas(
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
		&& commandSurfaceDesigner._btnUndo->Enable
		&& !commandSurfaceDesigner._btnRedo->Enable
		&& commandSurfaceUndo && commandSurfaceUndo->Enable
		&& commandSurfaceUndo->Text.find(L"添加控件") != std::wstring::npos
		&& commandSurfaceRedo && !commandSurfaceRedo->Enable
		&& commandSurfaceCut && commandSurfaceCut->Enable
		&& commandSurfaceDesigner._canvasMenu->IsOpen();
	commandSurfaceDesigner._canvasMenu->Hide();
	commandSurfaceDesigner.OnUndoClick();
	const bool commandSurfaceAfterUndo =
		!commandSurfaceDesigner._btnUndo->Enable
		&& commandSurfaceDesigner._btnRedo->Enable
		&& commandSurfaceDesigner._canvas->GetAllControls().empty();
	commandSurfaceDesigner.OnRedoClick();
	const bool commandSurfaceAfterRedo =
		commandSurfaceDesigner._btnUndo->Enable
		&& !commandSurfaceDesigner._btnRedo->Enable
		&& commandSurfaceDesigner._canvas->GetAllControls().size() == 1;
	AppendFailure(failures,
		commandSurfaceInitial && commandSurfaceAfterAdd
		&& commandSurfaceAfterUndo && commandSurfaceAfterRedo
		&& gridSettingsReady,
		L"designer command surface: toolbar, context menu, history, or session-only grid settings failed");

	Designer contextPasteDesigner;
	contextPasteDesigner.InitializeComponents();
	const auto contextPasteAdd = contextPasteDesigner._canvas
		? contextPasteDesigner._canvas->AddControlToCanvas(
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
		&& contextPasteDesigner._btnPaste->Enable;
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
		&& !contextPasteDesigner._btnPaste->Enable
		&& blockedContextPaste && !blockedContextPaste->Enable
		&& blockedContextPasteInPlace && !blockedContextPasteInPlace->Enable
		&& blockedContextPasteHere && !blockedContextPasteHere->Enable;
	if (contextPasteDesigner._canvasMenu)
		contextPasteDesigner._canvasMenu->Hide();
	if (contextPasteDesigner._canvas)
		(void)contextPasteDesigner._canvas->RollbackDocumentEditTransaction();
	contextPasteDesigner.RefreshCommandAvailability();
	if (contextPasteDesigner._btnPaste)
		contextPasteDesigner._btnPaste->Enable = false;
	const bool contextPasteClipboardUpdateHandled =
		contextPasteDesigner.ProcessMessage(
			WM_CLIPBOARDUPDATE, 0, 0, 0, 0)
		&& contextPasteDesigner._btnPaste
		&& contextPasteDesigner._btnPaste->Enable;
	const bool contextPasteEmptyTextPublished =
		ReplaceClipboardTextForSelfTest(L"");
	bool contextPasteEmptyTextDisabled = false;
	for (int attempt = 0;
		attempt < 20 && !contextPasteEmptyTextDisabled; ++attempt)
	{
		const bool handled = contextPasteDesigner.ProcessMessage(
			WM_CLIPBOARDUPDATE, 0, 0, 0, 0);
		contextPasteEmptyTextDisabled = handled
			&& contextPasteDesigner._btnPaste
			&& !contextPasteDesigner._btnPaste->Enable
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
		&& contextPasteDesigner._btnPaste->Enable;
	contextPasteDesigner.OnCanvasContextMenuRequested(
		DesignerCanvasContextMenuEventArgs{
			requestedPasteViewPoint, true });
	auto* pasteHereCommand = contextPasteDesigner._canvasMenu
		? contextPasteDesigner._canvasMenu->FindItemByText(
			L"粘贴到此处", false) : nullptr;
	if (pasteHereCommand)
		contextPasteDesigner.OnCanvasMenuCommand(pasteHereCommand->Id);
	if (contextPasteDesigner._canvasMenu)
		contextPasteDesigner._canvasMenu->Hide();
	const auto contextPastedControl = contextPasteDesigner._canvas
		? contextPasteDesigner._canvas->GetSelectedControl() : nullptr;
	POINT contextPastedCanvasLocation{};
	if (contextPastedControl && contextPastedControl->ControlInstance
		&& contextPasteDesigner._canvas)
	{
		contextPastedCanvasLocation = {
			contextPastedControl->ControlInstance->AbsLocation.x
				- contextPasteDesigner._canvas->AbsLocation.x,
			contextPastedControl->ControlInstance->AbsLocation.y
				- contextPasteDesigner._canvas->AbsLocation.y };
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
		pasteHereCommand && pasteHereCommand->Enable
		&& contextPasteDesigner._hasCanvasContextPastePoint
		&& contextPasteDesigner._canvasContextPastePoint.x
			== requestedPastePoint.x
		&& contextPasteDesigner._canvasContextPastePoint.y
			== requestedPastePoint.y
		&& contextPastedControl
		&& contextPastedCanvasLocation.x == requestedPastePoint.x
		&& contextPastedCanvasLocation.y == requestedPastePoint.y
		&& undoContextPaste.HasChanges()
		&& contextPasteDesigner._canvas->GetAllControls().size() == 1,
		L"designer context paste: view-to-canvas point, command route, placement, or one-step Undo failed");

	Designer outlineDesigner;
	outlineDesigner.InitializeComponents();
	outlineDesigner._canvas->AddControlToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 280, 210 });
	const auto outlineParent = outlineDesigner._canvas->GetSelectedControl();
	if (outlineParent && outlineParent->ControlInstance)
	{
		const POINT inside{
			outlineParent->ControlInstance->AbsLocation.x
				- outlineDesigner._canvas->AbsLocation.x + 30,
			outlineParent->ControlInstance->AbsLocation.y
				- outlineDesigner._canvas->AbsLocation.y + 30 };
		outlineDesigner._canvas->AddControlToCanvasCore(
			UIClass::UI_Button, inside);
	}
	const auto outlineChild = outlineDesigner._canvas->GetSelectedControl();
	if (outlineChild && outlineChild->ControlInstance)
		outlineChild->ControlInstance->Visible = false;
	if (outlineParent) outlineParent->IsLocked = true;
	outlineDesigner.RebuildDocumentOutline();
	outlineDesigner.SetSidebarView(true);
	TreeNode* outlineParentNode = outlineParent
		? outlineDesigner._outlineNodesByStableId[outlineParent->StableId]
		: nullptr;
	TreeNode* outlineChildNode = outlineChild
		? outlineDesigner._outlineNodesByStableId[outlineChild->StableId]
		: nullptr;
	const bool outlineNested = outlineParentNode && outlineChildNode
		&& std::find(
			outlineParentNode->Children.begin(),
			outlineParentNode->Children.end(), outlineChildNode)
			!= outlineParentNode->Children.end();
	const bool outlineHiddenMarked = outlineChildNode
		&& outlineChildNode->Text.find(L"[隐藏]") != std::wstring::npos;
	const bool outlineLockedMarked = outlineParentNode
		&& outlineParentNode->Text.rfind(L"[锁定]", 0) == 0;
	if (outlineChildNode)
	{
		outlineDesigner._outlineTree->SelectedNode = outlineChildNode;
		outlineDesigner.OnDocumentOutlineSelectionChanged();
	}
	const bool outlineSelectedHidden = outlineChild
		&& outlineDesigner._canvas->GetSelectedControl() == outlineChild;
	if (outlineDesigner._outlineFormNode)
	{
		outlineDesigner._outlineTree->SelectedNode =
			outlineDesigner._outlineFormNode;
		outlineDesigner.OnDocumentOutlineSelectionChanged();
	}
	const bool outlineSelectedForm =
		outlineDesigner._canvas->GetSelectedControls().empty();
	outlineDesigner._canvas->AddControlToCanvasCore(
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
		&& outlineChild->DesignerParent == nullptr
		&& outlineChild->ControlInstance->Parent
			== outlineRootSibling->ControlInstance->Parent
		&& outlineChild->ControlInstance->Parent->IndexOfControl(
			outlineChild->ControlInstance)
			< outlineChild->ControlInstance->Parent->IndexOfControl(
				outlineRootSibling->ControlInstance);
	const auto outlineMoveUndo = outlineDesigner._canvas->UndoCommand();
	const bool outlineMoveUndone = outlineMoveUndo.HasChanges()
		&& outlineChild && outlineParent
		&& outlineChild->DesignerParent == outlineParent->ControlInstance
		&& outlineChild->ControlInstance->Parent
			== outlineParent->ControlInstance;
	const auto outlineMoveRedo = outlineDesigner._canvas->RedoCommand();
	const bool outlineMoveRedone = outlineMoveRedo.HasChanges()
		&& outlineChild && outlineRootSibling
		&& outlineChild->DesignerParent == nullptr
		&& outlineChild->ControlInstance->Parent
			== outlineRootSibling->ControlInstance->Parent;
	const auto latestParentNode = outlineParent
		? outlineDesigner._outlineNodesByStableId[outlineParent->StableId]
		: nullptr;
	if (latestParentNode) latestParentNode->SetExpanded(false, false);
	outlineDesigner.RebuildDocumentOutline();
	const auto rebuiltParentNode = outlineParent
		? outlineDesigner._outlineNodesByStableId[outlineParent->StableId]
		: nullptr;
	AppendFailure(failures,
		outlineDesigner._btnToolboxView
		&& outlineDesigner._btnOutlineView
		&& outlineDesigner._outlineTree
		&& outlineDesigner._outlineTree->Visible
		&& !outlineDesigner._toolBox->Visible
		&& outlineNested && outlineHiddenMarked && outlineLockedMarked
		&& outlineSelectedHidden && outlineSelectedForm
		&& outlineMovedToRoot && outlineMoveUndone && outlineMoveRedone
		&& rebuiltParentNode && !rebuiltParentNode->Expand,
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
		outlineShortcutDesigner._outlineTree->SelectedNode = node->second;
		outlineShortcutDesigner.OnDocumentOutlineSelectionChanged();
		return true;
	};
	const bool outlineShortcutChildSelected = selectOutlineNode(L"button1");
	const bool outlineShortcutTreeFocused =
		outlineShortcutDesigner.Selected == outlineShortcutDesigner._outlineTree;
	const bool outlineShortcutCopied = outlineShortcutDesigner.QueueOutlineShortcut(
		'C', true, false);
	const bool outlineShortcutParentSelected = selectOutlineNode(L"panel1");
	outlineShortcutDesigner.SetSelectedControl(
		outlineShortcutDesigner._outlineTree, false);
	(void)outlineShortcutDesigner.ProcessMessage(
		WM_KEYDOWN, VK_CONTROL, 0, 0, 0);
	const bool outlineShortcutPasted = outlineShortcutDesigner.ProcessMessage(
		WM_KEYDOWN, 'V', 0, 0, 0);
	const size_t outlineShortcutCountAfterPaste =
		outlineShortcutDesigner._canvas->GetAllControls().size();
	const bool outlineShortcutCharacterSuppressed =
		outlineShortcutDesigner.ProcessMessage(WM_CHAR, L'\x16', 0, 0, 0)
		&& outlineShortcutDesigner._canvas->GetAllControls().size()
			== outlineShortcutCountAfterPaste;
	(void)outlineShortcutDesigner.ProcessMessage(
		WM_KEYUP, 'V', 0, 0, 0);
	(void)outlineShortcutDesigner.ProcessMessage(
		WM_KEYUP, VK_CONTROL, 0, 0, 0);
	const bool outlineShortcutParentReselected = selectOutlineNode(L"panel1");
	const bool outlineShortcutInPlacePasted =
		outlineShortcutDesigner.QueueOutlineShortcut('V', true, true);
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
		outlineShortcutDesigner.QueueOutlineShortcut('Z', true, false)
		&& outlineShortcutDesigner._canvas->GetAllControls().size() == 3;
	const bool outlineShortcutUndone = outlineShortcutDesigner.QueueOutlineShortcut(
		'Z', true, false)
		&& outlineShortcutDesigner._canvas->GetAllControls().size() == 2;
	const bool outlineShortcutReselected = selectOutlineNode(L"button1");
	const auto outlineShortcutLockTarget =
		outlineShortcutDesigner._canvas->GetSelectedControl();
	const bool outlineShortcutLocked =
		outlineShortcutDesigner.QueueOutlineShortcut('L', true, false)
		&& outlineShortcutLockTarget && outlineShortcutLockTarget->IsLocked;
	const bool outlineShortcutUnlocked =
		outlineShortcutDesigner.QueueOutlineShortcut('L', true, false)
		&& outlineShortcutLockTarget && !outlineShortcutLockTarget->IsLocked;
	const bool outlineShortcutDuplicated =
		outlineShortcutDesigner.QueueOutlineShortcut('D', true, false)
		&& outlineShortcutDesigner._canvas->GetAllControls().size() == 3;
	const bool outlineShortcutDeleted =
		outlineShortcutDesigner.QueueOutlineShortcut(
			VK_DELETE, false, false)
		&& outlineShortcutDesigner._canvas->GetAllControls().size() == 2;
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
	subtreeCanvas.AddControlToCanvasCore(
		UIClass::UI_StackPanel, POINT{ 260, 220 });
	const auto subtreeRoot = subtreeCanvas.GetSelectedControl();
	auto* const subtreeRootRuntime = subtreeRoot
		? subtreeRoot->ControlInstance : nullptr;
	if (subtreeRootRuntime)
	{
		const POINT inside{
			subtreeRootRuntime->AbsLocation.x - subtreeCanvas.AbsLocation.x + 60,
			subtreeRootRuntime->AbsLocation.y - subtreeCanvas.AbsLocation.y + 50
		};
		subtreeCanvas.AddControlToCanvasCore(UIClass::UI_Button, inside);
		subtreeCanvas.AddControlToCanvasCore(
			UIClass::UI_Label, POINT{ inside.x + 20, inside.y + 70 });
	}
	const auto subtreeFirstChild = subtreeCanvas.GetAllControls().size() > 1
		? subtreeCanvas.GetAllControls()[1] : nullptr;
	const auto subtreeSecondChild = subtreeCanvas.GetAllControls().size() > 2
		? subtreeCanvas.GetAllControls()[2] : nullptr;
	auto* const subtreeFirstRuntime = subtreeFirstChild
		? subtreeFirstChild->ControlInstance : nullptr;
	auto* const subtreeSecondRuntime = subtreeSecondChild
		? subtreeSecondChild->ControlInstance : nullptr;
	DesignerModel::DesignDocument subtreeBaseline;
	std::wstring subtreeBaselineError;
	const bool subtreeSetup = subtreeRoot && subtreeRootRuntime
		&& subtreeFirstChild && subtreeFirstRuntime
		&& subtreeSecondChild && subtreeSecondRuntime
		&& subtreeRootRuntime->Count == 2
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
			&& subtreeCanvas.GetAllControls().empty()
			&& deleteSubtreeMemory > 0
			&& deleteSubtreeMemory < 64 * 1024,
			L"subtree delta: nested delete retained a full document or left wrappers");
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
			&& subtreeRootRuntime->Count == 2
			&& subtreeRootRuntime->operator[](0) == subtreeFirstRuntime
			&& subtreeRootRuntime->operator[](1) == subtreeSecondRuntime
			&& subtreeCanvas.GetSelectedControl() == subtreeRoot,
			L"subtree delta: nested undo lost document, order, identity, or selection");
		AppendFailure(failures,
			subtreeCanvas.RedoCommand().HasChanges()
			&& subtreeCanvas.GetAllControls().empty(),
			L"subtree delta: nested redo did not detach the full subtree");
	}

	DesignerCanvas siblingDeleteCanvas(0, 0, 900, 680);
	siblingDeleteCanvas.AddControlToCanvasCore(
		UIClass::UI_Panel, POINT{ 300, 240 });
	const auto siblingParent = siblingDeleteCanvas.GetSelectedControl();
	auto* const siblingParentRuntime = siblingParent
		? siblingParent->ControlInstance : nullptr;
	if (siblingParentRuntime)
	{
		const POINT inside{
			siblingParentRuntime->AbsLocation.x
				- siblingDeleteCanvas.AbsLocation.x + 55,
			siblingParentRuntime->AbsLocation.y
				- siblingDeleteCanvas.AbsLocation.y + 45
		};
		for (int index = 0; index < 3; ++index)
			siblingDeleteCanvas.AddControlToCanvasCore(
				UIClass::UI_Button,
				POINT{ inside.x + index * 45, inside.y + index * 45 });
	}
	const auto siblingFirst = siblingDeleteCanvas.GetAllControls().size() > 1
		? siblingDeleteCanvas.GetAllControls()[1] : nullptr;
	const auto siblingMiddle = siblingDeleteCanvas.GetAllControls().size() > 2
		? siblingDeleteCanvas.GetAllControls()[2] : nullptr;
	const auto siblingLast = siblingDeleteCanvas.GetAllControls().size() > 3
		? siblingDeleteCanvas.GetAllControls()[3] : nullptr;
	auto* const siblingFirstRuntime = siblingFirst
		? siblingFirst->ControlInstance : nullptr;
	auto* const siblingMiddleRuntime = siblingMiddle
		? siblingMiddle->ControlInstance : nullptr;
	auto* const siblingLastRuntime = siblingLast
		? siblingLast->ControlInstance : nullptr;
	const bool siblingSetup = siblingParentRuntime
		&& siblingFirstRuntime && siblingMiddleRuntime && siblingLastRuntime
		&& siblingParentRuntime->Count == 3;
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
			&& siblingParentRuntime->Count == 1
			&& siblingParentRuntime->operator[](0) == siblingMiddleRuntime,
			L"subtree delta: multi-root delete damaged the remaining sibling");
		const auto undoSiblings = siblingDeleteCanvas.UndoCommand();
		AppendFailure(failures,
			undoSiblings.HasChanges()
			&& siblingParentRuntime->Count == 3
			&& siblingParentRuntime->operator[](0) == siblingFirstRuntime
			&& siblingParentRuntime->operator[](1) == siblingMiddleRuntime
			&& siblingParentRuntime->operator[](2) == siblingLastRuntime
			&& FindControl(siblingDeleteCanvas, siblingFirst->Name)
				== siblingFirst
			&& FindControl(siblingDeleteCanvas, siblingLast->Name)
				== siblingLast,
			L"subtree delta: multi-root undo lost sibling order or identity");
	}

	DesignerCanvas guardedAddCanvas(0, 0, 800, 640);
	const auto guardedAdd = guardedAddCanvas.AddControlToCanvas(
		UIClass::UI_Button, POINT{ 150, 150 });
	const auto guardedAddIdentity = guardedAddCanvas.GetSelectedControl();
	auto* const guardedAddRuntime = guardedAddIdentity
		? guardedAddIdentity->ControlInstance : nullptr;
	const auto guardedAddName = guardedAddIdentity
		? guardedAddIdentity->Name : std::wstring{};
	const auto guardedAddText = guardedAddRuntime
		? guardedAddRuntime->Text : std::wstring{};
	if (guardedAddRuntime) guardedAddRuntime->Text = L"ExternalMutation";
	const auto guardedAddUndo = guardedAddCanvas.UndoCommand();
	AppendFailure(failures,
		guardedAdd.HasChanges()
		&& guardedAddUndo.State == DesignerDocumentTransactionState::Failed
		&& !guardedAddUndo.DocumentRestored
		&& guardedAddCanvas.GetUndoCommandCount() == 1
		&& guardedAddCanvas.GetAllControls().size() == 1
		&& guardedAddCanvas.GetSelectedControl() == guardedAddIdentity
		&& guardedAddRuntime
		&& guardedAddRuntime->Text == L"ExternalMutation",
		L"subtree delta: mismatched Add endpoint damaged state or history");
	if (guardedAddRuntime) guardedAddRuntime->Text = guardedAddText;
	const auto repairedAddUndo = guardedAddCanvas.UndoCommand();
	guardedAddCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 310, 190 });
	const auto conflictingAdd = guardedAddCanvas.GetSelectedControl();
	if (conflictingAdd) conflictingAdd->Name = guardedAddName;
	const auto conflictingRedo = guardedAddCanvas.RedoCommand();
	AppendFailure(failures,
		repairedAddUndo.HasChanges()
		&& conflictingRedo.State == DesignerDocumentTransactionState::Failed
		&& guardedAddCanvas.GetRedoCommandCount() == 1
		&& guardedAddCanvas.GetAllControls().size() == 1,
		L"subtree delta: absent-name conflict did not preserve redo history");
	guardedAddCanvas.DeleteSelectedControlCore();
	AppendFailure(failures,
		guardedAddCanvas.RedoCommand().HasChanges()
		&& guardedAddCanvas.GetSelectedControl() == guardedAddIdentity
		&& guardedAddIdentity
		&& guardedAddIdentity->ControlInstance == guardedAddRuntime,
		L"subtree delta: Add redo did not recover after name conflict repair");

	DesignerCanvas rebuiltDeleteCanvas(0, 0, 900, 680);
	rebuiltDeleteCanvas.AddControlToCanvasCore(
		UIClass::UI_ToolBar, POINT{ 300, 180 });
	const auto originalToolBar = rebuiltDeleteCanvas.GetSelectedControl();
	const auto originalToolBarName = originalToolBar
		? originalToolBar->Name : std::wstring{};
	auto* const originalToolBarRuntime = originalToolBar
		? dynamic_cast<ToolBar*>(originalToolBar->ControlInstance) : nullptr;
	if (originalToolBarRuntime)
	{
		const POINT inside{
			originalToolBarRuntime->AbsLocation.x
				- rebuiltDeleteCanvas.AbsLocation.x + 50,
			originalToolBarRuntime->AbsLocation.y
				- rebuiltDeleteCanvas.AbsLocation.y + 16
		};
		rebuiltDeleteCanvas.AddControlToCanvasCore(
			UIClass::UI_Button, inside);
	}
	const auto rebuiltDeleteChild = rebuiltDeleteCanvas.GetSelectedControl();
	auto* const rebuiltDeleteChildRuntime = rebuiltDeleteChild
		? rebuiltDeleteChild->ControlInstance : nullptr;
	if (originalToolBarRuntime && rebuiltDeleteChildRuntime)
		originalToolBarRuntime->SetToolItemSizeOverride(
			rebuiltDeleteChildRuntime, SIZE{ 137, -2 });
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
				[&rebuiltDeleteCanvas](std::wstring&)
				{
					rebuiltDeleteCanvas.SetDesignedFormText(
						L"Temporary rebuild text");
					return true;
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
		SIZE restoredToolItemOverride{};
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
				== rebuiltToolBarRuntime
			&& rebuiltToolBarRuntime->TryGetToolItemSizeOverride(
				rebuiltDeleteChildRuntime, restoredToolItemOverride)
			&& restoredToolItemOverride.cx == 137
			&& restoredToolItemOverride.cy == -2,
			L"subtree delta: undo after rebuild lost parent, identity, document, or ToolBar metadata");
	}

	auto verifySubtreeSpecialParent = [&failures](
		UIClass parentType,
		const std::wstring& label)
	{
		DesignerCanvas specialCanvas(0, 0, 850, 660);
		specialCanvas.AddControlToCanvasCore(
			parentType, POINT{ 280, 220 });
		const auto parent = specialCanvas.GetSelectedControl();
		POINT childDrop{ 330, 280 };
		if (parent && parent->ControlInstance)
		{
			if (auto* split = dynamic_cast<SplitContainer*>(
				parent->ControlInstance))
			{
				split->RefreshSplitterLayout();
				if (auto* first = split->FirstPanel())
					childDrop = POINT{
						first->AbsLocation.x - specialCanvas.AbsLocation.x + 35,
						first->AbsLocation.y - specialCanvas.AbsLocation.y + 40
					};
			}
			else
			{
				childDrop = POINT{
					parent->ControlInstance->AbsLocation.x
						- specialCanvas.AbsLocation.x + 80,
					parent->ControlInstance->AbsLocation.y
						- specialCanvas.AbsLocation.y + 80
				};
			}
		}
		specialCanvas.AddControlToCanvasCore(
			UIClass::UI_Button, childDrop);
		const auto child = specialCanvas.GetSelectedControl();
		auto* const childRuntime = child
			? child->ControlInstance : nullptr;
		auto* const runtimeParent = childRuntime
			? childRuntime->Parent : nullptr;
		auto* const designerParent = child
			? child->DesignerParent : nullptr;
		bool parentKindMatches = false;
		if (parent && parent->ControlInstance && childRuntime)
		{
			if (auto* split = dynamic_cast<SplitContainer*>(
				parent->ControlInstance))
				parentKindMatches = runtimeParent == split->FirstPanel()
					&& designerParent == split;
			else if (auto* tabs = dynamic_cast<TabControl*>(
				parent->ControlInstance))
				parentKindMatches = tabs->Count > 0
					&& runtimeParent == tabs->operator[](0)
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
			&& childRuntime->Parent == runtimeParent
			&& child->DesignerParent == designerParent,
			L"subtree delta: " + label
				+ L" locator did not restore identity and parent");
	};
	verifySubtreeSpecialParent(
		UIClass::UI_SplitContainer, L"SplitFirst");
	verifySubtreeSpecialParent(
		UIClass::UI_TabControl, L"TabPage");

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
	auto rejectedAddResult = emptyCommandCanvas.AddControlToCanvas(
		UIClass::UI_Button, POINT{ 0, 0 });
	AppendFailure(failures,
		rejectedAddResult.State
			== DesignerDocumentTransactionState::Rejected
		&& !rejectedAddResult.Error.empty()
		&& emptyCommandEventCount == 3
		&& lastEmptyCommandEvent.Operation == L"AddControl"
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
	auto lifecycleAdd = lifecycleCanvas.AddControlToCanvas(
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
		auto branchEdit = lifecycleCanvas.AddControlToCanvas(
			UIClass::UI_Label, POINT{ 180, 180 });
		AppendFailure(failures,
			branchEdit.HasChanges()
			&& lifecycleCanvas.IsDocumentDirty()
			&& lifecycleCanvas.GetCurrentDocumentStateId() != savedStateId
			&& IsUnchanged(lifecycleCanvas.RedoCommand()),
			L"document lifecycle: branched history matched stale save point");

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
					"<Form><Unknown /></Form>",
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

		(void)lifecycleCanvas.AddControlToCanvas(
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
					L"document lifecycle: failed open did not restore document, selection, and dirty state");
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

		(void)lifecycleCanvas.AddControlToCanvas(
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
			&& lifecycleCanvas.GetAllControls().empty()
			&& lifecycleCanvas.BuildDesignDocument(
				newDocumentModel, &newDocumentError)
			&& newDocumentModel.Nodes.empty()
			&& newDocumentModel.Form.Name == L"MainForm"
			&& newDocumentModel.Form.Text == L"Form"
			&& IsUnchanged(lifecycleCanvas.UndoCommand()),
			L"document lifecycle: new document did not restore defaults and clean history");
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
	failedRestoreCanvas.AddControlToCanvasCore(
		UIClass::UI_Button, POINT{ 120, 120 });
	DesignerModel::DesignDocument validDocument;
	std::wstring captureError;
	const bool captured = failedRestoreCanvas.BuildDesignDocument(
		validDocument, &captureError);
	AppendFailure(failures, captured && validDocument.Nodes.size() == 1,
		L"failed undo: could not capture valid setup document");
	if (captured && validDocument.Nodes.size() == 1)
	{
		auto invalidDocument = validDocument;
		invalidDocument.Nodes.push_back(validDocument.Nodes.front());
		const auto controlName = validDocument.Nodes.front().Name;
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
			failedRestoreCanvas.GetAllControls().size() == 1
				&& failedRestoreCanvas.GetAllControls().front()->Name == controlName
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
			failedRestoreCanvas.GetAllControls().size() == 1
				&& failedRestoreCanvas.GetAllControls().front()->Name == controlName,
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
			&& !freshnessDesigner._btnArrange->Enable
			&& freshnessDesigner._arrangeMenu
			&& freshnessDesigner._arrangeMenu->ItemCount() == 7
			&& duplicateArrangeItem
			&& duplicateArrangeItem->Shortcut == L"Ctrl+D"
			&& lockArrangeItem
			&& lockArrangeItem->Shortcut == L"Ctrl+L"
			&& layerArrangeItem && layerArrangeItem->SubItems.size() == 4
			&& frontArrangeItem
			&& frontArrangeItem->Shortcut == L"Ctrl+Shift+]";
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
			&& freshnessDesigner._btnRegenerate->Enable
			&& freshnessDesigner._btnRegenerate->Text == L"重新生成";

		auto eventEdit = freshnessDesigner._canvas
			? freshnessDesigner._canvas->UpdateEventHandler(
				nullptr, L"OnShown", L"HandleShown", &freshnessError)
			: DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"missing freshness canvas");
		const bool eventMarkedStale = eventEdit.HasChanges()
			&& freshnessDesigner._codeFreshness.State
				== DesignerModel::DesignCodeFreshnessState::Stale
			&& freshnessDesigner._btnRegenerate->Text == L"重新生成 *";
		auto freshnessUndo = freshnessDesigner._canvas
			? freshnessDesigner._canvas->UndoCommand()
			: DesignerDocumentTransactionResult{};
		const bool undoRestoredCurrent = freshnessUndo.HasChanges()
			&& freshnessDesigner._codeFreshness.State
				== DesignerModel::DesignCodeFreshnessState::Current
			&& freshnessDesigner._btnRegenerate->Text == L"重新生成";
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
			&& freshnessDesigner._btnRegenerate->Text == L"重新生成 *";

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
			&& freshnessDesigner._btnRegenerate->Text == L"重新生成 !";

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
			&& freshnessDesigner._btnRegenerate->Text == L"生成受阻 !"
			&& !freshnessDesigner._btnRegenerate->AccessibleDescription.empty();
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
		const POINT declaredStatusLocation = demoStatus
			&& demoStatus->ControlInstance
			? demoStatus->ControlInstance->Location : POINT{};
		const SIZE declaredStatusSize = demoStatus
			&& demoStatus->ControlInstance
			? demoStatus->ControlInstance->Size : SIZE{};
		if (demoApplied) demoCanvas.Update();
		bool statusPreviewMatchesRuntime = false;
		if (demoStatus && demoStatus->ControlInstance
			&& demoStatus->ControlInstance->Parent)
		{
			auto* status = dynamic_cast<StatusBar*>(
				demoStatus->ControlInstance);
			const auto actual = demoStatus->ControlInstance
				->GetActualLocationDip();
			const auto actualSize = demoStatus->ControlInstance
				->GetActualSizeDip();
			const auto parentSize = demoStatus->ControlInstance->Parent
				->GetActualSizeDip();
			statusPreviewMatchesRuntime = status && status->TopMost
				&& std::fabs(actual.x) < 0.01f
				&& std::fabs(actual.y + actualSize.height
					- parentSize.height) < 0.01f
				&& demoStatus->ControlInstance->Location.x
					== declaredStatusLocation.x
				&& demoStatus->ControlInstance->Location.y
					== declaredStatusLocation.y
				&& demoStatus->ControlInstance->Size.cx
					== declaredStatusSize.cx
				&& demoStatus->ControlInstance->Size.cy
					== declaredStatusSize.cy;
		}
		DesignerModel::DesignDocument recapturedDemo;
		std::wstring demoCaptureError;
		const bool demoRecaptured = demoApplied
			&& demoCanvas.BuildDesignDocument(
				recapturedDemo, &demoCaptureError);
		CodeGenInput demoCodeInput;
		const bool demoCodeInputBuilt = demoRecaptured
			&& DesignerModel::DesignDocumentCodeGenInputBuilder::Build(
				recapturedDemo, demoCodeInput, &demoCaptureError);
		CodeGenerator demoCodeGenerator(L"DemoResourceForm", demoCodeInput);
		const auto demoGeneratedCpp = demoCodeInputBuilt
			? demoCodeGenerator.GenerateCpp() : std::string{};
		const bool drawingResourcesGenerated = demoCodeInputBuilt
			&& demoGeneratedCpp.find(
				"SetResource(L\"GradientLabelClip\"") != std::string::npos
			&& demoGeneratedCpp.find(
				"cui::drawing::GeometryKind::Path") != std::string::npos
			&& demoGeneratedCpp.find(
				"SetResource(L\"GradientLabelTransform\"") != std::string::npos
			&& demoGeneratedCpp.find(
				"cui::drawing::TransformKind::Rotate") != std::string::npos;
		const bool imageBrushGenerated = demoCodeInputBuilt
			&& demoGeneratedCpp.find(
				"SetResource(L\"RuntimeBadgeForeground\"") != std::string::npos
			&& demoGeneratedCpp.find(
				"cui::drawing::BrushKind::Image") != std::string::npos
			&& demoGeneratedCpp.find(
				"cui::drawing::ImageBrushStretch::UniformToFill") != std::string::npos;
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
					&& compactDemo.find("RenderTransformOrigin=\"0.5,0.5\"")
						!= std::string::npos
					&& compactDemo.find("<SideBar.Items>") != std::string::npos
					&& compactDemo.find("<BreadcrumbBar.Items>") != std::string::npos
					&& compactDemo.find("<FilterBar.Items>") != std::string::npos
					&& compactDemo.find("<KpiCard.Sparkline>") != std::string::npos
					&& compactDemo.find("<ChartView.Series>") != std::string::npos
					&& compactDemo.find("<ReportView.Columns>") != std::string::npos
					&& compactDemo.find("<ReportView.Rows>") != std::string::npos
					&& compactDemo.find("<BitmapImage") != std::string::npos
					&& compactDemo.find(
						"UriSource=\"Assets/nav-overview.svg\"") != std::string::npos
					&& compactDemo.find("<LinearGradientBrush") != std::string::npos
					&& compactDemo.find(
						"<ResourceDictionary.MergedDictionaries>")
						!= std::string::npos
					&& compactDemo.find(
						"Source=\"Assets/DemoTheme.xaml\"")
						!= std::string::npos
					&& compactDemo.find(
						"Icon=\"Assets/nav-overview.svg\"") != std::string::npos;
			}
			catch (const std::exception& exception)
			{
				demoCaptureError = Convert::Utf8ToUnicode(exception.what());
			}
		}
		const auto hasType = [&](UIClass type)
		{
			return std::any_of(
				demoDocument.Nodes.begin(), demoDocument.Nodes.end(),
				[type](const auto& node) { return node.Type == type; });
		};
		bool advancedDataMaterialized = false;
		bool objectResourcesMaterialized = false;
		bool drawingResourcesMaterialized = false;
		if (demoApplied)
		{
			auto sideBarWrapper = FindControl(demoCanvas, L"sideBar");
			auto breadcrumbWrapper = FindControl(demoCanvas, L"breadcrumb");
			auto filterWrapper = FindControl(demoCanvas, L"analyticsFilter");
			auto kpiWrapper = FindControl(demoCanvas, L"kpiRevenue");
			auto chartWrapper = FindControl(demoCanvas, L"salesChart");
			auto reportWrapper = FindControl(demoCanvas, L"salesReport");
			auto titleWrapper = FindControl(demoCanvas, L"basicTitle");
			auto badgeWrapper = FindControl(demoCanvas, L"runtimeBadge");
			auto pictureWrapper = FindControl(demoCanvas, L"demoPicture");
			auto gradientWrapper = FindControl(demoCanvas, L"gradientLabel");
			auto* sideBar = sideBarWrapper
				? dynamic_cast<NavigationView*>(sideBarWrapper->ControlInstance) : nullptr;
			auto* breadcrumb = breadcrumbWrapper
				? dynamic_cast<BreadcrumbBar*>(breadcrumbWrapper->ControlInstance) : nullptr;
			auto* filter = filterWrapper
				? dynamic_cast<FilterBar*>(filterWrapper->ControlInstance) : nullptr;
			auto* kpi = kpiWrapper
				? dynamic_cast<KpiCard*>(kpiWrapper->ControlInstance) : nullptr;
			auto* chart = chartWrapper
				? dynamic_cast<ChartView*>(chartWrapper->ControlInstance) : nullptr;
			auto* report = reportWrapper
				? dynamic_cast<ReportView*>(reportWrapper->ControlInstance) : nullptr;
			auto* title = titleWrapper ? titleWrapper->ControlInstance : nullptr;
			auto* badge = badgeWrapper ? badgeWrapper->ControlInstance : nullptr;
			auto* picture = pictureWrapper
				? dynamic_cast<PictureBox*>(pictureWrapper->ControlInstance) : nullptr;
			auto* gradient = gradientWrapper
				? gradientWrapper->ControlInstance : nullptr;
			advancedDataMaterialized = sideBar && sideBar->Items.size() == 5
				&& sideBar->SelectedIndex == 1
				&& sideBar->Items[1].BadgeText == L"3"
				&& breadcrumb && breadcrumb->Items.size() == 3
				&& breadcrumb->SelectedIndex == 2
				&& filter && filter->Items.size() == 4
				&& filter->Items[0].Selected
				&& filter->Placeholder == L"搜索客户、区域或阶段"
				&& kpi && kpi->Title == L"成交额"
				&& kpi->TrendDirection == KpiTrendDirection::Up
				&& std::fabs(kpi->CornerRadius - 10.0f) < 0.01f
				&& kpi->SparklineValues.size() == 8
				&& chart && chart->Title == L"成交趋势"
				&& chart->Series.size() == 3
				&& chart->Series[0].Points.size() == 8
				&& report && report->Title == L"成交报表"
				&& report->Columns.size() == 5
				&& report->Rows.size() == 8
				&& report->Rows[0].Kind == ReportRowKind::Group
				&& report->Rows[3].Kind == ReportRowKind::Summary;
			const auto& titleBrush = title
				? title->GetForegroundBrush()
				: std::optional<cui::drawing::Brush>{};
			const auto& badgeBrush = badge
				? badge->GetForegroundBrush()
				: std::optional<cui::drawing::Brush>{};
			objectResourcesMaterialized = sideBar
				&& sideBar->Items.size() == 5
				&& sideBar->Items[1].Icon
				&& sideBar->Items[1].Icon->GetSourceUri()
					== L"Assets/nav-overview.svg"
				&& sideBar->Items[2].Icon
				&& sideBar->Items[2].Icon->GetSourceUri()
					== L"Assets/nav-assets.svg"
				&& sideBar->Items[4].Icon
				&& sideBar->Items[4].Icon->GetSourceUri()
					== L"Assets/nav-settings.svg"
				&& picture && picture->Image
				&& picture->Image->GetSourceUri() == L"Assets/nav-overview.svg"
				&& titleBrush
				&& titleBrush->Kind == cui::drawing::BrushKind::LinearGradient
				&& titleBrush->GradientStops.size() == 2
				&& badgeBrush
				&& badgeBrush->Kind == cui::drawing::BrushKind::Image
				&& badgeBrush->ImageSource
				&& badgeBrush->ImageSource->GetSourceUri()
					== L"Assets/nav-overview.svg"
				&& badgeBrush->Stretch
					== cui::drawing::ImageBrushStretch::UniformToFill
				&& std::fabs(badgeBrush->Opacity - 0.9f) < 0.01f;
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
					== ControlPropertyValueSource::Style
				&& gradient->GetPropertyValueSource(L"RenderTransform")
					== ControlPropertyValueSource::Style;
		}
		bool surfaceChildMoveStable = false;
		if (auto basicButton = demoApplied
			? FindControl(demoCanvas, L"basicButton") : nullptr;
			basicButton && basicButton->ControlInstance)
		{
			auto* control = basicButton->ControlInstance;
			const auto beforeLocation = control->Location;
			const auto beforeSize = control->Size;
			const auto beforeAbsolute = control->GetAbsoluteLocationDip();
			demoCanvas.RestoreSelectionByNames(
				{ basicButton->Name }, basicButton->Name, false);
			const auto moved = demoCanvas.NudgeSelectionBy(1, 1);
			const auto afterAbsolute = control->GetAbsoluteLocationDip();
			surfaceChildMoveStable = moved.HasChanges()
				&& control->Location.x == beforeLocation.x + 1
				&& control->Location.y == beforeLocation.y + 1
				&& control->Size.cx == beforeSize.cx
				&& control->Size.cy == beforeSize.cy
				&& std::fabs(afterAbsolute.x - beforeAbsolute.x - 1.0f) < 0.01f
				&& std::fabs(afterAbsolute.y - beforeAbsolute.y - 1.0f) < 0.01f;
		}
		bool transformedChildMoveStable = false;
		if (auto gradientLabel = demoApplied
			? FindControl(demoCanvas, L"gradientLabel") : nullptr;
			gradientLabel && gradientLabel->ControlInstance)
		{
			auto* control = gradientLabel->ControlInstance;
			const auto beforeLocation = control->Location;
			const auto beforeSize = control->Size;
			demoCanvas.RestoreSelectionByNames(
				{ gradientLabel->Name }, gradientLabel->Name, false);
			const auto moved = demoCanvas.NudgeSelectionBy(1, 0);
			transformedChildMoveStable = moved.HasChanges()
				&& control->Location.x == beforeLocation.x + 1
				&& control->Location.y == beforeLocation.y
				&& control->Size.cx == beforeSize.cx
				&& control->Size.cy == beforeSize.cy;
		}
		const auto nestedContainerChildMoveStable =
			[&](const std::wstring& name)
			{
				auto wrapper = demoApplied
					? FindControl(demoCanvas, name) : nullptr;
				if (!wrapper || !wrapper->ControlInstance) return false;
				auto* control = wrapper->ControlInstance;
				const auto beforeLocation = control->Location;
				const auto beforeSize = control->Size;
				const auto beforeAbsolute = control->GetAbsoluteLocationDip();
				demoCanvas.RestoreSelectionByNames(
					{ wrapper->Name }, wrapper->Name, false);
				const auto moved = demoCanvas.NudgeSelectionBy(1, 1);
				const auto afterAbsolute = control->GetAbsoluteLocationDip();
				return moved.HasChanges()
					&& control->Location.x == beforeLocation.x + 1
					&& control->Location.y == beforeLocation.y + 1
					&& control->Size.cx == beforeSize.cx
					&& control->Size.cy == beforeSize.cy
					&& std::fabs(afterAbsolute.x
						- beforeAbsolute.x - 1.0f) < 0.01f
					&& std::fabs(afterAbsolute.y
						- beforeAbsolute.y - 1.0f) < 0.01f;
			};
		const bool groupChildMoveStable =
			nestedContainerChildMoveStable(L"groupName");
		const bool expanderChildMoveStable =
			nestedContainerChildMoveStable(L"expanderText");
		const bool specialContainerMovesStable =
			groupChildMoveStable && expanderChildMoveStable;
		AppendFailure(failures,
			demoParsed && demoApplied && demoRecaptured && demoCompact
			&& drawingResourcesGenerated
			&& imageBrushGenerated
			&& advancedDataMaterialized
			&& objectResourcesMaterialized
			&& drawingResourcesMaterialized
			&& statusPreviewMatchesRuntime
			&& surfaceChildMoveStable
			&& transformedChildMoveStable
			&& specialContainerMovesStable
			&& hasType(UIClass::UI_SideBar)
			&& hasType(UIClass::UI_BreadcrumbBar)
			&& hasType(UIClass::UI_PagedGridView)
			&& hasType(UIClass::UI_WebBrowser)
			&& hasType(UIClass::UI_MediaPlayer),
			L"public XAML gallery: DemoWindow preview geometry, movement, or compact serialization regressed"
			+ std::wstring(L" [path=") + demoPath.wstring()
			+ L", parse=" + demoParseError
			+ L", apply=" + demoApplyError
			+ L", capture=" + demoCaptureError
			+ L", status=" + (statusPreviewMatchesRuntime ? L"1" : L"0")
			+ L", surface=" + (surfaceChildMoveStable ? L"1" : L"0")
			+ L", transform=" + (transformedChildMoveStable ? L"1" : L"0")
			+ L", group=" + (groupChildMoveStable ? L"1" : L"0")
			+ L", expander=" + (expanderChildMoveStable ? L"1" : L"0")
			+ L", advanced=" + (advancedDataMaterialized ? L"1" : L"0")
			+ L", objects=" + (objectResourcesMaterialized ? L"1" : L"0")
			+ L", drawing=" + (drawingResourcesMaterialized ? L"1" : L"0")
			+ L", drawingCode=" + (drawingResourcesGenerated ? L"1" : L"0")
			+ L", imageBrushCode=" + (imageBrushGenerated ? L"1" : L"0")
			+ L"]");
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
