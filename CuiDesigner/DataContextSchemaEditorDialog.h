#pragma once

#include "DesignerTypes.h"
#include "../CUI/include/Form.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/CheckBox.h"
#include "../CUI/include/Button.h"

/** Structured editor for the document-level design-time DataContext schema. */
class DataContextSchemaEditorDialog : public Form
{
public:
	bool Applied = false;
	DesignerDataContextSchema ResultSchema;

	explicit DataContextSchemaEditorDialog(
		const DesignerDataContextSchema& schema,
		const IBindingSource* runtimeSource = nullptr);
	~DataContextSchemaEditorDialog() = default;

private:
	ComboBox* _existingPath = nullptr;
	TextBox* _path = nullptr;
	ComboBox* _kind = nullptr;
	CheckBox* _canRead = nullptr;
	CheckBox* _canWrite = nullptr;
	CheckBox* _canObserve = nullptr;
	Label* _validation = nullptr;
	RichTextBox* _summary = nullptr;
	const IBindingSource* _runtimeSource = nullptr;
	bool _loading = false;

	void SelectComboValue(ComboBox* combo, const std::wstring& value);
	void RefreshPathOptions(const std::wstring& preferredPath = L"");
	void LoadSelectedProperty();
	void RefreshSummary();
	void ShowValidation(const std::wstring& message, bool isError);
	bool SaveProperty();
	void RemoveProperty();
	void ImportRuntimeSchema();
};
