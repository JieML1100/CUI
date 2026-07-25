#include "ContentPresenter.h"

#include "Label.h"
#include "Layout/OverlayLayout.h"
#include "Window.h"

#include <algorithm>
#include <cwctype>
#include <utility>

namespace
{
	bool EqualsTypeName(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	template<typename TValue>
	DependencyPropertyOptions<ContentPresenter, TValue> DataOptions(
		TValue defaultValue,
		int order,
		DependencyPropertyPersistence persistence =
			DependencyPropertyPersistence::Metadata)
	{
		DependencyPropertyOptions<ContentPresenter, TValue> options;
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

ContentPresenter::ContentPresenter()
	: Control()
{
	RendererBackgroundColor = D2D1_COLOR_F{ 0, 0, 0, 0 };
}

void ContentPresenter::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	BeginRender();
	EndRender();
}

cui::core::Size ContentPresenter::MeasureCore(
	const cui::core::Constraints& available)
{
	return cui::layout::MeasureOverlayChildren(
		GetLayoutChildrenView(), available);
}

void ContentPresenter::Arrange(cui::core::Rect finalRect)
{
	Control::Arrange(finalRect);
	PerformPendingLayout();
}

void ContentPresenter::RequestLayout()
{
	_contentLayoutPending = true;
	Control::RequestLayout();
}

void ContentPresenter::OnComputedLayoutSizeChanged()
{
	_contentLayoutPending = true;
}

void ContentPresenter::PerformPendingLayout()
{
	if (IsLayoutSuspended() || !_contentLayoutPending) return;
	const auto size = GetActualSizeDip();
	cui::layout::ArrangeOverlayChildren(
		GetLayoutChildrenView(),
		cui::core::Rect{ 0.0f, 0.0f, size.width, size.height });
	_contentLayoutPending = false;
}

bool ContentPresenter::ValidateVisualChildCollection(
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
	for (auto* child : GetVisualChildrenView())
	{
		if (!child || child == _generatedContent) continue;
		if (result) return nullptr;
		result = child;
	}
	return result;
}

void ContentPresenter::OnVisualChildCollectionChanged(
	const CollectionChangedEventArgs& change,
	std::span<Control* const> previousChildren)
{
	(void)change;
	(void)previousChildren;
	_contentLayoutPending = true;
}

void ContentPresenter::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto contentOptions = DataOptions(
			BindingValue{}, 10,
			DependencyPropertyPersistence::Native);
		contentOptions.Design.Browsable = false;
		contentOptions.Coerce = [](
			ContentPresenter& target,
			const BindingValue& proposed) -> std::optional<BindingValue>
		{
			std::wstring error;
			try
			{
				if (!target.ValidateContentCandidate(
					proposed, target._contentTemplate, error))
				{
					target._lastTemplateError = std::move(error);
					return std::nullopt;
				}
			}
			catch (...)
			{
				target._lastTemplateError = std::move(error);
				throw;
			}
			return proposed;
		};
		contentOptions.Changed = [](
			ContentPresenter& target,
			const BindingValue&, const BindingValue&)
		{
			(void)target.RebuildContent();
		};
		DependencyPropertyRegistry::Register<ContentPresenter,
			BindingValue>(L"Content",
			[](ContentPresenter& target) { return target.GetContent(); },
			[](ContentPresenter& target, const BindingValue& value)
			{ target.SetContent(value); }, {}, std::move(contentOptions));

		auto templateOptions = DataOptions(
			ItemTemplateReference{}, 20,
			DependencyPropertyPersistence::Native);
		templateOptions.Design.Browsable = false;
		templateOptions.Coerce = [](
			ContentPresenter& target,
			const ItemTemplateReference& proposed)
			-> std::optional<ItemTemplateReference>
		{
			std::wstring error;
			try
			{
				if (!target.ValidateContentCandidate(
					target._content, proposed, error))
				{
					target._lastTemplateError = std::move(error);
					return std::nullopt;
				}
			}
			catch (...)
			{
				target._lastTemplateError = std::move(error);
				throw;
			}
			return proposed;
		};
		templateOptions.Changed = [](
			ContentPresenter& target,
			const ItemTemplateReference&, const ItemTemplateReference&)
		{
			(void)target.RebuildContent();
		};
		DependencyPropertyRegistry::Register<ContentPresenter,
			ItemTemplateReference>(L"ContentTemplate",
			[](ContentPresenter& target) { return target.GetContentTemplate(); },
			[](ContentPresenter& target, const ItemTemplateReference& value)
			{ target.SetContentTemplate(value); }, {}, std::move(templateOptions));

		auto pathOptions = DataOptions(std::wstring{}, 30);
		pathOptions.Design.Editor = DependencyPropertyEditorKind::Text;
		pathOptions.Changed = [](
			ContentPresenter& target,
			const std::wstring&, const std::wstring&)
		{
			if (!target._contentTemplate) (void)target.RebuildContent();
		};
		DependencyPropertyRegistry::Register<ContentPresenter, std::wstring>(
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
		&& !EqualsTypeName(
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
		&& !EqualsTypeName(_contentTypeName,
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
			auto label = std::make_unique<Label>();
			label->Text = hasSource
				? GetBindingRecordText(source, _displayMemberPath)
				: _content.ToString();
			replacement = std::move(label);
			if (hasSource)
				observation = ObserveBindingPaths(
					source, { _displayMemberPath }, [this]
					{ if (!_contentTemplate) (void)RebuildContent(); });
		}
	}

	_changingGeneratedContent = true;
	try
	{
		auto previous = _generatedContent
			? DetachVisualChild(_generatedContent) : std::unique_ptr<Control>{};
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
	RequestLayout();
	InvalidateVisual();
	return true;
}
