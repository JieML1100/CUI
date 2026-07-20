#pragma once

#include "DesignerPropertyValue.h"
#include "DesignerTypes.h"
#include "DesignerModel/DesignDocument.h"
#include <functional>
#include <string>
#include <vector>

enum class DesignerControlPropertyEditorKind : unsigned char
{
	Text,
	Boolean,
	FontName,
	FontSize,
	Anchor,
	Choice
};

/** One property owned by the Designer wrapper rather than Binding metadata. */
struct DesignerControlPropertyDescriptor
{
	std::wstring Name;
	std::wstring DisplayName;
	std::wstring Category;
	int CategoryOrder = 1000;
	int Order = 0;
	DesignerStyleValueKind ValueKind = DesignerStyleValueKind::String;
	DesignerControlPropertyEditorKind Editor = DesignerControlPropertyEditorKind::Text;
	bool CanReset = false;
};

struct DesignerControlPropertyContext
{
	::Font* SharedFont = nullptr;
	std::function<std::wstring(DesignerControl&, const std::wstring&)> MakeUniqueName;
	std::function<void(UIClass, const std::wstring&)> SyncDefaultNameCounter;
	std::function<void(const std::wstring&, const std::wstring&)>
		RewriteElementNameReferences;
	std::function<void(Control*, uint8_t)> ApplyAnchorStylesKeepingBounds;
	const std::vector<DesignerModel::DesignControlTemplate>*
		ControlTemplates = nullptr;
	/** Document plus root-to-target local ControlTemplates after lexical shadowing. */
	std::vector<DesignerModel::DesignControlTemplate> ScopedControlTemplates;
	const std::vector<DesignerModel::DesignDataTemplate>* DataTemplates = nullptr;
	/** Document plus root-to-target local DataTemplates after lexical shadowing. */
	std::vector<DesignerModel::DesignDataTemplate> ScopedDataTemplates;
	const std::vector<DesignerModel::DesignDataList>* DataLists = nullptr;
	const std::vector<DesignerModel::DesignCollectionViewSource>*
		CollectionViews = nullptr;
	const DesignerDataContextSchema* DataContextSchema = nullptr;
	const std::vector<DesignerModel::DesignItemsPanelTemplate>*
		ItemsPanelTemplates = nullptr;
	/** Document plus root-to-target local panel templates after lexical shadowing. */
	std::vector<DesignerModel::DesignItemsPanelTemplate>
		ScopedItemsPanelTemplates;
	const std::vector<DesignerModel::DesignGroupStyle>* GroupStyles = nullptr;
	/** Document plus root-to-target local group styles after lexical shadowing. */
	std::vector<DesignerModel::DesignGroupStyle> ScopedGroupStyles;
	const DesignerStyleSheet* StyleSheet = nullptr;
};

namespace DesignerControlPropertyCatalog
{
	/** Returns presentation-ordered properties applicable to this wrapper. */
	std::vector<DesignerControlPropertyDescriptor> GetProperties(
		const DesignerControl& target);

	const DesignerControlPropertyDescriptor* Find(
		const DesignerControl& target,
		const std::wstring& propertyName);

	bool CaptureValue(
		const DesignerControl& target,
		const DesignerControlPropertyContext& context,
		const std::wstring& propertyName,
		DesignerStyleValue& out,
		std::wstring* outError = nullptr);

	bool ApplyValue(
		DesignerControl& target,
		DesignerControlPropertyContext& context,
		const std::wstring& propertyName,
		const DesignerStyleValue& value,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);

	bool ResetValue(
		DesignerControl& target,
		DesignerControlPropertyContext& context,
		const std::wstring& propertyName,
		DesignerStyleValue* outEffective = nullptr,
		std::wstring* outError = nullptr);
}
