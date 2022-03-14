/* checksum.c: compute the internet checksum. */
/* (c) Edoardo Biagioni, April 2000, minor revisions February 2003 */
/* released under the X11 license -- see "license" for details */

/*
#define DEBUG_PRINT
*/

/* this code expects that buffer is aligned on a multiple of two bytes.
 * this holds if the pointer is even, and is true for most C data
 * structures and arrays.
 */
unsigned short inverted_checksum (char * buffer, unsigned int size) {
  unsigned int i, sum;
  unsigned short * p = (unsigned short *) buffer;

  sum = 0;
  for (i = 0; i < size / 2; i++) {
#ifdef DEBUG_PRINT
    printf ("checksum %04x + %04x = %04x\n",
            (*p) & 0xffff, sum, ((*p) & 0xffff) + sum);
#endif /* DEBUG_PRINT */
    sum += (*(p++)) & 0xffff;
  }
  if ((size & 1) != 0) {        /* odd byte */
    char alignment [2];

    alignment [0] = buffer [size - 1];
    alignment [1] = 0;
    p = (unsigned short *) alignment;
#ifdef DEBUG_PRINT
    printf ("odd checksum %04x + %04x = %04x\n",
            (*p) & 0xffff, sum, ((*p) & 0xffff) + sum);
#endif /* DEBUG_PRINT */
    sum += (*p) & 0xffff;
  }
  while (sum > 0xffff) {   /* add the carries back in to the checksum */
  /* this is how we convert a 2's complement result to 1's complement */
#ifdef DEBUG_PRINT
    printf ("checksum is %04x\n", sum);
#endif /* DEBUG_PRINT */
    sum = (sum >> 16) + (sum & 0xffff);
  }
#ifdef DEBUG_PRINT
  printf ("final checksum is %04x\n", sum);
#endif /* DEBUG_PRINT */
  sum = ((~sum) & 0xffff);
#ifdef DEBUG_PRINT
  printf ("inverted checksum is %04x\n", sum);
#endif /* DEBUG_PRINT */
  return sum;
}
