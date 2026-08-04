#pragma once

#include <Binding.h>

#include <memory>
#include <string>
#include <vector>

namespace CuiAotFixture::Converters
{
/** Header-only fixture: generated Production code must call this symbol directly. */
inline std::shared_ptr<const IBindingValueConverter> CreatePrefixConverter()
{
	return std::make_shared<DelegateBindingValueConverter>(
		[](const BindingValue& value,
			const BindingValueConverterContext&,
			BindingValue& output)
		{
			std::wstring text;
			if (!value.TryGetString(text)) return false;
			output = BindingValue(L"typed-single: " + text);
			return true;
		});
}

/** Header-only fixture for the distinct IMultiBindingValueConverter ABI. */
inline std::shared_ptr<const IMultiBindingValueConverter> CreateJoinConverter()
{
	return std::make_shared<DelegateMultiBindingValueConverter>(
		[](const std::vector<BindingValue>& values,
			const MultiBindingValueConverterContext&,
			BindingValue& output)
		{
			if (values.size() < 2) return false;
			output = BindingValue(
				values[0].ToString() + L" | " + values[1].ToString());
			return true;
		});
}
}
