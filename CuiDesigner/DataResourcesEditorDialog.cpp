#include "DataResourcesEditorDialog.h"
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
		return _wcsicmp(left.c_str(), right.c_str()) == 0;
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
	: Form(L"编辑数据资源", POINT{ 260, 90 }, SIZE{ 940, 760 }),
	  ResultDocument(document)
{
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->AllowResize = false;
	this->BackColor = Colors::WhiteSmoke;

	auto title = AddControl(new Label(
		L"DataType 定义记录契约；DataList/DataRecord 是设计器与运行时共用的文件数据。", 20, 14));
	title->Size = { 880, 24 };

	auto typeTitle = AddControl(new Label(L"DataType 与字段", 20, 52));
	typeTitle->Font = new ::Font(L"Microsoft YaHei", 17.0f);
	typeTitle->Size = { 240, 28 };
	AddControl(new Label(L"类型", 20, 92))->Size = { 80, 24 };
	_typeList = AddControl(new ComboBox(L"", 100, 86, 410, 30));
	_typeList->ExpandCount = 10;
	AddControl(new Label(L"名称", 20, 132))->Size = { 80, 24 };
	_typeName = AddControl(new TextBox(L"", 100, 126, 240, 30));
	auto saveType = AddControl(new Button(L"保存/重命名类型", 350, 124, 160, 34));

	AddControl(new Label(L"字段", 20, 176))->Size = { 80, 24 };
	_propertyList = AddControl(new ComboBox(L"", 100, 170, 410, 30));
	_propertyList->ExpandCount = 10;
	AddControl(new Label(L"路径", 20, 216))->Size = { 80, 24 };
	_propertyPath = AddControl(new TextBox(L"", 100, 210, 240, 30));
	_propertyKind = AddControl(new ComboBox(L"", 350, 210, 160, 30));
	_propertyKind->Items = ValueKindNames();
	_propertyKind->ExpandCount = 7;
	AddControl(new Label(L"对象", 20, 256))->Size = { 80, 24 };
	_propertyObjectKind = AddControl(new ComboBox(L"", 100, 250, 160, 30));
	_propertyObjectKind->Items = { L"Opaque", L"BindingSource", L"BindingList" };
	_propertyObjectKind->ExpandCount = 3;
	_propertyItemType = AddControl(new ComboBox(L"", 270, 250, 240, 30));
	_propertyItemType->ExpandCount = 10;
	_propertyCanRead = AddControl(new CheckBox(L"可读", 20, 294));
	_propertyCanWrite = AddControl(new CheckBox(L"可写", 105, 294));
	_propertyCanObserve = AddControl(new CheckBox(L"通知", 190, 294));
	auto saveProperty = AddControl(new Button(L"保存字段", 290, 290, 105, 34));
	auto removeProperty = AddControl(new Button(L"删除字段", 405, 290, 105, 34));
	auto removeType = AddControl(new Button(L"删除类型", 405, 330, 105, 32));

	auto listTitle = AddControl(new Label(L"DataList 与记录", 20, 376));
	listTitle->Font = new ::Font(L"Microsoft YaHei", 17.0f);
	listTitle->Size = { 240, 28 };
	AddControl(new Label(L"列表", 20, 416))->Size = { 80, 24 };
	_listList = AddControl(new ComboBox(L"", 100, 410, 410, 30));
	_listList->ExpandCount = 10;
	AddControl(new Label(L"键", 20, 456))->Size = { 80, 24 };
	_listKey = AddControl(new TextBox(L"", 100, 450, 200, 30));
	_listItemType = AddControl(new ComboBox(L"", 310, 450, 200, 30));
	_listItemType->ExpandCount = 10;
	auto saveList = AddControl(new Button(L"保存/重命名列表", 20, 492, 160, 34));
	auto removeList = AddControl(new Button(L"删除列表", 190, 492, 105, 34));

	AddControl(new Label(L"记录", 20, 542))->Size = { 80, 24 };
	_recordList = AddControl(new ComboBox(L"", 100, 536, 410, 30));
	_recordList->ExpandCount = 10;
	AddControl(new Label(L"字段（每行 Path=Value）", 20, 580))->Size = { 300, 24 };
	_recordFields = AddControl(new RichTextBox(L"", 20, 608, 365, 70));
	_recordFields->AllowMultiLine = true;
	_recordFields->BackColor = Colors::White;
	auto saveRecord = AddControl(new Button(L"保存记录", 395, 608, 115, 32));
	auto removeRecord = AddControl(new Button(L"删除记录", 395, 646, 115, 32));

	auto templateTitle = AddControl(new Label(L"DataTemplate", 540, 52));
	templateTitle->Font = new ::Font(L"Microsoft YaHei", 17.0f);
	templateTitle->Size = { 180, 28 };
	_templateList = AddControl(new ComboBox(L"", 540, 86, 365, 30));
	_templateList->ExpandCount = 10;
	_templateKey = AddControl(new TextBox(L"", 540, 126, 175, 30));
	_templateDataType = AddControl(new ComboBox(L"", 725, 126, 180, 30));
	_templateDataType->ExpandCount = 10;
	auto saveTemplate = AddControl(new Button(L"保存/创建模板", 540, 166, 145, 34));
	auto removeTemplate = AddControl(new Button(L"删除模板", 695, 166, 105, 34));
	auto editTemplateHint = AddControl(new Label(L"视觉树：画布/XAML 编辑", 540, 204));
	editTemplateHint->Size = { 300, 24 };

	auto summaryTitle = AddControl(new Label(L"资源摘要", 540, 236));
	summaryTitle->Font = new ::Font(L"Microsoft YaHei", 17.0f);
	summaryTitle->Size = { 180, 28 };
	_summary = AddControl(new RichTextBox(L"", 540, 270, 365, 326));
	_summary->ReadOnly = true;
	_summary->AllowMultiLine = true;
	_summary->BackColor = Colors::White;
	_validation = AddControl(new Label(L"", 540, 606));
	_validation->Size = { 365, 56 };
	auto ok = AddControl(new Button(L"确定", 650, 680, 120, 36));
	auto cancel = AddControl(new Button(L"取消", 785, 680, 120, 36));

	_typeList->OnSelectionChanged += [this](Control*) {
		if (!_loading) LoadSelectedType();
	};
	_propertyList->OnSelectionChanged += [this](Control*) {
		if (!_loading) LoadSelectedProperty();
	};
	_propertyKind->OnSelectionChanged += [this](Control*) {
		if (!_loading) RefreshObjectEditors();
	};
	_propertyObjectKind->OnSelectionChanged += [this](Control*) {
		if (!_loading) RefreshObjectEditors();
	};
	_listList->OnSelectionChanged += [this](Control*) {
		if (!_loading) LoadSelectedList();
	};
	_recordList->OnSelectionChanged += [this](Control*) {
		if (!_loading) LoadSelectedRecord();
	};
	_templateList->OnSelectionChanged += [this](Control*) {
		if (!_loading) LoadSelectedTemplate();
	};
	saveType->OnMouseClick += [this](Control*, MouseEventArgs) { (void)SaveType(); };
	removeType->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveType(); };
	saveProperty->OnMouseClick += [this](Control*, MouseEventArgs) { (void)SaveProperty(); };
	removeProperty->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveProperty(); };
	saveList->OnMouseClick += [this](Control*, MouseEventArgs) { (void)SaveList(); };
	removeList->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveList(); };
	saveRecord->OnMouseClick += [this](Control*, MouseEventArgs) { (void)SaveRecord(); };
	removeRecord->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveRecord(); };
	saveTemplate->OnMouseClick += [this](Control*, MouseEventArgs) { (void)SaveTemplate(); };
	removeTemplate->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveTemplate(); };
	ok->OnMouseClick += [this](Control*, MouseEventArgs) {
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
	cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
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
	const auto found = std::find_if(combo->Items.begin(), combo->Items.end(),
		[&](const auto& item) { return Equals(item, value); });
	const auto index = found == combo->Items.end() ? 0
		: static_cast<int>(found - combo->Items.begin());
	combo->SelectedIndex = combo->Items.empty() ? -1 : index;
	combo->Text = combo->Items.empty() ? L"" : combo->Items[index];
}

void DataResourcesEditorDialog::RefreshItemTypeChoices()
{
	std::vector<std::wstring> choices;
	for (const auto& type : ResultDocument.DataTypes) choices.push_back(type.Name);
	_propertyItemType->Items = choices;
	_listItemType->Items = std::move(choices);
	_templateDataType->Items = _propertyItemType->Items;
}

void DataResourcesEditorDialog::RefreshTypeList(const std::wstring& preferred)
{
	_loading = true;
	_typeList->Items = { kNewType };
	for (const auto& type : ResultDocument.DataTypes)
		_typeList->Items.push_back(type.Name);
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
	_propertyList->Items = { kNewProperty };
	if (const auto* type = ResultDocument.FindDataType(_typeList->Text))
		for (const auto& property : type->Properties)
			_propertyList->Items.push_back(property.Path);
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
	_propertyCanRead->Checked = property ? property->CanRead : true;
	_propertyCanWrite->Checked = property ? property->CanWrite : true;
	_propertyCanObserve->Checked = property ? property->CanObserve : true;
	_loading = false;
	RefreshObjectEditors();
}

void DataResourcesEditorDialog::RefreshObjectEditors()
{
	const bool object = _propertyKind->Text == L"Object";
	_propertyObjectKind->Enable = object;
	_propertyItemType->Enable = object
		&& (_propertyObjectKind->Text == L"BindingList"
			|| _propertyObjectKind->Text == L"BindingSource");
	if (!_propertyItemType->Enable && !_loading) _propertyItemType->Text = L"";
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
	property.CanRead = _propertyCanRead->Checked;
	property.CanWrite = _propertyCanWrite->Checked;
	property.CanObserve = _propertyCanObserve->Checked;
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
	_listList->Items = { kNewList };
	for (const auto& list : ResultDocument.DataLists)
		_listList->Items.push_back(list.Key);
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
	_recordList->Items = { kNewRecord };
	if (const auto* list = ResultDocument.FindDataList(_listList->Text))
		for (size_t index = 0; index < list->Records.size(); ++index)
			_recordList->Items.push_back(RecordName(index));
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
	_templateList->Items = { kNewTemplate };
	for (const auto& item : ResultDocument.DataTemplates)
		if (!item.IsImplicit()) _templateList->Items.push_back(item.Key);
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
			root.Bindings["Text"] = DesignerModel::DesignValue{
				{ "source", Convert::UnicodeToUtf8(display->Path) },
				{ "mode", static_cast<int>(BindingMode::OneWay) },
				{ "updateMode", static_cast<int>(
					DataSourceUpdateMode::OnPropertyChanged) }
			};
		else root.Props["text"] = "Item";
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
		text += L"  HeaderHeight=" + std::to_wstring(item.HeaderHeight);
		if (!item.SourceDictionary.empty()) text += L"  [外部]";
		text += L"\r\n";
	}
	_summary->Text = std::move(text);
}

void DataResourcesEditorDialog::ShowValidation(
	const std::wstring& message, bool isError)
{
	_validation->Text = message;
	_validation->ForeColor = isError ? Colors::Red : Colors::DimGrey;
	_validation->InvalidateVisual();
}
