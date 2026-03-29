#ifndef NO_GZIP
#  define GUNZIP
#endif
typedef enum {
	HEAD = 16180,
	FLAGS,
	TIME,
	OS,
	EXLEN,
	EXTRA,
	NAME,
	COMMENT,
	HCRC,
	DICTID,
	DICT,
	TYPE,
	TYPEDO,
	STORED,
	COPY_,
	COPY,
	TABLE,
	LENLENS,
	CODELENS,
	LEN_,
	LEN,
	LENEXT,
	DIST,
	DISTEXT,
	MATCH,
	LIT,
	CHECK,
	LENGTH,
	DONE,
	BAD,
	MEM,
	SYNC
} inflate_mode;
struct inflate_state {
	z_streamp strm;
	inflate_mode mode;
	int last;
	int wrap;
	int havedict;
	int flags;
	unsigned dmax;
	unsigned long check;
	unsigned long total;
	gz_headerp head;
	unsigned wbits;
	unsigned wsize;
	unsigned whave;
	unsigned wnext;
	unsigned char FAR* window;
	unsigned long hold;
	unsigned bits;
	unsigned length;
	unsigned offset;
	unsigned extra;
	code const FAR* lencode;
	code const FAR* distcode;
	unsigned lenbits;
	unsigned distbits;
	unsigned ncode;
	unsigned nlen;
	unsigned ndist;
	unsigned have;
	code FAR* next;
	unsigned short lens[320];
	unsigned short work[288];
	code codes[ENOUGH];
	int sane;
	int back;
	unsigned was;
};
