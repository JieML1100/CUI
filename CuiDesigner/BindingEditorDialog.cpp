#include "BindingEditorDialog.h"
#include "ProgrammaticControlFactory.h"
#include "../CUI/include/BindingList.h"
#include "../CuiRuntime/include/BindingConverterRegistry.h"
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
		case DesignerStyleValueKind::NullableBool:
			return BindingValueKind::NullableBool;
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
	: Window(),
	  ResultBindings(bindings),
	  _target(target),
	  _runtimeSource(runtimeSource),
	  _sourceSchema(sourceSchema),
	  _elementSources(std::move(elementSources))
{
	this->Title = L"编辑数据绑定";
	this->Left = 320.0f;
	this->Top = 100.0f;
	this->Width = 820.0f;
	this->Height = 850.0f;
	DesignerDataContextSchemaUtils::Canonicalize(_sourceSchema);
	this->ResizeMode = ::ResizeMode::NoResize;
	this->Background = Colors::WhiteSmoke;
	auto contentOwner = std::make_unique<Panel>();
	contentOwner->BorderThickness = 0.0f;
	contentOwner->Background = D2D1_COLOR_F{ 0, 0, 0, 0 };
	auto* contentRoot = static_cast<Panel*>(SetVisualContent(std::move(contentOwner)));
	auto addContent = [contentRoot](auto* child) { return contentRoot->AdoptVisualChild(child); };

	auto tip = addContent(cui::designer::NewControl<Label>(
		L"选择目标属性，填写数据上下文中的源路径；修改后点击“保存绑定”。", 20, 16));
	tip->Width = 770.0f;
	tip->Height = 22.0f;

	auto targetLabel = addContent(cui::designer::NewControl<Label>(L"目标属性", 20, 60));
	targetLabel->Width = 120.0f;
	targetLabel->Height = 24.0f;
	_targetProperty = addContent(cui::designer::NewControl<ComboBox>(L"", 150, 54, 630, 30));
	_targetProperty->MaxDropDownHeight = 280.0f;

	auto sourceLabel = addContent(cui::designer::NewControl<Label>(L"源对象 / 路径", 20, 106));
	sourceLabel->Width = 120.0f;
	sourceLabel->Height = 24.0f;
	_sourceObject = addContent(cui::designer::NewControl<ComboBox>(L"", 150, 100, 190, 30));
	_sourceObject->MaxDropDownHeight = 280.0f;
	cui::designer::AddComboBoxItem(*_sourceObject, kDataContextSource);
	cui::designer::AddComboBoxItem(*_sourceObject, kSelfSource);
	cui::designer::AddComboBoxItem(*_sourceObject, kFindAncestorSource);
	for (const auto& source : _elementSources)
		if (!source.Name.empty())
			cui::designer::AddComboBoxItem(*_sourceObject, source.Name);
	_sourceObject->SelectedIndex = 0;
	_sourceObject->Text = kDataContextSource;
	_sourcePath = addContent(cui::designer::NewControl<TextBox>(L"", 350, 100, 200, 30));
	_knownSourcePath = addContent(cui::designer::NewControl<ComboBox>(L"", 560, 100, 220, 30));
	_knownSourcePath->MaxDropDownHeight = 280.0f;
	RefreshKnownSourcePaths();

	auto modeLabel = addContent(cui::designer::NewControl<Label>(L"绑定模式", 20, 152));
	modeLabel->Width = 120.0f;
	modeLabel->Height = 24.0f;
	_mode = addContent(cui::designer::NewControl<ComboBox>(L"", 150, 146, 250, 30));
	_mode->MaxDropDownHeight = 140.0f;

	auto updateLabel = addContent(cui::designer::NewControl<Label>(L"更新策略", 430, 152));
	updateLabel->Width = 100.0f;
	updateLabel->Height = 24.0f;
	_updateMode = addContent(cui::designer::NewControl<ComboBox>(L"", 530, 146, 250, 30));
	_updateMode->MaxDropDownHeight = 84.0f;

	auto converterLabel = addContent(cui::designer::NewControl<Label>(L"Converter", 20, 198));
	converterLabel->Width = 120.0f;
	converterLabel->Height = 24.0f;
	_converter = addContent(cui::designer::NewControl<ComboBox>(L"", 150, 192, 250, 30));
	_converter->MaxDropDownHeight = 224.0f;
	auto customConverterLabel = addContent(cui::designer::NewControl<Label>(L"自定义 ID", 430, 198));
	customConverterLabel->Width = 100.0f;
	customConverterLabel->Height = 24.0f;
	_customConverter = addContent(cui::designer::NewControl<TextBox>(L"", 530, 192, 250, 30));

	_useFallbackValue = addContent(cui::designer::NewControl<CheckBox>(
		L"FallbackValue", 20, 238));
	_useFallbackValue->Width = 120.0f;
	_useFallbackValue->Height = 30.0f;
	_fallbackValue = addContent(cui::designer::NewControl<TextBox>(L"", 150, 238, 250, 30));
	_useTargetNullValue = addContent(cui::designer::NewControl<CheckBox>(
		L"TargetNullValue", 430, 238));
	_useTargetNullValue->Width = 120.0f;
	_useTargetNullValue->Height = 30.0f;
	_targetNullValue = addContent(cui::designer::NewControl<TextBox>(L"", 560, 238, 220, 30));

	_useConverterParameter = addContent(cui::designer::NewControl<CheckBox>(
		L"ConverterParameter", 20, 284));
	_useConverterParameter->Width = 130.0f;
	_useConverterParameter->Height = 30.0f;
	_converterParameter = addContent(cui::designer::NewControl<TextBox>(L"", 160, 284, 240, 30));
	_useStringFormat = addContent(cui::designer::NewControl<CheckBox>(
		L"StringFormat", 430, 284));
	_useStringFormat->Width = 120.0f;
	_useStringFormat->Height = 30.0f;
	_stringFormat = addContent(cui::designer::NewControl<TextBox>(L"", 560, 284, 220, 30));

	auto ancestorTypeLabel = addContent(cui::designer::NewControl<Label>(L"AncestorType", 20, 336));
	ancestorTypeLabel->Width = 120.0f;
	ancestorTypeLabel->Height = 24.0f;
	_ancestorType = addContent(cui::designer::NewControl<TextBox>(L"", 150, 330, 360, 30));
	auto ancestorLevelLabel = addContent(cui::designer::NewControl<Label>(L"AncestorLevel", 530, 336));
	ancestorLevelLabel->Width = 110.0f;
	ancestorLevelLabel->Height = 24.0f;
	_ancestorLevel = addContent(cui::designer::NewControl<NumericUpDown>(650, 330, 130, 30));
	_ancestorLevel->Minimum = 1;
	_ancestorLevel->Maximum = 100;
	_ancestorLevel->Value = 1;

	_capabilities = addContent(cui::designer::NewControl<Label>(L"", 20, 376));
	_capabilities->Width = 760.0f;
	_capabilities->Height = 24.0f;
	_runtimeValidation = addContent(cui::designer::NewControl<Label>(L"", 20, 406));
	_runtimeValidation->Width = 760.0f;
	_runtimeValidation->Height = 40.0f;
	_validation = addContent(cui::designer::NewControl<Label>(L"", 20, 450));
	_validation->Width = 760.0f;
	_validation->Height = 40.0f;

	_saveBinding = addContent(cui::designer::NewControl<Button>(L"保存绑定", 20, 494, 130, 34));
	_removeBinding = addContent(cui::designer::NewControl<Button>(L"删除绑定", 162, 494, 130, 34));

	auto summaryLabel = addContent(cui::designer::NewControl<Label>(L"当前绑定", 20, 550));
	summaryLabel->Width = 120.0f;
	summaryLabel->Height = 24.0f;
	_summary = addContent(cui::designer::NewControl<RichTextBox>(L"", 20, 578, 760, 160));
	_summary->IsReadOnly = true;
	_summary->Background = Colors::White;
	_summary->BorderBrush = Colors::White;

	_ok = addContent(cui::designer::NewControl<Button>(L"确定", 20, 756, 120, 36));
	_cancel = addContent(cui::designer::NewControl<Button>(L"取消", 152, 756, 120, 36));

	if (_target)
	{
		for (const auto* metadata : DependencyPropertyRegistry::GetProperties(*_target))
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
	cui::designer::SetComboBoxItems(
		*_targetProperty, std::move(propertyNames));

	_targetProperty->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loadingEditor) LoadSelectedBinding();
	};
	_sourceObject->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (_loadingEditor) return;
		RefreshAncestorState();
		RefreshKnownSourcePaths();
		RefreshCapabilities();
		RefreshConverterOptions(CurrentConverterName());
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_ancestorType->OnTextChanged += [this](Control*, TextChangedEventArgs&) {
		if (_loadingEditor) return;
		RefreshKnownSourcePaths();
		RefreshCapabilities();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_ancestorLevel->ValueChanged += [this](
		Control*, RoutedPropertyChangedEventArgs<double>&) {
		if (_loadingEditor) return;
		RefreshKnownSourcePaths();
		RefreshCapabilities();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	auto optionalValueStateChanged = [this](Control*, RoutedEventArgs&) {
		if (_loadingEditor) return;
		RefreshOptionalValueState();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_useFallbackValue->Checked += optionalValueStateChanged;
	_useFallbackValue->Unchecked += optionalValueStateChanged;
	_useTargetNullValue->Checked += optionalValueStateChanged;
	_useTargetNullValue->Unchecked += optionalValueStateChanged;
	_fallbackValue->OnTextChanged += [this](Control*, TextChangedEventArgs&) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_targetNullValue->OnTextChanged += [this](Control*, TextChangedEventArgs&) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_useConverterParameter->Checked += optionalValueStateChanged;
	_useConverterParameter->Unchecked += optionalValueStateChanged;
	_useStringFormat->Checked += optionalValueStateChanged;
	_useStringFormat->Unchecked += optionalValueStateChanged;
	_converterParameter->OnTextChanged += [this](Control*, TextChangedEventArgs&) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_stringFormat->OnTextChanged += [this](Control*, TextChangedEventArgs&) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_knownSourcePath->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (_loadingEditor || _knownSourcePath->Text == kManualSourcePath) return;
		_sourcePath->Text = _knownSourcePath->Text;
		RefreshCapabilities();
		RefreshConverterOptions(CurrentConverterName());
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_mode->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (_loadingEditor) return;
		BindingMode mode = BindingMode::OneWay;
		if (DesignerBindingUtils::TryParseBindingMode(_mode->Text, mode))
			RefreshUpdateModeOptions(DataSourceUpdateMode::Default);
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_updateMode->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_converter->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (_loadingEditor) return;
		RefreshCustomConverterState();
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_customConverter->OnTextChanged += [this](Control*, TextChangedEventArgs&) {
		if (!_loadingEditor) ShowValidation(L"修改后请点击“保存绑定”。", false);
	};
	_sourcePath->OnTextChanged += [this](Control*, TextChangedEventArgs&) {
		if (_loadingEditor) return;
		SelectKnownSourcePath(_sourcePath->Text);
		RefreshCapabilities();
		RefreshConverterOptions(CurrentConverterName());
		ShowValidation(L"修改后请点击“保存绑定”。", false);
	};

	_saveBinding->Click += [this](Control*, RoutedEventArgs&) {
		(void)SaveCurrentBinding();
	};
	_removeBinding->Click += [this](Control*, RoutedEventArgs&) {
		RemoveCurrentBinding();
	};
	_ok->Click += [this](Control*, RoutedEventArgs&) {
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
	_cancel->Click += [this](Control*, RoutedEventArgs&) {
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
		_targetProperty->IsEnabled = false;
		_sourceObject->IsEnabled = false;
		_sourcePath->IsEnabled = false;
		_knownSourcePath->IsEnabled = false;
		_mode->IsEnabled = false;
		_updateMode->IsEnabled = false;
		_converter->IsEnabled = false;
		_customConverter->IsEnabled = false;
		_ancestorType->IsEnabled = false;
		_ancestorLevel->IsEnabled = false;
		_useFallbackValue->IsEnabled = false;
		_useTargetNullValue->IsEnabled = false;
		_useConverterParameter->IsEnabled = false;
		_useStringFormat->IsEnabled = false;
		_fallbackValue->IsEnabled = false;
		_targetNullValue->IsEnabled = false;
		_converterParameter->IsEnabled = false;
		_stringFormat->IsEnabled = false;
		_saveBinding->IsEnabled = false;
		_removeBinding->IsEnabled = false;
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
	const int found = cui::designer::FindComboBoxItem(*combo, value);
	const int index = found < 0 ? 0 : found;
	combo->SelectedIndex = index;
	combo->Text = combo->ItemCount() == 0
		? L"" : cui::designer::ComboBoxItemText(
			*combo, static_cast<size_t>(index));
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
		for (auto* ancestor = _target->GetRoutedParent(); ancestor;
			ancestor = ancestor->GetRoutedParent())
		{
			if (!ancestor->GetDeclarativeTypeNamespace().empty()
				&& ancestor->GetDeclarativeTypeName() == localName)
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
			return source.Name == elementName;
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
	if (_ancestorType) _ancestorType->IsEnabled = enabled;
	if (_ancestorLevel) _ancestorLevel->IsEnabled = enabled;
}

void BindingEditorDialog::RefreshOptionalValueState()
{
	if (_fallbackValue)
		_fallbackValue->IsEnabled = _useFallbackValue
			&& _useFallbackValue->IsChecked;
	if (_targetNullValue)
		_targetNullValue->IsEnabled = _useTargetNullValue
			&& _useTargetNullValue->IsChecked;
	if (_converterParameter)
		_converterParameter->IsEnabled = _useConverterParameter
			&& _useConverterParameter->IsChecked;
	if (_stringFormat)
		_stringFormat->IsEnabled = _useStringFormat
			&& _useStringFormat->IsChecked;
}

void BindingEditorDialog::RefreshKnownSourcePaths()
{
	if (!_knownSourcePath) return;
	const auto currentPath = _sourcePath ? _sourcePath->Text : std::wstring{};
	const auto schema = CurrentSourceSchema();
	std::vector<std::wstring> items{ kManualSourcePath };
	for (const auto& property : schema)
	{
		const auto path = DesignerDataContextSchemaUtils::NormalizePath(
			property.Path);
		items.push_back(path);
		if (property.ObjectKind == DesignerDataObjectKind::BindingList)
			items.push_back(path + L"[0]");
	}
	cui::designer::SetComboBoxItems(
		*_knownSourcePath, std::move(items));
	_knownSourcePath->IsEnabled = !schema.empty();
	SelectKnownSourcePath(currentPath);
}

void BindingEditorDialog::LoadSelectedBinding()
{
	const auto* metadata = SelectedMetadata();
	if (!metadata) return;

	_loadingEditor = true;
	_loadedMultiBinding = false;
	_sourceObject->IsEnabled = true;
	_sourcePath->IsEnabled = true;
	_knownSourcePath->IsEnabled = true;
	_ancestorType->IsEnabled = true;
	_ancestorLevel->IsEnabled = true;
	_useFallbackValue->IsEnabled = true;
	_useTargetNullValue->IsEnabled = true;
	_useConverterParameter->IsEnabled = true;
	_useStringFormat->IsEnabled = true;
	_mode->IsEnabled = true;
	_converter->IsEnabled = true;
	_saveBinding->IsEnabled = true;
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
			editor->IsEnabled = false;
		_removeBinding->IsEnabled = true;
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
	_useFallbackValue->IsChecked = binding.FallbackValue.has_value();
	_fallbackValue->Text = binding.FallbackValue
		? binding.FallbackValue->Text : L"";
	_useTargetNullValue->IsChecked = binding.TargetNullValue.has_value();
	_targetNullValue->Text = binding.TargetNullValue
		? binding.TargetNullValue->Text : L"";
	_useConverterParameter->IsChecked = binding.ConverterParameter.has_value();
	_converterParameter->Text = binding.ConverterParameter
		? binding.ConverterParameter->Text : L"";
	_useStringFormat->IsChecked = binding.StringFormat.has_value();
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
	cui::designer::SetComboBoxItems(*_mode, std::move(names));
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
		_updateMode->IsEnabled = false;
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
		_updateMode->IsEnabled = names.size() > 1;
	}
	else
	{
		names = AllUpdateModeNames();
		_updateMode->IsEnabled = true;
	}
	cui::designer::SetComboBoxItems(*_updateMode, std::move(names));
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
	cui::designer::SetComboBoxItems(*_converter, std::move(names));

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
		if (cui::designer::FindComboBoxItem(
			*_converter, registeredName) >= 0)
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
	const auto items = cui::designer::ComboBoxItems(*_knownSourcePath);
	auto it = std::find_if(items.begin(), items.end(),
		[&](const std::wstring& candidate)
		{
			return candidate != kManualSourcePath && candidate == normalized;
		});
	const int index = it == items.end() ? 0 : static_cast<int>(it - items.begin());
	_knownSourcePath->SelectedIndex = index;
	_knownSourcePath->Text = items.empty() ? L"" : items[static_cast<size_t>(index)];
}

void BindingEditorDialog::RefreshCustomConverterState()
{
	const bool custom = _converter && _converter->Text == kCustomConverter;
	if (_customConverter) _customConverter->IsEnabled = custom;
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
							&& e.PropertyName != expectedProperty)
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
						&& e.PropertyName != expectedProperty)
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
		_runtimeValidation->Foreground = Colors::DimGrey;
		_runtimeValidation->InvalidateVisual();
		return;
	}
	if (!DesignerBindingUtils::IsValidSourcePath(sourcePath))
	{
		_runtimeValidation->Text = L"运行时校验：源路径无效，无法查询。";
		_runtimeValidation->Foreground = Colors::DimGrey;
		_runtimeValidation->InvalidateVisual();
		return;
	}

	const auto issues = GetBindingValidationIssuesForPath(
		*runtimeSource, sourcePath);
	if (issues.empty())
	{
		_runtimeValidation->Text = L"运行时校验：当前没有活动问题。";
		_runtimeValidation->Foreground = Colors::DimGrey;
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
	_runtimeValidation->Foreground = hasError ? Colors::Red : Colors::DimGrey;
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
	_validation->Foreground = isError ? Colors::Red : Colors::DimGrey;
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
			&& existing->second.AncestorType == binding.AncestorType)
			binding.AncestorTypeNamespace =
				existing->second.AncestorTypeNamespace;
		if (binding.AncestorTypeNamespace.empty())
		{
			const auto separator = binding.AncestorType.find(L':');
			const auto localName = separator == std::wstring::npos
				? binding.AncestorType
				: binding.AncestorType.substr(separator + 1);
			for (auto* ancestor = _target->GetRoutedParent(); ancestor;
				ancestor = ancestor->GetRoutedParent())
			{
				if (!ancestor->GetDeclarativeTypeNamespace().empty()
					&& ancestor->GetDeclarativeTypeName() == localName)
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
	if ((_useFallbackValue->IsChecked || _useTargetNullValue->IsChecked)
		&& (!targetRuntimeMetadata
			|| !DesignerPropertyCatalog::TryGetStyleValueKind(
				*targetRuntimeMetadata, targetLiteralKind)))
	{
		error = L"该目标属性类型暂不支持 Binding 缺省值。";
		return false;
	}
	if (_useFallbackValue->IsChecked)
		binding.FallbackValue = DesignerStyleValue{
			targetLiteralKind, _fallbackValue->Text };
	if (_useTargetNullValue->IsChecked)
		binding.TargetNullValue = DesignerStyleValue{
			targetLiteralKind, _targetNullValue->Text };
	if (_useConverterParameter->IsChecked)
		binding.ConverterParameter = DesignerStyleValue{
			DesignerStyleValueKind::String, _converterParameter->Text };
	if (_useStringFormat->IsChecked)
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
