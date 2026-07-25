#include "HeaderedContentControl.h"
#include "Layout/OverlayLayout.h"

#include <algorithm>
#include <array>
#include <cmath>
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
	DependencyPropertyOptions<HeaderedContentControl, TValue> HeaderOptions(
		TValue defaultValue,
		int order,
		DependencyPropertyPersistence persistence =
			DependencyPropertyPersistence::Metadata)
	{
		DependencyPropertyOptions<HeaderedContentControl, TValue> options;
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

HeaderedContentControl::HeaderedContentControl()
	: ContentControl()
{
}

void HeaderedContentControl::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto headerOptions = HeaderOptions(
			BindingValue{}, 40, DependencyPropertyPersistence::Native);
		headerOptions.Design.Browsable = false;
		headerOptions.Coerce = [](
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
		headerOptions.Changed = [](
			HeaderedContentControl& target,
			const BindingValue&, const BindingValue&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		DependencyPropertyRegistry::Register<HeaderedContentControl, BindingValue>(
			L"Header",
			[](HeaderedContentControl& target) { return target.GetHeader(); },
			[](HeaderedContentControl& target, const BindingValue& value)
			{ target.SetHeader(value); }, {}, std::move(headerOptions));

		auto templateOptions = HeaderOptions(
			ItemTemplateReference{}, 50,
			DependencyPropertyPersistence::Native);
		templateOptions.Design.Browsable = false;
		templateOptions.Coerce = [](
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
		templateOptions.Changed = [](
			HeaderedContentControl& target,
			const ItemTemplateReference&, const ItemTemplateReference&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		DependencyPropertyRegistry::Register<HeaderedContentControl,
			ItemTemplateReference>(L"HeaderTemplate",
			[](HeaderedContentControl& target)
			{ return target.GetHeaderTemplate(); },
			[](HeaderedContentControl& target,
				const ItemTemplateReference& value)
			{ target.SetHeaderTemplate(value); }, {},
			std::move(templateOptions));

		auto pathOptions = HeaderOptions(std::wstring{}, 60);
		pathOptions.Design.Editor = DependencyPropertyEditorKind::Text;
		pathOptions.Changed = [](
			HeaderedContentControl& target,
			const std::wstring&, const std::wstring&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		DependencyPropertyRegistry::Register<HeaderedContentControl, std::wstring>(
			L"HeaderDisplayMemberPath",
			[](HeaderedContentControl& target)
			{ return target.GetHeaderDisplayMemberPath(); },
			[](HeaderedContentControl& target, const std::wstring& value)
			{ target.SetHeaderDisplayMemberPath(value); }, {},
			std::move(pathOptions));
		return true;
	}();
	(void)registered;
}

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
	(void)SetPropertyField(L"Header", _header, std::move(value));
}

void HeaderedContentControl::SetHeaderTemplate(ItemTemplateReference value)
{
	_lastHeaderError.clear();
	(void)SetPropertyField(
		L"HeaderTemplate", _headerTemplate, std::move(value));
}

void HeaderedContentControl::SetHeaderDisplayMemberPath(std::wstring value)
{
	(void)SetPropertyField(
		L"HeaderDisplayMemberPath", _headerDisplayMemberPath,
		std::move(value));
}

void HeaderedContentControl::SetHeaderTypeName(std::wstring value)
{
	if (_headerTypeName == value) return;
	const auto previous = _headerTypeName;
	_headerTypeName = value;
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

Control* HeaderedContentControl::SetVisualHeader(
	std::unique_ptr<Control> value)
{
	if (value.get() == _visualHeader) return _visualHeader;
	if (value && (!_header.Empty() || _headerTemplate))
		throw std::logic_error(
			"HeaderedContentControl visual Header cannot be combined with data Header");

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
		AddInfrastructureChild(
			std::move(value), InfrastructureChildRole::LogicalSlot);
	}
	catch (...)
	{
		_visualHeader = nullptr;
		if (previous)
		{
			ConfigureHeaderVisual(*previous);
			_visualHeader = previous.get();
			AddInfrastructureChild(
				std::move(previous), InfrastructureChildRole::LogicalSlot);
		}
		throw;
	}
	RequestLayout();
	InvalidateVisual();
	return _visualHeader;
}

std::unique_ptr<Control> HeaderedContentControl::DetachVisualHeader()
{
	if (!_visualHeader) return {};
	auto* previous = _visualHeader;
	_visualHeader = nullptr;
	auto result = DetachInfrastructureChild(previous);
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
		? DetachInfrastructureChild(_headerPresenter)
		: std::unique_ptr<Control>{};
	_headerPresenter = replacement.get();
	if (replacement) AddInfrastructureChild(std::move(replacement));
	RequestLayout();
	InvalidateVisual();
	return true;
}
