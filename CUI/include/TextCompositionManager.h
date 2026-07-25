#pragma once

#include "Event.h"

#include <d2d1.h>

#include <cstdint>
#include <deque>
#include <string>
#include <vector>

class Control;
class InputManager;
class Window;
struct TextCompositionManagerTestAccess;
enum class FocusChangeReason : unsigned char;

/** Result returned to Window for one native text-services message. */
struct NativeTextMessageResult final
{
	bool Recognized = false;
	bool CallDefaultWindowProcedure = true;
	LRESULT Result = 0;
	bool TextApplied = false;
};

/** Immutable diagnostics snapshot for the current/most recent transaction. */
struct TextCompositionSnapshot final
{
	bool IsComposing = false;
	Control* Source = nullptr;
	std::uint64_t CompositionId = 0;
	TextCompositionStage Stage = TextCompositionStage::None;
	TextCompositionInputKind InputKind = TextCompositionInputKind::Keyboard;
	TextCompositionCancelReason CancelReason =
		TextCompositionCancelReason::None;
	std::wstring Text;
	std::wstring CompositionText;
	std::wstring SystemText;
	std::wstring ControlText;
	std::vector<unsigned char> Attributes;
	std::vector<std::uint32_t> Clauses;
	int CaretIndex = -1;
	bool HasCaretRect = false;
	D2D1_RECT_F CaretRect{};
};

/** Stable counters for the per-Window composition pipeline. */
struct TextCompositionStatistics final
{
	std::uint64_t NativeReports = 0;
	std::uint64_t CompositionsStarted = 0;
	std::uint64_t CompositionsUpdated = 0;
	std::uint64_t CompositionsCompleted = 0;
	std::uint64_t CompositionsCanceled = 0;
	std::uint64_t TextCommits = 0;
	std::uint64_t TextApplications = 0;
	std::uint64_t PreviewApplicationsSuppressed = 0;
	std::uint64_t ImeEchoesSuppressed = 0;
	std::uint64_t NativeImeSessionsTombstoned = 0;
	std::uint64_t StaleImeResultsDiscarded = 0;
	std::uint64_t StaleImeCompositionUpdatesDiscarded = 0;
	std::uint64_t StaleImeEndsDiscarded = 0;
	std::uint64_t ControlCharactersIgnored = 0;
	std::uint64_t InvalidUnicodeReports = 0;
	std::uint64_t FocusCompletionRequests = 0;
	std::uint64_t FocusCancellations = 0;
	std::uint64_t CaretPlacementRequests = 0;
	std::uint64_t CaretPlacementsApplied = 0;
	std::uint64_t CaretPlacementReentriesSuppressed = 0;
	std::uint64_t CaretNotificationsIgnored = 0;
};

/**
 * Per-Window WPF-style text composition coordinator. It is the only CUI
 * layer allowed to interpret WM_CHAR/WM_UNICHAR/WM_IME_* or query IMM32.
 */
class TextCompositionManager final
{
public:
	TextCompositionManager(Window& owner, InputManager& inputManager) noexcept;

	NativeTextMessageResult ProcessWindowMessage(
		UINT message, WPARAM wParam, LPARAM lParam);

	bool StartComposition(
		Control* source = nullptr,
		TextCompositionInputKind inputKind = TextCompositionInputKind::Ime);
	bool UpdateComposition(
		std::wstring compositionText,
		int caretIndex = -1,
		std::vector<unsigned char> attributes = {},
		std::vector<std::uint32_t> clauses = {},
		std::wstring systemText = {},
		std::wstring controlText = {});
	bool CompleteComposition(
		std::wstring text,
		std::wstring systemText = {},
		std::wstring controlText = {});
	bool CommitText(
		std::wstring text,
		Control* source = nullptr,
		TextCompositionInputKind inputKind =
			TextCompositionInputKind::Programmatic);
	void CancelComposition(
		TextCompositionCancelReason reason =
			TextCompositionCancelReason::Explicit);

	void BeforeKeyboardFocusCommit(
		Control* oldFocus,
		Control* newFocus,
		FocusChangeReason reason);
	void DetachVisualChild(Control* root);
	void Reset() noexcept;
	bool UpdateCaretPosition();
	void SuppressNextCharacter(wchar_t value) noexcept;

	TextCompositionSnapshot Snapshot() const { return _snapshot; }
	TextCompositionStatistics Statistics() const noexcept
	{
		return _statistics;
	}

private:
	enum class NativeImeSessionDisposition : unsigned char
	{
		Idle,
		Active,
		Tombstoned,
	};

	Window* _window = nullptr;
	InputManager* _inputManager = nullptr;
	TextCompositionSnapshot _snapshot;
	TextCompositionStatistics _statistics;
	std::uint64_t _nextCompositionId = 0;
	wchar_t _pendingHighSurrogate = L'\0';
	wchar_t _suppressedCharacter = L'\0';
	std::deque<wchar_t> _imeEchoUnits;
	ULONGLONG _imeEchoDeadline = 0;
	TextCompositionCancelReason _nativeCancelReason =
		TextCompositionCancelReason::None;
	NativeImeSessionDisposition _nativeImeSessionDisposition =
		NativeImeSessionDisposition::Idle;
	bool _resolvingFocus = false;
	bool _updatingCaretPosition = false;

	Control* ResolveSource(Control* requested) const noexcept;
	bool PublishLifecycle(RoutedEventId bubbleEvent);
	bool CommitCore(
		Control* source,
		std::wstring text,
		TextCompositionInputKind inputKind,
		std::uint64_t compositionId,
		const std::wstring& compositionText,
		const std::wstring& systemText,
		const std::wstring& controlText);
	bool CommitUtf16Unit(wchar_t value, TextCompositionInputKind inputKind);
	bool ProcessNativeCompositionPayload(
		bool hasResult,
		std::wstring committed,
		bool hasComposition,
		std::wstring composition,
		int caretIndex,
		std::vector<unsigned char> attributes,
		std::vector<std::uint32_t> clauses,
		bool& suppressed);
	void TombstoneNativeImeSession(
		TextCompositionCancelReason reason) noexcept;
	bool ConsumeImeEcho(wchar_t value) noexcept;
	void QueueImeEcho(const std::wstring& value);
	void ClearTransientUnicodeState() noexcept;
	bool RequestNativeResolution(
		bool complete,
		TextCompositionCancelReason cancelReason =
			TextCompositionCancelReason::NativeCanceled);
	bool ApplyNativeCaretPlacement(const D2D1_RECT_F& logicalRect);
	static bool IsDescendantOrSelf(
		Control* element, Control* ancestor) noexcept;

	friend struct TextCompositionManagerTestAccess;
};
