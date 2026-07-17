#include "ControlStructureCommand.h"

#include "../../DesignerCanvas.h"
#include "../PropertyGridBinder.h"

#include <algorithm>

namespace
{
	size_t StringMemory(const std::wstring& value) noexcept
	{
		return sizeof(value)
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

ControlStructureCommand::ControlStructureCommand(
	DesignerCanvas* canvas,
	DesignerStructureSnapshot before,
	DesignerStructureSnapshot after,
	std::vector<std::wstring> beforeSelectionNames,
	std::vector<std::wstring> afterSelectionNames,
	std::wstring beforePrimarySelectionName,
	std::wstring afterPrimarySelectionName,
	std::wstring label,
	bool skipInitialExecute)
	: _canvas(canvas),
	  _before(std::move(before)),
	  _after(std::move(after)),
	  _beforeSelectionNames(std::move(beforeSelectionNames)),
	  _afterSelectionNames(std::move(afterSelectionNames)),
	  _beforePrimarySelectionName(std::move(beforePrimarySelectionName)),
	  _afterPrimarySelectionName(std::move(afterPrimarySelectionName)),
	  _label(std::move(label)),
	  _skipInitialExecute(skipInitialExecute)
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

DesignerDocumentTransactionResult ControlStructureCommand::Execute()
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

DesignerDocumentTransactionResult ControlStructureCommand::Undo()
{
	return Apply(
		_after, _before,
		_beforeSelectionNames, _beforePrimarySelectionName);
}

std::wstring ControlStructureCommand::GetLabel() const
{
	return _label;
}

size_t ControlStructureCommand::GetEstimatedMemoryUsage() const noexcept
{
	return _estimatedMemoryUsage;
}

DesignerDocumentTransactionResult ControlStructureCommand::Apply(
	const DesignerStructureSnapshot& expected,
	const DesignerStructureSnapshot& desired,
	const std::vector<std::wstring>& selectionNames,
	const std::wstring& primarySelectionName) const
{
	if (!_canvas)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"设计画布不可用，无法应用结构差量。", false);
	if (expected.StableId != desired.StableId
		|| expected.TargetName != desired.TargetName
		|| expected.TargetType != desired.TargetType
		|| expected.Kind != desired.Kind)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"结构差量的前后状态不兼容。", false);

	auto match = std::find_if(
		_canvas->GetAllControls().begin(), _canvas->GetAllControls().end(),
		[&](const std::shared_ptr<DesignerControl>& candidate)
		{
			return candidate && candidate->ControlInstance
				&& candidate->StableId == expected.StableId
				&& candidate->Name == expected.TargetName
				&& candidate->Type == expected.TargetType;
		});
	if (match == _canvas->GetAllControls().end())
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"结构差量的目标控件不存在。", false);

	DesignerStructureSnapshot current;
	std::wstring error;
	if (!DesignerStructureEdit::Capture(
		**match, expected.Kind, current, &error))
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法验证结构差量起点：" + error, false);
	if (!(current == expected))
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"结构差量起点与当前控件状态不一致。", false);

	if (!DesignerStructureEdit::Restore(**match, desired, &error))
	{
		std::wstring restoreError;
		const bool restored = DesignerStructureEdit::Restore(
			**match, current, &restoreError);
		std::wstring message = L"无法应用结构差量：" + error;
		if (!restored) message += L" 恢复失败：" + restoreError;
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			std::move(message), restored);
	}

	PropertyGridBinder binder;
	binder.SetCanvas(_canvas);
	binder.NotifyControlChanged((*match)->ControlInstance);
	_canvas->RestoreSelectionByNames(
		selectionNames, primarySelectionName, true);
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Committed);
}
