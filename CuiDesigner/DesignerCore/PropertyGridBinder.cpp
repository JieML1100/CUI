#include "PropertyGridBinder.h"
#include "../DesignerCanvas.h"
#include "../../CUI/GUI/SplitContainer.h"

void PropertyGridBinder::SetCanvas(DesignerCanvas* canvas)
{
	_canvas = canvas;
}

DesignerCanvas* PropertyGridBinder::GetCanvas() const
{
	return _canvas;
}

void PropertyGridBinder::BindControl(const std::shared_ptr<DesignerControl>& control)
{
	_control = control;
}

bool PropertyGridBinder::IsFormBinding() const
{
	return !_control;
}

std::shared_ptr<DesignerControl> PropertyGridBinder::GetBoundControl() const
{
	return _control;
}

Control* PropertyGridBinder::GetBoundRuntimeControl() const
{
	return _control ? _control->ControlInstance : nullptr;
}

DesignedFormSnapshot PropertyGridBinder::CaptureFormSnapshot() const
{
	DesignedFormSnapshot snapshot;
	if (!_canvas)
	{
		return snapshot;
	}

	snapshot.Name = _canvas->GetDesignedFormName();
	snapshot.Text = _canvas->GetDesignedFormText();
	snapshot.FontName = _canvas->GetDesignedFormFontName();
	snapshot.FontSize = _canvas->GetDesignedFormFontSize();
	snapshot.BackColor = _canvas->GetDesignedFormBackColor();
	snapshot.ForeColor = _canvas->GetDesignedFormForeColor();
	snapshot.ShowInTaskBar = _canvas->GetDesignedFormShowInTaskBar();
	snapshot.TopMost = _canvas->GetDesignedFormTopMost();
	snapshot.Enable = _canvas->GetDesignedFormEnable();
	snapshot.Visible = _canvas->GetDesignedFormVisible();
	snapshot.VisibleHead = _canvas->GetDesignedFormVisibleHead();
	snapshot.HeadHeight = _canvas->GetDesignedFormHeadHeight();
	snapshot.MinBox = _canvas->GetDesignedFormMinBox();
	snapshot.MaxBox = _canvas->GetDesignedFormMaxBox();
	snapshot.CloseBox = _canvas->GetDesignedFormCloseBox();
	snapshot.CenterTitle = _canvas->GetDesignedFormCenterTitle();
	snapshot.AllowResize = _canvas->GetDesignedFormAllowResize();
	snapshot.Location = _canvas->GetDesignedFormLocation();
	snapshot.Size = _canvas->GetDesignedFormSize();
	snapshot.EventHandlers = _canvas->GetDesignedFormEventHandlers();
	return snapshot;
}

bool PropertyGridBinder::ApplyFormTextProperty(const std::wstring& propertyName, const std::wstring& value) const
{
	if (!_canvas)
	{
		return false;
	}

	if (propertyName == L"Name") _canvas->SetDesignedFormName(value);
	else if (propertyName == L"Text") _canvas->SetDesignedFormText(value);
	else if (propertyName == L"FontName") _canvas->SetDesignedFormFontName(value);
	else if (propertyName == L"FontSize") _canvas->SetDesignedFormFontSize((float)std::stod(value));
	else if (propertyName == L"BackColor") return false;
	else if (propertyName == L"ForeColor") return false;
	else if (propertyName == L"HeadHeight") _canvas->SetDesignedFormHeadHeight(std::stoi(value));
	else if (propertyName == L"X")
	{
		auto p = _canvas->GetDesignedFormLocation();
		p.x = std::stoi(value);
		_canvas->SetDesignedFormLocation(p);
	}
	else if (propertyName == L"Y")
	{
		auto p = _canvas->GetDesignedFormLocation();
		p.y = std::stoi(value);
		_canvas->SetDesignedFormLocation(p);
	}
	else if (propertyName == L"Width")
	{
		auto s = _canvas->GetDesignedFormSize();
		s.cx = std::stoi(value);
		_canvas->SetDesignedFormSize(s);
	}
	else if (propertyName == L"Height")
	{
		auto s = _canvas->GetDesignedFormSize();
		s.cy = std::stoi(value);
		_canvas->SetDesignedFormSize(s);
	}
	else
	{
		return false;
	}

	return true;
}

bool PropertyGridBinder::ApplyFormBoolProperty(const std::wstring& propertyName, bool value) const
{
	if (!_canvas)
	{
		return false;
	}

	if (propertyName == L"VisibleHead") _canvas->SetDesignedFormVisibleHead(value);
	else if (propertyName == L"MinBox") _canvas->SetDesignedFormMinBox(value);
	else if (propertyName == L"MaxBox") _canvas->SetDesignedFormMaxBox(value);
	else if (propertyName == L"CloseBox") _canvas->SetDesignedFormCloseBox(value);
	else if (propertyName == L"CenterTitle") _canvas->SetDesignedFormCenterTitle(value);
	else if (propertyName == L"AllowResize") _canvas->SetDesignedFormAllowResize(value);
	else if (propertyName == L"ShowInTaskBar") _canvas->SetDesignedFormShowInTaskBar(value);
	else if (propertyName == L"TopMost") _canvas->SetDesignedFormTopMost(value);
	else if (propertyName == L"Enable") _canvas->SetDesignedFormEnable(value);
	else if (propertyName == L"Visible") _canvas->SetDesignedFormVisible(value);
	else return false;
	return true;
}

bool PropertyGridBinder::IsFormEventEnabled(const std::wstring& eventName) const
{
	return _canvas && _canvas->GetDesignedFormEventEnabled(eventName);
}

::Font* PropertyGridBinder::GetDesignedFormSharedFont() const
{
	return _canvas ? _canvas->GetDesignedFormSharedFont() : nullptr;
}

void PropertyGridBinder::NotifyControlChanged(Control* control) const
{
	if (!control)
	{
		return;
	}

	if (auto* panel = dynamic_cast<Panel*>(control->Parent))
	{
		panel->InvalidateLayout();
		panel->PerformLayout();
	}
	if (auto* split = dynamic_cast<SplitContainer*>(control))
	{
		split->RefreshSplitterLayout();
	}
	else if (auto* panel = dynamic_cast<Panel*>(control))
	{
		panel->InvalidateLayout();
		panel->PerformLayout();
	}
	if (_canvas)
	{
		_canvas->ClampControlToDesignSurface(control);
	}
	control->PostRender();
}

void PropertyGridBinder::ApplyAnchorStylesKeepingBounds(Control* control, uint8_t anchorStyles) const
{
	if (!control)
	{
		return;
	}
	if (_canvas)
	{
		_canvas->ApplyAnchorStylesKeepingBounds(control, anchorStyles);
	}
	else
	{
		control->AnchorStyles = anchorStyles;
	}
}

std::wstring PropertyGridBinder::MakeUniqueControlName(const std::shared_ptr<DesignerControl>& target, const std::wstring& desired) const
{
	return _canvas ? _canvas->MakeUniqueControlName(target, desired) : desired;
}

void PropertyGridBinder::SyncDefaultNameCounter(UIClass type, const std::wstring& name) const
{
	if (_canvas)
	{
		_canvas->SyncDefaultNameCounter(type, name);
	}
}

void PropertyGridBinder::RemoveDesignerControlsInSubtree(Control* root) const
{
	if (_canvas)
	{
		_canvas->RemoveDesignerControlsInSubtree(root);
	}
}