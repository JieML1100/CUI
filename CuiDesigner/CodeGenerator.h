#pragma once

/**
 * @file CodeGenerator.h
 * @brief CodeGenerator：将 Designer 模型导出为 C++ 代码的生成器。
 */
#include "DesignerTypes.h"
#include "DesignerStyleSheet.h"
#include "DesignerEventCatalog.h"
#include "DesignerModel/DesignDocument.h"
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <sstream>
#include <string_view>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <limits>
#include <optional>
#include <utility>

namespace DesignerModel
{
class BindingConverterCatalog;
class DataGridAutoColumnCatalog;
}

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

enum class CodeGeneratorOutputKind : unsigned char
{
	/** Legacy designer export retaining dynamic-document compatibility views. */
	Window,
	/** Build-owned application output with no runtime-XAML compatibility API. */
	StaticWindow,
	/** Immutable, Control-free construction program for generated themes. */
	FrameworkThemeProgram,
};

class CodeGenerator
{
private:
	std::wstring _className;
	DesignerModel::DesignDocument _sourceDocument;
	DesignerStyleSheet _styleSheet;
	std::wstring _resourceBasePath;
	CodeGeneratorOutputKind _outputKind = CodeGeneratorOutputKind::Window;
	/** Optional build-owned typed converter catalog; null preserves legacy Design. */
	std::shared_ptr<const DesignerModel::BindingConverterCatalog>
		_bindingConverterCatalog;
	/** Optional build-owned DataType/property-to-column transformation rules. */
	std::shared_ptr<const DesignerModel::DataGridAutoColumnCatalog>
		_dataGridAutoColumnCatalog;
	std::unordered_map<const DesignerModel::DesignNode*, std::string> _varNameOf;
	std::wstring _lastError;
	
	struct TypedPropertyInfo
	{
		std::string SetterName;     // e.g. "SetWidth"
		/** BindingValue-valued properties keep the generated wrapper intact. */
		bool PassBindingValue = false;
		/** Non-empty for WPF attached-property owners such as Canvas/Grid. */
		std::string StaticOwner;
		/** Optional native enum/value wrapper applied after BindingValue unwrap. */
		std::string ValueType;
		/**
		 * The runtime metadata stores a normalized XAML name while the CLR
		 * wrapper accepts an enum. Resolve that name at code-generation time
		 * instead of emitting an invalid pointer-to-enum static_cast.
		 */
		bool NamedEnumValue = false;
		/** Exact type extracted when a shared StaticResource is a BindingValue. */
		std::string SharedValueType;
	};
	using DeclarativePropertyResolver = std::function<std::string(
		const std::wstring& targetName,
		const std::wstring& propertyName,
		bool requireWritable)>;
	using DeclarativeTargetResolver = std::function<std::string(
		const std::wstring& targetName)>;
	struct DeclarativeEventReferenceExpression
	{
		std::string Expression;
		bool Routed = false;
	};
	using DeclarativeEventResolver = std::function<
		DeclarativeEventReferenceExpression(const std::wstring& eventName)>;
	static const std::unordered_map<std::wstring, TypedPropertyInfo>& GetKnownProperties();
	static std::optional<TypedPropertyInfo> FindKnownProperty(
		UIClass type,
		const std::wstring& propertyName);
	static std::string UnwrapBindingValue(const std::string& expression);
	static std::string GenerateTypedPropertyCall(
		const std::string& target,
		const TypedPropertyInfo& property,
		const std::string& valueExpression,
		bool valueIsBindingValueVariable = false);

	std::string WStringToString(const std::wstring& wstr) const;
	std::wstring StringToWString(const std::string& str) const;
	std::string GetControlTypeName(UIClass type) const;
	std::string GetIncludeForType(UIClass type) const;
	std::string GetComponentClassName(
		const DesignerModel::DesignComponentDefinition& component) const;
	std::string GetGeneratedControlTypeName(
		const DesignerModel::DesignNode& node) const;
	std::optional<TypedPropertyInfo> FindGeneratedProperty(
		const DesignerModel::DesignNode& node,
		const std::wstring& propertyName) const;
	std::string FindKnownDependencyPropertyExpression(
		UIClass type,
		const std::wstring& propertyName,
		bool requireWritable) const;
	std::string FindGeneratedDependencyPropertyExpression(
		const DesignerModel::DesignNode& node,
		const std::wstring& propertyName,
		bool requireWritable) const;
	std::string FindComponentDependencyPropertyExpression(
		const DesignerModel::DesignComponentDefinition& component,
		const std::wstring& propertyName,
		bool requireWritable) const;
	std::string FindStyleDependencyPropertyExpression(
		const DesignerModel::DesignDocument& document,
		const DesignerStyleRule& rule,
		const std::wstring& propertyName,
		bool requireWritable) const;
	void BuildVarNameMap();
	std::string GetVarName(const DesignerModel::DesignNode& node) const;
	const std::vector<DesignerComponentEventDescriptor>& ComponentEvents(
		const DesignerModel::DesignNode& node) const noexcept;
	std::optional<DesignerEventDescriptor> FindNodeEventDescriptor(
		const DesignerModel::DesignNode& node,
		const std::wstring& eventName) const;
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
	std::string GenerateBindingValueExpression(const BindingValue& value);
	std::string GenerateDeclarativePropertyReference(
		const std::wstring& targetName,
		const std::wstring& propertyName,
		bool requireWritable,
		const DeclarativePropertyResolver& resolver);
	std::string GenerateDeclarativeAnimationCode(
		const DeclarativeVisualStateAnimation& animation,
		const std::string& collectionExpression,
		const DeclarativePropertyResolver& resolver,
		const DeclarativeTargetResolver& targetResolver,
		int indent);
	std::string GenerateDeclarativeStoryboardActionsCode(
		const std::vector<DeclarativeEventTriggerActionDefinition>& actions,
		const std::string& collectionExpression,
		const DeclarativePropertyResolver& resolver,
		const DeclarativeTargetResolver& targetResolver,
		int indent);
	std::string GenerateDeclarativeInteractionsCode(
		const std::vector<DeclarativeVisualStateGroupDefinition>& visualStateGroups,
		const std::vector<DeclarativeEventTriggerDefinition>& eventTriggers,
		const DeclarativePropertyResolver& resolver,
		const DeclarativeTargetResolver& targetResolver,
		const DeclarativeEventResolver& eventResolver,
		const std::string& targetExpression,
		int indent);
	std::string GenerateStyleSheetCode(
		int indent,
		const std::vector<std::pair<std::wstring, std::string>>&
			objectResources = {},
		const std::unordered_map<std::wstring, std::string>*
			sharedDocumentResources = nullptr,
		bool emitEmptyStyleSheet = false,
		const DesignerStyleSheet* styleSheetOverride = nullptr,
		const DesignerModel::DesignDocument* styleDocumentOverride = nullptr,
		const std::string& styleSheetVariable = "__styleSheet",
		const std::vector<std::pair<std::wstring, std::string>>*
			inlineObjectResources = nullptr);
	bool CollectEventHandlers(
		std::vector<std::pair<std::string, std::string>>& handlers,
		std::wstring* outError = nullptr) const;

	std::string GenerateControlInstantiation(
		const DesignerModel::DesignNode& node, int indent);
	std::string GenerateControlCommonProperties(
		const DesignerModel::DesignNode& node, int indent);
	std::string GenerateRelativePanelConstraints(
		const DesignerModel::DesignNode& node,
		const std::string& parentExpression,
		const std::unordered_map<std::wstring, std::string>&
			controlExpressions,
		int indent,
		bool returnViaFail);
	std::string GenerateAuthoredProperties(
		const DesignerModel::DesignNode& node,
		int indent,
		const std::unordered_map<std::wstring, std::string>*
			sharedDocumentResources = nullptr,
		const std::unordered_set<
			const DesignerModel::DesignPropertyAssignment*>*
			sharedDocumentResourceAssignments = nullptr,
		const std::unordered_set<std::wstring>*
			omittedProperties = nullptr);
	std::string GenerateLocalResources(
		const DesignerModel::DesignNode& node,
		int indent,
		const DesignerModel::DesignDocument* sourceDocument = nullptr,
		const std::vector<std::pair<std::wstring, std::string>>*
			visibleObjectResources = nullptr,
		const std::vector<std::pair<std::wstring, std::string>>*
			ownedObjectResources = nullptr,
		bool returnViaFail = false);
	std::string GenerateContainerProperties(
		const DesignerModel::DesignNode& node, int indent);
	std::string GenerateCppForBaseName(
		const std::string& generatedHeaderBaseName);
	
public:
	CodeGenerator(
		std::wstring className,
		const DesignerModel::DesignDocument& document,
		CodeGeneratorOutputKind outputKind = CodeGeneratorOutputKind::Window);
	/** Supplies the immutable application converter contract before generation. */
	void SetBindingConverterCatalog(
		std::shared_ptr<const DesignerModel::BindingConverterCatalog> catalog) noexcept
	{
		_bindingConverterCatalog = std::move(catalog);
	}
	const DesignerModel::BindingConverterCatalog*
		GetBindingConverterCatalog() const noexcept
	{
		return _bindingConverterCatalog.get();
	}
	/** Supplies generation-time auto-column rules before static lowering. */
	void SetDataGridAutoColumnCatalog(
		std::shared_ptr<const DesignerModel::DataGridAutoColumnCatalog>
			catalog) noexcept
	{
		_dataGridAutoColumnCatalog = std::move(catalog);
	}
	const DesignerModel::DataGridAutoColumnCatalog*
		GetDataGridAutoColumnCatalog() const noexcept
	{
		return _dataGridAutoColumnCatalog.get();
	}
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
	/** Current event declarations for a user class deriving the generated base. */
	std::string GenerateHandlerDeclarations();
	/** Emits a source file including the supplied generated-header basename. */
	std::string GenerateCppForHeader(
		const std::string& generatedHeaderBaseName)
	{
		return GenerateCppForBaseName(generatedHeaderBaseName);
	}
	const std::wstring& GetLastError() const noexcept { return _lastError; }
};
