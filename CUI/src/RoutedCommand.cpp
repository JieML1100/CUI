#include "RoutedCommand.h"
#include "EventInfrastructure.h"

#include "Control.h"
#include "Core/Threading.h"
#include "InputManager.h"
#include "UIElement.h"
#include "Window.h"
#include "WindowInfrastructure.h"
#include "RuntimeTypeMetadata.h"

#include <algorithm>
#include <atomic>
#include <cwctype>
#include <mutex>
#include <unordered_map>

struct CommandBindingCollectionState final
{
	struct Entry final
	{
		std::uint64_t Token = 0;
		CommandBinding Binding;
	};

	Control* Owner = nullptr;
	std::uint64_t NextToken = 1;
	std::vector<Entry> Entries;
};

struct CommandCanExecuteObserverState final
{
	struct Entry final
	{
		std::uint64_t Token = 0;
		RoutedCommandSourceQuery Query;
		RoutedCommandManager::CanExecuteObserver Observer;
	};

	Control* Owner = nullptr;
	Window* Domain = nullptr;
	std::uint64_t NextToken = 1;
	std::vector<Entry> Entries;
};

struct RoutedCommandManager::RequeryState final
{
	explicit RequeryState(Window& owner) noexcept : Owner(&owner) {}

	std::mutex Mutex;
	Window* Owner = nullptr;
	std::uint64_t Generation = 0;
	bool Pending = false;
	bool Posted = false;
	Event<void(const RoutedCommandRequeryEventArgs&)> Suggested;
	std::vector<std::weak_ptr<CommandCanExecuteObserverState>> Observers;
};

namespace
{
	std::wstring Trim(std::wstring_view text)
	{
		auto first = text.begin();
		auto last = text.end();
		while (first != last && std::iswspace(*first)) ++first;
		while (last != first && std::iswspace(*(last - 1))) --last;
		return { first, last };
	}

	std::wstring Fold(std::wstring_view text)
	{
		auto value = Trim(text);
		std::transform(value.begin(), value.end(), value.begin(),
			[](wchar_t ch) { return static_cast<wchar_t>(std::towupper(ch)); });
		return value;
	}

	bool TryParseKey(std::wstring_view token, Key& result)
	{
		const auto value = Fold(token);
		if (value.size() == 1)
		{
			const wchar_t ch = value.front();
			if (ch >= L'A' && ch <= L'Z')
			{
				result = static_cast<Key>(static_cast<std::uint16_t>(Key::A)
					+ static_cast<std::uint16_t>(ch - L'A'));
				return true;
			}
			if (ch >= L'0' && ch <= L'9')
			{
				result = static_cast<Key>(static_cast<std::uint16_t>(Key::D0)
					+ static_cast<std::uint16_t>(ch - L'0'));
				return true;
			}
		}
		if (value.size() >= 2 && value.front() == L'F')
		{
			wchar_t* end = nullptr;
			const auto number = std::wcstol(value.c_str() + 1, &end, 10);
			if (end && *end == L'\0' && number >= 1 && number <= 24)
			{
				result = static_cast<Key>(static_cast<int>(Key::F1)
					+ static_cast<int>(number - 1));
				return true;
			}
		}

		static const std::unordered_map<std::wstring, Key> names{
			{ L"BACK", Key::Back }, { L"BACKSPACE", Key::Back },
			{ L"TAB", Key::Tab }, { L"ENTER", Key::Enter },
			{ L"RETURN", Key::Return }, { L"ESC", Key::Escape },
			{ L"ESCAPE", Key::Escape }, { L"SPACE", Key::Space },
			{ L"PAGEUP", Key::PageUp }, { L"PAGEDOWN", Key::PageDown },
			{ L"HOME", Key::Home }, { L"END", Key::End },
			{ L"LEFT", Key::Left }, { L"UP", Key::Up },
			{ L"RIGHT", Key::Right }, { L"DOWN", Key::Down },
			{ L"INSERT", Key::Insert }, { L"DELETE", Key::Delete },
			{ L"PLUS", Key::OemPlus }, { L"OEMPLUS", Key::OemPlus },
			{ L"MINUS", Key::OemMinus }, { L"OEMMINUS", Key::OemMinus },
			{ L"COMMA", Key::OemComma }, { L"PERIOD", Key::OemPeriod },
		};
		if (const auto found = names.find(value); found != names.end())
		{
			result = found->second;
			return true;
		}
		return false;
	}

	std::wstring KeyName(Key key)
	{
		const auto code = static_cast<int>(key);
		if (key >= Key::A && key <= Key::Z)
			return std::wstring(1, static_cast<wchar_t>(
				L'A' + code - static_cast<int>(Key::A)));
		if (key >= Key::D0 && key <= Key::D9)
			return std::wstring(1, static_cast<wchar_t>(
				L'0' + code - static_cast<int>(Key::D0)));
		if (code >= static_cast<int>(Key::F1)
			&& code <= static_cast<int>(Key::F24))
			return L"F" + std::to_wstring(code - static_cast<int>(Key::F1) + 1);
		switch (static_cast<Key>(code))
		{
		case Key::Back: return L"Backspace";
		case Key::Tab: return L"Tab";
		case Key::Enter: return L"Enter";
		case Key::Escape: return L"Escape";
		case Key::Space: return L"Space";
		case Key::PageUp: return L"PageUp";
		case Key::PageDown: return L"PageDown";
		case Key::Home: return L"Home";
		case Key::End: return L"End";
		case Key::Left: return L"Left";
		case Key::Up: return L"Up";
		case Key::Right: return L"Right";
		case Key::Down: return L"Down";
		case Key::Insert: return L"Insert";
		case Key::Delete: return L"Delete";
		case Key::OemPlus: return L"Plus";
		case Key::OemMinus: return L"Minus";
		case Key::OemComma: return L"Comma";
		case Key::OemPeriod: return L"Period";
		default: return {};
		}
	}

	bool CommandMatches(const RoutedCommand& expected, const RoutedCommand& actual)
	{
		return !expected.Empty() && expected == actual;
	}

	enum class ClassCommandBindingOwnerKind : unsigned char
	{
		Declarative,
		Native,
	};

	struct ClassCommandBindingEntry final
	{
		std::uint64_t Token = 0;
		ClassCommandBindingOwnerKind OwnerKind =
			ClassCommandBindingOwnerKind::Native;
		ComponentTypeToken ComponentOwner;
		UIClass NativeOwner = UIClass::UI_Base;
		CommandBinding Binding;
	};

	struct ClassCommandBindingRegistry final
	{
		std::mutex Mutex;
		std::uint64_t NextToken = 1;
		std::vector<ClassCommandBindingEntry> Entries;
	};

	ClassCommandBindingRegistry& ClassCommandBindings()
	{
		static ClassCommandBindingRegistry registry;
		return registry;
	}

	std::atomic<std::uint64_t> NextCommandTransactionId{ 1 };
	std::atomic<std::uint64_t> NextCommandRouteId{ 1 };

	bool TryParseMouseAction(std::wstring_view token, MouseAction& result)
	{
		const auto value = Fold(token);
		if (value == L"LEFTCLICK") result = MouseAction::LeftClick;
		else if (value == L"RIGHTCLICK") result = MouseAction::RightClick;
		else if (value == L"MIDDLECLICK") result = MouseAction::MiddleClick;
		else if (value == L"WHEELCLICK") result = MouseAction::WheelClick;
		else if (value == L"LEFTDOUBLECLICK") result = MouseAction::LeftDoubleClick;
		else if (value == L"RIGHTDOUBLECLICK") result = MouseAction::RightDoubleClick;
		else if (value == L"MIDDLEDOUBLECLICK") result = MouseAction::MiddleDoubleClick;
		else return false;
		return true;
	}

	std::wstring MouseActionName(MouseAction action)
	{
		switch (action)
		{
		case MouseAction::LeftClick: return L"LeftClick";
		case MouseAction::RightClick: return L"RightClick";
		case MouseAction::MiddleClick: return L"MiddleClick";
		case MouseAction::WheelClick: return L"WheelClick";
		case MouseAction::LeftDoubleClick: return L"LeftDoubleClick";
		case MouseAction::RightDoubleClick: return L"RightDoubleClick";
		case MouseAction::MiddleDoubleClick: return L"MiddleDoubleClick";
		default: return {};
		}
	}
}

bool KeyGesture::IsValid() const noexcept
{
	return Key != ::Key::None && Key != ::Key::System;
}

bool KeyGesture::Matches(
	::Key key,
	ModifierKeys modifiers) const noexcept
{
	return IsValid() && key == Key && modifiers == Modifiers;
}

bool TryParseKeyGesture(
	std::wstring_view text,
	KeyGesture& result,
	std::wstring* error)
{
	result = {};
	const auto value = Trim(text);
	if (value.empty())
	{
		if (error) *error = L"KeyGesture 不能为空。";
		return false;
	}

	std::size_t start = 0;
	bool hasKey = false;
	while (start <= value.size())
	{
		const auto separator = value.find(L'+', start);
		const auto token = Trim(std::wstring_view(value).substr(
			start, separator == std::wstring::npos
				? value.size() - start : separator - start));
		const auto folded = Fold(token);
		if (folded == L"CTRL" || folded == L"CONTROL")
			result.Modifiers |= ModifierKeys::Control;
		else if (folded == L"ALT")
			result.Modifiers |= ModifierKeys::Alt;
		else if (folded == L"SHIFT")
			result.Modifiers |= ModifierKeys::Shift;
		else if (folded == L"WIN" || folded == L"WINDOWS")
			result.Modifiers |= ModifierKeys::Windows;
		else if (!token.empty() && !hasKey && TryParseKey(token, result.Key))
			hasKey = true;
		else
		{
			if (error) *error = L"无法识别 KeyGesture 片段: " + token;
			result = {};
			return false;
		}
		if (separator == std::wstring::npos) break;
		start = separator + 1;
	}
	if (!hasKey)
	{
		if (error) *error = L"KeyGesture 缺少按键。";
		result = {};
		return false;
	}
	return true;
}

std::wstring FormatKeyGesture(const KeyGesture& gesture)
{
	if (!gesture.IsValid()) return {};
	std::wstring result;
	if (HasModifier(gesture.Modifiers, ModifierKeys::Control)) result += L"Ctrl+";
	if (HasModifier(gesture.Modifiers, ModifierKeys::Alt)) result += L"Alt+";
	if (HasModifier(gesture.Modifiers, ModifierKeys::Shift)) result += L"Shift+";
	if (HasModifier(gesture.Modifiers, ModifierKeys::Windows)) result += L"Windows+";
	result += KeyName(gesture.Key);
	return result;
}

bool MouseGesture::IsValid() const noexcept
{
	return Action != MouseAction::None;
}

bool MouseGesture::Matches(
	const MouseEventArgs& input,
	ModifierKeys modifiers) const noexcept
{
	if (!IsValid() || modifiers != Modifiers)
		return false;
	switch (Action)
	{
	case MouseAction::LeftClick:
		return input.ChangedButton == MouseButton::Left && input.ClickCount == 1;
	case MouseAction::RightClick:
		return input.ChangedButton == MouseButton::Right && input.ClickCount == 1;
	case MouseAction::MiddleClick:
		return input.ChangedButton == MouseButton::Middle && input.ClickCount == 1;
	case MouseAction::WheelClick:
		return input.WheelDelta != 0;
	case MouseAction::LeftDoubleClick:
		return input.ChangedButton == MouseButton::Left && input.ClickCount == 2;
	case MouseAction::RightDoubleClick:
		return input.ChangedButton == MouseButton::Right && input.ClickCount == 2;
	case MouseAction::MiddleDoubleClick:
		return input.ChangedButton == MouseButton::Middle && input.ClickCount == 2;
	default:
		return false;
	}
}

bool TryParseMouseGesture(
	std::wstring_view text,
	MouseGesture& result,
	std::wstring* error)
{
	result = {};
	const auto value = Trim(text);
	if (value.empty())
	{
		if (error) *error = L"MouseGesture 不能为空。";
		return false;
	}

	std::size_t start = 0;
	bool hasAction = false;
	while (start <= value.size())
	{
		const auto separator = value.find(L'+', start);
		const auto token = Trim(std::wstring_view(value).substr(
			start, separator == std::wstring::npos
				? value.size() - start : separator - start));
		const auto folded = Fold(token);
		if (folded == L"CTRL" || folded == L"CONTROL")
			result.Modifiers |= ModifierKeys::Control;
		else if (folded == L"ALT")
			result.Modifiers |= ModifierKeys::Alt;
		else if (folded == L"SHIFT")
			result.Modifiers |= ModifierKeys::Shift;
		else if (folded == L"WIN" || folded == L"WINDOWS")
			result.Modifiers |= ModifierKeys::Windows;
		else if (!token.empty() && !hasAction
			&& TryParseMouseAction(token, result.Action)) hasAction = true;
		else
		{
			if (error) *error = L"无法识别 MouseGesture 片段: " + token;
			result = {};
			return false;
		}
		if (separator == std::wstring::npos) break;
		start = separator + 1;
	}
	if (!hasAction)
	{
		if (error) *error = L"MouseGesture 缺少鼠标动作。";
		result = {};
		return false;
	}
	return true;
}

std::wstring FormatMouseGesture(const MouseGesture& gesture)
{
	if (!gesture.IsValid()) return {};
	std::wstring result;
	if (HasModifier(gesture.Modifiers, ModifierKeys::Control)) result += L"Ctrl+";
	if (HasModifier(gesture.Modifiers, ModifierKeys::Alt)) result += L"Alt+";
	if (HasModifier(gesture.Modifiers, ModifierKeys::Shift)) result += L"Shift+";
	if (HasModifier(gesture.Modifiers, ModifierKeys::Windows)) result += L"Windows+";
	result += MouseActionName(gesture.Action);
	return result;
}

UIElement::~UIElement()
{
	InvalidateCommandInfrastructureForDestruction();
}

void UIElement::InvalidateCommandInfrastructureForDestruction() noexcept
{
	if (_commandBindings)
	{
		_commandBindings->Owner = nullptr;
		_commandBindings->Entries.clear();
	}
	if (_commandCanExecuteObservers)
	{
		_commandCanExecuteObservers->Owner = nullptr;
		_commandCanExecuteObservers->Domain = nullptr;
		_commandCanExecuteObservers->Entries.clear();
	}
}

EventConnection UIElement::AddCommandBinding(CommandBinding binding)
{
	auto* owner = dynamic_cast<Control*>(this);
	if (!owner || binding.Command.Empty()
		|| (!binding.PreviewCanExecute && !binding.CanExecute
			&& !binding.PreviewExecuted && !binding.Executed)) return {};
	if (!_commandBindings)
	{
		_commandBindings = std::make_shared<CommandBindingCollectionState>();
		_commandBindings->Owner = owner;
	}
	const auto token = _commandBindings->NextToken++;
	_commandBindings->Entries.push_back(
		CommandBindingCollectionState::Entry{ token, std::move(binding) });
	RoutedCommandManager::InvalidateRequerySuggested(*owner);
	std::weak_ptr<CommandBindingCollectionState> weak = _commandBindings;
	return EventConnection([weak, token]()
	{
		auto state = weak.lock();
		if (!state) return;
		state->Entries.erase(std::remove_if(
			state->Entries.begin(), state->Entries.end(),
			[token](const auto& entry) { return entry.Token == token; }),
			state->Entries.end());
		if (state->Owner)
			RoutedCommandManager::InvalidateRequerySuggested(*state->Owner);
	});
}

bool UIElement::AddInputBinding(KeyBinding binding)
{
	if (binding.Command.Empty() || !binding.Gesture.IsValid()) return false;
	_inputBindings.push_back(std::move(binding));
	return true;
}

bool UIElement::AddInputBinding(MouseBinding binding)
{
	if (binding.Command.Empty() || !binding.Gesture.IsValid()) return false;
	_inputBindings.push_back(std::move(binding));
	return true;
}

bool UIElement::SetInputBindings(std::vector<InputBinding> bindings)
{
	for (const auto& binding : bindings)
	{
		const bool valid = std::visit([](const auto& value)
		{
			return !value.Command.Empty() && value.Gesture.IsValid();
		}, binding);
		if (!valid) return false;
	}
	_inputBindings = std::move(bindings);
	return true;
}

RoutedCommandManager::RoutedCommandManager(Window& owner)
	: _requeryState(std::make_shared<RequeryState>(owner))
{
}

RoutedCommandManager::~RoutedCommandManager()
{
	if (!_requeryState) return;
	std::scoped_lock lock(_requeryState->Mutex);
	_requeryState->Owner = nullptr;
	_requeryState->Pending = false;
	_requeryState->Posted = false;
	_requeryState->Observers.clear();
}

namespace
{
	RoutedCommandCanExecuteResult QueryCanExecuteOnRoute(
		const RoutedCommand& command,
		Control& target,
		std::any parameter,
		std::span<const ControlWeakReference> route,
		std::uint64_t transactionId,
		std::uint64_t routeId)
	{
		RoutedCommandCanExecuteResult result;
		const ControlWeakReference targetLifetime(&target);
		result.TransactionId = transactionId;
		result.RouteId = routeId;
		result.RouteDepth = route.size();
		result.RequeryGeneration =
			RoutedCommandManager::GetRequeryGeneration(target);
		if (command.Empty() || route.empty())
		{
			auto* routeTarget = route.empty() ? nullptr : route.front().Get();
			result.Target = targetLifetime.Get() == routeTarget
				? routeTarget : nullptr;
			return result;
		}
		CanExecuteRoutedEventArgs args(command, std::move(parameter));
		args.CommandTransactionId = transactionId;
		args.CommandRouteId = routeId;
		(void)RaiseRoutedEventOnRoute(
			target, RoutedEventId::CanExecute, args, route);
		auto* liveTarget = targetLifetime.Get();
		auto* routeTarget = route.front().Get();
		const bool targetSurvived = liveTarget
			&& routeTarget == liveTarget;
		result.Target = targetSurvived ? liveTarget : nullptr;
		// A handler may synchronously destroy the command target.  The routed
		// args are only a transient answer; never publish a stale positive
		// CanExecute result for an expired route front.
		result.CanExecute = targetSurvived && args.CanExecute;
		result.ContinueRouting = args.ContinueRouting;
		result.Handled = args.Handled;
		result.CommandBindingMatched = args.CommandBindingMatched;
		return result;
	}

	bool ConsumesInput(const RoutedCommandExecutionResult& result) noexcept
	{
		if (result.Executed) return true;
		if (result.Query.ContinueRouting) return false;
		return result.Query.CanExecute || result.Query.Handled
			|| result.Query.CommandBindingMatched;
	}
}

RoutedCommandCanExecuteResult RoutedCommandManager::QueryCanExecute(
	const RoutedCommand& command,
	Control& target,
	std::any parameter)
{
	auto route = BuildRoutedEventRoute(
		&target, RoutedEventRoutingStrategy::Bubble);
	return QueryCanExecuteOnRoute(
		command, target, std::move(parameter), route,
		NextCommandTransactionId.fetch_add(1, std::memory_order_relaxed),
		NextCommandRouteId.fetch_add(1, std::memory_order_relaxed));
}

RoutedCommandCanExecuteResult RoutedCommandManager::QueryCommandSource(
	Control& source,
	const RoutedCommandSourceQuery& query)
{
	auto* target = ResolveCommandTarget(source, query.CommandTarget);
	return target ? QueryCanExecute(query.Command, *target, query.Parameter)
		: RoutedCommandCanExecuteResult{};
}

bool RoutedCommandManager::CanExecute(
	const RoutedCommand& command,
	Control& target,
	std::any parameter)
{
	return QueryCanExecute(command, target, std::move(parameter)).CanExecute;
}

RoutedCommandExecutionResult RoutedCommandManager::ExecuteCommand(
	const RoutedCommand& command,
	Control& target,
	std::any parameter)
{
	RoutedCommandExecutionResult result;
	const auto transactionId =
		NextCommandTransactionId.fetch_add(1, std::memory_order_relaxed);
	const auto routeId =
		NextCommandRouteId.fetch_add(1, std::memory_order_relaxed);
	auto route = BuildRoutedEventRoute(
		&target, RoutedEventRoutingStrategy::Bubble);
	result.TransactionId = transactionId;
	result.RouteId = routeId;
	result.Query = QueryCanExecuteOnRoute(
		command, target, parameter, route, transactionId, routeId);
	if (!result.Query.CanExecute || route.empty()
		|| route.front().Get() != &target) return result;

	ExecutedRoutedEventArgs args(command, std::move(parameter));
	args.CommandTransactionId = transactionId;
	args.CommandRouteId = routeId;
	(void)RaiseRoutedEventOnRoute(
		target, RoutedEventId::Executed, args, route);
	result.Query.Target = route.front().Get();
	result.Handled = args.Handled;
	result.Executed = args.Executed;
	return result;
}

RoutedCommandExecutionResult RoutedCommandManager::ExecuteCommandSource(
	Control& source,
	const RoutedCommandSourceQuery& query)
{
	auto* target = ResolveCommandTarget(source, query.CommandTarget);
	return target ? ExecuteCommand(query.Command, *target, query.Parameter)
		: RoutedCommandExecutionResult{};
}

bool RoutedCommandManager::Execute(
	const RoutedCommand& command,
	Control& target,
	std::any parameter)
{
	return ExecuteCommand(command, target, std::move(parameter)).Executed;
}

bool RoutedCommandManager::ProcessInput(
	Control& source,
	const KeyEventArgs& input)
{
	const auto route = BuildRoutedEventRoute(
		&source, RoutedEventRoutingStrategy::Bubble);
	const ControlWeakReference sourceLifetime(&source);
	for (const auto& currentReference : route)
	{
		if (!sourceLifetime) break;
		auto* current = currentReference.Get();
		if (!current) continue;
		const auto bindings = current->GetInputBindings();
		const std::vector<InputBinding> snapshot(bindings.begin(), bindings.end());
		for (const auto& binding : snapshot)
		{
			if (!sourceLifetime || !currentReference) break;
			const auto* keyBinding = std::get_if<KeyBinding>(&binding);
			const auto key = input.Key == Key::System
				? input.SystemKey : input.Key;
			if (!keyBinding
				|| !keyBinding->Gesture.Matches(key, input.Modifiers)) continue;
			const auto result = ExecuteCommandSource(source,
				RoutedCommandSourceQuery{ keyBinding->Command,
					keyBinding->CommandParameter, keyBinding->CommandTarget });
			if (ConsumesInput(result)) return true;
		}
	}
	return false;
}

bool RoutedCommandManager::ProcessInput(
	Control& source,
	const MouseEventArgs& input,
	ModifierKeys modifiers)
{
	const auto route = BuildRoutedEventRoute(
		&source, RoutedEventRoutingStrategy::Bubble);
	const ControlWeakReference sourceLifetime(&source);
	for (const auto& currentReference : route)
	{
		if (!sourceLifetime) break;
		auto* current = currentReference.Get();
		if (!current) continue;
		const auto bindings = current->GetInputBindings();
		const std::vector<InputBinding> snapshot(bindings.begin(), bindings.end());
		for (const auto& binding : snapshot)
		{
			if (!sourceLifetime || !currentReference) break;
			const auto* mouseBinding = std::get_if<MouseBinding>(&binding);
			if (!mouseBinding
				|| !mouseBinding->Gesture.Matches(input, modifiers)) continue;
			const auto result = ExecuteCommandSource(source,
				RoutedCommandSourceQuery{ mouseBinding->Command,
					mouseBinding->CommandParameter, mouseBinding->CommandTarget });
			if (ConsumesInput(result)) return true;
		}
	}
	return false;
}

EventConnection RoutedCommandManager::RegisterClassCommandBinding(
	ComponentTypeToken ownerType,
	CommandBinding binding)
{
	if (!ownerType || binding.Command.Empty()) return {};
	auto& registry = ClassCommandBindings();
	std::uint64_t token = 0;
	{
		std::scoped_lock lock(registry.Mutex);
		token = registry.NextToken++;
		registry.Entries.push_back(ClassCommandBindingEntry{
			token, ClassCommandBindingOwnerKind::Declarative,
			ownerType, UIClass::UI_Base, std::move(binding) });
	}
	return EventConnection([token]()
	{
		auto& current = ClassCommandBindings();
		std::scoped_lock lock(current.Mutex);
		current.Entries.erase(std::remove_if(
			current.Entries.begin(), current.Entries.end(),
			[token](const auto& entry) { return entry.Token == token; }),
			current.Entries.end());
	});
}

#if CUI_ENABLE_DYNAMIC_XAML
EventConnection RoutedCommandManager::RegisterClassCommandBinding(
	const RuntimeTypeId& ownerType,
	CommandBinding binding)
{
	return RegisterClassCommandBinding(MakeComponentTypeToken(
		ownerType.NamespaceUri, ownerType.LocalName), std::move(binding));
}
#endif

EventConnection RoutedCommandManager::RegisterClassCommandBinding(
	UIClass ownerClass,
	CommandBinding binding)
{
	if (binding.Command.Empty()) return {};
	auto& registry = ClassCommandBindings();
	std::uint64_t token = 0;
	{
		std::scoped_lock lock(registry.Mutex);
		token = registry.NextToken++;
		registry.Entries.push_back(ClassCommandBindingEntry{
			token, ClassCommandBindingOwnerKind::Native,
			{}, ownerClass, std::move(binding) });
	}
	return EventConnection([token]()
	{
		auto& current = ClassCommandBindings();
		std::scoped_lock lock(current.Mutex);
		current.Entries.erase(std::remove_if(
			current.Entries.begin(), current.Entries.end(),
			[token](const auto& entry) { return entry.Token == token; }),
			current.Entries.end());
	});
}

RoutedHandlerInvocationCount RoutedCommandManager::InvokeCommandBindings(
	Control& target,
	RoutedEventArgs& args)
{
	const ControlWeakReference targetLifetime(&target);
	const bool canExecuteEvent = args.EventId == RoutedEventId::PreviewCanExecute
		|| args.EventId == RoutedEventId::CanExecute;
	const bool executedEvent = args.EventId == RoutedEventId::PreviewExecuted
		|| args.EventId == RoutedEventId::Executed;
	if (!canExecuteEvent && !executedEvent) return {};

	std::vector<ClassCommandBindingEntry> exact;
	std::vector<ClassCommandBindingEntry> native;
	{
		auto& registry = ClassCommandBindings();
		std::scoped_lock lock(registry.Mutex);
		const auto componentType = target.GetDeclarativeTypeToken();
		for (const auto& entry : registry.Entries)
		{
			if (entry.OwnerKind == ClassCommandBindingOwnerKind::Declarative)
			{
				if (componentType
					&& entry.ComponentOwner == componentType)
					exact.push_back(entry);
			}
			else if (IsUIClassAssignableFrom(entry.NativeOwner, target.Type()))
				native.push_back(entry);
		}
	}
	std::stable_sort(native.begin(), native.end(),
		[&](const auto& left, const auto& right)
		{
			return GetUIClassInheritanceDistance(left.NativeOwner, target.Type())
				< GetUIClassInheritanceDistance(right.NativeOwner, target.Type());
		});

	std::vector<CommandBinding> instance;
	if (target._commandBindings)
	{
		instance.reserve(target._commandBindings->Entries.size());
		for (const auto& entry : target._commandBindings->Entries)
			instance.push_back(entry.Binding);
	}
	RoutedHandlerInvocationCount count;
	auto invoke = [&](const CommandBinding& binding)
	{
		if (!targetLifetime) return false;
		if (canExecuteEvent)
		{
			auto& commandArgs = static_cast<CanExecuteRoutedEventArgs&>(args);
			if (!CommandMatches(binding.Command, commandArgs.Command)) return true;
			const auto& handler = args.EventId == RoutedEventId::PreviewCanExecute
				? binding.PreviewCanExecute : binding.CanExecute;
			const bool hasDefaultExecution =
				args.EventId == RoutedEventId::PreviewCanExecute
					? static_cast<bool>(binding.PreviewExecuted)
					: static_cast<bool>(binding.Executed);
			if (!handler && !hasDefaultExecution) return true;
			if (args.Handled) { ++count.Skipped; return true; }
			commandArgs.CommandBindingMatched = true;
			if (handler) handler(&target, commandArgs);
			else commandArgs.CanExecute = true;
			++count.Invoked;
			if (commandArgs.CanExecute && !commandArgs.ContinueRouting)
				commandArgs.Handled = true;
			return static_cast<bool>(targetLifetime);
		}
		auto& commandArgs = static_cast<ExecutedRoutedEventArgs&>(args);
		if (!CommandMatches(binding.Command, commandArgs.Command)) return true;
		const auto& handler = args.EventId == RoutedEventId::PreviewExecuted
			? binding.PreviewExecuted : binding.Executed;
		if (!handler) return true;
		if (args.Handled) { ++count.Skipped; return true; }
		handler(&target, commandArgs);
		++count.Invoked;
		commandArgs.Executed = true;
		commandArgs.Handled = true;
		return static_cast<bool>(targetLifetime);
	};
	for (const auto& entry : exact)
		if (!invoke(entry.Binding)) return count;
	for (const auto& entry : native)
		if (!invoke(entry.Binding)) return count;
	for (const auto& binding : instance)
		if (!invoke(binding)) return count;
	return count;
}

Control* RoutedCommandManager::ResolveCommandTarget(
	Control& source,
	const ControlWeakReference& requested) noexcept
{
	auto resolveDomain = [](Control& control) noexcept -> Window*
	{
		for (auto* current = &control; current;
			current = current->GetRoutedParent())
		{
			if (auto* window = current->GetPresentationWindow())
				return window;
			if (auto* window = dynamic_cast<Window*>(current))
				return window;
		}
		return nullptr;
	};
	if (requested.HasValue())
	{
		auto* target = requested.Get();
		if (!target) return nullptr;
		// A target can be temporarily absent from the presentation tree while it
		// remains in the same logical command route (for example inactive
		// TabItem content referenced by a ContextMenu).  Compare routed Window
		// domains instead of the target's direct visual-mount cache so that the
		// explicit route remains usable without admitting cross-Window targets.
		auto* sourceDomain = resolveDomain(source);
		auto* targetDomain = resolveDomain(*target);
		if (sourceDomain != targetDomain && (sourceDomain || targetDomain))
			return nullptr;
		return target;
	}
	if (source.GetPresentationWindow())
		if (auto* focused = source.GetPresentationWindow()->GetKeyboardFocusedElement())
			return focused;
	return &source;
}

void RoutedCommandManager::NotifySourceScopeChanged(Control& source)
{
	if (!source._commandCanExecuteObservers)
	{
		// A container without its own command source may still own logical command
		// sources (for example ContextMenu items). Same-domain reparenting changes
		// their route, so invalidate the Window domain once after propagation.
		if (source.GetPresentationWindow())
			(void)InvalidateRequerySuggested(source);
		return;
	}
	auto observerState = source._commandCanExecuteObservers;
	auto* nextDomain = source.GetPresentationWindow();
	if (observerState->Domain == nextDomain)
	{
		if (nextDomain) (void)InvalidateRequerySuggested(source);
		return;
	}
	observerState->Domain = nextDomain;
	if (nextDomain)
	{
		auto& manager = cui::framework::WindowAccess::Commands(*nextDomain);
		auto domain = manager._requeryState;
		if (domain)
		{
			std::scoped_lock lock(domain->Mutex);
			const auto wanted = observerState.get();
			bool found = false;
			for (auto current = domain->Observers.begin();
				current != domain->Observers.end();)
			{
				if (auto state = current->lock())
				{
					found = found || state.get() == wanted;
					++current;
				}
				else current = domain->Observers.erase(current);
			}
			if (!found) domain->Observers.push_back(observerState);
		}
	}
	// Publish every real domain transition synchronously, including Window ->
	// nullptr. Detached sources must not retain the old Window predicate while
	// also remaining absent from every Window requery registry.
	const ControlWeakReference sourceLifetime(&source);
	const auto entries = observerState->Entries;
	for (const auto& entry : entries)
	{
		if (!sourceLifetime || !observerState->Owner) break;
		const auto active = std::find_if(
			observerState->Entries.begin(), observerState->Entries.end(),
			[&](const auto& value) { return value.Token == entry.Token; });
		if (active == observerState->Entries.end()) continue;
		auto result = QueryCommandSource(source, entry.Query);
		if (!sourceLifetime || !observerState->Owner) break;
		entry.Observer(source, result);
	}
}

EventConnection RoutedCommandManager::ObserveCanExecute(
	Control& source,
	RoutedCommandSourceQuery query,
	CanExecuteObserver observer)
{
	if (query.Command.Empty() || !observer) return {};
	if (!source._commandCanExecuteObservers)
	{
		source._commandCanExecuteObservers =
			std::make_shared<CommandCanExecuteObserverState>();
		source._commandCanExecuteObservers->Owner = &source;
	}
	auto state = source._commandCanExecuteObservers;
	const auto token = state->NextToken++;
	state->Entries.push_back(CommandCanExecuteObserverState::Entry{
		token, std::move(query), std::move(observer) });
	const auto addedEntry = state->Entries.back();
	const ControlWeakReference sourceLifetime(&source);
	const auto* previousDomain = state->Domain;
	try
	{
		NotifySourceScopeChanged(source);
		// Domain entry refreshes every observer, including this new one. Detached
		// and already-mounted additions still need their own initial publication.
		if (previousDomain == state->Domain
			&& sourceLifetime && state->Owner)
		{
			const auto active = std::find_if(
				state->Entries.begin(), state->Entries.end(),
				[token](const auto& entry) { return entry.Token == token; });
			if (active != state->Entries.end())
			{
				auto result = QueryCommandSource(source, addedEntry.Query);
				if (sourceLifetime && state->Owner)
					addedEntry.Observer(source, result);
			}
		}
	}
	catch (...)
	{
		state->Entries.erase(std::remove_if(
			state->Entries.begin(), state->Entries.end(),
			[token](const auto& entry) { return entry.Token == token; }),
			state->Entries.end());
		throw;
	}
	std::weak_ptr<CommandCanExecuteObserverState> weak = state;
	return EventConnection([weak, token]()
	{
		if (auto current = weak.lock())
			current->Entries.erase(std::remove_if(
				current->Entries.begin(), current->Entries.end(),
				[token](const auto& entry) { return entry.Token == token; }),
				current->Entries.end());
	});
}

EventConnection RoutedCommandManager::SubscribeRequerySuggested(
	Control& scope,
	RequeryHandler handler)
{
	if (!handler || !scope.GetPresentationWindow()) return {};
	auto state = cui::framework::WindowAccess::Commands(
		*scope.GetPresentationWindow())._requeryState;
	return state ? state->Suggested.Subscribe(std::move(handler))
		: EventConnection{};
}

bool RoutedCommandManager::InvalidateRequerySuggested(Control& scope)
{
	if (!scope.GetPresentationWindow()) return false;
	auto& manager = cui::framework::WindowAccess::Commands(
		*scope.GetPresentationWindow());
	auto state = manager._requeryState;
	if (!state) return false;
	{
		std::scoped_lock lock(state->Mutex);
		if (!state->Owner) return false;
		state->Pending = true;
		if (state->Posted) return true;
		state->Posted = true;
	}
	std::weak_ptr<RequeryState> weak = state;
	if (cui::PostToUIThread([weak]()
	{
		auto current = weak.lock();
		if (!current) return;
		Window* owner = nullptr;
		{
			std::scoped_lock lock(current->Mutex);
			owner = current->Owner;
		}
		if (owner) cui::framework::WindowAccess::Commands(
			*owner).ProcessPendingRequery();
	})) return true;
	{
		std::scoped_lock lock(state->Mutex);
		state->Posted = false;
	}
	if (!cui::IsUIThread()) return false;
	manager.ProcessPendingRequery();
	return true;
}

std::uint64_t RoutedCommandManager::GetRequeryGeneration(
	const Control& scope) noexcept
{
	if (!scope.GetPresentationWindow()) return 0;
	auto state = cui::framework::WindowAccess::Commands(
		*scope.GetPresentationWindow())._requeryState;
	if (!state) return 0;
	std::scoped_lock lock(state->Mutex);
	return state->Generation;
}

void RoutedCommandManager::RefreshCanExecuteObservers(
	Control& root,
	std::uint64_t generation)
{
	auto* window = root.GetPresentationWindow();
	if (!window) return;
	auto domain = cui::framework::WindowAccess::Commands(
		*window)._requeryState;
	if (!domain) return;
	std::vector<std::shared_ptr<CommandCanExecuteObserverState>> states;
	{
		std::scoped_lock lock(domain->Mutex);
		for (auto current = domain->Observers.begin();
			current != domain->Observers.end();)
		{
			if (auto state = current->lock())
			{
				states.push_back(std::move(state));
				++current;
			}
			else current = domain->Observers.erase(current);
		}
	}
	for (const auto& state : states)
	{
		auto* source = state->Owner;
		if (!source || source->GetPresentationWindow() != window) continue;
		const ControlWeakReference sourceLifetime(source);
		const auto entries = state->Entries;
		for (const auto& entry : entries)
		{
			if (!sourceLifetime || !state->Owner) break;
			const auto active = std::find_if(
				state->Entries.begin(), state->Entries.end(),
				[&](const auto& value) { return value.Token == entry.Token; });
			if (active == state->Entries.end()) continue;
			auto result = QueryCommandSource(*source, entry.Query);
			if (!sourceLifetime || !state->Owner) break;
			result.RequeryGeneration = generation;
			entry.Observer(*source, result);
		}
	}
}

void RoutedCommandManager::ProcessPendingRequery()
{
	auto state = _requeryState;
	if (!state) return;
	Window* owner = nullptr;
	std::uint64_t generation = 0;
	{
		std::scoped_lock lock(state->Mutex);
		state->Posted = false;
		if (!state->Owner || !state->Pending) return;
		state->Pending = false;
		owner = state->Owner;
		generation = ++state->Generation;
	}
	const ControlWeakReference ownerLifetime(owner);
	RefreshCanExecuteObservers(*owner, generation);
	if (!ownerLifetime) return;
	cui::framework::EventAccess::Raise(
		state->Suggested,
		RoutedCommandRequeryEventArgs{ owner, generation });
}
