#ifndef __SYSTIME_H__
#define __SYSTIME_H__


/* Global structure definition */

struct systime
{
  uint32_t wraps;
  uint32_t last;
  uint32_t curr;

  /* Accumulator */
  uint32_t ticks;

  /* Test value */
  uint32_t startval;
};


/* Global structure */

extern struct systime _systime;

/* Methods */

void systime_setup();

void systime_tick();

#endif
