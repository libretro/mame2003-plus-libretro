/***********************************************************

  random.h

  Mame's own random number generator: the generator of current MAME,
  with its state in the save states.

***********************************************************/

#ifndef MAME_RAND_H
#define MAME_RAND_H

#ifdef __cplusplus
extern "C" {
#endif

/* initializes random number generator with a seed */
void mame_srand(unsigned long s);

/* generates a random number on [0,0xffffffff]-interval */
unsigned long mame_rand(void);

/* the same, on [0,0x7fffffff]: use it in place of the C library's rand(),
   whose numbers are not in the save states and differ between hosts */
int mame_rand_int(void);

/* seeds the generator and adds its state to the save states; called once
   at machine init */
void mame_rand_init(void);

#ifdef __cplusplus
}
#endif

#endif
