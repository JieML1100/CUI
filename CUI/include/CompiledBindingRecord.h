#pragma once

#include "Binding.h"

#include <span>
#include <type_traits>
#include <typeindex>

class CompiledBindingRecord;

enum class CompiledBindingRecordWriteResult : unsigned char
{
	Failed,
	Unchanged,
	Changed
};

/**
 * One process-lifetime property entry for an AOT-generated data record.
 *
 * The callbacks are generated against a concrete C++ record type. A direct
 * endpoint addresses this entry itself; the token remains only for compatible
 * external-path lookup and change filtering. No property-name map or
 * per-record metadata table is allocated.
 */
struct CompiledBindingRecordProperty final
{
	using ReadCallback = bool(*)(
		const CompiledBindingRecord&, BindingValue&);
	using WriteCallback = CompiledBindingRecordWriteResult(*)(
		CompiledBindingRecord&, const BindingValue&);

	BindingSourcePropertyToken Token;
	BindingValueKind ValueKind = BindingValueKind::Empty;
	std::type_index ValueType{ typeid(void) };
	bool CanRead = true;
	bool CanWrite = true;
	bool CanObserve = true;
	ReadCallback Read = nullptr;
	WriteCallback Write = nullptr;
};

/**
 * Shared direct-endpoint publisher and compatibility token executor for
 * AOT-generated, strongly typed record classes. Concrete generated classes
 * keep their fields inline and publish only one sorted static property/thunk
 * span through this base.
 */
class CompiledBindingRecord : public IBindingSource
{
public:
	~CompiledBindingRecord() override = default;
	CompiledBindingRecord(const CompiledBindingRecord&) = delete;
	CompiledBindingRecord(CompiledBindingRecord&&) = delete;
	CompiledBindingRecord& operator=(const CompiledBindingRecord&) = delete;
	CompiledBindingRecord& operator=(CompiledBindingRecord&&) = delete;
	using IBindingSource::GetValidationIssues;

	/**
	 * Publishes one statically known property as a direct binding endpoint.
	 *
	 * The index addresses the process-lifetime property span supplied to this
	 * record's constructor. Endpoint reads, writes and observation subsequently
	 * call that entry directly: no property token lookup, source-name storage or
	 * endpoint allocation is involved. An out-of-range index returns an empty
	 * handle.
	 */
	[[nodiscard]] CompiledSourceHandle MakeCompiledPropertySource(
		size_t propertyIndex) noexcept;

	PropertyChangedEvent& PropertyChanged() override
	{
		return _propertyChanged;
	}
	bool TryGetValue(
		BindingSourcePropertyToken property,
		BindingValue& out) const override;
	bool TrySetValue(
		BindingSourcePropertyToken property,
		const BindingValue& value) override;
	bool TryGetPropertyMetadata(
		BindingSourcePropertyToken property,
		BindingSourcePropertyMetadata& out) const override;

#if CUI_ENABLE_DYNAMIC_XAML
	// A generated Production record has no reversible source names. These
	// overrides only keep the class concrete in the Design flavor; live XAML
	// continues to use ObservableObject and the Design adapter.
	bool TryGetValue(
		const std::wstring& propertyName,
		BindingValue& out) const override;
	bool TrySetValue(
		const std::wstring& propertyName,
		const BindingValue& value) override;
	bool TryGetPropertyMetadata(
		const std::wstring& propertyName,
		BindingSourcePropertyMetadata& out) const override;
	std::vector<BindingSourcePropertyMetadata> GetProperties() const override;
	std::vector<BindingValidationIssue> GetValidationIssues(
		const std::wstring& propertyName) const override;
#endif

protected:
	explicit CompiledBindingRecord(
		std::span<const CompiledBindingRecordProperty> properties);

	[[nodiscard]] std::span<const CompiledBindingRecordProperty>
		CompiledProperties() const noexcept
	{
		return _properties;
	}

private:
	[[nodiscard]] CompiledSourceHandle MakeCompiledPropertySource(
		const CompiledBindingRecordProperty& property) noexcept;
	[[nodiscard]] const CompiledBindingRecordProperty* FindProperty(
		BindingSourcePropertyToken property) const noexcept;
	bool TrySetCompiledProperty(
		const CompiledBindingRecordProperty& property,
		const BindingValue& value);

	std::span<const CompiledBindingRecordProperty> _properties;
	PropertyChangedEvent _propertyChanged;
};

namespace cui::binding
{
	/**
	 * Resolves a generated record to its exact table entry once. A source with
	 * the same public DataType contract but a different native implementation
	 * remains valid through the explicit token adapter endpoint.
	 */
	template<typename TRecord>
		requires std::is_base_of_v<CompiledBindingRecord, TRecord>
	[[nodiscard]] CompiledSourceHandle MakeCompiledRecordPropertySource(
		IBindingSource& source,
		size_t propertyIndex,
		const CompiledBindingPathStep& property) noexcept
	{
		if (auto* record = dynamic_cast<TRecord*>(&source))
		{
			auto endpoint = record->MakeCompiledPropertySource(propertyIndex);
			if (endpoint) return endpoint;
		}
		return MakeCompiledBindingSourcePropertyAdapter(source, property);
	}
}
