#pragma once

#include "ContentPresenter.h"

/**
 * Owns one default content position. Authored visual content and data content
 * are mutually exclusive; data content is rendered by an internal
 * ContentPresenter so both paths keep one-child ownership semantics.
 */
class ContentControl : public GridPanel
{
public:
	ContentControl(int x = 0, int y = 0, int width = 200, int height = 80);
	UIClass Type() override { return UIClass::UI_ContentControl; }
	void EnsureBindingPropertiesRegistered() override;
	void Update() override;

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

	/** Returns the authored child, excluding the generated presenter. */
	Control* GetVisualContent() const noexcept;
	ContentPresenter* GetGeneratedPresenter() const noexcept
	{
		return _presenter;
	}
	ContentPresenter* GetTemplateContentPresenter() const noexcept
	{
		return _templateContentPresenter;
	}
	Control* GetGeneratedContent() const noexcept
	{
		auto* presenter = _templateContentPresenter
			? _templateContentPresenter : _presenter;
		return presenter ? presenter->GetGeneratedContent() : nullptr;
	}
	/** Framework hook used when a ControlTemplate declares ContentSource=Content. */
	bool RegisterTemplateContentPresenter(ContentPresenter* presenter);
	/**
	 * Framework-owned visual root instantiated from ControlTemplate. It is
	 * independent from Content, participates in the normal visual tree, and is
	 * always arranged behind the authored/generated content.
	 */
	Control* SetControlTemplateRoot(std::unique_ptr<Control> value);
	std::unique_ptr<Control> DetachControlTemplateRoot();
	Control* GetControlTemplateRoot() const noexcept
	{
		return _controlTemplateRoot;
	}
	const std::wstring& LastContentError() const noexcept
	{
		return _lastContentError;
	}

protected:
	bool ValidateChildCollection(
		std::span<Control* const> children,
		std::string& error) const override;
	void OnChildCollectionChanged(
		const CollectionChangedEventArgs& change,
		std::span<Control* const> previousChildren) override;

	/**
	 * Framework-owned visuals (presenters, headers, decorators) participate in
	 * layout and rendering without consuming the authored Content slot.
	 */
	Control* AddInfrastructureChild(std::unique_ptr<Control> child);
	std::unique_ptr<Control> DetachInfrastructureChild(Control* child);
	bool IsInfrastructureChild(const Control* child) const noexcept;
	virtual void ConfigureContentVisual(Control& child);
	virtual void ConfigureControlTemplateVisual(Control& child);
	virtual void OnControlTemplatePresentationChanged();

private:
	BindingValue _content;
	ItemTemplateReference _contentTemplate;
	std::wstring _displayMemberPath;
	std::wstring _contentTypeName;
	std::wstring _lastContentError;
	ContentPresenter* _presenter = nullptr;
	ContentPresenter* _templateContentPresenter = nullptr;
	Control* _controlTemplateRoot = nullptr;
	std::vector<Control*> _infrastructureChildren;
	bool _changingInfrastructure = false;

	bool ValidateContentCandidate(
		const BindingValue& content,
		const ItemTemplateReference& contentTemplate,
		std::wstring& error) const;
	bool RebuildPresenter();
};
