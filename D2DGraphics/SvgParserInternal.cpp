#include "SvgParserInternal.h"
#include <string>
#include <math.h>
#include <vector>
#pragma warning(disable: 4267)
#pragma warning(disable: 4244)
#pragma warning(disable: 4018)

#define SVG_EPSILON (1e-12)
#define SVG_PI (3.14159265358979323846264338327f)
#define SVG_KAPPA90 (0.5522847493f)	
#define SVG_ALIGN_MIN 0
#define SVG_ALIGN_MID 1
#define SVG_ALIGN_MAX 2
#define SVG_ALIGN_NONE 0
#define SVG_ALIGN_MEET 1
#define SVG_ALIGN_SLICE 2
#define SVG_NOTUSED(v) do { (void)(1 ? (void)0 : ( (void)(v) ) ); } while(0)
#define SVG_RGB(r, g, b) (((unsigned int)r) | ((unsigned int)g << 8) | ((unsigned int)b << 16))

#define SVG_XML_TAG 1
#define SVG_XML_CONTENT 2
#define SVG_XML_MAX_ATTRIBUTES 256
#ifdef _MSC_VER
#pragma warning (disable: 4996)
#pragma warning (disable: 4100)
#ifdef __cplusplus
#define SVG_INLINE inline
#else
#define SVG_INLINE
#endif
#else
#define SVG_INLINE inline
#endif

const std::vector<SvgNamedColor> SvgParse_colors = {
	{ "red", SVG_RGB(255, 0, 0) },
	{ "green", SVG_RGB(0, 128, 0) },
	{ "blue", SVG_RGB(0, 0, 255) },
	{ "yellow", SVG_RGB(255, 255, 0) },
	{ "cyan", SVG_RGB(0, 255, 255) },
	{ "magenta", SVG_RGB(255, 0, 255) },
	{ "black", SVG_RGB(0, 0, 0) },
	{ "grey", SVG_RGB(128, 128, 128) },
	{ "gray", SVG_RGB(128, 128, 128) },
	{ "white", SVG_RGB(255, 255, 255) },
	{ "aliceblue", SVG_RGB(240, 248, 255) },
	{ "antiquewhite", SVG_RGB(250, 235, 215) },
	{ "aqua", SVG_RGB(0, 255, 255) },
	{ "aquamarine", SVG_RGB(127, 255, 212) },
	{ "azure", SVG_RGB(240, 255, 255) },
	{ "beige", SVG_RGB(245, 245, 220) },
	{ "bisque", SVG_RGB(255, 228, 196) },
	{ "blanchedalmond", SVG_RGB(255, 235, 205) },
	{ "blueviolet", SVG_RGB(138, 43, 226) },
	{ "brown", SVG_RGB(165, 42, 42) },
	{ "burlywood", SVG_RGB(222, 184, 135) },
	{ "cadetblue", SVG_RGB(95, 158, 160) },
	{ "chartreuse", SVG_RGB(127, 255, 0) },
	{ "chocolate", SVG_RGB(210, 105, 30) },
	{ "coral", SVG_RGB(255, 127, 80) },
	{ "cornflowerblue", SVG_RGB(100, 149, 237) },
	{ "cornsilk", SVG_RGB(255, 248, 220) },
	{ "crimson", SVG_RGB(220, 20, 60) },
	{ "darkblue", SVG_RGB(0, 0, 139) },
	{ "darkcyan", SVG_RGB(0, 139, 139) },
	{ "darkgoldenrod", SVG_RGB(184, 134, 11) },
	{ "darkgray", SVG_RGB(169, 169, 169) },
	{ "darkgreen", SVG_RGB(0, 100, 0) },
	{ "darkgrey", SVG_RGB(169, 169, 169) },
	{ "darkkhaki", SVG_RGB(189, 183, 107) },
	{ "darkmagenta", SVG_RGB(139, 0, 139) },
	{ "darkolivegreen", SVG_RGB(85, 107, 47) },
	{ "darkorange", SVG_RGB(255, 140, 0) },
	{ "darkorchid", SVG_RGB(153, 50, 204) },
	{ "darkred", SVG_RGB(139, 0, 0) },
	{ "darksalmon", SVG_RGB(233, 150, 122) },
	{ "darkseagreen", SVG_RGB(143, 188, 143) },
	{ "darkslateblue", SVG_RGB(72, 61, 139) },
	{ "darkslategray", SVG_RGB(47, 79, 79) },
	{ "darkslategrey", SVG_RGB(47, 79, 79) },
	{ "darkturquoise", SVG_RGB(0, 206, 209) },
	{ "darkviolet", SVG_RGB(148, 0, 211) },
	{ "deeppink", SVG_RGB(255, 20, 147) },
	{ "deepskyblue", SVG_RGB(0, 191, 255) },
	{ "dimgray", SVG_RGB(105, 105, 105) },
	{ "dimgrey", SVG_RGB(105, 105, 105) },
	{ "dodgerblue", SVG_RGB(30, 144, 255) },
	{ "firebrick", SVG_RGB(178, 34, 34) },
	{ "floralwhite", SVG_RGB(255, 250, 240) },
	{ "forestgreen", SVG_RGB(34, 139, 34) },
	{ "fuchsia", SVG_RGB(255, 0, 255) },
	{ "gainsboro", SVG_RGB(220, 220, 220) },
	{ "ghostwhite", SVG_RGB(248, 248, 255) },
	{ "gold", SVG_RGB(255, 215, 0) },
	{ "goldenrod", SVG_RGB(218, 165, 32) },
	{ "greenyellow", SVG_RGB(173, 255, 47) },
	{ "honeydew", SVG_RGB(240, 255, 240) },
	{ "hotpink", SVG_RGB(255, 105, 180) },
	{ "indianred", SVG_RGB(205, 92, 92) },
	{ "indigo", SVG_RGB(75, 0, 130) },
	{ "ivory", SVG_RGB(255, 255, 240) },
	{ "khaki", SVG_RGB(240, 230, 140) },
	{ "lavender", SVG_RGB(230, 230, 250) },
	{ "lavenderblush", SVG_RGB(255, 240, 245) },
	{ "lawngreen", SVG_RGB(124, 252, 0) },
	{ "lemonchiffon", SVG_RGB(255, 250, 205) },
	{ "lightblue", SVG_RGB(173, 216, 230) },
	{ "lightcoral", SVG_RGB(240, 128, 128) },
	{ "lightcyan", SVG_RGB(224, 255, 255) },
	{ "lightgoldenrodyellow", SVG_RGB(250, 250, 210) },
	{ "lightgray", SVG_RGB(211, 211, 211) },
	{ "lightgreen", SVG_RGB(144, 238, 144) },
	{ "lightgrey", SVG_RGB(211, 211, 211) },
	{ "lightpink", SVG_RGB(255, 182, 193) },
	{ "lightsalmon", SVG_RGB(255, 160, 122) },
	{ "lightseagreen", SVG_RGB(32, 178, 170) },
	{ "lightskyblue", SVG_RGB(135, 206, 250) },
	{ "lightslategray", SVG_RGB(119, 136, 153) },
	{ "lightslategrey", SVG_RGB(119, 136, 153) },
	{ "lightsteelblue", SVG_RGB(176, 196, 222) },
	{ "lightyellow", SVG_RGB(255, 255, 224) },
	{ "lime", SVG_RGB(0, 255, 0) },
	{ "limegreen", SVG_RGB(50, 205, 50) },
	{ "linen", SVG_RGB(250, 240, 230) },
	{ "maroon", SVG_RGB(128, 0, 0) },
	{ "mediumaquamarine", SVG_RGB(102, 205, 170) },
	{ "mediumblue", SVG_RGB(0, 0, 205) },
	{ "mediumorchid", SVG_RGB(186, 85, 211) },
	{ "mediumpurple", SVG_RGB(147, 112, 219) },
	{ "mediumseagreen", SVG_RGB(60, 179, 113) },
	{ "mediumslateblue", SVG_RGB(123, 104, 238) },
	{ "mediumspringgreen", SVG_RGB(0, 250, 154) },
	{ "mediumturquoise", SVG_RGB(72, 209, 204) },
	{ "mediumvioletred", SVG_RGB(199, 21, 133) },
	{ "midnightblue", SVG_RGB(25, 25, 112) },
	{ "mintcream", SVG_RGB(245, 255, 250) },
	{ "mistyrose", SVG_RGB(255, 228, 225) },
	{ "moccasin", SVG_RGB(255, 228, 181) },
	{ "navajowhite", SVG_RGB(255, 222, 173) },
	{ "navy", SVG_RGB(0, 0, 128) },
	{ "oldlace", SVG_RGB(253, 245, 230) },
	{ "olive", SVG_RGB(128, 128, 0) },
	{ "olivedrab", SVG_RGB(107, 142, 35) },
	{ "orange", SVG_RGB(255, 165, 0) },
	{ "orangered", SVG_RGB(255, 69, 0) },
	{ "orchid", SVG_RGB(218, 112, 214) },
	{ "palegoldenrod", SVG_RGB(238, 232, 170) },
	{ "palegreen", SVG_RGB(152, 251, 152) },
	{ "paleturquoise", SVG_RGB(175, 238, 238) },
	{ "palevioletred", SVG_RGB(219, 112, 147) },
	{ "papayawhip", SVG_RGB(255, 239, 213) },
	{ "peachpuff", SVG_RGB(255, 218, 185) },
	{ "peru", SVG_RGB(205, 133, 63) },
	{ "pink", SVG_RGB(255, 192, 203) },
	{ "plum", SVG_RGB(221, 160, 221) },
	{ "powderblue", SVG_RGB(176, 224, 230) },
	{ "purple", SVG_RGB(128, 0, 128) },
	{ "rosybrown", SVG_RGB(188, 143, 143) },
	{ "royalblue", SVG_RGB(65, 105, 225) },
	{ "saddlebrown", SVG_RGB(139, 69, 19) },
	{ "salmon", SVG_RGB(250, 128, 114) },
	{ "sandybrown", SVG_RGB(244, 164, 96) },
	{ "seagreen", SVG_RGB(46, 139, 87) },
	{ "seashell", SVG_RGB(255, 245, 238) },
	{ "sienna", SVG_RGB(160, 82, 45) },
	{ "silver", SVG_RGB(192, 192, 192) },
	{ "skyblue", SVG_RGB(135, 206, 235) },
	{ "slateblue", SVG_RGB(106, 90, 205) },
	{ "slategray", SVG_RGB(112, 128, 144) },
	{ "slategrey", SVG_RGB(112, 128, 144) },
	{ "snow", SVG_RGB(255, 250, 250) },
	{ "springgreen", SVG_RGB(0, 255, 127) },
	{ "steelblue", SVG_RGB(70, 130, 180) },
	{ "tan", SVG_RGB(210, 180, 140) },
	{ "teal", SVG_RGB(0, 128, 128) },
	{ "thistle", SVG_RGB(216, 191, 216) },
	{ "tomato", SVG_RGB(255, 99, 71) },
	{ "turquoise", SVG_RGB(64, 224, 208) },
	{ "violet", SVG_RGB(238, 130, 238) },
	{ "wheat", SVG_RGB(245, 222, 179) },
	{ "whitesmoke", SVG_RGB(245, 245, 245) },
	{ "yellowgreen", SVG_RGB(154, 205, 50) }
};

__forceinline int SvgParse_isspace(char c);
__forceinline int SvgParse_isdigit(char c);
__forceinline float SvgParse_minf(float a, float b);
__forceinline float SvgParse_maxf(float a, float b);
void SvgParse_parseContent(char* s, void (*contentCb)(void* ud, const char* s), void* ud);
void SvgParse_parseElement(char* s, void (*startelCb)(void* ud, const char* el, const char** attr), void (*endelCb)(void* ud, const char* el), void* ud);
int SvgParse_parseXML(char* input, void (*startelCb)(void* ud, const char* el, const char** attr), void (*endelCb)(void* ud, const char* el), void (*contentCb)(void* ud, const char* s), void* ud);
void SvgParse_xformIdentity(float* t);
void SvgParse_xformSetTranslation(float* t, float tx, float ty);
void SvgParse_xformSetScale(float* t, float sx, float sy);
void SvgParse_xformSetSkewX(float* t, float a);
void SvgParse_xformSetSkewY(float* t, float a);
void SvgParse_xformSetRotation(float* t, float a);
void SvgParse_xformMultiply(float* t, float* s);
void SvgParse_xformInverse(float* inv, float* t);
void SvgParse_xformPremultiply(float* t, float* s);
void SvgParse_xformPoint(float* dx, float* dy, float x, float y, float* t);
void SvgParse_xformVec(float* dx, float* dy, float x, float y, float* t);
int SvgParse_ptInBounds(float* pt, float* bounds);
double SvgParse_evalBezier(double t, double p0, double p1, double p2, double p3);
void SvgParse_curveBounds(float* bounds, float* curve);
SvgParser* SvgParse_createParser(void);
void SvgParse_deletePaths(SvgPath* path);
void SvgParse_deletePaint(SvgPaint* paint);
void SvgParse_deleteGradientData(SvgGradientData* grad);
void SvgParse_deleteParser(SvgParser* p);
void SvgParse_resetPath(SvgParser* p);
void SvgParse_addPoint(SvgParser* p, float x, float y);
void SvgParse_moveTo(SvgParser* p, float x, float y);
void SvgParse_lineTo(SvgParser* p, float x, float y);
void SvgParse_cubicBezTo(SvgParser* p, float cpx1, float cpy1, float cpx2, float cpy2, float x, float y);
SvgAttributeState* SvgParse_getAttr(SvgParser* p);
void SvgParse_pushAttr(SvgParser* p);
void SvgParse_popAttr(SvgParser* p);
float SvgParse_actualOrigX(SvgParser* p);
float SvgParse_actualOrigY(SvgParser* p);
float SvgParse_actualWidth(SvgParser* p);
float SvgParse_actualHeight(SvgParser* p);
float SvgParse_actualLength(SvgParser* p);
float SvgParse_convertToPixels(SvgParser* p, SvgCoordinate c, float orig, float length);
SvgGradientData* SvgParse_findGradientData(SvgParser* p, const char* id);
SvgGradient* SvgParse_createGradient(SvgParser* p, const char* id, const float* localBounds, float* xform, signed char* paintType);
float SvgParse_getAverageScale(float* t);
void SvgParse_getLocalBounds(float* bounds, SvgShape* shape, float* xform);
void SvgParse_addShape(SvgParser* p);
void SvgParse_addPath(SvgParser* p, char closed);
double SvgParse_atof(const char* s);
const char* SvgParse_parseNumber(const char* s, char* it, const int size);
const char* SvgParse_getNextPathItemWhenArcFlag(const char* s, char* it);
const char* SvgParse_getNextPathItem(const char* s, char* it);
unsigned int SvgParse_parseColorHex(const char* str);
unsigned int SvgParse_parseColorRGB(const char* str);
unsigned int SvgParse_parseColorName(const char* str);
unsigned int SvgParse_parseColor(const char* str);
float SvgParse_parseOpacity(const char* str);
float SvgParse_parseMiterLimit(const char* str);
int SvgParse_parseUnits(const char* units);
int SvgParse_isCoordinate(const char* s);
SvgCoordinate SvgParse_parseCoordinateRaw(const char* str);
SvgCoordinate SvgParse_coord(float v, int units);
float SvgParse_parseCoordinate(SvgParser* p, const char* str, float orig, float length);
int SvgParse_parseTransformArgs(const char* str, float* args, int maxNa, int* na);
int SvgParse_parseMatrix(float* xform, const char* str);
int SvgParse_parseTranslate(float* xform, const char* str);
int SvgParse_parseScale(float* xform, const char* str);
int SvgParse_parseSkewX(float* xform, const char* str);
int SvgParse_parseSkewY(float* xform, const char* str);
int SvgParse_parseRotate(float* xform, const char* str);
void SvgParse_parseTransform(float* xform, const char* str);
void SvgParse_parseUrl(char* id, const char* str);
char SvgParse_parseLineCap(const char* str);
char SvgParse_parseLineJoin(const char* str);
char SvgParse_parseFillRule(const char* str);
const char* SvgParse_getNextDashItem(const char* s, char* it);
int SvgParse_parseStrokeDashArray(SvgParser* p, const char* str, float* strokeDashArray);
int SvgParse_parseAttr(SvgParser* p, const char* name, const char* value);
int SvgParse_parseNameValue(SvgParser* p, const char* start, const char* end);
void SvgParse_parseStyle(SvgParser* p, const char* str);
void SvgParse_parseAttribs(SvgParser* p, const char** attr);
int SvgParse_getArgsPerElement(char cmd);
void SvgParse_pathMoveTo(SvgParser* p, float* cpx, float* cpy, float* args, int rel);
void SvgParse_pathLineTo(SvgParser* p, float* cpx, float* cpy, float* args, int rel);
void SvgParse_pathHLineTo(SvgParser* p, float* cpx, float* cpy, float* args, int rel);
void SvgParse_pathVLineTo(SvgParser* p, float* cpx, float* cpy, float* args, int rel);
void SvgParse_pathCubicBezTo(SvgParser* p, float* cpx, float* cpy, float* cpx2, float* cpy2, float* args, int rel);
void SvgParse_pathCubicBezShortTo(SvgParser* p, float* cpx, float* cpy, float* cpx2, float* cpy2, float* args, int rel);
void SvgParse_pathQuadBezTo(SvgParser* p, float* cpx, float* cpy, float* cpx2, float* cpy2, float* args, int rel);
void SvgParse_pathQuadBezShortTo(SvgParser* p, float* cpx, float* cpy, float* cpx2, float* cpy2, float* args, int rel);
float SvgParse_sqr(float x);
float SvgParse_vmag(float x, float y);
float SvgParse_vecrat(float ux, float uy, float vx, float vy);
float SvgParse_vecang(float ux, float uy, float vx, float vy);
void SvgParse_pathArcTo(SvgParser* p, float* cpx, float* cpy, float* args, int rel);
void SvgParse_parsePath(SvgParser* p, const char** attr);
void SvgParse_parseRect(SvgParser* p, const char** attr);
void SvgParse_parseCircle(SvgParser* p, const char** attr);
void SvgParse_parseEllipse(SvgParser* p, const char** attr);
void SvgParse_parseLine(SvgParser* p, const char** attr);
void SvgParse_parsePoly(SvgParser* p, const char** attr, int closeFlag);
void SvgParse_parseSVG(SvgParser* p, const char** attr);
void SvgParse_parseGradient(SvgParser* p, const char** attr, signed char type);
void SvgParse_parseGradientStop(SvgParser* p, const char** attr);
void SvgParse_startElement(void* ud, const char* el, const char** attr);
void SvgParse_endElement(void* ud, const char* el);
void SvgParse_content(void* ud, const char* s);
void SvgParse_imageBounds(SvgParser* p, float* bounds);
float SvgParse_viewAlign(float content, float container, int type);
void SvgParse_scaleGradient(SvgGradient* grad, float tx, float ty, float sx, float sy);
void SvgParse_scaleToViewbox(SvgParser* p, const char* units);
void SvgParse_createGradients(SvgParser* p);

__forceinline int SvgParse_isspace(char c)
{
	return strchr(" \t\n\v\f\r", c) != 0;
}
__forceinline int SvgParse_isdigit(char c)
{
	return c >= '0' && c <= '9';
}
__forceinline float SvgParse_minf(float a, float b)
{
	return a < b ? a : b;
}
__forceinline float SvgParse_maxf(float a, float b)
{
	return a > b ? a : b;
}
void SvgParse_parseContent(char* s, void (*contentCb)(void* ud, const char* s), void* ud)
{
	
	while (*s && SvgParse_isspace(*s)) s++;
	if (!*s) return;
	if (contentCb)
		(*contentCb)(ud, s);
}
void SvgParse_parseElement(char* s, void (*startelCb)(void* ud, const char* el, const char** attr), void (*endelCb)(void* ud, const char* el), void* ud)
{
	const char* attr[SVG_XML_MAX_ATTRIBUTES];
	int nattr = 0;
	char* name;
	int start = 0;
	int end = 0;
	char quote;
	
	while (*s && SvgParse_isspace(*s)) s++;
	
	if (*s == '/') {
		s++;
		end = 1;
	}
	else {
		start = 1;
	}
	
	if (!*s || *s == '?' || *s == '!')
		return;
	
	name = s;
	while (*s && !SvgParse_isspace(*s)) s++;
	if (*s) { *s++ = '\0'; }
	
	while (!end && *s && nattr < SVG_XML_MAX_ATTRIBUTES - 3) {
		char* name = NULL;
		char* value = NULL;
		
		while (*s && SvgParse_isspace(*s)) s++;
		if (!*s) break;
		if (*s == '/') {
			end = 1;
			break;
		}
		name = s;
		
		while (*s && !SvgParse_isspace(*s) && *s != '=') s++;
		if (*s) { *s++ = '\0'; }
		
		while (*s && *s != '\"' && *s != '\'') s++;
		if (!*s) break;
		quote = *s;
		s++;
		
		value = s;
		while (*s && *s != quote) s++;
		if (*s) { *s++ = '\0'; }
		
		if (name && value) {
			attr[nattr++] = name;
			attr[nattr++] = value;
		}
	}
	
	attr[nattr++] = 0;
	attr[nattr++] = 0;
	
	if (start && startelCb)
		(*startelCb)(ud, name, attr);
	if (end && endelCb)
		(*endelCb)(ud, name);
}
int SvgParse_parseXML(char* input, void (*startelCb)(void* ud, const char* el, const char** attr), void (*endelCb)(void* ud, const char* el), void (*contentCb)(void* ud, const char* s), void* ud)
{
	char* s = input;
	char* mark = s;
	int state = SVG_XML_CONTENT;
	while (*s) {
		if (*s == '<' && state == SVG_XML_CONTENT) {
			
			*s++ = '\0';
			SvgParse_parseContent(mark, contentCb, ud);
			mark = s;
			state = SVG_XML_TAG;
		}
		else if (*s == '>' && state == SVG_XML_TAG) {
			
			*s++ = '\0';
			SvgParse_parseElement(mark, startelCb, endelCb, ud);
			mark = s;
			state = SVG_XML_CONTENT;
		}
		else {
			s++;
		}
	}
	return 1;
}
void SvgParse_xformIdentity(float* t)
{
	t[0] = 1.0f; t[1] = 0.0f;
	t[2] = 0.0f; t[3] = 1.0f;
	t[4] = 0.0f; t[5] = 0.0f;
}
void SvgParse_xformSetTranslation(float* t, float tx, float ty)
{
	t[0] = 1.0f; t[1] = 0.0f;
	t[2] = 0.0f; t[3] = 1.0f;
	t[4] = tx; t[5] = ty;
}
void SvgParse_xformSetScale(float* t, float sx, float sy)
{
	t[0] = sx; t[1] = 0.0f;
	t[2] = 0.0f; t[3] = sy;
	t[4] = 0.0f; t[5] = 0.0f;
}
void SvgParse_xformSetSkewX(float* t, float a)
{
	t[0] = 1.0f; t[1] = 0.0f;
	t[2] = tanf(a); t[3] = 1.0f;
	t[4] = 0.0f; t[5] = 0.0f;
}
void SvgParse_xformSetSkewY(float* t, float a)
{
	t[0] = 1.0f; t[1] = tanf(a);
	t[2] = 0.0f; t[3] = 1.0f;
	t[4] = 0.0f; t[5] = 0.0f;
}
void SvgParse_xformSetRotation(float* t, float a)
{
	float cs = cosf(a), sn = sinf(a);
	t[0] = cs; t[1] = sn;
	t[2] = -sn; t[3] = cs;
	t[4] = 0.0f; t[5] = 0.0f;
}
void SvgParse_xformMultiply(float* t, float* s)
{
	float t0 = t[0] * s[0] + t[1] * s[2];
	float t2 = t[2] * s[0] + t[3] * s[2];
	float t4 = t[4] * s[0] + t[5] * s[2] + s[4];
	t[1] = t[0] * s[1] + t[1] * s[3];
	t[3] = t[2] * s[1] + t[3] * s[3];
	t[5] = t[4] * s[1] + t[5] * s[3] + s[5];
	t[0] = t0;
	t[2] = t2;
	t[4] = t4;
}
void SvgParse_xformInverse(float* inv, float* t)
{
	double invdet, det = (double)t[0] * t[3] - (double)t[2] * t[1];
	if (det > -1e-6 && det < 1e-6) {
		SvgParse_xformIdentity(t);
		return;
	}
	invdet = 1.0 / det;
	inv[0] = (float)(t[3] * invdet);
	inv[2] = (float)(-t[2] * invdet);
	inv[4] = (float)(((double)t[2] * t[5] - (double)t[3] * t[4]) * invdet);
	inv[1] = (float)(-t[1] * invdet);
	inv[3] = (float)(t[0] * invdet);
	inv[5] = (float)(((double)t[1] * t[4] - (double)t[0] * t[5]) * invdet);
}
void SvgParse_xformPremultiply(float* t, float* s)
{
	float s2[6];
	memcpy(s2, s, sizeof(float) * 6);
	SvgParse_xformMultiply(s2, t);
	memcpy(t, s2, sizeof(float) * 6);
}
void SvgParse_xformPoint(float* dx, float* dy, float x, float y, float* t)
{
	*dx = x * t[0] + y * t[2] + t[4];
	*dy = x * t[1] + y * t[3] + t[5];
}
void SvgParse_xformVec(float* dx, float* dy, float x, float y, float* t)
{
	*dx = x * t[0] + y * t[2];
	*dy = x * t[1] + y * t[3];
}
int SvgParse_ptInBounds(float* pt, float* bounds)
{
	return pt[0] >= bounds[0] && pt[0] <= bounds[2] && pt[1] >= bounds[1] && pt[1] <= bounds[3];
}
double SvgParse_evalBezier(double t, double p0, double p1, double p2, double p3)
{
	double it = 1.0 - t;
	return it * it * it * p0 + 3.0 * it * it * t * p1 + 3.0 * it * t * t * p2 + t * t * t * p3;
}
void SvgParse_curveBounds(float* bounds, float* curve)
{
	int i, j, count;
	double roots[2], a, b, c, b2ac, t, v;
	float* v0 = &curve[0];
	float* v1 = &curve[2];
	float* v2 = &curve[4];
	float* v3 = &curve[6];
	
	bounds[0] = SvgParse_minf(v0[0], v3[0]);
	bounds[1] = SvgParse_minf(v0[1], v3[1]);
	bounds[2] = SvgParse_maxf(v0[0], v3[0]);
	bounds[3] = SvgParse_maxf(v0[1], v3[1]);
	
	
	if (SvgParse_ptInBounds(v1, bounds) && SvgParse_ptInBounds(v2, bounds))
		return;
	
	for (i = 0; i < 2; i++) {
		a = -3.0 * v0[i] + 9.0 * v1[i] - 9.0 * v2[i] + 3.0 * v3[i];
		b = 6.0 * v0[i] - 12.0 * v1[i] + 6.0 * v2[i];
		c = 3.0 * v1[i] - 3.0 * v0[i];
		count = 0;
		if (fabs(a) < SVG_EPSILON) {
			if (fabs(b) > SVG_EPSILON) {
				t = -c / b;
				if (t > SVG_EPSILON && t < 1.0 - SVG_EPSILON)
					roots[count++] = t;
			}
		}
		else {
			b2ac = b * b - 4.0 * c * a;
			if (b2ac > SVG_EPSILON) {
				t = (-b + sqrt(b2ac)) / (2.0 * a);
				if (t > SVG_EPSILON && t < 1.0 - SVG_EPSILON)
					roots[count++] = t;
				t = (-b - sqrt(b2ac)) / (2.0 * a);
				if (t > SVG_EPSILON && t < 1.0 - SVG_EPSILON)
					roots[count++] = t;
			}
		}
		for (j = 0; j < count; j++) {
			v = SvgParse_evalBezier(roots[j], v0[i], v1[i], v2[i], v3[i]);
			bounds[0 + i] = SvgParse_minf(bounds[0 + i], (float)v);
			bounds[2 + i] = SvgParse_maxf(bounds[2 + i], (float)v);
		}
	}
}
SvgParser* SvgParse_createParser(void)
{
	SvgParser* p;
	p = (SvgParser*)malloc(sizeof(SvgParser));
	if (p == NULL) goto error;
	memset(p, 0, sizeof(SvgParser));
	p->image = (SvgImage*)malloc(sizeof(SvgImage));
	if (p->image == NULL) goto error;
	memset(p->image, 0, sizeof(SvgImage));
	
	SvgParse_xformIdentity(p->attr[0].xform);
	memset(p->attr[0].id, 0, sizeof p->attr[0].id);
	p->attr[0].fillColor = SVG_RGB(0, 0, 0);
	p->attr[0].strokeColor = SVG_RGB(0, 0, 0);
	p->attr[0].opacity = 1;
	p->attr[0].fillOpacity = 1;
	p->attr[0].strokeOpacity = 1;
	p->attr[0].stopOpacity = 1;
	p->attr[0].strokeWidth = 1;
	p->attr[0].strokeLineJoin = SVG_JOIN_MITER;
	p->attr[0].strokeLineCap = SVG_CAP_BUTT;
	p->attr[0].miterLimit = 4;
	p->attr[0].fillRule = SVG_FILLRULE_NONZERO;
	p->attr[0].hasFill = 1;
	p->attr[0].visible = 1;
	return p;
error:
	if (p) {
		if (p->image) free(p->image);
		free(p);
	}
	return NULL;
}
void SvgParse_deletePaths(SvgPath* path)
{
	while (path) {
		SvgPath* next = path->next;
		if (path->pts != NULL)
			free(path->pts);
		free(path);
		path = next;
	}
}
void SvgParse_deletePaint(SvgPaint* paint)
{
	if (paint->type == SVG_PAINT_LINEAR_GRADIENT || paint->type == SVG_PAINT_RADIAL_GRADIENT)
		free(paint->gradient);
}
void SvgParse_deleteGradientData(SvgGradientData* grad)
{
	SvgGradientData* next;
	while (grad != NULL) {
		next = grad->next;
		free(grad->stops);
		free(grad);
		grad = next;
	}
}
void DeleteSvgImageInternal(SvgImage* image)
{
	SvgShape* snext, * shape;
	if (image == NULL) return;
	shape = image->shapes;
	while (shape != NULL) {
		snext = shape->next;
		SvgParse_deletePaths(shape->paths);
		SvgParse_deletePaint(&shape->fill);
		SvgParse_deletePaint(&shape->stroke);
		free(shape);
		shape = snext;
	}
	free(image);
}
void SvgParse_deleteParser(SvgParser* p)
{
	if (p != NULL) {
		SvgParse_deletePaths(p->plist);
		SvgParse_deleteGradientData(p->gradients);
		DeleteSvgImageInternal(p->image);
		free(p->pts);
		free(p);
	}
}
void SvgParse_resetPath(SvgParser* p)
{
	p->npts = 0;
}
void SvgParse_addPoint(SvgParser* p, float x, float y)
{
	if (p->npts + 1 > p->cpts) {
		p->cpts = p->cpts ? p->cpts * 2 : 8;
		p->pts = (float*)realloc(p->pts, p->cpts * 2 * sizeof(float));
		if (!p->pts) return;
	}
	p->pts[p->npts * 2 + 0] = x;
	p->pts[p->npts * 2 + 1] = y;
	p->npts++;
}
void SvgParse_moveTo(SvgParser* p, float x, float y)
{
	if (p->npts > 0) {
		p->pts[(p->npts - 1) * 2 + 0] = x;
		p->pts[(p->npts - 1) * 2 + 1] = y;
	}
	else {
		SvgParse_addPoint(p, x, y);
	}
}
void SvgParse_lineTo(SvgParser* p, float x, float y)
{
	float px, py, dx, dy;
	if (p->npts > 0) {
		px = p->pts[(p->npts - 1) * 2 + 0];
		py = p->pts[(p->npts - 1) * 2 + 1];
		dx = x - px;
		dy = y - py;
		SvgParse_addPoint(p, px + dx / 3.0f, py + dy / 3.0f);
		SvgParse_addPoint(p, x - dx / 3.0f, y - dy / 3.0f);
		SvgParse_addPoint(p, x, y);
	}
}
void SvgParse_cubicBezTo(SvgParser* p, float cpx1, float cpy1, float cpx2, float cpy2, float x, float y)
{
	if (p->npts > 0) {
		SvgParse_addPoint(p, cpx1, cpy1);
		SvgParse_addPoint(p, cpx2, cpy2);
		SvgParse_addPoint(p, x, y);
	}
}
SvgAttributeState* SvgParse_getAttr(SvgParser* p)
{
	return &p->attr[p->attrHead];
}
void SvgParse_pushAttr(SvgParser* p)
{
	if (p->attrHead < SVG_MAX_ATTRIBUTES - 1) {
		p->attrHead++;
		memcpy(&p->attr[p->attrHead], &p->attr[p->attrHead - 1], sizeof(SvgAttributeState));
	}
}
void SvgParse_popAttr(SvgParser* p)
{
	if (p->attrHead > 0)
		p->attrHead--;
}
float SvgParse_actualOrigX(SvgParser* p)
{
	return p->viewMinx;
}
float SvgParse_actualOrigY(SvgParser* p)
{
	return p->viewMiny;
}
float SvgParse_actualWidth(SvgParser* p)
{
	return p->viewWidth;
}
float SvgParse_actualHeight(SvgParser* p)
{
	return p->viewHeight;
}
float SvgParse_actualLength(SvgParser* p)
{
	float w = SvgParse_actualWidth(p), h = SvgParse_actualHeight(p);
	return sqrtf(w * w + h * h) / sqrtf(2.0f);
}
float SvgParse_convertToPixels(SvgParser* p, SvgCoordinate c, float orig, float length)
{
	SvgAttributeState* attr = SvgParse_getAttr(p);
	switch (c.units) {
	case SVG_UNITS_USER:		return c.value;
	case SVG_UNITS_PX:			return c.value;
	case SVG_UNITS_PT:			return c.value / 72.0f * p->dpi;
	case SVG_UNITS_PC:			return c.value / 6.0f * p->dpi;
	case SVG_UNITS_MM:			return c.value / 25.4f * p->dpi;
	case SVG_UNITS_CM:			return c.value / 2.54f * p->dpi;
	case SVG_UNITS_IN:			return c.value * p->dpi;
	case SVG_UNITS_EM:			return c.value * attr->fontSize;
	case SVG_UNITS_EX:			return c.value * attr->fontSize * 0.52f; 
	case SVG_UNITS_PERCENT:	return orig + c.value / 100.0f * length;
	default:					return c.value;
	}
	return c.value;
}
SvgGradientData* SvgParse_findGradientData(SvgParser* p, const char* id)
{
	SvgGradientData* grad = p->gradients;
	if (id == NULL || *id == '\0')
		return NULL;
	while (grad != NULL) {
		if (strcmp(grad->id, id) == 0)
			return grad;
		grad = grad->next;
	}
	return NULL;
}
SvgGradient* SvgParse_createGradient(SvgParser* p, const char* id, const float* localBounds, float* xform, signed char* paintType)
{
	SvgGradientData* data = NULL;
	SvgGradientData* ref = NULL;
	SvgGradientStop* stops = NULL;
	SvgGradient* grad;
	float ox, oy, sw, sh, sl;
	int nstops = 0;
	int refIter;
	data = SvgParse_findGradientData(p, id);
	if (data == NULL) return NULL;
	
	ref = data;
	refIter = 0;
	while (ref != NULL) {
		SvgGradientData* nextRef = NULL;
		if (stops == NULL && ref->stops != NULL) {
			stops = ref->stops;
			nstops = ref->nstops;
			break;
		}
		nextRef = SvgParse_findGradientData(p, ref->ref);
		if (nextRef == ref) break; 
		ref = nextRef;
		refIter++;
		if (refIter > 32) break; 
	}
	if (stops == NULL) return NULL;
	grad = (SvgGradient*)malloc(sizeof(SvgGradient) + sizeof(SvgGradientStop) * (nstops - 1));
	if (grad == NULL) return NULL;
	
	if (data->units == SVG_OBJECT_SPACE) {
		ox = localBounds[0];
		oy = localBounds[1];
		sw = localBounds[2] - localBounds[0];
		sh = localBounds[3] - localBounds[1];
	}
	else {
		ox = SvgParse_actualOrigX(p);
		oy = SvgParse_actualOrigY(p);
		sw = SvgParse_actualWidth(p);
		sh = SvgParse_actualHeight(p);
	}
	sl = sqrtf(sw * sw + sh * sh) / sqrtf(2.0f);
	if (data->type == SVG_PAINT_LINEAR_GRADIENT) {
		float x1, y1, x2, y2, dx, dy;
		x1 = SvgParse_convertToPixels(p, data->linear.x1, ox, sw);
		y1 = SvgParse_convertToPixels(p, data->linear.y1, oy, sh);
		x2 = SvgParse_convertToPixels(p, data->linear.x2, ox, sw);
		y2 = SvgParse_convertToPixels(p, data->linear.y2, oy, sh);
		
		dx = x2 - x1;
		dy = y2 - y1;
		grad->xform[0] = dy; grad->xform[1] = -dx;
		grad->xform[2] = dx; grad->xform[3] = dy;
		grad->xform[4] = x1; grad->xform[5] = y1;
	}
	else {
		float cx, cy, fx, fy, r;
		cx = SvgParse_convertToPixels(p, data->radial.cx, ox, sw);
		cy = SvgParse_convertToPixels(p, data->radial.cy, oy, sh);
		fx = SvgParse_convertToPixels(p, data->radial.fx, ox, sw);
		fy = SvgParse_convertToPixels(p, data->radial.fy, oy, sh);
		r = SvgParse_convertToPixels(p, data->radial.r, 0, sl);
		
		grad->xform[0] = r; grad->xform[1] = 0;
		grad->xform[2] = 0; grad->xform[3] = r;
		grad->xform[4] = cx; grad->xform[5] = cy;
		grad->fx = fx / r;
		grad->fy = fy / r;
	}
	SvgParse_xformMultiply(grad->xform, data->xform);
	SvgParse_xformMultiply(grad->xform, xform);
	grad->spread = data->spread;
	memcpy(grad->stops, stops, nstops * sizeof(SvgGradientStop));
	grad->nstops = nstops;
	*paintType = data->type;
	return grad;
}
float SvgParse_getAverageScale(float* t)
{
	float sx = sqrtf(t[0] * t[0] + t[2] * t[2]);
	float sy = sqrtf(t[1] * t[1] + t[3] * t[3]);
	return (sx + sy) * 0.5f;
}
void SvgParse_getLocalBounds(float* bounds, SvgShape* shape, float* xform)
{
	SvgPath* path;
	float curve[4 * 2], curveBounds[4];
	int i, first = 1;
	for (path = shape->paths; path != NULL; path = path->next) {
		SvgParse_xformPoint(&curve[0], &curve[1], path->pts[0], path->pts[1], xform);
		for (i = 0; i < path->npts - 1; i += 3) {
			SvgParse_xformPoint(&curve[2], &curve[3], path->pts[(i + 1) * 2], path->pts[(i + 1) * 2 + 1], xform);
			SvgParse_xformPoint(&curve[4], &curve[5], path->pts[(i + 2) * 2], path->pts[(i + 2) * 2 + 1], xform);
			SvgParse_xformPoint(&curve[6], &curve[7], path->pts[(i + 3) * 2], path->pts[(i + 3) * 2 + 1], xform);
			SvgParse_curveBounds(curveBounds, curve);
			if (first) {
				bounds[0] = curveBounds[0];
				bounds[1] = curveBounds[1];
				bounds[2] = curveBounds[2];
				bounds[3] = curveBounds[3];
				first = 0;
			}
			else {
				bounds[0] = SvgParse_minf(bounds[0], curveBounds[0]);
				bounds[1] = SvgParse_minf(bounds[1], curveBounds[1]);
				bounds[2] = SvgParse_maxf(bounds[2], curveBounds[2]);
				bounds[3] = SvgParse_maxf(bounds[3], curveBounds[3]);
			}
			curve[0] = curve[6];
			curve[1] = curve[7];
		}
	}
}
void SvgParse_addShape(SvgParser* p)
{
	SvgAttributeState* attr = SvgParse_getAttr(p);
	float scale = 1.0f;
	SvgShape* shape;
	SvgPath* path;
	int i;
	if (p->plist == NULL)
		return;
	shape = (SvgShape*)malloc(sizeof(SvgShape));
	if (shape == NULL) goto error;
	memset(shape, 0, sizeof(SvgShape));
	memcpy(shape->id, attr->id, sizeof shape->id);
	memcpy(shape->fillGradient, attr->fillGradient, sizeof shape->fillGradient);
	memcpy(shape->strokeGradient, attr->strokeGradient, sizeof shape->strokeGradient);
	memcpy(shape->xform, attr->xform, sizeof shape->xform);
	scale = SvgParse_getAverageScale(attr->xform);
	shape->strokeWidth = attr->strokeWidth * scale;
	shape->strokeDashOffset = attr->strokeDashOffset * scale;
	shape->strokeDashCount = (char)attr->strokeDashCount;
	for (i = 0; i < attr->strokeDashCount; i++)
		shape->strokeDashArray[i] = attr->strokeDashArray[i] * scale;
	shape->strokeLineJoin = attr->strokeLineJoin;
	shape->strokeLineCap = attr->strokeLineCap;
	shape->miterLimit = attr->miterLimit;
	shape->fillRule = attr->fillRule;
	shape->opacity = attr->opacity;
	shape->paths = p->plist;
	p->plist = NULL;
	
	shape->bounds[0] = shape->paths->bounds[0];
	shape->bounds[1] = shape->paths->bounds[1];
	shape->bounds[2] = shape->paths->bounds[2];
	shape->bounds[3] = shape->paths->bounds[3];
	for (path = shape->paths->next; path != NULL; path = path->next) {
		shape->bounds[0] = SvgParse_minf(shape->bounds[0], path->bounds[0]);
		shape->bounds[1] = SvgParse_minf(shape->bounds[1], path->bounds[1]);
		shape->bounds[2] = SvgParse_maxf(shape->bounds[2], path->bounds[2]);
		shape->bounds[3] = SvgParse_maxf(shape->bounds[3], path->bounds[3]);
	}
	
	if (attr->hasFill == 0) {
		shape->fill.type = SVG_PAINT_NONE;
	}
	else if (attr->hasFill == 1) {
		shape->fill.type = SVG_PAINT_COLOR;
		shape->fill.color = attr->fillColor;
		shape->fill.color |= (unsigned int)(attr->fillOpacity * 255) << 24;
	}
	else if (attr->hasFill == 2) {
		shape->fill.type = SVG_PAINT_UNDEF;
	}
	
	if (attr->hasStroke == 0) {
		shape->stroke.type = SVG_PAINT_NONE;
	}
	else if (attr->hasStroke == 1) {
		shape->stroke.type = SVG_PAINT_COLOR;
		shape->stroke.color = attr->strokeColor;
		shape->stroke.color |= (unsigned int)(attr->strokeOpacity * 255) << 24;
	}
	else if (attr->hasStroke == 2) {
		shape->stroke.type = SVG_PAINT_UNDEF;
	}
	
	shape->flags = (attr->visible ? SVG_FLAGS_VISIBLE : 0x00);
	
	if (p->image->shapes == NULL)
		p->image->shapes = shape;
	else
		p->shapesTail->next = shape;
	p->shapesTail = shape;
	return;
error:
	if (shape) free(shape);
}
void SvgParse_addPath(SvgParser* p, char closed)
{
	SvgAttributeState* attr = SvgParse_getAttr(p);
	SvgPath* path = NULL;
	float bounds[4];
	float* curve;
	int i;
	if (p->npts < 4)
		return;
	if (closed)
		SvgParse_lineTo(p, p->pts[0], p->pts[1]);
	
	if ((p->npts % 3) != 1)
		return;
	path = (SvgPath*)malloc(sizeof(SvgPath));
	if (path == NULL) goto error;
	memset(path, 0, sizeof(SvgPath));
	path->pts = (float*)malloc(p->npts * 2 * sizeof(float));
	if (path->pts == NULL) goto error;
	path->closed = closed;
	path->npts = p->npts;
	
	for (i = 0; i < p->npts; ++i)
		SvgParse_xformPoint(&path->pts[i * 2], &path->pts[i * 2 + 1], p->pts[i * 2], p->pts[i * 2 + 1], attr->xform);
	
	for (i = 0; i < path->npts - 1; i += 3) {
		curve = &path->pts[i * 2];
		SvgParse_curveBounds(bounds, curve);
		if (i == 0) {
			path->bounds[0] = bounds[0];
			path->bounds[1] = bounds[1];
			path->bounds[2] = bounds[2];
			path->bounds[3] = bounds[3];
		}
		else {
			path->bounds[0] = SvgParse_minf(path->bounds[0], bounds[0]);
			path->bounds[1] = SvgParse_minf(path->bounds[1], bounds[1]);
			path->bounds[2] = SvgParse_maxf(path->bounds[2], bounds[2]);
			path->bounds[3] = SvgParse_maxf(path->bounds[3], bounds[3]);
		}
	}
	path->next = p->plist;
	p->plist = path;
	return;
error:
	if (path != NULL) {
		if (path->pts != NULL) free(path->pts);
		free(path);
	}
}
double SvgParse_atof(const char* s)
{
	char* cur = (char*)s;
	char* end = NULL;
	double res = 0.0, sign = 1.0;
	long long intPart = 0, fracPart = 0;
	char hasIntPart = 0, hasFracPart = 0;
	
	if (*cur == '+') {
		cur++;
	}
	else if (*cur == '-') {
		sign = -1;
		cur++;
	}
	
	if (SvgParse_isdigit(*cur)) {
		
		intPart = strtoll(cur, &end, 10);
		if (cur != end) {
			res = (double)intPart;
			hasIntPart = 1;
			cur = end;
		}
	}
	
	if (*cur == '.') {
		cur++; 
		if (SvgParse_isdigit(*cur)) {
			
			fracPart = strtoll(cur, &end, 10);
			if (cur != end) {
				res += (double)fracPart / pow(10.0, (double)(end - cur));
				hasFracPart = 1;
				cur = end;
			}
		}
	}
	
	if (!hasIntPart && !hasFracPart)
		return 0.0;
	
	if (*cur == 'e' || *cur == 'E') {
		long expPart = 0;
		cur++; 
		expPart = strtol(cur, &end, 10); 
		if (cur != end) {
			res *= pow(10.0, (double)expPart);
		}
	}
	return res * sign;
}
const char* SvgParse_parseNumber(const char* s, char* it, const int size)
{
	const int last = size - 1;
	int i = 0;
	
	if (*s == '-' || *s == '+') {
		if (i < last) it[i++] = *s;
		s++;
	}
	
	while (*s && SvgParse_isdigit(*s)) {
		if (i < last) it[i++] = *s;
		s++;
	}
	if (*s == '.') {
		
		if (i < last) it[i++] = *s;
		s++;
		
		while (*s && SvgParse_isdigit(*s)) {
			if (i < last) it[i++] = *s;
			s++;
		}
	}
	
	if ((*s == 'e' || *s == 'E') && (s[1] != 'm' && s[1] != 'x')) {
		if (i < last) it[i++] = *s;
		s++;
		if (*s == '-' || *s == '+') {
			if (i < last) it[i++] = *s;
			s++;
		}
		while (*s && SvgParse_isdigit(*s)) {
			if (i < last) it[i++] = *s;
			s++;
		}
	}
	it[i] = '\0';
	return s;
}
const char* SvgParse_getNextPathItemWhenArcFlag(const char* s, char* it)
{
	it[0] = '\0';
	while (*s && (SvgParse_isspace(*s) || *s == ',')) s++;
	if (!*s) return s;
	if (*s == '0' || *s == '1') {
		it[0] = *s++;
		it[1] = '\0';
		return s;
	}
	return s;
}
const char* SvgParse_getNextPathItem(const char* s, char* it)
{
	it[0] = '\0';
	
	while (*s && (SvgParse_isspace(*s) || *s == ',')) s++;
	if (!*s) return s;
	if (*s == '-' || *s == '+' || *s == '.' || SvgParse_isdigit(*s)) {
		s = SvgParse_parseNumber(s, it, 64);
	}
	else {
		
		it[0] = *s++;
		it[1] = '\0';
		return s;
	}
	return s;
}
unsigned int SvgParse_parseColorHex(const char* str)
{
	unsigned int r = 0, g = 0, b = 0;
	if (sscanf(str, "#%2x%2x%2x", &r, &g, &b) == 3)		
		return SVG_RGB(r, g, b);
	if (sscanf(str, "#%1x%1x%1x", &r, &g, &b) == 3)		
		return SVG_RGB(r * 17, g * 17, b * 17);			
	return SVG_RGB(128, 128, 128);
}
unsigned int SvgParse_parseColorRGB(const char* str)
{
	int i;
	unsigned int rgbi[3];
	float rgbf[3];
	
	if (sscanf(str, "rgb(%u, %u, %u)", &rgbi[0], &rgbi[1], &rgbi[2]) != 3) {
		
		const char delimiter[3] = { ',', ',', ')' };
		str += 4; 
		for (i = 0; i < 3; i++) {
			while (*str && (SvgParse_isspace(*str))) str++; 	
			if (*str == '+') str++;				
			if (!*str) break;
			rgbf[i] = SvgParse_atof(str);
			
			
			
			
			
			
			while (*str && SvgParse_isdigit(*str)) str++;		
			if (*str == '.') {
				str++;
				if (!SvgParse_isdigit(*str)) break;		
				while (*str && SvgParse_isdigit(*str)) str++;	
			}
			if (*str == '%') str++; else break;
			while (SvgParse_isspace(*str)) str++;
			if (*str == delimiter[i]) str++;
			else break;
		}
		if (i == 3) {
			rgbi[0] = roundf(rgbf[0] * 2.55f);
			rgbi[1] = roundf(rgbf[1] * 2.55f);
			rgbi[2] = roundf(rgbf[2] * 2.55f);
		}
		else {
			rgbi[0] = rgbi[1] = rgbi[2] = 128;
		}
	}
	
	for (i = 0; i < 3; i++) {
		if (rgbi[i] > 255) rgbi[i] = 255;
	}
	return SVG_RGB(rgbi[0], rgbi[1], rgbi[2]);
}
unsigned int SvgParse_parseColorName(const char* str)
{
	int i, ncolors = SvgParse_colors.size();
	for (i = 0; i < ncolors; i++) {
		if (strcmp(SvgParse_colors[i].name, str) == 0) {
			return SvgParse_colors[i].color;
		}
	}
	return SVG_RGB(128, 128, 128);
}
unsigned int SvgParse_parseColor(const char* str)
{
	size_t len = 0;
	while (*str == ' ') ++str;
	len = strlen(str);
	if (len >= 1 && *str == '#')
		return SvgParse_parseColorHex(str);
	else if (len >= 4 && str[0] == 'r' && str[1] == 'g' && str[2] == 'b' && str[3] == '(')
		return SvgParse_parseColorRGB(str);
	return SvgParse_parseColorName(str);
}
float SvgParse_parseOpacity(const char* str)
{
	float val = SvgParse_atof(str);
	if (val < 0.0f) val = 0.0f;
	if (val > 1.0f) val = 1.0f;
	return val;
}
float SvgParse_parseMiterLimit(const char* str)
{
	float val = SvgParse_atof(str);
	if (val < 0.0f) val = 0.0f;
	return val;
}
int SvgParse_parseUnits(const char* units)
{
	if (units[0] == 'p' && units[1] == 'x')
		return SVG_UNITS_PX;
	else if (units[0] == 'p' && units[1] == 't')
		return SVG_UNITS_PT;
	else if (units[0] == 'p' && units[1] == 'c')
		return SVG_UNITS_PC;
	else if (units[0] == 'm' && units[1] == 'm')
		return SVG_UNITS_MM;
	else if (units[0] == 'c' && units[1] == 'm')
		return SVG_UNITS_CM;
	else if (units[0] == 'i' && units[1] == 'n')
		return SVG_UNITS_IN;
	else if (units[0] == '%')
		return SVG_UNITS_PERCENT;
	else if (units[0] == 'e' && units[1] == 'm')
		return SVG_UNITS_EM;
	else if (units[0] == 'e' && units[1] == 'x')
		return SVG_UNITS_EX;
	return SVG_UNITS_USER;
}
int SvgParse_isCoordinate(const char* s)
{
	
	if (*s == '-' || *s == '+')
		s++;
	
	return (SvgParse_isdigit(*s) || *s == '.');
}
SvgCoordinate SvgParse_parseCoordinateRaw(const char* str)
{
	SvgCoordinate coord = { 0, SVG_UNITS_USER };
	char buf[64];
	coord.units = SvgParse_parseUnits(SvgParse_parseNumber(str, buf, 64));
	coord.value = SvgParse_atof(buf);
	return coord;
}
SvgCoordinate SvgParse_coord(float v, int units)
{
	SvgCoordinate coord = { v, units };
	return coord;
}
float SvgParse_parseCoordinate(SvgParser* p, const char* str, float orig, float length)
{
	SvgCoordinate coord = SvgParse_parseCoordinateRaw(str);
	return SvgParse_convertToPixels(p, coord, orig, length);
}
int SvgParse_parseTransformArgs(const char* str, float* args, int maxNa, int* na)
{
	const char* end;
	const char* ptr;
	char it[64];
	*na = 0;
	ptr = str;
	while (*ptr && *ptr != '(') ++ptr;
	if (*ptr == 0)
		return 1;
	end = ptr;
	while (*end && *end != ')') ++end;
	if (*end == 0)
		return 1;
	while (ptr < end) {
		if (*ptr == '-' || *ptr == '+' || *ptr == '.' || SvgParse_isdigit(*ptr)) {
			if (*na >= maxNa) return NULL;
			ptr = SvgParse_parseNumber(ptr, it, 64);
			args[(*na)++] = (float)SvgParse_atof(it);
		}
		else {
			++ptr;
		}
	}
	return (int)(end - str);
}
int SvgParse_parseMatrix(float* xform, const char* str)
{
	float t[6];
	int na = 0;
	int len = SvgParse_parseTransformArgs(str, t, 6, &na);
	if (na != 6) return len;
	memcpy(xform, t, sizeof(float) * 6);
	return len;
}
int SvgParse_parseTranslate(float* xform, const char* str)
{
	float args[2];
	float t[6];
	int na = 0;
	int len = SvgParse_parseTransformArgs(str, args, 2, &na);
	if (na == 1) args[1] = 0.0;
	SvgParse_xformSetTranslation(t, args[0], args[1]);
	memcpy(xform, t, sizeof(float) * 6);
	return len;
}
int SvgParse_parseScale(float* xform, const char* str)
{
	float args[2];
	int na = 0;
	float t[6];
	int len = SvgParse_parseTransformArgs(str, args, 2, &na);
	if (na == 1) args[1] = args[0];
	SvgParse_xformSetScale(t, args[0], args[1]);
	memcpy(xform, t, sizeof(float) * 6);
	return len;
}
int SvgParse_parseSkewX(float* xform, const char* str)
{
	float args[1];
	int na = 0;
	float t[6];
	int len = SvgParse_parseTransformArgs(str, args, 1, &na);
	SvgParse_xformSetSkewX(t, args[0] / 180.0f * SVG_PI);
	memcpy(xform, t, sizeof(float) * 6);
	return len;
}
int SvgParse_parseSkewY(float* xform, const char* str)
{
	float args[1];
	int na = 0;
	float t[6];
	int len = SvgParse_parseTransformArgs(str, args, 1, &na);
	SvgParse_xformSetSkewY(t, args[0] / 180.0f * SVG_PI);
	memcpy(xform, t, sizeof(float) * 6);
	return len;
}
int SvgParse_parseRotate(float* xform, const char* str)
{
	float args[3];
	int na = 0;
	float m[6];
	float t[6];
	int len = SvgParse_parseTransformArgs(str, args, 3, &na);
	if (na == 1)
		args[1] = args[2] = 0.0f;
	SvgParse_xformIdentity(m);
	if (na > 1) {
		SvgParse_xformSetTranslation(t, -args[1], -args[2]);
		SvgParse_xformMultiply(m, t);
	}
	SvgParse_xformSetRotation(t, args[0] / 180.0f * SVG_PI);
	SvgParse_xformMultiply(m, t);
	if (na > 1) {
		SvgParse_xformSetTranslation(t, args[1], args[2]);
		SvgParse_xformMultiply(m, t);
	}
	memcpy(xform, m, sizeof(float) * 6);
	return len;
}
void SvgParse_parseTransform(float* xform, const char* str)
{
	float t[6];
	int len;
	SvgParse_xformIdentity(xform);
	while (*str)
	{
		if (strncmp(str, "matrix", 6) == 0)
			len = SvgParse_parseMatrix(t, str);
		else if (strncmp(str, "translate", 9) == 0)
			len = SvgParse_parseTranslate(t, str);
		else if (strncmp(str, "scale", 5) == 0)
			len = SvgParse_parseScale(t, str);
		else if (strncmp(str, "rotate", 6) == 0)
			len = SvgParse_parseRotate(t, str);
		else if (strncmp(str, "skewX", 5) == 0)
			len = SvgParse_parseSkewX(t, str);
		else if (strncmp(str, "skewY", 5) == 0)
			len = SvgParse_parseSkewY(t, str);
		else {
			++str;
			continue;
		}
		if (len != 0) {
			str += len;
		}
		else {
			++str;
			continue;
		}
		SvgParse_xformPremultiply(xform, t);
	}
}
void SvgParse_parseUrl(char* id, const char* str)
{
	int i = 0;
	str += 4; 
	if (*str && *str == '#')
		str++;
	while (i < 63 && *str && *str != ')') {
		id[i] = *str++;
		i++;
	}
	id[i] = '\0';
}
char SvgParse_parseLineCap(const char* str)
{
	if (strcmp(str, "butt") == 0)
		return SVG_CAP_BUTT;
	else if (strcmp(str, "round") == 0)
		return SVG_CAP_ROUND;
	else if (strcmp(str, "square") == 0)
		return SVG_CAP_SQUARE;
	
	return SVG_CAP_BUTT;
}
char SvgParse_parseLineJoin(const char* str)
{
	if (strcmp(str, "miter") == 0)
		return SVG_JOIN_MITER;
	else if (strcmp(str, "round") == 0)
		return SVG_JOIN_ROUND;
	else if (strcmp(str, "bevel") == 0)
		return SVG_JOIN_BEVEL;
	
	return SVG_JOIN_MITER;
}
char SvgParse_parseFillRule(const char* str)
{
	if (strcmp(str, "nonzero") == 0)
		return SVG_FILLRULE_NONZERO;
	else if (strcmp(str, "evenodd") == 0)
		return SVG_FILLRULE_EVENODD;
	
	return SVG_FILLRULE_NONZERO;
}
const char* SvgParse_getNextDashItem(const char* s, char* it)
{
	int n = 0;
	it[0] = '\0';
	
	while (*s && (SvgParse_isspace(*s) || *s == ',')) s++;
	
	while (*s && (!SvgParse_isspace(*s) && *s != ',')) {
		if (n < 63)
			it[n++] = *s;
		s++;
	}
	it[n++] = '\0';
	return s;
}
int SvgParse_parseStrokeDashArray(SvgParser* p, const char* str, float* strokeDashArray)
{
	char item[64];
	int count = 0, i;
	float sum = 0.0f;
	
	if (str[0] == 'n')
		return NULL;
	
	while (*str) {
		str = SvgParse_getNextDashItem(str, item);
		if (!*item) break;
		if (count < SVG_MAX_DASHES)
			strokeDashArray[count++] = fabsf(SvgParse_parseCoordinate(p, item, 0.0f, SvgParse_actualLength(p)));
	}
	for (i = 0; i < count; i++)
		sum += strokeDashArray[i];
	if (sum <= 1e-6f)
		count = 0;
	return count;
}
int SvgParse_parseAttr(SvgParser* p, const char* name, const char* value)
{
	const auto __parseStyle = [](SvgParser* p, const char* str)
		{
			const auto __parseNameValue = [](SvgParser* p, const char* start, const char* end)
				{
					const char* str;
					const char* val;
					char name[512];
					char value[512];
					int n;
					str = start;
					while (str < end && *str != ':') ++str;
					val = str;
					while (str > start && (*str == ':' || SvgParse_isspace(*str))) --str;
					++str;
					n = (int)(str - start);
					if (n > 511) n = 511;
					if (n) memcpy(name, start, n);
					name[n] = 0;
					while (val < end && (*val == ':' || SvgParse_isspace(*val))) ++val;
					n = (int)(end - val);
					if (n > 511) n = 511;
					if (n) memcpy(value, val, n);
					value[n] = 0;
					return SvgParse_parseAttr(p, name, value);
				};
			const char* start;
			const char* end;
			while (*str) {
				
				while (*str && SvgParse_isspace(*str)) ++str;
				start = str;
				while (*str && *str != ';') ++str;
				end = str;
				
				while (end > start && (*end == ';' || SvgParse_isspace(*end))) --end;
				++end;
				__parseNameValue(p, start, end);
				if (*str) ++str;
			}
		};
	float xform[6];
	SvgAttributeState* attr = SvgParse_getAttr(p);
	if (!attr) return NULL;
	if (strcmp(name, "style") == 0) {
		__parseStyle(p, value);
	}
	else if (strcmp(name, "display") == 0) {
		if (strcmp(value, "none") == 0)
			attr->visible = 0;
		
	}
	else if (strcmp(name, "fill") == 0) {
		if (strcmp(value, "none") == 0) {
			attr->hasFill = 0;
		}
		else if (strncmp(value, "url(", 4) == 0) {
			attr->hasFill = 2;
			SvgParse_parseUrl(attr->fillGradient, value);
		}
		else {
			attr->hasFill = 1;
			attr->fillColor = SvgParse_parseColor(value);
		}
	}
	else if (strcmp(name, "opacity") == 0) {
		attr->opacity = SvgParse_parseOpacity(value);
	}
	else if (strcmp(name, "fill-opacity") == 0) {
		attr->fillOpacity = SvgParse_parseOpacity(value);
	}
	else if (strcmp(name, "stroke") == 0) {
		if (strcmp(value, "none") == 0) {
			attr->hasStroke = 0;
		}
		else if (strncmp(value, "url(", 4) == 0) {
			attr->hasStroke = 2;
			SvgParse_parseUrl(attr->strokeGradient, value);
		}
		else {
			attr->hasStroke = 1;
			attr->strokeColor = SvgParse_parseColor(value);
		}
	}
	else if (strcmp(name, "stroke-width") == 0) {
		attr->strokeWidth = SvgParse_parseCoordinate(p, value, 0.0f, SvgParse_actualLength(p));
	}
	else if (strcmp(name, "stroke-dasharray") == 0) {
		attr->strokeDashCount = SvgParse_parseStrokeDashArray(p, value, attr->strokeDashArray);
	}
	else if (strcmp(name, "stroke-dashoffset") == 0) {
		attr->strokeDashOffset = SvgParse_parseCoordinate(p, value, 0.0f, SvgParse_actualLength(p));
	}
	else if (strcmp(name, "stroke-opacity") == 0) {
		attr->strokeOpacity = SvgParse_parseOpacity(value);
	}
	else if (strcmp(name, "stroke-linecap") == 0) {
		attr->strokeLineCap = SvgParse_parseLineCap(value);
	}
	else if (strcmp(name, "stroke-linejoin") == 0) {
		attr->strokeLineJoin = SvgParse_parseLineJoin(value);
	}
	else if (strcmp(name, "stroke-miterlimit") == 0) {
		attr->miterLimit = SvgParse_parseMiterLimit(value);
	}
	else if (strcmp(name, "fill-rule") == 0) {
		attr->fillRule = SvgParse_parseFillRule(value);
	}
	else if (strcmp(name, "font-size") == 0) {
		attr->fontSize = SvgParse_parseCoordinate(p, value, 0.0f, SvgParse_actualLength(p));
	}
	else if (strcmp(name, "transform") == 0) {
		SvgParse_parseTransform(xform, value);
		SvgParse_xformPremultiply(attr->xform, xform);
	}
	else if (strcmp(name, "stop-color") == 0) {
		attr->stopColor = SvgParse_parseColor(value);
	}
	else if (strcmp(name, "stop-opacity") == 0) {
		attr->stopOpacity = SvgParse_parseOpacity(value);
	}
	else if (strcmp(name, "offset") == 0) {
		attr->stopOffset = SvgParse_parseCoordinate(p, value, 0.0f, 1.0f);
	}
	else if (strcmp(name, "id") == 0) {
		strncpy(attr->id, value, 63);
		attr->id[63] = '\0';
	}
	else {
		return NULL;
	}
	return 1;
}
int SvgParse_parseNameValue(SvgParser* p, const char* start, const char* end)
{
	const char* str;
	const char* val;
	char name[512];
	char value[512];
	int n;
	str = start;
	while (str < end && *str != ':') ++str;
	val = str;
	
	while (str > start && (*str == ':' || SvgParse_isspace(*str))) --str;
	++str;
	n = (int)(str - start);
	if (n > 511) n = 511;
	if (n) memcpy(name, start, n);
	name[n] = 0;
	while (val < end && (*val == ':' || SvgParse_isspace(*val))) ++val;
	n = (int)(end - val);
	if (n > 511) n = 511;
	if (n) memcpy(value, val, n);
	value[n] = 0;
	return SvgParse_parseAttr(p, name, value);
}
void SvgParse_parseStyle(SvgParser* p, const char* str)
{
	const char* start;
	const char* end;
	while (*str) {
		
		while (*str && SvgParse_isspace(*str)) ++str;
		start = str;
		while (*str && *str != ';') ++str;
		end = str;
		
		while (end > start && (*end == ';' || SvgParse_isspace(*end))) --end;
		++end;
		SvgParse_parseNameValue(p, start, end);
		if (*str) ++str;
	}
}
void SvgParse_parseAttribs(SvgParser* p, const char** attr)
{
	int i;
	for (i = 0; attr[i]; i += 2)
	{
		if (strcmp(attr[i], "style") == 0)
			SvgParse_parseStyle(p, attr[i + 1]);
		else
			SvgParse_parseAttr(p, attr[i], attr[i + 1]);
	}
}
int SvgParse_getArgsPerElement(char cmd)
{
	switch (cmd) {
	case 'v':
	case 'V':
	case 'h':
	case 'H':
		return 1;
	case 'm':
	case 'M':
	case 'l':
	case 'L':
	case 't':
	case 'T':
		return 2;
	case 'q':
	case 'Q':
	case 's':
	case 'S':
		return 4;
	case 'c':
	case 'C':
		return 6;
	case 'a':
	case 'A':
		return 7;
	case 'z':
	case 'Z':
		return NULL;
	}
	return -1;
}
void SvgParse_pathMoveTo(SvgParser* p, float* cpx, float* cpy, float* args, int rel)
{
	if (rel) {
		*cpx += args[0];
		*cpy += args[1];
	}
	else {
		*cpx = args[0];
		*cpy = args[1];
	}
	SvgParse_moveTo(p, *cpx, *cpy);
}
void SvgParse_pathLineTo(SvgParser* p, float* cpx, float* cpy, float* args, int rel)
{
	if (rel) {
		*cpx += args[0];
		*cpy += args[1];
	}
	else {
		*cpx = args[0];
		*cpy = args[1];
	}
	SvgParse_lineTo(p, *cpx, *cpy);
}
void SvgParse_pathHLineTo(SvgParser* p, float* cpx, float* cpy, float* args, int rel)
{
	if (rel)
		*cpx += args[0];
	else
		*cpx = args[0];
	SvgParse_lineTo(p, *cpx, *cpy);
}
void SvgParse_pathVLineTo(SvgParser* p, float* cpx, float* cpy, float* args, int rel)
{
	if (rel)
		*cpy += args[0];
	else
		*cpy = args[0];
	SvgParse_lineTo(p, *cpx, *cpy);
}
void SvgParse_pathCubicBezTo(SvgParser* p, float* cpx, float* cpy, float* cpx2, float* cpy2, float* args, int rel)
{
	float x2, y2, cx1, cy1, cx2, cy2;
	if (rel) {
		cx1 = *cpx + args[0];
		cy1 = *cpy + args[1];
		cx2 = *cpx + args[2];
		cy2 = *cpy + args[3];
		x2 = *cpx + args[4];
		y2 = *cpy + args[5];
	}
	else {
		cx1 = args[0];
		cy1 = args[1];
		cx2 = args[2];
		cy2 = args[3];
		x2 = args[4];
		y2 = args[5];
	}
	SvgParse_cubicBezTo(p, cx1, cy1, cx2, cy2, x2, y2);
	*cpx2 = cx2;
	*cpy2 = cy2;
	*cpx = x2;
	*cpy = y2;
}
void SvgParse_pathCubicBezShortTo(SvgParser* p, float* cpx, float* cpy, float* cpx2, float* cpy2, float* args, int rel)
{
	float x1, y1, x2, y2, cx1, cy1, cx2, cy2;
	x1 = *cpx;
	y1 = *cpy;
	if (rel) {
		cx2 = *cpx + args[0];
		cy2 = *cpy + args[1];
		x2 = *cpx + args[2];
		y2 = *cpy + args[3];
	}
	else {
		cx2 = args[0];
		cy2 = args[1];
		x2 = args[2];
		y2 = args[3];
	}
	cx1 = 2 * x1 - *cpx2;
	cy1 = 2 * y1 - *cpy2;
	SvgParse_cubicBezTo(p, cx1, cy1, cx2, cy2, x2, y2);
	*cpx2 = cx2;
	*cpy2 = cy2;
	*cpx = x2;
	*cpy = y2;
}
void SvgParse_pathQuadBezTo(SvgParser* p, float* cpx, float* cpy, float* cpx2, float* cpy2, float* args, int rel)
{
	float x1, y1, x2, y2, cx, cy;
	float cx1, cy1, cx2, cy2;
	x1 = *cpx;
	y1 = *cpy;
	if (rel) {
		cx = *cpx + args[0];
		cy = *cpy + args[1];
		x2 = *cpx + args[2];
		y2 = *cpy + args[3];
	}
	else {
		cx = args[0];
		cy = args[1];
		x2 = args[2];
		y2 = args[3];
	}
	
	cx1 = x1 + 2.0f / 3.0f * (cx - x1);
	cy1 = y1 + 2.0f / 3.0f * (cy - y1);
	cx2 = x2 + 2.0f / 3.0f * (cx - x2);
	cy2 = y2 + 2.0f / 3.0f * (cy - y2);
	SvgParse_cubicBezTo(p, cx1, cy1, cx2, cy2, x2, y2);
	*cpx2 = cx;
	*cpy2 = cy;
	*cpx = x2;
	*cpy = y2;
}
void SvgParse_pathQuadBezShortTo(SvgParser* p, float* cpx, float* cpy, float* cpx2, float* cpy2, float* args, int rel)
{
	float x1, y1, x2, y2, cx, cy;
	float cx1, cy1, cx2, cy2;
	x1 = *cpx;
	y1 = *cpy;
	if (rel) {
		x2 = *cpx + args[0];
		y2 = *cpy + args[1];
	}
	else {
		x2 = args[0];
		y2 = args[1];
	}
	cx = 2 * x1 - *cpx2;
	cy = 2 * y1 - *cpy2;
	
	cx1 = x1 + 2.0f / 3.0f * (cx - x1);
	cy1 = y1 + 2.0f / 3.0f * (cy - y1);
	cx2 = x2 + 2.0f / 3.0f * (cx - x2);
	cy2 = y2 + 2.0f / 3.0f * (cy - y2);
	SvgParse_cubicBezTo(p, cx1, cy1, cx2, cy2, x2, y2);
	*cpx2 = cx;
	*cpy2 = cy;
	*cpx = x2;
	*cpy = y2;
}
float SvgParse_sqr(float x)
{
	return x * x;
}
float SvgParse_vmag(float x, float y)
{
	return sqrtf(x * x + y * y);
}
float SvgParse_vecrat(float ux, float uy, float vx, float vy)
{
	return (ux * vx + uy * vy) / (SvgParse_vmag(ux, uy) * SvgParse_vmag(vx, vy));
}
float SvgParse_vecang(float ux, float uy, float vx, float vy)
{
	float r = SvgParse_vecrat(ux, uy, vx, vy);
	if (r < -1.0f) r = -1.0f;
	if (r > 1.0f) r = 1.0f;
	return ((ux * vy < uy * vx) ? -1.0f : 1.0f) * acosf(r);
}
void SvgParse_pathArcTo(SvgParser* p, float* cpx, float* cpy, float* args, int rel)
{
	
	float rx, ry, rotx;
	float x1, y1, x2, y2, cx, cy, dx, dy, d;
	float x1p, y1p, cxp, cyp, s, sa, sb;
	float ux, uy, vx, vy, a1, da;
	float x, y, tanx, tany, a, px = 0, py = 0, ptanx = 0, ptany = 0, t[6];
	float sinrx, cosrx;
	int fa, fs;
	int i, ndivs;
	float hda, kappa;
	rx = fabsf(args[0]);				
	ry = fabsf(args[1]);				
	rotx = args[2] / 180.0f * SVG_PI;		
	fa = fabsf(args[3]) > 1e-6 ? 1 : 0;	
	fs = fabsf(args[4]) > 1e-6 ? 1 : 0;	
	x1 = *cpx;							
	y1 = *cpy;
	if (rel) {							
		x2 = *cpx + args[5];
		y2 = *cpy + args[6];
	}
	else {
		x2 = args[5];
		y2 = args[6];
	}
	dx = x1 - x2;
	dy = y1 - y2;
	d = sqrtf(dx * dx + dy * dy);
	if (d < 1e-6f || rx < 1e-6f || ry < 1e-6f) {
		
		SvgParse_lineTo(p, x2, y2);
		*cpx = x2;
		*cpy = y2;
		return;
	}
	sinrx = sinf(rotx);
	cosrx = cosf(rotx);
	
	
	
	x1p = cosrx * dx / 2.0f + sinrx * dy / 2.0f;
	y1p = -sinrx * dx / 2.0f + cosrx * dy / 2.0f;
	d = SvgParse_sqr(x1p) / SvgParse_sqr(rx) + SvgParse_sqr(y1p) / SvgParse_sqr(ry);
	if (d > 1) {
		d = sqrtf(d);
		rx *= d;
		ry *= d;
	}
	
	s = 0.0f;
	sa = SvgParse_sqr(rx) * SvgParse_sqr(ry) - SvgParse_sqr(rx) * SvgParse_sqr(y1p) - SvgParse_sqr(ry) * SvgParse_sqr(x1p);
	sb = SvgParse_sqr(rx) * SvgParse_sqr(y1p) + SvgParse_sqr(ry) * SvgParse_sqr(x1p);
	if (sa < 0.0f) sa = 0.0f;
	if (sb > 0.0f)
		s = sqrtf(sa / sb);
	if (fa == fs)
		s = -s;
	cxp = s * rx * y1p / ry;
	cyp = s * -ry * x1p / rx;
	
	cx = (x1 + x2) / 2.0f + cosrx * cxp - sinrx * cyp;
	cy = (y1 + y2) / 2.0f + sinrx * cxp + cosrx * cyp;
	
	ux = (x1p - cxp) / rx;
	uy = (y1p - cyp) / ry;
	vx = (-x1p - cxp) / rx;
	vy = (-y1p - cyp) / ry;
	a1 = SvgParse_vecang(1.0f, 0.0f, ux, uy);	
	da = SvgParse_vecang(ux, uy, vx, vy);		
	
	
	if (fs == 0 && da > 0)
		da -= 2 * SVG_PI;
	else if (fs == 1 && da < 0)
		da += 2 * SVG_PI;
	
	t[0] = cosrx; t[1] = sinrx;
	t[2] = -sinrx; t[3] = cosrx;
	t[4] = cx; t[5] = cy;
	
	
	ndivs = (int)(fabsf(da) / (SVG_PI * 0.5f) + 1.0f);
	hda = (da / (float)ndivs) / 2.0f;
	
	if ((hda < 1e-3f) && (hda > -1e-3f))
		hda *= 0.5f;
	else
		hda = (1.0f - cosf(hda)) / sinf(hda);
	kappa = fabsf(4.0f / 3.0f * hda);
	if (da < 0.0f)
		kappa = -kappa;
	for (i = 0; i <= ndivs; i++) {
		a = a1 + da * ((float)i / (float)ndivs);
		dx = cosf(a);
		dy = sinf(a);
		SvgParse_xformPoint(&x, &y, dx * rx, dy * ry, t); 
		SvgParse_xformVec(&tanx, &tany, -dy * rx * kappa, dx * ry * kappa, t); 
		if (i > 0)
			SvgParse_cubicBezTo(p, px + ptanx, py + ptany, x - tanx, y - tany, x, y);
		px = x;
		py = y;
		ptanx = tanx;
		ptany = tany;
	}
	*cpx = x2;
	*cpy = y2;
}
void SvgParse_parsePath(SvgParser* p, const char** attr)
{
	const char* s = NULL;
	char cmd = '\0';
	float args[10];
	int nargs;
	int rargs = 0;
	char initPoint;
	float cpx, cpy, cpx2, cpy2;
	const char* tmp[4];
	char closedFlag;
	int i;
	char item[64];
	for (i = 0; attr[i]; i += 2) {
		if (strcmp(attr[i], "d") == 0) {
			s = attr[i + 1];
		}
		else {
			tmp[0] = attr[i];
			tmp[1] = attr[i + 1];
			tmp[2] = 0;
			tmp[3] = 0;
			SvgParse_parseAttribs(p, tmp);
		}
	}
	if (s) {
		SvgParse_resetPath(p);
		cpx = 0; cpy = 0;
		cpx2 = 0; cpy2 = 0;
		initPoint = 0;
		closedFlag = 0;
		nargs = 0;
		while (*s) {
			item[0] = '\0';
			if ((cmd == 'A' || cmd == 'a') && (nargs == 3 || nargs == 4))
				s = SvgParse_getNextPathItemWhenArcFlag(s, item);
			if (!*item)
				s = SvgParse_getNextPathItem(s, item);
			if (!*item) break;
			if (cmd != '\0' && SvgParse_isCoordinate(item)) {
				if (nargs < 10)
					args[nargs++] = (float)SvgParse_atof(item);
				if (nargs >= rargs) {
					switch (cmd) {
					case 'm':
					case 'M':
						SvgParse_pathMoveTo(p, &cpx, &cpy, args, cmd == 'm' ? 1 : 0);
						
						
						cmd = (cmd == 'm') ? 'l' : 'L';
						rargs = SvgParse_getArgsPerElement(cmd);
						cpx2 = cpx; cpy2 = cpy;
						initPoint = 1;
						break;
					case 'l':
					case 'L':
						SvgParse_pathLineTo(p, &cpx, &cpy, args, cmd == 'l' ? 1 : 0);
						cpx2 = cpx; cpy2 = cpy;
						break;
					case 'H':
					case 'h':
						SvgParse_pathHLineTo(p, &cpx, &cpy, args, cmd == 'h' ? 1 : 0);
						cpx2 = cpx; cpy2 = cpy;
						break;
					case 'V':
					case 'v':
						SvgParse_pathVLineTo(p, &cpx, &cpy, args, cmd == 'v' ? 1 : 0);
						cpx2 = cpx; cpy2 = cpy;
						break;
					case 'C':
					case 'c':
						SvgParse_pathCubicBezTo(p, &cpx, &cpy, &cpx2, &cpy2, args, cmd == 'c' ? 1 : 0);
						break;
					case 'S':
					case 's':
						SvgParse_pathCubicBezShortTo(p, &cpx, &cpy, &cpx2, &cpy2, args, cmd == 's' ? 1 : 0);
						break;
					case 'Q':
					case 'q':
						SvgParse_pathQuadBezTo(p, &cpx, &cpy, &cpx2, &cpy2, args, cmd == 'q' ? 1 : 0);
						break;
					case 'T':
					case 't':
						SvgParse_pathQuadBezShortTo(p, &cpx, &cpy, &cpx2, &cpy2, args, cmd == 't' ? 1 : 0);
						break;
					case 'A':
					case 'a':
						SvgParse_pathArcTo(p, &cpx, &cpy, args, cmd == 'a' ? 1 : 0);
						cpx2 = cpx; cpy2 = cpy;
						break;
					default:
						if (nargs >= 2) {
							cpx = args[nargs - 2];
							cpy = args[nargs - 1];
							cpx2 = cpx; cpy2 = cpy;
						}
						break;
					}
					nargs = 0;
				}
			}
			else {
				cmd = item[0];
				if (cmd == 'M' || cmd == 'm') {
					
					if (p->npts > 0)
						SvgParse_addPath(p, closedFlag);
					
					SvgParse_resetPath(p);
					closedFlag = 0;
					nargs = 0;
				}
				else if (initPoint == 0) {
					
					cmd = '\0';
				}
				if (cmd == 'Z' || cmd == 'z') {
					closedFlag = 1;
					
					if (p->npts > 0) {
						
						cpx = p->pts[0];
						cpy = p->pts[1];
						cpx2 = cpx; cpy2 = cpy;
						SvgParse_addPath(p, closedFlag);
					}
					
					SvgParse_resetPath(p);
					SvgParse_moveTo(p, cpx, cpy);
					closedFlag = 0;
					nargs = 0;
				}
				rargs = SvgParse_getArgsPerElement(cmd);
				if (rargs == -1) {
					
					cmd = '\0';
					rargs = 0;
				}
			}
		}
		
		if (p->npts)
			SvgParse_addPath(p, closedFlag);
	}
	SvgParse_addShape(p);
}
void SvgParse_parseRect(SvgParser* p, const char** attr)
{
	float x = 0.0f;
	float y = 0.0f;
	float w = 0.0f;
	float h = 0.0f;
	float rx = -1.0f; 
	float ry = -1.0f;
	int i;
	for (i = 0; attr[i]; i += 2) {
		if (!SvgParse_parseAttr(p, attr[i], attr[i + 1])) {
			if (strcmp(attr[i], "x") == 0) x = SvgParse_parseCoordinate(p, attr[i + 1], SvgParse_actualOrigX(p), SvgParse_actualWidth(p));
			if (strcmp(attr[i], "y") == 0) y = SvgParse_parseCoordinate(p, attr[i + 1], SvgParse_actualOrigY(p), SvgParse_actualHeight(p));
			if (strcmp(attr[i], "width") == 0) w = SvgParse_parseCoordinate(p, attr[i + 1], 0.0f, SvgParse_actualWidth(p));
			if (strcmp(attr[i], "height") == 0) h = SvgParse_parseCoordinate(p, attr[i + 1], 0.0f, SvgParse_actualHeight(p));
			if (strcmp(attr[i], "rx") == 0) rx = fabsf(SvgParse_parseCoordinate(p, attr[i + 1], 0.0f, SvgParse_actualWidth(p)));
			if (strcmp(attr[i], "ry") == 0) ry = fabsf(SvgParse_parseCoordinate(p, attr[i + 1], 0.0f, SvgParse_actualHeight(p)));
		}
	}
	if (rx < 0.0f && ry > 0.0f) rx = ry;
	if (ry < 0.0f && rx > 0.0f) ry = rx;
	if (rx < 0.0f) rx = 0.0f;
	if (ry < 0.0f) ry = 0.0f;
	if (rx > w / 2.0f) rx = w / 2.0f;
	if (ry > h / 2.0f) ry = h / 2.0f;
	if (w != 0.0f && h != 0.0f) {
		SvgParse_resetPath(p);
		if (rx < 0.00001f || ry < 0.0001f) {
			SvgParse_moveTo(p, x, y);
			SvgParse_lineTo(p, x + w, y);
			SvgParse_lineTo(p, x + w, y + h);
			SvgParse_lineTo(p, x, y + h);
		}
		else {
			
			SvgParse_moveTo(p, x + rx, y);
			SvgParse_lineTo(p, x + w - rx, y);
			SvgParse_cubicBezTo(p, x + w - rx * (1 - SVG_KAPPA90), y, x + w, y + ry * (1 - SVG_KAPPA90), x + w, y + ry);
			SvgParse_lineTo(p, x + w, y + h - ry);
			SvgParse_cubicBezTo(p, x + w, y + h - ry * (1 - SVG_KAPPA90), x + w - rx * (1 - SVG_KAPPA90), y + h, x + w - rx, y + h);
			SvgParse_lineTo(p, x + rx, y + h);
			SvgParse_cubicBezTo(p, x + rx * (1 - SVG_KAPPA90), y + h, x, y + h - ry * (1 - SVG_KAPPA90), x, y + h - ry);
			SvgParse_lineTo(p, x, y + ry);
			SvgParse_cubicBezTo(p, x, y + ry * (1 - SVG_KAPPA90), x + rx * (1 - SVG_KAPPA90), y, x + rx, y);
		}
		SvgParse_addPath(p, 1);
		SvgParse_addShape(p);
	}
}
void SvgParse_parseCircle(SvgParser* p, const char** attr)
{
	float cx = 0.0f;
	float cy = 0.0f;
	float r = 0.0f;
	int i;
	for (i = 0; attr[i]; i += 2) {
		if (!SvgParse_parseAttr(p, attr[i], attr[i + 1])) {
			if (strcmp(attr[i], "cx") == 0) cx = SvgParse_parseCoordinate(p, attr[i + 1], SvgParse_actualOrigX(p), SvgParse_actualWidth(p));
			if (strcmp(attr[i], "cy") == 0) cy = SvgParse_parseCoordinate(p, attr[i + 1], SvgParse_actualOrigY(p), SvgParse_actualHeight(p));
			if (strcmp(attr[i], "r") == 0) r = fabsf(SvgParse_parseCoordinate(p, attr[i + 1], 0.0f, SvgParse_actualLength(p)));
		}
	}
	if (r > 0.0f) {
		SvgParse_resetPath(p);
		SvgParse_moveTo(p, cx + r, cy);
		SvgParse_cubicBezTo(p, cx + r, cy + r * SVG_KAPPA90, cx + r * SVG_KAPPA90, cy + r, cx, cy + r);
		SvgParse_cubicBezTo(p, cx - r * SVG_KAPPA90, cy + r, cx - r, cy + r * SVG_KAPPA90, cx - r, cy);
		SvgParse_cubicBezTo(p, cx - r, cy - r * SVG_KAPPA90, cx - r * SVG_KAPPA90, cy - r, cx, cy - r);
		SvgParse_cubicBezTo(p, cx + r * SVG_KAPPA90, cy - r, cx + r, cy - r * SVG_KAPPA90, cx + r, cy);
		SvgParse_addPath(p, 1);
		SvgParse_addShape(p);
	}
}
void SvgParse_parseEllipse(SvgParser* p, const char** attr)
{
	float cx = 0.0f;
	float cy = 0.0f;
	float rx = 0.0f;
	float ry = 0.0f;
	int i;
	for (i = 0; attr[i]; i += 2) {
		if (!SvgParse_parseAttr(p, attr[i], attr[i + 1])) {
			if (strcmp(attr[i], "cx") == 0) cx = SvgParse_parseCoordinate(p, attr[i + 1], SvgParse_actualOrigX(p), SvgParse_actualWidth(p));
			if (strcmp(attr[i], "cy") == 0) cy = SvgParse_parseCoordinate(p, attr[i + 1], SvgParse_actualOrigY(p), SvgParse_actualHeight(p));
			if (strcmp(attr[i], "rx") == 0) rx = fabsf(SvgParse_parseCoordinate(p, attr[i + 1], 0.0f, SvgParse_actualWidth(p)));
			if (strcmp(attr[i], "ry") == 0) ry = fabsf(SvgParse_parseCoordinate(p, attr[i + 1], 0.0f, SvgParse_actualHeight(p)));
		}
	}
	if (rx > 0.0f && ry > 0.0f) {
		SvgParse_resetPath(p);
		SvgParse_moveTo(p, cx + rx, cy);
		SvgParse_cubicBezTo(p, cx + rx, cy + ry * SVG_KAPPA90, cx + rx * SVG_KAPPA90, cy + ry, cx, cy + ry);
		SvgParse_cubicBezTo(p, cx - rx * SVG_KAPPA90, cy + ry, cx - rx, cy + ry * SVG_KAPPA90, cx - rx, cy);
		SvgParse_cubicBezTo(p, cx - rx, cy - ry * SVG_KAPPA90, cx - rx * SVG_KAPPA90, cy - ry, cx, cy - ry);
		SvgParse_cubicBezTo(p, cx + rx * SVG_KAPPA90, cy - ry, cx + rx, cy - ry * SVG_KAPPA90, cx + rx, cy);
		SvgParse_addPath(p, 1);
		SvgParse_addShape(p);
	}
}
void SvgParse_parseLine(SvgParser* p, const char** attr)
{
	float x1 = 0.0;
	float y1 = 0.0;
	float x2 = 0.0;
	float y2 = 0.0;
	int i;
	for (i = 0; attr[i]; i += 2) {
		if (!SvgParse_parseAttr(p, attr[i], attr[i + 1])) {
			if (strcmp(attr[i], "x1") == 0) x1 = SvgParse_parseCoordinate(p, attr[i + 1], SvgParse_actualOrigX(p), SvgParse_actualWidth(p));
			if (strcmp(attr[i], "y1") == 0) y1 = SvgParse_parseCoordinate(p, attr[i + 1], SvgParse_actualOrigY(p), SvgParse_actualHeight(p));
			if (strcmp(attr[i], "x2") == 0) x2 = SvgParse_parseCoordinate(p, attr[i + 1], SvgParse_actualOrigX(p), SvgParse_actualWidth(p));
			if (strcmp(attr[i], "y2") == 0) y2 = SvgParse_parseCoordinate(p, attr[i + 1], SvgParse_actualOrigY(p), SvgParse_actualHeight(p));
		}
	}
	SvgParse_resetPath(p);
	SvgParse_moveTo(p, x1, y1);
	SvgParse_lineTo(p, x2, y2);
	SvgParse_addPath(p, 0);
	SvgParse_addShape(p);
}
void SvgParse_parsePoly(SvgParser* p, const char** attr, int closeFlag)
{
	int i;
	const char* s;
	float args[2];
	int nargs, npts = 0;
	char item[64];
	SvgParse_resetPath(p);
	for (i = 0; attr[i]; i += 2) {
		if (!SvgParse_parseAttr(p, attr[i], attr[i + 1])) {
			if (strcmp(attr[i], "points") == 0) {
				s = attr[i + 1];
				nargs = 0;
				while (*s) {
					s = SvgParse_getNextPathItem(s, item);
					args[nargs++] = (float)SvgParse_atof(item);
					if (nargs >= 2) {
						if (npts == 0)
							SvgParse_moveTo(p, args[0], args[1]);
						else
							SvgParse_lineTo(p, args[0], args[1]);
						nargs = 0;
						npts++;
					}
				}
			}
		}
	}
	SvgParse_addPath(p, (char)closeFlag);
	SvgParse_addShape(p);
}
void SvgParse_parseSVG(SvgParser* p, const char** attr)
{
	int i;
	for (i = 0; attr[i]; i += 2) {
		if (!SvgParse_parseAttr(p, attr[i], attr[i + 1])) {
			if (strcmp(attr[i], "width") == 0) {
				p->image->width = SvgParse_parseCoordinate(p, attr[i + 1], 0.0f, 0.0f);
			}
			else if (strcmp(attr[i], "height") == 0) {
				p->image->height = SvgParse_parseCoordinate(p, attr[i + 1], 0.0f, 0.0f);
			}
			else if (strcmp(attr[i], "viewBox") == 0) {
				const char* s = attr[i + 1];
				char buf[64];
				s = SvgParse_parseNumber(s, buf, 64);
				p->viewMinx = SvgParse_atof(buf);
				while (*s && (SvgParse_isspace(*s) || *s == '%' || *s == ',')) s++;
				if (!*s) return;
				s = SvgParse_parseNumber(s, buf, 64);
				p->viewMiny = SvgParse_atof(buf);
				while (*s && (SvgParse_isspace(*s) || *s == '%' || *s == ',')) s++;
				if (!*s) return;
				s = SvgParse_parseNumber(s, buf, 64);
				p->viewWidth = SvgParse_atof(buf);
				while (*s && (SvgParse_isspace(*s) || *s == '%' || *s == ',')) s++;
				if (!*s) return;
				s = SvgParse_parseNumber(s, buf, 64);
				p->viewHeight = SvgParse_atof(buf);
			}
			else if (strcmp(attr[i], "preserveAspectRatio") == 0) {
				if (strstr(attr[i + 1], "none") != 0) {
					
					p->alignType = SVG_ALIGN_NONE;
				}
				else {
					
					if (strstr(attr[i + 1], "xMin") != 0)
						p->alignX = SVG_ALIGN_MIN;
					else if (strstr(attr[i + 1], "xMid") != 0)
						p->alignX = SVG_ALIGN_MID;
					else if (strstr(attr[i + 1], "xMax") != 0)
						p->alignX = SVG_ALIGN_MAX;
					
					if (strstr(attr[i + 1], "yMin") != 0)
						p->alignY = SVG_ALIGN_MIN;
					else if (strstr(attr[i + 1], "yMid") != 0)
						p->alignY = SVG_ALIGN_MID;
					else if (strstr(attr[i + 1], "yMax") != 0)
						p->alignY = SVG_ALIGN_MAX;
					
					p->alignType = SVG_ALIGN_MEET;
					if (strstr(attr[i + 1], "slice") != 0)
						p->alignType = SVG_ALIGN_SLICE;
				}
			}
		}
	}
}
void SvgParse_parseGradient(SvgParser* p, const char** attr, signed char type)
{
	int i;
	SvgGradientData* grad = (SvgGradientData*)malloc(sizeof(SvgGradientData));
	if (grad == NULL) return;
	memset(grad, 0, sizeof(SvgGradientData));
	grad->units = SVG_OBJECT_SPACE;
	grad->type = type;
	if (grad->type == SVG_PAINT_LINEAR_GRADIENT) {
		grad->linear.x1 = SvgParse_coord(0.0f, SVG_UNITS_PERCENT);
		grad->linear.y1 = SvgParse_coord(0.0f, SVG_UNITS_PERCENT);
		grad->linear.x2 = SvgParse_coord(100.0f, SVG_UNITS_PERCENT);
		grad->linear.y2 = SvgParse_coord(0.0f, SVG_UNITS_PERCENT);
	}
	else if (grad->type == SVG_PAINT_RADIAL_GRADIENT) {
		grad->radial.cx = SvgParse_coord(50.0f, SVG_UNITS_PERCENT);
		grad->radial.cy = SvgParse_coord(50.0f, SVG_UNITS_PERCENT);
		grad->radial.r = SvgParse_coord(50.0f, SVG_UNITS_PERCENT);
	}
	SvgParse_xformIdentity(grad->xform);
	for (i = 0; attr[i]; i += 2) {
		if (strcmp(attr[i], "id") == 0) {
			strncpy(grad->id, attr[i + 1], 63);
			grad->id[63] = '\0';
		}
		else if (!SvgParse_parseAttr(p, attr[i], attr[i + 1])) {
			if (strcmp(attr[i], "gradientUnits") == 0) {
				if (strcmp(attr[i + 1], "objectBoundingBox") == 0)
					grad->units = SVG_OBJECT_SPACE;
				else
					grad->units = SVG_USER_SPACE;
			}
			else if (strcmp(attr[i], "gradientTransform") == 0) {
				SvgParse_parseTransform(grad->xform, attr[i + 1]);
			}
			else if (strcmp(attr[i], "cx") == 0) {
				grad->radial.cx = SvgParse_parseCoordinateRaw(attr[i + 1]);
			}
			else if (strcmp(attr[i], "cy") == 0) {
				grad->radial.cy = SvgParse_parseCoordinateRaw(attr[i + 1]);
			}
			else if (strcmp(attr[i], "r") == 0) {
				grad->radial.r = SvgParse_parseCoordinateRaw(attr[i + 1]);
			}
			else if (strcmp(attr[i], "fx") == 0) {
				grad->radial.fx = SvgParse_parseCoordinateRaw(attr[i + 1]);
			}
			else if (strcmp(attr[i], "fy") == 0) {
				grad->radial.fy = SvgParse_parseCoordinateRaw(attr[i + 1]);
			}
			else if (strcmp(attr[i], "x1") == 0) {
				grad->linear.x1 = SvgParse_parseCoordinateRaw(attr[i + 1]);
			}
			else if (strcmp(attr[i], "y1") == 0) {
				grad->linear.y1 = SvgParse_parseCoordinateRaw(attr[i + 1]);
			}
			else if (strcmp(attr[i], "x2") == 0) {
				grad->linear.x2 = SvgParse_parseCoordinateRaw(attr[i + 1]);
			}
			else if (strcmp(attr[i], "y2") == 0) {
				grad->linear.y2 = SvgParse_parseCoordinateRaw(attr[i + 1]);
			}
			else if (strcmp(attr[i], "spreadMethod") == 0) {
				if (strcmp(attr[i + 1], "pad") == 0)
					grad->spread = SVG_SPREAD_PAD;
				else if (strcmp(attr[i + 1], "reflect") == 0)
					grad->spread = SVG_SPREAD_REFLECT;
				else if (strcmp(attr[i + 1], "repeat") == 0)
					grad->spread = SVG_SPREAD_REPEAT;
			}
			else if (strcmp(attr[i], "xlink:href") == 0) {
				const char* href = attr[i + 1];
				strncpy(grad->ref, href + 1, 62);
				grad->ref[62] = '\0';
			}
		}
	}
	grad->next = p->gradients;
	p->gradients = grad;
}
void SvgParse_parseGradientStop(SvgParser* p, const char** attr)
{
	SvgAttributeState* curAttr = SvgParse_getAttr(p);
	SvgGradientData* grad;
	SvgGradientStop* stop;
	int i, idx;
	curAttr->stopOffset = 0;
	curAttr->stopColor = 0;
	curAttr->stopOpacity = 1.0f;
	for (i = 0; attr[i]; i += 2) {
		SvgParse_parseAttr(p, attr[i], attr[i + 1]);
	}
	
	grad = p->gradients;
	if (grad == NULL) return;
	grad->nstops++;
	grad->stops = (SvgGradientStop*)realloc(grad->stops, sizeof(SvgGradientStop) * grad->nstops);
	if (grad->stops == NULL) return;
	
	idx = grad->nstops - 1;
	for (i = 0; i < grad->nstops - 1; i++) {
		if (curAttr->stopOffset < grad->stops[i].offset) {
			idx = i;
			break;
		}
	}
	if (idx != grad->nstops - 1) {
		for (i = grad->nstops - 1; i > idx; i--)
			grad->stops[i] = grad->stops[i - 1];
	}
	stop = &grad->stops[idx];
	stop->color = curAttr->stopColor;
	stop->color |= (unsigned int)(curAttr->stopOpacity * 255) << 24;
	stop->offset = curAttr->stopOffset;
}
void SvgParse_startElement(void* ud, const char* el, const char** attr)
{
	SvgParser* p = (SvgParser*)ud;
	if (p->defsFlag) {
		
		if (strcmp(el, "linearGradient") == 0) {
			SvgParse_parseGradient(p, attr, SVG_PAINT_LINEAR_GRADIENT);
		}
		else if (strcmp(el, "radialGradient") == 0) {
			SvgParse_parseGradient(p, attr, SVG_PAINT_RADIAL_GRADIENT);
		}
		else if (strcmp(el, "stop") == 0) {
			SvgParse_parseGradientStop(p, attr);
		}
		return;
	}
	if (strcmp(el, "g") == 0) {
		SvgParse_pushAttr(p);
		SvgParse_parseAttribs(p, attr);
	}
	else if (strcmp(el, "path") == 0) {
		if (p->pathFlag)	
			return;
		SvgParse_pushAttr(p);
		SvgParse_parsePath(p, attr);
		SvgParse_popAttr(p);
	}
	else if (strcmp(el, "rect") == 0) {
		SvgParse_pushAttr(p);
		SvgParse_parseRect(p, attr);
		SvgParse_popAttr(p);
	}
	else if (strcmp(el, "circle") == 0) {
		SvgParse_pushAttr(p);
		SvgParse_parseCircle(p, attr);
		SvgParse_popAttr(p);
	}
	else if (strcmp(el, "ellipse") == 0) {
		SvgParse_pushAttr(p);
		SvgParse_parseEllipse(p, attr);
		SvgParse_popAttr(p);
	}
	else if (strcmp(el, "line") == 0) {
		SvgParse_pushAttr(p);
		SvgParse_parseLine(p, attr);
		SvgParse_popAttr(p);
	}
	else if (strcmp(el, "polyline") == 0) {
		SvgParse_pushAttr(p);
		SvgParse_parsePoly(p, attr, 0);
		SvgParse_popAttr(p);
	}
	else if (strcmp(el, "polygon") == 0) {
		SvgParse_pushAttr(p);
		SvgParse_parsePoly(p, attr, 1);
		SvgParse_popAttr(p);
	}
	else  if (strcmp(el, "linearGradient") == 0) {
		SvgParse_parseGradient(p, attr, SVG_PAINT_LINEAR_GRADIENT);
	}
	else if (strcmp(el, "radialGradient") == 0) {
		SvgParse_parseGradient(p, attr, SVG_PAINT_RADIAL_GRADIENT);
	}
	else if (strcmp(el, "stop") == 0) {
		SvgParse_parseGradientStop(p, attr);
	}
	else if (strcmp(el, "defs") == 0) {
		p->defsFlag = 1;
	}
	else if (strcmp(el, "svg") == 0) {
		SvgParse_parseSVG(p, attr);
	}
}
void SvgParse_endElement(void* ud, const char* el)
{
	SvgParser* p = (SvgParser*)ud;
	if (strcmp(el, "g") == 0) {
		SvgParse_popAttr(p);
	}
	else if (strcmp(el, "path") == 0) {
		p->pathFlag = 0;
	}
	else if (strcmp(el, "defs") == 0) {
		p->defsFlag = 0;
	}
}
void SvgParse_content(void* ud, const char* s)
{
	SVG_NOTUSED(ud);
	SVG_NOTUSED(s);
	
}
void SvgParse_imageBounds(SvgParser* p, float* bounds)
{
	SvgShape* shape;
	shape = p->image->shapes;
	if (shape == NULL) {
		bounds[0] = bounds[1] = bounds[2] = bounds[3] = 0.0;
		return;
	}
	bounds[0] = shape->bounds[0];
	bounds[1] = shape->bounds[1];
	bounds[2] = shape->bounds[2];
	bounds[3] = shape->bounds[3];
	for (shape = shape->next; shape != NULL; shape = shape->next) {
		bounds[0] = SvgParse_minf(bounds[0], shape->bounds[0]);
		bounds[1] = SvgParse_minf(bounds[1], shape->bounds[1]);
		bounds[2] = SvgParse_maxf(bounds[2], shape->bounds[2]);
		bounds[3] = SvgParse_maxf(bounds[3], shape->bounds[3]);
	}
}
float SvgParse_viewAlign(float content, float container, int type)
{
	if (type == SVG_ALIGN_MIN)
		return NULL;
	else if (type == SVG_ALIGN_MAX)
		return container - content;
	
	return (container - content) * 0.5f;
}
void SvgParse_scaleGradient(SvgGradient* grad, float tx, float ty, float sx, float sy)
{
	float t[6];
	SvgParse_xformSetTranslation(t, tx, ty);
	SvgParse_xformMultiply(grad->xform, t);
	SvgParse_xformSetScale(t, sx, sy);
	SvgParse_xformMultiply(grad->xform, t);
}
void SvgParse_scaleToViewbox(SvgParser* p, const char* units)
{
	SvgShape* shape;
	SvgPath* path;
	float tx, ty, sx, sy, us, bounds[4], t[6], avgs;
	int i;
	float* pt;
	
	SvgParse_imageBounds(p, bounds);
	if (p->viewWidth == 0) {
		if (p->image->width > 0) {
			p->viewWidth = p->image->width;
		}
		else {
			p->viewMinx = bounds[0];
			p->viewWidth = bounds[2] - bounds[0];
		}
	}
	if (p->viewHeight == 0) {
		if (p->image->height > 0) {
			p->viewHeight = p->image->height;
		}
		else {
			p->viewMiny = bounds[1];
			p->viewHeight = bounds[3] - bounds[1];
		}
	}
	if (p->image->width == 0)
		p->image->width = p->viewWidth;
	if (p->image->height == 0)
		p->image->height = p->viewHeight;
	tx = -p->viewMinx;
	ty = -p->viewMiny;
	sx = p->viewWidth > 0 ? p->image->width / p->viewWidth : 0;
	sy = p->viewHeight > 0 ? p->image->height / p->viewHeight : 0;
	
	us = 1.0f / SvgParse_convertToPixels(p, SvgParse_coord(1.0f, SvgParse_parseUnits(units)), 0.0f, 1.0f);
	
	if (p->alignType == SVG_ALIGN_MEET) {
		
		sx = sy = SvgParse_minf(sx, sy);
		tx += SvgParse_viewAlign(p->viewWidth * sx, p->image->width, p->alignX) / sx;
		ty += SvgParse_viewAlign(p->viewHeight * sy, p->image->height, p->alignY) / sy;
	}
	else if (p->alignType == SVG_ALIGN_SLICE) {
		
		sx = sy = SvgParse_maxf(sx, sy);
		tx += SvgParse_viewAlign(p->viewWidth * sx, p->image->width, p->alignX) / sx;
		ty += SvgParse_viewAlign(p->viewHeight * sy, p->image->height, p->alignY) / sy;
	}
	
	sx *= us;
	sy *= us;
	avgs = (sx + sy) / 2.0f;
	for (shape = p->image->shapes; shape != NULL; shape = shape->next) {
		shape->bounds[0] = (shape->bounds[0] + tx) * sx;
		shape->bounds[1] = (shape->bounds[1] + ty) * sy;
		shape->bounds[2] = (shape->bounds[2] + tx) * sx;
		shape->bounds[3] = (shape->bounds[3] + ty) * sy;
		for (path = shape->paths; path != NULL; path = path->next) {
			path->bounds[0] = (path->bounds[0] + tx) * sx;
			path->bounds[1] = (path->bounds[1] + ty) * sy;
			path->bounds[2] = (path->bounds[2] + tx) * sx;
			path->bounds[3] = (path->bounds[3] + ty) * sy;
			for (i = 0; i < path->npts; i++) {
				pt = &path->pts[i * 2];
				pt[0] = (pt[0] + tx) * sx;
				pt[1] = (pt[1] + ty) * sy;
			}
		}
		if (shape->fill.type == SVG_PAINT_LINEAR_GRADIENT || shape->fill.type == SVG_PAINT_RADIAL_GRADIENT) {
			SvgParse_scaleGradient(shape->fill.gradient, tx, ty, sx, sy);
			memcpy(t, shape->fill.gradient->xform, sizeof(float) * 6);
			SvgParse_xformInverse(shape->fill.gradient->xform, t);
		}
		if (shape->stroke.type == SVG_PAINT_LINEAR_GRADIENT || shape->stroke.type == SVG_PAINT_RADIAL_GRADIENT) {
			SvgParse_scaleGradient(shape->stroke.gradient, tx, ty, sx, sy);
			memcpy(t, shape->stroke.gradient->xform, sizeof(float) * 6);
			SvgParse_xformInverse(shape->stroke.gradient->xform, t);
		}
		shape->strokeWidth *= avgs;
		shape->strokeDashOffset *= avgs;
		for (i = 0; i < shape->strokeDashCount; i++)
			shape->strokeDashArray[i] *= avgs;
	}
}
void SvgParse_createGradients(SvgParser* p)
{
	SvgShape* shape;
	for (shape = p->image->shapes; shape != NULL; shape = shape->next) {
		if (shape->fill.type == SVG_PAINT_UNDEF) {
			if (shape->fillGradient[0] != '\0') {
				float inv[6], localBounds[4];
				SvgParse_xformInverse(inv, shape->xform);
				SvgParse_getLocalBounds(localBounds, shape, inv);
				shape->fill.gradient = SvgParse_createGradient(p, shape->fillGradient, localBounds, shape->xform, &shape->fill.type);
			}
			if (shape->fill.type == SVG_PAINT_UNDEF) {
				shape->fill.type = SVG_PAINT_NONE;
			}
		}
		if (shape->stroke.type == SVG_PAINT_UNDEF) {
			if (shape->strokeGradient[0] != '\0') {
				float inv[6], localBounds[4];
				SvgParse_xformInverse(inv, shape->xform);
				SvgParse_getLocalBounds(localBounds, shape, inv);
				shape->stroke.gradient = SvgParse_createGradient(p, shape->strokeGradient, localBounds, shape->xform, &shape->stroke.type);
			}
			if (shape->stroke.type == SVG_PAINT_UNDEF) {
				shape->stroke.type = SVG_PAINT_NONE;
			}
		}
	}
}
SvgImage* ParseSvgImageInternal(char* input, const char* units, float dpi)
{
	SvgParser* p;
	SvgImage* ret = 0;
	p = SvgParse_createParser();
	if (p == NULL) {
		return NULL;
	}
	p->dpi = dpi;
	SvgParse_parseXML(input, SvgParse_startElement, SvgParse_endElement, SvgParse_content, p);
	
	SvgParse_createGradients(p);
	
	SvgParse_scaleToViewbox(p, units);
	ret = p->image;
	p->image = NULL;
	SvgParse_deleteParser(p);
	return ret;
}
SvgPath* DuplicateSvgPathInternal(SvgPath* p)
{
	SvgPath* res = NULL;
	if (p == NULL)
		return NULL;
	res = (SvgPath*)malloc(sizeof(SvgPath));
	if (res == NULL) goto error;
	memset(res, 0, sizeof(SvgPath));
	res->pts = (float*)malloc(p->npts * 2 * sizeof(float));
	if (res->pts == NULL) goto error;
	memcpy(res->pts, p->pts, p->npts * sizeof(float) * 2);
	res->npts = p->npts;
	memcpy(res->bounds, p->bounds, sizeof(p->bounds));
	res->closed = p->closed;
	return res;
error:
	if (res != NULL) {
		free(res->pts);
		free(res);
	}
	return NULL;
}
