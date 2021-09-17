#include <stdlib.h>
unsigned char *save_p;
unsigned char *save_p_copy;
unsigned char *savebuffer;

#define doom_wtohl(x) (long int)(x)
//#define doom_wtohl(x) doom_swap_l(x)

#define doom_swap_l(x) \
        ((long int)((((unsigned long int)(x) & 0x000000ffU) << 24) | \
                             (((unsigned long int)(x) & 0x0000ff00U) <<  8) | \
                             (((unsigned long int)(x) & 0x00ff0000U) >>  8) | \
                             (((unsigned long int)(x) & 0xff000000U) >> 24)))

#define LONG(x) doom_wtohl(x)

typedef enum {
  sk_none=-1, //jff 3/24/98 create unpicked skill setting
  sk_baby=0,
  sk_easy,
  sk_medium,
  sk_hard,
  sk_nightmare
} skill_t;

void main()
{
    int savegamesize = 10;
    int idmusnum = 2;
    int totalleveltimes;

    totalleveltimes = 65534;

    save_p = savebuffer = (unsigned char*)malloc(savegamesize);

    skill_t gameskill = sk_medium;

    *save_p++ = gameskill;
    *save_p++ = idmusnum;

    //*(long*)save_p = LONG(totalleveltimes);

    /*
    unsigned char bt;

    // little endian
    long totalleveltimes_l = (long)totalleveltimes;
    bt = (long)totalleveltimes & 255;
    *save_p++ = bt;

    bt = ((long)totalleveltimes>>8) & 255;
    *save_p++ = bt;

    bt = ((long)totalleveltimes>>16) & 255;
    *save_p++ = bt;

    bt = ((long)totalleveltimes>>24) & 255;
    *save_p++ = bt;
    */

    // little endian
    save_p_copy = save_p;
    *save_p++ = (long)totalleveltimes & 255;
    *save_p++ = ((long)totalleveltimes>>8) & 255;
    *save_p++ = ((long)totalleveltimes>>16) & 255;
    *save_p++ = ((long)totalleveltimes>>24) & 255;

    save_p = save_p_copy;
    unsigned char bt1, bt2, bt3, bt4;
    bt1 = *save_p++;
    bt2 = *save_p++;
    bt3 = *save_p++;
    bt4 = *save_p++;

    long value = (bt4<<24) + (bt3<<16) + (bt2<<8) + bt1;

    free(savebuffer);
}