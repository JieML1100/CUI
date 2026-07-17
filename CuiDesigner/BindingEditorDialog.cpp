#include "BindingEditorDialog.h"
#include "DesignerBindingUtils.h"
#include "DesignerDataContextSchemaUtils.h"
#include <algorithm>

namespace
{
	const std::wstring kNoConverter = L"<None>";
	const std::wstring kCustomConverter = L"<Custom ID>";
	const std::wstring kManualSourcePath = L"<手动输入>";

	std::vector<std::wstring> AllBindingModeNames()
	{
		return { L"OneWay", L"TwoWay", L"OneWayToSource", L"OneTime" };
	}

	std::vector<std::wstring> AllUpdateModeNames()
	{
		return { L"OnPropertyChanged", L"OnValidation", L"Never" };
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
	const std::vector<DesignerCustomPropertyDescriptor>& customProperties)
	: Form(L"编辑数据绑定", POINT{ 320, 190 }, SIZE{ 820, 704 }),
	  ResultBindings(bindings),
	  _target(target),
	  _runtimeSource(runtimeSource),
	  _sourceSchema(sourceSchema)
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

	auto sourceLabel = this->AddControl(new Label(L"源路径", 20, 106));
	sourceLabel->Size = { 120, 24 };
	_sourcePath = this->AddControl(new TextBox(L"", 150, 100, 400, 30));
	_knownSourcePath = this->AddControl(new ComboBox(L"", 560, 100, 220, 30));
	_knownSourcePath->ExpandCount = 10;
	auto& knownSourceItems = _knownSourcePath->Items;
	knownSourceItems.clear();
	knownSourceItems.push_back(kManualSourcePath);
	for (const auto& path : DesignerDataContextSchemaUtils::GetPaths(_sourceSchema))
		knownSourceItems.push_back(path);
	_knownSourcePath->SelectedIndex = 0;
	_knownSourcePath->Text = kManualSourcePath;
	_knownSourcePath->Enable = !_sourceSchema.empty();

	auto modeLabel = this->AddControl(new Label(L"绑定模式", 20, 152));
	modeLabel->Size = { 120, 24 };
	_mode = this->AddControl(new ComboBox(L"", 150, 146, 250, 30));
	_mode->ExpandCount = 4;

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

	_capabilities = this->AddControl(new Label(L"", 20, 242));
	_capabilities->Size = { 760, 24 };
	_runtimeValidation = this->AddControl(new Label(L"", 20, 272));
	_runtimeValidation->Size = { 760, 40 };
	_validation = this->AddControl(new Label(L"", 20, 316));
	_validation->Size = { 760, 40 };

	_saveBinding = this->AddControl(new Button(L"保存绑定", 20, 360, 130, 34));
	_removeBinding = this->AddControl(new Button(L"删除绑定", 162, 360, 130, 34));

	auto summaryLabel = this->AddControl(new Label(L"当前绑定", 20, 416));
	summaryLabel->Size = { 120, 24 };
	_summary = this->AddControl(new RichTextBox(L"", 20, 444, 760, 160));
	_summary->ReadOnly = true;
	_summary->AllowMultiLine = true;
	_summary->BackColor = Colors::White;
	_summary->FocusedColor = Colors::White;

	_ok = this->AddControl(new Button(L"确定", 20, 622, 120, 36));
	_cancel = this->AddControl(new Button(L"取消", 152, 622, 120, 36));

	if (_target)
	{
		for (const auto* metadata : BindingPropertyRegistry::GetProperties(*_target))
		{
			if (!metadata) continue;
			_properties.push_back({
				metadata->Name(), metadata->ValueKind(),
				metadata->CanRead(), metadata->CanWrite(), metadata->CanObserve() });
		}
	}
	for (const auto& property : customProperties)
	{
		if (!property.Bindable) continue;
		const auto duplicate = std::find_if(
			_properties.begin(), _properties.end(), [&](const auto& existing)
			{
				return _wcsicmp(existing.Name.c_str(), property.Name.c_str()) == 0;
			});
		if (duplicate != _properties.end()) continue;
		_properties.push_back({
			property.Name,
			ToBindingValueKind(property.DefaultValue.Kind),
			property.SupportsTwoWayBinding,
			true,
			property.SupportsTwoWayBinding });
	}
	std::vector<std::wstring> propertyNames;
	propertyNames.reserve(_properties.size());
	for (const auto& metadata : _properties)
		propertyNames.push_back(metadata.Name);
	_targetProperty->Items = propertyNames;

	_targetProperty->OnSelectionChanged += [this](Control*) {
		if (!_loadingEditor) LoadSelectedBinding();
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
			RefreshUpdateModeOptions(DataSourceUpdateMode::OnPropertyChanged);
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
		if (metadata)
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
		_sourcePath->Enable = false;
		_knownSourcePath->Enable = false;
		_mode->Enable = false;
		_updateMode->Enable = false;
		_converter->Enable = false;
		_customConverter->Enable = false;
		_saveBinding->Enable = false;
		_removeBinding->Enable = false;
		ShowValidation(L"该控件没有公开可绑定属性。", true);
	}
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

void BindingEditorDialog::LoadSelectedBinding()
{
	const auto* metadata = SelectedMetadata();
	if (!metadata) return;

	_loadingEditor = true;
	DesignerDataBinding binding;
	auto it = ResultBindings.find(metadata->Name);
	if (it != ResultBindings.end()) binding = it->second;
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

	std::vector<std::wstring> names;
	if (!IsTargetToSourceMode(mode))
	{
		names.push_back(L"OnPropertyChanged");
		_updateMode->Enable = false;
	}
	else if (metadata && !metadata->CanObserve)
	{
		names.push_back(L"Never");
		preferredMode = DataSourceUpdateMode::Never;
		_updateMode->Enable = false;
	}
	else
	{
		names = AllUpdateModeNames();
		_updateMode->Enable = true;
	}
	_updateMode->Items = names;
	SelectComboValue(_updateMode, DesignerBindingUtils::UpdateModeName(preferredMode));
}

void BindingEditorDialog::RefreshConverterOptions(const std::wstring& preferredConverter)
{
	std::vector<std::wstring> names{ kNoConverter };
	const auto* targetMetadata = SelectedMetadata();
	const auto* sourceMetadata = DesignerDataContextSchemaUtils::Find(
		_sourceSchema, _sourcePath ? _sourcePath->Text : L"");
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
		+ L"    可通知: " + YesNo(metadata->CanObserve);
	if (const auto* source = DesignerDataContextSchemaUtils::Find(
		_sourceSchema, _sourcePath ? _sourcePath->Text : L""))
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
	if (!_runtimeSource || !_sourcePath) return;

	const auto sourcePath = DesignerBindingUtils::Trim(_sourcePath->Text);
	if (!DesignerBindingUtils::IsValidSourcePath(sourcePath)) return;
	std::vector<std::wstring> segments;
	size_t start = 0;
	while (start <= sourcePath.size())
	{
		const size_t separator = sourcePath.find(L'.', start);
		const size_t end = separator == std::wstring::npos
			? sourcePath.size() : separator;
		segments.push_back(DesignerBindingUtils::Trim(
			sourcePath.substr(start, end - start)));
		if (separator == std::wstring::npos) break;
		start = separator + 1;
	}

	IBindingSource* current = _runtimeSource;
	for (size_t index = 0; index < segments.size(); ++index)
	{
		const auto expectedProperty = segments[index];
		if (auto* validationChanged = current->ValidationChanged())
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
		if (index + 1 == segments.size()) break;

		auto pathConnection = current->PropertyChanged().Subscribe(
			[this, expectedProperty](const PropertyChangedEventArgs& e)
			{
				if (!e.PropertyName.empty()
					&& _wcsicmp(e.PropertyName.c_str(), expectedProperty.c_str()) != 0)
					return;
				AttachRuntimeValidation();
				RefreshRuntimeValidation();
			});
		if (pathConnection.Connected())
			_runtimePathConnections.push_back(std::move(pathConnection));

		BindingValue value;
		BindingSourceReference reference;
		if (!current->TryGetValue(expectedProperty, value)
			|| !value.TryGet(reference)
			|| !reference)
			break;
		_runtimePathOwners.push_back(reference.Shared());
		current = reference.Get();
	}
}

void BindingEditorDialog::RefreshRuntimeValidation()
{
	if (!_runtimeValidation) return;
	const auto sourcePath = DesignerBindingUtils::Trim(
		_sourcePath ? _sourcePath->Text : L"");
	if (!_runtimeSource || sourcePath.empty())
	{
		_runtimeValidation->Text = _runtimeSource
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
		*_runtimeSource, sourcePath);
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
	return DesignerBindingUtils::ValidateTarget(
		*metadata, binding, &error, &_sourceSchema);
}

bool BindingEditorDialog::SaveCurrentBinding()
{
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
	_sourcePath->Text = L"";
	RefreshConverterOptions(L"");
	RefreshSummary();
	ShowValidation(removed ? L"绑定已删除；点击“确定”写入设计文档。"
		: L"该属性当前没有绑定。", false);
}
