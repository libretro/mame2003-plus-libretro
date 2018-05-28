/*********************************************************************

	bootstrap.h
    
    nvram "bootstraps" for games which do not start without service
    mode or other intervention
    
*********************************************************************/    

#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

#include "fileio.h"

extern const struct bin2cFILE avengrgs_bootstrap;

extern const struct bin2cFILE bubblem_bootstrap;

extern const struct bin2cFILE mk2_bootstrap;

extern const struct bin2cFILE mk2chal_bootstrap;

extern const struct bin2cFILE mk2r14_bootstrap;

extern const struct bin2cFILE mk2r21_bootstrap;

extern const struct bin2cFILE mk2r32_bootstrap;

extern const struct bin2cFILE mk2r42_bootstrap;

extern const struct bin2cFILE mk2r91_bootstrap;

/* used for qix, qix2, qixa, qixb */
extern const struct bin2cFILE qix_bootstrap;

extern const struct bin2cFILE rungun_bootstrap;

extern const struct bin2cFILE rungunu_bootstrap;

extern const struct bin2cFILE sinistar_bootstrap;

extern const struct bin2cFILE sinista1_bootstrap;

extern const struct bin2cFILE sinista2_bootstrap;

/* used for zookeep, zookeep2, and zookeep3 */
extern const struct bin2cFILE zookeep_bootstrap;

#endif /* BOOTSTRAP_H */