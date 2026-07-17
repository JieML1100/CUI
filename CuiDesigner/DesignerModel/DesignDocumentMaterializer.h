#pragma once

#include "DesignDocument.h"

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
	/** Creates the real runtime instance for nodes with CustomType metadata. */
	std::function<std::unique_ptr<Control>(const DesignNode&)> CustomControlFactory;
	/** Use the built-in base type when a custom factory is unavailable. */
	bool AllowCustomControlProxy = false;
	/** Preserve typed custom-only metadata/bindings when using a tool proxy. */
	bool AllowDeferredCustomMetadata = false;
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
