#pragma once

#include "DesignDocument.h"
#include "../../CUI/include/BindingList.h"
#include <memory>
#include <string>

namespace DesignerModel::DesignDataResourceUtils
{
	/** True for the built-in, non-user-declarable group header contract. */
	bool IsCollectionViewGroupDataType(const std::wstring& name);

	/** Builds the group header binding schema, optionally expanding FirstItem. */
	DesignerDataContextSchema BuildCollectionViewGroupSchema(
		const DesignDataTypeDefinition* itemType = nullptr,
		const std::vector<DesignCollectionAggregateDescription>* aggregates = nullptr);

	/** Canonicalizes and validates every DataType/DataTemplate/DataList relation. */
	bool ValidateAndCanonicalize(
		DesignDocument& document,
		std::wstring* outError = nullptr);

	/** Materializes an embedded DataList as an observable, strongly typed source. */
	std::shared_ptr<ObservableBindingList> BuildRuntimeList(
		const DesignDocument& document,
		const DesignDataList& list,
		std::wstring* outError = nullptr);
}
