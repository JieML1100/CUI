#include "ToolBox.h"
#include "../CUI/GUI/Label.h"
#include "../CUI/GUI/Form.h"
#include <BitmapSource.h>

const char* _ico = R"(<svg t="1766410686901" class="icon" viewBox="0 0 1024 1024" version="1.1" xmlns="http://www.w3.org/2000/svg" p-id="5087" data-darkreader-inline-fill="" width="200" height="200"><path d="M496 895.2L138.4 771.2c-6.4-2.4-10.4-8-10.4-15.2V287.2l368 112v496z m32 0l357.6-124c6.4-2.4 10.4-8 10.4-15.2V287.2l-368 112v496z m-400-640l384 112 384-112-379.2-125.6c-3.2-0.8-7.2-0.8-10.4 0L128 255.2z" p-id="5088" fill="#1afa29" data-darkreader-inline-fill="" style="--darkreader-inline-fill: var(--darkreader-background-1afa29, #11ce4a);"></path></svg>)";

static const char* GetToolBoxSvg(UIClass type)
{
	return _ico;
}

ToolBoxItem::~ToolBoxItem()
{
	_iconSource.reset();
	_iconCache.Reset();
	_iconCacheTarget = nullptr;
}

void ToolBoxItem::EnsureIconSource()
{
	if (_iconSource || !SvgData) return;
	_iconSource = D2DGraphics::ToBitmapFromSvg(SvgData);
}

ID2D1Bitmap* ToolBoxItem::GetIconBitmap(D2DGraphics* render)
{
	EnsureIconSource();
	if (!render || !_iconSource)
		return nullptr;
	auto* target = render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (_iconCache && _iconCacheTarget == target && _iconCacheSource == _iconSource.get())
		return _iconCache.Get();
	_iconCache.Reset();
	_iconCacheTarget = target;
	_iconCacheSource = _iconSource.get();
	auto* bmp = render->CreateBitmap(_iconSource);
	if (!bmp)
		return nullptr;
	_iconCache.Attach(bmp);
	return _iconCache.Get();
}

void ToolBoxItem::Update()
{
	if (!this->IsVisual) return;
	EnsureIconSource();
	bool isUnderMouse = this->ParentForm->UnderMouse == this;
	bool isSelected = this->ParentForm->Selected == this;
	auto d2d = this->ParentForm->Render;
	auto size = this->ActualSize();
	this->BeginRender();
	{
		float roundVal = this->Height * Round;
		d2d->FillRoundRect(this->Boder * 0.5f, this->Boder * 0.5f, size.cx - this->Boder, size.cy - this->Boder, this->BackColor, roundVal);
		D2D1::ColorF color = isUnderMouse ? (isSelected ? D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.7f) : D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.4f)) : D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f);
		d2d->FillRoundRect(0, 0, size.cx, size.cy, color, roundVal);

		float paddingLeft = 8.0f;
		float gap = 8.0f;
		float iconSize = 20.0f;
		float iconLeft = paddingLeft;
		float iconTop = ((float)size.cy - iconSize) / 2.0f;
		if (auto* bmp = GetIconBitmap(d2d))
		{
			d2d->DrawBitmap(bmp, iconLeft, iconTop, iconSize, iconSize);
		}

		auto font = this->Font;
		auto textSize = font->GetTextSize(this->Text);
		float textLeft = paddingLeft + iconSize + gap;
		float textTop = (((float)size.cy - textSize.height) / 2.0f);
		d2d->DrawString(this->Text, textLeft, textTop, this->ForeColor, this->Font);

		d2d->DrawRoundRect(this->Boder * 0.5f, this->Boder * 0.5f,
			size.cx - this->Boder, size.cy - this->Boder,
			this->BolderColor, this->Boder, roundVal);
	}

	if (!this->Enable)
		d2d->FillRect(0, 0, size.cx, size.cy, { 1.0f ,1.0f ,1.0f ,0.5f });
	this->EndRender();
}

ToolBox::ToolBox(int x, int y, int width, int height)
	: Panel(x, y, width, height)
{
	this->BackColor = D2D1::ColorF(0.95f, 0.95f, 0.95f, 1.0f);
	this->Boder = 1.0f;
	
	// 标题
	this->_titleLabel = new Label(L"工具箱", 10, 10);
	this->_titleLabel->Size = {width - 20, 25};
	this->_titleLabel->Font = new ::Font(L"Microsoft YaHei", 16.0f);
	this->AddControl(_titleLabel);

	_scrollView = new ScrollView(0, _contentTop, width, std::max(0, height - _contentTop));
	_scrollView->BackColor = D2D1::ColorF(0, 0, 0, 0);
	_scrollView->Boder = 0.0f;
	_scrollView->MouseWheelStep = 39;
	this->AddControl(_scrollView);

	_itemsHost = new Panel(0, 0, width, std::max(0, height - _contentTop));
	_itemsHost->BackColor = D2D1::ColorF(0, 0, 0, 0);
	_itemsHost->Boder = 0.0f;
	_scrollView->AddControl(_itemsHost);
	
	// 获取可用控件
	auto controls = ControlRegistry::GetAvailableControls();
	int yOffset = 0;
	
	for (const auto& ctrl : controls)
	{
		auto item = new ToolBoxItem(ctrl.DisplayName, ctrl.Type, GetToolBoxSvg(ctrl.Type), 10, yOffset, width - 25, 34);
		item->Round = 0.18f;
		
		// 点击事件
		item->OnMouseClick += [this, ctrl](Control* sender, MouseEventArgs e) {
			OnControlSelected(ctrl.Type);
		};
		
		_itemsHost->AddControl(item);
		_items.push_back(item);
		
		yOffset += 39;
	}

	UpdateScrollLayout();
}

ToolBox::~ToolBox()
{
}

void ToolBox::UpdateScrollLayout()
{
	if (_titleLabel)
	{
		_titleLabel->Size = { std::max(0, this->Width - 20), 25 };
	}
	if (_scrollView)
	{
		_scrollView->Location = { 0, _contentTop };
		_scrollView->Size = { this->Width, std::max(0, this->Height - _contentTop) };
	}
	if (_itemsHost)
	{
		_itemsHost->Location = { 0, 0 };
	}

	int maxBottom = 0;
	for (auto* item : _items)
	{
		if (!item) continue;
		if (_itemsHost)
		{
			item->Width = std::max(60, _itemsHost->Width - 20);
		}
		maxBottom = std::max(maxBottom, item->BaseY + item->Height);
	}
	_contentHeight = maxBottom + _contentBottomPadding;
	if (_itemsHost)
	{
		int hostWidth = _scrollView ? std::max(0, _scrollView->Width - 12) : this->Width;
		int hostHeight = _scrollView ? std::max(_contentHeight, _scrollView->Height) : _contentHeight;
		_itemsHost->Size = { hostWidth, hostHeight };
	}

	for (auto* item : _items)
	{
		if (!item) continue;
		item->Top = item->BaseY;
	}
}

void ToolBox::Update()
{
	UpdateScrollLayout();
	Panel::Update();
}

bool ToolBox::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int xof, int yof)
{
	return Panel::ProcessMessage(message, wParam, lParam, xof, yof);
}
