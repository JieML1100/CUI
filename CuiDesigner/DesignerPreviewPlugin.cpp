#include "DesignerPreviewPlugin.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <limits>

namespace
{
	constexpr float MaximumCoordinate = 1000000.0f;
	constexpr size_t MinimumPluginSize =
		offsetof(CuiDesignerPreviewPluginV1, Shutdown)
		+ sizeof(((CuiDesignerPreviewPluginV1*)nullptr)->Shutdown);
	constexpr size_t MinimumFrameSize =
		offsetof(CuiDesignerPreviewFrameV1, PrimitiveCount)
		+ sizeof(((CuiDesignerPreviewFrameV1*)nullptr)->PrimitiveCount);
	constexpr size_t MinimumPrimitiveSize =
		offsetof(CuiDesignerPreviewPrimitiveV1, FontSize)
		+ sizeof(((CuiDesignerPreviewPrimitiveV1*)nullptr)->FontSize);

	bool Fail(std::wstring message, std::wstring* outError)
	{
		if (outError) *outError = std::move(message);
		return false;
	}

	bool TryUtf8ToWide(
		const char* value,
		size_t size,
		std::wstring& output)
	{
		output.clear();
		if (size == 0) return true;
		if (!value || size > static_cast<size_t>((std::numeric_limits<int>::max)()))
			return false;
		const int count = ::MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value, static_cast<int>(size),
			nullptr, 0);
		if (count <= 0) return false;
		output.resize(static_cast<size_t>(count));
		return ::MultiByteToWideChar(
			CP_UTF8, MB_ERR_INVALID_CHARS, value, static_cast<int>(size),
			output.data(), count) == count;
	}

	bool TryWideToUtf8(const std::wstring& value, std::string& output)
	{
		output.clear();
		if (value.empty()) return true;
		if (value.size() > static_cast<size_t>((std::numeric_limits<int>::max)()))
			return false;
		const int count = ::WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
		if (count <= 0) return false;
		output.resize(static_cast<size_t>(count));
		return ::WideCharToMultiByte(
			CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
			static_cast<int>(value.size()), output.data(), count,
			nullptr, nullptr) == count;
	}

	bool FiniteBounded(float value) noexcept
	{
		return std::isfinite(value)
			&& value >= -MaximumCoordinate && value <= MaximumCoordinate;
	}

	std::wstring StatusMessage(
		const wchar_t* operation,
		CuiDesignerPreviewStatusV1 status)
	{
		return std::wstring(operation) + L"失败，插件状态码 "
			+ std::to_wstring(status) + L"。";
	}

	bool IsCurrentThread(const DesignerPreviewPluginState& state) noexcept;
}

struct DesignerPreviewPluginState
{
	HMODULE Module = nullptr;
	const CuiDesignerPreviewPluginV1* Plugin = nullptr;
	CuiDesignerPreviewHostV1 Host{};
	DesignerPreviewPluginModule::LogHandler LogHandler;
	std::wstring CanonicalPath;
	DWORD ThreadId = 0;

	~DesignerPreviewPluginState()
	{
		if (Plugin && Plugin->Shutdown)
		{
			try { Plugin->Shutdown(Plugin->PluginContext); }
			catch (...) {}
		}
		if (Module) (void)::FreeLibrary(Module);
	}

	static void CUI_DESIGNER_PREVIEW_CALL LogThunk(
		void* context,
		uint32_t level,
		CuiDesignerUtf8ViewV1 message) noexcept
	{
		try
		{
			auto* state = static_cast<DesignerPreviewPluginState*>(context);
			if (!state || !state->LogHandler
				|| level > CUI_DESIGNER_PREVIEW_LOG_ERROR_V1
				|| message.Size > CUI_DESIGNER_PREVIEW_MAX_UTF8_BYTES_V1)
				return;
			std::wstring text;
			if (!TryUtf8ToWide(message.Data, message.Size, text)) return;
			state->LogHandler(
				static_cast<CuiDesignerPreviewLogLevelV1>(level), text);
		}
		catch (...) {}
	}
};

namespace
{
	bool IsCurrentThread(const DesignerPreviewPluginState& state) noexcept
	{
		return state.ThreadId == ::GetCurrentThreadId();
	}
}

bool DesignerPreviewPluginContract::ValidatePluginTable(
	const CuiDesignerPreviewPluginV1* plugin,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (!plugin) return Fail(L"插件没有返回函数表。", outError);
	if (plugin->StructSize < MinimumPluginSize)
		return Fail(L"插件函数表结构过小。", outError);
	if (plugin->AbiVersion != CUI_DESIGNER_PREVIEW_ABI_V1)
		return Fail(L"插件 ABI 版本不兼容。", outError);
	if (!plugin->CreateSession || !plugin->DestroySession
		|| !plugin->SetValue || !plugin->Render || !plugin->Shutdown)
		return Fail(L"插件函数表缺少 V1 必需入口。", outError);
	return true;
}

bool DesignerPreviewPluginContract::CopyFrame(
	const CuiDesignerPreviewFrameV1& frame,
	std::vector<DesignerPreviewPrimitive>& output,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (frame.StructSize < MinimumFrameSize)
		return Fail(L"插件帧结构过小。", outError);
	if (frame.PrimitiveCount > CUI_DESIGNER_PREVIEW_MAX_PRIMITIVES_V1)
		return Fail(L"插件帧绘制原语超过 4096 项限制。", outError);
	if (frame.PrimitiveCount != 0 && !frame.Primitives)
		return Fail(L"插件帧缺少绘制原语缓冲区。", outError);

	std::vector<DesignerPreviewPrimitive> candidate;
	candidate.reserve(frame.PrimitiveCount);
	size_t utf8Bytes = 0;
	for (uint32_t index = 0; index < frame.PrimitiveCount; ++index)
	{
		const auto& source = frame.Primitives[index];
		if (source.StructSize < MinimumPrimitiveSize)
			return Fail(L"插件绘制原语结构过小。", outError);
		if (source.Kind < CUI_DESIGNER_PREVIEW_PRIMITIVE_RECT_V1
			|| source.Kind > CUI_DESIGNER_PREVIEW_PRIMITIVE_TEXT_V1)
			return Fail(L"插件绘制原语类型无效。", outError);
		if (!FiniteBounded(source.X) || !FiniteBounded(source.Y)
			|| !FiniteBounded(source.Width) || !FiniteBounded(source.Height)
			|| !FiniteBounded(source.RadiusX) || !FiniteBounded(source.RadiusY)
			|| !FiniteBounded(source.StrokeWidth) || !FiniteBounded(source.FontSize)
			|| source.RadiusX < 0.0f || source.RadiusY < 0.0f
			|| source.StrokeWidth < 0.0f)
			return Fail(L"插件绘制原语包含无效或越界几何值。", outError);
		if (source.Kind != CUI_DESIGNER_PREVIEW_PRIMITIVE_LINE_V1
			&& (source.Width < 0.0f || source.Height < 0.0f))
			return Fail(L"插件区域原语不能使用负尺寸。", outError);
		if (source.Kind == CUI_DESIGNER_PREVIEW_PRIMITIVE_TEXT_V1
			&& source.FontSize <= 0.0f)
			return Fail(L"插件文本原语字号必须为正数。", outError);
		if (source.Text.Size > CUI_DESIGNER_PREVIEW_MAX_UTF8_BYTES_V1
			|| utf8Bytes > CUI_DESIGNER_PREVIEW_MAX_UTF8_BYTES_V1
				- source.Text.Size)
			return Fail(L"插件帧 UTF-8 文本超过 1 MiB 限制。", outError);
		utf8Bytes += source.Text.Size;

		DesignerPreviewPrimitive target;
		target.Kind = static_cast<CuiDesignerPreviewPrimitiveKindV1>(source.Kind);
		target.X = source.X;
		target.Y = source.Y;
		target.Width = source.Width;
		target.Height = source.Height;
		target.RadiusX = source.RadiusX;
		target.RadiusY = source.RadiusY;
		target.StrokeWidth = source.StrokeWidth;
		target.FillArgb32 = source.FillArgb32;
		target.StrokeArgb32 = source.StrokeArgb32;
		target.FontSize = source.FontSize;
		if (!TryUtf8ToWide(source.Text.Data, source.Text.Size, target.Text))
			return Fail(L"插件帧包含无效 UTF-8 文本。", outError);
		candidate.push_back(std::move(target));
	}
	output = std::move(candidate);
	return true;
}

DesignerPreviewPluginSession::DesignerPreviewPluginSession(
	std::shared_ptr<DesignerPreviewPluginState> state,
	void* session) noexcept
	: _state(std::move(state)), _session(session)
{
}

DesignerPreviewPluginSession::~DesignerPreviewPluginSession()
{
	Reset();
}

DesignerPreviewPluginSession::DesignerPreviewPluginSession(
	DesignerPreviewPluginSession&& other) noexcept
	: _state(std::move(other._state)), _session(other._session)
{
	other._session = nullptr;
}

DesignerPreviewPluginSession& DesignerPreviewPluginSession::operator=(
	DesignerPreviewPluginSession&& other) noexcept
{
	if (this == &other) return *this;
	Reset();
	_state = std::move(other._state);
	_session = other._session;
	other._session = nullptr;
	return *this;
}

void DesignerPreviewPluginSession::Reset() noexcept
{
	if (_session && _state && _state->Plugin
		&& _state->Plugin->DestroySession)
	{
		try
		{
			_state->Plugin->DestroySession(
				_state->Plugin->PluginContext, _session);
		}
		catch (...) {}
	}
	_session = nullptr;
	_state.reset();
}

bool DesignerPreviewPluginSession::SetValue(
	const std::wstring& propertyName,
	const CuiDesignerPreviewValueV1& value,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (!_state || !_session || !_state->Plugin)
		return Fail(L"预览 session 未初始化。", outError);
	if (!IsCurrentThread(*_state))
		return Fail(L"预览插件只能在加载它的 UI 线程调用。", outError);
	std::string name;
	if (propertyName.empty() || !TryWideToUtf8(propertyName, name))
		return Fail(L"预览属性名称无效。", outError);
	CuiDesignerPreviewStatusV1 status = CUI_DESIGNER_PREVIEW_FAILED_V1;
	try
	{
		status = _state->Plugin->SetValue(
			_state->Plugin->PluginContext, _session,
			{ name.data(), static_cast<uint32_t>(name.size()) }, &value);
	}
	catch (...)
	{
		return Fail(L"预览插件 SetValue 越过 C ABI 抛出了异常。", outError);
	}
	return status == CUI_DESIGNER_PREVIEW_OK_V1
		|| Fail(StatusMessage(L"设置预览属性", status), outError);
}

bool DesignerPreviewPluginSession::SetBool(
	const std::wstring& propertyName, bool value, std::wstring* outError)
{
	CuiDesignerPreviewValueV1 input{};
	input.StructSize = sizeof(input);
	input.Kind = CUI_DESIGNER_PREVIEW_VALUE_BOOL_V1;
	input.Data.BoolValue = value ? 1u : 0u;
	return SetValue(propertyName, input, outError);
}

bool DesignerPreviewPluginSession::SetInt64(
	const std::wstring& propertyName, int64_t value, std::wstring* outError)
{
	CuiDesignerPreviewValueV1 input{};
	input.StructSize = sizeof(input);
	input.Kind = CUI_DESIGNER_PREVIEW_VALUE_INT64_V1;
	input.Data.Int64Value = value;
	return SetValue(propertyName, input, outError);
}

bool DesignerPreviewPluginSession::SetDouble(
	const std::wstring& propertyName, double value, std::wstring* outError)
{
	CuiDesignerPreviewValueV1 input{};
	input.StructSize = sizeof(input);
	input.Kind = CUI_DESIGNER_PREVIEW_VALUE_DOUBLE_V1;
	input.Data.DoubleValue = value;
	return SetValue(propertyName, input, outError);
}

bool DesignerPreviewPluginSession::SetArgb32(
	const std::wstring& propertyName, uint32_t value, std::wstring* outError)
{
	CuiDesignerPreviewValueV1 input{};
	input.StructSize = sizeof(input);
	input.Kind = CUI_DESIGNER_PREVIEW_VALUE_ARGB32_V1;
	input.Data.Argb32Value = value;
	return SetValue(propertyName, input, outError);
}

bool DesignerPreviewPluginSession::SetUtf8(
	const std::wstring& propertyName,
	const std::string& value,
	std::wstring* outError)
{
	if (value.size() > CUI_DESIGNER_PREVIEW_MAX_UTF8_BYTES_V1)
		return Fail(L"预览属性 UTF-8 文本超过 1 MiB 限制。", outError);
	std::wstring validation;
	if (!TryUtf8ToWide(value.data(), value.size(), validation))
		return Fail(L"预览属性包含无效 UTF-8。", outError);
	CuiDesignerPreviewValueV1 input{};
	input.StructSize = sizeof(input);
	input.Kind = CUI_DESIGNER_PREVIEW_VALUE_UTF8_V1;
	input.Data.Utf8Value = {
		value.data(), static_cast<uint32_t>(value.size()) };
	return SetValue(propertyName, input, outError);
}

bool DesignerPreviewPluginSession::Render(
	const CuiDesignerPreviewFrameInputV1& input,
	std::vector<DesignerPreviewPrimitive>& output,
	std::wstring* outError)
{
	if (outError) outError->clear();
	if (!_state || !_session || !_state->Plugin)
		return Fail(L"预览 session 未初始化。", outError);
	if (!IsCurrentThread(*_state))
		return Fail(L"预览插件只能在加载它的 UI 线程调用。", outError);
	if (input.StructSize < sizeof(CuiDesignerPreviewFrameInputV1)
		|| !std::isfinite(input.Width) || input.Width < 0.0f
		|| !std::isfinite(input.Height) || input.Height < 0.0f
		|| !std::isfinite(input.DpiScale) || input.DpiScale <= 0.0f)
		return Fail(L"预览帧输入无效。", outError);

	CuiDesignerPreviewFrameV1 frame{};
	frame.StructSize = sizeof(frame);
	CuiDesignerPreviewStatusV1 status = CUI_DESIGNER_PREVIEW_FAILED_V1;
	try
	{
		status = _state->Plugin->Render(
			_state->Plugin->PluginContext, _session, &input, &frame);
	}
	catch (...)
	{
		return Fail(L"预览插件 Render 越过 C ABI 抛出了异常。", outError);
	}
	if (status != CUI_DESIGNER_PREVIEW_OK_V1)
		return Fail(StatusMessage(L"生成预览帧", status), outError);
	return DesignerPreviewPluginContract::CopyFrame(frame, output, outError);
}

bool DesignerPreviewPluginModule::Load(
	const std::wstring& absolutePath,
	LogHandler logHandler,
	std::wstring* outError)
{
	if (outError) outError->clear();
	try
	{
		const std::filesystem::path input(absolutePath);
		if (!input.is_absolute())
			return Fail(L"预览插件必须使用显式绝对路径。", outError);
		std::error_code error;
		const auto canonical = std::filesystem::canonical(input, error);
		if (error || !std::filesystem::is_regular_file(canonical, error))
			return Fail(L"预览插件路径不存在或不是普通文件。", outError);
		if (_wcsicmp(canonical.extension().c_str(), L".dll") != 0)
			return Fail(L"预览插件必须是 .dll 文件。", outError);

		auto candidate = std::make_shared<DesignerPreviewPluginState>();
		candidate->CanonicalPath = canonical.wstring();
		candidate->ThreadId = ::GetCurrentThreadId();
		candidate->LogHandler = std::move(logHandler);
		candidate->Host.StructSize = sizeof(candidate->Host);
		candidate->Host.AbiVersion = CUI_DESIGNER_PREVIEW_ABI_V1;
		candidate->Host.HostContext = candidate.get();
		candidate->Host.Log = &DesignerPreviewPluginState::LogThunk;
		candidate->Module = ::LoadLibraryExW(
			candidate->CanonicalPath.c_str(), nullptr,
			LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
				| LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (!candidate->Module)
			return Fail(L"无法加载预览插件，Win32 错误 "
				+ std::to_wstring(::GetLastError()) + L"。", outError);
		const auto entry = reinterpret_cast<CuiDesignerGetPreviewPluginFnV1>(
			::GetProcAddress(candidate->Module, "CuiDesignerGetPreviewPluginV1"));
		if (!entry)
			return Fail(L"预览插件缺少 CuiDesignerGetPreviewPluginV1 导出。",
				outError);

		const CuiDesignerPreviewPluginV1* table = nullptr;
		CuiDesignerPreviewStatusV1 status = CUI_DESIGNER_PREVIEW_FAILED_V1;
		try { status = entry(&candidate->Host, &table); }
		catch (...)
		{
			return Fail(L"预览插件入口越过 C ABI 抛出了异常。", outError);
		}
		if (status != CUI_DESIGNER_PREVIEW_OK_V1)
			return Fail(StatusMessage(L"协商预览插件 ABI", status), outError);
		if (!DesignerPreviewPluginContract::ValidatePluginTable(table, outError))
			return false;
		candidate->Plugin = table;
		_state = std::move(candidate);
		return true;
	}
	catch (...)
	{
		return Fail(L"规范化或加载预览插件路径时失败。", outError);
	}
}

const std::wstring& DesignerPreviewPluginModule::Path() const noexcept
{
	static const std::wstring empty;
	return _state ? _state->CanonicalPath : empty;
}

bool DesignerPreviewPluginModule::CreateSession(
	const std::wstring& xamlNamespace,
	const std::wstring& xamlName,
	DesignerPreviewPluginSession& output,
	std::wstring* outError) const
{
	if (outError) outError->clear();
	if (!_state || !_state->Plugin)
		return Fail(L"预览插件尚未加载。", outError);
	if (!IsCurrentThread(*_state))
		return Fail(L"预览插件只能在加载它的 UI 线程调用。", outError);
	std::string ns;
	std::string name;
	if (xamlNamespace.empty() || xamlName.empty()
		|| !TryWideToUtf8(xamlNamespace, ns) || !TryWideToUtf8(xamlName, name))
		return Fail(L"预览控件 XAML identity 无效。", outError);

	void* session = nullptr;
	CuiDesignerPreviewStatusV1 status = CUI_DESIGNER_PREVIEW_FAILED_V1;
	try
	{
		status = _state->Plugin->CreateSession(
			_state->Plugin->PluginContext,
			{ ns.data(), static_cast<uint32_t>(ns.size()) },
			{ name.data(), static_cast<uint32_t>(name.size()) },
			&session);
	}
	catch (...)
	{
		return Fail(L"预览插件 CreateSession 越过 C ABI 抛出了异常。", outError);
	}
	if (status != CUI_DESIGNER_PREVIEW_OK_V1 || !session)
	{
		if (session)
		{
			try
			{
				_state->Plugin->DestroySession(
					_state->Plugin->PluginContext, session);
			}
			catch (...) {}
		}
		return Fail(StatusMessage(L"创建预览 session", status), outError);
	}
	DesignerPreviewPluginSession candidate(_state, session);
	output = std::move(candidate);
	return true;
}
