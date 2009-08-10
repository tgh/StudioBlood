/*
 * Copyright © 2009 Tyler Hayes
 * ALL RIGHTS RESERVED
 * [This program is licensed under the GPL version 3 or later.]
 * Please see the file COPYING in the source
 * distribution of this software for license terms.
 *
 * This LADSPA plugin takes the given sound from the host, and reverses
 * sub-sections of random length between 0.2 and 1.5 seconds.
 *
 * Thanks to:
 * - Bart Massey of Portland State University (http://web.cecs.pdx.edu/~bart/)
 *   for his direct help
 * - Richard Furse (http://www.muse.demon.co.uk/) for his examples
 * - Richard Brent (http://wwwmaths.anu.edu.au/~brent/random.html) for his
 *   uniform random number generator
 * - David Benson (http://gdam.ffem.org/ladspa-doc/ladspa.html) for his
 *   tutorial
 * - Dave Phillips(http://linuxdevcenter.com/pub/a/linux/2001/02/02/ladspa.html)
 *   for his ladspa information
 * 
 * NOTE: even though the official name of this plugin is "esreveR", the name
 * 'Reverse' is used throughout this code for readability.
 * 
 * Also, I use the terms 'buffer' and 'block' kind of interchangeably in my
 * comments.  By buffer I mean the array of floating point numbers that
 * represent the audio samples sent in by the host (the audio program using
 * the plugin).  By block I just mean a chunk of samples--a sub-section of the
 * buffer.  Most likey, I sometimes use one term when I mean the other, so sorry
 * for the confusion.  They pretty much mean the same thing anyway.
 */


//----------------
//-- INCLUSIONS --
//----------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "xorgens.h"
#include "ladspa.h"


//-----------------------
//-- DEFINED CONSTANTS --
//-----------------------
/*
 * These are the port numbers for the plugin
 */
#define REVERSE_INPUT 0
#define REVERSE_OUTPUT 1

/*
 * Other constants
 */
// the plugin's unique ID given by Richard Furse (ladspa@muse.demon.co.uk)
#define UNIQUE_ID 4302
// the number of ports involved
#define PORT_COUNT 2


//-------------------------
//-- FUNCTION PROTOTYPES --
//-------------------------

// gets a random unsigned long integer
unsigned long GetRandomNaturalNumber(unsigned long lower_bound,
                                     unsigned long upper_bound);


//--------------------------------
//-- STRUCT FOR PORT CONNECTION --
//--------------------------------


typedef struct
{
    // the sample rate (samples per second) of the sound from the host
    LADSPA_Data sample_rate;
    // data locations for the input & output audio ports
    LADSPA_Data * Input;
    LADSPA_Data * Output;
} Reverse;


//---------------
//-- FUNCTIONS --
//---------------


/*
 * Creates a Reverse plugin instance by allocating space for a plugin handle.
 * This function returns a LADSPA_Handle (which is a void * -- a pointer to
 * anything).
 */
LADSPA_Handle instantiate_Reverse(const LADSPA_Descriptor * Descriptor,
                                  unsigned long sample_rate)
{
    Reverse * reverse;

    // allocate space for a Reverse struct instance
    reverse = (Reverse *) malloc(sizeof (Reverse));

    // set the Reverse instance sample rate value
    reverse->sample_rate = sample_rate;

    // send the LADSPA_Handle to the host. If malloc failed, NULL is returned.
    return reverse;
}

//-----------------------------------------------------------------------------


/*
 * Make a connection between a specified port and it's corresponding data
 * location. For example, the output port should be "connected" to the place in
 * memory where that sound data to be played is located.
 */
void connect_port_to_Reverse(LADSPA_Handle instance, unsigned long Port,
                             LADSPA_Data * data_location)
{
    Reverse * reverse;

    // cast the (void *) instance to (Revolution *) and set it to local pointer
    reverse = (Reverse *) instance;

    // direct the appropriate data pointer to the appropriate data location
    if (Port == REVERSE_INPUT)
        reverse->Input = data_location;
    else if (Port == REVERSE_OUTPUT)
        reverse->Output = data_location;
}

//-----------------------------------------------------------------------------


/*
 * Here is where the actual audio manipulation is done.  It takes the sample
 * buffer and reverses sub-blocks of random length between 0.2 seconds and
 * 1.5 seconds.
 */
void run_Reverse(LADSPA_Handle instance, unsigned long total_sample_count)
{
    // set local pointer to plugin instance
    Reverse * reverse = (Reverse *) instance;

    /*
     * NOTE: these special cases should never happen, but you never know--like
     * if someone is developing a host program and it has some bugs in it, it
     * might pass some bad data.
     */
    if (total_sample_count <= 1)
    {
        printf("\nEither 0, 1, or a negative numer of sample(s) were passed");
        printf(" into the plugin.\n");
        printf("\nPlugin not executed.\n");
        return;
    }
    if (!reverse)
    {
        printf("\nPlugin received NULL pointer for plugin instance.\n");
        printf("\nPlugin not executed.\n");
        return;
    }
    // with a sample rate of anything less than 10, a sub-block could be of
    // length 0 or 1, which does nothing when reversed.  Hence, the condition
    // of the sample rate being at least 10 (a sub-block of at least 2 samples).
    if (reverse->sample_rate < 10)
    {
        printf("\nThis plugin does not accept sample rates less than 10");
        printf(" samples per second.\n");
        printf("\nPlugin not executed.\n");
        return;
    }

    // set local pointers to appropriate sample buffers
    LADSPA_Data * input_reader = reverse->Input;
    LADSPA_Data * output_writer = reverse->Output;

    // set the minimum amount of samples to reverse (0.2 seconds of sound)
    const unsigned long MIN_SAMPLES = 0.2 * reverse->sample_rate;
    // set the maximum amount of samples to reverse (1.5 seconds of sound)
    const unsigned long MAX_SAMPLES = 1.5 * reverse->sample_rate;

    // set a starting position index for the input buffer. This holds the
    // position at which the next sub-block of samples starts so that
    // the input index knows where to stop since it traverses backwards through
    // the input buffer.
    unsigned long start_position = 0;

    // set an index for the input reader and output writer
    unsigned long in_index = 0;
    unsigned long out_index = 0;

    // break loop when the output index has reached the end of the output buffer
    while (out_index < total_sample_count)
    {
        // set a random number lower bound
        unsigned long rand_num_lower_bound = start_position + MIN_SAMPLES;
        // set a random number upper bound
        unsigned long rand_num_upper_bound = start_position + MAX_SAMPLES;
        // a random number to be set later
        unsigned long random_num = 0;

        // set the input index to the end of the buffer if the lower bound is
        // within MIN_SAMPLES of the end of the buffer or if it is beyond the
        // end of the buffer (this catches the special case where the whole
        // block passed in by the host is shorter than MIN_SAMPLES).
        if (MIN_SAMPLES >= total_sample_count
            || rand_num_lower_bound >= total_sample_count - MIN_SAMPLES)
            in_index = total_sample_count - 1;

        else
        {
            // set the random number upper bound to the point where the number
            // of samples left in the buffer is MIN_SAMPLES if the upper bound
            // is within minimum samples away from the end of the buffer
            if (rand_num_upper_bound > total_sample_count - MIN_SAMPLES)
                rand_num_upper_bound = total_sample_count - MIN_SAMPLES;

            /*
             * get a random number from a random number generator.
             *
             * The purpose of the random number here is to pick a random point
             * in the input buffer (between the bounds) to which the input
             * reader will start reading backwards in order to get random sizes
             * of blocks to reverse.
             */
            random_num = GetRandomNaturalNumber(rand_num_lower_bound,
                                                rand_num_upper_bound);

            // set the input index to one less than the random number, because
            // the start position will become the point at random_num.
            in_index = random_num - 1;
        }

        // reverse the block.
        // NOTE: the test condition 'in_index != 0xFFFFFFFFFFFFFFFF' is for when
        // the start position is at zero (the beginning of the input buffer).
        // When 0 is decremented as an unsigned long it becomes that huge
        // positive number represented in hex as 0xFFFFFFFFFFFFFFFF, which of
        // course is always going to be greater than 0.
        while (in_index >= start_position && in_index != 0xFFFFFFFFFFFFFFFF)
        {
            // set the output buffer's value to the appropriate input buffer
            // value
            output_writer[out_index] = input_reader[in_index];

            // move forward in the output buffer
            ++out_index;
            // move backward in the input buffer
            --in_index;
        }

        // reset the start position to the end of the block just reversed
        start_position = random_num;
    }
}

//-----------------------------------------------------------------------------


/*
 * Frees the dynamic memory allocated for the Reverse structure instance.
 */
void cleanup_Reverse(LADSPA_Handle instance)
{
    if (instance)
        free(instance);
}

//-----------------------------------------------------------------------------

// global LADSPA_Descriptor used by _init(), _fini(), and ladspa_descriptor()
LADSPA_Descriptor * reverse_descriptor = NULL;

//-----------------------------------------------------------------------------


/*
 * The _init() function is called whenever this plugin is first loaded
 * by the host using it.
 */
void _init()
{
    // allocate memory for reverse_descriptor (it's just a pointer at this
    // point). In other words create an actual LADSPA_Descriptor struct instance
    // that reverse_descriptor will point to.
    reverse_descriptor = (LADSPA_Descriptor *)
            malloc(sizeof (LADSPA_Descriptor));

    // make sure malloc worked properly before initializing the struct fields
    if (reverse_descriptor)
    {
        // assign the unique ID of the plugin
        reverse_descriptor->UniqueID = UNIQUE_ID;

        // assign the label of the plugin. since there are no control features
        // for this plugin, "reverse" is fine.
        /*
         * NOTE: it must not have white spaces as per ladspa.h.
         * NOTE: in case you were wondering, strdup() from the string library
         * makes a duplicate string of the argument and returns the duplicate's
         * pointer (a char *).
         */
        reverse_descriptor->Label = strdup("esreveR");

        // assign the special property of the plugin, which is any of the three
        // defined in ladspa.h: LADSPA_PROPERTY_REALTIME,
        // LADSPA_PROPERTY_INPLACE_BROKEN, and LADSPA_PROPERTY_HARD_RT_CAPABLE.
        // They are just ints (1, 2, and 4, respectively).  See ladspa.h for
        // what they actually mean.
        reverse_descriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;

        // assign the plugin name
        reverse_descriptor->Name = strdup("esreveR (Random Reversal)");

        // assign the author of the plugin
        reverse_descriptor->Maker = strdup("Tyler Hayes (tgh@pdx.edu)");

        // assign the copyright info of the plugin (NOTE: use "None" for no
        // copyright as per ladspa.h)
        reverse_descriptor->Copyright = strdup("GPL");

        // assign the number of ports for the plugin.  since there are no
        // control features for Reverse, there are only 2 ports: audio input
        // and output.
        reverse_descriptor->PortCount = PORT_COUNT;

        /*
         * used for allocatingand initailizing a LADSPA_PortDescriptor array
         * (which is an array of ints) since Adt_descriptor-> PortDescriptors
         * is a const *.
         */
        LADSPA_PortDescriptor * temp_descriptor_array;

        // allocate space for the temporary array with a length of the number
        // of ports (PortCount)
        temp_descriptor_array = (LADSPA_PortDescriptor *)
                calloc(PORT_COUNT, sizeof (LADSPA_PortDescriptor));

        // set the instance LADSPA_PortDescriptor array (PortDescriptors)
        // pointer to the location temp_descriptor_array is pointing at.
        reverse_descriptor->PortDescriptors = (const LADSPA_PortDescriptor *)
                temp_descriptor_array;

        // set the port properties by ORing specific bit masks defined in
        // ladspa.h. this first one gives the first port the properties that
        // tell the host that this port takes input and is an audio port (not a
        // control port).
        temp_descriptor_array[REVERSE_INPUT] = LADSPA_PORT_INPUT |
                LADSPA_PORT_AUDIO;

        // this gives the second port the properties that tell the host that
        // this port is an output port and that it is an audio port (I don't
        // see any situation where one might be an output port, but not an
        // audio port...).
        temp_descriptor_array[REVERSE_OUTPUT] = LADSPA_PORT_OUTPUT |
                LADSPA_PORT_AUDIO;

        // set temp_descriptor_array to NULL for housekeeping--we don't need
        // that local variable anymore.
        temp_descriptor_array = NULL;

        /*
         * temporary local variable (which is a pointer to an array of arrays
         * of characters) for the names of the ports since Adt_descriptor->
         * PortNames is a const char * const *.
         */
        char ** temp_port_names;

        // allocate the space for two port names
        temp_port_names = (char **) calloc(PORT_COUNT, sizeof (char *));

        // set the instance PortNames array pointer to the location
        // temp_port_names is pointing at.
        reverse_descriptor->PortNames = (const char **) temp_port_names;

        // set the name of the input port
        temp_port_names[REVERSE_INPUT] = strdup("Input");

        // set the name of the ouput port
        temp_port_names[REVERSE_OUTPUT] = strdup("Output");

        // reset temp variable to NULL for housekeeping
        temp_port_names = NULL;

        /*
         * temporary local variable (pointerto a PortRangeHint struct) since
         * Adt_descriptor->PortRangeHints is a const *.
         */
        LADSPA_PortRangeHint * temp_hints;

        // allocate space for two port hints (see ladspa.h for info on 'hints')
        temp_hints = (LADSPA_PortRangeHint *)
                calloc(PORT_COUNT, sizeof (LADSPA_PortRangeHint));

        // set the instance PortRangeHints pointer to the location temp_hints
        // is pointed at.
        reverse_descriptor->PortRangeHints = (const LADSPA_PortRangeHint *)
                temp_hints;

        // set the port hint descriptors (which are ints). Since this is a
        // simple reverse effect, input and ouput don't need any range hints.
        temp_hints[REVERSE_INPUT].HintDescriptor = 0;
        temp_hints[REVERSE_OUTPUT].HintDescriptor = 0;

        // reset temp variable to NULL for housekeeping
        temp_hints = NULL;

        // set the instance's function pointers to appropriate functions
        reverse_descriptor->instantiate = instantiate_Reverse;
        reverse_descriptor->connect_port = connect_port_to_Reverse;
        reverse_descriptor->activate = NULL;
        reverse_descriptor->run = run_Reverse;
        reverse_descriptor->run_adding = NULL;
        reverse_descriptor->set_run_adding_gain = NULL;
        reverse_descriptor->deactivate = NULL;
        reverse_descriptor->cleanup = cleanup_Reverse;
    }
}

//-----------------------------------------------------------------------------


/*
 * Returns a descriptor of the requested plugin type (there is only one plugin
 * type in this library).
 */
const LADSPA_Descriptor * ladspa_descriptor(unsigned long index)
{
    if (index == 0)
        return reverse_descriptor;
    else
        return NULL;
}

//-----------------------------------------------------------------------------


/*
 * This function is automatically called when the host is done with the plugin
 * (when the dynamic library is unloaded).  If frees all dynamic memory
 * allocated for the LADSPA_Descriptor structure instantiated by
 * reverse_descriptor.
 */
void _fini()
{
    if (reverse_descriptor)
    {
        free((char *) reverse_descriptor->Label);
        free((char *) reverse_descriptor->Name);
        free((char *) reverse_descriptor->Maker);
        free((char *) reverse_descriptor->Copyright);

        int i = 0;
        for (i = 0; i < reverse_descriptor->PortCount; ++i)
            free((char *) reverse_descriptor->PortNames[i]);

        free((char **) reverse_descriptor->PortNames);
        free((LADSPA_PortRangeHint *) reverse_descriptor->PortRangeHints);

        free(reverse_descriptor);
    }
}

//-----------------------------------------------------------------------------


/*
 * This function uses Richard Brent's uniform random number generator (see
 * comments in the function) to get a random unsigned long integer.  It is
 * seeded with the current time's seconds and nanoseconds.
 */
unsigned long GetRandomNaturalNumber(unsigned long lower_bound,
                                     unsigned long upper_bound)
{
    // get the current time to seed the generator
    struct timeval current_time;
    gettimeofday(&current_time, NULL);

    /*
     * This next line uses a uniform random number generator by Richard Brent.
     * (http://wwwmaths.anu.edu.au/~brent/random.html)
     * which is licensed under the GNU Public License v2.
     * See xorgens.c and xorgens.h for the source code.  Many thanks
     * to Richard Brent.
     *
     * NOTE: the tv_sec and tv_usec members of the timeval struct are
     * long integers that represent the current time in seconds and
     * nanoseconds, respectively, since Jan. 1, 1970.  They are used
     * here to seed the generator.  The generator is called with
     * xor4096i(), which, unlike the C standard generator, is seeded
     * and returns a number with the same call.
     */
    return lower_bound
            + (xor4096i((unsigned long) (current_time.tv_usec *
                                         current_time.tv_sec))
               % (upper_bound - lower_bound + 1));
}

//---------------------------------- EOF --------------------------------------
