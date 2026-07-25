#define NOMINMAX
#include "TextCompositionManager.h"

#include "Control.h"
#include "FocusManager.h"
#include "InputManager.h"
#include "TextEditCore.h"
#include "Window.h"

#include <imm.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

#pragma comment(lib, "Imm32.lib")

namespace
{
	ModifierKeys CurrentModifierKeys() noexcept
	{
		auto modifiers = ModifierKeys::None;
		if ((::GetKeyState(VK_MENU) & 0x8000) != 0)
			modifiers |= ModifierKeys::Alt;
		if ((::GetKeyState(VK_CONTROL) & 0x8000) != 0)
			modifiers |= ModifierKeys::Control;
		if ((::GetKeyState(VK_SHIFT) & 0x8000) != 0)
			modifiers |= ModifierKeys::Shift;
		if ((::GetKeyState(VK_LWIN) & 0x8000) != 0
			|| (::GetKeyState(VK_RWIN) & 0x8000) != 0)
			modifiers |= ModifierKeys::Windows;
		return modifiers;
	}

	constexpr wchar_t ReplacementCharacter = L'\xFFFD';
	constexpr ULONGLONG ImeEchoLifetimeMs = 750;
	constexpr LONG MaxCompositionBytes = 4 * 1024 * 1024;

	std::wstring ReadCompositionString(HIMC context, DWORD index)
	{
		if (!context) return {};
		const LONG bytes = ImmGetCompositionStringW(context, index, nullptr, 0);
		if (bytes <= 0 || bytes > MaxCompositionBytes
			|| (bytes % static_cast<LONG>(sizeof(wchar_t))) != 0) return {};
		std::wstring value(static_cast<std::size_t>(bytes)
			/ sizeof(wchar_t), L'\0');
		const LONG copied = ImmGetCompositionStringW(
			context, index, value.data(), static_cast<DWORD>(bytes));
		if (copied != bytes) return {};
		return value;
	}

	std::vector<unsigned char> ReadCompositionAttributes(HIMC context)
	{
		if (!context) return {};
		const LONG bytes = ImmGetCompositionStringW(
			context, GCS_COMPATTR, nullptr, 0);
		if (bytes <= 0 || bytes > MaxCompositionBytes) return {};
		std::vector<unsigned char> value(static_cast<std::size_t>(bytes));
		const LONG copied = ImmGetCompositionStringW(
			context, GCS_COMPATTR, value.data(), static_cast<DWORD>(bytes));
		if (copied != bytes) return {};
		return value;
	}

	std::vector<std::uint32_t> ReadCompositionClauses(HIMC context)
	{
		if (!context) return {};
		const LONG bytes = ImmGetCompositionStringW(
			context, GCS_COMPCLAUSE, nullptr, 0);
		if (bytes <= 0 || bytes > MaxCompositionBytes
			|| (bytes % static_cast<LONG>(sizeof(DWORD))) != 0) return {};
		std::vector<DWORD> native(static_cast<std::size_t>(bytes)
			/ sizeof(DWORD));
		const LONG copied = ImmGetCompositionStringW(
			context, GCS_COMPCLAUSE, native.data(), static_cast<DWORD>(bytes));
		if (copied != bytes) return {};
		return std::vector<std::uint32_t>(native.begin(), native.end());
	}

	std::wstring ScalarToUtf16(std::uint32_t scalar)
	{
		if (scalar > 0x10FFFF || (scalar >= 0xD800 && scalar <= 0xDFFF))
			return {};
		if (scalar <= 0xFFFF)
			return std::wstring(1, static_cast<wchar_t>(scalar));
		scalar -= 0x10000;
		return std::wstring{
			static_cast<wchar_t>(0xD800 + (scalar >> 10)),
			static_cast<wchar_t>(0xDC00 + (scalar & 0x3FF)) };
	}
}

TextCompositionManager::TextCompositionManager(
	Window& owner,
	InputManager& inputManager) noexcept
	: _window(&owner), _inputManager(&inputManager)
{
}

Control* TextCompositionManager::ResolveSource(Control* requested) const noexcept
{
	if (!_window) return nullptr;
	auto isOwned = [this](Control* value) noexcept
	{
		return value && (value == _window || value->GetPresentationWindow() == _window);
	};
	if (requested) return isOwned(requested) ? requested : nullptr;
	if (auto* focused = _window->GetKeyboardFocusedElement(); isOwned(focused))
		return focused;
	return _window;
}

bool TextCompositionManager::PublishLifecycle(RoutedEventId bubbleEvent)
{
	if (!_inputManager || !_snapshot.Source) return false;
	TextCompositionEventArgs args(
		_snapshot.Stage,
		_snapshot.Text,
		_snapshot.CompositionText,
		_snapshot.InputKind,
		_snapshot.CompositionId,
		_snapshot.CaretIndex,
		_snapshot.CancelReason);
	args.SystemText = _snapshot.SystemText;
	args.ControlText = _snapshot.ControlText;
	args.Modifiers = CurrentModifierKeys();
	args.CompositionAttributes = _snapshot.Attributes;
	args.CompositionClauses = _snapshot.Clauses;
	InputManager::StagingScope staging(
		*_inputManager, _snapshot.Source, bubbleEvent);
	staging.Preview(args);
	staging.Complete(args);
	return staging.Handled();
}

bool TextCompositionManager::StartComposition(
	Control* source,
	TextCompositionInputKind inputKind)
{
	source = ResolveSource(source);
	if (!source) return false;
	if (_snapshot.IsComposing)
	{
		if (_snapshot.Source == source) return true;
		CancelComposition(TextCompositionCancelReason::FocusChanged);
	}
	_snapshot = {};
	_snapshot.IsComposing = true;
	_snapshot.Source = source;
	_snapshot.CompositionId = ++_nextCompositionId;
	_snapshot.Stage = TextCompositionStage::Started;
	_snapshot.InputKind = inputKind;
	_snapshot.CaretIndex = 0;
	++_statistics.CompositionsStarted;
	(void)UpdateCaretPosition();
	(void)PublishLifecycle(RoutedEventId::TextInputStart);
	return true;
}

bool TextCompositionManager::UpdateComposition(
	std::wstring compositionText,
	int caretIndex,
	std::vector<unsigned char> attributes,
	std::vector<std::uint32_t> clauses,
	std::wstring systemText,
	std::wstring controlText)
{
	if (!_snapshot.IsComposing
		&& !StartComposition(nullptr, TextCompositionInputKind::Ime)) return false;
	_snapshot.Stage = TextCompositionStage::Updated;
	_snapshot.CompositionText = std::move(compositionText);
	_snapshot.SystemText = std::move(systemText);
	_snapshot.ControlText = std::move(controlText);
	_snapshot.Attributes = std::move(attributes);
	_snapshot.Clauses = std::move(clauses);
	_snapshot.CaretIndex = (std::clamp)(
		caretIndex < 0 ? static_cast<int>(_snapshot.CompositionText.size())
			: caretIndex,
		0,
		static_cast<int>(_snapshot.CompositionText.size()));
	++_statistics.CompositionsUpdated;
	(void)UpdateCaretPosition();
	(void)PublishLifecycle(RoutedEventId::TextInputUpdate);
	return true;
}

bool TextCompositionManager::CommitCore(
	Control* source,
	std::wstring text,
	TextCompositionInputKind inputKind,
	std::uint64_t compositionId,
	const std::wstring& compositionText,
	const std::wstring& systemText,
	const std::wstring& controlText)
{
	if (!_inputManager || !source || text.empty()) return false;
	TextCompositionEventArgs args(
		TextCompositionStage::Completed,
		std::move(text),
		compositionText,
		inputKind,
		compositionId);
	args.SystemText = systemText;
	args.ControlText = controlText;
	args.Modifiers = CurrentModifierKeys();
	if (_snapshot.CompositionId == compositionId)
	{
		args.CompositionAttributes = _snapshot.Attributes;
		args.CompositionClauses = _snapshot.Clauses;
		args.CaretIndex = _snapshot.CaretIndex;
	}
	InputManager::StagingScope staging(
		*_inputManager, source, RoutedEventId::TextInput);
	staging.Preview(args);
	bool applied = false;
	if (!staging.Handled())
		applied = source->DispatchTextInput(args);
	else
		++_statistics.PreviewApplicationsSuppressed;
	args.TextApplied = applied;
	staging.Complete(args);
	++_statistics.TextCommits;
	if (applied) ++_statistics.TextApplications;
	return applied;
}

bool TextCompositionManager::CompleteComposition(
	std::wstring text,
	std::wstring systemText,
	std::wstring controlText)
{
	if (text.empty())
	{
		CancelComposition(TextCompositionCancelReason::NativeCanceled);
		return false;
	}
	Control* source = _snapshot.IsComposing
		? _snapshot.Source : ResolveSource(nullptr);
	const auto id = _snapshot.IsComposing
		? _snapshot.CompositionId : ++_nextCompositionId;
	const auto kind = _snapshot.IsComposing
		? _snapshot.InputKind : TextCompositionInputKind::Ime;
	const std::wstring compositionText = _snapshot.CompositionText;
	const bool wasComposing = _snapshot.IsComposing;
	if (!wasComposing) _snapshot = {};
	// The composition is already closed while TextInput is routed. Event
	// handlers may synchronously move focus or begin another composition; the
	// transaction id check below prevents this outer completion from replacing
	// that newer state.
	_snapshot.IsComposing = false;
	_snapshot.Source = source;
	_snapshot.CompositionId = id;
	_snapshot.Stage = TextCompositionStage::Completed;
	_snapshot.InputKind = kind;
	_snapshot.Text = text;
	_snapshot.SystemText = systemText;
	_snapshot.ControlText = controlText;
	_snapshot.CancelReason = TextCompositionCancelReason::None;
	_nativeCancelReason = TextCompositionCancelReason::None;
	const bool applied = CommitCore(
		source, text, kind, id, compositionText, systemText, controlText);
	if (_snapshot.CompositionId == id
		&& _snapshot.Stage == TextCompositionStage::Completed)
	{
		_snapshot.Source = nullptr;
		_snapshot.Text = std::move(text);
		_snapshot.CompositionText.clear();
		_snapshot.SystemText = std::move(systemText);
		_snapshot.ControlText = std::move(controlText);
		_snapshot.CaretIndex = -1;
		_snapshot.Attributes.clear();
		_snapshot.Clauses.clear();
	}
	if (wasComposing) ++_statistics.CompositionsCompleted;
	_pendingHighSurrogate = L'\0';
	return applied;
}

bool TextCompositionManager::CommitText(
	std::wstring text,
	Control* source,
	TextCompositionInputKind inputKind)
{
	if (text.empty()) return false;
	if (_snapshot.IsComposing)
		return CompleteComposition(std::move(text));
	source = ResolveSource(source);
	const auto id = ++_nextCompositionId;
	_snapshot = {};
	_snapshot.Source = source;
	_snapshot.CompositionId = id;
	_snapshot.Stage = TextCompositionStage::Completed;
	_snapshot.InputKind = inputKind;
	_snapshot.Text = text;
	const bool applied = CommitCore(
		source, text, inputKind, id, {}, {}, {});
	if (_snapshot.CompositionId == id
		&& _snapshot.Stage == TextCompositionStage::Completed)
	{
		_snapshot.Source = nullptr;
		_snapshot.Text = std::move(text);
	}
	return applied;
}

void TextCompositionManager::TombstoneNativeImeSession(
	TextCompositionCancelReason reason) noexcept
{
	if (reason != TextCompositionCancelReason::NativeCanceled
		&& _nativeImeSessionDisposition ==
			NativeImeSessionDisposition::Active
		&& _snapshot.IsComposing
		&& _snapshot.InputKind == TextCompositionInputKind::Ime)
	{
		_nativeImeSessionDisposition =
			NativeImeSessionDisposition::Tombstoned;
		++_statistics.NativeImeSessionsTombstoned;
	}
}

void TextCompositionManager::CancelComposition(
	TextCompositionCancelReason reason)
{
	_nativeCancelReason = TextCompositionCancelReason::None;
	if (!_snapshot.IsComposing)
	{
		_pendingHighSurrogate = L'\0';
		return;
	}
	TombstoneNativeImeSession(reason);
	if (reason == TextCompositionCancelReason::NativeCanceled
		&& _nativeImeSessionDisposition ==
			NativeImeSessionDisposition::Active
		&& _snapshot.InputKind == TextCompositionInputKind::Ime)
	{
		_nativeImeSessionDisposition = NativeImeSessionDisposition::Idle;
	}
	_snapshot.IsComposing = false;
	_snapshot.Source = nullptr;
	_snapshot.Stage = TextCompositionStage::Canceled;
	_snapshot.CancelReason = reason;
	_snapshot.Text.clear();
	_snapshot.CompositionText.clear();
	_snapshot.SystemText.clear();
	_snapshot.ControlText.clear();
	_snapshot.Attributes.clear();
	_snapshot.Clauses.clear();
	_snapshot.CaretIndex = -1;
	++_statistics.CompositionsCanceled;
	_pendingHighSurrogate = L'\0';
}

bool TextCompositionManager::ConsumeImeEcho(wchar_t value) noexcept
{
	if (_imeEchoUnits.empty()) return false;
	if (GetTickCount64() > _imeEchoDeadline)
	{
		_imeEchoUnits.clear();
		return false;
	}
	if (_imeEchoUnits.front() != value)
	{
		_imeEchoUnits.clear();
		return false;
	}
	_imeEchoUnits.pop_front();
	++_statistics.ImeEchoesSuppressed;
	return true;
}

void TextCompositionManager::QueueImeEcho(const std::wstring& value)
{
	_imeEchoUnits.assign(value.begin(), value.end());
	_imeEchoDeadline = GetTickCount64() + ImeEchoLifetimeMs;
}

bool TextCompositionManager::CommitUtf16Unit(
	wchar_t value,
	TextCompositionInputKind inputKind)
{
	bool applied = false;
	if (CuiTextEdit::IsHighSurrogate(value))
	{
		if (_pendingHighSurrogate != L'\0')
		{
			++_statistics.InvalidUnicodeReports;
			applied = CommitText(
				std::wstring(1, ReplacementCharacter), nullptr, inputKind);
		}
		_pendingHighSurrogate = value;
		return applied;
	}
	if (CuiTextEdit::IsLowSurrogate(value))
	{
		if (_pendingHighSurrogate == L'\0')
		{
			++_statistics.InvalidUnicodeReports;
			return CommitText(std::wstring(1, ReplacementCharacter), nullptr, inputKind);
		}
		std::wstring pair{ _pendingHighSurrogate, value };
		_pendingHighSurrogate = L'\0';
		return CommitText(std::move(pair), nullptr, inputKind);
	}
	if (_pendingHighSurrogate != L'\0')
	{
		_pendingHighSurrogate = L'\0';
		++_statistics.InvalidUnicodeReports;
		applied = CommitText(
			std::wstring(1, ReplacementCharacter), nullptr, inputKind);
	}
	if (value < L' ' || value == 0x7F)
	{
		++_statistics.ControlCharactersIgnored;
		return applied;
	}
	const bool currentApplied = CommitText(
		std::wstring(1, value), nullptr, inputKind);
	return applied || currentApplied;
}

bool TextCompositionManager::ProcessNativeCompositionPayload(
	bool hasResult,
	std::wstring committed,
	bool hasComposition,
	std::wstring composition,
	int caretIndex,
	std::vector<unsigned char> attributes,
	std::vector<std::uint32_t> clauses,
	bool& suppressed)
{
	suppressed = false;
	if (_nativeImeSessionDisposition ==
		NativeImeSessionDisposition::Tombstoned)
	{
		suppressed = true;
		if (hasResult)
		{
			++_statistics.StaleImeResultsDiscarded;
			if (!committed.empty()) QueueImeEcho(committed);
		}
		if (hasComposition)
			++_statistics.StaleImeCompositionUpdatesDiscarded;
		return false;
	}

	if (_nativeImeSessionDisposition == NativeImeSessionDisposition::Idle
		&& (hasResult || hasComposition))
	{
		_nativeImeSessionDisposition = NativeImeSessionDisposition::Active;
	}

	bool applied = false;
	if (hasResult && !committed.empty())
	{
		QueueImeEcho(committed);
		applied = CompleteComposition(std::move(committed));
	}
	if (hasComposition)
	{
		if (!_snapshot.IsComposing)
			(void)StartComposition(nullptr, TextCompositionInputKind::Ime);
		(void)UpdateComposition(
			std::move(composition), caretIndex,
			std::move(attributes), std::move(clauses));
	}
	return applied;
}

NativeTextMessageResult TextCompositionManager::ProcessWindowMessage(
	UINT message,
	WPARAM wParam,
	LPARAM lParam)
{
	NativeTextMessageResult result;
	auto recognize = [&]() -> NativeTextMessageResult&
	{
		result.Recognized = true;
		++_statistics.NativeReports;
		return result;
	};
	switch (message)
	{
	case WM_CHAR:
	{
		recognize().CallDefaultWindowProcedure = false;
		const wchar_t value = static_cast<wchar_t>(wParam);
		if (_suppressedCharacter != L'\0')
		{
			const bool matches = _suppressedCharacter == value;
			_suppressedCharacter = L'\0';
			if (matches) return result;
		}
		if (ConsumeImeEcho(value)) return result;
		result.TextApplied = CommitUtf16Unit(
			value, TextCompositionInputKind::Keyboard);
		return result;
	}
	case WM_UNICHAR:
	{
		recognize().CallDefaultWindowProcedure = false;
		if (wParam == UNICODE_NOCHAR)
		{
			result.Result = TRUE;
			return result;
		}
		const auto text = ScalarToUtf16(static_cast<std::uint32_t>(wParam));
		if (text.empty())
		{
			++_statistics.InvalidUnicodeReports;
			return result;
		}
		if (_pendingHighSurrogate != L'\0')
			++_statistics.InvalidUnicodeReports;
		_pendingHighSurrogate = L'\0';
		result.TextApplied = CommitText(
			text, nullptr, TextCompositionInputKind::Unicode);
		return result;
	}
	case WM_IME_CHAR:
	{
		recognize().CallDefaultWindowProcedure = false;
		const wchar_t value = static_cast<wchar_t>(wParam);
		if (ConsumeImeEcho(value)) return result;
		result.TextApplied = CommitUtf16Unit(
			value, TextCompositionInputKind::Ime);
		return result;
	}
	case WM_IME_STARTCOMPOSITION:
		recognize();
		_nativeImeSessionDisposition = NativeImeSessionDisposition::Active;
		_nativeCancelReason = TextCompositionCancelReason::None;
		(void)StartComposition(nullptr, TextCompositionInputKind::Ime);
		(void)UpdateCaretPosition();
		return result;
	case WM_IME_COMPOSITION:
	{
		recognize();
		HIMC context = _window && _window->Handle
			? ImmGetContext(_window->Handle) : nullptr;
		const bool hasResult = (lParam & GCS_RESULTSTR) != 0;
		const bool hasComposition = (lParam & GCS_COMPSTR) != 0;
		std::wstring committed;
		if (hasResult) committed = ReadCompositionString(context, GCS_RESULTSTR);
		std::wstring composition;
		std::vector<unsigned char> attributes;
		std::vector<std::uint32_t> clauses;
		int caret = -1;
		if (hasComposition)
		{
			composition = ReadCompositionString(context, GCS_COMPSTR);
			attributes = ReadCompositionAttributes(context);
			clauses = ReadCompositionClauses(context);
			if (context)
			{
				const LONG cursor = ImmGetCompositionStringW(
					context, GCS_CURSORPOS, nullptr, 0);
				if (cursor >= 0 && cursor <= (std::numeric_limits<int>::max)())
					caret = static_cast<int>(cursor);
			}
		}
		if (context) ImmReleaseContext(_window->Handle, context);
		bool suppressed = false;
		result.TextApplied = ProcessNativeCompositionPayload(
			hasResult, std::move(committed),
			hasComposition, std::move(composition), caret,
			std::move(attributes), std::move(clauses), suppressed);
		if (suppressed)
		{
			// The stale payload is fully owned here. Do not let DefWindowProc
			// turn it into another character path; the echo queue still covers
			// character messages that were already posted by the IME.
			result.CallDefaultWindowProcedure = false;
			return result;
		}
		if (!hasComposition && !hasResult && lParam == 0)
		{
			const auto reason = _nativeCancelReason !=
				TextCompositionCancelReason::None
				? _nativeCancelReason
				: TextCompositionCancelReason::NativeCanceled;
			CancelComposition(reason);
		}
		(void)UpdateCaretPosition();
		return result;
	}
	case WM_IME_ENDCOMPOSITION:
		recognize();
		if (_nativeImeSessionDisposition ==
			NativeImeSessionDisposition::Tombstoned)
		{
			_nativeCancelReason = TextCompositionCancelReason::None;
			++_statistics.StaleImeEndsDiscarded;
			return result;
		}
		if (_snapshot.IsComposing
			&& _snapshot.InputKind == TextCompositionInputKind::Ime)
		{
			const auto reason = _nativeCancelReason !=
				TextCompositionCancelReason::None
				? _nativeCancelReason
				: TextCompositionCancelReason::NativeCanceled;
			CancelComposition(reason);
		}
		else _nativeCancelReason = TextCompositionCancelReason::None;
		if (_nativeImeSessionDisposition !=
			NativeImeSessionDisposition::Tombstoned)
		{
			_nativeImeSessionDisposition = NativeImeSessionDisposition::Idle;
		}
		return result;
	case WM_IME_NOTIFY:
		recognize();
		if (wParam == IMN_OPENCANDIDATE
			|| wParam == IMN_CHANGECANDIDATE)
		{
			(void)UpdateCaretPosition();
		}
		else
		{
			// Placement APIs synchronously report IMN_SETCOMPOSITIONWINDOW and
			// IMN_SETCANDIDATEPOS on some IMEs. Those notifications describe
			// the placement we just applied; feeding them back would recurse.
			++_statistics.CaretNotificationsIgnored;
		}
		return result;
	case WM_INPUTLANGCHANGE:
		recognize();
		if (_snapshot.IsComposing
			&& _snapshot.InputKind == TextCompositionInputKind::Ime)
		{
			CancelComposition(TextCompositionCancelReason::NativeCanceled);
		}
		else _nativeCancelReason = TextCompositionCancelReason::None;
		_nativeImeSessionDisposition = NativeImeSessionDisposition::Idle;
		ClearTransientUnicodeState();
		return result;
	default:
		return result;
	}
}

bool TextCompositionManager::RequestNativeResolution(
	bool complete,
	TextCompositionCancelReason cancelReason)
{
	if (!complete) TombstoneNativeImeSession(cancelReason);
	_nativeCancelReason = cancelReason;
	if (!_window || !_window->Handle) return false;
	HIMC context = ImmGetContext(_window->Handle);
	if (!context) return false;
	const BOOL resolved = ImmNotifyIME(
		context,
		NI_COMPOSITIONSTR,
		complete ? CPS_COMPLETE : CPS_CANCEL,
		0);
	ImmReleaseContext(_window->Handle, context);
	return resolved != FALSE;
}

bool TextCompositionManager::ApplyNativeCaretPlacement(
	const D2D1_RECT_F& logicalRect)
{
	if (!_window || !_window->Handle || !::IsWindow(_window->Handle))
		return false;
	if (!std::isfinite(logicalRect.left)
		|| !std::isfinite(logicalRect.top)
		|| !std::isfinite(logicalRect.right)
		|| !std::isfinite(logicalRect.bottom)) return false;

	HIMC context = ImmGetContext(_window->Handle);
	if (!context) return false;

	float dpiScale = _window->GetDpiScale();
	if (!std::isfinite(dpiScale) || dpiScale <= 0.0f) dpiScale = 1.0f;
	const float headLogical = static_cast<float>(
		_window->GetTitleBarHeightPixels()) / dpiScale;
	const float normalizedLeft = (std::min)(logicalRect.left, logicalRect.right);
	const float normalizedTop = (std::min)(logicalRect.top, logicalRect.bottom);
	const float normalizedRight = (std::max)(logicalRect.left, logicalRect.right);
	const float normalizedBottom = (std::max)(logicalRect.top, logicalRect.bottom);

	const LONG left = static_cast<LONG>(std::lround(normalizedLeft * dpiScale));
	const LONG top = static_cast<LONG>(std::lround(
		(normalizedTop + headLogical) * dpiScale));
	const LONG right = static_cast<LONG>(std::lround(normalizedRight * dpiScale));
	const LONG bottom = static_cast<LONG>(std::lround(
		(normalizedBottom + headLogical) * dpiScale));
	const POINT anchor{ left, bottom };

	COMPOSITIONFORM composition{};
	composition.dwStyle = CFS_POINT;
	composition.ptCurrentPos = anchor;
	bool applied = ImmSetCompositionWindow(context, &composition) != FALSE;

	CANDIDATEFORM candidate{};
	candidate.dwStyle = CFS_EXCLUDE;
	candidate.ptCurrentPos = anchor;
	candidate.rcArea = RECT{
		left,
		top,
		(std::max)(left + 1, right),
		(std::max)(top + 1, bottom)
	};
	for (DWORD index = 0; index < 4; ++index)
	{
		candidate.dwIndex = index;
		applied = ImmSetCandidateWindow(context, &candidate) != FALSE
			|| applied;
	}
	ImmReleaseContext(_window->Handle, context);
	return applied;
}

void TextCompositionManager::BeforeKeyboardFocusCommit(
	Control* oldFocus,
	Control* newFocus,
	FocusChangeReason reason)
{
	if (oldFocus == newFocus || _resolvingFocus) return;
	if (_pendingHighSurrogate != L'\0')
	{
		_pendingHighSurrogate = L'\0';
		++_statistics.InvalidUnicodeReports;
	}
	if (!_snapshot.IsComposing || _snapshot.Source != oldFocus) return;
	_resolvingFocus = true;
	struct ResolvingFocusReset final
	{
		bool& Value;
		~ResolvingFocusReset() { Value = false; }
	} resolvingFocusReset{ _resolvingFocus };
	const bool forceCancel = reason == FocusChangeReason::TreeDetach
		|| reason == FocusChangeReason::EligibilityChanged
		|| (reason == FocusChangeReason::WindowActivation && newFocus == nullptr);
	if (forceCancel)
	{
		++_statistics.FocusCancellations;
		const auto cancelReason = reason == FocusChangeReason::TreeDetach
			? TextCompositionCancelReason::SourceDetached
			: (reason == FocusChangeReason::WindowActivation
				? TextCompositionCancelReason::WindowDeactivated
				: TextCompositionCancelReason::FocusChanged);
		(void)RequestNativeResolution(false, cancelReason);
		CancelComposition(cancelReason);
	}
	else
	{
		++_statistics.FocusCompletionRequests;
		(void)RequestNativeResolution(
			true, TextCompositionCancelReason::FocusChanged);
		if (_snapshot.IsComposing)
		{
			(void)RequestNativeResolution(
				false, TextCompositionCancelReason::FocusChanged);
			CancelComposition(TextCompositionCancelReason::FocusChanged);
		}
	}
}

bool TextCompositionManager::IsDescendantOrSelf(
	Control* element,
	Control* ancestor) noexcept
{
	if (!element || !ancestor) return false;
	for (auto* current = element; current; current = current->GetRoutedParent())
		if (current == ancestor) return true;
	return false;
}

void TextCompositionManager::DetachVisualChild(Control* root)
{
	if (_snapshot.IsComposing
		&& IsDescendantOrSelf(_snapshot.Source, root))
	{
		(void)RequestNativeResolution(
			false, TextCompositionCancelReason::SourceDetached);
		CancelComposition(TextCompositionCancelReason::SourceDetached);
	}
}

void TextCompositionManager::ClearTransientUnicodeState() noexcept
{
	_pendingHighSurrogate = L'\0';
	_suppressedCharacter = L'\0';
	_imeEchoUnits.clear();
	_imeEchoDeadline = 0;
}

void TextCompositionManager::Reset() noexcept
{
	if (_nativeImeSessionDisposition == NativeImeSessionDisposition::Active
		&& (!_snapshot.IsComposing
			|| _snapshot.InputKind != TextCompositionInputKind::Ime))
	{
		// A native result can close the managed transaction before the native
		// END arrives. Reset still invalidates that native session generation.
		_nativeImeSessionDisposition =
			NativeImeSessionDisposition::Tombstoned;
		++_statistics.NativeImeSessionsTombstoned;
	}
	try
	{
		if (_snapshot.IsComposing)
			(void)RequestNativeResolution(
				false, TextCompositionCancelReason::Explicit);
	}
	catch (...)
	{
	}
	_snapshot = {};
	_nativeCancelReason = TextCompositionCancelReason::None;
	if (_nativeImeSessionDisposition !=
		NativeImeSessionDisposition::Tombstoned)
	{
		_nativeImeSessionDisposition = NativeImeSessionDisposition::Idle;
	}
	ClearTransientUnicodeState();
}

bool TextCompositionManager::UpdateCaretPosition()
{
	if (_updatingCaretPosition)
	{
		++_statistics.CaretPlacementReentriesSuppressed;
		return false;
	}
	_updatingCaretPosition = true;
	struct CaretUpdateReset final
	{
		bool& Value;
		~CaretUpdateReset() { Value = false; }
	} caretUpdateReset{ _updatingCaretPosition };

	Control* source = _snapshot.IsComposing
		? _snapshot.Source : ResolveSource(nullptr);
	if (!source) return false;
	++_statistics.CaretPlacementRequests;
	D2D1_RECT_F rect{};
	_snapshot.HasCaretRect = false;
	if (!source->ResolveTextInputCaretRect(rect)) return false;
	if (!std::isfinite(rect.left) || !std::isfinite(rect.top)
		|| !std::isfinite(rect.right) || !std::isfinite(rect.bottom))
		return false;
	rect = D2D1_RECT_F{
		(std::min)(rect.left, rect.right),
		(std::min)(rect.top, rect.bottom),
		(std::max)(rect.left, rect.right),
		(std::max)(rect.top, rect.bottom) };
	_snapshot.CaretRect = rect;
	_snapshot.HasCaretRect = true;
	if (ApplyNativeCaretPlacement(rect))
	{
		++_statistics.CaretPlacementsApplied;
		return true;
	}
	return false;
}

void TextCompositionManager::SuppressNextCharacter(wchar_t value) noexcept
{
	_suppressedCharacter = value;
}
