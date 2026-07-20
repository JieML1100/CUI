#include "ContentControl.h"
#include "Form.h"

#include <cwctype>
#include <algorithm>
#include <limits>
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
	ControlPropertyOptions<ContentControl, TValue> ContentOptions(
		TValue defaultValue,
		int order,
		ControlPropertyPersistence persistence =
			ControlPropertyPersistence::Metadata)
	{
		ControlPropertyOptions<ContentControl, TValue> options;
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

ContentControl::ContentControl(int x, int y, int width, int height)
	: GridPanel(x, y, width, height)
{
	(void)TrySetPropertyValue(
		L"BorderThickness", BindingValue(0.0f),
		ControlPropertyValueSource::Theme);
}

void ContentControl::Update()
{
	if (!_controlTemplateRoot)
	{
		GridPanel::Update();
		return;
	}
	if (!IsVisual || !ParentForm || !ParentForm->Render) return;
	PerformPendingLayout();
	BeginRender();
	if (!ParentForm->IsDCompSceneRenderActive())
	{
		for (auto* child : GetChildrenInZOrder())
			if (child && child->Visible) child->Update();
	}
	EndRender();
}

void ContentControl::EnsureBindingPropertiesRegistered()
{
	GridPanel::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto contentOptions = ContentOptions(
			BindingValue{}, 10, ControlPropertyPersistence::Transient);
		contentOptions.Design.Browsable = false;
		contentOptions.Coerce = [](
			ContentControl& target,
			const BindingValue& proposed) -> std::optional<BindingValue>
		{
			std::wstring error;
			if (!target.ValidateContentCandidate(
				proposed, target._contentTemplate, error))
			{
				target._lastContentError = std::move(error);
				return std::nullopt;
			}
			return proposed;
		};
		contentOptions.Changed = [](
			ContentControl& target,
			const BindingValue&, const BindingValue&)
		{
			(void)target.RebuildPresenter();
		};
		BindingPropertyRegistry::Register<ContentControl, BindingValue>(
			L"Content",
			[](ContentControl& target) { return target.GetContent(); },
			[](ContentControl& target, const BindingValue& value)
			{ target.SetContent(value); }, {}, std::move(contentOptions));

		auto templateOptions = ContentOptions(
			ItemTemplateReference{}, 20,
			ControlPropertyPersistence::Transient);
		templateOptions.Design.Browsable = false;
		templateOptions.Coerce = [](
			ContentControl& target,
			const ItemTemplateReference& proposed)
			-> std::optional<ItemTemplateReference>
		{
			std::wstring error;
			if (!target.ValidateContentCandidate(
				target._content, proposed, error))
			{
				target._lastContentError = std::move(error);
				return std::nullopt;
			}
			return proposed;
		};
		templateOptions.Changed = [](
			ContentControl& target,
			const ItemTemplateReference&, const ItemTemplateReference&)
		{
			(void)target.RebuildPresenter();
		};
		BindingPropertyRegistry::Register<ContentControl,
			ItemTemplateReference>(L"ContentTemplate",
			[](ContentControl& target) { return target.GetContentTemplate(); },
			[](ContentControl& target, const ItemTemplateReference& value)
			{ target.SetContentTemplate(value); }, {},
			std::move(templateOptions));

		auto pathOptions = ContentOptions(std::wstring{}, 30);
		pathOptions.Design.Editor = ControlPropertyEditorKind::Text;
		pathOptions.Changed = [](
			ContentControl& target,
			const std::wstring&, const std::wstring&)
		{
			(void)target.RebuildPresenter();
		};
		BindingPropertyRegistry::Register<ContentControl, std::wstring>(
			L"DisplayMemberPath",
			[](ContentControl& target)
			{ return target.GetDisplayMemberPath(); },
			[](ContentControl& target, const std::wstring& value)
			{ target.SetDisplayMemberPath(value); }, {},
			std::move(pathOptions));
		return true;
	}();
	(void)registered;
}

Control* ContentControl::GetVisualContent() const noexcept
{
	Control* result = nullptr;
	for (auto* child : Children)
	{
		if (!child || IsInfrastructureChild(child)) continue;
		if (result) return nullptr;
		result = child;
	}
	if (result) return result;
	return _templateContentPresenter
		? _templateContentPresenter->GetVisualContent() : nullptr;
}

bool ContentControl::ValidateChildCollection(
	std::span<Control* const> children,
	std::string& error) const
{
	if (!_changingInfrastructure)
	{
		for (auto* infrastructure : _infrastructureChildren)
		{
			if (std::find(children.begin(), children.end(), infrastructure)
				== children.end())
			{
				error = "ContentControl infrastructure children cannot be mutated directly";
				return false;
			}
		}
	}

	size_t authoredCount = 0;
	for (auto* child : children)
		if (child && !IsInfrastructureChild(child)) ++authoredCount;
	if (authoredCount > 1)
	{
		error = "ContentControl accepts at most one visual child";
		return false;
	}
	if (authoredCount != 0
		&& (!_content.Empty() || _contentTemplate))
	{
		error = "ContentControl visual content cannot be combined with data content";
		return false;
	}
	return true;
}

void ContentControl::OnChildCollectionChanged(
	const CollectionChangedEventArgs& change,
	std::span<Control* const> previousChildren)
{
	(void)change;
	(void)previousChildren;
	if (auto* content = GetVisualContent()) ConfigureContentVisual(*content);
}

Control* ContentControl::AddInfrastructureChild(std::unique_ptr<Control> child)
{
	if (!child) return nullptr;
	auto* raw = child.get();
	_infrastructureChildren.push_back(raw);
	_changingInfrastructure = true;
	try
	{
		AddOwned(std::move(child));
		_changingInfrastructure = false;
	}
	catch (...)
	{
		_changingInfrastructure = false;
		_infrastructureChildren.erase(std::remove(
			_infrastructureChildren.begin(),
			_infrastructureChildren.end(), raw),
			_infrastructureChildren.end());
		throw;
	}
	return raw;
}

std::unique_ptr<Control> ContentControl::DetachInfrastructureChild(Control* child)
{
	if (!child || !IsInfrastructureChild(child)) return {};
	_changingInfrastructure = true;
	std::unique_ptr<Control> result;
	try
	{
		result = DetachControl(child);
		_changingInfrastructure = false;
	}
	catch (...)
	{
		_changingInfrastructure = false;
		throw;
	}
	_infrastructureChildren.erase(std::remove(
		_infrastructureChildren.begin(),
		_infrastructureChildren.end(), child),
		_infrastructureChildren.end());
	return result;
}

bool ContentControl::IsInfrastructureChild(const Control* child) const noexcept
{
	return child && std::find(
		_infrastructureChildren.begin(),
		_infrastructureChildren.end(), child) != _infrastructureChildren.end();
}

void ContentControl::ConfigureContentVisual(Control& child)
{
	child.GridRow = 0;
	child.GridColumn = 0;
	child.GridRowSpan = 1;
	child.GridColumnSpan = 1;
	child.HAlign = HorizontalAlignment::Stretch;
	child.VAlign = VerticalAlignment::Stretch;
}

void ContentControl::ConfigureControlTemplateVisual(Control& child)
{
	child.GridRow = 0;
	child.GridColumn = 0;
	child.GridRowSpan = (std::numeric_limits<int>::max)();
	child.GridColumnSpan = (std::numeric_limits<int>::max)();
	child.HAlign = HorizontalAlignment::Stretch;
	child.VAlign = VerticalAlignment::Stretch;
	child.ZIndex = (std::numeric_limits<int>::min)() / 2;
}

Control* ContentControl::SetControlTemplateRoot(
	std::unique_ptr<Control> value)
{
	if (value.get() == _controlTemplateRoot) return _controlTemplateRoot;
	auto previous = DetachControlTemplateRoot();
	if (!value)
	{
		OnControlTemplatePresentationChanged();
		InvalidateLayout();
		InvalidateVisual();
		return nullptr;
	}

	ConfigureControlTemplateVisual(*value);
	_controlTemplateRoot = value.get();
	try
	{
		AddInfrastructureChild(std::move(value));
	}
	catch (...)
	{
		_controlTemplateRoot = nullptr;
		OnControlTemplatePresentationChanged();
		if (previous)
		{
			ConfigureControlTemplateVisual(*previous);
			_controlTemplateRoot = previous.get();
			AddInfrastructureChild(std::move(previous));
		}
		throw;
	}
	OnControlTemplatePresentationChanged();
	InvalidateLayout();
	InvalidateVisual();
	return _controlTemplateRoot;
}

std::unique_ptr<Control> ContentControl::DetachControlTemplateRoot()
{
	if (!_controlTemplateRoot) return {};
	auto* previous = _controlTemplateRoot;
	_controlTemplateRoot = nullptr;
	auto result = DetachInfrastructureChild(previous);
	OnControlTemplatePresentationChanged();
	return result;
}

bool ContentControl::RegisterTemplateContentPresenter(
	ContentPresenter* presenter)
{
	if (!presenter || (_templateContentPresenter
		&& _templateContentPresenter != presenter)) return false;
	_templateContentPresenter = presenter;
	return RebuildPresenter();
}

void ContentControl::OnControlTemplatePresentationChanged()
{
	if (!_controlTemplateRoot) _templateContentPresenter = nullptr;
	(void)RebuildPresenter();
}

void ContentControl::SetContent(BindingValue value)
{
	_lastContentError.clear();
	(void)SetPropertyField(L"Content", _content, std::move(value));
}

void ContentControl::SetContentTemplate(ItemTemplateReference value)
{
	_lastContentError.clear();
	(void)SetPropertyField(
		L"ContentTemplate", _contentTemplate, std::move(value));
}

void ContentControl::SetDisplayMemberPath(std::wstring value)
{
	(void)SetPropertyField(
		L"DisplayMemberPath", _displayMemberPath, std::move(value));
}

void ContentControl::SetContentTypeName(std::wstring value)
{
	if (_contentTypeName == value) return;
	const auto oldType = _contentTypeName;
	_contentTypeName = value;
	std::wstring validationError;
	if (!ValidateContentCandidate(
		_content, _contentTemplate, validationError))
	{
		_contentTypeName = oldType;
		_lastContentError = std::move(validationError);
		return;
	}
	if (RebuildPresenter()) return;
	const auto error = _lastContentError;
	_contentTypeName = oldType;
	(void)RebuildPresenter();
	_lastContentError = error;
}

bool ContentControl::ValidateContentCandidate(
	const BindingValue& content,
	const ItemTemplateReference& contentTemplate,
	std::wstring& error) const
{
	error.clear();
	if (GetVisualContent())
	{
		if (!content.Empty() || contentTemplate)
		{
			error = L"ContentControl 的直接视觉内容不能与数据内容同时使用。";
			return false;
		}
		return true;
	}
	ContentPresenter probe(0, 0, 0, 0);
	probe.SetContentTypeName(_contentTypeName);
	probe.SetDisplayMemberPath(_displayMemberPath);
	probe.SetContentTemplate(contentTemplate);
	if (!probe.LastTemplateError().empty())
	{
		error = probe.LastTemplateError();
		return false;
	}
	probe.SetContent(content);
	if (probe.LastTemplateError().empty()) return true;
	error = probe.LastTemplateError();
	return false;
}

bool ContentControl::RebuildPresenter()
{
	_lastContentError.clear();
	if (_contentTemplate && !_contentTypeName.empty()
		&& !_contentTemplate.Get()->DataTypeName().empty()
		&& !EqualsIgnoreCase(_contentTypeName,
			_contentTemplate.Get()->DataTypeName()))
	{
		_lastContentError =
			L"ContentTemplate DataType 与 Content DataType 不一致。";
		return false;
	}
	if (_templateContentPresenter || _controlTemplateRoot)
	{
		if (_presenter)
		{
			auto previous = DetachInfrastructureChild(_presenter);
			_presenter = nullptr;
		}
		InvalidateLayout();
		InvalidateVisual();
		return true;
	}
	if (GetVisualContent())
	{
		if (!_content.Empty() || _contentTemplate)
		{
			_lastContentError =
				L"ContentControl 的直接视觉内容不能与数据内容同时使用。";
			return false;
		}
		return true;
	}

	std::unique_ptr<ContentPresenter> replacement;
	if (!_content.Empty())
	{
		replacement = std::make_unique<ContentPresenter>(0, 0, 0, 0);
		replacement->HAlign = HorizontalAlignment::Stretch;
		replacement->VAlign = VerticalAlignment::Stretch;
		replacement->SetContentTypeName(_contentTypeName);
		replacement->SetDisplayMemberPath(_displayMemberPath);
		replacement->SetContentTemplate(_contentTemplate);
		replacement->SetContent(_content);
		ConfigureContentVisual(*replacement);
		if (!replacement->LastTemplateError().empty())
		{
			_lastContentError = replacement->LastTemplateError();
			return false;
		}
	}

	try
	{
		auto previous = _presenter
			? DetachInfrastructureChild(_presenter) : std::unique_ptr<Control>{};
		_presenter = replacement.get();
		if (replacement) AddInfrastructureChild(std::move(replacement));
	}
	catch (...)
	{
		_presenter = nullptr;
		throw;
	}
	InvalidateLayout();
	InvalidateVisual();
	return true;
}
