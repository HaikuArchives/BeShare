#include <stdio.h>
#include <stdlib.h>
#include <storage/File.h>
#include <app/Message.h>
#include "md5.h"

int main(int argc, char ** argv)
{
   if (argc < 2)
   {
      printf("Usage:  md5 <filename> [numBytes]\n");
      exit(0);
   }
   off_t len = (argc > 2) ? atol(argv[2]) : 0LL;
   off_t origLen = len;
   const char * filename = argv[1];
   if (len > 0) printf("Calculating md5 for the first %Li bytes of [%s]...\n", len, filename);
           else printf("Calculating md5 for [%s]\n", filename);

   BEntry entry(filename, true);
   if (entry.InitCheck() == B_NO_ERROR)
   {
      uint8 digest[16];
      off_t retBytesHashed;
      if (HashFileMD5(entry, len, 0, retBytesHashed, digest, NULL) == B_NO_ERROR)
      {
         if (origLen > 0) printf("Hash of first %Lu bytes: ", len);
                     else printf("Hash of all %Lu bytes: ", len);
         for (int i=0; i<16; i++) printf("%02x ", digest[i]);
         printf("\n");
      }
   }
   else printf("Couldn't open file %s!\n", filename);
   return 0;
}
