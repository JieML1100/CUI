#include "EventHandlerCommand.h"

#include "../../DesignerCanvas.h"
#include "../../DesignerEventCatalog.h"
#include "../../DesignerModel/AtomicFile.h"
#include "../../DesignerModel/CppUserCodeIndex.h"
#include "../../DesignerModel/DesignCodeGenerationService.h"
#include "../../DesignerModel/DesignDocumentEventIndex.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <string_view>
#include <utility>

namespace
{
	size_t StringMemory(const std::wstring& value) noexcept
	{
		return sizeof(std::wstring)
			+ value.capacity() * sizeof(std::wstring::value_type);
	}

	struct ResolvedEventTarget
	{
		std::map<std::wstring, std::wstring>* Handlers = nullptr;
		const DesignerEventHandlerDelta* Delta = nullptr;
	};

	struct HandlerMapChange
	{
		std::map<std::wstring, std::wstring>* Target = nullptr;
		std::map<std::wstring, std::wstring> Desired;
	};

	DesignerEventHandlerValueSnapshot ReadValue(
		const std::map<std::wstring, std::wstring>& handlers,
		const std::wstring& eventName)
	{
		DesignerEventHandlerValueSnapshot value;
		const auto found = handlers.find(eventName);
		if (found != handlers.end())
		{
			value.Exists = true;
			value.StoredHandler = found->second;
		}
		return value;
	}

	void WriteValue(
		std::map<std::wstring, std::wstring>& handlers,
		const std::wstring& eventName,
		const DesignerEventHandlerValueSnapshot& value)
	{
		if (!value.Exists)
			handlers.erase(eventName);
		else
			handlers[eventName] = value.StoredHandler;
	}

	std::string NarrowAscii(std::wstring_view value)
	{
		std::string result;
		result.reserve(value.size());
		for (const auto character : value)
			result.push_back(static_cast<char>(character));
		return result;
	}

	bool ReadSourceFile(
		const std::wstring& path,
		std::string& source,
		std::wstring& error)
	{
		source.clear();
		std::ifstream stream(std::filesystem::path(path), std::ios::binary);
		if (!stream)
		{
			error = L"无法读取待迁移的用户代码文件：" + path;
			return false;
		}
		source.assign(std::istreambuf_iterator<char>(stream),
			std::istreambuf_iterator<char>());
		if (!stream.eof() && stream.fail())
		{
			error = L"读取待迁移的用户代码文件时发生错误：" + path;
			return false;
		}
		return true;
	}

	std::vector<std::wstring> MigrationOutputFiles(
		const DesignerEventHandlerCodeMigration& migration)
	{
		return {
			migration.OutputBasePath + L".h",
			migration.OutputBasePath + L".cpp",
			migration.OutputBasePath + L".g.h",
			migration.OutputBasePath + L".g.cpp",
			migration.OutputBasePath + L".handlers.g.inc",
		};
	}
}

size_t DesignerEventHandlerCodeMigration::GetEstimatedMemoryUsage() const noexcept
{
	return sizeof(*this)
		+ StringMemory(OutputBasePath)
		+ StringMemory(ClassName)
		+ StringMemory(UserCodePath)
		+ sizeof(std::string) + ParameterList.capacity()
		+ StringMemory(OldName)
		+ StringMemory(NewName);
}

size_t DesignerEventHandlerValueSnapshot::GetEstimatedMemoryUsage() const noexcept
{
	return sizeof(*this) + StringMemory(StoredHandler);
}

size_t DesignerEventHandlerDelta::GetEstimatedMemoryUsage() const noexcept
{
	return sizeof(*this)
		+ StringMemory(SubjectName)
		+ StringMemory(EventName)
		+ Before.GetEstimatedMemoryUsage()
		+ After.GetEstimatedMemoryUsage();
}

EventHandlerCommand::EventHandlerCommand(
	DesignerCanvas* canvas,
	std::vector<DesignerEventHandlerDelta> deltas,
	std::vector<std::wstring> selectionNames,
	std::wstring primarySelectionName,
	std::wstring label,
	DesignerEventHandlerCodeMigration codeMigration)
	: _canvas(canvas),
	  _deltas(std::move(deltas)),
	  _selectionNames(std::move(selectionNames)),
	  _primarySelectionName(std::move(primarySelectionName)),
	  _label(std::move(label)),
	  _codeMigration(std::move(codeMigration))
{
	_estimatedMemoryUsage = sizeof(*this)
		+ _deltas.capacity() * sizeof(DesignerEventHandlerDelta)
		+ _selectionNames.capacity() * sizeof(std::wstring)
		+ StringMemory(_primarySelectionName)
		+ StringMemory(_label)
		+ _codeMigration.GetEstimatedMemoryUsage();
	for (const auto& delta : _deltas)
		_estimatedMemoryUsage += delta.GetEstimatedMemoryUsage();
	for (const auto& name : _selectionNames)
		_estimatedMemoryUsage += StringMemory(name);
}

DesignerDocumentTransactionResult EventHandlerCommand::Execute()
{
	return Apply(false);
}

DesignerDocumentTransactionResult EventHandlerCommand::Undo()
{
	return Apply(true);
}

std::wstring EventHandlerCommand::GetLabel() const
{
	return _label;
}

size_t EventHandlerCommand::GetEstimatedMemoryUsage() const noexcept
{
	return _estimatedMemoryUsage;
}

DesignerDocumentTransactionResult EventHandlerCommand::Apply(bool undo) const
{
	if (!_canvas)
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"设计画布不可用，无法应用事件差量。", false);
	if (_deltas.empty())
		return DesignerDocumentTransactionResult::Success(
			DesignerDocumentTransactionState::Unchanged);

	std::vector<ResolvedEventTarget> targets;
	targets.reserve(_deltas.size());
	for (const auto& delta : _deltas)
	{
		if (delta.EventName.empty()
			|| (delta.Before.Exists && delta.Before.StoredHandler.empty())
			|| (delta.After.Exists && delta.After.StoredHandler.empty()))
		{
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"事件差量包含空事件名或空处理函数。", false);
		}

		std::map<std::wstring, std::wstring>* handlers = nullptr;
		if (delta.IsForm)
		{
			if (delta.StableId != 0
				|| _canvas->GetDesignedFormName() != delta.SubjectName
				|| !DesignerEventCatalog::FindFormEvent(delta.EventName))
			{
				return DesignerDocumentTransactionResult::Failure(
					DesignerDocumentTransactionState::Failed,
					L"事件差量的窗体目标或事件契约已变化。", false);
			}
			handlers = &_canvas->_designedFormEventHandlers;
		}
		else
		{
			const auto found = std::find_if(
				_canvas->GetAllControls().begin(),
				_canvas->GetAllControls().end(),
				[&](const std::shared_ptr<DesignerControl>& control)
				{
					return control && control->StableId == delta.StableId;
				});
			if (found == _canvas->GetAllControls().end()
				|| !*found
				|| (*found)->Name != delta.SubjectName
				|| (*found)->Type != delta.ControlType
				|| !DesignerEventCatalog::FindControlEvent(
					(*found)->Type, delta.EventName, (*found)->CustomEvents))
			{
				return DesignerDocumentTransactionResult::Failure(
					DesignerDocumentTransactionState::Failed,
					L"事件差量目标不存在或事件契约已变化："
						+ delta.SubjectName + L"." + delta.EventName,
					false);
			}
			handlers = &(*found)->EventHandlers;
		}

		const auto duplicate = std::find_if(
			targets.begin(), targets.end(),
			[&](const ResolvedEventTarget& target)
			{
				return target.Handlers == handlers && target.Delta
					&& target.Delta->EventName == delta.EventName;
			});
		if (duplicate != targets.end())
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"事件差量包含重复目标。", false);

		const auto& expected = undo ? delta.After : delta.Before;
		if (!ReadValue(*handlers, delta.EventName).EquivalentTo(expected))
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"事件差量起点与当前状态不一致："
					+ delta.SubjectName + L"." + delta.EventName,
				false);
		targets.push_back({ handlers, &delta });
	}

	std::vector<HandlerMapChange> changes;
	try
	{
		changes.reserve(targets.size());
		for (const auto& target : targets)
		{
			auto found = std::find_if(
				changes.begin(), changes.end(),
				[&](const HandlerMapChange& change)
				{
					return change.Target == target.Handlers;
				});
			if (found == changes.end())
			{
				changes.push_back({ target.Handlers, *target.Handlers });
				found = std::prev(changes.end());
			}
			const auto& desired = undo
				? target.Delta->Before : target.Delta->After;
			WriteValue(found->Desired, target.Delta->EventName, desired);
		}
	}
	catch (...)
	{
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			L"准备事件差量时发生异常，文档未修改。", true);
	}

	const bool migrateCode = _codeMigration.Enabled();
	DesignerModel::AtomicFileBatchSnapshot codeSnapshot;
	DesignerModel::AtomicFileBatchSnapshot codeExpectedCurrent;
	std::wstring userCodePath;
	std::string originalSource;
	std::string migratedSource;
	if (migrateCode)
	{
		const auto& sourceName = undo
			? _codeMigration.NewName : _codeMigration.OldName;
		const auto& targetName = undo
			? _codeMigration.OldName : _codeMigration.NewName;
		userCodePath = _codeMigration.UserCodePath;
		std::wstring migrationError;
		if (!ReadSourceFile(userCodePath, originalSource, migrationError))
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				std::move(migrationError), true);

		DesignerModel::CppUserCodeIndex sourceIndex;
		if (!DesignerModel::CppUserCodeIndex::Build(
			originalSource, NarrowAscii(_codeMigration.ClassName),
			sourceIndex, &migrationError)
			|| !sourceIndex.TryRenameUniqueCompatibleHandler(
				originalSource, NarrowAscii(sourceName), NarrowAscii(targetName),
				_codeMigration.ParameterList, migratedSource, &migrationError))
		{
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				migrationError.empty()
					? L"无法安全迁移用户事件函数体。"
					: std::move(migrationError),
				true);
		}
		if (!DesignerModel::AtomicFileBatchSnapshot::Capture(
			MigrationOutputFiles(_codeMigration),
			codeSnapshot, &migrationError))
		{
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				migrationError.empty()
					? L"无法在重命名前捕获代码文件快照。"
					: std::move(migrationError),
				true);
		}
		const auto migratedFile = std::find_if(
			codeSnapshot.Entries().begin(), codeSnapshot.Entries().end(),
			[&](const auto& entry)
			{
				return _wcsicmp(
					entry.FilePath.c_str(), userCodePath.c_str()) == 0;
			});
		if (codeSnapshot.Entries().size() != 5
			|| migratedFile == codeSnapshot.Entries().end()
			|| !migratedFile->Existed
			|| migratedFile->Content != originalSource)
		{
			return DesignerDocumentTransactionResult::Failure(
				DesignerDocumentTransactionState::Failed,
				L"用户代码文件在事件迁移计划建立期间已被外部修改；"
				L"未覆盖该修改，请重新执行操作。",
				true);
		}
	}

	for (auto& change : changes)
		change.Target->swap(change.Desired);
	bool codeFilesChanged = false;
	auto rollback = [&](std::wstring message, bool selectionRestored)
	{
		for (auto change = changes.rbegin();
			change != changes.rend(); ++change)
			change->Target->swap(change->Desired);
		bool restored = selectionRestored;
		if (codeFilesChanged)
		{
			std::wstring restoreError;
			bool codeRestored = false;
			if (codeExpectedCurrent.Empty())
			{
				DesignerModel::AtomicFileWriteEntry sourceRestore;
				sourceRestore.FilePath = userCodePath;
				sourceRestore.Content = originalSource;
				sourceRestore.RequireExpectedState = true;
				sourceRestore.ExpectedExisted = true;
				sourceRestore.ExpectedContent = migratedSource;
				codeRestored = DesignerModel::AtomicFile::WriteBatch(
					{ sourceRestore }, &restoreError);
			}
			else
			{
				codeRestored = codeSnapshot.RestoreIfCurrentMatches(
					codeExpectedCurrent, &restoreError);
			}
			if (!codeRestored)
			{
				restored = false;
				message += L"\n代码文件恢复失败："
					+ (restoreError.empty()
						? std::wstring(L"未知错误。") : restoreError);
			}
		}
		return DesignerDocumentTransactionResult::Failure(
			DesignerDocumentTransactionState::Failed,
			std::move(message), restored);
	};

	DesignerModel::DesignDocumentEventIndex index;
	std::wstring validationError;
	bool valid = false;
	try
	{
		valid = _canvas->BuildEventHandlerIndex(index, &validationError);
	}
	catch (...)
	{
		validationError = L"校验事件索引时发生异常。";
	}
	if (!valid)
	{
		return rollback(
			L"事件差量违反文档处理函数契约：" + validationError,
			true);
	}

	if (migrateCode)
	{
		try
		{
			std::wstring migrationError;
			DesignerModel::AtomicFileWriteEntry migrationWrite;
			migrationWrite.FilePath = userCodePath;
			migrationWrite.Content = migratedSource;
			migrationWrite.RequireExpectedState = true;
			migrationWrite.ExpectedExisted = true;
			migrationWrite.ExpectedContent = originalSource;
			if (!DesignerModel::AtomicFile::WriteBatch(
				{ migrationWrite }, &migrationError))
			{
				return rollback(migrationError.empty()
					? L"写入迁移后的用户代码文件失败。"
					: std::move(migrationError), true);
			}
			codeFilesChanged = true;
			if (!DesignerModel::AtomicFileBatchSnapshot::Capture(
				MigrationOutputFiles(_codeMigration),
				codeExpectedCurrent, &migrationError))
			{
				return rollback(migrationError.empty()
					? L"迁移函数体后无法捕获代码文件状态。"
					: std::move(migrationError), true);
			}
			DesignerModel::DesignDocument document;
			if (!_canvas->BuildDesignDocument(document, &migrationError))
			{
				return rollback(migrationError.empty()
					? L"迁移函数体后无法构建设计文档。"
					: std::move(migrationError), true);
			}
			DesignerModel::DesignCodeGenerationOptions options;
			options.OutputBasePath = _codeMigration.OutputBasePath;
			options.ClassName = _codeMigration.ClassName;
			if (!DesignerModel::DesignCodeGenerationService::Generate(
				document, L"", options, nullptr, &migrationError))
			{
				return rollback(migrationError.empty()
					? L"迁移函数体后的代码重新生成失败。"
					: std::move(migrationError), true);
			}
			if (!DesignerModel::AtomicFileBatchSnapshot::Capture(
				MigrationOutputFiles(_codeMigration),
				codeExpectedCurrent, &migrationError))
			{
				return rollback(migrationError.empty()
					? L"重新生成后无法捕获代码文件状态。"
					: std::move(migrationError), true);
			}
		}
		catch (...)
		{
			return rollback(
				L"迁移用户函数体或重新生成代码时发生异常。", true);
		}
	}

	try
	{
		_canvas->RestoreSelectionByNames(
			_selectionNames, _primarySelectionName, true);
	}
	catch (...)
	{
		return rollback(
			L"事件差量已回滚，但选择恢复时发生异常。", false);
	}
	return DesignerDocumentTransactionResult::Success(
		DesignerDocumentTransactionState::Committed);
}
