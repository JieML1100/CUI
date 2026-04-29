#include "ToolBar.h"
#include "Form.h"

UIClass ToolBar::Type() { return UIClass::UI_ToolBar; }

ToolBar::ToolBar(int x, int y, int width, int height)
{
	this->Location = POINT{ x,y };
	this->Size = SIZE{ width,height };
	this->BackColor = D2D1_COLOR_F{ 1,1,1,0.12f };
	this->BolderColor = D2D1_COLOR_F{ 1,1,1,0.20f };
	this->Boder = 1.0f;
	ItemHeight = static_cast<int>(height * 0.75f);
}

Button* ToolBar::AddToolButton(std::wstring text, int width)
{
	Button* b = this->AddControl(new Button(text, 0, 0, width, ItemHeight));
	b->BackColor = D2D1_COLOR_F{ 1,1,1,0.20f };
	b->BolderColor = D2D1_COLOR_F{ 1,1,1,0.25f };
	b->ForeColor = Colors::WhiteSmoke;
	b->Round = 0.35f;
	b->Boder = 1.0f;
	LayoutItems();
	return b;
}

Button* ToolBar::AddToolButton(Button* button)
{
	if (!button) return NULL;
	// 允许外部自定义 Image/SizeMode/颜色等；ToolBar 只负责布局尺寸与位置
	Button* b = this->AddControl(button);
	// 如果外部没有给宽度，默认给一个方形按钮
	if (b->Width <= 0) b->Width = ItemHeight;
	b->Height = ItemHeight;
	LayoutItems();
	return b;
}

bool ToolBar::RemoveToolButton(Button* button, bool deleteButton)
{
	if (!button) return false;
	for (int i = 0; i < this->Count; i++)
	{
		if (this->operator[](i) == button)
			return RemoveToolButtonAt(i, deleteButton);
	}
	return false;
}

bool ToolBar::RemoveToolButtonAt(int index, bool deleteButton)
{
	if (index < 0 || index >= this->Count) return false;
	auto* button = dynamic_cast<Button*>(this->operator[](index));
	auto* child = this->operator[](index);
	this->Children.erase(this->Children.begin() + index);
	if (child)
	{
		child->Parent = NULL;
		child->ParentForm = NULL;
		if (deleteButton)
			delete child;
	}
	LayoutItems();
	this->PostRender();
	return button != nullptr || child != nullptr;
}

void ToolBar::ClearToolButtons(bool deleteButtons)
{
	auto children = this->Children;
	this->Children.clear();
	for (auto* child : children)
	{
		if (!child) continue;
		child->Parent = NULL;
		child->ParentForm = NULL;
		if (deleteButtons)
			delete child;
	}
	this->PostRender();
}

void ToolBar::LayoutItems()
{
	int x = Padding;
	int y = (this->Height - ItemHeight) / 2;
	if (y < 0) y = 0;
	for (int i = 0; i < this->Count; i++)
	{
		auto c = this->operator[](i);
		if (!c) continue;
		c->SetRuntimeLocation(POINT{ x, y });
		c->Height = ItemHeight;
		x += c->Width + Gap;
	}
}

void ToolBar::Update()
{
	if (this->ParentForm && this->Parent == NULL)
	{
		this->ParentForm->MainToolBar = this;
	}

	if (this->IsVisual == false) return;
	LayoutItems();

	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	const float actualWidth = static_cast<float>(size.cx);
	const float actualHeight = static_cast<float>(size.cy);
	this->BeginRender();
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, this->BackColor);
		if (this->Image)
		{
			this->RenderImage();
		}
		for (int i = 0; i < this->Count; i++)
		{
			auto c = this->operator[](i);
			if (!c) continue;
			c->Update();
		}
		d2d->DrawRect(0, 0, actualWidth, actualHeight, this->BolderColor, this->Boder);
	}
	if (!this->Enable)
	{
		d2d->FillRect(0, 0, actualWidth, actualHeight, { 1.0f ,1.0f ,1.0f ,0.5f });
	}
	this->EndRender();
}

