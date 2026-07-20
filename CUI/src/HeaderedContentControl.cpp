#include "HeaderedContentControl.h"

#include <cwctype>
#include <stdexcept>
#include <utility>

namespace
{
	bool EqualsIgnoreCase(const std::wstring& left, const std::wstring& right)
	{
		if (left.size() != right.size()) return false;
		for (size_t index = 0; index < left.size(); ++index)
			if (std::towlower(left[index]) != std::towlower(right[index]))
				return false;
		return true;
	}

	template<typename TValue>
	ControlPropertyOptions<HeaderedContentControl, TValue> HeaderOptions(
		TValue defaultValue,
		int order,
		ControlPropertyPersistence persistence =
			ControlPropertyPersistence::Metadata)
	{
		ControlPropertyOptions<HeaderedContentControl, TValue> options;
		options.DefaultValue = std::move(defaultValue);
		options.Flags = ControlPropertyFlags::AffectsMeasure
			| ControlPropertyFlags::AffectsArrange
			| ControlPropertyFlags::AffectsRender
			| ControlPropertyFlags::TracksLocalValue;
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = order;
		options.Design.Editor = ControlPropertyEditorKind::Auto;
		options.Design.Persistence = persistence;
		return options;
	}
}

HeaderedContentControl::HeaderedContentControl(
	int x, int y, int width, int height)
	: ContentControl(x, y, width, height)
{
	ClearRows();
	AddRow(GridLength::Auto());
	AddRow(GridLength::Star(1.0f));
}

void HeaderedContentControl::EnsureBindingPropertiesRegistered()
{
	ContentControl::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto headerOptions = HeaderOptions(
			BindingValue{}, 40, ControlPropertyPersistence::Transient);
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
		BindingPropertyRegistry::Register<HeaderedContentControl, BindingValue>(
			L"Header",
			[](HeaderedContentControl& target) { return target.GetHeader(); },
			[](HeaderedContentControl& target, const BindingValue& value)
			{ target.SetHeader(value); }, {}, std::move(headerOptions));

		auto templateOptions = HeaderOptions(
			ItemTemplateReference{}, 50,
			ControlPropertyPersistence::Transient);
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
		BindingPropertyRegistry::Register<HeaderedContentControl,
			ItemTemplateReference>(L"HeaderTemplate",
			[](HeaderedContentControl& target)
			{ return target.GetHeaderTemplate(); },
			[](HeaderedContentControl& target,
				const ItemTemplateReference& value)
			{ target.SetHeaderTemplate(value); }, {},
			std::move(templateOptions));

		auto pathOptions = HeaderOptions(std::wstring{}, 60);
		pathOptions.Design.Editor = ControlPropertyEditorKind::Text;
		pathOptions.Changed = [](
			HeaderedContentControl& target,
			const std::wstring&, const std::wstring&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		BindingPropertyRegistry::Register<HeaderedContentControl, std::wstring>(
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
	child.GridRow = 1;
}

void HeaderedContentControl::ConfigureHeaderVisual(Control& child)
{
	child.GridRow = 0;
	child.GridColumn = 0;
	child.GridRowSpan = 1;
	child.GridColumnSpan = 1;
	child.HAlign = HorizontalAlignment::Stretch;
	child.VAlign = VerticalAlignment::Stretch;
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
	ContentPresenter probe(0, 0, 0, 0);
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
		InvalidateLayout();
		InvalidateVisual();
		return nullptr;
	}

	ConfigureHeaderVisual(*value);
	_visualHeader = value.get();
	try
	{
		AddInfrastructureChild(std::move(value));
	}
	catch (...)
	{
		_visualHeader = nullptr;
		if (previous)
		{
			ConfigureHeaderVisual(*previous);
			_visualHeader = previous.get();
			AddInfrastructureChild(std::move(previous));
		}
		throw;
	}
	InvalidateLayout();
	InvalidateVisual();
	return _visualHeader;
}

std::unique_ptr<Control> HeaderedContentControl::DetachVisualHeader()
{
	if (!_visualHeader) return {};
	auto* previous = _visualHeader;
	_visualHeader = nullptr;
	return DetachInfrastructureChild(previous);
}

bool HeaderedContentControl::RegisterTemplateHeaderPresenter(
	ContentPresenter* presenter)
{
	if (!presenter || (_templateHeaderPresenter
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
		&& !EqualsIgnoreCase(
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
		InvalidateLayout();
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
		replacement = std::make_unique<ContentPresenter>(0, 0, 0, 0);
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
	InvalidateLayout();
	InvalidateVisual();
	return true;
}
