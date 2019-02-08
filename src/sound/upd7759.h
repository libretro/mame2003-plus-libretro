#ifndef UPD7759S_H
#define UPD7759S_H

#define MAX_UPD7759 2

/* There are two modes for the uPD7759, selected through the !MD pin.
   This is the mode select input.  High is stand alone, low is slave.
   We're making the assumption that nobody switches modes through
   software. */

#define UPD7759_STANDALONE_MODE     1
#define UPD7759_SLAVE_MODE			0

struct UPD7759_interface
{
	int num;		/* num of upd chips */
	int volume[MAX_UPD7759];
	int region[MAX_UPD7759]; 	/* memory region from which the samples came */
	int mode;		/* standalone or slave mode */
	void (*irqcallback[MAX_UPD7759])(int param);	/* for slave mode only */
};

int UPD7759_sh_start (const struct MachineSound *msound);
void UPD7759_sh_stop (void);

void UPD7759_set_bank_base(int which, offs_t base);

void UPD7759_reset_w (int num, UINT8 data);

void UPD7759_w (int num, UINT8 data);
void UPD7759_port_w (int num, UINT8 data);
void UPD7759_start_w (int num, UINT8 data);
int UPD7759_busy_r (int num);

WRITE_HANDLER( UPD7759_0_reset_w );
WRITE_HANDLER( UPD7759_0_port_w );
WRITE_HANDLER( UPD7759_0_start_w );
READ_HANDLER( UPD7759_0_busy_r );

#endif

