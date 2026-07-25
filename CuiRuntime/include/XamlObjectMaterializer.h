#pragma once

#include "../../CuiDesigner/DesignerModel/DesignDocument.h"
#include "../../CUI/include/ControlWeakReference.h"
#include "../../CUI/include/NativeSurface.h"
#include "../../CUI/include/XamlSchema.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace CuiRuntime
{
/**
 * Non-owning authored ICommandSource reference produced during materialization.
 * SourceName/MenuItemPath are runtime namescope locators used only when a
 * transactional topology reload substitutes an existing control subtree.
 */
struct XamlCommandTargetReference
{
	ControlWeakReference Source;
	std::wstring SourceName;
	std::vector<std::size_t> MenuItemPath;
	std::wstring TargetName;
	bool TargetsWindow = false;
};

/**
 * Non-owning locator for one InputBinding.CommandTarget.  InputBinding stores
 * the target weakly too, but Window references must be rebound when detached
 * Content enters or leaves its actual Window namescope.
 */
struct XamlInputBindingTargetReference
{
	ControlWeakReference Source;
	std::wstring SourceName;
	std::size_t BindingIndex = 0;
	std::wstring TargetName;
	bool TargetsWindow = false;
};

/**
 * A complete, detached runtime control tree produced from one Window
 * document. DesignerControl records are transient non-owning projections over
 * ContentRoot for lookup, event wiring and Designer preview; the source
 * DesignDocument remains the only authored model.
 */
struct XamlObjectTree
{
	std::unique_ptr<Control> ContentRoot;
	std::vector<std::shared_ptr<DesignerControl>> Controls;
	/** Owns reusable views, including dynamic DataContext-bound resources. */
	std::vector<std::shared_ptr<CollectionViewSource>> CollectionViews;
	/** Authored weak command sources, including unresolved Window targets. */
	std::vector<XamlCommandTargetReference> CommandTargets;
	/** Authored weak InputBinding targets, including unresolved Window targets. */
	std::vector<XamlInputBindingTargetReference> InputBindingTargets;

	XamlObjectTree() = default;
	XamlObjectTree(const XamlObjectTree&) = delete;
	XamlObjectTree& operator=(const XamlObjectTree&) = delete;
	XamlObjectTree(XamlObjectTree&&) noexcept = default;
	XamlObjectTree& operator=(XamlObjectTree&&) noexcept = default;
};

struct XamlMaterializationOptions
{
	/**
	 * Internal per-materialization XAML schema context. Nested template builds
	 * inherit it so one RuntimeTypeId always resolves to one shared descriptor.
	 * Leave empty at the outer load boundary.
	 */
	std::shared_ptr<XamlSchemaContext> SchemaContext;
	/**
	 * Optional control factory. The default creates production runtime controls;
	 * the Designer supplies its lightweight preview factory explicitly.
	 */
	std::function<std::unique_ptr<Control>(UIClass)> ControlFactory;
	/** Resolves application-owned behavior for a declarative NativeSurface. */
	std::function<std::unique_ptr<INativeSurfaceBehavior>(
		const DesignerModel::DesignNode&, NativeSurface&)>
		NativeSurfaceBehaviorFactory;
	/** Resolves optional application behavior by declarative component QName. */
	std::function<std::unique_ptr<IDeclarativeComponentBehavior>(
		const DeclarativeComponentBehaviorContext&)>
		DeclarativeComponentBehaviorFactory;
	/** Tooling may preview a NativeSurface without loading application code. */
	bool AllowNativeSurfacePlaceholder = false;
	/** Explicit theme used by the shared compiler; empty selects Generic.xaml. */
	std::shared_ptr<const DesignerModel::DesignDocument> Theme;
	bool UseFrameworkTheme = true;
};

/**
 * Neutral document-to-control-tree materializer shared by DesignerCanvas and
 * dynamic XAML loading. Static code generation deliberately does not enter
 * this layer and lowers the typed DesignDocument directly.
 */
class XamlObjectMaterializer final
{
public:
	static bool Materialize(
		const DesignerModel::DesignDocument& document,
		XamlObjectTree& output,
		std::wstring* outError = nullptr,
		DesignerModel::XamlDocumentDiagnostic* outDiagnostic = nullptr);
	static bool Materialize(
		const DesignerModel::DesignDocument& document,
		XamlObjectTree& output,
		const XamlMaterializationOptions& options,
		std::wstring* outError = nullptr,
		DesignerModel::XamlDocumentDiagnostic* outDiagnostic = nullptr);

	/** Shared declarative VSM installation used by dynamic and static templates. */
	static bool InstallControlTemplateVisualStates(
		Control& owner,
		const DesignerModel::DesignControlTemplate& definition,
		const DesignerModel::DesignDocument& resourceDocument,
		std::wstring* outError = nullptr);
};
}
