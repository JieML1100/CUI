#pragma once

#include "../../CuiDesigner/DesignerModel/DesignDocument.h"

#include <memory>
#include <string>

namespace DesignerModel
{
class DesignDocumentGraph;
}

namespace CuiRuntime
{
struct XamlDocumentCompilationOptions
{
	/** Explicit theme for tests/hosts; otherwise the embedded Generic.xaml. */
	std::shared_ptr<const DesignerModel::DesignDocument> Theme;
	bool UseFrameworkTheme = true;
};

struct XamlCompiledDocument
{
	DesignerModel::DesignDocument Document;
	std::shared_ptr<const DesignerModel::DesignDocument> Theme;
	/**
	 * Immutable topology and metadata preflight produced with the expanded
	 * document. Template factories instantiate one compiled plan repeatedly,
	 * so these document invariants must not be rebuilt for every item.
	 */
	std::shared_ptr<const DesignerModel::DesignDocumentGraph> DocumentGraph;
	bool RuntimePreflightValidated = false;
};

/**
 * Canonical pre-materialization pass shared by dynamic runtime, Designer
 * preview and static C++ generation. It expands component/control templates
 * without collapsing Theme and authored Style precedence into one sheet.
 */
class XamlDocumentCompiler final
{
public:
	static bool Compile(
		const DesignerModel::DesignDocument& source,
		XamlCompiledDocument& output,
		const XamlDocumentCompilationOptions& options = {},
		std::wstring* outError = nullptr,
		DesignerModel::XamlDocumentDiagnostic* outDiagnostic = nullptr);
	/**
	 * Ownership-taking path for synthetic template documents. It preserves the
	 * same compiler semantics while avoiding a second deep copy of a temporary
	 * document assembled solely for this compilation.
	 */
	static bool Compile(
		DesignerModel::DesignDocument&& source,
		XamlCompiledDocument& output,
		const XamlDocumentCompilationOptions& options = {},
		std::wstring* outError = nullptr,
		DesignerModel::XamlDocumentDiagnostic* outDiagnostic = nullptr);
};
}
