#pragma once
#include "Binding.h"
#include "Button.h"
#include "Control.h"
#include "Controls/StatusBadge.h"
#include "Form.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace Acme::Views
{

class MainWindowEventSink
{
public:
	MainWindowEventSink() = default;
	virtual ~MainWindowEventSink() { UnregisterDynamicEventHandlers(); }
	MainWindowEventSink(const MainWindowEventSink&) = delete;
	MainWindowEventSink& operator=(const MainWindowEventSink&) = delete;
	MainWindowEventSink(MainWindowEventSink&&) = delete;
	MainWindowEventSink& operator=(MainWindowEventSink&&) = delete;

	template<typename TRegistry>
	bool RegisterDynamicEventHandlers(
		TRegistry& registry, std::wstring* outError = nullptr)
	{
		try
		{
			auto lifetime = std::make_shared<int>(0);
			auto registration = registry.RegisterScopedBatch(
			[this, lifetime = std::weak_ptr<void>(lifetime)](
				auto& routes, std::wstring& error)
			{
				if (!routes.RegisterForm(
					L"HandleWindowShown", L"OnShown", &Form::OnShown,
					GuardDynamicEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(Form*)>(
								&MainWindowEventSink::HandleWindowShown), this)), &error))
					return false;
				if (!routes.RegisterControl(
					L"HandleNamespacedDrop", UIClass::UI_Base, L"OnDropFile", &Control::OnDropFile,
					GuardDynamicEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(Control*,std::vector<std::wstring>)>(
								&MainWindowEventSink::HandleNamespacedDrop), this)), &error))
					return false;
				if (!routes.RegisterControl(
					L"HandleNamespacedClick", UIClass::UI_Base, L"OnMouseClick", &Control::OnMouseClick,
					GuardDynamicEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(Control*,MouseEventArgs)>(
								&MainWindowEventSink::HandleNamespacedClick), this)), &error))
					return false;
				if (!routes.RegisterControl(
					L"HandleNamespacedPropertyChanged", UIClass::UI_Base, L"OnPropertyValueChanged", &Control::OnPropertyValueChanged,
					GuardDynamicEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(Control*,const ControlPropertyChangedEventArgs&)>(
								&MainWindowEventSink::HandleNamespacedPropertyChanged), this)), &error))
					return false;
				if (!routes.RegisterControl(
					L"HandleNamespacedValidationChanged", UIClass::UI_Base, L"OnValidationStateChanged", &Control::OnValidationStateChanged,
					GuardDynamicEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(const BindingValidationChangedEventArgs&)>(
								&MainWindowEventSink::HandleNamespacedValidationChanged), this)), &error))
					return false;
				if (!routes.RegisterGeneratedCustomControl(
					L"HandleSeverityInvoked", L"urn:cui:samples", L"StatusBadge", L"OnSeverityInvoked", "OnSeverityInvoked", L"SenderInt",
					&Acme::Controls::StatusBadge::OnSeverityInvoked,
					GuardDynamicEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(Control*,int)>(
								&MainWindowEventSink::HandleSeverityInvoked), this)), &error))
					return false;
				return true;
			}, outError);
			if (!registration) return false;
			struct DynamicEventRegistration final
			{
				decltype(registration) Lease;
				std::shared_ptr<void> Lifetime;
				DynamicEventRegistration(
					decltype(registration)&& lease,
					std::shared_ptr<void> lifetime) noexcept
					: Lease(std::move(lease)),
					Lifetime(std::move(lifetime)) {}
			};
			auto owned = std::make_shared<DynamicEventRegistration>(
				std::move(registration), std::move(lifetime));
			_dynamicEventRegistration = std::move(owned);
			if (outError) outError->clear();
			return true;
		}
		catch (...)
		{
			if (outError) *outError =
				L"无法保存动态事件注册租约。";
			return false;
		}
	}

	void UnregisterDynamicEventHandlers() noexcept
	{
		_dynamicEventRegistration.reset();
	}

private:
	template<typename TCallback>
	static auto GuardDynamicEventHandler(
		std::weak_ptr<void> lifetime, TCallback callback)
	{
		return [lifetime = std::move(lifetime),
			callback = std::move(callback)](auto&&... args) mutable
		{
			auto alive = lifetime.lock();
			if (!alive) return;
			std::invoke(callback,
				std::forward<decltype(args)>(args)...);
		};
	}

	std::shared_ptr<void> _dynamicEventRegistration;

protected:
	virtual void HandleWindowShown(Form* sender) = 0;
	virtual void HandleNamespacedDrop(Control* sender, std::vector<std::wstring> files) = 0;
	virtual void HandleNamespacedClick(Control* sender, MouseEventArgs e) = 0;
	virtual void HandleNamespacedPropertyChanged(Control* sender, const ControlPropertyChangedEventArgs& e) = 0;
	virtual void HandleNamespacedValidationChanged(const BindingValidationChangedEventArgs& e) = 0;
	virtual void HandleSeverityInvoked(Control* sender, int value) = 0;
};

class MainWindowGenerated : public Form, public MainWindowEventSink
{
protected:
	Button* namespaceButton = nullptr;
	Acme::Controls::StatusBadge* statusBadge = nullptr;
	std::vector<EventConnection> _generatedEventConnections;

	void HandleWindowShown(Form* sender) override;
	void HandleNamespacedDrop(Control* sender, std::vector<std::wstring> files) override;
	void HandleNamespacedClick(Control* sender, MouseEventArgs e) override;
	void HandleNamespacedPropertyChanged(Control* sender, const ControlPropertyChangedEventArgs& e) override;
	void HandleNamespacedValidationChanged(const BindingValidationChangedEventArgs& e) override;
	void HandleSeverityInvoked(Control* sender, int value) override;

public:
	// Stable identities shared by static and dynamic document paths.
	struct ControlIds final
	{
		static constexpr int namespaceButton = 77;
		static constexpr int statusBadge = 78;
	};

	// Type-safe x:Name accessors; ownership remains with the generated Form.
	[[nodiscard]] Button* GetNamespaceButton() noexcept { return namespaceButton; }
	[[nodiscard]] const Button* GetNamespaceButton() const noexcept { return namespaceButton; }
	[[nodiscard]] Acme::Controls::StatusBadge* GetStatusBadge() noexcept { return statusBadge; }
	[[nodiscard]] const Acme::Controls::StatusBadge* GetStatusBadge() const noexcept { return statusBadge; }

	MainWindowGenerated();
	virtual ~MainWindowGenerated();
};

// Non-owning typed access for a dynamically loaded document.
// GetXxx resolves the current instance; ReferenceXxx follows reloads.
template<typename TDocument>
class MainWindowReferences final
{
public:
	using DocumentReference = decltype(
		std::declval<TDocument&>().Reference());

	explicit MainWindowReferences(TDocument& document) noexcept
		: _document(document.Reference()) {}

	[[nodiscard]] explicit operator bool() const noexcept
	{
		return static_cast<bool>(_document);
	}
	[[nodiscard]] TDocument* TryDocument() const noexcept
	{
		return _document.Get();
	}
	// Precondition: the view is still alive; prefer TryDocument() when uncertain.
	[[nodiscard]] TDocument& Document() const noexcept { return *_document.Get(); }
	[[nodiscard]] Button* GetNamespaceButton() const noexcept
	{
		return _document.template FindControlByDesignId<Button>(
			MainWindowGenerated::ControlIds::namespaceButton);
	}
	[[nodiscard]] auto ReferenceNamespaceButton() const noexcept
	{
		return _document.template ReferenceByDesignId<Button>(
			MainWindowGenerated::ControlIds::namespaceButton);
	}
	[[nodiscard]] Acme::Controls::StatusBadge* GetStatusBadge() const noexcept
	{
		return _document.template FindControlByDesignId<Acme::Controls::StatusBadge>(
			MainWindowGenerated::ControlIds::statusBadge);
	}
	[[nodiscard]] auto ReferenceStatusBadge() const noexcept
	{
		return _document.template ReferenceByDesignId<Acme::Controls::StatusBadge>(
			MainWindowGenerated::ControlIds::statusBadge);
	}

private:
	DocumentReference _document;
};

}
