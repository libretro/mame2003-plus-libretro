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

extern const struct bin2cFILE charlien_bootstrap;

extern const struct bin2cFILE defcmnd_bootstrap;

extern const struct bin2cFILE defence_bootstrap;

extern const struct bin2cFILE defender_bootstrap;

/* used for defendg and defendw */
extern const struct bin2cFILE defendg_bootstrap;

/* used for foodf and foodf2 */
extern const struct bin2cFILE foodf_bootstrap;
extern const struct bin2cFILE foodfc_bootstrap;

/* used for gaiapols */
extern const struct bin2cFILE gaiapols_bootstrap;

/* used for joust, joustr, and joustwr */
extern const struct bin2cFILE joust_bootstrap;
extern const struct bin2cFILE joust2_bootstrap;

/* used for mk2 */
extern const struct bin2cFILE mk2_bootstrap;
extern const struct bin2cFILE mk2chal_bootstrap;
extern const struct bin2cFILE mk2r14_bootstrap;
extern const struct bin2cFILE mk2r21_bootstrap;
extern const struct bin2cFILE mk2r32_bootstrap;
extern const struct bin2cFILE mk2r42_bootstrap;
extern const struct bin2cFILE mk2r91_bootstrap;

/* used for mmaulers */
extern const struct bin2cFILE mmaulers_bootstrap;

/* used for narc, narc3 */
extern const struct bin2cFILE narc_bootstrap;
extern const struct bin2cFILE narc3_bootstrap;

/* used for qix, qix2, qixa, qixb */
extern const struct bin2cFILE qix_bootstrap;

/* used for revx */
extern const struct bin2cFILE revx_bootstrap;

/* used for rmpgwt11, rmpgwt */
extern const struct bin2cFILE rmpgwt11_bootstrap;
extern const struct bin2cFILE rmpgwt_bootstrap;

/* used for robotron, robotryo */
extern const struct bin2cFILE robotron_bootstrap;
extern const struct bin2cFILE robotryo_bootstrap;

/* used for rungun, rungunu */
extern const struct bin2cFILE rungun_bootstrap;
extern const struct bin2cFILE rungunu_bootstrap;

/* used for sinistar, sinista1, sinista2 */
extern const struct bin2cFILE sinistar_bootstrap;
extern const struct bin2cFILE sinista1_bootstrap;
extern const struct bin2cFILE sinista2_bootstrap;

/* used for term2, term2la1, term2la2 */
extern const struct bin2cFILE term2_bootstrap;
extern const struct bin2cFILE term2la1_bootstrap;
extern const struct bin2cFILE term2la2_bootstrap;

/* used for zookeep, zookeep2, and zookeep3 */
extern const struct bin2cFILE zookeep_bootstrap;

#endif /* BOOTSTRAP_H */
