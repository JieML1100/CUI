#include "PropertyGrid.h"
#include "../CUI/include/EventInfrastructure.h"
#include "../CUI/include/Canvas.h"
#include "../CUI/include/StyleInfrastructure.h"
#include "ProgrammaticControlFactory.h"
#include "../CUI/include/Window.h"
#include "../CUI/include/NativeVisualStateInfrastructure.h"
#include "GridDefinitionsEditorDialog.h"
#include "BindingEditorDialog.h"
#include "DataContextSchemaEditorDialog.h"
#include "DataResourcesEditorDialog.h"
#include "DesignerPropertyCatalog.h"
#include "DesignerPropertyRowCatalog.h"
#include "DesignerStyleSheetUtils.h"
#include "DesignerCustomEditorCatalog.h"
#include "DesignerEventCatalog.h"
#include "EventHandlerEditorDialog.h"
#include "DesignerCore/Commands/EventHandlerCodeMigration.h"
#include "StyleSheetEditorDialog.h"
#include "DesignerCanvas.h"
#include "DesignerCore/Commands/ControlPropertyCommand.h"
#include "DesignerCore/Commands/ControlStructureCommand.h"
#include "DesignerStructureEdit.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/ListView.h"
#include "../CUI/include/TreeView.h"
#include "../CUI/include/TabControl.h"
#include "../CUI/include/ToolBar.h"
#include "../CUI/include/StatusBar.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <set>

namespace
{
	void ArrangePropertyGridChild(Control* child, float x, float y,
		float width, float height)
	{
		if (!child) return;
		width = (std::max)(0.0f, width);
		height = (std::max)(0.0f, height);
		Canvas::SetLeft(*(child), x);
		Canvas::SetTop(*(child), y);
		Canvas::SetRight(*(child), cui::layout::UnsetCanvasOffset);
		Canvas::SetBottom(*(child), cui::layout::UnsetCanvasOffset);
		child->Width = width;
		child->Height = height;
		child->Arrange({ x, y, width, height });
	}

	static bool PropertyNamesEqual(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	static std::wstring DesignerCategoryCaption(const std::wstring& category)
	{
		if (PropertyNamesEqual(category, L"Common")) return L"常用";
		if (PropertyNamesEqual(category, L"Layout")) return L"布局";
		if (PropertyNamesEqual(category, L"Appearance")) return L"外观";
		if (PropertyNamesEqual(category, L"Behavior")) return L"行为";
		if (PropertyNamesEqual(category, L"Validation")) return L"校验";
		if (PropertyNamesEqual(category, L"Accessibility")) return L"可访问性";
		if (PropertyNamesEqual(category, L"Data")) return L"数据";
		if (PropertyNamesEqual(category, L"Misc")) return L"其他";
		return category;
	}

	static std::wstring DesignerValueSourceCaption(
		DependencyPropertyValueSource source)
	{
	switch (source)
	{
	case DependencyPropertyValueSource::Inherited: return L"继承";
	case DependencyPropertyValueSource::Theme: return L"主题";
		case DependencyPropertyValueSource::Style: return L"样式";
	case DependencyPropertyValueSource::Template: return L"模板";
	case DependencyPropertyValueSource::Local: return L"本地";
	case DependencyPropertyValueSource::VisualState: return L"视觉状态";
	case DependencyPropertyValueSource::Animation: return L"动画";
	case DependencyPropertyValueSource::Default:
		default:
			return L"默认";
		}
	}

	static std::wstring TrimWs(const std::wstring& s);

	static const std::wstring kFontDefaultOption = L"<Default>";
	static const std::wstring kMixedValueText = L"<多个值>";

	static std::wstring FloatToText(float v)
	{
		std::wostringstream oss;
		oss.setf(std::ios::fixed);
		oss << std::setprecision(2) << v;
		return oss.str();
	}

	static std::wstring DoubleToText(double v)
	{
		std::wostringstream oss;
		oss.setf(std::ios::fixed);
		oss << std::setprecision(4) << v;
		auto s = oss.str();
		while (!s.empty() && s.find(L'.') != std::wstring::npos && s.back() == L'0')
			s.pop_back();
		if (!s.empty() && s.back() == L'.')
			s.pop_back();
		return s.empty() ? L"0" : s;
	}

	static bool TryParseFloatWs(const std::wstring& s, float& out)
	{
		try
		{
			out = std::stof(s);
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	static bool EnumOptionMatchesValue(const std::wstring& option, const std::wstring& value)
	{
		auto left = TrimWs(option);
		auto right = TrimWs(value);
		if (left == right)
			return true;

		float leftNumber = 0.0f;
		float rightNumber = 0.0f;
		if (TryParseFloatWs(left, leftNumber) && TryParseFloatWs(right, rightNumber))
			return std::fabs(leftNumber - rightNumber) < 1e-4f;

		return false;
	}

	static std::vector<std::wstring> GetFontFamilyOptions()
	{
		std::vector<std::wstring> out;
		out.push_back(kFontDefaultOption);

		try
		{
			auto fonts = ::Font::GetSystemFonts();
			std::set<std::wstring> uniq;
			for (auto& f : fonts)
			{
				auto t = TrimWs(f);
				if (!t.empty()) uniq.insert(t);
			}
			for (auto& n : uniq) out.push_back(n);
		}
		catch (...) {}
		return out;
	}

	static std::vector<std::wstring> GetFontSizeOptions()
	{
		static const int sizes[] = { 8,9,10,11,12,14,16,18,20,22,24,26,28,32,36,48,72 };
		std::vector<std::wstring> out;
		out.reserve(_countof(sizes));
		for (int s : sizes) out.push_back(std::to_wstring(s));
		return out;
	}

	static std::wstring TrimWs(const std::wstring& s)
	{
		size_t b = 0;
		while (b < s.size() && iswspace(s[b])) b++;
		size_t e = s.size();
		while (e > b && iswspace(s[e - 1])) e--;
		return s.substr(b, e - b);
	}

	static bool IsEventPropertyName(
		const PropertyGridBinder& binding,
		const std::wstring& name)
	{
		if (binding.IsWindowBinding())
			return DesignerEventCatalog::FindWindowEvent(name).has_value();
		const auto control = binding.GetBoundControl();
		return control && DesignerEventCatalog::FindControlEvent(
			control->Type, name, control->ComponentEvents).has_value();
	}

	static std::vector<std::wstring> GetCompatibleHandlerNames(
		const DesignerCanvas* canvas,
		const DesignerEventDescriptor& requested,
		const std::wstring& defaultName,
		const std::wstring& currentName,
		const DesignerModel::DesignEventHandlerCodeInspection& codeInspection)
	{
		if (canvas)
			return canvas->GetCompatibleEventHandlerNames(
				requested, defaultName, currentName,
				codeInspection.CompatibleUserHandlers);
		std::set<std::wstring> compatible;
		if (!defaultName.empty()) compatible.insert(defaultName);
		if (!currentName.empty()) compatible.insert(currentName);

		std::vector<std::wstring> result;
		auto appendFirst = [&](const std::wstring& name) {
			if (name.empty()) return;
			auto it = compatible.find(name);
			if (it == compatible.end()) return;
			result.push_back(*it);
			compatible.erase(it);
		};
		appendFirst(defaultName);
		appendFirst(currentName);
		result.insert(result.end(), compatible.begin(), compatible.end());
		return result;
	}

	struct EventCodePresentation
	{
		std::wstring Badge;
		std::wstring Diagnostic;
	};

	static EventCodePresentation GetEventCodePresentation(
		const DesignerModel::DesignEventHandlerCodeInspection& inspection,
		const std::wstring& handlerName)
	{
		if (handlerName.empty()) return {};
		if (inspection.Pending)
			return { L"检查中", L"正在检查用户头/源文件中的处理函数。" };
		if (!inspection.Associated)
			return { L"未关联代码",
				L"首次导出代码后可检查并定位处理函数。" };
		const auto found = inspection.Handlers.find(handlerName);
		if (found == inspection.Handlers.end())
		{
			if (!inspection.Diagnostic.empty())
				return { L"诊断失败", inspection.Diagnostic };
			return { L"待检查", L"当前代码检查结果尚未包含该处理函数。" };
		}
		const auto& entry = found->second;
		switch (entry.State)
		{
		case DesignerModel::DesignEventHandlerCodeState::Current:
			return { L"已实现", entry.Diagnostic };
		case DesignerModel::DesignEventHandlerCodeState::SourceMissing:
			return { L"源文件缺失", entry.Diagnostic };
		case DesignerModel::DesignEventHandlerCodeState::SignatureMismatch:
			return { L"签名错误", entry.Diagnostic };
		case DesignerModel::DesignEventHandlerCodeState::DuplicateDefinition:
			return { L"重复定义", entry.Diagnostic };
		case DesignerModel::DesignEventHandlerCodeState::DefinitionMissing:
		default:
			return { L"待生成", entry.Diagnostic };
		}
	}

}

PropertyGrid::PropertyGrid(int x, int y, int width, int height)
	: Panel()
{
	Canvas::SetLeft(*this, static_cast<float>(x));
	Canvas::SetTop(*this, static_cast<float>(y));
	Width = static_cast<float>(width);
	Height = static_cast<float>(height);
	this->Background = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
	this->BorderThickness = 1.0f;

	// 标题
	_titleLabel = cui::designer::NewControl<Label>(L"属性", 10, 10);
	_titleLabel->Width = static_cast<float>(width - 20);
	_titleLabel->Height = 25.0f;
	cui::designer::ApplyProgrammaticTypography(
		*_titleLabel, L"Microsoft YaHei", 16.0);
	this->AdoptVisualChild(_titleLabel);

	auto configureModeButton = [this](Button*& button,
		const std::wstring& text, const std::wstring& accessibleName)
	{
		button = cui::designer::NewControl<Button>(text, 0, 7, 54, 27);
		cui::designer::ApplyProgrammaticTypography(
			*button, L"Microsoft YaHei", 11.0);
		button->BorderThickness = 1.0f;
		button->Background = D2D1::ColorF(0.88f, 0.90f, 0.94f, 1.0f);
		button->AutomationName = accessibleName;
		this->AdoptVisualChild(button);
	};
	configureModeButton(_propertiesModeButton, L"属性", L"显示属性");
	configureModeButton(_eventsModeButton, L"事件", L"显示事件");
	_propertiesModeButton->Click += [this](Control*, RoutedEventArgs&)
	{
		SetViewMode(DesignerPropertyGridViewMode::Properties);
	};
	_eventsModeButton->Click += [this](Control*, RoutedEventArgs&)
	{
		SetViewMode(DesignerPropertyGridViewMode::Events);
	};

	_filterLabel = cui::designer::NewControl<Label>(L"筛选", 10, 40);
	_filterLabel->Width = 42.0f;
	_filterLabel->Height = 22.0f;
	cui::designer::ApplyProgrammaticTypography(
		*_filterLabel, L"Microsoft YaHei", 12.0);
	_filterLabel->AutomationName = L"属性筛选标签";
	this->AdoptVisualChild(_filterLabel);

	_filterBox = cui::designer::NewControl<TextBox>(L"", 56.0f, 38.0f,
		static_cast<float>((std::max)(0, width - 66)), 24.0f);
	_filterBox->AutomationName = L"筛选属性";
	_filterBox->AutomationFullDescription =
		L"按属性名称、分类、当前值或值来源筛选；多个关键词需同时匹配。";
	_filterBox->OnTextChanged += [this](Control*, TextChangedEventArgs& args)
	{
		if (_syncingViewModeControls) return;
		auto value = args.NewText;
		if (_propertyFilter == value) return;
		_propertyFilter = std::move(value);
		CurrentViewState().Filter = _propertyFilter;
		_reloadRequested = true;
		this->InvalidateVisual();
	};
	this->AdoptVisualChild(_filterBox);

	_editErrorLabel = cui::designer::NewControl<Label>(L"", 10, 66);
	_editErrorLabel->Width = static_cast<float>(std::max(0, width - 20));
	_editErrorLabel->Height = 24.0f;
	cui::designer::ApplyProgrammaticTypography(
		*_editErrorLabel, L"Microsoft YaHei", 11.0);
	_editErrorLabel->Foreground = Colors::Red;
	_editErrorLabel->Visibility = Visibility::Collapsed;
	_editErrorLabel->AutomationName = L"属性编辑状态";
	this->AdoptVisualChild(_editErrorLabel);

	_nativeGrid = new PropertyGridView(
		0, _contentTop, width, std::max(0, height - _contentTop));
	_nativeGrid->AutomationName = L"\u8bbe\u8ba1\u5668\u5c5e\u6027\u7f51\u683c";
	_nativeGrid->AutomationFullDescription =
		L"\u4f7f\u7528 CUI \u539f\u751f PropertyGrid \u663e\u793a\u548c\u7f16\u8f91\u8bbe\u8ba1\u671f\u5c5e\u6027\u3002";
	_nativeGrid->Background = Colors::White;
	_nativeGrid->Foreground = D2D1::ColorF(0.12f, 0.14f, 0.18f, 1.0f);
	_nativeGrid->HeaderBackColor = D2D1::ColorF(0.91f, 0.93f, 0.96f, 1.0f);
	_nativeGrid->HeaderForeColor = D2D1::ColorF(0.14f, 0.17f, 0.22f, 1.0f);
	_nativeGrid->CategoryBackColor = D2D1::ColorF(0.94f, 0.95f, 0.97f, 1.0f);
	_nativeGrid->CategoryForeColor = D2D1::ColorF(0.20f, 0.24f, 0.31f, 1.0f);
	_nativeGrid->AlternateRowBackColor = D2D1::ColorF(0.12f, 0.18f, 0.30f, 0.035f);
	_nativeGrid->GridLineColor = D2D1::ColorF(0.38f, 0.43f, 0.52f, 0.22f);
	_nativeGrid->ReadOnlyForeColor = D2D1::ColorF(0.46f, 0.49f, 0.55f, 1.0f);
	_nativeGrid->NameColumnWidth = std::max(96.0f, width * 0.46f);
	_nativeGrid->SetHeaderLabels(L"属性", L"值");
	_nativeGrid->OnValueChanged += [this](
		PropertyGridView*, int index, std::wstring oldValue, std::wstring newValue)
	{
		HandleNativeValueChanged(index, oldValue, newValue);
	};
	_nativeGrid->OnItemClick += [this](PropertyGridView*, int index)
	{
		HandleNativeItemClick(index);
	};
	_nativeGrid->SelectionChanged += [this](PropertyGridView*, int index)
	{
		RememberNativeSelection(index);
	};
	_nativeGrid->OnKeyDown += [this](Control*, KeyEventArgs& args)
	{
		if (_viewMode == DesignerPropertyGridViewMode::Events
			&& args.Key == Key::F12)
			ActivateSelectedEventHandler();
	};
	_nativeGrid->OnMouseDoubleClick += [this](Control*, MouseEventArgs& eventArgs)
	{
		if (!_nativeGrid) return;
		const auto index = _nativeGrid->HitTestItem(eventArgs.X, eventArgs.Y);
		HandleNativeDoubleClick(index);
	};
	_nativeGrid->OnResetRequested += [this](PropertyGridView*, int index)
	{
		HandleNativeResetRequested(index);
	};
	_nativeGrid->OnEditStarted += [this](PropertyGridView*, int index)
	{
		if (index < 0 || index >= static_cast<int>(_nativeEntries.size())) return;
		const auto& entry = _nativeEntries[static_cast<size_t>(index)];
		const auto* row = DesignerPropertyRowCatalog::Find(
			_propertyRows, entry.PropertyName);
		if (entry.Kind == NativeGridEntryKind::Property && row
			&& row->Editor == DesignerPropertyRowEditorKind::FloatSlider)
			_nativeSliderEditAccepted =
				BeginGroupedFloatSliderEdit(entry.PropertyName);
	};
	_nativeGrid->OnEditCompleted += [this](PropertyGridView*, int)
	{
		if (_nativeSliderEditAccepted) CommitGroupedFloatSliderEdit();
		_nativeSliderEditAccepted = true;
	};
	_nativeGrid->OnEditCanceled += [this](PropertyGridView*, int)
	{
		if (_nativeSliderEditAccepted) RollbackGroupedFloatSliderEdit(L"");
		_nativeSliderEditAccepted = true;
	};
	this->AdoptVisualChild(_nativeGrid);
	UpdateViewModePresentation();
	UpdateContentHostLayout();
}

PropertyGrid::~PropertyGrid()
{
}

void PropertyGrid::UpdateContentHostLayout()
{
	const float width = ActualWidth > 0.0f
		? ActualWidth : (Width.IsFixed() ? Width.value : 0.0f);
	const float height = ActualHeight > 0.0f
		? ActualHeight : (Height.IsFixed() ? Height.value : 0.0f);
	ArrangePropertyGridChild(_titleLabel, 10.0f, 10.0f,
		(std::max)(0.0f, width - 138.0f), 25.0f);
	ArrangePropertyGridChild(_propertiesModeButton,
		(std::max)(10.0f, width - 124.0f), 7.0f, 54.0f, 27.0f);
	ArrangePropertyGridChild(_eventsModeButton,
		(std::max)(68.0f, width - 66.0f), 7.0f, 54.0f, 27.0f);
	ArrangePropertyGridChild(_filterLabel, 10.0f, 40.0f, 42.0f, 22.0f);
	ArrangePropertyGridChild(_filterBox, 56.0f, 38.0f,
		(std::max)(0.0f, width - 66.0f), 24.0f);
	ArrangePropertyGridChild(_editErrorLabel, 10.0f, 66.0f,
		(std::max)(0.0f, width - 20.0f), 24.0f);
	if (_nativeGrid)
	{
		ArrangePropertyGridChild(_nativeGrid, 0.0f,
			static_cast<float>(_contentTop), width,
			(std::max)(0.0f, height - static_cast<float>(_contentTop)));
		_nativeGrid->NameColumnWidth = std::clamp(
			_nativeGrid->NameColumnWidth,
			96.0f,
			std::max(96.0f, width - 96.0f));
	}
}

PropertyGrid::ViewState& PropertyGrid::CurrentViewState() noexcept
{
	return _viewMode == DesignerPropertyGridViewMode::Events
		? _eventsViewState : _propertiesViewState;
}

const PropertyGrid::ViewState& PropertyGrid::CurrentViewState() const noexcept
{
	return _viewMode == DesignerPropertyGridViewMode::Events
		? _eventsViewState : _propertiesViewState;
}

void PropertyGrid::CaptureCurrentViewState()
{
	// The grid may still show the previous mode when two shortcuts arrive before
	// the deferred reload. Never save those rows into the newly selected mode.
	if (_loadedViewMode != _viewMode) return;
	auto& state = CurrentViewState();
	state.Filter = _propertyFilter;
	state.CollapsedCategories.clear();
	if (!_nativeGrid) return;
	state.ScrollOffset = _nativeGrid->ScrollYOffset;
	for (const auto& item : _nativeGrid->Items)
		if (!item.Category.empty()
			&& _nativeGrid->IsCategoryCollapsed(item.Category))
			state.CollapsedCategories.insert(item.Category);
}

void PropertyGrid::RestoreCurrentViewState()
{
	if (!_nativeGrid) return;
	const auto& state = CurrentViewState();
	for (const auto& category : state.CollapsedCategories)
		_nativeGrid->CollapseCategory(category, true);
	_nativeGrid->SetScrollOffset(state.ScrollOffset);
}

void PropertyGrid::UpdateViewModePresentation()
{
	const bool events = _viewMode == DesignerPropertyGridViewMode::Events;
	if (_propertiesModeButton)
	{
		cui::framework::NativeVisualStateAccess::Set(
			*_propertiesModeButton, ControlStyleState::Checked, !events);
		_propertiesModeButton->AutomationFullDescription = !events
			? L"当前正在显示属性；可按 Ctrl+1 切换"
			: L"切换到属性视图；快捷键 Ctrl+1";
		_propertiesModeButton->InvalidateVisual();
	}
	if (_eventsModeButton)
	{
		cui::framework::NativeVisualStateAccess::Set(
			*_eventsModeButton, ControlStyleState::Checked, events);
		_eventsModeButton->AutomationFullDescription = events
			? L"当前正在显示事件；可按 Ctrl+2 切换"
			: L"切换到事件视图；快捷键 Ctrl+2";
		_eventsModeButton->InvalidateVisual();
	}
	if (_nativeGrid)
	{
		_nativeGrid->SetHeaderLabels(events ? L"事件" : L"属性",
			events ? L"处理函数" : L"值");
		_nativeGrid->AutomationName = events
			? L"设计器事件网格" : L"设计器属性网格";
	}
	if (_filterLabel)
		_filterLabel->AutomationName = events ? L"事件筛选标签" : L"属性筛选标签";
	if (_filterBox)
	{
		_filterBox->AutomationName = events ? L"筛选事件" : L"筛选属性";
		_filterBox->AutomationFullDescription = events
			? L"按事件名称、分类或处理函数筛选；多个关键词需同时匹配。"
			: L"按属性名称、分类、当前值或值来源筛选；多个关键词需同时匹配。";
	}
}

void PropertyGrid::SetViewMode(DesignerPropertyGridViewMode mode)
{
	if (_viewMode == mode) return;
	CommitPendingEdits();
	CaptureCurrentViewState();
	_viewMode = mode;
	_propertyFilter = CurrentViewState().Filter;
	_syncingViewModeControls = true;
	if (_filterBox) _filterBox->Text = _propertyFilter;
	_syncingViewModeControls = false;
	_restoreViewStatePending = true;
	UpdateViewModePresentation();
	_reloadRequested = true;
	InvalidateVisual();
}

void PropertyGrid::SetEventHandlerCodeInspection(
	DesignerModel::DesignEventHandlerCodeInspection inspection)
{
	_eventCodeInspection = std::move(inspection);
	if (_viewMode == DesignerPropertyGridViewMode::Events)
		_reloadRequested = true;
}

void PropertyGrid::SetFilterText(std::wstring value)
{
	if (_propertyFilter == value) return;
	_propertyFilter = std::move(value);
	CurrentViewState().Filter = _propertyFilter;
	_syncingViewModeControls = true;
	if (_filterBox) _filterBox->Text = _propertyFilter;
	_syncingViewModeControls = false;
	_reloadRequested = true;
	InvalidateVisual();
}

void PropertyGrid::BeginNativeRowsReload()
{
	CommitGroupedFloatSliderEdit();
	_diagnosticConnections.clear();
	_nativeEntries.clear();
	_nativeItemBuffer.clear();
	_nativeSliderEditAccepted = true;
	_propertyRows.clear();
}

void PropertyGrid::CommitNativeRowsReload()
{
	if (!_nativeGrid)
	{
		_nativeItemBuffer.clear();
		return;
	}
	_syncingNativeGrid = true;
	_nativeGrid->SetItems(std::move(_nativeItemBuffer));
	_syncingNativeGrid = false;
	_loadedViewMode = _viewMode;
	if (_restoreViewStatePending)
	{
		_restoreViewStatePending = false;
		RestoreCurrentViewState();
	}
}

void PropertyGrid::PopulateNativePropertyRows(
	const std::vector<DesignerPropertyRow>& rows,
	const std::wstring& scopeCaption)
{
	if (!_nativeGrid) return;
	if (rows.empty() && HasActivePropertyFilter())
	{
		AddNativeInformationalRow(
			L"筛选", L"没有匹配的标量属性");
		return;
	}

	auto appendDescription = [](std::wstring& target, const std::wstring& value)
	{
		if (value.empty()) return;
		if (!target.empty()) target += L"；";
		target += value;
	};

	for (const auto& row : rows)
	{
		PropertyGridItem item;
		item.Category = scopeCaption + L" · "
			+ DesignerCategoryCaption(row.Category);
		item.Name = row.DisplayName;
		item.Value = row.HasMixedValue ? kMixedValueText : row.Value.Text;
		item.ReadOnly = row.IsReadOnly;
		item.IsMixed = row.HasMixedValue;
		item.CanReset = row.CanReset && !row.IsReadOnly;

		if (row.HasMixedValueSource)
		{
			item.Name += L"  [混合来源]";
			appendDescription(item.Description, L"所选控件的有效值来源不同");
		}
		else if (row.EffectiveValueSource)
		{
			const auto source = DesignerValueSourceCaption(*row.EffectiveValueSource);
			item.Name += L"  [" + source + L"]";
			appendDescription(item.Description, L"当前有效值来源：" + source);
		}
		if (row.HasConfiguredBinding)
		{
			item.Name += L"  [绑定配置]";
			appendDescription(item.Description, L"此属性存在绑定配置或运行时绑定");
		}
		if (row.HasMixedDiagnostics)
		{
			item.Name += L"  [诊断不一致]";
			appendDescription(item.Description,
				L"所选控件的绑定、校验或样式诊断不同");
		}
		bool hasError = false;
		bool hasWarning = false;
		for (const auto& diagnostic : row.Diagnostics)
		{
			hasError = hasError
				|| diagnostic.Severity == BindingValidationSeverity::Error;
			hasWarning = hasWarning
				|| diagnostic.Severity == BindingValidationSeverity::Warning;
			appendDescription(item.Description,
				diagnostic.Summary + (diagnostic.Details.empty()
					? L"" : L"：" + diagnostic.Details));
		}
		if (hasError) item.Name += L"  [错误]";
		else if (hasWarning) item.Name += L"  [警告]";

		switch (row.Editor)
		{
		case DesignerPropertyRowEditorKind::Boolean:
			item.ValueType = PropertyGridValueType::Bool;
			break;
		case DesignerPropertyRowEditorKind::Choice:
			item.ValueType = PropertyGridValueType::Enum;
			for (const auto& choice : row.Choices)
			{
				item.Options.push_back(choice.DisplayName);
				if (!row.HasMixedValue && EnumOptionMatchesValue(
					choice.ValueText, row.Value.Text))
					item.Value = choice.DisplayName;
			}
			break;
		case DesignerPropertyRowEditorKind::Color:
			item.ValueType = PropertyGridValueType::Color;
			break;
		case DesignerPropertyRowEditorKind::FloatSlider:
			item.ValueType = PropertyGridValueType::Slider;
			item.Minimum = row.Minimum.value_or(0.0);
			item.Maximum = row.Maximum.value_or(1.0);
			item.Step = row.Step.value_or(
				(item.Maximum - item.Minimum) / 100.0);
			if (!std::isfinite(item.Step) || item.Step <= 0.0)
				item.Step = 0.01;
			if (row.Minimum && row.Maximum)
				appendDescription(item.Description,
					L"范围 " + DoubleToText(*row.Minimum)
					+ L" – " + DoubleToText(*row.Maximum));
			break;
		case DesignerPropertyRowEditorKind::FontFamily:
			item.ValueType = PropertyGridValueType::Enum;
			item.Options = GetFontFamilyOptions();
			if (!row.HasMixedValue && item.Value.empty())
				item.Value = kFontDefaultOption;
			break;
		case DesignerPropertyRowEditorKind::FontSize:
			item.ValueType = PropertyGridValueType::Enum;
			item.Options = GetFontSizeOptions();
			break;
		case DesignerPropertyRowEditorKind::Thickness:
		case DesignerPropertyRowEditorKind::Text:
		default:
			item.ValueType = PropertyGridValueType::Text;
			break;
		}

		_nativeItemBuffer.push_back(std::move(item));
		NativeGridEntry entry;
		entry.Kind = NativeGridEntryKind::Property;
		entry.PropertyName = row.Name;
		_nativeEntries.push_back(std::move(entry));
	}
}

void PropertyGrid::AddNativeEventRow(
	const DesignerEventDescriptor& event,
	const std::wstring& subjectName,
	const std::wstring& storedHandler,
	const std::wstring& category,
	bool hasMixedValue,
	size_t targetCount)
{
	if (!_nativeGrid) return;
	const auto resolvedName = DesignerEventCatalog::NormalizeHandlerName(
		storedHandler);
	const auto currentName = hasMixedValue ? std::wstring{} : resolvedName;
	const auto defaultName = DesignerEventCatalog::MakeDefaultHandlerName(
		subjectName, event.Name);
	const auto code = GetEventCodePresentation(
		_eventCodeInspection, currentName);
	PropertyGridItem item(
		category,
		event.DisplayName.empty() ? event.Name : event.DisplayName,
		hasMixedValue ? kMixedValueText : currentName,
		PropertyGridValueType::EditableEnum);
	item.IsMixed = hasMixedValue;
	item.CanReset = hasMixedValue || !currentName.empty();
	if (!code.Badge.empty()) item.Name += L"  [" + code.Badge + L"]";
	item.Options = GetCompatibleHandlerNames(
		_binding.GetCanvas(), event, defaultName, currentName,
		_eventCodeInspection);
	item.Description = event.IsDefault ? L"默认事件。" : L"";
	if (targetCount > 1)
	{
		if (!item.Description.empty()) item.Description += L" ";
		item.Description += L"编辑会同时应用到 "
			+ std::to_wstring(targetCount) + L" 个选中控件。";
	}
	if (!code.Diagnostic.empty())
	{
		if (!item.Description.empty()) item.Description += L" ";
		item.Description += L"代码状态：" + code.Diagnostic;
	}
	item.Description += L"签名：void Handler("
		+ std::wstring(event.ParameterList.begin(), event.ParameterList.end())
		+ L")。留空表示不绑定；F4 可复用文档或用户源码中的同签名函数；"
			L"双击或按 F12 生成/定位处理函数。";
	_nativeItemBuffer.push_back(std::move(item));
	NativeGridEntry entry;
	entry.Kind = NativeGridEntryKind::Event;
	entry.PropertyName = event.Name;
	_nativeEntries.push_back(std::move(entry));
}

void PropertyGrid::PopulateNativeEventRows(
	const std::vector<DesignerEventDescriptor>& events,
	const std::wstring& subjectName,
	const DesignerModel::DesignEventHandlerMap& handlers,
	const std::wstring& scopeCaption)
{
	const auto before = _nativeItemBuffer.size();
	for (const auto& event : events)
	{
		const std::wstring category = scopeCaption + L" · "
			+ std::wstring(DesignerEventCatalog::GetCategoryDisplayName(
				event.Category));
		const auto it = handlers.find(event.Name);
		const std::wstring storedHandler =
			it == handlers.end() ? L"" : it->second;
		const auto currentHandler = DesignerEventCatalog::NormalizeHandlerName(
			storedHandler);
		const auto code = GetEventCodePresentation(
			_eventCodeInspection, currentHandler);
		if (!MatchesCurrentFilter(event.Name + L" " + event.DisplayName
			+ L" Event 事件 "
			+ category + L" " + currentHandler + L" "
			+ code.Badge + L" " + code.Diagnostic + L" "
			+ std::wstring(event.ParameterList.begin(),
				event.ParameterList.end()))) continue;
		AddNativeEventRow(
			event, subjectName, storedHandler, category);
	}
	const bool hasVisibleEventRows = _nativeItemBuffer.size() > before;
	if (hasVisibleEventRows)
		AddNativeEventActivationRow(scopeCaption);
	AddNativeEventHandlerManagerRow(scopeCaption);
	if (!hasVisibleEventRows && HasActivePropertyFilter())
		AddNativeInformationalRow(L"筛选", L"没有匹配的事件");
}

void PropertyGrid::PopulateNativeMultiSelectionEventRows(
	const std::vector<std::shared_ptr<DesignerControl>>& controls,
	const std::shared_ptr<DesignerControl>& primaryControl,
	const std::wstring& scopeCaption)
{
	if (!_nativeGrid || controls.empty() || !primaryControl) return;
	auto common = DesignerEventCatalog::GetControlEvents(
		primaryControl->Type, primaryControl->ComponentEvents);
	for (const auto& control : controls)
	{
		if (!control || control == primaryControl) continue;
		common.erase(std::remove_if(
			common.begin(), common.end(), [&](DesignerEventDescriptor& candidate)
			{
				const auto matching = DesignerEventCatalog::FindControlEvent(
					control->Type, candidate.Name, control->ComponentEvents);
				if (!matching || !candidate.SameSignature(*matching)) return true;
				candidate.IsDefault = candidate.IsDefault && matching->IsDefault;
				return false;
			}), common.end());
	}

	const auto before = _nativeItemBuffer.size();
	for (const auto& event : common)
	{
		std::wstring primaryStored;
		if (const auto found = primaryControl->EventHandlers.find(event.Name);
			found != primaryControl->EventHandlers.end())
			primaryStored = found->second;
		const auto primaryResolved = DesignerEventCatalog::NormalizeHandlerName(
			primaryStored);
		bool mixed = false;
		for (const auto& control : controls)
		{
			if (!control) { mixed = true; break; }
			const auto matching = DesignerEventCatalog::FindControlEvent(
				control->Type, event.Name, control->ComponentEvents);
			if (!matching || !event.SameSignature(*matching))
			{
				mixed = true;
				break;
			}
			std::wstring stored;
			if (const auto found = control->EventHandlers.find(matching->Name);
				found != control->EventHandlers.end())
				stored = found->second;
			if (DesignerEventCatalog::NormalizeHandlerName(stored)
				!= primaryResolved)
			{
				mixed = true;
				break;
			}
		}

		const std::wstring category = scopeCaption + L" · "
			+ std::wstring(DesignerEventCatalog::GetCategoryDisplayName(
				event.Category));
		const auto code = GetEventCodePresentation(
			_eventCodeInspection, mixed ? std::wstring{} : primaryResolved);
		if (!MatchesCurrentFilter(event.Name + L" " + event.DisplayName
			+ L" Event 事件 公共 多选 " + category + L" "
			+ (mixed ? kMixedValueText : primaryResolved) + L" "
			+ code.Badge + L" " + code.Diagnostic + L" "
			+ std::wstring(event.ParameterList.begin(),
				event.ParameterList.end()))) continue;
		AddNativeEventRow(
			event, primaryControl->Name, primaryStored, category,
			mixed, controls.size());
	}
	const bool hasVisibleEventRows = _nativeItemBuffer.size() > before;
	if (hasVisibleEventRows)
		AddNativeEventActivationRow(scopeCaption);
	AddNativeEventHandlerManagerRow(scopeCaption);
	if (!hasVisibleEventRows)
	{
		AddNativeInformationalRow(
			HasActivePropertyFilter() ? L"筛选" : scopeCaption,
			HasActivePropertyFilter()
				? L"没有匹配的公共事件"
				: L"所选控件没有签名一致的公共事件");
	}
}

void PropertyGrid::AddNativeEventActivationRow(
	const std::wstring& category)
{
	const auto eventName = ResolveEventActivationName();
	AddNativeActionRow(
		category,
		L"生成/定位处理函数",
		eventName.empty() ? L"F12" : L"F12 · " + eventName,
		L"先选中事件行；未绑定时会通过可撤销文档命令写入约定处理函数名，"
			L"再生成或定位用户代码；不会覆盖现有函数体。",
		[this]() { ActivateSelectedEventHandler(); });
}

void PropertyGrid::AddNativeEventHandlerManagerRow(
	const std::wstring& category)
{
	if (!_nativeGrid
		|| !MatchesCurrentFilter(L"Event Handler Rename 事件 处理函数 重命名"))
		return;
	auto* canvas = _binding.GetCanvas();
	if (!canvas) return;
	DesignerModel::DesignDocumentEventIndex index;
	std::wstring error;
	if (!canvas->BuildEventHandlerIndex(index, &error))
	{
		AddNativeInformationalRow(category, L"处理函数索引",
			error.empty() ? L"无法建立事件索引" : error);
		return;
	}
	if (index.Handlers().empty()) return;

	std::wstring preferred;
	if (_binding.IsWindowBinding())
	{
		const auto& handlers = canvas->GetDesignedWindowEventHandlers();
		if (!handlers.empty())
			preferred = DesignerEventCatalog::NormalizeHandlerName(
				handlers.begin()->second);
	}
	else if (const auto control = _binding.GetBoundControl();
		control && !control->EventHandlers.empty())
	{
		const auto& first = *control->EventHandlers.begin();
		preferred = DesignerEventCatalog::NormalizeHandlerName(first.second);
	}

	AddNativeActionRow(
		category,
		L"重命名处理函数",
		L"管理 (" + std::to_wstring(index.Handlers().size()) + L")…",
		L"按签名索引文档中的函数名；可选择同步迁移唯一兼容的用户函数体。",
		[this, preferred]()
		{
			auto* currentCanvas = _binding.GetCanvas();
			if (!currentCanvas || !GetPresentationWindow()) return;
			DesignerModel::DesignDocumentEventIndex currentIndex;
			std::wstring indexError;
			if (!currentCanvas->BuildEventHandlerIndex(
				currentIndex, &indexError))
			{
				ShowPropertyEditError(L"重命名处理函数", indexError);
				return;
			}
			EventHandlerEditorDialog dialog(
				currentIndex, preferred, &_eventCodeInspection);
			dialog.Owner = GetPresentationWindow();
			(void)dialog.ShowDialog();
			if (!dialog.Applied) return;

			size_t renamed = 0;
			std::wstring renameError;
			DesignerEventHandlerCodeMigration migration;
			const DesignerEventHandlerCodeMigration* migrationRequest = nullptr;
			if (dialog.MigrateUserCode)
			{
				const auto* source = currentIndex.FindHandler(dialog.OldName);
				if (!source)
				{
					ShowPropertyEditError(L"重命名处理函数",
						L"重命名确认后事件索引已变化，请重试。");
					return;
				}
				migration.OutputBasePath =
					_eventCodeInspection.Target.OutputBasePath;
				migration.ClassName = _eventCodeInspection.Target.ClassName;
				const auto codeEntry =
					_eventCodeInspection.Handlers.find(dialog.OldName);
				if (codeEntry == _eventCodeInspection.Handlers.end()
					|| codeEntry->second.DefinitionFilePath.empty())
				{
					ShowPropertyEditError(L"重命名处理函数",
						L"无法确定用户函数体所在文件，请刷新代码状态后重试。");
					return;
				}
				migration.UserCodePath =
					codeEntry->second.DefinitionFilePath;
				migration.ParameterList = source->ParameterList;
				migration.OldName = dialog.OldName;
				migration.NewName = dialog.NewName;
				migrationRequest = &migration;
			}
			auto transaction = currentCanvas->RenameEventHandler(
				dialog.OldName, dialog.NewName, &renamed, &renameError,
				migrationRequest);
			if (transaction)
			{
				_reloadRequested = true;
				ClearPropertyEditError();
			}
			else
			{
				ShowPropertyEditError(
					L"RenameEventHandler",
					renameError.empty() ? transaction.Error : renameError);
			}
		});
}

void PropertyGrid::AddNativeActionRow(
	const std::wstring& category,
	const std::wstring& name,
	const std::wstring& value,
	const std::wstring& description,
	std::function<void()> action)
{
	if (!_nativeGrid) return;
	PropertyGridItem item(
		category, name, value, PropertyGridValueType::Action);
	item.Description = description;
	_nativeItemBuffer.push_back(std::move(item));
	NativeGridEntry entry;
	entry.Kind = NativeGridEntryKind::Action;
	entry.PropertyName = name;
	entry.Action = std::move(action);
	_nativeEntries.push_back(std::move(entry));
}

void PropertyGrid::AddNativeInformationalRow(
	const std::wstring& category,
	const std::wstring& name,
	const std::wstring& value)
{
	if (!_nativeGrid) return;
	PropertyGridItem item(
		category, name, value, PropertyGridValueType::ReadOnly);
	item.ReadOnly = true;
	_nativeItemBuffer.push_back(std::move(item));
	_nativeEntries.push_back({ NativeGridEntryKind::Informational, L"", {} });
}

void PropertyGrid::HandleNativeValueChanged(
	int index,
	const std::wstring& oldValue,
	const std::wstring& newValue)
{
	if (_syncingNativeGrid || !_nativeGrid
		|| index < 0 || index >= static_cast<int>(_nativeEntries.size())
		|| index >= static_cast<int>(_nativeGrid->Items.size())) return;
	const auto entry = _nativeEntries[static_cast<size_t>(index)];
	DesignerPropertyEditResult result = DesignerPropertyEditResult::Failure(
		L"属性行不支持标量编辑。");
	bool groupedSliderPreview = false;
	if (entry.Kind == NativeGridEntryKind::Event)
	{
		result = UpdatePropertyFromTextBox(entry.PropertyName, newValue);
	}
	else if (entry.Kind == NativeGridEntryKind::Property)
	{
		auto valueText = newValue;
		const auto* row = DesignerPropertyRowCatalog::Find(
			_propertyRows, entry.PropertyName);
		if (row)
		{
			if (row->Editor == DesignerPropertyRowEditorKind::Choice)
			{
				for (const auto& choice : row->Choices)
				{
					if (choice.DisplayName == newValue)
					{
						valueText = choice.ValueText;
						break;
					}
				}
			}
		}
		if (row && row->Editor == DesignerPropertyRowEditorKind::FloatSlider
			&& !_nativeSliderEditAccepted)
		{
			result = DesignerPropertyEditResult::Failure(
				_editErrorMessage.empty()
					? L"滑块事务无法开始。" : _editErrorMessage);
		}
		else if (row && row->Editor == DesignerPropertyRowEditorKind::FloatSlider
			&& _pendingFloatSliderCommand.Active
			&& _pendingFloatSliderCommand.PropertyName == entry.PropertyName)
		{
			groupedSliderPreview = true;
			float value = 0.0f;
			if (TryParseFloatWs(valueText, value))
				result = UpdateFloatPropertyPreview(entry.PropertyName, value);
			else
				result = DesignerPropertyEditResult::Failure(
					L"滑块生成了无效数值。");
		}
		else
			result = UpdatePropertyFromTextBox(entry.PropertyName, valueText);
	}

	if (result)
	{
		if (!groupedSliderPreview) _reloadRequested = true;
		return;
	}

	_syncingNativeGrid = true;
	const auto* row = DesignerPropertyRowCatalog::Find(
		_propertyRows, entry.PropertyName);
	(void)_nativeGrid->Items.Modify(
		static_cast<size_t>(index),
		[&](PropertyGridItem& item)
		{
			item.Value = oldValue;
			if (row) item.IsMixed = row->HasMixedValue;
		});
	_syncingNativeGrid = false;
	_nativeGrid->InvalidateVisual();
}

void PropertyGrid::HandleNativeItemClick(int index)
{
	if (index < 0 || index >= static_cast<int>(_nativeEntries.size())) return;
	auto action = _nativeEntries[static_cast<size_t>(index)].Action;
	if (_nativeEntries[static_cast<size_t>(index)].Kind
		== NativeGridEntryKind::Action && action)
		action();
}

void PropertyGrid::HandleNativeDoubleClick(int index)
{
	if (index < 0 || index >= static_cast<int>(_nativeEntries.size())) return;
	const auto& entry = _nativeEntries[static_cast<size_t>(index)];
	if (entry.Kind != NativeGridEntryKind::Event) return;
	RememberNativeSelection(index);
	if (_nativeGrid) _nativeGrid->CancelEdit();
	ActivateSelectedEventHandler();
}

void PropertyGrid::RememberNativeSelection(int index)
{
	if (index < 0 || index >= static_cast<int>(_nativeEntries.size())) return;
	const auto& entry = _nativeEntries[static_cast<size_t>(index)];
	if (entry.Kind == NativeGridEntryKind::Event
		&& !entry.PropertyName.empty())
		_lastSelectedEventName = entry.PropertyName;
}

std::wstring PropertyGrid::ResolveEventActivationName() const
{
	auto isVisibleEvent = [this](const std::wstring& name)
	{
		return !name.empty() && std::any_of(
			_nativeEntries.begin(), _nativeEntries.end(),
			[&name](const NativeGridEntry& entry)
			{
				return entry.Kind == NativeGridEntryKind::Event
					&& entry.PropertyName == name;
			});
	};
	if (isVisibleEvent(_lastSelectedEventName))
		return _lastSelectedEventName;

	std::optional<DesignerEventDescriptor> defaultEvent;
	if (_binding.IsWindowBinding())
		defaultEvent = DesignerEventCatalog::GetDefaultWindowEvent();
	else if (const auto control = _binding.GetBoundControl())
		defaultEvent = DesignerEventCatalog::GetDefaultControlEvent(
			control->Type, control->ComponentEvents);
	if (defaultEvent && isVisibleEvent(defaultEvent->Name))
		return defaultEvent->Name;

	for (const auto& entry : _nativeEntries)
		if (entry.Kind == NativeGridEntryKind::Event
			&& !entry.PropertyName.empty())
			return entry.PropertyName;
	return L"";
}

void PropertyGrid::ActivateSelectedEventHandler()
{
	const auto eventName = ResolveEventActivationName();
	if (eventName.empty())
	{
		ShowPropertyEditError(L"事件", L"请先选择一个事件。");
		return;
	}
	if (_nativeGrid)
	{
		ClearPropertyEditError();
		_nativeGrid->CommitEdit();
		if (HasPropertyEditError()) return;
	}
	const auto result = ActivateEventHandler(eventName);
	if (result) ClearPropertyEditError();
	else ShowPropertyEditError(eventName, result.Error);
}

void PropertyGrid::HandleNativeResetRequested(int index)
{
	if (index < 0 || index >= static_cast<int>(_nativeEntries.size())) return;
	const auto entry = _nativeEntries[static_cast<size_t>(index)];
	if (entry.Kind == NativeGridEntryKind::Event
		&& !entry.PropertyName.empty())
	{
		const auto result = UpdatePropertyFromTextBox(entry.PropertyName, L"");
		if (result) _reloadRequested = true;
		return;
	}
	if (entry.Kind != NativeGridEntryKind::Property
		|| entry.PropertyName.empty()) return;
	ResetCurrentProperty(entry.PropertyName);
}

void PropertyGrid::PreparePresentation()
{
	Panel::PreparePresentation();
	if (_reloadRequested)
	{
		_reloadRequested = false;
		auto controls = _binding.GetBoundControls();
		auto primary = _binding.GetBoundControl();
		LoadControls(controls, primary);
	}
	UpdateContentHostLayout();
}

void PropertyGrid::OnRender()
{
	Panel::OnRender();
}

bool PropertyGrid::HasActivePropertyFilter() const
{
	return !DesignerPropertyRowCatalog::MatchesFilterText(L"", _propertyFilter);
}

bool PropertyGrid::MatchesCurrentFilter(
	const std::wstring& searchableText) const
{
	return DesignerPropertyRowCatalog::MatchesFilterText(
		searchableText, _propertyFilter);
}

void PropertyGrid::ShowPropertyEditError(
	const std::wstring& propertyName,
	const std::wstring& message)
{
	_editErrorProperty = propertyName;
	_editErrorMessage = message.empty() ? L"属性修改被拒绝。" : message;
	if (!_editErrorLabel) return;
	_editErrorLabel->Text = L"错误 · "
		+ (propertyName.empty() ? std::wstring(L"属性") : propertyName)
		+ L"：" + _editErrorMessage;
	_editErrorLabel->AutomationName = L"属性编辑错误";
	_editErrorLabel->AutomationFullDescription = _editErrorLabel->Text;
	_editErrorLabel->Visibility = Visibility::Visible;
	_editErrorLabel->InvalidateVisual();
}

void PropertyGrid::ClearPropertyEditError()
{
	_editErrorProperty.clear();
	_editErrorMessage.clear();
	if (!_editErrorLabel) return;
	_editErrorLabel->Text.clear();
	_editErrorLabel->AutomationFullDescription.clear();
	_editErrorLabel->Visibility = Visibility::Collapsed;
	_editErrorLabel->InvalidateVisual();
}

void PropertyGrid::SubscribePropertyDiagnosticChanges()
{
	for (const auto& control : _binding.GetBoundControls())
	{
		if (!control || !control->ControlInstance) continue;
		auto* runtime = control->ControlInstance;
		_diagnosticConnections.push_back(
			runtime->OnValidationStateChanged.Subscribe(
				[this](const BindingValidationChangedEventArgs&)
				{
					_reloadRequested = true;
				}));
		if (const auto style =
			cui::framework::StyleAccess::DocumentStyles(*runtime))
			_diagnosticConnections.push_back(style->SubscribeChanged([this]()
			{
				_reloadRequested = true;
			}));
		if (const auto theme = cui::framework::StyleAccess::Theme(*runtime))
			_diagnosticConnections.push_back(theme->SubscribeChanged([this]()
			{
				_reloadRequested = true;
			}));
		if (const auto resources =
			cui::framework::StyleAccess::Resources(*runtime))
			_diagnosticConnections.push_back(resources->SubscribeChanged([this]()
			{
				_reloadRequested = true;
			}));
	}
}

void PropertyGrid::RefreshPropertyValueSource(
	const std::wstring& propertyName)
{
	(void)propertyName;
	// Effective source, binding validation, and rule attribution form one row
	// snapshot; rebuild them together so a small in-place update cannot leave
	// stale diagnostic text behind.
	_reloadRequested = true;
}

DesignerPropertyEditResult PropertyGrid::ResetCurrentProperty(
	const std::wstring& propertyName)
{
	auto result = ExecutePropertyEditCommand(
		L"Reset " + propertyName, [this, propertyName]
	{
		if (_binding.IsWindowBinding())
		{
			std::wstring error;
			if (!_binding.ResetWindowProperty(
				propertyName, nullptr, &error))
				return DesignerPropertyEditResult::Failure(error);
			return DesignerPropertyEditResult::Success(1);
		}
		return _binding.ResetControlPropertyValue(propertyName);
	});
	if (result)
	{
		_reloadRequested = true;
		this->InvalidateVisual();
	}
	return result;
}

bool PropertyGrid::ProcessInput(const InputReport& input)
{
	if (input.Kind == InputReportKind::KeyDown)
	{
		const bool controlDown = input.HasModifier(ModifierKeys::Control);
		if (controlDown && input.Key == Key::D1)
		{
			SetViewMode(DesignerPropertyGridViewMode::Properties);
			return true;
		}
		if (controlDown && input.Key == Key::D2)
		{
			SetViewMode(DesignerPropertyGridViewMode::Events);
			return true;
		}
		auto* canvas = _binding.GetCanvas();
		if (canvas && controlDown)
		{
			const bool shiftDown = input.HasModifier(ModifierKeys::Shift);
			if (input.Key == Key::Z && !shiftDown)
			{
				auto result = canvas->UndoCommand();
				if (!result || result.HasChanges()) return true;
			}
			else if (input.Key == Key::Y
				|| (input.Key == Key::Z && shiftDown))
			{
				auto result = canvas->RedoCommand();
				if (!result || result.HasChanges()) return true;
			}
		}
	}
	return Panel::ProcessInput(input);
}

bool PropertyGrid::BeginGroupedFloatSliderEdit(const std::wstring& propertyName)
{
	if (_pendingFloatSliderCommand.Active)
	{
		if (_pendingFloatSliderCommand.PropertyName == propertyName) return true;
		ShowPropertyEditError(
			propertyName, L"另一个滑块事务仍在进行，当前修改未执行。");
		return false;
	}

	auto* canvas = _binding.GetCanvas();
	if (!canvas)
	{
		ShowPropertyEditError(
			propertyName, L"设计画布不可用，滑块修改未执行。");
		return false;
	}
	DesignerPropertyBatchSnapshot before;
	std::wstring error;
	if (!_binding.CaptureControlPropertySnapshot(
		propertyName, before, &error))
	{
		ShowPropertyEditError(propertyName, error);
		return false;
	}

	_pendingFloatSliderCommand.Active = true;
	_pendingFloatSliderCommand.PropertyName = propertyName;
	_pendingFloatSliderCommand.Before = std::move(before);
	for (const auto& control : canvas->GetSelectedControls())
		if (control && !control->Name.empty())
			_pendingFloatSliderCommand.BeforeSelectionNames.push_back(
				control->Name);
	_pendingFloatSliderCommand.BeforePrimarySelectionName =
		canvas->GetSelectedControl()
			? canvas->GetSelectedControl()->Name : std::wstring{};
	return true;
}

void PropertyGrid::CommitGroupedFloatSliderEdit()
{
	if (!_pendingFloatSliderCommand.Active)
	{
		return;
	}

	auto pending = std::move(_pendingFloatSliderCommand);
	const auto propertyName = pending.PropertyName;
	_pendingFloatSliderCommand = PendingFloatSliderCommand{};
	auto* canvas = _binding.GetCanvas();
	if (!canvas)
	{
		ShowPropertyEditError(
			propertyName, L"设计画布不可用，滑块修改未提交。");
		return;
	}

	DesignerPropertyBatchSnapshot after;
	std::wstring error;
	if (!_binding.CaptureControlPropertySnapshot(
		propertyName, after, &error))
	{
		std::wstring restoreError;
		const bool restored = _binding.RestoreBoundControlPropertySnapshot(
			pending.Before, &restoreError);
		canvas->RestoreSelectionByNames(
			pending.BeforeSelectionNames,
			pending.BeforePrimarySelectionName,
			true);
		ShowPropertyEditError(propertyName,
			L"无法建立滑块修改后的差量：" + error
				+ (restored ? L"" : L" 属性恢复失败：" + restoreError));
		return;
	}
	std::vector<std::wstring> afterSelectionNames;
	for (const auto& control : canvas->GetSelectedControls())
		if (control && !control->Name.empty())
			afterSelectionNames.push_back(control->Name);
	const auto afterPrimarySelectionName = canvas->GetSelectedControl()
		? canvas->GetSelectedControl()->Name : std::wstring{};
	if (!pending.Before.EquivalentTo(after)
		|| pending.BeforeSelectionNames != afterSelectionNames
		|| pending.BeforePrimarySelectionName != afterPrimarySelectionName)
	{
		const auto rollback = pending.Before;
		DesignerDocumentTransactionResult result =
			DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"无法把滑块差量加入撤销栈。", false);
		try
		{
			auto command = std::make_unique<ControlPropertyCommand>(
				canvas,
				std::move(pending.Before),
				std::move(after),
				pending.BeforeSelectionNames,
				std::move(afterSelectionNames),
				pending.BeforePrimarySelectionName,
				afterPrimarySelectionName,
				L"UpdateProperty:" + propertyName,
				true);
			result = canvas->CommitAlreadyAppliedCommand(std::move(command));
		}
		catch (...)
		{
			result = DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"记录滑块差量时抛出异常。", false);
		}
		if (!result || !result.HasChanges())
		{
			std::wstring restoreError;
			const bool restored = _binding.RestoreBoundControlPropertySnapshot(
				rollback, &restoreError);
			canvas->RestoreSelectionByNames(
				pending.BeforeSelectionNames,
				pending.BeforePrimarySelectionName,
				true);
			ShowPropertyEditError(propertyName,
				(result.Error.empty()
					? L"无法把滑块差量加入撤销栈。" : result.Error)
				+ (restored ? L"" : L" 属性恢复失败：" + restoreError));
			return;
		}
	}
	ClearPropertyEditError();
	_reloadRequested = true;
}

void PropertyGrid::RollbackGroupedFloatSliderEdit(const std::wstring& error)
{
	if (!_pendingFloatSliderCommand.Active)
	{
		if (!error.empty()) ShowPropertyEditError(L"Slider", error);
		return;
	}
	auto pending = std::move(_pendingFloatSliderCommand);
	const auto propertyName = pending.PropertyName;
	_pendingFloatSliderCommand = PendingFloatSliderCommand{};
	std::wstring message = error;
	if (auto* canvas = _binding.GetCanvas())
	{
		std::wstring restoreError;
		if (!_binding.RestoreBoundControlPropertySnapshot(
			pending.Before, &restoreError))
			message += L" 属性恢复失败：" + restoreError;
		canvas->RestoreSelectionByNames(
			pending.BeforeSelectionNames,
			pending.BeforePrimarySelectionName,
			true);
	}
	else
	{
		message += L" 设计画布不可用，文档未能恢复。";
	}
	_reloadRequested = true;
	if (message.empty()) ClearPropertyEditError();
	else ShowPropertyEditError(propertyName, message);
}

DesignerPropertyEditResult PropertyGrid::UpdateFloatPropertyPreview(
	const std::wstring& propertyName,
	float value)
{
	auto result = _binding.ApplyControlPropertyValue(
		propertyName, FloatToText(value));
	if (!result)
	{
		if (_pendingFloatSliderCommand.Active)
			RollbackGroupedFloatSliderEdit(result.Error);
		else
			ShowPropertyEditError(propertyName, result.Error);
		return result;
	}
	ClearPropertyEditError();
	if (!_pendingFloatSliderCommand.Active)
		RefreshPropertyValueSource(propertyName);
	return result;
}

DesignerPropertyEditResult PropertyGrid::UpdatePropertyFromTextBox(
	std::wstring propertyName,
	std::wstring value)
{
	std::optional<DesignerEventDescriptor> currentEvent;
	std::shared_ptr<DesignerControl> eventControl;
	if (_binding.IsWindowBinding())
		currentEvent = DesignerEventCatalog::FindWindowEvent(propertyName);
	else if ((eventControl = _binding.GetBoundControl()))
		currentEvent = DesignerEventCatalog::FindControlEvent(
			eventControl->Type, propertyName, eventControl->ComponentEvents);
	if (currentEvent)
	{
		auto* canvas = _binding.GetCanvas();
		if (!canvas)
		{
			auto failure = DesignerPropertyEditResult::Failure(
				L"设计画布不可用。");
			ShowPropertyEditError(propertyName, failure.Error);
			return failure;
		}
		std::wstring error;
		const auto eventTargetCount = !_binding.IsWindowBinding()
			? _binding.GetBoundControls().size() : size_t{ 1 };
		const bool multiControlEvent = eventTargetCount > 1;
		auto transaction = multiControlEvent
			? canvas->UpdateEventHandlers(
				_binding.GetBoundControls(), currentEvent->Name,
				TrimWs(value), &error)
			: canvas->UpdateEventHandler(
				eventControl, currentEvent->Name, TrimWs(value), &error);
		if (!transaction)
		{
			auto failure = DesignerPropertyEditResult::Failure(
				error.empty() ? transaction.Error : std::move(error));
			ShowPropertyEditError(propertyName, failure.Error);
			return failure;
		}
		ClearPropertyEditError();
		return DesignerPropertyEditResult::Success(
			transaction.HasChanges() ? eventTargetCount : 0);
	}

	auto result = ExecutePropertyEditCommand(propertyName, [this, propertyName, value]()
	{
		if (_binding.IsWindowBinding())
		{
			const auto rows = _binding.GetPropertyRows();
			const auto* property = DesignerPropertyRowCatalog::Find(
				rows, propertyName);
			if (!property)
				return DesignerPropertyEditResult::Failure(
					L"窗体没有属性 " + propertyName + L"。");
			auto normalizedValue = value;
			if (PropertyNamesEqual(property->Name, L"FontFamily"))
			{
				normalizedValue = TrimWs(normalizedValue);
				if (normalizedValue == kFontDefaultOption)
				{
					std::wstring error;
					if (!_binding.ResetWindowProperty(
						property->Name, nullptr, &error))
						return DesignerPropertyEditResult::Failure(error);
					return DesignerPropertyEditResult::Success(1);
				}
			}
			std::wstring error;
			if (!_binding.ApplyWindowProperty(
				property->Name,
				DesignerStyleValue{ property->Value.Kind, normalizedValue },
				nullptr,
				&error))
				return DesignerPropertyEditResult::Failure(error);
			return DesignerPropertyEditResult::Success(1);
		}

		return _binding.ApplyControlPropertyValue(propertyName, value);
	});
	if (result && !_binding.IsWindowBinding()
		&& !IsEventPropertyName(_binding, propertyName))
		RefreshPropertyValueSource(propertyName);
	return result;
}

DesignerPropertyEditResult PropertyGrid::UpdatePropertyFromBool(
	std::wstring propertyName,
	bool value)
{
	auto result = ExecutePropertyEditCommand(propertyName,
		[this, propertyName, value]()
	{
	// 未选中控件时：编辑“被设计窗体”属性
	if (_binding.IsWindowBinding())
	{
		auto* canvas = _binding.GetCanvas();
		if (!canvas) return DesignerPropertyEditResult::Failure(L"设计画布不可用。");
		const auto rows = _binding.GetPropertyRows();
		const auto* property = DesignerPropertyRowCatalog::Find(rows, propertyName);
		if (property && property->Value.Kind == DesignerStyleValueKind::Bool)
		{
			std::wstring error;
			if (!_binding.ApplyWindowProperty(
				property->Name,
				DesignerStyleValue{
					DesignerStyleValueKind::Bool,
					value ? L"true" : L"false" },
				nullptr,
				&error))
				return DesignerPropertyEditResult::Failure(error);
			return DesignerPropertyEditResult::Success(1);
		}
		return DesignerPropertyEditResult::Failure(
			L"窗体属性不是 Boolean：" + propertyName);
	}
	return _binding.ApplyControlPropertyValue(
		propertyName, value ? L"true" : L"false");
	});
	if (result && !_binding.IsWindowBinding()
		&& !IsEventPropertyName(_binding, propertyName))
		RefreshPropertyValueSource(propertyName);
	return result;
}

DesignerPropertyEditResult PropertyGrid::ExecutePropertyEditCommand(
	const std::wstring& propertyName,
	const std::function<DesignerPropertyEditResult()>& applyChange)
{
	if (!applyChange)
	{
		auto result = DesignerPropertyEditResult::Failure(
			L"属性编辑操作无效。");
		ShowPropertyEditError(propertyName, result.Error);
		return result;
	}

	auto* canvas = _binding.GetCanvas();
	if (!canvas)
	{
		DesignerPropertyEditResult result;
		try
		{
			result = applyChange();
		}
		catch (...)
		{
			result = DesignerPropertyEditResult::Failure(
				L"属性编辑操作抛出异常。");
		}
		if (result) ClearPropertyEditError();
		else ShowPropertyEditError(propertyName, result.Error);
		return result;
	}

	std::wstring snapshotPropertyName = propertyName;
	if (snapshotPropertyName.rfind(L"Reset ", 0) == 0)
		snapshotPropertyName.erase(0, 6);
	if (!_binding.IsWindowBinding()
		&& !IsEventPropertyName(_binding, snapshotPropertyName))
	{
		DesignerPropertyBatchSnapshot before;
		std::wstring captureError;
		if (_binding.CaptureControlPropertySnapshot(
			snapshotPropertyName, before, &captureError))
		{
			auto captureSelection = [canvas]()
			{
				std::vector<std::wstring> names;
				names.reserve(canvas->GetSelectedControls().size());
				for (const auto& control : canvas->GetSelectedControls())
					if (control && !control->Name.empty())
						names.push_back(control->Name);
				return names;
			};
			auto beforeSelectionNames = captureSelection();
			const auto beforePrimarySelectionName = canvas->GetSelectedControl()
				? canvas->GetSelectedControl()->Name : std::wstring{};

			DesignerPropertyEditResult result =
				DesignerPropertyEditResult::Failure(L"属性修改未执行。");
			try
			{
				result = applyChange();
			}
			catch (...)
			{
				result = DesignerPropertyEditResult::Failure(
					L"属性编辑操作抛出异常。");
			}
			if (!result)
			{
				std::wstring restoreError;
				const bool restored = _binding.RestoreBoundControlPropertySnapshot(
					before, &restoreError);
				canvas->RestoreSelectionByNames(
					beforeSelectionNames,
					beforePrimarySelectionName,
					true);
				if (!restored)
					result.Error += L" 属性恢复失败：" + restoreError;
				ShowPropertyEditError(propertyName, result.Error);
				return result;
			}

			DesignerPropertyBatchSnapshot after;
			if (!_binding.CaptureControlPropertySnapshot(
				snapshotPropertyName, after, &captureError))
			{
				std::wstring restoreError;
				const bool restored = _binding.RestoreBoundControlPropertySnapshot(
					before, &restoreError);
				canvas->RestoreSelectionByNames(
					beforeSelectionNames,
					beforePrimarySelectionName,
					true);
				result = DesignerPropertyEditResult::Failure(
					L"无法建立属性修改后的差量：" + captureError
					+ (restored ? L"" : L" 属性恢复失败：" + restoreError),
					result.AppliedCount);
				ShowPropertyEditError(propertyName, result.Error);
				return result;
			}
			auto afterSelectionNames = captureSelection();
			const auto afterPrimarySelectionName = canvas->GetSelectedControl()
				? canvas->GetSelectedControl()->Name : std::wstring{};
			if (before.EquivalentTo(after)
				&& beforeSelectionNames == afterSelectionNames
				&& beforePrimarySelectionName == afterPrimarySelectionName)
			{
				ClearPropertyEditError();
				return result;
			}

			const auto rollbackSnapshot = before;
			const auto rollbackSelectionNames = beforeSelectionNames;
			DesignerDocumentTransactionResult transaction =
				DesignerDocumentTransactionResult::Failure(
					DesignerDocumentTransactionState::Failed,
					L"无法把属性差量加入撤销栈。", false);
			try
			{
				auto command = std::make_unique<ControlPropertyCommand>(
					canvas,
					std::move(before),
					std::move(after),
					std::move(beforeSelectionNames),
					std::move(afterSelectionNames),
					beforePrimarySelectionName,
					afterPrimarySelectionName,
					L"UpdateProperty:" + propertyName,
					true);
				transaction = canvas->CommitAlreadyAppliedCommand(
					std::move(command));
			}
			catch (...)
			{
				transaction = DesignerDocumentTransactionResult::Failure(
					DesignerDocumentTransactionState::Failed,
					L"记录属性差量时抛出异常。", false);
			}
			if (!transaction || !transaction.HasChanges())
			{
				std::wstring restoreError;
				const bool restored = _binding.RestoreBoundControlPropertySnapshot(
					rollbackSnapshot, &restoreError);
				canvas->RestoreSelectionByNames(
					rollbackSelectionNames,
					beforePrimarySelectionName,
					true);
				result = DesignerPropertyEditResult::Failure(
					transaction.Error.empty()
						? L"无法把属性差量加入撤销栈。"
						: transaction.Error,
					result.AppliedCount);
				if (!restored)
					result.Error += L" 属性恢复失败：" + restoreError;
				ShowPropertyEditError(propertyName, result.Error);
				return result;
			}
			ClearPropertyEditError();
			return result;
		}
	}

	DesignerPropertyEditResult result =
		DesignerPropertyEditResult::Failure(L"属性修改未执行。");
	auto transaction = canvas->ExecuteDocumentEditTransaction(
		L"UpdateProperty:" + propertyName,
		[&applyChange, &result](std::wstring& error)
		{
			result = applyChange();
			if (result) return true;
			error = result.Error;
			return false;
		});
	if (!transaction)
	{
		result = DesignerPropertyEditResult::Failure(
			transaction.Error, result.AppliedCount);
		ShowPropertyEditError(propertyName, result.Error);
		return result;
	}
	ClearPropertyEditError();
	return result;
}

DesignerDocumentTransactionResult PropertyGrid::ExecutePropertyCommand(
	const std::wstring& propertyName,
	const std::function<bool(std::wstring& error)>& applyChange)
{
	if (!applyChange)
	{
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Rejected,
			L"属性操作无效。");
		ShowPropertyEditError(propertyName, result.Error);
		return result;
	}

	auto* canvas = _binding.GetCanvas();
	if (!canvas)
	{
		std::wstring error;
		try
		{
			if (applyChange(error))
			{
				ClearPropertyEditError();
				return DesignerDocumentTransactionResult::Success(
					DesignerDocumentTransactionState::Committed);
			}
		}
		catch (...)
		{
			error = L"属性操作抛出异常。";
		}
		if (error.empty()) error = L"属性操作被拒绝。";
		auto result = DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Aborted,
			std::move(error), false);
		ShowPropertyEditError(propertyName, result.Error);
		return result;
	}

	auto result = canvas->ExecuteDocumentEditTransaction(
		L"UpdateProperty:" + propertyName, applyChange);
	if (!result)
	{
		ShowPropertyEditError(propertyName, result.Error);
		return result;
	}
	ClearPropertyEditError();
	return result;
}

void PropertyGrid::CommitPendingEdits()
{
	CommitGroupedFloatSliderEdit();
	if (_nativeGrid)
		_nativeGrid->CommitEdit();
}

void PropertyGrid::LoadControl(std::shared_ptr<DesignerControl> control)
{
	LoadControls(control
		? std::vector<std::shared_ptr<DesignerControl>>{ control }
		: std::vector<std::shared_ptr<DesignerControl>>{}, control);
}

DesignerPropertyEditResult PropertyGrid::ApplyPropertyValue(
	const std::wstring& propertyName,
	const std::wstring& valueText)
{
	return UpdatePropertyFromTextBox(propertyName, valueText);
}

DesignerPropertyEditResult PropertyGrid::ActivateEventHandler(
	const std::wstring& eventName,
	std::wstring* outHandlerName)
{
	if (outHandlerName) outHandlerName->clear();
	auto* canvas = _binding.GetCanvas();
	if (!canvas)
		return DesignerPropertyEditResult::Failure(L"设计画布不可用。");

	std::optional<DesignerEventDescriptor> descriptor;
	std::wstring subjectName;
	std::wstring storedHandler;
	if (_binding.IsWindowBinding())
	{
		descriptor = DesignerEventCatalog::FindWindowEvent(eventName);
		subjectName = canvas->GetDesignedWindowName();
		const auto& handlers = canvas->GetDesignedWindowEventHandlers();
		if (const auto found = handlers.find(eventName);
			found != handlers.end()) storedHandler = found->second;
	}
	else if (const auto control = _binding.GetBoundControl())
	{
		descriptor = DesignerEventCatalog::FindControlEvent(
			control->Type, eventName, control->ComponentEvents);
		subjectName = control->Name;
		if (const auto found = control->EventHandlers.find(eventName);
			found != control->EventHandlers.end()) storedHandler = found->second;
	}
	if (!descriptor)
		return DesignerPropertyEditResult::Failure(
			L"目标不支持事件 " + eventName + L"。");

	auto handlerName = DesignerEventCatalog::NormalizeHandlerName(storedHandler);
	DesignerPropertyEditResult result =
		DesignerPropertyEditResult::Success();
	if (handlerName.empty())
	{
		handlerName = DesignerEventCatalog::MakeDefaultHandlerName(
			subjectName, descriptor->Name);
		result = UpdatePropertyFromTextBox(eventName, handlerName);
		if (!result) return result;
		_reloadRequested = true;
	}
	if (handlerName.empty())
		return DesignerPropertyEditResult::Failure(
			L"无法为事件生成默认处理函数名。");
	if (outHandlerName) *outHandlerName = handlerName;
	cui::framework::EventAccess::Raise(
		OnEventHandlerActivated, this, handlerName);
	return result;
}

DesignerPropertyEditResult PropertyGrid::ActivateDefaultEventHandler(
	std::wstring* outHandlerName)
{
	std::optional<DesignerEventDescriptor> descriptor;
	if (_binding.IsWindowBinding())
		descriptor = DesignerEventCatalog::GetDefaultWindowEvent();
	else if (const auto control = _binding.GetBoundControl())
		descriptor = DesignerEventCatalog::GetDefaultControlEvent(
			control->Type, control->ComponentEvents);
	if (!descriptor)
		return DesignerPropertyEditResult::Failure(
			L"当前目标没有声明默认事件。");
	return ActivateEventHandler(descriptor->Name, outHandlerName);
}

DesignerPropertyEditResult PropertyGrid::ResetPropertyValue(
	const std::wstring& propertyName)
{
	return ResetCurrentProperty(propertyName);
}

void PropertyGrid::LoadControls(
	const std::vector<std::shared_ptr<DesignerControl>>& controls,
	std::shared_ptr<DesignerControl> primaryControl)
{
	// A direct selection/view refresh satisfies any queued deferred reload.
	_reloadRequested = false;
	ClearPropertyEditError();
	BeginNativeRowsReload();
	_binding.BindControls(controls, primaryControl);
	if (_viewMode == DesignerPropertyGridViewMode::Properties)
		SubscribePropertyDiagnosticChanges();
	auto control = _binding.GetBoundControl();

	if (!control || !control->ControlInstance)
	{
		// 未选中控件时：展示被设计窗体属性
		auto* canvas = _binding.GetCanvas();
		if (canvas)
		{
			const bool events = _viewMode == DesignerPropertyGridViewMode::Events;
			_titleLabel->Text = events ? L"事件 - 窗体" : L"属性 - 窗体";
			if (!events)
			{
				_propertyRows = _binding.GetPropertyRows();
				const auto visibleRows = DesignerPropertyRowCatalog::FilterRows(
					_propertyRows, _propertyFilter);
				PopulateNativePropertyRows(visibleRows, L"窗体");

				if (MatchesCurrentFilter(
				L"DataContext Schema 数据上下文 架构"))
				{
					AddNativeActionRow(
					L"窗体 · 数据", L"DataContext Schema",
					L"编辑 (" + std::to_wstring(
						canvas->GetDataContextSchema().size()) + L")…",
					L"编辑设计期数据上下文架构。",
					[this, canvas]() {
					if (!this->GetPresentationWindow()) return;
					DataContextSchemaEditorDialog dialog(
						canvas->GetDataContextSchema(),
						canvas->GetDesignDataContext().get());
					dialog.Owner = this->GetPresentationWindow();
					(void)dialog.ShowDialog();
					if (!dialog.Applied
						|| dialog.ResultSchema == canvas->GetDataContextSchema()) return;

					auto result = std::move(dialog.ResultSchema);
					auto transaction = ExecutePropertyCommand(
						L"DataContextSchema",
						[canvas, result = std::move(result)](
							std::wstring& error) mutable {
							return canvas->SetDataContextSchema(
								std::move(result), &error);
						});
					if (!transaction)
					{
						::MessageBoxW(this->GetPresentationWindow()->Handle,
							transaction.Error.c_str(),
							L"DataContext Schema 无效", MB_OK | MB_ICONWARNING);
						return;
					}
					_reloadRequested = true;
				});
				}

				if (MatchesCurrentFilter(
					L"Data Resources DataType DataList DataRecord CollectionViewSource DataTemplate ItemsPanelTemplate GroupStyle 数据资源 数据类型 列表 视图 记录 模板 集合面板 分组样式"))
				{
					AddNativeActionRow(
						L"窗体 · 数据", L"数据资源",
						L"编辑 (" + std::to_wstring(canvas->GetDataTypes().size())
							+ L" 类型, " + std::to_wstring(canvas->GetDataLists().size())
							+ L" 列表, " + std::to_wstring(canvas->GetDataTemplates().size())
							+ L" 数据模板, " + std::to_wstring(canvas->GetCollectionViews().size())
							+ L" 集合视图, " + std::to_wstring(
								canvas->GetItemsPanelTemplates().size())
							+ L" 集合面板模板, "
							+ std::to_wstring(canvas->GetGroupStyles().size())
							+ L" 分组样式)…",
						L"创建和编辑本地 DataType、DataList、DataRecord 与 DataTemplate 标识；CollectionViewSource、GroupStyle 和模板视觉树由 XAML 维护。",
						[this, canvas]() {
						if (!this->GetPresentationWindow()) return;
						DesignerModel::DesignDocument current;
						std::wstring error;
						if (!canvas->BuildDesignDocument(current, &error))
						{
							::MessageBoxW(this->GetPresentationWindow()->Handle, error.c_str(),
								L"无法建立数据资源文档", MB_OK | MB_ICONWARNING);
							return;
						}
						DataResourcesEditorDialog dialog(current);
						dialog.Owner = this->GetPresentationWindow();
						(void)dialog.ShowDialog();
						if (!dialog.Applied || dialog.ResultDocument == current) return;
						auto result = std::move(dialog.ResultDocument);
						auto transaction = ExecutePropertyCommand(
							L"DataResources",
							[canvas, result = std::move(result)](
								std::wstring& applyError) mutable {
								return canvas->ApplyDesignDocument(result, &applyError);
							});
						if (!transaction)
						{
							::MessageBoxW(this->GetPresentationWindow()->Handle,
								transaction.Error.c_str(), L"数据资源无效",
								MB_OK | MB_ICONWARNING);
							return;
						}
						_reloadRequested = true;
					});
				}

				if (MatchesCurrentFilter(
				L"StyleSheet Style 样式表 文档样式"))
				{
					const auto& styleSheet = canvas->GetDocumentStyleSheet();
					AddNativeActionRow(
					L"窗体 · 外观", L"文档样式表",
					L"编辑 (" + std::to_wstring(styleSheet.Resources.size())
						+ L" 资源, " + std::to_wstring(styleSheet.Rules.size())
						+ L" 规则)…",
					L"编辑文档级样式资源和规则。",
					[this, canvas]() {
					if (!this->GetPresentationWindow()) return;
					StyleSheetEditorDialog dialog(
						canvas->GetDocumentStyleSheet(),
						canvas->GetDocumentResourceBasePath());
					dialog.Owner = this->GetPresentationWindow();
					(void)dialog.ShowDialog();
					if (!dialog.Applied
						|| dialog.ResultStyleSheet == canvas->GetDocumentStyleSheet()) return;

					auto result = std::move(dialog.ResultStyleSheet);
					auto resourceRenames = std::move(dialog.ResourceRenames);
					auto transaction = ExecutePropertyCommand(
						L"StyleSheet",
						[canvas, result = std::move(result),
							resourceRenames = std::move(resourceRenames)](
							std::wstring& error) mutable {
							return canvas->SetDocumentStyleSheet(
								std::move(result), &error, resourceRenames);
						});
					if (!transaction)
					{
						::MessageBoxW(this->GetPresentationWindow()->Handle,
							transaction.Error.c_str(),
							L"样式表无效", MB_OK | MB_ICONWARNING);
						return;
					}
					_reloadRequested = true;
				});
				}
			}
			else
			{
				PopulateNativeEventRows(
					DesignerEventCatalog::GetWindowEvents(),
					canvas->GetDesignedWindowName(),
					canvas->GetDesignedWindowEventHandlers(),
					L"窗体 · 事件");
			}
			Control::PropagatePresentationWindow(this, this->GetPresentationWindow());
			CommitNativeRowsReload();
			return;
		}
		_titleLabel->Text = _viewMode == DesignerPropertyGridViewMode::Events
			? L"事件" : L"属性";
		CommitNativeRowsReload();
		return;
	}

	const auto selectionCount = _binding.GetBoundControls().size();
	const bool events = _viewMode == DesignerPropertyGridViewMode::Events;
	const std::wstring viewCaption = events ? L"事件" : L"属性";
	_titleLabel->Text = selectionCount > 1
		? viewCaption + L" - " + std::to_wstring(selectionCount) + L" 个控件"
		: viewCaption + L" - " + control->Name;

	auto targetControl = control->ControlInstance;
	if (!events)
	{
		_propertyRows = _binding.GetPropertyRows();
		const auto visibleRows = DesignerPropertyRowCatalog::FilterRows(
			_propertyRows, _propertyFilter);
		PopulateNativePropertyRows(
			visibleRows, selectionCount > 1 ? L"公共属性" : L"属性");
	}

	if (selectionCount > 1)
	{
		if (events)
			PopulateNativeMultiSelectionEventRows(
				_binding.GetBoundControls(), control, L"公共事件");
		Control::PropagatePresentationWindow(this, this->GetPresentationWindow());
		CommitNativeRowsReload();
		return;
	}

	// 数据绑定使用运行时属性元数据驱动的结构化编辑器。
	if (!events
		&& !DependencyPropertyRegistry::GetProperties(*targetControl).empty()
		&& MatchesCurrentFilter(L"Binding DataBinding 数据绑定 绑定"))
	{
		AddNativeActionRow(
			L"属性 · 数据", L"数据绑定",
			L"编辑 (" + std::to_wstring(control->DataBindings.size()) + L")…",
			L"使用属性能力元数据配置 OneWay、TwoWay 等绑定。",
			[this]() {
			auto currentControl = _binding.GetBoundControl();
			if (!currentControl || !currentControl->ControlInstance || !this->GetPresentationWindow()) return;

			const auto* canvas = _binding.GetCanvas();
			std::vector<DesignerBindingElementSource> elementSources;
			if (canvas)
				for (const auto& candidate : canvas->GetAllControls())
					if (candidate && candidate->ControlInstance
						&& !candidate->Name.empty())
						elementSources.push_back({
							candidate->Name, candidate->ControlInstance });
			BindingEditorDialog dialog(
				currentControl->ControlInstance,
				currentControl->DataBindings,
				canvas ? canvas->GetEffectiveDataContextSchema(*currentControl)
					: DesignerDataContextSchema{},
				canvas ? canvas->GetEffectiveDesignDataContextSource(*currentControl)
					: nullptr,
				std::move(elementSources));
			dialog.Owner = this->GetPresentationWindow();
			(void)dialog.ShowDialog();
			if (!dialog.Applied || dialog.ResultBindings == currentControl->DataBindings) return;

			auto result = std::move(dialog.ResultBindings);
			auto transaction = ExecutePropertyCommand(
				L"DataBindings",
				[this, currentControl, result = std::move(result)](
					std::wstring& error) mutable {
					currentControl->DataBindings = std::move(result);
					if (auto* canvas = _binding.GetCanvas())
						return canvas->RefreshDesignBindings(
							*currentControl, &error);
					error = L"设计画布不可用。";
					return false;
				});
			if (!transaction) return;
			_reloadRequested = true;
		});
	}

	if (events)
	{
		PopulateNativeEventRows(
			DesignerEventCatalog::GetControlEvents(
				control->Type, control->ComponentEvents),
			control->Name, control->EventHandlers,
			L"属性 · 事件");
	}
	else
	{
		AddNativeCustomEditorRows(control->Type);
	}

	// 将新创建的编辑器子树接入当前 presentation source。
	Control::PropagatePresentationWindow(this, this->GetPresentationWindow());
	CommitNativeRowsReload();
}

void PropertyGrid::AddNativeCustomEditorRows(UIClass targetType)
{
	for (const auto& editor : DesignerCustomEditorCatalog::GetEditors(targetType))
	{
		if (!MatchesCurrentFilter(
			editor.ButtonText + L" Editor 编辑器 结构")) continue;
		AddNativeActionRow(
			L"属性 · 结构",
			editor.ButtonText,
			L"编辑…",
			L"使用设计器结构化编辑器修改集合内容。",
			[this, kind = editor.Kind]() { OpenCustomEditor(kind); });
	}
}

void PropertyGrid::OpenCustomEditor(DesignerCustomEditorKind kind)
{
	auto currentControl = _binding.GetBoundControl();
	if (!currentControl || !currentControl->ControlInstance || !this->GetPresentationWindow()) return;
	auto* target = currentControl->ControlInstance;
	auto* canvas = _binding.GetCanvas();
	if (!canvas)
	{
		ShowPropertyEditError(L"Structure", L"设计画布不可用，结构修改未执行。");
		return;
	}

	std::wstring transactionName;
	switch (kind)
	{
	case DesignerCustomEditorKind::GridDefinitions: transactionName = L"GridDefinitions"; break;
	}
	const auto transactionLabel = L"EditStructure:" + transactionName;
	const bool useStructureDelta =
		DesignerStructureEdit::SupportsDelta(kind);
	auto captureSelectionNames = [&]()
	{
		std::vector<std::wstring> names;
		names.reserve(canvas->GetSelectedControls().size());
		for (const auto& selected : canvas->GetSelectedControls())
			if (selected && !selected->Name.empty())
				names.push_back(selected->Name);
		return names;
	};
	DesignerStructureSnapshot deltaBefore;
	std::vector<std::wstring> beforeSelectionNames;
	std::wstring beforePrimarySelectionName;
	if (useStructureDelta)
	{
		std::wstring captureError;
		if (!DesignerStructureEdit::Capture(
			*currentControl, kind, deltaBefore, &captureError))
		{
			ShowPropertyEditError(transactionName, captureError);
			return;
		}
		beforeSelectionNames = captureSelectionNames();
		beforePrimarySelectionName = canvas->GetSelectedControl()
			? canvas->GetSelectedControl()->Name : std::wstring{};
	}
	else
	{
		auto begin = canvas->BeginDocumentEditTransaction(transactionLabel);
		if (!begin)
		{
			ShowPropertyEditError(transactionName, begin.Error);
			return;
		}
	}
	auto restoreDeltaState = [&](std::wstring& restoreError)
	{
		if (!useStructureDelta) return true;
		DesignerStructureSnapshot current;
		if (!DesignerStructureEdit::Capture(
			*currentControl, kind, current, &restoreError)) return false;
		const bool restored = current == deltaBefore
			|| DesignerStructureEdit::Restore(
				*currentControl, deltaBefore, &restoreError);
		if (restored)
			canvas->RestoreSelectionByNames(
				beforeSelectionNames, beforePrimarySelectionName, true);
		return restored;
	};

	bool handled = false;
	bool applied = false;
	try
	{
		switch (kind)
		{
		case DesignerCustomEditorKind::GridDefinitions:
			if (auto* gridPanel = dynamic_cast<Grid*>(target))
			{
				handled = true;
				GridDefinitionsEditorDialog dialog(gridPanel);
				dialog.Owner = this->GetPresentationWindow();
				(void)dialog.ShowDialog();
				applied = dialog.Applied;
			}
			break;
		}
	}
	catch (...)
	{
		std::wstring rollbackError;
		bool rolledBack = false;
		if (useStructureDelta)
			rolledBack = restoreDeltaState(rollbackError);
		else
		{
			auto rollback = canvas->RollbackDocumentEditTransaction();
			rolledBack = static_cast<bool>(rollback);
			rollbackError = std::move(rollback.Error);
		}
		ShowPropertyEditError(
			transactionName,
			L"结构编辑器发生异常，修改已回滚。"
			+ std::wstring(rolledBack
				? L"" : L" " + rollbackError));
		_reloadRequested = true;
		return;
	}
	if (!handled)
	{
		std::wstring cancelError;
		bool cancelled = false;
		if (useStructureDelta)
			cancelled = restoreDeltaState(cancelError);
		else
		{
			auto cancel = canvas->CancelDocumentEditTransaction();
			cancelled = static_cast<bool>(cancel);
			cancelError = std::move(cancel.Error);
		}
		ShowPropertyEditError(
			transactionName,
			L"结构编辑器与当前控件类型不兼容。"
			+ std::wstring(cancelled
				? L"" : L" " + cancelError));
		return;
	}
	if (!applied)
	{
		std::wstring cancelError;
		bool cancelled = false;
		if (useStructureDelta)
			cancelled = restoreDeltaState(cancelError);
		else
		{
			auto cancel = canvas->CancelDocumentEditTransaction();
			cancelled = static_cast<bool>(cancel);
			cancelError = std::move(cancel.Error);
		}
		if (!cancelled)
		{
			ShowPropertyEditError(transactionName, cancelError);
			_reloadRequested = true;
		}
		return;
	}
	DesignerDocumentTransactionResult commit;
	if (useStructureDelta)
	{
		DesignerStructureSnapshot deltaAfter;
		std::wstring captureError;
		if (!DesignerStructureEdit::Capture(
			*currentControl, kind, deltaAfter, &captureError))
		{
			std::wstring restoreError;
			(void)restoreDeltaState(restoreError);
			ShowPropertyEditError(transactionName,
				L"无法捕获结构编辑结果：" + captureError
				+ (restoreError.empty() ? std::wstring()
					: L" 恢复失败：" + restoreError));
			_reloadRequested = true;
			return;
		}
		if (deltaBefore == deltaAfter)
		{
			ClearPropertyEditError();
			_reloadRequested = true;
			return;
		}
		auto afterSelectionNames = captureSelectionNames();
		const auto afterPrimarySelectionName = canvas->GetSelectedControl()
			? canvas->GetSelectedControl()->Name : std::wstring{};
		commit = canvas->CommitAlreadyAppliedCommand(
			std::make_unique<ControlStructureCommand>(
				canvas,
				deltaBefore,
				std::move(deltaAfter),
				beforeSelectionNames,
				std::move(afterSelectionNames),
				beforePrimarySelectionName,
				afterPrimarySelectionName,
				transactionLabel,
				true));
		if (!commit)
		{
			std::wstring restoreError;
			(void)DesignerStructureEdit::Restore(
				*currentControl, deltaBefore, &restoreError);
			canvas->RestoreSelectionByNames(
				beforeSelectionNames, beforePrimarySelectionName, true);
			if (!restoreError.empty())
				commit.Error += L" 恢复失败：" + restoreError;
		}
	}
	else
	{
		commit = canvas->CommitDocumentEditTransaction();
	}
	if (!commit)
	{
		ShowPropertyEditError(
			transactionName,
			commit.Error);
		_reloadRequested = true;
		return;
	}

	ClearPropertyEditError();
	_reloadRequested = true;
}

void PropertyGrid::Clear()
{
	BeginNativeRowsReload();
	if (_nativeGrid)
	{
		_syncingNativeGrid = true;
		_nativeGrid->Clear();
		_syncingNativeGrid = false;
	}
}
