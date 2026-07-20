#include "Control.h"
#include "Binding.h"
#include "Form.h"
#include "Panel.h"
#include "PropertyPath.h"
#include "Style.h"
#include "Core/Threading.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <cwctype>
#include <atomic>
#include <unordered_set>
#include <type_traits>
#include <variant>

#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)

namespace
{
	std::atomic<uint32_t> NextAccessibilityRuntimeId{ 1 };
	std::atomic<uint32_t> NextAccessibilityVirtualRuntimeId{ 1 };

	uint32_t AllocateAccessibilityRuntimeId() noexcept
	{
		uint32_t value = NextAccessibilityRuntimeId.fetch_add(
			1, std::memory_order_relaxed);
		if (value == 0)
			value = NextAccessibilityRuntimeId.fetch_add(
				1, std::memory_order_relaxed);
		return value;
	}

	D2D1::Matrix3x2F AsMatrix(const D2D1_MATRIX_3X2_F& value) noexcept
	{
		return D2D1::Matrix3x2F(
			value._11, value._12, value._21,
			value._22, value._31, value._32);
	}

	bool IsFiniteMatrix(const D2D1_MATRIX_3X2_F& value) noexcept
	{
		return std::isfinite(value._11) && std::isfinite(value._12)
			&& std::isfinite(value._21) && std::isfinite(value._22)
			&& std::isfinite(value._31) && std::isfinite(value._32);
	}

	int StoredPropertySourceIndex(ControlPropertyValueSource source) noexcept
	{
		const int value = static_cast<int>(source);
		return value >= static_cast<int>(ControlPropertyValueSource::Inherited)
			&& value <= static_cast<int>(ControlPropertyValueSource::Animation)
			? value - static_cast<int>(ControlPropertyValueSource::Inherited)
			: -1;
	}

	cui::layout::Alignment ToLayoutAlignment(HorizontalAlignment value)
	{
		switch (value)
		{
		case HorizontalAlignment::Center: return cui::layout::Alignment::Center;
		case HorizontalAlignment::Right: return cui::layout::Alignment::End;
		case HorizontalAlignment::Stretch: return cui::layout::Alignment::Stretch;
		case HorizontalAlignment::Left:
		default: return cui::layout::Alignment::Start;
		}
	}

	cui::layout::Alignment ToLayoutAlignment(VerticalAlignment value)
	{
		switch (value)
		{
		case VerticalAlignment::Center: return cui::layout::Alignment::Center;
		case VerticalAlignment::Bottom: return cui::layout::Alignment::End;
		case VerticalAlignment::Stretch: return cui::layout::Alignment::Stretch;
		case VerticalAlignment::Top:
		default: return cui::layout::Alignment::Start;
		}
	}

	cui::core::Dip ToMaximumDip(LONG value)
	{
		return value >= INT_MAX
			? cui::core::Infinity
			: (std::max)(0.0f, (float)value);
	}

	cui::core::Constraints ToMeasureConstraints(SIZE availableSize)
	{
		return cui::core::Constraints{
			cui::core::Size{
				ToMaximumDip(availableSize.cx),
				ToMaximumDip(availableSize.cy) }
		}.Normalized();
	}

	LONG ToLayoutLong(cui::core::Dip value)
	{
		if (!(value > 0.0f)) return 0;
		const auto maximum = (cui::core::Dip)(std::numeric_limits<LONG>::max)();
		return value >= maximum ? (std::numeric_limits<LONG>::max)() : (LONG)value;
	}

	LONG ToMeasureLong(cui::core::Dip value)
	{
		if (!(value > 0.0f)) return 0;
		const auto maximum = (cui::core::Dip)(std::numeric_limits<LONG>::max)();
		return value >= maximum
			? (std::numeric_limits<LONG>::max)()
			: static_cast<LONG>(std::ceil(value));
	}

	LONG ToCoordinateLong(cui::core::Dip value)
	{
		if (value != value) return 0;
		const auto maximum = (cui::core::Dip)(std::numeric_limits<LONG>::max)();
		const auto minimum = (cui::core::Dip)(std::numeric_limits<LONG>::min)();
		if (value >= maximum) return (std::numeric_limits<LONG>::max)();
		if (value <= minimum) return (std::numeric_limits<LONG>::min)();
		return (LONG)value;
	}

	cui::core::Rect ToCoreRect(D2D1_RECT_F value)
	{
		return cui::core::Rect::FromLTRB(
			value.left, value.top, value.right, value.bottom);
	}

	D2D1_RECT_F ToD2DRect(cui::core::Rect value)
	{
		return D2D1_RECT_F{
			value.Left(), value.Top(), value.Right(), value.Bottom() };
	}

	int ValidationSeverityRank(BindingValidationSeverity severity) noexcept
	{
		switch (severity)
		{
		case BindingValidationSeverity::Info: return 0;
		case BindingValidationSeverity::Warning: return 1;
		case BindingValidationSeverity::Error: return 2;
		}
		return 0;
	}

	D2D1_COLOR_F DefaultValidationColor(
		BindingValidationSeverity severity) noexcept
	{
		switch (severity)
		{
		case BindingValidationSeverity::Info:
			return D2D1_COLOR_F{ 0.12f, 0.52f, 0.88f, 1.0f };
		case BindingValidationSeverity::Warning:
			return D2D1_COLOR_F{ 0.95f, 0.62f, 0.12f, 1.0f };
		case BindingValidationSeverity::Error:
		default:
			return D2D1_COLOR_F{ 0.90f, 0.20f, 0.24f, 1.0f };
		}
	}

	ControlPropertyDesignMetadata PropertyDesign(
		std::wstring category,
		int categoryOrder,
		int order,
		ControlPropertyPersistence persistence,
		ControlPropertyEditorKind editor = ControlPropertyEditorKind::Auto,
		std::wstring displayName = {})
	{
		ControlPropertyDesignMetadata design;
		design.DisplayName = std::move(displayName);
		design.Category = std::move(category);
		design.CategoryOrder = categoryOrder;
		design.Order = order;
		design.Editor = editor;
		design.Persistence = persistence;
		return design;
	}

	template<typename TOwner, typename TValue>
	ControlPropertyOptions<TOwner, TValue> WithPropertyDesign(
		ControlPropertyOptions<TOwner, TValue> options,
		ControlPropertyDesignMetadata design)
	{
		options.Design = std::move(design);
		return options;
	}

	template<typename TValue>
	ControlPropertyChoice PropertyChoice(std::wstring displayName, TValue value)
	{
		return { std::move(displayName), BindingValue(std::move(value)) };
	}

	std::wstring StripAccessKeyMarkers(const std::wstring& text)
	{
		std::wstring result;
		result.reserve(text.size());
		for (size_t index = 0; index < text.size(); ++index)
		{
			if (text[index] != L'&')
			{
				result.push_back(text[index]);
				continue;
			}
			if (index + 1 < text.size() && text[index + 1] == L'&')
			{
				result.push_back(L'&');
				++index;
			}
		}
		return result;
	}

	wchar_t FindAccessKeyMarker(const std::wstring& text)
	{
		for (size_t index = 0; index + 1 < text.size(); ++index)
		{
			if (text[index] != L'&') continue;
			if (text[index + 1] == L'&')
			{
				++index;
				continue;
			}
			if (!std::iswspace(text[index + 1]))
				return static_cast<wchar_t>(std::towupper(text[index + 1]));
		}
		return L'\0';
	}
}

uint32_t AllocateAccessibilityVirtualId() noexcept
{
	uint32_t value = NextAccessibilityVirtualRuntimeId.fetch_add(
		1, std::memory_order_relaxed);
	if (value == 0)
		value = NextAccessibilityVirtualRuntimeId.fetch_add(
			1, std::memory_order_relaxed);
	return value;
}

Control::Control()
	:
	Enable(true),
	Checked(false),
	ParentForm(nullptr),
	Parent(nullptr),
	Tag(0),
	SizeMode(ImageSizeMode::Zoom),
	_text(L"")
{
	Children.SetOwnerSynchronizationDuringUpdates(true);
	Children.SetOwnerChangedHandler(
		[this](const CollectionChangedEventArgs& change)
		{ SynchronizeChildCollection(change); });
	this->_accessibilityRuntimeId = AllocateAccessibilityRuntimeId();
	this->_location = POINT{ 0, 0 };
	this->_runtimeLocation = POINT{ 0, 0 };
	this->_layoutStyle.horizontalAlignment = cui::layout::Alignment::Start;
	this->_layoutStyle.verticalAlignment = cui::layout::Alignment::Start;
	this->_layoutState.CommitArrange(cui::core::Rect{
		0.0f, 0.0f, (float)this->_size.cx, (float)this->_size.cy });
	_styleStateConnections.reserve(7);
	_styleStateConnections.push_back(OnMouseEnter.Subscribe(
		[this](Control*, MouseEventArgs)
		{
			SetStyleState(ControlStyleState::Hovered, true);
		}));
	_styleStateConnections.push_back(OnMouseLeave.Subscribe(
		[this](Control*, MouseEventArgs)
		{
			SetStyleState(ControlStyleState::Hovered, false);
		}));
	_styleStateConnections.push_back(OnGotFocus.Subscribe(
		[this](Control*)
		{
			SetStyleState(ControlStyleState::Focused, true);
		}));
	_styleStateConnections.push_back(OnLostFocus.Subscribe(
		[this](Control*)
		{
			_defaultLeftButtonPressActive = false;
			SetStyleState(ControlStyleState::Focused, false);
			SetStyleState(ControlStyleState::Pressed, false);
		}));
	_styleStateConnections.push_back(OnMouseDown.Subscribe(
		[this](Control*, MouseEventArgs)
		{
			SetStyleState(ControlStyleState::Pressed, true);
		}));
	_styleStateConnections.push_back(OnMouseUp.Subscribe(
		[this](Control*, MouseEventArgs)
		{
			SetStyleState(ControlStyleState::Pressed, false);
		}));
	_styleStateConnections.push_back(OnChecked.Subscribe(
		[this](Control*)
		{
			RefreshStyleValues(false);
			if (ParentForm)
				ParentForm->NotifyAccessibilityEvent(
					this, AccessibilityChange::State);
		}));
}
Control::~Control()
{
	_isDestroying = true;
	ClearDeclarativeComponentBehavior();
	_declarativeVisualStates.reset();
	// 使任何已封送但尚未执行的跨线程回调失效。
	if (_lifetimeToken) *_lifetimeToken = false;
	Children.SetOwnerChangedHandler({});
	_dataBindings.reset();
	this->_imageCache.Reset();
	this->_imageCacheTarget = nullptr;
	this->_imageSource.reset();
	if (this->_font && this->_ownsFont)
	{
		delete this->_font;
	}
	this->_font = nullptr;
	this->_ownsFont = false;
	for (auto child : this->Children)
	{
		delete child;
	}
	static_cast<ChildCollection::Base&>(Children).clear();
	_observedChildren.clear();
}
UIClass Control::Type() { return UIClass::UI_Base; }

void Control::SetLogicalParent(Control* value)
{
	if (Parent == value) return;
	auto* previous = Parent;
	Parent = value;
	RefreshInheritedPropertiesRecursive();
	// ResourceDictionary is a lexical scope rather than a copied inherited
	// property. Re-evaluate this whole subtree whenever its logical route moves.
	RebuildStyleSubscriptions(true);
	(void)RefreshDynamicResourceValues(true);
	(void)RefreshStyleValues(true);
	OnParentChanged.Invoke(this, previous, value);
}

void Control::SynchronizeChildCollection(
	const CollectionChangedEventArgs& change)
{
	const std::vector<Control*> previous = _observedChildren;
	const std::unordered_set<Control*> previousSet(
		previous.begin(), previous.end());
	std::unordered_set<Control*> currentSet;
	currentSet.reserve(Children.size());
	auto reject = [&](const char* message, bool invalidArgument = false)
		{
			static_cast<ChildCollection::Base&>(Children) = previous;
			if (invalidArgument) throw std::invalid_argument(message);
			throw std::logic_error(message);
		};

	for (auto* child : Children)
	{
		if (!child)
			reject("不能添加空控件", true);
		if (!currentSet.insert(child).second)
			reject("不能重复添加同一控件");
		for (Control* ancestor = this; ancestor; ancestor = ancestor->Parent)
		{
			if (ancestor == child)
				reject("不能将控件添加到自身或其后代");
		}
		const bool alreadyObserved = previousSet.contains(child);
		if (alreadyObserved)
		{
			if (child->Parent != this)
				reject("子控件 Parent 已在集合外被修改");
		}
		else if (child->_isFormRoot || child->Parent
			|| (child->ParentForm && child->ParentForm != this->ParentForm))
		{
			reject("该控件已属于其他容器");
		}
	}

	std::string validationError;
	if (!ValidateChildCollection(
			std::span<Control* const>{ Children.data(), Children.size() },
			validationError))
	{
		static_cast<ChildCollection::Base&>(Children) = previous;
		throw std::logic_error(validationError.empty()
			? "Specialized container rejected the child collection"
			: validationError);
	}

	Form* form = this->ParentForm;
	for (auto* child : previous)
	{
		if (!child || currentSet.contains(child)) continue;
		if (form) form->ClearDetachedControlReferences(child);
		if (child->Parent == this) child->SetLogicalParent(nullptr);
		child->_isFormRoot = false;
		child->SetInheritedDataContext({});
		SetChildrenParentForm(child, nullptr);
	}
	for (auto* child : Children)
	{
		if (previousSet.contains(child)) continue;
		child->SetLogicalParent(this);
		child->_isFormRoot = false;
		child->SetInheritedDataContext(_effectiveDataContext);
		SetChildrenParentForm(child, this->ParentForm);
		if (this->_themeStyleSheet)
			child->SetThemeStyleSheet(this->_themeStyleSheet, true);
		if (this->_styleSheet)
			child->SetStyleSheet(this->_styleSheet, true);
	}

	_observedChildren.assign(Children.begin(), Children.end());
	OnChildCollectionChanged(
		change,
		std::span<Control* const>{ previous.data(), previous.size() });
	this->RequestLayout();
	this->NotifyAccessibilityStructureChanged();
}

void Control::SetTextInternal(std::wstring text)
{
	this->_text = std::move(text);
}
void Control::Update() {}

void Control::RequestLayout()
{
	this->_layoutState.InvalidateMeasure();
	if (this->_layoutDeferral.IsSuspended())
	{
		this->_layoutDeferral.QueueLayout();
		return;
	}
	if (this->Parent)
	{
		auto* panelParent = dynamic_cast<Panel*>(this->Parent);
		if (panelParent)
		{
			panelParent->InvalidateLayout();
		}
		else
		{
			// Some composite controls are not Panel-derived but still participate in
			// the visual tree. Keep walking until a real layout boundary is found.
			this->Parent->RequestLayout();
		}
		return;
	}

	if (this->ParentForm)
	{
		this->ParentForm->InvalidateLayout();
	}
}

void Control::RequestArrange()
{
	this->_layoutState.InvalidateArrange();
	if (this->_layoutDeferral.IsSuspended())
	{
		this->_layoutDeferral.QueueLayout();
		return;
	}
	if (this->Parent)
	{
		if (auto* panelParent = dynamic_cast<Panel*>(this->Parent))
			panelParent->InvalidateLayout();
		else
			this->Parent->RequestLayout();
		return;
	}
	if (this->ParentForm)
		this->ParentForm->InvalidateLayout();
}

void Control::SuspendLayout()
{
	_layoutDeferral.Suspend();
}

void Control::ResumeLayout(bool performLayout)
{
	const auto work = _layoutDeferral.Resume();
	if (!work.ready)
		return;

	if (work.layoutRequested)
	{
		RequestLayout();
		if (performLayout)
			PerformPendingLayout();
	}

	if (work.visualRequested && !work.visualBounds.IsEmpty())
	{
		DispatchInvalidatedClientRect(ToD2DRect(work.visualBounds));
	}
}

void Control::InvalidateMeasureSubtree()
{
	_layoutState.InvalidateMeasure();
	for (auto* child : Children)
	{
		if (child)
			child->InvalidateMeasureSubtree();
	}
}

void Control::InvalidateVisualSubtree()
{
	InvalidateVisual();
	for (auto* child : Children)
		if (child) child->InvalidateVisualSubtree();
}

void Control::BeginRender()
{
	auto actualSize = this->GetActualSizeDip();
	BeginRender(actualSize.width, actualSize.height);
}
void Control::BeginRender(float clipW, float clipH)
{
	_activeGeometryClipCount = 0;
	if (!this->ParentForm || !this->ParentForm->Render) return;
	// HeadHeight is physical; divide by dpiScale to match the logical DIP transform.
	const float dpiScale = this->ParentForm->GetDpiScale();
	const float titleBarOffset = (this->ParentForm->VisibleHead ? this->ParentForm->HeadHeight / dpiScale : 0.0f);
	// Layout coordinates are relative to the form content. The control-local
	// transform is followed by ancestor transforms and finally the title bar.
	const auto transform = AsMatrix(GetLocalToRenderTransform())
		* D2D1::Matrix3x2F::Translation(0.0f, titleBarOffset);
	this->ParentForm->Render->PushLocalTransform(transform, clipW, clipH);

	std::vector<const Control*> clipOwners;
	for (auto* current = this; current; current = current->Parent)
		if (current->_clip) clipOwners.push_back(current);
	if (clipOwners.empty()) return;
	std::reverse(clipOwners.begin(), clipOwners.end());
	auto renderToLocal = AsMatrix(GetLocalToRenderTransform());
	if (!renderToLocal.Invert()) return;
	for (const auto* owner : clipOwners)
	{
		const auto ownerToLocal = AsMatrix(owner->GetLocalToRenderTransform())
			* renderToLocal;
		Microsoft::WRL::ComPtr<ID2D1Geometry> native;
		native.Attach(owner->_clip->CreateD2DGeometry(&ownerToLocal));
		if (native && this->ParentForm->Render->PushGeometryClip(native.Get()))
			++_activeGeometryClipCount;
	}
}
void Control::SetRenderDecorator(
	std::function<void(Control&, D2DGraphics&)> decorator)
{
	_renderDecorator = std::move(decorator);
	InvalidateVisual();
}

bool Control::SetDeclarativeComponentBehavior(
	std::unique_ptr<IDeclarativeComponentBehavior> behavior,
	const DeclarativeComponentBehaviorContext& context,
	std::wstring* outError)
{
	if (&context.Host != this)
	{
		if (outError) *outError = L"组件 Behavior 上下文与宿主不匹配。";
		return false;
	}
	ClearDeclarativeComponentBehavior();
	if (!behavior)
	{
		if (outError) outError->clear();
		return true;
	}

	_declarativeComponentBehavior = std::move(behavior);
	bool attached = false;
	try
	{
		attached = _declarativeComponentBehavior->Attach(
			*this, context, outError);
	}
	catch (...)
	{
		if (outError)
			*outError = L"组件 Behavior Attach 抛出异常。";
	}
	if (!attached)
	{
		auto failed = std::move(_declarativeComponentBehavior);
		try { failed->Detach(*this); } catch (...) {}
		if (outError && outError->empty())
			*outError = L"组件 Behavior 拒绝附加。";
		return false;
	}
	try
	{
		_declarativeComponentBehavior->DpiChanged(
			*this, ParentForm ? ParentForm->GetDpiScale() : 1.0f);
	}
	catch (...)
	{
	}
	InvalidateVisual();
	if (outError) outError->clear();
	return true;
}

void Control::ClearDeclarativeComponentBehavior() noexcept
{
	if (!_declarativeComponentBehavior) return;
	auto behavior = std::move(_declarativeComponentBehavior);
	try { behavior->Detach(*this); } catch (...) {}
	if (!_isDestroying) InvalidateVisual();
}

void Control::NotifyDpiChanged(float dpiScale)
{
	if (!_declarativeComponentBehavior) return;
	try
	{
		_declarativeComponentBehavior->DpiChanged(*this, dpiScale);
	}
	catch (...)
	{
	}
}

void Control::NotifyDeviceResourcesInvalidated() noexcept
{
	if (!_declarativeComponentBehavior) return;
	try
	{
		_declarativeComponentBehavior->DeviceResourcesInvalidated(*this);
	}
	catch (...)
	{
	}
}
void Control::SetForegroundBrush(const cui::drawing::Brush& brush)
{
	_foregroundBrush = brush;
	InvalidateVisual();
}
void Control::ClearForegroundBrush()
{
	if (!_foregroundBrush) return;
	_foregroundBrush.reset();
	InvalidateVisual();
}
ID2D1Brush* Control::CreateForegroundBrush(
	D2DGraphics& graphics,
	D2D1_SIZE_F bounds) const
{
	return _foregroundBrush
		? _foregroundBrush->CreateBrush(graphics, bounds)
		: nullptr;
}
void Control::SetClip(const cui::drawing::Geometry& geometry)
{
	if (_clip && *_clip == geometry) return;
	InvalidateVisualSubtree();
	_clip = geometry;
	InvalidateVisualSubtree();
}
void Control::ClearClip()
{
	if (!_clip) return;
	InvalidateVisualSubtree();
	_clip.reset();
	InvalidateVisualSubtree();
}
void Control::SetRenderTransform(const cui::drawing::Transform& transform)
{
	if (transform.Empty())
	{
		ClearRenderTransform();
		return;
	}
	if (_renderTransform && *_renderTransform == transform) return;
	InvalidateVisualSubtree();
	_renderTransform = transform;
	InvalidateVisualSubtree();
}
void Control::ClearRenderTransform()
{
	if (!_renderTransform) return;
	InvalidateVisualSubtree();
	_renderTransform.reset();
	InvalidateVisualSubtree();
}
void Control::SetRenderTransformOrigin(D2D1_POINT_2F origin)
{
	SetRenderTransformOriginDip(cui::core::Point{ origin.x, origin.y });
}
void Control::SetRenderTransformOriginDip(cui::core::Point origin)
{
	if (!std::isfinite(origin.x) || !std::isfinite(origin.y)) return;
	if (_renderTransformOrigin.x == origin.x
		&& _renderTransformOrigin.y == origin.y) return;
	InvalidateVisualSubtree();
	_renderTransformOrigin = D2D1::Point2F(origin.x, origin.y);
	InvalidateVisualSubtree();
}
void Control::EndRender()
{
	if (!this->ParentForm || !this->ParentForm->Render) return;
	if (_renderDecorator)
	{
		// Rendering extensions are optional. Never allow one to unbalance the
		// transform stack or abort the framework paint pass.
		try
		{
			_renderDecorator(*this, *this->ParentForm->Render);
		}
		catch (...)
		{
		}
	}
	if (_declarativeComponentBehavior)
	{
		try
		{
			_declarativeComponentBehavior->RenderOverlay(
				*this, *this->ParentForm->Render);
		}
		catch (...)
		{
		}
	}
	RenderFocusAdorner();
	RenderValidationAdorner();
	while (_activeGeometryClipCount > 0)
	{
		this->ParentForm->Render->PopGeometryClip();
		--_activeGeometryClipCount;
	}
	this->ParentForm->Render->PopLocalTransform();
	this->_layoutState.CommitPaint();
}

void Control::InvalidateVisual()
{
	this->InvalidateVisualRect(this->AbsRect);
}

void Control::InvalidateVisualRect(const D2D1_RECT_F& contentRect)
{
	// 线程亲和防护：失效会读写 _layoutState/ParentForm/_lastInvalidatedClientRect
	// 等 UI 线程私有状态。工作线程（如 MediaPlayer 播放线程）直接调用会造成
	// 数据竞争，因此封送回 UI 线程再真正执行。控件可能在工作线程回调时已被
	// 部分销毁，这里通过 PostToUIThread 的异步性避免在工作线程上触碰任何状态。
	if (!cui::IsUIThread())
	{
		D2D1_RECT_F rectCopy = contentRect;
		// 以弱引用捕获生命周期令牌：控件若在回调执行前销毁，令牌失效，跳过。
		std::weak_ptr<bool> weakLifetime = _lifetimeToken;
		cui::PostToUIThread([this, rectCopy, weakLifetime]() {
			auto lifetime = weakLifetime.lock();
			if (!lifetime || !*lifetime) return; // 控件已销毁
			this->InvalidateVisualRect(rectCopy);
		});
		return;
	}
	this->_layoutState.InvalidatePaint();
	if (!this->IsVisual || !this->ParentForm) return;
	const auto renderedRect = TransformAbsoluteRectToRenderSpace(contentRect);
	const RECT currentClientPixels = this->ParentForm->ContentDipRectToClientPixels(renderedRect);
	const D2D1_RECT_F currentRect{
		(float)currentClientPixels.left,
		(float)currentClientPixels.top,
		(float)currentClientPixels.right,
		(float)currentClientPixels.bottom
	};

	D2D1_RECT_F invalidRect = currentRect;
	if (_hasLastInvalidatedClientRect)
	{
		invalidRect.left = (std::min)(_lastInvalidatedClientRect.left, currentRect.left);
		invalidRect.top = (std::min)(_lastInvalidatedClientRect.top, currentRect.top);
		invalidRect.right = (std::max)(_lastInvalidatedClientRect.right, currentRect.right);
		invalidRect.bottom = (std::max)(_lastInvalidatedClientRect.bottom, currentRect.bottom);
	}
	DispatchInvalidatedClientRect(invalidRect);

	_lastInvalidatedClientRect = currentRect;
	_hasLastInvalidatedClientRect = true;
}

void Control::DispatchInvalidatedClientRect(const D2D1_RECT_F& clientRect)
{
	for (Control* current = this; current; current = current->Parent)
	{
		if (current->_layoutDeferral.IsSuspended())
		{
			current->_layoutDeferral.QueueVisual(ToCoreRect(clientRect));
			return;
		}
	}
	if (this->ParentForm)
		this->ParentForm->Invalidate(clientRect, false);
}

void Control::UpdateCaretBlinkState(bool focused, int selectionStart, int selectionEnd, bool caretRectValid, const D2D1_RECT_F* caretRect)
{
	bool shouldResetBlink = false;
	if (focused != _caretBlinkFocused)
		shouldResetBlink = focused;
	if (selectionStart != _caretBlinkSelectionStart || selectionEnd != _caretBlinkSelectionEnd)
		shouldResetBlink = true;
	if (caretRectValid != _caretBlinkRectValid)
		shouldResetBlink = true;
	if (caretRectValid && caretRect)
	{
		if (!_caretBlinkRectValid ||
			std::fabs(_caretBlinkRect.left - caretRect->left) > 0.1f ||
			std::fabs(_caretBlinkRect.top - caretRect->top) > 0.1f ||
			std::fabs(_caretBlinkRect.right - caretRect->right) > 0.1f ||
			std::fabs(_caretBlinkRect.bottom - caretRect->bottom) > 0.1f)
		{
			shouldResetBlink = true;
		}
		_caretBlinkRect = *caretRect;
	}
	else
	{
		_caretBlinkRect = { 0,0,0,0 };
	}

	_caretBlinkFocused = focused;
	_caretBlinkSelectionStart = selectionStart;
	_caretBlinkSelectionEnd = selectionEnd;
	_caretBlinkRectValid = caretRectValid;

	if (shouldResetBlink || _caretBlinkResetTick == 0)
		_caretBlinkResetTick = ::GetTickCount64();
}

bool Control::IsCaretBlinkVisible() const
{
	if (!_caretBlinkFocused) return false;
	if (!_caretBlinkRectValid) return false;
	if (_caretBlinkSelectionStart != _caretBlinkSelectionEnd) return false;

	const UINT blinkTime = ::GetCaretBlinkTime();
	if (blinkTime == INFINITE || blinkTime == 0)
		return true;

	const ULONGLONG elapsed = ::GetTickCount64() - _caretBlinkResetTick;
	return ((elapsed / blinkTime) % 2ULL) == 0;
}

bool Control::IsCaretBlinkAnimating() const
{
	if (!_caretBlinkFocused) return false;
	if (!_caretBlinkRectValid) return false;
	if (_caretBlinkSelectionStart != _caretBlinkSelectionEnd) return false;

	const UINT blinkTime = ::GetCaretBlinkTime();
	return blinkTime != 0 && blinkTime != INFINITE;
}

bool Control::GetCaretBlinkInvalidRect(D2D1_RECT_F& outRect) const
{
	if (!_caretBlinkFocused) return false;
	if (!_caretBlinkRectValid) return false;
	if (_caretBlinkSelectionStart != _caretBlinkSelectionEnd) return false;
	outRect = _caretBlinkRect;
	return true;
}

GET_CPP(Control, class Font*, Font)
{
	if (!this->_font)
		return this->ParentForm
			? this->ParentForm->GetFont() : GetDefaultFontObject();
	const float factor = this->ParentForm
		? this->ParentForm->GetTextScaleFactor() : 1.0f;
	if (!(factor > 1.0001f)) return this->_font;
	const float sourceSize = this->_font->FontSize;
	if (!this->_systemScaledFont || this->_systemScaledFontSource != this->_font
		|| std::fabs(this->_systemScaledFontSourceSize - sourceSize) > 0.001f
		|| std::fabs(this->_systemScaledFontFactor - factor) > 0.001f)
	{
		this->_systemScaledFont = std::make_unique<::Font>(
			this->_font->FontName, sourceSize * factor);
		this->_systemScaledFontSource = this->_font;
		this->_systemScaledFontSourceSize = sourceSize;
		this->_systemScaledFontFactor = factor;
	}
	return this->_systemScaledFont.get();
}
SET_CPP(Control, class Font*, Font)
{
	this->SetFontEx(value, true);
}

void Control::SetFontEx(class Font* value, bool takeOwnership)
{
	if (value == GetDefaultFontObject())
	{
		value = nullptr;
		takeOwnership = false;
	}

	if (value == this->_font)
	{
		this->_ownsFont = takeOwnership;
		return;
	}

	this->_systemScaledFont.reset();
	this->_systemScaledFontSource = nullptr;
	if (this->_font && this->_ownsFont)
	{
		delete this->_font;
	}
	this->_font = value;
	this->_ownsFont = takeOwnership;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, BindingCollection&, DataBindings)
{
	if (!this->_dataBindings)
		this->_dataBindings = std::make_unique<BindingCollection>(this);
	return *this->_dataBindings;
}

namespace
{
	std::wstring DynamicPropertyKey(const std::wstring& value)
	{
		std::wstring result;
		result.reserve(value.size());
		for (const auto ch : value)
			result.push_back(static_cast<wchar_t>(std::towlower(ch)));
		return result;
	}

	bool IsDynamicPropertyName(const std::wstring& value)
	{
		if (value.empty()
			|| !(std::iswalpha(value.front()) || value.front() == L'_'))
			return false;
		return std::all_of(value.begin() + 1, value.end(), [](wchar_t ch)
		{
			return std::iswalnum(ch) || ch == L'_';
		});
	}

	std::type_index DynamicPropertyValueType(
		BindingValueKind kind,
		const BindingValue& defaultValue)
	{
		switch (kind)
		{
		case BindingValueKind::Bool: return std::type_index(typeid(bool));
		case BindingValueKind::Int: return std::type_index(typeid(int));
		case BindingValueKind::Int64: return std::type_index(typeid(long long));
		case BindingValueKind::Float: return std::type_index(typeid(float));
		case BindingValueKind::Double: return std::type_index(typeid(double));
		case BindingValueKind::String: return std::type_index(typeid(std::wstring));
		case BindingValueKind::Object:
			return std::type_index(defaultValue.Type());
		default: return std::type_index(typeid(void));
		}
	}

	bool DynamicPropertyValuesEqual(
		const BindingValue& left,
		const BindingValue& right)
	{
		if (left.Kind() != BindingValueKind::Object
			|| right.Kind() != BindingValueKind::Object)
			return BindingValuesEqual(left, right);
		if (std::type_index(left.Type()) != std::type_index(right.Type()))
			return false;

		if (left.Type() == typeid(D2D1_COLOR_F))
		{
			D2D1_COLOR_F a{}, b{};
			return left.TryGet(a) && right.TryGet(b)
				&& a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
		}
		if (left.Type() == typeid(Thickness))
		{
			Thickness a, b;
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(SIZE))
		{
			SIZE a{}, b{};
			return left.TryGet(a) && right.TryGet(b)
				&& a.cx == b.cx && a.cy == b.cy;
		}
		if (left.Type() == typeid(cui::core::Size))
		{
			cui::core::Size a{}, b{};
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(D2D1_MATRIX_3X2_F))
		{
			D2D1_MATRIX_3X2_F a{}, b{};
			return left.TryGet(a) && right.TryGet(b)
				&& a._11 == b._11 && a._12 == b._12
				&& a._21 == b._21 && a._22 == b._22
				&& a._31 == b._31 && a._32 == b._32;
		}
		if (left.Type() == typeid(cui::core::Point))
		{
			cui::core::Point a{}, b{};
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::core::Vector))
		{
			cui::core::Vector a{}, b{};
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::core::Rect))
		{
			cui::core::Rect a{}, b{};
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::layout::Length))
		{
			cui::layout::Length a, b;
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::drawing::Transform))
		{
			cui::drawing::Transform a, b;
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::drawing::Geometry))
		{
			cui::drawing::Geometry a, b;
			return left.TryGet(a) && right.TryGet(b) && a == b;
		}
		if (left.Type() == typeid(cui::drawing::Brush))
		{
			cui::drawing::Brush a, b;
			if (!left.TryGet(a) || !right.TryGet(b)) return false;
			return a.Kind == b.Kind
				&& a.MappingMode == b.MappingMode
				&& a.Color.r == b.Color.r && a.Color.g == b.Color.g
				&& a.Color.b == b.Color.b && a.Color.a == b.Color.a
				&& a.Opacity == b.Opacity
				&& a.StartPoint.x == b.StartPoint.x && a.StartPoint.y == b.StartPoint.y
				&& a.EndPoint.x == b.EndPoint.x && a.EndPoint.y == b.EndPoint.y
				&& a.Center.x == b.Center.x && a.Center.y == b.Center.y
				&& a.GradientOrigin.x == b.GradientOrigin.x
				&& a.GradientOrigin.y == b.GradientOrigin.y
				&& a.RadiusX == b.RadiusX && a.RadiusY == b.RadiusY
				&& a.GradientStops == b.GradientStops
				&& a.Transform == b.Transform
				&& a.RelativeTransform == b.RelativeTransform
				&& a.ImageSource == b.ImageSource
				&& a.Stretch == b.Stretch
				&& a.AlignmentX == b.AlignmentX
				&& a.AlignmentY == b.AlignmentY;
		}
		return false;
	}
}

bool Control::DefineDynamicProperty(
	DynamicControlPropertyDefinition definition,
	std::wstring* outError)
{
	auto fail = [&](const wchar_t* message)
	{
		if (outError) *outError = message;
		return false;
	};
	if (!IsDynamicPropertyName(definition.Name))
		return fail(L"动态属性名称必须是有效的 XAML 标识符。");
	if (definition.ValueKind == BindingValueKind::Empty)
		return fail(L"动态属性必须声明非空值类型。");
	if (definition.ValueKind == BindingValueKind::Object
		&& (definition.DefaultValue.Kind() != BindingValueKind::Object
			|| definition.DefaultValue.Type() == typeid(void)))
		return fail(L"对象动态属性必须提供具体类型的默认值。");
	if (HasControlPropertyFlag(definition.Flags, ControlPropertyFlags::Inherits)
		&& definition.InheritanceKey.empty())
		return fail(L"可继承动态属性必须提供稳定的 InheritanceKey。");
	if (definition.DefaultUpdateMode == DataSourceUpdateMode::Default)
		return fail(L"动态属性的默认更新触发器必须是具体值。");
	if (definition.IsReadOnly && HasControlPropertyFlag(
		definition.Flags, ControlPropertyFlags::BindsTwoWayByDefault))
		return fail(L"只读动态属性不能声明 BindsTwoWayByDefault。");
	if (definition.IsReadOnly && definition.DefaultUpdateMode
		!= DataSourceUpdateMode::OnPropertyChanged)
		return fail(L"只读动态属性不能声明默认源更新触发器。");

	const auto key = DynamicPropertyKey(definition.Name);
	if (_dynamicProperties.find(key) != _dynamicProperties.end())
		return fail(L"动态属性名称重复。");
	// Registry::Find cannot see the new entry yet, so this safely checks native
	// metadata without allowing a declarative component to change its meaning.
	if (BindingPropertyRegistry::Find(*this, definition.Name))
		return fail(L"动态属性不能覆盖控件已有属性。");

	BindingValue normalizedDefault;
	if (definition.ValueKind == BindingValueKind::Object)
		normalizedDefault = definition.DefaultValue;
	else if (!TryConvertBindingValue(
		definition.DefaultValue, definition.ValueKind, normalizedDefault))
		return fail(L"动态属性默认值无法转换为声明类型。");
	std::vector<BindingValue> normalizedAllowedValues;
	normalizedAllowedValues.reserve(definition.AllowedValues.size());
	for (const auto& candidate : definition.AllowedValues)
	{
		BindingValue normalized;
		const bool converted = definition.ValueKind == BindingValueKind::Object
			? TryConvertBindingValue(candidate, normalizedDefault, normalized)
			: TryConvertBindingValue(candidate, definition.ValueKind, normalized);
		if (!converted)
			return fail(L"动态属性候选值无法转换为声明类型。");
		if (std::any_of(normalizedAllowedValues.begin(), normalizedAllowedValues.end(),
			[&](const auto& existing)
			{
				return DynamicPropertyValuesEqual(existing, normalized);
			}))
			return fail(L"动态属性候选值重复。");
		normalizedAllowedValues.push_back(std::move(normalized));
	}
	if (!normalizedAllowedValues.empty()
		&& std::none_of(normalizedAllowedValues.begin(), normalizedAllowedValues.end(),
			[&](const auto& candidate)
			{
				return DynamicPropertyValuesEqual(candidate, normalizedDefault);
			}))
		return fail(L"动态属性默认值不在允许的候选集合中。");
	if (definition.Design.Persistence == ControlPropertyPersistence::Automatic)
		definition.Design.Persistence = ControlPropertyPersistence::Metadata;

	const auto canonicalName = definition.Name;
	auto metadata = std::unique_ptr<BindingPropertyMetadata>(
		new BindingPropertyMetadata(
			canonicalName,
			definition.ValueKind,
			DynamicPropertyValueType(definition.ValueKind, normalizedDefault),
			std::type_index(typeid(Control)),
			[this](const Control& target) { return &target == this; },
			[kind = definition.ValueKind, objectDefault = normalizedDefault,
				allowedValues = std::move(normalizedAllowedValues)](
				const BindingValue& value, BindingValue& converted)
			{
				bool success = false;
				if (kind == BindingValueKind::Object)
					success = TryConvertBindingValue(value, objectDefault, converted);
				else
					success = TryConvertBindingValue(value, kind, converted);
				if (!success) return false;
				if (allowedValues.empty()) return true;
				if (kind == BindingValueKind::String)
				{
					std::wstring actual;
					if (!converted.TryGet(actual)) return false;
					for (const auto& candidate : allowedValues)
					{
						std::wstring expected;
						if (candidate.TryGet(expected)
							&& _wcsicmp(actual.c_str(), expected.c_str()) == 0)
						{
							converted = candidate;
							return true;
						}
					}
					return false;
				}
				return std::any_of(allowedValues.begin(), allowedValues.end(),
					[&](const auto& candidate)
					{
						return DynamicPropertyValuesEqual(candidate, converted);
					});
			},
			{},
			[](const BindingValue& left, const BindingValue& right)
			{
				return DynamicPropertyValuesEqual(left, right);
			},
			[canonicalName](Control& target, BindingValue& value)
			{
				return target.TryGetDynamicPropertyBacking(canonicalName, value);
			},
			[canonicalName](Control& target, const BindingValue& value)
			{
				return target.TrySetDynamicPropertyBacking(canonicalName, value);
			},
			[canonicalName](
				Control& target,
				BindingPropertyMetadata::ChangeHandler handler,
				DataSourceUpdateMode updateMode)
			{
				if (updateMode == DataSourceUpdateMode::OnValidation)
					return target.OnLostFocus.Subscribe(
						[handler = std::move(handler)](Control*) { handler(); });
				return target.OnPropertyValueChanged.Subscribe(
					[canonicalName, handler = std::move(handler)](
						Control*, const ControlPropertyChangedEventArgs& args)
					{
						if (_wcsicmp(args.PropertyName.c_str(), canonicalName.c_str()) == 0)
							handler();
					});
			},
			{},
			normalizedDefault,
			true,
			definition.Flags,
			definition.IsReadOnly,
			definition.DefaultUpdateMode,
			std::move(definition.InheritanceKey),
			std::move(definition.Design)));

	DynamicPropertyEntry entry;
	entry.Metadata = std::move(metadata);
	entry.Value = std::move(normalizedDefault);
	_dynamicProperties.emplace(key, std::move(entry));
	if (HasControlPropertyFlag(
		definition.Flags, ControlPropertyFlags::Inherits))
		RefreshInheritedPropertiesRecursive();
	if (outError) outError->clear();
	return true;
}

const BindingPropertyMetadata* Control::FindDynamicPropertyMetadata(
	const std::wstring& propertyName) const
{
	const auto entry = _dynamicProperties.find(DynamicPropertyKey(propertyName));
	return entry == _dynamicProperties.end() ? nullptr : entry->second.Metadata.get();
}

std::vector<const BindingPropertyMetadata*> Control::GetDynamicPropertyMetadata() const
{
	std::vector<const BindingPropertyMetadata*> result;
	result.reserve(_dynamicProperties.size());
	for (const auto& [_, entry] : _dynamicProperties)
		result.push_back(entry.Metadata.get());
	return result;
}

bool Control::TryGetDynamicPropertyBacking(
	const std::wstring& propertyName,
	BindingValue& out) const
{
	const auto entry = _dynamicProperties.find(DynamicPropertyKey(propertyName));
	if (entry == _dynamicProperties.end()) return false;
	out = entry->second.Value;
	return true;
}

bool Control::TrySetDynamicPropertyBacking(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto entry = _dynamicProperties.find(DynamicPropertyKey(propertyName));
	if (entry == _dynamicProperties.end()) return false;
	entry->second.Value = value;
	return true;
}

const BindingPropertyMetadata* Control::FindPropertyMetadata(
	const std::wstring& propertyName)
{
	return BindingPropertyRegistry::Find(*this, propertyName);
}

bool Control::TryGetPropertyValue(
	const std::wstring& propertyName,
	BindingValue& out)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	return metadata && metadata->TryGet(*this, out);
}

bool Control::TryGetPropertyValue(
	const std::wstring& propertyName,
	ControlPropertyValueSource source,
	BindingValue& out)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return false;
	if (source == ControlPropertyValueSource::Default)
		return metadata->TryGetDefaultValue(out);
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return false;
	const auto entry = _propertyValues.find(metadata);
	if (entry == _propertyValues.end()
		|| !entry->second.Values[(size_t)index].has_value())
		return false;
	out = *entry->second.Values[(size_t)index];
	return true;
}

bool Control::TrySetPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	return TrySetPropertyValue(
		propertyName, value, ControlPropertyValueSource::Local);
}

bool Control::TrySetPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value,
	ControlPropertyValueSource source)
{
	return TrySetPropertyValueOwned(propertyName, value, source, nullptr);
}

bool Control::TrySetPropertyValueOwned(
	const std::wstring& propertyName,
	const BindingValue& value,
	ControlPropertyValueSource source,
	const Binding* owner,
	bool allowReadOnly)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0
		|| (!metadata->CanWrite()
			&& !(allowReadOnly && metadata->IsReadOnly()
				&& metadata->CanWriteInternally()))) return false;

	BindingValue converted;
	BindingValue effective;
	if (!metadata->TryConvert(value, converted)
		|| !metadata->TryCoerce(*this, converted, effective))
		return false;
	std::optional<std::wstring> replacedDynamicResource;
	if (source == ControlPropertyValueSource::Local
		&& !_applyingDynamicResource)
	{
		const auto expression = _dynamicResourceExpressions.find(metadata);
		if (expression != _dynamicResourceExpressions.end())
		{
			replacedDynamicResource = std::move(expression->second);
			_dynamicResourceExpressions.erase(expression);
		}
	}

	auto [entryIt, inserted] = _propertyValues.try_emplace(metadata);
	auto& entry = entryIt->second;
	const size_t sourceIndex = (size_t)index;
	const Binding* previousOwner = entry.BindingOwners[sourceIndex];
	if (source == ControlPropertyValueSource::Binding
		&& previousOwner && previousOwner != owner)
	{
		if (replacedDynamicResource)
			_dynamicResourceExpressions[metadata]
				= std::move(*replacedDynamicResource);
		if (inserted) _propertyValues.erase(entryIt);
		return false;
	}
	if (inserted)
	{
		entry.HasBaseValue = metadata->TryGet(*this, entry.BaseValue);
		if (!entry.HasBaseValue)
			entry.HasBaseValue = metadata->TryGetDefaultValue(entry.BaseValue);
	}

	BindingValue oldEffective;
	ControlPropertyValueSource oldSource = ControlPropertyValueSource::Default;
	const bool hadOldEffective = TryResolveEffectivePropertyValue(
		*metadata, entry, oldEffective, oldSource);
	auto previous = entry.Values[sourceIndex];
	entry.Values[sourceIndex] = effective;
	if (source == ControlPropertyValueSource::Binding)
		entry.BindingOwners[sourceIndex] = owner;

	BindingValue newEffective;
	ControlPropertyValueSource newSource = ControlPropertyValueSource::Default;
	if (!TryResolveEffectivePropertyValue(
		*metadata, entry, newEffective, newSource))
	{
		entry.Values[sourceIndex] = std::move(previous);
		entry.BindingOwners[sourceIndex] = previousOwner;
		if (replacedDynamicResource)
			_dynamicResourceExpressions[metadata]
				= std::move(*replacedDynamicResource);
		if (inserted) _propertyValues.erase(entryIt);
		return false;
	}

	const bool effectiveUnchanged = hadOldEffective
		&& oldSource == newSource
		&& (newSource != source
			|| metadata->ValuesEqual(oldEffective, newEffective));
	if (effectiveUnchanged) return true;
	if (ApplyEffectivePropertyValue(
		*metadata, newEffective, newSource, allowReadOnly))
		return true;

	entry.Values[sourceIndex] = std::move(previous);
	entry.BindingOwners[sourceIndex] = previousOwner;
	if (replacedDynamicResource)
		_dynamicResourceExpressions[metadata]
			= std::move(*replacedDynamicResource);
	if (inserted) _propertyValues.erase(entryIt);
	return false;
}

bool Control::CanAcquireBindingPropertyValue(
	const std::wstring& propertyName,
	const Binding* owner)
{
	if (!owner) return false;
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;
	const int index = StoredPropertySourceIndex(
		ControlPropertyValueSource::Binding);
	const auto entry = _propertyValues.find(metadata);
	if (entry == _propertyValues.end()) return true;
	const Binding* currentOwner = entry->second.BindingOwners[(size_t)index];
	return !currentOwner || currentOwner == owner;
}

bool Control::TrySetBindingPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value,
	const Binding* owner)
{
	if (!owner) return false;
	return TrySetPropertyValueOwned(
		propertyName, value, ControlPropertyValueSource::Binding, owner);
}

bool Control::TrySetCurrentPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto source = GetPropertyValueSource(propertyName);
	if (source != ControlPropertyValueSource::Binding)
		return TrySetPropertyValue(
			propertyName, value, ControlPropertyValueSource::Local);

	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	const auto entry = metadata ? _propertyValues.find(metadata) : _propertyValues.end();
	const Binding* owner = entry != _propertyValues.end()
		? entry->second.BindingOwners[(size_t)index]
		: nullptr;
	return TrySetPropertyValueOwned(propertyName, value, source, owner);
}

bool Control::TrySetReadOnlyPropertyValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->IsReadOnly()) return false;
	return TrySetPropertyValueOwned(
		metadata->Name(), value, ControlPropertyValueSource::Local, nullptr, true);
}

bool Control::ClearReadOnlyPropertyValue(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->IsReadOnly()) return false;
	return ClearPropertyValueOwned(
		metadata->Name(), ControlPropertyValueSource::Local, nullptr, true);
}

bool Control::ReevaluatePropertyValue(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;

	const auto entry = _propertyValues.find(metadata);
	if (entry != _propertyValues.end())
	{
		BindingValue proposed;
		ControlPropertyValueSource source = ControlPropertyValueSource::Default;
		if (!TryResolveEffectivePropertyValue(
			*metadata, entry->second, proposed, source)) return false;

		if (source != ControlPropertyValueSource::Default)
		{
			const int index = StoredPropertySourceIndex(source);
			const Binding* owner = index >= 0
				? entry->second.BindingOwners[(size_t)index] : nullptr;
			return TrySetPropertyValueOwned(
				propertyName, proposed, source, owner);
		}
	}

	BindingValue proposed;
	if (!metadata->TryGetDefaultValue(proposed)
		&& !metadata->TryGet(*this, proposed)) return false;
	BindingValue converted;
	BindingValue effective;
	if (!metadata->TryConvert(proposed, converted)
		|| !metadata->TryCoerce(*this, converted, effective)) return false;

	BindingValue current;
	if (metadata->TryGet(*this, current)
		&& metadata->ValuesEqual(current, effective)) return true;
	return ApplyEffectivePropertyValue(
		*metadata, effective, ControlPropertyValueSource::Default);
}

bool Control::ClearPropertyValue(
	const std::wstring& propertyName,
	ControlPropertyValueSource source)
{
	return ClearPropertyValueOwned(propertyName, source, nullptr);
}

bool Control::ClearPropertyValueOwned(
	const std::wstring& propertyName,
	ControlPropertyValueSource source,
	const Binding* owner,
	bool allowReadOnly)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0
		|| (!metadata->CanWrite()
			&& !(allowReadOnly && metadata->IsReadOnly()
				&& metadata->CanWriteInternally()))) return false;
	std::optional<std::wstring> removedDynamicResource;
	if (source == ControlPropertyValueSource::Local
		&& !_applyingDynamicResource)
	{
		const auto expression = _dynamicResourceExpressions.find(metadata);
		if (expression != _dynamicResourceExpressions.end())
		{
			removedDynamicResource = std::move(expression->second);
			_dynamicResourceExpressions.erase(expression);
		}
	}
	auto entryIt = _propertyValues.find(metadata);
	if (entryIt == _propertyValues.end()) return removedDynamicResource.has_value();
	auto& entry = entryIt->second;
	const size_t sourceIndex = (size_t)index;
	if (!entry.Values[sourceIndex].has_value()) return removedDynamicResource.has_value();
	if (source == ControlPropertyValueSource::Binding)
	{
		const Binding* currentOwner = entry.BindingOwners[sourceIndex];
		if (owner ? currentOwner != owner : currentOwner != nullptr)
			return false;
	}

	BindingValue oldEffective;
	ControlPropertyValueSource oldSource = ControlPropertyValueSource::Default;
	const bool hadOldEffective = TryResolveEffectivePropertyValue(
		*metadata, entry, oldEffective, oldSource);
	auto previous = std::move(entry.Values[sourceIndex]);
	const Binding* previousOwner = entry.BindingOwners[sourceIndex];
	entry.Values[sourceIndex].reset();
	entry.BindingOwners[sourceIndex] = nullptr;

	BindingValue newEffective;
	ControlPropertyValueSource newSource = ControlPropertyValueSource::Default;
	const bool hasNewEffective = TryResolveEffectivePropertyValue(
		*metadata, entry, newEffective, newSource);
	const bool effectiveUnchanged = hadOldEffective && hasNewEffective
		&& oldSource == newSource;
	const bool applied = effectiveUnchanged || !hasNewEffective
		|| metadata->ValuesEqual(oldEffective, newEffective)
		|| ApplyEffectivePropertyValue(
			*metadata, newEffective, newSource, allowReadOnly);
	if (!applied)
	{
		entry.Values[sourceIndex] = std::move(previous);
		entry.BindingOwners[sourceIndex] = previousOwner;
		if (removedDynamicResource)
			_dynamicResourceExpressions[metadata]
				= std::move(*removedDynamicResource);
		return false;
	}
	if (!entry.HasSources()) _propertyValues.erase(entryIt);
	return true;

}

bool Control::ClearBindingPropertyValue(
	const std::wstring& propertyName,
	const Binding* owner)
{
	if (!owner) return false;
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return false;
	const int index = StoredPropertySourceIndex(
		ControlPropertyValueSource::Binding);
	const auto entry = _propertyValues.find(metadata);
	if (entry == _propertyValues.end()
		|| entry->second.BindingOwners[(size_t)index] != owner)
		return false;
	return ClearPropertyValueOwned(
		propertyName, ControlPropertyValueSource::Binding, owner);
}

size_t Control::ClearPropertyValues(ControlPropertyValueSource source)
{
	const int index = StoredPropertySourceIndex(source);
	if (index < 0) return 0;
	std::vector<std::wstring> properties;
	properties.reserve(_propertyValues.size());
	for (const auto& [metadata, entry] : _propertyValues)
	{
		if (metadata && entry.Values[(size_t)index].has_value())
			properties.push_back(metadata->Name());
	}
	if (source == ControlPropertyValueSource::Local)
	{
		for (const auto& [metadata, resourceKey] : _dynamicResourceExpressions)
		{
			(void)resourceKey;
			if (metadata && std::find(properties.begin(), properties.end(),
				metadata->Name()) == properties.end())
				properties.push_back(metadata->Name());
		}
	}
	size_t cleared = 0;
	for (const auto& property : properties)
	{
		if (ClearPropertyValue(property, source)) ++cleared;
	}
	return cleared;
}

bool Control::HasPropertyValue(
	const std::wstring& propertyName,
	ControlPropertyValueSource source)
{
	BindingValue ignored;
	return TryGetPropertyValue(propertyName, source, ignored);
}

ControlPropertyValueSource Control::GetPropertyValueSource(
	const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata) return ControlPropertyValueSource::Default;
	const auto entry = _propertyValues.find(metadata);
	if (entry == _propertyValues.end())
		return ControlPropertyValueSource::Default;
	BindingValue value;
	ControlPropertyValueSource source = ControlPropertyValueSource::Default;
	TryResolveEffectivePropertyValue(*metadata, entry->second, value, source);
	return source;
}

bool Control::ResetPropertyValue(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;
	if (ClearPropertyValue(propertyName, ControlPropertyValueSource::Local))
		return true;
	const auto entry = _propertyValues.find(metadata);
	if (entry != _propertyValues.end() && entry->second.HasSources())
		return false;
	BindingValue defaultValue;
	return metadata->TryGetDefaultValue(defaultValue)
		&& ApplyEffectivePropertyValue(
			*metadata, defaultValue, ControlPropertyValueSource::Default);
}

bool Control::TrySetPropertyBaseValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanWrite()) return false;
	BindingValue converted;
	BindingValue effective;
	if (!metadata->TryConvert(value, converted)
		|| !metadata->TryCoerce(*this, converted, effective))
		return false;

	auto entryIt = _propertyValues.find(metadata);
	if (entryIt == _propertyValues.end())
		return ApplyEffectivePropertyValue(
			*metadata, effective, ControlPropertyValueSource::Default);

	auto& entry = entryIt->second;
	const auto previousBase = entry.BaseValue;
	const bool previouslyHadBase = entry.HasBaseValue;
	BindingValue previousEffective;
	ControlPropertyValueSource previousSource =
		ControlPropertyValueSource::Default;
	const bool hadPreviousEffective = TryResolveEffectivePropertyValue(
		*metadata, entry, previousEffective, previousSource);
	entry.BaseValue = effective;
	entry.HasBaseValue = true;

	BindingValue nextEffective;
	ControlPropertyValueSource nextSource = ControlPropertyValueSource::Default;
	if (!TryResolveEffectivePropertyValue(
		*metadata, entry, nextEffective, nextSource))
	{
		entry.BaseValue = previousBase;
		entry.HasBaseValue = previouslyHadBase;
		return false;
	}
	if (nextSource != ControlPropertyValueSource::Default
		|| (hadPreviousEffective
			&& previousSource == nextSource
			&& metadata->ValuesEqual(previousEffective, nextEffective)))
		return true;
	if (ApplyEffectivePropertyValue(*metadata, nextEffective, nextSource))
		return true;
	entry.BaseValue = previousBase;
	entry.HasBaseValue = previouslyHadBase;
	return false;
}

bool Control::IsPropertyValueDefault(const std::wstring& propertyName)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	if (!metadata || !metadata->CanRead()) return false;
	BindingValue currentValue;
	BindingValue defaultValue;
	return metadata->TryGet(*this, currentValue)
		&& metadata->TryGetDefaultValue(defaultValue)
		&& metadata->ValuesEqual(currentValue, defaultValue);
}

bool Control::TryResolveEffectivePropertyValue(
	const BindingPropertyMetadata& metadata,
	const PropertyValueEntry& entry,
	BindingValue& value,
	ControlPropertyValueSource& source) const
{
	for (int index = (int)entry.Values.size() - 1; index >= 0; --index)
	{
		if (!entry.Values[(size_t)index].has_value()) continue;
		value = *entry.Values[(size_t)index];
		source = static_cast<ControlPropertyValueSource>(
			index + static_cast<int>(ControlPropertyValueSource::Inherited));
		return true;
	}
	if (entry.HasBaseValue)
	{
		value = entry.BaseValue;
		source = ControlPropertyValueSource::Default;
		return true;
	}
	source = ControlPropertyValueSource::Default;
	return metadata.TryGetDefaultValue(value);
}

bool Control::ApplyEffectivePropertyValue(
	const BindingPropertyMetadata& metadata,
	const BindingValue& value,
	ControlPropertyValueSource source,
	bool allowReadOnly)
{
	const auto* previousMetadata = _applyingPropertyMetadata;
	const auto previousSource = _applyingPropertySource;
	_applyingPropertyMetadata = &metadata;
	_applyingPropertySource = source;
	bool result = false;
	try
	{
		result = allowReadOnly
			? metadata.TrySetInternal(*this, value)
			: metadata.TrySet(*this, value);
	}
	catch (...)
	{
		_applyingPropertyMetadata = previousMetadata;
		_applyingPropertySource = previousSource;
		throw;
	}
	_applyingPropertyMetadata = previousMetadata;
	_applyingPropertySource = previousSource;
	return result;
}

bool Control::HasStoredPropertyValues(
	const BindingPropertyMetadata& metadata) const
{
	const auto entry = _propertyValues.find(&metadata);
	return entry != _propertyValues.end() && entry->second.HasSources();
}

Control* Control::FindDeclarativeTemplatePart(
	const std::wstring& localName) noexcept
{
	return const_cast<Control*>(static_cast<const Control*>(this)
		->FindDeclarativeTemplatePart(localName));
}

const Control* Control::FindDeclarativeTemplatePart(
	const std::wstring& localName) const noexcept
{
	const auto found = std::find_if(
		_declarativeTemplateParts.begin(), _declarativeTemplateParts.end(),
		[&](const auto& item) { return item.first == localName; });
	return found == _declarativeTemplateParts.end() ? nullptr : found->second;
}

Control* Control::FindDeclarativeContentPresenter(
	const std::wstring& propertyName) noexcept
{
	return const_cast<Control*>(static_cast<const Control*>(this)
		->FindDeclarativeContentPresenter(propertyName));
}

const Control* Control::FindDeclarativeContentPresenter(
	const std::wstring& propertyName) const noexcept
{
	const auto found = std::find_if(
		_declarativeContentPresenters.begin(),
		_declarativeContentPresenters.end(),
		[&](const auto& item) { return item.first == propertyName; });
	return found == _declarativeContentPresenters.end() ? nullptr : found->second;
}

bool Control::RegisterDeclarativeTemplatePart(
	std::wstring localName,
	Control* instance)
{
	if (localName.empty() || !instance
		|| FindDeclarativeTemplatePart(localName)) return false;
	_declarativeTemplateParts.emplace_back(
		std::move(localName), instance);
	return true;
}

bool Control::RegisterDeclarativeContentPresenter(
	std::wstring propertyName,
	Control* instance)
{
	if (propertyName.empty() || !instance
		|| FindDeclarativeContentPresenter(propertyName)) return false;
	_declarativeContentPresenters.emplace_back(
		std::move(propertyName), instance);
	return true;
}

void Control::RefreshInheritedPropertyValues()
{
	const auto properties = BindingPropertyRegistry::GetProperties(*this);
	for (const auto* metadata : properties)
	{
		if (!metadata || !HasControlPropertyFlag(
			metadata->Flags(), ControlPropertyFlags::Inherits)) continue;

		BindingValue inheritedValue;
		bool found = false;
		for (auto* ancestor = Parent; ancestor; ancestor = ancestor->Parent)
		{
			const auto* candidate = BindingPropertyRegistry::Find(
				*ancestor, metadata->Name());
			if (!candidate
				|| !HasControlPropertyFlag(
					candidate->Flags(), ControlPropertyFlags::Inherits)
				|| !metadata->HasSameInheritanceIdentity(*candidate))
				continue;
			if (candidate->TryGet(*ancestor, inheritedValue))
			{
				found = true;
				break;
			}
		}

		if (found)
			(void)TrySetPropertyValueOwned(
				metadata->Name(), inheritedValue,
				ControlPropertyValueSource::Inherited, nullptr, true);
		else
			(void)ClearPropertyValueOwned(
				metadata->Name(), ControlPropertyValueSource::Inherited, nullptr, true);
	}
}

void Control::RefreshInheritedPropertiesRecursive()
{
	const bool previous = _refreshingInheritedProperties;
	_refreshingInheritedProperties = true;
	RefreshInheritedPropertyValues();
	for (auto* child : Children)
		if (child) child->RefreshInheritedPropertiesRecursive();
	_refreshingInheritedProperties = previous;
}

void Control::ApplyPropertyMetadataChange(
	const BindingPropertyMetadata& metadata,
	const BindingValue& oldValue,
	const BindingValue& newValue)
{
	++_propertyChangeVersion;
	metadata.NotifyChanged(*this, oldValue, newValue);
	const auto flags = metadata.Flags();
	if (HasControlPropertyFlag(flags, ControlPropertyFlags::AffectsMeasure))
		RequestLayout();
	else if (HasControlPropertyFlag(flags, ControlPropertyFlags::AffectsArrange))
		RequestArrange();
	if (HasControlPropertyFlag(flags, ControlPropertyFlags::AffectsRender))
		InvalidateVisual();
	if (Parent && HasControlPropertyFlag(
		flags, ControlPropertyFlags::AffectsParentMeasure))
		Parent->RequestLayout();
	else if (Parent && HasControlPropertyFlag(
		flags, ControlPropertyFlags::AffectsParentArrange))
		Parent->RequestArrange();
	if (!_refreshingInheritedProperties
		&& HasControlPropertyFlag(flags, ControlPropertyFlags::Inherits))
	{
		for (auto* child : Children)
			if (child) child->RefreshInheritedPropertiesRecursive();
	}

	ControlPropertyChangedEventArgs args{
		metadata.Name(), oldValue, newValue };
	OnPropertyValueChanged(this, args);
	_bindingSourcePropertyChanged.Notify(metadata.Name());
}

bool Control::TryGetValue(
	const std::wstring& propertyName,
	BindingValue& out) const
{
	return const_cast<Control*>(this)->TryGetPropertyValue(propertyName, out);
}

bool Control::DefineDynamicEvent(
	DynamicControlEventDefinition definition,
	std::wstring* outError)
{
	if (definition.Name.empty())
	{
		if (outError) *outError = L"动态事件名称不能为空。";
		return false;
	}
	if (definition.PayloadKind == BindingValueKind::Object)
	{
		if (outError) *outError = L"动态事件暂不支持 Object payload。";
		return false;
	}
	if (definition.OwnerNamespace.empty()
		!= definition.OwnerTypeName.empty())
	{
		if (outError) *outError = L"动态事件所有者必须同时包含命名空间和类型名。";
		return false;
	}
	auto name = definition.Name;
	if (!_dynamicEvents.emplace(std::move(name), std::move(definition)).second)
	{
		if (outError) *outError = L"动态事件名称重复。";
		return false;
	}
	if (outError) outError->clear();
	return true;
}

const DynamicControlEventDefinition* Control::FindDynamicEvent(
	const std::wstring& eventName) const noexcept
{
	const auto found = _dynamicEvents.find(eventName);
	return found == _dynamicEvents.end() ? nullptr : &found->second;
}

bool Control::RaiseDeclarativeEvent(
	std::wstring eventName,
	BindingValue value)
{
	DeclarativeEventArgs args;
	args.Name = std::move(eventName);
	args.Value = std::move(value);
	return RaiseDeclarativeEvent(args);
}

bool Control::RaiseDeclarativeEvent(DeclarativeEventArgs& args)
{
	const auto* definition = FindDynamicEvent(args.Name);
	if (!definition || definition->PayloadKind != args.Value.Kind()) return false;
	args.OwnerNamespace = definition->OwnerNamespace;
	args.OwnerTypeName = definition->OwnerTypeName;
	args.RoutingStrategy = definition->RoutingStrategy;
	args.OriginalSource = this;
	args.Source = this;

	std::vector<Control*> route;
	route.push_back(this);
	if (definition->RoutingStrategy
		!= DeclarativeEventRoutingStrategy::Direct)
	{
		for (auto* current = Parent; current; current = current->Parent)
			route.push_back(current);
		if (definition->RoutingStrategy
			== DeclarativeEventRoutingStrategy::Tunnel)
			std::reverse(route.begin(), route.end());
	}
	for (auto* current : route)
	{
		if (!current) continue;
		args.CurrentTarget = current;
		current->OnDeclarativeEvent(current, args);
	}
	args.CurrentTarget = nullptr;
	return true;
}

struct Control::DeclarativeVisualStateRuntime
{
	struct RuntimeCondition
	{
		const BindingPropertyMetadata* Metadata = nullptr;
		BindingValue Value;
	};

	struct RuntimeSetter
	{
		Control* Target = nullptr;
		std::wstring PropertyName;
		BindingValue Value;
	};

	enum class TransformMember : unsigned char
	{
		X,
		Y,
		ScaleX,
		ScaleY,
		Angle,
		AngleX,
		AngleY,
		CenterX,
		CenterY,
		Matrix,
	};

	struct TransformAccessor
	{
		size_t OperationIndex = 0;
		cui::drawing::TransformKind OperationKind =
			cui::drawing::TransformKind::Translate;
		TransformMember Member = TransformMember::X;
		std::wstring CanonicalPath;
	};

	enum class GeometryMember : unsigned char
	{
		Rect,
		Center,
		RadiusX,
		RadiusY,
		FillRule,
	};

	struct GeometryAccessor
	{
		std::vector<size_t> ChildIndices;
		cui::drawing::GeometryKind GeometryKind =
			cui::drawing::GeometryKind::Rectangle;
		GeometryMember Member = GeometryMember::Rect;
		std::wstring CanonicalPath;
	};

	enum class PathGeometryMember : unsigned char
	{
		FigureStartPoint,
		FigureIsClosed,
		FigureIsFilled,
		SegmentPoint,
		SegmentPoint1,
		SegmentPoint2,
		SegmentPoint3,
		ArcSize,
		ArcRotationAngle,
		ArcIsLargeArc,
		ArcSweepDirection,
	};

	struct PathGeometryAccessor
	{
		std::vector<size_t> ChildIndices;
		size_t FigureIndex = 0;
		size_t SegmentIndex = 0;
		bool HasSegment = false;
		cui::drawing::PathSegmentKind SegmentKind =
			cui::drawing::PathSegmentKind::Line;
		PathGeometryMember Member = PathGeometryMember::FigureStartPoint;
		std::wstring CanonicalPath;
	};

	struct GeometryTransformAccessor
	{
		std::vector<size_t> ChildIndices;
		cui::drawing::GeometryKind GeometryKind =
			cui::drawing::GeometryKind::Rectangle;
		TransformAccessor Transform;
		std::wstring CanonicalPath;
	};

	enum class BrushMember : unsigned char
	{
		SolidColor,
		Opacity,
		StartPoint,
		EndPoint,
		Center,
		GradientOrigin,
		RadiusX,
		RadiusY,
		GradientStopColor,
		GradientStopOffset,
	};

	struct BrushAccessor
	{
		size_t StopIndex = 0;
		cui::drawing::BrushKind BrushKind =
			cui::drawing::BrushKind::LinearGradient;
		BrushMember Member = BrushMember::GradientStopColor;
		std::wstring CanonicalPath;
	};

	struct BrushTransformAccessor
	{
		cui::drawing::BrushKind BrushKind =
			cui::drawing::BrushKind::LinearGradient;
		bool Relative = false;
		TransformAccessor Transform;
		std::wstring CanonicalPath;
	};

	/**
	 * Identifies an adapter from a Storyboard object-property path to the
	 * animatable value at its leaf. New object graphs extend this single
	 * boundary instead of adding parallel path fields throughout the runtime.
	 */
	using ObjectPathAccessor = std::variant<
		TransformAccessor, GeometryAccessor, PathGeometryAccessor,
		GeometryTransformAccessor, BrushAccessor, BrushTransformAccessor>;

	struct RuntimeAnimation
	{
		DeclarativeAnimationKind Kind = DeclarativeAnimationKind::Double;
		Control* Target = nullptr;
		const BindingPropertyMetadata* Metadata = nullptr;
		std::wstring PropertyName;
		std::optional<ObjectPathAccessor> ObjectPath;
		std::optional<BindingValue> From;
		std::optional<BindingValue> To;
		std::optional<BindingValue> By;
		bool IsAdditive = false;
		bool IsCumulative = false;
		std::vector<DeclarativeAnimationKeyFrame> KeyFrames;
		unsigned long long BeginTimeMilliseconds = 0;
		unsigned long long DurationMilliseconds = 0;
		DeclarativeRepeatBehaviorKind RepeatBehavior =
			DeclarativeRepeatBehaviorKind::Count;
		double RepeatCount = 1.0;
		unsigned long long RepeatDurationMilliseconds = 0;
		bool AutoReverse = false;
		DeclarativeTimelineFillBehavior FillBehavior =
			DeclarativeTimelineFillBehavior::HoldEnd;
		double SpeedRatio = 1.0;
		double AccelerationRatio = 0.0;
		double DecelerationRatio = 0.0;
		/** Transitions share the VisualState layer with the source state. */
		bool RestoreBaseOnStop = false;
		DeclarativeEasingKind Easing = DeclarativeEasingKind::Linear;
		DeclarativeEasingMode EasingMode = DeclarativeEasingMode::EaseOut;
	};

	struct RuntimeState
	{
		std::wstring Name;
		std::vector<RuntimeCondition> Conditions;
		std::vector<std::wstring> EventNames;
		std::vector<RuntimeSetter> Setters;
		std::vector<RuntimeAnimation> Animations;
	};

	struct RuntimeTransition
	{
		std::optional<size_t> FromState;
		std::optional<size_t> ToState;
		unsigned long long GeneratedDurationMilliseconds = 0;
		DeclarativeEasingKind GeneratedEasing = DeclarativeEasingKind::Linear;
		DeclarativeEasingMode GeneratedEasingMode =
			DeclarativeEasingMode::EaseOut;
		std::vector<RuntimeAnimation> Animations;
	};

	struct RuntimeEventStoryboard
	{
		std::wstring Name;
		std::vector<RuntimeAnimation> Animations;
		bool IsStyleStoryboard = false;
	};

	struct RuntimeEventTriggerAction
	{
		DeclarativeStoryboardActionKind Kind =
			DeclarativeStoryboardActionKind::Begin;
		size_t StoryboardIndex = 0;
		std::wstring PendingStoryboardName;
	};

	struct RuntimeEventTrigger
	{
		std::wstring EventName;
		std::vector<RuntimeEventTriggerAction> Actions;
	};

	struct RuntimeStyleTriggerScope
	{
		ControlPropertyValueSource Source = ControlPropertyValueSource::Style;
		const ControlStyleSheet* Sheet = nullptr;
		size_t RuleId = 0;
		bool Active = false;
		std::vector<size_t> StoryboardIndices;
		std::vector<RuntimeEventTriggerAction> EnterActions;
		std::vector<RuntimeEventTriggerAction> ExitActions;
	};

	struct PropertyKey
	{
		Control* Target = nullptr;
		std::wstring PropertyName;
	};

	struct PendingTransition
	{
		size_t TargetState = 0;
		unsigned long long EndTick = 0;
		std::vector<PropertyKey> Properties;
	};

	struct RuntimeGroup
	{
		std::wstring Name;
		std::vector<RuntimeState> States;
		std::vector<RuntimeTransition> Transitions;
		size_t FallbackState = 0;
		std::optional<size_t> CurrentState;
		std::optional<PendingTransition> Pending;
		std::vector<std::wstring> ConditionProperties;
	};

	struct PropertySnapshot
	{
		PropertyKey Key;
		std::optional<BindingValue> Value;
		ControlPropertyValueSource Source =
			ControlPropertyValueSource::VisualState;
	};

	struct ActiveAnimation
	{
		size_t GroupIndex = 0;
		Control* Target = nullptr;
		const BindingPropertyMetadata* Metadata = nullptr;
		std::wstring PropertyName;
		DeclarativeAnimationKind Kind = DeclarativeAnimationKind::Double;
		BindingValue Base;
		BindingValue Foundation;
		BindingValue From;
		BindingValue To;
		std::vector<DeclarativeAnimationKeyFrame> KeyFrames;
		bool IsCumulative = false;
		std::optional<ObjectPathAccessor> ObjectPath;
		unsigned long long StartTick = 0;
		unsigned long long BeginTimeMilliseconds = 0;
		unsigned long long DurationMilliseconds = 0;
		DeclarativeRepeatBehaviorKind RepeatBehavior =
			DeclarativeRepeatBehaviorKind::Count;
		double RepeatCount = 1.0;
		unsigned long long RepeatDurationMilliseconds = 0;
		bool AutoReverse = false;
		DeclarativeTimelineFillBehavior FillBehavior =
			DeclarativeTimelineFillBehavior::HoldEnd;
		double SpeedRatio = 1.0;
		double AccelerationRatio = 0.0;
		double DecelerationRatio = 0.0;
		bool RestoreBaseOnStop = false;
		DeclarativeEasingKind Easing = DeclarativeEasingKind::Linear;
		DeclarativeEasingMode EasingMode = DeclarativeEasingMode::EaseOut;
		bool IsEventStoryboard = false;
		bool Paused = false;
		unsigned long long PauseTick = 0;
		bool Completed = false;
	};

	static const TransformAccessor* AsTransformPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<TransformAccessor>(&*path) : nullptr;
	}

	static const GeometryAccessor* AsGeometryPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<GeometryAccessor>(&*path) : nullptr;
	}

	static const GeometryTransformAccessor* AsGeometryTransformPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<GeometryTransformAccessor>(&*path) : nullptr;
	}

	static const PathGeometryAccessor* AsPathGeometryPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<PathGeometryAccessor>(&*path) : nullptr;
	}

	static const BrushAccessor* AsBrushPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<BrushAccessor>(&*path) : nullptr;
	}

	static const BrushTransformAccessor* AsBrushTransformPath(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		return path ? std::get_if<BrushTransformAccessor>(&*path) : nullptr;
	}

	static std::wstring_view ObjectPathCanonical(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		if (const auto* transform = AsTransformPath(path))
			return transform->CanonicalPath;
		if (const auto* geometry = AsGeometryPath(path))
			return geometry->CanonicalPath;
		if (const auto* pathGeometry = AsPathGeometryPath(path))
			return pathGeometry->CanonicalPath;
		if (const auto* geometryTransform = AsGeometryTransformPath(path))
			return geometryTransform->CanonicalPath;
		if (const auto* brush = AsBrushPath(path))
			return brush->CanonicalPath;
		if (const auto* brushTransform = AsBrushTransformPath(path))
			return brushTransform->CanonicalPath;
		return {};
	}

	static bool ObjectPathUsesFloat(
		const std::optional<ObjectPathAccessor>& path) noexcept
	{
		if (const auto* transform = AsTransformPath(path))
			return transform->Member != TransformMember::Matrix;
		if (const auto* transform = AsGeometryTransformPath(path))
			return transform->Transform.Member != TransformMember::Matrix;
		if (const auto* transform = AsBrushTransformPath(path))
			return transform->Transform.Member != TransformMember::Matrix;
		if (const auto* pathGeometry = AsPathGeometryPath(path))
			return pathGeometry->Member == PathGeometryMember::ArcRotationAngle;
		if (const auto* geometry = AsGeometryPath(path))
			return geometry->Member == GeometryMember::RadiusX
				|| geometry->Member == GeometryMember::RadiusY;
		const auto* brush = AsBrushPath(path);
		return brush && (brush->Member == BrushMember::Opacity
			|| brush->Member == BrushMember::RadiusX
			|| brush->Member == BrushMember::RadiusY
			|| brush->Member == BrushMember::GradientStopOffset);
	}

	Control* Owner = nullptr;
	std::vector<RuntimeGroup> Groups;
	std::vector<EventConnection> Connections;
	std::vector<ActiveAnimation> ActiveAnimations;
	std::vector<RuntimeEventStoryboard> EventStoryboards;
	std::vector<RuntimeEventTrigger> EventTriggers;
	std::vector<RuntimeStyleTriggerScope> StyleTriggerScopes;
	std::vector<size_t> FreeStyleStoryboardIndices;
	bool DeclarativeInteractionsDefined = false;
	bool Applying = false;

	~DeclarativeVisualStateRuntime()
	{
		Connections.clear();
		ActiveAnimations.clear();
		ClearAppliedValues();
	}

	static bool EqualName(
		std::wstring_view left,
		std::wstring_view right) noexcept
	{
		return left.size() == right.size()
			&& std::equal(left.begin(), left.end(), right.begin(),
				[](wchar_t l, wchar_t r)
				{ return std::towlower(l) == std::towlower(r); });
	}

	static bool SameProperty(
		const PropertyKey& left,
		const PropertyKey& right) noexcept
	{
		return left.Target == right.Target
			&& EqualName(left.PropertyName, right.PropertyName);
	}

	static bool ContainsName(
		const std::vector<std::wstring>& values,
		const std::wstring& value)
	{
		return std::any_of(values.begin(), values.end(),
			[&](const auto& existing) { return EqualName(existing, value); });
	}

	static bool IsNumericKind(BindingValueKind kind) noexcept
	{
		return kind == BindingValueKind::Int
			|| kind == BindingValueKind::Int64
			|| kind == BindingValueKind::Float
			|| kind == BindingValueKind::Double;
	}

	static bool AnimationMatchesMetadata(
		DeclarativeAnimationKind kind,
		const BindingPropertyMetadata& metadata) noexcept
	{
		if (kind == DeclarativeAnimationKind::Object) return true;
		if (kind == DeclarativeAnimationKind::Double)
			return IsNumericKind(metadata.ValueKind());
		if (kind == DeclarativeAnimationKind::Thickness)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType() == std::type_index(typeid(Thickness));
		if (kind == DeclarativeAnimationKind::Point)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType() == std::type_index(typeid(cui::core::Point));
		if (kind == DeclarativeAnimationKind::Vector)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType() == std::type_index(typeid(cui::core::Vector));
		if (kind == DeclarativeAnimationKind::Rect)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType() == std::type_index(typeid(cui::core::Rect));
		if (kind == DeclarativeAnimationKind::Size)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType() == std::type_index(typeid(cui::core::Size));
		if (kind == DeclarativeAnimationKind::Matrix)
			return metadata.ValueKind() == BindingValueKind::Object
				&& metadata.ValueType()
					== std::type_index(typeid(D2D1_MATRIX_3X2_F));
		return metadata.ValueKind() == BindingValueKind::Object
			&& metadata.ValueType() == std::type_index(typeid(D2D1_COLOR_F));
	}

	static std::wstring_view LocalTypeName(std::wstring_view value) noexcept
	{
		const auto separator = value.rfind(L':');
		return separator == std::wstring_view::npos
			? value : value.substr(separator + 1);
	}

	static bool TryResolveTransformOperationAccessor(
		const cui::drawing::Transform& transform,
		size_t operationIndex,
		std::wstring_view owner,
		std::wstring_view property,
		std::wstring canonicalPrefix,
		TransformAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		if (operationIndex >= transform.Operations.size())
			return fail(L"动画目标没有路径所需的 Transform 操作。");
		const auto& operation = transform.Operations[operationIndex];
		auto assign = [&](TransformMember member,
			std::wstring_view canonicalOwner,
			std::wstring_view canonicalProperty)
		{
			output.OperationIndex = operationIndex;
			output.OperationKind = operation.Kind;
			output.Member = member;
			output.CanonicalPath = std::move(canonicalPrefix) + L"["
				+ std::to_wstring(operationIndex) + L"].("
				+ std::wstring(canonicalOwner) + L"."
				+ std::wstring(canonicalProperty) + L")";
			if (outError) outError->clear();
			return true;
		};
		switch (operation.Kind)
		{
		case cui::drawing::TransformKind::Translate:
			if (!EqualName(owner, L"TranslateTransform")) break;
			if (EqualName(property, L"X")) return assign(
				TransformMember::X, L"TranslateTransform", L"X");
			if (EqualName(property, L"Y")) return assign(
				TransformMember::Y, L"TranslateTransform", L"Y");
			break;
		case cui::drawing::TransformKind::Scale:
			if (!EqualName(owner, L"ScaleTransform")) break;
			if (EqualName(property, L"ScaleX")) return assign(
				TransformMember::ScaleX, L"ScaleTransform", L"ScaleX");
			if (EqualName(property, L"ScaleY")) return assign(
				TransformMember::ScaleY, L"ScaleTransform", L"ScaleY");
			if (EqualName(property, L"CenterX")) return assign(
				TransformMember::CenterX, L"ScaleTransform", L"CenterX");
			if (EqualName(property, L"CenterY")) return assign(
				TransformMember::CenterY, L"ScaleTransform", L"CenterY");
			break;
		case cui::drawing::TransformKind::Rotate:
			if (!EqualName(owner, L"RotateTransform")) break;
			if (EqualName(property, L"Angle")) return assign(
				TransformMember::Angle, L"RotateTransform", L"Angle");
			if (EqualName(property, L"CenterX")) return assign(
				TransformMember::CenterX, L"RotateTransform", L"CenterX");
			if (EqualName(property, L"CenterY")) return assign(
				TransformMember::CenterY, L"RotateTransform", L"CenterY");
			break;
		case cui::drawing::TransformKind::Skew:
			if (!EqualName(owner, L"SkewTransform")) break;
			if (EqualName(property, L"AngleX")) return assign(
				TransformMember::AngleX, L"SkewTransform", L"AngleX");
			if (EqualName(property, L"AngleY")) return assign(
				TransformMember::AngleY, L"SkewTransform", L"AngleY");
			if (EqualName(property, L"CenterX")) return assign(
				TransformMember::CenterX, L"SkewTransform", L"CenterX");
			if (EqualName(property, L"CenterY")) return assign(
				TransformMember::CenterY, L"SkewTransform", L"CenterY");
			break;
		case cui::drawing::TransformKind::Matrix:
			if (EqualName(owner, L"MatrixTransform")
				&& EqualName(property, L"Matrix"))
				return assign(TransformMember::Matrix,
					L"MatrixTransform", L"Matrix");
			break;
		default:
			break;
		}
		return fail(L"动画路径的 Transform 类型或末端属性与实际操作不匹配"
			L"（operationKind="
			+ std::to_wstring(static_cast<int>(operation.Kind))
			+ L"，owner=" + std::wstring(owner)
			+ L"，property=" + std::wstring(property) + L"）。");
	}

	static bool TryResolveTransformPath(
		Control& target,
		const std::wstring& text,
		const BindingPropertyMetadata*& outMetadata,
		TransformAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		if (path.Segments.size() != 4
			|| path.Segments[0].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[1].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[2].Kind
				!= cui::xaml::PropertyPathSegmentKind::Index
			|| path.Segments[3].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| !EqualName(path.Segments[0].Name, L"RenderTransform")
			|| (!EqualName(LocalTypeName(path.Segments[0].OwnerType), L"Control")
				&& !EqualName(LocalTypeName(path.Segments[0].OwnerType), L"UIElement"))
			|| !EqualName(LocalTypeName(path.Segments[1].OwnerType), L"TransformGroup")
			|| !EqualName(path.Segments[1].Name, L"Children"))
			return fail(L"首批复合动画路径必须是 "
				L"(Control.RenderTransform).(TransformGroup.Children)[n]."
				L"(TransformType.Property)。");

		const auto* metadata = target.FindPropertyMetadata(L"RenderTransform");
		BindingValue current;
		cui::drawing::Transform transform;
		if (!metadata || !metadata->CanWrite()
			|| metadata->ValueType()
				!= std::type_index(typeid(cui::drawing::Transform))
			|| !metadata->TryGet(target, current)
			|| !current.TryGet(transform)
			|| path.Segments[2].Index >= transform.Operations.size())
			return fail(L"动画目标没有路径所需的 RenderTransform 操作。");

		const auto owner = LocalTypeName(path.Segments[3].OwnerType);
		const auto& property = path.Segments[3].Name;
		if (!TryResolveTransformOperationAccessor(transform,
			path.Segments[2].Index, owner, property,
			L"(Control.RenderTransform).(TransformGroup.Children)",
			output, outError)) return false;
		outMetadata = metadata;
		return true;
	}

	static const cui::drawing::Geometry* TryGetGeometryChild(
		const cui::drawing::Geometry& root,
		const std::vector<size_t>& childIndices) noexcept
	{
		const auto* current = &root;
		for (const auto index : childIndices)
		{
			if (current->Kind != cui::drawing::GeometryKind::Group
				|| index >= current->Children.size()) return nullptr;
			current = &current->Children[index];
		}
		return current;
	}

	static cui::drawing::Geometry* TryGetGeometryChild(
		cui::drawing::Geometry& root,
		const std::vector<size_t>& childIndices) noexcept
	{
		auto* current = &root;
		for (const auto index : childIndices)
		{
			if (current->Kind != cui::drawing::GeometryKind::Group
				|| index >= current->Children.size()) return nullptr;
			current = &current->Children[index];
		}
		return current;
	}

	/**
	 * Resolves zero or more GeometryGroup.Children[index] hops after
	 * Control.Clip and returns the concrete Geometry that owns the leaf.
	 */
	static bool TryResolveGeometryTreeTarget(
		Control& target,
		const cui::xaml::PropertyPath& path,
		const BindingPropertyMetadata*& outMetadata,
		const cui::drawing::Geometry*& outGeometry,
		std::vector<size_t>& outChildIndices,
		size_t& outLeafStart,
		std::wstring& outCanonicalPrefix,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		if (path.Segments.empty()
			|| path.Segments[0].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| !EqualName(path.Segments[0].Name, L"Clip")
			|| (!EqualName(LocalTypeName(path.Segments[0].OwnerType), L"Control")
				&& !EqualName(LocalTypeName(path.Segments[0].OwnerType), L"UIElement")))
			return fail(L"Geometry 复合动画路径必须以 (Control.Clip) 开始。");

		const auto* metadata = target.FindPropertyMetadata(L"Clip");
		const auto& currentClip = target.GetClip();
		if (!metadata || !metadata->CanWrite()
			|| metadata->ValueType()
				!= std::type_index(typeid(cui::drawing::Geometry))
			|| !currentClip)
			return fail(L"动画目标必须显式持有 Control.Clip Geometry。");

		outChildIndices.clear();
		outCanonicalPrefix = L"(Control.Clip)";
		const auto* geometry = &*currentClip;
		size_t cursor = 1;
		while (cursor < path.Segments.size()
			&& path.Segments[cursor].Kind
				== cui::xaml::PropertyPathSegmentKind::Property
			&& EqualName(
				LocalTypeName(path.Segments[cursor].OwnerType), L"GeometryGroup")
			&& EqualName(path.Segments[cursor].Name, L"Children"))
		{
			if (cursor + 1 >= path.Segments.size()
				|| path.Segments[cursor + 1].Kind
					!= cui::xaml::PropertyPathSegmentKind::Index)
				return fail(L"GeometryGroup.Children 后必须提供有效索引。");
			const auto index = path.Segments[cursor + 1].Index;
			if (geometry->Kind != cui::drawing::GeometryKind::Group)
				return fail(L"GeometryGroup.Children 路径所有者与实际 Geometry 类型不匹配。");
			if (index >= geometry->Children.size())
				return fail(L"GeometryGroup.Children 动画索引超出范围。");
			outChildIndices.push_back(index);
			geometry = &geometry->Children[index];
			outCanonicalPrefix += L".(GeometryGroup.Children)["
				+ std::to_wstring(index) + L"]";
			cursor += 2;
		}
		if (cursor >= path.Segments.size())
			return fail(L"GeometryGroup.Children 索引后缺少动画末端属性。");
		outMetadata = metadata;
		outGeometry = geometry;
		outLeafStart = cursor;
		return true;
	}

	static bool TryResolveGeometryPath(
		Control& target,
		const std::wstring& text,
		const BindingPropertyMetadata*& outMetadata,
		GeometryAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const BindingPropertyMetadata* metadata = nullptr;
		const cui::drawing::Geometry* resolvedGeometry = nullptr;
		std::vector<size_t> childIndices;
		size_t leafStart = 0;
		std::wstring canonicalPrefix;
		if (!TryResolveGeometryTreeTarget(target, path, metadata,
			resolvedGeometry, childIndices, leafStart, canonicalPrefix, outError))
			return false;
		if (path.Segments.size() != leafStart + 1
			|| path.Segments[leafStart].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property)
			return fail(L"Geometry 动画路径必须在目标 Geometry 后定位一个公开成员。");
		const auto owner = LocalTypeName(path.Segments[leafStart].OwnerType);
		const auto& property = path.Segments[leafStart].Name;
		const auto& geometry = *resolvedGeometry;
		output = {};
		output.ChildIndices = std::move(childIndices);
		output.GeometryKind = geometry.Kind;
		auto assign = [&](GeometryMember member, std::wstring_view type,
			std::wstring_view canonicalProperty)
		{
			output.Member = member;
			output.CanonicalPath = canonicalPrefix + L".(" + std::wstring(type)
				+ L"." + std::wstring(canonicalProperty) + L")";
			outMetadata = metadata;
			if (outError) outError->clear();
			return true;
		};
		auto validRadius = [](float value)
		{
			return std::isfinite(value) && value >= 0.0f;
		};
		if (geometry.Kind == cui::drawing::GeometryKind::Rectangle
			&& EqualName(owner, L"RectangleGeometry"))
		{
			if (EqualName(property, L"Rect"))
			{
				const auto rect = ToCoreRect(geometry.Rect);
				if (!std::isfinite(rect.x) || !std::isfinite(rect.y)
					|| !std::isfinite(rect.width) || !std::isfinite(rect.height)
					|| rect.width < 0.0f || rect.height < 0.0f)
					return fail(L"动画目标的 RectangleGeometry.Rect 无效。");
				return assign(GeometryMember::Rect,
					L"RectangleGeometry", L"Rect");
			}
			if (EqualName(property, L"RadiusX")
				|| EqualName(property, L"RadiusY"))
			{
				const bool x = EqualName(property, L"RadiusX");
				if (!validRadius(x ? geometry.RadiusX : geometry.RadiusY))
					return fail(L"动画目标的 RectangleGeometry 圆角半径无效。");
				return assign(x ? GeometryMember::RadiusX : GeometryMember::RadiusY,
					L"RectangleGeometry", x ? L"RadiusX" : L"RadiusY");
			}
		}
		if (geometry.Kind == cui::drawing::GeometryKind::Ellipse
			&& EqualName(owner, L"EllipseGeometry"))
		{
			if (EqualName(property, L"Center"))
			{
				if (!std::isfinite(geometry.Center.x)
					|| !std::isfinite(geometry.Center.y))
					return fail(L"动画目标的 EllipseGeometry.Center 无效。");
				return assign(GeometryMember::Center,
					L"EllipseGeometry", L"Center");
			}
			if (EqualName(property, L"RadiusX")
				|| EqualName(property, L"RadiusY"))
			{
				const bool x = EqualName(property, L"RadiusX");
				if (!validRadius(x ? geometry.RadiusX : geometry.RadiusY))
					return fail(L"动画目标的 EllipseGeometry 半径无效。");
				return assign(x ? GeometryMember::RadiusX : GeometryMember::RadiusY,
					L"EllipseGeometry", x ? L"RadiusX" : L"RadiusY");
			}
		}
		if ((geometry.Kind == cui::drawing::GeometryKind::Path
				&& EqualName(owner, L"PathGeometry")
				|| geometry.Kind == cui::drawing::GeometryKind::Group
					&& EqualName(owner, L"GeometryGroup"))
			&& EqualName(property, L"FillRule"))
			return assign(GeometryMember::FillRule, owner, L"FillRule");
		return fail(L"Geometry 动画路径所有者或末端属性与实际 Clip 类型不匹配。");
	}

	static bool TryResolvePathGeometryPath(
		Control& target,
		const std::wstring& text,
		const BindingPropertyMetadata*& outMetadata,
		PathGeometryAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const BindingPropertyMetadata* metadata = nullptr;
		const cui::drawing::Geometry* resolvedGeometry = nullptr;
		std::vector<size_t> childIndices;
		size_t leafStart = 0;
		std::wstring canonicalPrefix;
		if (!TryResolveGeometryTreeTarget(target, path, metadata,
			resolvedGeometry, childIndices, leafStart, canonicalPrefix, outError))
			return false;
		const auto remaining = path.Segments.size() - leafStart;
		const bool figurePath = remaining == 3;
		const bool segmentPath = remaining == 5;
		if ((!figurePath && !segmentPath)
			|| path.Segments[leafStart].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[leafStart + 1].Kind
				!= cui::xaml::PropertyPathSegmentKind::Index
			|| path.Segments[leafStart + 2].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| (segmentPath
				&& (path.Segments[leafStart + 3].Kind
						!= cui::xaml::PropertyPathSegmentKind::Index
					|| path.Segments[leafStart + 4].Kind
						!= cui::xaml::PropertyPathSegmentKind::Property))
			|| !EqualName(
				LocalTypeName(path.Segments[leafStart].OwnerType), L"PathGeometry")
			|| !EqualName(path.Segments[leafStart].Name, L"Figures")
			|| !EqualName(
				LocalTypeName(path.Segments[leafStart + 2].OwnerType), L"PathFigure")
			|| (segmentPath
				&& !EqualName(path.Segments[leafStart + 2].Name, L"Segments")))
			return fail(L"PathGeometry 动画路径必须定位 Figures[n] 的 PathFigure "
				L"成员，或继续定位 Segments[n] 的具体 PathSegment 成员。");
		if (resolvedGeometry->Kind != cui::drawing::GeometryKind::Path)
			return fail(L"动画目标必须显式持有 PathGeometry 类型的 Control.Clip。");
		const auto figureIndex = path.Segments[leafStart + 1].Index;
		if (figureIndex >= resolvedGeometry->Figures.size())
			return fail(L"PathGeometry.Figures 动画索引超出范围。");
		const auto& figure = resolvedGeometry->Figures[figureIndex];
		output = {};
		output.ChildIndices = std::move(childIndices);
		output.FigureIndex = figureIndex;
		auto finitePoint = [](D2D1_POINT_2F point)
		{
			return std::isfinite(point.x) && std::isfinite(point.y);
		};
		auto assignFigure = [&](PathGeometryMember member,
			std::wstring_view property)
		{
			output.Member = member;
			output.CanonicalPath = canonicalPrefix + L".(PathGeometry.Figures)["
				+ std::to_wstring(figureIndex) + L"].(PathFigure."
				+ std::wstring(property) + L")";
			outMetadata = metadata;
			if (outError) outError->clear();
			return true;
		};
		if (figurePath)
		{
			const auto& property = path.Segments[leafStart + 2].Name;
			if (EqualName(property, L"StartPoint"))
			{
				if (!finitePoint(figure.StartPoint))
					return fail(L"动画目标的 PathFigure.StartPoint 无效。");
				return assignFigure(
					PathGeometryMember::FigureStartPoint, L"StartPoint");
			}
			if (EqualName(property, L"IsClosed"))
				return assignFigure(
					PathGeometryMember::FigureIsClosed, L"IsClosed");
			if (EqualName(property, L"IsFilled"))
				return assignFigure(
					PathGeometryMember::FigureIsFilled, L"IsFilled");
			return fail(L"尚未支持该 PathFigure 动画成员。");
		}

		const auto segmentIndex = path.Segments[leafStart + 3].Index;
		if (segmentIndex >= figure.Segments.size())
			return fail(L"PathFigure.Segments 动画索引超出范围。");
		const auto& segment = figure.Segments[segmentIndex];
		const auto owner = LocalTypeName(path.Segments[leafStart + 4].OwnerType);
		const auto& property = path.Segments[leafStart + 4].Name;
		output.HasSegment = true;
		output.SegmentIndex = segmentIndex;
		output.SegmentKind = segment.Kind;
		auto assignSegment = [&](PathGeometryMember member,
			std::wstring_view objectType, std::wstring_view canonicalProperty)
		{
			output.Member = member;
			output.CanonicalPath = canonicalPrefix + L".(PathGeometry.Figures)["
				+ std::to_wstring(figureIndex)
				+ L"].(PathFigure.Segments)["
				+ std::to_wstring(segmentIndex) + L"].("
				+ std::wstring(objectType) + L"."
				+ std::wstring(canonicalProperty) + L")";
			outMetadata = metadata;
			if (outError) outError->clear();
			return true;
		};
		auto pointMember = [&](PathGeometryMember member,
			std::wstring_view type, std::wstring_view name,
			D2D1_POINT_2F point)
		{
			if (!finitePoint(point))
				return fail(L"动画目标的 PathSegment Point 无效。");
			return assignSegment(member, type, name);
		};
		switch (segment.Kind)
		{
		case cui::drawing::PathSegmentKind::Line:
			if (EqualName(owner, L"LineSegment")
				&& EqualName(property, L"Point"))
				return pointMember(PathGeometryMember::SegmentPoint,
					L"LineSegment", L"Point", segment.Point);
			break;
		case cui::drawing::PathSegmentKind::Bezier:
			if (EqualName(owner, L"BezierSegment"))
			{
				if (EqualName(property, L"Point1")) return pointMember(
					PathGeometryMember::SegmentPoint1,
					L"BezierSegment", L"Point1", segment.Point1);
				if (EqualName(property, L"Point2")) return pointMember(
					PathGeometryMember::SegmentPoint2,
					L"BezierSegment", L"Point2", segment.Point2);
				if (EqualName(property, L"Point3")) return pointMember(
					PathGeometryMember::SegmentPoint3,
					L"BezierSegment", L"Point3", segment.Point3);
			}
			break;
		case cui::drawing::PathSegmentKind::QuadraticBezier:
			if (EqualName(owner, L"QuadraticBezierSegment"))
			{
				if (EqualName(property, L"Point1")) return pointMember(
					PathGeometryMember::SegmentPoint1,
					L"QuadraticBezierSegment", L"Point1", segment.Point1);
				if (EqualName(property, L"Point2")) return pointMember(
					PathGeometryMember::SegmentPoint2,
					L"QuadraticBezierSegment", L"Point2", segment.Point2);
			}
			break;
		case cui::drawing::PathSegmentKind::Arc:
			if (EqualName(owner, L"ArcSegment"))
			{
				if (EqualName(property, L"Point")) return pointMember(
					PathGeometryMember::SegmentPoint,
					L"ArcSegment", L"Point", segment.Point);
				if (EqualName(property, L"Size"))
				{
					if (!std::isfinite(segment.Size.width)
						|| !std::isfinite(segment.Size.height)
						|| segment.Size.width < 0.0f
						|| segment.Size.height < 0.0f)
						return fail(L"动画目标的 ArcSegment.Size 无效。");
					return assignSegment(PathGeometryMember::ArcSize,
						L"ArcSegment", L"Size");
				}
				if (EqualName(property, L"RotationAngle"))
				{
					if (!std::isfinite(segment.RotationAngle))
						return fail(L"动画目标的 ArcSegment.RotationAngle 无效。");
					return assignSegment(PathGeometryMember::ArcRotationAngle,
						L"ArcSegment", L"RotationAngle");
				}
				if (EqualName(property, L"IsLargeArc"))
					return assignSegment(PathGeometryMember::ArcIsLargeArc,
						L"ArcSegment", L"IsLargeArc");
				if (EqualName(property, L"SweepDirection"))
					return assignSegment(PathGeometryMember::ArcSweepDirection,
						L"ArcSegment", L"SweepDirection");
			}
			break;
		default:
			break;
		}
		return fail(L"PathSegment 动画路径所有者或末端属性与实际段类型不匹配。");
	}

	static bool TryResolveGeometryTransformPath(
		Control& target,
		const std::wstring& text,
		const BindingPropertyMetadata*& outMetadata,
		GeometryTransformAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const BindingPropertyMetadata* metadata = nullptr;
		const cui::drawing::Geometry* resolvedGeometry = nullptr;
		std::vector<size_t> childIndices;
		size_t leafStart = 0;
		std::wstring canonicalPrefix;
		if (!TryResolveGeometryTreeTarget(target, path, metadata,
			resolvedGeometry, childIndices, leafStart, canonicalPrefix, outError))
			return false;
		const auto geometryOwner = LocalTypeName(
			path.Segments[leafStart].OwnerType);
		if (path.Segments.size() != leafStart + 4
			|| path.Segments[leafStart].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[leafStart + 1].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[leafStart + 2].Kind
				!= cui::xaml::PropertyPathSegmentKind::Index
			|| path.Segments[leafStart + 3].Kind
				!= cui::xaml::PropertyPathSegmentKind::Property
			|| (!EqualName(geometryOwner, L"Geometry")
				&& !EqualName(geometryOwner, L"RectangleGeometry")
				&& !EqualName(geometryOwner, L"EllipseGeometry")
				&& !EqualName(geometryOwner, L"PathGeometry")
				&& !EqualName(geometryOwner, L"GeometryGroup"))
			|| !EqualName(path.Segments[leafStart].Name, L"Transform")
			|| !EqualName(
				LocalTypeName(path.Segments[leafStart + 1].OwnerType), L"TransformGroup")
			|| !EqualName(path.Segments[leafStart + 1].Name, L"Children"))
			return fail(L"Geometry Transform 动画路径必须是 "
				L"(Control.Clip)...(Geometry.Transform)."
				L"(TransformGroup.Children)[n].(TransformType.Property)。");
		auto ownerMatches = [&]()
		{
			if (EqualName(geometryOwner, L"Geometry")) return true;
			switch (resolvedGeometry->Kind)
			{
			case cui::drawing::GeometryKind::Rectangle:
				return EqualName(geometryOwner, L"RectangleGeometry");
			case cui::drawing::GeometryKind::Ellipse:
				return EqualName(geometryOwner, L"EllipseGeometry");
			case cui::drawing::GeometryKind::Path:
				return EqualName(geometryOwner, L"PathGeometry");
			case cui::drawing::GeometryKind::Group:
				return EqualName(geometryOwner, L"GeometryGroup");
			default:
				return false;
			}
		};
		if (!ownerMatches())
			return fail(L"Geometry.Transform 路径所有者与实际 Geometry 类型不匹配。");
		if (!resolvedGeometry->LocalTransform)
			return fail(L"动画目标没有路径所需的 Geometry.Transform。");

		TransformAccessor transformAccessor;
		if (!TryResolveTransformOperationAccessor(
			*resolvedGeometry->LocalTransform, path.Segments[leafStart + 2].Index,
			LocalTypeName(path.Segments[leafStart + 3].OwnerType),
			path.Segments[leafStart + 3].Name,
			canonicalPrefix
				+ L".(Geometry.Transform).(TransformGroup.Children)",
			transformAccessor, outError)) return false;
		output.ChildIndices = std::move(childIndices);
		output.GeometryKind = resolvedGeometry->Kind;
		output.Transform = std::move(transformAccessor);
		output.CanonicalPath = output.Transform.CanonicalPath;
		outMetadata = metadata;
		if (outError) outError->clear();
		return true;
	}

	static bool TryResolveBrushPath(
		Control& target,
		const std::wstring& text,
		const BindingPropertyMetadata*& outMetadata,
		BrushAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const auto brushOwner = path.Segments.size() > 1
			? LocalTypeName(path.Segments[1].OwnerType) : std::wstring_view{};
		const auto* metadata = target.FindPropertyMetadata(L"Foreground");
		const auto& currentBrush = target.GetForegroundBrush();
		if (!metadata || !metadata->CanWrite()
			|| metadata->ValueType()
				!= std::type_index(typeid(cui::drawing::Brush))
			|| !currentBrush)
			return fail(L"动画目标必须显式持有 Control.Foreground 画刷。");
		if (path.Segments.size() < 2
			|| path.Segments[0].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[1].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| !EqualName(path.Segments[0].Name, L"Foreground")
			|| (!EqualName(LocalTypeName(path.Segments[0].OwnerType), L"Control")
				&& !EqualName(LocalTypeName(path.Segments[0].OwnerType), L"UIElement")))
			return fail(L"Brush 复合动画路径必须以 "
				L"(Control.Foreground).(BrushProperty) 开始。");

		auto ownerMatches = [&](std::wstring_view owner)
		{
			if (EqualName(owner, L"Brush")) return true;
			switch (currentBrush->Kind)
			{
			case cui::drawing::BrushKind::Solid:
				return EqualName(owner, L"SolidColorBrush");
			case cui::drawing::BrushKind::LinearGradient:
				return EqualName(owner, L"GradientBrush")
					|| EqualName(owner, L"LinearGradientBrush");
			case cui::drawing::BrushKind::RadialGradient:
				return EqualName(owner, L"GradientBrush")
					|| EqualName(owner, L"RadialGradientBrush");
			case cui::drawing::BrushKind::Image:
				return EqualName(owner, L"ImageBrush");
			default:
				return false;
			}
		};
		if (!ownerMatches(brushOwner))
			return fail(L"Brush 动画路径所有者与实际画刷类型不匹配。");

		output = {};
		output.BrushKind = currentBrush->Kind;
		if (path.Segments.size() == 2)
		{
			const auto& property = path.Segments[1].Name;
			auto assign = [&](BrushMember member, std::wstring_view owner,
				std::wstring_view canonicalProperty)
			{
				output.Member = member;
				output.CanonicalPath = L"(Control.Foreground).("
					+ std::wstring(owner) + L"."
					+ std::wstring(canonicalProperty) + L")";
				outMetadata = metadata;
				if (outError) outError->clear();
				return true;
			};
			if (EqualName(property, L"Opacity"))
			{
				if (!std::isfinite(currentBrush->Opacity)
					|| currentBrush->Opacity < 0.0f || currentBrush->Opacity > 1.0f)
					return fail(L"动画目标的 Brush.Opacity 无效。");
				return assign(BrushMember::Opacity, L"Brush", L"Opacity");
			}
			if (currentBrush->Kind == cui::drawing::BrushKind::Solid
				&& EqualName(brushOwner, L"SolidColorBrush")
				&& EqualName(property, L"Color"))
			{
				const auto& color = currentBrush->Color;
				if (!std::isfinite(color.r) || !std::isfinite(color.g)
					|| !std::isfinite(color.b) || !std::isfinite(color.a))
					return fail(L"动画目标的 SolidColorBrush.Color 无效。");
				return assign(BrushMember::SolidColor,
					L"SolidColorBrush", L"Color");
			}
			auto finitePoint = [](D2D1_POINT_2F point)
			{
				return std::isfinite(point.x) && std::isfinite(point.y);
			};
			if (currentBrush->Kind == cui::drawing::BrushKind::LinearGradient
				&& EqualName(brushOwner, L"LinearGradientBrush"))
			{
				if (EqualName(property, L"StartPoint"))
				{
					if (!finitePoint(currentBrush->StartPoint))
						return fail(L"动画目标的 LinearGradientBrush.StartPoint 无效。");
					return assign(BrushMember::StartPoint,
						L"LinearGradientBrush", L"StartPoint");
				}
				if (EqualName(property, L"EndPoint"))
				{
					if (!finitePoint(currentBrush->EndPoint))
						return fail(L"动画目标的 LinearGradientBrush.EndPoint 无效。");
					return assign(BrushMember::EndPoint,
						L"LinearGradientBrush", L"EndPoint");
				}
			}
			if (currentBrush->Kind == cui::drawing::BrushKind::RadialGradient
				&& EqualName(brushOwner, L"RadialGradientBrush"))
			{
				if (EqualName(property, L"Center"))
				{
					if (!finitePoint(currentBrush->Center))
						return fail(L"动画目标的 RadialGradientBrush.Center 无效。");
					return assign(BrushMember::Center,
						L"RadialGradientBrush", L"Center");
				}
				if (EqualName(property, L"GradientOrigin"))
				{
					if (!finitePoint(currentBrush->GradientOrigin))
						return fail(L"动画目标的 RadialGradientBrush.GradientOrigin 无效。");
					return assign(BrushMember::GradientOrigin,
						L"RadialGradientBrush", L"GradientOrigin");
				}
				if (EqualName(property, L"RadiusX")
					|| EqualName(property, L"RadiusY"))
				{
					const bool x = EqualName(property, L"RadiusX");
					const auto radius = x ? currentBrush->RadiusX : currentBrush->RadiusY;
					if (!std::isfinite(radius) || radius < 0.0f)
						return fail(L"动画目标的 RadialGradientBrush 半径无效。");
					return assign(x ? BrushMember::RadiusX : BrushMember::RadiusY,
						L"RadialGradientBrush", x ? L"RadiusX" : L"RadiusY");
				}
			}
			return fail(L"Brush 动画路径末端属性与实际画刷类型不匹配。");
		}

		if (path.Segments.size() != 4
			|| path.Segments[2].Kind != cui::xaml::PropertyPathSegmentKind::Index
			|| path.Segments[3].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| (!EqualName(brushOwner, L"GradientBrush")
				&& !EqualName(brushOwner, L"LinearGradientBrush")
				&& !EqualName(brushOwner, L"RadialGradientBrush"))
			|| !EqualName(path.Segments[1].Name, L"GradientStops")
			|| !EqualName(LocalTypeName(path.Segments[3].OwnerType), L"GradientStop")
			|| (!EqualName(path.Segments[3].Name, L"Color")
				&& !EqualName(path.Segments[3].Name, L"Offset"))
			|| (currentBrush->Kind != cui::drawing::BrushKind::LinearGradient
				&& currentBrush->Kind != cui::drawing::BrushKind::RadialGradient)
			|| path.Segments[2].Index >= currentBrush->GradientStops.size())
			return fail(L"GradientStop 复合动画路径必须是 "
				L"(Control.Foreground).(GradientBrush.GradientStops)[n]."
				L"(GradientStop.Color|Offset)。");
		const auto& stop = currentBrush->GradientStops[path.Segments[2].Index];
		if (!std::isfinite(stop.Offset) || stop.Offset < 0.0f || stop.Offset > 1.0f
			|| !std::isfinite(stop.Color.r) || !std::isfinite(stop.Color.g)
			|| !std::isfinite(stop.Color.b) || !std::isfinite(stop.Color.a))
			return fail(L"动画目标的 GradientStop 值无效。");

		output.StopIndex = path.Segments[2].Index;
		output.Member = EqualName(path.Segments[3].Name, L"Color")
			? BrushMember::GradientStopColor : BrushMember::GradientStopOffset;
		output.CanonicalPath = L"(Control.Foreground)."
			L"(GradientBrush.GradientStops)["
			+ std::to_wstring(output.StopIndex) + L"].(GradientStop."
			+ (output.Member == BrushMember::GradientStopColor
				? std::wstring(L"Color") : std::wstring(L"Offset")) + L")";
		outMetadata = metadata;
		if (outError) outError->clear();
		return true;
	}

	static bool TryResolveBrushTransformPath(
		Control& target,
		const std::wstring& text,
		const BindingPropertyMetadata*& outMetadata,
		BrushTransformAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError))
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const auto brushOwner = path.Segments.size() > 1
			? LocalTypeName(path.Segments[1].OwnerType) : std::wstring_view{};
		if (path.Segments.size() != 5
			|| path.Segments[0].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[1].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[2].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| path.Segments[3].Kind != cui::xaml::PropertyPathSegmentKind::Index
			|| path.Segments[4].Kind != cui::xaml::PropertyPathSegmentKind::Property
			|| !EqualName(path.Segments[0].Name, L"Foreground")
			|| (!EqualName(LocalTypeName(path.Segments[0].OwnerType), L"Control")
				&& !EqualName(LocalTypeName(path.Segments[0].OwnerType), L"UIElement"))
			|| (!EqualName(brushOwner, L"Brush")
				&& !EqualName(brushOwner, L"SolidColorBrush")
				&& !EqualName(brushOwner, L"GradientBrush")
				&& !EqualName(brushOwner, L"LinearGradientBrush")
				&& !EqualName(brushOwner, L"RadialGradientBrush")
				&& !EqualName(brushOwner, L"ImageBrush"))
			|| (!EqualName(path.Segments[1].Name, L"Transform")
				&& !EqualName(path.Segments[1].Name, L"RelativeTransform"))
			|| !EqualName(LocalTypeName(path.Segments[2].OwnerType), L"TransformGroup")
			|| !EqualName(path.Segments[2].Name, L"Children"))
			return fail(L"Brush Transform 动画路径必须是 "
				L"(Control.Foreground).(Brush.Transform|RelativeTransform)."
				L"(TransformGroup.Children)[n].(TransformType.Property)。");

		const auto* metadata = target.FindPropertyMetadata(L"Foreground");
		const auto& currentBrush = target.GetForegroundBrush();
		if (!metadata || !metadata->CanWrite()
			|| metadata->ValueType() != std::type_index(typeid(cui::drawing::Brush))
			|| !currentBrush)
			return fail(L"动画目标必须显式持有包含变换的 Control.Foreground。");
		auto ownerMatches = [&]()
		{
			if (EqualName(brushOwner, L"Brush")) return true;
			switch (currentBrush->Kind)
			{
			case cui::drawing::BrushKind::Solid:
				return EqualName(brushOwner, L"SolidColorBrush");
			case cui::drawing::BrushKind::LinearGradient:
				return EqualName(brushOwner, L"GradientBrush")
					|| EqualName(brushOwner, L"LinearGradientBrush");
			case cui::drawing::BrushKind::RadialGradient:
				return EqualName(brushOwner, L"GradientBrush")
					|| EqualName(brushOwner, L"RadialGradientBrush");
			case cui::drawing::BrushKind::Image:
				return EqualName(brushOwner, L"ImageBrush");
			default:
				return false;
			}
		};
		if (!ownerMatches())
			return fail(L"Brush Transform 路径所有者与实际画刷类型不匹配。");
		const bool relative = EqualName(
			path.Segments[1].Name, L"RelativeTransform");
		const auto& transform = relative
			? currentBrush->RelativeTransform : currentBrush->Transform;
		if (!transform)
			return fail(L"动画目标没有路径所需的 Brush Transform。");

		TransformAccessor transformAccessor;
		const auto canonicalPrefix = L"(Control.Foreground).(Brush."
			+ std::wstring(relative ? L"RelativeTransform" : L"Transform")
			+ L").(TransformGroup.Children)";
		if (!TryResolveTransformOperationAccessor(
			*transform, path.Segments[3].Index,
			LocalTypeName(path.Segments[4].OwnerType), path.Segments[4].Name,
			canonicalPrefix, transformAccessor, outError)) return false;
		output.BrushKind = currentBrush->Kind;
		output.Relative = relative;
		output.Transform = std::move(transformAccessor);
		output.CanonicalPath = output.Transform.CanonicalPath;
		outMetadata = metadata;
		if (outError) outError->clear();
		return true;
	}

	static bool TryResolveObjectPath(
		Control& target,
		const std::wstring& text,
		DeclarativeAnimationKind animationKind,
		const BindingPropertyMetadata*& outMetadata,
		ObjectPathAccessor& output,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		cui::xaml::PropertyPath path;
		std::wstring parseError;
		if (!cui::xaml::TryParsePropertyPath(text, path, &parseError)
			|| path.Segments.empty()
			|| path.Segments.front().Kind
				!= cui::xaml::PropertyPathSegmentKind::Property)
			return fail(L"Storyboard.TargetProperty 路径无效：" + parseError);
		const auto& root = path.Segments.front().Name;
		if (EqualName(root, L"RenderTransform"))
		{
			TransformAccessor accessor;
			if (!TryResolveTransformPath(
				target, text, outMetadata, accessor, outError)) return false;
			const auto expected = accessor.Member == TransformMember::Matrix
				? DeclarativeAnimationKind::Matrix
				: DeclarativeAnimationKind::Double;
			if (animationKind != expected)
				return fail(L"RenderTransform 数值末端需要 DoubleAnimation，"
					L"MatrixTransform.Matrix 末端需要 MatrixAnimation。");
			output = std::move(accessor);
			return true;
		}
		if (EqualName(root, L"Clip"))
		{
			size_t geometryLeafStart = 1;
			while (geometryLeafStart + 1 < path.Segments.size()
				&& path.Segments[geometryLeafStart].Kind
					== cui::xaml::PropertyPathSegmentKind::Property
				&& EqualName(LocalTypeName(
					path.Segments[geometryLeafStart].OwnerType), L"GeometryGroup")
				&& EqualName(path.Segments[geometryLeafStart].Name, L"Children")
				&& path.Segments[geometryLeafStart + 1].Kind
					== cui::xaml::PropertyPathSegmentKind::Index)
				geometryLeafStart += 2;
			if (geometryLeafStart < path.Segments.size()
				&& EqualName(path.Segments[geometryLeafStart].Name, L"Figures"))
			{
				PathGeometryAccessor accessor;
				if (!TryResolvePathGeometryPath(
					target, text, outMetadata, accessor, outError)) return false;
				const bool pointMember = accessor.Member
					== PathGeometryMember::FigureStartPoint
					|| accessor.Member == PathGeometryMember::SegmentPoint
					|| accessor.Member == PathGeometryMember::SegmentPoint1
					|| accessor.Member == PathGeometryMember::SegmentPoint2
					|| accessor.Member == PathGeometryMember::SegmentPoint3;
				const bool sizeMember = accessor.Member
					== PathGeometryMember::ArcSize;
				const bool doubleMember = accessor.Member
					== PathGeometryMember::ArcRotationAngle;
				const bool objectMember = accessor.Member
					== PathGeometryMember::FigureIsClosed
					|| accessor.Member == PathGeometryMember::FigureIsFilled
					|| accessor.Member == PathGeometryMember::ArcIsLargeArc
					|| accessor.Member == PathGeometryMember::ArcSweepDirection;
				if ((pointMember && animationKind != DeclarativeAnimationKind::Point)
					|| (sizeMember && animationKind != DeclarativeAnimationKind::Size)
					|| (doubleMember && animationKind != DeclarativeAnimationKind::Double)
					|| (objectMember && animationKind != DeclarativeAnimationKind::Object))
					return fail(L"Path Point 末端需要 PointAnimation，Size 末端需要 "
						L"SizeAnimation，角度末端需要 DoubleAnimation，布尔/枚举末端"
						L"需要 ObjectAnimationUsingKeyFrames。");
				output = std::move(accessor);
				return true;
			}
			if (geometryLeafStart < path.Segments.size()
				&& EqualName(path.Segments[geometryLeafStart].Name, L"Transform"))
			{
				GeometryTransformAccessor accessor;
				if (!TryResolveGeometryTransformPath(
					target, text, outMetadata, accessor, outError)) return false;
				const auto expected = accessor.Transform.Member
					== TransformMember::Matrix
					? DeclarativeAnimationKind::Matrix
					: DeclarativeAnimationKind::Double;
				if (animationKind != expected)
					return fail(L"Geometry.Transform 数值末端需要 DoubleAnimation，"
						L"MatrixTransform.Matrix 末端需要 MatrixAnimation。");
				output = std::move(accessor);
				return true;
			}
			GeometryAccessor accessor;
			if (!TryResolveGeometryPath(
				target, text, outMetadata, accessor, outError)) return false;
			const bool rectMember = accessor.Member == GeometryMember::Rect;
			const bool pointMember = accessor.Member == GeometryMember::Center;
			const bool objectMember = accessor.Member == GeometryMember::FillRule;
			if ((rectMember && animationKind != DeclarativeAnimationKind::Rect)
				|| (pointMember && animationKind != DeclarativeAnimationKind::Point)
				|| (objectMember && animationKind != DeclarativeAnimationKind::Object)
				|| (!rectMember && !pointMember && !objectMember
					&& animationKind != DeclarativeAnimationKind::Double))
				return fail(L"Geometry Rect 末端需要 RectAnimation，Center 末端需要 "
					L"PointAnimation，半径末端需要 DoubleAnimation，FillRule 需要 "
					L"ObjectAnimationUsingKeyFrames。");
			output = std::move(accessor);
			return true;
		}
		if (EqualName(root, L"Foreground"))
		{
			if (path.Segments.size() > 1
				&& (EqualName(path.Segments[1].Name, L"Transform")
					|| EqualName(path.Segments[1].Name, L"RelativeTransform")))
			{
				BrushTransformAccessor accessor;
				if (!TryResolveBrushTransformPath(
					target, text, outMetadata, accessor, outError)) return false;
				const auto expected = accessor.Transform.Member
					== TransformMember::Matrix
					? DeclarativeAnimationKind::Matrix
					: DeclarativeAnimationKind::Double;
				if (animationKind != expected)
					return fail(L"Brush Transform 数值末端需要 DoubleAnimation，"
						L"MatrixTransform.Matrix 末端需要 MatrixAnimation。");
				output = std::move(accessor);
				return true;
			}
			BrushAccessor accessor;
			if (!TryResolveBrushPath(
				target, text, outMetadata, accessor, outError)) return false;
			const bool colorMember = accessor.Member == BrushMember::SolidColor
				|| accessor.Member == BrushMember::GradientStopColor;
			const bool pointMember = accessor.Member == BrushMember::StartPoint
				|| accessor.Member == BrushMember::EndPoint
				|| accessor.Member == BrushMember::Center
				|| accessor.Member == BrushMember::GradientOrigin;
			if ((colorMember && animationKind != DeclarativeAnimationKind::Color)
				|| (pointMember && animationKind != DeclarativeAnimationKind::Point)
				|| (!colorMember && !pointMember
					&& animationKind != DeclarativeAnimationKind::Double))
				return fail(L"Brush Color 末端需要 ColorAnimation，Point 末端需要 "
					L"PointAnimation，其余数值末端需要 DoubleAnimation。");
			output = std::move(accessor);
			return true;
		}
		return fail(L"尚未注册可处理此 Storyboard.TargetProperty 的对象路径适配器。");
	}

	static bool TryReadTransformMember(
		const cui::drawing::Transform& transform,
		const TransformAccessor& accessor,
		BindingValue& output) noexcept
	{
		if (accessor.OperationIndex >= transform.Operations.size()) return false;
		const auto& operation = transform.Operations[accessor.OperationIndex];
		if (operation.Kind != accessor.OperationKind) return false;
		float number = 0.0f;
		switch (accessor.Member)
		{
		case TransformMember::X: number = operation.X; break;
		case TransformMember::Y: number = operation.Y; break;
		case TransformMember::ScaleX: number = operation.ScaleX; break;
		case TransformMember::ScaleY: number = operation.ScaleY; break;
		case TransformMember::Angle: number = operation.Angle; break;
		case TransformMember::AngleX: number = operation.AngleX; break;
		case TransformMember::AngleY: number = operation.AngleY; break;
		case TransformMember::CenterX: number = operation.CenterX; break;
		case TransformMember::CenterY: number = operation.CenterY; break;
		case TransformMember::Matrix:
			if (!IsFiniteMatrix(operation.Matrix)) return false;
			output = BindingValue(operation.Matrix);
			return true;
		default: return false;
		}
		if (!std::isfinite(number)) return false;
		output = BindingValue(number);
		return true;
	}

	static bool TryWriteTransformMember(
		cui::drawing::Transform& transform,
		const TransformAccessor& accessor,
		const BindingValue& value) noexcept
	{
		if (accessor.OperationIndex >= transform.Operations.size()) return false;
		auto& operation = transform.Operations[accessor.OperationIndex];
		if (operation.Kind != accessor.OperationKind) return false;
		if (accessor.Member == TransformMember::Matrix)
		{
			D2D1_MATRIX_3X2_F matrix{};
			if (!value.TryGet(matrix) || !IsFiniteMatrix(matrix)) return false;
			operation.Matrix = matrix;
			return true;
		}
		double number = 0.0;
		if (!value.TryGetDouble(number) || !std::isfinite(number)
			|| number < -(std::numeric_limits<float>::max)()
			|| number > (std::numeric_limits<float>::max)()) return false;
		const auto result = static_cast<float>(number);
		switch (accessor.Member)
		{
		case TransformMember::X: operation.X = result; break;
		case TransformMember::Y: operation.Y = result; break;
		case TransformMember::ScaleX: operation.ScaleX = result; break;
		case TransformMember::ScaleY: operation.ScaleY = result; break;
		case TransformMember::Angle: operation.Angle = result; break;
		case TransformMember::AngleX: operation.AngleX = result; break;
		case TransformMember::AngleY: operation.AngleY = result; break;
		case TransformMember::CenterX: operation.CenterX = result; break;
		case TransformMember::CenterY: operation.CenterY = result; break;
		case TransformMember::Matrix: return false;
		default: return false;
		}
		return true;
	}

	static bool TryReadGeometryMember(
		const cui::drawing::Geometry& root,
		const GeometryAccessor& accessor,
		BindingValue& output) noexcept
	{
		const auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		const auto& geometry = *resolved;
		if (geometry.Kind != accessor.GeometryKind) return false;
		switch (accessor.Member)
		{
		case GeometryMember::Rect:
		{
			if (geometry.Kind != cui::drawing::GeometryKind::Rectangle) return false;
			const auto rect = ToCoreRect(geometry.Rect);
			if (!std::isfinite(rect.x) || !std::isfinite(rect.y)
				|| !std::isfinite(rect.width) || !std::isfinite(rect.height)
				|| rect.width < 0.0f || rect.height < 0.0f) return false;
			output = BindingValue(rect);
			return true;
		}
		case GeometryMember::Center:
			if (geometry.Kind != cui::drawing::GeometryKind::Ellipse
				|| !std::isfinite(geometry.Center.x)
				|| !std::isfinite(geometry.Center.y)) return false;
			output = BindingValue(cui::core::Point{
				geometry.Center.x, geometry.Center.y });
			return true;
		case GeometryMember::RadiusX:
		case GeometryMember::RadiusY:
		{
			if (geometry.Kind != cui::drawing::GeometryKind::Rectangle
				&& geometry.Kind != cui::drawing::GeometryKind::Ellipse) return false;
			const auto radius = accessor.Member == GeometryMember::RadiusX
				? geometry.RadiusX : geometry.RadiusY;
			if (!std::isfinite(radius) || radius < 0.0f) return false;
			output = BindingValue(radius);
			return true;
		}
		case GeometryMember::FillRule:
			if (geometry.Kind != cui::drawing::GeometryKind::Path
				&& geometry.Kind != cui::drawing::GeometryKind::Group) return false;
			output = BindingValue(std::wstring(geometry.FillRule
				== cui::drawing::GeometryFillRule::Nonzero
				? L"Nonzero" : L"EvenOdd"));
			return true;
		default:
			return false;
		}
	}

	static bool TryWriteGeometryMember(
		cui::drawing::Geometry& root,
		const GeometryAccessor& accessor,
		const BindingValue& value) noexcept
	{
		auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		auto& geometry = *resolved;
		if (geometry.Kind != accessor.GeometryKind) return false;
		switch (accessor.Member)
		{
		case GeometryMember::Rect:
		{
			cui::core::Rect rect{};
			if (geometry.Kind != cui::drawing::GeometryKind::Rectangle
				|| !value.TryGet(rect)
				|| !std::isfinite(rect.x) || !std::isfinite(rect.y)
				|| !std::isfinite(rect.width) || !std::isfinite(rect.height)
				|| rect.width < 0.0f || rect.height < 0.0f) return false;
			const auto converted = ToD2DRect(rect);
			if (!std::isfinite(converted.left) || !std::isfinite(converted.top)
				|| !std::isfinite(converted.right) || !std::isfinite(converted.bottom))
				return false;
			geometry.Rect = converted;
			return true;
		}
		case GeometryMember::Center:
		{
			cui::core::Point point{};
			if (geometry.Kind != cui::drawing::GeometryKind::Ellipse
				|| !value.TryGet(point) || !std::isfinite(point.x)
				|| !std::isfinite(point.y)) return false;
			geometry.Center = D2D1::Point2F(point.x, point.y);
			return true;
		}
		case GeometryMember::RadiusX:
		case GeometryMember::RadiusY:
		{
			double radius = 0.0;
			if ((geometry.Kind != cui::drawing::GeometryKind::Rectangle
				&& geometry.Kind != cui::drawing::GeometryKind::Ellipse)
				|| !value.TryGetDouble(radius) || !std::isfinite(radius)
				|| radius < -(std::numeric_limits<float>::max)()
				|| radius > (std::numeric_limits<float>::max)()) return false;
			const auto result = static_cast<float>((std::max)(radius, 0.0));
			if (accessor.Member == GeometryMember::RadiusX)
				geometry.RadiusX = result;
			else geometry.RadiusY = result;
			return true;
		}
		case GeometryMember::FillRule:
		{
			std::wstring converted;
			if ((geometry.Kind != cui::drawing::GeometryKind::Path
					&& geometry.Kind != cui::drawing::GeometryKind::Group)
				|| !value.TryGet(converted)) return false;
			if (EqualName(converted, L"Nonzero"))
				geometry.FillRule = cui::drawing::GeometryFillRule::Nonzero;
			else if (EqualName(converted, L"EvenOdd"))
				geometry.FillRule = cui::drawing::GeometryFillRule::EvenOdd;
			else return false;
			return true;
		}
		default:
			return false;
		}
	}

	static bool TryReadPathGeometryMember(
		const cui::drawing::Geometry& root,
		const PathGeometryAccessor& accessor,
		BindingValue& output) noexcept
	{
		const auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		const auto& geometry = *resolved;
		if (geometry.Kind != cui::drawing::GeometryKind::Path
			|| accessor.FigureIndex >= geometry.Figures.size()) return false;
		const auto& figure = geometry.Figures[accessor.FigureIndex];
		auto readPoint = [&](D2D1_POINT_2F point)
		{
			if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
			output = BindingValue(cui::core::Point{ point.x, point.y });
			return true;
		};
		if (!accessor.HasSegment)
		{
			switch (accessor.Member)
			{
			case PathGeometryMember::FigureStartPoint:
				return readPoint(figure.StartPoint);
			case PathGeometryMember::FigureIsClosed:
				output = BindingValue(figure.IsClosed); return true;
			case PathGeometryMember::FigureIsFilled:
				output = BindingValue(figure.IsFilled); return true;
			default:
				return false;
			}
		}
		if (accessor.SegmentIndex >= figure.Segments.size()) return false;
		const auto& segment = figure.Segments[accessor.SegmentIndex];
		if (segment.Kind != accessor.SegmentKind) return false;
		switch (accessor.Member)
		{
		case PathGeometryMember::SegmentPoint:
			return readPoint(segment.Point);
		case PathGeometryMember::SegmentPoint1:
			return readPoint(segment.Point1);
		case PathGeometryMember::SegmentPoint2:
			return readPoint(segment.Point2);
		case PathGeometryMember::SegmentPoint3:
			return readPoint(segment.Point3);
		case PathGeometryMember::ArcSize:
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !std::isfinite(segment.Size.width)
				|| !std::isfinite(segment.Size.height)
				|| segment.Size.width < 0.0f || segment.Size.height < 0.0f)
				return false;
			output = BindingValue(cui::core::Size{
				segment.Size.width, segment.Size.height });
			return true;
		case PathGeometryMember::ArcRotationAngle:
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !std::isfinite(segment.RotationAngle)) return false;
			output = BindingValue(segment.RotationAngle);
			return true;
		case PathGeometryMember::ArcIsLargeArc:
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc) return false;
			output = BindingValue(segment.IsLargeArc);
			return true;
		case PathGeometryMember::ArcSweepDirection:
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc) return false;
			output = BindingValue(std::wstring(segment.Sweep
				== cui::drawing::SweepDirection::Clockwise
				? L"Clockwise" : L"Counterclockwise"));
			return true;
		default:
			return false;
		}
	}

	static bool TryWritePathGeometryMember(
		cui::drawing::Geometry& root,
		const PathGeometryAccessor& accessor,
		const BindingValue& value) noexcept
	{
		auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		auto& geometry = *resolved;
		if (geometry.Kind != cui::drawing::GeometryKind::Path
			|| accessor.FigureIndex >= geometry.Figures.size()) return false;
		auto& figure = geometry.Figures[accessor.FigureIndex];
		auto readPoint = [&](D2D1_POINT_2F& point)
		{
			cui::core::Point converted{};
			if (!value.TryGet(converted) || !std::isfinite(converted.x)
				|| !std::isfinite(converted.y)) return false;
			point = D2D1::Point2F(converted.x, converted.y);
			return true;
		};
		if (!accessor.HasSegment)
		{
			switch (accessor.Member)
			{
			case PathGeometryMember::FigureStartPoint:
				return readPoint(figure.StartPoint);
			case PathGeometryMember::FigureIsClosed:
			{
				bool converted = false;
				if (!value.TryGet(converted)) return false;
				figure.IsClosed = converted;
				return true;
			}
			case PathGeometryMember::FigureIsFilled:
			{
				bool converted = false;
				if (!value.TryGet(converted)) return false;
				figure.IsFilled = converted;
				return true;
			}
			default:
				return false;
			}
		}
		if (accessor.SegmentIndex >= figure.Segments.size()) return false;
		auto& segment = figure.Segments[accessor.SegmentIndex];
		if (segment.Kind != accessor.SegmentKind) return false;
		switch (accessor.Member)
		{
		case PathGeometryMember::SegmentPoint:
			return readPoint(segment.Point);
		case PathGeometryMember::SegmentPoint1:
			return readPoint(segment.Point1);
		case PathGeometryMember::SegmentPoint2:
			return readPoint(segment.Point2);
		case PathGeometryMember::SegmentPoint3:
			return readPoint(segment.Point3);
		case PathGeometryMember::ArcSize:
		{
			cui::core::Size converted{};
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !value.TryGet(converted)
				|| !std::isfinite(converted.width)
				|| !std::isfinite(converted.height)) return false;
			segment.Size = D2D1::SizeF(
				(std::max)(converted.width, 0.0f),
				(std::max)(converted.height, 0.0f));
			return true;
		}
		case PathGeometryMember::ArcRotationAngle:
		{
			double converted = 0.0;
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !value.TryGetDouble(converted) || !std::isfinite(converted)
				|| converted < -(std::numeric_limits<float>::max)()
				|| converted > (std::numeric_limits<float>::max)()) return false;
			segment.RotationAngle = static_cast<float>(converted);
			return true;
		}
		case PathGeometryMember::ArcIsLargeArc:
		{
			bool converted = false;
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !value.TryGet(converted)) return false;
			segment.IsLargeArc = converted;
			return true;
		}
		case PathGeometryMember::ArcSweepDirection:
		{
			std::wstring converted;
			if (segment.Kind != cui::drawing::PathSegmentKind::Arc
				|| !value.TryGet(converted)) return false;
			if (EqualName(converted, L"Clockwise"))
				segment.Sweep = cui::drawing::SweepDirection::Clockwise;
			else if (EqualName(converted, L"Counterclockwise"))
				segment.Sweep = cui::drawing::SweepDirection::Counterclockwise;
			else return false;
			return true;
		}
		default:
			return false;
		}
	}

	static bool TryReadGeometryTransformMember(
		const cui::drawing::Geometry& root,
		const GeometryTransformAccessor& accessor,
		BindingValue& output) noexcept
	{
		const auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		const auto& geometry = *resolved;
		if (geometry.Kind != accessor.GeometryKind) return false;
		if (!geometry.LocalTransform
			|| !TryReadTransformMember(
				*geometry.LocalTransform, accessor.Transform, output)) return false;
		return true;
	}

	static bool TryWriteGeometryTransformMember(
		cui::drawing::Geometry& root,
		const GeometryTransformAccessor& accessor,
		const BindingValue& value) noexcept
	{
		auto* resolved = TryGetGeometryChild(root, accessor.ChildIndices);
		if (!resolved) return false;
		auto& geometry = *resolved;
		return geometry.Kind == accessor.GeometryKind
			&& geometry.LocalTransform
			&& TryWriteTransformMember(
				*geometry.LocalTransform, accessor.Transform, value);
	}

	static bool TryReadBrushMember(
		const cui::drawing::Brush& brush,
		const BrushAccessor& accessor,
		BindingValue& output) noexcept
	{
		if (brush.Kind != accessor.BrushKind) return false;
		auto finiteColor = [](const D2D1_COLOR_F& color)
		{
			return std::isfinite(color.r) && std::isfinite(color.g)
				&& std::isfinite(color.b) && std::isfinite(color.a);
		};
		auto readPoint = [&](D2D1_POINT_2F point)
		{
			if (!std::isfinite(point.x) || !std::isfinite(point.y)) return false;
			output = BindingValue(cui::core::Point{ point.x, point.y });
			return true;
		};
		switch (accessor.Member)
		{
		case BrushMember::SolidColor:
			if (brush.Kind != cui::drawing::BrushKind::Solid
				|| !finiteColor(brush.Color)) return false;
			output = BindingValue(brush.Color);
			return true;
		case BrushMember::Opacity:
			if (!std::isfinite(brush.Opacity)
				|| brush.Opacity < 0.0f || brush.Opacity > 1.0f) return false;
			output = BindingValue(brush.Opacity);
			return true;
		case BrushMember::StartPoint:
			return brush.Kind == cui::drawing::BrushKind::LinearGradient
				&& readPoint(brush.StartPoint);
		case BrushMember::EndPoint:
			return brush.Kind == cui::drawing::BrushKind::LinearGradient
				&& readPoint(brush.EndPoint);
		case BrushMember::Center:
			return brush.Kind == cui::drawing::BrushKind::RadialGradient
				&& readPoint(brush.Center);
		case BrushMember::GradientOrigin:
			return brush.Kind == cui::drawing::BrushKind::RadialGradient
				&& readPoint(brush.GradientOrigin);
		case BrushMember::RadiusX:
		case BrushMember::RadiusY:
		{
			if (brush.Kind != cui::drawing::BrushKind::RadialGradient) return false;
			const auto radius = accessor.Member == BrushMember::RadiusX
				? brush.RadiusX : brush.RadiusY;
			if (!std::isfinite(radius) || radius < 0.0f) return false;
			output = BindingValue(radius);
			return true;
		}
		case BrushMember::GradientStopColor:
		case BrushMember::GradientStopOffset:
		{
			if ((brush.Kind != cui::drawing::BrushKind::LinearGradient
					&& brush.Kind != cui::drawing::BrushKind::RadialGradient)
				|| accessor.StopIndex >= brush.GradientStops.size()) return false;
			const auto& stop = brush.GradientStops[accessor.StopIndex];
			if (accessor.Member == BrushMember::GradientStopColor)
			{
				if (!finiteColor(stop.Color)) return false;
				output = BindingValue(stop.Color);
				return true;
			}
			if (!std::isfinite(stop.Offset)
				|| stop.Offset < 0.0f || stop.Offset > 1.0f) return false;
			output = BindingValue(stop.Offset);
			return true;
		}
		default:
			return false;
		}
	}

	static bool TryWriteBrushMember(
		cui::drawing::Brush& brush,
		const BrushAccessor& accessor,
		const BindingValue& value) noexcept
	{
		if (brush.Kind != accessor.BrushKind) return false;
		auto readColor = [&](D2D1_COLOR_F& color)
		{
			return value.TryGet(color)
				&& std::isfinite(color.r) && std::isfinite(color.g)
				&& std::isfinite(color.b) && std::isfinite(color.a);
		};
		auto readPoint = [&](D2D1_POINT_2F& point)
		{
			cui::core::Point parsed{};
			if (!value.TryGet(parsed) || !std::isfinite(parsed.x)
				|| !std::isfinite(parsed.y)) return false;
			point = D2D1::Point2F(parsed.x, parsed.y);
			return true;
		};
		auto readNumber = [&](double& number)
		{
			return value.TryGetDouble(number) && std::isfinite(number)
				&& number >= -(std::numeric_limits<float>::max)()
				&& number <= (std::numeric_limits<float>::max)();
		};
		switch (accessor.Member)
		{
		case BrushMember::SolidColor:
		{
			D2D1_COLOR_F color{};
			if (brush.Kind != cui::drawing::BrushKind::Solid || !readColor(color))
				return false;
			brush.Color = color;
			return true;
		}
		case BrushMember::Opacity:
		{
			double opacity = 0.0;
			if (!readNumber(opacity)) return false;
			brush.Opacity = static_cast<float>((std::clamp)(opacity, 0.0, 1.0));
			return true;
		}
		case BrushMember::StartPoint:
			return brush.Kind == cui::drawing::BrushKind::LinearGradient
				&& readPoint(brush.StartPoint);
		case BrushMember::EndPoint:
			return brush.Kind == cui::drawing::BrushKind::LinearGradient
				&& readPoint(brush.EndPoint);
		case BrushMember::Center:
			return brush.Kind == cui::drawing::BrushKind::RadialGradient
				&& readPoint(brush.Center);
		case BrushMember::GradientOrigin:
			return brush.Kind == cui::drawing::BrushKind::RadialGradient
				&& readPoint(brush.GradientOrigin);
		case BrushMember::RadiusX:
		case BrushMember::RadiusY:
		{
			double radius = 0.0;
			if (brush.Kind != cui::drawing::BrushKind::RadialGradient
				|| !readNumber(radius)) return false;
			const auto result = static_cast<float>((std::max)(radius, 0.0));
			if (accessor.Member == BrushMember::RadiusX) brush.RadiusX = result;
			else brush.RadiusY = result;
			return true;
		}
		case BrushMember::GradientStopColor:
		case BrushMember::GradientStopOffset:
		{
			if ((brush.Kind != cui::drawing::BrushKind::LinearGradient
					&& brush.Kind != cui::drawing::BrushKind::RadialGradient)
				|| accessor.StopIndex >= brush.GradientStops.size()) return false;
			auto& stop = brush.GradientStops[accessor.StopIndex];
			if (accessor.Member == BrushMember::GradientStopColor)
			{
				D2D1_COLOR_F color{};
				if (!readColor(color)) return false;
				stop.Color = color;
				return true;
			}
			double offset = 0.0;
			if (!readNumber(offset)) return false;
			// GradientStop.Offset is authored in 0..1 in CUI. Animation results
			// pass through the same target-property coercion at frame write time.
			stop.Offset = static_cast<float>((std::clamp)(offset, 0.0, 1.0));
			return true;
		}
		default:
			return false;
		}
	}

	static bool TryReadBrushTransformMember(
		const cui::drawing::Brush& brush,
		const BrushTransformAccessor& accessor,
		BindingValue& output) noexcept
	{
		if (brush.Kind != accessor.BrushKind) return false;
		const auto& transform = accessor.Relative
			? brush.RelativeTransform : brush.Transform;
		if (!transform
			|| !TryReadTransformMember(*transform, accessor.Transform, output))
			return false;
		return true;
	}

	static bool TryWriteBrushTransformMember(
		cui::drawing::Brush& brush,
		const BrushTransformAccessor& accessor,
		const BindingValue& value) noexcept
	{
		if (brush.Kind != accessor.BrushKind) return false;
		auto& transform = accessor.Relative
			? brush.RelativeTransform : brush.Transform;
		return transform
			&& TryWriteTransformMember(*transform, accessor.Transform, value);
	}

	static bool TryReadObjectPathMember(
		const BindingValue& root,
		const ObjectPathAccessor& accessor,
		BindingValue& output) noexcept
	{
		return std::visit([&](const auto& typed)
		{
			using T = std::decay_t<decltype(typed)>;
			if constexpr (std::is_same_v<T, TransformAccessor>)
			{
				cui::drawing::Transform transform;
				if (!root.TryGet(transform)
					|| !TryReadTransformMember(transform, typed, output)) return false;
				return true;
			}
			else if constexpr (std::is_same_v<T, GeometryAccessor>)
			{
				cui::drawing::Geometry geometry;
				return root.TryGet(geometry)
					&& TryReadGeometryMember(geometry, typed, output);
			}
			else if constexpr (std::is_same_v<T, PathGeometryAccessor>)
			{
				cui::drawing::Geometry geometry;
				return root.TryGet(geometry)
					&& TryReadPathGeometryMember(geometry, typed, output);
			}
			else if constexpr (std::is_same_v<T, GeometryTransformAccessor>)
			{
				cui::drawing::Geometry geometry;
				return root.TryGet(geometry)
					&& TryReadGeometryTransformMember(geometry, typed, output);
			}
			else if constexpr (std::is_same_v<T, BrushAccessor>)
			{
				cui::drawing::Brush brush;
				return root.TryGet(brush)
					&& TryReadBrushMember(brush, typed, output);
			}
			else if constexpr (std::is_same_v<T, BrushTransformAccessor>)
			{
				cui::drawing::Brush brush;
				return root.TryGet(brush)
					&& TryReadBrushTransformMember(brush, typed, output);
			}
			else return false;
		}, accessor);
	}

	static bool TryWriteObjectPathMember(
		BindingValue& root,
		const ObjectPathAccessor& accessor,
		const BindingValue& member) noexcept
	{
		return std::visit([&](const auto& typed)
		{
			using T = std::decay_t<decltype(typed)>;
			if constexpr (std::is_same_v<T, TransformAccessor>)
			{
				cui::drawing::Transform transform;
				if (!root.TryGet(transform)
					|| !TryWriteTransformMember(transform, typed, member)) return false;
				root = BindingValue(std::move(transform));
				return true;
			}
			else if constexpr (std::is_same_v<T, GeometryAccessor>)
			{
				cui::drawing::Geometry geometry;
				if (!root.TryGet(geometry)
					|| !TryWriteGeometryMember(geometry, typed, member)) return false;
				root = BindingValue(std::move(geometry));
				return true;
			}
			else if constexpr (std::is_same_v<T, PathGeometryAccessor>)
			{
				cui::drawing::Geometry geometry;
				if (!root.TryGet(geometry)
					|| !TryWritePathGeometryMember(
						geometry, typed, member)) return false;
				root = BindingValue(std::move(geometry));
				return true;
			}
			else if constexpr (std::is_same_v<T, GeometryTransformAccessor>)
			{
				cui::drawing::Geometry geometry;
				if (!root.TryGet(geometry)
					|| !TryWriteGeometryTransformMember(
						geometry, typed, member)) return false;
				root = BindingValue(std::move(geometry));
				return true;
			}
			else if constexpr (std::is_same_v<T, BrushAccessor>)
			{
				cui::drawing::Brush brush;
				if (!root.TryGet(brush)
					|| !TryWriteBrushMember(brush, typed, member)) return false;
				root = BindingValue(std::move(brush));
				return true;
			}
			else if constexpr (std::is_same_v<T, BrushTransformAccessor>)
			{
				cui::drawing::Brush brush;
				if (!root.TryGet(brush)
					|| !TryWriteBrushTransformMember(brush, typed, member)) return false;
				root = BindingValue(std::move(brush));
				return true;
			}
			else return false;
		}, accessor);
	}

	static double Ease(
		double progress,
		DeclarativeEasingKind kind,
		DeclarativeEasingMode mode) noexcept
	{
		progress = (std::clamp)(progress, 0.0, 1.0);
		if (kind == DeclarativeEasingKind::Linear) return progress;
		auto easeIn = [kind](double value)
		{
			switch (kind)
			{
			case DeclarativeEasingKind::Quadratic: return value * value;
			case DeclarativeEasingKind::Cubic: return value * value * value;
			case DeclarativeEasingKind::Sine:
				return 1.0 - std::cos(value * 1.57079632679489661923);
			case DeclarativeEasingKind::Linear:
			default: return value;
			}
		};
		switch (mode)
		{
		case DeclarativeEasingMode::EaseIn:
			return easeIn(progress);
		case DeclarativeEasingMode::EaseInOut:
			return progress < 0.5
				? easeIn(progress * 2.0) * 0.5
				: 1.0 - easeIn((1.0 - progress) * 2.0) * 0.5;
		case DeclarativeEasingMode::EaseOut:
		default:
			return 1.0 - easeIn(1.0 - progress);
		}
	}

	static double CubicBezier(double t, double p1, double p2) noexcept
	{
		const auto inverse = 1.0 - t;
		return 3.0 * inverse * inverse * t * p1
			+ 3.0 * inverse * t * t * p2 + t * t * t;
	}

	static double CubicBezierDerivative(double t, double p1, double p2) noexcept
	{
		const auto inverse = 1.0 - t;
		return 3.0 * inverse * inverse * p1
			+ 6.0 * inverse * t * (p2 - p1)
			+ 3.0 * t * t * (1.0 - p2);
	}

	static double KeySplineProgress(
		double progress,
		const DeclarativeAnimationKeyFrame& keyFrame) noexcept
	{
		progress = (std::clamp)(progress, 0.0, 1.0);
		double parameter = progress;
		for (int iteration = 0; iteration < 8; ++iteration)
		{
			const auto error = CubicBezier(
				parameter, keyFrame.KeySplineX1, keyFrame.KeySplineX2) - progress;
			if (std::fabs(error) <= 1e-7) break;
			const auto derivative = CubicBezierDerivative(
				parameter, keyFrame.KeySplineX1, keyFrame.KeySplineX2);
			if (std::fabs(derivative) <= 1e-7) break;
			const auto candidate = parameter - error / derivative;
			if (candidate < 0.0 || candidate > 1.0) break;
			parameter = candidate;
		}
		double low = 0.0;
		double high = 1.0;
		for (int iteration = 0; iteration < 18; ++iteration)
		{
			const auto x = CubicBezier(
				parameter, keyFrame.KeySplineX1, keyFrame.KeySplineX2);
			if (std::fabs(x - progress) <= 1e-7) break;
			if (x < progress) low = parameter;
			else high = parameter;
			parameter = (low + high) * 0.5;
		}
		return CubicBezier(
			parameter, keyFrame.KeySplineY1, keyFrame.KeySplineY2);
	}

	static bool InterpolateValues(
		DeclarativeAnimationKind kind,
		const BindingPropertyMetadata* metadata,
		bool transformPath,
		const BindingValue& fromValue,
		const BindingValue& toValue,
		double progress,
		BindingValue& output)
	{
		progress = (std::clamp)(progress, 0.0, 1.0);
		if (kind == DeclarativeAnimationKind::Thickness)
		{
			Thickness from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			Thickness result;
			if (!lerp(from.Left, to.Left, result.Left)
				|| !lerp(from.Top, to.Top, result.Top)
				|| !lerp(from.Right, to.Right, result.Right)
				|| !lerp(from.Bottom, to.Bottom, result.Bottom)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Size)
		{
			cui::core::Size from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Size result;
			if (!lerp(from.width, to.width, result.width)
				|| !lerp(from.height, to.height, result.height)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Matrix)
		{
			D2D1_MATRIX_3X2_F from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)
				|| !IsFiniteMatrix(from) || !IsFiniteMatrix(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			D2D1_MATRIX_3X2_F result{};
			if (!lerp(from._11, to._11, result._11)
				|| !lerp(from._12, to._12, result._12)
				|| !lerp(from._21, to._21, result._21)
				|| !lerp(from._22, to._22, result._22)
				|| !lerp(from._31, to._31, result._31)
				|| !lerp(from._32, to._32, result._32)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Point)
		{
			cui::core::Point from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Point result;
			if (!lerp(from.x, to.x, result.x)
				|| !lerp(from.y, to.y, result.y)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Vector)
		{
			cui::core::Vector from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Vector result;
			if (!lerp(from.x, to.x, result.x)
				|| !lerp(from.y, to.y, result.y)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Rect)
		{
			cui::core::Rect from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right, float& result)
			{
				const auto exact = static_cast<long double>(left)
					+ (static_cast<long double>(right)
						- static_cast<long double>(left)) * progress;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Rect result;
			if (!lerp(from.x, to.x, result.x)
				|| !lerp(from.y, to.y, result.y)
				|| !lerp(from.width, to.width, result.width)
				|| !lerp(from.height, to.height, result.height)) return false;
			output = BindingValue(result);
			return true;
		}
		if (kind == DeclarativeAnimationKind::Color)
		{
			D2D1_COLOR_F from{}, to{};
			if (!fromValue.TryGet(from) || !toValue.TryGet(to)) return false;
			auto lerp = [progress](float left, float right)
			{
				return static_cast<float>(left + (right - left) * progress);
			};
			output = BindingValue(D2D1_COLOR_F{
				lerp(from.r, to.r), lerp(from.g, to.g),
				lerp(from.b, to.b), lerp(from.a, to.a) });
			return true;
		}

		double from = 0.0;
		double to = 0.0;
		if (!fromValue.TryGetDouble(from)
			|| !toValue.TryGetDouble(to)) return false;
		const double value = from + (to - from) * progress;
		if (transformPath)
		{
			if (!std::isfinite(value)
				|| value < -(std::numeric_limits<float>::max)()
				|| value > (std::numeric_limits<float>::max)()) return false;
			output = BindingValue(static_cast<float>(value));
			return true;
		}
		if (!metadata) return false;
		switch (metadata->ValueKind())
		{
		case BindingValueKind::Int:
			if (value <= static_cast<double>((std::numeric_limits<int>::min)()))
				output = BindingValue((std::numeric_limits<int>::min)());
			else if (value >= static_cast<double>((std::numeric_limits<int>::max)()))
				output = BindingValue((std::numeric_limits<int>::max)());
			else
				output = BindingValue(static_cast<int>(std::llround(value)));
			return true;
		case BindingValueKind::Int64:
			// The double representation of LLONG_MAX rounds to 2^63, which is
			// outside llround's domain. Clamp before rounding so a legal endpoint
			// cannot introduce undefined behaviour on the final animation frame.
			if (value <= static_cast<double>((std::numeric_limits<long long>::min)()))
				output = BindingValue((std::numeric_limits<long long>::min)());
			else if (value >= static_cast<double>((std::numeric_limits<long long>::max)()))
				output = BindingValue((std::numeric_limits<long long>::max)());
			else
				output = BindingValue(static_cast<long long>(std::llround(value)));
			return true;
		case BindingValueKind::Float:
			output = BindingValue(static_cast<float>(value));
			return true;
		case BindingValueKind::Double:
			output = BindingValue(value);
			return true;
		default:
			return false;
		}
	}

	template<typename TAnimation>
	static bool CombineAnimationValues(
		const TAnimation& animation,
		const BindingValue& left,
		const BindingValue& right,
		long double rightScale,
		BindingValue& output)
	{
		if (animation.Kind == DeclarativeAnimationKind::Thickness)
		{
			Thickness base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			Thickness result;
			if (!combine(base.Left, increment.Left, result.Left)
				|| !combine(base.Top, increment.Top, result.Top)
				|| !combine(base.Right, increment.Right, result.Right)
				|| !combine(base.Bottom, increment.Bottom, result.Bottom)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Size)
		{
			cui::core::Size base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Size result;
			if (!combine(base.width, increment.width, result.width)
				|| !combine(base.height, increment.height, result.height)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Matrix)
		{
			D2D1_MATRIX_3X2_F base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)
				|| !IsFiniteMatrix(base) || !IsFiniteMatrix(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			D2D1_MATRIX_3X2_F result{};
			if (!combine(base._11, increment._11, result._11)
				|| !combine(base._12, increment._12, result._12)
				|| !combine(base._21, increment._21, result._21)
				|| !combine(base._22, increment._22, result._22)
				|| !combine(base._31, increment._31, result._31)
				|| !combine(base._32, increment._32, result._32)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Point)
		{
			cui::core::Point base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Point result;
			if (!combine(base.x, increment.x, result.x)
				|| !combine(base.y, increment.y, result.y)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Vector)
		{
			cui::core::Vector base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Vector result;
			if (!combine(base.x, increment.x, result.x)
				|| !combine(base.y, increment.y, result.y)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Rect)
		{
			cui::core::Rect base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			auto combine = [rightScale](float leftValue, float rightValue,
				float& result)
			{
				const auto exact = static_cast<long double>(leftValue)
					+ static_cast<long double>(rightValue) * rightScale;
				if (!std::isfinite(exact)
					|| exact < -(std::numeric_limits<float>::max)()
					|| exact > (std::numeric_limits<float>::max)()) return false;
				result = static_cast<float>(exact);
				return std::isfinite(result);
			};
			cui::core::Rect result;
			if (!combine(base.x, increment.x, result.x)
				|| !combine(base.y, increment.y, result.y)
				|| !combine(base.width, increment.width, result.width)
				|| !combine(base.height, increment.height, result.height)) return false;
			output = BindingValue(result);
			return true;
		}
		if (animation.Kind == DeclarativeAnimationKind::Color)
		{
			D2D1_COLOR_F base{}, increment{};
			if (!left.TryGet(base) || !right.TryGet(increment)) return false;
			const D2D1_COLOR_F result{
				static_cast<float>(base.r + increment.r * rightScale),
				static_cast<float>(base.g + increment.g * rightScale),
				static_cast<float>(base.b + increment.b * rightScale),
				static_cast<float>(base.a + increment.a * rightScale) };
			if (!std::isfinite(result.r) || !std::isfinite(result.g)
				|| !std::isfinite(result.b) || !std::isfinite(result.a)) return false;
			output = BindingValue(result);
			return true;
		}

		double base = 0.0;
		double increment = 0.0;
		if (!left.TryGetDouble(base) || !right.TryGetDouble(increment)) return false;
		const long double exact = static_cast<long double>(base)
			+ static_cast<long double>(increment) * rightScale;
		if (!std::isfinite(exact)
			|| exact < -(std::numeric_limits<double>::max)()
			|| exact > (std::numeric_limits<double>::max)()) return false;
		const double result = static_cast<double>(exact);
		if (!std::isfinite(result)) return false;
		if ((ObjectPathUsesFloat(animation.ObjectPath)
				|| (animation.Metadata
					&& animation.Metadata->ValueKind() == BindingValueKind::Float))
			&& (result < -(std::numeric_limits<float>::max)()
				|| result > (std::numeric_limits<float>::max)())) return false;
		output = BindingValue(result);
		return true;
	}

	template<typename TAnimation>
	static BindingValue ZeroAnimationValue(const TAnimation& animation)
	{
		if (animation.Kind == DeclarativeAnimationKind::Object)
			return BindingValue{};
		if (animation.Kind == DeclarativeAnimationKind::Thickness)
			return BindingValue(Thickness{});
		if (animation.Kind == DeclarativeAnimationKind::Point)
			return BindingValue(cui::core::Point{});
		if (animation.Kind == DeclarativeAnimationKind::Vector)
			return BindingValue(cui::core::Vector{});
		if (animation.Kind == DeclarativeAnimationKind::Rect)
			return BindingValue(cui::core::Rect{});
		if (animation.Kind == DeclarativeAnimationKind::Size)
			return BindingValue(cui::core::Size{});
		if (animation.Kind == DeclarativeAnimationKind::Matrix)
			return BindingValue(D2D1_MATRIX_3X2_F{});
		return animation.Kind == DeclarativeAnimationKind::Color
			? BindingValue(D2D1_COLOR_F{}) : BindingValue(0.0);
	}

	template<typename TAnimation>
	static bool AddAnimationValues(
		const TAnimation& animation,
		const BindingValue& left,
		const BindingValue& delta,
		BindingValue& output)
	{
		return CombineAnimationValues(animation, left, delta, 1.0L, output);
	}

	static bool ResolveAnimationEndpoints(
		const RuntimeAnimation& animation,
		const BindingValue& defaultOrigin,
		const BindingValue& defaultDestination,
		BindingValue& from,
		BindingValue& to,
		BindingValue& foundation)
	{
		if (defaultOrigin.Kind() == BindingValueKind::Empty
			|| defaultDestination.Kind() == BindingValueKind::Empty) return false;
		const auto zero = ZeroAnimationValue(animation);
		if (!animation.KeyFrames.empty())
		{
			if (animation.Kind == DeclarativeAnimationKind::Object)
			{
				from = defaultOrigin;
				to = defaultDestination;
				foundation = BindingValue{};
				return true;
			}
			from = animation.IsAdditive ? zero : defaultOrigin;
			to = defaultDestination;
			foundation = animation.IsAdditive ? defaultOrigin : zero;
			return true;
		}
		const bool hasFrom = animation.From.has_value();
		const bool hasTo = animation.To.has_value();
		const bool hasBy = animation.By.has_value() && !hasTo;
		if (!hasFrom && hasBy)
		{
			from = zero;
			to = *animation.By;
			foundation = defaultOrigin;
			return true;
		}
		from = hasFrom ? *animation.From : defaultOrigin;
		if (animation.To)
			to = *animation.To;
		else if (hasBy)
		{
			if (!AddAnimationValues(animation, from, *animation.By, to)) return false;
		}
		else to = defaultDestination;
		foundation = animation.IsAdditive && hasFrom && (hasTo || hasBy)
			? defaultOrigin : zero;
		return true;
	}

	static long double TimelineActiveDurationExact(
		unsigned long long durationMilliseconds,
		DeclarativeRepeatBehaviorKind repeatBehavior,
		double repeatCount,
		unsigned long long repeatDurationMilliseconds,
		bool autoReverse,
		double speedRatio) noexcept
	{
		if (repeatBehavior == DeclarativeRepeatBehaviorKind::Forever)
			return std::numeric_limits<long double>::infinity();
		if (repeatBehavior == DeclarativeRepeatBehaviorKind::Duration)
			return static_cast<long double>(repeatDurationMilliseconds);
		return static_cast<long double>(durationMilliseconds)
			* (autoReverse ? 2.0L : 1.0L)
			* static_cast<long double>(repeatCount)
			/ static_cast<long double>(speedRatio);
	}

	static long double TimelineActiveDurationExact(
		const RuntimeAnimation& animation) noexcept
	{
		return TimelineActiveDurationExact(animation.DurationMilliseconds,
			animation.RepeatBehavior, animation.RepeatCount,
			animation.RepeatDurationMilliseconds, animation.AutoReverse,
			animation.SpeedRatio);
	}

	static long double TimelineActiveDurationExact(
		const ActiveAnimation& animation) noexcept
	{
		return TimelineActiveDurationExact(animation.DurationMilliseconds,
			animation.RepeatBehavior, animation.RepeatCount,
			animation.RepeatDurationMilliseconds, animation.AutoReverse,
			animation.SpeedRatio);
	}

	static unsigned long long TimelineActiveDurationMilliseconds(
		const RuntimeAnimation& animation) noexcept
	{
		const auto duration = TimelineActiveDurationExact(animation);
		const auto maximum = (std::numeric_limits<unsigned long long>::max)();
		if (!std::isfinite(duration)
			|| duration >= static_cast<long double>(maximum)) return maximum;
		return static_cast<unsigned long long>(std::ceil(duration));
	}

	static unsigned long long TimelineActiveDurationMilliseconds(
		const ActiveAnimation& animation) noexcept
	{
		const auto duration = TimelineActiveDurationExact(animation);
		const auto maximum = (std::numeric_limits<unsigned long long>::max)();
		if (!std::isfinite(duration)
			|| duration >= static_cast<long double>(maximum)) return maximum;
		return static_cast<unsigned long long>(std::ceil(duration));
	}

	static long double ApplyTimelineAcceleration(
		long double simpleElapsed,
		long double simpleDuration,
		double accelerationRatio,
		double decelerationRatio) noexcept
	{
		if (simpleDuration <= 0.0L) return 0.0L;
		const long double transition = static_cast<long double>(
			accelerationRatio + decelerationRatio);
		if (transition <= 0.0L) return simpleElapsed;
		auto progress = (std::clamp)(
			simpleElapsed / simpleDuration, 0.0L, 1.0L);
		const auto acceleration = static_cast<long double>(accelerationRatio);
		const auto deceleration = static_cast<long double>(decelerationRatio);
		const auto maximumRate = 2.0L / (2.0L - transition);
		if (progress < acceleration)
			progress = maximumRate * progress * progress
				/ (2.0L * acceleration);
		else if (progress <= 1.0L - deceleration)
			progress = maximumRate * (progress - acceleration / 2.0L);
		else
		{
			const auto complement = 1.0L - progress;
			progress = 1.0L - maximumRate * complement * complement
				/ (2.0L * deceleration);
		}
		return (std::clamp)(progress, 0.0L, 1.0L) * simpleDuration;
	}

	static bool ComposeAnimationValue(
		const ActiveAnimation& animation,
		const BindingValue& localValue,
		long double completedIterations,
		BindingValue& output)
	{
		if (animation.Kind == DeclarativeAnimationKind::Object)
		{
			output = localValue;
			return true;
		}
		BindingValue value = localValue;
		if (animation.IsCumulative && completedIterations > 0.0L)
		{
			BindingValue delta;
			if (!animation.KeyFrames.empty())
				delta = animation.KeyFrames.back().Value;
			else if (!CombineAnimationValues(
				animation, animation.To, animation.From, -1.0L, delta))
				return false;
			BindingValue accumulated;
			if (!CombineAnimationValues(animation, value, delta,
				completedIterations, accumulated)) return false;
			value = std::move(accumulated);
		}
		return AddAnimationValues(
			animation, value, animation.Foundation, output);
	}

	static bool Interpolate(
		const ActiveAnimation& animation,
		unsigned long long activeElapsedMilliseconds,
		BindingValue& output)
	{
		const auto activeDurationExact = TimelineActiveDurationExact(animation);
		long double parentElapsed = static_cast<long double>(
			activeElapsedMilliseconds);
		const bool atActiveBoundary = std::isfinite(activeDurationExact)
			&& parentElapsed >= activeDurationExact;
		if (std::isfinite(activeDurationExact))
			parentElapsed = (std::min)(parentElapsed, activeDurationExact);
		long double simpleElapsed = parentElapsed
			* static_cast<long double>(animation.SpeedRatio);
		const auto simpleDuration = static_cast<long double>(
			animation.DurationMilliseconds);
		long double completedIterations = 0.0L;
		if (simpleDuration > 0.0L)
		{
			const auto repetitionDuration = simpleDuration
				* (animation.AutoReverse ? 2.0L : 1.0L);
			if (!std::isfinite(simpleElapsed)) return false;
			completedIterations = std::floor(simpleElapsed / repetitionDuration);
			auto local = std::fmod(simpleElapsed, repetitionDuration);
			// Interior repetition boundaries begin the next iteration. At the
			// finite active-period boundary, sample the completed repetition so
			// HoldEnd gets To (or From after an auto-reverse), matching WPF.
			if (atActiveBoundary
				&& simpleElapsed > 0.0L
				&& std::fabs(local) < 0.0000001L)
			{
				local = repetitionDuration;
				completedIterations = (std::max)(
					0.0L, completedIterations - 1.0L);
			}
			if (animation.AutoReverse && local > simpleDuration)
				local = repetitionDuration - local;
			simpleElapsed = (std::clamp)(
				local, 0.0L, simpleDuration);
		}
		else simpleElapsed = 0.0L;
		simpleElapsed = ApplyTimelineAcceleration(simpleElapsed, simpleDuration,
			animation.AccelerationRatio, animation.DecelerationRatio);
		if (animation.KeyFrames.empty())
		{
			const double progress = animation.DurationMilliseconds == 0
				? 1.0
				: (std::min)(1.0,
					static_cast<double>(simpleElapsed)
						/ static_cast<double>(animation.DurationMilliseconds));
			BindingValue localValue;
			if (!InterpolateValues(animation.Kind, animation.Metadata,
				ObjectPathUsesFloat(animation.ObjectPath),
				animation.From, animation.To,
				Ease(progress, animation.Easing, animation.EasingMode), localValue))
				return false;
			return ComposeAnimationValue(
				animation, localValue, completedIterations, output);
		}

		BindingValue previousValue = animation.From;
		unsigned long long previousTime = 0;
		for (const auto& keyFrame : animation.KeyFrames)
		{
			if (simpleElapsed
				< static_cast<long double>(keyFrame.KeyTimeMilliseconds))
			{
				if (keyFrame.Kind == DeclarativeKeyFrameKind::Discrete)
				{
					return ComposeAnimationValue(animation, previousValue,
						completedIterations, output);
				}
				const auto span = keyFrame.KeyTimeMilliseconds - previousTime;
				const double segmentProgress = span == 0 ? 1.0
					: static_cast<double>(simpleElapsed
						- static_cast<long double>(previousTime))
						/ static_cast<double>(span);
				double eased = segmentProgress;
				if (keyFrame.Kind == DeclarativeKeyFrameKind::Easing)
					eased = Ease(segmentProgress,
						keyFrame.Easing, keyFrame.EasingMode);
				else if (keyFrame.Kind == DeclarativeKeyFrameKind::Spline)
					eased = KeySplineProgress(segmentProgress, keyFrame);
				BindingValue localValue;
				if (!InterpolateValues(animation.Kind, animation.Metadata,
					ObjectPathUsesFloat(animation.ObjectPath), previousValue,
					keyFrame.Value, eased, localValue)) return false;
				return ComposeAnimationValue(animation, localValue,
					completedIterations, output);
			}
			previousTime = keyFrame.KeyTimeMilliseconds;
			previousValue = keyFrame.Value;
		}
		return ComposeAnimationValue(animation, previousValue,
			completedIterations, output);
	}

	struct AnimationFrameValue
	{
		const ActiveAnimation* Animation = nullptr;
		BindingValue Value;
	};

	static ControlPropertyValueSource AnimationValueSource(
		const ActiveAnimation& animation) noexcept
	{
		return animation.IsEventStoryboard
			? ControlPropertyValueSource::Animation
			: ControlPropertyValueSource::VisualState;
	}

	bool TryReadAnimationFrameRoot(
		Control* target,
		const BindingPropertyMetadata* metadata,
		const std::wstring& propertyName,
		ControlPropertyValueSource source,
		BindingValue& output)
	{
		if (!target || !metadata) return false;
		if (target->TryGetPropertyValue(propertyName, source, output))
			return true;
		if (source != ControlPropertyValueSource::VisualState)
			return metadata->TryGet(*target, output);

		// A VisualState object value must be composed from the layer below it.
		// Temporarily hide the higher animation layer so an event clock cannot
		// leak its snapshot into the state value that it is masking.
		BindingValue animationValue;
		const bool hadAnimation = target->TryGetPropertyValue(
			propertyName, ControlPropertyValueSource::Animation, animationValue);
		if (hadAnimation && !target->ClearPropertyValue(
			propertyName, ControlPropertyValueSource::Animation)) return false;
		const bool read = metadata->TryGet(*target, output);
		const bool restored = !hadAnimation || target->TrySetPropertyValue(
			propertyName, animationValue, ControlPropertyValueSource::Animation);
		return read && restored;
	}

	bool ApplyAnimationFrame(
		const std::vector<AnimationFrameValue>& values)
	{
		struct ObjectFrame
		{
			Control* Target = nullptr;
			const BindingPropertyMetadata* Metadata = nullptr;
			std::wstring PropertyName;
			ControlPropertyValueSource Source =
				ControlPropertyValueSource::VisualState;
			BindingValue Value;
		};
		std::vector<ObjectFrame> objects;
		for (const auto& frame : values)
		{
			const auto* animation = frame.Animation;
			if (!animation || !animation->Target || !animation->Metadata)
				return false;
			const auto source = AnimationValueSource(*animation);
			if (!animation->ObjectPath)
			{
				if (!animation->Target->TrySetPropertyValue(
					animation->PropertyName, frame.Value,
					source)) return false;
				continue;
			}
			auto found = std::find_if(objects.begin(), objects.end(),
			[&](const auto& candidate)
			{
				return candidate.Target == animation->Target
					&& candidate.Metadata == animation->Metadata
					&& candidate.Source == source;
			});
			if (found == objects.end())
			{
				BindingValue current;
				if (!TryReadAnimationFrameRoot(animation->Target,
					animation->Metadata, animation->PropertyName, source, current))
					return false;
				objects.push_back({ animation->Target, animation->Metadata,
					animation->PropertyName, source, std::move(current) });
				found = std::prev(objects.end());
			}
			if (!TryWriteObjectPathMember(
				found->Value, *animation->ObjectPath, frame.Value)) return false;
		}
		for (auto& object : objects)
			if (!object.Target->TrySetPropertyValue(
				object.PropertyName, object.Value,
				object.Source)) return false;
		return true;
	}

	void ReleaseStoppedAnimationValues(
		const std::vector<const ActiveAnimation*>& stoppingAnimations,
		const std::vector<ActiveAnimation>& animations,
		unsigned long long nowMilliseconds) noexcept
	{
		for (const auto* stopping : stoppingAnimations)
		{
			if (!stopping || !stopping->Target || !stopping->Metadata) continue;
			const auto source = AnimationValueSource(*stopping);
			const auto siblingAffectsRoot = std::any_of(
				animations.begin(), animations.end(), [&](const auto& candidate)
				{
					if (&candidate == stopping
						|| candidate.IsEventStoryboard
							!= stopping->IsEventStoryboard
						|| candidate.Target != stopping->Target
						|| !EqualName(candidate.PropertyName,
							stopping->PropertyName)) return false;
					const auto clockTick = candidate.Paused
						? candidate.PauseTick : nowMilliseconds;
					const auto elapsed = clockTick >= candidate.StartTick
						? clockTick - candidate.StartTick : 0;
					if (elapsed < candidate.BeginTimeMilliseconds) return false;
					if (candidate.Completed)
						return candidate.FillBehavior
							== DeclarativeTimelineFillBehavior::HoldEnd;
					const auto completed = elapsed - candidate.BeginTimeMilliseconds
						>= TimelineActiveDurationMilliseconds(candidate);
					return !completed || candidate.FillBehavior
						== DeclarativeTimelineFillBehavior::HoldEnd;
				});
			if (!stopping->ObjectPath)
			{
				if (stopping->RestoreBaseOnStop
					&& source == ControlPropertyValueSource::VisualState)
					(void)stopping->Target->TrySetPropertyValue(
						stopping->PropertyName, stopping->Base, source);
				else if (!siblingAffectsRoot)
					(void)stopping->Target->ClearPropertyValue(
						stopping->PropertyName, source);
				continue;
			}
			if (!siblingAffectsRoot && (!stopping->RestoreBaseOnStop
				|| source == ControlPropertyValueSource::Animation))
			{
				(void)stopping->Target->ClearPropertyValue(
					stopping->PropertyName, source);
				continue;
			}
			BindingValue root;
			if (!(stopping->Target->TryGetPropertyValue(
				stopping->PropertyName, source, root)
					|| stopping->Metadata->TryGet(*stopping->Target, root))) continue;
			if (TryWriteObjectPathMember(
				root, *stopping->ObjectPath, stopping->Base))
				(void)stopping->Target->TrySetPropertyValue(
					stopping->PropertyName, root, source);
		}
	}

	bool HasActiveAnimations() const noexcept
	{
		return std::any_of(ActiveAnimations.begin(), ActiveAnimations.end(),
			[](const auto& animation)
			{ return !animation.Completed && !animation.Paused; })
			|| std::any_of(Groups.begin(), Groups.end(),
				[](const auto& group) { return group.Pending.has_value(); });
	}

	bool AdvanceAnimations(unsigned long long nowMilliseconds)
	{
		if (!HasActiveAnimations()) return false;
		const bool hadActive = true;
		Applying = true;
		std::vector<AnimationFrameValue> frameValues;
		std::vector<const ActiveAnimation*> stoppingAnimations;
		frameValues.reserve(ActiveAnimations.size());
		for (auto& animation : ActiveAnimations)
		{
			const auto clockTick = animation.Paused
				? animation.PauseTick : nowMilliseconds;
			const auto elapsed = clockTick >= animation.StartTick
				? clockTick - animation.StartTick : 0;
			if (elapsed < animation.BeginTimeMilliseconds) continue;
			const auto activeDuration =
				TimelineActiveDurationMilliseconds(animation);
			const auto activeElapsed = animation.Completed
				? activeDuration : elapsed - animation.BeginTimeMilliseconds;
			BindingValue value;
			if (Interpolate(animation, activeElapsed, value))
				frameValues.push_back({ &animation, std::move(value) });
			if (!animation.Completed && animation.FillBehavior
				== DeclarativeTimelineFillBehavior::Stop
				&& activeElapsed >= activeDuration)
				stoppingAnimations.push_back(&animation);
		}
		(void)ApplyAnimationFrame(frameValues);
		ReleaseStoppedAnimationValues(
			stoppingAnimations, ActiveAnimations, nowMilliseconds);
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](auto& animation)
			{
				const auto clockTick = animation.Paused
					? animation.PauseTick : nowMilliseconds;
				const auto elapsed = clockTick >= animation.StartTick
					? clockTick - animation.StartTick : 0;
				const bool completed = animation.Completed
					|| (elapsed >= animation.BeginTimeMilliseconds
					&& elapsed - animation.BeginTimeMilliseconds
						>= TimelineActiveDurationMilliseconds(animation));
				if (!completed) return false;
				if (animation.IsEventStoryboard && animation.FillBehavior
					== DeclarativeTimelineFillBehavior::HoldEnd)
				{
					animation.Completed = true;
					return false;
				}
				return true;
			}), ActiveAnimations.end());
		if (!stoppingAnimations.empty())
			(void)ApplyRetainedAnimationFrame(nowMilliseconds);
		Applying = false;
		for (size_t groupIndex = 0; groupIndex < Groups.size(); ++groupIndex)
		{
			auto& group = Groups[groupIndex];
			if (!group.Pending || nowMilliseconds < group.Pending->EndTick) continue;
			const auto targetState = group.Pending->TargetState;
			auto transitionProperties = std::move(group.Pending->Properties);
			group.Pending.reset();
			ActiveAnimations.erase(std::remove_if(
				ActiveAnimations.begin(), ActiveAnimations.end(),
				[&](const auto& animation)
				{ return !animation.IsEventStoryboard
					&& animation.GroupIndex == groupIndex; }),
				ActiveAnimations.end());
			if (targetState < group.States.size()
				&& GoToImmediate(groupIndex, targetState, nullptr,
					nowMilliseconds, false))
				ClearTransitionOnlyProperties(
					transitionProperties, group.States[targetState]);
		}
		return hadActive;
	}

	void ClearAppliedValues() noexcept
	{
		if (!Owner) return;
		Applying = true;
		std::vector<PropertyKey> cleared;
		for (const auto& group : Groups)
		{
			if (group.Pending)
				for (const auto& key : group.Pending->Properties)
				{
					if (!key.Target || std::any_of(cleared.begin(), cleared.end(),
						[&](const auto& existing)
						{ return SameProperty(existing, key); })) continue;
					cleared.push_back(key);
					(void)key.Target->ClearPropertyValue(
						key.PropertyName, ControlPropertyValueSource::VisualState);
				}
			if (!group.CurrentState || *group.CurrentState >= group.States.size())
				continue;
			for (const auto& setter : group.States[*group.CurrentState].Setters)
			{
				PropertyKey key{ setter.Target, setter.PropertyName };
				if (std::any_of(cleared.begin(), cleared.end(),
					[&](const auto& existing) { return SameProperty(existing, key); }))
					continue;
				cleared.push_back(key);
				if (key.Target)
					(void)key.Target->ClearPropertyValue(
						key.PropertyName, ControlPropertyValueSource::VisualState);
			}
			for (const auto& animation : group.States[*group.CurrentState].Animations)
			{
				PropertyKey key{ animation.Target, animation.PropertyName };
				if (std::any_of(cleared.begin(), cleared.end(),
					[&](const auto& existing) { return SameProperty(existing, key); }))
					continue;
				cleared.push_back(key);
				if (key.Target)
					(void)key.Target->ClearPropertyValue(
						key.PropertyName, ControlPropertyValueSource::VisualState);
			}
		}
		std::vector<PropertyKey> animationCleared;
		for (const auto& storyboard : EventStoryboards)
			for (const auto& animation : storyboard.Animations)
			{
				PropertyKey key{ animation.Target, animation.PropertyName };
				if (!key.Target || std::any_of(
					animationCleared.begin(), animationCleared.end(),
					[&](const auto& existing)
					{ return SameProperty(existing, key); })) continue;
				animationCleared.push_back(key);
				(void)key.Target->ClearPropertyValue(
					key.PropertyName, ControlPropertyValueSource::Animation);
			}
		Applying = false;
	}

	bool StateMatches(const RuntimeState& state) const
	{
		if (state.Conditions.empty()) return false;
		for (const auto& condition : state.Conditions)
		{
			BindingValue actual;
			if (!condition.Metadata
				|| !condition.Metadata->TryGet(*Owner, actual)
				|| !condition.Metadata->ValuesEqual(actual, condition.Value))
				return false;
		}
		return true;
	}

	size_t EvaluateState(const RuntimeGroup& group) const
	{
		for (size_t index = 0; index < group.States.size(); ++index)
			if (!group.States[index].Conditions.empty()
				&& StateMatches(group.States[index])) return index;
		return group.FallbackState;
	}

	bool RestoreSnapshots(const std::vector<PropertySnapshot>& snapshots) noexcept
	{
		bool restored = true;
		for (const auto& snapshot : snapshots)
		{
			if (!snapshot.Key.Target) continue;
			if (snapshot.Value)
				restored = snapshot.Key.Target->TrySetPropertyValue(
					snapshot.Key.PropertyName, *snapshot.Value,
					snapshot.Source) && restored;
			else if (snapshot.Key.Target->HasPropertyValue(
				snapshot.Key.PropertyName, snapshot.Source))
				restored = snapshot.Key.Target->ClearPropertyValue(
					snapshot.Key.PropertyName, snapshot.Source) && restored;
		}
		return restored;
	}

	bool GoToImmediate(
		size_t groupIndex,
		size_t stateIndex,
		std::wstring* outError,
		std::optional<unsigned long long> requestedStartTick,
		bool force)
	{
		if (groupIndex >= Groups.size()
			|| stateIndex >= Groups[groupIndex].States.size())
		{
			if (outError) *outError = L"视觉状态索引无效。";
			return false;
		}
		auto& group = Groups[groupIndex];
		if (!force && group.CurrentState && *group.CurrentState == stateIndex)
		{
			if (outError) outError->clear();
			return true;
		}
		const RuntimeState* previous = group.CurrentState
			? &group.States[*group.CurrentState] : nullptr;
		const auto& next = group.States[stateIndex];

		std::vector<PropertyKey> affected;
		auto addAffected = [&](Control* target, const std::wstring& propertyName)
		{
			PropertyKey key{ target, propertyName };
			if (std::none_of(affected.begin(), affected.end(),
				[&](const auto& existing) { return SameProperty(existing, key); }))
				affected.push_back(std::move(key));
		};
		if (previous)
		{
			for (const auto& setter : previous->Setters)
				addAffected(setter.Target, setter.PropertyName);
			for (const auto& animation : previous->Animations)
				addAffected(animation.Target, animation.PropertyName);
		}
		for (const auto& setter : next.Setters)
			addAffected(setter.Target, setter.PropertyName);
		for (const auto& animation : next.Animations)
			addAffected(animation.Target, animation.PropertyName);

		std::vector<PropertySnapshot> snapshots;
		snapshots.reserve(affected.size());
		for (const auto& key : affected)
		{
			PropertySnapshot snapshot;
			snapshot.Key = key;
			BindingValue value;
			if (key.Target && key.Target->TryGetPropertyValue(
				key.PropertyName, ControlPropertyValueSource::VisualState, value))
				snapshot.Value = std::move(value);
			snapshots.push_back(std::move(snapshot));
		}

		unsigned long long startTick = requestedStartTick.value_or(0);
		std::vector<ActiveAnimation> pendingAnimations;
		pendingAnimations.reserve(next.Animations.size());
		for (const auto& animation : next.Animations)
		{
			BindingValue current;
			if (!TryReadAnimationValue(animation, current))
			{
				if (outError) *outError = L"视觉状态动画无法捕获起始值："
					+ animation.PropertyName;
				return false;
			}
			BindingValue base;
			if (!TryReadBaseAnimationValue(animation, base))
			{
				if (outError) *outError = L"视觉状态动画无法捕获基础值："
					+ animation.PropertyName;
				return false;
			}
			BindingValue from;
			BindingValue to;
			BindingValue foundation;
			if (!ResolveAnimationEndpoints(
				animation, current, base, from, to, foundation))
			{
				if (outError) *outError = L"视觉状态动画无法解析 From/To/By："
					+ animation.PropertyName;
				return false;
			}
			pendingAnimations.push_back({
				groupIndex, animation.Target, animation.Metadata,
				animation.PropertyName, animation.Kind,
				std::move(base), std::move(foundation),
				std::move(from), std::move(to),
				animation.KeyFrames,
				animation.IsCumulative,
				animation.ObjectPath, startTick,
				animation.BeginTimeMilliseconds,
				animation.DurationMilliseconds,
				animation.RepeatBehavior, animation.RepeatCount,
				animation.RepeatDurationMilliseconds,
				animation.AutoReverse, animation.FillBehavior,
				animation.SpeedRatio, animation.AccelerationRatio,
				animation.DecelerationRatio,
				animation.RestoreBaseOnStop,
				animation.Easing, animation.EasingMode });
		}

		auto nextControls = [&](Control* target, const std::wstring& propertyName)
		{
			return std::any_of(next.Setters.begin(), next.Setters.end(),
				[&](const auto& candidate)
				{
					return candidate.Target == target
						&& EqualName(candidate.PropertyName, propertyName);
				}) || std::any_of(next.Animations.begin(), next.Animations.end(),
				[&](const auto& candidate)
				{
					return candidate.Target == target
						&& EqualName(candidate.PropertyName, propertyName);
				});
		};

		Applying = true;
		bool success = true;
		if (previous)
		{
			for (const auto& key : affected)
			{
				if (!nextControls(key.Target, key.PropertyName) && key.Target
					&& key.Target->HasPropertyValue(
						key.PropertyName, ControlPropertyValueSource::VisualState)
					&& !key.Target->ClearPropertyValue(
						key.PropertyName,
						ControlPropertyValueSource::VisualState))
				{
					success = false;
					break;
				}
			}
		}
		const bool animationsEnabled = Owner->AreSystemAnimationsEnabled();
		if (success)
		{
			for (const auto& setter : next.Setters)
				if (!setter.Target || !setter.Target->TrySetPropertyValue(
					setter.PropertyName, setter.Value,
					ControlPropertyValueSource::VisualState))
				{
					success = false;
					break;
				}
		}
		if (success)
		{
			std::vector<AnimationFrameValue> initialValues;
			initialValues.reserve(pendingAnimations.size());
			for (const auto& animation : pendingAnimations)
			{
				const auto activeDuration =
					TimelineActiveDurationMilliseconds(animation);
				const bool active = animationsEnabled
					&& (animation.BeginTimeMilliseconds > 0
						|| activeDuration > 0);
				BindingValue value;
				if (active && animation.BeginTimeMilliseconds > 0)
					value = animation.Base;
				else if (!active && animation.FillBehavior
					== DeclarativeTimelineFillBehavior::Stop)
					value = animation.Base;
				else if (!Interpolate(animation,
					active ? 0 : activeDuration, value))
				{
					success = false;
					break;
				}
				initialValues.push_back({ &animation, std::move(value) });
			}
			if (success) success = ApplyAnimationFrame(initialValues);
			if (success)
			{
				startTick = requestedStartTick.value_or(::GetTickCount64());
				for (auto& animation : pendingAnimations)
					animation.StartTick = startTick;
				std::vector<const ActiveAnimation*> stopped;
				for (const auto& animation : pendingAnimations)
					if (animation.FillBehavior
						== DeclarativeTimelineFillBehavior::Stop
						&& (!animationsEnabled
							|| TimelineActiveDurationMilliseconds(animation) == 0))
						stopped.push_back(&animation);
				ReleaseStoppedAnimationValues(stopped, pendingAnimations,
					animationsEnabled ? startTick
						: (std::numeric_limits<unsigned long long>::max)());
			}
		}
		if (!success)
		{
			(void)RestoreSnapshots(snapshots);
			Applying = false;
			if (outError) *outError = L"视觉状态 Setter/Storyboard 无法事务性应用。";
			return false;
		}

		const auto oldState = previous ? previous->Name : std::wstring{};
		group.CurrentState = stateIndex;
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](const auto& animation) { return !animation.IsEventStoryboard
				&& animation.GroupIndex == groupIndex; }),
			ActiveAnimations.end());
		if (animationsEnabled)
			for (auto& animation : pendingAnimations)
				if (animation.BeginTimeMilliseconds > 0
					|| TimelineActiveDurationMilliseconds(animation) > 0)
					ActiveAnimations.push_back(std::move(animation));
		Applying = false;
		if (std::any_of(ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](const auto& animation) { return !animation.IsEventStoryboard
				&& animation.GroupIndex == groupIndex; }))
			Owner->InvalidateVisual();
		DeclarativeVisualStateChangedEventArgs args{
			group.Name, oldState, next.Name };
		Owner->OnVisualStateChanged(Owner, args);
		if (outError) outError->clear();
		return true;
	}

	static unsigned long long SaturatingAdd(
		unsigned long long left,
		unsigned long long right) noexcept
	{
		const auto maximum = (std::numeric_limits<unsigned long long>::max)();
		return right > maximum - left ? maximum : left + right;
	}

	static bool SameAnimationTarget(
		const RuntimeAnimation& left,
		const RuntimeAnimation& right) noexcept
	{
		if (left.Target != right.Target
			|| !EqualName(left.PropertyName, right.PropertyName)) return false;
		const auto leftPath = ObjectPathCanonical(left.ObjectPath);
		const auto rightPath = ObjectPathCanonical(right.ObjectPath);
		return leftPath.empty() || rightPath.empty()
			? leftPath.empty() && rightPath.empty()
			: EqualName(leftPath, rightPath);
	}

	static bool StateControlsProperty(
		const RuntimeState& state,
		const PropertyKey& key) noexcept
	{
		return std::any_of(state.Setters.begin(), state.Setters.end(),
			[&](const auto& setter)
			{
				return setter.Target == key.Target
					&& EqualName(setter.PropertyName, key.PropertyName);
			}) || std::any_of(state.Animations.begin(), state.Animations.end(),
			[&](const auto& animation)
			{
				return animation.Target == key.Target
					&& EqualName(animation.PropertyName, key.PropertyName);
			});
	}

	bool TryReadAnimationValue(
		const RuntimeAnimation& animation,
		BindingValue& output) const
	{
		if (!animation.Target || !animation.Metadata) return false;
		BindingValue root;
		if (!animation.Metadata->TryGet(*animation.Target, root)) return false;
		if (!animation.ObjectPath)
		{
			output = std::move(root);
			return true;
		}
		return TryReadObjectPathMember(
			root, *animation.ObjectPath, output);
	}

	bool TryReadBaseAnimationValue(
		const RuntimeAnimation& animation,
		BindingValue& output)
	{
		if (!animation.Target || !animation.Metadata) return false;
		const bool previousApplying = Applying;
		Applying = true;
		BindingValue animationValue;
		const bool hadAnimation = animation.Target->TryGetPropertyValue(
			animation.PropertyName, ControlPropertyValueSource::Animation,
			animationValue);
		if (hadAnimation && !animation.Target->ClearPropertyValue(
			animation.PropertyName,
			ControlPropertyValueSource::Animation))
		{
			Applying = previousApplying;
			return false;
		}
		BindingValue visualStateValue;
		const bool hadVisualState = animation.Target->TryGetPropertyValue(
			animation.PropertyName, ControlPropertyValueSource::VisualState,
			visualStateValue);
		if (hadVisualState && !animation.Target->ClearPropertyValue(
			animation.PropertyName,
			ControlPropertyValueSource::VisualState))
		{
			if (hadAnimation)
				(void)animation.Target->TrySetPropertyValue(
					animation.PropertyName, animationValue,
					ControlPropertyValueSource::Animation);
			Applying = previousApplying;
			return false;
		}
		BindingValue root;
		const bool read = animation.Metadata->TryGet(*animation.Target, root);
		const bool restoredVisualState = !hadVisualState
			|| animation.Target->TrySetPropertyValue(
			animation.PropertyName, visualStateValue,
			ControlPropertyValueSource::VisualState);
		const bool restoredAnimation = !hadAnimation
			|| animation.Target->TrySetPropertyValue(
				animation.PropertyName, animationValue,
				ControlPropertyValueSource::Animation);
		Applying = previousApplying;
		if (!read || !restoredVisualState || !restoredAnimation) return false;
		if (!animation.ObjectPath)
		{
			output = std::move(root);
			return true;
		}
		return TryReadObjectPathMember(
			root, *animation.ObjectPath, output);
	}

	static bool EnteringAnimationValue(
		const RuntimeAnimation& animation,
		const BindingValue& current,
		BindingValue& output)
	{
		if (!animation.KeyFrames.empty())
		{
			output = animation.KeyFrames.front().Value;
			if (animation.IsAdditive)
				return AddAnimationValues(animation, output, current, output);
			return true;
		}
		if (animation.From)
		{
			output = *animation.From;
			if (animation.IsAdditive && (animation.To || animation.By))
				return AddAnimationValues(animation, output, current, output);
			return true;
		}
		if (animation.To)
		{
			output = *animation.To;
			return true;
		}
		output = current;
		return true;
	}

	static ActiveAnimation MakeActiveAnimation(
		size_t groupIndex,
		const RuntimeAnimation& animation,
		BindingValue base,
		BindingValue foundation,
		BindingValue from,
		BindingValue to,
		unsigned long long startTick)
	{
		return {
			groupIndex, animation.Target, animation.Metadata,
			animation.PropertyName, animation.Kind,
			std::move(base), std::move(foundation),
			std::move(from), std::move(to),
			animation.KeyFrames,
			animation.IsCumulative,
			animation.ObjectPath, startTick,
			animation.BeginTimeMilliseconds,
			animation.DurationMilliseconds,
			animation.RepeatBehavior, animation.RepeatCount,
			animation.RepeatDurationMilliseconds,
			animation.AutoReverse, animation.FillBehavior,
			animation.SpeedRatio, animation.AccelerationRatio,
			animation.DecelerationRatio,
			animation.RestoreBaseOnStop,
			animation.Easing, animation.EasingMode };
	}

	const RuntimeTransition* FindTransition(
		const RuntimeGroup& group,
		std::optional<size_t> fromState,
		size_t toState) const noexcept
	{
		const RuntimeTransition* best = nullptr;
		const RuntimeTransition* fallback = nullptr;
		int bestScore = -1;
		for (const auto& transition : group.Transitions)
		{
			if (!transition.FromState && !transition.ToState)
			{
				if (!fallback) fallback = &transition;
				continue;
			}
			int score = -1;
			if (transition.FromState == fromState) ++score;
			else if (transition.FromState) continue;
			if (transition.ToState && *transition.ToState == toState) score += 2;
			else if (transition.ToState) continue;
			if (score > bestScore)
			{
				bestScore = score;
				best = &transition;
			}
		}
		return best ? best : fallback;
	}

	void ClearTransitionOnlyProperties(
		const std::vector<PropertyKey>& properties,
		const RuntimeState& state) noexcept
	{
		for (const auto& key : properties)
			if (key.Target && !StateControlsProperty(state, key))
				(void)key.Target->ClearPropertyValue(
					key.PropertyName, ControlPropertyValueSource::VisualState);
	}

	bool GoTo(
		size_t groupIndex,
		size_t stateIndex,
		bool useTransitions,
		std::wstring* outError)
	{
		if (groupIndex >= Groups.size()
			|| stateIndex >= Groups[groupIndex].States.size())
		{
			if (outError) *outError = L"视觉状态索引无效。";
			return false;
		}
		auto& group = Groups[groupIndex];
		const auto logicalCurrent = group.Pending
			? std::optional<size_t>(group.Pending->TargetState)
			: group.CurrentState;
		if (logicalCurrent && *logicalCurrent == stateIndex
			&& (!group.Pending || useTransitions))
		{
			if (outError) outError->clear();
			return true;
		}
		const auto now = ::GetTickCount64();
		const auto* transition = useTransitions
			&& Owner->AreSystemAnimationsEnabled()
			? FindTransition(group, logicalCurrent, stateIndex) : nullptr;
		unsigned long long totalDuration = transition
			? transition->GeneratedDurationMilliseconds : 0;
		if (transition)
			for (const auto& animation : transition->Animations)
				totalDuration = (std::max)(totalDuration,
					SaturatingAdd(animation.BeginTimeMilliseconds,
						TimelineActiveDurationMilliseconds(animation)));

		if (!transition || totalDuration == 0)
		{
			std::vector<PropertyKey> oldTransitionProperties;
			const bool force = group.Pending.has_value();
			if (group.Pending)
				oldTransitionProperties = std::move(group.Pending->Properties);
			group.Pending.reset();
			ActiveAnimations.erase(std::remove_if(
				ActiveAnimations.begin(), ActiveAnimations.end(),
				[&](const auto& animation)
				{ return !animation.IsEventStoryboard
					&& animation.GroupIndex == groupIndex; }),
				ActiveAnimations.end());
			if (!GoToImmediate(
				groupIndex, stateIndex, outError, std::nullopt, force)) return false;
			ClearTransitionOnlyProperties(
				oldTransitionProperties, group.States[stateIndex]);
			return true;
		}

		const RuntimeState* fromState = logicalCurrent
			&& *logicalCurrent < group.States.size()
			? &group.States[*logicalCurrent] : nullptr;
		const auto& toState = group.States[stateIndex];
		std::vector<ActiveAnimation> pendingAnimations;
		std::vector<PropertyKey> pendingProperties;
		auto addProperty = [&](const RuntimeAnimation& animation)
		{
			PropertyKey key{ animation.Target, animation.PropertyName };
			if (std::none_of(pendingProperties.begin(), pendingProperties.end(),
				[&](const auto& existing) { return SameProperty(existing, key); }))
				pendingProperties.push_back(std::move(key));
		};
		auto explicitlyControls = [&](const RuntimeAnimation& candidate)
		{
			return std::any_of(transition->Animations.begin(),
				transition->Animations.end(), [&](const auto& explicitAnimation)
				{ return SameAnimationTarget(candidate, explicitAnimation); });
		};
		Applying = true;
		auto addObjectBaseHold = [&](const RuntimeAnimation& animation)
		{
			BindingValue current;
			BindingValue base;
			if (!TryReadAnimationValue(animation, current)
				|| !TryReadBaseAnimationValue(animation, base)) return false;
			auto generated = animation;
			generated.From.reset();
			generated.To.reset();
			generated.By.reset();
			generated.IsAdditive = false;
			generated.IsCumulative = false;
			generated.KeyFrames = { DeclarativeAnimationKeyFrame{
				DeclarativeKeyFrameKind::Discrete, 0, base } };
			generated.BeginTimeMilliseconds = 0;
			generated.DurationMilliseconds = totalDuration;
			generated.RepeatBehavior = DeclarativeRepeatBehaviorKind::Count;
			generated.RepeatCount = 1.0;
			generated.RepeatDurationMilliseconds = 0;
			generated.AutoReverse = false;
			generated.FillBehavior = DeclarativeTimelineFillBehavior::HoldEnd;
			generated.SpeedRatio = 1.0;
			generated.AccelerationRatio = 0.0;
			generated.DecelerationRatio = 0.0;
			generated.RestoreBaseOnStop = true;
			generated.Easing = DeclarativeEasingKind::Linear;
			generated.EasingMode = DeclarativeEasingMode::EaseOut;
			pendingAnimations.push_back(MakeActiveAnimation(
				groupIndex, generated, base, BindingValue{},
				std::move(current), std::move(base), now));
			addProperty(generated);
			return true;
		};
		for (const auto& animation : toState.Animations)
			if (animation.Kind == DeclarativeAnimationKind::Object
				&& !explicitlyControls(animation)
				&& !addObjectBaseHold(animation))
			{
				Applying = false;
				if (outError) *outError = L"VisualTransition 无法释放 Object 动画基础值："
					+ animation.PropertyName;
				return false;
			}
		if (fromState)
			for (const auto& animation : fromState->Animations)
			{
				if (animation.Kind != DeclarativeAnimationKind::Object
					|| explicitlyControls(animation)
					|| std::any_of(toState.Animations.begin(),
						toState.Animations.end(), [&](const auto& nextAnimation)
						{ return SameAnimationTarget(animation, nextAnimation); }))
					continue;
				if (!addObjectBaseHold(animation))
				{
					Applying = false;
					if (outError) *outError = L"VisualTransition 无法释放 Object 动画基础值："
						+ animation.PropertyName;
					return false;
				}
			}
		if (transition->GeneratedDurationMilliseconds > 0)
		{
			for (const auto& animation : toState.Animations)
			{
				if (animation.Kind == DeclarativeAnimationKind::Object
					|| explicitlyControls(animation)) continue;
				BindingValue from;
				if (!TryReadAnimationValue(animation, from))
				{
					Applying = false;
					if (outError) *outError = L"VisualTransition 无法读取进入动画起始值："
						+ animation.PropertyName;
					return false;
				}
				auto generated = animation;
				generated.From = from;
				BindingValue to;
				if (!EnteringAnimationValue(animation, from, to))
				{
					Applying = false;
					if (outError) *outError = L"VisualTransition 无法合成进入动画值："
						+ animation.PropertyName;
					return false;
				}
				generated.To = to;
				generated.By.reset();
				generated.IsAdditive = false;
				generated.IsCumulative = false;
				generated.KeyFrames.clear();
				generated.BeginTimeMilliseconds = 0;
				generated.DurationMilliseconds =
					transition->GeneratedDurationMilliseconds;
				generated.RepeatBehavior =
					DeclarativeRepeatBehaviorKind::Count;
				generated.RepeatCount = 1.0;
				generated.RepeatDurationMilliseconds = 0;
				generated.AutoReverse = false;
				generated.FillBehavior =
					DeclarativeTimelineFillBehavior::HoldEnd;
				generated.SpeedRatio = 1.0;
				generated.AccelerationRatio = 0.0;
				generated.DecelerationRatio = 0.0;
				generated.RestoreBaseOnStop = true;
				generated.Easing = transition->GeneratedEasing;
				generated.EasingMode = transition->GeneratedEasingMode;
				BindingValue base = from;
				BindingValue foundation = ZeroAnimationValue(generated);
				pendingAnimations.push_back(MakeActiveAnimation(
					groupIndex, generated, std::move(base),
					std::move(foundation),
					std::move(from), std::move(to), now));
				addProperty(generated);
			}
			if (fromState)
				for (const auto& animation : fromState->Animations)
				{
					if (animation.Kind == DeclarativeAnimationKind::Object
						|| explicitlyControls(animation)
						|| std::any_of(toState.Animations.begin(),
							toState.Animations.end(), [&](const auto& nextAnimation)
							{ return SameAnimationTarget(animation, nextAnimation); }))
						continue;
					BindingValue from;
					BindingValue to;
					if (!TryReadAnimationValue(animation, from)
						|| !TryReadBaseAnimationValue(animation, to))
					{
						Applying = false;
						if (outError) *outError = L"VisualTransition 无法生成退出动画："
							+ animation.PropertyName;
						return false;
					}
					auto generated = animation;
					generated.From = from;
					generated.To = to;
					generated.By.reset();
					generated.IsAdditive = false;
					generated.IsCumulative = false;
					generated.KeyFrames.clear();
					generated.BeginTimeMilliseconds = 0;
					generated.DurationMilliseconds =
						transition->GeneratedDurationMilliseconds;
					generated.RepeatBehavior =
						DeclarativeRepeatBehaviorKind::Count;
					generated.RepeatCount = 1.0;
					generated.RepeatDurationMilliseconds = 0;
					generated.AutoReverse = false;
					generated.FillBehavior =
						DeclarativeTimelineFillBehavior::HoldEnd;
					generated.SpeedRatio = 1.0;
					generated.AccelerationRatio = 0.0;
					generated.DecelerationRatio = 0.0;
					generated.RestoreBaseOnStop = true;
					generated.Easing = transition->GeneratedEasing;
					generated.EasingMode = transition->GeneratedEasingMode;
					BindingValue base = from;
					BindingValue foundation = ZeroAnimationValue(generated);
					pendingAnimations.push_back(MakeActiveAnimation(
						groupIndex, generated, std::move(base),
						std::move(foundation),
						std::move(from), std::move(to), now));
					addProperty(generated);
				}
		}
		for (const auto& animation : transition->Animations)
		{
			BindingValue base;
			if (!TryReadAnimationValue(animation, base))
			{
				Applying = false;
				if (outError) *outError = L"VisualTransition Storyboard 无法捕获基础值："
					+ animation.PropertyName;
				return false;
			}
			BindingValue from;
			BindingValue to;
			BindingValue foundation;
			if (!ResolveAnimationEndpoints(
				animation, base, base, from, to, foundation))
			{
				Applying = false;
				if (outError) *outError = L"VisualTransition Storyboard 无法解析 From/To/By："
					+ animation.PropertyName;
				return false;
			}
			pendingAnimations.push_back(MakeActiveAnimation(
				groupIndex, animation, std::move(base),
				std::move(foundation),
				std::move(from), std::move(to), now));
			addProperty(animation);
		}

		std::vector<PropertyKey> oldTransitionProperties;
		if (group.Pending)
			oldTransitionProperties = group.Pending->Properties;
		std::vector<PropertyKey> changedProperties = oldTransitionProperties;
		for (const auto& key : pendingProperties)
			if (std::none_of(changedProperties.begin(), changedProperties.end(),
				[&](const auto& existing) { return SameProperty(existing, key); }))
				changedProperties.push_back(key);
		std::vector<PropertySnapshot> snapshots;
		snapshots.reserve(changedProperties.size());
		for (const auto& key : changedProperties)
		{
			PropertySnapshot snapshot;
			snapshot.Key = key;
			BindingValue value;
			if (key.Target && key.Target->TryGetPropertyValue(
				key.PropertyName, ControlPropertyValueSource::VisualState, value))
				snapshot.Value = std::move(value);
			snapshots.push_back(std::move(snapshot));
		}
		for (const auto& key : oldTransitionProperties)
			if (key.Target && key.Target->HasPropertyValue(
				key.PropertyName, ControlPropertyValueSource::VisualState))
				(void)key.Target->ClearPropertyValue(
					key.PropertyName, ControlPropertyValueSource::VisualState);
		std::vector<AnimationFrameValue> initialValues;
		initialValues.reserve(pendingAnimations.size());
		for (const auto& animation : pendingAnimations)
		{
			BindingValue value;
			if (animation.BeginTimeMilliseconds > 0
				|| (TimelineActiveDurationMilliseconds(animation) == 0
					&& animation.FillBehavior
						== DeclarativeTimelineFillBehavior::Stop))
				value = animation.Base;
			else if (!Interpolate(animation, 0, value))
			{
				(void)RestoreSnapshots(snapshots);
				Applying = false;
				if (outError) *outError = L"VisualTransition 初始帧无效。";
				return false;
			}
			initialValues.push_back({ &animation, std::move(value) });
		}
		if (!ApplyAnimationFrame(initialValues))
		{
			(void)RestoreSnapshots(snapshots);
			Applying = false;
			if (outError) *outError = L"VisualTransition 无法事务性应用。";
			return false;
		}
		const auto startTick = ::GetTickCount64();
		for (auto& animation : pendingAnimations)
			animation.StartTick = startTick;
		std::vector<const ActiveAnimation*> initiallyStopped;
		for (const auto& animation : pendingAnimations)
			if (animation.FillBehavior
				== DeclarativeTimelineFillBehavior::Stop
				&& animation.BeginTimeMilliseconds == 0
				&& TimelineActiveDurationMilliseconds(animation) == 0)
				initiallyStopped.push_back(&animation);
		ReleaseStoppedAnimationValues(
			initiallyStopped, pendingAnimations, startTick);
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](const auto& animation)
			{ return !animation.IsEventStoryboard
				&& animation.GroupIndex == groupIndex; }),
			ActiveAnimations.end());
		for (auto& animation : pendingAnimations)
			ActiveAnimations.push_back(std::move(animation));
		group.Pending = PendingTransition{
			stateIndex, SaturatingAdd(startTick, totalDuration),
			std::move(pendingProperties) };
		Applying = false;
		Owner->InvalidateVisual();
		if (outError) outError->clear();
		return true;
	}

	bool EvaluateGroup(size_t groupIndex, std::wstring* outError = nullptr)
	{
		if (groupIndex >= Groups.size()) return false;
		return GoTo(groupIndex, EvaluateState(Groups[groupIndex]), true, outError);
	}

	void OnHostPropertyChanged(const ControlPropertyChangedEventArgs& args)
	{
		if (Applying) return;
		for (size_t index = 0; index < Groups.size(); ++index)
			if (ContainsName(Groups[index].ConditionProperties, args.PropertyName))
				(void)EvaluateGroup(index);
	}

	bool ApplyRetainedAnimationFrame(unsigned long long nowMilliseconds)
	{
		std::vector<AnimationFrameValue> values;
		values.reserve(ActiveAnimations.size());
		for (const auto& animation : ActiveAnimations)
		{
			const auto clockTick = animation.Paused
				? animation.PauseTick : nowMilliseconds;
			const auto elapsed = clockTick >= animation.StartTick
				? clockTick - animation.StartTick : 0;
			if (elapsed < animation.BeginTimeMilliseconds) continue;
			const auto activeDuration =
				TimelineActiveDurationMilliseconds(animation);
			BindingValue value;
			if (!Interpolate(animation, animation.Completed
				? activeDuration
				: elapsed - animation.BeginTimeMilliseconds, value)) return false;
			values.push_back({ &animation, std::move(value) });
		}
		return ApplyAnimationFrame(values);
	}

	bool BeginEventStoryboard(size_t storyboardIndex, std::wstring* outError)
	{
		if (storyboardIndex >= EventStoryboards.size())
		{
			if (outError) *outError = L"BeginStoryboard 索引无效。";
			return false;
		}
		const auto& storyboard = EventStoryboards[storyboardIndex];
		std::vector<PropertySnapshot> snapshots;
		for (const auto& animation : storyboard.Animations)
		{
			PropertyKey key{ animation.Target, animation.PropertyName };
			if (std::any_of(snapshots.begin(), snapshots.end(),
				[&](const auto& existing)
				{ return SameProperty(existing.Key, key); })) continue;
			PropertySnapshot snapshot;
			snapshot.Key = key;
			snapshot.Source = ControlPropertyValueSource::Animation;
			BindingValue value;
			if (key.Target && key.Target->TryGetPropertyValue(
				key.PropertyName, ControlPropertyValueSource::Animation, value))
				snapshot.Value = std::move(value);
			snapshots.push_back(std::move(snapshot));
		}

		std::vector<ActiveAnimation> pending;
		pending.reserve(storyboard.Animations.size());
		for (const auto& animation : storyboard.Animations)
		{
			BindingValue current;
			if (!TryReadAnimationValue(animation, current))
			{
				if (outError) *outError = L"BeginStoryboard 无法捕获当前值："
					+ animation.PropertyName;
				return false;
			}
			BindingValue from;
			BindingValue to;
			BindingValue foundation;
			if (!ResolveAnimationEndpoints(animation, current, current,
				from, to, foundation))
			{
				if (outError) *outError = L"BeginStoryboard 无法解析 From/To/By："
					+ animation.PropertyName;
				return false;
			}
			auto active = MakeActiveAnimation(storyboardIndex, animation,
				current, std::move(foundation), std::move(from), std::move(to), 0);
			active.IsEventStoryboard = true;
			pending.push_back(std::move(active));
		}

		const bool animationsEnabled = Owner->AreSystemAnimationsEnabled();
		Applying = true;
		std::vector<AnimationFrameValue> initialValues;
		initialValues.reserve(pending.size());
		bool success = true;
		for (const auto& animation : pending)
		{
			const auto activeDuration =
				TimelineActiveDurationMilliseconds(animation);
			const bool active = animationsEnabled
				&& (animation.BeginTimeMilliseconds > 0 || activeDuration > 0);
			BindingValue value;
			if (active && animation.BeginTimeMilliseconds > 0)
				value = animation.Base;
			else if (!active && animation.FillBehavior
				== DeclarativeTimelineFillBehavior::Stop)
				value = animation.Base;
			else if (!Interpolate(animation,
				active ? 0 : activeDuration, value))
			{
				success = false;
				break;
			}
			initialValues.push_back({ &animation, std::move(value) });
		}
		if (success) success = ApplyAnimationFrame(initialValues);
		if (!success)
		{
			(void)RestoreSnapshots(snapshots);
			Applying = false;
			if (outError) *outError = L"BeginStoryboard 初始帧无法事务性应用。";
			return false;
		}

		const auto startTick = ::GetTickCount64();
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](const auto& animation)
			{
				return animation.IsEventStoryboard
					&& animation.GroupIndex == storyboardIndex;
			}), ActiveAnimations.end());
		for (auto& animation : pending)
			animation.StartTick = startTick;
		std::vector<ActiveAnimation> releaseContext = ActiveAnimations;
		const auto pendingOffset = releaseContext.size();
		for (const auto& animation : pending)
			releaseContext.push_back(animation);
		std::vector<const ActiveAnimation*> initiallyStopped;
		for (size_t index = 0; index < pending.size(); ++index)
		{
			const auto& animation = pending[index];
			const auto activeDuration =
				TimelineActiveDurationMilliseconds(animation);
			const bool active = animationsEnabled
				&& (animation.BeginTimeMilliseconds > 0 || activeDuration > 0);
			if (!active && animation.FillBehavior
				== DeclarativeTimelineFillBehavior::Stop)
				initiallyStopped.push_back(
					&releaseContext[pendingOffset + index]);
		}
		ReleaseStoppedAnimationValues(
			initiallyStopped, releaseContext, startTick);
		for (auto& animation : pending)
		{
			const auto activeDuration =
				TimelineActiveDurationMilliseconds(animation);
			const bool active = animationsEnabled
				&& (animation.BeginTimeMilliseconds > 0 || activeDuration > 0);
			if (active)
				ActiveAnimations.push_back(std::move(animation));
			else if (animation.FillBehavior
				== DeclarativeTimelineFillBehavior::HoldEnd)
			{
				animation.Completed = true;
				ActiveAnimations.push_back(std::move(animation));
			}
		}
		if (!initiallyStopped.empty())
			(void)ApplyRetainedAnimationFrame(startTick);
		Applying = false;
		if (HasActiveAnimations() || !initiallyStopped.empty())
			Owner->InvalidateVisual();
		if (outError) outError->clear();
		return true;
	}

	bool PauseEventStoryboard(size_t storyboardIndex)
	{
		const auto now = ::GetTickCount64();
		bool changed = false;
		for (auto& animation : ActiveAnimations)
			if (animation.IsEventStoryboard
				&& animation.GroupIndex == storyboardIndex
				&& !animation.Completed && !animation.Paused)
			{
				animation.Paused = true;
				animation.PauseTick = now;
				changed = true;
			}
		return changed;
	}

	bool ResumeEventStoryboard(size_t storyboardIndex)
	{
		const auto now = ::GetTickCount64();
		bool changed = false;
		for (auto& animation : ActiveAnimations)
			if (animation.IsEventStoryboard
				&& animation.GroupIndex == storyboardIndex
				&& !animation.Completed && animation.Paused)
			{
				const auto pausedFor = now >= animation.PauseTick
					? now - animation.PauseTick : 0;
				animation.StartTick = SaturatingAdd(
					animation.StartTick, pausedFor);
				animation.Paused = false;
				animation.PauseTick = 0;
				changed = true;
			}
		if (changed) Owner->InvalidateVisual();
		return changed;
	}

	bool StopEventStoryboard(size_t storyboardIndex)
	{
		const auto now = ::GetTickCount64();
		std::vector<const ActiveAnimation*> stopping;
		for (const auto& animation : ActiveAnimations)
			if (animation.IsEventStoryboard
				&& animation.GroupIndex == storyboardIndex)
				stopping.push_back(&animation);
		if (stopping.empty()) return false;
		Applying = true;
		ReleaseStoppedAnimationValues(stopping, ActiveAnimations, now);
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[&](const auto& animation)
			{
				return animation.IsEventStoryboard
					&& animation.GroupIndex == storyboardIndex;
			}), ActiveAnimations.end());
		(void)ApplyRetainedAnimationFrame(now);
		Applying = false;
		Owner->InvalidateVisual();
		return true;
	}

	void ExecuteEventTriggerAction(const RuntimeEventTriggerAction& action)
	{
		switch (action.Kind)
		{
		case DeclarativeStoryboardActionKind::Begin:
			(void)BeginEventStoryboard(action.StoryboardIndex, nullptr);
			break;
		case DeclarativeStoryboardActionKind::Pause:
			(void)PauseEventStoryboard(action.StoryboardIndex);
			break;
		case DeclarativeStoryboardActionKind::Resume:
			(void)ResumeEventStoryboard(action.StoryboardIndex);
			break;
		case DeclarativeStoryboardActionKind::Stop:
			(void)StopEventStoryboard(action.StoryboardIndex);
			break;
		}
	}

	bool ExecuteStyleTriggerActions(
		const std::vector<RuntimeEventTriggerAction>& actions,
		std::wstring* outError)
	{
		for (const auto& action : actions)
		{
			switch (action.Kind)
			{
			case DeclarativeStoryboardActionKind::Begin:
				if (!BeginEventStoryboard(action.StoryboardIndex, outError))
					return false;
				break;
			case DeclarativeStoryboardActionKind::Pause:
				(void)PauseEventStoryboard(action.StoryboardIndex);
				break;
			case DeclarativeStoryboardActionKind::Resume:
				(void)ResumeEventStoryboard(action.StoryboardIndex);
				break;
			case DeclarativeStoryboardActionKind::Stop:
				(void)StopEventStoryboard(action.StoryboardIndex);
				break;
			}
		}
		if (outError) outError->clear();
		return true;
	}

	size_t AllocateStyleStoryboardIndex()
	{
		if (!FreeStyleStoryboardIndices.empty())
		{
			const auto index = FreeStyleStoryboardIndices.back();
			FreeStyleStoryboardIndices.pop_back();
			return index;
		}
		EventStoryboards.emplace_back();
		return EventStoryboards.size() - 1;
	}

	void ReleaseStyleStoryboardIndex(size_t index)
	{
		if (index >= EventStoryboards.size()) return;
		(void)StopEventStoryboard(index);
		EventStoryboards[index] = {};
		if (std::find(FreeStyleStoryboardIndices.begin(),
			FreeStyleStoryboardIndices.end(), index)
			== FreeStyleStoryboardIndices.end())
			FreeStyleStoryboardIndices.push_back(index);
	}

	bool CompileStyleTriggerScope(
		const ResolvedControlStyleTrigger& source,
		RuntimeStyleTriggerScope& scope,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = L"Style DataTrigger：" + std::move(message);
			return false;
		};
		std::vector<RuntimeEventStoryboard> storyboards;
		std::vector<RuntimeEventTriggerAction> enterActions;
		std::vector<RuntimeEventTriggerAction> exitActions;
		std::vector<std::wstring> beginNames;
		auto compileActions = [&](
			const std::vector<DeclarativeEventTriggerActionDefinition>& definitions,
			std::vector<RuntimeEventTriggerAction>& output) -> bool
		{
			for (const auto& definition : definitions)
			{
				RuntimeEventTriggerAction action;
				action.Kind = definition.Kind;
				if (definition.Kind == DeclarativeStoryboardActionKind::Begin)
				{
					if (!definition.StoryboardName.empty()
						&& ContainsName(beginNames, definition.StoryboardName))
						return fail(L"BeginStoryboard x:Name 重复："
							+ definition.StoryboardName);
					if (definition.Animations.empty())
						return fail(L"BeginStoryboard 的 Storyboard 不能为空。");
					RuntimeEventStoryboard storyboard;
					storyboard.Name = definition.StoryboardName;
					storyboard.IsStyleStoryboard = true;
					struct PropertyOwnership
					{
						PropertyKey Root;
						bool Exclusive = false;
						std::vector<std::wstring> Paths;
					};
					std::vector<PropertyOwnership> properties;
					for (const auto& sourceAnimation : definition.Animations)
					{
						if (!sourceAnimation.TargetName.empty())
							return fail(L"Style Storyboard 不支持 TargetName："
								+ sourceAnimation.TargetName);
						RuntimeAnimation animation;
						if (!TryBuildAnimation(sourceAnimation, animation,
							L"Style BeginStoryboard", outError)) return false;
						animation.RestoreBaseOnStop = true;
						PropertyKey key{ animation.Target, animation.PropertyName };
						const auto path = std::wstring(
							ObjectPathCanonical(animation.ObjectPath));
						auto owner = std::find_if(properties.begin(), properties.end(),
							[&](const auto& existing)
							{ return SameProperty(existing.Root, key); });
						if (owner != properties.end())
						{
							if (path.empty() || owner->Exclusive
								|| ContainsName(owner->Paths, path))
								return fail(L"BeginStoryboard 目标重复："
									+ sourceAnimation.PropertyName);
							owner->Paths.push_back(path);
						}
						else
						{
							PropertyOwnership ownership;
							ownership.Root = key;
							ownership.Exclusive = path.empty();
							if (!path.empty()) ownership.Paths.push_back(path);
							properties.push_back(std::move(ownership));
						}
						storyboard.Animations.push_back(std::move(animation));
					}
					action.StoryboardIndex = storyboards.size();
					storyboards.push_back(std::move(storyboard));
					if (!definition.StoryboardName.empty())
						beginNames.push_back(definition.StoryboardName);
				}
				else
				{
					if (definition.StoryboardName.empty())
						return fail(L"Storyboard 控制动作缺少 BeginStoryboardName。");
					action.PendingStoryboardName = definition.StoryboardName;
				}
				output.push_back(std::move(action));
			}
			return true;
		};
		if (!compileActions(source.EnterActions, enterActions)
			|| !compileActions(source.ExitActions, exitActions)) return false;
		auto resolveReferences = [&](std::vector<RuntimeEventTriggerAction>& actions)
		{
			for (auto& action : actions)
			{
				if (action.Kind == DeclarativeStoryboardActionKind::Begin) continue;
				const auto found = std::find_if(storyboards.begin(), storyboards.end(),
					[&](const auto& storyboard)
					{ return EqualName(storyboard.Name,
						action.PendingStoryboardName); });
				if (found == storyboards.end())
					return fail(L"Storyboard 控制动作找不到 BeginStoryboard："
						+ action.PendingStoryboardName);
				action.StoryboardIndex = static_cast<size_t>(
					std::distance(storyboards.begin(), found));
				action.PendingStoryboardName.clear();
			}
			return true;
		};
		if (!resolveReferences(enterActions)
			|| !resolveReferences(exitActions)) return false;

		auto indices = scope.StoryboardIndices;
		while (indices.size() < storyboards.size())
			indices.push_back(AllocateStyleStoryboardIndex());
		for (size_t index = 0; index < storyboards.size(); ++index)
			EventStoryboards[indices[index]] = std::move(storyboards[index]);
		for (size_t index = storyboards.size(); index < indices.size(); ++index)
			ReleaseStyleStoryboardIndex(indices[index]);
		indices.resize(storyboards.size());
		auto mapIndices = [&](std::vector<RuntimeEventTriggerAction>& actions)
		{
			for (auto& action : actions)
				if (action.StoryboardIndex < indices.size())
					action.StoryboardIndex = indices[action.StoryboardIndex];
		};
		mapIndices(enterActions);
		mapIndices(exitActions);
		scope.StoryboardIndices = std::move(indices);
		scope.EnterActions = std::move(enterActions);
		scope.ExitActions = std::move(exitActions);
		if (outError) outError->clear();
		return true;
	}

	void RemoveStyleTriggerScope(size_t index)
	{
		if (index >= StyleTriggerScopes.size()) return;
		for (const auto storyboardIndex
			: StyleTriggerScopes[index].StoryboardIndices)
			ReleaseStyleStoryboardIndex(storyboardIndex);
		StyleTriggerScopes.erase(StyleTriggerScopes.begin() + index);
	}

	bool SynchronizeStyleTriggerActions(
		ControlPropertyValueSource source,
		const ControlStyleSheet* sheet,
		const std::vector<ResolvedControlStyleTrigger>& triggers,
		std::wstring* outError)
	{
		bool success = true;
		for (const auto& trigger : triggers)
		{
			auto found = std::find_if(StyleTriggerScopes.begin(),
				StyleTriggerScopes.end(), [&](const auto& existing)
				{
					return existing.Source == source && existing.Sheet == sheet
						&& existing.RuleId == trigger.RuleId;
				});
			if (found == StyleTriggerScopes.end())
			{
				RuntimeStyleTriggerScope scope;
				scope.Source = source;
				scope.Sheet = sheet;
				scope.RuleId = trigger.RuleId;
				if (!CompileStyleTriggerScope(trigger, scope, outError))
				{
					success = false;
					continue;
				}
				StyleTriggerScopes.push_back(std::move(scope));
				found = std::prev(StyleTriggerScopes.end());
			}
			else if (!CompileStyleTriggerScope(trigger, *found, outError))
			{
				success = false;
				continue;
			}
			if (found->Active == trigger.IsActive) continue;
			const bool previous = found->Active;
			found->Active = trigger.IsActive;
			if (!ExecuteStyleTriggerActions(
				trigger.IsActive ? found->EnterActions : found->ExitActions,
				outError))
			{
				found->Active = previous;
				success = false;
			}
		}
		for (size_t index = StyleTriggerScopes.size(); index-- > 0;)
		{
			const auto& scope = StyleTriggerScopes[index];
			if (scope.Source != source || scope.Sheet != sheet) continue;
			if (std::none_of(triggers.begin(), triggers.end(),
				[&](const auto& trigger) { return trigger.RuleId == scope.RuleId; }))
				RemoveStyleTriggerScope(index);
		}
		if (success && outError) outError->clear();
		return success;
	}

	void PruneStyleTriggerActions(
		ControlPropertyValueSource source,
		const std::vector<const ControlStyleSheet*>& visibleSheets)
	{
		for (size_t index = StyleTriggerScopes.size(); index-- > 0;)
		{
			const auto& scope = StyleTriggerScopes[index];
			if (scope.Source != source) continue;
			if (std::find(visibleSheets.begin(), visibleSheets.end(), scope.Sheet)
				== visibleSheets.end())
				RemoveStyleTriggerScope(index);
		}
	}

	void ResetFailedDeclarativeInteractionBuild()
	{
		Connections.clear();
		ClearAppliedValues();
		ActiveAnimations.erase(std::remove_if(
			ActiveAnimations.begin(), ActiveAnimations.end(),
			[](const auto& animation) { return !animation.IsEventStoryboard; }),
			ActiveAnimations.end());
		Groups.clear();
		EventTriggers.clear();
		for (auto& storyboard : EventStoryboards)
			if (!storyboard.IsStyleStoryboard) storyboard = {};
		(void)ApplyRetainedAnimationFrame(::GetTickCount64());
		DeclarativeInteractionsDefined = false;
	}

	void OnHostDeclarativeEvent(DeclarativeEventArgs& args)
	{
		if (Applying || args.OriginalSource != Owner
			|| !EqualName(args.OwnerNamespace,
				Owner->GetDeclarativeTypeNamespace())
			|| !EqualName(args.OwnerTypeName,
				Owner->GetDeclarativeTypeName())) return;
		for (size_t groupIndex = 0; groupIndex < Groups.size(); ++groupIndex)
		{
			auto& group = Groups[groupIndex];
			for (size_t stateIndex = 0;
				stateIndex < group.States.size(); ++stateIndex)
				if (ContainsName(
					group.States[stateIndex].EventNames, args.Name))
				{
					(void)GoTo(groupIndex, stateIndex, true, nullptr);
					break;
				}
		}
		for (const auto& trigger : EventTriggers)
			if (EqualName(trigger.EventName, args.Name))
				for (const auto& action : trigger.Actions)
					ExecuteEventTriggerAction(action);
	}

	bool TryBuildAnimation(
		const DeclarativeVisualStateAnimation& sourceAnimation,
		RuntimeAnimation& animation,
		const std::wstring& context,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = context + L"：" + std::move(message);
			return false;
		};
		Control* target = sourceAnimation.TargetName.empty()
			? Owner : Owner->FindDeclarativeTemplatePart(sourceAnimation.TargetName);
		if (!target)
			return fail(L"Storyboard 找不到模板部件："
				+ sourceAnimation.TargetName);
		const auto* metadata = target->FindPropertyMetadata(
			sourceAnimation.PropertyName);
		std::optional<ObjectPathAccessor> objectPath;
		if (!metadata || !metadata->CanWrite()
			|| !AnimationMatchesMetadata(sourceAnimation.Kind, *metadata))
		{
			std::wstring pathError;
			ObjectPathAccessor accessor;
			if (!TryResolveObjectPath(*target, sourceAnimation.PropertyName,
				sourceAnimation.Kind, metadata, accessor, &pathError))
				return fail(pathError + L"：" + sourceAnimation.PropertyName);
			objectPath = std::move(accessor);
		}
		BindingValue convertedScratch;
		auto validTypedAnimationValue = [&](const BindingValue& value)
		{
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Point)
			{
				cui::core::Point point{};
				return value.TryGet(point)
					&& std::isfinite(point.x) && std::isfinite(point.y);
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Vector)
			{
				cui::core::Vector vector{};
				return value.TryGet(vector)
					&& std::isfinite(vector.x) && std::isfinite(vector.y);
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Rect)
			{
				cui::core::Rect rect{};
				return value.TryGet(rect)
					&& std::isfinite(rect.x) && std::isfinite(rect.y)
					&& std::isfinite(rect.width) && std::isfinite(rect.height)
					&& rect.width >= 0.0f && rect.height >= 0.0f;
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Size)
			{
				cui::core::Size size{};
				return value.TryGet(size)
					&& std::isfinite(size.width) && std::isfinite(size.height)
					&& size.width >= 0.0f && size.height >= 0.0f;
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Matrix)
			{
				D2D1_MATRIX_3X2_F matrix{};
				return value.TryGet(matrix) && IsFiniteMatrix(matrix);
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Thickness)
			{
				Thickness thickness;
				return value.TryGet(thickness)
					&& std::isfinite(thickness.Left)
					&& std::isfinite(thickness.Top)
					&& std::isfinite(thickness.Right)
					&& std::isfinite(thickness.Bottom);
			}
			if (sourceAnimation.Kind == DeclarativeAnimationKind::Color)
			{
				D2D1_COLOR_F color{};
				return value.TryGet(color)
					&& std::isfinite(color.r) && std::isfinite(color.g)
					&& std::isfinite(color.b) && std::isfinite(color.a);
			}
			return true;
		};
		auto convertEndpoint = [&](const BindingValue& source,
			BindingValue& output, bool isDelta = false)
		{
			if (!objectPath)
				return metadata->TryConvert(source, convertedScratch)
					&& validTypedAnimationValue(convertedScratch)
					&& metadata->TryCoerce(*target, convertedScratch, output);
			if (sourceAnimation.Kind != DeclarativeAnimationKind::Double)
			{
				if (!validTypedAnimationValue(source)) return false;
				if (const auto* pathGeometry = AsPathGeometryPath(objectPath);
					pathGeometry && sourceAnimation.Kind
						== DeclarativeAnimationKind::Object)
				{
					if (pathGeometry->Member == PathGeometryMember::ArcSweepDirection)
					{
						std::wstring sweep;
						if (!source.TryGet(sweep)
							|| (!EqualName(sweep, L"Clockwise")
								&& !EqualName(sweep, L"Counterclockwise")))
							return false;
					}
					else
					{
						bool flag = false;
						if (!source.TryGet(flag)) return false;
					}
				}
				if (const auto* geometry = AsGeometryPath(objectPath);
					geometry && geometry->Member == GeometryMember::FillRule
					&& sourceAnimation.Kind == DeclarativeAnimationKind::Object)
				{
					std::wstring fillRule;
					if (!source.TryGet(fillRule)
						|| (!EqualName(fillRule, L"EvenOdd")
							&& !EqualName(fillRule, L"Nonzero"))) return false;
				}
				output = source;
				return true;
			}
			double value = 0.0;
			if (!source.TryGetDouble(value) || !std::isfinite(value)
				|| value < -(std::numeric_limits<float>::max)()
				|| value > (std::numeric_limits<float>::max)()) return false;
			if (!isDelta)
			{
				if (const auto* brushPath = AsBrushPath(objectPath))
				{
					if ((brushPath->Member == BrushMember::GradientStopOffset
							|| brushPath->Member == BrushMember::Opacity)
						&& (value < 0.0 || value > 1.0)) return false;
					if ((brushPath->Member == BrushMember::RadiusX
							|| brushPath->Member == BrushMember::RadiusY)
						&& value < 0.0) return false;
				}
				if (const auto* geometryPath = AsGeometryPath(objectPath);
					geometryPath
					&& (geometryPath->Member == GeometryMember::RadiusX
						|| geometryPath->Member == GeometryMember::RadiusY)
					&& value < 0.0) return false;
			}
			output = BindingValue(static_cast<float>(value));
			return true;
		};
		std::optional<BindingValue> coercedFrom;
		std::optional<BindingValue> coercedTo;
		std::optional<BindingValue> coercedBy;
		std::vector<DeclarativeAnimationKeyFrame> keyFrames;
		if (sourceAnimation.KeyFrames.empty())
		{
			if (sourceAnimation.To)
			{
				BindingValue value;
				if (!convertEndpoint(*sourceAnimation.To, value))
					return fail(L"动画 To 无效：" + sourceAnimation.PropertyName);
				coercedTo = std::move(value);
			}
			if (sourceAnimation.From)
			{
				BindingValue value;
				if (!convertEndpoint(*sourceAnimation.From, value))
					return fail(L"动画 From 无效："
						+ sourceAnimation.PropertyName);
				coercedFrom = std::move(value);
			}
			if (sourceAnimation.By)
			{
				BindingValue value;
				if (objectPath)
				{
					if (!convertEndpoint(*sourceAnimation.By, value, true))
						return fail(L"动画 By 无效："
							+ sourceAnimation.PropertyName);
				}
				else if (!metadata->TryConvert(*sourceAnimation.By, value)
					|| !validTypedAnimationValue(value))
					return fail(L"动画 By 无法转换为目标属性类型："
						+ sourceAnimation.PropertyName);
				coercedBy = std::move(value);
			}
		}
		else
		{
			if (sourceAnimation.From || sourceAnimation.To || sourceAnimation.By)
				return fail(L"关键帧动画不能同时声明 From/To/By："
					+ sourceAnimation.PropertyName);
			keyFrames.reserve(sourceAnimation.KeyFrames.size());
			for (auto keyFrame : sourceAnimation.KeyFrames)
			{
				if (sourceAnimation.Kind == DeclarativeAnimationKind::Object
					&& keyFrame.Kind != DeclarativeKeyFrameKind::Discrete)
					return fail(L"ObjectAnimationUsingKeyFrames 只能包含 DiscreteObjectKeyFrame。");
				if (keyFrame.Kind != DeclarativeKeyFrameKind::Discrete
					&& keyFrame.Kind != DeclarativeKeyFrameKind::Linear
					&& keyFrame.Kind != DeclarativeKeyFrameKind::Easing
					&& keyFrame.Kind != DeclarativeKeyFrameKind::Spline)
					return fail(L"关键帧类型无效。");
				if (keyFrame.Kind == DeclarativeKeyFrameKind::Spline
					&& (!std::isfinite(keyFrame.KeySplineX1)
						|| !std::isfinite(keyFrame.KeySplineY1)
						|| !std::isfinite(keyFrame.KeySplineX2)
						|| !std::isfinite(keyFrame.KeySplineY2)
						|| keyFrame.KeySplineX1 < 0.0f
						|| keyFrame.KeySplineX1 > 1.0f
						|| keyFrame.KeySplineY1 < 0.0f
						|| keyFrame.KeySplineY1 > 1.0f
						|| keyFrame.KeySplineX2 < 0.0f
						|| keyFrame.KeySplineX2 > 1.0f
						|| keyFrame.KeySplineY2 < 0.0f
						|| keyFrame.KeySplineY2 > 1.0f))
					return fail(L"KeySpline 控制点必须位于 0..1。");
				BindingValue value;
				if (!convertEndpoint(keyFrame.Value, value))
					return fail(L"关键帧值无效："
						+ sourceAnimation.PropertyName);
				keyFrame.Value = std::move(value);
				keyFrames.push_back(std::move(keyFrame));
			}
			std::stable_sort(keyFrames.begin(), keyFrames.end(),
				[](const auto& left, const auto& right)
				{
					return left.KeyTimeMilliseconds
						< right.KeyTimeMilliseconds;
				});
		}
		animation.Kind = sourceAnimation.Kind;
		animation.Target = target;
		animation.Metadata = metadata;
		animation.PropertyName = metadata->Name();
		animation.ObjectPath = std::move(objectPath);
		animation.From = std::move(coercedFrom);
		animation.To = std::move(coercedTo);
		animation.By = std::move(coercedBy);
		animation.IsAdditive = sourceAnimation.IsAdditive;
		animation.IsCumulative = sourceAnimation.IsCumulative;
		animation.KeyFrames = std::move(keyFrames);
		animation.BeginTimeMilliseconds = sourceAnimation.BeginTimeMilliseconds;
		animation.DurationMilliseconds = sourceAnimation.DurationMilliseconds;
		if (sourceAnimation.Kind == DeclarativeAnimationKind::Object)
		{
			if (sourceAnimation.KeyFrames.empty())
				return fail(L"ObjectAnimationUsingKeyFrames 至少需要一个关键帧。");
			if (sourceAnimation.IsAdditive || sourceAnimation.IsCumulative)
				return fail(L"ObjectAnimationUsingKeyFrames 不支持 IsAdditive/IsCumulative。");
			if (sourceAnimation.Easing != DeclarativeEasingKind::Linear)
				return fail(L"ObjectAnimationUsingKeyFrames 不支持 EasingFunction。");
		}
		if (sourceAnimation.RepeatBehavior
			== DeclarativeRepeatBehaviorKind::Count)
		{
			if (!std::isfinite(sourceAnimation.RepeatCount)
				|| sourceAnimation.RepeatCount <= 0.0)
				return fail(L"RepeatBehavior Count 必须是有限正数。");
		}
		else if (sourceAnimation.RepeatBehavior
			== DeclarativeRepeatBehaviorKind::Duration)
		{
			if (sourceAnimation.RepeatDurationMilliseconds == 0)
				return fail(L"RepeatBehavior Duration 必须大于零。");
		}
		else if (sourceAnimation.RepeatBehavior
			!= DeclarativeRepeatBehaviorKind::Forever)
			return fail(L"RepeatBehavior 类型无效。");
		if (sourceAnimation.FillBehavior
			!= DeclarativeTimelineFillBehavior::HoldEnd
			&& sourceAnimation.FillBehavior
				!= DeclarativeTimelineFillBehavior::Stop)
			return fail(L"FillBehavior 类型无效。");
		if (!std::isfinite(sourceAnimation.SpeedRatio)
			|| sourceAnimation.SpeedRatio <= 0.0)
			return fail(L"SpeedRatio 必须是有限正数。");
		if (!std::isfinite(sourceAnimation.AccelerationRatio)
			|| sourceAnimation.AccelerationRatio < 0.0
			|| sourceAnimation.AccelerationRatio > 1.0)
			return fail(L"AccelerationRatio 必须位于 0..1。");
		if (!std::isfinite(sourceAnimation.DecelerationRatio)
			|| sourceAnimation.DecelerationRatio < 0.0
			|| sourceAnimation.DecelerationRatio > 1.0)
			return fail(L"DecelerationRatio 必须位于 0..1。");
		if (sourceAnimation.AccelerationRatio
			+ sourceAnimation.DecelerationRatio > 1.0)
			return fail(L"AccelerationRatio 与 DecelerationRatio 之和不能超过 1。");
		animation.RepeatBehavior = sourceAnimation.RepeatBehavior;
		animation.RepeatCount = sourceAnimation.RepeatCount;
		animation.RepeatDurationMilliseconds =
			sourceAnimation.RepeatDurationMilliseconds;
		animation.AutoReverse = sourceAnimation.AutoReverse;
		animation.FillBehavior = sourceAnimation.FillBehavior;
		animation.SpeedRatio = sourceAnimation.SpeedRatio;
		animation.AccelerationRatio = sourceAnimation.AccelerationRatio;
		animation.DecelerationRatio = sourceAnimation.DecelerationRatio;
		animation.Easing = sourceAnimation.Easing;
		animation.EasingMode = sourceAnimation.EasingMode;
		if (outError) outError->clear();
		return true;
	}

	bool Build(
		std::vector<DeclarativeVisualStateGroupDefinition> definitions,
		std::vector<DeclarativeEventTriggerDefinition> eventTriggerDefinitions,
		std::wstring* outError)
	{
		auto fail = [&](std::wstring message)
		{
			if (outError) *outError = std::move(message);
			return false;
		};
		if (!Owner || (definitions.empty() && eventTriggerDefinitions.empty()))
			return fail(L"视觉状态组和事件触发器不能同时为空。");
		std::vector<std::pair<PropertyKey, size_t>> groupProperties;
		for (auto& sourceGroup : definitions)
		{
			if (sourceGroup.Name.empty())
				return fail(L"视觉状态组名称不能为空。");
			if (std::any_of(Groups.begin(), Groups.end(), [&](const auto& existing)
				{ return EqualName(existing.Name, sourceGroup.Name); }))
				return fail(L"视觉状态组名称重复：" + sourceGroup.Name);
			if (sourceGroup.States.empty())
				return fail(L"视觉状态组至少需要一个状态：" + sourceGroup.Name);
			RuntimeGroup group;
			group.Name = std::move(sourceGroup.Name);
			std::optional<size_t> fallback;
			std::vector<std::wstring> groupEvents;
			for (auto& sourceState : sourceGroup.States)
			{
				if (sourceState.Name.empty())
					return fail(L"视觉状态名称不能为空。");
				if (std::any_of(group.States.begin(), group.States.end(),
					[&](const auto& existing)
					{ return EqualName(existing.Name, sourceState.Name); }))
					return fail(L"视觉状态名称重复：" + sourceState.Name);
				if (!sourceState.Conditions.empty()
					&& !sourceState.EventNames.empty())
					return fail(L"视觉状态不能同时声明属性和事件触发器："
						+ sourceState.Name);
				RuntimeState state;
				state.Name = std::move(sourceState.Name);
				if (sourceState.Conditions.empty()
					&& sourceState.EventNames.empty())
				{
					if (fallback)
						return fail(L"每个视觉状态组只能有一个无触发器的回退状态："
							+ group.Name);
					fallback = group.States.size();
				}
				std::vector<std::wstring> stateConditions;
				for (auto& sourceCondition : sourceState.Conditions)
				{
					if (sourceCondition.PropertyName.empty()
						|| ContainsName(stateConditions,
							sourceCondition.PropertyName))
						return fail(L"视觉状态条件属性为空或重复：" + state.Name);
					const auto* metadata = Owner->FindPropertyMetadata(
						sourceCondition.PropertyName);
					BindingValue converted;
					BindingValue coerced;
					if (!metadata || !metadata->CanRead()
						|| !metadata->TryConvert(sourceCondition.Value, converted)
						|| !metadata->TryCoerce(*Owner, converted, coerced))
						return fail(L"视觉状态条件属性不存在或值无效："
							+ sourceCondition.PropertyName);
					stateConditions.push_back(metadata->Name());
					if (!ContainsName(group.ConditionProperties, metadata->Name()))
						group.ConditionProperties.push_back(metadata->Name());
					state.Conditions.push_back({ metadata, std::move(coerced) });
				}
				for (auto& eventName : sourceState.EventNames)
				{
					const auto* event = Owner->FindDynamicEvent(eventName);
					if (eventName.empty() || !event
						|| ContainsName(groupEvents, eventName))
						return fail(L"视觉状态事件不存在或在组内重复：" + eventName);
					groupEvents.push_back(event->Name);
					state.EventNames.push_back(event->Name);
				}
				struct StatePropertyOwnership
				{
					PropertyKey Root;
					bool Exclusive = false;
					std::vector<std::wstring> AnimationPaths;
				};
				std::vector<StatePropertyOwnership> stateProperties;
				auto registerControlledProperty = [&](const PropertyKey& key,
					const std::wstring& source,
					const std::wstring& animationPath = {}) -> bool
				{
					auto stateOwner = std::find_if(
						stateProperties.begin(), stateProperties.end(),
						[&](const auto& existing)
						{ return SameProperty(existing.Root, key); });
					if (stateOwner != stateProperties.end())
					{
						if (animationPath.empty() || stateOwner->Exclusive
							|| ContainsName(stateOwner->AnimationPaths, animationPath))
							return fail(L"视觉状态 Setter/Storyboard 目标重复："
								+ state.Name + L"." + source);
						stateOwner->AnimationPaths.push_back(animationPath);
					}
					else
					{
						StatePropertyOwnership ownership;
						ownership.Root = key;
						ownership.Exclusive = animationPath.empty();
						if (!animationPath.empty())
							ownership.AnimationPaths.push_back(animationPath);
						stateProperties.push_back(std::move(ownership));
					}
					const auto owner = std::find_if(
						groupProperties.begin(), groupProperties.end(),
						[&](const auto& existing)
						{ return SameProperty(existing.first, key); });
					if (owner != groupProperties.end()
						&& owner->second != Groups.size())
						return fail(L"不同视觉状态组不能控制同一属性：" + source);
					if (owner == groupProperties.end())
						groupProperties.emplace_back(key, Groups.size());
					return true;
				};
				for (auto& sourceSetter : sourceState.Setters)
				{
					Control* target = sourceSetter.TargetName.empty()
						? Owner
						: Owner->FindDeclarativeTemplatePart(
							sourceSetter.TargetName);
					if (!target)
						return fail(L"视觉状态 Setter 找不到模板部件："
							+ sourceSetter.TargetName);
					const auto* metadata = target->FindPropertyMetadata(
						sourceSetter.PropertyName);
					BindingValue converted;
					BindingValue coerced;
					if (!metadata || !metadata->CanWrite()
						|| !metadata->TryConvert(sourceSetter.Value, converted)
						|| !metadata->TryCoerce(*target, converted, coerced))
						return fail(L"视觉状态 Setter 属性不存在、只读或值无效："
							+ sourceSetter.PropertyName);
					PropertyKey key{ target, metadata->Name() };
					if (!registerControlledProperty(key, metadata->Name())) return false;
					state.Setters.push_back({
						target, metadata->Name(), std::move(coerced) });
				}
				for (auto& sourceAnimation : sourceState.Animations)
				{
					RuntimeAnimation animation;
					if (!TryBuildAnimation(sourceAnimation, animation,
						L"视觉状态 Storyboard", outError)) return false;
					PropertyKey key{ animation.Target, animation.PropertyName };
					const auto path = std::wstring(
						ObjectPathCanonical(animation.ObjectPath));
					if (!registerControlledProperty(key,
						path.empty() ? animation.PropertyName : path, path))
						return false;
					state.Animations.push_back(std::move(animation));
				}
				group.States.push_back(std::move(state));
			}
			if (!fallback)
				return fail(L"视觉状态组缺少无触发器的回退状态：" + group.Name);
			group.FallbackState = *fallback;
			auto findState = [&](const std::wstring& name) -> std::optional<size_t>
			{
				if (name.empty()) return std::nullopt;
				for (size_t index = 0; index < group.States.size(); ++index)
					if (EqualName(group.States[index].Name, name)) return index;
				return std::nullopt;
			};
			for (const auto& sourceTransition : sourceGroup.Transitions)
			{
				RuntimeTransition transition;
				transition.FromState = findState(sourceTransition.FromState);
				transition.ToState = findState(sourceTransition.ToState);
				if (!sourceTransition.FromState.empty() && !transition.FromState)
					return fail(L"VisualTransition.From 状态不存在："
						+ sourceTransition.FromState);
				if (!sourceTransition.ToState.empty() && !transition.ToState)
					return fail(L"VisualTransition.To 状态不存在："
						+ sourceTransition.ToState);
				if (std::any_of(group.Transitions.begin(), group.Transitions.end(),
					[&](const auto& existing)
					{
						return existing.FromState == transition.FromState
							&& existing.ToState == transition.ToState;
					}))
					return fail(L"VisualTransition From/To 选择器重复："
						+ sourceTransition.FromState + L" -> "
						+ sourceTransition.ToState);
				transition.GeneratedDurationMilliseconds =
					sourceTransition.GeneratedDurationMilliseconds;
				transition.GeneratedEasing = sourceTransition.GeneratedEasing;
				transition.GeneratedEasingMode =
					sourceTransition.GeneratedEasingMode;
				struct TransitionPropertyOwnership
				{
					PropertyKey Root;
					bool Exclusive = false;
					std::vector<std::wstring> AnimationPaths;
				};
				std::vector<TransitionPropertyOwnership> transitionProperties;
				for (const auto& sourceAnimation : sourceTransition.Animations)
				{
					RuntimeAnimation animation;
					if (!TryBuildAnimation(sourceAnimation, animation,
						L"VisualTransition Storyboard", outError)) return false;
					animation.RestoreBaseOnStop = true;
					PropertyKey key{ animation.Target, animation.PropertyName };
					const auto path = std::wstring(
						ObjectPathCanonical(animation.ObjectPath));
					auto owner = std::find_if(transitionProperties.begin(),
						transitionProperties.end(), [&](const auto& existing)
						{ return SameProperty(existing.Root, key); });
					if (owner != transitionProperties.end())
					{
						if (path.empty() || owner->Exclusive
							|| ContainsName(owner->AnimationPaths, path))
							return fail(L"VisualTransition Storyboard 目标重复："
								+ sourceAnimation.PropertyName);
						owner->AnimationPaths.push_back(path);
					}
					else
					{
						TransitionPropertyOwnership ownership;
						ownership.Root = key;
						ownership.Exclusive = path.empty();
						if (!path.empty()) ownership.AnimationPaths.push_back(path);
						transitionProperties.push_back(std::move(ownership));
					}
					const auto groupOwner = std::find_if(groupProperties.begin(),
						groupProperties.end(), [&](const auto& existing)
						{ return SameProperty(existing.first, key); });
					if (groupOwner != groupProperties.end()
						&& groupOwner->second != Groups.size())
						return fail(L"不同视觉状态组不能控制同一 Transition 属性："
							+ sourceAnimation.PropertyName);
					if (groupOwner == groupProperties.end())
						groupProperties.emplace_back(key, Groups.size());
					transition.Animations.push_back(std::move(animation));
				}
				group.Transitions.push_back(std::move(transition));
			}
			Groups.push_back(std::move(group));
		}

		for (auto& sourceTrigger : eventTriggerDefinitions)
		{
			const auto* sourceEvent = Owner->FindDynamicEvent(
				sourceTrigger.EventName);
			if (sourceTrigger.EventName.empty() || !sourceEvent)
				return fail(L"EventTrigger 事件不存在："
					+ sourceTrigger.EventName);
			if (sourceTrigger.Actions.empty())
				return fail(L"EventTrigger 至少需要一个 TriggerAction："
					+ sourceEvent->Name);
			RuntimeEventTrigger trigger;
			trigger.EventName = sourceEvent->Name;
			for (auto& sourceAction : sourceTrigger.Actions)
			{
				RuntimeEventTriggerAction action;
				action.Kind = sourceAction.Kind;
				if (sourceAction.Kind
					== DeclarativeStoryboardActionKind::Begin)
				{
					if (!sourceAction.StoryboardName.empty()
						&& std::any_of(EventStoryboards.begin(),
							EventStoryboards.end(), [&](const auto& existing)
							{ return !existing.IsStyleStoryboard
								&& EqualName(existing.Name,
								sourceAction.StoryboardName); }))
						return fail(L"BeginStoryboard x:Name 重复："
							+ sourceAction.StoryboardName);
					if (sourceAction.Animations.empty())
						return fail(L"BeginStoryboard 的 Storyboard 不能为空。");
					RuntimeEventStoryboard storyboard;
					storyboard.Name = std::move(sourceAction.StoryboardName);
					struct StoryboardPropertyOwnership
					{
						PropertyKey Root;
						bool Exclusive = false;
						std::vector<std::wstring> Paths;
					};
					std::vector<StoryboardPropertyOwnership> properties;
					for (const auto& sourceAnimation : sourceAction.Animations)
					{
						RuntimeAnimation animation;
						if (!TryBuildAnimation(sourceAnimation, animation,
							L"BeginStoryboard", outError)) return false;
						animation.RestoreBaseOnStop = true;
						PropertyKey key{ animation.Target,
							animation.PropertyName };
						const auto path = std::wstring(
							ObjectPathCanonical(animation.ObjectPath));
						auto owner = std::find_if(properties.begin(),
							properties.end(), [&](const auto& existing)
							{ return SameProperty(existing.Root, key); });
						if (owner != properties.end())
						{
							if (path.empty() || owner->Exclusive
								|| ContainsName(owner->Paths, path))
								return fail(L"BeginStoryboard 目标重复："
									+ sourceAnimation.PropertyName);
							owner->Paths.push_back(path);
						}
						else
						{
							StoryboardPropertyOwnership ownership;
							ownership.Root = key;
							ownership.Exclusive = path.empty();
							if (!path.empty()) ownership.Paths.push_back(path);
							properties.push_back(std::move(ownership));
						}
						storyboard.Animations.push_back(std::move(animation));
					}
					action.StoryboardIndex = EventStoryboards.size();
					EventStoryboards.push_back(std::move(storyboard));
				}
				else
				{
					if (sourceAction.StoryboardName.empty())
						return fail(L"Storyboard 控制动作缺少 BeginStoryboardName。");
					action.PendingStoryboardName =
						std::move(sourceAction.StoryboardName);
				}
				trigger.Actions.push_back(std::move(action));
			}
			EventTriggers.push_back(std::move(trigger));
		}
		for (auto& trigger : EventTriggers)
			for (auto& action : trigger.Actions)
			{
				if (action.Kind == DeclarativeStoryboardActionKind::Begin)
					continue;
				const auto found = std::find_if(EventStoryboards.begin(),
					EventStoryboards.end(), [&](const auto& storyboard)
					{ return !storyboard.IsStyleStoryboard
						&& EqualName(storyboard.Name,
						action.PendingStoryboardName); });
				if (found == EventStoryboards.end())
					return fail(L"Storyboard 控制动作找不到 BeginStoryboard："
						+ action.PendingStoryboardName);
				action.StoryboardIndex = static_cast<size_t>(
					std::distance(EventStoryboards.begin(), found));
				action.PendingStoryboardName.clear();
			}

		Connections.push_back(Owner->OnPropertyValueChanged.Subscribe(
			[this](Control*, const ControlPropertyChangedEventArgs& args)
			{ OnHostPropertyChanged(args); }));
		Connections.push_back(Owner->OnDeclarativeEvent.Subscribe(
			[this](Control*, DeclarativeEventArgs& args)
			{ OnHostDeclarativeEvent(args); }));
		for (size_t index = 0; index < Groups.size(); ++index)
			if (!GoTo(index, EvaluateState(Groups[index]), false, outError)) return false;
		if (outError) outError->clear();
		return true;
	}
};

bool Control::DefineVisualStateGroups(
	std::vector<DeclarativeVisualStateGroupDefinition> groups,
	std::wstring* outError)
{
	return DefineDeclarativeInteractions(
		std::move(groups), {}, outError);
}

bool Control::DefineDeclarativeInteractions(
	std::vector<DeclarativeVisualStateGroupDefinition> groups,
	std::vector<DeclarativeEventTriggerDefinition> eventTriggers,
	std::wstring* outError)
{
	if (_declarativeVisualStates
		&& _declarativeVisualStates->DeclarativeInteractionsDefined)
	{
		if (outError) *outError = L"声明交互已经安装。";
		return false;
	}
	if (!_declarativeVisualStates)
	{
		_declarativeVisualStates =
			std::make_unique<DeclarativeVisualStateRuntime>();
		_declarativeVisualStates->Owner = this;
	}
	if (!_declarativeVisualStates->Build(
		std::move(groups), std::move(eventTriggers), outError))
	{
		_declarativeVisualStates->ResetFailedDeclarativeInteractionBuild();
		return false;
	}
	_declarativeVisualStates->DeclarativeInteractionsDefined = true;
	if (outError) outError->clear();
	return true;
}

bool Control::SynchronizeStyleTriggerActions(
	ControlPropertyValueSource source,
	const std::shared_ptr<const ControlStyleSheet>& sheet,
	const ControlStyleResolution& resolution)
{
	if (!_declarativeVisualStates && resolution.Triggers.empty()) return true;
	if (!_declarativeVisualStates)
	{
		_declarativeVisualStates =
			std::make_unique<DeclarativeVisualStateRuntime>();
		_declarativeVisualStates->Owner = this;
	}
	std::wstring ignored;
	return _declarativeVisualStates->SynchronizeStyleTriggerActions(
		source, sheet.get(), resolution.Triggers, &ignored);
}

void Control::PruneStyleTriggerActions(
	ControlPropertyValueSource source,
	const std::vector<std::shared_ptr<const ControlStyleSheet>>& sheets)
{
	if (!_declarativeVisualStates) return;
	std::vector<const ControlStyleSheet*> visible;
	visible.reserve(sheets.size());
	for (const auto& sheet : sheets)
		if (sheet) visible.push_back(sheet.get());
	_declarativeVisualStates->PruneStyleTriggerActions(source, visible);
}

bool Control::GoToVisualState(
	const std::wstring& groupName,
	const std::wstring& stateName,
	std::wstring* outError)
{
	return GoToVisualState(groupName, stateName, true, outError);
}

bool Control::GoToVisualState(
	const std::wstring& groupName,
	const std::wstring& stateName,
	bool useTransitions,
	std::wstring* outError)
{
	if (!_declarativeVisualStates)
	{
		if (outError) *outError = L"控件未安装视觉状态组。";
		return false;
	}
	for (size_t groupIndex = 0;
		groupIndex < _declarativeVisualStates->Groups.size(); ++groupIndex)
	{
		auto& group = _declarativeVisualStates->Groups[groupIndex];
		if (!DeclarativeVisualStateRuntime::EqualName(group.Name, groupName))
			continue;
		for (size_t stateIndex = 0; stateIndex < group.States.size(); ++stateIndex)
			if (DeclarativeVisualStateRuntime::EqualName(
				group.States[stateIndex].Name, stateName))
				return _declarativeVisualStates->GoTo(
					groupIndex, stateIndex, useTransitions, outError);
		if (outError) *outError = L"视觉状态不存在：" + stateName;
		return false;
	}
	if (outError) *outError = L"视觉状态组不存在：" + groupName;
	return false;
}

bool Control::GoToVisualState(
	const std::wstring& stateName,
	std::wstring* outError)
{
	return GoToVisualState(stateName, true, outError);
}

bool Control::GoToVisualState(
	const std::wstring& stateName,
	bool useTransitions,
	std::wstring* outError)
{
	if (!_declarativeVisualStates)
	{
		if (outError) *outError = L"控件未安装视觉状态组。";
		return false;
	}
	std::optional<std::pair<size_t, size_t>> found;
	for (size_t groupIndex = 0;
		groupIndex < _declarativeVisualStates->Groups.size(); ++groupIndex)
	{
		const auto& group = _declarativeVisualStates->Groups[groupIndex];
		for (size_t stateIndex = 0; stateIndex < group.States.size(); ++stateIndex)
		{
			if (!DeclarativeVisualStateRuntime::EqualName(
				group.States[stateIndex].Name, stateName)) continue;
			if (found)
			{
				if (outError) *outError = L"视觉状态名称跨组重复，请指定组："
					+ stateName;
				return false;
			}
			found = std::pair{ groupIndex, stateIndex };
		}
	}
	if (!found)
	{
		if (outError) *outError = L"视觉状态不存在：" + stateName;
		return false;
	}
	return _declarativeVisualStates->GoTo(
		found->first, found->second, useTransitions, outError);
}

std::wstring Control::GetCurrentVisualState(
	const std::wstring& groupName) const
{
	if (!_declarativeVisualStates) return {};
	for (const auto& group : _declarativeVisualStates->Groups)
		if (DeclarativeVisualStateRuntime::EqualName(group.Name, groupName))
		{
			if (group.Pending
				&& group.Pending->TargetState < group.States.size())
				return group.States[group.Pending->TargetState].Name;
			if (group.CurrentState && *group.CurrentState < group.States.size())
				return group.States[*group.CurrentState].Name;
		}
	return {};
}

bool Control::HasActiveVisualStateAnimations() const noexcept
{
	return _declarativeVisualStates
		&& _declarativeVisualStates->HasActiveAnimations();
}

bool Control::AdvanceVisualStateAnimations(
	unsigned long long nowMilliseconds)
{
	return _declarativeVisualStates
		&& _declarativeVisualStates->AdvanceAnimations(nowMilliseconds);
}

bool Control::TrySetValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	return TrySetCurrentPropertyValue(propertyName, value);
}

bool Control::TryGetPropertyMetadata(
	const std::wstring& propertyName,
	BindingSourcePropertyMetadata& out) const
{
	auto& target = *const_cast<Control*>(this);
	const auto* metadata = BindingPropertyRegistry::Find(target, propertyName);
	if (!metadata) return false;
	out.Name = metadata->Name();
	out.ValueKind = metadata->ValueKind();
	out.ValueType = metadata->ValueType();
	out.CanRead = metadata->CanRead();
	out.CanWrite = metadata->CanWrite();
	// Every metadata-backed Control write is published by
	// ApplyPropertyMetadataChange through PropertyChanged(). Individual metadata
	// subscribers remain useful for legacy interaction events, but are not the
	// boundary of the Control's IBindingSource observability.
	out.CanObserve = true;
	return true;
}

PropertyChangedEvent& Control::PropertyChanged()
{
	if (!_bindingSourceMetadataConnectionsInitialized)
	{
		_bindingSourceMetadataConnectionsInitialized = true;
		for (const auto* metadata : BindingPropertyRegistry::GetProperties(*this))
		{
			if (!metadata || !metadata->CanObserve()) continue;
			auto connection = metadata->Subscribe(
				*this,
				[this, metadata]
				{
					// A metadata write publishes once from ApplyPropertyMetadataChange.
					// This bridge is for legacy/user-interaction events that otherwise
					// bypass the property system's IBindingSource notification.
					if (_applyingPropertyMetadata == metadata) return;
					_bindingSourcePropertyChanged.Notify(metadata->Name());
				},
				DataSourceUpdateMode::OnPropertyChanged);
			if (connection.Connected())
				_bindingSourceMetadataConnections.push_back(std::move(connection));
		}
	}
	return _bindingSourcePropertyChanged;
}

bool Control::SetDataContext(BindingSourceReference value)
{
	return TrySetPropertyValue(
		L"DataContext", BindingValue(std::move(value)),
		ControlPropertyValueSource::Local);
}

bool Control::ClearDataContext()
{
	return ClearPropertyValue(
		L"DataContext", ControlPropertyValueSource::Local);
}

IBindingSource& Control::DataContextSource()
{
	if (!_dataContextSource)
		_dataContextSource = std::make_unique<BindingSourceProxy>(
			_effectiveDataContext);
	return *_dataContextSource;
}

void Control::SetInheritedDataContext(BindingSourceReference value)
{
	if (_inheritedDataContext == value) return;
	_inheritedDataContext = std::move(value);
	if (GetPropertyValueSource(L"DataContext")
		== ControlPropertyValueSource::Default)
		UpdateEffectiveDataContext(_inheritedDataContext);
}

void Control::UpdateEffectiveDataContext(BindingSourceReference value)
{
	if (_effectiveDataContext == value) return;
	_effectiveDataContext = std::move(value);
	if (_dataContextSource)
		_dataContextSource->SetSource(_effectiveDataContext);
	RebuildStyleDataContextSubscriptions();
	RefreshStyleValues(false);
	_dataContextChanged.Notify(L"DataContext");
	if (!_applyingPropertyMetadata)
		_bindingSourcePropertyChanged.Notify(L"DataContext");
	for (auto* child : Children)
		if (child) child->SetInheritedDataContext(_effectiveDataContext);
}

std::vector<BindingSourcePropertyMetadata> Control::GetProperties() const
{
	auto& target = *const_cast<Control*>(this);
	std::vector<BindingSourcePropertyMetadata> result;
	for (const auto* metadata : BindingPropertyRegistry::GetProperties(target))
	{
		if (!metadata) continue;
		result.push_back({
			metadata->Name(), metadata->ValueKind(), metadata->ValueType(),
			metadata->CanRead(), metadata->CanWrite(), true });
	}
	return result;
}

GET_CPP(Control, bool, ShowValidationBorder)
{
	return _showValidationBorder;
}

SET_CPP(Control, bool, ShowValidationBorder)
{
	SetPropertyField(L"ShowValidationBorder", _showValidationBorder, value);
}

GET_CPP(Control, bool, ShowValidationToolTip)
{
	return _showValidationToolTip;
}

SET_CPP(Control, bool, ShowValidationToolTip)
{
	SetPropertyField(L"ShowValidationToolTip", _showValidationToolTip, value);
}

GET_CPP(Control, float, ValidationBorderThickness)
{
	return _validationBorderThickness;
}

SET_CPP(Control, float, ValidationBorderThickness)
{
	SetPropertyField(
		L"ValidationBorderThickness", _validationBorderThickness, value);
}

GET_CPP(Control, float, ValidationCornerRadius)
{
	return _validationCornerRadius;
}

SET_CPP(Control, float, ValidationCornerRadius)
{
	SetPropertyField(L"ValidationCornerRadius", _validationCornerRadius, value);
}

GET_CPP(Control, float, ValidationToolTipMaxWidth)
{
	return _validationToolTipMaxWidth;
}

SET_CPP(Control, float, ValidationToolTipMaxWidth)
{
	SetPropertyField(
		L"ValidationToolTipMaxWidth", _validationToolTipMaxWidth, value);
}

GET_CPP(Control, bool, IsTabStop)
{
	return _isTabStop;
}

SET_CPP(Control, bool, IsTabStop)
{
	if (SetPropertyField(L"IsTabStop", _isTabStop, value) && ParentForm)
		ParentForm->NotifyAccessibilityEvent(this, AccessibilityChange::State);
}

GET_CPP(Control, int, TabIndex)
{
	return _tabIndex;
}

SET_CPP(Control, int, TabIndex)
{
	if (SetPropertyField(L"TabIndex", _tabIndex, (std::max)(0, value)) && ParentForm)
		ParentForm->NotifyAccessibilityEvent(nullptr, AccessibilityChange::Structure);
}

GET_CPP(Control, std::wstring, AccessKey)
{
	return _accessKey;
}

SET_CPP(Control, std::wstring, AccessKey)
{
	if (value.size() > 1) value.resize(1);
	if (!value.empty()) value[0] = static_cast<wchar_t>(std::towupper(value[0]));
	if (SetPropertyField(L"AccessKey", _accessKey, std::move(value)) && ParentForm)
		ParentForm->NotifyAccessibilityEvent(this, AccessibilityChange::State);
}

GET_CPP(Control, std::wstring, AccessibleName)
{
	return _accessibleName;
}

SET_CPP(Control, std::wstring, AccessibleName)
{
	if (SetPropertyField(L"AccessibleName", _accessibleName, std::move(value)) && ParentForm)
		ParentForm->NotifyAccessibilityEvent(this, AccessibilityChange::Name);
}

GET_CPP(Control, std::wstring, AccessibleDescription)
{
	return _accessibleDescription;
}

SET_CPP(Control, std::wstring, AccessibleDescription)
{
	if (SetPropertyField(
		L"AccessibleDescription", _accessibleDescription, std::move(value)) && ParentForm)
		ParentForm->NotifyAccessibilityEvent(this, AccessibilityChange::Description);
}

GET_CPP(Control, std::wstring, AccessibleHelpText)
{
	return _accessibleHelpText;
}

SET_CPP(Control, std::wstring, AccessibleHelpText)
{
	if (SetPropertyField(L"AccessibleHelpText", _accessibleHelpText, std::move(value)) && ParentForm)
		ParentForm->NotifyAccessibilityEvent(this, AccessibilityChange::Help);
}

GET_CPP(Control, std::wstring, AutomationId)
{
	return _automationId;
}

SET_CPP(Control, std::wstring, AutomationId)
{
	if (SetPropertyField(L"AutomationId", _automationId, std::move(value)) && ParentForm)
		ParentForm->NotifyAccessibilityEvent(this, AccessibilityChange::Structure);
}

GET_CPP(Control, ::AccessibleRole, AccessibleRole)
{
	return _accessibleRole;
}

SET_CPP(Control, ::AccessibleRole, AccessibleRole)
{
	if (SetPropertyField(L"AccessibleRole", _accessibleRole, value) && ParentForm)
		ParentForm->NotifyAccessibilityEvent(this, AccessibilityChange::State);
}

GET_CPP(Control, bool, ShowFocusVisual)
{
	return _showFocusVisual;
}

SET_CPP(Control, bool, ShowFocusVisual)
{
	SetPropertyField(L"ShowFocusVisual", _showFocusVisual, value);
}

GET_CPP(Control, D2D1_COLOR_F, FocusVisualColor)
{
	return ParentForm
		? ParentForm->GetEffectiveFocusColor(_focusVisualColor)
		: _focusVisualColor;
}

SET_CPP(Control, D2D1_COLOR_F, FocusVisualColor)
{
	SetPropertyField(L"FocusVisualColor", _focusVisualColor, value);
}

GET_CPP(Control, float, FocusVisualThickness)
{
	return _focusVisualThickness;
}

SET_CPP(Control, float, FocusVisualThickness)
{
	const float normalized = std::isfinite(value)
		? (std::clamp)(value, 0.0f, 8.0f)
		: 1.5f;
	SetPropertyField(L"FocusVisualThickness", _focusVisualThickness, normalized);
}

std::vector<BindingValidationResult> Control::GetValidationResults() const
{
	return _dataBindings
		? _dataBindings->GetValidationResults()
		: std::vector<BindingValidationResult>{};
}

bool Control::HasValidationIssues() const
{
	return _dataBindings && _dataBindings->HasValidationIssues();
}

bool Control::HasValidationErrors() const
{
	return _dataBindings && _dataBindings->HasValidationErrors();
}

bool Control::TryGetValidationSeverity(
	BindingValidationSeverity& severity) const
{
	const auto results = GetValidationResults();
	if (results.empty()) return false;
	severity = BindingValidationSeverity::Info;
	for (const auto& result : results)
	{
		if (ValidationSeverityRank(result.Issue.Severity)
			> ValidationSeverityRank(severity))
			severity = result.Issue.Severity;
	}
	return true;
}

std::wstring Control::GetValidationSummary(size_t maxIssues) const
{
	const auto results = GetValidationResults();
	std::vector<BindingValidationIssue> unique;
	unique.reserve(results.size());
	for (const auto& result : results)
	{
		if (std::find(unique.begin(), unique.end(), result.Issue) == unique.end())
			unique.push_back(result.Issue);
	}

	const size_t visibleCount = maxIssues == 0
		? unique.size()
		: (std::min)(unique.size(), maxIssues);
	std::wstring summary;
	for (size_t index = 0; index < visibleCount; ++index)
	{
		if (!summary.empty()) summary += L"\r\n";
		summary += L"[";
		summary += BindingValidationSeverityName(unique[index].Severity);
		summary += L"] ";
		summary += unique[index].Message;
	}
	if (unique.size() > visibleCount)
	{
		if (!summary.empty()) summary += L"\r\n";
		summary += L"+" + std::to_wstring(unique.size() - visibleCount)
			+ L" more validation issue(s)";
	}
	return summary;
}

std::wstring Control::GetEffectiveAccessibleDescription() const
{
	const auto validation = GetValidationSummary();
	if (_accessibleDescription.empty()) return validation;
	if (validation.empty()) return _accessibleDescription;
	return _accessibleDescription + L"\r\n" + validation;
}

std::wstring Control::GetEffectiveAccessibleName() const
{
	if (!_accessibleName.empty()) return _accessibleName;
	// Editable content is a value, not a label. Password content must never leak.
	switch (const_cast<Control*>(this)->Type())
	{
	case UIClass::UI_TextBox:
	case UIClass::UI_RichTextBox:
	case UIClass::UI_PasswordBox:
	case UIClass::UI_ComboBox:
		return {};
	default:
		break;
	}
	return GetDisplayText();
}

std::wstring Control::GetDisplayText() const
{
	switch (GetEffectiveAccessibleRole())
	{
	case ::AccessibleRole::Button:
	case ::AccessibleRole::Link:
	case ::AccessibleRole::CheckBox:
	case ::AccessibleRole::RadioButton:
	case ::AccessibleRole::Switch:
	case ::AccessibleRole::Group:
	case ::AccessibleRole::MenuItem:
	case ::AccessibleRole::TabItem:
		return StripAccessKeyMarkers(_text);
	default:
		return _text;
	}
}

wchar_t Control::GetEffectiveAccessKey() const
{
	if (!_accessKey.empty())
		return static_cast<wchar_t>(std::towupper(_accessKey.front()));
	switch (GetEffectiveAccessibleRole())
	{
	case ::AccessibleRole::Button:
	case ::AccessibleRole::Link:
	case ::AccessibleRole::CheckBox:
	case ::AccessibleRole::RadioButton:
	case ::AccessibleRole::Switch:
	case ::AccessibleRole::Group:
	case ::AccessibleRole::MenuItem:
	case ::AccessibleRole::TabItem:
		return FindAccessKeyMarker(_text);
	default:
		return L'\0';
	}
}

std::wstring Control::GetEffectiveKeyboardShortcut() const
{
	const wchar_t key = GetEffectiveAccessKey();
	return key == L'\0' ? std::wstring{} : std::wstring(L"Alt+") + key;
}

::AccessibleRole Control::GetEffectiveAccessibleRole() const
{
	if (_accessibleRole != ::AccessibleRole::Default)
		return _accessibleRole;

	switch (const_cast<Control*>(this)->Type())
	{
	case UIClass::UI_Label: return ::AccessibleRole::Text;
	case UIClass::UI_LinkLabel: return ::AccessibleRole::Link;
	case UIClass::UI_Button: return ::AccessibleRole::Button;
	case UIClass::UI_CheckBox: return ::AccessibleRole::CheckBox;
	case UIClass::UI_RadioBox: return ::AccessibleRole::RadioButton;
	case UIClass::UI_Switch: return ::AccessibleRole::Switch;
	case UIClass::UI_TextBox:
	case UIClass::UI_RichTextBox: return ::AccessibleRole::TextBox;
	case UIClass::UI_PasswordBox: return ::AccessibleRole::PasswordBox;
	case UIClass::UI_ComboBox:
	case UIClass::UI_DateTimePicker:
	case UIClass::UI_ColorPicker:
	case UIClass::UI_NumericUpDown: return ::AccessibleRole::ComboBox;
	case UIClass::UI_ListView:
	case UIClass::UI_ListBox:
	case UIClass::UI_ItemsControl:
	case UIClass::UI_NavigationView:
	case UIClass::UI_SideBar: return ::AccessibleRole::List;
	case UIClass::UI_SelectorItem:
	case UIClass::UI_ComboBoxItem:
	case UIClass::UI_TreeViewItem: return ::AccessibleRole::ListItem;
	case UIClass::UI_GridView:
	case UIClass::UI_PropertyGrid:
	case UIClass::UI_PagedGridView:
	case UIClass::UI_ReportView: return ::AccessibleRole::Table;
	case UIClass::UI_TreeView: return ::AccessibleRole::Tree;
	case UIClass::UI_TabControl: return ::AccessibleRole::Tab;
	case UIClass::UI_TabPage: return ::AccessibleRole::TabItem;
	case UIClass::UI_Menu:
	case UIClass::UI_ContextMenu: return ::AccessibleRole::Menu;
	case UIClass::UI_MenuItem: return ::AccessibleRole::MenuItem;
	case UIClass::UI_ToolBar: return ::AccessibleRole::ToolBar;
	case UIClass::UI_StatusBar: return ::AccessibleRole::StatusBar;
	case UIClass::UI_Slider: return ::AccessibleRole::Slider;
	case UIClass::UI_ProgressBar:
	case UIClass::UI_ProgressRing:
	case UIClass::UI_LoadingRing: return ::AccessibleRole::ProgressBar;
	case UIClass::UI_PictureBox:
	case UIClass::UI_ChartView: return ::AccessibleRole::Image;
	case UIClass::UI_WebBrowser: return ::AccessibleRole::Document;
	case UIClass::UI_GroupBox:
	case UIClass::UI_Expander: return ::AccessibleRole::Group;
	case UIClass::UI_Panel:
	case UIClass::UI_NativeSurface:
	case UIClass::UI_ScrollView:
	case UIClass::UI_StackPanel:
	case UIClass::UI_GridPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_SplitContainer:
	case UIClass::UI_ContentPresenter: return ::AccessibleRole::Pane;
	case UIClass::UI_ItemsPresenter: return ::AccessibleRole::Pane;
	case UIClass::UI_ContentControl: return ::AccessibleRole::Group;
	default: return ::AccessibleRole::Custom;
	}
}

bool Control::IsKeyboardFocusable() const
{
	switch (const_cast<Control*>(this)->Type())
	{
	case UIClass::UI_LinkLabel:
	case UIClass::UI_Button:
	case UIClass::UI_TextBox:
	case UIClass::UI_RichTextBox:
	case UIClass::UI_PasswordBox:
	case UIClass::UI_ComboBox:
	case UIClass::UI_ListView:
	case UIClass::UI_ListBox:
	case UIClass::UI_GridView:
	case UIClass::UI_PropertyGrid:
	case UIClass::UI_CheckBox:
	case UIClass::UI_RadioBox:
	case UIClass::UI_TreeView:
	case UIClass::UI_TabControl:
	case UIClass::UI_Switch:
	case UIClass::UI_Slider:
	case UIClass::UI_WebBrowser:
	case UIClass::UI_MediaPlayer:
	case UIClass::UI_SplitContainer:
	case UIClass::UI_DateTimePicker:
	case UIClass::UI_FilterBar:
	case UIClass::UI_NavigationView:
	case UIClass::UI_SideBar:
	case UIClass::UI_BreadcrumbBar:
	case UIClass::UI_CalendarView:
	case UIClass::UI_DateRangePicker:
	case UIClass::UI_ColorPicker:
	case UIClass::UI_PagedGridView:
	case UIClass::UI_NumericUpDown:
	case UIClass::UI_Expander:
	case UIClass::UI_NativeSurface:
		return true;
	default:
		return false;
	}
}

bool Control::CanReceiveKeyboardFocus() const
{
	if (!_isTabStop || !IsKeyboardFocusable() || !Enable || !_visible)
		return false;
	for (auto* ancestor = Parent; ancestor; ancestor = ancestor->Parent)
	{
		if (!ancestor->Enable || !ancestor->_visible)
			return false;
	}
	return true;
}

bool Control::Focus()
{
	if (!ParentForm || !CanReceiveKeyboardFocus()) return false;
	if (ParentForm->Handle && ::GetFocus() != ParentForm->Handle)
		::SetFocus(ParentForm->Handle);
	ParentForm->SetSelectedControl(this, true);
	return ParentForm->Selected == this;
}

bool Control::Invoke()
{
	return false;
}

bool Control::AreSystemAnimationsEnabled() const
{
	return !ParentForm || ParentForm->AreSystemAnimationsEnabled();
}

UINT Control::EffectiveAnimationDuration(UINT configuredDurationMs) const
{
	return AreSystemAnimationsEnabled() ? configuredDurationMs : 0U;
}

AccessibilitySnapshot Control::GetAccessibilitySnapshot() const
{
	AccessibilitySnapshot snapshot;
	snapshot.Role = GetEffectiveAccessibleRole();
	snapshot.Name = GetEffectiveAccessibleName();
	snapshot.Description = GetEffectiveAccessibleDescription();
	snapshot.HelpText = _accessibleHelpText;
	snapshot.AutomationId = _automationId;
	snapshot.KeyboardShortcut = GetEffectiveKeyboardShortcut();
	snapshot.Enabled = Enable;
	snapshot.Visible = _visible;
	for (auto* ancestor = Parent; ancestor; ancestor = ancestor->Parent)
	{
		snapshot.Enabled = snapshot.Enabled && ancestor->Enable;
		snapshot.Visible = snapshot.Visible && ancestor->_visible;
	}
	snapshot.Focusable = CanReceiveKeyboardFocus();
	snapshot.Focused = IsSelected();
	snapshot.Selected = snapshot.Focused;
	snapshot.Checked = Checked;
	snapshot.Password = const_cast<Control*>(this)->Type() == UIClass::UI_PasswordBox;
	snapshot.ReadOnly = IsAccessibilityReadOnly();
	switch (const_cast<Control*>(this)->Type())
	{
	case UIClass::UI_TextBox:
	case UIClass::UI_RichTextBox:
	case UIClass::UI_ComboBox:
		snapshot.Value = _text;
		break;
	default:
		break;
	}
	if (snapshot.Value.empty() && !snapshot.Password)
	{
		BindingValue value;
		if (const_cast<Control*>(this)->TryGetPropertyValue(L"Value", value))
			snapshot.Value = value.ToString();
	}
	return snapshot;
}

bool Control::ShouldShowValidationToolTip() const
{
	return _showValidationToolTip && HasValidationIssues();
}

void Control::OnBindingValidationChanged(
	const std::wstring& targetProperty)
{
	InvalidateVisual();
	if (ParentForm && ParentForm->UnderMouse == this)
		ParentForm->Invalidate(false);
	if (ParentForm)
		ParentForm->NotifyAccessibilityEvent(
			this, AccessibilityChange::Description);
	OnValidationStateChanged.Notify(targetProperty);
}

void Control::NotifyAccessibilityStructureChanged()
{
	if (ParentForm)
		ParentForm->NotifyAccessibilityEvent(nullptr, AccessibilityChange::Structure);
}

void Control::NotifyAccessibilityScrollChanged()
{
	if (ParentForm)
		ParentForm->NotifyAccessibilityEvent(this, AccessibilityChange::Scroll);
}

void Control::NotifyAccessibilityVirtualChanged(
	uint32_t virtualId, AccessibilityChange change)
{
	if (ParentForm && virtualId != 0)
		ParentForm->NotifyAccessibilityVirtualEvent(this, virtualId, change);
}

void Control::RenderFocusAdorner()
{
	if (!_showFocusVisual || _focusVisualThickness <= 0.0f
		|| FocusVisualColor.a <= 0.0f || !IsSelected()
		|| !ParentForm || !ParentForm->Render)
		return;
	if (!ParentForm->ShouldShowKeyboardFocusVisual()) return;
	const auto size = GetActualSizeDip();
	if (!(size.width > 0.0f) || !(size.height > 0.0f)) return;
	const float thickness = (std::min)(_focusVisualThickness,
		(std::min)(size.width, size.height));
	const float inset = thickness * 0.5f + 1.0f;
	const float width = (std::max)(0.0f, size.width - inset * 2.0f);
	const float height = (std::max)(0.0f, size.height - inset * 2.0f);
	const float radius = (std::min)(4.0f, (std::min)(width, height) * 0.5f);
	ParentForm->Render->DrawRoundRect(
		inset, inset, width, height, FocusVisualColor, thickness, radius);
}

void Control::RenderValidationAdorner()
{
	if (!_showValidationBorder || _validationBorderThickness <= 0.0f
		|| !ParentForm || !ParentForm->Render)
		return;
	BindingValidationSeverity severity;
	if (!TryGetValidationSeverity(severity)) return;

	const auto size = GetActualSizeDip();
	if (!(size.width > 0.0f) || !(size.height > 0.0f)) return;
	const float thickness = (std::min)(_validationBorderThickness,
		(std::min)(size.width, size.height));
	const float inset = thickness * 0.5f;
	const float width = (std::max)(0.0f, size.width - thickness);
	const float height = (std::max)(0.0f, size.height - thickness);
	const float radius = (std::min)(_validationCornerRadius,
		(std::min)(width, height) * 0.5f);
	const auto color = ParentForm
		? ParentForm->GetValidationColor(severity)
		: DefaultValidationColor(severity);
	if (color.a <= 0.0f) return;
	ParentForm->Render->DrawRoundRect(
		inset, inset, width, height, color, thickness, radius);
}

void Control::EnsureBindingPropertiesRegistered()
{
	static std::once_flag once;
	std::call_once(once, []
	{
		using Handler = BindingPropertyMetadata::ChangeHandler;
		auto dataContextDesign = PropertyDesign(
			L"Data", 250, 0, ControlPropertyPersistence::Transient);
		dataContextDesign.Browsable = false;
		ControlPropertyOptions<Control, BindingSourceReference> dataContextOptions;
		dataContextOptions.DefaultValue = BindingSourceReference{};
		dataContextOptions.Equals = [](const BindingSourceReference& left,
			const BindingSourceReference& right) { return left == right; };
		dataContextOptions.Design = std::move(dataContextDesign);
		BindingPropertyRegistry::Register<Control, BindingSourceReference>(
			L"DataContext",
			[](Control& target) { return target.GetDataContext(); },
			[](Control& target, const BindingSourceReference& value)
			{
				target.UpdateEffectiveDataContext(
					target._applyingPropertySource
						== ControlPropertyValueSource::Default
						? target._inheritedDataContext
						: value);
			},
			[](Control& target, Handler handler, DataSourceUpdateMode)
			{
				return target.DataContextChanged().Subscribe(
					[handler = std::move(handler)](const PropertyChangedEventArgs&)
					{ handler(); });
			},
			std::move(dataContextOptions));
		auto checkedDesign = PropertyDesign(
			L"Behavior", 300, 10, ControlPropertyPersistence::Metadata,
			ControlPropertyEditorKind::Boolean);
		checkedDesign.BrowsableWhen = [](Control& target)
		{
			switch (target.Type())
			{
			case UIClass::UI_Button:
			case UIClass::UI_CheckBox:
			case UIClass::UI_RadioBox:
			case UIClass::UI_Switch:
				return true;
			default:
				return false;
			}
		};
		BindingPropertyRegistry::Register<Control, std::wstring>(L"Text",
			[](Control& target) { return target.Text; },
			[](Control& target, const std::wstring& value) { target.Text = value; },
			[](Control& target, Handler handler, DataSourceUpdateMode mode)
			{
				if (mode == DataSourceUpdateMode::OnValidation)
					return target.OnLostFocus.Subscribe(
						[handler = std::move(handler)](Control*) { handler(); });
				return target.OnTextChanged.Subscribe(
					[handler = std::move(handler)](Control*, std::wstring, std::wstring) { handler(); });
			},
			WithPropertyDesign(ControlPropertyOptions<Control, std::wstring>{
				std::wstring{},
				ControlPropertyFlags::AffectsMeasure
					| ControlPropertyFlags::AffectsRender },
				PropertyDesign(L"Common", 0, 10, ControlPropertyPersistence::Legacy)));

		BindingPropertyRegistry::Register<Control, bool>(L"Checked",
			[](Control& target) { return target.Checked; },
			[](Control& target, const bool& value)
			{
				if (target.Checked == value) return;
				target.Checked = value;
				target.RefreshStyleValues(false);
				target.InvalidateVisual();
			},
			[](Control& target, Handler handler, DataSourceUpdateMode)
			{
				return target.OnChecked.Subscribe(
					[handler = std::move(handler)](Control*) { handler(); });
			},
			WithPropertyDesign(ControlPropertyOptions<Control, bool>{
				false, ControlPropertyFlags::AffectsRender }, std::move(checkedDesign)));

		BindingPropertyRegistry::Register<Control, bool>(L"Visible",
			[](Control& target) { return target.Visible; },
			[](Control& target, const bool& value) { target.Visible = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, bool>{
				true,
				ControlPropertyFlags::AffectsMeasure
					| ControlPropertyFlags::AffectsRender },
				PropertyDesign(L"Common", 0, 30, ControlPropertyPersistence::Legacy)));

		auto registerEnabled = [](const wchar_t* name, bool browsable)
		{
			auto design = PropertyDesign(L"Common", 0, 20,
				ControlPropertyPersistence::Legacy,
				ControlPropertyEditorKind::Boolean, L"Enabled");
			design.Browsable = browsable;
			BindingPropertyRegistry::Register<Control, bool>(name,
				[](Control& target) { return target.Enable; },
				[](Control& target, const bool& value)
				{
					if (target.Enable == value) return;
					target.Enable = value;
					target.RefreshStyleValues(false);
					target.InvalidateVisual();
				},
				{},
				WithPropertyDesign(ControlPropertyOptions<Control, bool>{
					true, ControlPropertyFlags::AffectsRender },
					std::move(design)));
		};
		registerEnabled(L"Enable", true);
		registerEnabled(L"Enabled", false);

		auto movedSubscriber = [](Control& target, Handler handler, DataSourceUpdateMode)
		{
			return target.OnMoved.Subscribe(
				[handler = std::move(handler)](Control*) { handler(); });
		};
		auto sizedSubscriber = [](Control& target, Handler handler, DataSourceUpdateMode)
		{
			return target.OnSizeChanged.Subscribe(
				[handler = std::move(handler)](Control*) { handler(); });
		};

		BindingPropertyRegistry::Register<Control, int>(L"Left",
			[](Control& target) { return target.Left; },
			[](Control& target, const int& value) { target.Left = value; }, movedSubscriber,
			WithPropertyDesign(ControlPropertyOptions<Control, int>{
				0, ControlPropertyFlags::AffectsArrange | ControlPropertyFlags::AffectsRender },
				PropertyDesign(L"Layout", 100, 10, ControlPropertyPersistence::Legacy,
					ControlPropertyEditorKind::Number, L"X")));
		BindingPropertyRegistry::Register<Control, int>(L"Top",
			[](Control& target) { return target.Top; },
			[](Control& target, const int& value) { target.Top = value; }, movedSubscriber,
			WithPropertyDesign(ControlPropertyOptions<Control, int>{
				0, ControlPropertyFlags::AffectsArrange | ControlPropertyFlags::AffectsRender },
				PropertyDesign(L"Layout", 100, 20, ControlPropertyPersistence::Legacy,
					ControlPropertyEditorKind::Number, L"Y")));
		BindingPropertyRegistry::Register<Control, int>(L"Width",
			[](Control& target) { return target.Width; },
			[](Control& target, const int& value) { target.Width = value; }, sizedSubscriber,
			WithPropertyDesign(ControlPropertyOptions<Control, int>{
				120, ControlPropertyFlags::AffectsMeasure | ControlPropertyFlags::AffectsRender },
				PropertyDesign(L"Layout", 100, 30, ControlPropertyPersistence::Legacy)));
		BindingPropertyRegistry::Register<Control, int>(L"Height",
			[](Control& target) { return target.Height; },
			[](Control& target, const int& value) { target.Height = value; }, sizedSubscriber,
			WithPropertyDesign(ControlPropertyOptions<Control, int>{
				20, ControlPropertyFlags::AffectsMeasure | ControlPropertyFlags::AffectsRender },
				PropertyDesign(L"Layout", 100, 40, ControlPropertyPersistence::Legacy)));

		BindingPropertyRegistry::Register<Control, cui::layout::Length>(L"LayoutWidth",
			[](Control& target) { return target.GetLayoutWidth(); },
			[](Control& target, const cui::layout::Length& value) { target.SetLayoutWidth(value); },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, cui::layout::Length>{
				cui::layout::Length::Auto(), ControlPropertyFlags::AffectsMeasure },
				PropertyDesign(L"Layout", 100, 50, ControlPropertyPersistence::Metadata,
					ControlPropertyEditorKind::Length, L"Width (Auto)")));
		BindingPropertyRegistry::Register<Control, cui::layout::Length>(L"LayoutHeight",
			[](Control& target) { return target.GetLayoutHeight(); },
			[](Control& target, const cui::layout::Length& value) { target.SetLayoutHeight(value); },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, cui::layout::Length>{
				cui::layout::Length::Auto(), ControlPropertyFlags::AffectsMeasure },
				PropertyDesign(L"Layout", 100, 60, ControlPropertyPersistence::Metadata,
					ControlPropertyEditorKind::Length, L"Height (Auto)")));
		BindingPropertyRegistry::Register<Control, Thickness>(L"Margin",
			[](Control& target) { return target.Margin; },
			[](Control& target, const Thickness& value) { target.Margin = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, Thickness>{
				Thickness{}, ControlPropertyFlags::AffectsMeasure },
				PropertyDesign(L"Layout", 100, 70, ControlPropertyPersistence::Legacy)));
		BindingPropertyRegistry::Register<Control, Thickness>(L"Padding",
			[](Control& target) { return target.Padding; },
			[](Control& target, const Thickness& value) { target.Padding = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, Thickness>{
				Thickness{}, ControlPropertyFlags::AffectsMeasure },
				PropertyDesign(L"Layout", 100, 80, ControlPropertyPersistence::Legacy)));
		auto horizontalAlignmentDesign = PropertyDesign(
			L"Layout", 100, 90, ControlPropertyPersistence::Legacy,
			ControlPropertyEditorKind::Choice);
		horizontalAlignmentDesign.Choices = {
			PropertyChoice(L"Left", HorizontalAlignment::Left),
			PropertyChoice(L"Center", HorizontalAlignment::Center),
			PropertyChoice(L"Right", HorizontalAlignment::Right),
			PropertyChoice(L"Stretch", HorizontalAlignment::Stretch)
		};
		BindingPropertyRegistry::Register<Control, HorizontalAlignment>(L"HAlign",
			[](Control& target) { return target.HAlign; },
			[](Control& target, const HorizontalAlignment& value) { target.HAlign = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, HorizontalAlignment>{
				HorizontalAlignment::Left, ControlPropertyFlags::AffectsArrange },
				std::move(horizontalAlignmentDesign)));
		auto verticalAlignmentDesign = PropertyDesign(
			L"Layout", 100, 100, ControlPropertyPersistence::Legacy,
			ControlPropertyEditorKind::Choice);
		verticalAlignmentDesign.Choices = {
			PropertyChoice(L"Top", VerticalAlignment::Top),
			PropertyChoice(L"Center", VerticalAlignment::Center),
			PropertyChoice(L"Bottom", VerticalAlignment::Bottom),
			PropertyChoice(L"Stretch", VerticalAlignment::Stretch)
		};
		BindingPropertyRegistry::Register<Control, VerticalAlignment>(L"VAlign",
			[](Control& target) { return target.VAlign; },
			[](Control& target, const VerticalAlignment& value) { target.VAlign = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, VerticalAlignment>{
				VerticalAlignment::Top, ControlPropertyFlags::AffectsArrange },
				std::move(verticalAlignmentDesign)));
		BindingPropertyRegistry::Register<Control, int>(L"ZIndex",
			[](Control& target) { return target.ZIndex; },
			[](Control& target, const int& value) { target.ZIndex = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, int>{
				0, ControlPropertyFlags::AffectsRender },
				PropertyDesign(L"Layout", 100, 105,
					ControlPropertyPersistence::Legacy,
					ControlPropertyEditorKind::Number)));
		auto gridPlacementDesign = [](int order)
		{
			auto design = PropertyDesign(L"Layout", 100, order,
				ControlPropertyPersistence::Legacy,
				ControlPropertyEditorKind::Number);
			design.BrowsableWhen = [](Control& target)
			{
				return target.Parent
					&& target.Parent->Type() == UIClass::UI_GridPanel;
			};
			return design;
		};
		BindingPropertyRegistry::Register<Control, int>(L"GridRow",
			[](Control& target) { return target.GridRow; },
			[](Control& target, const int& value) { target.GridRow = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, int>{
				0,
				ControlPropertyFlags::AffectsMeasure,
				[](Control&, const int& proposed) -> std::optional<int>
				{
					return (std::max)(0, proposed);
				} }, gridPlacementDesign(110)));
		BindingPropertyRegistry::Register<Control, int>(L"GridColumn",
			[](Control& target) { return target.GridColumn; },
			[](Control& target, const int& value) { target.GridColumn = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, int>{
				0,
				ControlPropertyFlags::AffectsMeasure,
				[](Control&, const int& proposed) -> std::optional<int>
				{
					return (std::max)(0, proposed);
				} }, gridPlacementDesign(120)));
		BindingPropertyRegistry::Register<Control, int>(L"GridRowSpan",
			[](Control& target) { return target.GridRowSpan; },
			[](Control& target, const int& value) { target.GridRowSpan = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, int>{
				1,
				ControlPropertyFlags::AffectsMeasure,
				[](Control&, const int& proposed) -> std::optional<int>
				{
					return (std::max)(1, proposed);
				} }, gridPlacementDesign(130)));
		BindingPropertyRegistry::Register<Control, int>(L"GridColumnSpan",
			[](Control& target) { return target.GridColumnSpan; },
			[](Control& target, const int& value) { target.GridColumnSpan = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, int>{
				1,
				ControlPropertyFlags::AffectsMeasure,
				[](Control&, const int& proposed) -> std::optional<int>
				{
					return (std::max)(1, proposed);
				} }, gridPlacementDesign(140)));
		auto dockDesign = PropertyDesign(
			L"Layout", 100, 150, ControlPropertyPersistence::Legacy,
			ControlPropertyEditorKind::Choice);
		dockDesign.Choices = {
			PropertyChoice(L"Left", Dock::Left),
			PropertyChoice(L"Top", Dock::Top),
			PropertyChoice(L"Right", Dock::Right),
			PropertyChoice(L"Bottom", Dock::Bottom),
			PropertyChoice(L"Fill", Dock::Fill)
		};
		dockDesign.DisplayName = L"Dock";
		dockDesign.BrowsableWhen = [](Control& target)
		{
			return target.Parent
				&& target.Parent->Type() == UIClass::UI_DockPanel;
		};
		BindingPropertyRegistry::Register<Control, Dock>(L"DockPosition",
			[](Control& target) { return target.DockPosition; },
			[](Control& target, const Dock& value) { target.DockPosition = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, Dock>{
				Dock::Fill, ControlPropertyFlags::AffectsMeasure }, std::move(dockDesign)));
		BindingPropertyRegistry::Register<Control, cui::core::Size>(L"MinSize",
			[](Control& target) { return target.GetMinSizeDip(); },
			[](Control& target, const cui::core::Size& value)
			{ target.SetMinSizeDip(value); },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, cui::core::Size>{
				cui::core::Size{ 0.0f, 0.0f },
				ControlPropertyFlags::AffectsMeasure,
				{}, {},
				[](const cui::core::Size& left, const cui::core::Size& right)
				{
					return left == right;
				} }, PropertyDesign(L"Layout", 100, 160,
					ControlPropertyPersistence::Metadata, ControlPropertyEditorKind::Size)));
		BindingPropertyRegistry::Register<Control, cui::core::Size>(L"MaxSize",
			[](Control& target) { return target.GetMaxSizeDip(); },
			[](Control& target, const cui::core::Size& value)
			{ target.SetMaxSizeDip(value); },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, cui::core::Size>{
				cui::core::Size{ cui::core::Infinity, cui::core::Infinity },
				ControlPropertyFlags::AffectsMeasure,
				{}, {},
				[](const cui::core::Size& left, const cui::core::Size& right)
				{
					return left == right;
				} }, PropertyDesign(L"Layout", 100, 170,
					ControlPropertyPersistence::Metadata, ControlPropertyEditorKind::Size)));
		BindingPropertyRegistry::Register<Control, D2D1_COLOR_F>(L"BackColor",
			[](Control& target) { return target.BackColor; },
			[](Control& target, const D2D1_COLOR_F& value) { target.BackColor = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, D2D1_COLOR_F>{
				cui::theme::palette::Surface,
				ControlPropertyFlags::AffectsRender,
				{}, {},
				[](const D2D1_COLOR_F& left, const D2D1_COLOR_F& right)
				{
					return left.r == right.r && left.g == right.g
						&& left.b == right.b && left.a == right.a;
				} }, PropertyDesign(L"Appearance", 200, 10,
					ControlPropertyPersistence::Legacy, ControlPropertyEditorKind::Color)));
		BindingPropertyRegistry::Register<Control, D2D1_COLOR_F>(L"ForeColor",
			[](Control& target) { return target.ForeColor; },
			[](Control& target, const D2D1_COLOR_F& value) { target.ForeColor = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, D2D1_COLOR_F>{
				cui::theme::palette::TextPrimary,
				ControlPropertyFlags::AffectsRender,
				{}, {},
				[](const D2D1_COLOR_F& left, const D2D1_COLOR_F& right)
				{
					return left.r == right.r && left.g == right.g
						&& left.b == right.b && left.a == right.a;
				} }, PropertyDesign(L"Appearance", 200, 20,
					ControlPropertyPersistence::Legacy, ControlPropertyEditorKind::Color)));
		ControlPropertyOptions<Control, cui::drawing::Brush> foregroundOptions;
		foregroundOptions.Flags = ControlPropertyFlags::AffectsRender;
		foregroundOptions.Equals = [](const cui::drawing::Brush& left,
			const cui::drawing::Brush& right)
		{
			return left.Kind == right.Kind
				&& left.MappingMode == right.MappingMode
				&& left.Color.r == right.Color.r
				&& left.Color.g == right.Color.g
				&& left.Color.b == right.Color.b
				&& left.Color.a == right.Color.a
				&& left.Opacity == right.Opacity
				&& left.StartPoint.x == right.StartPoint.x
				&& left.StartPoint.y == right.StartPoint.y
				&& left.EndPoint.x == right.EndPoint.x
				&& left.EndPoint.y == right.EndPoint.y
				&& left.Center.x == right.Center.x
				&& left.Center.y == right.Center.y
				&& left.GradientOrigin.x == right.GradientOrigin.x
				&& left.GradientOrigin.y == right.GradientOrigin.y
				&& left.RadiusX == right.RadiusX
				&& left.RadiusY == right.RadiusY
				&& left.GradientStops == right.GradientStops
				&& left.ImageSource == right.ImageSource
				&& left.Stretch == right.Stretch
				&& left.AlignmentX == right.AlignmentX
				&& left.AlignmentY == right.AlignmentY
				&& left.Transform == right.Transform
				&& left.RelativeTransform == right.RelativeTransform;
		};
		foregroundOptions.Design = PropertyDesign(
			L"Appearance", 200, 21, ControlPropertyPersistence::Metadata,
			ControlPropertyEditorKind::Text, L"Foreground");
		// Object editors are handled by XAML/Style resources in this batch.
		foregroundOptions.Design.Browsable = false;
		BindingPropertyRegistry::Register<Control, cui::drawing::Brush>(L"Foreground",
			[](Control& target)
			{
				if (const auto& brush = target.GetForegroundBrush(); brush)
					return *brush;
				cui::drawing::Brush fallback;
				fallback.Color = target.ForeColor;
				return fallback;
			},
			[](Control& target, const cui::drawing::Brush& value)
			{
				target.SetForegroundBrush(value);
			}, {}, std::move(foregroundOptions));

		ControlPropertyOptions<Control, std::shared_ptr<BitmapSource>> imageOptions;
		imageOptions.Flags = ControlPropertyFlags::AffectsRender;
		imageOptions.Equals = [](const std::shared_ptr<BitmapSource>& left,
			const std::shared_ptr<BitmapSource>& right)
		{
			return left == right;
		};
		imageOptions.Design = PropertyDesign(
			L"Appearance", 200, 25, ControlPropertyPersistence::Metadata,
			ControlPropertyEditorKind::Text, L"Image source");
		imageOptions.Design.BrowsableWhen = [](Control& target)
		{
			return target.Type() == UIClass::UI_PictureBox;
		};
		BindingPropertyRegistry::Register<Control, std::shared_ptr<BitmapSource>>(
			L"ImageSource",
			[](Control& target) { return target.Image; },
			[](Control& target, const std::shared_ptr<BitmapSource>& value)
			{
				target.SetImageEx(value);
			}, {}, std::move(imageOptions));

		ControlPropertyOptions<Control, cui::drawing::Geometry> clipOptions;
		clipOptions.DefaultValue = cui::drawing::Geometry{};
		clipOptions.Flags = ControlPropertyFlags::AffectsRender;
		clipOptions.Equals = [](const cui::drawing::Geometry& left,
			const cui::drawing::Geometry& right) { return left == right; };
		clipOptions.Design = PropertyDesign(
			L"Appearance", 200, 26, ControlPropertyPersistence::Metadata,
			ControlPropertyEditorKind::Text, L"Clip geometry");
		clipOptions.Design.Browsable = false;
		BindingPropertyRegistry::Register<Control, cui::drawing::Geometry>(L"Clip",
			[](Control& target)
			{
				return target.GetClip().value_or(cui::drawing::Geometry{});
			},
			[](Control& target, const cui::drawing::Geometry& value)
			{
				if (value == cui::drawing::Geometry{}) target.ClearClip();
				else target.SetClip(value);
			}, {}, std::move(clipOptions));

		ControlPropertyOptions<Control, cui::drawing::Transform> transformOptions;
		transformOptions.DefaultValue = cui::drawing::Transform{};
		transformOptions.Flags = ControlPropertyFlags::AffectsRender;
		transformOptions.Equals = [](const cui::drawing::Transform& left,
			const cui::drawing::Transform& right) { return left == right; };
		transformOptions.Design = PropertyDesign(
			L"Appearance", 200, 27, ControlPropertyPersistence::Metadata,
			ControlPropertyEditorKind::Text, L"Render transform");
		transformOptions.Design.Browsable = false;
		BindingPropertyRegistry::Register<Control, cui::drawing::Transform>(
			L"RenderTransform",
			[](Control& target)
			{
				return target.GetRenderTransform().value_or(cui::drawing::Transform{});
			},
			[](Control& target, const cui::drawing::Transform& value)
			{
				target.SetRenderTransform(value);
			}, {}, std::move(transformOptions));
		BindingPropertyRegistry::Register<Control, cui::core::Point>(
			L"RenderTransformOrigin",
			[](Control& target) { return target.GetRenderTransformOriginDip(); },
			[](Control& target, const cui::core::Point& value)
			{ target.SetRenderTransformOriginDip(value); },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, cui::core::Point>{
				cui::core::Point{},
				ControlPropertyFlags::AffectsRender,
				[](Control&, const cui::core::Point& value)
					-> std::optional<cui::core::Point>
				{
					return std::isfinite(value.x) && std::isfinite(value.y)
						? std::optional<cui::core::Point>(value) : std::nullopt;
				}, {},
				[](const cui::core::Point& left, const cui::core::Point& right)
				{
					return left == right;
				} }, PropertyDesign(L"Appearance", 200, 28,
					ControlPropertyPersistence::Metadata,
					ControlPropertyEditorKind::Text, L"Transform origin")));
		BindingPropertyRegistry::Register<Control, D2D1_COLOR_F>(L"BorderColor",
			[](Control& target) { return target.BorderColor; },
			[](Control& target, const D2D1_COLOR_F& value) { target.BorderColor = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, D2D1_COLOR_F>{
				cui::theme::palette::Border,
				ControlPropertyFlags::AffectsRender,
				{}, {},
				[](const D2D1_COLOR_F& left, const D2D1_COLOR_F& right)
				{
					return left.r == right.r && left.g == right.g
						&& left.b == right.b && left.a == right.a;
				} }, PropertyDesign(L"Appearance", 200, 30,
					ControlPropertyPersistence::Legacy, ControlPropertyEditorKind::Color)));
		BindingPropertyRegistry::Register<Control, bool>(L"ShowValidationBorder",
			[](Control& target) { return target.ShowValidationBorder; },
			[](Control& target, const bool& value) { target.ShowValidationBorder = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, bool>{
				true, ControlPropertyFlags::AffectsRender },
				PropertyDesign(L"Validation", 400, 10, ControlPropertyPersistence::Legacy)));
		BindingPropertyRegistry::Register<Control, bool>(L"ShowValidationToolTip",
			[](Control& target) { return target.ShowValidationToolTip; },
			[](Control& target, const bool& value) { target.ShowValidationToolTip = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, bool>{
				true,
				ControlPropertyFlags::None,
				{},
				[](Control& target, const bool&, const bool&)
				{
					if (target.ParentForm && target.ParentForm->UnderMouse == &target)
						target.ParentForm->Invalidate(false);
				} }, PropertyDesign(L"Validation", 400, 20,
					ControlPropertyPersistence::Legacy)));
		BindingPropertyRegistry::Register<Control, float>(L"ValidationBorderThickness",
			[](Control& target) { return target.ValidationBorderThickness; },
			[](Control& target, const float& value) { target.ValidationBorderThickness = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, float>{
				2.0f,
				ControlPropertyFlags::AffectsRender,
				[](Control&, const float& proposed) -> std::optional<float>
				{
					const float value = std::isfinite(proposed) ? proposed : 2.0f;
					return (std::clamp)(value, 0.0f, 16.0f);
				} }, PropertyDesign(L"Validation", 400, 30,
					ControlPropertyPersistence::Legacy, ControlPropertyEditorKind::Number)));
		BindingPropertyRegistry::Register<Control, float>(L"ValidationCornerRadius",
			[](Control& target) { return target.ValidationCornerRadius; },
			[](Control& target, const float& value) { target.ValidationCornerRadius = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, float>{
				4.0f,
				ControlPropertyFlags::AffectsRender,
				[](Control&, const float& proposed) -> std::optional<float>
				{
					const float value = std::isfinite(proposed) ? proposed : 4.0f;
					return (std::clamp)(value, 0.0f, 1000.0f);
				} }, PropertyDesign(L"Validation", 400, 40,
					ControlPropertyPersistence::Legacy, ControlPropertyEditorKind::Number)));
		BindingPropertyRegistry::Register<Control, float>(L"ValidationToolTipMaxWidth",
			[](Control& target) { return target.ValidationToolTipMaxWidth; },
			[](Control& target, const float& value) { target.ValidationToolTipMaxWidth = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, float>{
				320.0f,
				ControlPropertyFlags::None,
				[](Control&, const float& proposed) -> std::optional<float>
				{
					const float value = std::isfinite(proposed) ? proposed : 320.0f;
					return (std::clamp)(value, 120.0f, 1000.0f);
				},
				[](Control& target, const float&, const float&)
				{
					if (target.ParentForm && target.ParentForm->UnderMouse == &target)
						target.ParentForm->Invalidate(false);
				} }, PropertyDesign(L"Validation", 400, 50,
					ControlPropertyPersistence::Legacy, ControlPropertyEditorKind::Number)));
		BindingPropertyRegistry::Register<Control, bool>(L"IsTabStop",
			[](Control& target) { return target.IsTabStop; },
			[](Control& target, const bool& value) { target.IsTabStop = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, bool>{ true },
				PropertyDesign(L"Behavior", 300, 20,
					ControlPropertyPersistence::Metadata,
					ControlPropertyEditorKind::Boolean)));
		BindingPropertyRegistry::Register<Control, int>(L"TabIndex",
			[](Control& target) { return target.TabIndex; },
			[](Control& target, const int& value) { target.TabIndex = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, int>{
				0, ControlPropertyFlags::None,
				[](Control&, const int& proposed) -> std::optional<int>
				{
					return (std::max)(0, proposed);
				} }, PropertyDesign(L"Behavior", 300, 30,
					ControlPropertyPersistence::Metadata,
					ControlPropertyEditorKind::Number)));
		BindingPropertyRegistry::Register<Control, std::wstring>(L"AccessKey",
			[](Control& target) { return target.AccessKey; },
			[](Control& target, const std::wstring& value) { target.AccessKey = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, std::wstring>{
				std::wstring{}, ControlPropertyFlags::None,
				[](Control&, const std::wstring& proposed) -> std::optional<std::wstring>
				{
					if (proposed.empty()) return std::wstring{};
					return std::wstring(1, static_cast<wchar_t>(std::towupper(proposed.front())));
				} }, PropertyDesign(L"Accessibility", 500, 10,
					ControlPropertyPersistence::Metadata,
					ControlPropertyEditorKind::Text)));
		BindingPropertyRegistry::Register<Control, std::wstring>(L"AccessibleName",
			[](Control& target) { return target.AccessibleName; },
			[](Control& target, const std::wstring& value) { target.AccessibleName = value; },
			{},
			WithPropertyDesign(
				ControlPropertyOptions<Control, std::wstring>{ std::wstring{} },
				PropertyDesign(L"Accessibility", 500, 20,
					ControlPropertyPersistence::Metadata, ControlPropertyEditorKind::Text)));
		BindingPropertyRegistry::Register<Control, std::wstring>(L"AccessibleDescription",
			[](Control& target) { return target.AccessibleDescription; },
			[](Control& target, const std::wstring& value) { target.AccessibleDescription = value; },
			{},
			WithPropertyDesign(
				ControlPropertyOptions<Control, std::wstring>{ std::wstring{} },
				PropertyDesign(L"Accessibility", 500, 30,
					ControlPropertyPersistence::Legacy, ControlPropertyEditorKind::Text)));
		BindingPropertyRegistry::Register<Control, std::wstring>(L"AccessibleHelpText",
			[](Control& target) { return target.AccessibleHelpText; },
			[](Control& target, const std::wstring& value) { target.AccessibleHelpText = value; },
			{},
			WithPropertyDesign(
				ControlPropertyOptions<Control, std::wstring>{ std::wstring{} },
				PropertyDesign(L"Accessibility", 500, 40,
					ControlPropertyPersistence::Metadata, ControlPropertyEditorKind::Text)));
		BindingPropertyRegistry::Register<Control, std::wstring>(L"AutomationId",
			[](Control& target) { return target.AutomationId; },
			[](Control& target, const std::wstring& value) { target.AutomationId = value; },
			{},
			WithPropertyDesign(
				ControlPropertyOptions<Control, std::wstring>{ std::wstring{} },
				PropertyDesign(L"Accessibility", 500, 50,
					ControlPropertyPersistence::Metadata, ControlPropertyEditorKind::Text)));
		auto roleDesign = PropertyDesign(L"Accessibility", 500, 60,
			ControlPropertyPersistence::Metadata, ControlPropertyEditorKind::Choice);
		roleDesign.Choices = {
			PropertyChoice(L"Default", ::AccessibleRole::Default),
			PropertyChoice(L"Pane", ::AccessibleRole::Pane),
			PropertyChoice(L"Group", ::AccessibleRole::Group),
			PropertyChoice(L"Text", ::AccessibleRole::Text),
			PropertyChoice(L"Link", ::AccessibleRole::Link),
			PropertyChoice(L"Button", ::AccessibleRole::Button),
			PropertyChoice(L"Check box", ::AccessibleRole::CheckBox),
			PropertyChoice(L"Radio button", ::AccessibleRole::RadioButton),
			PropertyChoice(L"Switch", ::AccessibleRole::Switch),
			PropertyChoice(L"Text box", ::AccessibleRole::TextBox),
			PropertyChoice(L"Password box", ::AccessibleRole::PasswordBox),
			PropertyChoice(L"Combo box", ::AccessibleRole::ComboBox),
			PropertyChoice(L"List", ::AccessibleRole::List),
			PropertyChoice(L"Table", ::AccessibleRole::Table),
			PropertyChoice(L"Tree", ::AccessibleRole::Tree),
			PropertyChoice(L"Tab", ::AccessibleRole::Tab),
			PropertyChoice(L"Menu", ::AccessibleRole::Menu),
			PropertyChoice(L"Slider", ::AccessibleRole::Slider),
			PropertyChoice(L"Progress bar", ::AccessibleRole::ProgressBar),
			PropertyChoice(L"Image", ::AccessibleRole::Image),
			PropertyChoice(L"Document", ::AccessibleRole::Document),
			PropertyChoice(L"Custom", ::AccessibleRole::Custom)
		};
		BindingPropertyRegistry::Register<Control, ::AccessibleRole>(L"AccessibleRole",
			[](Control& target) { return target.AccessibleRole; },
			[](Control& target, const ::AccessibleRole& value) { target.AccessibleRole = value; },
			{}, WithPropertyDesign(ControlPropertyOptions<Control, ::AccessibleRole>{
				::AccessibleRole::Default }, std::move(roleDesign)));
		BindingPropertyRegistry::Register<Control, bool>(L"ShowFocusVisual",
			[](Control& target) { return target.ShowFocusVisual; },
			[](Control& target, const bool& value) { target.ShowFocusVisual = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, bool>{
				true, ControlPropertyFlags::AffectsRender },
				PropertyDesign(L"Accessibility", 500, 70,
					ControlPropertyPersistence::Metadata,
					ControlPropertyEditorKind::Boolean)));
		BindingPropertyRegistry::Register<Control, D2D1_COLOR_F>(L"FocusVisualColor",
			[](Control& target) { return target.FocusVisualColor; },
			[](Control& target, const D2D1_COLOR_F& value) { target.FocusVisualColor = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, D2D1_COLOR_F>{
				D2D1_COLOR_F{ 0.20f, 0.46f, 0.90f, 0.95f },
				ControlPropertyFlags::AffectsRender, {}, {},
				[](const D2D1_COLOR_F& left, const D2D1_COLOR_F& right)
				{
					return left.r == right.r && left.g == right.g
						&& left.b == right.b && left.a == right.a;
				} }, PropertyDesign(L"Accessibility", 500, 80,
					ControlPropertyPersistence::Metadata,
					ControlPropertyEditorKind::Color)));
		BindingPropertyRegistry::Register<Control, float>(L"FocusVisualThickness",
			[](Control& target) { return target.FocusVisualThickness; },
			[](Control& target, const float& value) { target.FocusVisualThickness = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, float>{
				1.5f, ControlPropertyFlags::AffectsRender,
				[](Control&, const float& proposed) -> std::optional<float>
				{
					return (std::clamp)(std::isfinite(proposed) ? proposed : 1.5f, 0.0f, 8.0f);
				} }, PropertyDesign(L"Accessibility", 500, 90,
					ControlPropertyPersistence::Metadata,
					ControlPropertyEditorKind::Number)));
	});
}

GET_CPP(Control, int, Count)
{
	return this->Children.size();
}
Control* Control::operator[](int index)
{
	return this->Children[index];
}
Control* Control::GetChild(int index)
{
	if (this->Children.size() <= index)
		return nullptr;
	return this->Children[index];
}

Control* Control::FindControlByDesignId(int designId) noexcept
{
	return const_cast<Control*>(
		static_cast<const Control*>(this)->FindControlByDesignId(designId));
}

const Control* Control::FindControlByDesignId(int designId) const noexcept
{
	if (designId <= 0) return nullptr;
	if (DesignId == designId) return this;
	for (const auto* child : Children)
	{
		if (!child) continue;
		if (const auto* match = child->FindControlByDesignId(designId))
			return match;
	}
	return nullptr;
}

std::vector<Control*> Control::GetChildrenInZOrder() const
{
	std::vector<Control*> result = this->Children;
	std::stable_sort(result.begin(), result.end(), [](Control* left, Control* right)
		{
			if (!left || !right) return left != nullptr;
			return left->ZIndex < right->ZIndex;
		});
	return result;
}

std::vector<Control*> Control::GetChildrenInReverseZOrder() const
{
	auto result = GetChildrenInZOrder();
	std::reverse(result.begin(), result.end());
	return result;
}
std::unique_ptr<Control> Control::DetachControl(Control* child)
{
	if (!child)
		return {};
	auto position = std::find(this->Children.begin(), this->Children.end(), child);
	if (position == this->Children.end())
		return {};

	this->Children.erase(position);
	return std::unique_ptr<Control>(child);
}

std::unique_ptr<Control> Control::DetachControlAt(int index)
{
	if (index < 0 || static_cast<size_t>(index) >= Children.size())
		return {};
	return DetachControl(Children[static_cast<size_t>(index)]);
}

bool Control::DeleteControl(Control* child)
{
	auto detached = DetachControl(child);
	return detached != nullptr;
}

bool Control::DeleteControlAt(int index)
{
	auto detached = DetachControlAt(index);
	return detached != nullptr;
}

void Control::ClearControls()
{
	if (Children.empty()) return;
	std::vector<Control*> removed(Children.begin(), Children.end());
	Children.clear();
	for (auto* child : removed) delete child;
}

int Control::IndexOfControl(const Control* child) const noexcept
{
	if (!child) return -1;
	auto found = std::find(Children.begin(), Children.end(), child);
	return found == Children.end()
		? -1 : static_cast<int>(found - Children.begin());
}

void Control::RemoveControl(Control* child)
{
	auto detached = DetachControl(child);
	detached.release();
}
GET_CPP(Control, POINT, AbsLocation)
{
	const auto absoluteLocation = GetAbsoluteLocationDip();
	return POINT{
		ToCoordinateLong(absoluteLocation.x),
		ToCoordinateLong(absoluteLocation.y) };
}
GET_CPP(Control, POINT, ActualLocation)
{
	return _runtimeLocation;
}
GET_CPP(Control, D2D1_RECT_F, AbsRect)
{
	const auto rect = GetAbsoluteRectDip();
	return D2D1_RECT_F{
		rect.Left(), rect.Top(), rect.Right(), rect.Bottom()
	};
}
GET_CPP(Control, bool, IsVisual)
{
	if (!this->_visible) return false;
	Control* ancestor = this;
	while (ancestor->Parent)
	{
		ancestor = ancestor->Parent;
		if (!ancestor->Visible) return false;
	}
	return true;
}
GET_CPP(Control, bool, Visible)
{
	return this->_visible;
}
SET_CPP(Control, bool, Visible)
{
	if (this->_visible == value)
		return;

	this->_visible = value;
	this->RequestLayout();

	if (this->ParentForm)
	{
		this->ParentForm->Invalidate(false);
		this->ParentForm->NotifyAccessibilityEvent(
			this, AccessibilityChange::State);
	}
}
GET_CPP(Control, POINT, Location)
{
	return _location;
}
SET_CPP(Control, POINT, Location)
{
	// 收敛几何写路径：_location 是用户配置，_runtimeLocation/_layoutState 是
	// 运行时投影。过去这里同时直写两份，与 ApplyLayout 的布局回写并存，是
	// 漂移源。现在统一经由 SetRuntimeLocation->ApplyLayout 更新运行时投影，
	// 让 _layoutState 成为运行时几何的唯一权威，兼容字段仅作为其投影。
	const POINT oldConfiguredLocation = this->_location;
	const bool configuredChanged =
		oldConfiguredLocation.x != value.x || oldConfiguredLocation.y != value.y;
	const POINT oldRuntimeLocation = this->_runtimeLocation;
	_location = value;
	this->SetRuntimeLocation(value);
	this->RequestLayout();
	// 仅当配置变化但运行时坐标未变（布局被锁定/覆盖）时补发 OnMoved，
	// 避免与 ApplyLayout 内部的事件重复。
	const bool runtimeChanged =
		oldRuntimeLocation.x != _runtimeLocation.x || oldRuntimeLocation.y != _runtimeLocation.y;
	if (configuredChanged && !runtimeChanged)
	{
		this->OnMoved(this);
	}
	this->InvalidateVisual();
}
GET_CPP(Control, SIZE, Size)
{
	return _size;
}
SET_CPP(Control, SIZE, Size)
{
	const bool specifiedChanged = !_layoutStyle.width.IsFixed()
		|| !_layoutStyle.height.IsFixed()
		|| _layoutStyle.width.value != (float)(std::max)(0L, value.cx)
		|| _layoutStyle.height.value != (float)(std::max)(0L, value.cy);
	const bool actualChanged = _size.cx != value.cx || _size.cy != value.cy;
	if (!specifiedChanged && !actualChanged)
		return;

	_size = value;
	this->UpdateLayoutBaseSize(value);
	this->SyncComputedLayoutFromCompatibilityGeometry();
	this->RequestLayout();
	if (actualChanged)
		this->OnSizeChanged(this);
	this->InvalidateVisual();
}
GET_CPP(Control, int, Left)
{
	return this->_location.x;
}
SET_CPP(Control, int, Left)
{
	this->Location = POINT{ value, this->_location.y };
}
GET_CPP(Control, int, Top)
{
	return this->_location.y;
}
SET_CPP(Control, int, Top)
{
	this->Location = POINT{ this->_location.x, value };
}
GET_CPP(Control, int, Width)
{
	return this->_size.cx;
}
SET_CPP(Control, int, Width)
{
	const auto specifiedWidth = cui::layout::Length::Fixed((float)value);
	const bool specifiedChanged = _layoutStyle.width != specifiedWidth;
	const bool actualChanged = this->_size.cx != value;
	if (!specifiedChanged && !actualChanged)
		return;

	this->_size.cx = value;
	this->_layoutStyle.width = specifiedWidth;
	this->SyncComputedLayoutFromCompatibilityGeometry();
	this->RequestLayout();
	if (actualChanged)
		this->OnSizeChanged(this);
	this->InvalidateVisual();
}
GET_CPP(Control, int, Height)
{
	return this->_size.cy;
}
SET_CPP(Control, int, Height)
{
	const auto specifiedHeight = cui::layout::Length::Fixed((float)value);
	const bool specifiedChanged = _layoutStyle.height != specifiedHeight;
	const bool actualChanged = this->_size.cy != value;
	if (!specifiedChanged && !actualChanged)
		return;

	_size.cy = value;
	this->_layoutStyle.height = specifiedHeight;
	this->SyncComputedLayoutFromCompatibilityGeometry();
	this->RequestLayout();
	if (actualChanged)
		this->OnSizeChanged(this);
	this->InvalidateVisual();
}
GET_CPP(Control, float, Right)
{
	return this->Left + this->Width;
}
GET_CPP(Control, float, Bottom)
{
	return this->Top + this->Height;
}
GET_CPP(Control, std::wstring, Text)
{
	return _text;
}
SET_CPP(Control, std::wstring, Text)
{
	if (value != _text)
	{
		this->TextChanged = true;
		std::wstring oldValue = _text;
		_text = std::move(value);
		this->OnTextChanged(this, std::move(oldValue), _text);
		if (ParentForm)
		{
			ParentForm->NotifyAccessibilityEvent(this, AccessibilityChange::Name);
			ParentForm->NotifyAccessibilityEvent(this, AccessibilityChange::Value);
		}
		this->RequestLayout();
		this->InvalidateVisual();
		return;
	}
	_text = value;
}
GET_CPP(Control, D2D1_COLOR_F, BorderColor)
{
	return _bordercolor;
}
SET_CPP(Control, D2D1_COLOR_F, BorderColor)
{
	SetPropertyField(L"BorderColor", _bordercolor, value);
}
GET_CPP(Control, D2D1_COLOR_F, BackColor)
{
	return ParentForm
		? ParentForm->GetEffectiveControlBackColor(_backcolor) : _backcolor;
}
SET_CPP(Control, D2D1_COLOR_F, BackColor)
{
	SetPropertyField(L"BackColor", _backcolor, value);
}
GET_CPP(Control, D2D1_COLOR_F, ForeColor)
{
	return ParentForm
		? ParentForm->GetEffectiveControlForeColor(_forecolor) : _forecolor;
}
SET_CPP(Control, D2D1_COLOR_F, ForeColor)
{
	SetPropertyField(L"ForeColor", _forecolor, value);
}
GET_CPP(Control, std::shared_ptr<BitmapSource>, Image)
{
	return _imageSource;
}
SET_CPP(Control, std::shared_ptr<BitmapSource>, Image)
{
	this->SetImageEx(std::move(value));
}

void Control::SetImageEx(std::shared_ptr<BitmapSource> value)
{
	if (value == this->_imageSource)
		return;
	this->_imageSource = std::move(value);
	this->_imageCache.Reset();
	this->_imageCacheTarget = nullptr;
	this->InvalidateVisual();
}

ID2D1Bitmap* Control::EnsureImageCache()
{
	if (!this->_imageSource || !this->ParentForm || !this->ParentForm->Render)
		return nullptr;
	auto* target = this->ParentForm->Render->GetRenderTargetRaw();
	if (!target)
		return nullptr;
	if (this->_imageCache && this->_imageCacheTarget == target)
		return this->_imageCache.Get();
	this->_imageCache.Reset();
	this->_imageCacheTarget = target;
	auto* bmp = this->ParentForm->Render->CreateBitmap(this->_imageSource);
	if (!bmp)
		return nullptr;
	this->_imageCache.Attach(bmp);
	return this->_imageCache.Get();
}
void Control::RenderImage(float cornerRadius)
{
	auto* bitmap = this->EnsureImageCache();
	if (bitmap)
	{
		auto imageSize = bitmap->GetSize();
		if (imageSize.width > 0 && imageSize.height > 0)
		{
			auto actualSize = this->GetActualSizeDip();
			const float clipRadius = (std::clamp)(cornerRadius, 0.0f, (std::min)(actualSize.width, actualSize.height) * 0.5f);
			const bool clipPushed = clipRadius > 0.0f && this->ParentForm && this->ParentForm->Render &&
				this->ParentForm->Render->PushRoundClip(0.0f, 0.0f, actualSize.width, actualSize.height, clipRadius);
			switch (this->SizeMode)
			{
			case ImageSizeMode::Normal:
			{
				this->ParentForm->Render->DrawBitmap(bitmap, 0.0f, 0.0f, imageSize.width, imageSize.height);
			}
			break;
			case ImageSizeMode::CenterImage:
			{
				this->ParentForm->Render->DrawBitmap(bitmap, (actualSize.width - imageSize.width) / 2.0f, (actualSize.height - imageSize.height) / 2.0f, imageSize.width, imageSize.height);
			}
			break;
			case ImageSizeMode::StretchImage:
			{
				this->ParentForm->Render->DrawBitmap(bitmap, 0.0f, 0.0f, actualSize.width, actualSize.height);
			}
			break;
			case ImageSizeMode::Zoom:
			{
				float scaleX = actualSize.width / imageSize.width;
				float scaleY = actualSize.height / imageSize.height;
				float scale = scaleX < scaleY ? scaleX : scaleY;
				float renderWidth = imageSize.width * scale;
				float renderHeight = imageSize.height * scale;
				float renderX = (actualSize.width - renderWidth) / 2.0f;
				float renderY = (actualSize.height - renderHeight) / 2.0f;
				this->ParentForm->Render->DrawBitmap(bitmap, renderX, renderY, renderWidth, renderHeight);
			}
			break;
			default:
				break;
			}
			if (clipPushed)
				this->ParentForm->Render->PopRoundClip();
		}
	}
}
SIZE Control::ActualSize()
{
	return this->_size;
}

bool Control::IsSelected() const
{
	return this->ParentForm && this->ParentForm->Selected == this;
}

bool Control::DispatchMessage(
	UINT message,
	WPARAM wParam,
	LPARAM lParam,
	int localX,
	int localY)
{
	const bool previousDispatch = _dispatchingComponentBehaviorMessage;
	_dispatchingComponentBehaviorMessage = true;
	if (!previousDispatch && _declarativeComponentBehavior)
	{
		bool handled = false;
		try
		{
			handled = _declarativeComponentBehavior->HandleMessage(
				*this, message, wParam, lParam, localX, localY);
		}
		catch (...)
		{
		}
		if (handled)
		{
			_dispatchingComponentBehaviorMessage = previousDispatch;
			return true;
		}
	}
	try
	{
		const bool result = ProcessMessage(
			message, wParam, lParam, localX, localY);
		_dispatchingComponentBehaviorMessage = previousDispatch;
		return result;
	}
	catch (...)
	{
		_dispatchingComponentBehaviorMessage = previousDispatch;
		throw;
	}
}

bool Control::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
	if (!_dispatchingComponentBehaviorMessage
		&& _declarativeComponentBehavior)
		return DispatchMessage(message, wParam, lParam, localX, localY);
	if (!this->Enable || !this->Visible) return true;
	switch (message)
	{
	case WM_DROPFILES:
	{
		HDROP hDropInfo = HDROP(wParam);
		UINT fileCount = DragQueryFile(hDropInfo, 0xffffffff, nullptr, 0);
		TCHAR fileName[MAX_PATH];
		std::vector<std::wstring> files;
		for (UINT fileIndex = 0; fileIndex < fileCount; fileIndex++)
		{
			DragQueryFile(hDropInfo, fileIndex, fileName, MAX_PATH);
			files.push_back(fileName);
		}
		DragFinish(hDropInfo);
		if (files.size() > 0)
		{
			this->OnDropFile(this, files);
		}
	}
	break;
	case WM_MOUSEWHEEL:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, GET_WHEEL_DELTA_WPARAM(wParam));
		this->OnMouseWheel(this, eventArgs);
	}
	break;
	case WM_MOUSEMOVE:
	{
		MouseEventArgs eventArgs = MouseEventArgs(MouseButtons::None, 0, localX, localY, HIWORD(wParam));
		if (this->ParentForm && this->DefaultTrackUnderMouse())
			this->ParentForm->UnderMouse = this;
		this->BeforeDefaultMouseMove(eventArgs);
		this->OnMouseMove(this, eventArgs);
	}
	break;
	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	{
		if (WM_LBUTTONDOWN == message)
			_defaultLeftButtonPressActive = true;
		if (WM_LBUTTONDOWN == message && this->ParentForm && this->DefaultSelectOnLeftButtonDown())
		{
			this->ParentForm->SetSelectedControl(this, false);
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->BeforeDefaultMouseDown(message, eventArgs);
		this->OnMouseDown(this, eventArgs);
		if (this->DefaultInvalidateVisualOnMouseDown(message))
			this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	{
		bool wasSelected = this->ParentForm && this->ParentForm->Selected == this;
		const bool hasMatchingPress = message != WM_LBUTTONUP
			|| _defaultLeftButtonPressActive;
		if (message == WM_LBUTTONUP)
			_defaultLeftButtonPressActive = false;
		const bool selectedForDefaultAction = wasSelected && hasMatchingPress;
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->BeforeDefaultMouseUp(message, eventArgs, selectedForDefaultAction);
		if (WM_LBUTTONUP == message && selectedForDefaultAction && this->DefaultRaiseClickOnLeftButtonUp())
		{
			this->BeforeDefaultClick(message, eventArgs);
			this->OnMouseClick(this, eventArgs);
		}
		if (selectedForDefaultAction && this->DefaultClearSelectionOnMouseUp() && this->ParentForm && this->ParentForm->Selected == this)
		{
			this->ParentForm->SetSelectedControl(nullptr, false);
		}
		this->OnMouseUp(this, eventArgs);
		if (this->DefaultInvalidateVisualOnMouseUp(message))
			this->InvalidateVisual();
	}
	break;
	case WM_LBUTTONDBLCLK:
	{
		_defaultLeftButtonPressActive = true;
		bool wasSelected = this->ParentForm && this->ParentForm->Selected == this;
		if (this->ParentForm && this->DefaultSelectOnLeftButtonDoubleClick())
		{
			this->ParentForm->SetSelectedControl(this, false);
		}
		MouseEventArgs eventArgs = MouseEventArgs(FromParamToMouseButtons(message), 0, localX, localY, HIWORD(wParam));
		this->BeforeDefaultMouseDoubleClick(message, eventArgs, wasSelected);
		if (this->DefaultRaiseMouseDoubleClick(message, wasSelected))
			this->OnMouseDoubleClick(this, eventArgs);
		if (this->DefaultInvalidateVisualOnMouseDoubleClick(message, wasSelected))
			this->InvalidateVisual();
	}
	break;
	case WM_CANCELMODE:
	case WM_CAPTURECHANGED:
		_defaultLeftButtonPressActive = false;
		SetStyleState(ControlStyleState::Pressed, false);
		break;
	case WM_KEYDOWN:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyDown(this, eventArgs);
	}
	break;
	case WM_KEYUP:
	{
		KeyEventArgs eventArgs = KeyEventArgs((Keys)(wParam | 0));
		this->OnKeyUp(this, eventArgs);
	}
	break;
	}
	return true;
}

// 布局属性实现
GET_CPP(Control, Thickness, Margin)
{
	return _margin;
}
SET_CPP(Control, Thickness, Margin)
{
	if (_margin != value)
	{
		_margin = value;
		_layoutStyle.margin = cui::core::Insets{
			value.Left, value.Top, value.Right, value.Bottom };
		this->RequestLayout();
		this->InvalidateVisual();
	}
}

GET_CPP(Control, Thickness, Padding)
{
	return _padding;
}
SET_CPP(Control, Thickness, Padding)
{
	if (_padding != value)
	{
		_padding = value;
		_layoutStyle.padding = cui::core::Insets{
			value.Left, value.Top, value.Right, value.Bottom };
		this->RequestLayout();
		this->InvalidateVisual();
	}
}

GET_CPP(Control, HorizontalAlignment, HAlign)
{
	return _horizontalAlignment;
}
SET_CPP(Control, HorizontalAlignment, HAlign)
{
	if (_horizontalAlignment == value)
		return;
	_horizontalAlignment = value;
	_layoutStyle.horizontalAlignment = ToLayoutAlignment(value);
	this->RequestLayout();
	this->InvalidateVisual();
}

void Control::SetLayoutWidth(cui::layout::Length value)
{
	if (value.IsFixed())
		value = cui::layout::Length::Fixed(value.value);
	else
		value = cui::layout::Length::Auto();
	if (_layoutStyle.width == value) return;
	_layoutStyle.width = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

void Control::SetLayoutHeight(cui::layout::Length value)
{
	if (value.IsFixed())
		value = cui::layout::Length::Fixed(value.value);
	else
		value = cui::layout::Length::Auto();
	if (_layoutStyle.height == value) return;
	_layoutStyle.height = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

void Control::SetAutoSize(bool width, bool height)
{
	bool changed = false;
	if (width && !_layoutStyle.width.IsAuto())
	{
		_layoutStyle.width = cui::layout::Length::Auto();
		changed = true;
	}
	if (height && !_layoutStyle.height.IsAuto())
	{
		_layoutStyle.height = cui::layout::Length::Auto();
		changed = true;
	}
	if (!changed) return;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, VerticalAlignment, VAlign)
{
	return _verticalAlignment;
}
SET_CPP(Control, VerticalAlignment, VAlign)
{
	if (_verticalAlignment == value)
		return;
	_verticalAlignment = value;
	_layoutStyle.verticalAlignment = ToLayoutAlignment(value);
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, uint8_t, AnchorStyles)
{
	return _anchorStyles;
}
SET_CPP(Control, uint8_t, AnchorStyles)
{
	_anchorStyles = value;
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, int, GridRow)
{
	return _gridRow;
}
SET_CPP(Control, int, GridRow)
{
	SetPropertyField(L"GridRow", _gridRow, value);
}

GET_CPP(Control, int, GridColumn)
{
	return _gridColumn;
}
SET_CPP(Control, int, GridColumn)
{
	SetPropertyField(L"GridColumn", _gridColumn, value);
}

GET_CPP(Control, int, GridRowSpan)
{
	return _gridRowSpan;
}
SET_CPP(Control, int, GridRowSpan)
{
	SetPropertyField(L"GridRowSpan", _gridRowSpan, value);
}

GET_CPP(Control, int, GridColumnSpan)
{
	return _gridColumnSpan;
}
SET_CPP(Control, int, GridColumnSpan)
{
	SetPropertyField(L"GridColumnSpan", _gridColumnSpan, value);
}

GET_CPP(Control, Dock, DockPosition)
{
	return _dock;
}
SET_CPP(Control, Dock, DockPosition)
{
	SetPropertyField(L"DockPosition", _dock, value);
}

GET_CPP(Control, SIZE, MinSize)
{
	return _minSize;
}
SET_CPP(Control, SIZE, MinSize)
{
	SetMinSizeDip(cui::core::Size{
		static_cast<float>(value.cx), static_cast<float>(value.cy) });
}

cui::core::Size Control::GetMinSizeDip() const noexcept
{
	return _layoutStyle.minimumSize;
}

void Control::SetMinSizeDip(cui::core::Size value)
{
	value.width = std::isfinite(value.width)
		? (std::max)(0.0f, value.width) : 0.0f;
	value.height = std::isfinite(value.height)
		? (std::max)(0.0f, value.height) : 0.0f;
	if (!SetPropertyField(L"MinSize", _layoutStyle.minimumSize, value)) return;
	_minSize = SIZE{ ToMeasureLong(value.width), ToMeasureLong(value.height) };
}

GET_CPP(Control, SIZE, MaxSize)
{
	return _maxSize;
}
SET_CPP(Control, SIZE, MaxSize)
{
	SetMaxSizeDip(cui::core::Size{
		ToMaximumDip(value.cx), ToMaximumDip(value.cy) });
}

cui::core::Size Control::GetMaxSizeDip() const noexcept
{
	return _layoutStyle.maximumSize;
}

void Control::SetMaxSizeDip(cui::core::Size value)
{
	auto normalize = [](float item)
	{
		if (std::isnan(item) || item < 0.0f) return 0.0f;
		return item;
	};
	value.width = normalize(value.width);
	value.height = normalize(value.height);
	if (!SetPropertyField(L"MaxSize", _layoutStyle.maximumSize, value)) return;
	_maxSize = SIZE{ ToMeasureLong(value.width), ToMeasureLong(value.height) };
}

// 测量控件期望尺寸。浮点入口是主路径；默认实现转发到旧扩展点，
// 因而现有自定义控件仍可逐步迁移。
cui::core::Size Control::MeasureCore(const cui::core::Constraints& available)
{
	const auto maximum = available.Normalized().maximum;
	const SIZE legacyDesired = MeasureCore(SIZE{
		ToMeasureLong(maximum.width),
		ToMeasureLong(maximum.height) });
	return cui::core::Size{
		static_cast<float>((std::max)(0L, legacyDesired.cx)),
		static_cast<float>((std::max)(0L, legacyDesired.cy)) };
}

SIZE Control::MeasureCore(SIZE availableSize)
{
	(void)availableSize;
	return SIZE{
		(std::max)(0L, _size.cx),
		(std::max)(0L, _size.cy) };
}

cui::core::Size Control::ResolveDesiredSize(
	cui::core::Size intrinsicSize,
	const cui::core::Constraints& available) const
{
	intrinsicSize = intrinsicSize.NonNegative();
	if (_layoutStyle.width.IsFixed())
		intrinsicSize.width = _layoutStyle.width.value;
	if (_layoutStyle.height.IsFixed())
		intrinsicSize.height = _layoutStyle.height.value;

	const auto styleConstraints = _layoutStyle.SizeConstraints();
	const auto availableConstraints = available.Normalized();
	const cui::core::Size minimum{
		(std::max)(styleConstraints.minimum.width, availableConstraints.minimum.width),
		(std::max)(styleConstraints.minimum.height, availableConstraints.minimum.height) };
	const cui::core::Size maximum{
		(std::max)(minimum.width, (std::min)(styleConstraints.maximum.width, availableConstraints.maximum.width)),
		(std::max)(minimum.height, (std::min)(styleConstraints.maximum.height, availableConstraints.maximum.height)) };
	return cui::core::Constraints{ minimum, maximum }.Constrain(intrinsicSize);
}

cui::core::Size Control::Measure(const cui::core::Constraints& available)
{
	const cui::core::Constraints constraints = available.Normalized();
	if (_layoutState.NeedsMeasure() ||
		_layoutState.lastMeasureConstraints != constraints)
	{
		const auto intrinsic = MeasureCore(constraints);
		_layoutState.CommitMeasure(ResolveDesiredSize(intrinsic, constraints), constraints);
	}
	return _layoutState.desiredSize;
}


SIZE Control::Measure(SIZE availableSize)
{
	const auto desired = Measure(ToMeasureConstraints(availableSize));
	return SIZE{
		ToMeasureLong(desired.width),
		ToMeasureLong(desired.height)
	};
}

cui::core::Point Control::GetActualLocationDip() const
{
	if (_layoutState.hasArranged)
		return _layoutState.arrangedRect.Origin();
	return cui::core::Point{
		(float)_runtimeLocation.x, (float)_runtimeLocation.y };
}

cui::core::Size Control::GetActualSizeDip()
{
	const SIZE compatibilitySize = ActualSize();
	if (_layoutState.hasArranged)
	{
		const auto arrangedSize = _layoutState.arrangedRect.Extent();
		return cui::core::Size{
			arrangedSize.width + (float)(compatibilitySize.cx - _size.cx),
			arrangedSize.height + (float)(compatibilitySize.cy - _size.cy)
		}.NonNegative();
	}
	return cui::core::Size{
		(float)compatibilitySize.cx, (float)compatibilitySize.cy
	}.NonNegative();
}

cui::core::Point Control::GetAbsoluteLocationDip() const
{
	const Control* ancestor = this;
	cui::core::Point absoluteLocation = ancestor->GetActualLocationDip();
	while (ancestor->Parent)
	{
		ancestor = ancestor->Parent;
		const auto ancestorLocation = ancestor->GetActualLocationDip();
		absoluteLocation += cui::core::Vector{
			ancestorLocation.x, ancestorLocation.y };
		const auto childOffset = ancestor->GetChildrenRenderOffset();
		absoluteLocation += cui::core::Vector{
			(float)childOffset.x, (float)childOffset.y };
	}
	return absoluteLocation;
}

cui::core::Rect Control::GetAbsoluteRectDip()
{
	return cui::core::Rect{
		GetAbsoluteLocationDip(), GetActualSizeDip() };
}

D2D1_MATRIX_3X2_F Control::GetInheritedRenderTransform() const
{
	auto result = D2D1::Matrix3x2F::Identity();
	for (auto* ancestor = this->Parent; ancestor; ancestor = ancestor->Parent)
		result = result * AsMatrix(
			ancestor->GetEffectiveDescendantRenderTransform());
	return result;
}

D2D1_MATRIX_3X2_F Control::GetEffectiveDescendantRenderTransform() const
{
	auto result = D2D1::Matrix3x2F::Identity();
	if (_renderTransform)
	{
		const auto size = const_cast<Control*>(this)->GetActualSizeDip();
		const auto local = AsMatrix(_renderTransform->ToMatrix(
			D2D1::SizeF(size.width, size.height), _renderTransformOrigin));
		const auto absolute = GetAbsoluteLocationDip();
		result = D2D1::Matrix3x2F::Translation(-absolute.x, -absolute.y)
			* local
			* D2D1::Matrix3x2F::Translation(absolute.x, absolute.y);
	}
	D2D1_MATRIX_3X2_F extra{};
	if (TryGetDescendantRenderTransform(extra))
		result = result * AsMatrix(extra);
	return result;
}

D2D1_MATRIX_3X2_F Control::GetLocalToRenderTransform() const
{
	const auto size = const_cast<Control*>(this)->GetActualSizeDip();
	const auto local = _renderTransform
		? AsMatrix(_renderTransform->ToMatrix(
			D2D1::SizeF(size.width, size.height), _renderTransformOrigin))
		: D2D1::Matrix3x2F::Identity();
	const auto absolute = GetAbsoluteLocationDip();
	return local
		* D2D1::Matrix3x2F::Translation(absolute.x, absolute.y)
		* AsMatrix(GetInheritedRenderTransform());
}

bool Control::TryTransformRenderPointToLocal(
	D2D1_POINT_2F renderPoint,
	D2D1_POINT_2F& localPoint) const
{
	auto inverse = AsMatrix(GetLocalToRenderTransform());
	if (!inverse.Invert()) return false;
	localPoint = inverse.TransformPoint(renderPoint);
	return std::isfinite(localPoint.x) && std::isfinite(localPoint.y);
}

bool Control::IsRenderPointInsideClip(D2D1_POINT_2F renderPoint) const
{
	for (auto* current = this; current; current = current->Parent)
	{
		if (!current->_clip) continue;
		D2D1_POINT_2F local{};
		if (!current->TryTransformRenderPointToLocal(renderPoint, local)
			|| !current->_clip->ContainsPoint(local)) return false;
	}
	return true;
}

D2D1_RECT_F Control::TransformAbsoluteRectToRenderSpace(
	const D2D1_RECT_F& rect) const
{
	const auto absolute = GetAbsoluteLocationDip();
	const auto transform = D2D1::Matrix3x2F::Translation(
		-absolute.x, -absolute.y)
		* AsMatrix(GetLocalToRenderTransform());
	const D2D1_POINT_2F points[] = {
		transform.TransformPoint(D2D1::Point2F(rect.left, rect.top)),
		transform.TransformPoint(D2D1::Point2F(rect.right, rect.top)),
		transform.TransformPoint(D2D1::Point2F(rect.left, rect.bottom)),
		transform.TransformPoint(D2D1::Point2F(rect.right, rect.bottom))
	};
	D2D1_RECT_F bounds{
		points[0].x, points[0].y, points[0].x, points[0].y };
	for (size_t index = 1; index < std::size(points); ++index)
	{
		bounds.left = (std::min)(bounds.left, points[index].x);
		bounds.top = (std::min)(bounds.top, points[index].y);
		bounds.right = (std::max)(bounds.right, points[index].x);
		bounds.bottom = (std::max)(bounds.bottom, points[index].y);
	}
	return bounds;
}

D2D1_RECT_F Control::GetRenderedAbsoluteRectDip()
{
	const auto rect = GetAbsoluteRectDip();
	return TransformAbsoluteRectToRenderSpace(D2D1_RECT_F{
		rect.Left(), rect.Top(), rect.Right(), rect.Bottom() });
}

// 应用浮点 DIP 布局结果；POINT/SIZE 仅作为兼容投影保留。
void Control::ApplyLayout(cui::core::Rect finalRect)
{
	finalRect = finalRect.Normalized();
	const cui::core::Rect previousRect = _layoutState.hasArranged
		? _layoutState.arrangedRect
		: cui::core::Rect{
			(float)_runtimeLocation.x, (float)_runtimeLocation.y,
			(float)_size.cx, (float)_size.cy };
	const bool geometryChanged = previousRect != finalRect;
	const bool layoutSizeChanged = previousRect.width != finalRect.width
		|| previousRect.height != finalRect.height;

	const POINT projectedLocation{
		ToCoordinateLong(finalRect.x), ToCoordinateLong(finalRect.y) };
	const SIZE projectedSize{
		ToLayoutLong(finalRect.width), ToLayoutLong(finalRect.height) };
	const bool locationChanged = _runtimeLocation.x != projectedLocation.x
		|| _runtimeLocation.y != projectedLocation.y;
	const bool sizeChanged = _size.cx != projectedSize.cx
		|| _size.cy != projectedSize.cy;

	_layoutState.CommitArrange(finalRect);
	_runtimeLocation = projectedLocation;
	_size = projectedSize;

	if (locationChanged)
		this->OnMoved(this);
	if (sizeChanged)
		this->OnSizeChanged(this);
	if (layoutSizeChanged)
		this->OnComputedLayoutSizeChanged();

	if (geometryChanged)
	{
		this->InvalidateVisual();
	}
}

void Control::ApplyLayout(POINT location, SIZE size)
{
	ApplyLayout(cui::core::Rect{
		(float)location.x, (float)location.y,
		(float)size.cx, (float)size.cy });
}

void Control::SetRuntimeLocation(cui::core::Point value)
{
	const auto currentSize = _layoutState.hasArranged
		? _layoutState.arrangedRect.Extent()
		: cui::core::Size{ (float)_size.cx, (float)_size.cy };
	ApplyLayout(cui::core::Rect{ value, currentSize });
}

void Control::SetRuntimeLocation(POINT value)
{
	SetRuntimeLocation(cui::core::Point{
		(float)value.x, (float)value.y });
}
