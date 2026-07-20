#pragma once

#include "DesignerTypes.h"
#include <functional>
#include <string>

namespace DesignerBindingUtils
{
	struct TargetMetadata
	{
		std::wstring Name;
		BindingValueKind ValueKind = BindingValueKind::Empty;
		bool CanRead = false;
		bool CanWrite = false;
		bool CanObserve = false;
		DesignerDataObjectKind ObjectKind = DesignerDataObjectKind::Opaque;
		ControlPropertyFlags Flags = ControlPropertyFlags::None;
		DataSourceUpdateMode DefaultUpdateMode =
			DataSourceUpdateMode::OnPropertyChanged;
		bool IsReadOnly = false;
	};

	std::wstring Trim(const std::wstring& value);
	bool IsValidSourcePath(const std::wstring& path);
	DesignerDataContextSchema BuildSourceSchema(const IBindingSource& source);
	void WriteOptionalLiteral(
		DesignerModel::DesignValue& object,
		const char* valueKey,
		const char* kindKey,
		const std::optional<DesignerStyleValue>& value);
	bool TryReadOptionalLiteral(
		const DesignerModel::DesignValue& object,
		const char* valueKey,
		const char* kindKey,
		std::optional<DesignerStyleValue>& value,
		std::wstring* outError = nullptr);
	bool TryConvertOptionalLiteral(
		const std::optional<DesignerStyleValue>& value,
		std::optional<BindingValue>& output,
		std::wstring* outError = nullptr);
	DesignerModel::DesignValue WriteBindingDefinition(
		const DesignerDataBinding& binding);
	bool TryReadBindingDefinition(
		const DesignerModel::DesignValue& value,
		DesignerDataBinding& binding,
		std::wstring* outError = nullptr);
	bool VisitLeafBindingDefinitions(
		const DesignerModel::DesignValue& value,
		const std::function<bool(const DesignerModel::DesignValue&)>& visitor,
		std::wstring* outError = nullptr);
	bool VisitLeafBindingDefinitions(
		DesignerModel::DesignValue& value,
		const std::function<bool(DesignerModel::DesignValue&)>& visitor,
		std::wstring* outError = nullptr);
	struct ResolvedBindingSource
	{
		IBindingSource* Source = nullptr;
		BindingSourceReference OwnedSource;
	};
	using BindingSourceResolver = std::function<bool(
		const DesignerDataBinding& binding,
		ResolvedBindingSource& source,
		std::wstring* outError)>;
	/** Creates either an ordinary Binding or a MultiBinding from one design model. */
	bool InstallBinding(
		Control& target,
		const std::wstring& targetProperty,
		const DesignerDataBinding& binding,
		const BindingSourceResolver& resolveSource,
		std::wstring* outError = nullptr);
	/** Resolves the current nth matching logical ancestor, if any. */
	Control* FindAncestorSource(
		Control& target,
		const DesignerDataBinding& binding) noexcept;
	/**
	 * Creates a stable source that follows logical-parent changes and delegates
	 * property/validation notifications to the currently matching ancestor.
	 */
	BindingSourceReference CreateAncestorSource(
		Control& target,
		const DesignerDataBinding& binding);

	const wchar_t* BindingModeName(BindingMode mode) noexcept;
	bool TryParseBindingMode(const std::wstring& value, BindingMode& mode);
	BindingMode ResolveBindingMode(
		const TargetMetadata& target,
		BindingMode requested) noexcept;
	DataSourceUpdateMode ResolveUpdateMode(
		const TargetMetadata& target,
		DataSourceUpdateMode requested) noexcept;
	const wchar_t* UpdateModeName(DataSourceUpdateMode mode) noexcept;
	/** Canonical WPF spelling used by XAML UpdateSourceTrigger. */
	const wchar_t* UpdateSourceTriggerName(DataSourceUpdateMode mode) noexcept;
	bool TryParseUpdateMode(const std::wstring& value, DataSourceUpdateMode& mode);
	const wchar_t* ValueKindName(BindingValueKind kind) noexcept;

	bool IsModeStructurallyCompatible(
		const BindingPropertyMetadata& metadata,
		BindingMode mode) noexcept;
	bool IsCompatible(
		const BindingPropertyMetadata& metadata,
		const DesignerDataBinding& binding) noexcept;
	bool IsModeStructurallyCompatible(
		const TargetMetadata& metadata,
		BindingMode mode) noexcept;
	/** Projects runtime property metadata into the portable designer contract. */
	TargetMetadata ProjectTargetMetadata(
		const BindingPropertyMetadata& metadata);

	/** Validates a portable design-time target without requiring runtime registration. */
	bool ValidateTarget(
		const TargetMetadata& target,
		const DesignerDataBinding& binding,
		std::wstring* outError = nullptr,
		const DesignerDataContextSchema* sourceSchema = nullptr);

	bool Validate(
		Control& target,
		const std::wstring& targetProperty,
		const DesignerDataBinding& binding,
		const BindingPropertyMetadata** outMetadata = nullptr,
		std::wstring* outError = nullptr,
		const DesignerDataContextSchema* sourceSchema = nullptr);

	std::wstring Describe(
		const std::wstring& targetProperty,
		const DesignerDataBinding& binding);
}
