#include "filter.h"

#include "driver.h"
#include <assert.h>
#include <math.h>
#include <stdlib.h>

static filter* filter_alloc(void) {
	filter* f = malloc(sizeof(filter));
	return f;
}

void filter_free(filter* f) {
	free(f);
}

void filter_state_reset(filter* f, filter_state* s) {
	int i;
	s->prev_mac = 0;
	for(i=0;i<f->order;++i) {
		s->xprev[i] = 0;
	}
}

filter_state* filter_state_alloc(void) {
	int i;
        filter_state* s = malloc(sizeof(filter_state));
	s->prev_mac = 0;
	for(i=0;i<FILTER_ORDER_MAX;++i)
		s->xprev[i] = 0;
	return s;
}

void filter_state_free(filter_state* s) {
	free(s);
}

/****************************************************************************/
/* FIR */

filter_real filter_compute(filter* f, filter_state* s) {
	unsigned order = f->order;
	unsigned midorder = f->order / 2;
	filter_real y = 0;
	unsigned i,j,k;

	/* i == [0] */
	/* j == [-2*midorder] */
	i = s->prev_mac;
	j = i + 1;
	if (j == order)
		j = 0;

	/* x */
	for(k=0;k<midorder;++k) {
		y += f->xcoeffs[midorder-k] * (s->xprev[i] + s->xprev[j]);
		++j;
		if (j == order)
			j = 0;
		if (i == 0)
			i = order - 1;
		else
			--i;
	}
	y += f->xcoeffs[0] * s->xprev[i];

#ifdef FILTER_USE_INT
	return y >> FILTER_INT_FRACT;
#else
	return y;
#endif
}

filter* filter_lp_fir_alloc(double freq, int order) {
	filter* f = filter_alloc();
	unsigned midorder = (order - 1) / 2;
	unsigned i;
	double gain;

	assert( order <= FILTER_ORDER_MAX );
	assert( order % 2 == 1 );
	assert( 0 < freq && freq <= 0.5 );

	/* Compute the antitrasform of the perfect low pass filter */
	gain = 2*freq;
#ifdef FILTER_USE_INT
	f->xcoeffs[0] = gain * (1 << FILTER_INT_FRACT);
#else
	f->xcoeffs[0] = gain;
#endif
	for(i=1;i<=midorder;++i) {
		/* number of the sample starting from 0 to (order-1) included */
		unsigned n = i + midorder;

		/* sample value */
		double c = sin(2*M_PI*freq*i) / (M_PI*i);

		/* apply only one window or none */
		/* double w = 2 - 2*n/(order-1); */ /* Bartlett (triangular) */
		/* double w = 0.5 * (1 - cos(2*M_PI*n/(order-1))); */ /* Hanning */
		double w = 0.54 - 0.46 * cos(2*M_PI*n/(order-1)); /* Hamming */
		/* double w = 0.42 - 0.5 * cos(2*M_PI*n/(order-1)) + 0.08 * cos(4*M_PI*n/(order-1)); */ /* Blackman */

		/* apply the window */
		c *= w;

		/* update the gain */
		gain += 2*c;

		/* insert the coeff */
#ifdef FILTER_USE_INT
		f->xcoeffs[i] = c * (1 << FILTER_INT_FRACT);
#else
		f->xcoeffs[i] = c;
#endif
	}

	/* adjust the gain to be exact 1.0 */
	for(i=0;i<=midorder;++i) {
#ifdef FILTER_USE_INT
		f->xcoeffs[i] /= gain;
#else
		f->xcoeffs[i] = f->xcoeffs[i] * (double)(1 << FILTER_INT_FRAC) / gain;
#endif
	}

	/* decrease the order if the last coeffs are 0 */
	i = midorder;
	while (i > 0 && f->xcoeffs[i] == 0.0)
		--i;

	f->order = i * 2 + 1;

	return f;
}


void filter2_setup(int type, double fc, double d, double gain,
					struct filter2_context *filter)
{
	double w;	/* cutoff freq, in radians/sec */
	double w_squared;
	double den;	/* temp variable */
	double two_over_T = 2*Machine->sample_rate;
	double two_over_T_squared = two_over_T * two_over_T;

	/* calculate digital filter coefficents */
	/*w = 2.0*M_PI*fc; no pre-warping */
	w = Machine->sample_rate*2.0*tan(M_PI*fc/Machine->sample_rate); /* pre-warping */
	w_squared = w*w;

	den = two_over_T_squared + d*w*two_over_T + w_squared;

	filter->a1 = 2.0*(-two_over_T_squared + w_squared)/den;
	filter->a2 = (two_over_T_squared - d*w*two_over_T + w_squared)/den;

	switch (type)
	{
		case FILTER_LOWPASS:
			filter->b0 = filter->b2 = w_squared/den;
			filter->b1 = 2.0*(filter->b0);
			break;
		case FILTER_BANDPASS:
			filter->b0 = d*w*two_over_T/den;
			filter->b1 = 0.0;
			filter->b2 = -(filter->b0);
			break;
		case FILTER_HIGHPASS:
			filter->b0 = filter->b2 = two_over_T_squared/den;
			filter->b1 = -2.0*(filter->b0);
			break;
		default:
			logerror("filter2_setup() - Invalid filter type for 2nd order filter.");
			break;
	}

    filter->b0 *= gain;
    filter->b1 *= gain;
    filter->b2 *= gain;
}


/* Reset the input/output voltages to 0. */
void filter2_reset(struct filter2_context *filter)
{
	filter->x0 = 0;
	filter->x1 = 0;
	filter->x2 = 0;
	filter->y0 = 0;
	filter->y1 = 0;
	filter->y2 = 0;
}


/* Step the filter. */
void filter2_step(struct filter2_context *filter)
{
	filter->y0 = -filter->a1 * filter->y1 - filter->a2 * filter->y2 +
	                filter->b0 * filter->x0 + filter->b1 * filter->x1 + filter->b2 * filter->x2;
	filter->x2 = filter->x1;
	filter->x1 = filter->x0;
	filter->y2 = filter->y1;
	filter->y1 = filter->y0;
}


/* Setup a filter2 structure based on an op-amp multipole bandpass circuit. */
void filter_opamp_m_bandpass_setup(double r1, double r2, double r3, double c1, double c2,
					struct filter2_context *filter)
{
	double	r_in, fc, d, gain;

	if (r1 == 0)
	{
		logerror("filter_opamp_m_bandpass_setup() - r1 can not be 0");
		return;	/* Filter can not be setup.  Undefined results. */
	}

	if (r2 == 0)
	{
		gain = 1;
		r_in = r1;
	}
	else
	{
		gain = r2 / (r1 + r2);
		r_in = 1.0 / (1.0/r1 + 1.0/r2);
	}

	fc = 1.0 / (2 * M_PI * sqrt(r_in * r3 * c1 * c2));
	d = (c1 + c2) / sqrt(r3 / r_in * c1 * c2);
	gain *= -r3 / r_in * c2 / (c1 + c2);

	filter2_setup(FILTER_BANDPASS, fc, d, gain, filter);
}
