#pragma once

/**
 * @file CodeGenerator.h
 * @brief CodeGenerator：将 Designer 模型导出为 C++ 代码的生成器。
 */
#include "DesignerTypes.h"
#include "DesignerStyleSheet.h"
#include "DesignerModel/DesignDocument.h"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <map>
#include <limits>

struct CodeGeneratorFileContent
{
	std::wstring Path;
	std::string Content;
	/** Exact target state observed before this plan read any user files. */
	bool ExpectedExisted = false;
	std::string ExpectedContent;
};

enum class CodeGeneratorHandlerDefinitionState : unsigned char
{
	Missing,
	Compatible,
	Incompatible,
	DuplicateCompatible,
};

struct CodeGeneratorHandlerDefinitionInspection
{
	std::string Name;
	std::string ParameterList;
	CodeGeneratorHandlerDefinitionState State =
		CodeGeneratorHandlerDefinitionState::Missing;
	size_t DefinitionCount = 0;
	size_t CompatibleDefinitionCount = 0;
	size_t IncompatibleShapeDefinitionCount = 0;
	size_t DeletedCompatibleDefinitionCount = 0;
	size_t HeaderDefinitionCount = 0;
	size_t HeaderCompatibleDefinitionCount = 0;
	size_t HeaderIncompatibleShapeDefinitionCount = 0;
	size_t HeaderDeletedCompatibleDefinitionCount = 0;
	size_t SourceDefinitionCount = 0;
	size_t SourceCompatibleDefinitionCount = 0;
	size_t SourceIncompatibleShapeDefinitionCount = 0;
	size_t SourceDeletedCompatibleDefinitionCount = 0;
	size_t FirstHeaderDefinitionLine = 0;
	size_t FirstHeaderCompatibleDefinitionLine = 0;
	size_t FirstSourceDefinitionLine = 0;
	size_t FirstSourceCompatibleDefinitionLine = 0;
};

class CodeGenerator
{
private:
	std::wstring _className;
	DesignerModel::DesignDocument _sourceDocument;
	DesignerStyleSheet _styleSheet;
	std::wstring _resourceBasePath;
	std::unordered_map<const DesignerModel::DesignNode*, std::string> _varNameOf;
	std::wstring _lastError;
	
	std::string WStringToString(const std::wstring& wstr) const;
	std::wstring StringToWString(const std::string& str) const;
	std::string GetControlTypeName(UIClass type);
	std::string GetIncludeForType(UIClass type);
	void BuildVarNameMap();
	std::string GetVarName(const DesignerModel::DesignNode& node) const;
	const std::vector<DesignerComponentEventDescriptor>& ComponentEvents(
		const DesignerModel::DesignNode& node) const noexcept;
	std::string CommandTargetExpression(const std::wstring& name) const;
	static std::string SanitizeCppIdentifier(const std::string& raw);
	std::string EscapeWStringLiteral(const std::wstring& s);
	std::string FloatLiteral(float v);
	std::string DoubleLiteral(double v);
	std::string ColorToString(D2D1_COLOR_F color);
	std::string ThicknessToString(const Thickness& t);
	std::string GridLengthToCtorString(
		const DesignerModel::DesignGridLength& length);
	std::string GenerateTransformExpression(const cui::drawing::Transform& value);
	std::string GenerateGeometryExpression(const cui::drawing::Geometry& value);
	std::string GenerateStyleValueExpression(const DesignerStyleValue& value);
	std::string GenerateStyleSheetCode(int indent);
	bool CollectEventHandlers(
		std::vector<std::pair<std::string, std::string>>& handlers,
		std::wstring* outError = nullptr) const;

	std::string GenerateControlInstantiation(
		const DesignerModel::DesignNode& node, int indent);
	std::string GenerateControlCommonProperties(
		const DesignerModel::DesignNode& node, int indent);
	std::string GenerateAuthoredProperties(
		const DesignerModel::DesignNode& node, int indent);
	std::string GenerateLocalResources(
		const DesignerModel::DesignNode& node, int indent);
	std::string GenerateContainerProperties(
		const DesignerModel::DesignNode& node, int indent);
	std::string GenerateCppForBaseName(
		const std::string& generatedHeaderBaseName);
	
public:
	CodeGenerator(
		std::wstring className,
		const DesignerModel::DesignDocument& document);
	/** Schema-only static-lowering validation; never materializes preview controls. */
	static bool ValidateDocument(
		const DesignerModel::DesignDocument& document,
		std::wstring* outError = nullptr,
		DesignerModel::XamlDocumentDiagnostic* outDiagnostic = nullptr);
	
	bool GenerateFiles(std::wstring headerPath, std::wstring cppPath);
	/** Builds the exact five-file result without creating or modifying files. */
	bool BuildFilePlan(
		std::wstring headerPath,
		std::wstring cppPath,
		std::vector<CodeGeneratorFileContent>& files);
	/** Uses the same token/signature rules as generation without writing files. */
	bool InspectUserHandlerDefinitions(
		std::string_view userSource,
		std::vector<CodeGeneratorHandlerDefinitionInspection>& inspections);
	/** Jointly inspects inline header and out-of-class source definitions. */
	bool InspectUserHandlerDefinitions(
		std::string_view userHeader,
		std::string_view userSource,
		std::vector<CodeGeneratorHandlerDefinitionInspection>& inspections);
	std::string GenerateHeader();
	std::string GenerateCpp();
	const std::wstring& GetLastError() const noexcept { return _lastError; }
};
