#include "PropertyGridBinder.h"
#include "../DesignerCanvas.h"
#include "../../CUI/include/SplitContainer.h"
#include <algorithm>

void PropertyGridBinder::SetCanvas(DesignerCanvas* canvas)
{
	_canvas = canvas;
}

DesignerCanvas* PropertyGridBinder::GetCanvas() const
{
	return _canvas;
}

void PropertyGridBinder::BindControl(const std::shared_ptr<DesignerControl>& control)
{
	BindControls(control
		? std::vector<std::shared_ptr<DesignerControl>>{ control }
		: std::vector<std::shared_ptr<DesignerControl>>{}, control);
}

void PropertyGridBinder::BindControls(
	const std::vector<std::shared_ptr<DesignerControl>>& controls,
	const std::shared_ptr<DesignerControl>& primaryControl)
{
	_controls.clear();
	_controls.reserve(controls.size());
	for (const auto& control : controls)
	{
		if (!control || !control->ControlInstance) continue;
		if (std::find(_controls.begin(), _controls.end(), control)
			== _controls.end())
			_controls.push_back(control);
	}
	_control = nullptr;
	if (primaryControl
		&& std::find(_controls.begin(), _controls.end(), primaryControl)
			!= _controls.end())
		_control = primaryControl;
	else if (!_controls.empty())
		_control = _controls.front();
}

bool PropertyGridBinder::IsFormBinding() const
{
	return !_control;
}

std::shared_ptr<DesignerControl> PropertyGridBinder::GetBoundControl() const
{
	return _control;
}

const std::vector<std::shared_ptr<DesignerControl>>&
PropertyGridBinder::GetBoundControls() const
{
	return _controls;
}

Control* PropertyGridBinder::GetBoundRuntimeControl() const
{
	return _control ? _control->ControlInstance : nullptr;
}

DesignerModel::DesignFormModel PropertyGridBinder::CaptureFormModel() const
{
	return _canvas
		? _canvas->CaptureDesignedFormModel()
		: DesignerModel::DesignFormModel{};
}

bool PropertyGridBinder::ApplyFormProperty(
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	DesignerStyleValue* outEffective,
	std::wstring* outError) const
{
	if (!_canvas) return false;
	auto form = _canvas->CaptureDesignedFormModel();
	if (!DesignerFormPropertyCatalog::ApplyValue(
		form, propertyName, value, outEffective, outError)) return false;
	_canvas->ApplyDesignedFormModel(form);
	return true;
}

bool PropertyGridBinder::ResetFormProperty(
	const std::wstring& propertyName,
	DesignerStyleValue* outEffective,
	std::wstring* outError) const
{
	if (!_canvas) return false;
	auto form = _canvas->CaptureDesignedFormModel();
	if (!DesignerFormPropertyCatalog::ResetValue(
		form, propertyName, outEffective, outError)) return false;
	_canvas->ApplyDesignedFormModel(form);
	return true;
}

::Font* PropertyGridBinder::GetDesignedFormSharedFont() const
{
	return _canvas ? _canvas->GetDesignedFormSharedFont() : nullptr;
}

DesignerControlPropertyContext PropertyGridBinder::CreateControlPropertyContext(
	const std::shared_ptr<DesignerControl>& target) const
{
	DesignerControlPropertyContext context;
	context.SharedFont = GetDesignedFormSharedFont();
	context.MakeUniqueName = [this, target](DesignerControl&, const std::wstring& desired)
	{
		return MakeUniqueControlName(target, desired);
	};
	context.SyncDefaultNameCounter = [this](UIClass type, const std::wstring& name)
	{
		SyncDefaultNameCounter(type, name);
	};
	context.ApplyAnchorStylesKeepingBounds = [this](Control* control, uint8_t anchorStyles)
	{
		ApplyAnchorStylesKeepingBounds(control, anchorStyles);
	};
	return context;
}

std::vector<DesignerPropertyRow> PropertyGridBinder::GetPropertyRows() const
{
	if (_control)
	{
		std::vector<std::vector<DesignerPropertyRow>> controlRows;
		controlRows.reserve(_controls.size());
		auto primaryContext = CreateControlPropertyContext(_control);
		controlRows.push_back(DesignerPropertyRowCatalog::GetControlRows(
			*_control, primaryContext));
		for (const auto& control : _controls)
		{
			if (control == _control) continue;
			auto context = CreateControlPropertyContext(control);
			controlRows.push_back(DesignerPropertyRowCatalog::GetControlRows(
				*control, context));
		}
		return DesignerPropertyRowCatalog::GetCommonControlRows(controlRows);
	}
	return _canvas
		? DesignerPropertyRowCatalog::GetFormRows(CaptureFormModel())
		: std::vector<DesignerPropertyRow>{};
}

std::vector<DesignerPropertyEditTarget>
PropertyGridBinder::CreatePropertyEditTargets() const
{
	std::vector<DesignerPropertyEditTarget> targets;
	targets.reserve(_controls.size());
	for (const auto& control : _controls)
	{
		if (!control || !control->ControlInstance) continue;
		targets.push_back({
			control.get(), CreateControlPropertyContext(control)
		});
	}
	return targets;
}

DesignerPropertyEditResult PropertyGridBinder::ApplyControlPropertyValue(
	const std::wstring& propertyName,
	const std::wstring& valueText) const
{
	const auto rows = GetPropertyRows();
	const auto* row = DesignerPropertyRowCatalog::Find(rows, propertyName);
	if (!row)
		return DesignerPropertyEditResult::Failure(
			L"当前选择没有公共属性 " + propertyName + L"。");
	auto result = DesignerPropertyEdit::Apply(
		CreatePropertyEditTargets(), *row, valueText);
	if (result)
		for (const auto& control : _controls)
			if (control && control->ControlInstance)
				NotifyControlChanged(control->ControlInstance);
	return result;
}

DesignerPropertyEditResult PropertyGridBinder::ResetControlPropertyValue(
	const std::wstring& propertyName) const
{
	const auto rows = GetPropertyRows();
	const auto* row = DesignerPropertyRowCatalog::Find(rows, propertyName);
	if (!row)
		return DesignerPropertyEditResult::Failure(
			L"当前选择没有公共属性 " + propertyName + L"。");
	auto result = DesignerPropertyEdit::Reset(
		CreatePropertyEditTargets(), *row);
	if (result)
		for (const auto& control : _controls)
			if (control && control->ControlInstance)
				NotifyControlChanged(control->ControlInstance);
	return result;
}

bool PropertyGridBinder::CaptureControlPropertySnapshot(
	const std::wstring& propertyName,
	DesignerPropertyBatchSnapshot& out,
	std::wstring* outError) const
{
	out = DesignerPropertyBatchSnapshot{};
	const auto rows = GetPropertyRows();
	const auto* row = DesignerPropertyRowCatalog::Find(rows, propertyName);
	if (!row || row->Source == DesignerPropertyRowSource::Form)
	{
		if (outError)
			*outError = L"当前选择没有可快照的控件属性 "
				+ propertyName + L"。";
		return false;
	}
	const auto targets = CreatePropertyEditTargets();
	if (targets.empty())
	{
		if (outError) *outError = L"没有可快照的目标控件。";
		return false;
	}
	out.Source = row->Source;
	out.PropertyName = row->Name;
	out.Targets.reserve(targets.size());
	for (const auto& target : targets)
	{
		DesignerPropertyTargetSnapshot item;
		item.TargetName = target.Control->Name;
		item.TargetType = target.Control->Type;
		std::wstring error;
		if (!DesignerPropertyEdit::CaptureSnapshot(
			target, *row, item.Value, &error))
		{
			out = DesignerPropertyBatchSnapshot{};
			if (outError)
				*outError = L"控件 " + target.Control->Name + L"：" + error;
			return false;
		}
		out.Targets.push_back(std::move(item));
	}
	if (outError) outError->clear();
	return true;
}

bool PropertyGridBinder::RestoreBoundControlPropertySnapshot(
	const DesignerPropertyBatchSnapshot& snapshot,
	std::wstring* outError) const
{
	const auto targets = CreatePropertyEditTargets();
	if (targets.size() != snapshot.Targets.size() || targets.empty())
	{
		if (outError) *outError = L"属性快照的目标集合已经变化。";
		return false;
	}
	DesignerPropertyRow row;
	row.Source = snapshot.Source;
	row.Name = snapshot.PropertyName;
	std::vector<DesignerPropertyValueSnapshot> rollback(targets.size());
	for (size_t index = 0; index < targets.size(); ++index)
	{
		if (!targets[index].Control
			|| targets[index].Control->Type != snapshot.Targets[index].TargetType)
		{
			if (outError) *outError = L"属性快照的目标类型已经变化。";
			return false;
		}
		std::wstring error;
		if (!DesignerPropertyEdit::CaptureSnapshot(
			targets[index], row, rollback[index], &error))
		{
			if (outError) *outError = std::move(error);
			return false;
		}
	}

	for (size_t index = 0; index < targets.size(); ++index)
	{
		std::wstring error;
		if (DesignerPropertyEdit::RestoreSnapshot(
			targets[index], row, snapshot.Targets[index].Value, &error))
			continue;
		bool rolledBack = true;
		for (size_t rollbackIndex = index + 1; rollbackIndex > 0; --rollbackIndex)
			rolledBack = DesignerPropertyEdit::RestoreSnapshot(
				targets[rollbackIndex - 1], row,
				rollback[rollbackIndex - 1]) && rolledBack;
		if (outError)
			*outError = error + (rolledBack
				? L"" : L" 属性快照回滚未能完整恢复所有目标。");
		return false;
	}
	for (const auto& target : targets)
		NotifyControlChanged(target.Control->ControlInstance);
	if (outError) outError->clear();
	return true;
}

std::vector<DesignerControlPropertyDescriptor>
PropertyGridBinder::GetControlDesignProperties() const
{
	return _control
		? DesignerControlPropertyCatalog::GetProperties(*_control)
		: std::vector<DesignerControlPropertyDescriptor>{};
}

bool PropertyGridBinder::CaptureControlDesignProperty(
	const std::wstring& propertyName,
	DesignerStyleValue& out,
	std::wstring* outError) const
{
	if (!_control) return false;
	const auto context = CreateControlPropertyContext(_control);
	return DesignerControlPropertyCatalog::CaptureValue(
		*_control, context, propertyName, out, outError);
}

bool PropertyGridBinder::ApplyControlDesignProperty(
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	DesignerStyleValue* outEffective,
	std::wstring* outError) const
{
	return ApplyControlDesignProperty(
		_control, propertyName, value, outEffective, outError);
}

bool PropertyGridBinder::ApplyControlDesignProperty(
	const std::shared_ptr<DesignerControl>& target,
	const std::wstring& propertyName,
	const DesignerStyleValue& value,
	DesignerStyleValue* outEffective,
	std::wstring* outError) const
{
	if (!target) return false;
	auto context = CreateControlPropertyContext(target);
	return DesignerControlPropertyCatalog::ApplyValue(
		*target, context, propertyName, value, outEffective, outError);
}

bool PropertyGridBinder::ResetControlDesignProperty(
	const std::wstring& propertyName,
	DesignerStyleValue* outEffective,
	std::wstring* outError) const
{
	return ResetControlDesignProperty(
		_control, propertyName, outEffective, outError);
}

bool PropertyGridBinder::ResetControlDesignProperty(
	const std::shared_ptr<DesignerControl>& target,
	const std::wstring& propertyName,
	DesignerStyleValue* outEffective,
	std::wstring* outError) const
{
	if (!target) return false;
	auto context = CreateControlPropertyContext(target);
	return DesignerControlPropertyCatalog::ResetValue(
		*target, context, propertyName, outEffective, outError);
}

void PropertyGridBinder::NotifyControlChanged(Control* control) const
{
	if (!control)
	{
		return;
	}

	if (auto* panel = dynamic_cast<Panel*>(control->Parent))
	{
		panel->InvalidateLayout();
		panel->PerformLayout();
	}
	if (auto* split = dynamic_cast<SplitContainer*>(control))
	{
		split->RefreshSplitterLayout();
	}
	else if (auto* panel = dynamic_cast<Panel*>(control))
	{
		panel->InvalidateLayout();
		panel->PerformLayout();
	}
	if (_canvas)
	{
		_canvas->ClampControlToDesignSurface(control);
	}
	control->InvalidateVisual();
}

void PropertyGridBinder::ApplyAnchorStylesKeepingBounds(Control* control, uint8_t anchorStyles) const
{
	if (!control)
	{
		return;
	}
	if (_canvas)
	{
		_canvas->ApplyAnchorStylesKeepingBounds(control, anchorStyles);
	}
	else
	{
		control->AnchorStyles = anchorStyles;
	}
}

std::wstring PropertyGridBinder::MakeUniqueControlName(const std::shared_ptr<DesignerControl>& target, const std::wstring& desired) const
{
	return _canvas ? _canvas->MakeUniqueControlName(target, desired) : desired;
}

void PropertyGridBinder::SyncDefaultNameCounter(UIClass type, const std::wstring& name) const
{
	if (_canvas)
	{
		_canvas->SyncDefaultNameCounter(type, name);
	}
}

void PropertyGridBinder::RemoveDesignerControlsInSubtree(Control* root) const
{
	if (_canvas)
	{
		_canvas->RemoveDesignerControlsInSubtree(root);
	}
}
