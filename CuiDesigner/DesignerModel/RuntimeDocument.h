#pragma once

#include "DesignDocument.h"
#include "../CodeGenInput.h"
#include "../DesignerEventCatalog.h"
#include "../../CUI/include/Core/EventConnection.h"

#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

class Form;

namespace DesignerModel
{
template<typename T>
class RuntimeControlRef;

/** Thread-safe registry used to materialize custom XAML controls. */
class RuntimeCustomControlRegistry final
{
public:
	using Factory = std::function<std::unique_ptr<Control>(const DesignNode&)>;

	bool Register(
		std::wstring xamlNamespace,
		std::wstring xamlName,
		Factory factory,
		std::wstring* outError = nullptr);
	bool Unregister(
		const std::wstring& xamlNamespace,
		const std::wstring& xamlName) noexcept;
	std::unique_ptr<Control> Create(const DesignNode& node) const;

private:
	static std::wstring MakeKey(
		const std::wstring& xamlNamespace,
		const std::wstring& xamlName);
	mutable std::mutex _mutex;
	std::unordered_map<std::wstring, Factory> _factories;
};

/**
 * One named control-event request produced by a dynamic design document.
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
	DesignerCustomControlType CustomType;
	DesignerEventDescriptor Event;
	std::wstring HandlerName;
};

using RuntimeControlEventResolver = std::function<bool(
	const RuntimeControlEventRequest& request,
	EventConnection& connection,
	std::wstring& error)>;

struct RuntimeFormEventRequest
{
	Form& Target;
	std::wstring FormName;
	DesignerEventDescriptor Event;
	std::wstring HandlerName;
};

using RuntimeFormEventResolver = std::function<bool(
	const RuntimeFormEventRequest& request,
	EventConnection& connection,
	std::wstring& error)>;

enum class RuntimeRootHostAttachMode
{
	/** First transfer; append using the host's normal root placement policy. */
	Initial,
	/** Commit replacement roots at the detached document forest's anchor. */
	Replacement,
	/** Restore the exact detached roots to their previously captured slots. */
	Rollback,
};

/**
 * Ownership bridge for a host that stores RuntimeDocument roots externally.
 *
 * Every operation is atomic from the caller's perspective. On failure,
 * DetachRoots must leave the host unchanged and output empty; AttachRoots must
 * leave the host unchanged and preserve every unique_ptr in roots. A successful
 * detach opens one transaction which a successful Replacement or Rollback
 * attach closes. Implementations must report failures through the return value
 * and must not throw after mutating host ownership.
 */
class RuntimeDocumentRootHost
{
public:
	virtual ~RuntimeDocumentRootHost() = default;
	virtual bool DetachRoots(
		std::span<Control* const> roots,
		std::vector<std::unique_ptr<Control>>& output,
		std::wstring* outError = nullptr) = 0;
	virtual bool AttachRoots(
		std::vector<std::unique_ptr<Control>>& roots,
		RuntimeRootHostAttachMode mode,
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
	/** Registry for nodes carrying a custom XAML/C++ type descriptor. */
	std::shared_ptr<const RuntimeCustomControlRegistry> CustomControls;
	/** Primarily for tools: materialize the declared built-in base as a proxy. */
	bool AllowCustomControlProxy = false;
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
 * Root controls are owned by this object until ReleaseRootControls() or
 * TransferRootControlsTo() is called. The latter retains an ownership adapter
 * so topology reload can still commit or roll back the host's root forest.
 * The DesignerControl records remain available so code generation, named event
 * resolution, bindings, and diagnostic tools all consume the same materialized
 * tree instead of reconstructing different partial models. Form targets passed
 * to ApplyFormProperties(), BindFormEvents(), or TransferRootControlsTo(Form)
 * are retained non-owning and must outlive this document.
 */
class RuntimeDocument final
{
public:
	RuntimeDocument() = default;
	~RuntimeDocument();

	RuntimeDocument(const RuntimeDocument&) = delete;
	RuntimeDocument& operator=(const RuntimeDocument&) = delete;
	RuntimeDocument(RuntimeDocument&&) noexcept = default;
	RuntimeDocument& operator=(RuntimeDocument&& other) noexcept;

	const DesignFormModel& FormModel() const noexcept { return _form; }
	const DesignerDataContextSchema& DataContextSchema() const noexcept
	{
		return _dataContextSchema;
	}
	const DesignerStyleSheet& StyleSheet() const noexcept { return _styleSheet; }
	const std::vector<std::shared_ptr<DesignerControl>>& Controls() const noexcept
	{
		return _controls;
	}
	const std::vector<Control*>& RootControls() const noexcept
	{
		return _rootControls;
	}
	bool OwnsRootControls() const noexcept { return !_rootsReleased; }
	bool HasRootHostAdapter() const noexcept
	{
		return static_cast<bool>(_rootHost);
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
	 * long as this RuntimeDocument object itself remains alive and is not moved.
	 */
	template<typename T = Control>
	RuntimeControlRef<T> ReferenceByDesignId(int stableId) noexcept;

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
		return _eventConnections.size();
	}
	/** Applies persisted values and retains the Form for future reload refreshes. */
	bool ApplyFormProperties(::Form& form, std::wstring* outError = nullptr) const;
	/** Resolves handlers and retains the non-owning Form/resolver across reloads. */
	bool BindFormEvents(
		::Form& form,
		const RuntimeFormEventResolver& resolver,
		std::wstring* outError = nullptr);
	void ClearFormEvents() noexcept;
	size_t BoundFormEventCount() const noexcept
	{
		return _formEventConnections.size();
	}
	/**
	 * Atomically applies Form presentation, resolves Form events, and transfers
	 * the root forest to Form's built-in transactional host adapter.
	 */
	bool AttachToForm(
		::Form& form,
		const RuntimeFormEventResolver& resolver,
		std::wstring* outError = nullptr);
	/** Form-event-free convenience overload. */
	bool AttachToForm(
		::Form& form,
		std::wstring* outError = nullptr);
	/** Advanced overload for a custom root host associated with the Form. */
	bool AttachToForm(
		::Form& form,
		std::shared_ptr<RuntimeDocumentRootHost> rootHost,
		const RuntimeFormEventResolver& resolver,
		std::wstring* outError = nullptr);

	/**
	 * Transfers the complete root forest to the caller. Runtime metadata keeps
	 * non-owning control pointers; keep those controls alive while using this
	 * RuntimeDocument for rebinding, lookup, or event management.
	 */
	std::vector<std::unique_ptr<Control>> ReleaseRootControls();
	/** Atomically transfers roots to an adapter retained for future reloads. */
	bool TransferRootControlsTo(
		std::shared_ptr<RuntimeDocumentRootHost> host,
		std::wstring* outError = nullptr);
	/** Convenience overload using CUI Form's built-in transactional adapter. */
	bool TransferRootControlsTo(
		::Form& form,
		std::wstring* outError = nullptr);

	/** Creates the exact input consumed by the existing static C++ generator. */
	CodeGenInput BuildCodeGenInput() const;

private:
	friend class RuntimeDocumentLoader;
	friend class RuntimeDocumentTopologyReloader;

	struct InstalledBinding
	{
		Control* Target = nullptr;
		std::wstring Property;
		bool LocalValueWasSuspended = false;
		std::optional<BindingValue> PreviousLocalValue;
	};

	DesignFormModel _form;
	DesignerDataContextSchema _dataContextSchema;
	DesignerStyleSheet _styleSheet;
	std::shared_ptr<IBindingSource> _dataContext;
	std::vector<std::unique_ptr<Control>> _ownedRoots;
	std::vector<Control*> _rootControls;
	std::vector<std::shared_ptr<DesignerControl>> _controls;
	std::unordered_map<int, Control*> _controlsByDesignId;
	std::unordered_map<std::wstring, Control*> _controlsByName;
	std::vector<InstalledBinding> _installedBindings;
	std::vector<EventConnection> _eventConnections;
	std::vector<EventConnection> _formEventConnections;
	RuntimeControlEventResolver _controlEventResolver;
	RuntimeFormEventResolver _formEventResolver;
	::Form* _formEventTarget = nullptr;
	mutable ::Form* _appliedForm = nullptr;
	std::shared_ptr<RuntimeDocumentRootHost> _rootHost;
	std::shared_ptr<const RuntimeCustomControlRegistry> _customControls;
	bool _allowCustomControlProxy = false;
	std::optional<DesignDocument> _sourceDocument;
	bool _rootsReleased = false;

	bool InstallDataBindings(
		const std::shared_ptr<IBindingSource>& source,
		std::vector<InstalledBinding>& installed,
		std::wstring* outError);
	static void RemoveDataBindings(
		std::vector<InstalledBinding>& installed) noexcept;
	void RebuildControlIndex();
	bool CommitInheritedFormAttachments(
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
		return _document
			? _document->FindControlByDesignId<T>(_stableId) : nullptr;
	}
	explicit operator bool() const noexcept { return Get() != nullptr; }
	T* operator->() const noexcept { return Get(); }
	T& operator*() const { return *Get(); }

private:
	friend class RuntimeDocument;
	RuntimeControlRef(RuntimeDocument& document, int stableId) noexcept
		: _document(&document), _stableId(stableId)
	{
	}

	RuntimeDocument* _document = nullptr;
	int _stableId = 0;
};

template<typename T>
RuntimeControlRef<T> RuntimeDocument::ReferenceByDesignId(
	int stableId) noexcept
{
	static_assert(std::is_base_of_v<Control, T>,
		"RuntimeControlRef<T> requires a Control-derived type");
	return RuntimeControlRef<T>(*this, stableId);
}

/**
 * Transactional entry points for dynamic document and XML/XAML loading.
 * Load* replaces only a detached output document. Load*IntoForm additionally
 * commits Form presentation/events/roots as one transaction. Once attached,
 * callers must use Reload* so the retained host adapter can participate.
 */
class RuntimeDocumentLoader final
{
public:
	static bool Load(
		const DesignDocument& document,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		std::wstring* outError = nullptr);
	static bool LoadXml(
		const std::string& xml,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		std::wstring* outError = nullptr);
	static bool LoadFile(
		const std::wstring& filePath,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		std::wstring* outError = nullptr);
	/** Loads CUI's readable XAML-style frontend through the same document path. */
	static bool LoadXaml(
		const std::string& xaml,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		std::wstring* outError = nullptr);
	static bool LoadXamlFile(
		const std::wstring& filePath,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		std::wstring* outError = nullptr);

	/**
	 * Loads a detached candidate, then atomically attaches its Form presentation,
	 * Form events, and root forest before replacing output.
	 */
	static bool LoadIntoForm(
		const DesignDocument& document,
		::Form& form,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		const RuntimeFormEventResolver& formResolver = {},
		std::wstring* outError = nullptr);
	static bool LoadXmlIntoForm(
		const std::string& xml,
		::Form& form,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		const RuntimeFormEventResolver& formResolver = {},
		std::wstring* outError = nullptr);
	static bool LoadFileIntoForm(
		const std::wstring& filePath,
		::Form& form,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		const RuntimeFormEventResolver& formResolver = {},
		std::wstring* outError = nullptr);
	static bool LoadXamlIntoForm(
		const std::string& xaml,
		::Form& form,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		const RuntimeFormEventResolver& formResolver = {},
		std::wstring* outError = nullptr);
	static bool LoadXamlFileIntoForm(
		const std::wstring& filePath,
		::Form& form,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		const RuntimeFormEventResolver& formResolver = {},
		std::wstring* outError = nullptr);

	/**
	 * Reloads transactionally. Common scalar/metadata properties, Binding and
	 * DataContext schema, document styles, control events, and form presentation
	 * reuse every control instance by DesignId. Topology changes first recompose
	 * a candidate tree with maximal unchanged DesignId subtrees; if none can be
	 * retained, control-specific Extra data, font ownership, and unsupported
	 * property bags fall back to full replacement while RuntimeDocument owns its
	 * roots or has a transactional external-root host adapter.
	 *
	 * Omitted DataContext and event resolver inherit the current document's
	 * runtime attachments.
	 */
	static bool Reload(
		const DesignDocument& document,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		RuntimeDocumentReloadMode* outMode = nullptr,
		std::wstring* outError = nullptr);
	static bool ReloadXml(
		const std::string& xml,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		RuntimeDocumentReloadMode* outMode = nullptr,
		std::wstring* outError = nullptr);
	static bool ReloadFile(
		const std::wstring& filePath,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		RuntimeDocumentReloadMode* outMode = nullptr,
		std::wstring* outError = nullptr);
	static bool ReloadXaml(
		const std::string& xaml,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		RuntimeDocumentReloadMode* outMode = nullptr,
		std::wstring* outError = nullptr);
	static bool ReloadXamlFile(
		const std::wstring& filePath,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& options = {},
		RuntimeDocumentReloadMode* outMode = nullptr,
		std::wstring* outError = nullptr);

private:
	static bool ReloadHosted(
		const DesignDocument& document,
		RuntimeDocument& output,
		const RuntimeDocumentLoadOptions& effectiveOptions,
		RuntimeDocumentReloadMode* outMode,
		std::wstring* outError);
};
}
