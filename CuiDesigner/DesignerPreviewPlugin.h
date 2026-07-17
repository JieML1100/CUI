#pragma once

#include "DesignerPreviewPluginAbi.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct DesignerPreviewPrimitive
{
	CuiDesignerPreviewPrimitiveKindV1 Kind =
		CUI_DESIGNER_PREVIEW_PRIMITIVE_RECT_V1;
	float X = 0.0f;
	float Y = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;
	float RadiusX = 0.0f;
	float RadiusY = 0.0f;
	float StrokeWidth = 0.0f;
	uint32_t FillArgb32 = 0;
	uint32_t StrokeArgb32 = 0;
	std::wstring Text;
	float FontSize = 0.0f;
};

namespace DesignerPreviewPluginContract
{
	bool ValidatePluginTable(
		const CuiDesignerPreviewPluginV1* plugin,
		std::wstring* outError = nullptr);

	/** Validates bounds/UTF-8 and copies all plugin-owned frame memory. */
	bool CopyFrame(
		const CuiDesignerPreviewFrameV1& frame,
		std::vector<DesignerPreviewPrimitive>& output,
		std::wstring* outError = nullptr);
}

struct DesignerPreviewPluginState;

class DesignerPreviewPluginSession
{
public:
	DesignerPreviewPluginSession() = default;
	~DesignerPreviewPluginSession();
	DesignerPreviewPluginSession(const DesignerPreviewPluginSession&) = delete;
	DesignerPreviewPluginSession& operator=(
		const DesignerPreviewPluginSession&) = delete;
	DesignerPreviewPluginSession(DesignerPreviewPluginSession&& other) noexcept;
	DesignerPreviewPluginSession& operator=(
		DesignerPreviewPluginSession&& other) noexcept;

	bool IsValid() const noexcept { return _session != nullptr; }
	bool SetBool(const std::wstring& propertyName, bool value,
		std::wstring* outError = nullptr);
	bool SetInt64(const std::wstring& propertyName, int64_t value,
		std::wstring* outError = nullptr);
	bool SetDouble(const std::wstring& propertyName, double value,
		std::wstring* outError = nullptr);
	bool SetArgb32(const std::wstring& propertyName, uint32_t value,
		std::wstring* outError = nullptr);
	bool SetUtf8(const std::wstring& propertyName, const std::string& value,
		std::wstring* outError = nullptr);
	bool Render(
		const CuiDesignerPreviewFrameInputV1& input,
		std::vector<DesignerPreviewPrimitive>& output,
		std::wstring* outError = nullptr);

private:
	friend class DesignerPreviewPluginModule;
	DesignerPreviewPluginSession(
		std::shared_ptr<DesignerPreviewPluginState> state,
		void* session) noexcept;
	bool SetValue(
		const std::wstring& propertyName,
		const CuiDesignerPreviewValueV1& value,
		std::wstring* outError);
	void Reset() noexcept;

	std::shared_ptr<DesignerPreviewPluginState> _state;
	void* _session = nullptr;
};

/** RAII owner for one explicitly trusted preview DLL. */
class DesignerPreviewPluginModule
{
public:
	using LogHandler = std::function<void(
		CuiDesignerPreviewLogLevelV1 level,
		const std::wstring& message)>;

	DesignerPreviewPluginModule() = default;
	~DesignerPreviewPluginModule() = default;
	DesignerPreviewPluginModule(const DesignerPreviewPluginModule&) = delete;
	DesignerPreviewPluginModule& operator=(
		const DesignerPreviewPluginModule&) = delete;
	DesignerPreviewPluginModule(DesignerPreviewPluginModule&&) noexcept = default;
	DesignerPreviewPluginModule& operator=(
		DesignerPreviewPluginModule&&) noexcept = default;

	/** Requires an existing absolute .dll path supplied by trusted host config. */
	bool Load(
		const std::wstring& absolutePath,
		LogHandler logHandler = {},
		std::wstring* outError = nullptr);
	bool IsLoaded() const noexcept { return _state != nullptr; }
	const std::wstring& Path() const noexcept;

	bool CreateSession(
		const std::wstring& xamlNamespace,
		const std::wstring& xamlName,
		DesignerPreviewPluginSession& output,
		std::wstring* outError = nullptr) const;

private:
	std::shared_ptr<DesignerPreviewPluginState> _state;
};

