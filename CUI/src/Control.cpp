#include "Control.h"
#include "Binding.h"
#include "Form.h"
#include "Panel.h"
#include "Core/Threading.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <mutex>
#include <cwctype>
#include <atomic>
#include <unordered_set>

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

	int StoredPropertySourceIndex(ControlPropertyValueSource source) noexcept
	{
		const int value = static_cast<int>(source);
		return value >= static_cast<int>(ControlPropertyValueSource::Theme)
			&& value <= static_cast<int>(ControlPropertyValueSource::Local)
			? value - static_cast<int>(ControlPropertyValueSource::Theme)
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
		if (child->Parent == this) child->Parent = nullptr;
		child->_isFormRoot = false;
		SetChildrenParentForm(child, nullptr);
	}
	for (auto* child : Children)
	{
		if (previousSet.contains(child)) continue;
		child->Parent = this;
		child->_isFormRoot = false;
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
void Control::BeginRender()
{
	auto actualSize = this->GetActualSizeDip();
	BeginRender(actualSize.width, actualSize.height);
}
void Control::BeginRender(float clipW, float clipH)
{
	if (!this->ParentForm || !this->ParentForm->Render) return;
	auto absoluteLocation = this->GetAbsoluteLocationDip();
	// HeadHeight is physical; divide by dpiScale to match the logical DIP transform.
	const float dpiScale = this->ParentForm->GetDpiScale();
	const float titleBarOffset = (this->ParentForm->VisibleHead ? this->ParentForm->HeadHeight / dpiScale : 0.0f);
	this->ParentForm->Render->PushLocalTransform(absoluteLocation.x, absoluteLocation.y + titleBarOffset, clipW, clipH);
}
void Control::SetRenderDecorator(
	std::function<void(Control&, D2DGraphics&)> decorator)
{
	_renderDecorator = std::move(decorator);
	InvalidateVisual();
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
	RenderFocusAdorner();
	RenderValidationAdorner();
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
	const RECT currentClientPixels = this->ParentForm->ContentDipRectToClientPixels(contentRect);
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
	const Binding* owner)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || !metadata->CanWrite() || index < 0) return false;

	BindingValue converted;
	BindingValue effective;
	if (!metadata->TryConvert(value, converted)
		|| !metadata->TryCoerce(*this, converted, effective))
		return false;

	auto [entryIt, inserted] = _propertyValues.try_emplace(metadata);
	auto& entry = entryIt->second;
	const size_t sourceIndex = (size_t)index;
	const Binding* previousOwner = entry.BindingOwners[sourceIndex];
	if (source == ControlPropertyValueSource::Binding
		&& previousOwner && previousOwner != owner)
	{
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
		if (inserted) _propertyValues.erase(entryIt);
		return false;
	}

	const bool effectiveUnchanged = hadOldEffective
		&& oldSource == newSource
		&& (newSource != source
			|| metadata->ValuesEqual(oldEffective, newEffective));
	if (effectiveUnchanged) return true;
	if (ApplyEffectivePropertyValue(*metadata, newEffective, newSource))
		return true;

	entry.Values[sourceIndex] = std::move(previous);
	entry.BindingOwners[sourceIndex] = previousOwner;
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
	const Binding* owner)
{
	const auto* metadata = FindPropertyMetadata(propertyName);
	const int index = StoredPropertySourceIndex(source);
	if (!metadata || index < 0) return false;
	auto entryIt = _propertyValues.find(metadata);
	if (entryIt == _propertyValues.end()) return false;
	auto& entry = entryIt->second;
	const size_t sourceIndex = (size_t)index;
	if (!entry.Values[sourceIndex].has_value()) return false;
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
		|| ApplyEffectivePropertyValue(*metadata, newEffective, newSource);
	if (!applied)
	{
		entry.Values[sourceIndex] = std::move(previous);
		entry.BindingOwners[sourceIndex] = previousOwner;
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
			index + static_cast<int>(ControlPropertyValueSource::Theme));
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
	ControlPropertyValueSource source)
{
	const auto* previousMetadata = _applyingPropertyMetadata;
	const auto previousSource = _applyingPropertySource;
	_applyingPropertyMetadata = &metadata;
	_applyingPropertySource = source;
	bool result = false;
	try
	{
		result = metadata.TrySet(*this, value);
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

	ControlPropertyChangedEventArgs args{
		metadata.Name(), oldValue, newValue };
	OnPropertyValueChanged(this, args);
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
	case UIClass::UI_NavigationView:
	case UIClass::UI_SideBar: return ::AccessibleRole::List;
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
	case UIClass::UI_ScrollView:
	case UIClass::UI_StackPanel:
	case UIClass::UI_GridPanel:
	case UIClass::UI_DockPanel:
	case UIClass::UI_WrapPanel:
	case UIClass::UI_RelativePanel:
	case UIClass::UI_SplitContainer: return ::AccessibleRole::Pane;
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
		BindingPropertyRegistry::Register<Control, SIZE>(L"MinSize",
			[](Control& target) { return target.MinSize; },
			[](Control& target, const SIZE& value) { target.MinSize = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, SIZE>{
				SIZE{ 0, 0 },
				ControlPropertyFlags::AffectsMeasure,
				{}, {},
				[](const SIZE& left, const SIZE& right)
				{
					return left.cx == right.cx && left.cy == right.cy;
				} }, PropertyDesign(L"Layout", 100, 160,
					ControlPropertyPersistence::Metadata, ControlPropertyEditorKind::Size)));
		BindingPropertyRegistry::Register<Control, SIZE>(L"MaxSize",
			[](Control& target) { return target.MaxSize; },
			[](Control& target, const SIZE& value) { target.MaxSize = value; },
			{},
			WithPropertyDesign(ControlPropertyOptions<Control, SIZE>{
				SIZE{ INT_MAX, INT_MAX },
				ControlPropertyFlags::AffectsMeasure,
				{}, {},
				[](const SIZE& left, const SIZE& right)
				{
					return left.cx == right.cx && left.cy == right.cy;
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
bool Control::ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam, int localX, int localY)
{
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
	if (_minSize.cx == value.cx && _minSize.cy == value.cy)
		return;
	_minSize = value;
	_layoutStyle.minimumSize = cui::core::Size{
		(std::max)(0.0f, (float)value.cx),
		(std::max)(0.0f, (float)value.cy) };
	this->RequestLayout();
	this->InvalidateVisual();
}

GET_CPP(Control, SIZE, MaxSize)
{
	return _maxSize;
}
SET_CPP(Control, SIZE, MaxSize)
{
	if (_maxSize.cx == value.cx && _maxSize.cy == value.cy)
		return;
	_maxSize = value;
	_layoutStyle.maximumSize = cui::core::Size{
		ToMaximumDip(value.cx), ToMaximumDip(value.cy) };
	this->RequestLayout();
	this->InvalidateVisual();
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
		absoluteLocation += ancestor->GetActualLocationDip();
		const auto childOffset = ancestor->GetChildrenRenderOffset();
		absoluteLocation += cui::core::Point{
			(float)childOffset.x, (float)childOffset.y };
	}
	return absoluteLocation;
}

cui::core::Rect Control::GetAbsoluteRectDip()
{
	return cui::core::Rect{
		GetAbsoluteLocationDip(), GetActualSizeDip() };
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
