#include "HeaderedItemsControl.h"
#include "HeaderedContentControl.h"
#include "Layout/OverlayLayout.h"
#include "TreeInfrastructure.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <exception>
#include <stdexcept>
#include <utility>

namespace
{
	template<typename TValue>
	DependencyPropertyOptions<HeaderedItemsControl, TValue> HeaderOptions(
		TValue defaultValue
		CUI_DESIGN_METADATA_ARGUMENTS(
			int order,
			DependencyPropertyPersistence persistence =
				DependencyPropertyPersistence::Metadata))
	{
		DependencyPropertyOptions<HeaderedItemsControl, TValue> options;
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

const DependencyPropertyMetadataRegistration&
HeaderedItemsControl::HeaderPropertyMetadataRelation()
{
	static const DependencyPropertyMetadataRegistration relation = []
	{
		auto options = HeaderOptions(
			BindingValue{}
			CUI_DESIGN_METADATA_ARGUMENTS(
				40, DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		options.Coerce = [](
			HeaderedItemsControl& target,
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
			HeaderedItemsControl& target,
			const BindingValue&, const BindingValue&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		return DependencyPropertyRegistry::AddOwnerStatic<
			HeaderedItemsControl, BindingValue>(
				HeaderedContentControl::HeaderProperty(),
				[](HeaderedItemsControl& target) { return target.GetHeader(); },
				[](HeaderedItemsControl& target, const BindingValue& value)
				{ target.SetHeader(value); }, {}, std::move(options));
	}();
	return relation;
}

const DependencyPropertyMetadataRegistration&
HeaderedItemsControl::HeaderTemplatePropertyMetadataRelation()
{
	static const DependencyPropertyMetadataRegistration relation = []
	{
		auto options = HeaderOptions(
			ItemTemplateReference{}
			CUI_DESIGN_METADATA_ARGUMENTS(
				50, DependencyPropertyPersistence::Native));
		CUI_DESIGN_METADATA_ONLY(
		options.Design.Browsable = false;
		)
		options.Coerce = [](
			HeaderedItemsControl& target,
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
			HeaderedItemsControl& target,
			const ItemTemplateReference&, const ItemTemplateReference&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		return DependencyPropertyRegistry::AddOwnerStatic<
			HeaderedItemsControl, ItemTemplateReference>(
				HeaderedContentControl::HeaderTemplateProperty(),
				[](HeaderedItemsControl& target)
				{ return target.GetHeaderTemplate(); },
				[](HeaderedItemsControl& target,
					const ItemTemplateReference& value)
				{ target.SetHeaderTemplate(value); }, {}, std::move(options));
	}();
	return relation;
}

const DependencyProperty& HeaderedItemsControl::HeaderProperty()
{
	return HeaderPropertyMetadataRelation().Property();
}

const DependencyProperty& HeaderedItemsControl::HeaderTemplateProperty()
{
	return HeaderTemplatePropertyMetadataRelation().Property();
}

HeaderedItemsControl::HeaderedItemsControl()
	: ItemsControl()
{
}

#if !CUI_ENABLE_DYNAMIC_XAML
void HeaderedItemsControl::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
}
#endif

const DependencyPropertyMetadata*
HeaderedItemsControl::ResolveExactDependencyPropertyMetadata(
	const DependencyProperty& property) const
{
	if (&property == &HeaderedContentControl::HeaderProperty())
		return &HeaderPropertyMetadataRelation().Metadata();
	if (&property == &HeaderedContentControl::HeaderTemplateProperty())
		return &HeaderTemplatePropertyMetadataRelation().Metadata();
	return ItemsControl::ResolveExactDependencyPropertyMetadata(property);
}

Control* HeaderedItemsControl::AddHeaderInfrastructure(
	std::unique_ptr<Control> child,
	HeaderInfrastructureRole role)
{
	if (!child) return nullptr;
	auto* raw = child.get();
	const ControlWeakReference lifetime(raw);
	Control* expectedLogicalParent =
		role == HeaderInfrastructureRole::LogicalSlot
		? this
		: nullptr;
	_headerInfrastructure.push_back(raw);
	const bool wasChangingHeaderInfrastructure =
		_changingHeaderInfrastructure;
	_changingHeaderInfrastructure = true;
	try
	{
		if (role == HeaderInfrastructureRole::TemplateImplementation)
		{
			if (raw->GetTemplatedParent()
				&& raw->GetTemplatedParent() != this)
				throw std::logic_error(
					"header infrastructure already has another template parent");
			if (!raw->GetTemplatedParent()
				&& !cui::framework::TreeAccess::
					SetTemplatedParentPreservingOwnership(
						child, this))
				throw std::logic_error(
					"header infrastructure ownership changed during template-parent publication");
		}
		if (!child)
			throw std::logic_error(
				"header infrastructure ownership changed before attachment");
		(void)cui::framework::TreeAccess::
			InsertOwnedVisualChildPreserving(
				*this, VisualChildCount(), child,
				expectedLogicalParent);

		auto* live = lifetime.Get();
		if (!live || live->GetVisualParent() != this
			|| IndexOfVisualChild(live) < 0
			|| !IsHeaderInfrastructure(live)
			|| live->GetLogicalParent() != expectedLogicalParent
			|| (role == HeaderInfrastructureRole::TemplateImplementation
				&& live->GetTemplatedParent() != this))
			throw std::logic_error(
				"header infrastructure attachment did not commit");
		_changingHeaderInfrastructure =
			wasChangingHeaderInfrastructure;
		return live;
	}
	catch (...)
	{
		_changingHeaderInfrastructure =
			wasChangingHeaderInfrastructure;
		auto* live = lifetime.Get();
		const bool stillOwnedHere = live
			&& live->GetVisualParent() == this
			&& IndexOfVisualChild(live) >= 0;
		if (!stillOwnedHere)
			_headerInfrastructure.erase(std::remove(
				_headerInfrastructure.begin(),
				_headerInfrastructure.end(), raw),
				_headerInfrastructure.end());
		if (child && (!live || live->GetVisualParent()
			|| live->GetPresentationWindow()))
			(void)child.release();
		throw;
	}
}

std::unique_ptr<Control> HeaderedItemsControl::DetachHeaderInfrastructure(
	Control* child)
{
	if (!IsHeaderInfrastructure(child)) return {};
	const ControlWeakReference lifetime(child);
	const bool wasChangingHeaderInfrastructure =
		_changingHeaderInfrastructure;
	_changingHeaderInfrastructure = true;
	std::unique_ptr<Control> result;
	try
	{
		result = DetachVisualChild(child);
		_changingHeaderInfrastructure =
			wasChangingHeaderInfrastructure;
	}
	catch (...)
	{
		_changingHeaderInfrastructure =
			wasChangingHeaderInfrastructure;
		auto* live = lifetime.Get();
		if (!live || live->GetVisualParent() != this
			|| IndexOfVisualChild(live) < 0)
			_headerInfrastructure.erase(std::remove(
				_headerInfrastructure.begin(),
				_headerInfrastructure.end(), child),
				_headerInfrastructure.end());
		throw;
	}
	auto* live = lifetime.Get();
	const bool stillOwnedHere = live
		&& live->GetVisualParent() == this
		&& IndexOfVisualChild(live) >= 0;
	if (!stillOwnedHere)
		_headerInfrastructure.erase(std::remove(
			_headerInfrastructure.begin(),
			_headerInfrastructure.end(), child),
			_headerInfrastructure.end());
	else if (result && result.get() == live)
		(void)result.release();
	return result;
}

bool HeaderedItemsControl::IsHeaderInfrastructure(
	const Control* child) const noexcept
{
	return child && std::find(
		_headerInfrastructure.begin(), _headerInfrastructure.end(), child)
		!= _headerInfrastructure.end();
}

bool HeaderedItemsControl::ValidateVisualChildCollection(
	std::span<Control* const> children, std::string& error) const
{
	if (!_changingHeaderInfrastructure && !IsChangingItemsInfrastructure())
	{
		for (auto* infrastructure : _headerInfrastructure)
		{
			if (std::find(children.begin(), children.end(), infrastructure)
				!= children.end()) continue;
			error = "HeaderedItemsControl header infrastructure cannot be mutated directly";
			return false;
		}
		auto requireDirectChild = [&](Control* child)
		{
			return !child || child->GetVisualParent() != this
				|| std::find(children.begin(), children.end(), child)
					!= children.end();
		};
		if (!requireDirectChild(GetItemsHost())
			|| !requireDirectChild(GetControlTemplateRoot()))
		{
			error = "HeaderedItemsControl template infrastructure cannot be mutated directly";
			return false;
		}
	}

	for (auto* child : children)
	{
		if (child == GetItemsHost() || child == GetControlTemplateRoot()
			|| IsHeaderInfrastructure(child)) continue;
		error = "HeaderedItemsControl direct children belong to Header, ItemsHost, or ControlTemplate";
		return false;
	}
	return true;
}

void HeaderedItemsControl::ConfigureHeaderVisual(Control& child)
{
	(void)child;
}

void HeaderedItemsControl::ReleaseHeaderVisual(Control& child)
{
	(void)child;
}

cui::core::Insets
HeaderedItemsControl::GetHeaderPresentationInsets() const noexcept
{
	return {};
}

cui::core::Insets
HeaderedItemsControl::GetItemsPresentationInsets() const noexcept
{
	return {};
}

std::wstring HeaderedItemsControl::GetSemanticText() const
{
	std::wstring value;
	return _header.TryGetString(value) ? value : std::wstring{};
}

float HeaderedItemsControl::GetHeaderSlotHeightDip(float availableWidth)
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

cui::core::Size HeaderedItemsControl::MeasureCore(
	const cui::core::Constraints& available)
{
	if (GetControlTemplateRoot()) return ItemsControl::MeasureCore(available);
	const auto padding = GetSpecifiedLayout().padding;
	const auto inner = available.Deflate(padding).Normalized();
	const float headerHeight = GetHeaderSlotHeightDip(inner.maximum.width);
	float desiredWidth = 0.0f;
	if (auto* header = GetHeaderVisual(); header && !header->IsCollapsed())
	{
		const auto insets = GetHeaderPresentationInsets();
		const auto margin = header->GetSpecifiedLayout().margin;
		const auto desired = header->Measure(cui::core::Constraints{
			cui::core::Size{ 0.0f, 0.0f },
			cui::core::Size{
				(std::max)(0.0f, inner.maximum.width
					- insets.Horizontal() - margin.Horizontal()),
				cui::core::Infinity } });
		desiredWidth = desired.width + margin.Horizontal()
			+ insets.Horizontal();
	}
	float itemsHeight = 0.0f;
	if (auto* host = GetItemsHost(); host && !host->IsCollapsed())
	{
		const auto insets = GetItemsPresentationInsets();
		const auto margin = host->GetSpecifiedLayout().margin;
		const float maximumHeight = inner.IsHeightBounded()
			? (std::max)(0.0f,
				inner.maximum.height - headerHeight
					- insets.Vertical() - margin.Vertical())
			: cui::core::Infinity;
		const auto desired = host->Measure(cui::core::Constraints{
			cui::core::Size{ 0.0f, 0.0f },
			cui::core::Size{
				(std::max)(0.0f, inner.maximum.width
					- insets.Horizontal() - margin.Horizontal()),
				maximumHeight } });
		desiredWidth = (std::max)(
			desiredWidth, desired.width + margin.Horizontal()
				+ insets.Horizontal());
		itemsHeight = desired.height + margin.Vertical()
			+ insets.Vertical();
	}
	return {
		desiredWidth + padding.Horizontal(),
		headerHeight + itemsHeight + padding.Vertical()
	};
}

void HeaderedItemsControl::RequestLayout()
{
	_headeredLayoutPending = true;
	ItemsControl::RequestLayout();
}

void HeaderedItemsControl::PerformPendingLayout()
{
	if (IsLayoutSuspended() || !_headeredLayoutPending) return;
	if (GetControlTemplateRoot())
	{
		// The base Control has already arranged the template root to the full
		// control slot. Template-authored padding must not be applied twice, but
		// hierarchy changes inside that root still need a synchronous commit.
		GetControlTemplateRoot()->UpdateLayout();
		_headeredLayoutPending = false;
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
	if (auto* host = GetItemsHost())
	{
		const std::array<Control*, 1> children{ host };
		cui::layout::ArrangeOverlayChildren(children, cui::core::Rect{
			inner.x,
			inner.y + headerHeight,
			inner.width,
			(std::max)(0.0f, inner.height - headerHeight) }
			.Inset(GetItemsPresentationInsets()));
	}
	_headeredLayoutPending = false;
}

void HeaderedItemsControl::SetHeader(BindingValue value)
{
	if (BindingValuesEqual(_header, value)) return;
	_lastHeaderError.clear();
	(void)SetPropertyField(
		HeaderProperty(), _header, std::move(value));
}

void HeaderedItemsControl::SetHeaderTemplate(ItemTemplateReference value)
{
	if (_headerTemplate == value) return;
	_lastHeaderError.clear();
	(void)SetPropertyField(
		HeaderTemplateProperty(), _headerTemplate, std::move(value));
}

void HeaderedItemsControl::SetCompiledHeaderDisplayMemberPath(
	CompiledBindingPathView value)
{
	if (value.Version != CompiledBindingPathVersion)
		throw std::invalid_argument(
			"HeaderedItemsControl compiled header display path version is unsupported");
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

void HeaderedItemsControl::SetHeaderTypeToken(DataTypeToken value)
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

bool HeaderedItemsControl::ValidateHeaderCandidate(
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
	// Template construction is intentionally deferred to the committed header
	// presenter so a coercion reevaluation cannot invoke a factory repeatedly.
	return true;
}

void HeaderedItemsControl::ApplyHeaderProjection(
	ContentPresenter& presenter) const
{
	presenter.SetContentTypeToken(_headerTypeToken);
	presenter.SetCompiledDisplayMemberPath(_compiledHeaderDisplayMemberPath);
#if CUI_ENABLE_DYNAMIC_XAML
	ApplyAuthoredHeaderProjection(presenter);
#endif
}

Control* HeaderedItemsControl::SetVisualHeader(
	std::unique_ptr<Control> value)
{
	if (value && value.get() == _visualHeader)
		return value.release();
	if (value && (!_header.Empty() || _headerTemplate))
		throw std::logic_error(
			"HeaderedItemsControl visual Header cannot be combined with data Header");

	auto isDirectlyOwnedHere = [this](const Control* child)
	{
		return child && child->GetVisualParent() == this
			&& IndexOfVisualChild(child) >= 0;
	};
	auto isPublishedHeader = [&](const Control* child)
	{
		return isDirectlyOwnedHere(child)
			&& IsHeaderInfrastructure(child);
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
		// Treat customization as a preflight while the established Header is
		// still intact. Reentrant ownership changes cannot consume both slots.
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
		// Parent callbacks may reattach the old Header or commit a newer nested
		// slot. Either state makes this replacement transaction obsolete.
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
		AddHeaderInfrastructure(
			std::move(value), HeaderInfrastructureRole::LogicalSlot);
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
			// Post-commit observer failures do not roll back ownership. Keep
			// the candidate reachable through the slot; a valid nested slot,
			// if present, represents a newer committed operation.
			if (!_visualHeader || _visualHeader == candidateRaw
				|| !isPublishedHeader(_visualHeader))
				_visualHeader = liveCandidate;
			std::rethrow_exception(originalError);
		}

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
					AddHeaderInfrastructure(
						std::move(previous),
						HeaderInfrastructureRole::LogicalSlot);
				}
			}
			catch (...)
			{
				// Preserve the candidate failure even if best-effort
				// restoration reports its own observer/configuration error.
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

std::unique_ptr<Control> HeaderedItemsControl::DetachVisualHeader()
{
	if (!_visualHeader) return {};
	auto* previous = _visualHeader;
	const ControlWeakReference lifetime(previous);
	_visualHeader = nullptr;
	std::unique_ptr<Control> result;
	try
	{
		result = DetachHeaderInfrastructure(previous);
	}
	catch (...)
	{
		auto* live = lifetime.Get();
		if (live && live->GetVisualParent() == this
			&& IndexOfVisualChild(live) >= 0)
		{
			if (!IsHeaderInfrastructure(live))
				_headerInfrastructure.push_back(live);
			_visualHeader = live;
		}
		throw;
	}
	auto* live = lifetime.Get();
	if (live && live->GetVisualParent() == this
		&& IndexOfVisualChild(live) >= 0)
	{
		if (result && result.get() == live)
			(void)result.release();
		if (!IsHeaderInfrastructure(live))
			_headerInfrastructure.push_back(live);
		_visualHeader = live;
		return {};
	}
	if (_visualHeader == previous)
		_visualHeader = nullptr;
	if (result) ReleaseHeaderVisual(*result);
	return result;
}

bool HeaderedItemsControl::RegisterTemplateHeaderPresenter(
	ContentPresenter* presenter)
{
	if (!presenter || presenter->GetTemplatedParent() != this
		|| (_templateHeaderPresenter
			&& _templateHeaderPresenter != presenter)) return false;
	_templateHeaderPresenter = presenter;
	return RebuildHeaderPresenter();
}

void HeaderedItemsControl::OnControlTemplatePresentationChanged()
{
	if (!GetControlTemplateRoot()) _templateHeaderPresenter = nullptr;
	(void)RebuildHeaderPresenter();
}

bool HeaderedItemsControl::RebuildHeaderPresenter()
{
	_lastHeaderError.clear();
	if (_templateHeaderPresenter || GetControlTemplateRoot())
	{
		if (_templateHeaderPresenter)
			ApplyHeaderProjection(*_templateHeaderPresenter);
		if (_headerPresenter)
		{
			auto previous = DetachHeaderInfrastructure(_headerPresenter);
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
	if (!_header.Empty() || _headerTemplate)
	{
		replacement = std::make_unique<ContentPresenter>();
		ApplyHeaderProjection(*replacement);
		replacement->SetContent(_header);
		replacement->SetContentTemplate(_headerTemplate);
		ConfigureHeaderVisual(*replacement);
		if (!replacement->LastTemplateError().empty())
		{
			_lastHeaderError = replacement->LastTemplateError();
			return false;
		}
	}
	auto previous = _headerPresenter
		? DetachHeaderInfrastructure(_headerPresenter)
		: std::unique_ptr<Control>{};
	_headerPresenter = replacement.get();
	if (replacement) AddHeaderInfrastructure(std::move(replacement));
	RequestLayout();
	InvalidateVisual();
	return true;
}
