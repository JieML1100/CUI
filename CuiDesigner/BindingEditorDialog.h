#pragma once

#include "DesignerTypes.h"
#include "../CUI/include/Form.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/Button.h"
#include <map>
#include <memory>
#include <vector>

/** Structured editor for the bindings attached to one design-time control. */
class BindingEditorDialog : public Form
{
public:
	bool Applied = false;
	std::map<std::wstring, DesignerDataBinding> ResultBindings;

	BindingEditorDialog(
		Control* target,
		const std::map<std::wstring, DesignerDataBinding>& bindings,
		const DesignerDataContextSchema& sourceSchema = {},
		IBindingSource* runtimeSource = nullptr);
	~BindingEditorDialog() = default;

private:
	Control* _target = nullptr;
	IBindingSource* _runtimeSource = nullptr;
	std::vector<std::shared_ptr<IBindingSource>> _runtimePathOwners;
	std::vector<EventConnection> _runtimeValidationConnections;
	std::vector<EventConnection> _runtimePathConnections;
	DesignerDataContextSchema _sourceSchema;
	std::vector<const BindingPropertyMetadata*> _properties;
	ComboBox* _targetProperty = nullptr;
	TextBox* _sourcePath = nullptr;
	ComboBox* _knownSourcePath = nullptr;
	ComboBox* _mode = nullptr;
	ComboBox* _updateMode = nullptr;
	ComboBox* _converter = nullptr;
	TextBox* _customConverter = nullptr;
	Label* _capabilities = nullptr;
	Label* _runtimeValidation = nullptr;
	Label* _validation = nullptr;
	RichTextBox* _summary = nullptr;
	Button* _saveBinding = nullptr;
	Button* _removeBinding = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;
	bool _loadingEditor = false;

	const BindingPropertyMetadata* SelectedMetadata() const;
	void SelectComboValue(ComboBox* combo, const std::wstring& value);
	void LoadSelectedBinding();
	void RefreshModeOptions(BindingMode preferredMode);
	void RefreshUpdateModeOptions(DataSourceUpdateMode preferredMode);
	void RefreshConverterOptions(const std::wstring& preferredConverter);
	std::wstring CurrentConverterName() const;
	void SelectKnownSourcePath(const std::wstring& path);
	void RefreshCustomConverterState();
	void RefreshCapabilities();
	void AttachRuntimeValidation();
	void RefreshRuntimeValidation();
	void RefreshSummary();
	void ShowValidation(const std::wstring& message, bool isError);
	bool TryReadEditor(
		std::wstring& targetProperty,
		DesignerDataBinding& binding,
		std::wstring& error) const;
	bool SaveCurrentBinding();
	void RemoveCurrentBinding();
};
