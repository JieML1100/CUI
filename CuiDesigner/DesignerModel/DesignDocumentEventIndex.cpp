#include "DesignDocumentEventIndex.h"

#include "../DesignerEventCatalog.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <map>
#include <utility>

namespace DesignerModel
{
namespace
{
	std::wstring Trim(const std::wstring& value)
	{
		size_t begin = 0;
		while (begin < value.size() && iswspace(value[begin])) ++begin;
		size_t end = value.size();
		while (end > begin && iswspace(value[end - 1])) --end;
		return value.substr(begin, end - begin);
	}

}

bool DesignDocumentEventIndex::Build(
	const DesignDocument& document,
	DesignDocumentEventIndex& output,
	std::wstring* outError)
{
	DesignDocumentEventIndex candidate;
	std::map<std::wstring, size_t> handlerByName;
	auto fail = [&](std::wstring error)
	{
		if (outError) *outError = std::move(error);
		return false;
	};
	auto add = [&](DesignEventReference reference) -> bool
	{
		std::wstring validationError;
		if (reference.HandlerName.empty()
			|| !DesignerEventCatalog::ValidateHandlerName(
				reference.HandlerName, &validationError))
		{
			return fail(L"事件 “" + reference.SubjectName + L"."
				+ reference.EventName + L"” 的处理函数无效："
				+ (validationError.empty()
					? std::wstring(L"函数名为空。") : validationError));
		}

		const auto referenceIndex = candidate._references.size();
		auto existing = handlerByName.find(reference.HandlerName);
		if (existing == handlerByName.end())
		{
			const auto handlerIndex = candidate._handlers.size();
			handlerByName.emplace(reference.HandlerName, handlerIndex);
			candidate._handlers.push_back({
				reference.HandlerName,
				reference.ParameterList,
				reference.Signature,
				{ referenceIndex } });
		}
		else
		{
			auto& handler = candidate._handlers[existing->second];
			if (handler.Signature != reference.Signature)
				return fail(L"事件处理函数 “" + reference.HandlerName
					+ L"” 被不同参数签名的事件复用。");
			handler.ReferenceIndices.push_back(referenceIndex);
		}
		candidate._references.push_back(std::move(reference));
		return true;
	};

	for (const auto& [eventName, storedHandler] : document.Window.Events)
	{
		if (Trim(storedHandler).empty()) continue;
		const auto descriptor = DesignerEventCatalog::FindWindowEvent(eventName);
		if (!descriptor)
			return fail(L"窗体包含未知事件：" + eventName);
		const auto resolved = DesignerEventCatalog::NormalizeHandlerName(
			storedHandler);
		if (!add({
			DesignEventOwnerKind::Window,
			0,
			document.Window.Name,
			UIClass::UI_Base,
			eventName,
			descriptor->EventField,
			descriptor->ParameterList,
			descriptor->Signature,
			resolved }))
			return false;
	}
	for (const auto& binding : document.Window.CommandBindings)
	{
		for (const auto& [eventName, stored] : binding.HandlerRoutes())
		{
			if (!stored || Trim(*stored).empty()) continue;
			const auto descriptor = DesignerEventCatalog::FindWindowEvent(eventName);
			if (!descriptor) return fail(
				L"Window 包含未知命令事件：" + std::wstring(eventName));
			if (!add({ DesignEventOwnerKind::Window, 0,
				document.Window.Name + L"[" + binding.Command + L"]",
				UIClass::UI_Base, eventName, descriptor->EventField,
				descriptor->ParameterList, descriptor->Signature,
				DesignerEventCatalog::NormalizeHandlerName(*stored) })) return false;
		}
	}

	for (const auto& node : document.Nodes)
	{
		for (const auto& [eventKey, stored] : node.Events)
		{
			if (Trim(stored).empty()) continue;
			const auto& storedEventName = eventKey;
			auto eventName = storedEventName;
			DesignerComponentType attachedOwnerType;
			std::wstring attachedEventName;
			std::optional<DesignerEventDescriptor> descriptor;
			if (DesignerEventCatalog::TryParseAttachedComponentEventKey(
				storedEventName, attachedOwnerType, attachedEventName))
			{
				const auto* owner = document.FindComponent(attachedOwnerType);
				if (!owner)
					return fail(L"控件 “" + node.Name
						+ L"” 的附加组件事件所有者不存在：" + storedEventName);
				const auto contract = std::find_if(
					owner->Events.begin(), owner->Events.end(),
					[&](const auto& event) { return event.Name == attachedEventName; });
				if (contract != owner->Events.end())
					descriptor = DesignerEventCatalog::FromComponentEvent(*contract);
				eventName = attachedEventName;
			}
			else
			{
				const auto* component = document.FindComponent(node.ComponentType);
				descriptor = DesignerEventCatalog::FindControlEvent(
					node.Type, eventName,
					component ? component->Events
						: std::vector<DesignerComponentEventDescriptor>{});
			}
			if (!descriptor)
				return fail(L"控件 “" + node.Name + L"” 包含未知事件："
					+ storedEventName);
			const auto resolved = DesignerEventCatalog::NormalizeHandlerName(
				stored);
			if (!add({
				DesignEventOwnerKind::Control,
				node.Id,
				node.Name,
				node.Type,
				eventName,
				descriptor->EventField,
				descriptor->ParameterList,
				descriptor->Signature,
				resolved }))
				return false;
		}
		for (const auto& binding : node.CommandBindings)
		{
			for (const auto& [eventName, stored] : binding.HandlerRoutes())
			{
				if (!stored || Trim(*stored).empty()) continue;
				const auto descriptor = DesignerEventCatalog::FindControlEvent(
					node.Type, eventName);
				if (!descriptor) return fail(L"控件 “" + node.Name
					+ L"” 包含未知命令事件：" + std::wstring(eventName));
				if (!add({ DesignEventOwnerKind::Control, node.Id,
					node.Name + L"[" + binding.Command + L"]", node.Type,
					eventName, descriptor->EventField, descriptor->ParameterList,
					descriptor->Signature,
					DesignerEventCatalog::NormalizeHandlerName(*stored) })) return false;
			}
		}
	}

	output = std::move(candidate);
	if (outError) outError->clear();
	return true;
}

const DesignEventHandlerEntry* DesignDocumentEventIndex::FindHandler(
	const std::wstring& name) const noexcept
{
	const auto found = std::find_if(
		_handlers.begin(), _handlers.end(), [&](const auto& handler)
		{
			return handler.Name == name;
		});
	return found == _handlers.end() ? nullptr : &*found;
}

bool DesignDocumentEventIndex::RenameHandler(
	DesignDocument& document,
	const std::wstring& oldName,
	const std::wstring& newName,
	size_t* outRenamedReferenceCount,
	std::wstring* outError)
{
	if (outRenamedReferenceCount) *outRenamedReferenceCount = 0;
	const auto from = Trim(oldName);
	const auto to = Trim(newName);
	std::wstring validationError;
	if (from.empty() || to.empty())
	{
		if (outError) *outError = L"重命名的原函数名和新函数名都不能为空。";
		return false;
	}
	if (!DesignerEventCatalog::ValidateHandlerName(from, &validationError)
		|| !DesignerEventCatalog::ValidateHandlerName(to, &validationError))
	{
		if (outError) *outError = validationError;
		return false;
	}

	DesignDocumentEventIndex before;
	if (!Build(document, before, outError)) return false;
	const auto* source = before.FindHandler(from);
	if (!source)
	{
		if (outError) *outError = L"文档中不存在事件处理函数 “" + from + L"”。";
		return false;
	}
	if (from == to)
	{
		if (outError) outError->clear();
		return true;
	}
	if (const auto* target = before.FindHandler(to);
		target && target->Signature != source->Signature)
	{
		if (outError) *outError = L"不能重命名为 “" + to
			+ L"”：该名称已用于不同参数签名。";
		return false;
	}

	auto candidate = document;
	size_t renamed = 0;
	for (auto& [eventName, storedHandler] : candidate.Window.Events)
	{
		if (DesignerEventCatalog::NormalizeHandlerName(storedHandler) != from)
			continue;
		storedHandler = to;
		++renamed;
	}
	for (auto& binding : candidate.Window.CommandBindings)
	{
		for (auto* stored : { &binding.PreviewCanExecute, &binding.CanExecute,
			&binding.PreviewExecuted, &binding.Executed })
		{
			if (!stored
				|| DesignerEventCatalog::NormalizeHandlerName(*stored) != from) continue;
			*stored = to;
			++renamed;
		}
	}
	for (auto& node : candidate.Nodes)
	{
		for (auto& [eventKey, stored] : node.Events)
		{
			if (DesignerEventCatalog::NormalizeHandlerName(stored) != from)
				continue;
			stored = to;
			++renamed;
		}
		for (auto& binding : node.CommandBindings)
		{
			for (auto* stored : { &binding.PreviewCanExecute, &binding.CanExecute,
				&binding.PreviewExecuted, &binding.Executed })
			{
				if (!stored
					|| DesignerEventCatalog::NormalizeHandlerName(*stored) != from) continue;
				*stored = to;
				++renamed;
			}
		}
	}
	if (renamed != source->ReferenceIndices.size())
	{
		if (outError) *outError = L"事件处理函数索引在重命名期间发生不一致。";
		return false;
	}

	DesignDocumentEventIndex after;
	if (!Build(candidate, after, outError)) return false;
	document = std::move(candidate);
	if (outRenamedReferenceCount) *outRenamedReferenceCount = renamed;
	if (outError) outError->clear();
	return true;
}
}
