// The Windows SDK import libraries do not provide this EVR service GUID.
// Keep its one stable SDK definition in a media-only archive member:
// MediaElement's reference selects it, while unrelated CUI applications no
// longer select the full media backend through incidental WIC/MF GUID ownership.
#include <initguid.h>

// evr.h: MR_VIDEO_RENDER_SERVICE
DEFINE_GUID(
	MR_VIDEO_RENDER_SERVICE,
	0x1092a86c, 0xab1a, 0x459a,
	0xa3, 0x36, 0x83, 0x1f, 0xbc, 0x4d, 0x11, 0xff);
