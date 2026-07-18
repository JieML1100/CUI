#include "RuntimeDocumentSession.h"

#include "DesignDocumentFileFormat.h"

#include <Windows.h>

#include <utility>

namespace DesignerModel
{
namespace
{
	void SetError(std::wstring* output, std::wstring value)
	{
		if (output) *output = std::move(value);
	}

	RuntimeDocumentWatchResult SessionFailure(std::wstring error)
	{
		RuntimeDocumentWatchResult result;
		result.State = RuntimeDocumentWatchState::Failed;
		result.Error = std::move(error);
		return result;
	}
}

RuntimeDocumentSession::RuntimeDocumentSession(
	std::chrono::milliseconds debounce)
	: _watcher(debounce)
{
}

RuntimeDocumentLoadOptions RuntimeDocumentSession::MakeLoadOptions(
	std::shared_ptr<IBindingSource> dataContext,
	std::shared_ptr<const RuntimeCustomControlRegistry> customControls) const
{
	RuntimeDocumentLoadOptions result;
	result.DataContext = std::move(dataContext);
	result.ControlEventResolver = _handlers.ControlResolver();
	result.RequireControlEventResolver = true;
	result.CustomControls = std::move(customControls);
	return result;
}

bool RuntimeDocumentSession::CheckOwningThread(
	std::wstring* outError) const
{
	if (!_owningThreadId || _owningThreadId == GetCurrentThreadId())
	{
		if (outError) outError->clear();
		return true;
	}
	SetError(outError,
		L"RuntimeDocumentSession 只能从首次挂载它的 UI 线程访问。");
	return false;
}

bool RuntimeDocumentSession::MountFile(
	const std::wstring& filePath,
	::Form& form,
	const RuntimeDocumentSessionMountOptions& options,
	std::wstring* outError)
{
	if (IsMounted())
	{
		SetError(outError,
			L"RuntimeDocumentSession 已挂载；请通过 Poll 热重载现有文档。");
		return false;
	}
	if (filePath.empty())
	{
		SetError(outError, L"运行时会话文件路径不能为空。");
		return false;
	}

	RuntimeDocumentFileWatcher candidateWatcher(_watcher.Debounce());
	RuntimeDocumentLoadOptions loadOptions;
	RuntimeFormEventResolver formResolver;
	std::wstring candidateSource;
	try
	{
		candidateSource = filePath;
		if (options.WatchFile
			&& !candidateWatcher.Start(filePath, outError)) return false;
		loadOptions = MakeLoadOptions(
			options.DataContext, options.CustomControls);
		formResolver = _handlers.FormResolver();
	}
	catch (...)
	{
		SetError(outError, L"准备运行时会话挂载时资源分配失败。");
		return false;
	}
	const bool loaded = DetectDesignDocumentFileFormat(filePath)
		== DesignDocumentFileFormat::Xaml
		? RuntimeDocumentLoader::LoadXamlFileIntoForm(
			filePath, form, _document, loadOptions, formResolver, outError)
		: RuntimeDocumentLoader::LoadFileIntoForm(
			filePath, form, _document, loadOptions, formResolver, outError);
	if (!loaded) return false;
	if (options.WatchFile)
		candidateWatcher.SetDependencies(_document.ResourceDependencies());

	_sourceFile = std::move(candidateSource);
	_mountedForm = &form;
	_owningThreadId = GetCurrentThreadId();
	if (options.WatchFile)
		_watcher = std::move(candidateWatcher);
	else
		_watcher.Stop();
	if (outError) outError->clear();
	return true;
}

bool RuntimeDocumentSession::StartWatching(std::wstring* outError)
{
	if (!IsMounted() || _sourceFile.empty())
	{
		SetError(outError, L"运行时会话尚未挂载文件。");
		return false;
	}
	if (!CheckOwningThread(outError)) return false;
	if (!_watcher.Start(_sourceFile, outError)) return false;
	_watcher.SetDependencies(_document.ResourceDependencies());
	return true;
}

RuntimeDocumentWatchResult RuntimeDocumentSession::Poll()
{
	return PollAt(RuntimeDocumentFileWatcher::Clock::now());
}

RuntimeDocumentWatchResult RuntimeDocumentSession::PollAt(
	RuntimeDocumentFileWatcher::TimePoint now)
{
	std::wstring error;
	if (!CheckOwningThread(&error)) return SessionFailure(std::move(error));
	if (!IsMounted())
		return SessionFailure(L"运行时会话尚未挂载文件。");
	return _watcher.PollAt(_document, {}, now);
}

void RuntimeDocumentSession::RequestRetry()
{
	RequestRetryAt(RuntimeDocumentFileWatcher::Clock::now());
}

void RuntimeDocumentSession::RequestRetryAt(
	RuntimeDocumentFileWatcher::TimePoint now) noexcept
{
	if (_owningThreadId && _owningThreadId != GetCurrentThreadId()) return;
	_watcher.RequestRetryAt(now);
}
}
