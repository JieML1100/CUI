#pragma once

#include "DesignDocument.h"
#include <string>

namespace DesignerModel::DesignDataResourceEditorModel
{
	/** Adds or replaces a local DataType and rewrites every typed reference on rename. */
	bool UpsertDataType(
		DesignDocument& document,
		const std::wstring& originalName,
		DesignDataTypeDefinition definition,
		std::wstring* outError = nullptr);

	/** Removes an unreferenced local DataType. */
	bool RemoveDataType(
		DesignDocument& document,
		const std::wstring& name,
		std::wstring* outError = nullptr);

	/** Adds/replaces one DataType property and rewrites record/template paths on rename. */
	bool UpsertDataTypeProperty(
		DesignDocument& document,
		const std::wstring& typeName,
		const std::wstring& originalPath,
		DesignerDataContextProperty property,
		std::wstring* outError = nullptr);

	/** Adds or replaces a local DataList and rewrites StaticResource references on rename. */
	bool UpsertDataList(
		DesignDocument& document,
		const std::wstring& originalKey,
		DesignDataList definition,
		std::wstring* outError = nullptr);

	/** Removes an unreferenced local DataList. */
	bool RemoveDataList(
		DesignDocument& document,
		const std::wstring& key,
		std::wstring* outError = nullptr);

	/** Adds/replaces a local DataTemplate and rewrites ItemTemplate references. */
	bool UpsertDataTemplate(
		DesignDocument& document,
		const std::wstring& originalKey,
		DesignDataTemplate definition,
		std::wstring* outError = nullptr);

	/** Removes an unreferenced local DataTemplate. */
	bool RemoveDataTemplate(
		DesignDocument& document,
		const std::wstring& key,
		std::wstring* outError = nullptr);
}
