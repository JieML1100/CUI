#include "DataResourcesEditorDialog.h"
#include "ProgrammaticControlFactory.h"
#include "DesignerDataContextSchemaUtils.h"
#include "DesignerModel/DesignDataResourceEditorModel.h"
#include "DesignerModel/DesignDataResourceUtils.h"
#include "DesignerBindingUtils.h"
#include <Convert.h>
#include <algorithm>
#include <sstream>

namespace
{
	const std::wstring kNewType = L"<新增 DataType>";
	const std::wstring kNewProperty = L"<新增属性>";
	const std::wstring kNewList = L"<新增 DataList>";
	const std::wstring kNewRecord = L"<新增记录>";
	const std::wstring kNewTemplate = L"<新增 DataTemplate>";

	bool Equals(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	std::vector<std::wstring> ValueKindNames()
	{
		return { L"Bool", L"Int", L"Int64", L"Float", L"Double",
			L"String", L"Object" };
	}

	std::wstring RecordName(size_t index)
	{
		return L"记录 " + std::to_wstring(index + 1);
	}
}

DataResourcesEditorDialog::DataResourcesEditorDialog(
	const DesignerModel::DesignDocument& document)
	: Window(),
	  ResultDocument(document)
{
	this->Title = L"编辑数据资源";
	this->Left = 260.0f;
	this->Top = 90.0f;
	this->Width = 940.0f;
	this->Height = 760.0f;
	this->ResizeMode = ::ResizeMode::NoResize;
	this->Background = Colors::WhiteSmoke;
	auto contentOwner = std::make_unique<Panel>();
	contentOwner->BorderThickness = 0.0f;
	contentOwner->Background = D2D1_COLOR_F{ 0, 0, 0, 0 };
	auto* contentRoot = static_cast<Panel*>(SetVisualContent(std::move(contentOwner)));
	auto addContent = [contentRoot](auto* child) { return contentRoot->AdoptVisualChild(child); };

	auto title = addContent(cui::designer::NewControl<Label>(
		L"DataType 定义记录契约；DataList/DataRecord 是设计器与运行时共用的文件数据。", 20, 14));
	title->Width = 880.0f;
	title->Height = 24.0f;

	auto typeTitle = addContent(cui::designer::NewControl<Label>(L"DataType 与字段", 20, 52));
	cui::designer::ApplyProgrammaticTypography(
		*typeTitle, L"Microsoft YaHei", 17.0);
	typeTitle->Width = 240.0f;
	typeTitle->Height = 28.0f;
	addContent(cui::designer::NewControl<Label>(L"类型", 20, 92))->Width = 80.0f;
	addContent(cui::designer::NewControl<Label>(L"类型", 20, 92))->Height = 24.0f;
	_typeList = addContent(cui::designer::NewControl<ComboBox>(L"", 100, 86, 410, 30));
	_typeList->MaxDropDownHeight = 280.0f;
	addContent(cui::designer::NewControl<Label>(L"名称", 20, 132))->Width = 80.0f;
	addContent(cui::designer::NewControl<Label>(L"名称", 20, 132))->Height = 24.0f;
	_typeName = addContent(cui::designer::NewControl<TextBox>(L"", 100, 126, 240, 30));
	auto saveType = addContent(cui::designer::NewControl<Button>(L"保存/重命名类型", 350, 124, 160, 34));

	addContent(cui::designer::NewControl<Label>(L"字段", 20, 176))->Width = 80.0f;

	addContent(cui::designer::NewControl<Label>(L"字段", 20, 176))->Height = 24.0f;
	_propertyList = addContent(cui::designer::NewControl<ComboBox>(L"", 100, 170, 410, 30));
	_propertyList->MaxDropDownHeight = 280.0f;
	addContent(cui::designer::NewControl<Label>(L"路径", 20, 216))->Width = 80.0f;
	addContent(cui::designer::NewControl<Label>(L"路径", 20, 216))->Height = 24.0f;
	_propertyPath = addContent(cui::designer::NewControl<TextBox>(L"", 100, 210, 240, 30));
	_propertyKind = addContent(cui::designer::NewControl<ComboBox>(L"", 350, 210, 160, 30));
	cui::designer::SetComboBoxItems(*_propertyKind, ValueKindNames());
	_propertyKind->MaxDropDownHeight = 196.0f;
	addContent(cui::designer::NewControl<Label>(L"对象", 20, 256))->Width = 80.0f;
	addContent(cui::designer::NewControl<Label>(L"对象", 20, 256))->Height = 24.0f;
	_propertyObjectKind = addContent(cui::designer::NewControl<ComboBox>(L"", 100, 250, 160, 30));
	cui::designer::SetComboBoxItems(
		*_propertyObjectKind,
		{ L"Opaque", L"BindingSource", L"BindingList" });
	_propertyObjectKind->MaxDropDownHeight = 84.0f;
	_propertyItemType = addContent(cui::designer::NewControl<ComboBox>(L"", 270, 250, 240, 30));
	_propertyItemType->MaxDropDownHeight = 280.0f;
	_propertyCanRead = addContent(cui::designer::NewControl<CheckBox>(L"可读", 20, 294));
	_propertyCanWrite = addContent(cui::designer::NewControl<CheckBox>(L"可写", 105, 294));
	_propertyCanObserve = addContent(cui::designer::NewControl<CheckBox>(L"通知", 190, 294));
	auto saveProperty = addContent(cui::designer::NewControl<Button>(L"保存字段", 290, 290, 105, 34));
	auto removeProperty = addContent(cui::designer::NewControl<Button>(L"删除字段", 405, 290, 105, 34));
	auto removeType = addContent(cui::designer::NewControl<Button>(L"删除类型", 405, 330, 105, 32));

	auto listTitle = addContent(cui::designer::NewControl<Label>(L"DataList 与记录", 20, 376));
	cui::designer::ApplyProgrammaticTypography(
		*listTitle, L"Microsoft YaHei", 17.0);
	listTitle->Width = 240.0f;
	listTitle->Height = 28.0f;
	addContent(cui::designer::NewControl<Label>(L"列表", 20, 416))->Width = 80.0f;
	addContent(cui::designer::NewControl<Label>(L"列表", 20, 416))->Height = 24.0f;
	_listList = addContent(cui::designer::NewControl<ComboBox>(L"", 100, 410, 410, 30));
	_listList->MaxDropDownHeight = 280.0f;
	addContent(cui::designer::NewControl<Label>(L"键", 20, 456))->Width = 80.0f;
	addContent(cui::designer::NewControl<Label>(L"键", 20, 456))->Height = 24.0f;
	_listKey = addContent(cui::designer::NewControl<TextBox>(L"", 100, 450, 200, 30));
	_listItemType = addContent(cui::designer::NewControl<ComboBox>(L"", 310, 450, 200, 30));
	_listItemType->MaxDropDownHeight = 280.0f;
	auto saveList = addContent(cui::designer::NewControl<Button>(L"保存/重命名列表", 20, 492, 160, 34));
	auto removeList = addContent(cui::designer::NewControl<Button>(L"删除列表", 190, 492, 105, 34));

	addContent(cui::designer::NewControl<Label>(L"记录", 20, 542))->Width = 80.0f;

	addContent(cui::designer::NewControl<Label>(L"记录", 20, 542))->Height = 24.0f;
	_recordList = addContent(cui::designer::NewControl<ComboBox>(L"", 100, 536, 410, 30));
	_recordList->MaxDropDownHeight = 280.0f;
	addContent(cui::designer::NewControl<Label>(L"字段（每行 Path=Value）", 20, 580))->Width = 300.0f;
	addContent(cui::designer::NewControl<Label>(L"字段（每行 Path=Value）", 20, 580))->Height = 24.0f;
	_recordFields = addContent(cui::designer::NewControl<RichTextBox>(L"", 20, 608, 365, 70));
	_recordFields->Background = Colors::White;
	auto saveRecord = addContent(cui::designer::NewControl<Button>(L"保存记录", 395, 608, 115, 32));
	auto removeRecord = addContent(cui::designer::NewControl<Button>(L"删除记录", 395, 646, 115, 32));

	auto templateTitle = addContent(cui::designer::NewControl<Label>(L"DataTemplate", 540, 52));
	cui::designer::ApplyProgrammaticTypography(
		*templateTitle, L"Microsoft YaHei", 17.0);
	templateTitle->Width = 180.0f;
	templateTitle->Height = 28.0f;
	_templateList = addContent(cui::designer::NewControl<ComboBox>(L"", 540, 86, 365, 30));
	_templateList->MaxDropDownHeight = 280.0f;
	_templateKey = addContent(cui::designer::NewControl<TextBox>(L"", 540, 126, 175, 30));
	_templateDataType = addContent(cui::designer::NewControl<ComboBox>(L"", 725, 126, 180, 30));
	_templateDataType->MaxDropDownHeight = 280.0f;
	auto saveTemplate = addContent(cui::designer::NewControl<Button>(L"保存/创建模板", 540, 166, 145, 34));
	auto removeTemplate = addContent(cui::designer::NewControl<Button>(L"删除模板", 695, 166, 105, 34));
	auto editTemplateHint = addContent(cui::designer::NewControl<Label>(L"视觉树：画布/XAML 编辑", 540, 204));
	editTemplateHint->Width = 300.0f;
	editTemplateHint->Height = 24.0f;

	auto summaryTitle = addContent(cui::designer::NewControl<Label>(L"资源摘要", 540, 236));
	cui::designer::ApplyProgrammaticTypography(
		*summaryTitle, L"Microsoft YaHei", 17.0);
	summaryTitle->Width = 180.0f;
	summaryTitle->Height = 28.0f;
	_summary = addContent(cui::designer::NewControl<RichTextBox>(L"", 540, 270, 365, 326));
	_summary->IsReadOnly = true;
	_summary->Background = Colors::White;
	_validation = addContent(cui::designer::NewControl<Label>(L"", 540, 606));
	_validation->Width = 365.0f;
	_validation->Height = 56.0f;
	auto ok = addContent(cui::designer::NewControl<Button>(L"确定", 650, 680, 120, 36));
	auto cancel = addContent(cui::designer::NewControl<Button>(L"取消", 785, 680, 120, 36));

	_typeList->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) LoadSelectedType();
	};
	_propertyList->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) LoadSelectedProperty();
	};
	_propertyKind->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) RefreshObjectEditors();
	};
	_propertyObjectKind->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) RefreshObjectEditors();
	};
	_listList->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) LoadSelectedList();
	};
	_recordList->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) LoadSelectedRecord();
	};
	_templateList->SelectionChanged += [this](Control*, SelectionChangedEventArgs&) {
		if (!_loading) LoadSelectedTemplate();
	};
	saveType->Click += [this](Control*, RoutedEventArgs&) { (void)SaveType(); };
	removeType->Click += [this](Control*, RoutedEventArgs&) { RemoveType(); };
	saveProperty->Click += [this](Control*, RoutedEventArgs&) { (void)SaveProperty(); };
	removeProperty->Click += [this](Control*, RoutedEventArgs&) { RemoveProperty(); };
	saveList->Click += [this](Control*, RoutedEventArgs&) { (void)SaveList(); };
	removeList->Click += [this](Control*, RoutedEventArgs&) { RemoveList(); };
	saveRecord->Click += [this](Control*, RoutedEventArgs&) { (void)SaveRecord(); };
	removeRecord->Click += [this](Control*, RoutedEventArgs&) { RemoveRecord(); };
	saveTemplate->Click += [this](Control*, RoutedEventArgs&) { (void)SaveTemplate(); };
	removeTemplate->Click += [this](Control*, RoutedEventArgs&) { RemoveTemplate(); };
	ok->Click += [this](Control*, RoutedEventArgs&) {
		std::wstring error;
		if (!DesignerModel::DesignDataResourceUtils::ValidateAndCanonicalize(
			ResultDocument, &error))
		{
			ShowValidation(error, true);
			return;
		}
		Applied = true;
		Close();
	};
	cancel->Click += [this](Control*, RoutedEventArgs&) {
		Applied = false;
		Close();
	};

	RefreshItemTypeChoices();
	RefreshTypeList();
	LoadSelectedType();
	RefreshListList();
	LoadSelectedList();
	RefreshTemplateList();
	LoadSelectedTemplate();
	RefreshSummary();
}

void DataResourcesEditorDialog::SelectComboValue(
	ComboBox* combo, const std::wstring& value)
{
	if (!combo) return;
	const auto items = cui::designer::ComboBoxItems(*combo);
	const auto found = std::find_if(items.begin(), items.end(),
		[&](const auto& item) { return Equals(item, value); });
	const auto index = found == items.end() ? 0
		: static_cast<int>(found - items.begin());
	combo->SelectedIndex = items.empty() ? -1 : index;
	combo->Text = items.empty() ? L"" : items[static_cast<size_t>(index)];
}

void DataResourcesEditorDialog::RefreshItemTypeChoices()
{
	std::vector<std::wstring> choices;
	for (const auto& type : ResultDocument.DataTypes) choices.push_back(type.Name);
	cui::designer::SetComboBoxItems(*_propertyItemType, choices);
	cui::designer::SetComboBoxItems(*_listItemType, choices);
	std::vector<std::wstring> templateChoices{ L"" };
	templateChoices.insert(templateChoices.end(), choices.begin(), choices.end());
	cui::designer::SetComboBoxItems(
		*_templateDataType, std::move(templateChoices));
}

void DataResourcesEditorDialog::RefreshTypeList(const std::wstring& preferred)
{
	_loading = true;
	cui::designer::SetComboBoxItems(*_typeList, { kNewType });
	for (const auto& type : ResultDocument.DataTypes)
		cui::designer::AddComboBoxItem(*_typeList, type.Name);
	SelectComboValue(_typeList, preferred.empty() ? kNewType : preferred);
	_loading = false;
}

void DataResourcesEditorDialog::LoadSelectedType()
{
	_loading = true;
	const auto* type = _typeList->Text == kNewType ? nullptr
		: ResultDocument.FindDataType(_typeList->Text);
	_typeName->Text = type ? type->Name : L"";
	_loading = false;
	RefreshPropertyList();
	LoadSelectedProperty();
	ShowValidation(type && !type->SourceDictionary.empty()
		? L"外部 DataType：请打开其来源字典编辑。"
		: type ? L"已加载 DataType。" : L"新类型请先填写名称和首个字段。", false);
}

void DataResourcesEditorDialog::RefreshPropertyList(const std::wstring& preferred)
{
	_loading = true;
	cui::designer::SetComboBoxItems(*_propertyList, { kNewProperty });
	if (const auto* type = ResultDocument.FindDataType(_typeList->Text))
		for (const auto& property : type->Properties)
			cui::designer::AddComboBoxItem(*_propertyList, property.Path);
	SelectComboValue(_propertyList,
		preferred.empty() ? kNewProperty : preferred);
	_loading = false;
}

void DataResourcesEditorDialog::LoadSelectedProperty()
{
	_loading = true;
	const auto* type = ResultDocument.FindDataType(_typeList->Text);
	const auto* property = type && _propertyList->Text != kNewProperty
		? DesignerDataContextSchemaUtils::Find(
			type->Properties, _propertyList->Text) : nullptr;
	_propertyPath->Text = property ? property->Path : L"";
	SelectComboValue(_propertyKind, property
		? DesignerDataContextSchemaUtils::ValueKindName(property->ValueKind)
		: L"String");
	SelectComboValue(_propertyObjectKind, property
		? DesignerDataContextSchemaUtils::ObjectKindName(property->ObjectKind)
		: L"Opaque");
	SelectComboValue(_propertyItemType, property
		? (property->ObjectKind == DesignerDataObjectKind::BindingSource
			? property->DataType : property->ItemType) : L"");
	_propertyCanRead->IsChecked = property ? property->CanRead : true;
	_propertyCanWrite->IsChecked = property ? property->CanWrite : true;
	_propertyCanObserve->IsChecked = property ? property->CanObserve : true;
	_loading = false;
	RefreshObjectEditors();
}

void DataResourcesEditorDialog::RefreshObjectEditors()
{
	const bool object = _propertyKind->Text == L"Object";
	_propertyObjectKind->IsEnabled = object;
	_propertyItemType->IsEnabled = object
		&& (_propertyObjectKind->Text == L"BindingList"
			|| _propertyObjectKind->Text == L"BindingSource");
	if (!_propertyItemType->IsEnabled && !_loading) _propertyItemType->Text = L"";
}

bool DataResourcesEditorDialog::SaveType()
{
	const auto original = _typeList->Text == kNewType ? L"" : _typeList->Text;
	const auto* current = ResultDocument.FindDataType(original);
	if (!current)
	{
		ShowValidation(L"新 DataType 必须通过“保存字段”创建。", true);
		return false;
	}
	auto definition = *current;
	definition.Name = _typeName->Text;
	std::wstring error;
	if (!DesignerModel::DesignDataResourceEditorModel::UpsertDataType(
		ResultDocument, original, std::move(definition), &error))
	{
		ShowValidation(error, true);
		return false;
	}
	const auto name = DesignerBindingUtils::Trim(_typeName->Text);
	RefreshItemTypeChoices();
	RefreshTypeList(name);
	LoadSelectedType();
	RefreshListList(_listList->Text);
	LoadSelectedList();
	LoadSelectedTemplate();
	RefreshSummary();
	ShowValidation(L"DataType 已保存，相关 ItemType/DataType 引用已更新。", false);
	return true;
}

void DataResourcesEditorDialog::RemoveType()
{
	if (_typeList->Text == kNewType) return;
	std::wstring error;
	if (!DesignerModel::DesignDataResourceEditorModel::RemoveDataType(
		ResultDocument, _typeList->Text, &error))
	{
		ShowValidation(error, true);
		return;
	}
	RefreshItemTypeChoices();
	RefreshTypeList();
	LoadSelectedType();
	RefreshListList();
	LoadSelectedList();
	LoadSelectedTemplate();
	RefreshSummary();
	ShowValidation(L"DataType 已删除。", false);
}

bool DataResourcesEditorDialog::SaveProperty()
{
	DesignerDataContextProperty property;
	property.Path = DesignerDataContextSchemaUtils::NormalizePath(_propertyPath->Text);
	if (!DesignerDataContextSchemaUtils::IsValidPath(property.Path)
		|| !DesignerDataContextSchemaUtils::TryParseValueKind(
			_propertyKind->Text, property.ValueKind))
	{
		ShowValidation(L"字段路径或类型无效。", true);
		return false;
	}
	property.CanRead = _propertyCanRead->IsChecked;
	property.CanWrite = _propertyCanWrite->IsChecked;
	property.CanObserve = _propertyCanObserve->IsChecked;
	if (property.ValueKind == BindingValueKind::Object)
	{
		if (!DesignerDataContextSchemaUtils::TryParseObjectKind(
			_propertyObjectKind->Text, property.ObjectKind))
		{
			ShowValidation(L"对象字段契约无效。", true);
			return false;
		}
		if (property.ObjectKind == DesignerDataObjectKind::BindingList)
			property.ItemType = _propertyItemType->Text;
		else if (property.ObjectKind == DesignerDataObjectKind::BindingSource)
			property.DataType = _propertyItemType->Text;
	}
	const auto originalType = _typeList->Text == kNewType ? L"" : _typeList->Text;
	const auto selectedPath = _propertyList->Text;
	std::wstring error;
	auto candidate = ResultDocument;
	if (!originalType.empty()
		&& originalType != DesignerBindingUtils::Trim(_typeName->Text))
	{
		auto definition = *candidate.FindDataType(originalType);
		definition.Name = _typeName->Text;
		if (!DesignerModel::DesignDataResourceEditorModel::UpsertDataType(
			candidate, originalType, std::move(definition), &error))
		{
			ShowValidation(error, true);
			return false;
		}
	}
	const auto effectiveType = DesignerBindingUtils::Trim(_typeName->Text);
	if (!DesignerModel::DesignDataResourceEditorModel::UpsertDataTypeProperty(
		candidate, effectiveType,
		selectedPath == kNewProperty ? L"" : selectedPath,
		property, &error))
	{
		ShowValidation(error, true);
		return false;
	}
	ResultDocument = std::move(candidate);
	const auto typeName = effectiveType;
	RefreshItemTypeChoices();
	RefreshTypeList(typeName);
	LoadSelectedType();
	RefreshPropertyList(property.Path);
	LoadSelectedProperty();
	RefreshListList(_listList->Text);
	LoadSelectedList();
	LoadSelectedTemplate();
	RefreshSummary();
	ShowValidation(L"DataType 字段已保存。", false);
	return true;
}

void DataResourcesEditorDialog::RemoveProperty()
{
	const auto originalType = _typeList->Text;
	const auto selectedPath = _propertyList->Text;
	const auto* current = ResultDocument.FindDataType(originalType);
	if (!current || selectedPath == kNewProperty) return;
	auto definition = *current;
	definition.Properties.erase(std::remove_if(
		definition.Properties.begin(), definition.Properties.end(),
		[&](const auto& item) { return Equals(item.Path, selectedPath); }),
		definition.Properties.end());
	std::wstring error;
	if (!DesignerModel::DesignDataResourceEditorModel::UpsertDataType(
		ResultDocument, originalType, std::move(definition), &error))
	{
		ShowValidation(error, true);
		return;
	}
	RefreshPropertyList();
	LoadSelectedProperty();
	RefreshSummary();
	ShowValidation(L"字段已删除。", false);
}

void DataResourcesEditorDialog::RefreshListList(const std::wstring& preferred)
{
	_loading = true;
	cui::designer::SetComboBoxItems(*_listList, { kNewList });
	for (const auto& list : ResultDocument.DataLists)
		cui::designer::AddComboBoxItem(*_listList, list.Key);
	SelectComboValue(_listList, preferred.empty() ? kNewList : preferred);
	_loading = false;
}

void DataResourcesEditorDialog::LoadSelectedList()
{
	_loading = true;
	const auto* list = _listList->Text == kNewList ? nullptr
		: ResultDocument.FindDataList(_listList->Text);
	_listKey->Text = list ? list->Key : L"";
	SelectComboValue(_listItemType, list ? list->ItemType : L"");
	_loading = false;
	RefreshRecordList();
	LoadSelectedRecord();
	ShowValidation(list && !list->SourceDictionary.empty()
		? L"外部 DataList：请打开其来源字典编辑。"
		: list ? L"已加载 DataList。" : L"填写键和元素类型后保存列表。", false);
}

void DataResourcesEditorDialog::RefreshRecordList(int preferredIndex)
{
	_loading = true;
	cui::designer::SetComboBoxItems(*_recordList, { kNewRecord });
	if (const auto* list = ResultDocument.FindDataList(_listList->Text))
		for (size_t index = 0; index < list->Records.size(); ++index)
			cui::designer::AddComboBoxItem(
				*_recordList, RecordName(index));
	const auto value = preferredIndex < 0 ? kNewRecord
		: RecordName(static_cast<size_t>(preferredIndex));
	SelectComboValue(_recordList, value);
	_loading = false;
}

void DataResourcesEditorDialog::LoadSelectedRecord()
{
	_recordFields->Text.clear();
	const auto* list = ResultDocument.FindDataList(_listList->Text);
	if (!list || _recordList->SelectedIndex <= 0) return;
	const auto index = static_cast<size_t>(_recordList->SelectedIndex - 1);
	if (index >= list->Records.size()) return;
	std::wstring text;
	for (const auto& [path, value] : list->Records[index].Fields)
	{
		if (!text.empty()) text += L"\r\n";
		text += path + L"=" + value;
	}
	_recordFields->Text = std::move(text);
}

bool DataResourcesEditorDialog::SaveList()
{
	const auto original = _listList->Text == kNewList ? L"" : _listList->Text;
	DesignerModel::DesignDataList definition;
	if (const auto* current = ResultDocument.FindDataList(original))
		definition = *current;
	definition.Key = _listKey->Text;
	definition.ItemType = _listItemType->Text;
	std::wstring error;
	if (!DesignerModel::DesignDataResourceEditorModel::UpsertDataList(
		ResultDocument, original, std::move(definition), &error))
	{
		ShowValidation(error, true);
		return false;
	}
	const auto key = DesignerBindingUtils::Trim(_listKey->Text);
	RefreshListList(key);
	LoadSelectedList();
	RefreshSummary();
	ShowValidation(L"DataList 已保存，StaticResource 引用已更新。", false);
	return true;
}

void DataResourcesEditorDialog::RemoveList()
{
	if (_listList->Text == kNewList) return;
	std::wstring error;
	if (!DesignerModel::DesignDataResourceEditorModel::RemoveDataList(
		ResultDocument, _listList->Text, &error))
	{
		ShowValidation(error, true);
		return;
	}
	RefreshListList();
	LoadSelectedList();
	RefreshSummary();
	ShowValidation(L"DataList 已删除。", false);
}

bool DataResourcesEditorDialog::SaveRecord()
{
	const auto original = _listList->Text == kNewList ? L"" : _listList->Text;
	DesignerModel::DesignDataList definition;
	if (const auto* current = ResultDocument.FindDataList(original))
		definition = *current;
	definition.Key = _listKey->Text;
	definition.ItemType = _listItemType->Text;
	DesignerModel::DesignDataRecord record;
	std::wistringstream stream(_recordFields->Text);
	std::wstring line;
	while (std::getline(stream, line))
	{
		if (!line.empty() && line.back() == L'\r') line.pop_back();
		line = DesignerBindingUtils::Trim(std::move(line));
		if (line.empty()) continue;
		const auto separator = line.find(L'=');
		if (separator == std::wstring::npos)
		{
			ShowValidation(L"记录字段必须使用 Path=Value：" + line, true);
			return false;
		}
		auto path = DesignerDataContextSchemaUtils::NormalizePath(
			line.substr(0, separator));
		const auto duplicate = std::any_of(
			record.Fields.begin(), record.Fields.end(),
			[&](const auto& item) { return Equals(item.first, path); });
		if (!DesignerDataContextSchemaUtils::IsValidPath(path) || duplicate)
		{
			ShowValidation(L"记录字段路径无效或重复：" + path, true);
			return false;
		}
		record.Fields.emplace(std::move(path), line.substr(separator + 1));
	}
	int recordIndex = _recordList->SelectedIndex - 1;
	if (recordIndex < 0) definition.Records.push_back(std::move(record));
	else if (static_cast<size_t>(recordIndex) < definition.Records.size())
		definition.Records[static_cast<size_t>(recordIndex)] = std::move(record);
	else
	{
		ShowValidation(L"记录索引无效。", true);
		return false;
	}
	std::wstring error;
	if (!DesignerModel::DesignDataResourceEditorModel::UpsertDataList(
		ResultDocument, original, std::move(definition), &error))
	{
		ShowValidation(error, true);
		return false;
	}
	const auto key = DesignerBindingUtils::Trim(_listKey->Text);
	if (recordIndex < 0)
		recordIndex = static_cast<int>(ResultDocument.FindDataList(key)->Records.size()) - 1;
	RefreshListList(key);
	LoadSelectedList();
	RefreshRecordList(recordIndex);
	LoadSelectedRecord();
	RefreshSummary();
	ShowValidation(L"DataRecord 已保存并通过字段类型校验。", false);
	return true;
}

void DataResourcesEditorDialog::RemoveRecord()
{
	const auto original = _listList->Text;
	const auto* current = ResultDocument.FindDataList(original);
	const auto index = _recordList->SelectedIndex - 1;
	if (!current || index < 0
		|| static_cast<size_t>(index) >= current->Records.size()) return;
	auto definition = *current;
	definition.Records.erase(definition.Records.begin() + index);
	std::wstring error;
	if (!DesignerModel::DesignDataResourceEditorModel::UpsertDataList(
		ResultDocument, original, std::move(definition), &error))
	{
		ShowValidation(error, true);
		return;
	}
	RefreshRecordList();
	LoadSelectedRecord();
	RefreshSummary();
	ShowValidation(L"DataRecord 已删除。", false);
}

void DataResourcesEditorDialog::RefreshTemplateList(
	const std::wstring& preferred)
{
	_loading = true;
	cui::designer::SetComboBoxItems(*_templateList, { kNewTemplate });
	for (const auto& item : ResultDocument.DataTemplates)
		if (!item.IsImplicit())
			cui::designer::AddComboBoxItem(*_templateList, item.Key);
	SelectComboValue(_templateList,
		preferred.empty() ? kNewTemplate : preferred);
	_loading = false;
}

void DataResourcesEditorDialog::LoadSelectedTemplate()
{
	_loading = true;
	const auto* item = _templateList->Text == kNewTemplate ? nullptr
		: ResultDocument.FindDataTemplate(_templateList->Text);
	_templateKey->Text = item ? item->Key : L"";
	SelectComboValue(_templateDataType, item ? item->DataType : L"");
	_loading = false;
	ShowValidation(item && !item->SourceDictionary.empty()
		? L"外部 DataTemplate：请打开其来源字典编辑。"
		: item ? L"已加载 DataTemplate；视觉树请使用 XAML 编辑器。"
			: L"新模板会生成一个可继续编辑的 Label 视觉根。", false);
}

bool DataResourcesEditorDialog::SaveTemplate()
{
	const auto original = _templateList->Text == kNewTemplate
		? L"" : _templateList->Text;
	DesignerModel::DesignDataTemplate definition;
	if (const auto* current = ResultDocument.FindDataTemplate(original))
		definition = *current;
	definition.Key = _templateKey->Text;
	definition.DataType = _templateDataType->Text;
	if (definition.Template.empty())
	{
		DesignerModel::DesignNode root;
		root.Id = 1;
		root.Name = L"itemRoot";
		root.Type = UIClass::UI_Label;
		const auto* type = ResultDocument.FindDataType(definition.DataType);
		const DesignerDataContextProperty* display = nullptr;
		if (type)
			display = [&]() -> const DesignerDataContextProperty*
			{
				const auto found = std::find_if(
					type->Properties.begin(), type->Properties.end(),
					[](const auto& property)
					{
						return property.CanRead
							&& property.ValueKind != BindingValueKind::Object;
					});
				return found == type->Properties.end() ? nullptr : &*found;
			}();
		if (display)
			root.Bindings[L"Text"] = DesignerDataBinding{
				display->Path, BindingMode::OneWay,
				DataSourceUpdateMode::OnPropertyChanged };
		else root.Properties.Set(L"Text",
			{ { DesignerStyleValueKind::String, L"Item" } });
		definition.Template.push_back(std::move(root));
	}
	std::wstring error;
	if (!DesignerModel::DesignDataResourceEditorModel::UpsertDataTemplate(
		ResultDocument, original, std::move(definition), &error))
	{
		ShowValidation(error, true);
		return false;
	}
	const auto key = DesignerBindingUtils::Trim(_templateKey->Text);
	RefreshTemplateList(key);
	LoadSelectedTemplate();
	RefreshSummary();
	ShowValidation(original.empty()
		? L"DataTemplate 已创建；可在 XAML 编辑器中扩展视觉树。"
		: L"DataTemplate 已保存，ItemTemplate 引用已更新。", false);
	return true;
}

void DataResourcesEditorDialog::RemoveTemplate()
{
	if (_templateList->Text == kNewTemplate) return;
	std::wstring error;
	if (!DesignerModel::DesignDataResourceEditorModel::RemoveDataTemplate(
		ResultDocument, _templateList->Text, &error))
	{
		ShowValidation(error, true);
		return;
	}
	RefreshTemplateList();
	LoadSelectedTemplate();
	RefreshSummary();
	ShowValidation(L"DataTemplate 已删除。", false);
}

void DataResourcesEditorDialog::RefreshSummary()
{
	std::wstring text = L"DataType\r\n";
	for (const auto& type : ResultDocument.DataTypes)
	{
		text += L"  " + type.Name;
		if (!type.SourceDictionary.empty()) text += L"  [外部]";
		text += L"\r\n";
		for (const auto& property : type.Properties)
			text += L"    " + DesignerDataContextSchemaUtils::Describe(property)
				+ L"\r\n";
	}
	text += L"\r\nDataList\r\n";
	for (const auto& list : ResultDocument.DataLists)
	{
		text += L"  " + list.Key + L" : " + list.ItemType + L" ("
			+ std::to_wstring(list.Records.size()) + L" 条)";
		if (!list.SourceDictionary.empty()) text += L"  [外部]";
		text += L"\r\n";
	}
	text += L"\r\nCollectionViewSource（请在 XAML 编辑）\r\n";
	for (const auto& view : ResultDocument.CollectionViews)
	{
		text += L"  " + view.Key + L" : ";
		text += !view.SourceResource.empty()
			? L"{StaticResource " + view.SourceResource + L"}"
			: L"{Binding " + view.SourceBindingPath + L"}";
		text += L"  [分组 " + std::to_wstring(view.GroupDescriptions.size())
			+ L" / 聚合 " + std::to_wstring(view.AggregateDescriptions.size())
			+ L" / 排序 " + std::to_wstring(view.SortDescriptions.size())
			+ L" / 筛选 " + std::to_wstring(view.FilterDescriptions.size()) + L"]";
		if (!view.SourceDictionary.empty()) text += L"  [外部]";
		text += L"\r\n";
	}
	text += L"\r\nDataTemplate（视觉树请在画布/XAML 编辑）\r\n";
	for (const auto& item : ResultDocument.DataTemplates)
		text += L"  " + item.DisplayName() + L" : " + item.DataType + L"\r\n";
	text += L"\r\nItemsPanelTemplate（请在 XAML 编辑）\r\n";
	for (const auto& item : ResultDocument.ItemsPanelTemplates)
	{
		text += L"  " + item.Key + L" : ";
		text += item.Value.Kind == ItemsPanelKind::Wrap ? L"WrapPanel"
			: item.Value.Kind == ItemsPanelKind::VirtualizingStack
				? L"VirtualizingStackPanel" : L"StackPanel";
		if (!item.SourceDictionary.empty()) text += L"  [外部]";
		text += L"\r\n";
	}
	text += L"\r\nGroupStyle（请在 XAML 编辑）\r\n";
	for (const auto& item : ResultDocument.GroupStyles)
	{
		text += L"  " + item.Key;
		if (!item.HeaderTemplate.empty())
			text += L" : " + item.HeaderTemplate;
		if (!item.SourceDictionary.empty()) text += L"  [外部]";
		text += L"\r\n";
	}
	_summary->Text = std::move(text);
}

void DataResourcesEditorDialog::ShowValidation(
	const std::wstring& message, bool isError)
{
	_validation->Text = message;
	_validation->Foreground = isError ? Colors::Red : Colors::DimGrey;
	_validation->InvalidateVisual();
}
