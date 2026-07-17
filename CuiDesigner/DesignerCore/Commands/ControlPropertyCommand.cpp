#include "ControlPropertyCommand.h"
#include "../../DesignerCanvas.h"
#include "../PropertyGridBinder.h"
#include <algorithm>

namespace
{
	constexpr auto MergeWindow = std::chrono::milliseconds(1000);

	size_t StringMemory(const std::wstring& value) noexcept
	{
		return sizeof(std::wstring)
			+ value.capacity() * sizeof(std::wstring::value_type);
	}

	size_t SelectionMemory(
		const std::vector<std::wstring>& names) noexcept
	{
		size_t result = names.capacity() * sizeof(std::wstring);
		for (const auto& name : names) result += StringMemory(name);
		return result;
	}
}

ControlPropertyCommand::ControlPropertyCommand(
	DesignerCanvas* canvas,
	DesignerPropertyBatchSnapshot before,
	DesignerPropertyBatchSnapshot after,
	std::vector<std::wstring> beforeSelectionNames,
	std::vector<std::wstring> afterSelectionNames,
	std::wstring beforePrimarySelectionName,
	std::wstring afterPrimarySelectionName,
	std::wstring label,
	bool skipInitialExecute,
	bool allowMerge)
	: _canvas(canvas),
	  _before(std::move(before)),
	  _after(std::move(after)),
	  _beforeSelectionNames(std::move(beforeSelectionNames)),
	  _afterSelectionNames(std::move(afterSelectionNames)),
	  _beforePrimarySelectionName(std::move(beforePrimarySelectionName)),
	  _afterPrimarySelectionName(std::move(afterPrimarySelectionName)),
	  _label(std::move(label)),
	  _skipInitialExecute(skipInitialExecute),
	  _allowMerge(allowMerge),
	  _committedAt(std::chrono::steady_clock::now())
{
	RefreshEstimatedMemoryUsage();
}

DesignerDocumentTransactionResult ControlPropertyCommand::Execute()
{
	if (_skipInitialExecute)
	{
		_skipInitialExecute = false;
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Committed);
	}
	return Apply(
		_before, _after,
		_afterSelectionNames, _afterPrimarySelectionName);
}

DesignerDocumentTransactionResult ControlPropertyCommand::Undo()
{
	return Apply(
		_after, _before,
		_beforeSelectionNames, _beforePrimarySelectionName);
}

std::wstring ControlPropertyCommand::GetLabel() const
{
	return _label;
}

DesignerDocumentTransactionResult ControlPropertyCommand::Apply(
	const DesignerPropertyBatchSnapshot& expected,
	const DesignerPropertyBatchSnapshot& desired,
	const std::vector<std::wstring>& selectionNames,
	const std::wstring& primarySelectionName) const
{
	if (!_canvas)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"设计画布不可用，无法应用属性差量。", false);
	if (expected.Source != desired.Source
		|| expected.PropertyName != desired.PropertyName
		|| expected.Targets.size() != desired.Targets.size()
		|| expected.Targets.empty())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"属性差量的前后状态不兼容。", false);

	std::vector<std::shared_ptr<DesignerControl>> controls;
	controls.reserve(expected.Targets.size());
	for (const auto& target : expected.Targets)
	{
		auto match = std::find_if(
			_canvas->GetAllControls().begin(),
			_canvas->GetAllControls().end(),
			[&target](const std::shared_ptr<DesignerControl>& candidate)
			{
				return candidate && candidate->ControlInstance
					&& candidate->Name == target.TargetName
					&& candidate->Type == target.TargetType;
			});
		if (match == _canvas->GetAllControls().end()
			|| std::find(controls.begin(), controls.end(), *match) != controls.end())
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"属性差量的目标控件不存在或不唯一。", false);
		controls.push_back(*match);
	}

	PropertyGridBinder binder;
	binder.SetCanvas(_canvas);
	DesignerPropertyRow row;
	row.Source = expected.Source;
	row.Name = expected.PropertyName;
	std::vector<DesignerPropertyEditTarget> editTargets;
	std::vector<DesignerPropertyValueSnapshot> rollback;
	editTargets.reserve(controls.size());
	rollback.resize(controls.size());
	for (size_t index = 0; index < controls.size(); ++index)
	{
		editTargets.push_back({
			controls[index].get(),
			binder.CreateControlPropertyContext(controls[index])
		});
		std::wstring error;
		if (!DesignerPropertyEdit::CaptureSnapshot(
			editTargets.back(), row, rollback[index], &error))
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"无法验证属性差量起点：" + error, false);
		if (!rollback[index].EquivalentTo(expected.Targets[index].Value))
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"属性差量起点与当前控件状态不一致。", false);
	}

	for (size_t index = 0; index < editTargets.size(); ++index)
	{
		std::wstring error;
		if (DesignerPropertyEdit::RestoreSnapshot(
			editTargets[index], row, desired.Targets[index].Value, &error))
			continue;
		bool restored = true;
		for (size_t rollbackIndex = index + 1; rollbackIndex > 0; --rollbackIndex)
			restored = DesignerPropertyEdit::RestoreSnapshot(
				editTargets[rollbackIndex - 1], row,
				rollback[rollbackIndex - 1]) && restored;
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法应用属性差量：" + error,
			restored);
	}

	for (const auto& control : controls)
		binder.NotifyControlChanged(control->ControlInstance);
	_canvas->RestoreSelectionByNames(
		selectionNames, primarySelectionName, true);
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Committed);
}

bool ControlPropertyCommand::TryMergeWith(
	IDesignerCommand& newerCommand) noexcept
{
	auto* newer = dynamic_cast<ControlPropertyCommand*>(&newerCommand);
	if (!newer || newer == this || _canvas != newer->_canvas
		|| !_allowMerge || !newer->_allowMerge
		|| _label != newer->_label
		|| _label.rfind(L"UpdateProperty:", 0) != 0
		|| _skipInitialExecute || newer->_skipInitialExecute
		|| !_after.EquivalentTo(newer->_before)
		|| _afterSelectionNames != newer->_beforeSelectionNames
		|| _afterPrimarySelectionName
			!= newer->_beforePrimarySelectionName)
		return false;
	const auto elapsed = newer->_committedAt - _committedAt;
	if (elapsed < std::chrono::steady_clock::duration::zero()
		|| elapsed > MergeWindow)
		return false;

	_after = std::move(newer->_after);
	_afterSelectionNames = std::move(newer->_afterSelectionNames);
	_afterPrimarySelectionName =
		std::move(newer->_afterPrimarySelectionName);
	_committedAt = newer->_committedAt;
	RefreshEstimatedMemoryUsage();
	return true;
}

size_t ControlPropertyCommand::GetEstimatedMemoryUsage() const noexcept
{
	return _estimatedMemoryUsage;
}

void ControlPropertyCommand::RefreshEstimatedMemoryUsage() noexcept
{
	_estimatedMemoryUsage = sizeof(*this)
		+ _before.GetEstimatedMemoryUsage()
		+ _after.GetEstimatedMemoryUsage()
		+ SelectionMemory(_beforeSelectionNames)
		+ SelectionMemory(_afterSelectionNames)
		+ StringMemory(_beforePrimarySelectionName)
		+ StringMemory(_afterPrimarySelectionName)
		+ StringMemory(_label);
}
