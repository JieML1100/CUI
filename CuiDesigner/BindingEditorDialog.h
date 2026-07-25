#pragma once

#include "DesignerBindingUtils.h"
#include "DesignerTypes.h"
#include "../CUI/include/Window.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/RichTextBox.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/Button.h"
#include "../CUI/include/NumericUpDown.h"
#include "../CUI/include/CheckBox.h"
#include <map>
#include <memory>
#include <vector>

struct DesignerBindingElementSource
{
	std::wstring Name;
	IBindingSource* Source = nullptr;
};

/** Structured editor for the bindings attached to one design-time control. */
class BindingEditorDialog : public Window
{
public:
	bool Applied = false;
	std::map<std::wstring, DesignerDataBinding> ResultBindings;

	BindingEditorDialog(
		Control* target,
		const std::map<std::wstring, DesignerDataBinding>& bindings,
		const DesignerDataContextSchema& sourceSchema = {},
		IBindingSource* runtimeSource = nullptr,
		std::vector<DesignerBindingElementSource> elementSources = {});
	~BindingEditorDialog() = default;

private:
	Control* _target = nullptr;
	IBindingSource* _runtimeSource = nullptr;
	std::vector<std::shared_ptr<IBindingSource>> _runtimePathOwners;
	std::vector<std::shared_ptr<IBindingList>> _runtimePathListOwners;
	std::vector<EventConnection> _runtimeValidationConnections;
	std::vector<EventConnection> _runtimePathConnections;
	DesignerDataContextSchema _sourceSchema;
	std::vector<DesignerBindingElementSource> _elementSources;
	std::vector<DesignerBindingUtils::TargetMetadata> _properties;
	ComboBox* _targetProperty = nullptr;
	ComboBox* _sourceObject = nullptr;
	TextBox* _sourcePath = nullptr;
	ComboBox* _knownSourcePath = nullptr;
	TextBox* _ancestorType = nullptr;
	NumericUpDown* _ancestorLevel = nullptr;
	CheckBox* _useFallbackValue = nullptr;
	TextBox* _fallbackValue = nullptr;
	CheckBox* _useTargetNullValue = nullptr;
	TextBox* _targetNullValue = nullptr;
	CheckBox* _useConverterParameter = nullptr;
	TextBox* _converterParameter = nullptr;
	CheckBox* _useStringFormat = nullptr;
	TextBox* _stringFormat = nullptr;
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
	bool _loadedMultiBinding = false;

	const DesignerBindingUtils::TargetMetadata* SelectedMetadata() const;
	void SelectComboValue(ComboBox* combo, const std::wstring& value);
	IBindingSource* CurrentRuntimeSource() const;
	DesignerDataContextSchema CurrentSourceSchema() const;
	std::wstring CurrentElementName() const;
	DesignerBindingRelativeSource CurrentRelativeSource() const;
	void RefreshAncestorState();
	void RefreshOptionalValueState();
	void RefreshKnownSourcePaths();
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
