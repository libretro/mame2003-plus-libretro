#ifndef FD1094_H
#define FD1094_H

/* Hitachi FD1094 (encrypted 68000), see machine/fd1094.c.
   DRIVER_INIT: fd1094_driver_init(cpu, key region).
   MACHINE_INIT: fd1094_machine_init().
   When the hardware resets the CPU: fd1094_cpu_reset().
   The opcodes of CPU addresses start-end come decrypted from the ROM at
   rgnoffs: window 0 is the whole ROM at its own address, and
   fd1094_set_window() (after fd1094_driver_init) changes it or adds one. */
void fd1094_driver_init(int cpunum, int keyregion);
void fd1094_machine_init(void);
void fd1094_cpu_reset(void);
void fd1094_set_window(int which, offs_t start, offs_t end, offs_t rgnoffs);

#endif
