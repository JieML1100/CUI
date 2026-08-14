#include "CodeBehindExportDialog.h"
#include "ProgrammaticControlFactory.h"

#include "../CUI/include/Button.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/Panel.h"
#include "../CUI/include/TextBox.h"

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
	: Window(),
	  _existingAssociation(existingAssociation),
	  _outputBasePath(std::move(outputBasePath)),
	  _designFilePath(std::move(designFilePath))
{
	Title = L"配置 C++ code-behind";
	Left = 220.0f;
	Top = 160.0f;
	Width = 760.0f;
	Height = 370.0f;
	ResizeMode = ::ResizeMode::NoResize;
	Background = Colors::WhiteSmoke;
	auto contentOwner = std::make_unique<Panel>();
	contentOwner->BorderThickness = 0.0f;
	contentOwner->Background = D2D1_COLOR_F{ 0, 0, 0, 0 };
	auto* contentRoot = static_cast<Panel*>(SetVisualContent(std::move(contentOwner)));
	auto addContent = [contentRoot](auto* child) { return contentRoot->AdoptVisualChild(child); };

	auto tip = addContent(cui::designer::NewControl<Label>(
		L"确认生成代码使用的 C++ 类。输出文件名与限定类名彼此独立。", 20, 16));
	tip->Width = 710.0f;
	tip->Height = 24.0f;

	auto currentLabel = addContent(cui::designer::NewControl<Label>(L"当前 x:Class", 20, 58));
	currentLabel->Width = 120.0f;
	currentLabel->Height = 24.0f;
	auto currentValue = addContent(cui::designer::NewControl<Label>(
		_existingAssociation.ClassName.empty()
			? L"（尚未关联）" : _existingAssociation.ClassName,
		146, 58));
	currentValue->Width = 580.0f;
	currentValue->Height = 24.0f;
	currentValue->Foreground = Colors::DimGrey;

	auto outputLabel = addContent(cui::designer::NewControl<Label>(L"输出基路径", 20, 96));
	outputLabel->Width = 120.0f;
	outputLabel->Height = 24.0f;
	auto outputValue = addContent(cui::designer::NewControl<Label>(_outputBasePath, 146, 96));
	outputValue->Width = 580.0f;
	outputValue->Height = 42.0f;
	outputValue->Foreground = Colors::DimGrey;

	auto classLabel = addContent(cui::designer::NewControl<Label>(L"C++ 类名", 20, 150));
	classLabel->Width = 120.0f;
	classLabel->Height = 24.0f;
	_className = addContent(cui::designer::NewControl<TextBox>(suggestedClassName, 146, 144, 580, 32));

	_association = addContent(cui::designer::NewControl<Label>(L"", 20, 194));
	_association->Width = 706.0f;
	_association->Height = 42.0f;
	_validation = addContent(cui::designer::NewControl<Label>(L"", 20, 240));
	_validation->Width = 706.0f;
	_validation->Height = 50.0f;

	_ok = addContent(cui::designer::NewControl<Button>(L"导出", 20, 310, 128, 36));
	auto cancel = addContent(cui::designer::NewControl<Button>(L"取消", 160, 310, 128, 36));

	_className->OnTextChanged +=
		[this](Control*, TextChangedEventArgs&) { RefreshValidation(); };
	_ok->Click += [this](Control*, RoutedEventArgs&)
	{
		if (TryAccept()) Close();
	};
	cancel->Click += [this](Control*, RoutedEventArgs&) { Close(); };

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
		_validation->Foreground = Colors::IndianRed;
		_ok->SetContent(BindingValue(L"导出"));
		_ok->IsEnabled = false;
		return;
	}

	Plan = std::move(candidate);
	_association->Text = Plan.Association.RelativeBasePath.empty()
		? L"设计文件尚未保存；首次保存时将写入相对 d:CodeBehind。"
		: L"d:CodeBehind = " + Plan.Association.RelativeBasePath;
	_association->Foreground = Colors::DimGrey;
	if (Plan.MigratesClass)
	{
		_validation->Text =
			L"将迁移 x:Class。Designer 不会改写旧用户函数体；若目标属于旧类，导出会安全拒绝。";
		_validation->Foreground = Colors::DarkOrange;
		_ok->SetContent(BindingValue(L"迁移并导出"));
	}
	else if (Plan.CreatesAssociation)
	{
		_validation->Text = L"将创建新的 code-behind 关联和用户代码文件。";
		_validation->Foreground = Colors::DimGrey;
		_ok->SetContent(BindingValue(L"创建并导出"));
	}
	else
	{
		_validation->Text = Plan.ChangesRelativeOutput
			? L"将保留 x:Class，并把 d:CodeBehind 更新到新输出位置。"
			: L"将保留当前 x:Class 和输出关联，并安全重新生成代码。";
		_validation->Foreground = Colors::DimGrey;
		_ok->SetContent(BindingValue(L"导出"));
	}
	_ok->IsEnabled = true;
}

bool CodeBehindExportDialog::TryAccept()
{
	RefreshValidation();
	if (!_ok || !_ok->IsEnabled) return false;
	ClassName = Plan.Association.ClassName;
	Applied = true;
	return true;
}
