#pragma once

#include "DesignerModel/DesignDocument.h"
#include "../CUI/include/Window.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/CheckBox.h"
#include "../CUI/include/Button.h"

/** Structured editor for local DataType and DataList resources. */
class DataResourcesEditorDialog final : public Window
{
public:
	bool Applied = false;
	DesignerModel::DesignDocument ResultDocument;

	explicit DataResourcesEditorDialog(
		const DesignerModel::DesignDocument& document);

private:
	ComboBox* _typeList = nullptr;
	TextBox* _typeName = nullptr;
	ComboBox* _propertyList = nullptr;
	TextBox* _propertyPath = nullptr;
	ComboBox* _propertyKind = nullptr;
	ComboBox* _propertyObjectKind = nullptr;
	ComboBox* _propertyItemType = nullptr;
	CheckBox* _propertyCanRead = nullptr;
	CheckBox* _propertyCanWrite = nullptr;
	CheckBox* _propertyCanObserve = nullptr;

	ComboBox* _listList = nullptr;
	TextBox* _listKey = nullptr;
	ComboBox* _listItemType = nullptr;
	ComboBox* _recordList = nullptr;
	RichTextBox* _recordFields = nullptr;
	ComboBox* _templateList = nullptr;
	TextBox* _templateKey = nullptr;
	ComboBox* _templateDataType = nullptr;
	Label* _validation = nullptr;
	RichTextBox* _summary = nullptr;
	bool _loading = false;

	void SelectComboValue(ComboBox* combo, const std::wstring& value);
	void RefreshTypeList(const std::wstring& preferred = {});
	void LoadSelectedType();
	void RefreshPropertyList(const std::wstring& preferred = {});
	void LoadSelectedProperty();
	void RefreshObjectEditors();
	void RefreshListList(const std::wstring& preferred = {});
	void LoadSelectedList();
	void RefreshRecordList(int preferredIndex = -1);
	void LoadSelectedRecord();
	void RefreshItemTypeChoices();
	void RefreshTemplateList(const std::wstring& preferred = {});
	void LoadSelectedTemplate();
	void RefreshSummary();
	void ShowValidation(const std::wstring& message, bool isError);

	bool SaveType();
	void RemoveType();
	bool SaveProperty();
	void RemoveProperty();
	bool SaveList();
	void RemoveList();
	bool SaveRecord();
	void RemoveRecord();
	bool SaveTemplate();
	void RemoveTemplate();
};
