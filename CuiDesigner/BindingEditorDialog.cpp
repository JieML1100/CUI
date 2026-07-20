#include "BindingEditorDialog.h"
#include "../CUI/include/BindingList.h"
#include "DesignerBindingUtils.h"
#include "DesignerDataContextSchemaUtils.h"
#include "DesignerPropertyCatalog.h"
#include <algorithm>

namespace
{
	const std::wstring kNoConverter = L"<None>";
	const std::wstring kCustomConverter = L"<Custom ID>";
	const std::wstring kManualSourcePath = L"<手动输入>";
	const std::wstring kDataContextSource = L"<DataContext>";
	const std::wstring kSelfSource = L"<RelativeSource Self>";
	const std::wstring kFindAncestorSource = L"<RelativeSource FindAncestor>";

	std::vector<std::wstring> AllBindingModeNames()
	{
		return { L"Default", L"OneWay", L"TwoWay", L"OneWayToSource", L"OneTime" };
	}

	std::vector<std::wstring> AllUpdateModeNames()
	{
		return { L"Default", L"PropertyChanged", L"LostFocus", L"Explicit" };
	}

	bool IsTargetToSourceMode(BindingMode mode)
	{
		return mode == BindingMode::TwoWay
			|| mode == BindingMode::OneWayToSource;
	}

	const wchar_t* YesNo(bool value)
	{
		return value ? L"是" : L"否";
	}

	BindingValueKind ToBindingValueKind(DesignerStyleValueKind kind)
	{
		switch (kind)
		{
		case DesignerStyleValueKind::Bool: return BindingValueKind::Bool;
		case DesignerStyleValueKind::Int: return BindingValueKind::Int;
		case DesignerStyleValueKind::Int64: return BindingValueKind::Int64;
		case DesignerStyleValueKind::Float: return BindingValueKind::Float;
		case DesignerStyleValueKind::Double: return BindingValueKind::Double;
		case DesignerStyleValueKind::String: return BindingValueKind::String;
		default: return BindingValueKind::Object;
		}
	}

	const wchar_t* ValidationSeverityName(BindingValidationSeverity severity)
	{
		switch (severity)
		{
		case BindingValidationSeverity::Info: return L"信息";
		case BindingValidationSeverity::Warning: return L"警告";
		case BindingValidationSeverity::Error: return L"错误";
		}
		return L"未知";
	}
}

BindingEditorDialog::BindingEditorDialog(
	Control* target,
	const std::map<std::wstring, DesignerDataBinding>& bindings,
	const DesignerDataContextSchema& sourceSchema,
	IBindingSource* runtimeSource,
	std::vector<DesignerBindingElementSource> elementSources)
	: Form(L"编辑数据绑定", POINT{ 320, 100 }, SIZE{ 820, 850 }),
	  ResultBindings(bindings),
	  _target(target),
	  _runtimeSource(runtimeSource),
	  _sourceSchema(sourceSchema),
	  _elementSources(std::move(elementSources))
{
	DesignerDataContextSchemaUtils::Canonicalize(_sourceSchema);
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->AllowResize = false;
	this->BackColor = Colors::WhiteSmoke;

	auto tip = this->AddControl(new Label(
		L"选择目标属性，填写数据上下文中的源路径；修改后点击“保存绑定”。", 20, 16));
	tip->Size = { 770, 22 };

	auto targetLabel = this->AddControl(new Label(L"目标属性", 20, 60));
	targetLabel->Size = { 120, 24 };
	_targetProperty = this->AddControl(new ComboBox(L"", 150, 54, 630, 30));
	_targetProperty->ExpandCount = 10;

	auto sourceLabel = this->AddControl(new Label(L"源对象 / 路径", 20, 106));
	sourceLabel->Size = { 120, 24 };
	_sourceObject = this->AddControl(new ComboBox(L"", 150, 100, 190, 30));
	_sourceObject->ExpandCount = 10;
	_sourceObject->Items.push_back(kDataContextSource);
	_sourceObject->Items.push_back(kSelfSource);
	_sourceObject->Items.push_back(kFindAncestorSource);
	for (const auto& source : _elementSources)
		if (!source.Name.empty()) _sourceObject->Items.push_back(source.Name);
	_sourceObject->SelectedIndex = 0;
	_sourceObject->Text = kDataContextSource;
	_sourcePath = this->AddControl(new TextBox(L"", 350, 100, 200, 30));
	_knownSourcePath = this->AddControl(new ComboBox(L"", 560, 100, 220, 30));
	_knownSourcePath->ExpandCount = 10;
	RefreshKnownSourcePaths();

	auto modeLabel = this->AddControl(new Label(L"绑定模式", 20, 152));
	modeLabel->Size = { 120, 24 };
	_mode = this->AddControl(new ComboBox(L"", 150, 146, 250, 30));
	_mode->ExpandCount = 5;

	auto updateLabel = this->AddControl(new Label(L"更新策略", 430, 152));
	updateLabel->Size = { 100, 24 };
	_updateMode = this->AddControl(new ComboBox(L"", 530, 146, 250, 30));
	_updateMode->ExpandCount = 3;

	auto converterLabel = this->AddControl(new Label(L"Converter", 20, 198));
	converterLabel->Size = { 120, 24 };
	_converter = this->AddControl(new ComboBox(L"", 150, 192, 250, 30));
	_converter->ExpandCount = 8;
	auto customConverterLabel = this->AddControl(new Label(L"自定义 ID", 430, 198));
	customConverterLabel->Size = { 100, 24 };
	_customConverter = this->AddControl(new TextBox(L"", 530, 192, 250, 30));

	_useFallbackValue = this->AddControl(new CheckBox(
		L"FallbackValue", 20, 238));
	_useFallbackValue->Size = { 120, 30 };
	_fallbackValue = this->AddControl(new TextBox(L"", 150, 238, 250, 30));
	_useTargetNullValue = this->AddControl(new CheckBox(
		L"TargetNullValue", 430, 238));
	_useTargetNullValue->Size = { 120, 30 };
	_targetNullValue = this->AddControl(new TextBox(L"", 560, 238, 220, 30));

	_useConverterParameter = this->AddControl(new CheckBox(
		L"ConverterParameter", 20, 284));
	_useConverterParameter->Size = { 130, 30 };
	_converterParameter = this->AddControl(new TextBox(L"", 160, 284, 240, 30));
	_useStringFormat = this->AddControl(new CheckBox(
		L"StringFormat", 430, 284));
	_useStringFormat->Size = { 120, 30 };
	_stringFormat = this->AddControl(new TextBox(L"", 560, 284, 220, 30));

	auto ancestorTypeLabel = this->AddControl(new Label(L"AncestorType", 20, 336));
	ancestorTypeLabel->Size = { 120, 24 };
	_ancestorType = this->AddControl(new TextBox(L"", 150, 330, 360, 30));
	auto ancestorLevelLabel = this->AddControl(new Label(L"AncestorLevel", 530, 336));
	ancestorLevelLabel->Size = { 110, 24 };
	_ancestorLevel = this->AddControl(new NumericUpDown(650, 330, 130, 30));
	_ancestorLevel->Min = 1;
	_ancestorLevel->Max = 100;
	_ancestorLevel->Value = 1;

	_capabilities = this->AddControl(new Label(L"", 20, 376));
	_capabilities->Size = { 760, 24 };
	_runtimeValidation = this->AddControl(new Label(L"", 20, 406));
	_runtimeValidation->Size = { 760, 40 };
	_validation = this->AddControl(new Label(L"", 20, 450));
	_validation->Size = { 760, 40 };

	_saveBinding = this->AddControl(new Button(L"保存绑定", 20, 494, 130, 34));
	_removeBinding = this->AddControl(new Button(L"删除绑定", 162, 494, 130, 34));

	auto summaryLabel = this->AddControl(new Label(L"当前绑定", 20, 550));
	summaryLabel->Size = { 120, 24 };
	_summary = this->AddControl(new RichTextBox(L"", 20, 578, 760, 160));
	_summary->ReadOnly = true;
	_summary->AllowMultiLine = true;
	_summary->BackColor = Colors::White;
	_summary->FocusedColor = Colors::White;

	_ok = this->AddControl(new Button(L"确定", 20, 756, 120, 36));
	_cancel = this->AddControl(new Button(L"取消", 152, 756, 120, 36));

	if (_target)
	{
		for (const auto* metadata : BindingPropertyRegistry::GetProperties(*_target))
		{
			if (!metadata || metadata->IsReadOnly()) continue;
			_properties.push_back({
				metadata->Name(), metadata->ValueKind(),
				metadata->CanRead(), metadata->CanWrite(), metadata->CanObserve(),
				DesignerDataContextSchemaUtils::ObjectKindForValueType(
					metadata->ValueType()), metadata->Flags(),
				metadata->DefaultUpdateMode(), metadata->IsReadOnly() });
		}
	}
	std::vector<std::wstring> propertyNames;
	propertyNames.reserve(_properties.size());
	for (const auto& metadata : _properties)
		propertyNames.push_back(metadata.Name);
	_targetProperty->Items = propertyNames;

	_targetProperty->OnSelectionChanged += [this](Control*) {
		if (!_loadingEditor) LoadSelectedBinding();
	};
	_sourceObject->OnSelectionChanged += [this](Control*) {
		if (_loadingEditor) return;
		RefreshAncestorState();
		RefreshKnownSourcePaths();
		RefreshCapabilities();
		RefreshConverterOptions(CurrentConverterName());
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_ancestorType->OnTextChanged += [this](Control*, std::wstring, std::wstring) {
		if (_loadingEditor) return;
		RefreshKnownSourcePaths();
		RefreshCapabilities();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_ancestorLevel->OnValueChanged += [this](NumericUpDown*, double, double) {
		if (_loadingEditor) return;
		RefreshKnownSourcePaths();
		RefreshCapabilities();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_useFallbackValue->OnChecked += [this](Control*) {
		if (_loadingEditor) return;
		RefreshOptionalValueState();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_useTargetNullValue->OnChecked += [this](Control*) {
		if (_loadingEditor) return;
		RefreshOptionalValueState();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_fallbackValue->OnTextChanged += [this](Control*, std::wstring, std::wstring) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_targetNullValue->OnTextChanged += [this](Control*, std::wstring, std::wstring) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_useConverterParameter->OnChecked += [this](Control*) {
		if (_loadingEditor) return;
		RefreshOptionalValueState();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_useStringFormat->OnChecked += [this](Control*) {
		if (_loadingEditor) return;
		RefreshOptionalValueState();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_converterParameter->OnTextChanged += [this](Control*, std::wstring, std::wstring) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_stringFormat->OnTextChanged += [this](Control*, std::wstring, std::wstring) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_knownSourcePath->OnSelectionChanged += [this](Control*) {
		if (_loadingEditor || _knownSourcePath->Text == kManualSourcePath) return;
		_sourcePath->Text = _knownSourcePath->Text;
		RefreshCapabilities();
		RefreshConverterOptions(CurrentConverterName());
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_mode->OnSelectionChanged += [this](Control*) {
		if (_loadingEditor) return;
		BindingMode mode = BindingMode::OneWay;
		if (DesignerBindingUtils::TryParseBindingMode(_mode->Text, mode))
			RefreshUpdateModeOptions(DataSourceUpdateMode::Default);
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_updateMode->OnSelectionChanged += [this](Control*) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_converter->OnSelectionChanged += [this](Control*) {
		if (_loadingEditor) return;
		RefreshCustomConverterState();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_customConverter->OnTextChanged += [this](Control*, std::wstring, std::wstring) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_sourcePath->OnTextChanged += [this](Control*, std::wstring, std::wstring) {
		if (_loadingEditor) return;
		SelectKnownSourcePath(_sourcePath->Text);
		RefreshCapabilities();
		RefreshConverterOptions(CurrentConverterName());
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};

	_saveBinding->OnMouseClick += [this](Control*, MouseEventArgs) {
		(void)SaveCurrentBinding();
	};
	_removeBinding->OnMouseClick += [this](Control*, MouseEventArgs) {
		RemoveCurrentBinding();
	};
	_ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		const auto* metadata = SelectedMetadata();
		if (metadata && !_loadedMultiBinding)
		{
			const auto sourcePath = DesignerBindingUtils::Trim(_sourcePath->Text);
			if (!sourcePath.empty())
			{
				if (!SaveCurrentBinding()) return;
			}
			else if (ResultBindings.find(metadata->Name) != ResultBindings.end())
			{
				ResultBindings.erase(metadata->Name);
			}
		}
		Applied = true;
		this->Close();
	};
	_cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
		Applied = false;
		this->Close();
	};

	if (!_properties.empty())
	{
		_targetProperty->SelectedIndex = 0;
		_targetProperty->Text = _properties.front().Name;
		LoadSelectedBinding();
	}
	else
	{
		_targetProperty->Enable = false;
		_sourceObject->Enable = false;
		_sourcePath->Enable = false;
		_knownSourcePath->Enable = false;
		_mode->Enable = false;
		_updateMode->Enable = false;
		_converter->Enable = false;
		_customConverter->Enable = false;
		_ancestorType->Enable = false;
		_ancestorLevel->Enable = false;
		_useFallbackValue->Enable = false;
		_useTargetNullValue->Enable = false;
		_useConverterParameter->Enable = false;
		_useStringFormat->Enable = false;
		_fallbackValue->Enable = false;
		_targetNullValue->Enable = false;
		_converterParameter->Enable = false;
		_stringFormat->Enable = false;
		_saveBinding->Enable = false;
		_removeBinding->Enable = false;
		ShowValidation(L"该控件没有公开可绑定属性。", true);
	}
	RefreshAncestorState();
	RefreshOptionalValueState();
	RefreshSummary();
}

const DesignerBindingUtils::TargetMetadata*
BindingEditorDialog::SelectedMetadata() const
{
	if (!_targetProperty) return nullptr;
	const int index = _targetProperty->SelectedIndex;
	if (index < 0 || index >= static_cast<int>(_properties.size())) return nullptr;
	return &_properties[static_cast<size_t>(index)];
}

void BindingEditorDialog::SelectComboValue(ComboBox* combo, const std::wstring& value)
{
	if (!combo) return;
	auto& items = combo->Items;
	auto it = std::find(items.begin(), items.end(), value);
	const int index = it == items.end() ? 0 : static_cast<int>(it - items.begin());
	combo->SelectedIndex = index;
	combo->Text = items.empty() ? L"" : items[static_cast<size_t>(index)];
}

IBindingSource* BindingEditorDialog::CurrentRuntimeSource() const
{
	if (CurrentRelativeSource() == DesignerBindingRelativeSource::Self)
		return _target;
	if (CurrentRelativeSource() == DesignerBindingRelativeSource::FindAncestor)
	{
		if (!_target) return nullptr;
		DesignerDataBinding binding;
		binding.RelativeSource = DesignerBindingRelativeSource::FindAncestor;
		binding.AncestorType = _ancestorType ? _ancestorType->Text : L"";
		binding.AncestorLevel = _ancestorLevel
			? static_cast<int>(_ancestorLevel->Value) : 1;
		const auto separator = binding.AncestorType.find(L':');
		const auto localName = separator == std::wstring::npos
			? binding.AncestorType
			: binding.AncestorType.substr(separator + 1);
		for (auto* ancestor = _target->Parent; ancestor; ancestor = ancestor->Parent)
		{
			if (!ancestor->GetDeclarativeTypeNamespace().empty()
				&& _wcsicmp(ancestor->GetDeclarativeTypeName().c_str(),
					localName.c_str()) == 0)
			{
				binding.AncestorTypeNamespace =
					ancestor->GetDeclarativeTypeNamespace();
				break;
			}
		}
		return DesignerBindingUtils::FindAncestorSource(*_target, binding);
	}
	const auto elementName = CurrentElementName();
	if (elementName.empty()) return _runtimeSource;
	const auto found = std::find_if(
		_elementSources.begin(), _elementSources.end(), [&](const auto& source)
		{
			return _wcsicmp(source.Name.c_str(), elementName.c_str()) == 0;
		});
	return found == _elementSources.end() ? nullptr : found->Source;
}

DesignerDataContextSchema BindingEditorDialog::CurrentSourceSchema() const
{
	if (CurrentRelativeSource() == DesignerBindingRelativeSource::Self)
		return _target ? DesignerBindingUtils::BuildSourceSchema(*_target)
			: DesignerDataContextSchema{};
	if (CurrentRelativeSource() == DesignerBindingRelativeSource::FindAncestor)
	{
		const auto* source = CurrentRuntimeSource();
		return source ? DesignerBindingUtils::BuildSourceSchema(*source)
			: DesignerDataContextSchema{};
	}
	if (CurrentElementName().empty()) return _sourceSchema;
	const auto* source = CurrentRuntimeSource();
	return source ? DesignerBindingUtils::BuildSourceSchema(*source)
		: DesignerDataContextSchema{};
}

std::wstring BindingEditorDialog::CurrentElementName() const
{
	if (!_sourceObject || _sourceObject->Text == kDataContextSource
		|| _sourceObject->Text == kSelfSource
		|| _sourceObject->Text == kFindAncestorSource) return {};
	return DesignerBindingUtils::Trim(_sourceObject->Text);
}

DesignerBindingRelativeSource BindingEditorDialog::CurrentRelativeSource() const
{
	if (!_sourceObject) return DesignerBindingRelativeSource::None;
	if (_sourceObject->Text == kSelfSource)
		return DesignerBindingRelativeSource::Self;
	if (_sourceObject->Text == kFindAncestorSource)
		return DesignerBindingRelativeSource::FindAncestor;
	return DesignerBindingRelativeSource::None;
}

void BindingEditorDialog::RefreshAncestorState()
{
	const bool enabled = CurrentRelativeSource()
		== DesignerBindingRelativeSource::FindAncestor;
	if (_ancestorType) _ancestorType->Enable = enabled;
	if (_ancestorLevel) _ancestorLevel->Enable = enabled;
}

void BindingEditorDialog::RefreshOptionalValueState()
{
	if (_fallbackValue)
		_fallbackValue->Enable = _useFallbackValue
			&& _useFallbackValue->Checked;
	if (_targetNullValue)
		_targetNullValue->Enable = _useTargetNullValue
			&& _useTargetNullValue->Checked;
	if (_converterParameter)
		_converterParameter->Enable = _useConverterParameter
			&& _useConverterParameter->Checked;
	if (_stringFormat)
		_stringFormat->Enable = _useStringFormat
			&& _useStringFormat->Checked;
}

void BindingEditorDialog::RefreshKnownSourcePaths()
{
	if (!_knownSourcePath) return;
	const auto currentPath = _sourcePath ? _sourcePath->Text : std::wstring{};
	const auto schema = CurrentSourceSchema();
	auto& items = _knownSourcePath->Items;
	items.clear();
	items.push_back(kManualSourcePath);
	for (const auto& property : schema)
	{
		const auto path = DesignerDataContextSchemaUtils::NormalizePath(
			property.Path);
		items.push_back(path);
		if (property.ObjectKind == DesignerDataObjectKind::BindingList)
			items.push_back(path + L"[0]");
	}
	_knownSourcePath->Enable = !schema.empty();
	SelectKnownSourcePath(currentPath);
}

void BindingEditorDialog::LoadSelectedBinding()
{
	const auto* metadata = SelectedMetadata();
	if (!metadata) return;

	_loadingEditor = true;
	_loadedMultiBinding = false;
	_sourceObject->Enable = true;
	_sourcePath->Enable = true;
	_knownSourcePath->Enable = true;
	_ancestorType->Enable = true;
	_ancestorLevel->Enable = true;
	_useFallbackValue->Enable = true;
	_useTargetNullValue->Enable = true;
	_useConverterParameter->Enable = true;
	_useStringFormat->Enable = true;
	_mode->Enable = true;
	_converter->Enable = true;
	_saveBinding->Enable = true;
	DesignerDataBinding binding;
	auto it = ResultBindings.find(metadata->Name);
	if (it != ResultBindings.end()) binding = it->second;
	if (binding.IsMultiBinding())
	{
		_loadedMultiBinding = true;
		_sourcePath->Text = L"<MultiBinding: "
			+ std::to_wstring(binding.ChildBindings.size()) + L" 个源>";
		for (auto* editor : std::initializer_list<Control*>{
			_sourceObject, _sourcePath, _knownSourcePath, _ancestorType,
			_ancestorLevel, _useFallbackValue, _fallbackValue,
			_useTargetNullValue, _targetNullValue, _useConverterParameter,
			_converterParameter, _useStringFormat, _stringFormat, _mode,
			_updateMode, _converter, _customConverter, _saveBinding })
			editor->Enable = false;
		_removeBinding->Enable = true;
		_loadingEditor = false;
		ShowValidation(L"MultiBinding 已保留；当前请在 XAML 编辑器中修改，或先删除后改为普通 Binding。", false);
		return;
	}
	SelectComboValue(_sourceObject,
		binding.RelativeSource == DesignerBindingRelativeSource::Self
			? kSelfSource
			: binding.RelativeSource == DesignerBindingRelativeSource::FindAncestor
				? kFindAncestorSource
			: binding.ElementName.empty() ? kDataContextSource : binding.ElementName);
	_ancestorType->Text = binding.AncestorType;
	_ancestorLevel->Value = binding.AncestorLevel;
	_useFallbackValue->Checked = binding.FallbackValue.has_value();
	_fallbackValue->Text = binding.FallbackValue
		? binding.FallbackValue->Text : L"";
	_useTargetNullValue->Checked = binding.TargetNullValue.has_value();
	_targetNullValue->Text = binding.TargetNullValue
		? binding.TargetNullValue->Text : L"";
	_useConverterParameter->Checked = binding.ConverterParameter.has_value();
	_converterParameter->Text = binding.ConverterParameter
		? binding.ConverterParameter->Text : L"";
	_useStringFormat->Checked = binding.StringFormat.has_value();
	_stringFormat->Text = binding.StringFormat.value_or(L"");
	RefreshAncestorState();
	RefreshOptionalValueState();
	RefreshKnownSourcePaths();
	_sourcePath->Text = it == ResultBindings.end() ? L"" : binding.SourceProperty;
	SelectKnownSourcePath(_sourcePath->Text);
	RefreshModeOptions(binding.Mode);
	RefreshUpdateModeOptions(binding.UpdateMode);
	RefreshConverterOptions(binding.Converter);
	RefreshCapabilities();
	_loadingEditor = false;
	ShowValidation(it == ResultBindings.end()
		? L"尚未为此属性创建绑定。"
		: L"已加载现有绑定。", false);
}

void BindingEditorDialog::RefreshModeOptions(BindingMode preferredMode)
{
	const auto* metadata = SelectedMetadata();
	std::vector<std::wstring> names;
	if (metadata)
	{
		for (const auto& name : AllBindingModeNames())
		{
			BindingMode mode = BindingMode::OneWay;
			if (DesignerBindingUtils::TryParseBindingMode(name, mode)
				&& DesignerBindingUtils::IsModeStructurallyCompatible(*metadata, mode))
				names.push_back(name);
		}
	}
	_mode->Items = names;
	SelectComboValue(_mode, DesignerBindingUtils::BindingModeName(preferredMode));
}

void BindingEditorDialog::RefreshUpdateModeOptions(DataSourceUpdateMode preferredMode)
{
	const auto* metadata = SelectedMetadata();
	BindingMode mode = BindingMode::OneWay;
	(void)DesignerBindingUtils::TryParseBindingMode(_mode->Text, mode);
	if (metadata)
		mode = DesignerBindingUtils::ResolveBindingMode(*metadata, mode);

	std::vector<std::wstring> names;
	if (!IsTargetToSourceMode(mode))
	{
		names.push_back(L"Default");
		preferredMode = DataSourceUpdateMode::Default;
		_updateMode->Enable = false;
	}
	else if (metadata && !metadata->CanObserve)
	{
		if (metadata->DefaultUpdateMode == DataSourceUpdateMode::Never)
			names = { L"Default", L"Explicit" };
		else
		{
			names.push_back(L"Explicit");
			preferredMode = DataSourceUpdateMode::Never;
		}
		_updateMode->Enable = names.size() > 1;
	}
	else
	{
		names = AllUpdateModeNames();
		_updateMode->Enable = true;
	}
	_updateMode->Items = names;
	SelectComboValue(_updateMode,
		DesignerBindingUtils::UpdateSourceTriggerName(preferredMode));
}

void BindingEditorDialog::RefreshConverterOptions(const std::wstring& preferredConverter)
{
	std::vector<std::wstring> names{ kNoConverter };
	const auto* targetMetadata = SelectedMetadata();
	const auto sourceSchema = CurrentSourceSchema();
	const auto* sourceMetadata = DesignerDataContextSchemaUtils::Find(
		sourceSchema, _sourcePath ? _sourcePath->Text : L"");
	for (const auto& converter : BindingValueConverterRegistry::GetConverters())
	{
		const bool targetCompatible = !targetMetadata
			|| converter.TargetKind == BindingValueKind::Empty
		|| converter.TargetKind == targetMetadata->ValueKind;
		const bool sourceCompatible = !sourceMetadata
			|| sourceMetadata->ValueKind == BindingValueKind::Empty
			|| converter.SourceKind == BindingValueKind::Empty
			|| converter.SourceKind == sourceMetadata->ValueKind;
		if (targetCompatible && sourceCompatible)
			names.push_back(converter.Name);
	}
	names.push_back(kCustomConverter);
	_converter->Items = names;

	const auto normalized = DesignerBindingUtils::Trim(preferredConverter);
	if (normalized.empty())
	{
		SelectComboValue(_converter, kNoConverter);
		_customConverter->Text = L"";
	}
	else
	{
		const auto registered = BindingValueConverterRegistry::Find(normalized);
		const auto registeredName = registered ? registered->Name : normalized;
		auto& items = _converter->Items;
		if (std::find(items.begin(), items.end(), registeredName) != items.end())
		{
			SelectComboValue(_converter, registeredName);
			_customConverter->Text = L"";
		}
		else
		{
			SelectComboValue(_converter, kCustomConverter);
			_customConverter->Text = normalized;
		}
	}
	RefreshCustomConverterState();
}

std::wstring BindingEditorDialog::CurrentConverterName() const
{
	if (!_converter || _converter->Text == kNoConverter) return L"";
	if (_converter->Text == kCustomConverter)
		return _customConverter ? _customConverter->Text : L"";
	return _converter->Text;
}

void BindingEditorDialog::SelectKnownSourcePath(const std::wstring& path)
{
	if (!_knownSourcePath) return;
	const auto normalized = DesignerDataContextSchemaUtils::NormalizePath(path);
	auto& items = _knownSourcePath->Items;
	auto it = std::find_if(items.begin(), items.end(),
		[&](const std::wstring& candidate)
		{
			return candidate != kManualSourcePath
				&& _wcsicmp(candidate.c_str(), normalized.c_str()) == 0;
		});
	const int index = it == items.end() ? 0 : static_cast<int>(it - items.begin());
	_knownSourcePath->SelectedIndex = index;
	_knownSourcePath->Text = items.empty() ? L"" : items[static_cast<size_t>(index)];
}

void BindingEditorDialog::RefreshCustomConverterState()
{
	const bool custom = _converter && _converter->Text == kCustomConverter;
	if (_customConverter) _customConverter->Enable = custom;
}

void BindingEditorDialog::RefreshCapabilities()
{
	AttachRuntimeValidation();
	const auto* metadata = SelectedMetadata();
	if (!metadata)
	{
		_capabilities->Text = L"";
		RefreshRuntimeValidation();
		return;
	}
	_capabilities->Text = L"值类型: "
		+ std::wstring(DesignerBindingUtils::ValueKindName(metadata->ValueKind))
		+ L"    可读: " + YesNo(metadata->CanRead)
		+ L"    可写: " + YesNo(metadata->CanWrite)
		+ L"    可通知: " + YesNo(metadata->CanObserve)
		+ L"    默认模式: " + std::wstring(
			DesignerBindingUtils::BindingModeName(
				DesignerBindingUtils::ResolveBindingMode(
					*metadata, BindingMode::Default)))
		+ L"    默认更新: " + std::wstring(
			DesignerBindingUtils::UpdateSourceTriggerName(
				metadata->DefaultUpdateMode));
	const auto sourceSchema = CurrentSourceSchema();
	if (const auto* source = DesignerDataContextSchemaUtils::Find(
		sourceSchema, _sourcePath ? _sourcePath->Text : L""))
	{
		_capabilities->Text += L"    源类型: "
			+ std::wstring(DesignerDataContextSchemaUtils::ValueKindName(source->ValueKind));
	}
	RefreshRuntimeValidation();
}

void BindingEditorDialog::AttachRuntimeValidation()
{
	_runtimeValidationConnections.clear();
	_runtimePathConnections.clear();
	_runtimePathOwners.clear();
	_runtimePathListOwners.clear();
	auto* runtimeSource = CurrentRuntimeSource();
	if (!runtimeSource || !_sourcePath) return;

	const auto sourcePath = DesignerBindingUtils::Trim(_sourcePath->Text);
	if (!DesignerBindingUtils::IsValidSourcePath(sourcePath)) return;
	std::vector<BindingPathStep> steps;
	if (!TryParseBindingPropertyPath(sourcePath, steps)) return;

	IBindingSource* currentSource = runtimeSource;
	IBindingList* currentList = nullptr;
	for (size_t index = 0; index < steps.size(); ++index)
	{
		const auto expectedProperty = steps[index].Value;
		if (currentSource)
		{
			if (auto* validationChanged = currentSource->ValidationChanged())
			{
				auto connection = validationChanged->Subscribe(
					[this, expectedProperty](const BindingValidationChangedEventArgs& e)
					{
						if (!e.PropertyName.empty()
							&& _wcsicmp(e.PropertyName.c_str(), expectedProperty.c_str()) != 0)
							return;
						RefreshRuntimeValidation();
					});
				if (connection.Connected())
					_runtimeValidationConnections.push_back(std::move(connection));
			}
		}
		if (index + 1 == steps.size()) break;

		EventConnection pathConnection;
		if (currentSource)
			pathConnection = currentSource->PropertyChanged().Subscribe(
				[this, expectedProperty](const PropertyChangedEventArgs& e)
				{
					if (!e.PropertyName.empty()
						&& _wcsicmp(e.PropertyName.c_str(), expectedProperty.c_str()) != 0)
						return;
					AttachRuntimeValidation();
					RefreshRuntimeValidation();
				});
		else if (currentList)
			pathConnection = currentList->SubscribeChanged(
				[this](const CollectionChangedEventArgs&)
				{
					AttachRuntimeValidation();
					RefreshRuntimeValidation();
				});
		if (pathConnection.Connected())
			_runtimePathConnections.push_back(std::move(pathConnection));

		BindingValue value;
		if (currentSource)
		{
			if (!currentSource->TryGetValue(expectedProperty, value)) break;
		}
		else
		{
			if (!currentList || steps[index].Kind != BindingPathStepKind::Indexer
				|| expectedProperty.empty()
				|| !std::all_of(expectedProperty.begin(), expectedProperty.end(),
					[](wchar_t ch) { return std::iswdigit(ch) != 0; })) break;
			size_t itemIndex = 0;
			try { itemIndex = static_cast<size_t>(std::stoull(expectedProperty)); }
			catch (...) { break; }
			BindingSourceReference item;
			if (!currentList->TryGetItem(itemIndex, item) || !item) break;
			value = BindingValue(item);
		}

		BindingSourceReference sourceReference;
		if (value.TryGet(sourceReference) && sourceReference)
		{
			_runtimePathOwners.push_back(sourceReference.Shared());
			currentSource = sourceReference.Get();
			currentList = nullptr;
			continue;
		}
		BindingListReference listReference;
		if (value.TryGet(listReference) && listReference)
		{
			_runtimePathListOwners.push_back(listReference.Shared());
			currentSource = nullptr;
			currentList = listReference.Get();
			continue;
		}
		break;
	}
}

void BindingEditorDialog::RefreshRuntimeValidation()
{
	if (!_runtimeValidation) return;
	const auto sourcePath = DesignerBindingUtils::Trim(
		_sourcePath ? _sourcePath->Text : L"");
	auto* runtimeSource = CurrentRuntimeSource();
	if (!runtimeSource || sourcePath.empty())
	{
		_runtimeValidation->Text = runtimeSource
			? L"运行时校验：填写源路径后显示活动问题。"
			: L"运行时校验：未连接设计时数据源。";
		_runtimeValidation->ForeColor = Colors::DimGrey;
		_runtimeValidation->InvalidateVisual();
		return;
	}
	if (!DesignerBindingUtils::IsValidSourcePath(sourcePath))
	{
		_runtimeValidation->Text = L"运行时校验：源路径无效，无法查询。";
		_runtimeValidation->ForeColor = Colors::DimGrey;
		_runtimeValidation->InvalidateVisual();
		return;
	}

	const auto issues = GetBindingValidationIssuesForPath(
		*runtimeSource, sourcePath);
	if (issues.empty())
	{
		_runtimeValidation->Text = L"运行时校验：当前没有活动问题。";
		_runtimeValidation->ForeColor = Colors::DimGrey;
		_runtimeValidation->InvalidateVisual();
		return;
	}

	std::wstring text = L"运行时校验：";
	const size_t visibleCount = (std::min)(issues.size(), size_t{ 2 });
	bool hasError = false;
	for (size_t index = 0; index < issues.size(); ++index)
		hasError = hasError
			|| issues[index].Severity == BindingValidationSeverity::Error;
	for (size_t index = 0; index < visibleCount; ++index)
	{
		if (index != 0) text += L"；";
		text += L"[" + std::wstring(ValidationSeverityName(issues[index].Severity))
			+ L"] " + issues[index].Message;
		if (!issues[index].Code.empty()) text += L" (" + issues[index].Code + L")";
	}
	if (issues.size() > visibleCount)
		text += L"；另有 " + std::to_wstring(issues.size() - visibleCount) + L" 项";
	_runtimeValidation->Text = std::move(text);
	_runtimeValidation->ForeColor = hasError ? Colors::Red : Colors::DimGrey;
	_runtimeValidation->InvalidateVisual();
}

void BindingEditorDialog::RefreshSummary()
{
	if (!_summary) return;
	if (ResultBindings.empty())
	{
		_summary->Text = L"（无数据绑定）";
		return;
	}

	std::wstring text;
	for (const auto& [targetProperty, binding] : ResultBindings)
	{
		if (!text.empty()) text += L"\r\n";
		text += DesignerBindingUtils::Describe(targetProperty, binding);
	}
	_summary->Text = std::move(text);
}

void BindingEditorDialog::ShowValidation(const std::wstring& message, bool isError)
{
	if (!_validation) return;
	_validation->Text = message;
	_validation->ForeColor = isError ? Colors::Red : Colors::DimGrey;
	_validation->InvalidateVisual();
}

bool BindingEditorDialog::TryReadEditor(
	std::wstring& targetProperty,
	DesignerDataBinding& binding,
	std::wstring& error) const
{
	const auto* metadata = SelectedMetadata();
	if (!_target || !metadata)
	{
		error = L"请选择目标属性。";
		return false;
	}

	targetProperty = metadata->Name;
	binding.SourceProperty = DesignerBindingUtils::Trim(_sourcePath->Text);
	binding.ElementName = CurrentElementName();
	binding.RelativeSource = CurrentRelativeSource();
	if (binding.RelativeSource == DesignerBindingRelativeSource::FindAncestor)
	{
		binding.AncestorType = DesignerBindingUtils::Trim(_ancestorType->Text);
		binding.AncestorLevel = static_cast<int>(_ancestorLevel->Value);
		if (binding.AncestorType.empty())
		{
			error = L"FindAncestor 必须填写 AncestorType。";
			return false;
		}
		const auto existing = ResultBindings.find(targetProperty);
		if (existing != ResultBindings.end()
			&& _wcsicmp(existing->second.AncestorType.c_str(),
				binding.AncestorType.c_str()) == 0)
			binding.AncestorTypeNamespace =
				existing->second.AncestorTypeNamespace;
		if (binding.AncestorTypeNamespace.empty())
		{
			const auto separator = binding.AncestorType.find(L':');
			const auto localName = separator == std::wstring::npos
				? binding.AncestorType
				: binding.AncestorType.substr(separator + 1);
			for (auto* ancestor = _target->Parent; ancestor; ancestor = ancestor->Parent)
			{
				if (!ancestor->GetDeclarativeTypeNamespace().empty()
					&& _wcsicmp(ancestor->GetDeclarativeTypeName().c_str(),
						localName.c_str()) == 0)
				{
					binding.AncestorTypeNamespace =
						ancestor->GetDeclarativeTypeNamespace();
					break;
				}
			}
		}
	}
	DesignerStyleValueKind targetLiteralKind{};
	const auto* targetRuntimeMetadata = _target->FindPropertyMetadata(
		targetProperty);
	if ((_useFallbackValue->Checked || _useTargetNullValue->Checked)
		&& (!targetRuntimeMetadata
			|| !DesignerPropertyCatalog::TryGetStyleValueKind(
				*targetRuntimeMetadata, targetLiteralKind)))
	{
		error = L"该目标属性类型暂不支持 Binding 缺省值。";
		return false;
	}
	if (_useFallbackValue->Checked)
		binding.FallbackValue = DesignerStyleValue{
			targetLiteralKind, _fallbackValue->Text };
	if (_useTargetNullValue->Checked)
		binding.TargetNullValue = DesignerStyleValue{
			targetLiteralKind, _targetNullValue->Text };
	if (_useConverterParameter->Checked)
		binding.ConverterParameter = DesignerStyleValue{
			DesignerStyleValueKind::String, _converterParameter->Text };
	if (_useStringFormat->Checked)
		binding.StringFormat = _stringFormat->Text;
	if (!DesignerBindingUtils::TryParseBindingMode(_mode->Text, binding.Mode))
	{
		error = L"请选择有效的绑定模式。";
		return false;
	}
	if (!DesignerBindingUtils::TryParseUpdateMode(_updateMode->Text, binding.UpdateMode))
	{
		error = L"请选择有效的更新策略。";
		return false;
	}
	if (_converter->Text == kCustomConverter)
	{
		binding.Converter = DesignerBindingUtils::Trim(_customConverter->Text);
		if (binding.Converter.empty())
		{
			error = L"请输入自定义 Converter ID。";
			return false;
		}
	}
	else if (_converter->Text != kNoConverter)
	{
		binding.Converter = _converter->Text;
	}
	const auto sourceSchema = CurrentSourceSchema();
	return DesignerBindingUtils::Validate(
		*_target, targetProperty, binding, nullptr, &error,
		sourceSchema.empty() ? nullptr : &sourceSchema);
}

bool BindingEditorDialog::SaveCurrentBinding()
{
	if (_loadedMultiBinding)
	{
		ShowValidation(L"MultiBinding 请在 XAML 编辑器中修改。", true);
		return false;
	}
	std::wstring targetProperty;
	DesignerDataBinding binding;
	std::wstring error;
	if (!TryReadEditor(targetProperty, binding, error))
	{
		ShowValidation(error, true);
		return false;
	}

	ResultBindings[targetProperty] = std::move(binding);
	RefreshSummary();
	ShowValidation(L"绑定已暂存；点击“确定”写入设计文档。", false);
	return true;
}

void BindingEditorDialog::RemoveCurrentBinding()
{
	const auto* metadata = SelectedMetadata();
	if (!metadata) return;
	const size_t removed = ResultBindings.erase(metadata->Name);
	_loadedMultiBinding = false;
	_sourcePath->Text = L"";
	LoadSelectedBinding();
	RefreshSummary();
	ShowValidation(removed ? L"绑定已删除；点击“确定”写入设计文档。"
		: L"该属性当前没有绑定。", false);
}
