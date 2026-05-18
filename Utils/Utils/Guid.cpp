#include "Guid.h"
#include <objbase.h>
#include <rpc.h>
#pragma comment(lib, "Rpcrt4.lib")
Guid::Guid() {
	Data = { 0 };
}
Guid::Guid(GUID guid) {
	Data = guid;
}
Guid::Guid(std::string str) {
	Data = Parse(str).Data;
}
Guid Guid::NewGuid() {
	GUID guid;
	CoCreateGuid(&guid);
	return Guid(guid);
}
Guid Guid::Parse(const std::string& input) {
	GUID guid;
	UuidFromStringA((RPC_CSTR)input.c_str(), &guid);
	return Guid(guid);
}
std::string Guid::ToString() {
	RPC_CSTR str;
	UuidToStringA(&Data, &str);
	std::string result((char*)str);
	RpcStringFreeA(&str);
	return result;
}
bool Guid::operator==(const Guid& other) {
	return Data == other.Data;
}
bool Guid::operator!=(const Guid& other) {
	return Data != other.Data;
}