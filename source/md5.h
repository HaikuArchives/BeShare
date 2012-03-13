/*
 * This is the header file for the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 *
 * Changed so as no longer to depend on Colin Plumb's `usual.h'
 * header definitions; now uses stuff from dpkg's config.h
 *  - Ian Jackson <ijackson@nyx.cs.du.edu>.
 * Still in the public domain.
 */

#ifndef MD5_H
#define MD5_H

#include <support/SupportDefs.h>
#include <storage/Entry.h>

#define md5byte unsigned char
#define UWORD32 uint32

struct MD5Context {
   UWORD32 buf[4];
   UWORD32 bytes[2];
   UWORD32 in[16];
};

void MD5Init(struct MD5Context *context);
void MD5Update(struct MD5Context *context, md5byte const *buf, unsigned len);
void MD5Final(unsigned char digest[16], struct MD5Context *context);
void MD5Transform(UWORD32 buf[4], UWORD32 const in[16]);

/* Computes the hash code of the first (len) bytes of the given file.
 * (returnDigest) will have 16 bytes of MD5 hash data written into it. 
 * If (len) is equal to zero, the file size will be detected, used,
 * and returned in (len).
 * if (offset) is greater than zero, then it specifies the byte-offset from the beginning or end of the file
 * to begin reading from.  If (len) is greater than zero, then this offset is taken to be an offset from the
 * beginning of the file; otherwise, the offset is taken to mean an offset back from the end of the file.
 * If (optShutdownFlag) is specified, we will abort if we see it get set to true.
 * (retBytesHashed), on success, will contain the actual number of bytes of the file that were read in.
 */
status_t HashFileMD5(const BEntry & file, off_t & len, off_t offset, off_t & retBytesHashed, uint8 *returnDigest, volatile bool * optShutdownFlag);

#endif /* !MD5_H */
