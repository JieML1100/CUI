#include "DataContextSchemaEditorDialog.h"
#include "ProgrammaticControlFactory.h"
#include "DesignerDataContextSchemaUtils.h"
#include <algorithm>

namespace
{
	const std::wstring kNewProperty = L"<新增属性路径>";

	std::vector<std::wstring> ValueKindNames()
	{
		return {
			L"Unknown", L"Bool", L"Int", L"Int64",
			L"Float", L"Double", L"String", L"Object"
		};
	}
}

DataContextSchemaEditorDialog::DataContextSchemaEditorDialog(
	const DesignerDataContextSchema& schema,
	const IBindingSource* runtimeSource)
	: Window(),
	  ResultSchema(schema),
	  _runtimeSource(runtimeSource)
{
	this->Title = L"编辑 DataContext Schema";
	this->Left = 350.0f;
	this->Top = 150.0f;
	this->Width = 760.0f;
	this->Height = 715.0f;
	DesignerDataContextSchemaUtils::Canonicalize(ResultSchema);
	this->ResizeMode = ::ResizeMode::NoResize;
	this->Background = Colors::WhiteSmoke;
	auto contentOwner = std::make_unique<Panel>();
	contentOwner->BorderThickness = 0.0f;
	contentOwner->Background = D2D1_COLOR_F{ 0, 0, 0, 0 };
	auto* contentRoot = static_cast<Panel*>(SetVisualContent(std::move(contentOwner)));
	auto addContent = [contentRoot](auto* child) { return contentRoot->AdoptVisualChild(child); };

	auto tip = addContent(cui::designer::NewControl<Label>(
		L"声明绑定可用的点分源路径、值类型以及读写/通知能力。", 20, 16));
	tip->Width = 710.0f;
	tip->Height = 24.0f;

	auto existingLabel = addContent(cui::designer::NewControl<Label>(L"现有路径", 20, 62));
	existingLabel->Width = 110.0f;
	existingLabel->Height = 24.0f;
	_existingPath = addContent(cui::designer::NewControl<ComboBox>(L"", 140, 56, 580, 30));
	_existingPath->MaxDropDownHeight = 280.0f;

	auto pathLabel = addContent(cui::designer::NewControl<Label>(L"属性路径", 20, 108));
	pathLabel->Width = 110.0f;
	pathLabel->Height = 24.0f;
	_path = addContent(cui::designer::NewControl<TextBox>(L"", 140, 102, 580, 30));

	auto kindLabel = addContent(cui::designer::NewControl<Label>(L"值类型", 20, 154));
	kindLabel->Width = 110.0f;
	kindLabel->Height = 24.0f;
	_kind = addContent(cui::designer::NewControl<ComboBox>(L"", 140, 148, 220, 30));
	_kind->MaxDropDownHeight = 224.0f;
	auto kindNames = ValueKindNames();
	cui::designer::SetComboBoxItems(*_kind, std::move(kindNames));

	_canRead = addContent(cui::designer::NewControl<CheckBox>(L"可读", 400, 151));
	_canWrite = addContent(cui::designer::NewControl<CheckBox>(L"可写", 500, 151));
	_canObserve = addContent(cui::designer::NewControl<CheckBox>(L"变更通知", 600, 151));

	auto objectKindLabel = addContent(cui::designer::NewControl<Label>(L"对象契约", 20, 200));
	objectKindLabel->Width = 110.0f;
	objectKindLabel->Height = 24.0f;
	_objectKind = addContent(cui::designer::NewControl<ComboBox>(L"", 140, 194, 220, 30));
	cui::designer::SetComboBoxItems(
		*_objectKind, { L"Opaque", L"BindingSource", L"BindingList" });
	_objectKind->MaxDropDownHeight = 84.0f;
	auto itemTypeLabel = addContent(cui::designer::NewControl<Label>(L"关联类型", 390, 200));
	itemTypeLabel->Width = 90.0f;
	itemTypeLabel->Height = 24.0f;
	_itemType = addContent(cui::designer::NewControl<TextBox>(L"", 480, 194, 240, 30));

	auto save = addContent(cui::designer::NewControl<Button>(L"保存属性", 20, 250, 125, 34));
	auto remove = addContent(cui::designer::NewControl<Button>(L"删除属性", 158, 250, 125, 34));
	auto importRuntime = addContent(cui::designer::NewControl<Button>(
		_runtimeSource ? L"从运行时源导入" : L"未连接运行时源",
		296, 250, 150, 34));
	importRuntime->IsEnabled = _runtimeSource != nullptr;
	_validation = addContent(cui::designer::NewControl<Label>(L"", 460, 256));
	_validation->Width = 260.0f;
	_validation->Height = 40.0f;

	auto summaryLabel = addContent(cui::designer::NewControl<Label>(L"Schema 属性树（R=可读，W=可写，O=通知）", 20, 314));
	summaryLabel->Width = 700.0f;
	summaryLabel->Height = 24.0f;
	_summary = addContent(cui::designer::NewControl<RichTextBox>(L"", 20, 342, 700, 230));
	_summary->IsReadOnly = true;
	_summary->Background = Colors::White;
	_summary->BorderBrush = Colors::White;

	auto ok = addContent(cui::designer::NewControl<Button>(L"确定", 20, 594, 120, 36));
	auto cancel = addContent(cui::designer::NewControl<Button>(L"取消", 152, 594, 120, 36));

	_existingPath->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) LoadSelectedProperty();
	};
	_kind->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) RefreshObjectEditors();
	};
	_objectKind->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) RefreshObjectEditors();
	};
	save->Click += [this](Control*, RoutedEventArgs&) { (void)SaveProperty(); };
	remove->Click += [this](Control*, RoutedEventArgs&) { RemoveProperty(); };
	importRuntime->Click += [this](Control*, RoutedEventArgs&) { ImportRuntimeSchema(); };
	ok->Click += [this](Control*, RoutedEventArgs&) {
		std::wstring error;
		if (!DesignerDataContextSchemaUtils::Validate(ResultSchema, &error))
		{
			ShowValidation(error, true);
			return;
		}
		Applied = true;
		this->Close();
	};
	cancel->Click += [this](Control*, RoutedEventArgs&) {
		Applied = false;
		this->Close();
	};

	RefreshPathOptions();
	LoadSelectedProperty();
	RefreshSummary();
}

void DataContextSchemaEditorDialog::SelectComboValue(
	ComboBox* combo,
	const std::wstring& value)
{
	if (!combo) return;
	const int found = cui::designer::FindComboBoxItem(*combo, value);
	const int index = found < 0 ? 0 : found;
	combo->SelectedIndex = index;
	combo->Text = combo->ItemCount() == 0
		? L"" : cui::designer::ComboBoxItemText(
			*combo, static_cast<size_t>(index));
}

void DataContextSchemaEditorDialog::RefreshPathOptions(const std::wstring& preferredPath)
{
	_loading = true;
	std::vector<std::wstring> items{ kNewProperty };
	for (const auto& path : DesignerDataContextSchemaUtils::GetPaths(ResultSchema))
		items.push_back(path);
	cui::designer::SetComboBoxItems(*_existingPath, std::move(items));
	SelectComboValue(_existingPath,
		preferredPath.empty() ? kNewProperty
			: DesignerDataContextSchemaUtils::NormalizePath(preferredPath));
	_loading = false;
}

void DataContextSchemaEditorDialog::LoadSelectedProperty()
{
	_loading = true;
	const auto selected = _existingPath->Text;
	const auto* property = selected == kNewProperty
		? nullptr
		: DesignerDataContextSchemaUtils::Find(ResultSchema, selected);
	_path->Text = property ? property->Path : L"";
	SelectComboValue(_kind, property
		? DesignerDataContextSchemaUtils::ValueKindName(property->ValueKind)
		: L"Unknown");
	SelectComboValue(_objectKind, property
		? DesignerDataContextSchemaUtils::ObjectKindName(property->ObjectKind)
		: L"Opaque");
	_itemType->Text = property
		? (property->ObjectKind == DesignerDataObjectKind::BindingSource
			? property->DataType : property->ItemType) : L"";
	_canRead->IsChecked = property ? property->CanRead : true;
	_canWrite->IsChecked = property ? property->CanWrite : true;
	_canObserve->IsChecked = property ? property->CanObserve : true;
	_loading = false;
	RefreshObjectEditors();
	ShowValidation(property ? L"已加载属性。" : L"填写后点击“保存属性”。", false);
}

void DataContextSchemaEditorDialog::RefreshObjectEditors()
{
	const bool isObject = _kind && _kind->Text == L"Object";
	_objectKind->IsEnabled = isObject;
	const bool isTypedObject = isObject
		&& (_objectKind->Text == L"BindingList"
			|| _objectKind->Text == L"BindingSource");
	_itemType->IsEnabled = isTypedObject;
	if (!isTypedObject && !_loading) _itemType->Text = L"";
}

void DataContextSchemaEditorDialog::RefreshSummary()
{
	if (ResultSchema.empty())
	{
		_summary->Text = L"（未定义 Schema；Binding 源路径保持自由输入）";
		return;
	}

	std::wstring text;
	for (const auto& property : ResultSchema)
	{
		if (!text.empty()) text += L"\r\n";
		const auto path = DesignerDataContextSchemaUtils::NormalizePath(property.Path);
		const size_t depth = static_cast<size_t>(std::count(path.begin(), path.end(), L'.'));
		text.append(depth * 2, L' ');
		text += DesignerDataContextSchemaUtils::Describe(property);
	}
	_summary->Text = std::move(text);
}

void DataContextSchemaEditorDialog::ShowValidation(
	const std::wstring& message,
	bool isError)
{
	_validation->Text = message;
	_validation->Foreground = isError ? Colors::Red : Colors::DimGrey;
	_validation->InvalidateVisual();
}

bool DataContextSchemaEditorDialog::SaveProperty()
{
	DesignerDataContextProperty property;
	property.Path = DesignerDataContextSchemaUtils::NormalizePath(_path->Text);
	if (!DesignerDataContextSchemaUtils::IsValidPath(property.Path))
	{
		ShowValidation(L"属性路径及每个点分段都不能为空。", true);
		return false;
	}
	if (!DesignerDataContextSchemaUtils::TryParseValueKind(_kind->Text, property.ValueKind))
	{
		ShowValidation(L"请选择有效的值类型。", true);
		return false;
	}
	property.CanRead = _canRead->IsChecked;
	property.CanWrite = _canWrite->IsChecked;
	property.CanObserve = _canObserve->IsChecked;
	if (property.ValueKind == BindingValueKind::Object)
	{
		if (!DesignerDataContextSchemaUtils::TryParseObjectKind(
			_objectKind->Text, property.ObjectKind))
		{
			ShowValidation(L"请选择有效的对象契约。", true);
			return false;
		}
		if (property.ObjectKind == DesignerDataObjectKind::BindingList)
			property.ItemType = _itemType->Text;
		else if (property.ObjectKind == DesignerDataObjectKind::BindingSource)
			property.DataType = _itemType->Text;
	}

	auto candidate = ResultSchema;
	const auto selected = _existingPath->Text;
	auto selectedProperty = std::find_if(candidate.begin(), candidate.end(),
		[&](const DesignerDataContextProperty& item)
		{
			return selected != kNewProperty && item.Path == selected;
		});
	auto pathCollision = std::find_if(candidate.begin(), candidate.end(),
		[&](const DesignerDataContextProperty& item)
		{
			return item.Path == property.Path;
		});
	if (pathCollision != candidate.end() && pathCollision != selectedProperty)
	{
		ShowValidation(L"属性路径已存在：" + property.Path, true);
		return false;
	}
	if (selectedProperty == candidate.end()) candidate.push_back(property);
	else *selectedProperty = property;

	DesignerDataContextSchemaUtils::Canonicalize(candidate);
	std::wstring error;
	if (!DesignerDataContextSchemaUtils::Validate(candidate, &error))
	{
		ShowValidation(error, true);
		return false;
	}

	ResultSchema = std::move(candidate);
	RefreshPathOptions(property.Path);
	LoadSelectedProperty();
	RefreshSummary();
	ShowValidation(L"属性已暂存；点击“确定”写入设计文档。", false);
	return true;
}

void DataContextSchemaEditorDialog::RemoveProperty()
{
	const auto selected = _existingPath->Text;
	if (selected == kNewProperty)
	{
		ShowValidation(L"请选择要删除的属性。", false);
		return;
	}
	const auto oldSize = ResultSchema.size();
	std::erase_if(ResultSchema,
		[&](const DesignerDataContextProperty& property)
		{
			return property.Path == selected;
		});
	RefreshPathOptions();
	LoadSelectedProperty();
	RefreshSummary();
	ShowValidation(ResultSchema.size() != oldSize
		? L"属性已删除；点击“确定”写入设计文档。"
		: L"未找到该属性。", false);
}

void DataContextSchemaEditorDialog::ImportRuntimeSchema()
{
	if (!_runtimeSource)
	{
		ShowValidation(L"当前没有连接运行时数据源。", true);
		return;
	}

	DesignerDataContextSchema imported;
	std::wstring error;
	if (!DesignerDataContextSchemaUtils::BuildFromBindingSource(
		*_runtimeSource, imported, &error))
	{
		ShowValidation(error, true);
		return;
	}

	auto candidate = ResultSchema;
	for (const auto& property : imported)
	{
		auto existing = std::find_if(candidate.begin(), candidate.end(),
			[&](const DesignerDataContextProperty& candidate)
			{
				return candidate.Path == property.Path;
			});
		if (existing == candidate.end()) candidate.push_back(property);
		else *existing = property;
	}
	DesignerDataContextSchemaUtils::Canonicalize(candidate);
	if (!DesignerDataContextSchemaUtils::Validate(candidate, &error))
	{
		ShowValidation(error, true);
		return;
	}
	ResultSchema = std::move(candidate);

	RefreshPathOptions();
	LoadSelectedProperty();
	RefreshSummary();
	ShowValidation(L"已从运行时源导入 " + std::to_wstring(imported.size())
		+ L" 个路径。", false);
}
