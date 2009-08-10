/*
 * Copyright © 2009 Tyler Hayes
 * ALL RIGHTS RESERVED
 * [This program is licensed under the GPL version 3 or later.]
 * Please see the file COPYING in the source
 * distribution of this software for license terms.
 *
 * This plugin virtually takes a sound sample, randomly cuts it up into pieces
 * of various lengths, randomly reverses some of the pieces, and glues them all
 * back together randomly. This is based on the circus sound effects at the end
 * of "Being for the Benefit of Mr. Kite!" by The Beatles, which was created by
 * George Martin and Geoff Emerick by literally cutting up sound effects tape,
 * tossing them up in the air, and splicing the pieces back together randomly.
 *
 * Thanks to:
 * - Bart Massey of Portland State University (http://web.cecs.pdx.edu/~bart/)
 *   for suggesting LADSPA plugins as a project.
 */


//----------------
//-- INCLUSIONS --
//----------------
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "ladspa.h"
#include "xorgens.h"


//-----------------------
//-- DEFINED CONSTANTS --
//-----------------------
/*
 * These are the port numbers for the plugin
 */
// left channel input
#define KITE_INPUT_LEFT 0
// right channel output
#define KITE_INPUT_RIGHT 1
// left channel input
#define KITE_OUTPUT_LEFT 2
// right channel output
#define KITE_OUTPUT_RIGHT 3

/*
 * Other constants
 */
// the plugin's unique ID given by Richard Furse (ladspa@muse.demon.co.uk)
#define UNIQUE_ID 4304
// number of ports involved
#define PORT_COUNT 4


//-------------------------
//-- FUNCTION PROTOTYPES --
//-------------------------

// gets a random unsigned long integer
unsigned long GetRandomNaturalNumber(unsigned long lower_bound,
                                     unsigned long upper_bound);

// reverses the given block of samples (block meaning an array of LADSPA_Data,
// which are floats)
void ApplyReverse(LADSPA_Data * buffer, unsigned long start,
                  unsigned long end);

// copies a subsection of an array of LADSPA_Data (floats) into a subsection of
// another array
void CopySubBlock(LADSPA_Data * destination, unsigned long dest_start,
                  LADSPA_Data * source, unsigned long source_start,
                  unsigned long source_end);


//--------------------------------
//-- STRUCT FOR PORT CONNECTION --
//--------------------------------


typedef struct
{
    // the samples per second of the sound
    unsigned long sample_rate;
    // data locations for the input & output audio ports
    LADSPA_Data * Input_Left;
    LADSPA_Data * Input_Right;
    LADSPA_Data * Output_Left;
    LADSPA_Data * Output_Right;
} Kite;


//---------------
//-- FUNCTIONS --
//---------------


/*
 * Creates a plugin instance by allocating space for a plugin handle.
 * This function returns a LADSPA_Handle (which is a void * -- a pointer to
 * anything).
 */
LADSPA_Handle instantiate_Kite(const LADSPA_Descriptor * Descriptor,
                               unsigned long sample_rate)
{
    Kite * kite;

    // allocate space for a Kite struct instance
    kite = (Kite *) malloc(sizeof (Kite));
    // set the instance's sample rate
    if (kite)
        kite->sample_rate = sample_rate;
    // send the LADSPA_Handle to the host. If malloc failed, NULL is returned.
    return kite;
}

//-----------------------------------------------------------------------------


/*
 * Make a connection between a specified port and it's corresponding data
 * location. For example, the output port should be "connected" to the place
 * in memory where that sound data to be played is located.
 */
void connect_port_to_Kite(LADSPA_Handle instance, unsigned long Port,
                          LADSPA_Data * data_location)
{
    Kite * kite;

    // cast the (void *) instance to (Kite *) and set it to local pointer
    kite = (Kite *) instance;

    // direct the appropriate data pointer to the appropriate data location
    switch (Port)
    {
        case KITE_INPUT_LEFT:
            kite->Input_Left = data_location;
            break;
        case KITE_INPUT_RIGHT:
            kite->Input_Right = data_location;
            break;
        case KITE_OUTPUT_LEFT:
            kite->Output_Left = data_location;
            break;
        case KITE_OUTPUT_RIGHT:
            kite->Output_Right = data_location;
            break;
    }
}

//-----------------------------------------------------------------------------


/*
 * Here is where the rubber hits the road.  The actual sound manipulation
 * is done in run().
 * What is basically does is takes the block of samples and reorders them
 * in random order (sometimes reversing them).
 */
void run_Kite(LADSPA_Handle instance, unsigned long total_samples)
{
    Kite * kite = (Kite *) instance;

    /*
     * NOTE: these special cases should never happen, but you never know--like
     * if someone is developing a host program and it has some bugs in it, it
     * might pass some bad data.
     */
    if (total_samples <= 1)
    {
        printf("\nA sample count of 0 or 1 was sent to plugin.");
        printf("\nPlugin not executed.\n");
        return;
    }
    if (!kite)
    {
        printf("\nPlugin received NULL pointer for plugin instance.");
        printf("\nPlugin not executed.\n");
        return;
    }
    if (kite->sample_rate == 0)
    {
        printf("\nA sample rate of 0 was sent to plugin.");
        printf("\nPlugin not executed.\n");
        return;
    }

    // set the minimum index of the random sub-blocks to 0.25 seconds
    const unsigned long MIN_BLOCK_START = (unsigned long)
            (0.25 * kite->sample_rate);
    // set the maximum index of the random sub-block to 2.25 seconds
    const unsigned long MAX_BLOCK_END = MIN_BLOCK_START +
            (2 * kite->sample_rate);
    // buffer indexes
    unsigned long out_index = 0;
    // index points for the sub-blocks of random sizes
    unsigned long block_start_position = 0;
    unsigned long block_end_position = 0;
    // random number upper and lower bounds
    unsigned long rand_num_lower_bound = 0;
    unsigned long rand_num_upper_bound = 0;
    // the number of samples left to process (chop up into sub-blocks)
    unsigned long samples_remaining = total_samples;

    while (out_index < total_samples)
    {
        // set the lower bound for the random starting position of the sub-block
        // to 0.25 seconds worth of samples from the current point of the output
        // buffer
        rand_num_lower_bound = MIN_BLOCK_START;
        // set the upper bound for the random end position of the sub-block to
        // 2.25 seconds worth of samples from the current point of the output
        // buffer
        rand_num_upper_bound = MAX_BLOCK_END;

        // just set the start and end positions of the sub-block to process to
        // the start and end of the remaining buffer since it is less than 2
        // times the minimum size to process.
        // this takes care of the special case where the audio passed in from
        // the host is smaller than a half-second (2 times the minimum length).
        // NOTE: It is 2 times the minimum because otherwise if a sub-block was
        // processed out of it, the remaining section would be less than the
        // minimum length.
        if (samples_remaining <= MIN_BLOCK_START * 2)
        {
            block_start_position = 0;
            block_end_position = samples_remaining - 1;
        }

        // set the end position of the sub-block to process to the end of the
        // whole block if the whole block ends before the maximum cutoff point
        else if (samples_remaining <= MAX_BLOCK_END)
        {
            // set the upper bound for the random number to be used as the
            // start position of the sub-block to minimum length (.25s) from
            // the end of the remaining buffer
            rand_num_upper_bound = samples_remaining - MIN_BLOCK_START;
            // get a random start position for the sub-block
            block_start_position = GetRandomNaturalNumber(rand_num_lower_bound,
                                                          rand_num_upper_bound);
            block_end_position = samples_remaining - 1;
        }

            // get random start and end positions for the sub-block to process
        else
        {
            // get a random number for the start position
            block_start_position = GetRandomNaturalNumber(rand_num_lower_bound,
                                                          rand_num_upper_bound);
            // reset the lower bound for the random end position
            rand_num_lower_bound = block_start_position + MIN_BLOCK_START;
            /*
             * reset the upper bound for the random end position depending on
             * where the end of the remaining buffer lies.
             */
            // here it is set to the end of the remaining buffer if the end of
            // the remaining buffer comes before the maximum sub-block size
            // (2 seconds) after the recently acquired start position.
            if (samples_remaining < (block_start_position + MAX_BLOCK_END -
                                     MIN_BLOCK_START))
                rand_num_upper_bound = samples_remaining;
            // here the upper bound is set to the maximum sub-block size after
            // the start position, because the end of the remaining buffer is
            // beyond that point.
            else
                rand_num_upper_bound = block_start_position + MAX_BLOCK_END -
                    MIN_BLOCK_START - 1;
            // get a random number for the end position
            block_end_position = GetRandomNaturalNumber(rand_num_lower_bound,
                                                        rand_num_upper_bound);
        }

        // switch (or flag) for whether the sub-block should be reversed.
        short reverse = 0;
        // get a random state for reverse.  It receives 3 possible states:
        // 0, 1, or 2.  The block will only be reversed if 'reverse' is equal to
        // 0.  The reason for this is so the chances of being reversed is less
        // than not, as in 33% chance of reversal vs. 67% chance of not.
        reverse = (short) GetRandomNaturalNumber(0, 2);

        // reverse the sub-block if the reverse switch is 'on' (equals 0)
        if (reverse == 0)
        {
            // reverse the sub-block left channel
            ApplyReverse(kite->Input_Left, block_start_position,
                         block_end_position);
            // reverse the sub-block right channel
            ApplyReverse(kite->Input_Right, block_start_position,
                         block_end_position);
        }

        // append the sub-block to the output buffer for the left channel
        CopySubBlock(kite->Output_Left, out_index, kite->Input_Left,
                     block_start_position, block_end_position);

        // append the sub-block to the output buffer for the right channel
        CopySubBlock(kite->Output_Right, out_index, kite->Input_Right,
                     block_start_position, block_end_position);

        /*
         * copy the block at the end of the current valid input buffer (the 
         * portion we are still working in--0 to samples remaining) (that is
         * equal in length to the block just copied into the output buffer) to
         * the section of the input buffer that was just copied.  In other
         * words, write over the section of the input buffer that was just
         * copied with the same number of samples that end the portion of the
         * input buffer that is yet to be processed.
         * Sometimes the number of samples left in the remaining input buffer
         * is less than the number just copied.  In that case, the section
         * copied over is just the remaining section of the input buffer
         * immediately following the sub-block end position.
         */
        // get the number of samples copied
        unsigned long samples_copied = block_end_position -
                block_start_position + 1;
        // set the start index to the end section of input buffer that is
        // going to be copied over the used section
        unsigned long source_start = 0;
        // set the start index to the number of samples copied away from the
        // end of the remaining input buffer if there are more than the number
        // of samples copied left in the remaining input buffer
        if (samples_remaining - samples_copied > block_end_position)
            source_start = samples_remaining - samples_copied;
            // set the start index to the next position after the sub-block end
            // position if there are less than the number of samples copied left
            // in the remaining input buffer
        else
            source_start = block_end_position + 1;

        // write over used section of left channel input
        CopySubBlock(kite->Input_Left, block_start_position,
                     kite->Input_Left, source_start, samples_remaining - 1);

        // write over used section of right channel input
        CopySubBlock(kite->Input_Right, block_start_position,
                     kite->Input_Right, source_start, samples_remaining - 1);

        // update the output index
        out_index += samples_copied;
        // update the number of samples remaining to be processed
        samples_remaining -= samples_copied;
    }
}

//-----------------------------------------------------------------------------


/*
 * Frees dynamic memory associated with the plugin instance.
 */
void cleanup_Kite(LADSPA_Handle instance)
{
    if (instance)
        free(instance);
}

//-----------------------------------------------------------------------------

/*
 * Global LADSPA_Descriptor variable used in _init(), ladspa_descriptor(),
 * and _fini().
 */
LADSPA_Descriptor * Kite_descriptor = NULL;


/*
 * The _init() function is called whenever this plugin is first loaded
 * by the host using it (when the host program is first opened).
 */
void _init()
{
    /*
     * allocate memory for Kite_descriptor (it's just a pointer at this point).
     * in other words create an actual LADSPA_Descriptor struct instance that
     * Kite_descriptor will point to.
     */
    Kite_descriptor = (LADSPA_Descriptor *) malloc(sizeof (LADSPA_Descriptor));

    // make sure malloc worked properly before initializing the struct fields
    if (Kite_descriptor)
    {
        // assign the unique ID of the plugin given by Richard Furse
        Kite_descriptor->UniqueID = UNIQUE_ID;

        /*
         * assign the label of the plugin.
         * NOTE: it must not have white spaces as per ladspa.h.
         * NOTE: in case you were wondering, strdup() from the string library
         * makes a duplicate string of the argument and returns the duplicate's
         * pointer (a char *).
         */
        Kite_descriptor->Label = strdup("Kite");

        /*
         * assign the special property of the plugin, which is any of the three
         * defined in ladspa.h: LADSPA_PROPERTY_REALTIME,
         * LADSPA_PROPERTY_INPLACE_BROKEN, and LADSPA_PROPERTY_HARD_RT_CAPABLE.
         * They are just ints (1, 2, and 4, respectively).  See ladspa.h for
         * what they actually mean.
         */
        Kite_descriptor->Properties = LADSPA_PROPERTY_HARD_RT_CAPABLE;

        // assign the plugin name
        Kite_descriptor->Name = strdup("Kite");

        // assign the author of the plugin
        Kite_descriptor->Maker = strdup("Tyler Hayes (tgh@pdx.edu)");

        /*
         * assign the copyright info of the plugin (NOTE: use "None" for no
         * copyright as per ladspa.h)
         */
        Kite_descriptor->Copyright = strdup("GPL");

        /*
         * assign the number of ports for the plugin.
         */
        Kite_descriptor->PortCount = PORT_COUNT;

        /*
         * used for allocating and initailizing a LADSPA_PortDescriptor array
         * (which is an array of ints) since Kite_descriptor PortDescriptors
         * is a const *.
         */
        LADSPA_PortDescriptor * temp_descriptor_array;

        // allocate space for the temporary array with a length of the number
        // of ports (PortCount)
        temp_descriptor_array = (LADSPA_PortDescriptor *)
                calloc(PORT_COUNT, sizeof (LADSPA_PortDescriptor));

        /*
         * set the instance LADSPA_PortDescriptor array (PortDescriptors)
         * pointer to the location temp_descriptor_array is pointing at.
         */
        Kite_descriptor->PortDescriptors = (const LADSPA_PortDescriptor *)
                temp_descriptor_array;

        /*
         * set the port properties by ORing specific bit masks defined in
         * ladspa.h.
         *
         * these first two give the input ports the properties that tell the
         * host that the ports takes input and are audio ports (not control
         * ports).
         */
        temp_descriptor_array[KITE_INPUT_LEFT] = LADSPA_PORT_INPUT |
                LADSPA_PORT_AUDIO;
        temp_descriptor_array[KITE_INPUT_RIGHT] = LADSPA_PORT_INPUT |
                LADSPA_PORT_AUDIO;

        /*
         * this gives the output ports the properties that tell the host that
         * these ports are output ports and that they are audio ports (I don't
         * see any situation where one might be an output port but not an audio
         * port...).
         */
        temp_descriptor_array[KITE_OUTPUT_LEFT] = LADSPA_PORT_OUTPUT |
                LADSPA_PORT_AUDIO;
        temp_descriptor_array[KITE_OUTPUT_RIGHT] = LADSPA_PORT_OUTPUT |
                LADSPA_PORT_AUDIO;

        /*
         * set temp_descriptor_array to NULL for housekeeping--we don't need
         * that local variable anymore.
         */
        temp_descriptor_array = NULL;

        /*
         * temporary local variable (which is a pointer to an array of arrays
         * of characters) for the names of the ports since
         * Kite_descriptor->PortNames is a const char * const *.
         */
        char ** temp_port_names;

        // allocate the space for two port names
        temp_port_names = (char **) calloc(PORT_COUNT, sizeof (char *));

        /*
         * set the instance PortNames array pointer to the location
         * temp_port_names is pointing at.
         */
        Kite_descriptor->PortNames = (const char **) temp_port_names;

        // set the name of the input ports
        temp_port_names[KITE_INPUT_LEFT] = strdup("Input Left Channel");
        temp_port_names[KITE_INPUT_RIGHT] = strdup("Input Right Channel");

        // set the name of the ouput ports
        temp_port_names[KITE_OUTPUT_LEFT] = strdup("Output Left Channel");
        temp_port_names[KITE_OUTPUT_RIGHT] = strdup("Output Right Channel");

        // reset temp variable to NULL for housekeeping
        temp_port_names = NULL;

        /*
         * temporary local variable (pointer to a PortRangeHint struct) since
         * Kite_descriptor->PortRangeHints is a const *.
         */
        LADSPA_PortRangeHint * temp_hints;

        // allocate space for two port hints (see ladspa.h for info on 'hints')
        temp_hints = (LADSPA_PortRangeHint *)
                calloc(PORT_COUNT, sizeof (LADSPA_PortRangeHint));

        /*
         * set the instance PortRangeHints pointer to the location temp_hints
         * is pointed at.
         */
        Kite_descriptor->PortRangeHints = (const LADSPA_PortRangeHint *)
                temp_hints;

        /*
         * set the port hint descriptors (which are ints).
         */
        temp_hints[KITE_INPUT_LEFT].HintDescriptor = 0;
        temp_hints[KITE_INPUT_RIGHT].HintDescriptor = 0;
        temp_hints[KITE_OUTPUT_LEFT].HintDescriptor = 0;
        temp_hints[KITE_OUTPUT_RIGHT].HintDescriptor = 0;

        // reset temp variable to NULL for housekeeping
        temp_hints = NULL;

        // set the instance's function pointers to appropriate functions
        Kite_descriptor->instantiate = instantiate_Kite;
        Kite_descriptor->connect_port = connect_port_to_Kite;
        Kite_descriptor->activate = NULL;
        Kite_descriptor->run = run_Kite;
        Kite_descriptor->run_adding = NULL;
        Kite_descriptor->set_run_adding_gain = NULL;
        Kite_descriptor->deactivate = NULL;
        Kite_descriptor->cleanup = cleanup_Kite;
    }
}

//-----------------------------------------------------------------------------


/*
 * Returns a descriptor of this plugin.
 *
 * NOTE: this function MUST be called 'ladspa_descriptor' or else the plugin
 * will not be recognized.
 */
const LADSPA_Descriptor * ladspa_descriptor(unsigned long index)
{
    if (index == 0)
        return Kite_descriptor;
    else
        return NULL;
}

//-----------------------------------------------------------------------------


/*
 * This is called automatically when the host quits (when this dynamic library
 * is unloaded).  It frees all dynamically allocated memory associated with
 * the descriptor.
 */
void _fini()
{
    if (Kite_descriptor)
    {
        free((char *) Kite_descriptor->Label);
        free((char *) Kite_descriptor->Name);
        free((char *) Kite_descriptor->Maker);
        free((char *) Kite_descriptor->Copyright);
        free((LADSPA_PortDescriptor *) Kite_descriptor->PortDescriptors);

        /*
         * the for loop here is kind of unnecessary since the number of ports
         * was hard coded for this plugin as 2, but whatever.
         */
        int i = 0;
        for (i = 0; i < Kite_descriptor->PortCount; ++i)
            free((char *) (Kite_descriptor->PortNames[i]));

        free((char **) Kite_descriptor->PortNames);
        free((LADSPA_PortRangeHint *) Kite_descriptor->PortRangeHints);

        free(Kite_descriptor);
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
    unsigned long rand_num = 0;
    // seed the generator and retrieve a random number
    rand_num = xor4096i((unsigned long) (current_time.tv_usec *
                                         current_time.tv_sec));
    // reduce the random number to between 0 and the sample size
    rand_num = rand_num % (upper_bound - lower_bound + 1);
    // force the random number to at least the lower bound
    rand_num += lower_bound;

    return rand_num;
}

//-----------------------------------------------------------------------------


/*
 * This procedure takes a buffer (an array of floats) and resorts a subsection
 * of the array in reverse order based on the start and end indexes passed in.
 */
void ApplyReverse(LADSPA_Data * buffer, unsigned long start, unsigned long end)
{
    // a temporary holder of a value so the values in the indexes can be swapped
    LADSPA_Data holder = 0.0f;
    //swap the values in the indexes until the indexes meet in the middle
    while (start <= end)
    {
        holder = buffer[start];
        buffer[start] = buffer[end];
        buffer[end] = holder;
        ++start;
        --end;
    }
}

//-----------------------------------------------------------------------------


/*
 * This procedure copies a section of an array of LADSPA_Data (floats) into a
 * section of another array.
 *
 * NOTE: the source endpoint IS copied.
 *
 * ASSUMPTIONS: the destination array does not end before the source section
 * ends.
 */
void CopySubBlock(LADSPA_Data * destination, unsigned long dest_start,
                  LADSPA_Data * source, unsigned long src_start,
                  unsigned long src_end)
{
    if (dest_start == src_start || src_start > src_end)
        return;

    unsigned long dest_index = dest_start;
    unsigned long src_index = 0;

    for (src_index = src_start; src_index <= src_end; ++src_index)
    {
        destination[dest_index] = source[src_index];
        ++dest_index;
    }
}

// ------------------------------- EOF ----------------------------------------
