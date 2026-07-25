#pragma once

#include "../CommandManager.h"
#include "../../DesignerTypes.h"
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

class DesignerCanvas;

enum class DesignerPlacementParentKind : uint8_t
{
	Root,
	Control,
	ItemsControl
};

/**
 * Layout values participate in WPF dependency-property precedence. A placement
 * undo must restore both the effective value and whether an authored Local
 * contribution existed.
 */
enum class DesignerPlacementLocalValue : uint32_t
{
	Margin = 1u << 0,
	Width = 1u << 1,
	Height = 1u << 2,
	CanvasLeft = 1u << 3,
	CanvasTop = 1u << 4,
	CanvasRight = 1u << 5,
	CanvasBottom = 1u << 6,
	HorizontalAlignment = 1u << 7,
	VerticalAlignment = 1u << 8,
	Dock = 1u << 9,
	GridRow = 1u << 10,
	GridColumn = 1u << 11,
	GridRowSpan = 1u << 12,
	GridColumnSpan = 1u << 13,
	ZIndex = 1u << 14
};

struct DesignerControlPlacementState
{
	std::wstring TargetName;
	UIClass TargetType = UIClass::UI_Base;
	DesignerPlacementParentKind ParentKind =
		DesignerPlacementParentKind::Root;
	std::wstring ParentName;
	UIClass ParentType = UIClass::UI_Base;
	std::wstring ComponentContentProperty;
	int ChildIndex = -1;
	Thickness Margin{};
	cui::layout::Length Width = cui::layout::Length::Auto();
	cui::layout::Length Height = cui::layout::Length::Auto();
	float CanvasLeft = cui::layout::UnsetCanvasOffset;
	float CanvasTop = cui::layout::UnsetCanvasOffset;
	float CanvasRight = cui::layout::UnsetCanvasOffset;
	float CanvasBottom = cui::layout::UnsetCanvasOffset;
	::HorizontalAlignment Horizontal = ::HorizontalAlignment::Left;
	::VerticalAlignment Vertical = ::VerticalAlignment::Top;
	Dock DockPosition = Dock::Left;
	int GridRow = 0;
	int GridColumn = 0;
	int GridRowSpan = 1;
	int GridColumnSpan = 1;
	int ZIndex = 0;
	uint32_t LocalValueMask = 0;

	bool HasLocalValue(DesignerPlacementLocalValue value) const noexcept;
	void SetLocalValue(
		DesignerPlacementLocalValue value,
		bool present = true) noexcept;
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
