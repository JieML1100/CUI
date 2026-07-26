#pragma once

#include "DesignDocument.h"
#include "../DesignerEventCatalog.h"
#include "../../CUI/include/Core/EventConnection.h"
#include "../../CUI/include/ControlWeakReference.h"
#include "../../CUI/include/NativeSurface.h"
#include "../../CUI/include/RoutedCommand.h"
#include "../../CUI/include/XamlSchema.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

class Window;
struct CommandBinding;

namespace DesignerModel
{
class RuntimeDocument;

namespace Detail
{
struct RuntimeDocumentReferenceState final
{
	RuntimeDocument* Document = nullptr;
};
}

template<typename T>
class RuntimeControlRef;
class RuntimeDocumentRef;

/** Thread-safe application behavior registry for declarative NativeSurface hosts. */
class NativeSurfaceBehaviorRegistry final
{
public:
	using Factory = std::function<std::unique_ptr<INativeSurfaceBehavior>(
		NativeSurface& host)>;

	bool Register(
		std::wstring behaviorKey,
		Factory factory,
		std::wstring* outError = nullptr);
	bool Unregister(const std::wstring& behaviorKey) noexcept;
	std::unique_ptr<INativeSurfaceBehavior> Create(
		const std::wstring& behaviorKey,
		NativeSurface& host) const;

private:
	mutable std::mutex _mutex;
	std::unordered_map<std::wstring, Factory> _factories;
};

/**
 * Thread-safe application behavior registry keyed by the exact QName of an
 * XAML ComponentDefinition. Factories never create controls; they only create
 * behavior objects for already materialized component hosts.
 */
class DeclarativeComponentBehaviorRegistry final
{
public:
	using Factory = std::function<std::unique_ptr<IDeclarativeComponentBehavior>(
		const DeclarativeComponentBehaviorContext& context)>;

	bool Register(
		RuntimeTypeId componentType,
		Factory factory,
		std::wstring* outError = nullptr);
	bool Unregister(const RuntimeTypeId& componentType) noexcept;
	std::unique_ptr<IDeclarativeComponentBehavior> Create(
		const DeclarativeComponentBehaviorContext& context) const;

private:
	mutable std::mutex _mutex;
	std::unordered_map<std::wstring, Factory> _factories;
};

/**
 * One named control-event request produced by a declarative runtime document.
 *
 * C++ member functions cannot be looked up safely from a string. Applications
 * therefore provide a resolver which subscribes the requested handler and
 * returns its RAII EventConnection. Static generated forms do not need this
 * indirection: their generated code uses std::bind_front directly.
 */
struct RuntimeControlEventRequest
{
	Control& Target;
	int StableId = 0;
	std::wstring ControlName;
	UIClass ControlType = UIClass::UI_Base;
	RuntimeTypeId DeclarativeOwnerType;
	DesignerEventDescriptor Event;
	std::wstring HandlerName;
	/** Non-empty only for a handler declared by XAML CommandBinding. */
	std::wstring CommandName;
	/**
	 * Non-null only while resolving one XAML CommandBinding handler. The
	 * resolver must assign the matching callback into this binding instead of
	 * subscribing directly to a routed event.
	 */
	CommandBinding* CommandBindingSink = nullptr;
};

using RuntimeControlEventResolver = std::function<bool(
	const RuntimeControlEventRequest& request,
	EventConnection& connection,
	std::wstring& error)>;

struct RuntimeWindowEventRequest
{
	Window& Target;
	std::wstring WindowName;
	DesignerEventDescriptor Event;
	std::wstring HandlerName;
	std::wstring CommandName;
	CommandBinding* CommandBindingSink = nullptr;
};

using RuntimeWindowEventResolver = std::function<bool(
	const RuntimeWindowEventRequest& request,
	EventConnection& connection,
	std::wstring& error)>;

enum class RuntimeContentHostAttachMode
{
	/** First transfer into an empty content slot. */
	Initial,
	/** Commit replacement Content after a transactional detach. */
	Replacement,
	/** Restore the exact detached Content after a failed replacement. */
	Rollback,
};

/**
 * Ownership bridge for a host that stores RuntimeDocument Content externally.
 *
 * Every operation is atomic from the caller's perspective. On failure,
 * DetachContent must leave the host unchanged and output empty; AttachContent
 * must leave the host unchanged and preserve the unique_ptr. A successful
 * detach opens one transaction which a successful Replacement or Rollback
 * attach closes. Implementations must report failures through the return value
 * and must not throw after mutating host ownership.
 */
class RuntimeDocumentContentHost
{
public:
	virtual ~RuntimeDocumentContentHost() = default;
	virtual bool DetachContent(
		Control* content,
		std::unique_ptr<Control>& output,
		std::wstring* outError = nullptr) = 0;
	virtual bool AttachContent(
		std::unique_ptr<Control>& content,
		RuntimeContentHostAttachMode mode,
		std::wstring* outError = nullptr) = 0;
};

struct RuntimeDocumentLoadOptions
{
	/** Optional runtime data context. The document keeps it alive. */
	std::shared_ptr<IBindingSource> DataContext;
	/** Optional name-to-handler resolver for control events. */
	RuntimeControlEventResolver ControlEventResolver;
	/** Reject documents containing control handlers when no resolver is set. */
	bool RequireControlEventResolver = false;
	/** Application behavior implementations keyed by NativeSurface.BehaviorKey. */
	std::shared_ptr<const NativeSurfaceBehaviorRegistry> NativeSurfaceBehaviors;
	/** Optional C++ behaviors keyed by XAML ComponentDefinition QName. */
	std::shared_ptr<const DeclarativeComponentBehaviorRegistry>
		DeclarativeComponentBehaviors;
	/** Tooling-only: allow unresolved NativeSurface behavior placeholders. */
	bool AllowNativeSurfacePlaceholder = false;
	/** Recreate behavior attachments even when control topology is reusable. */
	bool ForceBehaviorRefresh = false;
	/** Rebuilds runtime resources when dependency bytes changed but XAML is identical. */
	bool ForceResourceRefresh = false;
};

enum class RuntimeDocumentReloadMode
{
	Unchanged,
	InPlace,
	/** Topology changed, but one or more unchanged DesignId subtrees were retained. */
	Recomposed,
	Replaced,
};

/**
 * Fully materialized, move-only runtime representation of one design document.
 *
 * ContentRoot is owned by this object until ReleaseContentRoot() or
 * TransferContentRootTo() is called. The latter retains an ownership adapter
 * so topology reload can still commit or roll back the host's Content slot.
 * The DesignerControl records are transient materialization projections used by
 * runtime lookup, event wiring and transactional reload. They are never an
 * authored declaration source; persistence and static lowering consume the
 * retained DesignDocument. Window targets passed to ApplyWindowProperties(),
 * BindWindowEvents(), or TransferContentRootTo(Window) are retained non-owning
 * and must outlive this document.
 */
class RuntimeDocument final
{
public:
	RuntimeDocument();
	~RuntimeDocument();

	RuntimeDocument(const RuntimeDocument&) = delete;
	RuntimeDocument& operator=(const RuntimeDocument&) = delete;
	RuntimeDocument(RuntimeDocument&& other) noexcept;
	RuntimeDocument& operator=(RuntimeDocument&& other) noexcept;

	const DesignNode& WindowNode() const noexcept { return _window; }
	const DesignerDataContextSchema& DataContextSchema() const noexcept
	{
		return _dataContextSchema;
	}
	const DesignerStyleSheet& StyleSheet() const noexcept { return _styleSheet; }
	std::vector<ResourceDependency> ResourceDependencies() const
	{
		return _sourceDocument
			? _sourceDocument->ResourceDependencies()
			: std::vector<ResourceDependency>{};
	}
	const std::vector<std::shared_ptr<DesignerControl>>& Controls() const noexcept
	{
		return _controls;
	}
	Control* ContentRoot() const noexcept { return _contentRoot.Get(); }
	bool OwnsContentRoot() const noexcept { return !_contentReleased; }
	bool HasContentHostAdapter() const noexcept
	{
		return static_cast<bool>(_contentHost);
	}

	Control* FindControlByDesignId(int stableId) noexcept;
	const Control* FindControlByDesignId(int stableId) const noexcept;
	Control* FindControlByName(const std::wstring& name) noexcept;
	const Control* FindControlByName(const std::wstring& name) const noexcept;

	template<typename T>
	T* FindControlByDesignId(int stableId) noexcept
	{
		return dynamic_cast<T*>(FindControlByDesignId(stableId));
	}

	/**
	 * Creates a lightweight typed reference resolved through the stable ID index
	 * on every access. It follows in-place, recomposed, and replaced reloads as
	 * well as move construction. Destroying the document expires the reference;
	 * Get() then returns nullptr without retaining document or control ownership.
	 */
	template<typename T = Control>
	RuntimeControlRef<T> ReferenceByDesignId(int stableId) noexcept;

	/**
	 * Creates a non-owning document view backed by the same weak lifetime state
	 * as RuntimeControlRef. Generated ClassReferences views use this so their
	 * GetXxx methods return nullptr after document destruction instead of
	 * dereferencing a stale RuntimeDocument address.
	 */
	RuntimeDocumentRef Reference() noexcept;

	/**
	 * Atomically replaces bindings installed by this RuntimeDocument. Existing
	 * unrelated runtime bindings are never removed.
	 */
	bool BindDataContext(
		std::shared_ptr<IBindingSource> source,
		std::wstring* outError = nullptr);
	void ClearDataBindings();
	const std::shared_ptr<IBindingSource>& BoundDataContext() const noexcept
	{
		return _dataContext;
	}

	/** Resolves and subscribes every configured control event transactionally. */
	bool BindControlEvents(
		const RuntimeControlEventResolver& resolver,
		std::wstring* outError = nullptr);
	void ClearControlEvents() noexcept;
	size_t BoundControlEventCount() const noexcept
	{
		return _eventConnections.size() + _boundControlCommandHandlerCount;
	}
	/** Applies persisted values and retains the Window for future reload refreshes. */
	bool ApplyWindowProperties(::Window& form, std::wstring* outError = nullptr);
	/** Resolves handlers and retains the non-owning Window/resolver across reloads. */
	bool BindWindowEvents(
		::Window& form,
		const RuntimeWindowEventResolver& resolver,
		std::wstring* outError = nullptr);
	void ClearWindowEvents() noexcept;
	size_t BoundWindowEventCount() const noexcept
	{
		return _windowEventConnections.size() + _boundWindowCommandHandlerCount;
	}
	/**
	 * Atomically applies Window presentation, resolves Window events, and transfers
	 * ContentRoot to Window's built-in transactional host adapter.
	 */
	bool AttachToWindow(
		::Window& form,
		const RuntimeWindowEventResolver& resolver,
		std::wstring* outError = nullptr);
	/** Window-event-free convenience overload. */
	bool AttachToWindow(
		::Window& form,
		std::wstring* outError = nullptr);
	/** Advanced overload for a custom Content host associated with the Window. */
	bool AttachToWindow(
		::Window& form,
		std::shared_ptr<RuntimeDocumentContentHost> contentHost,
		const RuntimeWindowEventResolver& resolver,
		std::wstring* outError = nullptr);

	/**
	 * Transfers ContentRoot to the caller and disconnects runtime
	 * bindings/events while the controls are alive. Lookup metadata remains a
	 * non-owning snapshot; use TransferContentRootTo() when later reload,
	 * rebinding, or event management is required.
	 */
	std::unique_ptr<Control> ReleaseContentRoot();
	/** Atomically transfers Content to an adapter retained for future reloads. */
	bool TransferContentRootTo(
		std::shared_ptr<RuntimeDocumentContentHost> host,
		std::wstring* outError = nullptr);
	/** Convenience overload using CUI Window's built-in transactional adapter. */
	bool TransferContentRootTo(
		::Window& form,
		std::wstring* outError = nullptr);

private:
	friend class RuntimeDocumentLoader;
	friend class RuntimeDocumentTopologyReloader;

	struct InstalledBinding
	{
		ControlWeakReference Target;
		std::wstring Property;
	};

	struct PendingCommandTargetReference
	{
		ControlWeakReference Source;
		std::wstring SourceName;
		std::vector<std::size_t> MenuItemPath;
		std::wstring TargetName;
		bool TargetsWindow = false;
	};

	struct CommandTargetSnapshot
	{
		ControlWeakReference Source;
		ControlWeakReference Target;
		bool Authored = false;
		bool IsInputBindingCollection = false;
		std::vector<InputBinding> InputBindings;
	};

	struct PendingInputBindingTargetReference
	{
		ControlWeakReference Source;
		std::wstring SourceName;
		std::size_t BindingIndex = 0;
		std::wstring TargetName;
		bool TargetsWindow = false;
	};

	DesignNode _window = DesignDocument{}.Window;
	DesignerDataContextSchema _dataContextSchema;
	DesignerStyleSheet _styleSheet;
	std::shared_ptr<IBindingSource> _dataContext;
	std::unique_ptr<Control> _ownedContentRoot;
	ControlWeakReference _contentRoot;
	std::vector<std::shared_ptr<DesignerControl>> _controls;
	std::vector<std::shared_ptr<CollectionViewSource>> _collectionViews;
	std::vector<PendingCommandTargetReference> _commandTargetReferences;
	std::vector<PendingInputBindingTargetReference>
		_inputBindingTargetReferences;
	std::unordered_map<int, Control*> _controlsByDesignId;
	std::unordered_map<std::wstring, Control*> _controlsByName;
	std::vector<InstalledBinding> _installedBindings;
	std::vector<EventConnection> _eventConnections;
	std::vector<EventConnection> _windowEventConnections;
	std::vector<EventConnection> _commandBindingConnections;
	std::vector<EventConnection> _windowCommandBindingConnections;
	size_t _boundControlCommandHandlerCount = 0;
	size_t _boundWindowCommandHandlerCount = 0;
	RuntimeControlEventResolver _controlEventResolver;
	RuntimeWindowEventResolver _windowEventResolver;
	::Window* _windowEventTarget = nullptr;
	mutable ::Window* _appliedWindow = nullptr;
	/** Actual Window that supplies the mounted Content inheritance context. */
	::Window* _dataContextWindow = nullptr;
	/** Actual Window currently satisfying authored Window CommandTarget records. */
	::Window* _commandTargetWindow = nullptr;
	std::shared_ptr<RuntimeDocumentContentHost> _contentHost;
	std::shared_ptr<const NativeSurfaceBehaviorRegistry> _nativeSurfaceBehaviors;
	std::shared_ptr<const DeclarativeComponentBehaviorRegistry>
		_declarativeComponentBehaviors;
	bool _allowNativeSurfacePlaceholder = false;
	std::optional<DesignDocument> _sourceDocument;
	bool _contentReleased = false;
	std::shared_ptr<Detail::RuntimeDocumentReferenceState> _referenceState;

	bool InstallDataBindings(
		const std::shared_ptr<IBindingSource>& source,
		std::vector<InstalledBinding>& installed,
		std::wstring* outError,
		::Window* windowTarget = nullptr,
		const DesignNode* windowNode = nullptr,
		bool includeControls = true);
	static void RemoveDataBindings(
		std::vector<InstalledBinding>& installed) noexcept;
	void RebuildControlIndex();
	bool ApplyCommandTargetReferences(
		::Window* windowTarget,
		bool allowPendingWindow,
		std::vector<CommandTargetSnapshot>* rollback,
		std::wstring* outError);
	static void RestoreCommandTargetSnapshots(
		const std::vector<CommandTargetSnapshot>& snapshots) noexcept;
	bool HasWindowCommandTargetReferences() const noexcept;
	bool CommitInheritedWindowAttachments(
		RuntimeDocument& previous,
		const std::function<bool(std::wstring*)>& finalCommit,
		std::wstring* outError);
};

/** Non-owning, reload-aware typed reference to one stable design control. */
template<typename T>
class RuntimeControlRef final
{
public:
	RuntimeControlRef() = default;

	int StableId() const noexcept { return _stableId; }
	T* Get() const noexcept
	{
		const auto state = _referenceState.lock();
		return state && state->Document
			? state->Document->FindControlByDesignId<T>(_stableId) : nullptr;
	}
	explicit operator bool() const noexcept { return Get() != nullptr; }
	T* operator->() const noexcept { return Get(); }
	T& operator*() const { return *Get(); }

private:
	friend class RuntimeDocument;
	RuntimeControlRef(
		const std::shared_ptr<Detail::RuntimeDocumentReferenceState>& state,
		int stableId) noexcept
		: _referenceState(state), _stableId(stableId)
	{
	}

	std::weak_ptr<Detail::RuntimeDocumentReferenceState> _referenceState;
	int _stableId = 0;
};

/** Non-owning, move-aware weak view of one RuntimeDocument object identity. */
class RuntimeDocumentRef final
{
public:
	RuntimeDocumentRef() = default;

	RuntimeDocument* Get() const noexcept
	{
		const auto state = _referenceState.lock();
		return state ? state->Document : nullptr;
	}
	explicit operator bool() const noexcept { return Get() != nullptr; }

	template<typename T = Control>
	T* FindControlByDesignId(int stableId) const noexcept
	{
		const auto document = Get();
		return document
			? document->FindControlByDesignId<T>(stableId) : nullptr;
	}

	template<typename T = Control>
	RuntimeControlRef<T> ReferenceByDesignId(int stableId) const noexcept
	{
		const auto document = Get();
		return document
			? document->ReferenceByDesignId<T>(stableId)
			: RuntimeControlRef<T>{};
	}

private:
	friend class RuntimeDocument;
	explicit RuntimeDocumentRef(
		const std::shared_ptr<Detail::RuntimeDocumentReferenceState>& state)
		noexcept : _referenceState(state)
	{
	}

	std::weak_ptr<Detail::RuntimeDocumentReferenceState> _referenceState;
};

template<typename T>
RuntimeControlRef<T> RuntimeDocument::ReferenceByDesignId(
	int stableId) noexcept
{
	static_assert(std::is_base_of_v<Control, T>,
		"RuntimeControlRef<T> requires a Control-derived type");
	return RuntimeControlRef<T>(_referenceState, stableId);
}

inline RuntimeDocumentRef RuntimeDocument::Reference() noexcept
{
	return RuntimeDocumentRef(_referenceState);
}

/**
 * Transactional entry points for normalized documents and XAML loading.
 * Load* replaces only a detached output document. Load*IntoWindow additionally
 * commits Window presentation/events/Content as one transaction. Once attached,
 * callers must use Reload* so the retained host adapter can participate.
 */
class RuntimeDocumentLoader final
{
public:
	static bool Load(
		const DesignDocument& document,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
	/** Loads the public declarative frontend through the normalized document path. */
	static bool LoadXaml(
		const std::string& xaml,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
	static bool LoadXamlFile(
		const std::wstring& filePath,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);

	/**
	 * Loads a detached candidate, then atomically attaches its Window presentation,
	 * Window events, and the sole Content before replacing output.
	 */
	static bool LoadIntoWindow(
		const DesignDocument& document,
		::Window& window,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		const RuntimeWindowEventResolver& windowResolver = {},
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
	static bool LoadXamlIntoWindow(
		const std::string& xaml,
		::Window& window,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		const RuntimeWindowEventResolver& windowResolver = {},
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
	static bool LoadXamlFileIntoWindow(
		const std::wstring& filePath,
		::Window& window,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		const RuntimeWindowEventResolver& windowResolver = {},
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);

	/**
	 * Reloads transactionally. Common scalar/metadata properties, Binding and
	 * DataContext schema, document styles, control events, and Window presentation
	 * reuse every control instance by DesignId. Topology changes first recompose
	 * a candidate tree with maximal unchanged DesignId subtrees; if none can be
	 * retained, structural collection payloads and font ownership fall back to
	 * full replacement while RuntimeDocument owns its Content or has a
	 * transactional external-Content host adapter.
	 *
	 * Omitted DataContext and event resolver inherit the current document's
	 * runtime attachments.
	 */
	static bool Reload(
		const DesignDocument& document,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		RuntimeDocumentReloadMode* outMode = nullptr,
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
	static bool ReloadXaml(
		const std::string& xaml,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		RuntimeDocumentReloadMode* outMode = nullptr,
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);
	static bool ReloadXamlFile(
		const std::wstring& filePath,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		RuntimeDocumentReloadMode* outMode = nullptr,
		std::wstring* outError = nullptr,
		XamlDocumentDiagnostic* outDiagnostic = nullptr);

private:
	static bool LoadCore(
		const DesignDocument& document,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options,
		bool deferDataBindings,
		std::wstring* outError,
		XamlDocumentDiagnostic* outDiagnostic);
	static bool ReloadHosted(
		const DesignDocument& document,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& effectiveOptions,
		RuntimeDocumentReloadMode* outMode,
		std::wstring* outError,
		XamlDocumentDiagnostic* outDiagnostic);
};
}
