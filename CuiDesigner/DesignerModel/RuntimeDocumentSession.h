#pragma once

#include "RuntimeDocumentFileWatcher.h"
#include "RuntimeEventHandlerRegistry.h"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

namespace DesignerModel
{
struct RuntimeDocumentSessionMountOptions
{
	/** Optional runtime data context retained by the mounted document. */
	std::shared_ptr<IBindingSource> DataContext;
	/** Custom controls available to the mounted XAML document and hot reloads. */
	std::shared_ptr<const RuntimeCustomControlRegistry> CustomControls;
	/** Start save-driven file watching after the initial atomic mount. */
	bool WatchFile = true;
};

/**
 * Opinionated UI-thread host for one file-backed dynamic Form.
 *
 * The session owns the RuntimeDocument, named-event registry, and threadless
 * file watcher while the caller owns the Form. Register handlers first, then
 * call MountFile(). The initial load, Form presentation/events, and root
 * transfer remain atomic; later Poll() calls expose the watcher's transactional
 * reload result instead of hiding reload failures behind a background thread.
 *
 * The Form and every object captured by registered callbacks must outlive this
 * session. Advanced custom-root-host scenarios should use RuntimeDocument and
 * RuntimeDocumentFileWatcher directly.
 */
class RuntimeDocumentSession final
{
public:
	explicit RuntimeDocumentSession(
		std::chrono::milliseconds debounce = std::chrono::milliseconds{ 200 });

	RuntimeDocumentSession(const RuntimeDocumentSession&) = delete;
	RuntimeDocumentSession& operator=(const RuntimeDocumentSession&) = delete;
	RuntimeDocumentSession(RuntimeDocumentSession&&) = delete;
	RuntimeDocumentSession& operator=(RuntimeDocumentSession&&) = delete;

	RuntimeEventHandlerRegistry& EventHandlers() noexcept { return _handlers; }
	const RuntimeEventHandlerRegistry& EventHandlers() const noexcept
	{
		return _handlers;
	}
	RuntimeDocument& Document() noexcept { return _document; }
	const RuntimeDocument& Document() const noexcept { return _document; }

	bool IsMounted() const noexcept { return _mountedForm != nullptr; }
	::Form* MountedForm() const noexcept { return _mountedForm; }
	const std::wstring& SourceFile() const noexcept { return _sourceFile; }
	uint32_t OwningThreadId() const noexcept { return _owningThreadId; }

	bool MountFile(
		const std::wstring& filePath,
		::Form& form,
		const RuntimeDocumentSessionMountOptions& options = {},
		std::wstring* outError = nullptr);

	bool StartWatching(std::wstring* outError = nullptr);
	void StopWatching() noexcept { _watcher.Stop(); }
	bool IsWatching() const noexcept { return _watcher.IsWatching(); }
	bool HasPendingChange() const noexcept { return _watcher.HasPendingChange(); }
	const std::wstring& LastWatchError() const noexcept
	{
		return _watcher.LastError();
	}
	std::chrono::milliseconds Debounce() const noexcept
	{
		return _watcher.Debounce();
	}
	void SetDebounce(std::chrono::milliseconds value) noexcept
	{
		_watcher.SetDebounce(value);
	}

	RuntimeDocumentWatchResult Poll();
	RuntimeDocumentWatchResult PollAt(
		RuntimeDocumentFileWatcher::TimePoint now);
	void RequestRetry();
	void RequestRetryAt(RuntimeDocumentFileWatcher::TimePoint now) noexcept;

private:
	RuntimeEventHandlerRegistry _handlers;
	RuntimeDocument _document;
	RuntimeDocumentFileWatcher _watcher;
	::Form* _mountedForm = nullptr;
	std::wstring _sourceFile;
	uint32_t _owningThreadId = 0;

	bool CheckOwningThread(std::wstring* outError) const;
	RuntimeDocumentLoadOptions MakeLoadOptions(
		std::shared_ptr<IBindingSource> dataContext,
		std::shared_ptr<const RuntimeCustomControlRegistry> customControls = {}) const;
};
}
