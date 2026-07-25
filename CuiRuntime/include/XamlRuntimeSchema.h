#pragma once

#include "../../CUI/include/Control.h"
#include "../../CuiDesigner/DesignerModel/DesignDocument.h"

#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace CuiRuntime
{
/**
 * One built-in XAML type identity mapped to a native C++ behavior host.
 *
 * The XAML name is authoritative. NativeType only selects the C++ behavior
 * implementation; it is not a second public type identity.
 */
struct BuiltInXamlTypeDescriptor final
{
	RuntimeTypeId TypeId;
	UIClass NativeType = UIClass::UI_Base;
	bool IsDefaultForNativeType = true;
	/**
	 * False for WPF type identities such as Panel that participate in
	 * x:Type, TargetType and type inheritance but cannot be authored as an
	 * object element directly.
	 */
	bool IsConstructible = true;
	/** XAML type metadata default; native behavior hosts do not infer this. */
	bool FocusableByDefault = false;
	/** Optional design-surface projection owned by this XAML type descriptor. */
	std::wstring_view DesignerDisplayName;
	float DesignerDefaultWidth = 0.0f;
	float DesignerDefaultHeight = 0.0f;
	bool DesignerIsContainer = false;
	std::wstring_view DesignerCategory;

	bool IsDesignerToolboxType() const noexcept
	{
		return !DesignerDisplayName.empty()
			&& DesignerDefaultWidth > 0.0f
			&& DesignerDefaultHeight > 0.0f
			&& !DesignerCategory.empty();
	}
};

/** A schema-owned attached property; RuntimePropertyName addresses the DP. */
struct XamlAttachedPropertyDescriptor final
{
	RuntimeTypeId OwnerType;
	std::wstring Name;
	std::wstring RuntimePropertyName;
	BindingValueKind ValueKind = BindingValueKind::Empty;
};

/** Immutable property view for one built-in or XAML-defined component type. */
struct XamlTypePropertySchema final
{
	UIClass NativeType = UIClass::UI_Base;
	std::shared_ptr<const DeclarativeTypeDescriptor> DeclarativeType;
	std::vector<const DependencyPropertyMetadata*> Properties;

	const DependencyPropertyMetadata* FindProperty(
		std::wstring_view propertyName) const noexcept;
};

/**
 * Runtime-owned XAML schema for built-in types and attached members.
 *
 * Applications cannot register C++ control types here. Application types are
 * supplied only by XAML ComponentDefinition descriptors and receive native
 * behavior through their declared BaseType/behavior attachments.
 */
class XamlRuntimeSchema final
{
public:
	static constexpr std::wstring_view CuiNamespace = L"urn:cui";

	static const BuiltInXamlTypeDescriptor* FindBuiltInType(
		std::wstring_view namespaceUri,
		std::wstring_view localName) noexcept;
	/** Enumerates the immutable built-in XAML type table. */
	static std::span<const BuiltInXamlTypeDescriptor>
		EnumerateBuiltInTypes() noexcept;
	static const BuiltInXamlTypeDescriptor* DefaultTypeFor(
		UIClass nativeType) noexcept;
	static const XamlAttachedPropertyDescriptor* FindAttachedProperty(
		std::wstring_view ownerNamespaceUri,
		std::wstring_view ownerLocalName,
		std::wstring_view memberName) noexcept;
	/** Native dependency-property schema without constructing a Control. */
	static std::vector<const DependencyPropertyMetadata*> NativeProperties(
		UIClass nativeType);
	/** Finds native metadata without constructing or mutating a Control. */
	static const DependencyPropertyMetadata* FindNativeProperty(
		UIClass nativeType,
		std::wstring_view propertyName);
	/** Builds the immutable XAML component descriptor without a behavior host. */
	static std::shared_ptr<const DeclarativeTypeDescriptor>
		CreateComponentTypeDescriptor(
			const DesignerModel::DesignComponentDefinition& component,
			const DesignerModel::DesignDocument& document,
			std::wstring* outError = nullptr);
	/** Builds the effective native + declarative member view for a XAML type. */
	static bool BuildPropertySchema(
		UIClass nativeType,
		const DesignerModel::DesignComponentDefinition* component,
		const DesignerModel::DesignDocument& document,
		XamlTypePropertySchema& output,
		std::wstring* outError = nullptr);

	/** Creates only the native behavior host selected by an existing schema. */
	static std::unique_ptr<Control> CreateNativeControl(UIClass nativeType);
	/** Attaches a shared immutable built-in descriptor to a materialized object. */
	static bool AttachBuiltInType(
		Control& control,
		const BuiltInXamlTypeDescriptor& type,
		XamlSchemaContext& context,
		std::wstring* outError = nullptr);
	/** Builds and attaches one XAML ComponentDefinition contract. */
	static bool AttachComponentContract(
		Control& control,
		const DesignerModel::DesignComponentDefinition& component,
		const DesignerModel::DesignDocument& document,
		std::wstring* outError = nullptr);
};
}
