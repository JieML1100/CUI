#pragma once

#include "../DesignerTypes.h"
#include <memory>
#include <string>
#include <vector>

class SelectionService
{
public:
	void Clear(std::vector<std::shared_ptr<DesignerControl>>& selectedControls, std::shared_ptr<DesignerControl>& selectedControl) const;
	bool IsSelected(const std::vector<std::shared_ptr<DesignerControl>>& selectedControls, const std::shared_ptr<DesignerControl>& dc) const;
	void SetPrimary(std::shared_ptr<DesignerControl>& selectedControl, const std::shared_ptr<DesignerControl>& dc) const;
	bool Add(std::vector<std::shared_ptr<DesignerControl>>& selectedControls, std::shared_ptr<DesignerControl>& selectedControl, const std::shared_ptr<DesignerControl>& dc, bool setPrimary) const;
	bool Toggle(std::vector<std::shared_ptr<DesignerControl>>& selectedControls, std::shared_ptr<DesignerControl>& selectedControl, const std::shared_ptr<DesignerControl>& dc) const;
	std::vector<std::wstring> CaptureNames(const std::vector<std::shared_ptr<DesignerControl>>& selectedControls) const;
	void RestoreByNames(
		const std::vector<std::shared_ptr<DesignerControl>>& allControls,
		std::vector<std::shared_ptr<DesignerControl>>& selectedControls,
		std::shared_ptr<DesignerControl>& selectedControl,
		const std::vector<std::wstring>& selectionNames,
		const std::wstring& primaryName) const;
};