/** 
 * @By Jesper And Thorbjørn 
 * 
 */

#include <stdio.h>
#include "GPIO.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <signal.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include <jack/jack.h>

jack_port_t **input_ports;
jack_port_t **output_ports;
jack_client_t *client;

static int gpioPinNumbers[8] = {17, 27, 22, 24, 5, 12, 13, 26};

float calculateRMS(jack_nframes_t nframes, jack_default_audio_sample_t *in);

static void signal_handler ( int sig )
{
    jack_client_close ( client );
    fprintf ( stderr, "signal received, exiting ...\n" );
    exit ( 0 );
}


/**
 * The process callback for this JACK application is called in a
 * special realtime thread once for each audio cycle.
 *
 * This is the workhose, calculating the compression and saving the value
 * on the output. It takes frames which contains the left and right sounds
 * of the song, and gets read once for each to get stereo output.
 */

int
process ( jack_nframes_t nframes, void *arg )
{
	
    int defaultSize = sizeof ( jack_default_audio_sample_t );
    int i;
	
	/* Set the desired values for the parameters */
	int x, offset = 16; //dB
	float kneeWidth = 2.4, ratio, attackTime = 0.05, releaseTime = 0.2;
	float threshold = -80;
	
	/* Save the value of the Dipswitch to the integer v and use that to set the ratio*/
	int v = (getDipValue(gpioPinNumbers)); 
	ratio = 1 + (float)v/4; //never 0
	
	/* Set the in and out buffers with values */
    jack_default_audio_sample_t *in, *out;
    jack_default_audio_sample_t buffer;
	
	/* Calculate MakeUpGain */
	float makeUpGain = 0;
	
	if(offset<= -threshold -  kneeWidth/2){
		makeUpGain = - threshold + (( threshold)/ratio);
	}
	else{
		if(offset <= -threshold +kneeWidth/2){
			makeUpGain = - (((1/ratio) -1)*(pow(0 - threshold+(kneeWidth/2), 2)))/(2*kneeWidth);
		}
		else{
			makeUpGain = 0;
		}
		
	}
	makeUpGain = makeUpGain + offset;
	
	
	uint32_t sampleRate = jack_get_sample_rate(client);

	/* Start the forloop for stereo */
    for ( i = 0; i < 2; i++ ){
		in = jack_port_get_buffer ( input_ports[i], nframes );
		out = jack_port_get_buffer ( output_ports[i], nframes );
	
		/* Calculate the coefficients */
		float attackTimeCoefficient = exp ((-log(9)/(sampleRate*attackTime)));
		float releaseTimeCoefficient = exp ((-log(9)/(sampleRate*releaseTime)));
		float computedGain = 0;
		static float smoothedGain = 0.0;
		
		float rms = calculateRMS(nframes, in)*sqrt(2);
		float xdb = 0, xsc = 0,madeUpGain;	
		
		/* Decibel Domain */
		xdb = 20*log10(rms);
		
		/* Calculate the knee with set thresholds and width */
		if ((threshold-(kneeWidth/2)) <=xdb && xdb<= (threshold+(kneeWidth/2))){
			xsc = xdb + (((1/ratio) -1)*(pow(xdb - threshold+(kneeWidth/2), 2)))/(2*kneeWidth);
		}
		else if ( xdb> (threshold+(kneeWidth/2))) {
			xsc = threshold + ((xdb - threshold)/ratio);
		}
		else {
			xsc = xdb;
		}
		computedGain = xsc - xdb;
	
		for(x = 0; x <= nframes-1; x++){
			/* Smoothing function, check previousValue of smoothedGain and compare */
			if (computedGain > smoothedGain /*previousValue of  S.G.*/){
				smoothedGain = smoothedGain*attackTimeCoefficient + (1-attackTimeCoefficient)*computedGain;
			}
			else if(computedGain<= smoothedGain){ 
				smoothedGain = smoothedGain*releaseTimeCoefficient + (1-releaseTimeCoefficient)*computedGain;
			}
			madeUpGain = makeUpGain + smoothedGain;
			buffer = pow(10, madeUpGain/20);
				
			/* Set output */
			out[x] = in[x] * buffer;
		    
		}
		
		/* Debug values that gets printed in the end, can be removed */
		float output = out[0];
		float outputDB = 20*log10(sqrt(output*output));
		static float input = 0;
		if(in[0] > input){
			input = in[0];
		}
		float inputDB = 20*log10(sqrt(input*input));
		printf("Input: %+f, output: %f, SG: %+f, ratio: %f, MakeUpGain: %f\n", inputDB, outputDB,  smoothedGain, ratio, makeUpGain);
		/* End of debug values */
	}
	return 0;
}
/** 
 * Calculates the Root Mean Square on the given input 
 */
float calculateRMS(jack_nframes_t nframes, jack_default_audio_sample_t *in){ //
	int i;
	float raw = 0;
	for( i = 0 ; i < nframes ; i++ ){
		raw += in[i] * in[i] ;
	}
	float rms = raw / ((float) nframes);
	rms = sqrt(rms) ;
	return rms;
}
	
int // a callback method that prints new sample rate at sample rate change
srate (jack_nframes_t nframes, void *arg)

{
	printf ("the sample rate is now %lu/sec\n", nframes);
	return 0;
}

/**
 * JACK calls this shutdown_callback if the server ever shuts down or
 * decides to disconnect the client.
 */
void
jack_shutdown ( void *arg )
{
    jack_free ( input_ports );
    jack_free ( output_ports );
    exit ( 1 );
}


int
main ( int argc, char *argv[] )
{
    /*Setup the IO pins for Dipswitch (for Ratio controll) */
    setup_io();
	
	
    int i;
    const char **ports;
    const char *client_name;
    const char *server_name = NULL;
    jack_options_t options = JackNullOption;
    jack_status_t status;

    if ( argc >= 2 )        
    {
        client_name = argv[1];
        if ( argc >= 3 )    
        {
            server_name = argv[2];
            options |= JackServerName;
        }
    }
    else              
    {
        client_name = strrchr ( argv[0], '/' );
        if ( client_name == 0 )
        {
            client_name = argv[0];
        }
        else
        {
            client_name++;
        }
    }

    /* open a client connection to the JACK server */

    client = jack_client_open ( client_name, options, &status, server_name );
    if ( client == NULL )
    {
        fprintf ( stderr, "jack_client_open() failed, "
                  "status = 0x%2.0x\n", status );
        if ( status & JackServerFailed )
        {
            fprintf ( stderr, "Unable to connect to JACK server\n" );
        }
        exit ( 1 );
    }
    if ( status & JackServerStarted )
    {
        fprintf ( stderr, "JACK server started\n" );
    }
    if ( status & JackNameNotUnique )
    {
        client_name = jack_get_client_name ( client );
        fprintf ( stderr, "unique name `%s' assigned\n", client_name );
    }

    /* tell the JACK server to call `process()' whenever
       there is work to be done.
    */

    processing_data_t processData = {};
    
    jack_set_process_callback ( client, process, &processData );
    
    jack_set_sample_rate_callback (client, srate, 0);

    /* tell the JACK server to call `jack_shutdown()' if
       it ever shuts down, either entirely, or if it
       just decides to stop calling us.
    */

    jack_on_shutdown ( client, jack_shutdown, 0 );

    /* create two ports pairs*/
    input_ports = ( jack_port_t** ) calloc ( 2, sizeof ( jack_port_t* ) );
    output_ports = ( jack_port_t** ) calloc ( 2, sizeof ( jack_port_t* ) );

    char port_name[16];
    for ( i = 0; i < 2; i++ )
    {
        sprintf ( port_name, "input_%d", i + 1 );
        input_ports[i] = jack_port_register ( client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0 );
        sprintf ( port_name, "output_%d", i + 1 );
        output_ports[i] = jack_port_register ( client, port_name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0 );
        if ( ( input_ports[i] == NULL ) || ( output_ports[i] == NULL ) )
        {
            fprintf ( stderr, "no more JACK ports available\n" );
            exit ( 1 );
        }
    }

    /* Tell the JACK server that we are ready to roll.  Our
     * process() callback will start running now. */

    if ( jack_activate ( client ) )
    {
        fprintf ( stderr, "cannot activate client" );
        exit ( 1 );
    }

    /* Connect the ports.  You can't do this before the client is
     * activated, because we can't make connections to 
     * that aren't running.  Note the confusing (but necessary)
     * orientation of the driver backend ports: playback ports are
     * "input" to the backend, and capture ports are "output" from
     * it.
     */
       ports = jack_get_ports ( client, NULL, NULL, JackPortIsPhysical|JackPortIsInput );
    if ( ports == NULL )
    {
        fprintf ( stderr, "no physical playback ports\n" );
        exit ( 1 );
    }

    for ( i = 0; i < 2; i++ )
        if ( jack_connect ( client, jack_port_name ( output_ports[i] ), ports[i] ) )
            fprintf ( stderr, "cannot connect input ports\n" );

    jack_free ( ports );

    ports = jack_get_ports ( client, NULL, NULL, JackPortIsPhysical | JackPortIsOutput );
    if ( ports == NULL )
    {
        fprintf ( stderr, "no physical capture ports\n" );
        exit ( 1 );
    }

    for ( i = 0; i < 2; i++ )
        if ( jack_connect ( client, ports[i], jack_port_name ( input_ports[i] ) ) )
            fprintf ( stderr, "cannot connect input ports\n" );

    jack_free ( ports );

 

    /* install a signal handler to properly quits jack client */
#ifdef WIN32
    signal ( SIGINT, signal_handler );
    signal ( SIGABRT, signal_handler );
    signal ( SIGTERM, signal_handler );
#else
    signal ( SIGQUIT, signal_handler );
    signal ( SIGTERM, signal_handler );
    signal ( SIGHUP, signal_handler );
    signal ( SIGINT, signal_handler );
#endif

    /* keep running until the transport stops */

    while (1)
    {
#ifdef WIN32
        Sleep ( 1000 );
#else
        sleep ( 1 );
#endif
    }
    jack_client_close ( client );
    exit ( 0 );
}