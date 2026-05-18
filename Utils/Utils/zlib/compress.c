#define ZLIB_INTERNAL
#include "zlib.h"
int ZEXPORT compress2(dest, destLen, source, sourceLen, level)
Bytef* dest;
uLongf* destLen;
const Bytef* source;
uLong sourceLen;
int level;
{
	z_stream stream;
	int err;
	const uInt max = (uInt)-1;
	uLong left;
	left = *destLen;
	*destLen = 0;
	stream.zalloc = (alloc_func)0;
	stream.zfree = (free_func)0;
	stream.opaque = (voidpf)0;
	err = deflateInit(&stream, level);
	if (err != Z_OK) return err;
	stream.next_out = dest;
	stream.avail_out = 0;
	stream.next_in = (z_const Bytef*)source;
	stream.avail_in = 0;
	do {
		if (stream.avail_out == 0) {
			stream.avail_out = left > (uLong)max ? max : (uInt)left;
			left -= stream.avail_out;
		}
		if (stream.avail_in == 0) {
			stream.avail_in = sourceLen > (uLong)max ? max : (uInt)sourceLen;
			sourceLen -= stream.avail_in;
		}
		err = deflate(&stream, sourceLen ? Z_NO_FLUSH : Z_FINISH);
	} while (err == Z_OK);
	*destLen = stream.total_out;
	deflateEnd(&stream);
	return err == Z_STREAM_END ? Z_OK : err;
}
int ZEXPORT compress(dest, destLen, source, sourceLen)
Bytef* dest;
uLongf* destLen;
const Bytef* source;
uLong sourceLen;
{
	return compress2(dest, destLen, source, sourceLen, Z_DEFAULT_COMPRESSION);
}
uLong ZEXPORT compressBound(sourceLen)
uLong sourceLen;
{
	return sourceLen + (sourceLen >> 12) + (sourceLen >> 14) +
		(sourceLen >> 25) + 13;
}
