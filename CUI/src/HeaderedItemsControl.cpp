#include "HeaderedItemsControl.h"
#include "Layout/OverlayLayout.h"
#include "TreeInfrastructure.h"
#include "XamlInfrastructure.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <stdexcept>
#include <utility>

namespace
{
	bool EqualsTypeName(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	template<typename TValue>
	DependencyPropertyOptions<HeaderedItemsControl, TValue> HeaderOptions(
		TValue defaultValue,
		int order,
		DependencyPropertyPersistence persistence =
			DependencyPropertyPersistence::Metadata)
	{
		DependencyPropertyOptions<HeaderedItemsControl, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Auto;
		options.Design.Persistence = persistence;
		return options;
	}
}

HeaderedItemsControl::HeaderedItemsControl()
	: ItemsControl()
{
}

void HeaderedItemsControl::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto headerOptions = HeaderOptions(
			BindingValue{}, 40, DependencyPropertyPersistence::Native);
		headerOptions.Design.Browsable = false;
		headerOptions.Coerce = [](
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
		headerOptions.Changed = [](
			HeaderedItemsControl& target,
			const BindingValue&, const BindingValue&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		DependencyPropertyRegistry::Register<HeaderedItemsControl, BindingValue>(
			L"Header",
			[](HeaderedItemsControl& target) { return target.GetHeader(); },
			[](HeaderedItemsControl& target, const BindingValue& value)
			{ target.SetHeader(value); }, {}, std::move(headerOptions));

		auto templateOptions = HeaderOptions(
			ItemTemplateReference{}, 50,
			DependencyPropertyPersistence::Native);
		templateOptions.Design.Browsable = false;
		templateOptions.Coerce = [](
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
		templateOptions.Changed = [](
			HeaderedItemsControl& target,
			const ItemTemplateReference&, const ItemTemplateReference&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		DependencyPropertyRegistry::Register<HeaderedItemsControl,
			ItemTemplateReference>(L"HeaderTemplate",
			[](HeaderedItemsControl& target)
			{ return target.GetHeaderTemplate(); },
			[](HeaderedItemsControl& target,
				const ItemTemplateReference& value)
			{ target.SetHeaderTemplate(value); }, {},
			std::move(templateOptions));

		auto pathOptions = HeaderOptions(std::wstring{}, 60);
		pathOptions.Design.Editor = DependencyPropertyEditorKind::Text;
		pathOptions.Changed = [](
			HeaderedItemsControl& target,
			const std::wstring&, const std::wstring&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		DependencyPropertyRegistry::Register<HeaderedItemsControl, std::wstring>(
			L"HeaderDisplayMemberPath",
			[](HeaderedItemsControl& target)
			{ return target.GetHeaderDisplayMemberPath(); },
			[](HeaderedItemsControl& target, const std::wstring& value)
			{ target.SetHeaderDisplayMemberPath(value); }, {},
			std::move(pathOptions));
		return true;
	}();
	(void)registered;
}

Control* HeaderedItemsControl::AddHeaderInfrastructure(
	std::unique_ptr<Control> child,
	HeaderInfrastructureRole role)
{
	if (!child) return nullptr;
	auto* raw = child.get();
	_headerInfrastructure.push_back(raw);
	_changingHeaderInfrastructure = true;
	try
	{
		if (role == HeaderInfrastructureRole::TemplateImplementation)
		{
			if (!raw->GetTemplatedParent())
				cui::framework::XamlAccess::SetTemplatedParent(*raw, this);
			cui::framework::TreeAccess::AddOwnedVisualChild(
				*this, std::move(child), nullptr);
		}
		else
			cui::framework::TreeAccess::AddOwnedVisualChild(
				*this, std::move(child), this);
		_changingHeaderInfrastructure = false;
	}
	catch (...)
	{
		_changingHeaderInfrastructure = false;
		_headerInfrastructure.erase(std::remove(
			_headerInfrastructure.begin(), _headerInfrastructure.end(), raw),
			_headerInfrastructure.end());
		throw;
	}
	return raw;
}

std::unique_ptr<Control> HeaderedItemsControl::DetachHeaderInfrastructure(
	Control* child)
{
	if (!IsHeaderInfrastructure(child)) return {};
	_changingHeaderInfrastructure = true;
	std::unique_ptr<Control> result;
	try
	{
		result = DetachVisualChild(child);
		_changingHeaderInfrastructure = false;
	}
	catch (...)
	{
		_changingHeaderInfrastructure = false;
		throw;
	}
	_headerInfrastructure.erase(std::remove(
		_headerInfrastructure.begin(), _headerInfrastructure.end(), child),
		_headerInfrastructure.end());
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
	_lastHeaderError.clear();
	(void)SetPropertyField(L"Header", _header, std::move(value));
}

void HeaderedItemsControl::SetHeaderTemplate(ItemTemplateReference value)
{
	_lastHeaderError.clear();
	(void)SetPropertyField(
		L"HeaderTemplate", _headerTemplate, std::move(value));
}

void HeaderedItemsControl::SetHeaderDisplayMemberPath(std::wstring value)
{
	(void)SetPropertyField(
		L"HeaderDisplayMemberPath", _headerDisplayMemberPath,
		std::move(value));
}

void HeaderedItemsControl::SetHeaderTypeName(std::wstring value)
{
	if (_headerTypeName == value) return;
	const auto previous = _headerTypeName;
	_headerTypeName = std::move(value);
	std::wstring validationError;
	if (!ValidateHeaderCandidate(
		_header, _headerTemplate, validationError))
	{
		_headerTypeName = previous;
		_lastHeaderError = std::move(validationError);
		return;
	}
	if (RebuildHeaderPresenter()) return;
	const auto error = _lastHeaderError;
	_headerTypeName = previous;
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
	ContentPresenter probe;
	probe.SetContentTypeName(_headerTypeName);
	probe.SetDisplayMemberPath(_headerDisplayMemberPath);
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

Control* HeaderedItemsControl::SetVisualHeader(
	std::unique_ptr<Control> value)
{
	if (value.get() == _visualHeader) return _visualHeader;
	if (value && (!_header.Empty() || _headerTemplate))
		throw std::logic_error(
			"HeaderedItemsControl visual Header cannot be combined with data Header");
	auto previous = DetachVisualHeader();
	if (!value)
	{
		RequestLayout();
		InvalidateVisual();
		return nullptr;
	}
	ConfigureHeaderVisual(*value);
	_visualHeader = value.get();
	try
	{
		AddHeaderInfrastructure(
			std::move(value), HeaderInfrastructureRole::LogicalSlot);
	}
	catch (...)
	{
		_visualHeader = nullptr;
		if (previous)
		{
			ConfigureHeaderVisual(*previous);
			_visualHeader = previous.get();
			AddHeaderInfrastructure(
				std::move(previous), HeaderInfrastructureRole::LogicalSlot);
		}
		throw;
	}
	RequestLayout();
	InvalidateVisual();
	return _visualHeader;
}

std::unique_ptr<Control> HeaderedItemsControl::DetachVisualHeader()
{
	if (!_visualHeader) return {};
	auto* previous = _visualHeader;
	_visualHeader = nullptr;
	auto result = DetachHeaderInfrastructure(previous);
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
	if (_headerTemplate && !_headerTypeName.empty()
		&& !_headerTemplate.Get()->DataTypeName().empty()
		&& !EqualsTypeName(
			_headerTypeName, _headerTemplate.Get()->DataTypeName()))
	{
		_lastHeaderError =
			L"HeaderTemplate DataType 与 Header DataType 不一致。";
		return false;
	}
	if (_templateHeaderPresenter || GetControlTemplateRoot())
	{
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
	if (!_header.Empty())
	{
		replacement = std::make_unique<ContentPresenter>();
		replacement->SetContentTypeName(_headerTypeName);
		replacement->SetDisplayMemberPath(_headerDisplayMemberPath);
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
		? DetachHeaderInfrastructure(_headerPresenter)
		: std::unique_ptr<Control>{};
	_headerPresenter = replacement.get();
	if (replacement) AddHeaderInfrastructure(std::move(replacement));
	RequestLayout();
	InvalidateVisual();
	return true;
}
