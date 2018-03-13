/*
 * OhmniLabs systime module, provides some simple
 * timing helper routines.
 *
 * Jared
 */

#include "Cpu.h"
#include "Events.h"
#include "PE_Types.h"

#include "SysTick_PDD.h"

#include "systime.h"

/* Global structure */
struct systime _systime = { 0 };


/* ---- System timer stuff ---- */

void systime_setup()
{
  /* Turn on the system tick, set for max reload value, 1.5 MHz tickrate */
  SysTick_PDD_WriteReloadValueReg(SysTick_BASE_PTR, 0x00ffffff);
  SysTick_PDD_SetClkSource(SysTick_BASE_PTR, SysTick_PDD_CORE_CLOCK_DIV16);
  SysTick_PDD_EnableDevice(SysTick_BASE_PTR, PDD_ENABLE);

  /* CRITICAL: sync the time here! Does not start at zero on reset! */  
  _systime.curr = SysTick_PDD_ReadCurrentValueReg(SysTick_BASE_PTR);
  _systime.last = _systime.curr;
}

void systime_tick()
{
  /* Read the current time */
  _systime.last = _systime.curr;
  _systime.curr = SysTick_PDD_ReadCurrentValueReg(SysTick_BASE_PTR);

  /* Check this */
  if (_systime.startval == 0) {
    _systime.startval = _systime.curr;
  }

  /* For now, use ordering to track wraps, avoid extra
   * reg read to use the wrap bit */
  int didwrap = _systime.curr > _systime.last;

  /* Accumulate here and handle wrapping */
  uint32_t dticks = 
  (didwrap ? _systime.last + 0x1000000 : _systime.last) 
    - _systime.curr;

  /* Increment here */
  _systime.ticks += dticks;
}
