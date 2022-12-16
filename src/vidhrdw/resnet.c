/*****************************************************************************

    resnet.c

    Compute weights for resistors networks.

    Copyright Nicola Salmoria and the MAME Team.
    Visit http://mamedev.org for licensing and usage restrictions.

******************************************************************************

    Function can evaluate from one to three networks at a time.

    The output weights can either be scaled with automatically calculated scaler
    or scaled with a 'scaler' provided on entry.

    On entry
    --------

    'minval','maxval' specify the range of output signals (sum of weights).
    'scaler'          if negative, function will calculate proper scaler,
                        otherwise it will use the one provided here.
    'count_x'         is the number of resistors in this network
    'resistances_x'   is the pointer to a table containing the resistances
    'weights_x'       is the pointer to a table to be filled with the weights
                        (it can contain negative values if 'minval' is below zero).
    'pulldown_x'      is the resistance of a pulldown resistor (0 means there's no pulldown resistor)
    'pullup_x'        is the resistance of a pullup resistor (0 means there's no pullup resistor)


    Return value
    ------------

    The value of the scaler that was used for fitting the output within the expected range.
    Note that if you provide your own scaler on entry it will be returned here.


    All resistances are expected in Ohms.


    Hint
    ----

    If there is no need to calculate all three networks at a time, just specify '0'
    for the 'count_x' for unused network(s).

*****************************************************************************/

#include "driver.h"
#include "res_net.h"

#define VERBOSE 0


double compute_resistor_weights(
	int minval, int maxval, double scaler,
	int count_1, const int * resistances_1, double * weights_1, int pulldown_1, int pullup_1,
	int count_2, const int * resistances_2, double * weights_2, int pulldown_2, int pullup_2,
	int count_3, const int * resistances_3, double * weights_3, int pulldown_3, int pullup_3 )
{

	int networks_no;

	int rescount[MAX_NETS];		/* number of resistors in each of the nets */
	double r[MAX_NETS][MAX_RES_PER_NET];		/* resistances */
	double w[MAX_NETS][MAX_RES_PER_NET];		/* calulated weights */
	double ws[MAX_NETS][MAX_RES_PER_NET];	/* calulated, scaled weights */
	int r_pd[MAX_NETS];			/* pulldown resistances */
	int r_pu[MAX_NETS];			/* pullup resistances */

	double max_out[MAX_NETS];
	double * out[MAX_NETS];

	int i,j,n;
	double scale;
	double max;

	/* parse input parameters */

	networks_no = 0;
	for (n = 0; n < MAX_NETS; n++)
	{
		int count, pd, pu;
		const int * resistances;
		double * weights;

		switch(n){
		case 0:
				count		= count_1;
				resistances	= resistances_1;
				weights		= weights_1;
				pd			= pulldown_1;
				pu			= pullup_1;
				break;
		case 1:
				count		= count_2;
				resistances	= resistances_2;
				weights		= weights_2;
				pd			= pulldown_2;
				pu			= pullup_2;
				break;
		case 2:
		default:
				count		= count_3;
				resistances	= resistances_3;
				weights		= weights_3;
				pd			= pulldown_3;
				pu			= pullup_3;
				break;
		}

		/* parameters validity check */
		if (count > MAX_RES_PER_NET)
		{
			//zerror("compute_resistor_weights(): too many resistors in net #%i. The maximum allowed is %i, the number requested was: %i\n",n, MAX_RES_PER_NET, count);
			log_cb(RETRO_LOG_ERROR,"compute_resistor_weights(): too many resistors in net #%i. The maximum allowed is %i, the number requested was: %i\n",n, MAX_RES_PER_NET, count);
			exit(0);
		}

		if (count > 0)
		{
			rescount[networks_no] = count;
			for (i=0; i < count; i++)
			{
				r[networks_no][i] = 1.0 * resistances[i];
			}
			out[networks_no] = weights;
			r_pd[networks_no] = pd;
			r_pu[networks_no] = pu;
			networks_no++;
		}
	}
	if (networks_no < 1)
	{
		//fatalerror("compute_resistor_weights(): no input data\n");
		log_cb(RETRO_LOG_ERROR, "compute_resistor_weights(): no input data\n");
		exit(0);
	}
	/* calculate outputs for all given networks */
	for( i = 0; i < networks_no; i++ )
	{
		double R0, R1, Vout, dst;

		/* of n resistors */
		for(n = 0; n < rescount[i]; n++)
		{
			R0 = ( r_pd[i] == 0 ) ? 1.0/1e12 : 1.0/r_pd[i];
			R1 = ( r_pu[i] == 0 ) ? 1.0/1e12 : 1.0/r_pu[i];

			for( j = 0; j < rescount[i]; j++ )
			{
				if( j==n )	/* only one resistance in the network connected to Vcc */
				{
					if (r[i][j] != 0.0)
						R1 += 1.0/r[i][j];
				}
				else
					if (r[i][j] != 0.0)
						R0 += 1.0/r[i][j];
			}

			/* now determine the voltage */
			R0 = 1.0/R0;
			R1 = 1.0/R1;
			Vout = (maxval - minval) * R0 / (R1 + R0) + minval;

			/* and convert it to a destination value */
			dst = (Vout < minval) ? minval : (Vout > maxval) ? maxval : Vout;

			w[i][n] = dst;
		}
	}

	/* calculate maximum outputs for all given networks */
	j = 0;
	max = 0.0;
	for( i = 0; i < networks_no; i++ )
	{
		double sum = 0.0;

		/* of n resistors */
		for( n = 0; n < rescount[i]; n++ )
			sum += w[i][n];	/* maximum output, ie when each resistance is connected to Vcc */

		max_out[i] = sum;
		if (max < sum)
		{
			max = sum;
			j = i;
		}
	}


	if (scaler < 0.0)	/* use autoscale ? */
		/* calculate the output scaler according to the network with the greatest output */
		scale = ((double)maxval) / max_out[j];
	else				/* use scaler provided on entry */
		scale = scaler;

	/* calculate scaled output and fill the output table(s)*/
	for(i = 0; i < networks_no;i++)
	{
		for (n = 0; n < rescount[i]; n++)
		{
			ws[i][n] = w[i][n]*scale;	/* scale the result */
			(out[i])[n] = ws[i][n];		/* fill the output table */
		}
	}

/* debug code */
if (VERBOSE)
{
	logerror("compute_resistor_weights():  scaler = %15.10f\n",scale);
	logerror("min val :%i  max val:%i  Total number of networks :%i\n", minval, maxval, networks_no );

	for(i = 0; i < networks_no;i++)
	{
		double sum = 0.0;

		logerror(" Network no.%i=>  resistances: %i", i, rescount[i] );
		if (r_pu[i] != 0)
			logerror(", pullup resistor: %i Ohms",r_pu[i]);
		if (r_pd[i] != 0)
			logerror(", pulldown resistor: %i Ohms",r_pd[i]);
		logerror("\n  maximum output of this network:%10.5f (scaled to %15.10f)\n", max_out[i], max_out[i]*scale );
		for (n = 0; n < rescount[i]; n++)
		{
			logerror("   res %2i:%9.1f Ohms  weight=%10.5f (scaled = %15.10f)\n", n, r[i][n], w[i][n], ws[i][n] );
			sum += ws[i][n];
		}
		logerror("                              sum of scaled weights = %15.10f\n", sum  );
	}
}
/* debug end */

	return (scale);

}


double compute_resistor_net_outputs(
	int minval, int maxval, double scaler,
	int count_1, const int * resistances_1, double * outputs_1, int pulldown_1, int pullup_1,
	int count_2, const int * resistances_2, double * outputs_2, int pulldown_2, int pullup_2,
	int count_3, const int * resistances_3, double * outputs_3, int pulldown_3, int pullup_3 )
{

	int networks_no;

	int rescount[MAX_NETS];		/* number of resistors in each of the nets */
	double r[MAX_NETS][MAX_RES_PER_NET];		/* resistances */
	double *o;					/* calulated outputs */
	double *os;					/* calulated, scaled outputss */
	int r_pd[MAX_NETS];			/* pulldown resistances */
	int r_pu[MAX_NETS];			/* pullup resistances */

	double max_out[MAX_NETS];
	double min_out[MAX_NETS];
	double * out[MAX_NETS];

	int i,j,n;
	double scale;
	double min;
	double max;

	/* parse input parameters */

	o  = (double *) malloc( sizeof(double) * (1<<MAX_RES_PER_NET) *  MAX_NETS);
	os = (double *) malloc( sizeof(double) * (1<<MAX_RES_PER_NET) *  MAX_NETS);

	networks_no = 0;
	for (n = 0; n < MAX_NETS; n++)
	{
		int count, pd, pu;
		const int * resistances;
		double * weights;

		switch(n){
		case 0:
				count		= count_1;
				resistances	= resistances_1;
				weights		= outputs_1;
				pd			= pulldown_1;
				pu			= pullup_1;
				break;
		case 1:
				count		= count_2;
				resistances	= resistances_2;
				weights		= outputs_2;
				pd			= pulldown_2;
				pu			= pullup_2;
				break;
		case 2:
		default:
				count		= count_3;
				resistances	= resistances_3;
				weights		= outputs_3;
				pd			= pulldown_3;
				pu			= pullup_3;
				break;
		}

		/* parameters validity check */
		if (count > MAX_RES_PER_NET)
		{
			//fatalerror("compute_resistor_net_outputs(): too many resistors in net #%i. The maximum allowed is %i, the number requested was: %i\n",n, MAX_RES_PER_NET, count);
			log_cb(RETRO_LOG_ERROR,"compute_resistor_net_outputs(): too many resistors in net #%i. The maximum allowed is %i, the number requested was: %i\n",n, MAX_RES_PER_NET, count);
			exit(0);
		}
		if (count > 0)
		{
			rescount[networks_no] = count;
			for (i=0; i < count; i++)
			{
				r[networks_no][i] = 1.0 * resistances[i];
			}
			out[networks_no] = weights;
			r_pd[networks_no] = pd;
			r_pu[networks_no] = pu;
			networks_no++;
		}
	}

	if (networks_no<1)
	{
		//fatalerror("compute_resistor_net_outputs(): no input data\n");
		log_cb(RETRO_LOG_ERROR,"compute_resistor_net_outputs(): no input data\n");
		exit(0);
	}
	/* calculate outputs for all given networks */
	for( i = 0; i < networks_no; i++ )
	{
		double R0, R1, Vout, dst;

		/* of n resistors, generating 1<<n possible outputs */
		for(n = 0; n < (1<<rescount[i]); n++)
		{
			R0 = ( r_pd[i] == 0 ) ? 1.0/1e12 : 1.0/r_pd[i];
			R1 = ( r_pu[i] == 0 ) ? 1.0/1e12 : 1.0/r_pu[i];

			for( j = 0; j < rescount[i]; j++ )
			{
				if( (n & (1<<j)) == 0 )/* only when this resistance in the network connected to GND */
					if (r[i][j] != 0.0)
						R0 += 1.0/r[i][j];
			}

			/* now determine the voltage */
			R0 = 1.0/R0;
			R1 = 1.0/R1;
			Vout = (maxval - minval) * R0 / (R1 + R0) + minval;

			/* and convert it to a destination value */
			dst = (Vout < minval) ? minval : (Vout > maxval) ? maxval : Vout;

			o[i*(1<<MAX_RES_PER_NET)+n] = dst;
		}
	}

	/* calculate minimum outputs for all given networks */
	j = 0;
	min = maxval;
	max = minval;
	for( i = 0; i < networks_no; i++ )
	{
		double val = 0.0;
		double max_tmp = minval;
		double min_tmp = maxval;

		for (n = 0; n < (1<<rescount[i]); n++)
		{
			if (min_tmp > o[i*(1<<MAX_RES_PER_NET)+n])
				min_tmp = o[i*(1<<MAX_RES_PER_NET)+n];
			if (max_tmp < o[i*(1<<MAX_RES_PER_NET)+n])
				max_tmp = o[i*(1<<MAX_RES_PER_NET)+n];
		}

		max_out[i] = max_tmp;	/* maximum output */
		min_out[i] = min_tmp;	/* minimum output */

		val = min_out[i];	/* minimum output of this network */
		if (min > val)
		{
			min = val;
		}
		val = max_out[i];	/* maximum output of this network */
		if (max < val)
		{
			max = val;
		}
	}


	if (scaler < 0.0)	/* use autoscale ? */
		/* calculate the output scaler according to the network with the smallest output */
		scale = ((double)maxval) / (max-min);
	else				/* use scaler provided on entry */
		scale = scaler;

	/* calculate scaled output and fill the output table(s) */
	for(i = 0; i < networks_no; i++)
	{
		for (n = 0; n < (1<<rescount[i]); n++)
		{
			os[i*(1<<MAX_RES_PER_NET)+n] = (o[i*(1<<MAX_RES_PER_NET)+n] - min) * scale;	/* scale the result */
			(out[i])[n] = os[i*(1<<MAX_RES_PER_NET)+n];		/* fill the output table */
		}
	}

/* debug code */
if (VERBOSE)
{
	logerror("compute_resistor_net_outputs():  scaler = %15.10f\n",scale);
	logerror("min val :%i  max val:%i  Total number of networks :%i\n", minval, maxval, networks_no );

	for(i = 0; i < networks_no;i++)
	{
		logerror(" Network no.%i=>  resistances: %i", i, rescount[i] );
		if (r_pu[i] != 0)
			logerror(", pullup resistor: %i Ohms",r_pu[i]);
		if (r_pd[i] != 0)
			logerror(", pulldown resistor: %i Ohms",r_pd[i]);
		logerror("\n  maximum output of this network:%10.5f", max_out[i] );
		logerror("\n  minimum output of this network:%10.5f\n", min_out[i] );
		for (n = 0; n < rescount[i]; n++)
		{
			logerror("   res %2i:%9.1f Ohms\n", n, r[i][n]);
		}
		for (n = 0; n < (1<<rescount[i]); n++)
		{
			logerror("   combination %2i  out=%10.5f (scaled = %15.10f)\n", n, o[i*(1<<MAX_RES_PER_NET)+n], os[i*(1<<MAX_RES_PER_NET)+n] );
		}
	}
}
/* debug end */

	free(o);
	free(os);
	return (scale);

}

/*****************************************************************************

 New Interface

*****************************************************************************/


/* Datasheets give a maximum of 0.4V to 0.5V
 * However in the circuit simulated here this will only
 * occur if (rBias + rOutn) = 50 Ohm, rBias exists.
 * This is highly unlikely. With the resistor values used
 * in such circuits VOL is likely to be around 50mV.
 */

#define	TTL_VOL			(0.05)


/* Likely, datasheets give a typical value of 3.4V to 3.6V
 * for VOH. Modelling the TTL circuit however backs a value
 * of 4V for typical currents involved in resistor networks.
 */

#define TTL_VOH			(4.0)

int compute_res_net(int inputs, int channel, const res_net_info *di)
{
	double rTotal=0.0;
	double v = 0;
	int i;

	double vBias = di->rgb[channel].vBias;
	double vOH = di->vOH;
	double vOL = di->vOL;
	double minout = di->rgb[channel].minout;
	double cut = di->rgb[channel].cut;
	double vcc = di->vcc;
	double ttlHRes = 0;
	double rGnd = di->rgb[channel].rGnd;
	UINT8  OpenCol = di->OpenCol;

	/* Global options */

	switch (di->options & RES_NET_AMP_MASK)
	{
		case RES_NET_AMP_USE_GLOBAL:
			/* just ignore */
			break;
		case RES_NET_AMP_NONE:
			minout = 0.0;
			cut = 0.0;
			break;
		case RES_NET_AMP_DARLINGTON:
			minout = 0.9;
			cut = 0.0;
			break;
		case RES_NET_AMP_EMITTER:
			minout = 0.0;
			cut = 0.7;
			break;
		case RES_NET_AMP_CUSTOM:
			/* Fall through */
			break;
		default:
			//fatalerror("compute_res_net: Unknown amplifier type");
			log_cb(RETRO_LOG_ERROR,"compute_res_net: Unknown amplifier type");
			exit(0);
	}

	switch (di->options & RES_NET_VCC_MASK)
	{
		case RES_NET_VCC_5V:
			vcc = 5.0;
			break;
		case RES_NET_VCC_CUSTOM:
			/* Fall through */
			break;
		default:
			//fatalerror("compute_res_net: Unknown vcc type");
			log_cb(RETRO_LOG_ERROR,"compute_res_net: Unknown vcc type");
			exit(0);

	}

	switch (di->options & RES_NET_VBIAS_MASK)
	{
		case RES_NET_VBIAS_USE_GLOBAL:
			/* just ignore */
			break;
		case RES_NET_VBIAS_5V:
			vBias = 5.0;
			break;
		case RES_NET_VBIAS_TTL:
			vBias = TTL_VOH;
			break;
		case RES_NET_VBIAS_CUSTOM:
			/* Fall through */
			break;
		default:
			//fatalerror("compute_res_net: Unknown vcc type");
			log_cb(RETRO_LOG_ERROR,"compute_res_net: Unknown vcc type");
			exit(0);
	}

	switch (di->options & RES_NET_VIN_MASK)
	{
		case RES_NET_VIN_OPEN_COL:
			OpenCol = 1;
			vOL = TTL_VOL;
			break;
		case RES_NET_VIN_VCC:
			vOL = 0.0;
			vOH = vcc;
			OpenCol = 0;
			break;
		case RES_NET_VIN_TTL_OUT:
			vOL = TTL_VOL;
			vOH = TTL_VOH;
			/* rough estimation from 82s129 (7052) datasheet and from various sources
             * 1.4k / 30
             */
			ttlHRes = 50;
			OpenCol = 0;
			break;
		case RES_NET_VIN_CUSTOM:
			/* Fall through */
			break;
		default:
			//fatalerror("compute_res_net: Unknown vin type");
			log_cb(RETRO_LOG_ERROR,"compute_res_net: Unknown vin type");
			exit(0);

	}

	/* Per channel options */

	switch (di->rgb[channel].options & RES_NET_AMP_MASK)
	{
		case RES_NET_AMP_USE_GLOBAL:
			/* use global defaults */
			break;
		case RES_NET_AMP_NONE:
			minout = 0.0;
			cut = 0.0;
			break;
		case RES_NET_AMP_DARLINGTON:
			minout = 0.9;
			cut = 0.0;
			break;
		case RES_NET_AMP_EMITTER:
			minout = 0.0;
			cut = 0.7;
			break;
		case RES_NET_AMP_CUSTOM:
			/* Fall through */
			break;
		default:
			//fatalerror("compute_res_net: Unknown amplifier type");
			log_cb(RETRO_LOG_ERROR,"compute_res_net: Unknown amplifier type");
			exit(0);

	}

	switch (di->rgb[channel].options & RES_NET_VBIAS_MASK)
	{
		case RES_NET_VBIAS_USE_GLOBAL:
			/* use global defaults */
			break;
		case RES_NET_VBIAS_5V:
			vBias = 5.0;
			break;
		case RES_NET_VBIAS_TTL:
			vBias = TTL_VOH;
			break;
		case RES_NET_VBIAS_CUSTOM:
			/* Fall through */
			break;
		default:
			//fatalerror("compute_res_net: Unknown vcc type");
			log_cb(RETRO_LOG_ERROR,"compute_res_net: Unknown vcc type");
			exit(0);

	}

	/* Input impedances */

	switch (di->options & RES_NET_MONITOR_MASK)
	{
		case RES_NET_MONITOR_INVERT:
		case RES_NET_MONITOR_SANYO_EZV20:
			/* Nothing */
			break;
		case RES_NET_MONITOR_ELECTROHOME_G07:
			if (rGnd != 0.0)
				rGnd = rGnd * 5600 / (rGnd + 5600);
			else
				rGnd = 5600;
			break;
	}

	/* compute here - pass a / low inputs */

	for (i=0; i<di->rgb[channel].num; i++)
	{
		int level = ((inputs >> i) & 1);
		if (di->rgb[channel].R[i] != 0.0 && !level)
		{
			if (OpenCol)
			{
				rTotal += 1.0 / di->rgb[channel].R[i];
				v += vOL / di->rgb[channel].R[i];
			}
			else
			{
				rTotal += 1.0 / di->rgb[channel].R[i];
				v += vOL / di->rgb[channel].R[i];
			}
		}
	}

	/* Mix in rbias and rgnd */
	if ( di->rgb[channel].rBias != 0.0 )
	{
		rTotal += 1.0 / di->rgb[channel].rBias;
		v += vBias / di->rgb[channel].rBias;
	}
	if (rGnd != 0.0)
		rTotal += 1.0 / rGnd;

	/* if the resulting voltage after application of all low inputs is
     * greater than vOH, treat high inputs as open collector/high impedance
     * There will be now current into/from the TTL gate
     */

	if ( (di->options & RES_NET_VIN_MASK)==RES_NET_VIN_TTL_OUT)
	{
		if (v / rTotal > vOH)
			OpenCol = 1;
	}

	/* Second pass - high inputs */

	for (i=0; i<di->rgb[channel].num; i++)
	{
		int level = ((inputs >> i) & 1);
		if (di->rgb[channel].R[i] != 0.0 && level)
		{
			if (OpenCol)
			{
				rTotal += 0;
				v += 0;
			}
			else
			{
				rTotal += 1.0 / (di->rgb[channel].R[i] + ttlHRes);
				v += vOH / (di->rgb[channel].R[i] + ttlHRes);
			}
		}
	}

	rTotal = 1.0 / rTotal;
	v *= rTotal;
	v = MAX(minout, v - cut);

	switch (di->options & RES_NET_MONITOR_MASK)
	{
		case RES_NET_MONITOR_INVERT:
			v = vcc - v;
			break;
		case RES_NET_MONITOR_SANYO_EZV20:
			v = vcc - v;
			v = MAX(0, v-0.7);
			v = MIN(v, vcc - 2 * 0.7);
			break;
		case RES_NET_MONITOR_ELECTROHOME_G07:
			/* Nothing */
			break;
	}

	return (int) (v *255 / vcc + 0.4);
}

 rgb_t *compute_res_net_all(const UINT8 *prom, const res_net_decode_info *rdi, const res_net_info *di)
{
	UINT8 r,g,b;
	int i,j,k;
	rgb_t *rgb;

	rgb = malloc(rdi->end - rdi->start + 1 * sizeof(rgb_t));
	for (i=rdi->start; i<=rdi->end; i++)
	{
		UINT8 t[3] = {0,0,0};
		int s;
		for (j=0;j<rdi->numcomp;j++)
			for (k=0; k<3; k++)
			{
				s = rdi->shift[3*j+k];
				if (s>0)
					t[k] = t[k] | ( (prom[i+rdi->offset[3*j+k]]>>s) & rdi->mask[3*j+k]);
				else
					t[k] = t[k] | ( (prom[i+rdi->offset[3*j+k]]<<(0-s)) & rdi->mask[3*j+k]);
			}

		r = compute_res_net(t[0], RES_NET_CHAN_RED, di);
		g = compute_res_net(t[1], RES_NET_CHAN_GREEN, di);
		b = compute_res_net(t[2], RES_NET_CHAN_BLUE, di);

		palette_set_color(i,r,g,b);//set the palette here but also pas rgb incase some processing needs done in palete init
		rgb[i-rdi->start] = MAKE_RGB(r,g,b);
	}
	return rgb;
}
