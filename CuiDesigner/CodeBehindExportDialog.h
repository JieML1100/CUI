#pragma once

#include "DesignerModel/DesignCodeGenerationService.h"
#include "../CUI/include/Window.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/Button.h"

/** Confirms the C++ class identity for an already selected export base. */
class CodeBehindExportDialog final : public Window
{
public:
	bool Applied = false;
	std::wstring ClassName;
	DesignerModel::DesignCodeExportPlan Plan;

	CodeBehindExportDialog(
		const DesignerModel::DesignCodeBehindModel& existingAssociation,
		const std::wstring& suggestedClassName,
		std::wstring outputBasePath,
		std::wstring designFilePath);
	~CodeBehindExportDialog() = default;
	[[nodiscard]] bool CanApply() const noexcept
	{
		return _ok && _ok->IsEnabled;
	}
	[[nodiscard]] std::wstring ValidationMessage() const
	{
		return _validation ? _validation->Text : std::wstring{};
	}

private:
	DesignerModel::DesignCodeBehindModel _existingAssociation;
	std::wstring _outputBasePath;
	std::wstring _designFilePath;
	TextBox* _className = nullptr;
	Label* _association = nullptr;
	Label* _validation = nullptr;
	Button* _ok = nullptr;

	void RefreshValidation();
	bool TryAccept();
	static std::wstring Trim(const std::wstring& value);
};
