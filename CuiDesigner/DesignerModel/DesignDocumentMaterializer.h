#pragma once

#include "DesignDocument.h"
#include "../../CUI/include/NativeSurface.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace DesignerModel
{
/**
 * A complete, detached runtime control forest produced from one design
 * document. DesignerControl records are non-owning views over Roots.
 */
struct MaterializedControlTree
{
	std::vector<std::unique_ptr<Control>> Roots;
	std::vector<std::shared_ptr<DesignerControl>> Controls;
	/** Owns reusable views, including dynamic DataContext-bound resources. */
	std::vector<std::shared_ptr<CollectionViewSource>> CollectionViews;

	MaterializedControlTree() = default;
	MaterializedControlTree(const MaterializedControlTree&) = delete;
	MaterializedControlTree& operator=(const MaterializedControlTree&) = delete;
	MaterializedControlTree(MaterializedControlTree&&) noexcept = default;
	MaterializedControlTree& operator=(MaterializedControlTree&&) noexcept = default;
};

struct DesignDocumentMaterializationOptions
{
	/**
	 * Optional control factory. The default creates production runtime controls;
	 * the Designer supplies its lightweight preview factory explicitly.
	 */
	std::function<std::unique_ptr<Control>(UIClass)> ControlFactory;
	/** Resolves application-owned behavior for a declarative NativeSurface. */
	std::function<std::unique_ptr<INativeSurfaceBehavior>(
		const DesignNode&, NativeSurface&)> NativeSurfaceBehaviorFactory;
	/** Resolves optional application behavior by declarative component QName. */
	std::function<std::unique_ptr<IDeclarativeComponentBehavior>(
		const DeclarativeComponentBehaviorContext&)>
		DeclarativeComponentBehaviorFactory;
	/** Tooling may preview a NativeSurface without loading application code. */
	bool AllowNativeSurfacePlaceholder = false;
};

/**
 * Neutral document-to-control-tree materializer shared by DesignerCanvas,
 * dynamic XML loading, and static code-generation input construction.
 */
class DesignDocumentMaterializer final
{
public:
	/**
	 * Creates the production control used by the neutral runtime path. Besides
	 * materialization, schema frontends use this as a metadata probe so property
	 * kinds and enum choices continue to come from the control itself.
	 */
	static std::unique_ptr<Control> CreateRuntimeControl(UIClass type);
	/** Installs one document component's dynamic property/event contract. */
	static bool InstallComponentContract(
		Control& control,
		const DesignComponentDefinition& component,
		const DesignDocument& document,
		std::wstring* outError = nullptr);

	static bool Materialize(
		const DesignDocument& document,
		MaterializedControlTree& output,
		std::wstring* outError = nullptr);
	static bool Materialize(
		const DesignDocument& document,
		MaterializedControlTree& output,
		const DesignDocumentMaterializationOptions& options,
		std::wstring* outError = nullptr);
};
}
