#pragma once

#include "ItemTemplate.h"
#include "Control.h"

/**
 * Presents either one authored visual or one data value. ControlTemplate
 * ContentSource slots use the visual path, while scalar/object Content uses
 * the existing DataTemplate path; the two ownership modes are exclusive.
 */
class ContentPresenter : public Control
{
public:
	ContentPresenter();
	UIClass Type() override { return UIClass::UI_ContentPresenter; }
	static void RegisterDependencyProperties();
	void EnsureBindingPropertiesRegistered() override { RegisterDependencyProperties(); }
protected:
	void OnRender() override;
public:
	cui::core::Size MeasureCore(
		const cui::core::Constraints& available) override;
	void Arrange(cui::core::Rect finalRect) override;

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
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Pane, L"ContentPresenter");
	}
	void RequestLayout() override;
	void OnComputedLayoutSizeChanged() override;
	void PerformPendingLayout() override;
	bool ValidateVisualChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;
	void OnVisualChildCollectionChanged(
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
	bool _contentLayoutPending = true;

	bool ValidateContentCandidate(
		const BindingValue& content,
		const ItemTemplateReference& contentTemplate,
		std::wstring& error) const;
	bool RebuildContent();
};
