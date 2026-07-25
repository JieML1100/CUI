#pragma once

#include "DesignerStyleSheet.h"
#include "DesignerPropertyCatalog.h"
#include "../CUI/include/Window.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/Button.h"

/** Structured editor for document-level WPF Style resources and setters. */
class StyleSheetEditorDialog : public Window
{
public:
	bool Applied = false;
	DesignerStyleSheet ResultStyleSheet;
	// Sequential resource-key edits performed in this dialog. The document
	// owner uses them to rewrite direct property StaticResource expressions.
	std::vector<std::pair<std::wstring, std::wstring>> ResourceRenames;

	explicit StyleSheetEditorDialog(
		const DesignerStyleSheet& styleSheet,
		std::wstring resourceBasePath = {});
	~StyleSheetEditorDialog() = default;

private:
	ComboBox* _resourceList = nullptr;
	TextBox* _resourceKey = nullptr;
	ComboBox* _resourceKind = nullptr;
	TextBox* _resourceValue = nullptr;

	ComboBox* _ruleList = nullptr;
	ComboBox* _ruleType = nullptr;
	TextBox* _ruleId = nullptr;
	TextBox* _ruleBasedOn = nullptr;

	ComboBox* _setterList = nullptr;
	ComboBox* _setterProperty = nullptr;
	ComboBox* _setterMode = nullptr;
	ComboBox* _setterKind = nullptr;
	TextBox* _setterValue = nullptr;

	Label* _validation = nullptr;
	RichTextBox* _summary = nullptr;
	std::vector<DesignerPropertyDescriptor> _setterProperties;
	std::wstring _resourceBasePath;
	bool _loading = false;

	void SelectComboIndex(ComboBox* combo, int index);
	int SelectedResourceIndex() const;
	int SelectedRuleIndex() const;
	int SelectedSetterIndex() const;

	void RefreshResourceList(int preferredIndex = -1);
	void LoadSelectedResource();
	void RefreshRuleList(int preferredIndex = -1);
	void LoadSelectedRule();
	void RefreshSetterList(int preferredIndex = -1);
	void LoadSelectedSetter();
	void RefreshSetterPropertyCatalog(const std::wstring& preferredProperty = L"");
	void ApplySelectedPropertyMetadata(bool replaceValue);
	void RefreshSetterMode(bool replaceValue);
	void RefreshSummary();
	void ShowValidation(const std::wstring& message, bool isError);

	bool SaveResource();
	void RemoveResource();
	bool SaveRule();
	void RemoveRule();
	bool SaveSetter();
	void RemoveSetter();
};
