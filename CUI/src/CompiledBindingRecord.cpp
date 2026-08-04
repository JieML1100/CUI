#include "CompiledBindingRecord.h"

#include <algorithm>
#include <utility>
#if CUI_ENABLE_DYNAMIC_XAML
#include <stdexcept>
#endif

CompiledBindingRecord::CompiledBindingRecord(
	std::span<const CompiledBindingRecordProperty> properties)
	: _properties(properties)
{
#if CUI_ENABLE_DYNAMIC_XAML
	// Design validates hand-authored probes. Production tables are emitted,
	// collision-checked and token-sorted by the AOT compiler, so every record
	// construction remains O(1).
	std::uint64_t previousToken = 0;
	for (const auto& property : _properties)
	{
		if (!property.Token
			|| property.Token.Value <= previousToken
			|| property.CanRead != (property.Read != nullptr)
			|| property.CanWrite != (property.Write != nullptr))
			throw std::invalid_argument(
				"Compiled binding record property table is invalid");
		previousToken = property.Token.Value;
	}
#endif
}

CompiledSourceHandle CompiledBindingRecord::MakeCompiledPropertySource(
	size_t propertyIndex) noexcept
{
	if (propertyIndex >= _properties.size()) return {};
	return MakeCompiledPropertySource(_properties[propertyIndex]);
}

CompiledSourceHandle CompiledBindingRecord::MakeCompiledPropertySource(
	const CompiledBindingRecordProperty& property) noexcept
{
	static const CompiledSourceOps operations{
		// Capabilities
		+[](const CompiledSourceHandle& source)
		{
			const auto* descriptor = static_cast<
				const CompiledBindingRecordProperty*>(source.Context);
			if (!descriptor)
				return CompiledBindingPathCapabilities::None;
			auto result = CompiledBindingPathCapabilities::None;
			if (descriptor->CanRead && descriptor->Read)
				result = result | CompiledBindingPathCapabilities::Read;
			if (descriptor->CanWrite && descriptor->Write)
				result = result | CompiledBindingPathCapabilities::Write;
			if (descriptor->CanObserve)
				result = result | CompiledBindingPathCapabilities::Observe;
			return result;
		},
		// ValueKind
		+[](const CompiledSourceHandle& source)
		{
			const auto* descriptor = static_cast<
				const CompiledBindingRecordProperty*>(source.Context);
			return descriptor
				? descriptor->ValueKind : BindingValueKind::Empty;
		},
		// Lifetime
		+[](const CompiledSourceHandle& source)
		{
			auto* record = static_cast<CompiledBindingRecord*>(source.Object);
			return record
				? record->BindingLifetime() : std::weak_ptr<const void>{};
		},
		// Read
		+[](const CompiledSourceHandle& source, BindingValue& out)
		{
			auto* record = static_cast<CompiledBindingRecord*>(source.Object);
			const auto* descriptor = static_cast<
				const CompiledBindingRecordProperty*>(source.Context);
			return record && descriptor && descriptor->Read
				&& descriptor->Read(*record, out);
		},
		// Write
		+[](const CompiledSourceHandle& source, const BindingValue& value)
		{
			auto* record = static_cast<CompiledBindingRecord*>(source.Object);
			const auto* descriptor = static_cast<
				const CompiledBindingRecordProperty*>(source.Context);
			return record && descriptor
				&& record->TrySetCompiledProperty(*descriptor, value);
		},
		// Subscribe
		+[](const CompiledSourceHandle& source,
			DependencyPropertyChangeHandler handler)
		{
			auto* record = static_cast<CompiledBindingRecord*>(source.Object);
			const auto* descriptor = static_cast<
				const CompiledBindingRecordProperty*>(source.Context);
			if (!record || !descriptor || !descriptor->CanObserve || !handler)
				return EventConnection{};
			const auto expectedProperty = descriptor->Token;
			return record->_propertyChanged.Subscribe(
				[expectedProperty, handler = std::move(handler)](
					const PropertyChangedEventArgs& eventArgs)
				{
					if (eventArgs.PropertyToken
						&& eventArgs.PropertyToken != expectedProperty) return;
					handler();
				});
		},
		// Validation
		+[](const CompiledSourceHandle& source)
		{
			auto* record = static_cast<CompiledBindingRecord*>(source.Object);
			const auto* descriptor = static_cast<
				const CompiledBindingRecordProperty*>(source.Context);
			return record && descriptor
				? record->GetValidationIssues(descriptor->Token)
				: std::vector<BindingValidationIssue>{};
		},
		// SubscribeValidation
		+[](const CompiledSourceHandle& source,
			DependencyPropertyChangeHandler handler)
		{
			auto* record = static_cast<CompiledBindingRecord*>(source.Object);
			const auto* descriptor = static_cast<
				const CompiledBindingRecordProperty*>(source.Context);
			if (!record || !descriptor || !handler)
				return EventConnection{};
			auto* validationChanged = record->ValidationChanged();
			if (!validationChanged) return EventConnection{};
			const auto expectedProperty = descriptor->Token;
			return validationChanged->Subscribe(
				[expectedProperty, handler = std::move(handler)](
					const BindingValidationChangedEventArgs& eventArgs)
				{
					if (eventArgs.PropertyToken
						&& eventArgs.PropertyToken != expectedProperty) return;
					handler();
				});
		}
	};

	return { this, &property, &operations };
}

const CompiledBindingRecordProperty* CompiledBindingRecord::FindProperty(
	BindingSourcePropertyToken property) const noexcept
{
	if (!property) return nullptr;
	const auto found = std::lower_bound(
		_properties.begin(), _properties.end(), property.Value,
		[](const CompiledBindingRecordProperty& candidate,
			std::uint64_t value)
		{
			return candidate.Token.Value < value;
		});
	return found != _properties.end() && found->Token == property
		? &*found : nullptr;
}

bool CompiledBindingRecord::TryGetValue(
	BindingSourcePropertyToken property,
	BindingValue& out) const
{
	const auto* descriptor = FindProperty(property);
	return descriptor && descriptor->Read
		&& descriptor->Read(*this, out);
}

bool CompiledBindingRecord::TrySetValue(
	BindingSourcePropertyToken property,
	const BindingValue& value)
{
	const auto* descriptor = FindProperty(property);
	return descriptor && TrySetCompiledProperty(*descriptor, value);
}

bool CompiledBindingRecord::TrySetCompiledProperty(
	const CompiledBindingRecordProperty& property,
	const BindingValue& value)
{
	if (!property.Write) return false;
	const auto result = property.Write(*this, value);
	if (result == CompiledBindingRecordWriteResult::Failed)
		return false;
	if (result == CompiledBindingRecordWriteResult::Changed
		&& property.CanObserve)
		_propertyChanged.Notify(property.Token);
	return true;
}

bool CompiledBindingRecord::TryGetPropertyMetadata(
	BindingSourcePropertyToken property,
	BindingSourcePropertyMetadata& out) const
{
	const auto* descriptor = FindProperty(property);
	if (!descriptor) return false;
	out = {};
	out.ValueKind = descriptor->ValueKind;
	out.ValueType = descriptor->ValueType;
	out.CanRead = descriptor->CanRead;
	out.CanWrite = descriptor->CanWrite;
	out.CanObserve = descriptor->CanObserve;
	return true;
}

#if CUI_ENABLE_DYNAMIC_XAML
bool CompiledBindingRecord::TryGetValue(
	const std::wstring& propertyName,
	BindingValue& out) const
{
	(void)propertyName;
	(void)out;
	return false;
}

bool CompiledBindingRecord::TrySetValue(
	const std::wstring& propertyName,
	const BindingValue& value)
{
	(void)propertyName;
	(void)value;
	return false;
}

bool CompiledBindingRecord::TryGetPropertyMetadata(
	const std::wstring& propertyName,
	BindingSourcePropertyMetadata& out) const
{
	(void)propertyName;
	(void)out;
	return false;
}

std::vector<BindingSourcePropertyMetadata>
CompiledBindingRecord::GetProperties() const
{
	return {};
}

std::vector<BindingValidationIssue>
CompiledBindingRecord::GetValidationIssues(
	const std::wstring& propertyName) const
{
	(void)propertyName;
	return {};
}
#endif
