#include "ContentPresenter.h"
#include "ContentControl.h"
#include "HeaderedContentControl.h"
#include "HeaderedItemsControl.h"
#include "ItemContainer.h"

#include <utility>

#if !CUI_ENABLE_DYNAMIC_XAML
#error ContentProjection.Design.cpp requires the Design runtime flavor.
#endif

namespace
{
	const DependencyProperty* contentPresenterDisplayMemberPathProperty = nullptr;
	const DependencyProperty* contentControlDisplayMemberPathProperty = nullptr;
	const DependencyProperty* headeredContentDisplayMemberPathProperty = nullptr;
	const DependencyProperty* headeredItemsDisplayMemberPathProperty = nullptr;

	template<typename TOwner>
	DependencyPropertyOptions<TOwner, std::wstring>
		ProjectionPathOptions(int order)
	{
		DependencyPropertyOptions<TOwner, std::wstring> options;
		options.DefaultValue = std::wstring{};
		options.Flags = DependencyPropertyFlags::AffectsMeasure
			| DependencyPropertyFlags::AffectsArrange
			| DependencyPropertyFlags::AffectsRender;
		options.Design.Category = L"Data";
		options.Design.CategoryOrder = 80;
		options.Design.Order = order;
		options.Design.Editor = DependencyPropertyEditorKind::Text;
		options.Design.Persistence = DependencyPropertyPersistence::Metadata;
		return options;
	}
}

const DependencyProperty& ContentPresenter::DisplayMemberPathProperty()
{
	static const auto* property = []
	{
		RegisterDependencyProperties();
		return contentPresenterDisplayMemberPathProperty;
	}();
	return *property;
}

void ContentPresenter::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	(void)ContentPropertyMetadataRelation();
	(void)ContentTemplatePropertyMetadataRelation();
	static const bool registered = []
	{
		auto pathOptions = ProjectionPathOptions<ContentPresenter>(30);
		pathOptions.Changed = [](
			ContentPresenter& target,
			const std::wstring&, const std::wstring&)
		{
			if (!target._contentTemplate) (void)target.RebuildContent();
		};
		contentPresenterDisplayMemberPathProperty =
			DependencyPropertyRegistry::Register<ContentPresenter, std::wstring>(
				L"DisplayMemberPath",
				[](ContentPresenter& target)
				{ return target.GetDisplayMemberPath(); },
				[](ContentPresenter& target, const std::wstring& value)
				{ target.SetDisplayMemberPath(value); }, {},
				std::move(pathOptions));
		return true;
	}();
	(void)registered;
}

void ContentPresenter::SetDisplayMemberPath(std::wstring value)
{
	const bool hadCompiledPath = !_compiledDisplayMemberPath.Empty();
	_compiledDisplayMemberPath = {};
	const bool changed = SetPropertyField(
		DisplayMemberPathProperty(),
		_displayMemberPath, std::move(value));
	if (hadCompiledPath && !changed && !_contentTemplate)
		(void)RebuildContent();
}

void ContentPresenter::SetContentTypeName(std::wstring value)
{
	const auto token = MakeDataTypeToken(value);
	if (_contentTypeName == value && _contentTypeToken == token) return;
	const auto previousName = _contentTypeName;
	const auto previousToken = _contentTypeToken;
	_contentTypeName = std::move(value);
	_contentTypeToken = token;
	if (RebuildContent()) return;
	const auto error = _lastTemplateError;
	_contentTypeName = previousName;
	_contentTypeToken = previousToken;
	(void)RebuildContent();
	_lastTemplateError = error;
}

std::wstring ContentPresenter::ReadAuthoredProjectedDisplayText(
	const BindingSourceReference& source) const
{
	return GetBindingRecordText(source, _displayMemberPath);
}

BindingPathObservation ContentPresenter::ObserveAuthoredProjectedDisplayPath(
	const BindingSourceReference& source,
	std::function<void()> changed) const
{
	return _displayMemberPath.empty()
		? BindingPathObservation{}
		: ObserveBindingPaths(
			source, { _displayMemberPath }, std::move(changed));
}

const DependencyProperty& ContentControl::DisplayMemberPathProperty()
{
	static const auto* property = []
	{
		RegisterDependencyProperties();
		return contentControlDisplayMemberPathProperty;
	}();
	return *property;
}

void ContentControl::RegisterDependencyProperties()
{
	Control::RegisterDependencyProperties();
	(void)ContentProperty();
	(void)ContentTemplateProperty();
	static const bool registered = []
	{
		auto pathOptions = ProjectionPathOptions<ContentControl>(30);
		pathOptions.Changed = [](
			ContentControl& target,
			const std::wstring&, const std::wstring&)
		{
			(void)target.RebuildPresenter();
		};
		contentControlDisplayMemberPathProperty =
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

void ContentControl::SetDisplayMemberPath(std::wstring value)
{
	const bool hadCompiledPath = !_compiledDisplayMemberPath.Empty();
	_compiledDisplayMemberPath = {};
	const bool changed = SetPropertyField(
		DisplayMemberPathProperty(),
		_displayMemberPath, std::move(value));
	if (hadCompiledPath && !changed) (void)RebuildPresenter();
}

void ContentControl::SetContentTypeName(std::wstring value)
{
	const auto token = MakeDataTypeToken(value);
	if (_contentTypeName == value && _contentTypeToken == token) return;
	const auto oldName = _contentTypeName;
	const auto oldToken = _contentTypeToken;
	_contentTypeName = std::move(value);
	_contentTypeToken = token;
	std::wstring validationError;
	if (!ValidateContentCandidate(
		_content, _contentTemplate, validationError))
	{
		_contentTypeName = oldName;
		_contentTypeToken = oldToken;
		_lastContentError = std::move(validationError);
		return;
	}
	if (RebuildPresenter()) return;
	const auto error = _lastContentError;
	_contentTypeName = oldName;
	_contentTypeToken = oldToken;
	(void)RebuildPresenter();
	_lastContentError = error;
}

void ContentControl::ApplyAuthoredContentProjection(
	ContentPresenter& presenter) const
{
	presenter.SetContentTypeName(_contentTypeName);
	if (_compiledDisplayMemberPath.Empty())
		presenter.SetDisplayMemberPath(_displayMemberPath);
}

const DependencyProperty&
HeaderedContentControl::HeaderDisplayMemberPathProperty()
{
	static const auto* property = []
	{
		RegisterDependencyProperties();
		return headeredContentDisplayMemberPathProperty;
	}();
	return *property;
}

void HeaderedContentControl::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
	(void)HeaderProperty();
	(void)HeaderTemplateProperty();
	static const bool registered = []
	{
		auto pathOptions =
			ProjectionPathOptions<HeaderedContentControl>(60);
		pathOptions.Changed = [](
			HeaderedContentControl& target,
			const std::wstring&, const std::wstring&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		headeredContentDisplayMemberPathProperty =
			DependencyPropertyRegistry::Register<HeaderedContentControl,
				std::wstring>(
				L"HeaderDisplayMemberPath",
				[](HeaderedContentControl& target)
				{ return target.GetHeaderDisplayMemberPath(); },
				[](HeaderedContentControl& target,
					const std::wstring& value)
				{ target.SetHeaderDisplayMemberPath(value); }, {},
				std::move(pathOptions));
		return true;
	}();
	(void)registered;
}

void HeaderedContentControl::SetHeaderDisplayMemberPath(std::wstring value)
{
	const bool hadCompiledPath = !_compiledHeaderDisplayMemberPath.Empty();
	_compiledHeaderDisplayMemberPath = {};
	const bool changed = SetPropertyField(
		HeaderDisplayMemberPathProperty(), _headerDisplayMemberPath,
		std::move(value));
	if (hadCompiledPath && !changed)
		(void)RebuildHeaderPresenter();
}

void HeaderedContentControl::SetHeaderTypeName(std::wstring value)
{
	const auto token = MakeDataTypeToken(value);
	if (_headerTypeName == value && _headerTypeToken == token) return;
	const auto previousName = _headerTypeName;
	const auto previousToken = _headerTypeToken;
	_headerTypeName = std::move(value);
	_headerTypeToken = token;
	std::wstring validationError;
	if (!ValidateHeaderCandidate(
		_header, _headerTemplate, validationError))
	{
		_headerTypeName = previousName;
		_headerTypeToken = previousToken;
		_lastHeaderError = std::move(validationError);
		return;
	}
	if (RebuildHeaderPresenter()) return;
	const auto error = _lastHeaderError;
	_headerTypeName = previousName;
	_headerTypeToken = previousToken;
	(void)RebuildHeaderPresenter();
	_lastHeaderError = error;
}

void HeaderedContentControl::ApplyAuthoredHeaderProjection(
	ContentPresenter& presenter) const
{
	presenter.SetContentTypeName(_headerTypeName);
	if (_compiledHeaderDisplayMemberPath.Empty())
		presenter.SetDisplayMemberPath(_headerDisplayMemberPath);
}

const DependencyProperty&
HeaderedItemsControl::HeaderDisplayMemberPathProperty()
{
	static const auto* property = []
	{
		RegisterDependencyProperties();
		return headeredItemsDisplayMemberPathProperty;
	}();
	return *property;
}

void HeaderedItemsControl::RegisterDependencyProperties()
{
	ItemsControl::RegisterDependencyProperties();
	(void)HeaderPropertyMetadataRelation();
	(void)HeaderTemplatePropertyMetadataRelation();
	static const bool registered = []
	{
		auto pathOptions =
			ProjectionPathOptions<HeaderedItemsControl>(60);
		pathOptions.Changed = [](
			HeaderedItemsControl& target,
			const std::wstring&, const std::wstring&)
		{
			(void)target.RebuildHeaderPresenter();
		};
		headeredItemsDisplayMemberPathProperty =
			DependencyPropertyRegistry::Register<HeaderedItemsControl,
				std::wstring>(
				L"HeaderDisplayMemberPath",
				[](HeaderedItemsControl& target)
				{ return target.GetHeaderDisplayMemberPath(); },
				[](HeaderedItemsControl& target,
					const std::wstring& value)
				{ target.SetHeaderDisplayMemberPath(value); }, {},
				std::move(pathOptions));
		return true;
	}();
	(void)registered;
}

void HeaderedItemsControl::SetHeaderDisplayMemberPath(std::wstring value)
{
	const bool hadCompiledPath = !_compiledHeaderDisplayMemberPath.Empty();
	_compiledHeaderDisplayMemberPath = {};
	const bool changed = SetPropertyField(
		HeaderDisplayMemberPathProperty(), _headerDisplayMemberPath,
		std::move(value));
	if (hadCompiledPath && !changed)
		(void)RebuildHeaderPresenter();
}

void HeaderedItemsControl::SetHeaderTypeName(std::wstring value)
{
	const auto token = MakeDataTypeToken(value);
	if (_headerTypeName == value && _headerTypeToken == token) return;
	const auto previousName = _headerTypeName;
	const auto previousToken = _headerTypeToken;
	_headerTypeName = std::move(value);
	_headerTypeToken = token;
	std::wstring validationError;
	if (!ValidateHeaderCandidate(
		_header, _headerTemplate, validationError))
	{
		_headerTypeName = previousName;
		_headerTypeToken = previousToken;
		_lastHeaderError = std::move(validationError);
		return;
	}
	if (RebuildHeaderPresenter()) return;
	const auto error = _lastHeaderError;
	_headerTypeName = previousName;
	_headerTypeToken = previousToken;
	(void)RebuildHeaderPresenter();
	_lastHeaderError = error;
}

void HeaderedItemsControl::ApplyAuthoredHeaderProjection(
	ContentPresenter& presenter) const
{
	presenter.SetContentTypeName(_headerTypeName);
	if (_compiledHeaderDisplayMemberPath.Empty())
		presenter.SetDisplayMemberPath(_headerDisplayMemberPath);
}

bool ItemContainerControl::InitializeItem(
	const BindingSourceReference& item,
	const ItemTemplateReference& contentTemplate,
	const std::wstring& displayMemberPath,
	size_t index,
	const std::wstring& publicTypeName,
	std::wstring* outError)
{
	if (!item)
	{
		if (outError) *outError = publicTypeName + L" 缺少数据项。";
		return false;
	}
	_index = index;
	SetContentTypeName(contentTemplate
		? contentTemplate.Get()->DataTypeName() : std::wstring{});
	SetDisplayMemberPath(displayMemberPath);
	SetContentTemplate(contentTemplate);
	SetContent(BindingValue(item));
	if (!LastContentError().empty())
	{
		if (outError) *outError = LastContentError();
		return false;
	}
	if (outError) outError->clear();
	return true;
}

void ItemContainerControl::RegisterDependencyProperties()
{
	ContentControl::RegisterDependencyProperties();
	(void)IsSelectedProperty();
}
