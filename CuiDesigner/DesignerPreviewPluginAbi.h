#pragma once

/*
 * Stable, value-only ABI for a future out-of-process-module Designer preview.
 * This header is deliberately valid C: no Control*, STL, exceptions, D2D
 * interfaces, or ownership transfer may cross the module boundary.
 */
#include <stdint.h>

#if defined(_WIN32)
#define CUI_DESIGNER_PREVIEW_CALL __cdecl
#if defined(CUI_DESIGNER_PREVIEW_PLUGIN_EXPORTS)
#define CUI_DESIGNER_PREVIEW_EXPORT __declspec(dllexport)
#else
#define CUI_DESIGNER_PREVIEW_EXPORT
#endif
#else
#define CUI_DESIGNER_PREVIEW_CALL
#define CUI_DESIGNER_PREVIEW_EXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

enum
{
	CUI_DESIGNER_PREVIEW_ABI_V1 = 0x00010000u,
	CUI_DESIGNER_PREVIEW_MAX_PRIMITIVES_V1 = 4096u,
	CUI_DESIGNER_PREVIEW_MAX_UTF8_BYTES_V1 = 1024u * 1024u
};

typedef int32_t CuiDesignerPreviewStatusV1;
enum
{
	CUI_DESIGNER_PREVIEW_OK_V1 = 0,
	CUI_DESIGNER_PREVIEW_INVALID_ARGUMENT_V1 = -1,
	CUI_DESIGNER_PREVIEW_UNSUPPORTED_V1 = -2,
	CUI_DESIGNER_PREVIEW_FAILED_V1 = -3,
	CUI_DESIGNER_PREVIEW_LIMIT_EXCEEDED_V1 = -4
};

typedef struct CuiDesignerUtf8ViewV1
{
	const char* Data;
	uint32_t Size;
} CuiDesignerUtf8ViewV1;

typedef enum CuiDesignerPreviewLogLevelV1
{
	CUI_DESIGNER_PREVIEW_LOG_INFO_V1 = 0,
	CUI_DESIGNER_PREVIEW_LOG_WARNING_V1 = 1,
	CUI_DESIGNER_PREVIEW_LOG_ERROR_V1 = 2
} CuiDesignerPreviewLogLevelV1;

typedef enum CuiDesignerPreviewValueKindV1
{
	CUI_DESIGNER_PREVIEW_VALUE_EMPTY_V1 = 0,
	CUI_DESIGNER_PREVIEW_VALUE_BOOL_V1 = 1,
	CUI_DESIGNER_PREVIEW_VALUE_INT64_V1 = 2,
	CUI_DESIGNER_PREVIEW_VALUE_DOUBLE_V1 = 3,
	CUI_DESIGNER_PREVIEW_VALUE_ARGB32_V1 = 4,
	CUI_DESIGNER_PREVIEW_VALUE_UTF8_V1 = 5
} CuiDesignerPreviewValueKindV1;

typedef struct CuiDesignerPreviewValueV1
{
	uint32_t StructSize;
	uint32_t Kind;
	union
	{
		uint32_t BoolValue;
		int64_t Int64Value;
		double DoubleValue;
		uint32_t Argb32Value;
		CuiDesignerUtf8ViewV1 Utf8Value;
	} Data;
} CuiDesignerPreviewValueV1;

typedef enum CuiDesignerPreviewPrimitiveKindV1
{
	CUI_DESIGNER_PREVIEW_PRIMITIVE_RECT_V1 = 1,
	CUI_DESIGNER_PREVIEW_PRIMITIVE_ROUNDED_RECT_V1 = 2,
	CUI_DESIGNER_PREVIEW_PRIMITIVE_ELLIPSE_V1 = 3,
	CUI_DESIGNER_PREVIEW_PRIMITIVE_LINE_V1 = 4,
	CUI_DESIGNER_PREVIEW_PRIMITIVE_TEXT_V1 = 5
} CuiDesignerPreviewPrimitiveKindV1;

typedef struct CuiDesignerPreviewPrimitiveV1
{
	uint32_t StructSize;
	uint32_t Kind;
	float X;
	float Y;
	float Width;
	float Height;
	float RadiusX;
	float RadiusY;
	float StrokeWidth;
	uint32_t FillArgb32;
	uint32_t StrokeArgb32;
	CuiDesignerUtf8ViewV1 Text;
	float FontSize;
} CuiDesignerPreviewPrimitiveV1;

typedef struct CuiDesignerPreviewFrameInputV1
{
	uint32_t StructSize;
	float Width;
	float Height;
	float DpiScale;
	uint32_t Enabled;
	uint32_t Checked;
	uint64_t FrameNumber;
} CuiDesignerPreviewFrameInputV1;

/* Plugin-owned read-only memory, valid only until the next call on the session. */
typedef struct CuiDesignerPreviewFrameV1
{
	uint32_t StructSize;
	const CuiDesignerPreviewPrimitiveV1* Primitives;
	uint32_t PrimitiveCount;
} CuiDesignerPreviewFrameV1;

typedef void (CUI_DESIGNER_PREVIEW_CALL *CuiDesignerPreviewLogFnV1)(
	void* hostContext,
	uint32_t level,
	CuiDesignerUtf8ViewV1 message);

typedef struct CuiDesignerPreviewHostV1
{
	uint32_t StructSize;
	uint32_t AbiVersion;
	void* HostContext;
	CuiDesignerPreviewLogFnV1 Log;
} CuiDesignerPreviewHostV1;

typedef CuiDesignerPreviewStatusV1
	(CUI_DESIGNER_PREVIEW_CALL *CuiDesignerPreviewCreateSessionFnV1)(
		void* pluginContext,
		CuiDesignerUtf8ViewV1 xamlNamespace,
		CuiDesignerUtf8ViewV1 xamlName,
		void** session);
typedef void (CUI_DESIGNER_PREVIEW_CALL *CuiDesignerPreviewDestroySessionFnV1)(
	void* pluginContext,
	void* session);
typedef CuiDesignerPreviewStatusV1
	(CUI_DESIGNER_PREVIEW_CALL *CuiDesignerPreviewSetValueFnV1)(
		void* pluginContext,
		void* session,
		CuiDesignerUtf8ViewV1 propertyName,
		const CuiDesignerPreviewValueV1* value);
typedef CuiDesignerPreviewStatusV1
	(CUI_DESIGNER_PREVIEW_CALL *CuiDesignerPreviewRenderFnV1)(
		void* pluginContext,
		void* session,
		const CuiDesignerPreviewFrameInputV1* input,
		CuiDesignerPreviewFrameV1* output);
typedef void (CUI_DESIGNER_PREVIEW_CALL *CuiDesignerPreviewShutdownFnV1)(
	void* pluginContext);

typedef struct CuiDesignerPreviewPluginV1
{
	uint32_t StructSize;
	uint32_t AbiVersion;
	void* PluginContext;
	CuiDesignerPreviewCreateSessionFnV1 CreateSession;
	CuiDesignerPreviewDestroySessionFnV1 DestroySession;
	CuiDesignerPreviewSetValueFnV1 SetValue;
	CuiDesignerPreviewRenderFnV1 Render;
	CuiDesignerPreviewShutdownFnV1 Shutdown;
} CuiDesignerPreviewPluginV1;

/* The only exported symbol. The returned table is plugin-owned and immutable. */
typedef CuiDesignerPreviewStatusV1
	(CUI_DESIGNER_PREVIEW_CALL *CuiDesignerGetPreviewPluginFnV1)(
		const CuiDesignerPreviewHostV1* host,
		const CuiDesignerPreviewPluginV1** plugin);

CUI_DESIGNER_PREVIEW_EXPORT CuiDesignerPreviewStatusV1
	CUI_DESIGNER_PREVIEW_CALL CuiDesignerGetPreviewPluginV1(
		const CuiDesignerPreviewHostV1* host,
		const CuiDesignerPreviewPluginV1** plugin);

#ifdef __cplusplus
}
#endif

