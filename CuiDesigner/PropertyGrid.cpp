#include "PropertyGrid.h"
#include "../CUI/GUI/Form.h"
#include "ComboBoxItemsEditorDialog.h"
#include "GridViewColumnsEditorDialog.h"
#include "TabControlPagesEditorDialog.h"
#include "ToolBarButtonsEditorDialog.h"
#include "TreeViewNodesEditorDialog.h"
#include "GridPanelDefinitionsEditorDialog.h"
#include "DesignerCanvas.h"
#include <sstream>
#include <iomanip>

PropertyGrid::PropertyGrid(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	this->BackColor = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
	this->Boder = 1.0f;
	
	// 标题
	_titleLabel = new Label(L"属性", 10, 10);
	_titleLabel->Size = {width - 20, 25};
	_titleLabel->Font = new ::Font(L"Microsoft YaHei", 16.0f);
	this->AddControl(_titleLabel);
}

PropertyGrid::~PropertyGrid()
{
}

void PropertyGrid::CreatePropertyItem(std::wstring propertyName, std::wstring value, int& yOffset)
{
	int width = this->Width;
	
	// 属性名标签
	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = {(width - 30) / 2, 20};
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	this->AddControl(nameLabel);
	// 确保ParentForm已设置
	nameLabel->ParentForm = this->ParentForm;
	
	// 值文本框
	auto valueTextBox = new TextBox(L"", (width - 30) / 2 + 15, yOffset, (width - 30) / 2, 20);
	valueTextBox->Text = value;
	
	// 文本改变事件
	valueTextBox->OnTextChanged += [this, propertyName](Control* sender, std::wstring oldText, std::wstring newText) {
		UpdatePropertyFromTextBox(propertyName, newText);
	};
	
	this->AddControl(valueTextBox);
	// 确保ParentForm已设置（关键！）
	valueTextBox->ParentForm = this->ParentForm;
	
	auto item = new PropertyItem(propertyName, nameLabel, valueTextBox);
	_items.push_back(item);
	
	yOffset += 25;
}

void PropertyGrid::CreateBoolPropertyItem(std::wstring propertyName, bool value, int& yOffset)
{
	int width = this->Width;

	// 属性名标签
	auto nameLabel = new Label(propertyName, 10, yOffset);
	nameLabel->Size = {(width - 30) / 2, 20};
	nameLabel->Font = new ::Font(L"Microsoft YaHei", 12.0f);
	this->AddControl(nameLabel);
	nameLabel->ParentForm = this->ParentForm;

	// 值复选框（不显示额外文字）
	auto valueCheckBox = new CheckBox(L"", (width - 30) / 2 + 15, yOffset);
	valueCheckBox->Checked = value;
	valueCheckBox->ParentForm = this->ParentForm;

	valueCheckBox->OnMouseClick += [this, propertyName](Control* sender, MouseEventArgs) {
		auto cb = (CheckBox*)sender;
		UpdatePropertyFromBool(propertyName, cb->Checked);
	};

	this->AddControl(valueCheckBox);

	auto item = new PropertyItem(propertyName, nameLabel, valueCheckBox);
	_items.push_back(item);

	yOffset += 25;
}

void PropertyGrid::UpdatePropertyFromTextBox(std::wstring propertyName, std::wstring value)
{
	if (!_currentControl || !_currentControl->ControlInstance) return;
	
	auto ctrl = _currentControl->ControlInstance;
	
	try
	{
		if (propertyName == L"Name")
		{
			_currentControl->Name = value;
		}
		else if (propertyName == L"Text")
		{
			ctrl->Text = value;
		}
		else if (propertyName == L"X")
		{
			auto loc = ctrl->Location;
			loc.x = std::stoi(value);
			ctrl->Location = loc;
		}
		else if (propertyName == L"Y")
		{
			auto loc = ctrl->Location;
			loc.y = std::stoi(value);
			ctrl->Location = loc;
		}
		else if (propertyName == L"Width")
		{
			auto size = ctrl->Size;
			size.cx = std::stoi(value);
			ctrl->Size = size;
		}
		else if (propertyName == L"Height")
		{
			auto size = ctrl->Size;
			size.cy = std::stoi(value);
			ctrl->Size = size;
		}
		else if (propertyName == L"Enabled")
		{
			ctrl->Enable = (value == L"true" || value == L"True" || value == L"1");
		}
		else if (propertyName == L"Visible")
		{
			ctrl->Visible = (value == L"true" || value == L"True" || value == L"1");
		}
	}
	catch (...)
	{
		// 忽略转换错误
	}
}

void PropertyGrid::UpdatePropertyFromBool(std::wstring propertyName, bool value)
{
	if (!_currentControl || !_currentControl->ControlInstance) return;
	auto ctrl = _currentControl->ControlInstance;

	if (propertyName == L"Enabled")
	{
		ctrl->Enable = value;
	}
	else if (propertyName == L"Visible")
	{
		ctrl->Visible = value;
	}
}

void PropertyGrid::LoadControl(std::shared_ptr<DesignerControl> control)
{
	Clear();
	_currentControl = control;
	
	if (!control || !control->ControlInstance)
	{
		_titleLabel->Text = L"属性";
		return;
	}
	
	_titleLabel->Text = L"属性 - " + control->Name;
	
	auto ctrl = control->ControlInstance;
	int yOffset = 45;
	
	// 基本属性
	CreatePropertyItem(L"Name", control->Name, yOffset);
	CreatePropertyItem(L"Text", ctrl->Text, yOffset);
	
	// 位置和大小
	CreatePropertyItem(L"X", std::to_wstring(ctrl->Location.x), yOffset);
	CreatePropertyItem(L"Y", std::to_wstring(ctrl->Location.y), yOffset);
	CreatePropertyItem(L"Width", std::to_wstring(ctrl->Size.cx), yOffset);
	CreatePropertyItem(L"Height", std::to_wstring(ctrl->Size.cy), yOffset);
	
	// 状态
	CreateBoolPropertyItem(L"Enabled", ctrl->Enable, yOffset);
	CreateBoolPropertyItem(L"Visible", ctrl->Visible, yOffset);

	// 高级编辑入口（模态窗口）
	if (control->Type == UIClass::UI_ComboBox)
	{
		auto editBtn = new Button(L"编辑下拉项...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto cb = dynamic_cast<ComboBox*>(_currentControl->ControlInstance);
			if (!cb) return;
			ComboBoxItemsEditorDialog dlg(cb);
			dlg.ShowDialog(this->ParentForm->Handle);
			cb->PostRender();
		};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_GridView)
	{
		auto editBtn = new Button(L"编辑列...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto gv = dynamic_cast<GridView*>(_currentControl->ControlInstance);
			if (!gv) return;
			GridViewColumnsEditorDialog dlg(gv);
			dlg.ShowDialog(this->ParentForm->Handle);
			gv->PostRender();
		};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_TabControl)
	{
		auto editBtn = new Button(L"编辑页...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto tc = dynamic_cast<TabControl*>(_currentControl->ControlInstance);
			if (!tc) return;
			TabControlPagesEditorDialog dlg(tc);
			// 如果删除页，需要同步移除该页下的 DesignerControl 以避免悬挂
			dlg.OnBeforeDeletePage = [this](Control* page) {
				if (_canvas && page) _canvas->RemoveDesignerControlsInSubtree(page);
			};
			dlg.ShowDialog(this->ParentForm->Handle);
			tc->PostRender();
		};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_ToolBar)
	{
		auto editBtn = new Button(L"编辑按钮...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto tb = dynamic_cast<ToolBar*>(_currentControl->ControlInstance);
			if (!tb) return;
			ToolBarButtonsEditorDialog dlg(tb);
			// 如果删除按钮控件，需要同步移除 DesignerControl
			dlg.OnBeforeDeleteButton = [this](Control* btn) {
				if (_canvas && btn) _canvas->RemoveDesignerControlsInSubtree(btn);
			};
			dlg.ShowDialog(this->ParentForm->Handle);
			tb->PostRender();
		};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_TreeView)
	{
		auto editBtn = new Button(L"编辑节点...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto tv = dynamic_cast<TreeView*>(_currentControl->ControlInstance);
			if (!tv) return;
			TreeViewNodesEditorDialog dlg(tv);
			dlg.ShowDialog(this->ParentForm->Handle);
			tv->PostRender();
		};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	else if (control->Type == UIClass::UI_GridPanel)
	{
		auto editBtn = new Button(L"编辑行/列...", 10, yOffset + 8, this->Width - 20, 28);
		editBtn->OnMouseClick += [this](Control*, MouseEventArgs) {
			if (!_currentControl || !_currentControl->ControlInstance || !this->ParentForm) return;
			auto gp = dynamic_cast<GridPanel*>(_currentControl->ControlInstance);
			if (!gp) return;
			GridPanelDefinitionsEditorDialog dlg(gp);
			dlg.ShowDialog(this->ParentForm->Handle);
			gp->PostRender();
		};
		this->AddControl(editBtn);
		_extraControls.push_back(editBtn);
		yOffset += 36;
	}
	
	// 确保所有新创建的子控件的ParentForm都被正确设置
	Control::SetChildrenParentForm(this, this->ParentForm);
}

void PropertyGrid::Clear()
{
	// 在移除控件前，如果Form的Selected是PropertyGrid的子控件，先清除Selected
	// 避免移除后的控件在处理鼠标事件时访问ParentForm
	if (this->ParentForm && this->ParentForm->Selected)
	{
		for (auto item : _items)
		{
			if (this->ParentForm->Selected == item->NameLabel ||
				(item->ValueTextBox && this->ParentForm->Selected == item->ValueTextBox) ||
				(item->ValueCheckBox && this->ParentForm->Selected == item->ValueCheckBox))
			{
				this->ParentForm->Selected = nullptr;
				break;
			}
		}
		if (this->ParentForm->Selected)
		{
			for (auto* c : _extraControls)
			{
				if (c && this->ParentForm->Selected == c)
				{
					this->ParentForm->Selected = nullptr;
					break;
				}
			}
		}
	}
	
	// 移除所有属性项（保留标题）
	for (auto item : _items)
	{
		this->RemoveControl(item->NameLabel);
		delete item->NameLabel;
		if (item->ValueTextBox)
		{
			this->RemoveControl(item->ValueTextBox);
			delete item->ValueTextBox;
		}
		if (item->ValueCheckBox)
		{
			this->RemoveControl(item->ValueCheckBox);
			delete item->ValueCheckBox;
		}
		delete item;
	}
	_items.clear();

	for (auto* c : _extraControls)
	{
		if (!c) continue;
		this->RemoveControl(c);
		delete c;
	}
	_extraControls.clear();
}
