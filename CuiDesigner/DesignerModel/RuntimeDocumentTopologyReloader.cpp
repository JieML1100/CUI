#include "RuntimeDocumentTopologyReloader.h"

#include "DesignDocumentGraph.h"
#include "../../CuiRuntime/include/XamlObjectMaterializer.h"
#include "../DesignerDataContextSchemaUtils.h"
#include "../DesignerStyleSheetUtils.h"
#include "../../CUI/include/ItemsControl.h"
#include "../../CUI/include/StyleInfrastructure.h"
#include "../../CUI/include/TemplateInfrastructure.h"
#include "../../CUI/include/XamlInfrastructure.h"

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
	class ReusedNativeSurfacePlaceholderBehavior final
		: public INativeSurfaceBehavior
	{
	public:
		void Render(
			NativeSurface&,
			NativeSurfaceRenderContext&) override
		{
		}
	};

	CuiRuntime::XamlMaterializationOptions MaterializationOptionsFor(
		const RuntimeDocumentLoadOptions& options)
	{
		CuiRuntime::XamlMaterializationOptions result;
		result.AllowNativeSurfacePlaceholder =
			options.AllowNativeSurfacePlaceholder;
		if (options.NativeSurfaceBehaviors)
		{
			auto registry = options.NativeSurfaceBehaviors;
			result.NativeSurfaceBehaviorFactory =
				[registry](const DesignNode&, NativeSurface& host)
				{
					return registry->Create(host.GetBehaviorKey(), host);
				};
		}
		if (options.DeclarativeComponentBehaviors)
		{
			auto registry = options.DeclarativeComponentBehaviors;
			result.DeclarativeComponentBehaviorFactory =
				[registry](const DeclarativeComponentBehaviorContext& context)
				{
					return registry->Create(context);
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
		std::unordered_map<std::wstring, int> IdByName;

		bool Build(const DesignDocument& document, std::wstring* outError)
		{
			Document = &document;
			if (!DesignDocumentGraph::Build(document, Graph, outError)) return false;
			IndexById.reserve(document.Nodes.size());
			for (size_t index = 0; index < document.Nodes.size(); ++index)
			{
				const auto& node = document.Nodes[index];
				IndexById.emplace(node.Id, index);
				IdByName.emplace(node.Name, node.Id);
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
			return result;
		}

		int OwningNodeId(const DesignNode& node) const noexcept
		{
			if (node.ParentId > 0) return node.ParentId;
			const auto found = IdByName.find(node.ParentRef);
			return found == IdByName.end() ? 0 : found->second;
		}
	};

	bool SameReusablePayload(const DesignNode& current, const DesignNode& next)
	{
		return current.Id == next.Id
			&& current.Name == next.Name
			&& current.Type == next.Type
			&& current.ComponentType == next.ComponentType
			&& current.ComponentContentProperty
				== next.ComponentContentProperty
			&& current.PresentedComponentContent
				== next.PresentedComponentContent
			&& current.Properties == next.Properties
			&& current.Structure == next.Structure
			&& current.TemplateState == next.TemplateState
			&& current.LocalResources == next.LocalResources
			&& current.LocalObjectResources == next.LocalObjectResources
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
				&& SameReusablePayload(*current, *next)
				&& SameLexicalResourceScope(stableId);
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
		bool SameLexicalResourceScope(int stableId) const
		{
			int currentId = stableId;
			int nextId = stableId;
			for (;;)
			{
				const auto* current = _current.Find(currentId);
				const auto* next = _next.Find(nextId);
				if (!current || !next
					|| current->LocalResources != next->LocalResources
					|| current->LocalObjectResources
						!= next->LocalObjectResources)
					return false;
				const auto currentOwner = _current.OwningNodeId(*current);
				const auto nextOwner = _next.OwningNodeId(*next);
				if (currentOwner <= 0 || nextOwner <= 0)
					return currentOwner <= 0 && nextOwner <= 0;
				currentId = currentOwner;
				nextId = nextOwner;
			}
		}

		const DocumentTopology& _current;
		const DocumentTopology& _next;
		std::unordered_map<int, bool> _memo;
	};

	struct Attachment
	{
		Control* Parent = nullptr;
		Control* LogicalParent = nullptr;
		Control* TemplatedParent = nullptr;
		ItemsControl* ItemsOwner = nullptr;
		int Index = -1;
		bool IsRoot = false;
	};

	bool DetachFrom(
		Control* control,
		std::unique_ptr<Control>& contentRoot,
		Attachment& attachment,
		std::unique_ptr<Control>& owner)
	{
		if (!control) return false;
		attachment = {};
		attachment.Parent = control->GetVisualParent();
		attachment.LogicalParent = control->GetLogicalParent();
		attachment.TemplatedParent = control->GetTemplatedParent();
		if (attachment.Parent)
		{
			attachment.Index = attachment.Parent->IndexOfVisualChild(control);
			if (attachment.Index < 0) return false;
			if (auto* items = dynamic_cast<ItemsControl*>(
				attachment.LogicalParent);
				items && attachment.Parent
					== cui::framework::TemplateAccess::GetItemsHost(*items))
			{
				for (size_t index = 0; index < items->AuthoredItemCount(); ++index)
				{
					if (items->GetAuthoredItem(index) != control) continue;
					attachment.ItemsOwner = items;
					attachment.Index = static_cast<int>(index);
					owner = items->DetachItemControlAt(index);
					return owner && owner.get() == control;
				}
			}
			owner = attachment.Parent->DetachVisualChild(control);
			return owner && owner.get() == control;
		}

		if (contentRoot.get() != control) return false;
		attachment.IsRoot = true;
		attachment.Index = 0;
		owner = std::move(contentRoot);
		return owner && owner.get() == control;
	}

	bool AttachTo(
		std::unique_ptr<Control>& owner,
		std::unique_ptr<Control>& contentRoot,
		const Attachment& attachment)
	{
		if (!owner) return false;
		if (attachment.IsRoot)
		{
			if (contentRoot) return false;
			auto* raw = owner.get();
			contentRoot = std::move(owner);
			cui::framework::XamlAccess::SetTemplatedParent(
				*raw, attachment.TemplatedParent);
			cui::framework::XamlAccess::SetLogicalParent(
				*raw, attachment.LogicalParent);
			return true;
		}
		if (!attachment.Parent) return false;

		auto* raw = owner.get();
		try
		{
			cui::framework::XamlAccess::SetLogicalParent(*raw, nullptr);
			cui::framework::XamlAccess::SetTemplatedParent(*raw, nullptr);
			if (attachment.ItemsOwner)
			{
				const auto index = (std::min)(
					static_cast<size_t>((std::max)(0, attachment.Index)),
					attachment.ItemsOwner->AuthoredItemCount());
				attachment.ItemsOwner->InsertItemControl(index, std::move(owner));
			}
			else
			{
				const auto index = (std::clamp)(
					attachment.Index, 0, attachment.Parent->VisualChildCount());
				attachment.Parent->InsertOwned(index, std::move(owner));
			}
			cui::framework::XamlAccess::SetTemplatedParent(
				*raw, attachment.TemplatedParent);
			cui::framework::XamlAccess::SetLogicalParent(
				*raw, attachment.LogicalParent);
			return true;
		}
		catch (...)
		{
			if (!owner && attachment.ItemsOwner)
			{
				try { owner = attachment.ItemsOwner->DetachItemControl(raw); }
				catch (...) {}
			}
			if (!owner && raw->GetVisualParent() == attachment.Parent)
			{
				try { owner = attachment.Parent->DetachVisualChild(raw); }
				catch (...) {}
			}
			if (owner)
			{
				cui::framework::XamlAccess::SetLogicalParent(*owner, nullptr);
				cui::framework::XamlAccess::SetTemplatedParent(*owner, nullptr);
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
		if (!control || !control->GetVisualParent()) return nullptr;
		return control->GetVisualParent();
	}

	void RefreshRecordsAndContentRoot(
		std::unique_ptr<Control>& ownedContentRoot,
		ControlWeakReference& contentRoot,
		std::vector<std::shared_ptr<DesignerControl>>& controls)
	{
		contentRoot = ownedContentRoot.get();
		for (const auto& record : controls)
			if (record && record->ControlInstance)
			{
				// Component content is physically parented by a generated presenter,
				// while its public designer parent remains the component instance.
				if (record->ComponentContentProperty.empty())
					record->DesignerParent =
						ResolveDesignerParent(record->ControlInstance);
			}
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

	bool HasLocalStyleRules(const DesignDocument& document)
	{
		return std::any_of(document.Nodes.begin(), document.Nodes.end(),
			[](const DesignNode& node)
			{ return !node.LocalResources.Rules.empty(); });
	}

	bool HasStructuralTemplateStyles(const DesignDocument& document)
	{
		auto hasTemplateSetter = [](const DesignerStyleSheet& sheet)
		{
			return std::any_of(sheet.Rules.begin(), sheet.Rules.end(),
				[](const auto& rule)
				{
					return std::any_of(
						rule.Setters.begin(), rule.Setters.end(), [](const auto& setter)
						{ return setter.PropertyName == L"Template"; });
				});
		};
		return hasTemplateSetter(document.StyleSheet)
			|| std::any_of(document.Nodes.begin(), document.Nodes.end(),
				[&](const auto& node) { return hasTemplateSetter(node.LocalResources); });
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
	if (effectiveOptions.ForceBehaviorRefresh) return true;
	if (!output._sourceDocument || output._contentReleased) return true;
	if (HasStructuralTemplateStyles(*output._sourceDocument)
		|| HasStructuralTemplateStyles(document)) return true;
	if ((output._sourceDocument->HasResourceBackedVisualStates()
			|| document.HasResourceBackedVisualStates()
			|| HasLocalStyleRules(*output._sourceDocument)
			|| HasLocalStyleRules(document))
		&& output._sourceDocument->StyleSheet != document.StyleSheet)
		return true;
	if (output._sourceDocument->Components != document.Components
		|| output._sourceDocument->ControlTemplates != document.ControlTemplates
		|| output._sourceDocument->DataTypes != document.DataTypes
		|| output._sourceDocument->DataTemplates != document.DataTemplates
		|| output._sourceDocument->ItemsPanelTemplates
			!= document.ItemsPanelTemplates
		|| output._sourceDocument->GroupStyles != document.GroupStyles
		|| output._sourceDocument->DataLists != document.DataLists
		|| output._sourceDocument->CollectionViews != document.CollectionViews)
		return true;

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
	std::vector<std::wstring> reusedRuntimePrefixes;
	reusedRuntimePrefixes.reserve(reusedIds.size());
	for (const auto stableId : reusedIds)
		if (const auto* node = nextTopology.Find(stableId))
			reusedRuntimePrefixes.push_back(node->Name);
	auto isReusedRuntimeNode = [&reusedRuntimePrefixes](
		const std::wstring& name)
	{
		return std::any_of(
			reusedRuntimePrefixes.begin(), reusedRuntimePrefixes.end(),
			[&](const auto& prefix)
			{
				return name == prefix
					|| (name.size() > prefix.size()
						&& name.starts_with(prefix)
						&& name[prefix.size()] == L'@');
			});
	};

	CuiRuntime::XamlObjectTree materialized;
	auto materializationOptions =
		MaterializationOptionsFor(effectiveOptions);
	if (materializationOptions.NativeSurfaceBehaviorFactory)
	{
		auto factory =
			std::move(materializationOptions.NativeSurfaceBehaviorFactory);
		materializationOptions.NativeSurfaceBehaviorFactory =
			[factory = std::move(factory), isReusedRuntimeNode](
				const DesignNode& node,
				NativeSurface& host) mutable
			{
				if (isReusedRuntimeNode(node.Name))
					return std::unique_ptr<INativeSurfaceBehavior>(
						std::make_unique<ReusedNativeSurfacePlaceholderBehavior>());
				return factory(node, host);
			};
	}
	if (materializationOptions.DeclarativeComponentBehaviorFactory)
	{
		auto factory = std::move(
			materializationOptions.DeclarativeComponentBehaviorFactory);
		materializationOptions.DeclarativeComponentBehaviorFactory =
			[factory = std::move(factory), isReusedRuntimeNode](
				const DeclarativeComponentBehaviorContext& context) mutable
			{
				return isReusedRuntimeNode(context.InstanceName)
					? std::unique_ptr<IDeclarativeComponentBehavior>{}
					: factory(context);
			};
	}
	if (!CuiRuntime::XamlObjectMaterializer::Materialize(
		document, materialized,
		materializationOptions, outError)) return false;

	RuntimeDocument candidate;
	candidate._sourceDocument = document;
	candidate._window = document.Window;
	candidate._dataContextSchema = document.DataContextSchema;
	DesignerDataContextSchemaUtils::Canonicalize(candidate._dataContextSchema);
	candidate._styleSheet = document.StyleSheet;
	candidate._nativeSurfaceBehaviors = effectiveOptions.NativeSurfaceBehaviors;
	candidate._declarativeComponentBehaviors =
		effectiveOptions.DeclarativeComponentBehaviors;
	candidate._allowNativeSurfacePlaceholder =
		effectiveOptions.AllowNativeSurfacePlaceholder;
	candidate._controls = std::move(materialized.Controls);
	candidate._collectionViews = std::move(materialized.CollectionViews);
	candidate._commandTargetReferences.reserve(
		materialized.CommandTargets.size());
	for (auto& reference : materialized.CommandTargets)
		candidate._commandTargetReferences.push_back({
			reference.Source,
			std::move(reference.SourceName),
			std::move(reference.MenuItemPath),
			std::move(reference.TargetName),
			reference.TargetsWindow });
	candidate._inputBindingTargetReferences.reserve(
		materialized.InputBindingTargets.size());
	for (auto& reference : materialized.InputBindingTargets)
		candidate._inputBindingTargetReferences.push_back({
			reference.Source,
			std::move(reference.SourceName),
			reference.BindingIndex,
			std::move(reference.TargetName),
			reference.TargetsWindow });
	candidate._ownedContentRoot = std::move(materialized.ContentRoot);
	RefreshRecordsAndContentRoot(
		candidate._ownedContentRoot, candidate._contentRoot, candidate._controls);
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
				candidate._ownedContentRoot,
				actualCandidateAttachment,
				reusedOwner)) continue;
			(void)AttachTo(
				position->Placeholder,
				candidate._ownedContentRoot,
				position->CandidateAttachment);
			(void)AttachTo(
				reusedOwner,
				output._ownedContentRoot,
				position->PreviousAttachment);
		}
		RefreshRecordsAndContentRoot(
			output._ownedContentRoot, output._contentRoot, output._controls);
		RefreshRecordsAndContentRoot(
			candidate._ownedContentRoot, candidate._contentRoot,
			candidate._controls);
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
			swap.Reused, output._ownedContentRoot,
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
			candidate._ownedContentRoot,
			swap.CandidateAttachment,
			swap.Placeholder))
		{
			(void)AttachTo(
				reusedOwner, output._ownedContentRoot, swap.PreviousAttachment);
			rollbackTopology();
			SetError(outError,
				L"拓扑重组无法从候选树分离控件："
				+ candidateFound->second->Name);
			return false;
		}

		if (!AttachTo(
			reusedOwner, candidate._ownedContentRoot, swap.CandidateAttachment))
		{
			(void)AttachTo(
				swap.Placeholder,
				candidate._ownedContentRoot,
				swap.CandidateAttachment);
			(void)AttachTo(
				reusedOwner, output._ownedContentRoot, swap.PreviousAttachment);
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
	RefreshRecordsAndContentRoot(
		candidate._ownedContentRoot, candidate._contentRoot, candidate._controls);
	candidate.RebuildControlIndex();
	std::vector<RuntimeDocument::CommandTargetSnapshot>
		commandTargetSnapshots;
	if (!candidate.ApplyCommandTargetReferences(
		nullptr, true, &commandTargetSnapshots, outError))
	{
		rollbackTopology();
		return false;
	}

	const auto previousDataContext = output._dataContext;
	bool oldBindingsCleared = false;
	auto restoreStyle = [](RuntimeDocument& target) noexcept
	{
		try
		{
			std::shared_ptr<ControlStyleSheet> styleSheet;
			if (!DesignerStyleSheetUtils::BuildRuntimeStyleSheet(
				target._styleSheet, styleSheet, nullptr,
				target._sourceDocument
					? target._sourceDocument->ResourceBasePath : std::wstring{},
				target._sourceDocument
					? target._sourceDocument->Resources : nullptr)) return;
			if (auto* contentRoot = target._contentRoot.Get())
				(void)cui::framework::StyleAccess::SetDocumentStyles(
					*contentRoot, styleSheet, true);
		}
		catch (...) {}
	};
	auto rollbackRuntime = [&]() noexcept
	{
		candidate.ClearControlEvents();
		candidate.ClearDataBindings();
		RuntimeDocument::RestoreCommandTargetSnapshots(
			commandTargetSnapshots);
		rollbackTopology();
		restoreStyle(output);
		if (oldBindingsCleared)
		{
			if (previousDataContext)
				(void)output.BindDataContext(previousDataContext, nullptr);
			else
			{
				std::vector<RuntimeDocument::InstalledBinding> restored;
				if (output.InstallDataBindings({}, restored, nullptr))
					output._installedBindings = std::move(restored);
			}
		}
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
		if (!effectiveOptions.DataContext)
		{
			std::vector<RuntimeDocument::InstalledBinding> installed;
			if (!candidate.InstallDataBindings({}, installed, outError))
			{
				const auto error = outError ? *outError : std::wstring{};
				rollbackRuntime();
				SetError(outError, error);
				return false;
			}
			candidate._installedBindings = std::move(installed);
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
			document.StyleSheet, runtimeStyleSheet, outError,
			document.ResourceBasePath, document.Resources))
		{
			const auto error = outError ? *outError : std::wstring{};
			rollbackRuntime();
			SetError(outError, error);
			return false;
		}
		if (auto* contentRoot = candidate._contentRoot.Get();
			contentRoot
			&& !cui::framework::StyleAccess::SetDocumentStyles(
				*contentRoot, runtimeStyleSheet, true))
		{
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
			SetError(outError, L"宿主提交重组后的 Content 时抛出异常。");
		}
	if (!candidateCommitted)
	{
		const auto error = outError ? *outError : std::wstring{};
		rollbackRuntime();
		SetError(outError, error.empty()
			? std::wstring(L"宿主拒绝提交重组后的 Content。") : error);
		return false;
	}

	outReusedControlCount = reusedIds.size();
	output = std::move(candidate);
	outApplied = true;
	if (outError) outError->clear();
	return true;
}
}
