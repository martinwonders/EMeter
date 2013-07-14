/**************************************************************************
 *
 * Filename:    PS2000con.c
 *
 * Copyright:   Pico Technology Limited 2006
 *
 * Author:      MAS
 *
 * Description:
 *   This is a console-mode program that demonstrates how to use the
 *   PS2000 driver, available for the PS2104 and PS2105.
 *
 * Examples:
 *    Collect a block of samples immediately
 *    Collect a block of samples when a trigger event occurs
 *    Collect a block of samples using an advanced trigger
 *			- PS2203, PS2204, PS2205 only
 *    Collect a block using ETS
 *			PS2104, PS2105, PS2203, PS2204, and PS2205
 *    Collect a stream of data
 *    Collect a stream of data using an advanced trigger
 *			- PS2203, PS2204, PS2205 only
 *    Set the signal generator with the built in signals
 *			- PS2203, PS2204, and PS2205 only
 *		Set the signal generator with the arbitarary signal
 *			- PS2203, PS2204, and PS2205 only
 *
 *
 * To build this application
 *	set up a project for a 32-bit console mode application
 *	add this file to the project
 *	add PS2000bc.lib to the project (Borland C only)
 *	add PS2000.lib to the project (Microsoft C only)
 *	build the project
 *
 * History:
 *     13Feb06 MAS Created
 *
 * Revision Info: "file %n date %f revision %v"
 *						""
 *
 ***************************************************************************/

#include "windows.h"
#include <conio.h>
#include <stdio.h>

/* Definitions of PS2000 driver routines */
#include "ps2000.h"

#define BUFFER_SIZE 	1024
#define BUFFER_SIZE_STREAMING 500000
#define MAX_CHANNELS 4

short  	values_a [BUFFER_SIZE];
short    overflow;
int		scale_to_mv = 1;

short		channel_mv [PS2000_MAX_CHANNELS];
short		timebase = 8;

typedef enum {
	MODEL_NONE = 0,
  MODEL_PS2104 = 2104,
	MODEL_PS2105 = 2105,
	MODEL_PS2202 = 2202,
	MODEL_PS2203 = 2203,
	MODEL_PS2204 = 2204,
	MODEL_PS2205 = 2205
} MODEL_TYPE;

typedef struct
{
	PS2000_THRESHOLD_DIRECTION	channelA;
	PS2000_THRESHOLD_DIRECTION	channelB;
	PS2000_THRESHOLD_DIRECTION	channelC;
	PS2000_THRESHOLD_DIRECTION	channelD;
	PS2000_THRESHOLD_DIRECTION	ext;
} DIRECTIONS;

typedef struct
{
	PS2000_PWQ_CONDITIONS					*	conditions;
	short														nConditions;
	PS2000_THRESHOLD_DIRECTION		  direction;
	unsigned long										lower;
	unsigned long										upper;
	PS2000_PULSE_WIDTH_TYPE					type;
} PULSE_WIDTH_QUALIFIER;


typedef struct
{
	PS2000_CHANNEL channel;
	float threshold;
	short direction;
	float delay;
} SIMPLE;

typedef struct
{
	short hysterisis;
	DIRECTIONS directions;
	short nProperties;
	PS2000_TRIGGER_CONDITIONS * conditions;
	PS2000_TRIGGER_CHANNEL_PROPERTIES * channelProperties;
	PULSE_WIDTH_QUALIFIER pwq;
 	unsigned long totalSamples;
	short autoStop;
	short triggered;
} ADVANCED;


typedef struct
{
	SIMPLE simple;
	ADVANCED advanced;
} TRIGGER_CHANNEL;

typedef struct {
	short DCcoupled;
	short range;
	short enabled;
	short values [BUFFER_SIZE];
} CHANNEL_SETTINGS;

typedef struct  {
	short handle;
	MODEL_TYPE model;
  PS2000_RANGE firstRange;
	PS2000_RANGE lastRange;
	TRIGGER_CHANNEL trigger;
	short maxTimebase;
	short timebases;
	short noOfChannels;
	CHANNEL_SETTINGS channelSettings[PS2000_MAX_CHANNELS];
	short				hasAdvancedTriggering;
	short				hasFastStreaming;
	short				hasEts;
	short				hasSignalGenerator;
} UNIT_MODEL;

UNIT_MODEL unitOpened;

long times[BUFFER_SIZE];

int input_ranges [PS2000_MAX_RANGES] = {10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000};

void  __stdcall ps2000FastStreamingReady( short **overviewBuffers,
															  short overflow,
																unsigned long triggeredAt,
																short triggered,
																short auto_stop,
															  unsigned long nValues)
{
	unitOpened.trigger.advanced.totalSamples += nValues;
	unitOpened.trigger.advanced.autoStop = auto_stop;
}

/****************************************************************************
 *
 * adc_units
 *
 ****************************************************************************/
char * adc_units (short time_units)
  {
	  time_units++;
	  //printf ( "time unit:  %d\n", time_units ) ;
	  switch ( time_units )
		{
		case 0:
		  return "ADC";
		case 1:
		  return "fs";
		case 2:
		  return "ps";
		case 3:
		  return "ns";
		case 4:
		  return "us";
		case 5:
		  return "ms";
		}
	  return "Not Known";
  }

/****************************************************************************
 * adc_to_mv
 *
 * If the user selects scaling to millivolts,
 * Convert an 12-bit ADC count into millivolts
 ****************************************************************************/
int adc_to_mv (long raw, int ch)
  {
  return ( scale_to_mv ) ? ( raw * input_ranges[ch] ) / 32767 : raw;
  }

/****************************************************************************
 * mv_to_adc
 *
 * Convert a millivolt value into a 12-bit ADC count
 *
 *  (useful for setting trigger thresholds)
 ****************************************************************************/
short mv_to_adc (short mv, short ch)
  {
  return ( ( mv * 32767 ) / input_ranges[ch] );
  }

/****************************************************************************
 * set_defaults - restore default settings
 ****************************************************************************/
void set_defaults (void)
{
	short ch = 0;
  ps2000_set_ets ( unitOpened.handle, PS2000_ETS_OFF, 0, 0 );

	for (ch = 0; ch < unitOpened.noOfChannels; ch++)
	{
		ps2000_set_channel ( unitOpened.handle,
			                   ch,
												 unitOpened.channelSettings[ch].enabled ,
												 unitOpened.channelSettings[ch].DCcoupled ,
												 unitOpened.channelSettings[ch].range);
	}
}

void set_trigger_advanced(void)
{
	short ok = 0;
  short 	auto_trigger_ms = 0;

	// to trigger of more than one channel set this parameter to 2 or more
	// each condition can only have on parameter set to PS2000_CONDITION_TRUE or PS2000_CONDITION_FALSE
	// if more than on condition is set then it will trigger off condition one, or condition two etc.
	unitOpened.trigger.advanced.nProperties = 1;
	// set the trigger channel to channel A by using PS2000_CONDITION_TRUE
	unitOpened.trigger.advanced.conditions = malloc (sizeof (PS2000_TRIGGER_CONDITIONS) * unitOpened.trigger.advanced.nProperties);
	unitOpened.trigger.advanced.conditions->channelA = PS2000_CONDITION_TRUE;
	unitOpened.trigger.advanced.conditions->channelB = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->channelC = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->channelD = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->external = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->pulseWidthQualifier = PS2000_CONDITION_DONT_CARE;

	// set channel A to rising
	// the remainder will be ignored as only a condition is set for channel A
	unitOpened.trigger.advanced.directions.channelA = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.channelB = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.channelC = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.channelD = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.ext = PS2000_ADV_RISING;


	unitOpened.trigger.advanced.channelProperties = malloc (sizeof (PS2000_TRIGGER_CHANNEL_PROPERTIES) * unitOpened.trigger.advanced.nProperties);
	// there is one property for each condition
	// set channel A
	// trigger level 1500 adc counts the trigger point will vary depending on the voltage range
	// hysterisis 4096 adc counts
	unitOpened.trigger.advanced.channelProperties->channel = (short) PS2000_CHANNEL_A;
	unitOpened.trigger.advanced.channelProperties->thresholdMajor = 1500;
	// not used in level triggering, should be set when in window mode
	unitOpened.trigger.advanced.channelProperties->thresholdMinor = 0;
	// used in level triggering, not used when in window mode
	unitOpened.trigger.advanced.channelProperties->hysteresis = (short) 4096;
	unitOpened.trigger.advanced.channelProperties->thresholdMode = PS2000_LEVEL;

	ok = ps2000SetAdvTriggerChannelConditions (unitOpened.handle, unitOpened.trigger.advanced.conditions, unitOpened.trigger.advanced.nProperties);
	ok = ps2000SetAdvTriggerChannelDirections (unitOpened.handle,
																				unitOpened.trigger.advanced.directions.channelA,
																				unitOpened.trigger.advanced.directions.channelB,
																				unitOpened.trigger.advanced.directions.channelC,
																				unitOpened.trigger.advanced.directions.channelD,
																				unitOpened.trigger.advanced.directions.ext);
	ok = ps2000SetAdvTriggerChannelProperties (unitOpened.handle,
																				unitOpened.trigger.advanced.channelProperties,
																				unitOpened.trigger.advanced.nProperties,
																				auto_trigger_ms);


	// remove comments to try triggering with a pulse width qualifier
	// add a condition for the pulse width eg. in addition to the channel A or as a replacement
	//unitOpened.trigger.advanced.pwq.conditions = malloc (sizeof (PS2000_PWQ_CONDITIONS));
	//unitOpened.trigger.advanced.pwq.conditions->channelA = PS2000_CONDITION_TRUE;
	//unitOpened.trigger.advanced.pwq.conditions->channelB = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.conditions->channelC = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.conditions->channelD = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.conditions->external = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.nConditions = 1;

	//unitOpened.trigger.advanced.pwq.direction = PS2000_RISING;
	//unitOpened.trigger.advanced.pwq.type = PS2000_PW_TYPE_LESS_THAN;
	//// used when type	PS2000_PW_TYPE_IN_RANGE,	PS2000_PW_TYPE_OUT_OF_RANGE
	//unitOpened.trigger.advanced.pwq.lower = 0;
	//unitOpened.trigger.advanced.pwq.upper = 10000;
	//ps2000SetPulseWidthQualifier (unitOpened.handle,
	//															unitOpened.trigger.advanced.pwq.conditions,
	//															unitOpened.trigger.advanced.pwq.nConditions,
	//															unitOpened.trigger.advanced.pwq.direction,
	//															unitOpened.trigger.advanced.pwq.lower,
	//															unitOpened.trigger.advanced.pwq.upper,
	//															unitOpened.trigger.advanced.pwq.type);

	ok = ps2000SetAdvTriggerDelay (unitOpened.handle, 0, -10);
}


void collect_fast_streaming_triggered (void)
{
	unsigned long	i;
	FILE 	*fp;
	short  overflow;
	int 	ok;
	short ch;
	unsigned long nPreviousValues = 0;
	short values_a[BUFFER_SIZE_STREAMING];
	short values_b[BUFFER_SIZE_STREAMING];
	unsigned long	triggerAt;
	short triggered;
	unsigned long no_of_samples;
	double startTime = 0;



	fprintf (stderr, "Ready to stream...\n" );
	printf (stderr, "Press a key to start\n" );
	getch ();

	set_defaults ();

	set_trigger_advanced ();

	unitOpened.trigger.advanced.autoStop = 0;
	unitOpened.trigger.advanced.totalSamples = 0;
	unitOpened.trigger.advanced.triggered = 0;

	//Enable and set channel A with AC coupling at a range of +/-2V
	if(ps2000_set_channel(unitOpened.handle,PS2000_CHANNEL_A,TRUE,FALSE,PS2000_2V))
        fprintf(stderr,"Channel A enabled and set..\n");
    else
        fprintf(stderr,"Unable to set channel A...\n");

    //Disable channel B
	if(ps2000_set_channel(unitOpened.handle,PS2000_CHANNEL_B,FALSE,FALSE,1))
        fprintf(stderr,"Channel B disabled..\n");
    else
        fprintf(stderr,"Unable to set channel B...\n");

	/* Collect data at 200ns intervals
	* 100000 points with an agregation of 1 : 1
	*	Auto stop after the 100000 samples
	*  Start it collecting,
	*/
	if(!ps2000_run_streaming_ns ( unitOpened.handle, 200, 2, BUFFER_SIZE_STREAMING, 1, 1, 40000 ))
        fprintf(stderr,"There was a problem running streaming...\n");

	/* From here on, we can get data whenever we want...
	*/

	while (!unitOpened.trigger.advanced.autoStop)
	{
		ps2000_get_streaming_last_values (unitOpened.handle, ps2000FastStreamingReady);
		if (nPreviousValues != unitOpened.trigger.advanced.totalSamples)
		{
			nPreviousValues = 	unitOpened.trigger.advanced.totalSamples;
		}
		Sleep (0);
	}

	ps2000_stop (unitOpened.handle);

	no_of_samples = ps2000_get_streaming_values_no_aggregation (unitOpened.handle,
                                                                &startTime, // get samples from the beginning
																values_a, // set buffer for channel A
                                                                values_b,	// set buffer for channel B
                                                                NULL,
																NULL,
																&overflow,
																&triggerAt,
																&triggered,
																BUFFER_SIZE_STREAMING);

    fprintf(stderr,"%d samples collected, representing %d mains cycles",no_of_samples,no_of_samples/99000);
	// if the unit triggered print out ten samples either side of the trigger point
	// otherwise print the first 20 readings
	/*for ( i = (triggered ? triggerAt - 10 : 0) ; i < ((triggered ? triggerAt - 10 : 0) + 20); i++)
	{
		for (ch = 0; ch < unitOpened.noOfChannels; ch++)
		{
			if (unitOpened.channelSettings[ch].enabled)
			{
				printf ("%d, ", adc_to_mv ((!ch ? values_a[i] : values_b[i]), unitOpened.channelSettings[ch].range) );
			}
		}
		printf ("\n");
	}
    */

	//fp = fopen ( "data.txt", "w" );

	for ( i = 0; i < no_of_samples; i++ )
	{
	    printf ("%d ", adc_to_mv (values_a[i], unitOpened.channelSettings[0].range) );
	}
	printf ("\n");
	//fclose ( fp );

	getch ();
}


/****************************************************************************
 *
 *
 ****************************************************************************/
void get_info (void)
  {
  char description [7][25]=  {	"Driver Version   ",
								"USB Version      ",
								"Hardware Version ",
								"Variant Info     ",
								"Serial           ",
								"Cal Date         ",
								"Error Code       " };
  short 		i;
  char		line [80];
	int    variant;

  if( unitOpened.handle )
  {
    for ( i = 0; i < 6; i++ )
    {
      ps2000_get_unit_info ( unitOpened.handle, line, sizeof (line), i );
			if (i == 3)
			{
			  variant = atoi(line);
			}
      fprintf (stderr, "%s: %s\n", description[i], line );
    }

  	switch (variant)
		{
		case MODEL_PS2104:
			unitOpened.model = MODEL_PS2104;
			unitOpened.firstRange = PS2000_100MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2104_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 1;
			unitOpened.hasAdvancedTriggering = FALSE;
			unitOpened.hasSignalGenerator = FALSE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = FALSE;
		break;

		case MODEL_PS2105:
			unitOpened.model = MODEL_PS2105;
			unitOpened.firstRange = PS2000_100MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2105_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 1;
			unitOpened.hasAdvancedTriggering = FALSE;
			unitOpened.hasSignalGenerator = FALSE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = FALSE;
		break;

		case MODEL_PS2202:
			unitOpened.model = MODEL_PS2202;
			unitOpened.firstRange = PS2000_100MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 2;
			unitOpened.hasAdvancedTriggering = FALSE;
			unitOpened.hasSignalGenerator = FALSE;
			unitOpened.hasEts = FALSE;
			unitOpened.hasFastStreaming = FALSE;
		break;

		case MODEL_PS2203:
			unitOpened.model = MODEL_PS2203;
			unitOpened.firstRange = PS2000_50MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 2;
			unitOpened.hasAdvancedTriggering = TRUE;
			unitOpened.hasSignalGenerator = TRUE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = TRUE;
		break;

		case MODEL_PS2204:
			unitOpened.model = MODEL_PS2204;
			unitOpened.firstRange = PS2000_50MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 2;
			unitOpened.hasAdvancedTriggering = TRUE;
			unitOpened.hasSignalGenerator = TRUE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = TRUE;
		break;

		case MODEL_PS2205:
			unitOpened.model = MODEL_PS2205;
			unitOpened.firstRange = PS2000_50MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 2;
			unitOpened.hasAdvancedTriggering = TRUE;
			unitOpened.hasSignalGenerator = TRUE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = TRUE;
		break;

 		default:
			fprintf(stderr,"Unit not supported");
		}
		unitOpened.channelSettings [PS2000_CHANNEL_A].enabled = 1;
		unitOpened.channelSettings [PS2000_CHANNEL_A].DCcoupled = 1;
		unitOpened.channelSettings [PS2000_CHANNEL_A].range = unitOpened.lastRange;

		if (unitOpened.noOfChannels == 2)
			unitOpened.channelSettings [PS2000_CHANNEL_B].enabled = 1;
		else
			unitOpened.channelSettings [PS2000_CHANNEL_B].enabled = 0;

		unitOpened.channelSettings [PS2000_CHANNEL_B].DCcoupled = 1;
		unitOpened.channelSettings [PS2000_CHANNEL_B].range = unitOpened.lastRange;

  }
  else
  {
    fprintf (stderr, "Unit Not Opened\n" );
    ps2000_get_unit_info ( unitOpened.handle, line, sizeof (line), 5 );
    fprintf (stderr, "%s: %s\n", description[5], line );
		unitOpened.model = MODEL_NONE;
		unitOpened.firstRange = PS2000_100MV;
		unitOpened.lastRange = PS2000_20V;
		unitOpened.timebases = PS2105_MAX_TIMEBASE;
		unitOpened.noOfChannels = 1;
	}
}

/****************************************************************************
 *
 *
 ****************************************************************************/

void main (void)
{
	char	ch;

	fprintf ( stderr,"\n\nOpening the device...\n");

	//open unit and show splash screen
	unitOpened.handle = ps2000_open_unit ();
	fprintf (stderr, "Handler: %d\n", unitOpened.handle );
	if ( !unitOpened.handle )
	{
		fprintf (stderr, "Unable to open device\n" );
		get_info ();
		while( !kbhit() );
		exit ( 99 );
	}
	else
	{
		fprintf (stderr, "Device opened successfully\n\n" );
		get_info ();

		timebase = 0;

		ch = ' ';
		while ( ch != 'X' )
		{
			fprintf (stderr,"\n" );
			fprintf (stderr, "D - Fast streaming triggered\n");
			fprintf (stderr, "X - exit\n" );
			fprintf (stderr, "Operation:" );

			ch = toupper ( getch () );

			fprintf (stderr, "\n\n" );
			switch ( ch )
			{

			case 'D':
				if (unitOpened.hasFastStreaming && unitOpened.hasAdvancedTriggering)
				{
					collect_fast_streaming_triggered ();
				}
				else
				{
					fprintf (stderr,"Not supported by this model\n\n");
				}
				break;

			case 'X':
				/* Handled by outer loop */
				break;

			default:
				fprintf (stderr, "Invalid operation\n" );
				break;
			}
		}

		ps2000_close_unit ( unitOpened.handle );
	}
}
