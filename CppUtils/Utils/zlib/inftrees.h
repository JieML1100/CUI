typedef struct {
	unsigned char op;
	unsigned char bits;
	unsigned short val;
} code;
#define ENOUGH_LENS 852
#define ENOUGH_DISTS 592
#define ENOUGH (ENOUGH_LENS+ENOUGH_DISTS)
typedef enum {
	CODES,
	LENS,
	DISTS
} codetype;
int ZLIB_INTERNAL inflate_table OF((codetype type, unsigned short FAR* lens,
	unsigned codes, code FAR* FAR* table,
	unsigned FAR* bits, unsigned short FAR* work));
