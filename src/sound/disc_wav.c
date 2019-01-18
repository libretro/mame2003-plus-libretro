/************************************************************************/
/*                                                                      */
/*  MAME - Discrete sound system emulation library                      */
/*                                                                      */
/*  Written by Keith Wilkins (mame@esplexo.co.uk)                       */
/*                                                                      */
/*  (c) K.Wilkins 2000                                                  */
/*                                                                      */
/************************************************************************/
/*                                                                      */
/* DISCRETE_COUNTER      - External clock Binary Counter                */
/* DISCRETE_COUNTER_FIX  - Fixed Frequency Binary Counter               */
/* DSS_LFSR_NOISE        - Linear Feedback Shift Register Noise         */
/* DSS_NOISE             - Noise Source - Random source                 */
/* DSS_SAWTOOTHWAVE      - Sawtooth waveform generator                  */
/* DSS_SCHMITT_OSC       - Schmitt Feedback Oscillator                  */
/* DSS_SINEWAVE          - Sinewave generator source code               */
/* DSS_SQUAREWAVE        - Squarewave generator source code             */
/* DSS_SQUAREWFIX        - Squarewave generator - fixed frequency       */
/* DSS_SQUAREWAVE2       - Squarewave generator - by tOn/tOff           */
/* DSS_TRIANGLEWAVE      - Triangle waveform generator                  */
/*                                                                      */
/************************************************************************/

struct dss_sinewave_context
{
	double phase;
};

struct dss_noise_context
{
	double phase;
};

struct dss_adsr_context
{
	double phase;
};

struct dss_lfsr_context
{
	unsigned int	lfsr_reg;
	double	sampleStep;
	double	shiftStep;
	double	t;
};

struct dss_squarewave_context
{
	double phase;
	double trigger;
};

struct dss_squarewfix_context
{
	int	flip_flop;
	double	sampleStep;
	double	tLeft;
	double	tOff;
	double	tOn;
};

struct dss_trianglewave_context
{
	double phase;
};

struct dss_sawtoothwave_context
{
	double	phase;
	int	type;
};

struct dss_counter_context
{
	int last;		// Last clock state
};

struct dss_counterfix_context
{
	double sampleStep;	// time taken up by 1 audio sample
	double tCycle;		// time period of selected frequency
	double tLeft;		// time left sampling current frequency cycle
};

struct dss_schmitt_osc_context
{
	double	ratioIn;
	double	ratioFeedback;
	double	vCap;
	double	rc;
	double	exponent;
	int	state;		// state of the ouput
};


/************************************************************************/
/*                                                                      */
/* DSS_SINEWAVE - Usage of node_description values for step function    */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Frequency input value                                    */
/* input2    - Amplitude input value                                    */
/* input3    - DC Bias                                                  */
/* input4    - Starting phase                                           */
/*                                                                      */
/************************************************************************/
int dss_sinewave_step(struct node_description *node)
{
	struct dss_sinewave_context *context=(struct dss_sinewave_context*)node->context;

	/* Set the output */
	if(node->input[0])
	{
		node->output=(node->input[2]/2.0) * sin(context->phase);
		/* Add DC Bias component */
		node->output=node->output+node->input[3];
	}
	else
	{
		node->output=0;
	}

	/* Work out the phase step based on phase/freq & sample rate */
	/* The enable input only curtails output, phase rotation     */
	/* still occurs                                              */
	/*     phase step = 2Pi/(output period/sample period)        */
	/*                    boils out to                           */
	/*     phase step = (2Pi*output freq)/sample freq)           */
	/* Also keep the new phasor in the 2Pi range.                */
	context->phase=fmod((context->phase+((2.0*PI*node->input[1])/Machine->sample_rate)),2.0*PI);

	return 0;
}

int dss_sinewave_reset(struct node_description *node)
{
	struct dss_sinewave_context *context;
	double start;
	context=(struct dss_sinewave_context*)node->context;
	/* Establish starting phase, convert from degrees to radians */
	start=(node->input[4]/360.0)*(2.0*PI);
	/* Make sure its always mod 2Pi */
	context->phase=fmod(start,2.0*PI);
	/* Step the output to make it correct */
	dss_sinewave_step(node);
	return 0;
}

int dss_sinewave_init(struct node_description *node)
{
	discrete_log("dss_sinewave_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_sinewave_context)))==NULL)
	{
		discrete_log("dss_sinewave_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_sinewave_context));
	}

	/* Initialize the object */
	dss_sinewave_reset(node);

	return 0;
}


/************************************************************************/
/*                                                                      */
/* DSS_SQUAREWAVE - Usage of node_description values for step function  */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Frequency input value                                    */
/* input2    - Amplitude input value                                    */
/* input3    - Duty Cycle                                               */
/* input4    - DC Bias level                                            */
/* input5    - Start Phase                                              */
/*                                                                      */
/************************************************************************/
int dss_squarewave_step(struct node_description *node)
{
	struct dss_squarewave_context *context=(struct dss_squarewave_context*)node->context;

	/* Establish trigger phase from duty */
	context->trigger=((100-node->input[3])/100)*(2.0*PI);

	/* Set the output */
	if(node->input[0])
	{
		if(context->phase>context->trigger)
			node->output=(node->input[2]/2.0);
		else
			node->output=-(node->input[2]/2.0);

		/* Add DC Bias component */
		node->output=node->output+node->input[4];
	}
	else
	{
		node->output=0;
	}

	/* Work out the phase step based on phase/freq & sample rate */
	/* The enable input only curtails output, phase rotation     */
	/* still occurs                                              */
	/*     phase step = 2Pi/(output period/sample period)        */
	/*                    boils out to                           */
	/*     phase step = (2Pi*output freq)/sample freq)           */
	/* Also keep the new phasor in the 2Pi range.                */
	context->phase=fmod((context->phase+((2.0*PI*node->input[1])/Machine->sample_rate)),2.0*PI);

	return 0;
}

int dss_squarewave_reset(struct node_description *node)
{
	struct dss_squarewave_context *context;
	double start;
	context=(struct dss_squarewave_context*)node->context;

	/* Establish starting phase, convert from degrees to radians */
	start=(node->input[5]/360.0)*(2.0*PI);
	/* Make sure its always mod 2Pi */
	context->phase=fmod(start,2.0*PI);

	/* Step the output */
	dss_squarewave_step(node);

	return 0;
}

int dss_squarewave_init(struct node_description *node)
{
	discrete_log("dss_squarewave_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_squarewave_context)))==NULL)
	{
		discrete_log("dss_squarewave_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_squarewave_context));
	}

	/* Initialize the object */
	dss_squarewave_reset(node);

	return 0;
}

/************************************************************************/
/*                                                                      */
/* DSS_SQUAREWFIX - Usage of node_description values for step function  */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Frequency input value                                    */
/* input2    - Amplitude input value                                    */
/* input3    - Duty Cycle                                               */
/* input4    - DC Bias level                                            */
/* input5    - Start Phase                                              */
/*                                                                      */
/************************************************************************/
int dss_squarewfix_step(struct node_description *node)
{
	struct dss_squarewfix_context *context=(struct dss_squarewfix_context*)node->context;

	context->tLeft -= context->sampleStep;

	/* The enable input only curtails output, phase rotation still occurs */
	while (context->tLeft <= 0)
	{
		context->flip_flop = context->flip_flop ? 0 : 1;
		context->tLeft += context->flip_flop ? context->tOn : context->tOff;
	}

	if(node->input[0])
	{
		/* Add gain and DC Bias component */

		context->tOff = 1.0 / node->input[1];	/* cycle time */
		context->tOn = context->tOff * (node->input[3] / 100.0);
		context->tOff -= context->tOn;

		node->output = (context->flip_flop ? node->input[2] / 2.0 : -(node->input[2] / 2.0)) + node->input[4];
	}
	else
	{
		node->output=0;
	}
	return 0;
}

int dss_squarewfix_reset(struct node_description *node)
{
	struct dss_squarewfix_context *context=(struct dss_squarewfix_context*)node->context;

	context->sampleStep = 1.0 / Machine->sample_rate;
	context->flip_flop = 1;

	/* Do the intial time shift and convert freq to off/on times */
	context->tOff = 1.0 / node->input[1];	/* cycle time */
	context->tLeft = node->input[5] / 360.0;	/* convert start phase to % */
	context->tLeft = context->tLeft - (int)context->tLeft;	/* keep % between 0 & 1 */
	context->tLeft = context->tLeft < 0 ? 1.0 + context->tLeft : context->tLeft;	/* if - then flip to + phase */
	context->tLeft *= context->tOff;
	context->tOn = context->tOff * (node->input[3] / 100.0);
	context->tOff -= context->tOn;

	context->tLeft = -context->tLeft;

	/* toggle output and work out intial time shift */
	while (context->tLeft <= 0)
	{
		context->flip_flop = context->flip_flop ? 0 : 1;
		context->tLeft += context->flip_flop ? context->tOn : context->tOff;
	}

	/* Step the output */
	dss_squarewfix_step(node);

	return 0;
}

int dss_squarewfix_init(struct node_description *node)
{
	discrete_log("dss_squarewfix_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_squarewfix_context)))==NULL)
	{
		discrete_log("dss_squarewfix_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_squarewfix_context));
	}

	/* Initialize the object */
	dss_squarewfix_reset(node);

	return 0;
}


/************************************************************************/
/*                                                                      */
/* DSS_SQUAREWAVE2 - Usage of node_description values                   */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Amplitude input value                                    */
/* input2    - OFF Time                                                 */
/* input3    - ON Time                                                  */
/* input4    - DC Bias level                                            */
/* input5    - Initial Time Shift                                       */
/*                                                                      */
/************************************************************************/
int dss_squarewave2_step(struct node_description *node)
{
	struct dss_squarewave_context *context=(struct dss_squarewave_context*)node->context;
	double newphase;

	/* Establish trigger phase from time periods */
	context->trigger=(node->input[2] / (node->input[2] + node->input[3])) * (2.0 * PI);

	/* Work out the phase step based on phase/freq & sample rate */
	/* The enable input only curtails output, phase rotation     */
	/* still occurs                                              */

	/*     phase step = 2Pi/(output period/sample period)        */
	/*                    boils out to                           */
	/*     phase step = 2Pi/(output period*sample freq)          */
	newphase = context->phase + ((2.0 * PI) / ((node->input[2] + node->input[3]) * Machine->sample_rate));
	/* Keep the new phasor in the 2Pi range.*/
	context->phase = fmod(newphase, 2.0 * PI);

	if(node->input[0])
	{
		if(context->phase>context->trigger)
			node->output=(node->input[1]/2.0);
		else
			node->output=-(node->input[1]/2.0);

		/* Add DC Bias component */
		node->output = node->output + node->input[4];
	}
	else
	{
		node->output=0;
	}
	return 0;
}

int dss_squarewave2_reset(struct node_description *node)
{
	struct dss_squarewave_context *context=(struct dss_squarewave_context*)node->context;
	double start;

	/* Establish starting phase, convert from degrees to radians */
	start = (node->input[5] / (node->input[2] + node->input[3])) * (2.0 * PI);
	/* Make sure its always mod 2Pi */
	context->phase = fmod(start, 2.0 * PI);

	/* Step the output */
	dss_squarewave2_step(node);

	return 0;
}

int dss_squarewave2_init(struct node_description *node)
{
	discrete_log("dss_squarewave_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_squarewave_context)))==NULL)
	{
		discrete_log("dss_squarewave_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_squarewave_context));
	}

	/* Initialize the object */
	dss_squarewave_reset(node);

	return 0;
}


/************************************************************************/
/*                                                                      */
/* DSS_TRIANGLEWAVE - Usage of node_description values for step function*/
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Frequency input value                                    */
/* input2    - Amplitde input value                                     */
/* input3    - DC Bias value                                            */
/* input4    - Initial Phase                                            */
/*                                                                      */
/************************************************************************/
int dss_trianglewave_step(struct node_description *node)
{
	struct dss_trianglewave_context *context=(struct dss_trianglewave_context*)node->context;

	if(node->input[0])
	{
		node->output=context->phase < PI ? (node->input[2] * (context->phase / (PI/2.0) - 1.0))/2.0 :
									(node->input[2] * (3.0 - context->phase / (PI/2.0)))/2.0 ;

		/* Add DC Bias component */
		node->output=node->output+node->input[3];
	}
	else
	{
		node->output=0;
	}

	/* Work out the phase step based on phase/freq & sample rate */
	/* The enable input only curtails output, phase rotation     */
	/* still occurs                                              */
	/*     phase step = 2Pi/(output period/sample period)        */
	/*                    boils out to                           */
	/*     phase step = (2Pi*output freq)/sample freq)           */
	/* Also keep the new phasor in the 2Pi range.                */
	context->phase=fmod((context->phase+((2.0*PI*node->input[1])/Machine->sample_rate)),2.0*PI);

	return 0;
}

int dss_trianglewave_reset(struct node_description *node)
{
	struct dss_trianglewave_context *context;
	double start;

	context=(struct dss_trianglewave_context*)node->context;
	/* Establish starting phase, convert from degrees to radians */
	start=(node->input[4]/360.0)*(2.0*PI);
	/* Make sure its always mod 2Pi */
	context->phase=fmod(start,2.0*PI);

	/* Step to set the output */
	dss_trianglewave_step(node);
	return 0;
}

int dss_trianglewave_init(struct node_description *node)
{
	discrete_log("dss_trianglewave_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_trianglewave_context)))==NULL)
	{
		discrete_log("dss_trianglewave_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_trianglewave_context));
	}

	/* Initialize the object */
	dss_trianglewave_reset(node);
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DSS_SAWTOOTHWAVE - Usage of node_description values for step function*/
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Frequency input value                                    */
/* input2    - Amplitde input value                                     */
/* input3    - DC Bias Value                                            */
/* input4    - Gradient                                                 */
/* input5    - Initial Phase                                            */
/*                                                                      */
/************************************************************************/
int dss_sawtoothwave_step(struct node_description *node)
{
	struct dss_sawtoothwave_context *context=(struct dss_sawtoothwave_context*)node->context;

	if(node->input[0])
	{
		node->output=(context->type==0)?context->phase*(node->input[2]/(2.0*PI)):node->input[2]-(context->phase*(node->input[2]/(2.0*PI)));
		node->output-=node->input[2]/2.0;
		/* Add DC Bias component */
		node->output=node->output+node->input[3];
	}
	else
	{
		node->output=0;
	}

	/* Work out the phase step based on phase/freq & sample rate */
	/* The enable input only curtails output, phase rotation     */
	/* still occurs                                              */
	/*     phase step = 2Pi/(output period/sample period)        */
	/*                    boils out to                           */
	/*     phase step = (2Pi*output freq)/sample freq)           */
	/* Also keep the new phasor in the 2Pi range.                */
	context->phase=fmod((context->phase+((2.0*PI*node->input[1])/Machine->sample_rate)),2.0*PI);

	return 0;
}

int dss_sawtoothwave_reset(struct node_description *node)
{
	struct dss_sawtoothwave_context *context;
	double start;

	context=(struct dss_sawtoothwave_context*)node->context;
	/* Establish starting phase, convert from degrees to radians */
	start=(node->input[5]/360.0)*(2.0*PI);
	/* Make sure its always mod 2Pi */
	context->phase=fmod(start,2.0*PI);

	/* Invert gradient depending on sawtooth type /|/|/|/|/| or |\|\|\|\|\ */
	context->type=(node->input[4])?1:0;

	/* Step the node to set the output */
	dss_sawtoothwave_step(node);

	return 0;
}

int dss_sawtoothwave_init(struct node_description *node)
{
	discrete_log("dss_trianglewave_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_sawtoothwave_context)))==NULL)
	{
		discrete_log("dss_sawtoothwave_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_sawtoothwave_context));
	}

	/* Initialize the object */
	dss_sawtoothwave_reset(node);
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DSS_NOISE - Usage of node_description values for white nose generator*/
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Noise sample frequency                                   */
/* input2    - Amplitude input value                                    */
/* input3    - DC Bias value                                            */
/*                                                                      */
/************************************************************************/
int dss_noise_step(struct node_description *node)
{
	struct dss_noise_context *context;
	context=(struct dss_noise_context*)node->context;

	if(node->input[0])
	{
		/* Only sample noise on rollover to next cycle */
		if(context->phase>(2.0*PI))
		{
			int newval=rand() & 0x7fff;
			node->output=node->input[2]*(1-(newval/16384.0));

			/* Add DC Bias component */
			node->output=node->output+node->input[3];
		}
	}
	else
	{
		node->output=0;
	}

	/* Work out the phase step based on phase/freq & sample rate */
	/* The enable input only curtails output, phase rotation     */
	/* still occurs                                              */
	/*     phase step = 2Pi/(output period/sample period)        */
	/*                    boils out to                           */
	/*     phase step = (2Pi*output freq)/sample freq)           */
	/* Also keep the new phasor in the 2Pi range.                */
	context->phase=fmod((context->phase+((2.0*PI*node->input[1])/Machine->sample_rate)),2.0*PI);

	return 0;
}


int dss_noise_reset(struct node_description *node)
{
	struct dss_noise_context *context=(struct dss_noise_context*)node->context;
	context->phase=0;
	dss_noise_step(node);
	return 0;
}

int dss_noise_init(struct node_description *node)
{
	discrete_log("dss_noise_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_noise_context)))==NULL)
	{
		discrete_log("dss_noise_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_noise_context));
	}

	/* Initialize the object */
	dss_noise_reset(node);

	return 0;
}

/************************************************************************/
/*                                                                      */
/* DSS_LFSR_NOISE - Usage of node_description values for LFSR noise gen */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Register reset                                           */
/* input2    - Noise sample frequency                                   */
/* input3    - Amplitude input value                                    */
/* input4    - Input feed bit                                           */
/* input5    - Bias                                                     */
/*                                                                      */
/************************************************************************/
int	dss_lfsr_function(int myfunc,int in0,int in1,int bitmask)
{
	int retval;

	in0&=bitmask;
	in1&=bitmask;

	switch(myfunc)
	{
		case DISC_LFSR_XOR:
			retval=in0^in1;
			break;
		case DISC_LFSR_OR:
			retval=in0|in1;
			break;
		case DISC_LFSR_AND:
			retval=in0&in1;
			break;
		case DISC_LFSR_XNOR:
			retval=in0^in1;
			retval=retval^bitmask;	/* Invert output */
			break;
		case DISC_LFSR_NOR:
			retval=in0|in1;
			retval=retval^bitmask;	/* Invert output */
			break;
		case DISC_LFSR_NAND:
			retval=in0&in1;
			retval=retval^bitmask;	/* Invert output */
			break;
		case DISC_LFSR_IN0:
			retval=in0;
			break;
		case DISC_LFSR_IN1:
			retval=in1;
			break;
		case DISC_LFSR_NOT_IN0:
			retval=in0^bitmask;
			break;
		case DISC_LFSR_NOT_IN1:
			retval=in1^bitmask;
			break;
		case DISC_LFSR_REPLACE:
			retval=in0&~in1;
			retval=in0|in1;
			break;
		default:
			discrete_log("dss_lfsr_function - Invalid function type passed");
			retval=0;
			break;
	}
	return retval;
}

/* reset prototype so that it can be used in init function */
int dss_lfsr_reset(struct node_description *node);

int dss_lfsr_step(struct node_description *node)
{
	struct dss_lfsr_context *context;
	double shiftAmount;
	int fb0,fb1,fbresult,i;
	struct discrete_lfsr_desc *lfsr_desc;
	context=(struct dss_lfsr_context*)node->context;

	/* Fetch the LFSR descriptor structure in a local for quick ref */
	lfsr_desc=(struct discrete_lfsr_desc*)(node->custom);

	/* Reset everything if necessary */
	if((node->input[1] ? 1 : 0) == ((lfsr_desc->flags & DISC_LFSR_FLAG_RESET_TYPE_H) ? 1 : 0))
	{
		dss_lfsr_reset(node);
	}

	i=0;
	/* Calculate the number of full shift register cycles since last machine sample. */
	shiftAmount = ((context->sampleStep + context->t) / context->shiftStep);
	context->t = (shiftAmount - (int)shiftAmount) * context->shiftStep;    /* left over amount of time */
	while(i<(int)shiftAmount)
	{
		i++;
		/* Now clock the LFSR by 1 cycle and output */

		/* Fetch the last feedback result */
		fbresult=((context->lfsr_reg)>>(lfsr_desc->bitlength))&0x01;

		/* Stage 2 feedback combine fbresultNew with infeed bit */
		fbresult=dss_lfsr_function(lfsr_desc->feedback_function1,fbresult,((node->input[4])?0x01:0x00),0x01);

		/* Stage 3 first we setup where the bit is going to be shifted into */
		fbresult=fbresult*lfsr_desc->feedback_function2_mask;
		/* Then we left shift the register, */
		context->lfsr_reg=(context->lfsr_reg)<<1;
		/* Now move the fbresult into the shift register and mask it to the bitlength */
		context->lfsr_reg=dss_lfsr_function(lfsr_desc->feedback_function2,fbresult, (context->lfsr_reg), ((1<<(lfsr_desc->bitlength))-1));

		/* Now get and store the new feedback result */
		/* Fetch the feedback bits */
		fb0=((context->lfsr_reg)>>(lfsr_desc->feedback_bitsel0))&0x01;
		fb1=((context->lfsr_reg)>>(lfsr_desc->feedback_bitsel1))&0x01;
		/* Now do the combo on them */
		fbresult=dss_lfsr_function(lfsr_desc->feedback_function0,fb0,fb1,0x01);
		context->lfsr_reg=dss_lfsr_function(DISC_LFSR_REPLACE,(context->lfsr_reg), fbresult<<(lfsr_desc->bitlength), ((2<<(lfsr_desc->bitlength))-1));

		/* Now select the output bit */
		node->output=((context->lfsr_reg)>>(lfsr_desc->output_bit))&0x01;

		/* Final inversion if required */
		if(lfsr_desc->flags & DISC_LFSR_FLAG_OUT_INVERT) node->output=(node->output)?0.0:1.0;

		/* Gain stage */
		node->output=(node->output)?(node->input[3])/2:-(node->input[3])/2;
		/* Bias input as required */
		node->output=node->output+node->input[5];
	}

	if(!node->input[0])
	{
		node->output=0;
	}

	return 0;
}

int dss_lfsr_reset(struct node_description *node)
{
	struct dss_lfsr_context *context;
	struct discrete_lfsr_desc *lfsr_desc;

	context=(struct dss_lfsr_context*)node->context;
	lfsr_desc=(struct discrete_lfsr_desc*)(node->custom);

	context->lfsr_reg=lfsr_desc->reset_value;

	context->lfsr_reg=dss_lfsr_function(DISC_LFSR_REPLACE,0, (dss_lfsr_function(lfsr_desc->feedback_function0,0,0,0x01))<<(lfsr_desc->bitlength),((2<<(lfsr_desc->bitlength))-1));

	/* Now select and setup the output bit */
	node->output=((context->lfsr_reg)>>(lfsr_desc->output_bit))&0x01;

	/* Final inversion if required */
	if(lfsr_desc->flags&DISC_LFSR_FLAG_OUT_INVERT) node->output=(node->output)?0.0:1.0;

	/* Gain stage */
	node->output=(node->output)?(node->input[3])/2:-(node->input[3])/2;
	/* Bias input as required */
	node->output=node->output+node->input[5];

	return 0;
}

int dss_lfsr_init(struct node_description *node)
{
	struct dss_lfsr_context *context;

	discrete_log("dss_lfsr_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_lfsr_context)))==NULL)
	{
		discrete_log("dss_lfsr_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_lfsr_context));
	}

	/* Initialize the object */
	context=(struct dss_lfsr_context*)node->context;
	context->sampleStep = 1.0 / Machine->sample_rate;
	context->shiftStep = 1.0 / node->input[2];
	context->t = 0;

	dss_lfsr_reset(node);

	return 0;
}


/************************************************************************/
/*                                                                      */
/* DSS_ADSR - Attack Decay Sustain Release                              */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Trigger value                                            */
/* input2    - gain scaling factor                                      */
/*                                                                      */
/************************************************************************/
int dss_adsrenv_step(struct node_description *node)
{
	struct dss_adsr_context *context;
	context=(struct dss_adsr_context*)node->context;

	if(node->input[0])
	{
		node->output=0;
	}
	else
	{
		node->output=0;
	}
	return 0;
}


int dss_adsrenv_reset(struct node_description *node)
{
//	struct dss_adsr_context *context=(struct dss_adsr_context*)node->context;
	dss_adsrenv_step(node);
	return 0;
}

int dss_adsrenv_init(struct node_description *node)
{
	discrete_log("dss_adsrenv_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_adsr_context)))==NULL)
	{
		discrete_log("dss_adsrenv_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_adsr_context));
	}

	/* Initialize the object */
	dss_noise_reset(node);

	return 0;
}

/************************************************************************/
/*                                                                      */
/* DISCRETE_COUNTER - External clock Binary Counter                     */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Reset input (active high)                                */
/* input2    - Clock Input                                              */
/* input3    - Max count                                                */
/* input4    - Direction - 0=down, 1=up                                 */
/* input5    - Reset Value                                              */
/* input6    - Clock type (count on 0/1)                                */
/*                                                                      */
/* Jan 2004, D Renaud.                                                                    */
/************************************************************************/
int dss_counter_step(struct node_description *node)
{
	struct dss_counter_context *context=(struct dss_counter_context*)node->context;
	int clock = node->input[2] && node->input[2];
	/*
	 * We will count at the selected changeover to high/low, only when enabled.
	 * We don't count if module is not enabled.
	 * This has the effect of holding the output at it's current value.
	 */
	if ((context->last != clock) && node->input[0])
	{
		/* Toggled */
		context->last = clock;

		if (node->input[6] == clock)
		{
			/* Proper edge */
			node->output += node->input[4] ? 1 : -1; // up/down
			if (node->output < 0) node->output = node->input[3];
			if (node->output > node->input[3]) node->output = 0;
		}
	}

	/* If reset enabled then set output to the reset value. */
	if (node->input[1]) node->output = node->input[5];

	return 0;
}

int dss_counter_reset(struct node_description *node)
{
	struct dss_counter_context *context=(struct dss_counter_context*)node->context;

	context->last = node->input[2] && node->input[2];
	node->output = node->input[5]; /* Output starts at reset value */

	return 0;
}

int dss_counter_init(struct node_description *node)
{
	discrete_log("dss_counter_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_counter_context)))==NULL)
	{
		discrete_log("dss_counter_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_counter_context));
	}

	/* Initialize the object */
	dss_counter_reset(node);

	return 0;
}


/************************************************************************/
/*                                                                      */
/* DISCRETE_COUNTER_FIX - Fixed Frequency Binary Counter                */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Reset input (active high)                                */
/* input2    - Frequency                                                */
/* input3    - Max count                                                */
/* input4    - Direction - 0=up, 1=down                                 */
/* input5    - Reset Value                                              */
/*                                                                      */
/* Jan 2004, D Renaud.                                                                    */
/************************************************************************/
int dss_counterfix_step(struct node_description *node)
{
	struct dss_counterfix_context *context=(struct dss_counterfix_context*)node->context;

	context->tLeft -= context->sampleStep;

	/* The enable input only curtails output, phase rotation still occurs. */
	while (context->tLeft <= 0)
	{

		/*
		 * We will count when enabled.
		 * We don't count if module is not enabled.
		 * This has the effect of holding the output at it's current value.
		 */
		if (node->input[0])
		{
			node->output += node->input[4] ? 1 : -1; // up/down
			if (node->output < 0) node->output = node->input[3];
			if (node->output > node->input[3]) node->output = 0;
		}

		context->tLeft += context->tCycle;
	}

	/* If reset enabled then set output to the reset value. */
	if (node->input[1]) node->output = node->input[5];

	return 0;
}

int dss_counterfix_reset(struct node_description *node)
{
	struct dss_counterfix_context *context=(struct dss_counterfix_context*)node->context;

	context->sampleStep = 1.0 / Machine->sample_rate;
	context->tCycle = 1.0 / node->input[2];
	context->tLeft = context->tCycle;
	node->output = node->input[5]; /* Output starts at reset value */

	return 0;
}

int dss_counterfix_init(struct node_description *node)
{
	discrete_log("dss_counterfix_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_counterfix_context)))==NULL)
	{
		discrete_log("dss_counterfix_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_counterfix_context));
	}

	/* Initialize the object */
	dss_counterfix_reset(node);

	return 0;
}


/************************************************************************/
/*                                                                      */
/* DISCRETE_OP_AMP_OSCILLATOR - Op Amp Oscillators                      */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Vin (if needed)                                          */
/* input2    - Type of oscillator circuit                               */
/*                                                                      */
/* also passed                                                          */
/*                                                                      */
/* Mar 2004, D Renaud.                                                                    */
/************************************************************************/
int dss_op_amp_osc_step(struct node_description *node)
{
	return 0;
}

int dss_op_amp_osc_reset(struct node_description *node)
{
	return 0;
}

int dss_op_amp_osc_init(struct node_description *node)
{
	return 0;
}


/************************************************************************/
/*                                                                      */
/* DISCRETE_SCHMITT_OSCILLATOR - Schmitt feedback oscillator            */
/*                                                                      */
/* input0    - Enable input value                                       */
/* input1    - Vin                                                      */
/* input2    - Amplitude                                                */
/*                                                                      */
/* also passed discrete_schmitt_osc_disc structure                      */
/*                                                                      */
/* Mar 2004, D Renaud.                                                                    */
/************************************************************************/
#define DSSSCHMITTOSC_ENABLE	(int)node->input[0]
#define DSSSCHMITTOSC_VIN	node->input[1]
#define DSSSCHMITTOSC_AMPL	node->input[2]

int dss_schmitt_osc_step(struct node_description *node)
{
	struct dss_schmitt_osc_context *context = (struct dss_schmitt_osc_context*)node->context;
	struct discrete_schmitt_osc_desc *info = (struct discrete_schmitt_osc_desc*)node->custom;

	double supply, vCap, new_vCap, t, exponent;

	/* We will always oscillate.  The enable just affects the output. */
	vCap = context->vCap;
	exponent = context->exponent;

	/* Keep looping until all toggling in time sample is used up. */
	do
	{
		t = 0;
		/* The charging voltage to the cap is the sum of the input voltage and the gate
		 * output voltage in the ratios determined by their resistors in a divider network.
		 * The input voltage is selectable as straight voltage in or logic level that will
		 * use vGate as its voltage.  Note that ratioIn is just the ratio of the total
		 * voltage and needs to be multipled by the input voltage.  ratioFeedback has
		 * already been multiplied by vGate to save time because that voltage never changes. */
		supply = (info->options & DISC_SCHMITT_OSC_IN_IS_VOLTAGE) ? context->ratioIn * DSSSCHMITTOSC_VIN : (DSSSCHMITTOSC_VIN ? context->ratioIn * info->vGate : 0);
		supply += (context->state ? context->ratioFeedback : 0);
		new_vCap = vCap + ((supply - vCap) * exponent);
		if (context->state)
		{
			/* Charging */
			/* has it charged past upper limit? */
			if (new_vCap >= info->trshRise)
			{
				if (new_vCap > info->trshRise)
				{
					/* calculate the overshoot time */
					t = context->rc * log(1.0 / (1.0 - ((new_vCap - info->trshRise) / (info->vGate - vCap))));
					/* calculate new exponent because of reduced time */
					exponent = 1.0 - exp(-t / context->rc);
				}
				vCap = info->trshRise;
				new_vCap = info->trshRise;
				context->state = 0;
			}
		}
		else
		{
			/* Discharging */
			/* has it discharged past lower limit? */
			if (new_vCap <= info->trshFall)
			{
				if (new_vCap < info->trshFall)
				{
					/* calculate the overshoot time */
					t = context->rc * log(1.0 / (1.0 - ((info->trshFall - new_vCap) / vCap)));
					/* calculate new exponent because of reduced time */
					exponent = 1.0 - exp(-t / context->rc);
				}
				vCap = info->trshFall;
				new_vCap = info->trshFall;
				context->state = 1;
			}
		}
	} while(t);

	context->vCap = new_vCap;

	switch (info->options & DISC_SCHMITT_OSC_ENAB_MASK)
	{
		case DISC_SCHMITT_OSC_ENAB_IS_AND:
			node->output = DSSSCHMITTOSC_ENABLE && context->state;
			break;
		case DISC_SCHMITT_OSC_ENAB_IS_NAND:
			node->output = !(DSSSCHMITTOSC_ENABLE && context->state);
			break;
		case DISC_SCHMITT_OSC_ENAB_IS_OR:
			node->output = DSSSCHMITTOSC_ENABLE || context->state;
			break;
		case DISC_SCHMITT_OSC_ENAB_IS_NOR:
			node->output = !(DSSSCHMITTOSC_ENABLE || context->state);
			break;
	}
	node->output = node->output * DSSSCHMITTOSC_AMPL;

	return 0;
}

int dss_schmitt_osc_reset(struct node_description *node)
{
	struct dss_schmitt_osc_context *context = (struct dss_schmitt_osc_context*)node->context;
	struct discrete_schmitt_osc_desc *info = (struct discrete_schmitt_osc_desc*)node->custom;
	double rSource;

	/* The 2 resistors make a voltage divider, so their ratios add together
	 * to make the charging voltage. */
	context->ratioIn = info->rFeedback / (info->rIn + info->rFeedback);
	context->ratioFeedback = info->rIn / (info->rIn + info->rFeedback) * info->vGate;

	/* The voltage source resistance works out to the 2 resistors in parallel.
	 * So use this for the RC charge constant. */
	rSource = 1.0 / ((1.0 / info->rIn) + (1.0 / info->rFeedback));
	context->rc = rSource * info->c;
	context->exponent = -1.0 / (context->rc  * Machine->sample_rate);
	context->exponent = 1.0 - exp(context->exponent);

	/* Cap is at 0V on power up.  Causing output to be high. */
	context->vCap = 0;
	context->state = 1;

	node->output = info->options ? 0 : node->input[2];

	return 0;
}

int dss_schmitt_osc_init(struct node_description *node)
{
	discrete_log("dss_schmitt_osc_init() - Creating node %d.",node->node-NODE_00);

	/* Allocate memory for the context array and the node execution order array */
	if((node->context=malloc(sizeof(struct dss_schmitt_osc_context)))==NULL)
	{
		discrete_log("dss_schmitt_osc_init() - Failed to allocate local context memory.");
		return 1;
	}
	else
	{
		/* Initialize memory */
		memset(node->context,0,sizeof(struct dss_schmitt_osc_context));
	}

	/* Initialize the object */
	dss_schmitt_osc_reset(node);

	return 0;
}
