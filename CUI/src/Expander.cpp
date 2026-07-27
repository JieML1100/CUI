#define NOMINMAX
#include "Expander.h"

#include "Layout/OverlayLayout.h"
#include "TemplateInfrastructure.h"
#include "Window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace
{
	constexpr float HeaderExtent = 36.0f;
	constexpr float HeaderPadding = 12.0f;
	constexpr float ChevronSize = 13.0f;
	constexpr float FallbackBorderThickness = 1.0f;
	constexpr float FallbackCornerRadius = 7.0f;
	constexpr D2D1_COLOR_F Surface = cui::theme::palette::Surface;
	constexpr D2D1_COLOR_F HeaderBackground = cui::theme::palette::SurfaceMuted;
	constexpr D2D1_COLOR_F HeaderHighlight = cui::theme::palette::AccentSoft;
	constexpr D2D1_COLOR_F ContentBackground = cui::theme::palette::SurfaceSubtle;
	constexpr D2D1_COLOR_F Accent = cui::theme::palette::Accent;
	constexpr D2D1_COLOR_F MutedText = cui::theme::palette::TextMuted;
	constexpr D2D1_COLOR_F DisabledOverlay = cui::theme::palette::DisabledOverlay;

	template<typename TValue>
	DependencyPropertyOptions<Expander, TValue> ExpanderPropertyOptions(
		TValue defaultValue,
		int order,
		DependencyPropertyEditorKind editor,
		DependencyPropertyFlags flags)
	{
		DependencyPropertyOptions<Expander, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = flags;
		options.Design.Category = L"Behavior";
		options.Design.CategoryOrder = 110;
		options.Design.Order = order;
		options.Design.Editor = editor;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		return options;
	}

	auto ExpanderPropertySubscriber(const wchar_t* propertyName)
	{
		return [propertyName = std::wstring(propertyName)](
			Expander& target,
			DependencyPropertyMetadata::ChangeHandler handler,
			DataSourceUpdateMode)
		{
			return target.OnPropertyValueChanged.Subscribe(
				[propertyName, handler = std::move(handler)](
					DependencyObject*,
					const DependencyPropertyChangedEventArgs& args)
				{
					if (args.PropertyName == propertyName) handler();
				});
		};
	}

	float RectWidth(const D2D1_RECT_F& rect) noexcept
	{
		return (std::max)(0.0f, rect.right - rect.left);
	}

	float RectHeight(const D2D1_RECT_F& rect) noexcept
	{
		return (std::max)(0.0f, rect.bottom - rect.top);
	}

	D2D1_POINT_2F RotateAround(
		D2D1_POINT_2F point, float cx, float cy, float angle) noexcept
	{
		const float dx = point.x - cx;
		const float dy = point.y - cy;
		const float sine = std::sin(angle);
		const float cosine = std::cos(angle);
		return D2D1::Point2F(
			cx + dx * cosine - dy * sine,
			cy + dx * sine + dy * cosine);
	}

	void DrawChevron(
		D2DGraphics* graphics,
		float cx,
		float cy,
		::ExpandDirection direction,
		bool expanded,
		D2D1_COLOR_F color)
	{
		if (!graphics) return;
		float angle = 0.0f;
		if (expanded)
		{
			switch (direction)
			{
			case ::ExpandDirection::Down: angle = 1.57079632679f; break;
			case ::ExpandDirection::Up: angle = -1.57079632679f; break;
			case ::ExpandDirection::Left: angle = 3.14159265359f; break;
			case ::ExpandDirection::Right: angle = 0.0f; break;
			}
		}
		else if (direction == ::ExpandDirection::Left
			|| direction == ::ExpandDirection::Right)
		{
			angle = 1.57079632679f;
		}
		const float halfWidth = ChevronSize * 0.28f;
		const float halfHeight = ChevronSize * 0.46f;
		auto first = RotateAround(
			D2D1::Point2F(cx - halfWidth, cy - halfHeight), cx, cy, angle);
		auto middle = RotateAround(
			D2D1::Point2F(cx + halfWidth, cy), cx, cy, angle);
		auto last = RotateAround(
			D2D1::Point2F(cx - halfWidth, cy + halfHeight), cx, cy, angle);
		graphics->DrawLine(first, middle, color, 1.8f);
		graphics->DrawLine(middle, last, color, 1.8f);
	}
}

UIClass Expander::Type()
{
	return UIClass::UI_Expander;
}

void Expander::RegisterDependencyProperties()
{
	HeaderedContentControl::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto expandedOptions = ExpanderPropertyOptions(
			true, 10, DependencyPropertyEditorKind::Boolean,
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsArrange
				| DependencyPropertyFlags::AffectsRender);
		expandedOptions.Changed = [](
			Expander& target, const bool& oldValue, const bool& newValue)
		{
			target.ApplyExpandedStateChange(oldValue, newValue);
		};
		DependencyPropertyRegistry::Register<Expander, bool>(L"IsExpanded",
			[](Expander& target) { return target.IsExpanded; },
			[](Expander& target, const bool& value)
			{ target.IsExpanded = value; },
			ExpanderPropertySubscriber(L"IsExpanded"),
			std::move(expandedOptions));

		auto directionOptions = ExpanderPropertyOptions(
			::ExpandDirection::Down, 20, DependencyPropertyEditorKind::Choice,
			DependencyPropertyFlags::AffectsMeasure
				| DependencyPropertyFlags::AffectsArrange
				| DependencyPropertyFlags::AffectsRender);
		directionOptions.Design.Choices = {
			{ L"Down", BindingValue(::ExpandDirection::Down) },
			{ L"Up", BindingValue(::ExpandDirection::Up) },
			{ L"Left", BindingValue(::ExpandDirection::Left) },
			{ L"Right", BindingValue(::ExpandDirection::Right) },
		};
		directionOptions.Changed = [](
			Expander& target, const ::ExpandDirection&, const ::ExpandDirection&)
		{
			target.RequestLayout();
			target.InvalidateVisual();
		};
		DependencyPropertyRegistry::Register<Expander, ::ExpandDirection>(
			L"ExpandDirection",
			[](Expander& target) { return target.ExpandDirection; },
			[](Expander& target, const ::ExpandDirection& value)
			{ target.ExpandDirection = value; },
			ExpanderPropertySubscriber(L"ExpandDirection"),
			std::move(directionOptions));
		return true;
	}();
	(void)registered;
}

Expander::Expander()
	: HeaderedContentControl()
{
	RegisterDependencyProperties();
	RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
	RendererBorderColor = cui::theme::palette::Border;
	RendererForegroundColor = cui::theme::palette::TextPrimary;
	RetainEventConnection(OnMouseMove.Subscribe(
		[this](Control*, MouseEventArgs& args)
		{
			const bool headerHit = HeaderHitTest(args.X, args.Y);
			if (_hoverHeader == headerHit) return;
			_hoverHeader = headerHit;
			InvalidateVisual();
		}));
	RetainEventConnection(OnMouseLeave.Subscribe(
		[this](Control*, MouseEventArgs&)
		{
			if (!_hoverHeader) return;
			_hoverHeader = false;
			InvalidateVisual();
		}));
}

GET_CPP(Expander, bool, IsExpanded)
{
	return _isExpanded;
}

SET_CPP(Expander, bool, IsExpanded)
{
	(void)SetPropertyField(L"IsExpanded", _isExpanded, value);
}

GET_CPP(Expander, ::ExpandDirection, ExpandDirection)
{
	return _expandDirection;
}

SET_CPP(Expander, ::ExpandDirection, ExpandDirection)
{
	(void)SetPropertyField(L"ExpandDirection", _expandDirection, value);
}

void Expander::ConfigureContentVisual(Control& child)
{
	HeaderedContentControl::ConfigureContentVisual(child);
	cui::framework::TemplateAccess::SetPresentationSuppressed(
		child, !_isExpanded);
}

cui::core::Insets
Expander::GetHeaderPresentationInsets() const noexcept
{
	return cui::core::Insets{
		HeaderPadding + ChevronSize + 9.0f,
		0.0f,
		HeaderPadding,
		0.0f };
}

void Expander::SynchronizeContentPresentation()
{
	auto* content = GetVisualContent();
	if (!content) content = GetGeneratedPresenter();
	if (content)
		cui::framework::TemplateAccess::SetPresentationSuppressed(
			*content, !_isExpanded);
}

float Expander::GetHeaderSlotHeightDip(float)
{
	return HeaderExtent;
}

cui::core::Rect Expander::HeaderRect() const noexcept
{
	const auto size = GetActualSizeDip();
	const auto padding = GetSpecifiedLayout().padding;
	const cui::core::Rect inner{
		padding.left,
		padding.top,
		(std::max)(0.0f, size.width - padding.Horizontal()),
		(std::max)(0.0f, size.height - padding.Vertical()) };
	const auto* header = GetHeaderVisual();
	auto desired = header ? header->GetDesiredSizeDip() : cui::core::Size{};
	if (header)
	{
		const auto insets = GetHeaderPresentationInsets();
		desired.width += insets.Horizontal();
		desired.height += insets.Vertical();
	}
	if (_expandDirection == ::ExpandDirection::Down
		|| _expandDirection == ::ExpandDirection::Up)
	{
		const float extent = (std::clamp)(
			(std::max)(HeaderExtent, desired.height), 0.0f, inner.height);
		return {
			inner.x,
			_expandDirection == ::ExpandDirection::Up
				? inner.Bottom() - extent : inner.y,
			inner.width,
			extent };
	}
	const float extent = (std::clamp)(
		(std::max)(HeaderExtent, desired.width), 0.0f, inner.width);
	return {
		_expandDirection == ::ExpandDirection::Left
			? inner.Right() - extent : inner.x,
		inner.y,
		extent,
		inner.height };
}

cui::core::Rect Expander::ContentRect() const noexcept
{
	const auto size = GetActualSizeDip();
	const auto padding = GetSpecifiedLayout().padding;
	const cui::core::Rect inner{
		padding.left,
		padding.top,
		(std::max)(0.0f, size.width - padding.Horizontal()),
		(std::max)(0.0f, size.height - padding.Vertical()) };
	const auto header = HeaderRect();
	switch (_expandDirection)
	{
	case ::ExpandDirection::Down:
		return { inner.x, header.Bottom(), inner.width,
			(std::max)(0.0f, inner.Bottom() - header.Bottom()) };
	case ::ExpandDirection::Up:
		return { inner.x, inner.y, inner.width,
			(std::max)(0.0f, header.y - inner.y) };
	case ::ExpandDirection::Left:
		return { inner.x, inner.y,
			(std::max)(0.0f, header.x - inner.x), inner.height };
	case ::ExpandDirection::Right:
		return { header.Right(), inner.y,
			(std::max)(0.0f, inner.Right() - header.Right()), inner.height };
	}
	return {};
}

cui::core::Size Expander::MeasureCore(
	const cui::core::Constraints& available)
{
	if (GetControlTemplateRoot())
		return HeaderedContentControl::MeasureCore(available);
	SynchronizeContentPresentation();
	const auto padding = GetSpecifiedLayout().padding;
	const auto inner = available.Deflate(padding).Normalized();

	cui::core::Size headerDesired{};
	if (auto* header = GetHeaderVisual(); header && !header->IsCollapsed())
	{
		const auto insets = GetHeaderPresentationInsets();
		const auto margin = header->GetSpecifiedLayout().margin;
		headerDesired = header->Measure(cui::core::Constraints{
			cui::core::Size{},
			cui::core::Size{
				(std::max)(0.0f, inner.maximum.width
					- insets.Horizontal() - margin.Horizontal()),
				(std::max)(0.0f, inner.maximum.height
					- insets.Vertical() - margin.Vertical()) } });
		headerDesired.width += margin.Horizontal() + insets.Horizontal();
		headerDesired.height += margin.Vertical() + insets.Vertical();
	}
	if (_expandDirection == ::ExpandDirection::Down
		|| _expandDirection == ::ExpandDirection::Up)
		headerDesired.height = (std::max)(HeaderExtent, headerDesired.height);
	else
		headerDesired.width = (std::max)(HeaderExtent, headerDesired.width);

	cui::core::Size contentDesired{};
	auto* content = GetVisualContent();
	if (!content) content = GetGeneratedPresenter();
	if (content && !content->IsCollapsed())
	{
		const auto margin = content->GetSpecifiedLayout().margin;
		float maximumWidth = inner.maximum.width;
		float maximumHeight = inner.maximum.height;
		if ((_expandDirection == ::ExpandDirection::Left
			|| _expandDirection == ::ExpandDirection::Right)
			&& inner.IsWidthBounded())
			maximumWidth = (std::max)(0.0f, maximumWidth - headerDesired.width);
		if ((_expandDirection == ::ExpandDirection::Down
			|| _expandDirection == ::ExpandDirection::Up)
			&& inner.IsHeightBounded())
			maximumHeight = (std::max)(0.0f, maximumHeight - headerDesired.height);
		contentDesired = content->Measure(cui::core::Constraints{
			cui::core::Size{},
			cui::core::Size{
				(std::max)(0.0f, maximumWidth - margin.Horizontal()),
				(std::max)(0.0f, maximumHeight - margin.Vertical()) } });
		contentDesired.width += margin.Horizontal();
		contentDesired.height += margin.Vertical();
	}

	cui::core::Size desired{};
	if (_expandDirection == ::ExpandDirection::Down
		|| _expandDirection == ::ExpandDirection::Up)
	{
		desired.width = (std::max)(headerDesired.width, contentDesired.width);
		desired.height = headerDesired.height + contentDesired.height;
	}
	else
	{
		desired.width = headerDesired.width + contentDesired.width;
		desired.height = (std::max)(headerDesired.height, contentDesired.height);
	}
	desired.width += padding.Horizontal();
	desired.height += padding.Vertical();
	return desired;
}

void Expander::PerformPendingLayout()
{
	if (IsLayoutSuspended() || !_contentLayoutPending) return;
	SynchronizeContentPresentation();
	if (GetControlTemplateRoot())
	{
		HeaderedContentControl::PerformPendingLayout();
		return;
	}
	const auto headerRect = HeaderRect();
	const auto contentRect = ContentRect();
	if (auto* header = GetHeaderVisual())
	{
		const std::array<Control*, 1> children{ header };
		cui::layout::ArrangeOverlayChildren(
			children, headerRect.Inset(GetHeaderPresentationInsets()));
	}
	auto* content = GetVisualContent();
	if (!content) content = GetGeneratedPresenter();
	if (content)
	{
		if (!_isExpanded || content->IsCollapsed())
		{
			// A collapsed child must not retain its last expanded arrange slot.
			// Presentation suppression removes it from measure/render, while this
			// zero slot completes the WPF Visibility.Collapsed layout contract.
			content->Arrange(cui::core::Rect{
				contentRect.x, contentRect.y, 0.0f, 0.0f });
		}
		else
		{
			const std::array<Control*, 1> children{ content };
			cui::layout::ArrangeOverlayChildren(children, contentRect);
		}
	}
	_contentLayoutPending = false;
}

bool Expander::HeaderHitTest(int localX, int localY) const
{
	return HeaderRect().Contains(cui::core::Point{
		static_cast<float>(localX), static_cast<float>(localY) });
}

void Expander::ApplyExpandedStateChange(bool oldValue, bool newValue)
{
	if (oldValue == newValue) return;
	const ControlWeakReference lifetime(this);
	SynchronizeContentPresentation();
	if (!newValue && GetPresentationWindow())
	{
		auto* focused = GetPresentationWindow()->GetKeyboardFocusedElement();
		for (auto* current = focused; current; current = current->GetVisualParent())
		{
			if (current != this) continue;
			GetPresentationWindow()->SetKeyboardFocus(nullptr, false);
			break;
		}
	}
	auto* source = dynamic_cast<Expander*>(lifetime.Get());
	if (!source) return;
	source->RequestLayout();
	source->InvalidateVisual();
	RoutedEventArgs args;
	if (newValue) source->Expanded(source, args);
	else source->Collapsed(source, args);
}

void Expander::SetCurrentExpanded(bool value)
{
	(void)SetCurrentPropertyField(L"IsExpanded", _isExpanded, value);
}

void Expander::SetExpanded(bool value)
{
	IsExpanded = value;
}

void Expander::Toggle()
{
	SetCurrentExpanded(!_isExpanded);
}

CursorKind Expander::QueryCursor(int localX, int localY)
{
	if (!IsEnabled) return CursorKind::Arrow;
	return HeaderHitTest(localX, localY)
		? CursorKind::Hand : Control::QueryCursor(localX, localY);
}

bool Expander::ShouldHitTestChildrenAt(int localX, int localY) const
{
	if (!HitTestChildren() || !_isExpanded) return false;
	return ContentRect().Contains(cui::core::Point{
		static_cast<float>(localX), static_cast<float>(localY) });
}

D2D1_RECT_F Expander::GetVisualChildrenClipRect()
{
	const auto content = ContentRect();
	return D2D1::RectF(
		content.Left(), content.Top(), content.Right(), content.Bottom());
}

bool Expander::HandlesNavigationKey(Key key) const
{
	return key == Key::Return || key == Key::Space;
}

void Expander::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	auto* graphics = GetDrawingContext();
	const auto size = GetActualSizeDip();
	const auto header = HeaderRect();
	const auto content = ContentRect();
	const float radius = (std::clamp)(FallbackCornerRadius, 0.0f,
		(std::min)(size.width, size.height) * 0.5f);

	BeginRender(size.width, size.height);
	if (GetControlTemplateRoot())
	{
		EndRender();
		return;
	}

	if (_isExpanded)
		graphics->FillRoundRect(0.0f, 0.0f,
			size.width, size.height, Surface, radius);
	if (_isExpanded && content.width > 0.0f && content.height > 0.0f)
		graphics->FillRect(content.x, content.y,
			content.width, content.height, ContentBackground);
	graphics->FillRoundRect(header.x, header.y,
		header.width, header.height, HeaderBackground, radius);
	if (_hoverHeader)
		graphics->FillRoundRect(
			header.x + 1.0f, header.y + 1.0f,
			(std::max)(0.0f, header.width - 2.0f),
			(std::max)(0.0f, header.height - 2.0f),
			HeaderHighlight, (std::max)(0.0f, radius - 1.0f));

	auto headerTextColor = IsEnabled ? RendererForegroundColor : MutedText;
	const float chevronX = header.x + HeaderPadding + ChevronSize * 0.5f;
	const float chevronY = header.y + header.height * 0.5f;
	DrawChevron(graphics, chevronX, chevronY,
		_expandDirection, _isExpanded, headerTextColor);

	if (!GetHeaderVisual())
	{
		const D2D1_RECT_F textRect{
			header.x + HeaderPadding + ChevronSize + 9.0f,
			header.y,
			header.Right() - HeaderPadding,
			header.Bottom() };
		graphics->PushDrawRect(
			textRect.left, textRect.top,
			(std::max)(1.0f, RectWidth(textRect)),
			(std::max)(1.0f, RectHeight(textRect)));
		graphics->DrawString(GetDisplayText(),
			textRect.left,
			textRect.top + (std::max)(0.0f,
				(RectHeight(textRect) - GetRenderFont()->FontHeight) * 0.5f),
			(std::max)(1.0f, RectWidth(textRect)),
			(std::max)(1.0f, RectHeight(textRect)),
			headerTextColor, GetRenderFont());
		graphics->PopDrawRect();
	}

	if (RendererBorderColor.a > 0.0f)
	{
		const auto chrome = _isExpanded
			? cui::core::Rect{ 0.0f, 0.0f, size.width, size.height }
			: header;
		graphics->DrawRoundRect(
			chrome.x + FallbackBorderThickness * 0.5f,
			chrome.y + FallbackBorderThickness * 0.5f,
			(std::max)(0.0f, chrome.width - FallbackBorderThickness),
			(std::max)(0.0f, chrome.height - FallbackBorderThickness),
			RendererBorderColor, FallbackBorderThickness, radius);
	}
	if (Accent.a > 0.0f)
	{
		const float accentExtent = (std::max)(
			6.0f, (std::min)(header.width, header.height) - 14.0f);
		graphics->FillRoundRect(
			header.x + 2.0f,
			header.y + (header.height - accentExtent) * 0.5f,
			3.0f, accentExtent, Accent, 1.5f);
	}
	if (!IsEnabled)
	{
		const auto chrome = _isExpanded
			? cui::core::Rect{ 0.0f, 0.0f, size.width, size.height }
			: header;
		graphics->FillRoundRect(
			chrome.x, chrome.y, chrome.width, chrome.height,
			DisabledOverlay, radius);
	}
	EndRender();
}

bool Expander::ProcessInput(const InputReport& input)
{
	if (!IsEnabled || !IsVisible) return true;
	PerformPendingLayout();
	const bool headerHit = HeaderHitTest(input.X, input.Y);
	switch (input.Kind)
	{
	case InputReportKind::PointerDown:
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		if (GetPresentationWindow()) GetPresentationWindow()->SetKeyboardFocus(this, false);
		if (headerHit)
		{
			_headerPressActive = true;
			(void)CaptureMouse();
			auto args = input.CreateMouseEventArgs();
			OnMouseDown(this, args);
			InvalidateVisual();
			return true;
		}
		break;
	case InputReportKind::PointerUp:
		if (input.ChangedButton != MouseButton::Left)
			return Control::ProcessInput(input);
		{
			const bool activate = _headerPressActive && headerHit;
			_headerPressActive = false;
			if (IsMouseCaptured()) (void)ReleaseMouseCapture();
			if (activate) Toggle();
			auto args = input.CreateMouseEventArgs();
			OnMouseUp(this, args);
			return true;
		}
	case InputReportKind::Cancel:
	case InputReportKind::CaptureLost:
		{
			_headerPressActive = false;
			if (input.Kind == InputReportKind::Cancel && IsMouseCaptured())
				(void)ReleaseMouseCapture();
			return Control::ProcessInput(input);
		}
	case InputReportKind::KeyDown:
		if (GetPresentationWindow() && GetPresentationWindow()->GetKeyboardFocusedElement() == this
			&& (input.Key == Key::Return
				|| input.Key == Key::Space))
		{
			Toggle();
			auto args = input.CreateKeyEventArgs();
			OnKeyDown(this, args);
			return true;
		}
		break;
	default:
		break;
	}
	return Control::ProcessInput(input);
}
