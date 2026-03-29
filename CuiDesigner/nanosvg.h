#pragma once
#ifndef NANOSVG_H
#define NANOSVG_H

#define NSVG_MAX_DASHES 8
#define NSVG_MAX_ATTR 128
enum NSVGpaintType {
	NSVG_PAINT_UNDEF = -1,
	NSVG_PAINT_NONE = 0,
	NSVG_PAINT_COLOR = 1,
	NSVG_PAINT_LINEAR_GRADIENT = 2,
	NSVG_PAINT_RADIAL_GRADIENT = 3
};
enum NSVGspreadType {
	NSVG_SPREAD_PAD = 0,
	NSVG_SPREAD_REFLECT = 1,
	NSVG_SPREAD_REPEAT = 2
};
enum NSVGlineJoin {
	NSVG_JOIN_MITER = 0,
	NSVG_JOIN_ROUND = 1,
	NSVG_JOIN_BEVEL = 2
};
enum NSVGlineCap {
	NSVG_CAP_BUTT = 0,
	NSVG_CAP_ROUND = 1,
	NSVG_CAP_SQUARE = 2
};
enum NSVGfillRule {
	NSVG_FILLRULE_NONZERO = 0,
	NSVG_FILLRULE_EVENODD = 1
};
enum NSVGflags {
	NSVG_FLAGS_VISIBLE = 0x01
};
enum NSVGgradientUnits {
	NSVG_USER_SPACE = 0,
	NSVG_OBJECT_SPACE = 1
};
enum NSVGunits {
	NSVG_UNITS_USER,
	NSVG_UNITS_PX,
	NSVG_UNITS_PT,
	NSVG_UNITS_PC,
	NSVG_UNITS_MM,
	NSVG_UNITS_CM,
	NSVG_UNITS_IN,
	NSVG_UNITS_PERCENT,
	NSVG_UNITS_EM,
	NSVG_UNITS_EX
};
typedef struct NSVGgradientStop {
	unsigned int color;
	float offset;
} NSVGgradientStop;
typedef struct NSVGgradient {
	float xform[6];
	char spread;
	float fx, fy;
	int nstops;
	NSVGgradientStop stops[1];
} NSVGgradient;
typedef struct NSVGpaint {
	signed char type;
	union {
		unsigned int color;
		NSVGgradient* gradient;
	};
} NSVGpaint;
typedef struct NSVGpath
{
	float* pts;					
	int npts;					
	char closed;				
	float bounds[4];			
	struct NSVGpath* next;		
} NSVGpath;
typedef struct NSVGshape
{
	char id[64];				
	NSVGpaint fill;				
	NSVGpaint stroke;			
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
	NSVGpath* paths;			
	struct NSVGshape* next;		
} NSVGshape;
typedef struct NSVGimage
{
	float width;				
	float height;				
	NSVGshape* shapes;			
} NSVGimage;
typedef struct NSVGNamedColor {
	const char* name;
	unsigned int color;
} NSVGNamedColor;
typedef struct NSVGcoordinate {
	float value;
	int units;
} NSVGcoordinate;
typedef struct NSVGlinearData {
	NSVGcoordinate x1, y1, x2, y2;
} NSVGlinearData;
typedef struct NSVGradialData {
	NSVGcoordinate cx, cy, r, fx, fy;
} NSVGradialData;
typedef struct NSVGgradientData
{
	char id[64];
	char ref[64];
	signed char type;
	union {
		NSVGlinearData linear;
		NSVGradialData radial;
	};
	char spread;
	char units;
	float xform[6];
	int nstops;
	NSVGgradientStop* stops;
	struct NSVGgradientData* next;
} NSVGgradientData;
typedef struct NSVGattrib
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
	float strokeDashArray[NSVG_MAX_DASHES];
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
} NSVGattrib;
typedef struct NSVGparser
{
	NSVGattrib attr[NSVG_MAX_ATTR];
	int attrHead;
	float* pts;
	int npts;
	int cpts;
	NSVGpath* plist;
	NSVGimage* image;
	NSVGgradientData* gradients;
	NSVGshape* shapesTail;
	float viewMinx, viewMiny, viewWidth, viewHeight;
	int alignX, alignY, alignType;
	float dpi;
	char pathFlag;
	char defsFlag;
} NSVGparser;

NSVGimage* nsvgParse(char* input, const char* units, float dpi);
NSVGpath* nsvgDuplicatePath(NSVGpath* p);
void nsvgDelete(NSVGimage* image);
#endif
