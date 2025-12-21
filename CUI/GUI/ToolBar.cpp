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
	ItemHeight = height * 0.75f;
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

void ToolBar::LayoutItems()
{
	int x = Padding;
	int y = (this->Height - ItemHeight) / 2;
	if (y < 0) y = 0;
	for (int i = 0; i < this->Count; i++)
	{
		auto c = this->operator[](i);
		if (!c) continue;
		c->Location = POINT{ x, y };
		c->Height = ItemHeight;
		x += c->Width + Gap;
	}
}

void ToolBar::Update()
{
	LayoutItems();
	Panel::Update();
}

