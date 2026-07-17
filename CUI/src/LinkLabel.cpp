#include "LinkLabel.h"
#include "Form.h"

UIClass LinkLabel::Type() { return UIClass::UI_LinkLabel; }

void LinkLabel::EnsureBindingPropertiesRegistered()
{
	Label::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		ControlPropertyOptions<LinkLabel, bool> options;
		options.DefaultValue = false;
		options.Flags = ControlPropertyFlags::AffectsRender;
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 300;
		options.Design.Order = 10;
		options.Design.Editor = ControlPropertyEditorKind::Boolean;
		options.Design.Persistence = ControlPropertyPersistence::Metadata;
		BindingPropertyRegistry::Register<LinkLabel, bool>(L"Visited",
			[](LinkLabel& target) { return target.Visited; },
			[](LinkLabel& target, const bool& value)
			{
				target.Visited = value;
				target.InvalidateVisual();
			}, {}, std::move(options));
		return true;
	}();
	(void)registered;
}

LinkLabel::LinkLabel(std::wstring text, int x, int y)
	: Label(text, x, y)
{
	this->BackColor = D2D1_COLOR_F{ 0.f, 0.f, 0.f, 0.f };
	this->ForeColor = Colors::DeepSkyBlue;
	this->UnderlineColor = this->ForeColor;
}

bool LinkLabel::Invoke()
{
	if (!Enable || !Visible) return false;
	Visited = true;
	const auto size = GetActualSizeDip();
	OnMouseClick(this, MouseEventArgs(MouseButtons::None, 0,
		static_cast<int>(size.width * 0.5f),
		static_cast<int>(size.height * 0.5f), 0));
	InvalidateVisual();
	return true;
}

void LinkLabel::Update()
{
	if (!this->IsVisual) return;
	auto d2d = this->ParentForm->Render;
	const auto size = this->GetActualSizeDip();
	auto font = this->Font;

	float clipW = lastMeasuredWidth > size.width ? lastMeasuredWidth : FLT_MAX;
	this->BeginRender(clipW, FLT_MAX);
	{
		if (this->Image)
		{
			this->RenderImage();
		}

		bool hover = this->ParentForm && this->ParentForm->UnderMouse == this;
		auto textColor = hover ? this->HoverColor : (this->Visited ? this->VisitedColor : this->ForeColor);
		auto underlineColor = hover ? this->HoverColor : this->UnderlineColor;
		const auto displayText = GetDisplayText();
		d2d->DrawString(displayText, 0, 0, textColor, font);

		auto textSize = font->GetTextSize(displayText);
		float underlineY = textSize.height - 1.0f;
		d2d->DrawLine({ 0.0f, underlineY },
			{ textSize.width, underlineY },
			underlineColor, 1.0f);
	}

	if (!this->Enable)
	{
		float w = lastMeasuredWidth > size.width ? lastMeasuredWidth : size.width;
		d2d->FillRect(0, 0, w, size.height, { 1.0f ,1.0f ,1.0f ,0.5f });
	}

	this->EndRender();
	lastMeasuredWidth = size.width;
}

CursorKind LinkLabel::QueryCursor(int localX, int localY)
{
	(void)localX;
	(void)localY;
	return CursorKind::Hand;
}

void LinkLabel::BeforeDefaultClick(UINT message, MouseEventArgs& e)
{
	(void)e;
	if (message == WM_LBUTTONUP)
		this->Visited = true;
}
