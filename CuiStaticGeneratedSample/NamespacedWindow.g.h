#pragma once
#include "Binding.h"
#include "Border.h"
#include "Button.h"
#include "ContentPresenter.h"
#include "Control.h"
#include "RoutedCommand.h"
#include "Window.h"
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
	virtual ~MainWindowEventSink() { UnregisterDeclarativeEventHandlers(); }
	MainWindowEventSink(const MainWindowEventSink&) = delete;
	MainWindowEventSink& operator=(const MainWindowEventSink&) = delete;
	MainWindowEventSink(MainWindowEventSink&&) = delete;
	MainWindowEventSink& operator=(MainWindowEventSink&&) = delete;

	template<typename TRegistry>
	bool RegisterDeclarativeEventHandlers(
		TRegistry& registry, std::wstring* outError = nullptr)
	{
		try
		{
			auto lifetime = std::make_shared<int>(0);
			auto registration = registry.RegisterScopedBatch(
			[this, lifetime = std::weak_ptr<void>(lifetime)](
				auto& routes, std::wstring& error)
			{
				if (!routes.RegisterWindow(
					L"HandleWindowContentRendered", L"ContentRendered", &Window::ContentRendered,
					GuardDeclarativeEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(Window*)>(
								&MainWindowEventSink::HandleWindowContentRendered), this)), &error))
					return false;
				if (!routes.RegisterWindow(
					L"HandleStaticRefreshCanExecute", L"CanExecute", &UIElement::OnCanExecute,
					GuardDeclarativeEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(Control*,CanExecuteRoutedEventArgs&)>(
								&MainWindowEventSink::HandleStaticRefreshCanExecute), this)), &error))
					return false;
				if (!routes.RegisterWindow(
					L"HandleStaticRefreshExecuted", L"Executed", &UIElement::OnExecuted,
					GuardDeclarativeEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(Control*,ExecutedRoutedEventArgs&)>(
								&MainWindowEventSink::HandleStaticRefreshExecuted), this)), &error))
					return false;
				if (!routes.RegisterControl(
					L"HandleNamespacedClick", static_cast<UIClass>(7), L"Click", &ButtonBase::Click,
					GuardDeclarativeEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(Control*,RoutedEventArgs&)>(
								&MainWindowEventSink::HandleNamespacedClick), this)), &error))
					return false;
				if (!routes.RegisterControl(
					L"HandleNamespacedDrop", static_cast<UIClass>(0), L"Drop", &UIElement::OnDrop,
					GuardDeclarativeEventHandler(
						lifetime, std::bind_front(
							static_cast<void (MainWindowEventSink::*)(Control*,DragEventArgs&)>(
								&MainWindowEventSink::HandleNamespacedDrop), this)), &error))
					return false;
				return true;
			}, outError);
			if (!registration) return false;
			struct DeclarativeEventRegistration final
			{
				decltype(registration) Lease;
				std::shared_ptr<void> Lifetime;
				DeclarativeEventRegistration(
					decltype(registration)&& lease,
					std::shared_ptr<void> lifetime) noexcept
					: Lease(std::move(lease)),
					Lifetime(std::move(lifetime)) {}
			};
			auto owned = std::make_shared<DeclarativeEventRegistration>(
				std::move(registration), std::move(lifetime));
			_declarativeEventRegistration = std::move(owned);
			if (outError) outError->clear();
			return true;
		}
		catch (...)
		{
			if (outError) *outError =
				L"无法保存声明事件注册租约。";
			return false;
		}
	}

	void UnregisterDeclarativeEventHandlers() noexcept
	{
		_declarativeEventRegistration.reset();
	}

private:
	template<typename TCallback>
	static auto GuardDeclarativeEventHandler(
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

	std::shared_ptr<void> _declarativeEventRegistration;

protected:
	virtual void HandleWindowContentRendered(Window* sender) = 0;
	virtual void HandleStaticRefreshCanExecute(Control* sender, CanExecuteRoutedEventArgs& e) = 0;
	virtual void HandleStaticRefreshExecuted(Control* sender, ExecutedRoutedEventArgs& e) = 0;
	virtual void HandleNamespacedClick(Control* sender, RoutedEventArgs& e) = 0;
	virtual void HandleNamespacedDrop(Control* sender, DragEventArgs& e) = 0;
};

class MainWindowGenerated : public Window, public MainWindowEventSink
{
protected:
	Button* namespaceButton = nullptr;
	std::vector<EventConnection> _generatedEventConnections;
	bool _componentInitialized = false;
	void InitializeComponent();

	void HandleWindowContentRendered(Window* sender) override;
	void HandleStaticRefreshCanExecute(Control* sender, CanExecuteRoutedEventArgs& e) override;
	void HandleStaticRefreshExecuted(Control* sender, ExecutedRoutedEventArgs& e) override;
	void HandleNamespacedClick(Control* sender, RoutedEventArgs& e) override;
	void HandleNamespacedDrop(Control* sender, DragEventArgs& e) override;

public:
	// Stable identities shared by static and dynamic document paths.
	struct ControlIds final
	{
		static constexpr int namespaceButton = 77;
	};

	// Type-safe x:Name accessors; ownership remains with the generated Window.
	[[nodiscard]] Button* GetNamespaceButton() noexcept { return namespaceButton; }
	[[nodiscard]] const Button* GetNamespaceButton() const noexcept { return namespaceButton; }

	MainWindowGenerated();
	virtual ~MainWindowGenerated();
	bool BindData(BindingSourceReference dataContext);
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

private:
	DocumentReference _document;
};

}
