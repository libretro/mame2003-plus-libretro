/*********************************************************************

	bootstrap.h
    
    nvram "bootstraps" for games which do not start without service
    mode or other intervention
    
*********************************************************************/    

#ifndef BOOTSTRAP_H
#define BOOTSTRAP_H

extern const unsigned char  avengrgs_bootstrap_bytes[];
extern const unsigned int   avengrgs_bootstrap_length;

extern const unsigned char  bubblem_bootstrap_bytes[];
extern const unsigned int   bubblem_bootstrap_length;

extern const unsigned char  mk2_bootstrap_bytes[];;
extern const unsigned int   mk2_bootstrap_length;

extern const unsigned char  mk2chal_bootstrap_bytes[];
extern const unsigned int   mk2chal_bootstrap_length;

extern const unsigned char  mk2r14_bootstrap_bytes[];
extern const unsigned int   mk2r14_bootstrap_length;

extern const unsigned char  mk2r21_bootstrap_bytes[];
extern const unsigned int   mk2r21_bootstrap_length;

extern const unsigned char  mk2r32_bootstrap_bytes[];
extern const unsigned int   mk2r32_bootstrap_length;

extern const unsigned char  mk2r42_bootstrap_bytes[];
extern const unsigned int   mk2r42_bootstrap_length;

extern const unsigned char  mk2r91_bootstrap_bytes[];
extern const unsigned int   mk2r91_bootstrap_length;

/* used for qix, qix2, qixa, qixb */
extern const unsigned char  qix_bootstrap_bytes[];
extern const unsigned int   qix_bootstrap_length;

extern const unsigned char  rungun_bootstrap_bytes[];
extern const unsigned int   rungun_bootstrap_length;

extern const unsigned char  sinistar_bootstrap_bytes[];
extern const unsigned int   sinistar_bootstrap_length;

extern const unsigned char  sinista1_bootstrap_bytes[];
extern const unsigned int   sinista1_bootstrap_length;

extern const unsigned char  sinista2_bootstrap_bytes[];
extern const unsigned int   sinista2_bootstrap_length;

/* used for zookeep, zookeep2, and zookeep3 */
extern const unsigned char  zookeep_bootstrap_bytes[];
extern const unsigned int   zookeep_bootstrap_length;

#endif /* BOOTSTRAP_H */