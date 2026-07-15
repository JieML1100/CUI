#include "DataPack.h"

#include <exception>
#include <limits>
#include <new>
#include <utility>

enum class DataPackKey : uint8_t {
	FileStart = 0x81,
	FileEnd = 0x98,
	IdStart = 0xC7,
	IdEnd = 0xC8,
	ValueStart = 0x55,
	ValueEnd = 0x56,
	ValueStart_Small = 0x57,
	ValueStart_Small_X = 0x58,
	ChildStart = 0xD4,
	ChildEnd = 0xD5,
	ChildStart_Small = 0xD6,
	ChildStart_Small_X = 0xD7,
};

namespace {
constexpr uint8_t DataPackEnvelopeMagic[4] = { 'D', 'P', 'K', '2' };
constexpr size_t DataPackEnvelopeHeaderSize = 10;
constexpr uint8_t DataPackEnvelopeSupportedFlags = 0x00;

using SizeList = std::vector<size_t>;

static DataPackParseResult parse_ok() {
	return {};
}

static DataPackParseResult parse_fail(DataPackParseStatus status, size_t offset, const char* message) {
	DataPackParseResult result;
	result.Status = status;
	result.Offset = offset;
	if (message != nullptr)
		result.Message = message;
	return result;
}

static bool can_read(size_t offset, size_t count, size_t limit) {
	return offset <= limit && count <= limit - offset;
}

static bool checked_add(size_t& value, size_t add) {
	if (add > std::numeric_limits<size_t>::max() - value)
		return false;
	value += add;
	return true;
}

static size_t section_overhead(size_t len) {
	if (len > UINT16_MAX)
		return 6;
	if (len > UINT8_MAX)
		return 4;
	return 3;
}

static bool checked_add_section(size_t& total, size_t payloadLen) {
	const size_t overhead = section_overhead(payloadLen);
	return checked_add(total, overhead) && checked_add(total, payloadLen);
}

static bool can_read_payload_with_terminator(size_t offset, size_t payloadLen, size_t limit) {
	return offset < limit && payloadLen <= limit - offset - 1;
}

static inline bool read_u16_le(const uint8_t* data, size_t data_len, size_t offset, uint16_t& out) {
	if (!can_read(offset, sizeof(uint16_t), data_len))
		return false;
	out = (uint16_t)data[offset] | (uint16_t)((uint16_t)data[offset + 1] << 8);
	return true;
}

static inline bool read_u32_le(const uint8_t* data, size_t data_len, size_t offset, uint32_t& out) {
	if (!can_read(offset, sizeof(uint32_t), data_len))
		return false;
	out = (uint32_t)data[offset] |
		((uint32_t)data[offset + 1] << 8) |
		((uint32_t)data[offset + 2] << 16) |
		((uint32_t)data[offset + 3] << 24);
	return true;
}

static inline void append_u8(std::vector<uint8_t>& out, uint8_t v) {
	out.push_back(v);
}

static inline void append_bytes(std::vector<uint8_t>& out, const void* data, size_t len) {
	if (len == 0 || data == nullptr)
		return;
	const size_t oldSize = out.size();
	if (len > std::numeric_limits<size_t>::max() - oldSize)
		return;
	out.resize(oldSize + len);
	std::memcpy(out.data() + oldSize, data, len);
}

static inline void append_u16_le(std::vector<uint8_t>& out, uint16_t v) {
	const size_t oldSize = out.size();
	out.resize(oldSize + sizeof(uint16_t));
	uint8_t* p = out.data() + oldSize;
	p[0] = (uint8_t)(v & 0xFFu);
	p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static inline void append_u32_le(std::vector<uint8_t>& out, uint32_t v) {
	const size_t oldSize = out.size();
	out.resize(oldSize + sizeof(uint32_t));
	uint8_t* p = out.data() + oldSize;
	p[0] = (uint8_t)(v & 0xFFu);
	p[1] = (uint8_t)((v >> 8) & 0xFFu);
	p[2] = (uint8_t)((v >> 16) & 0xFFu);
	p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

static bool has_envelope_magic(const uint8_t* data, size_t data_len) {
	return data_len >= 4 && std::memcmp(data, DataPackEnvelopeMagic, 4) == 0;
}

static bool has_datapack_magic_prefix(const uint8_t* data, size_t data_len) {
	return data_len >= 3 && data[0] == 'D' && data[1] == 'P' && data[2] == 'K';
}

static size_t compute_sizes(const DataPack& pack, SizeList& sizes) {
	const size_t myIndex = sizes.size();
	sizes.push_back(0);

	size_t total = 6;
	if (!pack.Id.empty()) {
		if (pack.Id.size() > UINT16_MAX)
			return 0;
		if (!checked_add(total, 1 + 2 + pack.Id.size() + 1))
			return 0;
	}

	if (!pack.Value.empty()) {
		const size_t valueLen = pack.Value.size();
		if (valueLen > UINT32_MAX)
			return 0;
		if (!checked_add_section(total, valueLen))
			return 0;
	}

	for (const auto& sub : pack.Child) {
		const size_t childSize = compute_sizes(sub, sizes);
		if (childSize == 0 || childSize > UINT32_MAX)
			return 0;
		if (!checked_add_section(total, childSize))
			return 0;
	}

	if (total > UINT32_MAX)
		return 0;
	sizes[myIndex] = total;
	return total;
}

static void append_value_header(std::vector<uint8_t>& out, size_t len) {
	if (len > UINT16_MAX) {
		append_u8(out, (uint8_t)DataPackKey::ValueStart);
		append_u32_le(out, (uint32_t)len);
	}
	else if (len > UINT8_MAX) {
		append_u8(out, (uint8_t)DataPackKey::ValueStart_Small);
		append_u16_le(out, (uint16_t)len);
	}
	else {
		append_u8(out, (uint8_t)DataPackKey::ValueStart_Small_X);
		append_u8(out, (uint8_t)len);
	}
}

static void append_child_header(std::vector<uint8_t>& out, size_t len) {
	if (len > UINT16_MAX) {
		append_u8(out, (uint8_t)DataPackKey::ChildStart);
		append_u32_le(out, (uint32_t)len);
	}
	else if (len > UINT8_MAX) {
		append_u8(out, (uint8_t)DataPackKey::ChildStart_Small);
		append_u16_le(out, (uint16_t)len);
	}
	else {
		append_u8(out, (uint8_t)DataPackKey::ChildStart_Small_X);
		append_u8(out, (uint8_t)len);
	}
}

static void write_to_sized(const DataPack& pack, std::vector<uint8_t>& out, const SizeList& sizes, size_t& cursor) {
	const size_t packSize = sizes[cursor++];
	append_u8(out, (uint8_t)DataPackKey::FileStart);
	append_u32_le(out, (uint32_t)packSize);

	if (!pack.Id.empty()) {
		append_u8(out, (uint8_t)DataPackKey::IdStart);
		append_u16_le(out, (uint16_t)pack.Id.size());
		append_bytes(out, pack.Id.data(), pack.Id.size());
		append_u8(out, (uint8_t)DataPackKey::IdEnd);
	}

	if (!pack.Value.empty()) {
		append_value_header(out, pack.Value.size());
		append_bytes(out, pack.Value.data(), pack.Value.size());
		append_u8(out, (uint8_t)DataPackKey::ValueEnd);
	}

	for (const auto& sub : pack.Child) {
		const size_t childLen = sizes[cursor];
		append_child_header(out, childLen);
		write_to_sized(sub, out, sizes, cursor);
		append_u8(out, (uint8_t)DataPackKey::ChildEnd);
	}

	append_u8(out, (uint8_t)DataPackKey::FileEnd);
}

static DataPackParseResult read_value_length(
	const uint8_t* data,
	size_t bufferSize,
	size_t& index,
	DataPackKey key,
	size_t baseOffset,
	size_t& valueLen) {
	switch (key) {
	case DataPackKey::ValueStart: {
		uint32_t valueLenU32 = 0;
		if (!read_u32_le(data, bufferSize, index, valueLenU32))
			return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Value length is truncated.");
		index += sizeof(uint32_t);
		valueLen = (size_t)valueLenU32;
		return parse_ok();
	}
	case DataPackKey::ValueStart_Small: {
		uint16_t valueLenU16 = 0;
		if (!read_u16_le(data, bufferSize, index, valueLenU16))
			return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Value length is truncated.");
		index += sizeof(uint16_t);
		valueLen = (size_t)valueLenU16;
		return parse_ok();
	}
	case DataPackKey::ValueStart_Small_X:
		if (!can_read(index, sizeof(uint8_t), bufferSize))
			return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Value length is truncated.");
		valueLen = (size_t)data[index++];
		return parse_ok();
	default:
		return parse_fail(DataPackParseStatus::InvalidMarker, baseOffset + index, "Invalid value marker.");
	}
}

static DataPackParseResult read_child_length(
	const uint8_t* data,
	size_t bufferSize,
	size_t& index,
	DataPackKey key,
	size_t baseOffset,
	size_t& childLen) {
	switch (key) {
	case DataPackKey::ChildStart: {
		uint32_t childLenU32 = 0;
		if (!read_u32_le(data, bufferSize, index, childLenU32))
			return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Child length is truncated.");
		index += sizeof(uint32_t);
		childLen = (size_t)childLenU32;
		return parse_ok();
	}
	case DataPackKey::ChildStart_Small: {
		uint16_t childLenU16 = 0;
		if (!read_u16_le(data, bufferSize, index, childLenU16))
			return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Child length is truncated.");
		index += sizeof(uint16_t);
		childLen = (size_t)childLenU16;
		return parse_ok();
	}
	case DataPackKey::ChildStart_Small_X:
		if (!can_read(index, sizeof(uint8_t), bufferSize))
			return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Child length is truncated.");
		childLen = (size_t)data[index++];
		return parse_ok();
	default:
		return parse_fail(DataPackParseStatus::InvalidMarker, baseOffset + index, "Invalid child marker.");
	}
}

static DataPackParseResult parse_legacy_node(
	const uint8_t* data,
	size_t data_len,
	DataPack& out,
	const DataPackParseOptions& options,
	size_t depth,
	size_t baseOffset,
	bool requireFullBuffer) {
	if (data == nullptr)
		return parse_fail(DataPackParseStatus::NullInput, baseOffset, "Data pointer is null.");
	if (options.MaxDepth > 0 && depth > options.MaxDepth)
		return parse_fail(DataPackParseStatus::DepthLimitExceeded, baseOffset, "DataPack nesting is deeper than allowed.");
	if (data_len < 6)
		return parse_fail(DataPackParseStatus::TooSmall, baseOffset, "DataPack node is too small.");
	if (options.MaxBytes > 0 && data_len > options.MaxBytes)
		return parse_fail(DataPackParseStatus::SizeLimitExceeded, baseOffset, "DataPack input is larger than allowed.");
	if (data[0] != (uint8_t)DataPackKey::FileStart)
		return parse_fail(DataPackParseStatus::InvalidMarker, baseOffset, "Missing DataPack node start marker.");

	uint32_t bufferSizeU32 = 0;
	if (!read_u32_le(data, data_len, 1, bufferSizeU32))
		return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + 1, "DataPack node length is truncated.");

	const size_t bufferSize = (size_t)bufferSizeU32;
	if (bufferSize < 6 || bufferSize > data_len)
		return parse_fail(DataPackParseStatus::SizeMismatch, baseOffset + 1, "DataPack node length does not match the available buffer.");
	if (requireFullBuffer && bufferSize != data_len)
		return parse_fail(DataPackParseStatus::SizeMismatch, baseOffset + bufferSize, "DataPack node has trailing bytes.");
	if (options.MaxBytes > 0 && bufferSize > options.MaxBytes)
		return parse_fail(DataPackParseStatus::SizeLimitExceeded, baseOffset + 1, "DataPack node length is larger than allowed.");
	if (data[bufferSize - 1] != (uint8_t)DataPackKey::FileEnd)
		return parse_fail(DataPackParseStatus::InvalidTerminator, baseOffset + bufferSize - 1, "Missing DataPack node end marker.");

	DataPack parsed;
	size_t index = 5;
	while (index < bufferSize - 1) {
		const DataPackKey key = (DataPackKey)data[index++];
		switch (key) {
		case DataPackKey::IdStart: {
			uint16_t idLen = 0;
			if (!read_u16_le(data, bufferSize, index, idLen))
				return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Id length is truncated.");
			index += sizeof(uint16_t);
			if ((size_t)idLen > options.MaxIdBytes)
				return parse_fail(DataPackParseStatus::IdLimitExceeded, baseOffset + index, "Id length is larger than allowed.");
			if (!can_read_payload_with_terminator(index, (size_t)idLen, bufferSize))
				return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Id payload is truncated.");
			if (data[index + idLen] != (uint8_t)DataPackKey::IdEnd)
				return parse_fail(DataPackParseStatus::InvalidTerminator, baseOffset + index + idLen, "Missing Id end marker.");
			parsed.Id.assign((const char*)&data[index], idLen);
			index += (size_t)idLen + 1;
			break;
		}
		case DataPackKey::ValueStart:
		case DataPackKey::ValueStart_Small:
		case DataPackKey::ValueStart_Small_X: {
			size_t valueLen = 0;
			DataPackParseResult lenResult = read_value_length(data, bufferSize, index, key, baseOffset, valueLen);
			if (!lenResult)
				return lenResult;
			if (options.MaxValueBytes > 0 && valueLen > options.MaxValueBytes)
				return parse_fail(DataPackParseStatus::ValueLimitExceeded, baseOffset + index, "Value length is larger than allowed.");
			if (!can_read_payload_with_terminator(index, valueLen, bufferSize))
				return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Value payload is truncated.");
			if (data[index + valueLen] != (uint8_t)DataPackKey::ValueEnd)
				return parse_fail(DataPackParseStatus::InvalidTerminator, baseOffset + index + valueLen, "Missing Value end marker.");
			parsed.Value.assign(&data[index], &data[index + valueLen]);
			index += valueLen + 1;
			break;
		}
		case DataPackKey::ChildStart:
		case DataPackKey::ChildStart_Small:
		case DataPackKey::ChildStart_Small_X: {
			if (options.MaxChildren > 0 && parsed.Child.size() >= options.MaxChildren)
				return parse_fail(DataPackParseStatus::ChildLimitExceeded, baseOffset + index - 1, "Child count is larger than allowed.");
			size_t childLen = 0;
			DataPackParseResult lenResult = read_child_length(data, bufferSize, index, key, baseOffset, childLen);
			if (!lenResult)
				return lenResult;
			if (childLen < 6)
				return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Child node is too small.");
			if (!can_read_payload_with_terminator(index, childLen, bufferSize))
				return parse_fail(DataPackParseStatus::InvalidLength, baseOffset + index, "Child payload is truncated.");
			if (data[index + childLen] != (uint8_t)DataPackKey::ChildEnd)
				return parse_fail(DataPackParseStatus::InvalidTerminator, baseOffset + index + childLen, "Missing Child end marker.");

			DataPack child;
			DataPackParseResult childResult = parse_legacy_node(data + index, childLen, child, options, depth + 1, baseOffset + index, true);
			if (!childResult)
				return childResult;
			parsed.Child.push_back(std::move(child));
			index += childLen + 1;
			break;
		}
		default:
			return parse_fail(DataPackParseStatus::InvalidMarker, baseOffset + index - 1, "Unknown DataPack marker.");
		}
	}

	out = std::move(parsed);
	return parse_ok();
}
}

DataPack& DataPack::operator[](int index) {
	if (index < 0)
		index = 0;
	const size_t pos = (size_t)index;
	if (pos >= this->Child.size())
		this->Child.resize(pos + 1);
	return this->Child[pos];
}

DataPack& DataPack::operator[](const std::string& id) {
	for (size_t i = 0; i < Child.size(); i++) {
		if (this->Child[i].Id == id)
			return this->Child[i];
	}
	Child.push_back(DataPack(id, 0));
	return this->Child[this->Child.size() - 1];
}

void DataPack::operator=(const std::initializer_list<uint8_t> data) {
	this->Value.resize(data.size());
	if (data.size() > 0)
		std::memcpy(this->Value.data(), data.begin(), data.size());
}

void DataPack::operator=(const std::initializer_list<uint8_t>* data) {
	if (data == nullptr) {
		this->Value.clear();
		return;
	}
	this->Value.resize(data->size());
	if (data->size() > 0)
		std::memcpy(this->Value.data(), data->begin(), data->size());
}

void DataPack::operator=(const char* data) {
	if (data == nullptr) {
		this->Value.clear();
		return;
	}
	const std::string str = data;
	*this = str;
}

void DataPack::operator=(const wchar_t* data) {
	if (data == nullptr) {
		this->Value.clear();
		return;
	}
	const std::wstring str = data;
	*this = str;
}

void DataPack::operator=(char* data) {
	*this = (const char*)data;
}

void DataPack::operator=(wchar_t* data) {
	*this = (const wchar_t*)data;
}

void DataPack::operator=(std::string data) {
	this->Value.resize(data.size());
	if (!data.empty())
		std::memcpy(this->Value.data(), data.data(), data.size());
}

void DataPack::operator=(std::wstring data) {
	const size_t byteCount = data.size() * sizeof(wchar_t);
	this->Value.resize(byteCount);
	if (!data.empty())
		std::memcpy(this->Value.data(), data.data(), byteCount);
}

void DataPack::RemoveAt(int index) {
	if (index < 0)
		return;
	const size_t pos = (size_t)index;
	if (pos >= this->Child.size())
		return;
	this->Child.erase(this->Child.begin() + pos);
}

DataPack::DataPack() : Id({}), Value({}) {}

DataPack::DataPack(const uint8_t* data, int data_len) {
	if (data_len <= 0)
		return;
	DataPack parsed;
	if (TryParse(data, (size_t)data_len, parsed))
		*this = std::move(parsed);
}

DataPack::DataPack(const uint8_t* data, size_t data_len) {
	DataPack parsed;
	if (TryParse(data, data_len, parsed))
		*this = std::move(parsed);
}

DataPack::DataPack(const char* key) {
	this->Id = key == nullptr ? "" : key;
	this->Value.resize(0);
}

DataPack::DataPack(std::string id, uint8_t* data, int len) : DataPack(std::move(id), (const uint8_t*)data, len > 0 ? (size_t)len : 0) {}

DataPack::DataPack(std::string id, const uint8_t* data, size_t len) {
	this->Id = std::move(id);
	this->Value.resize(data == nullptr ? 0 : len);
	if (data != nullptr && len > 0)
		std::memcpy(this->Value.data(), data, len);
}

DataPack::DataPack(std::vector<uint8_t> data) : DataPack(data.data(), data.size()) {}

DataPack::DataPack(std::initializer_list<uint8_t> data) : DataPack(data.begin(), data.size()) {}

DataPack::DataPack(std::string id, std::string data) {
	this->Id = std::move(id);
	*this = std::move(data);
}

DataPack::DataPack(std::string id, std::wstring data) {
	this->Id = std::move(id);
	*this = std::move(data);
}

DataPack::DataPack(std::string id, char* data) : DataPack(std::move(id), (const char*)data) {}

DataPack::DataPack(std::string id, const char* data) {
	this->Id = std::move(id);
	*this = data;
}

DataPack::DataPack(std::string id, wchar_t* data) : DataPack(std::move(id), (const wchar_t*)data) {}

DataPack::DataPack(std::string id, const wchar_t* data) {
	this->Id = std::move(id);
	*this = data;
}

void DataPack::Add(const DataPack& val) {
	this->Child.push_back(val);
}

bool DataPack::ContainsKey(const std::string& key) const {
	for (const auto& child : this->Child) {
		if (child.Id == key)
			return true;
	}
	return false;
}

void DataPack::clear() {
	this->Child.clear();
}

size_t DataPack::size() const {
	return this->Child.size();
}

void DataPack::resize(size_t value) {
	this->Child.resize(value);
}

void DataPack::WriteTo(std::vector<uint8_t>& out) const {
	SizeList sizes;
	const size_t total = compute_sizes(*this, sizes);
	if (total == 0)
		return;
	if (total > std::numeric_limits<size_t>::max() - out.size())
		return;
	out.reserve(out.size() + total);
	size_t cursor = 0;
	write_to_sized(*this, out, sizes, cursor);
}

std::vector<uint8_t> DataPack::GetBytes() const {
	return GetBytes({});
}

std::vector<uint8_t> DataPack::GetBytes(DataPackWriteOptions options) const {
	std::vector<uint8_t> payload;
	SizeList sizes;
	const size_t total = compute_sizes(*this, sizes);
	if (total == 0)
		return payload;

	payload.reserve(total);
	size_t cursor = 0;
	write_to_sized(*this, payload, sizes, cursor);

	if (options.Version == DataPackWireVersion::Legacy)
		return payload;
	if (options.Version != DataPackWireVersion::Version2)
		return {};
	if (payload.size() > UINT32_MAX)
		return {};

	std::vector<uint8_t> out;
	if (payload.size() > std::numeric_limits<size_t>::max() - DataPackEnvelopeHeaderSize)
		return {};
	out.reserve(DataPackEnvelopeHeaderSize + payload.size());
	append_bytes(out, DataPackEnvelopeMagic, sizeof(DataPackEnvelopeMagic));
	append_u8(out, (uint8_t)DataPackEnvelopeHeaderSize);
	append_u8(out, 0);
	append_u32_le(out, (uint32_t)payload.size());
	append_bytes(out, payload.data(), payload.size());
	return out;
}

DataPackParseResult DataPack::TryParse(const uint8_t* data, size_t data_len, DataPack& out, DataPackParseOptions options) {
	try {
		if (data == nullptr)
			return parse_fail(DataPackParseStatus::NullInput, 0, "Data pointer is null.");
		if (data_len < 6)
			return parse_fail(DataPackParseStatus::TooSmall, 0, "DataPack input is too small.");
		if (options.MaxBytes > 0 && data_len > options.MaxBytes)
			return parse_fail(DataPackParseStatus::SizeLimitExceeded, 0, "DataPack input is larger than allowed.");

		DataPack parsed;
		DataPackParseResult result;
		if (has_envelope_magic(data, data_len)) {
			if (data_len < DataPackEnvelopeHeaderSize)
				return parse_fail(DataPackParseStatus::TooSmall, 0, "DataPack v2 envelope is too small.");
			const size_t headerSize = (size_t)data[4];
			if (headerSize < DataPackEnvelopeHeaderSize || headerSize > data_len)
				return parse_fail(DataPackParseStatus::InvalidHeader, 4, "DataPack v2 header size is invalid.");

			const uint8_t flags = data[5];
			if ((flags & ~DataPackEnvelopeSupportedFlags) != 0)
				return parse_fail(DataPackParseStatus::UnsupportedFlags, 5, "DataPack v2 envelope contains unsupported flags.");

			uint32_t payloadLenU32 = 0;
			if (!read_u32_le(data, data_len, 6, payloadLenU32))
				return parse_fail(DataPackParseStatus::InvalidLength, 6, "DataPack v2 payload length is truncated.");

			const size_t payloadLen = (size_t)payloadLenU32;
			if (payloadLen > data_len - headerSize)
				return parse_fail(DataPackParseStatus::SizeMismatch, headerSize, "DataPack v2 payload length exceeds the available buffer.");
			if (options.RequireFullBuffer && headerSize + payloadLen != data_len)
				return parse_fail(DataPackParseStatus::SizeMismatch, headerSize + payloadLen, "DataPack v2 envelope has trailing bytes.");
			if (options.MaxBytes > 0 && payloadLen > options.MaxBytes)
				return parse_fail(DataPackParseStatus::SizeLimitExceeded, headerSize, "DataPack v2 payload is larger than allowed.");

			result = parse_legacy_node(data + headerSize, payloadLen, parsed, options, 0, headerSize, true);
		}
		else {
			if (has_datapack_magic_prefix(data, data_len))
				return parse_fail(DataPackParseStatus::UnsupportedVersion, 3, "Unsupported DataPack envelope version.");
			result = parse_legacy_node(data, data_len, parsed, options, 0, 0, options.RequireFullBuffer);
		}

		if (!result)
			return result;
		out = std::move(parsed);
		return parse_ok();
	}
	catch (const std::bad_alloc&) {
		return { DataPackParseStatus::AllocationFailed, 0, "" };
	}
	catch (const std::exception& ex) {
		return { DataPackParseStatus::Exception, 0, ex.what() };
	}
	catch (...) {
		return { DataPackParseStatus::Exception, 0, "" };
	}
}

bool DataPack::Validate(const uint8_t* data, size_t data_len, DataPackParseOptions options) {
	DataPack parsed;
	return TryParse(data, data_len, parsed, options).Success();
}
