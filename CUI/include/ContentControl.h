#pragma once

#include "ContentPresenter.h"

namespace cui::framework
{
	struct TemplateAccess;
}

/**
 * Owns one default content position. Authored visual content and data content
 * are mutually exclusive; data content is rendered by an internal
 * ContentPresenter so both paths keep one-child ownership semantics.
 */
class ContentControl : public Control
{
public:
	ContentControl();
	UIClass Type() override { return UIClass::UI_ContentControl; }
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

	/** Returns the authored child, excluding the generated presenter. */
	Control* GetVisualContent() const noexcept;
	/** Replaces the single authored visual Content and transfers ownership. */
	Control* SetVisualContent(std::unique_ptr<Control> value);
	/** Attempts to attach Content while preserving value on failure. */
	bool TrySetVisualContent(std::unique_ptr<Control>& value) noexcept;
	/** Detaches the authored visual Content and returns its ownership. */
	std::unique_ptr<Control> DetachVisualContent();
	const std::wstring& LastContentError() const noexcept
	{
		return _lastContentError;
	}

protected:
	std::unique_ptr<AutomationPeer> OnCreateAutomationPeer() override
	{
		return std::make_unique<AutomationPeer>(
			*this, AutomationControlType::Group, L"ContentControl");
	}
	std::wstring GetSemanticText() const override;
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
	/** Framework-owned visual root instantiated from ControlTemplate. */
	Control* SetControlTemplateRoot(std::unique_ptr<Control> value) override;
	std::unique_ptr<Control> DetachVisualChildTemplateRoot() override;
	Control* GetControlTemplateRoot() const noexcept override
	{
		return _controlTemplateRoot;
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

	/**
	 * Framework-owned visuals (presenters, headers, decorators) participate in
	 * layout and rendering without consuming the authored Content slot. A
	 * directly authored Header remains a logical slot child; generated
	 * presenters and template roots are implementation-only.
	 */
	enum class InfrastructureChildRole
	{
		TemplateImplementation,
		LogicalSlot
	};
	Control* AddInfrastructureChild(
		std::unique_ptr<Control> child,
		InfrastructureChildRole role =
			InfrastructureChildRole::TemplateImplementation);
	std::unique_ptr<Control> DetachInfrastructureChild(Control* child);
	bool IsInfrastructureChild(const Control* child) const noexcept;
	virtual void ConfigureContentVisual(Control& child);
	void ConfigureControlTemplateVisual(Control& child) override;
	void OnControlTemplatePresentationChanged() override;
	/** Shared pending bit for semantic content-slot specializations. */
	bool _contentLayoutPending = true;

private:
	friend struct cui::framework::TemplateAccess;

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
