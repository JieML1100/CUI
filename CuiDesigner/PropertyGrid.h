#pragma once

/**
 * @file PropertyGrid.h
 * @brief PropertyGrid：设计器属性面板（显示/编辑控件属性）。
 */
#include "../CUI/include/Panel.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/PropertyGrid.h"
#include "DesignerTypes.h"
#include "DesignerEventCatalog.h"
#include "DesignerModel/DesignCodeGenerationService.h"
#include "DesignerCore/PropertyGridBinder.h"
#include "DesignerCore/DesignerDocumentTransaction.h"
#include <memory>
#include <functional>
#include <map>
#include <set>

class DesignerCanvas;
struct DesignerPropertyRow;
enum class DesignerCustomEditorKind : unsigned char;
class PropertyGrid;

enum class DesignerPropertyGridViewMode : unsigned char
{
	Properties,
	Events
};

using DesignerEventHandlerActivatedEvent = Event<void(
	PropertyGrid*, const std::wstring& handlerName)>;

class PropertyGrid : public Panel
{
private:
	std::vector<DesignerPropertyRow> _propertyRows;
	DesignerModel::DesignEventHandlerCodeInspection _eventCodeInspection;
	std::vector<EventConnection> _diagnosticConnections;
	PropertyGridView* _nativeGrid = nullptr;
	enum class NativeGridEntryKind : unsigned char
	{
		Property,
		Event,
		Action,
		Informational
	};
	struct NativeGridEntry
	{
		NativeGridEntryKind Kind = NativeGridEntryKind::Informational;
		std::wstring PropertyName;
		std::function<void()> Action;
	};
	std::vector<NativeGridEntry> _nativeEntries;
	std::vector<PropertyGridItem> _nativeItemBuffer;
	bool _syncingNativeGrid = false;
	bool _nativeSliderEditAccepted = true;
	int _contentTop = 96;
	bool _reloadRequested = false;
	DesignerPropertyGridViewMode _viewMode =
		DesignerPropertyGridViewMode::Properties;
	DesignerPropertyGridViewMode _loadedViewMode =
		DesignerPropertyGridViewMode::Properties;
	struct ViewState
	{
		std::wstring Filter;
		float ScrollOffset = 0.0f;
		std::set<std::wstring> CollapsedCategories;
	};
	ViewState _propertiesViewState;
	ViewState _eventsViewState;
	bool _syncingViewModeControls = false;
	bool _restoreViewStatePending = false;
	struct PendingFloatSliderCommand
	{
		bool Active = false;
		std::wstring PropertyName;
		DesignerPropertyBatchSnapshot Before;
		std::vector<std::wstring> BeforeSelectionNames;
		std::wstring BeforePrimarySelectionName;
	};
	PendingFloatSliderCommand _pendingFloatSliderCommand;

	PropertyGridBinder _binding;
	Label* _titleLabel;
	Button* _propertiesModeButton = nullptr;
	Button* _eventsModeButton = nullptr;
	Label* _filterLabel = nullptr;
	TextBox* _filterBox = nullptr;
	Label* _editErrorLabel = nullptr;
	std::wstring _propertyFilter;
	std::wstring _editErrorProperty;
	std::wstring _editErrorMessage;

	void UpdateContentHostLayout();
	void UpdateViewModePresentation();
	ViewState& CurrentViewState() noexcept;
	const ViewState& CurrentViewState() const noexcept;
	void CaptureCurrentViewState();
	void RestoreCurrentViewState();
	void BeginNativeRowsReload();
	void CommitNativeRowsReload();
	void PopulateNativePropertyRows(
		const std::vector<DesignerPropertyRow>& rows,
		const std::wstring& scopeCaption);
	void AddNativeEventRow(
		const DesignerEventDescriptor& event,
		const std::wstring& subjectName,
		const std::wstring& storedHandler,
		const std::wstring& category = L"\u4e8b\u4ef6");
	void AddNativeEventHandlerManagerRow(const std::wstring& category);
	void PopulateNativeEventRows(
		const std::vector<DesignerEventDescriptor>& events,
		const std::wstring& subjectName,
		const std::map<std::wstring, std::wstring>& handlers,
		const std::wstring& scopeCaption);
	void AddNativeActionRow(
		const std::wstring& category,
		const std::wstring& name,
		const std::wstring& value,
		const std::wstring& description,
		std::function<void()> action);
	void AddNativeInformationalRow(
		const std::wstring& category,
		const std::wstring& name,
		const std::wstring& value = L"");
	void HandleNativeValueChanged(
		int index,
		const std::wstring& oldValue,
		const std::wstring& newValue);
	void HandleNativeItemClick(int index);
	void HandleNativeDoubleClick(int index);
	void HandleNativeResetRequested(int index);

	bool HasActivePropertyFilter() const;
	bool MatchesCurrentFilter(const std::wstring& searchableText) const;
	void ShowPropertyEditError(
		const std::wstring& propertyName,
		const std::wstring& message);
	void ClearPropertyEditError();
	void SubscribePropertyDiagnosticChanges();
	void RefreshPropertyValueSource(const std::wstring& propertyName);
	DesignerPropertyEditResult ResetCurrentProperty(
		const std::wstring& propertyName);
	void AddNativeCustomEditorRows(UIClass targetType);
	void OpenCustomEditor(DesignerCustomEditorKind kind);
	bool ShouldGroupFloatSliderProperty(const std::wstring& propertyName) const;
	bool BeginGroupedFloatSliderEdit(const std::wstring& propertyName);
	void CommitGroupedFloatSliderEdit();
	void RollbackGroupedFloatSliderEdit(const std::wstring& error);
	DesignerPropertyEditResult UpdateFloatPropertyPreview(
		const std::wstring& propertyName,
		float value);
	DesignerPropertyEditResult ExecutePropertyEditCommand(
		const std::wstring& propertyName,
		const std::function<DesignerPropertyEditResult()>& applyChange);
	DesignerDocumentTransactionResult ExecutePropertyCommand(
		const std::wstring& propertyName,
		const std::function<bool(std::wstring& error)>& applyChange);
	DesignerPropertyEditResult UpdatePropertyFromTextBox(
		std::wstring propertyName,
		std::wstring value);
	DesignerPropertyEditResult UpdatePropertyFromBool(
		std::wstring propertyName,
		bool value);

public:
	/** Fired after a double-click activation has resolved or created a handler. */
	DesignerEventHandlerActivatedEvent OnEventHandlerActivated;

	PropertyGrid(int x, int y, int width, int height);
	virtual ~PropertyGrid();
	void Update() override;
	bool ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY) override;
	
	void SetDesignerCanvas(DesignerCanvas* canvas) { _binding.SetCanvas(canvas); }
	void SetViewMode(DesignerPropertyGridViewMode mode);
	/** Updates per-handler source diagnostics without changing selection. */
	void SetEventHandlerCodeInspection(
		DesignerModel::DesignEventHandlerCodeInspection inspection);
	const DesignerModel::DesignEventHandlerCodeInspection&
		GetEventHandlerCodeInspection() const noexcept
	{
		return _eventCodeInspection;
	}
	DesignerPropertyGridViewMode GetViewMode() const noexcept
	{
		return _viewMode;
	}
	void SetFilterText(std::wstring value);
	const std::wstring& GetFilterText() const noexcept
	{
		return _propertyFilter;
	}
	void CommitPendingEdits();
	void LoadControl(std::shared_ptr<DesignerControl> control);
	void LoadControls(
		const std::vector<std::shared_ptr<DesignerControl>>& controls,
		std::shared_ptr<DesignerControl> primaryControl = nullptr);
	/**
	 * Programmatic counterpart of committing a PropertyGrid text editor. It
	 * intentionally uses the same validation, undo snapshot, and error path as
	 * interactive edits so automation does not bypass production behavior.
	 */
	DesignerPropertyEditResult ApplyPropertyValue(
		const std::wstring& propertyName,
		const std::wstring& valueText);
	/**
	 * WinForms-style event activation. An unbound event receives its conventional
	 * default handler name through the normal undoable edit path; an existing
	 * handler is reused. Success raises OnEventHandlerActivated.
	 */
	DesignerPropertyEditResult ActivateEventHandler(
		const std::wstring& eventName,
		std::wstring* outHandlerName = nullptr);
	/** Activates the catalog-declared default event for the current subject. */
	DesignerPropertyEditResult ActivateDefaultEventHandler(
		std::wstring* outHandlerName = nullptr);
	/** Programmatic counterpart of clicking a row's Reset button. */
	DesignerPropertyEditResult ResetPropertyValue(
		const std::wstring& propertyName);
	const std::vector<DesignerPropertyRow>& GetPresentedPropertyRows() const
	{
		return _propertyRows;
	}
	/** The actual CUI PropertyGridView used by the Designer surface. */
	PropertyGridView* GetNativePropertyGrid() const noexcept
	{
		return _nativeGrid;
	}
	bool HasPropertyEditError() const noexcept
	{
		return !_editErrorMessage.empty();
	}
	const std::wstring& GetPropertyEditErrorProperty() const noexcept
	{
		return _editErrorProperty;
	}
	const std::wstring& GetPropertyEditErrorMessage() const noexcept
	{
		return _editErrorMessage;
	}
	void Clear();
};
