#pragma once

#include "ItemTemplate.h"
#include "Layout/GridPanel.h"

/**
 * Presents either one authored visual or one data value. ControlTemplate
 * ContentSource slots use the visual path, while scalar/object Content uses
 * the existing DataTemplate path; the two ownership modes are exclusive.
 */
class ContentPresenter : public GridPanel
{
public:
	ContentPresenter(int x = 0, int y = 0, int width = 200, int height = 80);
	UIClass Type() override { return UIClass::UI_ContentPresenter; }
	void EnsureBindingPropertiesRegistered() override;

	BindingValue GetContent() const { return _content; }
	void SetContent(BindingValue value);
	ItemTemplateReference GetContentTemplate() const noexcept
	{
		return _contentTemplate;
	}
	void SetContentTemplate(ItemTemplateReference value);
	const std::wstring& GetDisplayMemberPath() const noexcept
	{
		return _displayMemberPath;
	}
	void SetDisplayMemberPath(std::wstring value);
	const std::wstring& ContentTypeName() const noexcept
	{
		return _contentTypeName;
	}
	void SetContentTypeName(std::wstring value);
	/** Returns authored visual content, excluding generated DataTemplate output. */
	Control* GetVisualContent() const noexcept;
	Control* GetGeneratedContent() const noexcept { return _generatedContent; }
	const std::wstring& LastTemplateError() const noexcept
	{
		return _lastTemplateError;
	}

protected:
	bool ValidateChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;
	void OnChildCollectionChanged(
		const CollectionChangedEventArgs& change,
		std::span<Control* const> previousChildren) override;

private:
	BindingValue _content;
	ItemTemplateReference _contentTemplate;
	BindingPathObservation _contentObservation;
	std::wstring _displayMemberPath;
	std::wstring _contentTypeName;
	std::wstring _lastTemplateError;
	Control* _generatedContent = nullptr;
	bool _changingGeneratedContent = false;

	bool ValidateContentCandidate(
		const BindingValue& content,
		const ItemTemplateReference& contentTemplate,
		std::wstring& error) const;
	bool RebuildContent();
};
