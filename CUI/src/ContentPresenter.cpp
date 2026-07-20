#include "ContentPresenter.h"

#include "Label.h"

#include <algorithm>
#include <cwctype>
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
	ControlPropertyOptions<ContentPresenter, TValue> DataOptions(
		TValue defaultValue,
		int order,
		ControlPropertyPersistence persistence =
			ControlPropertyPersistence::Metadata)
	{
		ControlPropertyOptions<ContentPresenter, TValue> options;
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

ContentPresenter::ContentPresenter(
	int x, int y, int width, int height)
	: GridPanel(x, y, width, height)
{
	BorderThickness = 0.0f;
	BackColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
}

bool ContentPresenter::ValidateChildCollection(
	std::span<Control* const> children,
	std::string& error) const
{
	if (!_changingGeneratedContent && _generatedContent
		&& std::find(children.begin(), children.end(), _generatedContent)
		== children.end())
	{
		error = "ContentPresenter generated content cannot be mutated directly";
		return false;
	}
	size_t authoredCount = 0;
	for (auto* child : children)
		if (child && child != _generatedContent) ++authoredCount;
	if (authoredCount > 1)
	{
		error = "ContentPresenter accepts at most one visual content root";
		return false;
	}
	if (authoredCount != 0
		&& (_generatedContent || !_content.Empty() || _contentTemplate))
	{
		error = "ContentPresenter visual content cannot be combined with data Content";
		return false;
	}
	return true;
}

Control* ContentPresenter::GetVisualContent() const noexcept
{
	Control* result = nullptr;
	for (auto* child : Children)
	{
		if (!child || child == _generatedContent) continue;
		if (result) return nullptr;
		result = child;
	}
	return result;
}

void ContentPresenter::OnChildCollectionChanged(
	const CollectionChangedEventArgs& change,
	std::span<Control* const> previousChildren)
{
	(void)change;
	(void)previousChildren;
	if (auto* content = GetVisualContent())
	{
		content->HAlign = HorizontalAlignment::Stretch;
		content->VAlign = VerticalAlignment::Stretch;
	}
}

void ContentPresenter::EnsureBindingPropertiesRegistered()
{
	GridPanel::EnsureBindingPropertiesRegistered();
	static const bool registered = []
	{
		auto contentOptions = DataOptions(
			BindingValue{}, 10,
			ControlPropertyPersistence::Transient);
		contentOptions.Design.Browsable = false;
		contentOptions.Coerce = [](
			ContentPresenter& target,
			const BindingValue& proposed) -> std::optional<BindingValue>
		{
			std::wstring error;
			if (!target.ValidateContentCandidate(
				proposed, target._contentTemplate, error))
			{
				target._lastTemplateError = std::move(error);
				return std::nullopt;
			}
			return proposed;
		};
		contentOptions.Changed = [](
			ContentPresenter& target,
			const BindingValue&, const BindingValue&)
		{
			(void)target.RebuildContent();
		};
		BindingPropertyRegistry::Register<ContentPresenter,
			BindingValue>(L"Content",
			[](ContentPresenter& target) { return target.GetContent(); },
			[](ContentPresenter& target, const BindingValue& value)
			{ target.SetContent(value); }, {}, std::move(contentOptions));

		auto templateOptions = DataOptions(
			ItemTemplateReference{}, 20,
			ControlPropertyPersistence::Transient);
		templateOptions.Design.Browsable = false;
		templateOptions.Coerce = [](
			ContentPresenter& target,
			const ItemTemplateReference& proposed)
			-> std::optional<ItemTemplateReference>
		{
			std::wstring error;
			if (!target.ValidateContentCandidate(
				target._content, proposed, error))
			{
				target._lastTemplateError = std::move(error);
				return std::nullopt;
			}
			return proposed;
		};
		templateOptions.Changed = [](
			ContentPresenter& target,
			const ItemTemplateReference&, const ItemTemplateReference&)
		{
			(void)target.RebuildContent();
		};
		BindingPropertyRegistry::Register<ContentPresenter,
			ItemTemplateReference>(L"ContentTemplate",
			[](ContentPresenter& target) { return target.GetContentTemplate(); },
			[](ContentPresenter& target, const ItemTemplateReference& value)
			{ target.SetContentTemplate(value); }, {}, std::move(templateOptions));

		auto pathOptions = DataOptions(std::wstring{}, 30);
		pathOptions.Design.Editor = ControlPropertyEditorKind::Text;
		pathOptions.Changed = [](
			ContentPresenter& target,
			const std::wstring&, const std::wstring&)
		{
			if (!target._contentTemplate) (void)target.RebuildContent();
		};
		BindingPropertyRegistry::Register<ContentPresenter, std::wstring>(
			L"DisplayMemberPath",
			[](ContentPresenter& target)
			{ return target.GetDisplayMemberPath(); },
			[](ContentPresenter& target, const std::wstring& value)
			{ target.SetDisplayMemberPath(value); }, {}, std::move(pathOptions));
		return true;
	}();
	(void)registered;
}

void ContentPresenter::SetContent(BindingValue value)
{
	_lastTemplateError.clear();
	(void)SetPropertyField(L"Content", _content, std::move(value));
}

void ContentPresenter::SetContentTemplate(ItemTemplateReference value)
{
	_lastTemplateError.clear();
	(void)SetPropertyField(
		L"ContentTemplate", _contentTemplate, std::move(value));
}

void ContentPresenter::SetDisplayMemberPath(std::wstring value)
{
	(void)SetPropertyField(
		L"DisplayMemberPath", _displayMemberPath, std::move(value));
}

void ContentPresenter::SetContentTypeName(std::wstring value)
{
	if (_contentTypeName == value) return;
	const auto previous = _contentTypeName;
	_contentTypeName = std::move(value);
	if (RebuildContent()) return;
	const auto error = _lastTemplateError;
	_contentTypeName = previous;
	(void)RebuildContent();
	_lastTemplateError = error;
}

bool ContentPresenter::ValidateContentCandidate(
	const BindingValue& content,
	const ItemTemplateReference& contentTemplate,
	std::wstring& error) const
{
	error.clear();
	if (GetVisualContent())
	{
		if (!content.Empty() || contentTemplate)
		{
			error = L"ContentPresenter 的视觉内容不能与数据内容同时使用。";
			return false;
		}
		return true;
	}
	if (contentTemplate && !_contentTypeName.empty()
		&& !contentTemplate.Get()->DataTypeName().empty()
		&& !EqualsIgnoreCase(
			_contentTypeName, contentTemplate.Get()->DataTypeName()))
	{
		error = L"ContentTemplate DataType 与 Content DataType 不一致。";
		return false;
	}
	if (content.Empty() || !contentTemplate) return true;
	BindingSourceReference source;
	if (!content.TryGet(source) || !source)
	{
		error = L"当前 DataTemplate 只支持 BindingSource 内容。";
		return false;
	}
	auto probe = contentTemplate.Get()->Build(source, 0, &error);
	if (probe) return true;
	if (error.empty()) error = L"ContentTemplate 未生成视觉根。";
	return false;
}

bool ContentPresenter::RebuildContent()
{
	_lastTemplateError.clear();
	if (GetVisualContent())
	{
		if (!_content.Empty() || _contentTemplate)
		{
			_lastTemplateError =
				L"ContentPresenter 的视觉内容不能与数据内容同时使用。";
			return false;
		}
		return true;
	}
	if (_contentTemplate && !_contentTypeName.empty()
		&& !_contentTemplate.Get()->DataTypeName().empty()
		&& !EqualsIgnoreCase(_contentTypeName,
			_contentTemplate.Get()->DataTypeName()))
	{
		_lastTemplateError =
			L"ContentTemplate DataType 与 Content DataType 不一致。";
		return false;
	}

	std::unique_ptr<Control> replacement;
	BindingPathObservation observation;
	if (!_content.Empty())
	{
		BindingSourceReference source;
		const bool hasSource = _content.TryGet(source) && source;
		if (_contentTemplate)
		{
			if (!hasSource)
			{
				_lastTemplateError =
					L"当前 DataTemplate 只支持 BindingSource 内容。";
				return false;
			}
			replacement = _contentTemplate.Get()->Build(
				source, 0, &_lastTemplateError);
			if (!replacement)
			{
				if (_lastTemplateError.empty())
					_lastTemplateError = L"ContentTemplate 未生成视觉根。";
				return false;
			}
		}
		else
		{
			replacement = std::make_unique<Label>(
				hasSource
					? GetBindingRecordText(source, _displayMemberPath,
						{ L"Text", L"Content", L"Name" })
					: _content.ToString(), 0, 0);
			if (hasSource)
				observation = ObserveBindingPaths(
					source, { _displayMemberPath }, [this]
					{ if (!_contentTemplate) (void)RebuildContent(); });
		}
		replacement->HAlign = HorizontalAlignment::Stretch;
		replacement->VAlign = VerticalAlignment::Stretch;
	}

	_changingGeneratedContent = true;
	try
	{
		auto previous = _generatedContent
			? DetachControl(_generatedContent) : std::unique_ptr<Control>{};
		_generatedContent = replacement.get();
		if (replacement) AddOwned(std::move(replacement));
		_changingGeneratedContent = false;
	}
	catch (...)
	{
		_changingGeneratedContent = false;
		_generatedContent = nullptr;
		throw;
	}
	_contentObservation = std::move(observation);
	InvalidateLayout();
	InvalidateVisual();
	return true;
}
