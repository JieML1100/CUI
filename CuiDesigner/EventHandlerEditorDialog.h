#pragma once

#include "DesignerModel/DesignDocumentEventIndex.h"
#include "../CUI/include/Form.h"
#include "../CUI/include/Label.h"
#include "../CUI/include/ComboBox.h"
#include "../CUI/include/TextBox.h"
#include "../CUI/include/CheckBox.h"
#include "../CUI/include/Button.h"
#include "DesignerModel/DesignCodeGenerationService.h"

#include <vector>

/** Document-wide, signature-aware event handler rename dialog. */
class EventHandlerEditorDialog final : public Form
{
public:
	bool Applied = false;
	std::wstring OldName;
	std::wstring NewName;
	bool MigrateUserCode = false;

	explicit EventHandlerEditorDialog(
		const DesignerModel::DesignDocumentEventIndex& index,
		const std::wstring& preferredHandler = {},
		const DesignerModel::DesignEventHandlerCodeInspection* codeInspection = nullptr);
	~EventHandlerEditorDialog() = default;

private:
	std::vector<DesignerModel::DesignEventHandlerEntry> _handlers;
	DesignerModel::DesignEventHandlerCodeInspection _codeInspection;
	ComboBox* _source = nullptr;
	TextBox* _target = nullptr;
	Label* _details = nullptr;
	Label* _validation = nullptr;
	CheckBox* _migrateCode = nullptr;
	Label* _migrationHint = nullptr;
	Button* _ok = nullptr;
	Button* _cancel = nullptr;
	bool _loading = false;

	const DesignerModel::DesignEventHandlerEntry* SelectedHandler() const;
	void LoadSelectedHandler();
	void RefreshValidation();
	bool TryAccept();
	static std::wstring Trim(const std::wstring& value);
};
