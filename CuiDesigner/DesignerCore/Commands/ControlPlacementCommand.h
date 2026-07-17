#pragma once

#include "../CommandManager.h"
#include "../../DesignerTypes.h"
#include <chrono>
#include <string>
#include <vector>

class DesignerCanvas;

enum class DesignerPlacementParentKind : uint8_t
{
	Root,
	Control,
	TabPage,
	SplitFirst,
	SplitSecond
};

struct DesignerControlPlacementState
{
	std::wstring TargetName;
	UIClass TargetType = UIClass::UI_Base;
	DesignerPlacementParentKind ParentKind =
		DesignerPlacementParentKind::Root;
	std::wstring ParentName;
	UIClass ParentType = UIClass::UI_Base;
	int ParentPageIndex = -1;
	int ChildIndex = -1;
	POINT Location{ 0, 0 };
	SIZE Size{ 0, 0 };
	Thickness Margin{};
	cui::layout::Length Width = cui::layout::Length::Auto();
	cui::layout::Length Height = cui::layout::Length::Auto();
	HorizontalAlignment HAlign = HorizontalAlignment::Left;
	VerticalAlignment VAlign = VerticalAlignment::Top;
	uint8_t AnchorStyles = ::AnchorStyles::None;
	Dock DockPosition = Dock::Fill;
	int GridRow = 0;
	int GridColumn = 0;
	int GridRowSpan = 1;
	int GridColumnSpan = 1;

	bool EquivalentTo(const DesignerControlPlacementState& other) const noexcept;
	size_t GetEstimatedMemoryUsage() const noexcept;
};

struct DesignerControlPlacementSnapshot
{
	std::vector<DesignerControlPlacementState> Targets;

	bool EquivalentTo(const DesignerControlPlacementSnapshot& other) const noexcept;
	size_t GetEstimatedMemoryUsage() const noexcept;
};

/** Reversible placement/tree delta used by nudging and pointer gestures. */
class ControlPlacementCommand final : public IDesignerCommand
{
public:
	ControlPlacementCommand(
		DesignerCanvas* canvas,
		DesignerControlPlacementSnapshot before,
		DesignerControlPlacementSnapshot after,
		std::vector<std::wstring> beforeSelectionNames,
		std::vector<std::wstring> afterSelectionNames,
		std::wstring beforePrimarySelectionName,
		std::wstring afterPrimarySelectionName,
		std::wstring label,
		bool skipInitialExecute);

	static bool Capture(
		DesignerCanvas* canvas,
		const std::vector<std::shared_ptr<DesignerControl>>& controls,
		DesignerControlPlacementSnapshot& out,
		std::wstring* outError = nullptr);
	static bool Restore(
		DesignerCanvas* canvas,
		const DesignerControlPlacementSnapshot& snapshot,
		std::wstring* outError = nullptr,
		bool* outOriginalRestored = nullptr);

	DesignerDocumentTransactionResult Execute() override;
	DesignerDocumentTransactionResult Undo() override;
	std::wstring GetLabel() const override;
	bool TryMergeWith(IDesignerCommand& newer) noexcept override;
	size_t GetEstimatedMemoryUsage() const noexcept override;

private:
	static std::shared_ptr<DesignerControl> ResolveTarget(
		DesignerCanvas* canvas,
		const DesignerControlPlacementState& state);
	static bool CaptureTarget(
		DesignerCanvas* canvas,
		const std::shared_ptr<DesignerControl>& target,
		DesignerControlPlacementState& out,
		std::wstring* outError);
	static bool ResolveParent(
		DesignerCanvas* canvas,
		const DesignerControlPlacementState& state,
		Control*& runtimeParent,
		Control*& designerParent,
		std::wstring* outError);
	static bool ApplyStateUnchecked(
		DesignerCanvas* canvas,
		const std::shared_ptr<DesignerControl>& target,
		const DesignerControlPlacementState& state,
		std::wstring* outError);
	static bool ApplyState(
		DesignerCanvas* canvas,
		const std::shared_ptr<DesignerControl>& target,
		const DesignerControlPlacementState& state,
		std::wstring* outError);
	DesignerDocumentTransactionResult Apply(
		const DesignerControlPlacementSnapshot& expected,
		const DesignerControlPlacementSnapshot& desired,
		const std::vector<std::wstring>& selectionNames,
		const std::wstring& primarySelectionName) const;
	void RefreshEstimatedMemoryUsage() noexcept;

	DesignerCanvas* _canvas = nullptr;
	DesignerControlPlacementSnapshot _before;
	DesignerControlPlacementSnapshot _after;
	std::vector<std::wstring> _beforeSelectionNames;
	std::vector<std::wstring> _afterSelectionNames;
	std::wstring _beforePrimarySelectionName;
	std::wstring _afterPrimarySelectionName;
	std::wstring _label;
	bool _skipInitialExecute = false;
	std::chrono::steady_clock::time_point _committedAt;
	size_t _estimatedMemoryUsage = 0;
};
