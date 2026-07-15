#include "DataContextSchemaEditorDialog.h"
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
	: Form(L"编辑 DataContext Schema", POINT{ 350, 180 }, SIZE{ 760, 650 }),
	  ResultSchema(schema),
	  _runtimeSource(runtimeSource)
{
	DesignerDataContextSchemaUtils::Canonicalize(ResultSchema);
	this->VisibleHead = true;
	this->MinBox = false;
	this->MaxBox = false;
	this->AllowResize = false;
	this->BackColor = Colors::WhiteSmoke;

	auto tip = this->AddControl(new Label(
		L"声明绑定可用的点分源路径、值类型以及读写/通知能力。", 20, 16));
	tip->Size = { 710, 24 };

	auto existingLabel = this->AddControl(new Label(L"现有路径", 20, 62));
	existingLabel->Size = { 110, 24 };
	_existingPath = this->AddControl(new ComboBox(L"", 140, 56, 580, 30));
	_existingPath->ExpandCount = 10;

	auto pathLabel = this->AddControl(new Label(L"属性路径", 20, 108));
	pathLabel->Size = { 110, 24 };
	_path = this->AddControl(new TextBox(L"", 140, 102, 580, 30));

	auto kindLabel = this->AddControl(new Label(L"值类型", 20, 154));
	kindLabel->Size = { 110, 24 };
	_kind = this->AddControl(new ComboBox(L"", 140, 148, 220, 30));
	_kind->ExpandCount = 8;
	auto kindNames = ValueKindNames();
	_kind->Items = kindNames;

	_canRead = this->AddControl(new CheckBox(L"可读", 400, 151));
	_canWrite = this->AddControl(new CheckBox(L"可写", 500, 151));
	_canObserve = this->AddControl(new CheckBox(L"变更通知", 600, 151));

	auto save = this->AddControl(new Button(L"保存属性", 20, 204, 125, 34));
	auto remove = this->AddControl(new Button(L"删除属性", 158, 204, 125, 34));
	auto importRuntime = this->AddControl(new Button(
		_runtimeSource ? L"从运行时源导入" : L"未连接运行时源",
		296, 204, 150, 34));
	importRuntime->Enable = _runtimeSource != nullptr;
	_validation = this->AddControl(new Label(L"", 460, 210));
	_validation->Size = { 260, 40 };

	auto summaryLabel = this->AddControl(new Label(L"Schema 属性树（R=可读，W=可写，O=通知）", 20, 268));
	summaryLabel->Size = { 700, 24 };
	_summary = this->AddControl(new RichTextBox(L"", 20, 296, 700, 230));
	_summary->ReadOnly = true;
	_summary->AllowMultiLine = true;
	_summary->BackColor = Colors::White;
	_summary->FocusedColor = Colors::White;

	auto ok = this->AddControl(new Button(L"确定", 20, 548, 120, 36));
	auto cancel = this->AddControl(new Button(L"取消", 152, 548, 120, 36));

	_existingPath->OnSelectionChanged += [this](Control*) {
		if (!_loading) LoadSelectedProperty();
	};
	save->OnMouseClick += [this](Control*, MouseEventArgs) { (void)SaveProperty(); };
	remove->OnMouseClick += [this](Control*, MouseEventArgs) { RemoveProperty(); };
	importRuntime->OnMouseClick += [this](Control*, MouseEventArgs) { ImportRuntimeSchema(); };
	ok->OnMouseClick += [this](Control*, MouseEventArgs) {
		std::wstring error;
		if (!DesignerDataContextSchemaUtils::Validate(ResultSchema, &error))
		{
			ShowValidation(error, true);
			return;
		}
		Applied = true;
		this->Close();
	};
	cancel->OnMouseClick += [this](Control*, MouseEventArgs) {
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
	auto& items = combo->Items;
	auto it = std::find(items.begin(), items.end(), value);
	const int index = it == items.end() ? 0 : static_cast<int>(it - items.begin());
	combo->SelectedIndex = index;
	combo->Text = items.empty() ? L"" : items[static_cast<size_t>(index)];
}

void DataContextSchemaEditorDialog::RefreshPathOptions(const std::wstring& preferredPath)
{
	_loading = true;
	auto& items = _existingPath->Items;
	items.clear();
	items.push_back(kNewProperty);
	for (const auto& path : DesignerDataContextSchemaUtils::GetPaths(ResultSchema))
		items.push_back(path);
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
	_canRead->Checked = property ? property->CanRead : true;
	_canWrite->Checked = property ? property->CanWrite : true;
	_canObserve->Checked = property ? property->CanObserve : true;
	_loading = false;
	ShowValidation(property ? L"已加载属性。" : L"填写后点击“保存属性”。", false);
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
	_validation->ForeColor = isError ? Colors::Red : Colors::DimGrey;
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
	property.CanRead = _canRead->Checked;
	property.CanWrite = _canWrite->Checked;
	property.CanObserve = _canObserve->Checked;

	auto candidate = ResultSchema;
	const auto selected = _existingPath->Text;
	auto selectedProperty = std::find_if(candidate.begin(), candidate.end(),
		[&](const DesignerDataContextProperty& item)
		{
			return selected != kNewProperty
				&& _wcsicmp(item.Path.c_str(), selected.c_str()) == 0;
		});
	auto pathCollision = std::find_if(candidate.begin(), candidate.end(),
		[&](const DesignerDataContextProperty& item)
		{
			return _wcsicmp(item.Path.c_str(), property.Path.c_str()) == 0;
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
			return _wcsicmp(property.Path.c_str(), selected.c_str()) == 0;
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
				return _wcsicmp(candidate.Path.c_str(), property.Path.c_str()) == 0;
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
