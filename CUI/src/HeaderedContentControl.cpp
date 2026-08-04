#include "HeaderedContentControl.h"
#include "Layout/OverlayLayout.h"
#include "TreeInfrastructure.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwctype>
#include <exception>
#include <stdexcept>
#include <utility>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<HeaderedContentControl, TValue> HeaderOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyPersistence persistence =
				DependencyPropertyPersistence::Metadata))
	{
		DependencyPropertyOptions<HeaderedContentControl, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = persistence;
		)
		return options;
	}

}

const DependencyProperty& HeaderedContentControl::HeaderProperty()
{
	static const auto registration = []
	{
		auto options = HeaderOptions(
			BindingValue{}
			CUI_DESIGN_METADATA_ARGUMENTS(
				40, DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		options.Coerce = [](
			HeaderedContentControl& target,
			const BindingValue& proposed) -> std::optional<BindingValue>
		{
			std::wstring error;
			if (!target.ValidateHeaderCandidate(
				proposed, target._headerTemplate, error))
			{
				target._lastHeaderError = std::move(error);
				return std::nullopt;
			}
			return proposed;
		};
		options.Changed = [](
			HeaderedContentControl& target,
			const BindingValue&, const BindingValue&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			HeaderedContentControl, BindingValue>(
				DependencyPropertyRegistrationLiteral(L"Header"),
				[](HeaderedContentControl& target)
				{ return target.GetHeader(); },
				[](HeaderedContentControl& target, const BindingValue& value)
				{ target.SetHeader(value); }, {}, std::move(options));
	}();
	return *registration;
}

const DependencyProperty& HeaderedContentControl::HeaderTemplateProperty()
{
	static const auto registration = []
	{
		auto options = HeaderOptions(
			ItemTemplateReference{}
			CUI_DESIGN_METADATA_ARGUMENTS(
				50, DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		options.Coerce = [](
			HeaderedContentControl& target,
			const ItemTemplateReference& proposed)
			-> std::optional<ItemTemplateReference>
		{
			std::wstring error;
			if (!target.ValidateHeaderCandidate(
				target._header, proposed, error))
			{
				target._lastHeaderError = std::move(error);
				return std::nullopt;
			}
			return proposed;
		};
		options.Changed = [](
			HeaderedContentControl& target,
			const ItemTemplateReference&, const ItemTemplateReference&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		return DependencyPropertyRegistry::RegisterStatic<
			HeaderedContentControl, ItemTemplateReference>(
				DependencyPropertyRegistrationLiteral(L"HeaderTemplate"),
				[](HeaderedContentControl& target)
				{ return target.GetHeaderTemplate(); },
				[](HeaderedContentControl& target,
					const ItemTemplateReference& value)
				{ target.SetHeaderTemplate(value); }, {}, std::move(options));
	}();
	return *registration;
}

HeaderedContentControl::HeaderedContentControl()
	: ContentControl()
{
}

#if !CUI_ENABLE_DYNAMIC_XAML
void HeaderedContentControl::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
}
#endif

void HeaderedContentControl::ConfigureContentVisual(Control& child)
{
	ContentControl::ConfigureContentVisual(child);
}

std::wstring HeaderedContentControl::GetSemanticText() const
{
	std::wstring value;
	return _header.TryGetString(value) ? value : std::wstring{};
}

void HeaderedContentControl::ConfigureHeaderVisual(Control& child)
{
	(void)child;
}

void HeaderedContentControl::ReleaseHeaderVisual(Control& child)
{
	(void)child;
}

cui::core::Insets
HeaderedContentControl::GetHeaderPresentationInsets() const noexcept
{
	return {};
}

float HeaderedContentControl::GetHeaderSlotHeightDip(float availableWidth)
{
	auto* header = GetHeaderVisual();
	if (!header || header->IsCollapsed()) return 0.0f;
	const auto insets = GetHeaderPresentationInsets();
	const auto margin = header->GetSpecifiedLayout().margin;
	const auto desired = header->Measure(cui::core::Constraints{
		cui::core::Size{ 0.0f, 0.0f },
		cui::core::Size{
			(std::max)(0.0f, availableWidth
				- insets.Horizontal() - margin.Horizontal()),
			cui::core::Infinity } });
	return desired.height + margin.Vertical() + insets.Vertical();
}

cui::core::Size
HeaderedContentControl::MeasureHeaderPresentationContent(
	const cui::core::Constraints& available)
{
	auto* content = GetVisualHeader();
	if (!content) content = GetGeneratedHeaderContent();
	if (!content || content->IsCollapsed()) return {};

	const auto margin = content->GetSpecifiedLayout().margin;
	const auto desired = content->Measure(available.Deflate(margin));
	return {
		desired.width + margin.Horizontal(),
		desired.height + margin.Vertical()
	};
}

cui::core::Size HeaderedContentControl::MeasureCore(
	const cui::core::Constraints& available)
{
	if (GetControlTemplateRoot())
		return ContentControl::MeasureCore(available);

	const auto padding = GetSpecifiedLayout().padding;
	const auto inner = available.Deflate(padding).Normalized();
	const float headerHeight = GetHeaderSlotHeightDip(inner.maximum.width);
	float desiredWidth = 0.0f;
	if (auto* header = GetHeaderVisual(); header && !header->IsCollapsed())
	{
		const auto insets = GetHeaderPresentationInsets();
		const auto margin = header->GetSpecifiedLayout().margin;
		desiredWidth = header->Measure(cui::core::Constraints{
			cui::core::Size{ 0.0f, 0.0f },
			cui::core::Size{
				(std::max)(0.0f, inner.maximum.width
					- insets.Horizontal() - margin.Horizontal()),
				cui::core::Infinity } }).width + margin.Horizontal();
		desiredWidth += insets.Horizontal();
	}

	float contentHeight = 0.0f;
	auto* content = GetVisualContent();
	if (!content) content = GetGeneratedPresenter();
	if (content && !content->IsCollapsed())
	{
		const auto margin = content->GetSpecifiedLayout().margin;
		const float maximumHeight = inner.IsHeightBounded()
			? (std::max)(0.0f,
				inner.maximum.height - headerHeight - margin.Vertical())
			: cui::core::Infinity;
		const auto desired = content->Measure(cui::core::Constraints{
			cui::core::Size{ 0.0f, 0.0f },
			cui::core::Size{
				(std::max)(0.0f, inner.maximum.width - margin.Horizontal()),
				maximumHeight } });
		desiredWidth = (std::max)(
			desiredWidth, desired.width + margin.Horizontal());
		contentHeight = desired.height + margin.Vertical();
	}
	return {
		desiredWidth + padding.Horizontal(),
		headerHeight + contentHeight + padding.Vertical()
	};
}

void HeaderedContentControl::PerformPendingLayout()
{
	if (IsLayoutSuspended() || !_contentLayoutPending) return;
	if (GetControlTemplateRoot())
	{
		ContentControl::PerformPendingLayout();
		return;
	}

	const auto size = GetActualSizeDip();
	const auto padding = GetSpecifiedLayout().padding;
	const cui::core::Rect inner{
		padding.left,
		padding.top,
		(std::max)(0.0f, size.width - padding.Horizontal()),
		(std::max)(0.0f, size.height - padding.Vertical())
	};
	const float headerHeight = (std::clamp)(
		GetHeaderSlotHeightDip(inner.width), 0.0f, inner.height);
	if (auto* header = GetHeaderVisual())
	{
		const std::array<Control*, 1> children{ header };
		cui::layout::ArrangeOverlayChildren(children,
			cui::core::Rect{ inner.x, inner.y, inner.width, headerHeight }
				.Inset(GetHeaderPresentationInsets()));
	}
	auto* content = GetVisualContent();
	if (!content) content = GetGeneratedPresenter();
	if (content)
	{
		const std::array<Control*, 1> children{ content };
		cui::layout::ArrangeOverlayChildren(children, cui::core::Rect{
			inner.x,
			inner.y + headerHeight,
			inner.width,
			(std::max)(0.0f, inner.height - headerHeight) });
	}
	_contentLayoutPending = false;
}

void HeaderedContentControl::SetHeader(BindingValue value)
{
	_lastHeaderError.clear();
	(void)SetPropertyField(
		HeaderProperty(), _header, std::move(value));
}

void HeaderedContentControl::SetHeaderTemplate(ItemTemplateReference value)
{
	_lastHeaderError.clear();
	(void)SetPropertyField(
		HeaderTemplateProperty(), _headerTemplate, std::move(value));
}

void HeaderedContentControl::SetCompiledHeaderDisplayMemberPath(
	CompiledBindingPathView value)
{
	if (value.Version != CompiledBindingPathVersion)
		throw std::invalid_argument(
			"HeaderedContentControl compiled header display path version is unsupported");
	const bool unchanged =
		_compiledHeaderDisplayMemberPath.Version == value.Version
		&& _compiledHeaderDisplayMemberPath.Steps.data() == value.Steps.data()
		&& _compiledHeaderDisplayMemberPath.Steps.size() == value.Steps.size();
#if CUI_ENABLE_DYNAMIC_XAML
	if (unchanged && _headerDisplayMemberPath.empty()) return;
#else
	if (unchanged) return;
#endif
	_compiledHeaderDisplayMemberPath = value;
#if CUI_ENABLE_DYNAMIC_XAML
	_headerDisplayMemberPath.clear();
#endif
	(void)RebuildHeaderPresenter();
}

void HeaderedContentControl::SetHeaderTypeToken(DataTypeToken value)
{
	if (_headerTypeToken == value) return;
	const auto previous = _headerTypeToken;
	_headerTypeToken = value;
	std::wstring validationError;
	if (!ValidateHeaderCandidate(
		_header, _headerTemplate, validationError))
	{
		_headerTypeToken = previous;
		_lastHeaderError = std::move(validationError);
		return;
	}
	if (RebuildHeaderPresenter()) return;
	const auto error = _lastHeaderError;
	_headerTypeToken = previous;
	(void)RebuildHeaderPresenter();
	_lastHeaderError = error;
}

bool HeaderedContentControl::ValidateHeaderCandidate(
	const BindingValue& header,
	const ItemTemplateReference& headerTemplate,
	std::wstring& error) const
{
	error.clear();
	const auto* visual = GetVisualHeader();
	if (visual)
	{
		if (!header.Empty() || headerTemplate)
		{
			error = L"视觉 Header 不能与数据 Header 同时使用。";
			return false;
		}
		return true;
	}
	ContentPresenter probe;
	ApplyHeaderProjection(probe);
	probe.SetContentTemplate(headerTemplate);
	if (!probe.LastTemplateError().empty())
	{
		error = probe.LastTemplateError();
		return false;
	}
	probe.SetContent(header);
	if (probe.LastTemplateError().empty()) return true;
	error = probe.LastTemplateError();
	return false;
}

void HeaderedContentControl::ApplyHeaderProjection(
	ContentPresenter& presenter) const
{
	presenter.SetContentTypeToken(_headerTypeToken);
	presenter.SetCompiledDisplayMemberPath(_compiledHeaderDisplayMemberPath);
#if CUI_ENABLE_DYNAMIC_XAML
	ApplyAuthoredHeaderProjection(presenter);
#endif
}

Control* HeaderedContentControl::SetVisualHeader(
	std::unique_ptr<Control> value)
{
	if (value && value.get() == _visualHeader)
		return value.release();
	if (value && (!_header.Empty() || _headerTemplate))
		throw std::logic_error(
			"HeaderedContentControl visual Header cannot be combined with data Header");

	auto isDirectlyOwnedHere = [this](const Control* child)
	{
		return child && child->GetVisualParent() == this
			&& IndexOfVisualChild(child) >= 0;
	};
	auto isPublishedHeader = [&](const Control* child)
	{
		return isDirectlyOwnedHere(child)
			&& IsInfrastructureChild(child);
	};
	auto configureOwned = [this](
		std::unique_ptr<Control>& owner,
		const char* destroyedMessage,
		const char* transferredMessage)
	{
		if (!owner) return;
		const ControlWeakReference lifetime(owner.get());
		if (!cui::framework::TreeAccess::
			InvokePreservingVisualOwnership(
				owner,
				[this](Control& child)
				{
					ConfigureHeaderVisual(child);
				}))
		{
			if (!lifetime)
				throw std::logic_error(destroyedMessage);
			throw std::logic_error(transferredMessage);
		}
	};

	auto* establishedRaw = _visualHeader;
	const ControlWeakReference establishedLifetime(establishedRaw);
	Control* candidateRaw = value.get();
	const ControlWeakReference candidateLifetime(candidateRaw);
	if (value)
	{
		// Configure while the established slot is still intact. A derived
		// hook may synchronously attach, transfer or destroy the candidate.
		configureOwned(
			value,
			"visual Header was destroyed during configuration",
			"visual Header ownership changed during configuration");
		if (_visualHeader != establishedRaw)
			throw std::logic_error(
				"visual Header slot changed during candidate configuration");
		auto* liveEstablished = establishedLifetime.Get();
		if (establishedRaw && !isPublishedHeader(liveEstablished))
		{
			if (_visualHeader == establishedRaw)
				_visualHeader = nullptr;
			throw std::logic_error(
				"established visual Header ownership changed during candidate configuration");
		}
	}

	auto previous = DetachVisualHeader();
	auto* liveEstablished = establishedLifetime.Get();
	if (_visualHeader || isDirectlyOwnedHere(liveEstablished))
	{
		// Detach can be synchronously undone by a parent observer, or a nested
		// SetVisualHeader can publish a newer slot. Never install another child
		// on top of either committed state.
		if (previous && previous.get() == liveEstablished
			&& isDirectlyOwnedHere(liveEstablished))
			(void)previous.release();
		throw std::logic_error(
			"established visual Header did not detach");
	}
	if (!value)
	{
		RequestLayout();
		InvalidateVisual();
		return nullptr;
	}

	_visualHeader = candidateRaw;
	try
	{
		AddInfrastructureChild(
			std::move(value), InfrastructureChildRole::LogicalSlot);
		auto* liveCandidate = candidateLifetime.Get();
		if (!isPublishedHeader(liveCandidate)
			|| liveCandidate->GetLogicalParent() != this
			|| _visualHeader != candidateRaw)
			throw std::logic_error(
				"visual Header attachment did not commit");
		_visualHeader = liveCandidate;
	}
	catch (...)
	{
		const auto originalError = std::current_exception();
		auto* liveCandidate = candidateLifetime.Get();
		if (isDirectlyOwnedHere(liveCandidate))
		{
			// An observer may throw after AddInfrastructureChild commits.
			// Preserve that published child instead of hiding it behind a
			// restored predecessor. A valid nested slot has newer authority.
			if (!_visualHeader || _visualHeader == candidateRaw
				|| !isPublishedHeader(_visualHeader))
				_visualHeader = liveCandidate;
			std::rethrow_exception(originalError);
		}

		// Candidate ownership moved elsewhere (or it was destroyed). Clear only
		// our in-flight publication; a nested SetVisualHeader may already have
		// committed a different slot and must not be overwritten.
		if (_visualHeader == candidateRaw)
			_visualHeader = nullptr;
		if (!_visualHeader && previous)
		{
			auto* previousRaw = previous.get();
			try
			{
				configureOwned(
					previous,
					"previous visual Header was destroyed during restoration",
					"previous visual Header ownership changed during restoration");
				if (!_visualHeader && previous)
				{
					_visualHeader = previousRaw;
					AddInfrastructureChild(
						std::move(previous),
						InfrastructureChildRole::LogicalSlot);
				}
			}
			catch (...)
			{
				// Restoration is best effort. The candidate failure is the
				// operation's primary error and must remain observable.
			}

			auto* livePrevious = establishedLifetime.Get();
			if (isPublishedHeader(livePrevious))
			{
				if (!_visualHeader || _visualHeader == previousRaw
					|| !isPublishedHeader(_visualHeader))
					_visualHeader = livePrevious;
			}
			else if (_visualHeader == previousRaw)
				_visualHeader = nullptr;
		}
		std::rethrow_exception(originalError);
	}
	RequestLayout();
	InvalidateVisual();
	return _visualHeader;
}

std::unique_ptr<Control> HeaderedContentControl::DetachVisualHeader()
{
	if (!_visualHeader) return {};
	auto* previous = _visualHeader;
	const ControlWeakReference lifetime(previous);
	_visualHeader = nullptr;
	std::unique_ptr<Control> result;
	try
	{
		result = DetachInfrastructureChild(previous);
	}
	catch (...)
	{
		auto* live = lifetime.Get();
		if (live && live->GetVisualParent() == this
			&& IndexOfVisualChild(live) >= 0
			&& IsInfrastructureChild(live))
			_visualHeader = live;
		throw;
	}
	auto* live = lifetime.Get();
	if (live && live->GetVisualParent() == this
		&& IndexOfVisualChild(live) >= 0
		&& IsInfrastructureChild(live))
	{
		if (result && result.get() == live)
			(void)result.release();
		_visualHeader = live;
		return {};
	}
	if (_visualHeader == previous)
		_visualHeader = nullptr;
	if (result) ReleaseHeaderVisual(*result);
	return result;
}

bool HeaderedContentControl::RegisterTemplateHeaderPresenter(
	ContentPresenter* presenter)
{
	if (!presenter || presenter->GetTemplatedParent() != this
		|| (_templateHeaderPresenter
		&& _templateHeaderPresenter != presenter)) return false;
	_templateHeaderPresenter = presenter;
	return RebuildHeaderPresenter();
}

void HeaderedContentControl::OnControlTemplatePresentationChanged()
{
	if (!GetControlTemplateRoot()) _templateHeaderPresenter = nullptr;
	ContentControl::OnControlTemplatePresentationChanged();
	(void)RebuildHeaderPresenter();
}

bool HeaderedContentControl::RebuildHeaderPresenter()
{
	_lastHeaderError.clear();
	if (_headerTemplate && !AreDataTypesCompatible(
		_headerTypeToken, _headerTemplate.Get()->GetDataTypeToken()))
	{
		_lastHeaderError =
			L"HeaderTemplate DataType 与 Header DataType 不一致。";
		return false;
	}
	if (_templateHeaderPresenter || GetControlTemplateRoot())
	{
		if (_templateHeaderPresenter)
			ApplyHeaderProjection(*_templateHeaderPresenter);
		if (_headerPresenter)
		{
			auto previous = DetachInfrastructureChild(_headerPresenter);
			_headerPresenter = nullptr;
		}
		RequestLayout();
		InvalidateVisual();
		return true;
	}
	if (_visualHeader)
	{
		if (!_header.Empty() || _headerTemplate)
		{
			_lastHeaderError =
				L"视觉 Header 不能与数据 Header 同时使用。";
			return false;
		}
		return true;
	}

	std::unique_ptr<ContentPresenter> replacement;
	if (!_header.Empty())
	{
		replacement = std::make_unique<ContentPresenter>();
		ApplyHeaderProjection(*replacement);
		replacement->SetContentTemplate(_headerTemplate);
		replacement->SetContent(_header);
		ConfigureHeaderVisual(*replacement);
		if (!replacement->LastTemplateError().empty())
		{
			_lastHeaderError = replacement->LastTemplateError();
			return false;
		}
	}

	auto previous = _headerPresenter
		? DetachInfrastructureChild(_headerPresenter)
		: std::unique_ptr<Control>{};
	_headerPresenter = replacement.get();
	if (replacement) AddInfrastructureChild(std::move(replacement));
	RequestLayout();
	InvalidateVisual();
	return true;
}
