#include "CodeBehindExportDialog.h"

#include <cwctype>

std::wstring CodeBehindExportDialog::Trim(const std::wstring& value)
{
	size_t begin = 0;
	while (begin < value.size() && std::iswspace(value[begin])) ++begin;
	size_t end = value.size();
	while (end > begin && std::iswspace(value[end - 1])) --end;
	return value.substr(begin, end - begin);
}

CodeBehindExportDialog::CodeBehindExportDialog(
	const DesignerModel::DesignCodeBehindModel& existingAssociation,
	const std::wstring& suggestedClassName,
	std::wstring outputBasePath,
	std::wstring designFilePath)
	: Form(L"配置 C++ code-behind", POINT{ 220, 160 }, SIZE{ 760, 370 }),
	  _existingAssociation(existingAssociation),
	  _outputBasePath(std::move(outputBasePath)),
	  _designFilePath(std::move(designFilePath))
{
	VisibleHead = true;
	MinBox = false;
	MaxBox = false;
	AllowResize = false;
	BackColor = Colors::WhiteSmoke;

	auto tip = AddControl(new Label(
		L"确认生成代码使用的 C++ 类。输出文件名与限定类名彼此独立。", 20, 16));
	tip->Size = { 710, 24 };

	auto currentLabel = AddControl(new Label(L"当前 x:Class", 20, 58));
	currentLabel->Size = { 120, 24 };
	auto currentValue = AddControl(new Label(
		_existingAssociation.ClassName.empty()
			? L"（尚未关联）" : _existingAssociation.ClassName,
		146, 58));
	currentValue->Size = { 580, 24 };
	currentValue->ForeColor = Colors::DimGrey;

	auto outputLabel = AddControl(new Label(L"输出基路径", 20, 96));
	outputLabel->Size = { 120, 24 };
	auto outputValue = AddControl(new Label(_outputBasePath, 146, 96));
	outputValue->Size = { 580, 42 };
	outputValue->ForeColor = Colors::DimGrey;

	auto classLabel = AddControl(new Label(L"C++ 类名", 20, 150));
	classLabel->Size = { 120, 24 };
	_className = AddControl(new TextBox(suggestedClassName, 146, 144, 580, 32));

	_association = AddControl(new Label(L"", 20, 194));
	_association->Size = { 706, 42 };
	_validation = AddControl(new Label(L"", 20, 240));
	_validation->Size = { 706, 50 };

	_ok = AddControl(new Button(L"导出", 20, 310, 128, 36));
	auto cancel = AddControl(new Button(L"取消", 160, 310, 128, 36));

	_className->OnTextChanged +=
		[this](Control*, std::wstring, std::wstring) { RefreshValidation(); };
	_ok->OnMouseClick += [this](Control*, MouseEventArgs)
	{
		if (TryAccept()) Close();
	};
	cancel->OnMouseClick += [this](Control*, MouseEventArgs) { Close(); };

	RefreshValidation();
}

void CodeBehindExportDialog::RefreshValidation()
{
	DesignerModel::DesignCodeExportPlan candidate;
	std::wstring error;
	const auto requestedClass = _className
		? Trim(_className->Text) : std::wstring{};
	if (!DesignerModel::DesignCodeGenerationService::BuildCodeExportPlan(
		_existingAssociation, requestedClass,
		_outputBasePath, _designFilePath, candidate, &error))
	{
		Plan = {};
		_association->Text.clear();
		_validation->Text = error.empty()
			? L"类名或输出关联无效。" : std::move(error);
		_validation->ForeColor = Colors::IndianRed;
		_ok->Text = L"导出";
		_ok->Enable = false;
		return;
	}

	Plan = std::move(candidate);
	_association->Text = Plan.Association.RelativeBasePath.empty()
		? L"设计文件尚未保存；首次保存时将写入相对 d:CodeBehind。"
		: L"d:CodeBehind = " + Plan.Association.RelativeBasePath;
	_association->ForeColor = Colors::DimGrey;
	if (Plan.MigratesClass)
	{
		_validation->Text =
			L"将迁移 x:Class。Designer 不会改写旧用户函数体；若目标属于旧类，导出会安全拒绝。";
		_validation->ForeColor = Colors::DarkOrange;
		_ok->Text = L"迁移并导出";
	}
	else if (Plan.CreatesAssociation)
	{
		_validation->Text = L"将创建新的 code-behind 关联和用户代码文件。";
		_validation->ForeColor = Colors::DimGrey;
		_ok->Text = L"创建并导出";
	}
	else
	{
		_validation->Text = Plan.ChangesRelativeOutput
			? L"将保留 x:Class，并把 d:CodeBehind 更新到新输出位置。"
			: L"将保留当前 x:Class 和输出关联，并安全重新生成代码。";
		_validation->ForeColor = Colors::DimGrey;
		_ok->Text = L"导出";
	}
	_ok->Enable = true;
}

bool CodeBehindExportDialog::TryAccept()
{
	RefreshValidation();
	if (!_ok || !_ok->Enable) return false;
	ClassName = Plan.Association.ClassName;
	Applied = true;
	return true;
}
