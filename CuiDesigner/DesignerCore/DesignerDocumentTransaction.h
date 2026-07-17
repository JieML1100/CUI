#pragma once

#include <cstdint>
#include <string>
#include <utility>

enum class DesignerDocumentTransactionState : uint8_t
{
	Begun,
	Committed,
	Unchanged,
	RolledBack,
	Canceled,
	Aborted,
	Rejected,
	Failed
};

struct DesignerDocumentTransactionResult
{
	DesignerDocumentTransactionState State =
		DesignerDocumentTransactionState::Failed;
	std::wstring Error;
	bool DocumentRestored = true;

	bool Succeeded() const noexcept
	{
		switch (State)
		{
		case DesignerDocumentTransactionState::Begun:
		case DesignerDocumentTransactionState::Committed:
		case DesignerDocumentTransactionState::Unchanged:
		case DesignerDocumentTransactionState::RolledBack:
		case DesignerDocumentTransactionState::Canceled:
			return true;
		default:
			return false;
		}
	}

	bool HasChanges() const noexcept
	{
		return State == DesignerDocumentTransactionState::Committed;
	}

	explicit operator bool() const noexcept { return Succeeded(); }

	static DesignerDocumentTransactionResult Success(
		DesignerDocumentTransactionState state)
	{
		return { state, {}, true };
	}

	static DesignerDocumentTransactionResult Failure(
		DesignerDocumentTransactionState state,
		std::wstring error,
		bool documentRestored = true)
	{
		return { state, std::move(error), documentRestored };
	}
};
