#include "NamespacedWindow.g.h"
#include <functional>
#include <memory>
#include <utility>
#include <vector>

Acme::Views::MainWindowGenerated::MainWindowGenerated()
	: Form(L"Acme::Views::MainWindow", POINT{ 100, 100 }, SIZE{ 800, 600 })
{

	[[maybe_unused]] auto __layoutScope_form = cui::layout::DeferLayout(*this);

	// 窗体属性（标题栏/按钮/缩放）
	this->VisibleHead = true;
	this->HeadHeight = 24;
	this->MinBox = true;
	this->MaxBox = true;
	this->CloseBox = true;
	this->CenterTitle = true;
	this->AllowResize = true;

	// 窗体属性（通用）
	this->BackColor = D2D1::ColorF(0.9608f, 0.9608f, 0.9608f, 1.f);
	this->ForeColor = D2D1::ColorF(0.f, 0.f, 0.f, 1.f);
	this->ShowInTaskBar = true;
	this->TopMost = false;
	this->Enable = true;
	this->Visible = true;

	// Font
	auto* __formFont = new ::Font(L"Arial", 18.f);
	this->SetFontEx(__formFont, true);

	// 创建控件
	// namespaceButton
	auto __owned_namespaceButton = std::make_unique<Button>(L"Namespaced", 0, 0, 120, 30);
	namespaceButton = __owned_namespaceButton.get();
	namespaceButton->DesignId = 77;
	namespaceButton->BackColor = D2D1::ColorF(0.97f, 0.98f, 0.99f, 1.f);
	namespaceButton->ForeColor = D2D1::ColorF(0.12f, 0.16f, 0.22f, 1.f);
	namespaceButton->BorderColor = D2D1::ColorF(0.7f, 0.76f, 0.86f, 1.f);
	// 属性元数据扩展
	(void)namespaceButton->TrySetPropertyValue(L"LayoutWidth", BindingValue(cui::layout::Length::Fixed(120.f)));
	(void)namespaceButton->TrySetPropertyValue(L"LayoutHeight", BindingValue(cui::layout::Length::Fixed(24.f)));

	// statusBadge
	auto __owned_statusBadge = std::make_unique<Acme::Controls::StatusBadge>(140, 0, 120, 30);
	statusBadge = __owned_statusBadge.get();
	statusBadge->Text = L"Custom control";
	statusBadge->DesignId = 78;
	statusBadge->BackColor = D2D1::ColorF(0.97f, 0.98f, 0.99f, 1.f);
	statusBadge->ForeColor = D2D1::ColorF(0.12f, 0.16f, 0.22f, 1.f);
	statusBadge->BorderColor = D2D1::ColorF(0.7f, 0.76f, 0.86f, 1.f);
	// 属性元数据扩展
	(void)statusBadge->TrySetPropertyValue(L"LayoutWidth", BindingValue(cui::layout::Length::Fixed(150.f)));
	(void)statusBadge->TrySetPropertyValue(L"LayoutHeight", BindingValue(cui::layout::Length::Fixed(30.f)));

	// 绑定事件
	_generatedEventConnections.emplace_back(
		namespaceButton->OnDropFile.Subscribe(std::bind_front(&Acme::Views::MainWindowGenerated::HandleNamespacedDrop, this)));
	_generatedEventConnections.emplace_back(
		namespaceButton->OnMouseClick.Subscribe(std::bind_front(&Acme::Views::MainWindowGenerated::HandleNamespacedClick, this)));
	_generatedEventConnections.emplace_back(
		namespaceButton->OnPropertyValueChanged.Subscribe(std::bind_front(&Acme::Views::MainWindowGenerated::HandleNamespacedPropertyChanged, this)));
	_generatedEventConnections.emplace_back(
		namespaceButton->OnValidationStateChanged.Subscribe(std::bind_front(&Acme::Views::MainWindowGenerated::HandleNamespacedValidationChanged, this)));
	_generatedEventConnections.emplace_back(
		statusBadge->OnSeverityInvoked.Subscribe(std::bind_front(&Acme::Views::MainWindowGenerated::HandleSeverityInvoked, this)));

	// 组装控件层级（包含布局容器）
	this->AddOwned(std::move(__owned_namespaceButton));

	this->AddOwned(std::move(__owned_statusBadge));

	// 父容器先确定子控件最终矩形，再由子容器完成内部布局。
	__layoutScope_form.Commit();
}

Acme::Views::MainWindowGenerated::~MainWindowGenerated()
{
}

void Acme::Views::MainWindowGenerated::HandleNamespacedDrop(Control* sender, std::vector<std::wstring> files)
{
	(void)sender;
	(void)files;
}

void Acme::Views::MainWindowGenerated::HandleNamespacedClick(Control* sender, MouseEventArgs e)
{
	(void)sender;
	(void)e;
}

void Acme::Views::MainWindowGenerated::HandleNamespacedPropertyChanged(Control* sender, const ControlPropertyChangedEventArgs& e)
{
	(void)sender;
	(void)e;
}

void Acme::Views::MainWindowGenerated::HandleNamespacedValidationChanged(const BindingValidationChangedEventArgs& e)
{
	(void)e;
}

void Acme::Views::MainWindowGenerated::HandleSeverityInvoked(Control* sender, int value)
{
	(void)sender;
	(void)value;
}

