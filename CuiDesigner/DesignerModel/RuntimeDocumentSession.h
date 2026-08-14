#pragma once

#include "RuntimeDocumentFileWatcher.h"
#include "RuntimeEventHandlerRegistry.h"
#include "XamlDocumentParser.h"

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
	/** NativeSurface behaviors available to the mounted document and reloads. */
	std::shared_ptr<const NativeSurfaceBehaviorRegistry> NativeSurfaceBehaviors;
	/** Optional application behaviors for XAML-declared component QNames. */
	std::shared_ptr<const DeclarativeComponentBehaviorRegistry>
		DeclarativeComponentBehaviors;
	/** Start save-driven file watching after the initial atomic mount. */
	bool WatchFile = true;
	/**
	 * Keep the authored DesignDocument alive after the atomic mount.
	 *
	 * Ship hosts that mount once can set this to false to drop the parsed
	 * document — the largest single allocation of a dynamic mount — as soon as
	 * the Window is up. Doing so disables in-place/recomposed reload (the watcher
	 * falls back to full replacement) and empties ResourceDependencies(), so
	 * StartWatching() no longer observes satellite resource files. Ignored when
	 * WatchFile is true, because watching without dependencies is a downgrade the
	 * session should not apply silently.
	 */
	bool RetainSourceDocument = true;
	/** Parser options for internal loads; forwarded to the loader. */
	XamlDocumentParseOptions ParseOptions;
};

/**
 * Opinionated UI-thread host for one file-backed dynamic Window.
 *
 * The session owns the RuntimeDocument, named-event registry, and threadless
 * file watcher while the caller owns the Window. Register handlers first, then
 * call MountFile(), or pre-parse and inspect a document before calling
 * MountDocument(). The initial load, Window presentation/events, and Content
 * transfer remain atomic; later Poll() calls expose the watcher's transactional
 * reload result instead of hiding reload failures behind a background thread.
 *
 * The Window and every object captured by registered callbacks must outlive this
 * session. Advanced custom-Content-host scenarios should use RuntimeDocument and
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

	bool IsMounted() const noexcept { return _mountedWindow != nullptr; }
	::Window* MountedWindow() const noexcept { return _mountedWindow; }
	const std::wstring& SourceFile() const noexcept { return _sourceFile; }
	uint32_t OwningThreadId() const noexcept { return _owningThreadId; }

	bool MountFile(
		const std::wstring& filePath,
		::Window& window,
		const RuntimeDocumentSessionMountOptions& options = {},
		std::wstring* outError = nullptr);

	/**
	 * Atomically mounts an already parsed document while retaining its source
	 * path for watching/retry. This lets hosts inspect XAML-declared contracts
	 * before registering handlers or behaviors without parsing the file twice.
	 */
	bool MountDocument(
		const DesignDocument& document,
		const std::wstring& sourceFile,
		::Window& window,
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
	::Window* _mountedWindow = nullptr;
	std::wstring _sourceFile;
	uint32_t _owningThreadId = 0;

	bool CheckOwningThread(std::wstring* outError) const;
	RuntimeDocumentLoadOptions MakeLoadOptions(
		const RuntimeDocumentSessionMountOptions& options) const;
};
}
