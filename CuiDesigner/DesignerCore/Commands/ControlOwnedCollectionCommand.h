#pragma once

#include "../CommandManager.h"

#include <memory>
#include <string>
#include <vector>

class Button;
class DesignerCanvas;
class DesignerControl;
class TabPage;

struct DesignerTabPageCollectionEdit
{
	TabPage* ExistingPage = nullptr;
	std::wstring Title;
};

struct DesignerToolBarButtonCollectionEdit
{
	Button* ExistingButton = nullptr;
	std::wstring Text;
	int Width = 90;
};

/**
 * Ownership-preserving collection command for TabControl pages and ToolBar
 * buttons. Attached roots belong only to the runtime tree; roots absent from
 * the active state are held by the command through unique_ptr.
 */
class ControlOwnedCollectionCommand final : public IDesignerCommand
{
public:
	struct Impl;
	~ControlOwnedCollectionCommand() override;

	static std::unique_ptr<ControlOwnedCollectionCommand> CreateTabPages(
		DesignerCanvas* canvas,
		const std::shared_ptr<DesignerControl>& target,
		const std::vector<DesignerTabPageCollectionEdit>& pages,
		std::wstring label,
		std::wstring* outError = nullptr);

	static std::unique_ptr<ControlOwnedCollectionCommand> CreateToolBarButtons(
		DesignerCanvas* canvas,
		const std::shared_ptr<DesignerControl>& target,
		const std::vector<DesignerToolBarButtonCollectionEdit>& buttons,
		std::wstring label,
		std::wstring* outError = nullptr);

	DesignerDocumentTransactionResult Execute() override;
	DesignerDocumentTransactionResult Undo() override;
	std::wstring GetLabel() const override;
	bool TryMergeWith(IDesignerCommand&) noexcept override { return false; }
	size_t GetEstimatedMemoryUsage() const noexcept override;

private:
	explicit ControlOwnedCollectionCommand(std::unique_ptr<Impl> impl);

	DesignerDocumentTransactionResult Apply(bool useAfterState);
	std::unique_ptr<Impl> _impl;
};
