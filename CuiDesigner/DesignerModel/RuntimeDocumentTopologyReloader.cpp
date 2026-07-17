#include "RuntimeDocumentTopologyReloader.h"

#include "DesignDocumentGraph.h"
#include "DesignDocumentMaterializer.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerStyleSheetUtils.h"
#include "../../CUI/include/SplitContainer.h"
#include "../../CUI/include/ToolBar.h"

#include <Convert.h>

#include <algorithm>
#include <exception>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace DesignerModel
{
namespace
{
	DesignDocumentMaterializationOptions MaterializationOptionsFor(
		const RuntimeDocumentLoadOptions& options)
	{
		DesignDocumentMaterializationOptions result;
		result.AllowCustomControlProxy = options.AllowCustomControlProxy;
		result.AllowDeferredCustomMetadata = options.AllowCustomControlProxy;
		if (options.CustomControls)
		{
			auto registry = options.CustomControls;
			result.CustomControlFactory = [registry](const DesignNode& node)
			{
				return registry->Create(node);
			};
		}
		return result;
	}

	void SetError(std::wstring* output, std::wstring value)
	{
		if (output) *output = std::move(value);
	}

	struct DocumentTopology
	{
		const DesignDocument* Document = nullptr;
		DesignDocumentGraph Graph;
		std::unordered_map<int, size_t> IndexById;
		std::unordered_map<std::wstring, int> TabOwnerByPageId;

		bool Build(const DesignDocument& document, std::wstring* outError)
		{
			Document = &document;
			if (!DesignDocumentGraph::Build(document, Graph, outError)) return false;
			IndexById.reserve(document.Nodes.size());
			for (size_t index = 0; index < document.Nodes.size(); ++index)
			{
				const auto& node = document.Nodes[index];
				IndexById.emplace(node.Id, index);
				if (node.Type != UIClass::UI_TabControl
					|| !node.Extra["pages"].is_array()) continue;
				for (const auto& page : node.Extra["pages"])
				{
					if (!page.is_object() || !page["id"].is_string()) continue;
					auto id = Convert::Utf8ToUnicode(page["id"].get<std::string>());
					if (!id.empty()) TabOwnerByPageId.emplace(std::move(id), node.Id);
				}
			}
			return true;
		}

		const DesignNode* Find(int stableId) const noexcept
		{
			const auto found = IndexById.find(stableId);
			return found == IndexById.end()
				? nullptr : &Document->Nodes[found->second];
		}

		std::vector<int> Children(const DesignNode& node) const
		{
			std::vector<int> result;
			auto append = [&](const std::wstring& key)
			{
				for (const auto index : Graph.ChildrenOf(key))
					result.push_back(Document->Nodes[index].Id);
			};
			append(node.Name);
			if (node.Type == UIClass::UI_TabControl
				&& node.Extra["pages"].is_array())
			{
				for (const auto& page : node.Extra["pages"])
				{
					if (!page.is_object() || !page["id"].is_string()) continue;
					append(Convert::Utf8ToUnicode(page["id"].get<std::string>()));
				}
			}
			return result;
		}

		int OwningNodeId(const DesignNode& node) const noexcept
		{
			if (node.ParentId > 0) return node.ParentId;
			const auto found = TabOwnerByPageId.find(node.ParentRef);
			return found == TabOwnerByPageId.end() ? 0 : found->second;
		}
	};

	bool SameReusablePayload(const DesignNode& current, const DesignNode& next)
	{
		return current.Id == next.Id
			&& current.Name == next.Name
			&& current.Type == next.Type
			&& current.CustomType == next.CustomType
			&& current.Props == next.Props
			&& current.Extra == next.Extra
			&& current.Events == next.Events
			&& current.Bindings == next.Bindings;
	}

	class ReusableSubtreeMatcher
	{
	public:
		ReusableSubtreeMatcher(
			const DocumentTopology& current,
			const DocumentTopology& next)
			: _current(current), _next(next)
		{
		}

		bool Equivalent(int stableId)
		{
			const auto memo = _memo.find(stableId);
			if (memo != _memo.end()) return memo->second;
			const auto* current = _current.Find(stableId);
			const auto* next = _next.Find(stableId);
			bool equivalent = current && next
				&& SameReusablePayload(*current, *next);
			if (equivalent)
			{
				const auto currentChildren = _current.Children(*current);
				const auto nextChildren = _next.Children(*next);
				equivalent = currentChildren == nextChildren;
				if (equivalent)
					for (const auto childId : nextChildren)
						if (!Equivalent(childId))
						{
							equivalent = false;
							break;
						}
			}
			_memo.emplace(stableId, equivalent);
			return equivalent;
		}

		void CollectSubtreeIds(int stableId, std::unordered_set<int>& result)
		{
			if (!result.insert(stableId).second) return;
			const auto* node = _next.Find(stableId);
			if (!node) return;
			for (const auto childId : _next.Children(*node))
				CollectSubtreeIds(childId, result);
		}

	private:
		const DocumentTopology& _current;
		const DocumentTopology& _next;
		std::unordered_map<int, bool> _memo;
	};

	struct Attachment
	{
		Control* Parent = nullptr;
		int Index = -1;
		bool IsRoot = false;
		bool HasToolBarOverride = false;
		SIZE ToolBarOverride{};
	};

	bool DetachFrom(
		Control* control,
		std::vector<std::unique_ptr<Control>>& roots,
		Attachment& attachment,
		std::unique_ptr<Control>& owner)
	{
		if (!control) return false;
		attachment = {};
		attachment.Parent = control->Parent;
		if (attachment.Parent)
		{
			attachment.Index = attachment.Parent->IndexOfControl(control);
			if (attachment.Index < 0) return false;
			if (auto* toolBar = dynamic_cast<ToolBar*>(attachment.Parent))
			{
				attachment.HasToolBarOverride =
					toolBar->TryGetToolItemSizeOverride(
						control, attachment.ToolBarOverride);
				toolBar->ClearToolItemSizeOverride(control);
			}
			owner = attachment.Parent->DetachControl(control);
			return owner && owner.get() == control;
		}

		const auto found = std::find_if(
			roots.begin(), roots.end(),
			[control](const auto& value) { return value.get() == control; });
		if (found == roots.end()) return false;
		attachment.IsRoot = true;
		attachment.Index = static_cast<int>(found - roots.begin());
		owner = std::move(*found);
		roots.erase(found);
		return owner && owner.get() == control;
	}

	bool AttachTo(
		std::unique_ptr<Control>& owner,
		std::vector<std::unique_ptr<Control>>& roots,
		const Attachment& attachment)
	{
		if (!owner) return false;
		if (attachment.IsRoot)
		{
			const auto index = (std::min)(
				static_cast<size_t>((std::max)(0, attachment.Index)),
				roots.size());
			roots.insert(roots.begin() + index, std::move(owner));
			return true;
		}
		if (!attachment.Parent) return false;

		auto* raw = owner.get();
		try
		{
			const auto index = (std::clamp)(
				attachment.Index, 0, attachment.Parent->Count);
			attachment.Parent->InsertOwned(index, std::move(owner));
			if (attachment.HasToolBarOverride)
				if (auto* toolBar = dynamic_cast<ToolBar*>(attachment.Parent))
					toolBar->SetToolItemSizeOverride(
						raw, attachment.ToolBarOverride);
			return true;
		}
		catch (...)
		{
			if (!owner && raw->Parent == attachment.Parent)
			{
				try { owner = attachment.Parent->DetachControl(raw); }
				catch (...) {}
			}
			return false;
		}
	}

	struct SubtreeSwap
	{
		int StableId = 0;
		Control* Reused = nullptr;
		Attachment PreviousAttachment;
		Attachment CandidateAttachment;
		std::unique_ptr<Control> Placeholder;
	};

	Control* ResolveDesignerParent(Control* control) noexcept
	{
		if (!control || !control->Parent) return nullptr;
		auto* actual = control->Parent;
		if (actual->Parent)
			if (auto* split = dynamic_cast<SplitContainer*>(actual->Parent))
				if (actual == split->FirstPanel()
					|| actual == split->SecondPanel()) return split;
		return actual;
	}

	void RefreshRecordsAndRoots(
		std::vector<std::unique_ptr<Control>>& ownedRoots,
		std::vector<Control*>& rootControls,
		std::vector<std::shared_ptr<DesignerControl>>& controls)
	{
		rootControls.clear();
		rootControls.reserve(ownedRoots.size());
		for (const auto& root : ownedRoots)
			if (root) rootControls.push_back(root.get());
		for (const auto& record : controls)
			if (record && record->ControlInstance)
				record->DesignerParent =
					ResolveDesignerParent(record->ControlInstance);
	}

	bool HasConfiguredControlEvents(const RuntimeDocument& document)
	{
		return std::any_of(
			document.Controls().begin(), document.Controls().end(),
			[](const auto& control)
			{
				return control && !control->EventHandlers.empty();
			});
	}
}

bool RuntimeDocumentTopologyReloader::TryReload(
	const DesignDocument& document,
	RuntimeDocument& output,
	const RuntimeDocumentLoadOptions& effectiveOptions,
	bool& outApplied,
	size_t& outReusedControlCount,
	std::wstring* outError,
	const CandidateCommit& candidateCommit)
{
	outApplied = false;
	outReusedControlCount = 0;
	if (!output._sourceDocument || output._rootsReleased) return true;

	DocumentTopology currentTopology;
	DocumentTopology nextTopology;
	if (!currentTopology.Build(*output._sourceDocument, outError)
		|| !nextTopology.Build(document, outError)) return false;

	ReusableSubtreeMatcher matcher(currentTopology, nextTopology);
	std::vector<int> reusableRoots;
	for (const auto& node : document.Nodes)
	{
		if (!matcher.Equivalent(node.Id)) continue;
		const auto ownerId = nextTopology.OwningNodeId(node);
		if (ownerId > 0 && matcher.Equivalent(ownerId)) continue;
		reusableRoots.push_back(node.Id);
	}
	if (reusableRoots.empty()) return true;
	std::unordered_set<int> reusedIds;
	for (const auto stableId : reusableRoots)
		matcher.CollectSubtreeIds(stableId, reusedIds);

	MaterializedControlTree materialized;
	if (!DesignDocumentMaterializer::Materialize(
		document, materialized,
		MaterializationOptionsFor(effectiveOptions), outError)) return false;

	RuntimeDocument candidate;
	candidate._sourceDocument = document;
	candidate._form = document.Form;
	candidate._dataContextSchema = document.DataContextSchema;
	DesignerDataContextSchemaUtils::Canonicalize(candidate._dataContextSchema);
	candidate._styleSheet = document.StyleSheet;
	candidate._customControls = effectiveOptions.CustomControls;
	candidate._allowCustomControlProxy =
		effectiveOptions.AllowCustomControlProxy;
	candidate._controls = std::move(materialized.Controls);
	candidate._ownedRoots = std::move(materialized.Roots);
	RefreshRecordsAndRoots(
		candidate._ownedRoots, candidate._rootControls, candidate._controls);
	candidate.RebuildControlIndex();

	std::unordered_map<int, std::shared_ptr<DesignerControl>> oldRecords;
	std::unordered_map<int, std::shared_ptr<DesignerControl>> candidateRecords;
	oldRecords.reserve(output._controls.size());
	candidateRecords.reserve(candidate._controls.size());
	for (const auto& record : output._controls)
		if (record) oldRecords.emplace(record->StableId, record);
	for (const auto& record : candidate._controls)
		if (record) candidateRecords.emplace(record->StableId, record);

	std::vector<SubtreeSwap> swaps;
	swaps.reserve(reusableRoots.size());
	auto rollbackTopology = [&]() noexcept
	{
		for (auto position = swaps.rbegin(); position != swaps.rend(); ++position)
		{
			std::unique_ptr<Control> reusedOwner;
			Attachment actualCandidateAttachment;
			if (!DetachFrom(
				position->Reused,
				candidate._ownedRoots,
				actualCandidateAttachment,
				reusedOwner)) continue;
			(void)AttachTo(
				position->Placeholder,
				candidate._ownedRoots,
				position->CandidateAttachment);
			(void)AttachTo(
				reusedOwner,
				output._ownedRoots,
				position->PreviousAttachment);
		}
		RefreshRecordsAndRoots(
			output._ownedRoots, output._rootControls, output._controls);
		RefreshRecordsAndRoots(
			candidate._ownedRoots, candidate._rootControls, candidate._controls);
	};

	for (const auto stableId : reusableRoots)
	{
		const auto oldFound = oldRecords.find(stableId);
		const auto candidateFound = candidateRecords.find(stableId);
		if (oldFound == oldRecords.end() || candidateFound == candidateRecords.end()
			|| !oldFound->second || !candidateFound->second
			|| !oldFound->second->ControlInstance
			|| !candidateFound->second->ControlInstance)
		{
			rollbackTopology();
			SetError(outError,
				L"拓扑重组无法解析可复用控件稳定 ID："
				+ std::to_wstring(stableId));
			return false;
		}

		SubtreeSwap swap;
		swap.StableId = stableId;
		swap.Reused = oldFound->second->ControlInstance;
		std::unique_ptr<Control> reusedOwner;
		if (!DetachFrom(
			swap.Reused, output._ownedRoots,
			swap.PreviousAttachment, reusedOwner))
		{
			rollbackTopology();
			SetError(outError,
				L"拓扑重组无法从旧树分离控件："
				+ oldFound->second->Name);
			return false;
		}

		if (!DetachFrom(
			candidateFound->second->ControlInstance,
			candidate._ownedRoots,
			swap.CandidateAttachment,
			swap.Placeholder))
		{
			(void)AttachTo(
				reusedOwner, output._ownedRoots, swap.PreviousAttachment);
			rollbackTopology();
			SetError(outError,
				L"拓扑重组无法从候选树分离控件："
				+ candidateFound->second->Name);
			return false;
		}

		if (!AttachTo(
			reusedOwner, candidate._ownedRoots, swap.CandidateAttachment))
		{
			(void)AttachTo(
				swap.Placeholder,
				candidate._ownedRoots,
				swap.CandidateAttachment);
			(void)AttachTo(
				reusedOwner, output._ownedRoots, swap.PreviousAttachment);
			rollbackTopology();
			SetError(outError,
				L"拓扑重组无法把旧控件挂载到候选树："
				+ oldFound->second->Name);
			return false;
		}
		swaps.push_back(std::move(swap));
	}

	for (auto& record : candidate._controls)
	{
		if (!record || !reusedIds.contains(record->StableId)) continue;
		const auto found = oldRecords.find(record->StableId);
		if (found != oldRecords.end()) record = found->second;
	}
	RefreshRecordsAndRoots(
		candidate._ownedRoots, candidate._rootControls, candidate._controls);
	candidate.RebuildControlIndex();

	const auto previousDataContext = output._dataContext;
	bool oldBindingsCleared = false;
	auto restoreStyle = [](RuntimeDocument& target) noexcept
	{
		try
		{
			std::shared_ptr<ControlStyleSheet> styleSheet;
			if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
				target._styleSheet, styleSheet, nullptr)) return;
			for (auto* root : target._rootControls)
				if (root) (void)root->SetStyleSheet(styleSheet, true);
		}
		catch (...) {}
	};
	auto rollbackRuntime = [&]() noexcept
	{
		candidate.ClearControlEvents();
		candidate.ClearDataBindings();
		rollbackTopology();
		restoreStyle(output);
		if (oldBindingsCleared && previousDataContext)
			(void)output.BindDataContext(previousDataContext, nullptr);
	};

	try
	{
		output.ClearDataBindings();
		oldBindingsCleared = true;
		if (effectiveOptions.DataContext
			&& !candidate.BindDataContext(effectiveOptions.DataContext, outError))
		{
			const auto error = outError ? *outError : std::wstring{};
			rollbackRuntime();
			SetError(outError, error);
			return false;
		}

		if (effectiveOptions.ControlEventResolver)
		{
			if (!candidate.BindControlEvents(
				effectiveOptions.ControlEventResolver, outError))
			{
				const auto error = outError ? *outError : std::wstring{};
				rollbackRuntime();
				SetError(outError, error);
				return false;
			}
		}
		else if (effectiveOptions.RequireControlEventResolver
			&& HasConfiguredControlEvents(candidate))
		{
			rollbackRuntime();
			SetError(outError,
				L"拓扑重组后的文档包含控件事件，但没有事件名称解析器。");
			return false;
		}

		std::shared_ptr<ControlStyleSheet> runtimeStyleSheet;
		if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
			document.StyleSheet, runtimeStyleSheet, outError))
		{
			const auto error = outError ? *outError : std::wstring{};
			rollbackRuntime();
			SetError(outError, error);
			return false;
		}
		for (auto* root : candidate._rootControls)
		{
			if (!root || root->SetStyleSheet(runtimeStyleSheet, true)) continue;
			rollbackRuntime();
			SetError(outError, L"文档样式表无法应用到重组后的完整控件树。");
			return false;
		}
	}
	catch (const std::exception&)
	{
		rollbackRuntime();
		SetError(outError, L"拓扑重组应用运行时附件时抛出异常。");
		return false;
	}
	catch (...)
	{
		rollbackRuntime();
		SetError(outError, L"拓扑重组应用运行时附件时发生未知异常。");
		return false;
	}

	bool candidateCommitted = true;
	if (candidateCommit)
		try
		{
			candidateCommitted = candidateCommit(candidate, outError);
		}
		catch (...)
		{
			candidateCommitted = false;
			SetError(outError, L"宿主提交重组后的根控件时抛出异常。");
		}
	if (!candidateCommitted)
	{
		const auto error = outError ? *outError : std::wstring{};
		rollbackRuntime();
		SetError(outError, error.empty()
			? std::wstring(L"宿主拒绝提交重组后的根控件。") : error);
		return false;
	}

	outReusedControlCount = reusedIds.size();
	output = std::move(candidate);
	outApplied = true;
	if (outError) outError->clear();
	return true;
}
}
