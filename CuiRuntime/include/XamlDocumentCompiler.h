#pragma once

#include "../../CuiDesigner/DesignerModel/DesignDocument.h"

#include <memory>
#include <string>

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
};
}
