#pragma once

#include "../DesignerTypes.h"
#include <map>
#include <memory>
#include <string>
#include <vector>

class DesignerCanvas;

struct DesignedFormSnapshot
{
	std::wstring Name = L"MainForm";
	std::wstring Text;
	std::wstring FontName;
	float FontSize = 18.0f;
	D2D1_COLOR_F BackColor = Colors::WhiteSmoke;
	D2D1_COLOR_F ForeColor = Colors::Black;
	bool ShowInTaskBar = true;
	bool TopMost = false;
	bool Enable = true;
	bool Visible = true;
	bool VisibleHead = true;
	int HeadHeight = 24;
	bool MinBox = true;
	bool MaxBox = true;
	bool CloseBox = true;
	bool CenterTitle = true;
	bool AllowResize = true;
	POINT Location{ 100, 100 };
	SIZE Size{ 800, 600 };
	std::map<std::wstring, std::wstring> EventHandlers;
};

class PropertyGridBinder
{
public:
	void SetCanvas(DesignerCanvas* canvas);
	DesignerCanvas* GetCanvas() const;

	void BindControl(const std::shared_ptr<DesignerControl>& control);
	bool IsFormBinding() const;
	std::shared_ptr<DesignerControl> GetBoundControl() const;
	Control* GetBoundRuntimeControl() const;

	DesignedFormSnapshot CaptureFormSnapshot() const;
	bool ApplyFormTextProperty(const std::wstring& propertyName, const std::wstring& value) const;
	bool ApplyFormBoolProperty(const std::wstring& propertyName, bool value) const;
	bool IsFormEventEnabled(const std::wstring& eventName) const;
	::Font* GetDesignedFormSharedFont() const;

	void NotifyControlChanged(Control* control) const;
	void ApplyAnchorStylesKeepingBounds(Control* control, uint8_t anchorStyles) const;
	std::wstring MakeUniqueControlName(const std::shared_ptr<DesignerControl>& target, const std::wstring& desired) const;
	void SyncDefaultNameCounter(UIClass type, const std::wstring& name) const;
	void RemoveDesignerControlsInSubtree(Control* root) const;

private:
	DesignerCanvas* _canvas = nullptr;
	std::shared_ptr<DesignerControl> _control;
};