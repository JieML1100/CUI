#include "ContentControl.h"
#include "Layout/OverlayLayout.h"
#include "Window.h"
#include "XamlInfrastructure.h"
#include "TreeInfrastructure.h"

#include <cwctype>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
	bool EqualsTypeName(const std::wstring& left, const std::wstring& right)
	{
		return left == right;
	}

	void ClearTemplateOwner(Control* root, Control* owner)
	{
		if (!root || !owner) return;
		std::vector<Control*> stack{ root };
		while (!stack.empty())
		{
			auto* current = stack.back();
			stack.pop_back();
			if (!current) continue;
			for (auto* child : current->GetVisualChildrenView())
				if (child) stack.push_back(child);
			if (current->GetTemplatedParent() == owner)
				cui::framework::XamlAccess::SetTemplatedParent(*current, nullptr);
		}
	}

	template<typename TValue>
	DependencyPropertyOptions<ContentControl, TValue> ContentOptions(
		TValue defaultValue,
		int order,
		DependencyPropertyPersistence persistence =
			DependencyPropertyPersistence::Metadata)
	{
		DependencyPropertyOptions<ContentControl, TValue> options;
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

ContentControl::ContentControl()
	: Control()
{
}

void ContentControl::OnRender()
{
	if (!IsVisible || !GetPresentationWindow() || !GetDrawingContext()) return;
	BeginRender();
	const auto size = GetActualSizeDip();
	if (auto* background = CreateBackgroundBrush(
		*GetDrawingContext(), D2D1_SIZE_F{ size.width, size.height }))
	{
		GetDrawingContext()->FillRect(
			0.0f, 0.0f, size.width, size.height, background);
		background->Release();
	}
	EndRender();
}

void ContentControl::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	static const bool registered = []
	{
		auto contentOptions = ContentOptions(
			BindingValue{}, 10, DependencyPropertyPersistence::Native);
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
		DependencyPropertyRegistry::Register<ContentControl, BindingValue>(
			L"Content",
			[](ContentControl& target) { return target.GetContent(); },
			[](ContentControl& target, const BindingValue& value)
			{ target.SetContent(value); }, {}, std::move(contentOptions));

		auto templateOptions = ContentOptions(
			ItemTemplateReference{}, 20,
			DependencyPropertyPersistence::Native);
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
		DependencyPropertyRegistry::Register<ContentControl,
			ItemTemplateReference>(L"ContentTemplate",
			[](ContentControl& target) { return target.GetContentTemplate(); },
			[](ContentControl& target, const ItemTemplateReference& value)
			{ target.SetContentTemplate(value); }, {},
			std::move(templateOptions));

		auto pathOptions = ContentOptions(std::wstring{}, 30);
		pathOptions.Design.Editor = DependencyPropertyEditorKind::Text;
		pathOptions.Changed = [](
			ContentControl& target,
			const std::wstring&, const std::wstring&)
		{
			(void)target.RebuildPresenter();
		};
		DependencyPropertyRegistry::Register<ContentControl, std::wstring>(
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

cui::core::Size ContentControl::MeasureCore(
	const cui::core::Constraints& available)
{
	return cui::layout::MeasureOverlayChildren(
		GetLayoutChildrenView(), available,
		GetSpecifiedLayout().padding);
}

void ContentControl::Arrange(cui::core::Rect finalRect)
{
	Control::Arrange(finalRect);
	PerformPendingLayout();
}

void ContentControl::RequestLayout()
{
	_contentLayoutPending = true;
	Control::RequestLayout();
}

void ContentControl::OnComputedLayoutSizeChanged()
{
	_contentLayoutPending = true;
}

void ContentControl::PerformPendingLayout()
{
	if (IsLayoutSuspended() || !_contentLayoutPending) return;
	if (GetControlTemplateRoot())
	{
		// Control::Arrange already assigns the complete control slot to the
		// template root. Padding belongs to the authored template (typically via
		// TemplateBinding); consuming it again here double-insets every templated
		// ContentControl and collapses small buttons and icon buttons.  The root
		// must still commit layout invalidated by changes inside the template.
		GetControlTemplateRoot()->UpdateLayout();
		_contentLayoutPending = false;
		return;
	}
	const auto size = GetActualSizeDip();
	const auto padding = GetSpecifiedLayout().padding;
	cui::layout::ArrangeOverlayChildren(
		GetLayoutChildrenView(),
		cui::core::Rect{
			padding.left,
			padding.top,
			(std::max)(0.0f, size.width - padding.Horizontal()),
			(std::max)(0.0f, size.height - padding.Vertical()) });
	_contentLayoutPending = false;
}

Control* ContentControl::GetVisualContent() const noexcept
{
	Control* result = nullptr;
	for (auto* child : GetVisualChildrenView())
	{
		if (!child || IsInfrastructureChild(child)) continue;
		if (result) return nullptr;
		result = child;
	}
	if (result) return result;
	return _templateContentPresenter
		? _templateContentPresenter->GetVisualContent() : nullptr;
}

Control* ContentControl::SetVisualContent(std::unique_ptr<Control> value)
{
	if (value.get() == GetVisualContent()) return value.release();
	auto previous = DetachVisualContent();
	if (!value) return nullptr;
	try
	{
		return AddOwned(std::move(value));
	}
	catch (...)
	{
		if (previous) AddOwned(std::move(previous));
		throw;
	}
}

bool ContentControl::TrySetVisualContent(
	std::unique_ptr<Control>& value) noexcept
{
	if (!value || GetVisualContent()) return false;
	auto* raw = value.get();
	try
	{
		InsertVisualChild(static_cast<int>(GetVisualChildrenView().size()), raw);
		value.release();
		return true;
	}
	catch (...)
	{
		if (raw->GetVisualParent() == this)
		{
			try { value = DetachVisualChild(raw); }
			catch (...) {}
		}
		return false;
	}
}

std::unique_ptr<Control> ContentControl::DetachVisualContent()
{
	auto* content = GetVisualContent();
	return content ? DetachVisualChild(content) : std::unique_ptr<Control>{};
}

bool ContentControl::ValidateVisualChildCollection(
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

void ContentControl::OnVisualChildCollectionChanged(
	const CollectionChangedEventArgs& change,
	std::span<Control* const> previousChildren)
{
	(void)change;
	(void)previousChildren;
	_contentLayoutPending = true;
	if (auto* content = GetVisualContent()) ConfigureContentVisual(*content);
}

Control* ContentControl::AddInfrastructureChild(
	std::unique_ptr<Control> child,
	InfrastructureChildRole role)
{
	if (!child) return nullptr;
	auto* raw = child.get();
	_infrastructureChildren.push_back(raw);
	_changingInfrastructure = true;
	try
	{
		if (role == InfrastructureChildRole::TemplateImplementation)
		{
			if (!raw->GetTemplatedParent())
				cui::framework::XamlAccess::SetTemplatedParent(*raw, this);
			cui::framework::TreeAccess::AddOwnedVisualChild(
				*this, std::move(child), nullptr);
		}
		else
			cui::framework::TreeAccess::AddOwnedVisualChild(
				*this, std::move(child), this);
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
		result = DetachVisualChild(child);
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
	(void)child;
}

void ContentControl::ConfigureControlTemplateVisual(Control& child)
{
	Control::ConfigureControlTemplateVisual(child);
}

Control* ContentControl::SetControlTemplateRoot(
	std::unique_ptr<Control> value)
{
	if (value.get() == _controlTemplateRoot) return _controlTemplateRoot;
	(void)DetachVisualChildTemplateRoot();
	if (!value)
	{
		OnControlTemplatePresentationChanged();
		RequestLayout();
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
		throw;
	}
	MarkControlTemplateRootAttached();
	OnControlTemplatePresentationChanged();
	RequestLayout();
	InvalidateVisual();
	return _controlTemplateRoot;
}

std::unique_ptr<Control> ContentControl::DetachVisualChildTemplateRoot()
{
	if (!_controlTemplateRoot) return {};
	auto* previous = _controlTemplateRoot;
	_controlTemplateRoot = nullptr;
	auto result = DetachInfrastructureChild(previous);
	ClearTemplateOwner(result.get(), this);
	ClearDeclarativeTemplateScope();
	MarkControlTemplateRootDetached();
	OnControlTemplatePresentationChanged();
	return result;
}

bool ContentControl::RegisterTemplateContentPresenter(
	ContentPresenter* presenter)
{
	if (!presenter || presenter->GetTemplatedParent() != this
		|| (_templateContentPresenter
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

std::wstring ContentControl::GetSemanticText() const
{
	std::wstring value;
	return _content.TryGetString(value) ? value : std::wstring{};
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
	ContentPresenter probe;
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
		&& !EqualsTypeName(_contentTypeName,
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
		RequestLayout();
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
		replacement = std::make_unique<ContentPresenter>();
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
	RequestLayout();
	InvalidateVisual();
	return true;
}
