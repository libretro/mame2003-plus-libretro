#ifndef FD1094_H
#define FD1094_H

/* Hitachi FD1094 (encrypted 68000), see machine/fd1094.c.
   DRIVER_INIT: fd1094_driver_init(cpu, key region).
   MACHINE_INIT: fd1094_machine_init().
   When the hardware resets the CPU: fd1094_cpu_reset(). */
void fd1094_driver_init(int cpunum, int keyregion);
void fd1094_machine_init(void);
void fd1094_cpu_reset(void);

#endif
