/***************************************************************************

	Generic (PC-style) IDE controller implementation

***************************************************************************/

#include "harddisk.h"


#define MAX_IDE_CONTROLLERS			1

struct ide_interface
{
	void 	(*interrupt)(int state);
	/* optional: a bus master DMA run of 'count' bytes to 'address' of the DMA CPU; */
	/* returns 1 when the driver stored it itself, 0 to go through the memory system */
	int		(*dma_write)(offs_t address, const UINT8 *data, UINT32 count);
};

int ide_controller_init(int which, struct ide_interface *intf);
int ide_controller_init_custom(int which, struct ide_interface *intf, struct hard_disk_file *diskhandle);
void ide_controller_reset(int which);
UINT8 *ide_get_features(int which);

void ide_set_master_password(int which, UINT8 *password);
void ide_set_user_password(int which, UINT8 *password);

READ32_HANDLER( ide_controller32_0_r );
WRITE32_HANDLER( ide_controller32_0_w );
READ32_HANDLER( ide_bus_master32_0_r );
WRITE32_HANDLER( ide_bus_master32_0_w );

READ16_HANDLER( ide_controller16_0_r );
WRITE16_HANDLER( ide_controller16_0_w );
READ16_HANDLER( ide_bus_master16_0_r );
WRITE16_HANDLER( ide_bus_master16_0_w );
