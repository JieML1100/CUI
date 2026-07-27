#include "Button.h"
#include "DependencyPropertyInfrastructure.h"
#include "Window.h"
#include <algorithm>
UIClass Button::Type() { return UIClass::UI_Button; }

namespace
{
	constexpr float FallbackCornerRadius = 7.0f;
	constexpr auto FallbackHoverColor = cui::theme::palette::AccentSoft;
	constexpr auto FallbackDisabledOverlayColor =
		cui::theme::palette::DisabledOverlay;
}

void Button::RegisterDependencyProperties()
{
	ButtonBase::RegisterDependencyProperties();
	static const bool registered = []
	{
		RegisterControlBorderThicknessMetadata<Button>(1.5f, 70);
		auto commandOptions = DependencyPropertyOptions<Button, std::wstring>{
			std::wstring{}, DependencyPropertyFlags::None };
		commandOptions.Design.Category = L"Behavior";
		commandOptions.Design.CategoryOrder = 300;
		commandOptions.Design.Order = 10;
		commandOptions.Design.Editor = DependencyPropertyEditorKind::Text;
		commandOptions.Design.Persistence = DependencyPropertyPersistence::Metadata;
		commandOptions.Changed = [](
			Button& target, const std::wstring&, const std::wstring&)
		{
			target.RefreshCommandSource();
		};
		DependencyPropertyRegistry::Register<Button, std::wstring>(L"Command",
			[](Button& target) { return target.Command; },
			[](Button& target, const std::wstring& value) { target.Command = value; },
			{}, commandOptions);
		commandOptions.Design.Order = 20;
		DependencyPropertyRegistry::Register<Button, std::wstring>(L"CommandParameter",
			[](Button& target) { return target.CommandParameter; },
			[](Button& target, const std::wstring& value) { target.CommandParameter = value; },
			{}, commandOptions);
		auto commandTargetOptions =
			DependencyPropertyOptions<Button, ControlWeakReference>{
			ControlWeakReference{},
			DependencyPropertyFlags::None };
		commandTargetOptions.Design.Category = L"Behavior";
		commandTargetOptions.Design.CategoryOrder = 300;
		commandTargetOptions.Design.Order = 30;
		commandTargetOptions.Design.Editor =
			DependencyPropertyEditorKind::Auto;
		commandTargetOptions.Design.Persistence =
			DependencyPropertyPersistence::Native;
		DependencyPropertyRegistry::Register<
			Button, ControlWeakReference>(
			L"CommandTarget",
			[](Button& target) { return target._commandTarget; },
			[](Button& target, const ControlWeakReference& value)
			{ target.ApplyCommandTarget(value); },
			{}, std::move(commandTargetOptions));
		auto dialogActionOptions = DependencyPropertyOptions<Button, bool>{
			false, DependencyPropertyFlags::AffectsRender };
		dialogActionOptions.Design.Category = L"Behavior";
		dialogActionOptions.Design.CategoryOrder = 300;
		dialogActionOptions.Design.Order = 40;
		dialogActionOptions.Design.Editor =
			DependencyPropertyEditorKind::Boolean;
		dialogActionOptions.Design.Persistence =
			DependencyPropertyPersistence::Metadata;
		DependencyPropertyRegistry::Register<Button, bool>(L"IsDefault",
			[](Button& target) { return target.IsDefault; },
			[](Button& target, const bool& value) { target.IsDefault = value; },
			{}, dialogActionOptions);
		dialogActionOptions.Design.Order = 50;
		DependencyPropertyRegistry::Register<Button, bool>(L"IsCancel",
			[](Button& target) { return target.IsCancel; },
			[](Button& target, const bool& value) { target.IsCancel = value; },
			{}, std::move(dialogActionOptions));
		return true;
	}();
	(void)registered;
}

GET_CPP(Button, std::wstring, Command) { return _command; }
SET_CPP(Button, std::wstring, Command) { SetPropertyField(L"Command", _command, value); }
GET_CPP(Button, std::wstring, CommandParameter) { return _commandParameter; }
SET_CPP(Button, std::wstring, CommandParameter) { SetPropertyField(L"CommandParameter", _commandParameter, value); }
GET_CPP(Button, Control*, CommandTarget) { return _commandTarget.Get(); }
SET_CPP(Button, Control*, CommandTarget)
{
	const ControlWeakReference sourceLifetime(this);
	if (value)
	{
		(void)TrySetPropertyValue(
			L"CommandTarget",
			BindingValue(ControlWeakReference(value)),
			DependencyPropertyValueSource::Local);
		return;
	}
	if (ClearPropertyValue(
		L"CommandTarget", DependencyPropertyValueSource::Local))
		return;
	if (auto* source = dynamic_cast<Button*>(sourceLifetime.Get()))
		source->ApplyCommandTarget({});
}

void Button::ClearCommandTarget()
{
	SetCommandTarget(nullptr);
}

void Button::ApplyCommandTarget(const ControlWeakReference& value)
{
	const ControlWeakReference sourceLifetime(this);
	if (_commandTarget == value) return;
	_commandTarget = value;
	if (auto* source = dynamic_cast<Button*>(sourceLifetime.Get()))
		source->RefreshCommandSource();
}
GET_CPP(Button, bool, IsDefault) { return _isDefault; }
SET_CPP(Button, bool, IsDefault) { SetPropertyField(L"IsDefault", _isDefault, value); }
GET_CPP(Button, bool, IsCancel) { return _isCancel; }
SET_CPP(Button, bool, IsCancel) { SetPropertyField(L"IsCancel", _isCancel, value); }

Button::Button()
	: ButtonBase()
{
	RegisterDependencyProperties();
	RetainEventConnection(OnLogicalParentChanged.Subscribe(
		[this](Control*, Control*, Control*) { RefreshCommandSource(); }));
	RetainEventConnection(OnVisualParentChanged.Subscribe(
		[this](Control*, Control*, Control*) { RefreshCommandSource(); }));
	this->RendererBackgroundColor = cui::theme::palette::Surface;
	this->RendererBorderColor = cui::theme::palette::BorderStrong;
	this->RendererForegroundColor = cui::theme::palette::TextPrimary;
}

void Button::RefreshCommandSource()
{
	const auto refreshVersion = ++_commandSourceRefreshVersion;
	_commandCanExecuteConnection.Disconnect();
	if (_command.empty())
	{
		ClearCommandCanExecuteState();
		return;
	}
	const ControlWeakReference sourceLifetime(this);
	auto connection = RoutedCommandManager::ObserveCanExecute(
		*this,
		RoutedCommandSourceQuery{
			RoutedCommand(_command), _commandParameter, _commandTarget },
		[sourceLifetime, refreshVersion](
			Control& source, const RoutedCommandCanExecuteResult& result)
		{
			auto* current = dynamic_cast<Button*>(sourceLifetime.Get());
			if (current != &source
				|| current->_commandSourceRefreshVersion != refreshVersion)
				return;
			current->SetCommandCanExecuteState(result.CanExecute);
		});
	// ObserveCanExecute publishes synchronously.  Its CanExecute path and the
	// effective IsEnabled notification are both user callback boundaries, so the
	// source may already be gone (or a nested refresh may have superseded us).
	auto* source = dynamic_cast<Button*>(sourceLifetime.Get());
	if (!source || source->_commandSourceRefreshVersion != refreshVersion)
		return;
	source->_commandCanExecuteConnection = std::move(connection);
}

bool Button::ExecuteCommandSource()
{
	if (_command.empty()) return true;
	return RoutedCommandManager::ExecuteCommandSource(
		*this,
		RoutedCommandSourceQuery{
			RoutedCommand(_command), _commandParameter,
			_commandTarget }).Executed;
}

void Button::AfterDefaultClick(MouseButton button, MouseEventArgs& e)
{
	(void)button;
	(void)e;
	const ControlWeakReference sourceLifetime(this);
	if (auto* source = dynamic_cast<Button*>(sourceLifetime.Get()))
		(void)source->ExecuteCommandSource();
}

bool Button::Invoke()
{
	const ControlWeakReference sourceLifetime(this);
	const auto snapshot = GetAccessibilitySnapshot();
	if (!snapshot.Enabled || !snapshot.Visible) return false;
	const auto size = GetActualSizeDip();
	RoutedEventArgs eventArgs;
	Click(this, eventArgs);
	auto* source = dynamic_cast<Button*>(sourceLifetime.Get());
	if (!source)
		return true;
	const bool executed = source->ExecuteCommandSource();
	if (auto* survivingSource =
		dynamic_cast<Button*>(sourceLifetime.Get()))
		survivingSource->InvalidateVisual();
	return executed;
}

void Button::ConfigureContentVisual(Control& child)
{
	ButtonBase::ConfigureContentVisual(child);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		child, L"HorizontalAlignment", BindingValue(HorizontalAlignment::Center),
		DependencyPropertyValueSource::Theme);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		child, L"VerticalAlignment", BindingValue(VerticalAlignment::Center),
		DependencyPropertyValueSource::Theme);
	(void)cui::framework::DependencyPropertyAccess::SetValue(
		child, L"Background",
		BindingValue(cui::drawing::MakeSolidColorBrush(
			D2D1_COLOR_F{ 0, 0, 0, 0 })),
		DependencyPropertyValueSource::Theme);
}

void Button::OnRender()
{
	if (!this->IsVisible) return;
	if (auto* presenter = GetGeneratedPresenter())
	{
		if (!GetContentTemplate())
		{
			if (auto* generated = presenter->GetGeneratedContent())
				(void)cui::framework::DependencyPropertyAccess::SetValue(
					*generated, L"Foreground",
					BindingValue(GetComputedForegroundBrush()),
					DependencyPropertyValueSource::Template);
		}
	}
	const bool isEnabled = this->IsEffectivelyEnabled();
	bool isUnderMouse = isEnabled && this->IsMouseOver;
	const bool isPressed = HasControlStyleState(
		this->GetStyleState(), ControlStyleState::Pressed);
	auto d2d = this->GetDrawingContext();
	const auto size = this->GetActualSizeDip();
	const float actualWidth = size.width;
	const float actualHeight = size.height;
	this->BeginRender();
	if (GetControlTemplateRoot())
	{
		this->EndRender();
		return;
	}
	{
		const float border = this->BorderThickness.MaxEdge();
		const bool latched = HasControlStyleState(
			GetStyleState(), ControlStyleState::Checked);
		const bool hasSurface = this->RendererBackgroundColor.a > 0.0f || latched;
		const float surfaceX = border * 0.5f;
		const float surfaceY = border * 0.5f;
		const float surfaceW = (std::max)(0.0f, actualWidth - border);
		const float surfaceH = (std::max)(0.0f, actualHeight - border);
		const float roundVal = (std::min)(FallbackCornerRadius,
			(std::min)(surfaceW, surfaceH) * 0.5f);
		const auto baseColor = latched
			? cui::theme::palette::AccentSelected : this->RendererBackgroundColor;
		if (hasSurface)
			d2d->FillRoundRect(surfaceX, surfaceY, surfaceW, surfaceH, baseColor, roundVal);
		if (isUnderMouse && FallbackHoverColor.a > 0.0f)
			d2d->FillRoundRect(surfaceX, surfaceY, surfaceW, surfaceH,
				isPressed ? cui::theme::palette::AccentSelected
					: FallbackHoverColor, roundVal);

		const bool hasContentVisual = GetVisualContent()
			|| GetGeneratedPresenter();
		if (!hasContentVisual)
		{
			const auto displayText = this->GetDisplayText();
			auto textSize = this->GetRenderFont()->GetTextSize(displayText);
			const float horizontalPad = 6.0f;
			const float textWidth = (std::max)(1.0f, actualWidth - horizontalPad * 2.0f);
			float drawLeft = actualWidth > textSize.width ? (actualWidth - textSize.width) / 2.0f : horizontalPad;
			if (drawLeft < horizontalPad) drawLeft = horizontalPad;
			float drawTop = actualHeight > textSize.height
				? (actualHeight - textSize.height) / 2.0f : 0.0f;
			d2d->DrawString(displayText, drawLeft, drawTop, textWidth, textSize.height + 2.0f, this->RendererForegroundColor, this->GetRenderFont());
		}
		if (border > 0.0f && this->RendererBorderColor.a > 0.0f)
		{
			d2d->DrawRoundRect(surfaceX, surfaceY,
				surfaceW, surfaceH,
				this->RendererBorderColor, border, roundVal);
		}
	}

	if (!isEnabled)
		d2d->FillRoundRect(0.0f, 0.0f, actualWidth, actualHeight,
			FallbackDisabledOverlayColor, actualHeight * 0.28f);
	this->EndRender();
}
