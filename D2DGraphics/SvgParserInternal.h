#pragma once
#ifndef SVG_PARSER_INTERNAL_H
#define SVG_PARSER_INTERNAL_H

#define SVG_MAX_DASHES 8
#define SVG_MAX_ATTRIBUTES 128
enum SvgPaintType {
	SVG_PAINT_UNDEF = -1,
	SVG_PAINT_NONE = 0,
	SVG_PAINT_COLOR = 1,
	SVG_PAINT_LINEAR_GRADIENT = 2,
	SVG_PAINT_RADIAL_GRADIENT = 3
};
enum SvgSpreadType {
	SVG_SPREAD_PAD = 0,
	SVG_SPREAD_REFLECT = 1,
	SVG_SPREAD_REPEAT = 2
};
enum SvgLineJoin {
	SVG_JOIN_MITER = 0,
	SVG_JOIN_ROUND = 1,
	SVG_JOIN_BEVEL = 2
};
enum SvgLineCap {
	SVG_CAP_BUTT = 0,
	SVG_CAP_ROUND = 1,
	SVG_CAP_SQUARE = 2
};
enum SvgFillRule {
	SVG_FILLRULE_NONZERO = 0,
	SVG_FILLRULE_EVENODD = 1
};
enum SvgFlags {
	SVG_FLAGS_VISIBLE = 0x01
};
enum SvgGradientUnits {
	SVG_USER_SPACE = 0,
	SVG_OBJECT_SPACE = 1
};
enum SvgUnits {
	SVG_UNITS_USER,
	SVG_UNITS_PX,
	SVG_UNITS_PT,
	SVG_UNITS_PC,
	SVG_UNITS_MM,
	SVG_UNITS_CM,
	SVG_UNITS_IN,
	SVG_UNITS_PERCENT,
	SVG_UNITS_EM,
	SVG_UNITS_EX
};
struct SvgGradientStop {
	unsigned int color;
	float offset;
};
struct SvgGradient {
	float xform[6];
	char spread;
	float fx, fy;
	int nstops;
	SvgGradientStop stops[1];
};
struct SvgPaint {
	signed char type;
	union {
		unsigned int color;
		SvgGradient* gradient;
	};
};
struct SvgPath
{
	float* pts;					
	int npts;					
	char closed;				
	float bounds[4];			
	struct SvgPath* next;		
};
struct SvgShape
{
	char id[64];				
	SvgPaint fill;				
	SvgPaint stroke;			
	float opacity;				
	float strokeWidth;			
	float strokeDashOffset;		
	float strokeDashArray[8];	
	char strokeDashCount;		
	char strokeLineJoin;		
	char strokeLineCap;			
	float miterLimit;			
	char fillRule;				
	unsigned char flags;		
	float bounds[4];			
	char fillGradient[64];		
	char strokeGradient[64];	
	float xform[6];				
	SvgPath* paths;			
	struct SvgShape* next;		
};
struct SvgImage
{
	float width;				
	float height;				
	SvgShape* shapes;			
};
struct SvgNamedColor {
	const char* name;
	unsigned int color;
};
struct SvgCoordinate {
	float value;
	int units;
};
struct SvgLinearData {
	SvgCoordinate x1, y1, x2, y2;
};
struct SvgRadialData {
	SvgCoordinate cx, cy, r, fx, fy;
};
struct SvgGradientData
{
	char id[64];
	char ref[64];
	signed char type;
	union {
		SvgLinearData linear;
		SvgRadialData radial;
	};
	char spread;
	char units;
	float xform[6];
	int nstops;
	SvgGradientStop* stops;
	struct SvgGradientData* next;
};
struct SvgAttributeState
{
	char id[64];
	float xform[6];
	unsigned int fillColor;
	unsigned int strokeColor;
	float opacity;
	float fillOpacity;
	float strokeOpacity;
	char fillGradient[64];
	char strokeGradient[64];
	float strokeWidth;
	float strokeDashOffset;
	float strokeDashArray[SVG_MAX_DASHES];
	int strokeDashCount;
	char strokeLineJoin;
	char strokeLineCap;
	float miterLimit;
	char fillRule;
	float fontSize;
	unsigned int stopColor;
	float stopOpacity;
	float stopOffset;
	char hasFill;
	char hasStroke;
	char visible;
};
struct SvgParser
{
	SvgAttributeState attr[SVG_MAX_ATTRIBUTES];
	int attrHead;
	float* pts;
	int npts;
	int cpts;
	SvgPath* plist;
	SvgImage* image;
	SvgGradientData* gradients;
	SvgShape* shapesTail;
	float viewMinx, viewMiny, viewWidth, viewHeight;
	int alignX, alignY, alignType;
	float dpi;
	char pathFlag;
	char defsFlag;
};

SvgImage* ParseSvgImageInternal(char* input, const char* units, float dpi);
SvgPath* DuplicateSvgPathInternal(SvgPath* p);
void DeleteSvgImageInternal(SvgImage* image);
#endif
