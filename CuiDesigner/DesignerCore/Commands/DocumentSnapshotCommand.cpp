#include "DocumentSnapshotCommand.h"
#include "../../DesignerCanvas.h"

namespace
{
	constexpr auto MergeWindow = std::chrono::milliseconds(1000);

	size_t WideStringMemory(const std::wstring& value) noexcept
	{
		return sizeof(std::wstring)
			+ value.capacity() * sizeof(std::wstring::value_type);
	}

	size_t NarrowStringMemory(const std::string& value) noexcept
	{
		return sizeof(std::string)
			+ value.capacity() * sizeof(std::string::value_type);
	}

	size_t DesignValueMemory(
		const DesignerModel::DesignValue& value) noexcept
	{
		size_t result = sizeof(value);
		if (value.is_string())
		{
			result += value.size();
		}
		else if (value.is_array())
		{
			const auto& items = value.ArrayItems();
			result += items.capacity() * sizeof(DesignerModel::DesignValue);
			for (const auto& item : items) result += DesignValueMemory(item);
		}
		else if (value.is_object())
		{
			for (const auto& [key, item] : value.ObjectItems())
			{
				result += sizeof(std::pair<const std::string,
					DesignerModel::DesignValue>) + 3 * sizeof(void*);
				result += key.capacity();
				result += DesignValueMemory(item);
			}
		}
		return result;
	}

	size_t StyleValueMemory(const DesignerStyleValue& value) noexcept
	{
		return sizeof(value) + WideStringMemory(value.Text);
	}

	size_t DocumentMemory(
		const DesignerModel::DesignDocument& document) noexcept
	{
		size_t result = sizeof(document)
			+ NarrowStringMemory(document.Schema)
			+ WideStringMemory(document.Form.Name)
			+ WideStringMemory(document.Form.Text)
			+ WideStringMemory(document.Form.FontName);
		for (const auto& [eventName, handler] : document.Form.EventHandlers)
			result += sizeof(eventName) + 3 * sizeof(void*)
				+ WideStringMemory(eventName) + WideStringMemory(handler);

		result += document.DataContextSchema.capacity()
			* sizeof(DesignerDataContextProperty);
		for (const auto& property : document.DataContextSchema)
			result += WideStringMemory(property.Path);

		result += document.StyleSheet.Resources.capacity()
			* sizeof(DesignerStyleResource);
		for (const auto& resource : document.StyleSheet.Resources)
			result += WideStringMemory(resource.Key)
				+ StyleValueMemory(resource.Value);
		result += document.StyleSheet.Rules.capacity()
			* sizeof(DesignerStyleRule);
		for (const auto& rule : document.StyleSheet.Rules)
		{
			result += WideStringMemory(rule.Id)
				+ rule.Classes.capacity() * sizeof(std::wstring)
				+ rule.Setters.capacity() * sizeof(DesignerStyleSetter);
			for (const auto& className : rule.Classes)
				result += WideStringMemory(className);
			for (const auto& setter : rule.Setters)
				result += WideStringMemory(setter.PropertyName)
					+ WideStringMemory(setter.ResourceKey)
					+ StyleValueMemory(setter.Literal);
		}

		result += document.Nodes.capacity()
			* sizeof(DesignerModel::DesignNode);
		for (const auto& node : document.Nodes)
		{
			result += WideStringMemory(node.ParentRef)
				+ WideStringMemory(node.Name)
				+ DesignValueMemory(node.Props)
				+ DesignValueMemory(node.Extra)
				+ DesignValueMemory(node.Events)
				+ DesignValueMemory(node.Bindings);
		}
		return result;
	}

	size_t SelectionMemory(
		const std::vector<std::wstring>& names) noexcept
	{
		size_t result = names.capacity() * sizeof(std::wstring);
		for (const auto& name : names) result += WideStringMemory(name);
		return result;
	}

	bool IsMergeableLabel(const std::wstring& label) noexcept
	{
		return label == L"NudgeSelection"
			|| label.rfind(L"UpdateProperty:", 0) == 0;
	}
}

DocumentSnapshotCommand::DocumentSnapshotCommand(
	DesignerCanvas* canvas,
	DesignerModel::DesignDocument beforeDocument,
	DesignerModel::DesignDocument afterDocument,
	std::vector<std::wstring> beforeSelectionNames,
	std::vector<std::wstring> afterSelectionNames,
	std::wstring beforeSelectionName,
	std::wstring afterSelectionName,
	std::wstring label,
	bool skipInitialExecute)
	: _canvas(canvas),
	  _beforeDocument(std::move(beforeDocument)),
	  _afterDocument(std::move(afterDocument)),
	  _beforeSelectionNames(std::move(beforeSelectionNames)),
	  _afterSelectionNames(std::move(afterSelectionNames)),
	  _beforeSelectionName(std::move(beforeSelectionName)),
	  _afterSelectionName(std::move(afterSelectionName)),
	  _label(std::move(label)),
	  _skipInitialExecute(skipInitialExecute),
	  _committedAt(std::chrono::steady_clock::now())
{
	_estimatedMemoryUsage = sizeof(*this)
		+ DocumentMemory(_beforeDocument)
		+ DocumentMemory(_afterDocument)
		+ SelectionMemory(_beforeSelectionNames)
		+ SelectionMemory(_afterSelectionNames)
		+ WideStringMemory(_beforeSelectionName)
		+ WideStringMemory(_afterSelectionName)
		+ WideStringMemory(_label);
}

DesignerDocumentTransactionResult DocumentSnapshotCommand::Execute()
{
	if (_skipInitialExecute)
	{
		_skipInitialExecute = false;
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Committed);
	}

	return Apply(_afterDocument, _afterSelectionNames, _afterSelectionName);
}

DesignerDocumentTransactionResult DocumentSnapshotCommand::Undo()
{
	return Apply(
		_beforeDocument, _beforeSelectionNames, _beforeSelectionName);
}

std::wstring DocumentSnapshotCommand::GetLabel() const
{
	return _label;
}

DesignerDocumentTransactionResult DocumentSnapshotCommand::Apply(
	const DesignerModel::DesignDocument& document,
	const std::vector<std::wstring>& selectionNames,
	const std::wstring& primarySelectionName) const
{
	if (!_canvas)
	{
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"设计画布不可用，无法应用文档快照。", false);
	}

	DesignerModel::DesignDocument rollbackDocument;
	std::wstring error;
	if (!_canvas->BuildDesignDocument(rollbackDocument, &error))
	{
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"无法建立当前文档的恢复快照：" + error);
	}
	std::vector<std::wstring> rollbackSelectionNames;
	rollbackSelectionNames.reserve(_canvas->GetSelectedControls().size());
	for (const auto& control : _canvas->GetSelectedControls())
		if (control && !control->Name.empty())
			rollbackSelectionNames.push_back(control->Name);
	const auto rollbackPrimary = _canvas->GetSelectedControl()
		? _canvas->GetSelectedControl()->Name : std::wstring{};

	if (!_canvas->ApplyDesignDocument(document, &error))
	{
		std::wstring restoreError;
		const bool restored = _canvas->ApplyDesignDocument(
			rollbackDocument, &restoreError);
		if (restored)
			_canvas->RestoreSelectionByNames(
				rollbackSelectionNames, rollbackPrimary, true);
		std::wstring message = L"无法应用文档快照：" + error;
		if (!restored)
			message += L" 文档恢复失败：" + restoreError;
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			std::move(message), restored);
	}
	_canvas->RestoreSelectionByNames(selectionNames, primarySelectionName, true);
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Committed);
}

bool DocumentSnapshotCommand::TryMergeWith(
	IDesignerCommand& newerCommand) noexcept
{
	auto* newer = dynamic_cast<DocumentSnapshotCommand*>(&newerCommand);
	if (!newer || newer == this || _canvas != newer->_canvas
		|| _label != newer->_label || !IsMergeableLabel(_label)
		|| _skipInitialExecute || newer->_skipInitialExecute
		|| _afterSelectionNames != newer->_beforeSelectionNames
		|| _afterSelectionName != newer->_beforeSelectionName
		|| !(_afterDocument == newer->_beforeDocument))
		return false;
	const auto elapsed = newer->_committedAt - _committedAt;
	if (elapsed < std::chrono::steady_clock::duration::zero()
		|| elapsed > MergeWindow)
		return false;

	_afterDocument = std::move(newer->_afterDocument);
	_afterSelectionNames = std::move(newer->_afterSelectionNames);
	_afterSelectionName = std::move(newer->_afterSelectionName);
	_committedAt = newer->_committedAt;
	_estimatedMemoryUsage = sizeof(*this)
		+ DocumentMemory(_beforeDocument)
		+ DocumentMemory(_afterDocument)
		+ SelectionMemory(_beforeSelectionNames)
		+ SelectionMemory(_afterSelectionNames)
		+ WideStringMemory(_beforeSelectionName)
		+ WideStringMemory(_afterSelectionName)
		+ WideStringMemory(_label);
	return true;
}

size_t DocumentSnapshotCommand::GetEstimatedMemoryUsage() const noexcept
{
	return _estimatedMemoryUsage;
}
