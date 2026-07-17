#include "EventHandlerEditorDialog.h"

#include "DesignerEventCatalog.h"

#include <algorithm>
#include <cwctype>

std::wstring EventHandlerEditorDialog::Trim(const std::wstring& value)
{
	size_t begin = 0;
	while (begin < value.size() && std::iswspace(value[begin])) ++begin;
	size_t end = value.size();
	while (end > begin && std::iswspace(value[end - 1])) --end;
	return value.substr(begin, end - begin);
}

EventHandlerEditorDialog::EventHandlerEditorDialog(
	const DesignerModel::DesignDocumentEventIndex& index,
	const std::wstring& preferredHandler,
	const DesignerModel::DesignEventHandlerCodeInspection* codeInspection)
	: Form(L"重命名事件处理函数", POINT{ 240, 180 }, SIZE{ 640, 390 }),
	  _handlers(index.Handlers()),
	  _codeInspection(codeInspection ? *codeInspection
		: DesignerModel::DesignEventHandlerCodeInspection{})
{
	VisibleHead = true;
	MinBox = false;
	MaxBox = false;
	AllowResize = false;
	BackColor = Colors::WhiteSmoke;

	auto tip = AddControl(new Label(
		L"一次更新文档内所有同名事件引用；合并到已有函数时必须具有相同签名。", 20, 18));
	tip->Size = { 600, 24 };

	auto sourceLabel = AddControl(new Label(L"当前函数", 20, 62));
	sourceLabel->Size = { 110, 24 };
	_source = AddControl(new ComboBox(L"", 136, 56, 474, 30));
	_source->ExpandCount = 10;
	for (const auto& handler : _handlers) _source->Items.push_back(handler.Name);

	auto targetLabel = AddControl(new Label(L"新函数名", 20, 106));
	targetLabel->Size = { 110, 24 };
	_target = AddControl(new TextBox(L"", 136, 100, 474, 30));

	_details = AddControl(new Label(L"", 20, 146));
	_details->Size = { 590, 42 };
	_migrateCode = AddControl(new CheckBox(
		L"同时迁移用户函数体并重新生成代码", 20, 194));
	_migrateCode->Size = { 590, 26 };
	_migrationHint = AddControl(new Label(L"", 42, 224));
	_migrationHint->Size = { 568, 34 };
	_migrationHint->ForeColor = Colors::DimGrey;

	_validation = AddControl(new Label(L"", 20, 266));
	_validation->Size = { 590, 34 };
	_validation->ForeColor = Colors::IndianRed;

	_ok = AddControl(new Button(L"重命名", 20, 326, 120, 34));
	_cancel = AddControl(new Button(L"取消", 152, 326, 120, 34));

	_source->OnSelectionChanged += [this](Control*)
	{
		if (!_loading) LoadSelectedHandler();
	};
	_target->OnTextChanged += [this](Control*, std::wstring, std::wstring)
	{
		if (!_loading) RefreshValidation();
	};
	_migrateCode->OnChecked += [this](Control*)
	{
		if (!_loading) RefreshValidation();
	};
	_ok->OnMouseClick += [this](Control*, MouseEventArgs)
	{
		if (TryAccept()) Close();
	};
	_cancel->OnMouseClick += [this](Control*, MouseEventArgs) { Close(); };

	_loading = true;
	int selectedIndex = 0;
	if (!preferredHandler.empty())
	{
		const auto found = std::find_if(
			_handlers.begin(), _handlers.end(), [&](const auto& handler)
			{
				return handler.Name == preferredHandler;
			});
		if (found != _handlers.end())
			selectedIndex = static_cast<int>(found - _handlers.begin());
	}
	if (!_handlers.empty())
	{
		_source->SelectedIndex = selectedIndex;
		_source->Text = _handlers[static_cast<size_t>(selectedIndex)].Name;
		_target->Text = _source->Text;
	}
	_loading = false;
	LoadSelectedHandler();
}

const DesignerModel::DesignEventHandlerEntry*
EventHandlerEditorDialog::SelectedHandler() const
{
	if (!_source) return nullptr;
	const auto name = Trim(_source->Text);
	const auto found = std::find_if(
		_handlers.begin(), _handlers.end(), [&](const auto& handler)
		{
			return handler.Name == name;
		});
	return found == _handlers.end() ? nullptr : &*found;
}

void EventHandlerEditorDialog::LoadSelectedHandler()
{
	const auto* handler = SelectedHandler();
	_loading = true;
	if (handler && _target) _target->Text = handler->Name;
	_loading = false;
	RefreshValidation();
}

void EventHandlerEditorDialog::RefreshValidation()
{
	_validation->ForeColor = Colors::IndianRed;
	_migrateCode->Enable = false;
	_migrationHint->Text = L"输入有效的新函数名后将检查源码迁移条件。";
	const auto* handler = SelectedHandler();
	if (!handler)
	{
		_details->Text.clear();
		_validation->Text = L"请选择文档中已有的事件处理函数。";
		_migrateCode->Enable = false;
		_migrateCode->Checked = false;
		_migrationHint->Text.clear();
		_ok->Enable = false;
		return;
	}
	_details->Text = L"引用 " + std::to_wstring(handler->ReferenceIndices.size())
		+ L" 处 · void Handler("
		+ std::wstring(handler->ParameterList.begin(), handler->ParameterList.end())
		+ L")";

	const auto targetName = _target ? Trim(_target->Text) : std::wstring{};
	std::wstring error;
	if (!DesignerEventCatalog::ValidateHandlerName(targetName, &error)
		|| targetName.empty())
	{
		_validation->Text = targetName.empty() ? L"新函数名不能为空。" : error;
		_ok->Enable = false;
		return;
	}
	if (targetName == handler->Name)
	{
		_validation->Text = L"请输入不同的新函数名。";
		_ok->Enable = false;
		return;
	}
	const auto existing = std::find_if(
		_handlers.begin(), _handlers.end(), [&](const auto& item)
		{
			return item.Name == targetName;
		});
	if (existing != _handlers.end()
		&& existing->Signature != handler->Signature)
	{
		_validation->Text = L"该名称已用于不同参数签名。";
		_ok->Enable = false;
		return;
	}
	const auto codeEntry = _codeInspection.Handlers.find(handler->Name);
	const bool hasCurrentBody = _codeInspection.Associated
		&& !_codeInspection.Pending
		&& !_codeInspection.Target.OutputBasePath.empty()
		&& !_codeInspection.Target.ClassName.empty()
		&& codeEntry != _codeInspection.Handlers.end()
		&& codeEntry->second.State
			== DesignerModel::DesignEventHandlerCodeState::Current;
	bool targetBodyExists = false;
	if (const auto candidates = _codeInspection.CompatibleUserHandlers.find(
		handler->ParameterList);
		candidates != _codeInspection.CompatibleUserHandlers.end())
	{
		targetBodyExists = std::find(
			candidates->second.begin(), candidates->second.end(), targetName)
			!= candidates->second.end();
	}
	const bool mergingDocumentHandler = existing != _handlers.end();
	const bool canMigrate = hasCurrentBody
		&& !mergingDocumentHandler && !targetBodyExists;
	_migrateCode->Enable = canMigrate;
	if (!canMigrate) _migrateCode->Checked = false;
	if (!hasCurrentBody)
		_migrationHint->Text = L"仅当用户头或源文件中存在唯一兼容定义时可迁移函数体。";
	else if (mergingDocumentHandler)
		_migrationHint->Text = L"合并到已有文档处理函数时保留两个用户函数体。";
	else if (targetBodyExists)
		_migrationHint->Text = L"用户头或源文件中已存在目标同签名定义，不能迁移。";
	else
		_migrationHint->Text = L"迁移会原子更新源码和生成文件，并随 Undo/Redo 往返。";
	_validation->Text = _migrateCode->Checked
		? L"将更新所有引用、迁移现有函数体并重新生成代码。"
		: existing == _handlers.end()
			? L"将更新所有引用；现有用户 C++ 函数体保持原名。"
			: L"将把引用合并到同签名的已有处理函数。";
	_validation->ForeColor = Colors::DimGrey;
	_ok->Enable = true;
}

bool EventHandlerEditorDialog::TryAccept()
{
	const auto* handler = SelectedHandler();
	if (!handler || !_target) return false;
	RefreshValidation();
	if (!_ok->Enable) return false;
	OldName = handler->Name;
	NewName = Trim(_target->Text);
	MigrateUserCode = _migrateCode && _migrateCode->Checked
		&& _migrateCode->Enable;
	Applied = true;
	return true;
}
