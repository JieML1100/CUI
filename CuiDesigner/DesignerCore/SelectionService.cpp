#include "SelectionService.h"

#include <algorithm>

void SelectionService::Clear(std::vector<std::shared_ptr<DesignerControl>>& selectedControls, std::shared_ptr<DesignerControl>& selectedControl) const
{
	for (auto& dc : selectedControls)
	{
		if (dc) dc->IsSelected = false;
	}
	selectedControls.clear();
	selectedControl = nullptr;
}

bool SelectionService::IsSelected(const std::vector<std::shared_ptr<DesignerControl>>& selectedControls, const std::shared_ptr<DesignerControl>& dc) const
{
	if (!dc) return false;
	for (const auto& selected : selectedControls)
	{
		if (selected == dc) return true;
	}
	return false;
}

void SelectionService::SetPrimary(std::shared_ptr<DesignerControl>& selectedControl, const std::shared_ptr<DesignerControl>& dc) const
{
	selectedControl = dc;
}

bool SelectionService::Add(std::vector<std::shared_ptr<DesignerControl>>& selectedControls, std::shared_ptr<DesignerControl>& selectedControl, const std::shared_ptr<DesignerControl>& dc, bool setPrimary) const
{
	if (!dc || !dc->ControlInstance) return false;
	if (!IsSelected(selectedControls, dc))
	{
		if (!selectedControls.empty() && selectedControl && selectedControl->ControlInstance)
		{
			auto* p0 = selectedControl->ControlInstance->Parent;
			auto* p1 = dc->ControlInstance->Parent;
			if (p0 != p1)
			{
				return false;
			}
		}
		selectedControls.push_back(dc);
		dc->IsSelected = true;
	}
	if (setPrimary)
	{
		SetPrimary(selectedControl, dc);
	}
	return true;
}

bool SelectionService::Toggle(std::vector<std::shared_ptr<DesignerControl>>& selectedControls, std::shared_ptr<DesignerControl>& selectedControl, const std::shared_ptr<DesignerControl>& dc) const
{
	if (!dc || !dc->ControlInstance) return false;
	if (!IsSelected(selectedControls, dc))
	{
		return Add(selectedControls, selectedControl, dc, true);
	}

	selectedControls.erase(
		std::remove_if(selectedControls.begin(), selectedControls.end(),
			[&](const std::shared_ptr<DesignerControl>& current) { return current == dc; }),
		selectedControls.end());
	dc->IsSelected = false;

	if (selectedControl == dc)
	{
		selectedControl = selectedControls.empty() ? nullptr : selectedControls.back();
	}
	return true;
}

std::vector<std::wstring> SelectionService::CaptureNames(const std::vector<std::shared_ptr<DesignerControl>>& selectedControls) const
{
	std::vector<std::wstring> names;
	names.reserve(selectedControls.size());
	for (const auto& dc : selectedControls)
	{
		if (!dc) continue;
		names.push_back(dc->Name);
	}
	return names;
}

void SelectionService::RestoreByNames(
	const std::vector<std::shared_ptr<DesignerControl>>& allControls,
	std::vector<std::shared_ptr<DesignerControl>>& selectedControls,
	std::shared_ptr<DesignerControl>& selectedControl,
	const std::vector<std::wstring>& selectionNames,
	const std::wstring& primaryName) const
{
	Clear(selectedControls, selectedControl);

	std::shared_ptr<DesignerControl> primary;
	for (const auto& name : selectionNames)
	{
		for (const auto& dc : allControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Name != name) continue;
			Add(selectedControls, selectedControl, dc, false);
			if (dc->Name == primaryName)
			{
				primary = dc;
			}
			break;
		}
	}

	if (!primary && !primaryName.empty())
	{
		for (const auto& dc : allControls)
		{
			if (!dc || !dc->ControlInstance) continue;
			if (dc->Name != primaryName) continue;
			if (!IsSelected(selectedControls, dc))
			{
				Add(selectedControls, selectedControl, dc, false);
			}
			primary = dc;
			break;
		}
	}

	if (primary)
	{
		SetPrimary(selectedControl, primary);
	}
	else if (!selectedControls.empty())
	{
		SetPrimary(selectedControl, selectedControls.back());
	}
}